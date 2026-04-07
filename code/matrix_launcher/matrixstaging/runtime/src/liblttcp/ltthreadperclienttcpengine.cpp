#include "ltthreadperclienttcpengine.h"

#include "../libltmessaging/messageconnection.h"
#include "../libltnet/sys/pc/pcsocket.h"
#include "../../../game/src/libltclientlogin/loginmediator.h"
#include <spdlog/spdlog.h>

#include <bits/stl_tree.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <process.h>

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mxo::liblttcp {

namespace {

// UNANCHORED helper used by the starter scaffold.
// No direct launcher.exe function anchor is assigned yet.
static bool ResolveIpv4Address(const char* hostName, uint32_t* outIpv4NetworkOrder) {
    if (!hostName || !hostName[0] || !outIpv4NetworkOrder || !mxo::libltnet::CLTSocketLayer::Init()) {
        return false;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &results) != 0 || !results) {
        return false;
    }

    bool ok = false;
    for (addrinfo* it = results; it; it = it->ai_next) {
        if (it->ai_family != AF_INET || !it->ai_addr || it->ai_addrlen < static_cast<int>(sizeof(sockaddr_in))) {
            continue;
        }
        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(it->ai_addr);
        *outIpv4NetworkOrder = addr->sin_addr.s_addr;
        ok = true;
        break;
    }

    freeaddrinfo(results);
    return ok;
}

// Per-logger SPDLOG_LEVEL overrides only apply on call sites that explicitly fetch a named logger.
// Keep the receive hot-path seam narrow by only routing labels with registered logger names through
// spdlog::get(...); all other bridge labels fall back to the default logger.
static spdlog::logger* LoggerForBridgeLabel(const char* label) {
    if (label && label[0]) {
        if (std::shared_ptr<spdlog::logger> logger = spdlog::get(label)) {
            return logger.get();
        }
    }
    return spdlog::default_logger_raw();
}

// Runtime ablation toggle for the `ab28b26` producer-side direct-context correction.
// Default remains the current higher-fidelity/original-backed mode:
// - queue parsed-packet / status / close work with the direct connection object as `context`
// Setting `MXO_USE_QUEUE_CONTEXT_BRIDGE=1` reverts just those producers back to the older
// source-owned `CBaseConnection_QueueContextScaffold` bridge so we can A/B late render/crash
// behavior without rolling back the later state-machine / close-tail fidelity work.
static bool UseLegacyQueueContextBridgeProducerAblation() {
    static int cached = -1;
    if (cached < 0) {
        const char* raw = std::getenv("MXO_USE_QUEUE_CONTEXT_BRIDGE");
        cached = (raw && raw[0] && !(raw[0] == '0' && raw[1] == '\0')) ? 1 : 0;
        spdlog::info(
            "CLTThreadPerClientTCPEngine queue-context producer mode={} [MXO_USE_QUEUE_CONTEXT_BRIDGE={}]",
            cached ? "legacy-bridge-ablation" : "direct-connection-original-default",
            (raw && raw[0]) ? raw : "<unset>");
    }
    return cached != 0;
}

static void* ResolveQueuedConnectionContextForProducerAblation(CLTTCPConnection* connection) {
    if (!connection) {
        return nullptr;
    }
    if (UseLegacyQueueContextBridgeProducerAblation()) {
        return connection->QueueContextScaffold();
    }
    return static_cast<void*>(connection);
}

struct CLTThreadPerClientTCPEngine_QueuePair {
    uint32_t value0;
    uint32_t value1;
};

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_LauncherConnectionBridgeWorkItemVtable[2] = {0};
static void* g_LauncherConnectionBridgeContextVtable[5] = {0};
static constexpr uint8_t kSocketFactoryFlagSkipDisableNagle = 0x01u;
static constexpr uint8_t kSocketFactoryFlagKeepBlocking = 0x02u;

// UNANCHORED: source-owned narrow mirror of the original queue block free-list behavior.
// Static RE already shows that the consumer path recycles exhausted blocks instead of treating the
// transition as a simple free-and-forget step. Current source keeps that narrower behavior in a
// side cache keyed by the queue object while the exact original in-object free-list plumbing is
// still unrecovered.
static std::unordered_map<CLTThreadPerClientTCPEngine_Queue*, std::vector<uint32_t*>>
    g_QueueRecycledBlocks;

// GHIDRA layout audit anchors:
// - derived ctor `launcher.exe:0x431c30`
// - base ctor `launcher.exe:0x4366f0`
// - derived dtor `launcher.exe:0x431310`
// - worker insert helper `launcher.exe:0x431ff0`
// - endpoint insert/search/remove family `0x4318f0 / 0x42fdb0 / 0x4154d0`
// - context insert/search/remove family `0x4196b0 / 0x42fe10 / 0x4154d0`
// Current best read:
// - the real `0xb4` engine object is already fully accounted for by the recovered in-object fields
//   at `+0x04/+0x08/+0x0c/+0x34/+0x5c/+0x60/+0x7c/+0x80/+0x84/+0x8c/+0x90/+0x98`
// - so none of the source-owned maps below should be mistaken for hidden original class fields
// - recovered runtime payload families are tracked separately from source-only launcher bridge
//   baggage using node shapes that match the launcher tree families rather than source vectors

static_assert(sizeof(std::_Rb_tree_node_base) == 0x10, "launcher tree node-base size mismatch");
using CLTThreadPerClientTCPEngine_EndpointTreeNode =
    std::_Rb_tree_node<std::pair<LTTCPEndpointKey, CLTThreadPerClientTCPEngine_AcceptThread*>>;
using CLTThreadPerClientTCPEngine_ContextTreeNode =
    std::_Rb_tree_node<std::pair<uint32_t, CLTThreadPerClientTCPEngine_WorkerThread*>>;
static_assert(sizeof(std::pair<LTTCPEndpointKey, CLTThreadPerClientTCPEngine_AcceptThread*>) == 0x14, "endpoint tree value size mismatch");
static_assert(sizeof(std::pair<uint32_t, CLTThreadPerClientTCPEngine_WorkerThread*>) == 0x8, "context tree value size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_EndpointTreeNode) == 0x24, "endpoint tree node size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_ContextTreeNode) == 0x18, "context tree node size mismatch");

struct CLTThreadPerClientTCPEngine_EndpointPayloadEntry {
    std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> payload;
    CLTThreadPerClientTCPEngine_EndpointTreeNode node = {};
};

struct CLTThreadPerClientTCPEngine_ContextPayloadEntry {
    std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> payload;
    CLTThreadPerClientTCPEngine_ContextTreeNode node = {};
};

struct CLTThreadPerClientTCPEngine_EndpointPayloadBacking {
    std::unordered_map<CLTThreadPerClientTCPEngine_EndpointTreeNode*, std::unique_ptr<CLTThreadPerClientTCPEngine_EndpointPayloadEntry>> entries;
};

struct CLTThreadPerClientTCPEngine_ContextPayloadBacking {
    std::unordered_map<CLTThreadPerClientTCPEngine_ContextTreeNode*, std::unique_ptr<CLTThreadPerClientTCPEngine_ContextPayloadEntry>> entries;
};

static std::unordered_map<CLTThreadPerClientTCPEngine*, CLTThreadPerClientTCPEngine_LauncherAbiAttachment>
    g_CLTThreadPerClientTCPEngineLauncherAbiAttachments;
static std::unordered_map<CLTThreadPerClientTCPEngine*, CLTThreadPerClientTCPEngine_EndpointPayloadBacking>
    g_CLTThreadPerClientTCPEngineEndpointPayloadBackings;
static std::unordered_map<CLTThreadPerClientTCPEngine*, CLTThreadPerClientTCPEngine_ContextPayloadBacking>
    g_CLTThreadPerClientTCPEngineContextPayloadBackings;

static CLTThreadPerClientTCPEngine_LauncherAbiAttachment* FindEngineLauncherAbiAttachment(
    const CLTThreadPerClientTCPEngine* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngineLauncherAbiAttachments.find(
        const_cast<CLTThreadPerClientTCPEngine*>(self));
    return (it != g_CLTThreadPerClientTCPEngineLauncherAbiAttachments.end()) ? &it->second
                                                                          : nullptr;
}

static CLTThreadPerClientTCPEngine_LauncherAbiAttachment& EnsureEngineLauncherAbiAttachment(
    CLTThreadPerClientTCPEngine* self) {
    return g_CLTThreadPerClientTCPEngineLauncherAbiAttachments[self];
}

static CLTThreadPerClientTCPEngine_EndpointPayloadBacking* FindEngineEndpointPayloadBacking(
    const CLTThreadPerClientTCPEngine* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngineEndpointPayloadBackings.find(
        const_cast<CLTThreadPerClientTCPEngine*>(self));
    return (it != g_CLTThreadPerClientTCPEngineEndpointPayloadBackings.end()) ? &it->second : nullptr;
}

static CLTThreadPerClientTCPEngine_EndpointPayloadBacking& EnsureEngineEndpointPayloadBacking(
    CLTThreadPerClientTCPEngine* self) {
    return g_CLTThreadPerClientTCPEngineEndpointPayloadBackings[self];
}

static CLTThreadPerClientTCPEngine_ContextPayloadBacking* FindEngineContextPayloadBacking(
    const CLTThreadPerClientTCPEngine* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngineContextPayloadBackings.find(
        const_cast<CLTThreadPerClientTCPEngine*>(self));
    return (it != g_CLTThreadPerClientTCPEngineContextPayloadBackings.end()) ? &it->second : nullptr;
}

static CLTThreadPerClientTCPEngine_ContextPayloadBacking& EnsureEngineContextPayloadBacking(
    CLTThreadPerClientTCPEngine* self) {
    return g_CLTThreadPerClientTCPEngineContextPayloadBackings[self];
}

static bool IsLauncherBridgeContextScaffold(
    const mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context) {
    return context != nullptr && context->vtable == g_LauncherConnectionBridgeContextVtable;
}

// Static-RE note for the current bridge seam:
// - original `0x4325d0/0x4328a0 -> 0x431ff0` and `0x449d40 -> 0x436820` paths are connection-keyed
// - auth/margin connection ctors (`0x41d170` / `0x41e500`) store the owning mediator directly at
//   connection `+0xa4`
// - current source therefore keeps the extra launcher-bridge context only as a mediator-owned
//   sidecar resolved from that direct owner pointer, not as the connection's actual owner/context
static mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*
ResolveLauncherBridgeContextForConnectionScaffold(
    const CMessageConnection* connection) {
    if (!connection) {
        return nullptr;
    }

    void* const ownerContext = connection->OwnerContext();
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* directContext =
        static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(ownerContext);
    if (IsLauncherBridgeContextScaffold(directContext) &&
        directContext->sidecarConnection == connection) {
        return directContext;
    }

    // Fidelity improvement:
    // - static RE for auth/margin connection construction says connection `+0xa4` stores the
    //   owning mediator directly
    // - prefer that concrete owner-context pointer over the replacement-only active-state global
    //   when resolving the launcher bridge sidecar
    // - fall back to the global only if owner-context is null
    mxo::ltlogin::CLTLoginMediator* mediator =
        ownerContext != nullptr
            ? static_cast<mxo::ltlogin::CLTLoginMediator*>(ownerContext)
            : mxo::ltlogin::CLTLoginMediator::ActiveStateSourceScaffold();
    if (mediator == nullptr) {
        return nullptr;
    }

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        mediator->ResolveConnectionBridgeContextScaffold(connection);
    return (context != nullptr && context->sidecarConnection == connection) ? context : nullptr;
}

template <typename Head>
static std::_Rb_tree_node_base* TreeHeaderBase(Head* head) {
    return reinterpret_cast<std::_Rb_tree_node_base*>(head);
}

template <typename Head>
static const std::_Rb_tree_node_base* TreeHeaderBase(const Head* head) {
    return reinterpret_cast<const std::_Rb_tree_node_base*>(head);
}

template <typename Node, typename Head>
static Node* TreeRootNode(Head* head) {
    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    return (header && header->_M_parent) ? static_cast<Node*>(header->_M_parent) : nullptr;
}

template <typename Node, typename Head>
static const Node* TreeRootNode(const Head* head) {
    const std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    return (header && header->_M_parent) ? static_cast<const Node*>(header->_M_parent) : nullptr;
}

// Tree users below now call the MinGW libstdc++ `bits/stl_tree.h` API directly.
// Reference donor/header lineage for this pass:
// - `/usr/lib/gcc/i686-w64-mingw32/13-win32/include/c++/bits/stl_tree.h`
// - the local `13-posix` copy is identical on this machine
// Launcher.exe remains the source of truth for object layout, call shape, and wrapper behavior.
//
// Retained helpers below are wrapper-level adapters, not duplicate `_Rb_tree` mechanics:
// - `TreeHeaderBase` / `TreeRootNode` adapt the recovered launcher head layout to
//   `_Rb_tree_node_base`
// - endpoint/context compare and find helpers match the recovered wrapper families above the shared
//   `_Rb_tree` core
// - insert/erase helpers still own duplicate handling and source-owned payload/backing lifetime
//
// anchor family: launcher.exe:0x44b040 used by 0x42fdb0 / 0x4318f0 / 0x431240
// Current best static read:
// - endpoint tree ordering compares only `portNetworkOrder`, then `ipv4NetworkOrder`
// - `family/reserved0/reserved1` remain part of the copied key payload, but are not currently
//   evidenced as tree-ordering fields in launcher.exe
static int CompareEndpointTreeKeys(
    const LTTCPEndpointKey& lhs,
    const LTTCPEndpointKey& rhs) {
    if (lhs.portNetworkOrder != rhs.portNetworkOrder) {
        return (lhs.portNetworkOrder < rhs.portNetworkOrder) ? -1 : 1;
    }
    if (lhs.ipv4NetworkOrder != rhs.ipv4NetworkOrder) {
        return (lhs.ipv4NetworkOrder < rhs.ipv4NetworkOrder) ? -1 : 1;
    }
    return 0;
}

static int CompareContextTreeKeys(uint32_t lhs, uint32_t rhs) {
    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }
    return 0;
}

template <typename Node, typename Head, typename Key, typename Compare>
static Node* LauncherTreeFindNode(const Head* head, const Key& key, Compare compare) {
    const Node* candidate = nullptr;
    const Node* node = TreeRootNode<Node>(head);
    while (node) {
        if (compare(node->_M_valptr()->first, key) >= 0) {
            candidate = node;
            node = static_cast<const Node*>(node->_M_left);
        } else {
            node = static_cast<const Node*>(node->_M_right);
        }
    }
    return (candidate && compare(candidate->_M_valptr()->first, key) == 0)
        ? const_cast<Node*>(candidate)
        : nullptr;
}

template <typename Node, typename Head, typename Backing, typename Entry, typename Key, typename Record, typename Compare>
static Record* LauncherTreeInsertUniqueOwnedNode(
    Backing& backing,
    Head* head,
    std::unique_ptr<Entry> entry,
    const Key& key,
    Record* payload,
    Compare compare) {
    if (!head || !entry || !payload) {
        return nullptr;
    }

    entry->node._M_valptr()->first = key;
    entry->node._M_valptr()->second = payload;

    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    std::_Rb_tree_node_base* parent = header;
    Node* current = TreeRootNode<Node>(head);
    bool insertLeft = true;
    while (current) {
        parent = current;
        const int cmp = compare(entry->node._M_valptr()->first, current->_M_valptr()->first);
        if (cmp == 0) {
            return current->_M_valptr()->second;
        }
        insertLeft = (cmp < 0);
        current = insertLeft ? static_cast<Node*>(current->_M_left)
                             : static_cast<Node*>(current->_M_right);
    }

    Node* insertedNode = &entry->node;
    insertedNode->_M_parent = nullptr;
    insertedNode->_M_left = nullptr;
    insertedNode->_M_right = nullptr;
    insertedNode->_M_color = std::_S_red;

    std::_Rb_tree_insert_and_rebalance(insertLeft, insertedNode, parent, *header);
    Record* insertedPayload = insertedNode->_M_valptr()->second;
    backing.entries.emplace(insertedNode, std::move(entry));
    return insertedPayload;
}

