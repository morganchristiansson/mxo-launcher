#include "ltthreadperclienttcpengine.h"

#include "../libltmessaging/messageconnection.h"
#include "../libltnet/sys/pc/pcsocket.h"
#include "../../../game/src/libltclientlogin/loginmediator.h"
#include <spdlog/spdlog.h>

#include <bits/stl_tree.h>

#include <winsock2.h>
#include <system_error>

#include <process.h>

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mxo::liblttcp {

namespace {

// Per-logger SPDLOG_LEVEL overrides only apply on call sites that explicitly fetch a named logger.
// Keep the receive hot-path seam narrow by only routing labels with registered logger names through
// spdlog::get(...); all other queue labels fall back to the default logger.
static spdlog::logger* LoggerForQueueLabel(const char* label) {
    if (label && label[0]) {
        if (std::shared_ptr<spdlog::logger> logger = spdlog::get(label)) {
            return logger.get();
        }
    }
    return spdlog::default_logger_raw();
}

struct CLTThreadPerClientTCPEngine_0x4b2768_QueuePair {
    uint32_t value0;
    uint32_t value1;
};

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_ConnectionStatusWorkItemVtable[2] = {0};
static void* g_CloseWorkItemVtable[2] = {0};
static constexpr uint8_t kSocketFactoryFlagSkipDisableNagle = 0x01u;
static constexpr uint8_t kSocketFactoryFlagKeepBlocking = 0x02u;

struct CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock {
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock* next;
};

struct CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode {
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode* next;
};

struct CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState {
    uint32_t objectCountPerBackingBlock = 0u;
    uint32_t backingBlockCount = 0u;
    size_t backingBlockBytes = 0u;
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock* backingBlockListHead = nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode* freeListHead = nullptr;
    uint32_t extraObjectBytes = 0u;
    CRITICAL_SECTION criticalSection = {};
    std::once_flag initOnce;
    bool initialized = false;
};

// Source-owned aggregate over the original connection-status work-item pool globals.
// This is not one byte-faithful launcher.exe global by itself; it groups the recovered pieces:
// - anchor: launcher.exe:0x004cb410 = g_ConnectionStatusWorkItemPoolLockHelper
// - anchor: launcher.exe:0x004f76c0 = g_ConnectionStatusWorkItemPoolObjectCountPerBackingBlock
// - anchor: launcher.exe:0x004f76c4 = g_ConnectionStatusWorkItemPoolBackingBlockCount
// - anchor: launcher.exe:0x004f76c8 = g_ConnectionStatusWorkItemPoolBackingBlockBytes
// - anchor: launcher.exe:0x004f76cc = g_ConnectionStatusWorkItemPoolBackingBlockListHead
// - anchor: launcher.exe:0x004f76d0 = g_ConnectionStatusWorkItemPoolFreeListHead
// - anchor: launcher.exe:0x004f76d4 = g_ConnectionStatusWorkItemPoolExtraObjectBytes
static CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState g_ConnectionStatusWorkItemPoolState;
// Source-owned aggregate over the original close-work-item pool globals.
// This is not one byte-faithful launcher.exe global by itself; it groups the recovered pieces:
// - anchor: launcher.exe:0x004cb42c = g_CloseWorkItemPoolLockHelper
// - anchor: launcher.exe:0x004f76d8 = g_CloseWorkItemPoolObjectCountPerBackingBlock
// - anchor: launcher.exe:0x004f76dc = g_CloseWorkItemPoolBackingBlockCount
// - anchor: launcher.exe:0x004f76e0 = g_CloseWorkItemPoolBackingBlockBytes
// - anchor: launcher.exe:0x004f76e4 = g_CloseWorkItemPoolBackingBlockListHead
// - anchor: launcher.exe:0x004f76e8 = g_CloseWorkItemPoolFreeListHead
// - anchor: launcher.exe:0x004f76ec = g_CloseWorkItemPoolExtraObjectBytes
static CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState g_CloseWorkItemPoolState;

static void SmallWorkItemPool_FreeStorageScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState,
    void* storage);
static void CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_Clear();
static void CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_Clear();

// UNANCHORED: source-owned narrow mirror of the original queue block free-list behavior.
// Static RE already shows that the consumer path recycles exhausted blocks instead of treating the
// transition as a simple free-and-forget step. Current source keeps that narrower behavior in a
// side cache keyed by the queue object while the exact original in-object free-list plumbing is
// still unrecovered.
static std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768_Queue*, std::vector<uint32_t*>>
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
// - recovered runtime payload families are tracked separately from source-only launcher ABI-shell
//   baggage using node shapes that match the launcher tree families rather than source vectors

static_assert(sizeof(std::_Rb_tree_node_base) == 0x10, "launcher tree node-base size mismatch");
using CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode =
    std::_Rb_tree_node<std::pair<LTTCPEndpointKey_0x44b070, CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread*>>;
using CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode =
    std::_Rb_tree_node<std::pair<uint32_t, CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread*>>;
static_assert(sizeof(std::pair<LTTCPEndpointKey_0x44b070, CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread*>) == 0x14, "endpoint tree value size mismatch");
static_assert(sizeof(std::pair<uint32_t, CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread*>) == 0x8, "context tree value size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode) == 0x24, "endpoint tree node size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode) == 0x18, "context tree node size mismatch");

struct CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry {
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> payload;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode node = {};
};

struct CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry {
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> payload;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode node = {};
};

struct CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking {
    std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*, std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry>> entries;
};

struct CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking {
    std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*, std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry>> entries;
};

static std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768*, CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment>
    g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments;
static std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768*, CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking>
    g_CLTThreadPerClientTCPEngine_0x4b2768EndpointPayloadBackings;
static std::unordered_map<CLTThreadPerClientTCPEngine_0x4b2768*, CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking>
    g_CLTThreadPerClientTCPEngine_0x4b2768ContextPayloadBackings;

static CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* FindEngineLauncherAbiAttachment(
    const CLTThreadPerClientTCPEngine_0x4b2768* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments.find(
        const_cast<CLTThreadPerClientTCPEngine_0x4b2768*>(self));
    return (it != g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments.end()) ? &it->second
                                                                          : nullptr;
}

static CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment& EnsureEngineLauncherAbiAttachment(
    CLTThreadPerClientTCPEngine_0x4b2768* self) {
    return g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments[self];
}

static CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking* FindEngineEndpointPayloadBacking(
    const CLTThreadPerClientTCPEngine_0x4b2768* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngine_0x4b2768EndpointPayloadBackings.find(
        const_cast<CLTThreadPerClientTCPEngine_0x4b2768*>(self));
    return (it != g_CLTThreadPerClientTCPEngine_0x4b2768EndpointPayloadBackings.end()) ? &it->second : nullptr;
}

static CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking& EnsureEngineEndpointPayloadBacking(
    CLTThreadPerClientTCPEngine_0x4b2768* self) {
    return g_CLTThreadPerClientTCPEngine_0x4b2768EndpointPayloadBackings[self];
}

static CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking* FindEngineContextPayloadBacking(
    const CLTThreadPerClientTCPEngine_0x4b2768* self) {
    if (!self) {
        return nullptr;
    }
    auto it = g_CLTThreadPerClientTCPEngine_0x4b2768ContextPayloadBackings.find(
        const_cast<CLTThreadPerClientTCPEngine_0x4b2768*>(self));
    return (it != g_CLTThreadPerClientTCPEngine_0x4b2768ContextPayloadBackings.end()) ? &it->second : nullptr;
}

static CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking& EnsureEngineContextPayloadBacking(
    CLTThreadPerClientTCPEngine_0x4b2768* self) {
    return g_CLTThreadPerClientTCPEngine_0x4b2768ContextPayloadBackings[self];
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
    const LTTCPEndpointKey_0x44b070& lhs,
    const LTTCPEndpointKey_0x44b070& rhs) {
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
static CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* EndpointTreeFindNode(
    const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    const LTTCPEndpointKey_0x44b070& key) {
    return LauncherTreeFindNode<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode>(
        head,
        key,
        CompareEndpointTreeKeys);
}

static CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry* FindEngineEndpointPayloadEntry(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node) {
    if (!node) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking* backing =
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
static CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* EndpointTreeInsertUniqueNode(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    const LTTCPEndpointKey_0x44b070& key,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking& backing =
        EnsureEngineEndpointPayloadBacking(self);
    auto entry = std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry>();
    if (!entry) {
        return nullptr;
    }

    entry->node._M_valptr()->first = key;
    entry->node._M_valptr()->second = nullptr;

    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    std::_Rb_tree_node_base* parent = header;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* current =
        TreeRootNode<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode>(head);
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
            ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(current->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(current->_M_right);
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* insertedNode = &entry->node;
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
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node,
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> payload) {
    if (!payload) {
        return false;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry* entry =
        FindEngineEndpointPayloadEntry(self, node);
    if (!entry) {
        return false;
    }

    node->_M_valptr()->second = payload.get();
    entry->payload = std::move(payload);
    return true;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> EndpointTreeDetachPayload(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node) {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadEntry* entry =
        FindEngineEndpointPayloadEntry(self, node);
    if (!entry) {
        return nullptr;
    }

    node->_M_valptr()->second = nullptr;
    return std::move(entry->payload);
}

// anchor: launcher.exe:0x42fe10
static CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* ContextTreeFindNode(
    const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    uint32_t key) {
    return LauncherTreeFindNode<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode>(
        head,
        key,
        CompareContextTreeKeys);
}

static CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry* FindEngineContextPayloadEntry(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node) {
    if (!node) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking* backing =
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
static CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* ContextTreeInsertUniqueNode(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    uint32_t key,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking& backing =
        EnsureEngineContextPayloadBacking(self);
    auto entry = std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry>();
    if (!entry) {
        return nullptr;
    }

    entry->node._M_valptr()->first = key;
    entry->node._M_valptr()->second = nullptr;

    std::_Rb_tree_node_base* header = TreeHeaderBase(head);
    std::_Rb_tree_node_base* parent = header;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* current =
        TreeRootNode<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode>(head);
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
            ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(current->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(current->_M_right);
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* insertedNode = &entry->node;
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
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node,
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> payload) {
    if (!payload) {
        return false;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry* entry =
        FindEngineContextPayloadEntry(self, node);
    if (!entry) {
        return false;
    }

    node->_M_valptr()->second = payload.get();
    entry->payload = std::move(payload);
    return true;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> ContextTreeDetachPayload(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node) {
    CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadEntry* entry =
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
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node) {
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
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node) {
    return LauncherTreeEraseOwnedNode(
        FindEngineContextPayloadBacking(self),
        head,
        node);
}

static void EraseEngineBackings(CLTThreadPerClientTCPEngine_0x4b2768* self) {
    g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments.erase(self);
    g_CLTThreadPerClientTCPEngine_0x4b2768EndpointPayloadBackings.erase(self);
    g_CLTThreadPerClientTCPEngine_0x4b2768ContextPayloadBackings.erase(self);
}

static const char* EngineWorkTypeName(uint32_t workType) {
    switch (workType) {
        case CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose:
            return "Close";
        case CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus:
            return "ConnectionStatus";
        case CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeParsedPacket:
            return "ParsedPacket";
        case CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeSyntheticReceiveDrain:
            return "SyntheticReceiveDrain";
        default:
            return "Unknown";
    }
}

// anchor: launcher.exe:0x435c30 / vtable `0x004b3df8`
static uint32_t __thiscall ConnectionStatusWorkItem_ReleaseScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* self) {
    if (!self) {
        return 1u;
    }

    // Source-owned narrowed stand-in for the deleting-dtor free-list return path in `0x435c30`.
    SmallWorkItemPool_FreeStorageScaffold(&g_ConnectionStatusWorkItemPoolState, self);
    return 1u;
}

// anchor: launcher.exe:0x435c80 / vtable `0x004b3e00`
static uint32_t __thiscall CloseWorkItem_ReleaseScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold* self) {
    if (!self) {
        return 1u;
    }

    // Source-owned narrowed stand-in for the deleting-dtor free-list return path in `0x435c80`.
    SmallWorkItemPool_FreeStorageScaffold(&g_CloseWorkItemPoolState, self);
    return 1u;
}

static void EnsureSmallConnectionWorkItemVtablesInitialized() {
    if (!g_ConnectionStatusWorkItemVtable[1]) {
        g_ConnectionStatusWorkItemVtable[1] =
            reinterpret_cast<void*>(ConnectionStatusWorkItem_ReleaseScaffold);
    }
    if (!g_CloseWorkItemVtable[1]) {
        g_CloseWorkItemVtable[1] =
            reinterpret_cast<void*>(CloseWorkItem_ReleaseScaffold);
    }
}

// UNANCHORED source-owned helper shared by the small fixed allocators rooted at `0x435720`
// and `0x435840`.
static uint32_t SmallWorkItemPool_SystemPageSizeScaffold() {
    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);
    return static_cast<uint32_t>(systemInfo.dwPageSize);
}

// UNANCHORED source-owned helper shared by the small fixed allocators rooted at `0x435720`
// and `0x435840`.
static void SmallWorkItemPool_InitScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState) {
    if (!poolState) {
        return;
    }

    InitializeCriticalSection(&poolState->criticalSection);
    poolState->extraObjectBytes = 0u;
    poolState->initialized = true;
}

// UNANCHORED source-owned helper shared by the small fixed allocators rooted at `0x4351b0`
// and `0x435240`.
static void SmallWorkItemPool_ClearScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState) {
    if (!poolState || !poolState->initialized) {
        return;
    }

    EnterCriticalSection(&poolState->criticalSection);
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock* backingBlock =
        poolState->backingBlockListHead;
    while (backingBlock) {
        CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock* nextBackingBlock =
            backingBlock->next;
        std::free(backingBlock);
        backingBlock = nextBackingBlock;
    }
    poolState->backingBlockListHead = nullptr;
    poolState->freeListHead = nullptr;
    poolState->backingBlockCount = 0u;
    LeaveCriticalSection(&poolState->criticalSection);
}

// UNANCHORED source-owned helper shared by the small fixed allocators rooted at `0x435720`
// and `0x435840`.
static void* SmallWorkItemPool_AllocateStorageScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState,
    uint32_t objectBaseBytes) {
    if (!poolState) {
        return nullptr;
    }

    const size_t objectStride =
        static_cast<size_t>(poolState->extraObjectBytes) + objectBaseBytes;
    EnterCriticalSection(&poolState->criticalSection);

    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode* storage = poolState->freeListHead;
    if (!storage) {
        if (!poolState->backingBlockListHead) {
            if (poolState->objectCountPerBackingBlock == 0u) {
                poolState->objectCountPerBackingBlock = 1u;
            }

            size_t backingPayloadBytes =
                static_cast<size_t>(poolState->objectCountPerBackingBlock) * objectStride;
            const size_t preferredBackingPayloadBytes =
                (static_cast<size_t>(SmallWorkItemPool_SystemPageSizeScaffold()) >> 1u) -
                sizeof(CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock);
            if (backingPayloadBytes < preferredBackingPayloadBytes) {
                poolState->objectCountPerBackingBlock =
                    static_cast<uint32_t>(preferredBackingPayloadBytes / objectStride);
                backingPayloadBytes =
                    static_cast<size_t>(poolState->objectCountPerBackingBlock) * objectStride;
            }
            poolState->backingBlockBytes =
                backingPayloadBytes +
                sizeof(CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock);
        }

        CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock* backingBlock =
            static_cast<CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolBackingBlock*>(
                std::malloc(poolState->backingBlockBytes));
        if (!backingBlock) {
            LeaveCriticalSection(&poolState->criticalSection);
            return nullptr;
        }

        backingBlock->next = poolState->backingBlockListHead;
        ++poolState->backingBlockCount;
        uint8_t* objectStorage = reinterpret_cast<uint8_t*>(backingBlock + 1);
        CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode* previousFreeListHead =
            poolState->freeListHead;
        poolState->backingBlockListHead = backingBlock;
        for (uint32_t remaining = poolState->objectCountPerBackingBlock;
             remaining != 0u;
             --remaining) {
            auto* freeListNode =
                reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode*>(
                    objectStorage);
            freeListNode->next = previousFreeListHead;
            previousFreeListHead = freeListNode;
            objectStorage += objectStride;
        }
        storage = previousFreeListHead;
    }

    poolState->freeListHead = storage ? storage->next : nullptr;
    LeaveCriticalSection(&poolState->criticalSection);
    return storage;
}

// UNANCHORED source-owned helper shared by the small fixed allocators rooted at `0x435c30`
// and `0x435c80`.
static void SmallWorkItemPool_FreeStorageScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState,
    void* storage) {
    if (!poolState || !storage || !poolState->initialized) {
        return;
    }

    EnterCriticalSection(&poolState->criticalSection);
    auto* freeListNode =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolFreeListNode*>(storage);
    freeListNode->next = poolState->freeListHead;
    poolState->freeListHead = freeListNode;
    LeaveCriticalSection(&poolState->criticalSection);
}

static void ConnectionStatusWorkItemPool_ShutdownScaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_Clear();
    if (g_ConnectionStatusWorkItemPoolState.initialized) {
        DeleteCriticalSection(&g_ConnectionStatusWorkItemPoolState.criticalSection);
        g_ConnectionStatusWorkItemPoolState.initialized = false;
    }
}

static void CloseWorkItemPool_ShutdownScaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_Clear();
    if (g_CloseWorkItemPoolState.initialized) {
        DeleteCriticalSection(&g_CloseWorkItemPoolState.criticalSection);
        g_CloseWorkItemPoolState.initialized = false;
    }
}

static void EnsureConnectionStatusWorkItemPoolInitializedScaffold() {
    std::call_once(
        g_ConnectionStatusWorkItemPoolState.initOnce,
        []() {
            SmallWorkItemPool_InitScaffold(&g_ConnectionStatusWorkItemPoolState);
            std::atexit(ConnectionStatusWorkItemPool_ShutdownScaffold);
        });
}