template <typename Node, typename Head, typename Backing>
static bool LauncherTreeEraseOwnedNode(Backing* backing, Head* head, Node* node) {
    if (!backing || !head || !node) {
        return false;
    }
    std::_Rb_tree_node_base* erased = std::_Rb_tree_rebalance_for_erase(node, *TreeHeaderBase(head));
    backing->entries.erase(static_cast<Node*>(erased));
    return true;
}

// anchor: launcher.exe:0x42fdb0
static CLTThreadPerClientTCPEngine_EndpointTreeNode* EndpointTreeFindNode(
    const CLTThreadPerClientTCPEngine_EndpointTreeHead24* head,
    const LTTCPEndpointKey& key) {
    return LauncherTreeFindNode<CLTThreadPerClientTCPEngine_EndpointTreeNode>(
        head,
        key,
        CompareEndpointTreeKeys);
}

static CLTThreadPerClientTCPEngine_EndpointPayloadEntry* FindEngineEndpointPayloadEntry(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node) {
    if (!node) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_EndpointPayloadBacking* backing =
        FindEngineEndpointPayloadBacking(self);
    if (!backing) {
        return nullptr;
    }

    auto it = backing->entries.find(node);
    return (it != backing->entries.end()) ? it->second.get() : nullptr;
}

// anchor family: launcher.exe:0x4318f0 / 0x431240
// Static-RE note:
// - launcher.exe inserts an endpoint node first, with `[node+0x20]` still null
// - later `0x431ce0` fills that payload slot with the direct `AcceptThread` object pointer on the
//   success path, or erases the node again on bind/listen failure via `0x431200`
static CLTThreadPerClientTCPEngine_EndpointTreeNode* EndpointTreeInsertUniqueNode(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* head,
    const LTTCPEndpointKey& key,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_EndpointPayloadBacking& backing =
        EnsureEngineEndpointPayloadBacking(self);
    auto entry = std::make_unique<CLTThreadPerClientTCPEngine_EndpointPayloadEntry>();
    if (!entry) {
        return nullptr;
    }

    entry->node._M_valptr()->first = key;
    entry->node._M_valptr()->second = nullptr;

    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    std::_Rb_tree_node_base* parent = header;
    CLTThreadPerClientTCPEngine_EndpointTreeNode* current =
        TreeRootNode<CLTThreadPerClientTCPEngine_EndpointTreeNode>(head);
    bool insertLeft = true;
    while (current) {
        parent = current;
        const int cmp = CompareEndpointTreeKeys(
            entry->node._M_valptr()->first,
            current->_M_valptr()->first);
        if (cmp == 0) {
            return current;
        }
        insertLeft = (cmp < 0);
        current = insertLeft
            ? static_cast<CLTThreadPerClientTCPEngine_EndpointTreeNode*>(current->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_EndpointTreeNode*>(current->_M_right);
    }

    CLTThreadPerClientTCPEngine_EndpointTreeNode* insertedNode = &entry->node;
    insertedNode->_M_parent = nullptr;
    insertedNode->_M_left = nullptr;
    insertedNode->_M_right = nullptr;
    insertedNode->_M_color = std::_S_red;

    std::_Rb_tree_insert_and_rebalance(insertLeft, insertedNode, parent, *header);
    backing.entries.emplace(insertedNode, std::move(entry));
    if (outInserted) {
        *outInserted = true;
    }
    return insertedNode;
}

static bool EndpointTreeAttachPayload(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node,
    std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> payload) {
    if (!payload) {
        return false;
    }

    CLTThreadPerClientTCPEngine_EndpointPayloadEntry* entry =
        FindEngineEndpointPayloadEntry(self, node);
    if (!entry) {
        return false;
    }

    node->_M_valptr()->second = payload.get();
    entry->payload = std::move(payload);
    return true;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> EndpointTreeDetachPayload(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node) {
    CLTThreadPerClientTCPEngine_EndpointPayloadEntry* entry =
        FindEngineEndpointPayloadEntry(self, node);
    if (!entry) {
        return nullptr;
    }

    node->_M_valptr()->second = nullptr;
    return std::move(entry->payload);
}

// anchor: launcher.exe:0x42fe10
static CLTThreadPerClientTCPEngine_ContextTreeNode* ContextTreeFindNode(
    const CLTThreadPerClientTCPEngine_ContextTreeHead18* head,
    uint32_t key) {
    return LauncherTreeFindNode<CLTThreadPerClientTCPEngine_ContextTreeNode>(
        head,
        key,
        CompareContextTreeKeys);
}

static CLTThreadPerClientTCPEngine_ContextPayloadEntry* FindEngineContextPayloadEntry(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_ContextTreeNode* node) {
    if (!node) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_ContextPayloadBacking* backing =
        FindEngineContextPayloadBacking(self);
    if (!backing) {
        return nullptr;
    }

    auto it = backing->entries.find(node);
    return (it != backing->entries.end()) ? it->second.get() : nullptr;
}

// anchor family: launcher.exe:0x4196b0 / 0x420ba0
// Static-RE note:
// - launcher.exe inserts a context-keyed node whose payload at `[node+0x14]` is the direct
//   `WorkerThread` object pointer
// - helper `0x431ff0` allocates that payload first, then inserts `(connection, workerThread)` into
//   the `+0x8c` tree under helper `+0x98` lock
static CLTThreadPerClientTCPEngine_ContextTreeNode* ContextTreeInsertUniqueNode(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_ContextTreeHead18* head,
    uint32_t key,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_ContextPayloadBacking& backing =
        EnsureEngineContextPayloadBacking(self);
    auto entry = std::make_unique<CLTThreadPerClientTCPEngine_ContextPayloadEntry>();
    if (!entry) {
        return nullptr;
    }

    entry->node._M_valptr()->first = key;
    entry->node._M_valptr()->second = nullptr;

    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    std::_Rb_tree_node_base* parent = header;
    CLTThreadPerClientTCPEngine_ContextTreeNode* current =
        TreeRootNode<CLTThreadPerClientTCPEngine_ContextTreeNode>(head);
    bool insertLeft = true;
    while (current) {
        parent = current;
        const int cmp = CompareContextTreeKeys(
            entry->node._M_valptr()->first,
            current->_M_valptr()->first);
        if (cmp == 0) {
            return current;
        }
        insertLeft = (cmp < 0);
        current = insertLeft
            ? static_cast<CLTThreadPerClientTCPEngine_ContextTreeNode*>(current->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_ContextTreeNode*>(current->_M_right);
    }

    CLTThreadPerClientTCPEngine_ContextTreeNode* insertedNode = &entry->node;
    insertedNode->_M_parent = nullptr;
    insertedNode->_M_left = nullptr;
    insertedNode->_M_right = nullptr;
    insertedNode->_M_color = std::_S_red;

    std::_Rb_tree_insert_and_rebalance(insertLeft, insertedNode, parent, *header);
    backing.entries.emplace(insertedNode, std::move(entry));
    if (outInserted) {
        *outInserted = true;
    }
    return insertedNode;
}

static bool ContextTreeAttachPayload(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_ContextTreeNode* node,
    std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> payload) {
    if (!payload) {
        return false;
    }

    CLTThreadPerClientTCPEngine_ContextPayloadEntry* entry =
        FindEngineContextPayloadEntry(self, node);
    if (!entry) {
        return false;
    }

    node->_M_valptr()->second = payload.get();
    entry->payload = std::move(payload);
    return true;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> ContextTreeDetachPayload(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_ContextTreeNode* node) {
    CLTThreadPerClientTCPEngine_ContextPayloadEntry* entry =
        FindEngineContextPayloadEntry(self, node);
    if (!entry) {
        return nullptr;
    }

    node->_M_valptr()->second = nullptr;
    return std::move(entry->payload);
}

// anchor family: launcher.exe:0x4154d0
// Static-RE note:
// - `_Rb_tree_rebalance_for_erase` handles the shared unlink/rebalance mechanics already
// - this retained wrapper only bridges recovered head/node layout and source-owned backing cleanup
static bool EndpointTreeEraseNode(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* head,
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node) {
    return LauncherTreeEraseOwnedNode(
        FindEngineEndpointPayloadBacking(self),
        head,
        node);
}

// anchor family: launcher.exe:0x4154d0
// Static-RE note:
// - `_Rb_tree_rebalance_for_erase` handles the shared unlink/rebalance mechanics already
// - this retained wrapper only bridges recovered head/node layout and source-owned backing cleanup
static bool ContextTreeEraseNode(
    CLTThreadPerClientTCPEngine* self,
    CLTThreadPerClientTCPEngine_ContextTreeHead18* head,
    CLTThreadPerClientTCPEngine_ContextTreeNode* node) {
    return LauncherTreeEraseOwnedNode(
        FindEngineContextPayloadBacking(self),
        head,
        node);
}

static void EraseEngineBackings(CLTThreadPerClientTCPEngine* self) {
    g_CLTThreadPerClientTCPEngineLauncherAbiAttachments.erase(self);
    g_CLTThreadPerClientTCPEngineEndpointPayloadBackings.erase(self);
    g_CLTThreadPerClientTCPEngineContextPayloadBackings.erase(self);
}

// UNANCHORED: source-owned helper for the current queue-context unwrapping seam.
// Current narrowed read:
// - when the queued context is the bridge object, worker-tree and slot-12 paths need the owning
//   connection object key instead
static void* ResolveEngineContextKeyScaffold(void* contextKey) {
    return CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
}

// UNANCHORED: source-owned helper for the current queue-context bridge lookup seams.
static CBaseConnection* ResolveEngineQueueContextOwnerScaffold(void* contextKey) {
    return CBaseConnection_FromQueueContextScaffold(contextKey);
}

static const char* LauncherBridgeWorkTypeName(uint32_t workType) {
    switch (workType) {
        case CLTThreadPerClientTCPEngine::kWorkTypeClose:
            return "Close";
        case CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus:
            return "ConnectionStatus";
        case CLTThreadPerClientTCPEngine::kWorkTypeParsedPacket:
            return "ParsedPacket";
        case CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain:
            return "SyntheticReceiveDrain";
        default:
            return "Unknown";
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall LauncherConnectionBridgeWorkItem_Release(
    mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* self) {
    if (self) {
        LoggerForBridgeLabel(self->debugLabel)->info(
            "CLTThreadPerClientTCPEngine launcher bridge releasing queued work item {} type=0x{:08x} ({}) payload=0x{:08x} label='{}'",
            fmt::ptr(self),
            self->header.workType,
            LauncherBridgeWorkTypeName(self->header.workType),
            self->workPayload,
            self->debugLabel ? self->debugLabel : "<null>");
        std::free(self);
    }
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void EnsureLauncherConnectionBridgeWorkItemVtableInitialized() {
    if (!g_LauncherConnectionBridgeWorkItemVtable[1]) {
        g_LauncherConnectionBridgeWorkItemVtable[1] =
            reinterpret_cast<void*>(LauncherConnectionBridgeWorkItem_Release);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void EnsureLauncherConnectionBridgeContextVtableInitialized() {
    if (!g_LauncherConnectionBridgeContextVtable[1]) {
        // Fidelity correction from the current RE pass:
        // - original queue/context traffic is still connection-centric
        // - launcher-bridge work items are now queued only through the sidecar connection's own
        //   queue-context object, not through this mediator-owned bridge record
        // - auth/margin connection ctors store the mediator directly at connection `+0xa4`, so this
        //   bridge vtable remains only as an unexpected-path guard / identity marker on the
        //   separate mediator-owned sidecar record
        g_LauncherConnectionBridgeContextVtable[1] =
            reinterpret_cast<void*>(mxo::ltlogin::LauncherConnectionBridgeContext_ReleaseScaffold);
        g_LauncherConnectionBridgeContextVtable[4] =
            reinterpret_cast<void*>(mxo::ltlogin::LauncherConnectionBridgeContext_OnOperationCompletedScaffold);
    }
}

// UNANCHORED internal helper used by the current thread-object scaffolds.
static void CloseSocketHandle(uint32_t* socketHandle) {
    if (!socketHandle || *socketHandle == kInvalidSocketHandle) {
        return;
    }

    closesocket(static_cast<SOCKET>(*socketHandle));
    *socketHandle = kInvalidSocketHandle;
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static void QueueRecycleBlockScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t* block) {
    if (!queue || !block) {
        return;
    }
    g_QueueRecycledBlocks[queue].push_back(block);
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static uint32_t* QueueTakeRecycledBlockScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return nullptr;
    }

    auto it = g_QueueRecycledBlocks.find(queue);
    if (it == g_QueueRecycledBlocks.end() || it->second.empty()) {
        return nullptr;
    }

    uint32_t* block = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) {
        g_QueueRecycledBlocks.erase(it);
    }
    return block;
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static void QueueFreeRecycledBlocksScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return;
    }

    auto it = g_QueueRecycledBlocks.find(queue);
    if (it == g_QueueRecycledBlocks.end()) {
        return;
    }

    for (uint32_t* block : it->second) {
        std::free(block);
    }
    g_QueueRecycledBlocks.erase(it);
}

static CRITICAL_SECTION* CriticalSectionFromOpaqueStorage(void* storage) {
    return static_cast<CRITICAL_SECTION*>(storage);
}

// anchor: launcher.exe:0x452270 / 0x452300 / 0x452320 helper family shape
static uint32_t CreateConnectedWakeupSocketHandle() {
    if (!mxo::libltnet::CLTSocketLayer::Init()) {
        return kInvalidSocketHandle;
    }

    SOCKET wakeupSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wakeupSocket == INVALID_SOCKET) {
        return kInvalidSocketHandle;
    }

    sockaddr_in wakeupAddr = {};
    wakeupAddr.sin_family = AF_INET;
    wakeupAddr.sin_port = htons(0);
    wakeupAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(wakeupSocket, reinterpret_cast<const sockaddr*>(&wakeupAddr), sizeof(wakeupAddr)) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    int wakeupAddrSize = sizeof(wakeupAddr);
    if (getsockname(wakeupSocket, reinterpret_cast<sockaddr*>(&wakeupAddr), &wakeupAddrSize) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    if (connect(wakeupSocket, reinterpret_cast<const sockaddr*>(&wakeupAddr), sizeof(wakeupAddr)) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(wakeupSocket);
}

// anchor: launcher.exe:0x452320 helper shape
static void SignalWakeupSocketHandle(uint32_t socketHandle) {
    if (socketHandle == kInvalidSocketHandle) {
        return;
    }

    (void)send(static_cast<SOCKET>(socketHandle), "", 0, 0);
}

static void DrainWakeupSocketHandleScaffold(uint32_t socketHandle) {
    if (socketHandle == kInvalidSocketHandle) {
        return;
    }

    char dummy[1] = {};
    (void)recv(static_cast<SOCKET>(socketHandle), dummy, sizeof(dummy), 0);
}

// anchor: launcher.exe:0x449b40 helper shape
static uint32_t CreateSocketHandleWithOriginalSetupScaffold(
    int socketType,
    int protocol,
    uint8_t flags) {
    if (!mxo::libltnet::CLTSocketLayer::Init()) {
        return kInvalidSocketHandle;
    }

    SOCKET socketHandle = socket(AF_INET, socketType, protocol);
    if (socketHandle == INVALID_SOCKET) {
        return kInvalidSocketHandle;
    }

    if (protocol == IPPROTO_TCP && (flags & kSocketFactoryFlagSkipDisableNagle) == 0u) {
        BOOL noDelay = TRUE;
        if (setsockopt(
                socketHandle,
                IPPROTO_TCP,
                TCP_NODELAY,
                reinterpret_cast<const char*>(&noDelay),
                sizeof(noDelay)) == SOCKET_ERROR) {
            closesocket(socketHandle);
            return kInvalidSocketHandle;
        }
    }

    if ((flags & kSocketFactoryFlagKeepBlocking) == 0u) {
        u_long nonBlocking = 1;
        if (ioctlsocket(socketHandle, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
            closesocket(socketHandle);
            return kInvalidSocketHandle;
        }
    }

    return static_cast<uint32_t>(socketHandle);
}

// UNANCHORED internal helper used by the current MonitorPort scaffold.
static uint32_t OpenTcpListenSocket(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    const uint32_t listenSocketHandle = CreateSocketHandleWithOriginalSetupScaffold(
        SOCK_STREAM,
        IPPROTO_TCP,
        /*flags=*/0u);
    if (listenSocketHandle == kInvalidSocketHandle) {
        return kInvalidSocketHandle;
    }

    SOCKET listenSocket = static_cast<SOCKET>(listenSocketHandle);

    sockaddr_in listenAddr = {};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(portHostOrder);
    listenAddr.sin_addr.s_addr = ipv4NetworkOrder;

    if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&listenAddr), sizeof(listenAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return kInvalidSocketHandle;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(listenSocket);
}

// UNANCHORED internal helper used by the current UDPMonitorPort scaffold.
static uint32_t OpenUdpMonitorSocket(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    const uint32_t udpSocketHandle = CreateSocketHandleWithOriginalSetupScaffold(
        SOCK_DGRAM,
        IPPROTO_UDP,
        /*flags=*/0u);
    if (udpSocketHandle == kInvalidSocketHandle) {
        return kInvalidSocketHandle;
    }

    SOCKET udpSocket = static_cast<SOCKET>(udpSocketHandle);

    BOOL reuseAddr = TRUE;
    if (setsockopt(
            udpSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuseAddr),
            sizeof(reuseAddr)) == SOCKET_ERROR) {
        closesocket(udpSocket);
        return kInvalidSocketHandle;
    }

    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(portHostOrder);
    bindAddr.sin_addr.s_addr = ipv4NetworkOrder;

    if (bind(udpSocket, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        closesocket(udpSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(udpSocket);
}

// UNANCHORED internal scaffold helper used by Queue_PushPair growth handling.
static bool RecenterQueueSlots(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t additionalBlocks,
    bool biasTowardTail) {
    if (!queue || !queue->slotsBase || !queue->slotsCurrent || !queue->slotsLast) {
        return false;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t activeBlocks = static_cast<uint32_t>((slotsLast - slotsCurrent) + 1);
    const uint32_t neededBlocks = activeBlocks + additionalBlocks;

    uint32_t newCapacity = queue->slotCapacity;
    uint32_t** newSlotsBase = slotsBase;
    if (queue->slotCapacity <= (neededBlocks * 2u)) {
        const uint32_t growthBase = (queue->slotCapacity >= additionalBlocks) ? queue->slotCapacity : additionalBlocks;
        newCapacity = queue->slotCapacity + growthBase + 2u;
        newSlotsBase = static_cast<uint32_t**>(std::calloc(newCapacity, sizeof(uint32_t*)));
        if (!newSlotsBase) {
            return false;
        }
    }

    uint32_t newIndex = (newCapacity - neededBlocks) >> 1;
    if (biasTowardTail) {
        newIndex += additionalBlocks;
    }

    uint32_t** newSlotsCurrent = newSlotsBase + newIndex;
    std::memmove(newSlotsCurrent, slotsCurrent, activeBlocks * sizeof(uint32_t*));

    if (newSlotsBase != slotsBase) {
        std::free(slotsBase);
        queue->slotsBase = newSlotsBase;
        queue->slotCapacity = newCapacity;
    }

    queue->slotsCurrent = newSlotsCurrent;
    queue->block0 = *newSlotsCurrent;
    queue->end0 = queue->block0 ? (static_cast<uint8_t*>(queue->block0) + 0x80) : nullptr;

    uint32_t** newSlotsLast = newSlotsCurrent + activeBlocks - 1;
    queue->slotsLast = newSlotsLast;
    queue->block1 = *newSlotsLast;
    queue->end1 = queue->block1 ? (static_cast<uint8_t*>(queue->block1) + 0x80) : nullptr;
    return true;
}

// UNANCHORED internal scaffold helper used by Queue_PushPair block-growth handling.
static bool GrowQueue(
    CLTThreadPerClientTCPEngine_Queue* queue,
    const CLTThreadPerClientTCPEngine_QueuePair* pendingPair) {
    if (!queue || !queue->slotsLast) {
        return false;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t tailFreeSlots = queue->slotCapacity - static_cast<uint32_t>(slotsLast - slotsBase);
    if (tailFreeSlots < 2u) {
        if (!RecenterQueueSlots(queue, 1, false)) {
            return false;
        }
        slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    }

    uint32_t* newBlock = QueueTakeRecycledBlockScaffold(queue);
    if (!newBlock) {
        newBlock = static_cast<uint32_t*>(std::calloc(1, 0x80));
        if (!newBlock) {
            return false;
        }
    } else {
        std::memset(newBlock, 0, 0x80);
    }

    slotsLast[1] = newBlock;

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    if (current1 && pendingPair) {
        current1[0] = pendingPair->value0;
        current1[1] = pendingPair->value1;
    }

    queue->slotsLast = slotsLast + 1;
    queue->block1 = newBlock;
    queue->end1 = static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80;
    queue->current1 = newBlock;
    return true;
}

}  // namespace

// anchor: launcher.exe:0x4319e0
CLTThread::CLTThread(const char* threadName)
    : threadName_(threadName ? threadName : ""),
      startPriority_(2),
      suspendDepth_(0),
      running_(false),
      threadId_(0),
      threadHandle_(0),
      stateMutex_() {}

// anchor: launcher.exe:0x452950 / 0x431a80 deleting wrapper
CLTThread::~CLTThread() {
    const HANDLE threadHandle = reinterpret_cast<HANDLE>(threadHandle_);
    if (threadHandle != nullptr) {
        CloseHandle(threadHandle);
        threadHandle_ = 0;
    }
}

// anchor: launcher.exe:0x4319d0
const std::string& CLTThread::GetNameString() const {
    return threadName_;
}

// anchor: launcher.exe:0x4528d0
uint32_t CLTThread::Start(int startPriority) {
    const auto mapThreadPriority = [](int priority) -> int {
        switch (priority) {
            case 0:
                return THREAD_PRIORITY_BELOW_NORMAL;
            case 1:
                return THREAD_PRIORITY_LOWEST;
            case 3:
                return THREAD_PRIORITY_ABOVE_NORMAL;
            case 4:
                return THREAD_PRIORITY_HIGHEST;
            default:
                return THREAD_PRIORITY_NORMAL;
        }
    };

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (threadHandle_ != 0) {
        const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0);
        if (waitResult == WAIT_TIMEOUT) {
            return kStartAlreadyRunning;
        }
        CloseHandle(reinterpret_cast<HANDLE>(threadHandle_));
        threadHandle_ = 0;
        threadId_ = 0;
        running_ = false;
    }

    unsigned threadId = 0;
    const uintptr_t threadHandle = _beginthreadex(
        nullptr,
        0x10000,
        &CLTThread::ThreadStartAddressScaffold,
        this,
        CREATE_SUSPENDED,
        &threadId);
    threadHandle_ = threadHandle;
    if (threadHandle_ == 0) {
        return kStartFailure;
    }

    startPriority_ = startPriority;
    suspendDepth_ = 0;
    running_ = true;
    threadId_ = static_cast<uint32_t>(threadId);

    if (startPriority != 2) {
        SetThreadPriority(reinterpret_cast<HANDLE>(threadHandle_), mapThreadPriority(startPriority));
    }

    const DWORD resumeResult = ResumeThread(reinterpret_cast<HANDLE>(threadHandle_));
    if (resumeResult == 0xffffffffu) {
        CloseHandle(reinterpret_cast<HANDLE>(threadHandle_));
        threadHandle_ = 0;
        threadId_ = 0;
        running_ = false;
        return kStartFailure;
    }
    return kStartSuccess;
}

// anchor: launcher.exe:0x4525d0
bool CLTThread::Resume() {
    uintptr_t threadHandle = 0;
    bool isCurrentThread = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
        if (threadHandle == 0) {
            return false;
        }

        isCurrentThread = (threadId_ != 0 && threadId_ == GetCurrentThreadId());
        if (isCurrentThread) {
            ++suspendDepth_;
        }
    }

    if (isCurrentThread) {
        return false;
    }

    const DWORD resumeResult = ResumeThread(reinterpret_cast<HANDLE>(threadHandle));
    return resumeResult != 0xffffffffu;
}

// anchor: launcher.exe:0x452660
int CLTThread::Stop(bool waitAfterTerminate) {
    uintptr_t threadHandle = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
    }

    if (threadHandle == 0) {
        return 0;
    }

    if (IsCurrentThread()) {
        ExitThread(0);
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0);
        if (waitResult != WAIT_TIMEOUT) {
            running_ = false;
            return 0;
        }
    }

    (void)TerminateThread(reinterpret_cast<HANDLE>(threadHandle), 0);
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
        threadId_ = 0;
    }

    if (waitAfterTerminate) {
        (void)Wait();
    }
    return 0;
}

// anchor: launcher.exe:0x431a60
bool CLTThread::IsRunning() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (threadHandle_ == 0) {
        return false;
    }
    return WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0) == WAIT_TIMEOUT;
}

// anchor: launcher.exe:0x4526e0
uint32_t CLTThread::Wait() {
    uintptr_t threadHandle = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
    }

    if (threadHandle == 0) {
        return WAIT_FAILED;
    }

    const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle), INFINITE);
    if (waitResult != WAIT_TIMEOUT) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
    }
    return waitResult;
}

// anchor: launcher.exe:0x452620
void CLTThread::Suspend() {
    uintptr_t threadHandle = 0;
    int suspendDepth = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
        suspendDepth = suspendDepth_;
        if (suspendDepth_ > 0) {
            --suspendDepth_;
        }
    }

    if (threadHandle == 0 || suspendDepth > 0) {
        return;
    }

    (void)SuspendThread(reinterpret_cast<HANDLE>(threadHandle));
}

// anchor: launcher.exe:0x431a40
bool CLTThread::IsCurrentThread() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return threadId_ != 0 && threadId_ == GetCurrentThreadId();
}

// anchor: launcher.exe:0x437b50 on the current shared base vtable family
uint32_t CLTThread::PreRun() {
    return 0;
}

// UNANCHORED scaffold base default; concrete derived thread classes override this slot
void CLTThread::Run() {}

// anchor: launcher.exe:0x452770
void CLTThread::LogExit() {
    if (!threadName_.empty()) {
        spdlog::debug("{} exiting.", threadName_);
    }
}

// UNANCHORED: source-owned wrapper mirroring `_StartAddress_00452800` thread-entry sequencing.
uint32_t CLTThread::ExecuteThreadMainScaffold() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadId_ = GetCurrentThreadId();
        running_ = true;
    }

    if (!threadName_.empty()) {
        spdlog::debug("I'm a {}.", threadName_);
    }

    (void)PreRun();
    Run();
    LogExit();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
        threadId_ = 0;
    }
    return 0;
}

// UNANCHORED: source-owned `_beginthreadex` entry thunk for ExecuteThreadMainScaffold.
unsigned __stdcall CLTThread::ThreadStartAddressScaffold(void* parameter) {
    CLTThread* self = static_cast<CLTThread*>(parameter);
    return self ? self->ExecuteThreadMainScaffold() : 0u;
}

// anchor: launcher.exe:0x4365a0
CLTThreadPerClientTCPEngine_QueueThread::CLTThreadPerClientTCPEngine_QueueThread(
    CLTThreadPerClientTCPEngine* owner)
    : CLTThread("ILTTCPEngine::QueueThread"),
      owner_(owner) {}

// UNANCHORED: current vtable family keeps the shared CLTThread deleting dtor at slot +0x2c
CLTThreadPerClientTCPEngine_QueueThread::~CLTThreadPerClientTCPEngine_QueueThread() = default;

// UNANCHORED: scaffold accessor for the recovered child +0x38 owner field
CLTThreadPerClientTCPEngine* CLTThreadPerClientTCPEngine_QueueThread::Owner() const {
    return owner_;
}

// anchor: launcher.exe:0x436fc0
void CLTThreadPerClientTCPEngine_QueueThread::Run() {
    if (owner_) {
        owner_->RunCompletedOperationQueue(/*nonBlocking=*/false);
    }
}

// anchor: launcher.exe:0x431ab0
CLTThreadPerClientTCPEngine_AcceptThread::CLTThreadPerClientTCPEngine_AcceptThread(
    uint32_t listenSocketHandle,
    void* ownerContext)
    : CLTThread("CLTThreadPerClientTCPEngine::AcceptThread"),
      ownerContext_(ownerContext),
      listenSocketHandle_(listenSocketHandle),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()) {}

// anchor: launcher.exe:0x431b30 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_AcceptThread::~CLTThreadPerClientTCPEngine_AcceptThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 owner/context field
void* CLTThreadPerClientTCPEngine_AcceptThread::OwnerContext() const {
    return ownerContext_;
}

// UNANCHORED: scaffold accessor for recovered child +0x3c listening socket field
uint32_t CLTThreadPerClientTCPEngine_AcceptThread::ListenSocketHandle() const {
    return listenSocketHandle_;
}

// UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
uint32_t CLTThreadPerClientTCPEngine_AcceptThread::WakeupSocketHandle() const {
    return wakeupSocketHandle_;
}

// UNANCHORED: source-owned bridge for the original external closesocket([payload+0x3c]) seam.
void CLTThreadPerClientTCPEngine_AcceptThread::CloseListenSocketScaffold() {
    CloseSocketHandle(&listenSocketHandle_);
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_AcceptThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x432070
void CLTThreadPerClientTCPEngine_AcceptThread::Run() {
    // Current source ownership only models the class/vtable/wakeup surface.
    // The full accept loop remains a later fidelity target.
}

// anchor: launcher.exe:0x431b60
CLTThreadPerClientTCPEngine_WorkerThread::CLTThreadPerClientTCPEngine_WorkerThread(
    void* contextKey,
    bool datagramMode)
    : CLTThread("CLTThreadPerClientTCPEngine::WorkerThread"),
      contextKey_(contextKey),
      datagramMode_(datagramMode),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()),
      exitRequested_(false) {}

// anchor: launcher.exe:0x431be0 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_WorkerThread::~CLTThreadPerClientTCPEngine_WorkerThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 context/connection key field
void* CLTThreadPerClientTCPEngine_WorkerThread::ContextKey() const {
    return contextKey_;
}

// UNANCHORED: scaffold accessor for recovered child +0x3c datagram-mode byte
bool CLTThreadPerClientTCPEngine_WorkerThread::DatagramMode() const {
    return datagramMode_;
}

// UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
uint32_t CLTThreadPerClientTCPEngine_WorkerThread::WakeupSocketHandle() const {
    return wakeupSocketHandle_;
}

// UNANCHORED: scaffold accessor for recovered child +0x44 exit-request byte
bool CLTThreadPerClientTCPEngine_WorkerThread::ExitRequested() const {
    return exitRequested_;
}