static void EnsureCloseWorkItemPoolInitializedScaffold() {
    std::call_once(
        g_CloseWorkItemPoolState.initOnce,
        []() {
            SmallWorkItemPool_InitScaffold(&g_CloseWorkItemPoolState);
            std::atexit(CloseWorkItemPool_ShutdownScaffold);
        });
}

// anchor: launcher.exe:0x4351b0
static void CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_Clear() {
    SmallWorkItemPool_ClearScaffold(&g_ConnectionStatusWorkItemPoolState);
}

// anchor: launcher.exe:0x435240
static void CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_Clear() {
    SmallWorkItemPool_ClearScaffold(&g_CloseWorkItemPoolState);
}

// anchor: launcher.exe:0x435720
static void* CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_AllocateStorage(
    uint32_t objectBaseBytes) {
    EnsureConnectionStatusWorkItemPoolInitializedScaffold();
    return SmallWorkItemPool_AllocateStorageScaffold(
        &g_ConnectionStatusWorkItemPoolState,
        objectBaseBytes);
}

// anchor: launcher.exe:0x435d90
static CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold*
CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate() {
    return static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold*>(
        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_AllocateStorage(
            sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold)));
}

// anchor: launcher.exe:0x435050
static CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold*
CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* self,
    uint32_t statusOrPayloadDword08) {
    if (!self) {
        return nullptr;
    }

    EnsureSmallConnectionWorkItemVtablesInitialized();
    self->header.workType = CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus;
    self->header.vtable = g_ConnectionStatusWorkItemVtable;
    self->header.statusOrPayloadDword08 = statusOrPayloadDword08;
    return self;
}

// anchor: launcher.exe:0x435840
static void* CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_AllocateStorage(
    uint32_t objectBaseBytes) {
    EnsureCloseWorkItemPoolInitializedScaffold();
    return SmallWorkItemPool_AllocateStorageScaffold(
        &g_CloseWorkItemPoolState,
        objectBaseBytes);
}

// anchor: launcher.exe:0x435da0
static CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold*
CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate() {
    return static_cast<CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold*>(
        CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_AllocateStorage(
            sizeof(CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold)));
}

// anchor: launcher.exe:0x435070
static CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold*
CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
    CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold* self) {
    if (!self) {
        return nullptr;
    }

    EnsureSmallConnectionWorkItemVtablesInitialized();
    self->header.workType = CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose;
    self->header.statusOrPayloadDword08 = 0u;
    self->header.vtable = g_CloseWorkItemVtable;
    return self;
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
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
    uint32_t* block) {
    if (!queue || !block) {
        return;
    }
    g_QueueRecycledBlocks[queue].push_back(block);
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static uint32_t* QueueTakeRecycledBlockScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue) {
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
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue) {
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
    if (!CLTSocketLayer::Init()) {
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

// anchor: launcher.exe:0x449b40
static uint32_t CLTIPSocket_StaticAllocateSocket(
    int socketType,
    int protocol,
    uint8_t flags) {
    if (!CLTSocketLayer::Init()) {
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
    const uint32_t listenSocketHandle = CLTIPSocket_StaticAllocateSocket(
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
    const uint32_t udpSocketHandle = CLTIPSocket_StaticAllocateSocket(
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
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
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
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
    const CLTThreadPerClientTCPEngine_0x4b2768_QueuePair* pendingPair) {
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

    // anchor: launcher.exe:0x4528d8
    // virtual self-dispatch through vtable slot +0x10 (`IsRunning`) before attempting
    // `_beginthreadex`, rather than open-coding the wait probe here.
    if (IsRunning()) {
        return kStartAlreadyRunning;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (threadHandle_ != 0) {
            CloseHandle(reinterpret_cast<HANDLE>(threadHandle_));
            threadHandle_ = 0;
            threadId_ = 0;
            running_ = false;
        }
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
        isCurrentThread = IsCurrentThread();
        if (isCurrentThread) {
            ++suspendDepth_;
        }
    }

    if (isCurrentThread) {
        return false;
    }

    return ResumeThread(reinterpret_cast<HANDLE>(threadHandle)) != 0xffffffffu;
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

    // anchor: launcher.exe:0x452685
    // external stop path also self-dispatches through vtable slot +0x10 (`IsRunning`) while
    // deciding whether the handle still needs force-termination.
    if (!IsRunning()) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
        return 0;
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
    int previousSuspendDepth = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
        previousSuspendDepth = suspendDepth_;
        if (previousSuspendDepth > 0) {
            --suspendDepth_;
        }
    }

    if (previousSuspendDepth <= 0) {
        (void)SuspendThread(reinterpret_cast<HANDLE>(threadHandle));
    }
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
CLTThreadPerClientTCPEngine_0x4b2768_QueueThread::CLTThreadPerClientTCPEngine_0x4b2768_QueueThread(
    CLTThreadPerClientTCPEngine_0x4b2768* owner)
    : CLTThread("ILTTCPEngine::QueueThread"),
      owner_(owner) {}

// UNANCHORED: current vtable family keeps the shared CLTThread deleting dtor at slot +0x2c
CLTThreadPerClientTCPEngine_0x4b2768_QueueThread::~CLTThreadPerClientTCPEngine_0x4b2768_QueueThread() = default;

// UNANCHORED: scaffold accessor for the recovered child +0x38 owner field
CLTThreadPerClientTCPEngine_0x4b2768* CLTThreadPerClientTCPEngine_0x4b2768_QueueThread::Owner() const {
    return owner_;
}

// anchor: launcher.exe:0x436fc0
void CLTThreadPerClientTCPEngine_0x4b2768_QueueThread::Run() {
    if (owner_) {
        owner_->RunCompletedOperationQueue(/*nonBlocking=*/false);
    }
}

// anchor: launcher.exe:0x431ab0
CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread(
    uint32_t listenSocketHandle,
    void* ownerContext)
    : CLTThread("CLTThreadPerClientTCPEngine_0x4b2768::AcceptThread"),
      ownerContext_(ownerContext),
      listenSocketHandle_(listenSocketHandle),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()) {}

// anchor: launcher.exe:0x431b30 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::~CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 owner/context field
void* CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::OwnerContext() const {
    return ownerContext_;
}

// UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
uint32_t CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::WakeupSocketHandle() const {
    return wakeupSocketHandle_;
}

// UNANCHORED: source-owned bridge for the original external closesocket([payload+0x3c]) seam.
void CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::CloseListenSocketScaffold() {
    CloseSocketHandle(&listenSocketHandle_);
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x432070
void CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread::Run() {
    // Current source ownership only models the class/vtable/wakeup surface.
    // The full accept loop remains a later fidelity target.
}

// anchor: launcher.exe:0x431b60
CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread(
    void* contextKey,
    bool datagramMode)
    : CLTThread("CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread"),
      contextKey_(contextKey),
      datagramMode_(datagramMode),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()),
      exitRequested_(false) {}

// anchor: launcher.exe:0x431be0 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::~CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 context/connection key field
void* CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::ContextKey() const {
    return contextKey_;
}

// UNANCHORED: source-owned bridge for the recovered child +0x44 exit-request byte
void CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::RequestExit() {
    exitRequested_ = true;
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x42fe50
void CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread::Run() {
    CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(contextKey_);
    if (!connection) {
        return;
    }

    CLTThreadPerClientTCPEngine_0x4b2768* engine = connection->Engine();
    if (!engine || wakeupSocketHandle_ == kInvalidSocketHandle) {
        return;
    }

    const bool isMarginConnection = dynamic_cast<CMarginConnection_0x4aff38*>(connection) != nullptr;
    const char* connectStatusLabel =
        isMarginConnection ? "MarginConnectStatus" : "AuthConnectStatus";
    const char* closeStatusLabel =
        isMarginConnection ? "MarginPeerClosed" : "AuthPeerClosed";
    spdlog::debug(
        "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread Run connection={} ownerContext={} isMargin={} wakeupSocket=0x{:08x} initialState={} connectCompletionPending={}",
        fmt::ptr(connection),
        fmt::ptr(connection->OwnerContext()),
        isMarginConnection ? 1u : 0u,
        wakeupSocketHandle_,
        static_cast<unsigned>(connection->State()),
        !datagramMode_ ? 1u : 0u);

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
    CLTTCPConnection_QueuedSendBufferWithEndpoint currentSend = {};
    size_t currentSendOffset = 0u;

    const auto queueConnectStatus =
        [&](uint32_t workPayload) {
            if (connectStatusQueued) {
                return;
            }
            const bool queued = engine->EnqueueDirectConnectionStatusWorkItemScaffold(
                connection,
                CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus,
                workPayload,
                connectStatusLabel,
                /*queueLockAlreadyHeld=*/false);
            if (!queued) {
                spdlog::warn(
                    "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread direct queue connect status failed connection={} ownerContext={} payload=0x{:08x}",
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
        const bool queued = engine->EnqueueDirectConnectionStatusWorkItemScaffold(
            connection,
            CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose,
            0u,
            closeStatusLabel,
            /*queueLockAlreadyHeld=*/false);
        if (!queued) {
            spdlog::warn(
                "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread direct queue close failed connection={} ownerContext={} state={}",
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
            connection->SendQueueEmptyFlag()) {
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
            if (closeQueued || connectStatusQueued || connection->State() == LTTCPEngineConnectionState::kClosed) {
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
            currentSendOffset < static_cast<size_t>(currentSend.sendBufferStorage00.BufferByteCount());
        bool monitorWrite = false;
        if (connectCompletionPending) {
            monitorWrite = true;
        } else if (hasCurrentSend) {
            monitorWrite = true;
        } else {
            if (connection->TryPopQueuedSendBufferWithEndpoint(&currentSend)) {
                currentSendOffset = 0u;
                monitorWrite = currentSend.sendBufferStorage00.BufferByteCount() != 0u;
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
                waitWakeupOnly = true;
                continue;
            }

            spdlog::debug(
                "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread select failed connection={} socket=0x{:08x} wsaError={}",
                fmt::ptr(connection),
                socketHandle,
                wsaError);
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
            waitWakeupOnly = true;
            continue;
        }

        if (FD_ISSET(socket, &readSet)) {
            while (true) {
                uint32_t wsaError = 0u;
                bool peerClosed = false;
                CLTTCPReadOperation* readOperationFragment =
                    new (std::nothrow) CLTTCPReadOperation;
                if (!readOperationFragment) {
                    spdlog::warn(
                        "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread recv fragment allocation failed connection={} socket=0x{:08x}",
                        fmt::ptr(connection),
                        socketHandle);
                    break;
                }

                // anchor: launcher.exe:0x42fe50 recv / deliver subpath
                // The original worker thread owns this fragment lifecycle directly rather than
                // splitting it into a separate connection helper boundary.
                readOperationFragment->referenceCount04 = 0;
                readOperationFragment->byteCount08 = 0u;
                readOperationFragment->AddRef();
                const int receiveResult = recv(
                    socket,
                    reinterpret_cast<char*>(readOperationFragment + 1),
                    0x1000,
                    0);
                if (receiveResult > 0) {
                    readOperationFragment->SetByteCount(static_cast<uint32_t>(receiveResult));
                    readOperationFragment->AddRef();
                    connection->OnReceive(readOperationFragment);
                    readOperationFragment->Release();
                    continue;
                }

                // anchor: launcher.exe:0x449fd0
                // Terminal recv cleanup flows through the connection close-callback seam.
                connection->OnClose(readOperationFragment, nullptr, nullptr);
                if (receiveResult == 0) {
                    peerClosed = true;
                } else {
                    wsaError = static_cast<uint32_t>(WSAGetLastError());
                    if (wsaError == WSAEWOULDBLOCK) {
                        break;
                    }
                }

                spdlog::debug(
                    "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread terminal recv connection={} socket=0x{:08x} peerClosed={} wsaError={}",
                    fmt::ptr(connection),
                    socketHandle,
                    peerClosed ? 1u : 0u,
                    wsaError);
                closeAndInvalidateSocket();
                connection->SetState(LTTCPEngineConnectionState::kClosed);
                queueClose();
                waitWakeupOnly = true;
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
                    waitWakeupOnly = true;
                    continue;
                }
            } else if (currentSendOffset <
                       static_cast<size_t>(currentSend.sendBufferStorage00.BufferByteCount())) {
                const int remainingByteCount = static_cast<int>(
                    currentSend.sendBufferStorage00.BufferByteCount() - currentSendOffset);
                const int sentByteCount = send(
                    socket,
                    reinterpret_cast<const char*>(
                        currentSend.sendBufferStorage00.BufferBytes() + currentSendOffset),
                    remainingByteCount,
                    0);
                if (sentByteCount != SOCKET_ERROR) {
                    currentSendOffset += static_cast<size_t>(sentByteCount);
                    if (currentSendOffset >=
                        static_cast<size_t>(currentSend.sendBufferStorage00.BufferByteCount())) {
                        currentSend = {};
                        currentSendOffset = 0u;
                    }
                } else {
                    const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
                    if (wsaError != WSAEWOULDBLOCK) {
                        spdlog::debug(
                            "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread send failed connection={} socket=0x{:08x} wsaError={} remaining={}",
                            fmt::ptr(connection),
                            socketHandle,
                            wsaError,
                            remainingByteCount);
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

    connection->SetWorkerThreadScaffold(nullptr);
}

// anchor: launcher.exe:0x436340
void CLTThreadPerClientTCPEngine_0x4b2768::Queue_Free(CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue) {
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
bool CLTThreadPerClientTCPEngine_0x4b2768::Queue_Init(
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
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
void CLTThreadPerClientTCPEngine_0x4b2768::Queue_PushPair(
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
    uint32_t value0,
    uint32_t value1) {
    if (!queue || !queue->current1) {
        return;
    }

    uint8_t* lastPairInBlock = queue->end1 ? (static_cast<uint8_t*>(queue->end1) - 8) : nullptr;
    if (static_cast<void*>(queue->current1) == static_cast<void*>(lastPairInBlock)) {
        CLTThreadPerClientTCPEngine_0x4b2768_QueuePair pair = {value0, value1};
        (void)GrowQueue(queue, &pair);
        return;
    }

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    current1[0] = value0;
    current1[1] = value1;
    queue->current1 = current1 + 2;
}

// anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
bool CLTThreadPerClientTCPEngine_0x4b2768::Queue_IsEmpty(const CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue) {
    return !queue || !queue->current0 || (queue->current1 == queue->current0);
}

// anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
bool CLTThreadPerClientTCPEngine_0x4b2768::Queue_TryPopPair(
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* queue,
    CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair* outPair) {
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
    const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
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
// Active source may dequeue either the direct connection object or the explicit queue-dispatch ABI
// adapter used when raw client.dll consumers are still on the path.
static void QueuedConnection_OnOperationCompleted(
    void* queuedContext,
    CBaseConnection* queuedConnection,
    void* workItem) {
    if (queuedConnection) {
        (void)queuedConnection->OnOperationCompleted(workItem);
        return;
    }

    if (!queuedContext) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(queuedContext);
    if (!vtable || !vtable[4]) {
        return;
    }

    typedef uint32_t (__thiscall *OnOperationCompletedFn)(void*, void*);
    OnOperationCompletedFn fn = reinterpret_cast<OnOperationCompletedFn>(vtable[4]);
    (void)fn(queuedContext, workItem);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the `(char)context[1]` test in 0x436d31..0x436ee7`.
static bool QueuedConnection_ShouldAutoReleaseAfterType1(
    void* queuedContext,
    const CBaseConnection* queuedConnection) {
    if (CBaseConnection_QueueContextScaffold* queueContext =
            static_cast<CBaseConnection_QueueContextScaffold*>(queuedContext);
        queueContext != nullptr &&
        CBaseConnection_FromQueueContextScaffold(queuedContext) != nullptr) {
        return queueContext->autoReleaseFlag != 0u;
    }
    return queuedConnection != nullptr && queuedConnection->AutoReleaseFlag04() != 0u;
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Adapter contexts still expose a source-owned release bridge at slot `+0x04`; direct connection
// objects keep the recovered base `+0x04` flag modeled but do not yet have a recovered release
// target on an active path.
static void QueuedConnection_ReleaseAfterType1(
    void* queuedContext,
    CBaseConnection* queuedConnection) {
    if (CBaseConnection_QueueContextScaffold* queueContext =
            static_cast<CBaseConnection_QueueContextScaffold*>(queuedContext);
        queueContext != nullptr &&
        CBaseConnection_FromQueueContextScaffold(queuedContext) != nullptr) {
        void** vtable = *reinterpret_cast<void***>(queueContext);
        if (!vtable || !vtable[1]) {
            return;
        }

        typedef uint32_t (__thiscall *ReleaseFn)(void*);
        ReleaseFn fn = reinterpret_cast<ReleaseFn>(vtable[1]);
        (void)fn(queueContext);
        return;
    }

    if (!queuedConnection || queuedConnection->AutoReleaseFlag04() == 0u) {
        return;
    }

    spdlog::warn(
        "QueuedConnection_ReleaseAfterType1 encountered unimplemented direct-connection auto-release queuedConnection={}",
        fmt::ptr(queuedConnection));
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
void CLTThreadPerClientTCPEngine_0x4b2768::InitializeLockHelperScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold* helper) {
    if (!helper) {
        return;
    }

    std::memset(&helper->crit, 0, sizeof(helper->crit));
    InitializeCriticalSection(&helper->crit);
}

void CLTThreadPerClientTCPEngine_0x4b2768::DeleteLockHelperScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold* helper) {
    if (!helper) {
        return;
    }

    DeleteCriticalSection(&helper->crit);
}

void CLTThreadPerClientTCPEngine_0x4b2768::InitializeEndpointTreeHead24(
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head) {
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

void CLTThreadPerClientTCPEngine_0x4b2768::InitializeContextTreeHead18(
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head) {
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

CLTThreadPerClientTCPEngine_0x4b2768::CLTThreadPerClientTCPEngine_0x4b2768()
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
    // - active auth/margin worker flow now stays on the direct connection object and its direct
    //   mediator owner pointer at `+0xa4`, not a mediator-owned bridge context
    // - queued completed-operation identity is still the direct connection object in launcher.exe,
    //   but the active replacement may wrap that object in a tiny queue-dispatch ABI adapter before
    //   it crosses into raw client.dll queue consumers compiled against the original MSVC vtable
    //   layout
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
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24)));
    if (ownedEndpointTreeHead80_) {
        InitializeEndpointTreeHead24(ownedEndpointTreeHead80_);
    }

    ownedContextTreeHead8C_ =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18)));
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
CLTThreadPerClientTCPEngine_0x4b2768::~CLTThreadPerClientTCPEngine_0x4b2768() {
    DetachLauncherAbiSurfaceScaffold();
    RebuildQueueThreadsForCtorCount(/*queueThreadCount=*/0);

    if (CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking* endpointBacking =
            FindEngineEndpointPayloadBacking(this)) {
        for (auto& it : endpointBacking->entries) {
            StopAcceptThreadScaffold(it.second->payload.get());
        }
        endpointBacking->entries.clear();
    }

    if (CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking* contextBacking =
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
int CLTThreadPerClientTCPEngine_0x4b2768::Release(uint32_t flags) {
    // Current sidecar owner still handles the real arg5 object lifetime/teardown.
    // Keep the primary-slot surface source-owned here so wrappers can forward through
    // ILTTCPEngine without open-coding placeholder returns.
    (void)flags;
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) {
    if (!ownerContext) {
        return 4u;
    }

    const uint32_t ipv4NetworkOrder = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reservedArg3));
    const LTTCPEndpointKey_0x44b070 key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    bool inserted = false;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node = EndpointTreeInsertUniqueNode(
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

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> acceptThread =
        std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread>(
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

    if (CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* payload = node->_M_valptr()->second) {
        (void)payload->Start(/*startPriority=*/2);
    }
    SyncAttachedLauncherObjectStateScaffold();
    return 0u;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ipv4NetworkOrder) {
    CMessageConnection_0x4b7928* connection = ResolveConnectionForEngineSlotScaffold(contextKey);
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
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = CreateAndInsertWorkerThread(
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
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ipv4NetworkOrder) {
    // Current best static read: thin helper around slot 2 / UDPMonitorPort(port=0, ...)
    // followed by getsockname/ntohs to report the chosen local port.
    const uint32_t result = UDPMonitorPort(/*portHostOrder=*/0, contextKey, ipv4NetworkOrder);
    if (result == 0u && outBoundPortHostOrder) {
        *outBoundPortHostOrder = 0;
        if (CMessageConnection_0x4b7928* connection = ResolveConnectionForEngineSlotScaffold(contextKey)) {
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
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::Slot4_42F7C0(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::UnmonitorPort(uint16_t portHostOrder, void** outOwnerContext, uint32_t ipv4NetworkOrder) {
    const LTTCPEndpointKey_0x44b070 key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node = EndpointTree_Find(key);
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* endpointTreeSentinel =
        reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(ownedEndpointTreeHead80_);
    if (node == endpointTreeSentinel || !(node)->_M_valptr()->second) {
        if (outOwnerContext) {
            *outOwnerContext = nullptr;
        }
        return kResultEndpointNotFound;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> acceptThread =
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

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::Connect(void* contextKey) {
    // Current best static read of `0x4328a0`:
    // - public caller `0x449cd0` passes the direct connection object, not a synthetic resolver key
    // - rejected non-CLOSED state queues type-2 payload `0x7000001`
    // - immediate socket/bind failure flows share the later type-2 payload `1` producer tail
    // - immediate non-`WSAEWOULDBLOCK` connect failure first queues a type-1 close work item, then
    //   reaches that same type-2 payload `1` tail
    static constexpr uint32_t kConnectRejectedNotClosedPayload = 0x7000001u;
    static constexpr uint32_t kConnectImmediateFailurePayload = 1u;

    CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(contextKey);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect rejected null connection context={}",
            fmt::ptr(contextKey));
        return 0u;
    }

    void* queuedConnectionContext = connection->QueueContextScaffold();
    const LTTCPEndpointKey_0x44b070& remoteEndpoint = connection->remoteEndpoint_;
    if (connection->State() != LTTCPEngineConnectionState::kClosed) {
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect rejected connection={} state={} port={} ip=0x{:08x}",
            fmt::ptr(connection),
            static_cast<unsigned>(connection->State()),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder));

        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectRejectedNotClosedPayload);
        (void)EnqueueCompletedOperationScaffold(
            connectionStatusWorkItem ? &connectionStatusWorkItem->header : nullptr,
            queuedConnectionContext,
            /*useQueue34=*/false,
            "connect:not-closed",
            /*queueLockAlreadyHeld=*/false);
        return 0u;
    }

    uint32_t socketHandle = CLTIPSocket_StaticAllocateSocket(
        SOCK_STREAM,
        IPPROTO_TCP,
        /*flags=*/0u);
    connection->SetSocketHandle(socketHandle);
    if (socketHandle == kInvalidSocketHandle) {
        const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect socket allocation failed connection={} port={} ip=0x{:08x} wsaError={} ({})",
            fmt::ptr(connection),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
            static_cast<unsigned>(wsaError),
            std::system_category().message(wsaError));

        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectImmediateFailurePayload);
        (void)EnqueueCompletedOperationScaffold(
            connectionStatusWorkItem ? &connectionStatusWorkItem->header : nullptr,
            queuedConnectionContext,
            /*useQueue34=*/false,
            "connect:socket-failed",
            /*queueLockAlreadyHeld=*/false);
        return 0u;
    }

    SOCKET connectSocket = static_cast<SOCKET>(socketHandle);
    sockaddr_in localBindAddress = {};
    localBindAddress.sin_family = AF_INET;
    if (bind(
            connectSocket,
            reinterpret_cast<const sockaddr*>(&localBindAddress),
            sizeof(localBindAddress)) == SOCKET_ERROR) {
        const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
        CloseSocketHandle(&socketHandle);
        connection->SetSocketHandle(socketHandle);
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect bind failed connection={} port={} ip=0x{:08x} wsaError={} ({})",
            fmt::ptr(connection),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
            static_cast<unsigned>(wsaError),
            std::system_category().message(wsaError));

        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectImmediateFailurePayload);
        (void)EnqueueCompletedOperationScaffold(
            connectionStatusWorkItem ? &connectionStatusWorkItem->header : nullptr,
            queuedConnectionContext,
            /*useQueue34=*/false,
            "connect:bind-failed",
            /*queueLockAlreadyHeld=*/false);
        return 0u;
    }

    sockaddr_in remoteSocketAddress = {};
    remoteSocketAddress.sin_family = remoteEndpoint.family;
    remoteSocketAddress.sin_port = remoteEndpoint.portNetworkOrder;
    remoteSocketAddress.sin_addr.s_addr = remoteEndpoint.ipv4NetworkOrder;
    if (connect(
            connectSocket,
            reinterpret_cast<const sockaddr*>(&remoteSocketAddress),
            sizeof(remoteSocketAddress)) == SOCKET_ERROR) {
        const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
        if (wsaError != WSAEWOULDBLOCK) {
            CloseSocketHandle(&socketHandle);
            connection->SetSocketHandle(socketHandle);
            spdlog::info(
                "CLTThreadPerClientTCPEngine_0x4b2768::Connect connect failed connection={} port={} ip=0x{:08x} wsaError={} ({})",
                fmt::ptr(connection),
                static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
                static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
                static_cast<unsigned>(wsaError),
                std::system_category().message(wsaError));

            CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold* closeWorkItem =
                CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
                    CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate());
            (void)EnqueueCompletedOperationScaffold(
                closeWorkItem ? &closeWorkItem->header : nullptr,
                queuedConnectionContext,
                /*useQueue34=*/false,
                "connect:immediate-close",
                /*queueLockAlreadyHeld=*/false);

            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* connectionStatusWorkItem =
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                    kConnectImmediateFailurePayload);
            (void)EnqueueCompletedOperationScaffold(
                connectionStatusWorkItem ? &connectionStatusWorkItem->header : nullptr,
                queuedConnectionContext,
                /*useQueue34=*/false,
                "connect:immediate-status",
                /*queueLockAlreadyHeld=*/false);
            return 0u;
        }
    }

    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread = CreateAndInsertWorkerThread(
        connection,
        /*datagramMode=*/false,
        /*startThread=*/false);
    if (!workerThread) {
        // UNANCHORED bounded source-side guard: current `0x431ff0` mirror can still fail or
        // deduplicate, while launcher.exe `0x4328a0` uses the returned worker directly.
        CloseSocketHandle(&socketHandle);
        connection->SetSocketHandle(socketHandle);
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect worker creation failed connection={} port={} ip=0x{:08x}",
            fmt::ptr(connection),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder));
        return 0u;
    }

    connection->SetState(LTTCPEngineConnectionState::kConnectActive);
    (void)workerThread->Start(/*startPriority=*/3);
    spdlog::info(
        "CLTThreadPerClientTCPEngine_0x4b2768::Connect started worker connection={} worker={} port={} ip=0x{:08x} state={} ownerContext={}",
        fmt::ptr(connection),
        fmt::ptr(workerThread),
        static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
        static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
        static_cast<unsigned>(connection->State()),
        fmt::ptr(connection->OwnerContext()));
    SyncAttachedLauncherObjectStateScaffold();
    return kResultSuccess;
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::Close(void* contextKey, bool graceful) {
    // `0x449ca0` forwards the direct connection object itself into this engine slot.
    // `0x42f970` then works on that connection payload directly; it does not resolve a synthetic
    // owner/context record through the engine's source-owned lookup scaffolds.
    CLTTCPConnection* connection = static_cast<CLTTCPConnection*>(contextKey);
    if (!connection) {
        return 0u;
    }

    const LTTCPEngineConnectionState state = connection->State();
    if (state != LTTCPEngineConnectionState::kConnectActive &&
        state != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    connection->SetState(LTTCPEngineConnectionState::kClosing);
    if (graceful) {
        // anchor: launcher.exe:0x42f997 / connection +0x34 = 4 before any graceful-close branch
        // anchor: launcher.exe:0x42f9a4 / connection +0x38 send-queue-empty byte gate
        // If queued sends are still pending, `0x42f970` returns success immediately and lets the
        // worker-thread write side issue the later half-close once the queue drains.
        if (!connection->SendQueueEmptyFlag()) {
            return 1u;
        }

        // anchor: launcher.exe:0x42f9af / shutdown(socket, 1)
        const int shutdownResult = shutdown(static_cast<SOCKET>(connection->SocketHandle()), SD_SEND);
        if (shutdownResult == SOCKET_ERROR) {
            const LTTCPEndpointKey_0x44b070& remoteEndpoint = connection->remoteEndpoint_;
            const unsigned ipv4Byte0 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 0u) & 0xffu);
            const unsigned ipv4Byte1 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 8u) & 0xffu);
            const unsigned ipv4Byte2 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 16u) & 0xffu);
            const unsigned ipv4Byte3 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 24u) & 0xffu);
            const unsigned portHostOrder = static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder));

            // anchor: launcher.exe:0x42f9c4..0x42fab2
            // Original source-location bookkeeping points at
            // `matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp:0x16e` before
            // this exact warning text.
            spdlog::warn(
                "CLTThreadPerClientTCPEngine_0x4b2768::Close: shutdown() failed on connection to {}.{}.{}.{}:{} with error = {}.",
                ipv4Byte0,
                ipv4Byte1,
                ipv4Byte2,
                ipv4Byte3,
                portHostOrder,
                WSAGetLastError());
        }
        return 1u;
    }

    // anchor: launcher.exe:0x42fac0 / closesocket(socket)
    const int closeResult = closesocket(static_cast<SOCKET>(connection->SocketHandle()));
    if (closeResult == SOCKET_ERROR) {
        const LTTCPEndpointKey_0x44b070& remoteEndpoint = connection->remoteEndpoint_;
        const unsigned ipv4Byte0 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 0u) & 0xffu);
        const unsigned ipv4Byte1 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 8u) & 0xffu);
        const unsigned ipv4Byte2 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 16u) & 0xffu);
        const unsigned ipv4Byte3 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 24u) & 0xffu);
        const unsigned portHostOrder = static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder));

        // anchor: launcher.exe:0x42fad3..0x42fbc0
        // Original source-location bookkeeping points at
        // `matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp:0x175` before
        // this exact warning text.
        spdlog::warn(
            "CLTThreadPerClientTCPEngine_0x4b2768::Close: closesocket() failed on connection to {}.{}.{}.{}:{} with error = {}.",
            ipv4Byte0,
            ipv4Byte1,
            ipv4Byte2,
            ipv4Byte3,
            portHostOrder,
            WSAGetLastError());
    }
    return 1u;
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::SendBuffer(
    const void* buffer,
    uint32_t byteCount,
    void* contextKey,
    void* completionContext) {
    // `0x449d20` forwards `(buffer, byteCount, this, completionContext)` into this engine slot.
    // `0x42fbd0` then works on that direct connection object rather than resolving a synthetic
    // owner/context record through engine-managed lookup scaffolds.
    CLTTCPConnection* connection = static_cast<CLTTCPConnection*>(contextKey);
    if (!connection || !buffer || byteCount == 0u) {
        return 0u;
    }

    const LTTCPEngineConnectionState state = connection->State();
    if (state == LTTCPEngineConnectionState::kConnectActive ||
        state == LTTCPEngineConnectionState::kUdpMonitorActive) {
        // anchor: launcher.exe:0x42fce8..0x42fcf6
        // - queue through the connection-owned `+0x3c` pending-send root via `0x44ad80`
        // - then signal the direct worker-thread wakeup handle at `[connection+0x08] + 0x40`
        const bool queued = connection->QueueSendBuffer(
            buffer,
            byteCount,
            reinterpret_cast<uintptr_t>(completionContext));
        if (!queued) {
            return 0u;
        }

        if (CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = connection->WorkerThreadScaffold()) {
            worker->SignalWakeup();
            return 1u;
        }

        // RE-backed slot `8` users are expected to have `[connection+0x08]` populated by
        // `0x431ff0`; without a worker thread there is no original detached raw-send fallback.
        return 0u;
    }

    const LTTCPEndpointKey_0x44b070& remoteEndpoint = connection->remoteEndpoint_;
    const unsigned ipv4Byte0 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 0u) & 0xffu);
    const unsigned ipv4Byte1 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 8u) & 0xffu);
    const unsigned ipv4Byte2 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 16u) & 0xffu);
    const unsigned ipv4Byte3 = static_cast<unsigned>((remoteEndpoint.ipv4NetworkOrder >> 24u) & 0xffu);
    const unsigned portHostOrder = static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder));

    // anchor: launcher.exe:0x42fbef..0x42fcdc
    // Original source-location bookkeeping points at
    // `matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp:0x181` before this
    // exact warning text.
    spdlog::warn(
        "CLTThreadPerClientTCPEngine_0x4b2768::SendBuffer: Send failed!  Connection obj associated with address {}.{}.{}.{}:{} is not connected/connecting (connstatus = {}).",
        ipv4Byte0,
        ipv4Byte1,
        ipv4Byte2,
        ipv4Byte3,
        portHostOrder,
        static_cast<unsigned>(state));
    return 0u;
}

// anchor: launcher.exe:0x42fd10
// vtable: launcher.exe:0x004b2768 slot +0x24
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::SendBufferWithEndpoint(
    void* buffer,
    uint32_t byteCount,
    LTTCPEndpointKey_0x44b070* remoteEndpoint,
    void* contextKey,
    void* ownershipMode) {
    // Current best static read of `0x42fd10`:
    // - active-state guard is identical to slot `8` / `0x42fbd0` (`state == 1 || state == 2`)
    // - callee is `0x44ac90 = QueueSendBufferWithEndpoint`
    // - wakeup target is the direct worker handle at `[connection+0x08] + 0x40`
    // - original return register is not yet evidenced as a meaningful public result, but the
    //   bounded source mirror keeps the same slot-8-style `0/1` success convention for launcher
    //   stability
    CLTTCPConnection* connection = static_cast<CLTTCPConnection*>(contextKey);
    if (!connection || !remoteEndpoint || !buffer || byteCount == 0u) {
        return 0u;
    }

    const LTTCPEngineConnectionState state = connection->State();
    if (state != LTTCPEngineConnectionState::kConnectActive &&
        state != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    if (!connection->QueueSendBufferWithEndpoint(
            buffer,
            byteCount,
            *remoteEndpoint,
            reinterpret_cast<uintptr_t>(ownershipMode))) {
        return 0u;
    }

    if (CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = connection->WorkerThreadScaffold()) {
        worker->SignalWakeup();
    }
    return 1u;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::Slot10_443810(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) {
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
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::CleanupConnection(void* contextKey) {
    // Current bounded fidelity correction:
    // - original queue consumers dequeue a real connection-family object as `context` before
    //   calling arg5 slot 12 / CleanupConnection
    // - active source accepts both that direct connection identity and the explicit queue-dispatch
    //   ABI adapter still needed on client-facing paths, then searches worker/message tables by the
    //   unwrapped owning connection object
    // - original `0x4316a0` also acquires arg5 helper `+0x98`; after the current ownership move,
    //   that lock behavior now lives here on the target class side and the shell wrapper only
    //   forwards the primary slot call
    // Active retry/deadlock correction:
    // - do not hold the cleanup lock while waiting for a worker thread to stop
    // - detach/erase the worker node under the lock, then stop the worker after releasing it
    // - this keeps new auth-connect worker creation from blocking on the same lock during retry
    (void)EnterCleanupLockHelper();

    void* cleanupContextKey = CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
    bool touchedConnectionState = false;
    uint32_t result = 0u;
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> workerPayload;

    if (CMessageConnection_0x4b7928* connection = FindMessageConnection(cleanupContextKey)) {
        connection->SetState(LTTCPEngineConnectionState::kClosed);
        connection->SetSocketHandle(kInvalidSocketHandle);
        connection->SetWorkerThreadScaffold(nullptr);
        touchedConnectionState = true;
    }

    if (CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = FindWorker(cleanupContextKey)) {
        CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node = ContextTreeFindNode(
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
                "CLTThreadPerClientTCPEngine_0x4b2768::CleanupConnection couldn't find socket/context key={} normalizedKey={}",
                fmt::ptr(contextKey),
                fmt::ptr(cleanupContextKey));
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
void CLTThreadPerClientTCPEngine_0x4b2768::AttachLauncherAbiSurfaceScaffold(
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment& attachment) {
    EnsureEngineLauncherAbiAttachment(this) = attachment;
    SyncAttachedLauncherObjectStateScaffold();
}

// UNANCHORED: launcher ABI-shell detach/reset helper.
void CLTThreadPerClientTCPEngine_0x4b2768::DetachLauncherAbiSurfaceScaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
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

    g_CLTThreadPerClientTCPEngine_0x4b2768LauncherAbiAttachments.erase(this);
}

void CLTThreadPerClientTCPEngine_0x4b2768::RefreshOwnedLauncherMirrorStateScaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointPayloadBacking* endpointBacking = FindEngineEndpointPayloadBacking(this);
    CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking* contextBacking = FindEngineContextPayloadBacking(this);
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
void CLTThreadPerClientTCPEngine_0x4b2768::SyncAttachedLauncherObjectStateScaffold() {
    RefreshOwnedLauncherMirrorStateScaffold();
    CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
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

CLTThreadPerClientTCPEngine_0x4b2768_Queue* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueue0CScaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue0C)
        ? attachment->queue0C
        : &ownedQueue0C_;
}

CLTThreadPerClientTCPEngine_0x4b2768_Queue* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueue34Scaffold() {
    CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue34)
        ? attachment->queue34
        : &ownedQueue34_;
}

const CLTThreadPerClientTCPEngine_0x4b2768_Queue* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueue0CScaffold() const {
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue0C)
        ? attachment->queue0C
        : &ownedQueue0C_;
}

const CLTThreadPerClientTCPEngine_0x4b2768_Queue* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueue34Scaffold() const {
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queue34)
        ? attachment->queue34
        : &ownedQueue34_;
}

void* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueueLockScaffold() const {
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queueLock)
        ? attachment->queueLock
        : const_cast<CRITICAL_SECTION*>(&ownedQueueLockHelper60_.crit);
}

void* CLTThreadPerClientTCPEngine_0x4b2768::ActiveQueueSignalEventScaffold() const {
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->queueSignalEvent)
        ? attachment->queueSignalEvent
        : ownedQueueSignalEvent7C_;
}

void* CLTThreadPerClientTCPEngine_0x4b2768::ActiveCleanupLockScaffold() const {
    const CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment* attachment =
        FindEngineLauncherAbiAttachment(this);
    return (attachment && attachment->cleanupLock)
        ? attachment->cleanupLock
        : const_cast<CRITICAL_SECTION*>(&ownedCleanupLockHelper98_.crit);
}

// anchor: launcher.exe:0x435f90
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::SignalQueueEventHelper() {
    HANDLE eventHandle = static_cast<HANDLE>(ActiveQueueSignalEventScaffold());
    return (eventHandle && SetEvent(eventHandle)) ? 0u : 1u;
}

// anchor: launcher.exe:0x435fa0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::WaitQueueEventHelper(int reasonMilliseconds) {
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
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::EnterQueueLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147c0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::LeaveQueueLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147b0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::EnterCleanupLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// anchor family: launcher.exe:0x4147c0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::LeaveCleanupLockHelper() {
    if (CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperationScaffold(
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
    CLTThreadPerClientTCPEngine_0x4b2768_Queue* targetQueue = useQueue34 ? ActiveQueue34Scaffold() : ActiveQueue0CScaffold();
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

    LoggerForQueueLabel(label)->info(
        "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperationScaffold label={} queue=[{}] workItem=0x{:08x} context={} pairWasEmpty={:08x} lockHeld={:08x}",
        label ? label : "<null>",
        useQueue34 ? "queue34" : "queue0C",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(workItem)),
        fmt::ptr(context),
        queuePairWasEmpty ? 1u : 0u,
        queueLockAlreadyHeld ? 1u : 0u);
    return true;
}

bool CLTThreadPerClientTCPEngine_0x4b2768::EnqueueDirectConnectionStatusWorkItemScaffold(
    CLTTCPConnection* connection,
    uint32_t workType,
    uint32_t workPayload,
    const char* label,
    bool queueLockAlreadyHeld) {
    if (!connection) {
        return false;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader* workItem = nullptr;
    if (workType == kWorkTypeConnectionStatus) {
        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold* statusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                workPayload);
        if (!statusWorkItem) {
            LoggerForQueueLabel(label)->info(
                "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueDirectConnectionStatusWorkItemScaffold failed label='{}'",
                label ? label : "<null>");
            return false;
        }
        workItem = &statusWorkItem->header;
    } else if (workType == kWorkTypeClose) {
        CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold* closeWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
                CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate());
        if (!closeWorkItem) {
            LoggerForQueueLabel(label)->info(
                "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueDirectConnectionStatusWorkItemScaffold failed label='{}'",
                label ? label : "<null>");
            return false;
        }
        workItem = &closeWorkItem->header;
    } else {
        LoggerForQueueLabel(label)->warn(
            "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueDirectConnectionStatusWorkItemScaffold unsupported workType=0x{:08x} ({}) connection={} label='{}'",
            workType,
            EngineWorkTypeName(workType),
            fmt::ptr(connection),
            label ? label : "<null>");
        return false;
    }

    void* queuedContext = connection ? connection->QueueContextScaffold() : nullptr;
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

    LoggerForQueueLabel(label)->info(
        "CLTThreadPerClientTCPEngine_0x4b2768 direct queued work label='{}' workItem={} context={} type=0x{:08x} ({}) payload=0x{:08x}",
        label ? label : "<null>",
        fmt::ptr(workItem),
        fmt::ptr(queuedContext),
        workType,
        EngineWorkTypeName(workType),
        workPayload);

    // Queue-timing rollback for late runtime stability:
    // - original producer paths only enqueue here and let the normal queue consumer family
    //   (`0x436fc0 -> 0x436b10` queue thread or client arg5 helper `+0x60`) drain later
    // - the earlier source-owned immediate drain let worker/connect threads re-enter margin/auth
    //   completion logic synchronously, which is a stronger replacement-only timing change than the
    //   now-restored direct-`context=this` producer shape
    // - keep type-1/type-2 completion ordering on the real queue-consumer path instead of
    //   short-circuiting it on the producer thread
    return true;
}

// UNANCHORED: connection-owned helper for the recovered `0x449d8a -> 0x436820` handoff.
void CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperationFromConnectionScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CLTTCPConnection* connection,
    const char* label) {
    // Current best original read for this receive path:
    // - `CLTTCPConnection::OnReceive` always calls `0x436820(engine+0x10, workItem, self, false)`
    // - queue selection is therefore fixed to queue0C here
    // - current parser read does not support an intentional `Parse(...) == 0` / `workItem == NULL`
    //   emit on this path; null work items belong to later lifecycle/shutdown producers instead
    // - original caller does not test a success result or reclaim `workItem`; ownership is already
    //   transferred to the queue/consumer boundary when this helper is entered
    // - active replacement still routes the direct connection object through its queue-dispatch ABI
    //   adapter so raw client.dll consumers do not misinterpret the MinGW virtual table layout
    (void)EnqueueCompletedOperationScaffold(
        workItem,
        connection ? connection->QueueContextScaffold() : nullptr,
        /*useQueue34=*/false,
        label,
        /*queueLockAlreadyHeld=*/false);
}

// anchor: launcher.exe:0x436b10
void CLTThreadPerClientTCPEngine_0x4b2768::RunCompletedOperationQueue(
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

        CLTThreadPerClientTCPEngine_0x4b2768_Queue* selectedQueue = nullptr;
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

        CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair pair = {};
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

        CBaseConnection* queuedBaseConnection = nullptr;
        CLTTCPConnection* queuedConnection = nullptr;
        if (context) {
            if (CBaseConnection* completionTarget = CBaseConnection_FromQueueContextScaffold(context)) {
                queuedBaseConnection = completionTarget;
                queuedConnection = dynamic_cast<CLTTCPConnection*>(completionTarget);
            } else if (CMessageConnection_0x4b7928* directConnection = FindMessageConnection(context)) {
                queuedBaseConnection = directConnection;
                queuedConnection = dynamic_cast<CLTTCPConnection*>(directConnection);
            }
        }
        const bool shouldAutoReleaseContext =
            isType1 && QueuedConnection_ShouldAutoReleaseAfterType1(context, queuedBaseConnection);
        const bool detectedSameWorkerThreadCloseSelfDispatch =
            isType1 && queuedConnection != nullptr && queuedConnection->WorkerThreadScaffold() != nullptr &&
            queuedConnection->WorkerThreadScaffold()->IsCurrentThread();
        const bool sameWorkerThreadCloseSelfDispatch =
            isType1 && (preferType1CallbackBeforeCleanup || detectedSameWorkerThreadCloseSelfDispatch);

        spdlog::debug(
            "CLTThreadPerClientTCPEngine_0x4b2768::RunCompletedOperationQueue consume queue=[{}] workItem={} workType=0x{:08x} context={} autoReleaseType1Context={} preferType1CallbackBeforeCleanup={} detectedSameWorkerThreadCloseSelfDispatch={} sameWorkerThreadCloseSelfDispatch={}",
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
                QueuedConnection_OnOperationCompleted(context, queuedBaseConnection, workItem);
            }
            if (shouldAutoReleaseContext) {
                QueuedConnection_ReleaseAfterType1(context, queuedBaseConnection);
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
            QueuedConnection_OnOperationCompleted(context, queuedBaseConnection, workItem);
        }

        if (shouldAutoReleaseContext) {
            QueuedConnection_ReleaseAfterType1(context, queuedBaseConnection);
        }
        QueueWorkItem_Release(workItem);
    }
}

// anchor family: launcher.exe:0x4366f0 / 0x436920
// Current source helper owns the real `+0x04/+0x08` queue-thread array/count fields directly.
void CLTThreadPerClientTCPEngine_0x4b2768::RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount) {
    CLTThreadPerClientTCPEngine_0x4b2768_QueueThread** queueThreadArray =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_QueueThread**>(queueThreadArrayField08_);
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
        CLTThreadPerClientTCPEngine_0x4b2768_QueueThread* thread = queueThreadArray ? queueThreadArray[i] : nullptr;
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

    queueThreadArray = static_cast<CLTThreadPerClientTCPEngine_0x4b2768_QueueThread**>(
        std::calloc(queueThreadCount, sizeof(CLTThreadPerClientTCPEngine_0x4b2768_QueueThread*)));
    if (!queueThreadArray) {
        SyncAttachedLauncherObjectStateScaffold();
        return;
    }

    for (uint32_t i = 0; i < queueThreadCount; ++i) {
        queueThreadArray[i] = new CLTThreadPerClientTCPEngine_0x4b2768_QueueThread(this);
        (void)queueThreadArray[i]->Start(/*startPriority=*/2);
    }

    queueThreadArrayField08_ = queueThreadArray;
    ctorFlagsField04_ = queueThreadCount;
    SyncAttachedLauncherObjectStateScaffold();
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
CMessageConnection_0x4b7928* CLTThreadPerClientTCPEngine_0x4b2768::FindMessageConnection(void* contextKey) {
    CBaseConnection* queueContextOwner = CBaseConnection_FromQueueContextScaffold(contextKey);
    void* resolvedContextKey = CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
    auto matchesConnectionKey =
        [contextKey, resolvedContextKey, queueContextOwner](CMessageConnection_0x4b7928* connection) -> bool {
        if (!connection) {
            return false;
        }

        return connection == contextKey ||
            connection == resolvedContextKey ||
            connection == queueContextOwner ||
            connection->OwnerContext() == contextKey ||
            connection->OwnerContext() == resolvedContextKey;
    };

    if (CMessageConnection_0x4b7928* queuedConnection =
            dynamic_cast<CMessageConnection_0x4b7928*>(queueContextOwner);
        queuedConnection && matchesConnectionKey(queuedConnection)) {
        return queuedConnection;
    }

    if (CLTThreadPerClientTCPEngine_0x4b2768_ContextPayloadBacking* contextBacking =
            FindEngineContextPayloadBacking(this)) {
        for (const auto& it : contextBacking->entries) {
            const CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = it.second->payload.get();
            CMessageConnection_0x4b7928* connection = worker
                ? static_cast<CMessageConnection_0x4b7928*>(worker->ContextKey())
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
LTTCPEndpointKey_0x44b070 CLTThreadPerClientTCPEngine_0x4b2768::MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    LTTCPEndpointKey_0x44b070 key = {};
    key.family = 2;
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = ipv4NetworkOrder;
    return key;
}

// anchor: launcher.exe:0x42fdb0
CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* CLTThreadPerClientTCPEngine_0x4b2768::EndpointTree_Find(const LTTCPEndpointKey_0x44b070& key) {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
        EndpointTreeFindNode(ownedEndpointTreeHead80_, key);
    return node ? node
                : reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(ownedEndpointTreeHead80_);
}

// anchor: launcher.exe:0x42fe10
CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* CLTThreadPerClientTCPEngine_0x4b2768::FindWorker(void* contextKey) {
    CMessageConnection_0x4b7928* connection = FindMessageConnection(contextKey);
    if (!connection) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node = ContextTreeFindNode(
        ownedContextTreeHead8C_,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(connection)));
    return node ? node->_M_valptr()->second : nullptr;
}

CMessageConnection_0x4b7928* CLTThreadPerClientTCPEngine_0x4b2768::ResolveConnectionForEngineSlotScaffold(
    void* contextKey) {
    if (!contextKey) {
        return nullptr;
    }

    void* normalizedContextKey = CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
    CMessageConnection_0x4b7928* connection = FindMessageConnection(contextKey);
    if (!connection && normalizedContextKey != contextKey) {
        connection = FindMessageConnection(normalizedContextKey);
    }

    // Static RE of `0x449cd0`, `0x449d20`, and `0x449d40` keeps the public engine slot family on
    // the direct connection object itself. After queue-context unwrapping above, remaining callers
    // are expected to already be passing that connection object.
    if (!connection) {
        connection = static_cast<CMessageConnection_0x4b7928*>(normalizedContextKey);
    }
    if (!connection) {
        return nullptr;
    }

    connection->SetEngine(this);
    if (connection->OwnerContext() == nullptr && normalizedContextKey != connection) {
        connection->SetOwnerContext(normalizedContextKey);
    }
    return connection;
}

// anchor: launcher.exe:0x431ff0
CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* CLTThreadPerClientTCPEngine_0x4b2768::CreateAndInsertWorkerThread(
    CMessageConnection_0x4b7928* connection,
    bool datagramMode,
    bool startThread) {
    if (!connection) {
        return nullptr;
    }

    connection->SetEngine(this);
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> worker =
        std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread>(connection, datagramMode);
    if (!worker) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* result = nullptr;
    const uint32_t key = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(connection));
    (void)EnterCleanupLockHelper();
    bool inserted = false;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node = ContextTreeInsertUniqueNode(
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
void CLTThreadPerClientTCPEngine_0x4b2768::StopAcceptThreadScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* acceptThread) {
    if (!acceptThread) {
        return;
    }

    acceptThread->SignalWakeup();
    (void)acceptThread->Stop(/*waitAfterTerminate=*/true);
    acceptThread->CloseListenSocketScaffold();
}

// UNANCHORED: source-owned teardown helper for the direct `WorkerThread` payload stored at
// `[contextNode+0x14]`.
void CLTThreadPerClientTCPEngine_0x4b2768::StopWorkerThreadScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread) {
    if (!workerThread) {
        return;
    }

    workerThread->RequestExit();
    workerThread->SignalWakeup();
    (void)workerThread->Stop(/*waitAfterTerminate=*/true);
    if (CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(workerThread->ContextKey())) {
        connection->SetWorkerThreadScaffold(nullptr);
    }
}

// UNANCHORED starter binding helper.
// Keeps current owner->engine binding state on the liblttcp side rather than in diagnostics.cpp.
CLTThreadPerClientTCPEngine_0x4b2768Binding::CLTThreadPerClientTCPEngine_0x4b2768Binding()
    : owner_(nullptr),
      engine_() {}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngine_0x4b2768Binding::~CLTThreadPerClientTCPEngine_0x4b2768Binding() = default;

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngine_0x4b2768Binding::Bind(void* owner) {
    if (owner_ == owner && engine_) {
        return true;
    }

    owner_ = owner;
    engine_ = std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768>();
    return static_cast<bool>(engine_);
}

// UNANCHORED starter binding helper.
void CLTThreadPerClientTCPEngine_0x4b2768Binding::Reset() {
    engine_.reset();
    owner_ = nullptr;
}

// UNANCHORED starter binding helper.
void* CLTThreadPerClientTCPEngine_0x4b2768Binding::Owner() const {
    return owner_;
}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngine_0x4b2768* CLTThreadPerClientTCPEngine_0x4b2768Binding::Engine() const {
    return engine_.get();
}

}  // namespace mxo::liblttcp