// UNANCHORED: source-owned bridge for the recovered child +0x44 exit-request byte
void CLTThreadPerClientTCPEngine_WorkerThread::RequestExit() {
    exitRequested_ = true;
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_WorkerThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x42fe50
void CLTThreadPerClientTCPEngine_WorkerThread::Run() {
    CMessageConnection* connection = static_cast<CMessageConnection*>(contextKey_);
    if (!connection) {
        return;
    }

    CLTThreadPerClientTCPEngine* engine = connection->Engine();
    if (!engine || wakeupSocketHandle_ == kInvalidSocketHandle) {
        return;
    }

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* launcherContext =
        ResolveLauncherBridgeContextForConnectionScaffold(connection);
    const char* connectStatusLabel =
        (launcherContext && launcherContext->isMarginConnection) ? "MarginConnectStatus"
                                                                 : "AuthConnectStatus";
    const char* closeStatusLabel =
        (launcherContext && launcherContext->isMarginConnection) ? "MarginPeerClosed"
                                                                 : "AuthPeerClosed";
    spdlog::info(
        "CLTThreadPerClientTCPEngine::WorkerThread Run connection={} ownerContext={} launcherContext={} isMargin={} wakeupSocket=0x{:08x} initialState={} connectCompletionPending={} remoteHost='{}'",
        fmt::ptr(connection),
        fmt::ptr(connection->OwnerContext()),
        fmt::ptr(launcherContext),
        (launcherContext && launcherContext->isMarginConnection) ? 1u : 0u,
        wakeupSocketHandle_,
        static_cast<unsigned>(connection->State()),
        !datagramMode_ ? 1u : 0u,
        connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName());

    // Tightened `0x42fe50` read/write/except/wakeup state:
    // - blocking select with no timeout
    // - sets are rebuilt each iteration as:
    //   1) read  = connection socket + wakeup socket
    //   2) except = connection socket
    //   3) write = connection socket only while connect completion is pending or a queued send is
    //      already being drained
    // - post-select work order is:
    //   1) except-on-socket (connect phase only)
    //   2) readable socket drain
    //   3) writable socket connect/send handling
    //   4) wakeup-socket drain / exit-by-request
    bool connectCompletionPending = !datagramMode_;
    bool connectStatusQueued = false;
    bool closeQueued = false;
    bool waitWakeupOnly = false;
    CLTTCPConnection_SendQueueItemScaffold currentSend = {};
    size_t currentSendOffset = 0u;

    const auto queueConnectStatus =
        [&](uint32_t workPayload) {
            if (connectStatusQueued) {
                return;
            }
            const bool queued = launcherContext != nullptr
                ? engine->EnqueueLauncherConnectionStatusWorkItemScaffold(
                      launcherContext,
                      CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus,
                      workPayload,
                      connectStatusLabel)
                : engine->EnqueueDirectConnectionStatusWorkItemScaffold(
                      connection,
                      CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus,
                      workPayload,
                      connectStatusLabel,
                      /*queueLockAlreadyHeld=*/false);
            if (!queued && launcherContext == nullptr) {
                spdlog::warn(
                    "CLTThreadPerClientTCPEngine::WorkerThread direct queue connect status failed connection={} ownerContext={} payload=0x{:08x}",
                    fmt::ptr(connection),
                    fmt::ptr(connection->OwnerContext()),
                    static_cast<unsigned>(workPayload));
            }
            connectStatusQueued = queued;
        };

    const auto queueClose = [&]() {
        if (closeQueued) {
            return;
        }
        if (launcherContext != nullptr) {
            launcherContext->peerCloseQueued = true;
        }
        const bool queued = launcherContext != nullptr
            ? engine->EnqueueLauncherConnectionStatusWorkItemScaffold(
                  launcherContext,
                  CLTThreadPerClientTCPEngine::kWorkTypeClose,
                  0u,
                  closeStatusLabel)
            : engine->EnqueueDirectConnectionStatusWorkItemScaffold(
                  connection,
                  CLTThreadPerClientTCPEngine::kWorkTypeClose,
                  0u,
                  closeStatusLabel,
                  /*queueLockAlreadyHeld=*/false);
        if (!queued && launcherContext == nullptr) {
            spdlog::warn(
                "CLTThreadPerClientTCPEngine::WorkerThread direct queue close failed connection={} ownerContext={} state={}",
                fmt::ptr(connection),
                fmt::ptr(connection->OwnerContext()),
                static_cast<unsigned>(connection->State()));
        }
        closeQueued = queued;
    };

    const auto closeAndInvalidateSocket = [&]() {
        const uint32_t socketHandle = connection->SocketHandle();
        if (socketHandle != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandle));
            connection->SetSocketHandle(kInvalidSocketHandle);
        }
    };

    const auto issueDeferredShutdownIfClosing = [&](SOCKET socket) {
        if (connection->State() == LTTCPEngineConnectionState::kClosing &&
            connection->SendQueueEmptyFlagScaffold()) {
            (void)shutdown(socket, SD_SEND);
        }
    };

    while (true) {
        if (waitWakeupOnly) {
            fd_set wakeupReadSet;
            FD_ZERO(&wakeupReadSet);
            const SOCKET wakeupSocket = static_cast<SOCKET>(wakeupSocketHandle_);
            FD_SET(wakeupSocket, &wakeupReadSet);

            const int readyCount = select(
                static_cast<int>(wakeupSocket + 1),
                &wakeupReadSet,
                nullptr,
                nullptr,
                nullptr);
            if (readyCount == SOCKET_ERROR) {
                continue;
            }
            if (FD_ISSET(wakeupSocket, &wakeupReadSet)) {
                DrainWakeupSocketHandleScaffold(wakeupSocketHandle_);
                if (exitRequested_) {
                    break;
                }
            }
            continue;
        }

        const uint32_t socketHandle = connection->SocketHandle();
        if (socketHandle == kInvalidSocketHandle) {
            if (launcherContext && (closeQueued || connectStatusQueued || connection->State() == LTTCPEngineConnectionState::kClosed)) {
                waitWakeupOnly = true;
                continue;
            }
            break;
        }

        const SOCKET socket = static_cast<SOCKET>(socketHandle);
        const SOCKET wakeupSocket = static_cast<SOCKET>(wakeupSocketHandle_);
        fd_set readSet;
        fd_set writeSet;
        fd_set exceptSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);

        FD_SET(socket, &readSet);
        FD_SET(wakeupSocket, &readSet);
        FD_SET(socket, &exceptSet);

        const bool hasCurrentSend =
            currentSendOffset < currentSend.ownedBytes.size();
        bool monitorWrite = false;
        if (connectCompletionPending) {
            monitorWrite = true;
        } else if (hasCurrentSend) {
            monitorWrite = true;
        } else {
            if (connection->TryPopQueuedSendBufferScaffold(&currentSend)) {
                currentSendOffset = 0u;
                monitorWrite = !currentSend.ownedBytes.empty();
            } else {
                issueDeferredShutdownIfClosing(socket);
            }
        }
        if (monitorWrite) {
            FD_SET(socket, &writeSet);
        }

        const int readyCount = select(
            static_cast<int>((socket > wakeupSocket ? socket : wakeupSocket) + 1),
            &readSet,
            &writeSet,
            &exceptSet,
            nullptr);
        if (readyCount == SOCKET_ERROR) {
            const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
            if (wsaError == WSAENOTSOCK || wsaError == WSAEBADF) {
                waitWakeupOnly = launcherContext != nullptr;
                if (!waitWakeupOnly) {
                    break;
                }
                continue;
            }

            spdlog::debug(
                "CLTThreadPerClientTCPEngine::WorkerThread select failed connection={} socket=0x{:08x} wsaError={} remoteHost='{}'",
                fmt::ptr(connection),
                socketHandle,
                wsaError,
                connection->RemoteHostName().empty() ? std::string("<empty>")
                                                     : connection->RemoteHostName());
            continue;
        }

        if (connectCompletionPending && FD_ISSET(socket, &exceptSet)) {
            int soError = 0;
            int soErrorSize = sizeof(soError);
            (void)getsockopt(
                socket,
                SOL_SOCKET,
                SO_ERROR,
                reinterpret_cast<char*>(&soError),
                &soErrorSize);
            connection->SetState(LTTCPEngineConnectionState::kClosed);
            queueConnectStatus(static_cast<uint32_t>(soError != 0 ? soError : 1u));
            queueClose();
            waitWakeupOnly = launcherContext != nullptr;
            if (!waitWakeupOnly) {
                break;
            }
            continue;
        }

        if (FD_ISSET(socket, &readSet)) {
            while (true) {
                uint32_t wsaError = 0u;
                bool peerClosed = false;
                const int receiveResult =
                    connection->ReceiveReadyReadOperationFragmentScaffold(&wsaError, &peerClosed);
                if (receiveResult > 0) {
                    continue;
                }
                if (receiveResult == 0) {
                    break;
                }

                spdlog::debug(
                    "CLTThreadPerClientTCPEngine::WorkerThread terminal recv connection={} socket=0x{:08x} peerClosed={} wsaError={} remoteHost='{}'",
                    fmt::ptr(connection),
                    socketHandle,
                    peerClosed ? 1u : 0u,
                    wsaError,
                    connection->RemoteHostName().empty() ? std::string("<empty>")
                                                         : connection->RemoteHostName());
                closeAndInvalidateSocket();
                connection->SetState(LTTCPEngineConnectionState::kClosed);
                queueClose();
                waitWakeupOnly = launcherContext != nullptr;
                if (!waitWakeupOnly) {
                    goto worker_exit;
                }
                goto worker_continue;
            }
        }

        if (FD_ISSET(socket, &writeSet)) {
            if (connectCompletionPending) {
                int soError = 0;
                int soErrorSize = sizeof(soError);
                const int getSockOptResult = getsockopt(
                    socket,
                    SOL_SOCKET,
                    SO_ERROR,
                    reinterpret_cast<char*>(&soError),
                    &soErrorSize);
                if (getSockOptResult == 0 && soError == 0) {
                    connection->SetState(LTTCPEngineConnectionState::kUdpMonitorActive);
                    queueConnectStatus(0u);
                    connectCompletionPending = false;
                } else {
                    connection->SetState(LTTCPEngineConnectionState::kClosed);
                    queueConnectStatus(static_cast<uint32_t>(soError != 0 ? soError : 1u));
                    queueClose();
                    waitWakeupOnly = launcherContext != nullptr;
                    if (!waitWakeupOnly) {
                        break;
                    }
                    continue;
                }
            } else if (currentSendOffset < currentSend.ownedBytes.size()) {
                const int remainingByteCount =
                    static_cast<int>(currentSend.ownedBytes.size() - currentSendOffset);
                const int sentByteCount = send(
                    socket,
                    reinterpret_cast<const char*>(currentSend.ownedBytes.data() + currentSendOffset),
                    remainingByteCount,
                    0);
                if (sentByteCount != SOCKET_ERROR) {
                    currentSendOffset += static_cast<size_t>(sentByteCount);
                    if (currentSendOffset >= currentSend.ownedBytes.size()) {
                        currentSend = {};
                        currentSendOffset = 0u;
                    }
                } else {
                    const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
                    if (wsaError != WSAEWOULDBLOCK) {
                        spdlog::debug(
                            "CLTThreadPerClientTCPEngine::WorkerThread send failed connection={} socket=0x{:08x} wsaError={} remaining={} remoteHost='{}'",
                            fmt::ptr(connection),
                            socketHandle,
                            wsaError,
                            remainingByteCount,
                            connection->RemoteHostName().empty() ? std::string("<empty>")
                                                                 : connection->RemoteHostName());
                    }
                }
            }
        }

        if (FD_ISSET(wakeupSocket, &readSet)) {
            DrainWakeupSocketHandleScaffold(wakeupSocketHandle_);
            if (exitRequested_) {
                break;
            }
        }

worker_continue:
        continue;
    }

worker_exit:
    connection->SetWorkerThreadScaffold(nullptr);
}

// anchor: launcher.exe:0x436340
void CLTThreadPerClientTCPEngine::Queue_Free(CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return;
    }

    if (queue->slotsBase) {
        uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
        uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
        if (slotsCurrent && slotsLast && slotsCurrent <= slotsLast) {
            for (uint32_t** slot = slotsCurrent; slot <= slotsLast; ++slot) {
                if (*slot) {
                    std::free(*slot);
                    *slot = nullptr;
                }
            }
        }
        std::free(slotsBase);
    }

    QueueFreeRecycledBlocksScaffold(queue);
    std::memset(queue, 0, sizeof(*queue));
}

// anchor: launcher.exe:0x436340
bool CLTThreadPerClientTCPEngine::Queue_Init(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t initialSize) {
    if (!queue) {
        return false;
    }

    Queue_Free(queue);

    uint32_t blockCount = (initialSize >> 4) + 1;
    uint32_t slotCapacity = blockCount + 2;
    if (slotCapacity < 8) {
        slotCapacity = 8;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(std::calloc(slotCapacity, sizeof(uint32_t*)));
    if (!slotsBase) {
        return false;
    }

    const uint32_t firstIndex = (slotCapacity - blockCount) >> 1;
    for (uint32_t i = 0; i < blockCount; ++i) {
        slotsBase[firstIndex + i] = static_cast<uint32_t*>(std::calloc(1, 0x80));
        if (!slotsBase[firstIndex + i]) {
            queue->slotsBase = slotsBase;
            queue->slotsCurrent = slotsBase + firstIndex;
            queue->slotsLast = slotsBase + firstIndex + i;
            Queue_Free(queue);
            return false;
        }
    }

    uint32_t** slotsCurrent = slotsBase + firstIndex;
    uint32_t** slotsLast = slotsCurrent + blockCount - 1;
    uint8_t* block0 = reinterpret_cast<uint8_t*>(*slotsCurrent);
    uint8_t* block1 = reinterpret_cast<uint8_t*>(*slotsLast);

    queue->slotsBase = slotsBase;
    queue->slotCapacity = slotCapacity;
    queue->slotsCurrent = slotsCurrent;
    queue->slotsLast = slotsLast;
    queue->block0 = block0;
    queue->end0 = block0 ? (block0 + 0x80) : nullptr;
    queue->current0 = block0;
    queue->block1 = block1;
    queue->end1 = block1 ? (block1 + 0x80) : nullptr;
    queue->current1 = block1 ? (block1 + ((initialSize & 0xfu) * 8u)) : nullptr;
    return true;
}

// anchor: launcher.exe:0x436670 selected-queue push body reached from `0x436820`
void CLTThreadPerClientTCPEngine::Queue_PushPair(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t value0,
    uint32_t value1) {
    if (!queue || !queue->current1) {
        return;
    }

    uint8_t* lastPairInBlock = queue->end1 ? (static_cast<uint8_t*>(queue->end1) - 8) : nullptr;
    if (static_cast<void*>(queue->current1) == static_cast<void*>(lastPairInBlock)) {
        CLTThreadPerClientTCPEngine_QueuePair pair = {value0, value1};
        (void)GrowQueue(queue, &pair);
        return;
    }

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    current1[0] = value0;
    current1[1] = value1;
    queue->current1 = current1 + 2;
}

// anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
bool CLTThreadPerClientTCPEngine::Queue_IsEmpty(const CLTThreadPerClientTCPEngine_Queue* queue) {
    return !queue || !queue->current0 || (queue->current1 == queue->current0);
}

// anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
bool CLTThreadPerClientTCPEngine::Queue_TryPopPair(
    CLTThreadPerClientTCPEngine_Queue* queue,
    CLTThreadPerClientTCPEngine_QueuedPair* outPair) {
    if (!queue || !outPair || Queue_IsEmpty(queue)) {
        return false;
    }

    uint32_t* current0 = static_cast<uint32_t*>(queue->current0);
    outPair->value0 = current0[0];
    outPair->value1 = current0[1];

    uint8_t* lastPairInBlock = queue->end0 ? (static_cast<uint8_t*>(queue->end0) - 8) : nullptr;
    if (static_cast<void*>(queue->current0) == static_cast<void*>(lastPairInBlock)) {
        uint32_t* oldBlock = static_cast<uint32_t*>(queue->block0);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
        uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
        if (!slotsCurrent || slotsCurrent >= slotsLast) {
            // No later block is active; the queue just becomes empty within the current block.
            // The recovered free-list behavior matters on the cross-block path below, not here.
            queue->current0 = current0 + 2;
            return true;
        }

        if (oldBlock) {
            // Current bounded fidelity step:
            // - original `0x436d31..0x436ee7` uses block recycling / free-list behavior when the
            //   dequeue cursor leaves a full `0x80` block
            // - current source now mirrors that more closely by caching the exhausted head block for
            //   later `Queue_PushPair` growth reuse instead of freeing it immediately
            QueueRecycleBlockScaffold(queue, oldBlock);
        }
        ++slotsCurrent;
        queue->slotsCurrent = slotsCurrent;
        uint32_t* newBlock = *slotsCurrent;
        queue->block0 = newBlock;
        queue->end0 = newBlock ? (static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80) : nullptr;
        queue->current0 = newBlock;
        return true;
    }

    queue->current0 = current0 + 2;
    return true;
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best anchor for the type read itself is launcher helper 0x4816f0,
// which returns [workItem+0x04]. We model that as a documented header view rather than
// open-coded pointer arithmetic.
static uint32_t QueueWorkItem_GetType(const void* workItem) {
    if (!workItem) {
        return 0;
    }
    const CLTThreadPerClientTCPEngine_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the trailing work-item release in 0x436d31..0x436ee7.
static void QueueWorkItem_Release(void* workItem) {
    if (!workItem) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(workItem);
    if (!vtable || !vtable[1]) {
        return;
    }

    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = reinterpret_cast<ReleaseFn>(vtable[1]);
    (void)fn(workItem);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is context->+0x10(workItem) in 0x436d31..0x436ee7.
static void QueueContext_OnOperationCompleted(void* context, void* workItem) {
    if (!context) {
        return;
    }

    // Current recovered producer set now reaches the queue consumer with either:
    // - the direct connection object (`context=this`) on the original type-1/type-2/type-3 paths
    // - the source-owned `CBaseConnection_QueueContextScaffold` bridge on older/fallback paths
    CBaseConnection* completionTarget = CBaseConnection_FromQueueContextScaffold(context);
    const bool usedQueueContextBridge = (completionTarget != nullptr);
    if (completionTarget == nullptr) {
        completionTarget = static_cast<CBaseConnection*>(context);
    }

    const uint32_t workType = QueueWorkItem_GetType(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        void* targetVtable = nullptr;
        if (completionTarget != nullptr) {
            targetVtable = *reinterpret_cast<void**>(completionTarget);
        }
        CLTTCPConnection* tcpTarget = dynamic_cast<CLTTCPConnection*>(completionTarget);
        spdlog::info(
            "QueueContext_OnOperationCompleted close work context={} completionTarget={} targetVtable={} usedQueueContextBridge={} ownerContext={} state={}",
            fmt::ptr(context),
            fmt::ptr(completionTarget),
            fmt::ptr(targetVtable),
            usedQueueContextBridge ? 1u : 0u,
            fmt::ptr(tcpTarget ? tcpTarget->OwnerContext() : nullptr),
            completionTarget ? static_cast<unsigned>(completionTarget->State()) : 0u);
    }

    const uint32_t handled = completionTarget->OnOperationCompleted(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        spdlog::info(
            "QueueContext_OnOperationCompleted close work result completionTarget={} handled=0x{:08x}",
            fmt::ptr(completionTarget),
            handled);
    }
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the conditional context->+0x04 release after type-1 work.
// Only the explicit source-owned queue-context bridge participates in that compatibility release.
static void QueueContext_Release(void* context) {
    CBaseConnection_QueueContextScaffold* queueContext =
        static_cast<CBaseConnection_QueueContextScaffold*>(context);
    if (!queueContext || CBaseConnection_FromQueueContextScaffold(queueContext) == nullptr) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(queueContext);
    if (!vtable || !vtable[1]) {
        return;
    }

    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = reinterpret_cast<ReleaseFn>(vtable[1]);
    (void)fn(queueContext);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the `(char)context[1]` test in 0x436d31..0x436ee7.
// The direct connection object path now reaches the queue consumer directly, so only the explicit
// queue-context bridge keeps a source-owned `autoReleaseFlag` here.
static bool QueueContext_ShouldAutoReleaseAfterType1(void* context) {
    CBaseConnection_QueueContextScaffold* queueContext =
        static_cast<CBaseConnection_QueueContextScaffold*>(context);
    return queueContext != nullptr &&
        CBaseConnection_FromQueueContextScaffold(context) != nullptr &&
        queueContext->autoReleaseFlag != 0u;
}

// Keep the implementation intentionally conservative.
// These methods currently provide original-name structure and evidence-backed state
// shaping, but they are not yet the fully faithful runtime path used by arg5.

// anchor: launcher.exe:0x431c30
// vtable: launcher.exe:0x004b2768
// current constructor-layout anchors that should stay aligned with source comments/docs:
// - base ctor `0x4366f0`
// - queue-pair init `0x436610 -> 0x436340(size=0)` covering the `+0x0c` / `+0x34` pair
// - base helper roots at `+0x5c` / `+0x60`
// - CreateEventA result at `+0x7c`
// - derived list heads at `+0x80` (0x24 bytes) and `+0x8c` (0x18 bytes)
// - derived helper root at `+0x98`
// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::InitializeLockHelperScaffold(
    CLTThreadPerClientTCPEngine_LockHelperScaffold* helper) {
    if (!helper) {
        return;
    }

    std::memset(&helper->crit, 0, sizeof(helper->crit));
    InitializeCriticalSection(&helper->crit);
}

void CLTThreadPerClientTCPEngine::DeleteLockHelperScaffold(
    CLTThreadPerClientTCPEngine_LockHelperScaffold* helper) {
    if (!helper) {
        return;
    }

    DeleteCriticalSection(&helper->crit);
}

void CLTThreadPerClientTCPEngine::InitializeEndpointTreeHead24(
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* head) {
    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    if (!header) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    header->_M_color = std::_S_red;
    header->_M_parent = nullptr;
    header->_M_left = header;
    header->_M_right = header;
}

void CLTThreadPerClientTCPEngine::InitializeContextTreeHead18(
    CLTThreadPerClientTCPEngine_ContextTreeHead18* head) {
    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    if (!header) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    header->_M_color = std::_S_red;
    header->_M_parent = nullptr;
    header->_M_left = header;
    header->_M_right = header;
}

CLTThreadPerClientTCPEngine::CLTThreadPerClientTCPEngine()
    : ctorFlagsField04_(0),
      queueThreadArrayField08_(nullptr),
      ownedQueue0C_(),
      ownedQueue34_(),
      ownedWaitHelper5C_{nullptr},
      ownedQueueLockHelper60_(),
      ownedQueueSignalEvent7C_(NULL),
      ownedEndpointTreeHead80_(nullptr),
      ownedEndpointCount84_(0),
      reserved88_(0),
      ownedContextTreeHead8C_(nullptr),
      ownedContextCount90_(0),
      reserved94_(0),
      ownedCleanupLockHelper98_() {
    // anchor: launcher.exe:0x4366f0
    // Faithfulness restructuring:
    // - keep the real recovered arg5 fields on the object body itself
    // - keep only source-only launcher-shell attachment + generic direct-connection bookkeeping in
    //   discrete engine-keyed maps instead of a synthetic per-engine side-state record
    // - keep launcher bridge contexts as mediator-owned sidecars resolved from the connection's
    //   direct owner pointer rather than inventing auth/margin-specific engine maps
    // - keep recovered payload families (`+0x08`, `+0x80/+0x84`, `+0x8c/+0x90`) in dedicated
    //   source backings keyed by `this`, not as pretend hidden object fields
    ownedQueueLockHelper60_.vtable = nullptr;
    ownedCleanupLockHelper98_.vtable = nullptr;
    Queue_Init(&ownedQueue0C_, 0);
    Queue_Init(&ownedQueue34_, 0);
    ownedQueueSignalEvent7C_ = CreateEventA(NULL, FALSE, FALSE, NULL);
    InitializeLockHelperScaffold(&ownedQueueLockHelper60_);
    InitializeLockHelperScaffold(&ownedCleanupLockHelper98_);

    ownedEndpointTreeHead80_ =
        static_cast<CLTThreadPerClientTCPEngine_EndpointTreeHead24*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_EndpointTreeHead24)));
    if (ownedEndpointTreeHead80_) {
        InitializeEndpointTreeHead24(ownedEndpointTreeHead80_);
    }

    ownedContextTreeHead8C_ =
        static_cast<CLTThreadPerClientTCPEngine_ContextTreeHead18*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_ContextTreeHead18)));
    if (ownedContextTreeHead8C_) {
        InitializeContextTreeHead18(ownedContextTreeHead8C_);
    }

    RefreshOwnedLauncherMirrorStateScaffold();

    // Original base ctor 0x4366f0 allocates queue-thread children only when the effective
    // ctor flag/count at +0x04 is non-zero. The current scaffold still enters through a
    // zero-count binder path, so keep the recovered child family in source but default it empty.
    RebuildQueueThreadsForCtorCount(/*queueThreadCount=*/0);
}

// anchor: launcher.exe:0x40b389..0x40b404 teardown releases arg5 through vtable slot 0
// vtable: launcher.exe:0x004b2768
// NOTE: starter C++ destructor only models local sidecar cleanup, not the full original dtor body.
CLTThreadPerClientTCPEngine::~CLTThreadPerClientTCPEngine() {
    DetachLauncherAbiSurfaceScaffold();
    RebuildQueueThreadsForCtorCount(/*queueThreadCount=*/0);

    if (CLTThreadPerClientTCPEngine_EndpointPayloadBacking* endpointBacking =
            FindEngineEndpointPayloadBacking(this)) {
        for (auto& it : endpointBacking->entries) {
            StopAcceptThreadScaffold(it.second->payload.get());
        }
        endpointBacking->entries.clear();
    }

    if (CLTThreadPerClientTCPEngine_ContextPayloadBacking* contextBacking =
            FindEngineContextPayloadBacking(this)) {
        for (auto& it : contextBacking->entries) {
            StopWorkerThreadScaffold(it.second->payload.get());
        }
        contextBacking->entries.clear();
    }

    Queue_Free(&ownedQueue0C_);
    Queue_Free(&ownedQueue34_);
    DeleteLockHelperScaffold(&ownedQueueLockHelper60_);
    DeleteLockHelperScaffold(&ownedCleanupLockHelper98_);
    if (ownedQueueSignalEvent7C_) {
        CloseHandle(ownedQueueSignalEvent7C_);
        ownedQueueSignalEvent7C_ = NULL;
    }
    if (ownedEndpointTreeHead80_) {
        std::free(ownedEndpointTreeHead80_);
        ownedEndpointTreeHead80_ = nullptr;
    }
    if (ownedContextTreeHead8C_) {
        std::free(ownedContextTreeHead8C_);
        ownedContextTreeHead8C_ = nullptr;
    }

    EraseEngineBackings(this);
}

// anchor: launcher.exe:0x4319a0
// vtable: launcher.exe:0x004b2768 slot +0x00
int CLTThreadPerClientTCPEngine::Release(uint32_t flags) {
    // Current sidecar owner still handles the real arg5 object lifetime/teardown.
    // Keep the primary-slot surface source-owned here so wrappers can forward through
    // ILTTCPEngine without open-coding placeholder returns.
    (void)flags;
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
uint32_t CLTThreadPerClientTCPEngine::MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) {
    if (!ownerContext) {
        return 4u;
    }

    const uint32_t ipv4NetworkOrder = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reservedArg3));
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    bool inserted = false;
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node = EndpointTreeInsertUniqueNode(
        this,
        ownedEndpointTreeHead80_,
        key,
        &inserted);
    if (!node) {
        return 1u;
    }
    if (!inserted) {
        return kResultAlreadyMonitored;
    }

    const uint32_t listenSocketHandle = OpenTcpListenSocket(portHostOrder, ipv4NetworkOrder);
    if (listenSocketHandle == kInvalidSocketHandle) {
        (void)EndpointTreeEraseNode(this, ownedEndpointTreeHead80_, node);
        return 1u;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> acceptThread =
        std::make_unique<CLTThreadPerClientTCPEngine_AcceptThread>(
            listenSocketHandle,
            ownerContext);
    if (!acceptThread) {
        uint32_t socketHandleToClose = listenSocketHandle;
        CloseSocketHandle(&socketHandleToClose);
        (void)EndpointTreeEraseNode(this, ownedEndpointTreeHead80_, node);
        return 1u;
    }
    if (!EndpointTreeAttachPayload(this, node, std::move(acceptThread))) {
        uint32_t socketHandleToClose = listenSocketHandle;
        CloseSocketHandle(&socketHandleToClose);
        (void)EndpointTreeEraseNode(this, ownedEndpointTreeHead80_, node);
        return 1u;
    }

    if (CLTThreadPerClientTCPEngine_AcceptThread* payload = node->_M_valptr()->second) {
        (void)payload->Start(/*startPriority=*/2);
    }
    SyncAttachedLauncherObjectStateScaffold();
    return 0u;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
uint32_t CLTThreadPerClientTCPEngine::UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ipv4NetworkOrder) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
    if (!connection || connection->State() != LTTCPEngineConnectionState::kClosed) {
        return 1u;
    }

    const uint32_t socketHandle = OpenUdpMonitorSocket(
        portHostOrder,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ipv4NetworkOrder)));
    if (socketHandle == kInvalidSocketHandle) {
        return 1u;
    }

    connection->SetSocketHandle(socketHandle);
    CLTThreadPerClientTCPEngine_WorkerThread* worker = CreateAndInsertWorkerThreadScaffold(
        connection,
        /*datagramMode=*/true,
        /*startThread=*/false);
    if (!worker) {
        uint32_t socketHandleToClose = socketHandle;
        CloseSocketHandle(&socketHandleToClose);
        connection->SetSocketHandle(kInvalidSocketHandle);
        return 1u;
    }

    connection->SetState(LTTCPEngineConnectionState::kUdpMonitorActive);
    (void)worker->Start(/*startPriority=*/2);
    SyncAttachedLauncherObjectStateScaffold();
    return 0u;
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b2768 slot +0x0c
uint32_t CLTThreadPerClientTCPEngine::MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ipv4NetworkOrder) {
    // Current best static read: thin helper around slot 2 / UDPMonitorPort(port=0, ...)
    // followed by getsockname/ntohs to report the chosen local port.
    const uint32_t result = UDPMonitorPort(/*portHostOrder=*/0, contextKey, ipv4NetworkOrder);
    if (result == 0u && outBoundPortHostOrder) {
        *outBoundPortHostOrder = 0;
        if (CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey)) {
            sockaddr_in boundAddr = {};
            int boundAddrSize = sizeof(boundAddr);
            if (connection->SocketHandle() != kInvalidSocketHandle &&
                getsockname(
                    static_cast<SOCKET>(connection->SocketHandle()),
                    reinterpret_cast<sockaddr*>(&boundAddr),
                    &boundAddrSize) == 0) {
                *outBoundPortHostOrder = ntohs(boundAddr.sin_port);
            }
        }
    }
    return result;
}

// anchor: launcher.exe:0x42f7c0
// vtable: launcher.exe:0x004b2768 slot +0x10
uint32_t CLTThreadPerClientTCPEngine::Slot4_42F7C0(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
uint32_t CLTThreadPerClientTCPEngine::UnmonitorPort(uint16_t portHostOrder, void** outOwnerContext, uint32_t ipv4NetworkOrder) {
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node =
        EndpointTreeFindNode(ownedEndpointTreeHead80_, key);
    if (!node || !(node)->_M_valptr()->second) {
        if (outOwnerContext) {
            *outOwnerContext = nullptr;
        }
        return kResultEndpointNotFound;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> acceptThread =
        EndpointTreeDetachPayload(this, node);
    if (outOwnerContext) {
        *outOwnerContext = acceptThread ? acceptThread->OwnerContext() : nullptr;
    }

    (void)EndpointTreeEraseNode(this, ownedEndpointTreeHead80_, node);
    StopAcceptThreadScaffold(acceptThread.get());
    acceptThread.reset();
    SyncAttachedLauncherObjectStateScaffold();
    return 0u;
}

// UNANCHORED source-side helper used by the current connection scaffolding.
// Current original anchor is the lower-level connect family at launcher.exe:0x4328a0.
uint32_t CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, void* contextKey, void* unusedArg3) {
    (void)unusedArg3;

    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
    if (!connection || connection->State() != LTTCPEngineConnectionState::kClosed) {
        spdlog::info(
            "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold rejected connect context={} connection={} state={} remoteHost='{}' port={} ip=0x{:08x}",
            fmt::ptr(contextKey),
            fmt::ptr(connection),
            connection ? static_cast<unsigned>(connection->State()) : 0xffffffffu,
            (connection && !connection->RemoteHostName().empty()) ? connection->RemoteHostName() : std::string("<empty>"),
            static_cast<unsigned>(portHostOrder),
            static_cast<unsigned>(ipv4NetworkOrder));
        return 0u;
    }

    const uint32_t socketHandle = CreateSocketHandleWithOriginalSetupScaffold(
        SOCK_STREAM,
        IPPROTO_TCP,
        /*flags=*/0u);
    if (socketHandle == kInvalidSocketHandle) {
        spdlog::info(
            "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold socket allocation failed connection={} remoteHost='{}' port={} ip=0x{:08x} wsaError={}",
            fmt::ptr(connection),
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
            static_cast<unsigned>(portHostOrder),
            static_cast<unsigned>(ipv4NetworkOrder),
            WSAGetLastError());
        return 0u;
    }

    SOCKET sock = static_cast<SOCKET>(socketHandle);

    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(0);
    bindAddr.sin_addr.s_addr = 0;
    if (bind(sock, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        const int wsaError = WSAGetLastError();
        closesocket(sock);
        spdlog::info(
            "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold bind failed connection={} remoteHost='{}' port={} ip=0x{:08x} wsaError={}",
            fmt::ptr(connection),
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
            static_cast<unsigned>(portHostOrder),
            static_cast<unsigned>(ipv4NetworkOrder),
            wsaError);
        return 0u;
    }

    sockaddr_in remoteAddr = {};
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(portHostOrder);
    remoteAddr.sin_addr.s_addr = ipv4NetworkOrder;

    connection->SetSocketHandle(static_cast<uint32_t>(sock));
    if (connect(sock, reinterpret_cast<const sockaddr*>(&remoteAddr), sizeof(remoteAddr)) == SOCKET_ERROR) {
        const int wsaError = WSAGetLastError();
        if (wsaError != WSAEWOULDBLOCK) {
            uint32_t socketHandleToClose = static_cast<uint32_t>(sock);
            CloseSocketHandle(&socketHandleToClose);
            connection->SetSocketHandle(kInvalidSocketHandle);
            spdlog::info(
                "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold connect failed connection={} remoteHost='{}' port={} ip=0x{:08x} wsaError={}",
                fmt::ptr(connection),
                connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
                static_cast<unsigned>(portHostOrder),
                static_cast<unsigned>(ipv4NetworkOrder),
                wsaError);
            return 0u;
        }
    }

    CLTThreadPerClientTCPEngine_WorkerThread* worker = CreateAndInsertWorkerThreadScaffold(
        connection,
        /*datagramMode=*/false,
        /*startThread=*/false);
    if (!worker) {
        uint32_t socketHandleToClose = static_cast<uint32_t>(sock);
        CloseSocketHandle(&socketHandleToClose);
        connection->SetSocketHandle(kInvalidSocketHandle);
        spdlog::info(
            "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold worker creation failed connection={} remoteHost='{}' port={} ip=0x{:08x}",
            fmt::ptr(connection),
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
            static_cast<unsigned>(portHostOrder),
            static_cast<unsigned>(ipv4NetworkOrder));
        return 0u;
    }

    connection->SetState(LTTCPEngineConnectionState::kConnectActive);
    (void)worker->Start(/*startPriority=*/3);
    spdlog::info(
        "CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold started worker connection={} worker={} remoteHost='{}' port={} ip=0x{:08x} state={} ownerContext={}",
        fmt::ptr(connection),
        fmt::ptr(worker),
        connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
        static_cast<unsigned>(portHostOrder),
        static_cast<unsigned>(ipv4NetworkOrder),
        static_cast<unsigned>(connection->State()),
        fmt::ptr(connection->OwnerContext()));
    SyncAttachedLauncherObjectStateScaffold();
    return kResultSuccess;
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::ConnectConnectionScaffold(CLTTCPConnection* connection) {
    if (!connection) {
        return 0;
    }

    const LTTCPEndpointKey& endpoint = connection->RemoteEndpoint();
    const uint16_t portHostOrder =
        static_cast<uint16_t>((endpoint.portNetworkOrder << 8) | (endpoint.portNetworkOrder >> 8));

    uint32_t ipv4NetworkOrder = endpoint.ipv4NetworkOrder;
    if (ipv4NetworkOrder == 0 && !connection->RemoteHostName().empty()) {
        ResolveIpv4Address(connection->RemoteHostName().c_str(), &ipv4NetworkOrder);
    }
    if (ipv4NetworkOrder == 0) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Connect failed to resolve remote host '{}'",
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName());
        return 0;
    }

    return ConnectResolvedEndpointScaffold(
        portHostOrder,
        ipv4NetworkOrder,
        /*contextKey=*/connection,
        /*ownerContext=*/nullptr);
}

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
uint32_t CLTThreadPerClientTCPEngine::Connect(void* contextKey) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Connect couldn't resolve connection context={} normalizedContext={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)));
        return 0u;
    }
    return connection->EnsureConnected();
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::CloseConnectionScaffold(CLTTCPConnection* connection, bool graceful) {
    if (!connection) {
        return 0;
    }

    const LTTCPEngineConnectionState state = connection->State();
    if (state != LTTCPEngineConnectionState::kConnectActive &&
        state != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    connection->SetState(LTTCPEngineConnectionState::kClosing);
    if (graceful) {
        // Tightened slot `7` / `0x42f970` read:
        // - Close first writes connection state `4`
        // - if the recovered connection `+0x38` send-queue-empty byte is non-zero, it immediately
        //   issues `shutdown(socket, 1)`
        // - otherwise the worker-thread write loop defers that same half-close until the queued
        //   send backlog drains
        if (connection->SendQueueEmptyFlagScaffold() &&
            connection->SocketHandle() != kInvalidSocketHandle) {
            (void)shutdown(static_cast<SOCKET>(connection->SocketHandle()), SD_SEND);
        }
        SyncAttachedLauncherObjectStateScaffold();
        return 1u;
    }

    if (connection->SocketHandle() != kInvalidSocketHandle) {
        const int closeResult = closesocket(static_cast<SOCKET>(connection->SocketHandle()));
        if (closeResult == SOCKET_ERROR) {
            spdlog::debug(
                "CLTThreadPerClientTCPEngine::CloseConnectionScaffold closesocket failed connection={} socket=0x{:08x} remoteHost='{}' wsaError={}",
                fmt::ptr(connection),
                connection->SocketHandle(),
                connection->RemoteHostName().empty() ? std::string("<empty>")
                                                     : connection->RemoteHostName(),
                WSAGetLastError());
        }
    }

    SyncAttachedLauncherObjectStateScaffold();
    return 1u;
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
uint32_t CLTThreadPerClientTCPEngine::Close(void* contextKey, bool graceful) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Close couldn't resolve existing connection context={} normalizedContext={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)));
        return 0u;
    }
    return CloseConnectionScaffold(static_cast<CLTTCPConnection*>(connection), graceful);
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::SendBufferConnectionScaffold(CLTTCPConnection* connection, const void* buffer, uint32_t byteCount, void* completionContext) {
    if (!connection || !buffer || byteCount == 0u) {
        return 0;
    }

    if (connection->State() != LTTCPEngineConnectionState::kConnectActive &&
        connection->State() != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    if (CLTThreadPerClientTCPEngine_WorkerThread* worker = connection->WorkerThreadScaffold()) {
        // Tightened slot `8` / `0x42fbd0` read:
        // - enqueue through the connection-owned `+0x3c` send queue first
        // - clear the recovered `+0x38` empty flag on push
        // - then signal the direct worker pointer kept at connection `+0x08`
        const bool queued = connection->QueueSendBufferScaffold(
            buffer,
            byteCount,
            reinterpret_cast<uintptr_t>(completionContext));
        if (!queued) {
            return 0u;
        }
        worker->SignalWakeup();
        return 1u;
    }

    // Source-only fallback for unexpected no-worker paths; active RE-backed paths are expected to
    // have `[connection+0x08]` populated by `0x431ff0` before slot `8` is used.
    return connection->SendRawSocketBufferScaffold(buffer, byteCount, completionContext);
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
uint32_t CLTThreadPerClientTCPEngine::SendBuffer(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::SendBuffer couldn't resolve existing connection context={} normalizedContext={} byteCount={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)),
            byteCount);
        return 0u;
    }
    return connection->SendPacket(buffer, byteCount, completionContext);
}

// anchor: launcher.exe:0x42fd10
// vtable: launcher.exe:0x004b2768 slot +0x24
uint32_t CLTThreadPerClientTCPEngine::Slot9_42FD10(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    return 0;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
uint32_t CLTThreadPerClientTCPEngine::Slot10_443810(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
uint32_t CLTThreadPerClientTCPEngine::Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) {
    (void)arg1;
    if (out0) {
        *out0 = 0;
    }
    if (out1) {
        *out1 = 0;
    }
    return 0;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
uint32_t CLTThreadPerClientTCPEngine::CleanupConnection(void* contextKey) {
    // Current bounded fidelity correction:
    // - original queue consumers dequeue a real connection-family object as `context` before
    //   calling arg5 slot 12 / CleanupConnection
    // - current source now follows that direct-connection shape on the recovered type-1/type-2/
    //   type-3 producers, but still accepts the older `CBaseConnection_QueueContextScaffold`
    //   bridge on fallback paths
    // - slot-12-style worker lookup/teardown therefore still normalizes bridge inputs back to the
    //   owning connection object instead of searching worker/message tables with the bridge
    //   pointer itself
    // - original `0x4316a0` also acquires arg5 helper `+0x98`; after the current ownership move,
    //   that lock behavior now lives here on the target class side and the shell wrapper only
    //   forwards the primary slot call
    // Active retry/deadlock correction:
    // - do not hold the cleanup lock while waiting for a worker thread to stop
    // - detach/erase the worker node under the lock, then stop the worker after releasing it
    // - this keeps new auth-connect worker creation from blocking on the same lock during retry
    (void)EnterCleanupLockHelper();

    CBaseConnection* queuedConnectionOwner = CBaseConnection_FromQueueContextScaffold(contextKey);
    void* cleanupContextKey = CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
    bool touchedConnectionState = false;
    uint32_t result = 0u;
    std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> workerPayload;

    if (CLTTCPConnection* queuedTcpConnection =
            dynamic_cast<CLTTCPConnection*>(queuedConnectionOwner)) {
        queuedTcpConnection->SetState(LTTCPEngineConnectionState::kClosed);
        queuedTcpConnection->SetSocketHandle(kInvalidSocketHandle);
        queuedTcpConnection->SetWorkerThreadScaffold(nullptr);
        touchedConnectionState = true;
    }

    if (CMessageConnection* connection = FindMessageConnection(cleanupContextKey)) {
        connection->SetState(LTTCPEngineConnectionState::kClosed);
        connection->SetSocketHandle(kInvalidSocketHandle);
        connection->SetWorkerThreadScaffold(nullptr);
        touchedConnectionState = true;
    }

    if (CLTThreadPerClientTCPEngine_WorkerThread* worker = FindWorker(cleanupContextKey)) {
        CLTThreadPerClientTCPEngine_ContextTreeNode* node = ContextTreeFindNode(
            ownedContextTreeHead8C_,
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(worker->ContextKey())));
        workerPayload = ContextTreeDetachPayload(this, node);
        if (node) {
            (void)ContextTreeEraseNode(this, ownedContextTreeHead8C_, node);
        }
        result = kResultSuccess;
    } else {
        if (!touchedConnectionState) {
            spdlog::debug(
                "CLTThreadPerClientTCPEngine::CleanupConnection couldn't find socket/context key={} normalizedKey={} owner={}",
                fmt::ptr(contextKey),
                fmt::ptr(cleanupContextKey),
                fmt::ptr(queuedConnectionOwner));
        }
        result = touchedConnectionState ? kResultSuccess : 0u;
    }

    (void)LeaveCleanupLockHelper();

    if (workerPayload) {
        StopWorkerThreadScaffold(workerPayload.get());
        workerPayload.reset();
    }

    SyncAttachedLauncherObjectStateScaffold();
    return result;
}

// UNANCHORED: launcher ABI-shell attachment/mirror entrypoint.
void CLTThreadPerClientTCPEngine::AttachLauncherAbiSurfaceScaffold(
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment& attachment) {
    EnsureEngineLauncherAbiAttachment(this) = attachment;
    SyncAttachedLauncherObjectStateScaffold();
}

// UNANCHORED: launcher ABI-shell detach/reset helper.
void CLTThreadPerClientTCPEngine::DetachLauncherAbiSurfaceScaffold() {
    CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    if (!attachment) {
        return;
    }

    if (attachment->field04CtorFlags) {
        *attachment->field04CtorFlags = 0u;
    }
    if (attachment->field08QueueThreadArray) {
        *attachment->field08QueueThreadArray = nullptr;
    }
    if (attachment->list80EndpointTreeHead) {
        *attachment->list80EndpointTreeHead = nullptr;
    }
    if (attachment->field84EndpointCount) {
        *attachment->field84EndpointCount = 0u;
    }
    if (attachment->list8CContextTreeHead) {
        *attachment->list8CContextTreeHead = nullptr;
    }
    if (attachment->field90ContextCount) {
        *attachment->field90ContextCount = 0u;
    }

    g_CLTThreadPerClientTCPEngineLauncherAbiAttachments.erase(this);
}

void CLTThreadPerClientTCPEngine::RefreshOwnedLauncherMirrorStateScaffold() {
    CLTThreadPerClientTCPEngine_EndpointPayloadBacking* endpointBacking = FindEngineEndpointPayloadBacking(this);
    CLTThreadPerClientTCPEngine_ContextPayloadBacking* contextBacking = FindEngineContextPayloadBacking(this);
    ownedEndpointCount84_ = endpointBacking
        ? static_cast<uint32_t>(endpointBacking->entries.size())
        : 0u;
    ownedContextCount90_ = contextBacking
        ? static_cast<uint32_t>(contextBacking->entries.size())
        : 0u;

    // Current direct `_Rb_tree` API pass:
    // - `_Rb_tree_insert_and_rebalance` and `_Rb_tree_rebalance_for_erase` already keep the
    //   header `root/leftmost/rightmost` links live for non-empty trees
    // - so this mirror refresh only has to restore the canonical empty-header state when the
    //   source-owned payload backing becomes empty
    if (ownedEndpointTreeHead80_ && ownedEndpointCount84_ == 0u) {
        InitializeEndpointTreeHead24(ownedEndpointTreeHead80_);
    }
    if (ownedContextTreeHead8C_ && ownedContextCount90_ == 0u) {
        InitializeContextTreeHead18(ownedContextTreeHead8C_);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::SyncAttachedLauncherObjectStateScaffold() {
    RefreshOwnedLauncherMirrorStateScaffold();
    CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    if (!attachment) {
        return;
    }

    if (attachment->field04CtorFlags) {
        *attachment->field04CtorFlags = ctorFlagsField04_;
    }
    if (attachment->field08QueueThreadArray) {
        *attachment->field08QueueThreadArray = queueThreadArrayField08_;
    }
    if (attachment->list80EndpointTreeHead) {
        *attachment->list80EndpointTreeHead = ownedEndpointTreeHead80_;
    }
    if (attachment->field84EndpointCount) {
        *attachment->field84EndpointCount = ownedEndpointCount84_;
    }
    if (attachment->list8CContextTreeHead) {
        *attachment->list8CContextTreeHead = ownedContextTreeHead8C_;
    }
    if (attachment->field90ContextCount) {
        *attachment->field90ContextCount = ownedContextCount90_;
    }
}

CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue0CScaffold() {
    CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue0C)
        ? attachment->queue0C
        : &ownedQueue0C_;
}

CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue34Scaffold() {
    CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue34)
        ? attachment->queue34
        : &ownedQueue34_;
}

const CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue0CScaffold() const {
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue0C)
        ? attachment->queue0C
        : &ownedQueue0C_;
}

const CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue34Scaffold() const {
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue34)
        ? attachment->queue34
        : &ownedQueue34_;
}

void* CLTThreadPerClientTCPEngine::ActiveQueueLockScaffold() const {
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queueLock)
        ? attachment->queueLock
        : const_cast<CRITICAL_SECTION*>(&ownedQueueLockHelper60_.crit);
}

void* CLTThreadPerClientTCPEngine::ActiveQueueSignalEventScaffold() const {
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queueSignalEvent)
        ? attachment->queueSignalEvent
        : ownedQueueSignalEvent7C_;
}

void* CLTThreadPerClientTCPEngine::ActiveCleanupLockScaffold() const {
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->cleanupLock)
        ? attachment->cleanupLock
        : const_cast<CRITICAL_SECTION*>(&ownedCleanupLockHelper98_.crit);
}

// anchor: launcher.exe:0x435f90
uint32_t CLTThreadPerClientTCPEngine::SignalQueueEventHelper() {
    HANDLE eventHandle = static_cast<HANDLE>(ActiveQueueSignalEventScaffold());
    return (eventHandle && SetEvent(eventHandle)) ? 0u : 1u;
}

// anchor: launcher.exe:0x435fa0
uint32_t CLTThreadPerClientTCPEngine::WaitQueueEventHelper(int reasonMilliseconds) {
    (void)LeaveQueueLockHelper();
    HANDLE eventHandle = static_cast<HANDLE>(ActiveQueueSignalEventScaffold());
    const DWORD waitResult = eventHandle
        ? WaitForSingleObject(eventHandle, static_cast<DWORD>(reasonMilliseconds))
        : WAIT_FAILED;
    if (waitResult == WAIT_OBJECT_0) {
        (void)EnterQueueLockHelper();
        return 0u;
    }
    if (waitResult == WAIT_TIMEOUT) {
        (void)EnterQueueLockHelper();
        return 3u;
    }
    return 1u;
}

// anchor family: launcher.exe:0x4147b0
uint32_t CLTThreadPerClientTCPEngine::EnterQueueLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147c0
uint32_t CLTThreadPerClientTCPEngine::LeaveQueueLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147b0
uint32_t CLTThreadPerClientTCPEngine::EnterCleanupLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147c0
uint32_t CLTThreadPerClientTCPEngine::LeaveCleanupLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* CLTThreadPerClientTCPEngine::EnsureLauncherConnectionContextScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold** slot,
    mxo::ltlogin::CLTLoginMediator* mediator,
    const char* label,
    bool isMarginConnection) {
    if (!slot) {
        return nullptr;
    }

    EnsureLauncherConnectionBridgeContextVtableInitialized();
    if (!*slot) {
        *slot = static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(
            std::calloc(1, sizeof(mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold)));
        if (!*slot) {
            spdlog::info(
                "CLTThreadPerClientTCPEngine::EnsureLauncherConnectionContextScaffold failed label='{}'",
                label ? label : "<null>");
            return nullptr;
        }
        (*slot)->vtable = g_LauncherConnectionBridgeContextVtable;
        (*slot)->autoReleaseFlag = 0;
    }

    (*slot)->debugLabel = label;
    (*slot)->mediator = mediator;
    (*slot)->isMarginConnection = isMarginConnection;
    return *slot;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueCompletedOperationScaffold(
    void* workItem,
    void* context,
    bool useQueue34,
    const char* label,
    bool queueLockAlreadyHeld) {
    // Current best read of original `0x436820` / `0x436670`:
    // - `0x436820` itself returns `void`
    // - lock/order is:
    //   enter helper `+0x60` -> snapshot combined queue-pair emptiness -> push pair -> leave lock
    //   -> signal helper `+0x5c` only on pre-push empty -> non-empty transition
    // - no push-success result is surfaced back to callers; caller-side ownership/lifetime does not
    //   branch on queue-growth success/failure once this path is entered
    CLTThreadPerClientTCPEngine_Queue* targetQueue = useQueue34 ? ActiveQueue34Scaffold() : ActiveQueue0CScaffold();
    if (!targetQueue) {
        return false;
    }

    CRITICAL_SECTION* queueLock =
        CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold());
    if (queueLock && !queueLockAlreadyHeld) {
        EnterCriticalSection(queueLock);
    }

    const bool queuePairWasEmpty =
        Queue_IsEmpty(ActiveQueue0CScaffold()) && Queue_IsEmpty(ActiveQueue34Scaffold());
    Queue_PushPair(
        targetQueue,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(workItem)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));

    if (queueLock && !queueLockAlreadyHeld) {
        LeaveCriticalSection(queueLock);
    }

    if (queuePairWasEmpty) {
        (void)SignalQueueEventHelper();
    }

    LoggerForBridgeLabel(label)->info(
        "CLTThreadPerClientTCPEngine::EnqueueCompletedOperationScaffold label={} queue=[{}] workItem=0x{:08x} context={} pairWasEmpty={:08x} lockHeld={:08x}",
        label ? label : "<null>",
        useQueue34 ? "queue34" : "queue0C",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(workItem)),
        fmt::ptr(context),
        queuePairWasEmpty ? 1u : 0u,
        queueLockAlreadyHeld ? 1u : 0u);
    return true;
}

bool CLTThreadPerClientTCPEngine::EnqueueDirectConnectionStatusWorkItemScaffold(
    CLTTCPConnection* connection,
    uint32_t workType,
    uint32_t workPayload,
    const char* label,
    bool queueLockAlreadyHeld) {
    if (!connection) {
        return false;
    }

    EnsureLauncherConnectionBridgeWorkItemVtableInitialized();
    mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* workItem =
        static_cast<mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold*>(
            std::calloc(1, sizeof(mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold)));
    if (!workItem) {
        LoggerForBridgeLabel(label)->info(
            "CLTThreadPerClientTCPEngine::EnqueueDirectConnectionStatusWorkItemScaffold failed label='{}'",
            label ? label : "<null>");
        return false;
    }

    workItem->header.vtable = g_LauncherConnectionBridgeWorkItemVtable;
    workItem->header.workType = workType;
    workItem->workPayload = workPayload;
    workItem->debugLabel = label;

    void* queuedContext = ResolveQueuedConnectionContextForProducerAblation(connection);
    const bool queued = EnqueueCompletedOperationScaffold(
        workItem,
        queuedContext,
        /*useQueue34=*/false,
        label,
        queueLockAlreadyHeld);
    if (!queued) {
        std::free(workItem);
        return false;
    }

    LoggerForBridgeLabel(label)->info(
        "CLTThreadPerClientTCPEngine direct queued work label='{}' workItem={} context={} type=0x{:08x} ({}) payload=0x{:08x}",
        label ? label : "<null>",
        fmt::ptr(workItem),
        fmt::ptr(queuedContext),
        workType,
        LauncherBridgeWorkTypeName(workType),
        workPayload);

    bool shouldImmediateDrain = (workType == kWorkTypeConnectionStatus || workType == kWorkTypeClose);
    if (shouldImmediateDrain && workType == kWorkTypeClose) {
        if (auto* worker = connection->WorkerThreadScaffold(); worker != nullptr && worker->IsCurrentThread()) {
            shouldImmediateDrain = false;
        }
    }
    if (shouldImmediateDrain) {
        RunCompletedOperationQueue(
            /*nonBlocking=*/true,
            /*preferType1CallbackBeforeCleanup=*/workType == kWorkTypeClose);
    }
    return true;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    uint32_t workType,
    uint32_t workPayload,
    const char* label,
    bool queueLockAlreadyHeld) {
    if (!context) {
        return false;
    }

    // Fidelity note:
    // - original worker-thread producers queue the direct connection object as `context` for
    //   type-1 close and type-2 status work too
    // - when the replacement-only launcher bridge context is available, use its sidecar
    //   connection only as the route back to that same direct connection object
    if (context->sidecarConnection == nullptr) {
        LoggerForBridgeLabel(label)->warn(
            "CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemInternalScaffold missing sidecarConnection label='{}' context={} type=0x{:08x} ({}) payload=0x{:08x}",
            label ? label : "<null>",
            fmt::ptr(context),
            workType,
            LauncherBridgeWorkTypeName(workType),
            workPayload);
        return false;
    }

    const bool queued = EnqueueDirectConnectionStatusWorkItemScaffold(
        context->sidecarConnection,
        workType,
        workPayload,
        label,
        queueLockAlreadyHeld);
    if (!queued) {
        return false;
    }

    // Source-owned active-path tightening for the current launcher/client single-process bridge:
    // some late startup consumers expect queued status/close work to become visible with less
    // latency than the current queue-thread/poll cadence always guarantees.
    // In particular, the post-state9 healthy original tail eventually reaches the queued margin
    // peer-close -> `0x41afc0 -> 0x438df0 -> 0x41cfb0(0x0f)` path, while the replacement can crash
    // in late rendering before the next outer pump drains that close work.
    // Fidelity/retry correction:
    // - draining a type-1 close immediately on the same socket worker thread that produced it can
    //   hit the source-owned self-dispatch cleanup path in `RunCompletedOperationQueue`
    // - that path is still not faithful enough to tear down the worker without leaving retry-hostile
    //   state behind
    // - original queued close consumption belongs to the later shared queue-consumer family, so keep
    //   same-worker type-1 auth close work queued for the next outer pump instead of forcing an
    //   immediate local drain here
    bool shouldImmediateDrain = (workType == kWorkTypeConnectionStatus || workType == kWorkTypeClose);
    if (shouldImmediateDrain && workType == kWorkTypeClose && context->sidecarConnection != nullptr) {
        auto* worker = context->sidecarConnection->WorkerThreadScaffold();
        if (worker != nullptr && worker->IsCurrentThread()) {
            shouldImmediateDrain = false;
        }
    }
    if (shouldImmediateDrain) {
        RunCompletedOperationQueue(
            /*nonBlocking=*/true,
            /*preferType1CallbackBeforeCleanup=*/workType == kWorkTypeClose);
    }
    return true;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    uint32_t workType,
    uint32_t workPayload,
    const char* label) {
    return EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
        context,
        workType,
        workPayload,
        label,
        /*queueLockAlreadyHeld=*/false);
}

// UNANCHORED: connection-owned bridge for the recovered `0x449d8a -> 0x436820` handoff.
void CLTThreadPerClientTCPEngine::EnqueueCompletedOperationFromConnectionScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPConnection* connection,
    const char* label) {
    // Current best original read for this receive path:
    // - `CLTTCPConnection::OnReceive` always calls `0x436820(engine+0x10, workItem, self, false)`
    // - the queued context on that path is the direct connection object (`self`), not a wrapper
    // - queue selection is therefore fixed to queue0C here
    // - current parser read does not support an intentional `Parse(...) == 0` / `workItem == NULL`
    //   emit on this path; null work items belong to later lifecycle/shutdown producers instead
    // - original caller does not test a success result or reclaim `workItem`; ownership is already
    //   transferred to the queue/consumer boundary when this helper is entered
    const bool queued = EnqueueCompletedOperationScaffold(
        workItem,
        ResolveQueuedConnectionContextForProducerAblation(connection),
        /*useQueue34=*/false,
        label,
        /*queueLockAlreadyHeld=*/false);
    if (queued) {
        RunCompletedOperationQueue(/*nonBlocking=*/true);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::PumpLauncherConnectionContextScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    const char* receiveLabel) {
    if (!context || !context->sidecarConnection) {
        return;
    }

    // Current active-path tightening after the `0x42fe50` worker-loop pass:
    // - once the direct connection already has its recovered worker object in `[connection+0x08]`,
    //   that worker owns the blocking select/read/write/wakeup loop
    // - keep this helper only as a legacy fallback for source-owned no-worker paths instead of
    //   racing the live worker on the same socket from the arg5 helper poll
    if (context->sidecarConnection->WorkerThreadScaffold() != nullptr) {
        return;
    }

    // Keep the connection seam itself on the faithful one-fragment
    // `0x42fe50 -> 0x449d40 -> 0x469bf0` receive handoff, but let the launcher bridge re-enter
    // that helper repeatedly within one arg5 helper poll.
    //
    // Why this is currently bounded here instead of inside `CLTTCPConnection`:
    // - a previous full same-poll recv-drain restoration on the bridge path regressed live runs
    //   into a later "Loading Character" stall
    // - original queue type `3` already belongs to the parsed-packet work items that
    //   `CLTTCPConnection::OnReceive` enqueues before control returns here
    // - the extra `AuthReceivePacket` / `MarginReceivePacket` submission below is therefore only a
    //   source-owned receive-drain proxy for the later original `0x4490c0` dispatch tail that
    //   source still does not execute on that same callback
    // - newer bounded leaf-side corrections now source-own two later destinations from that tail:
    //   - handled auth copied packets can re-enter
    //     `0x449a30 -> owner+0x180 / 0x41f250`
    //   - handled margin copied packets can re-enter
    //     `0x44af20 -> 0x442d00 -> owner+0x184 / 0x41f260`
    // - so the later synthetic receive-drain item is increasingly a fallback/no-op path rather
    //   than the primary live consumer on those handled branches
    // - newer bounded active-path proof now narrows it further:
    //   successful launcher-into-game runs show the active auth/margin path no longer logging any
    //   `pendingCopiedPackets=` or synthetic receive-drain handling; copied packets are consumed on
    //   the in-callback post-copy tail instead
    // - tighter `2026-04-02` tail read now matches that runtime shape better:
    //   once `0x4490c0` reaches its post-copy virtual dispatch family, the packet is already
    //   consumed locally inside the same callback
    // - so the synthetic proxy below is now expected to stay dormant and only survives as
    //   compatibility scaffolding for unexpected source-owned paths
    // - current source queue order on one helper poll is therefore:
    //   - `OnReceive` first queues all parsed-packet work items emitted from the current fragment
    //   - only if some unexpected path still leaves copied packets pending would this helper queue a
    //     synthetic receive-drain proxy for the same fragment
    //   - if a later recv in the same poll returns peer-close/error, the type-1 close item queues
    //     after those successful-fragment submissions
    // - later peer-close notification still queues after any successful fragment notifications from
    //   the same helper poll, matching the original `0x42fe50` ordering more closely than the old
    //   once-per-fragment unconditional synthetic receive path
    while (true) {
        const int received =
            context->sidecarConnection->PollReceiveAndDeliverReadOperationFragmentsScaffold();
        if (received > 0) {
            if (context->sidecarConnection->HasPendingReceivedPacketsScaffold()) {
                (void)EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
                    context,
                    /*workType=*/kWorkTypeSyntheticReceiveDrain,
                    /*workPayload=*/static_cast<uint32_t>(received),
                    receiveLabel,
                    /*queueLockAlreadyHeld=*/true);
            }
            continue;
        }

        if (received < 0 && !context->peerCloseQueued) {
            context->peerCloseQueued = true;
            spdlog::info(
                "CLTThreadPerClientTCPEngine::PumpLauncherConnectionContextScaffold queued peer-close label='{}' context={} connection={}",
                (context->debugLabel && context->debugLabel[0]) ? context->debugLabel : "<null>",
                fmt::ptr(context),
                fmt::ptr(context->sidecarConnection));
            (void)EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
                context,
                /*workType=*/kWorkTypeClose,
                /*workPayload=*/0u,
                context->isMarginConnection ? "MarginPeerClosed" : "AuthPeerClosed",
                /*queueLockAlreadyHeld=*/true);
        }
        return;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::PumpLauncherConnectionBridgeFromArg5HelperScaffold() {
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* authContext = nullptr;
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* marginContext = nullptr;

    if (CLTThreadPerClientTCPEngine_ContextPayloadBacking* contextBacking =
            FindEngineContextPayloadBacking(this)) {
        for (const auto& it : contextBacking->entries) {
            const CLTThreadPerClientTCPEngine_WorkerThread* worker = it.second->payload.get();
            CMessageConnection* connection = worker
                ? static_cast<CMessageConnection*>(worker->ContextKey())
                : nullptr;
            mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
                ResolveLauncherBridgeContextForConnectionScaffold(connection);
            if (!context) {
                continue;
            }
            if (context->isMarginConnection) {
                marginContext = context;
            } else {
                authContext = context;
            }
        }
    }

    PumpLauncherConnectionContextScaffold(authContext, "AuthReceivePacket");
    PumpLauncherConnectionContextScaffold(marginContext, "MarginReceivePacket");
}

// anchor: launcher.exe:0x436b10
void CLTThreadPerClientTCPEngine::RunCompletedOperationQueue(
    bool nonBlocking,
    bool preferType1CallbackBeforeCleanup) {
    // Current bounded mirror of the shared launcher/client consumer family:
    // - prefer queue34, else queue0C
    // - nonBlocking=true matches the client poll form; false waits on the attached signal event
    // - null work item is the shutdown sentinel and cascades via the normal enqueue helper
    // - type-1 work runs slot-12-style cleanup before the later context callback
    // - callback runs before the later release tail
    // - on the type-1 path, conditional context auto-release precedes the final work-item release
    // - the release bodies themselves are still source-owned vtable-dispatch scaffolds
    // - queue selection/pop happens under the attached arg5 lock
    CRITICAL_SECTION* queueLock =
        CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold());
    while (true) {
        if (queueLock) {
            EnterCriticalSection(queueLock);
        }

        CLTThreadPerClientTCPEngine_Queue* selectedQueue = nullptr;
        if (!Queue_IsEmpty(ActiveQueue34Scaffold())) {
            selectedQueue = ActiveQueue34Scaffold();
        } else if (!Queue_IsEmpty(ActiveQueue0CScaffold())) {
            selectedQueue = ActiveQueue0CScaffold();
        }

        if (!selectedQueue) {
            if (queueLock) {
                LeaveCriticalSection(queueLock);
            }
            if (nonBlocking) {
                return;
            }

            // Bounded fidelity step from the shared `0x436b10` / `0x62531c10` queue-consumer family:
            // when both queues are empty, original code routes through arg5 helper `+0x5c` for the
            // wait path instead of immediately returning. With the current ownership move, that
            // helper body now lives on the engine side even though the launcher ABI shell still
            // supplies the raw embedded helper address when original code calls it directly.
            if (!ActiveQueueSignalEventScaffold()) {
                return;
            }

            const uint32_t waitResult = WaitQueueEventHelper(INFINITE);
            if (waitResult == 0u || waitResult == 3u) {
                continue;
            }
            return;
        }

        CLTThreadPerClientTCPEngine_QueuedPair pair = {};
        const bool popped = Queue_TryPopPair(selectedQueue, &pair);
        if (queueLock) {
            LeaveCriticalSection(queueLock);
        }
        if (!popped) {
            return;
        }

        void* workItem = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value0));
        void* context = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value1));
        if (!workItem) {
            // Current best read: a null work item is the shutdown-style queue sentinel.
            // Original `0x436f56..0x436fa8` cascades shutdown via the normal enqueue helper path,
            // not by open-coding another raw queue write.
            (void)EnqueueCompletedOperationScaffold(
                nullptr,
                nullptr,
                /*useQueue34=*/false,
                "RunCompletedOperationQueueShutdownCascade",
                /*queueLockAlreadyHeld=*/false);
            return;
        }

        const uint32_t workType = QueueWorkItem_GetType(workItem);
        const bool isType1 = (workType == kWorkTypeClose);
        const bool shouldAutoReleaseContext =
            context && isType1 && QueueContext_ShouldAutoReleaseAfterType1(context);

        CLTTCPConnection* queuedConnection = nullptr;
        if (context) {
            CBaseConnection* completionTarget = CBaseConnection_FromQueueContextScaffold(context);
            if (completionTarget == nullptr) {
                completionTarget = static_cast<CBaseConnection*>(context);
            }
            queuedConnection = dynamic_cast<CLTTCPConnection*>(completionTarget);
        }
        const bool detectedSameWorkerThreadCloseSelfDispatch =
            isType1 && queuedConnection != nullptr && queuedConnection->WorkerThreadScaffold() != nullptr &&
            queuedConnection->WorkerThreadScaffold()->IsCurrentThread();
        const bool sameWorkerThreadCloseSelfDispatch =
            isType1 && (preferType1CallbackBeforeCleanup || detectedSameWorkerThreadCloseSelfDispatch);

        spdlog::debug(
            "CLTThreadPerClientTCPEngine::RunCompletedOperationQueue consume queue=[{}] workItem={} workType=0x{:08x} context={} autoReleaseType1Context={} preferType1CallbackBeforeCleanup={} detectedSameWorkerThreadCloseSelfDispatch={} sameWorkerThreadCloseSelfDispatch={}",
            (selectedQueue == ActiveQueue34Scaffold()) ? "queue34" : "queue0C",
            fmt::ptr(workItem),
            workType,
            fmt::ptr(context),
            shouldAutoReleaseContext ? 1u : 0u,
            preferType1CallbackBeforeCleanup ? 1u : 0u,
            detectedSameWorkerThreadCloseSelfDispatch ? 1u : 0u,
            sameWorkerThreadCloseSelfDispatch ? 1u : 0u);

        if (sameWorkerThreadCloseSelfDispatch) {
            // Bounded single-process bridge correction:
            // - on the replacement path we may dequeue the close work on the same socket worker
            //   thread that produced it
            // - if we run slot-12 cleanup first there, `Stop(true)` hits `ExitThread(0)` on the
            //   current thread and we never reach the later margin callback / event-0x0f tail
            // - keep the normal original order everywhere else, but on this one self-dispatch seam
            //   let the callback run before worker teardown
            if (context) {
                QueueContext_OnOperationCompleted(context, workItem);
            }
            if (shouldAutoReleaseContext) {
                QueueContext_Release(context);
            }
            QueueWorkItem_Release(workItem);
            if (context) {
                CleanupConnection(context);
            }
            return;
        }

        if (context && isType1) {
            CleanupConnection(context);
        }

        if (context) {
            QueueContext_OnOperationCompleted(context, workItem);
        }

        if (shouldAutoReleaseContext) {
            QueueContext_Release(context);
        }
        QueueWorkItem_Release(workItem);
    }
}

// anchor family: launcher.exe:0x4366f0 / 0x436920
// Current source helper owns the real `+0x04/+0x08` queue-thread array/count fields directly.
void CLTThreadPerClientTCPEngine::RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount) {
    CLTThreadPerClientTCPEngine_QueueThread** queueThreadArray =
        static_cast<CLTThreadPerClientTCPEngine_QueueThread**>(queueThreadArrayField08_);
    const uint32_t existingQueueThreadCount = ctorFlagsField04_;
    if (existingQueueThreadCount != 0u) {
        (void)EnqueueCompletedOperationScaffold(
            nullptr,
            nullptr,
            /*useQueue34=*/false,
            "RebuildQueueThreadsForCtorCountShutdown",
            /*queueLockAlreadyHeld=*/false);
    }
    for (uint32_t i = 0; i < existingQueueThreadCount; ++i) {
        CLTThreadPerClientTCPEngine_QueueThread* thread = queueThreadArray ? queueThreadArray[i] : nullptr;
        if (!thread) {
            continue;
        }
        (void)thread->Wait();
        delete thread;
    }
    if (queueThreadArray) {
        std::free(queueThreadArray);
    }

    queueThreadArrayField08_ = nullptr;
    ctorFlagsField04_ = 0u;
    if (queueThreadCount == 0u) {
        SyncAttachedLauncherObjectStateScaffold();
        return;
    }

    queueThreadArray = static_cast<CLTThreadPerClientTCPEngine_QueueThread**>(
        std::calloc(queueThreadCount, sizeof(CLTThreadPerClientTCPEngine_QueueThread*)));
    if (!queueThreadArray) {
        SyncAttachedLauncherObjectStateScaffold();
        return;
    }

    for (uint32_t i = 0; i < queueThreadCount; ++i) {
        queueThreadArray[i] = new CLTThreadPerClientTCPEngine_QueueThread(this);
        (void)queueThreadArray[i]->Start(/*startPriority=*/2);
    }

    queueThreadArrayField08_ = queueThreadArray;
    ctorFlagsField04_ = queueThreadCount;
    SyncAttachedLauncherObjectStateScaffold();
}

// UNANCHORED scaffold accessor for source-side queue-thread child tracking.
size_t CLTThreadPerClientTCPEngine::QueueThreadCount() const {
    return static_cast<size_t>(ctorFlagsField04_);
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
CMessageConnection* CLTThreadPerClientTCPEngine::FindMessageConnection(void* contextKey) {
    CBaseConnection* queueContextOwner = ResolveEngineQueueContextOwnerScaffold(contextKey);
    void* resolvedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    auto matchesConnectionKey =
        [contextKey, resolvedContextKey, queueContextOwner](CMessageConnection* connection) -> bool {
        if (!connection) {
            return false;
        }

        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* bridgeContext =
            ResolveLauncherBridgeContextForConnectionScaffold(connection);
        return connection == contextKey ||
            connection == resolvedContextKey ||
            connection == queueContextOwner ||
            connection->OwnerContext() == contextKey ||
            connection->OwnerContext() == resolvedContextKey ||
            bridgeContext == contextKey ||
            bridgeContext == resolvedContextKey;
    };

    if (CMessageConnection* queuedConnection =
            dynamic_cast<CMessageConnection*>(queueContextOwner);
        queuedConnection && matchesConnectionKey(queuedConnection)) {
        return queuedConnection;
    }

    auto bridgeContextConnection =
        [](void* candidate) -> CMessageConnection* {
            mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
                static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(candidate);
            return IsLauncherBridgeContextScaffold(context) ? context->sidecarConnection : nullptr;
        };

    if (CMessageConnection* bridgedConnection = bridgeContextConnection(contextKey);
        bridgedConnection && matchesConnectionKey(bridgedConnection)) {
        return bridgedConnection;
    }
    if (resolvedContextKey != contextKey) {
        if (CMessageConnection* bridgedConnection = bridgeContextConnection(resolvedContextKey);
            bridgedConnection && matchesConnectionKey(bridgedConnection)) {
            return bridgedConnection;
        }
    }

    if (CLTThreadPerClientTCPEngine_ContextPayloadBacking* contextBacking =
            FindEngineContextPayloadBacking(this)) {
        for (const auto& it : contextBacking->entries) {
            const CLTThreadPerClientTCPEngine_WorkerThread* worker = it.second->payload.get();
            CMessageConnection* connection = worker
                ? static_cast<CMessageConnection*>(worker->ContextKey())
                : nullptr;
            if (matchesConnectionKey(connection)) {
                return connection;
            }
        }
    }

    return nullptr;
}


// UNANCHORED starter helper.
// No direct launcher.exe helper body is assigned yet; this just mirrors the recovered key shape.
LTTCPEndpointKey CLTThreadPerClientTCPEngine::MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    LTTCPEndpointKey key = {};
    key.family = 2;
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = ipv4NetworkOrder;
    return key;
}

// anchor: launcher.exe:0x42fdb0
CLTThreadPerClientTCPEngine_AcceptThread* CLTThreadPerClientTCPEngine::FindMonitoredPort(const LTTCPEndpointKey& key) {
    CLTThreadPerClientTCPEngine_EndpointTreeNode* node =
        EndpointTreeFindNode(ownedEndpointTreeHead80_, key);
    return node ? node->_M_valptr()->second : nullptr;
}

// anchor: launcher.exe:0x42fe10
CLTThreadPerClientTCPEngine_WorkerThread* CLTThreadPerClientTCPEngine::FindWorker(void* contextKey) {
    CMessageConnection* connection = FindMessageConnection(contextKey);
    if (!connection) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_ContextTreeNode* node = ContextTreeFindNode(
        ownedContextTreeHead8C_,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(connection)));
    return node ? node->_M_valptr()->second : nullptr;
}

CMessageConnection* CLTThreadPerClientTCPEngine::ResolveConnectionForEngineSlotScaffold(
    void* contextKey) {
    if (!contextKey) {
        return nullptr;
    }

    void* normalizedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    CMessageConnection* connection = FindMessageConnection(contextKey);
    if (!connection && normalizedContextKey != contextKey) {
        connection = FindMessageConnection(normalizedContextKey);
    }

    // Static RE of `0x449cd0`, `0x449d20`, and `0x449d40` keeps the public engine slot family on
    // the direct connection object itself. After queue-context unwrapping and bridge-sidecar
    // handling above, remaining callers are expected to already be passing that connection object.
    if (!connection) {
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* bridgeContext =
            static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(normalizedContextKey);
        if (IsLauncherBridgeContextScaffold(bridgeContext)) {
            connection = bridgeContext->sidecarConnection;
        } else {
            connection = static_cast<CMessageConnection*>(normalizedContextKey);
        }
    }
    if (!connection) {
        return nullptr;
    }

    connection->SetEngine(this);
    if (connection->OwnerContext() == nullptr && normalizedContextKey != connection) {
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* bridgeContext =
            static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(normalizedContextKey);
        connection->SetOwnerContext(
            IsLauncherBridgeContextScaffold(bridgeContext) && bridgeContext->mediator != nullptr
                ? static_cast<void*>(bridgeContext->mediator)
                : normalizedContextKey);
    }
    return connection;
}

// UNANCHORED: source-owned helper shaped after launcher.exe:0x431ff0 worker creation/insertion.
CLTThreadPerClientTCPEngine_WorkerThread* CLTThreadPerClientTCPEngine::CreateAndInsertWorkerThreadScaffold(
    CMessageConnection* connection,
    bool datagramMode,
    bool startThread) {
    if (!connection) {
        return nullptr;
    }

    connection->SetEngine(this);
    std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> worker =
        std::make_unique<CLTThreadPerClientTCPEngine_WorkerThread>(connection, datagramMode);
    if (!worker) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_WorkerThread* result = nullptr;
    const uint32_t key = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(connection));
    (void)EnterCleanupLockHelper();
    bool inserted = false;
    CLTThreadPerClientTCPEngine_ContextTreeNode* node = ContextTreeInsertUniqueNode(
        this,
        ownedContextTreeHead8C_,
        key,
        &inserted);
    if (node && inserted) {
        if (ContextTreeAttachPayload(this, node, std::move(worker))) {
            result = node->_M_valptr()->second;
        }
    } else if (node) {
        result = node->_M_valptr()->second;
    }
    (void)LeaveCleanupLockHelper();

    if (result) {
        connection->SetWorkerThreadScaffold(result);
    }
    if (result && startThread) {
        (void)result->Start(/*startPriority=*/2);
    }
    return result;
}

// UNANCHORED: source-owned teardown helper for the direct `AcceptThread` payload stored at
// `[endpointNode+0x20]`.
void CLTThreadPerClientTCPEngine::StopAcceptThreadScaffold(
    CLTThreadPerClientTCPEngine_AcceptThread* acceptThread) {
    if (!acceptThread) {
        return;
    }

    acceptThread->SignalWakeup();
    (void)acceptThread->Stop(/*waitAfterTerminate=*/true);
    acceptThread->CloseListenSocketScaffold();
}

// UNANCHORED: source-owned teardown helper for the direct `WorkerThread` payload stored at
// `[contextNode+0x14]`.
void CLTThreadPerClientTCPEngine::StopWorkerThreadScaffold(
    CLTThreadPerClientTCPEngine_WorkerThread* workerThread) {
    if (!workerThread) {
        return;
    }

    workerThread->RequestExit();
    workerThread->SignalWakeup();
    (void)workerThread->Stop(/*waitAfterTerminate=*/true);
    if (CMessageConnection* connection = static_cast<CMessageConnection*>(workerThread->ContextKey())) {
        connection->SetWorkerThreadScaffold(nullptr);
    }
}

// UNANCHORED starter binding helper.
// Keeps current owner->engine binding state on the liblttcp side rather than in diagnostics.cpp.
CLTThreadPerClientTCPEngineBinding::CLTThreadPerClientTCPEngineBinding()
    : owner_(nullptr),
      engine_() {}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngineBinding::~CLTThreadPerClientTCPEngineBinding() = default;

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::Bind(void* owner) {
    if (owner_ == owner && engine_) {
        return true;
    }

    owner_ = owner;
    engine_ = std::make_unique<CLTThreadPerClientTCPEngine>();
    return static_cast<bool>(engine_);
}

// UNANCHORED starter binding helper.
void CLTThreadPerClientTCPEngineBinding::Reset() {
    engine_.reset();
    owner_ = nullptr;
}

// UNANCHORED starter binding helper.
void* CLTThreadPerClientTCPEngineBinding::Owner() const {
    return owner_;
}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngine* CLTThreadPerClientTCPEngineBinding::Engine() const {
    return engine_.get();
}

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::HasEngine() const {
    return static_cast<bool>(engine_);
}

}  // namespace mxo::liblttcp
