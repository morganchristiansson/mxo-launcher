#include "ltthreadperclienttcpengine.h"

#include "../libltmessaging/messageconnection.h"
#include "../libltnet/sys/pc/pcsocket.h"
#include <spdlog/spdlog.h>

#include "../../../../compat/sgi_tree_compat.h"

#include <winsock2.h>
#include <system_error>

#include <process.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace mxo::liblttcp {

namespace {

static bool ShouldLogRepeatedQueueDiagnosticCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

static int CompareEndpointTreeKeys(
    const LTTCPEndpointKey_0x44b070& lhs,
    const LTTCPEndpointKey_0x44b070& rhs);

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

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static constexpr uint8_t kSocketFactoryFlagSkipDisableNagle = 0x01u;
static constexpr uint8_t kSocketFactoryFlagKeepBlocking = 0x02u;

static void LogConnectionDispatchForensics(
    const char* label,
    void* connectionObject,
    void* workItem,
    uint32_t workType) {
    static uint32_t s_logCount = 0u;
    if (!connectionObject) {
        return;
    }

    ++s_logCount;
    if (!ShouldLogRepeatedQueueDiagnosticCount(s_logCount)) {
        return;
    }

    CBaseConnection_0x4b8018* connection = static_cast<CBaseConnection_0x4b8018*>(connectionObject);
    spdlog::debug(
        "connection-dispatch forensic label={} object={} vtable={} slot1={} slot4={} autoReleaseByte={} workItem={} workType=0x{:08x} [count=0x{:08x}]",
        label ? label : "connection-dispatch",
        fmt::ptr(connectionObject),
        fmt::ptr(*reinterpret_cast<void***>(connectionObject)),
        fmt::ptr(((*reinterpret_cast<void***>(connectionObject)) != nullptr)
                     ? (*reinterpret_cast<void***>(connectionObject))[CBaseConnection_RawDispatchAbi::kReleaseSlotIndex]
                     : nullptr),
        fmt::ptr(((*reinterpret_cast<void***>(connectionObject)) != nullptr)
                     ? (*reinterpret_cast<void***>(connectionObject))[CBaseConnection_RawDispatchAbi::kOnOperationCompletedSlotIndex]
                     : nullptr),
        static_cast<unsigned>(connection->AutoReleaseFlag04()),
        fmt::ptr(workItem),
        workType,
        s_logCount);
}

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

// launcher.exe pool globals grouped into source-side state objects:
// - connection-status pool anchors: 0x004cb410 / 0x004f76c0..0x004f76d4
// - close-work-item pool anchors: 0x004cb42c / 0x004f76d8..0x004f76ec
static CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState g_ConnectionStatusWorkItemPoolState;
static CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState g_CloseWorkItemPoolState;

static void SmallWorkItemPool_FreeStorageScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_SmallWorkItemPoolState* poolState,
    void* storage);
static void CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_Clear();
static void CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_Clear();

// anchor set:
// - ctor/dtor: launcher.exe:0x431c30 / 0x4366f0 / 0x431310
// - endpoint tree: 0x4318f0 / 0x431240 / 0x42fdb0 / 0x4154d0
// - context tree: 0x4196b0 / 0x420ba0 / 0x42fe10 / 0x4154d0

static_assert(sizeof(mxo::sgi_tree::_Rb_tree_node_base) == 0x10, "launcher tree node-base size mismatch");
using CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode =
    mxo::sgi_tree::_Rb_tree_node<std::pair<LTTCPEndpointKey_0x44b070, CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread*>>;
using CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode =
    mxo::sgi_tree::_Rb_tree_node<std::pair<uint32_t, CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread*>>;
static_assert(sizeof(std::pair<LTTCPEndpointKey_0x44b070, CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread*>) == 0x14, "endpoint tree value size mismatch");
static_assert(sizeof(std::pair<uint32_t, CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread*>) == 0x8, "context tree value size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode) == 0x24, "endpoint tree node size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode) == 0x18, "context tree node size mismatch");

static CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610* ActiveQueuePairStorageScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768* self) {
    if (!self) {
        return nullptr;
    }

    return &static_cast<CLTBaseThreadPerClientTCPEngine_0x4b3e74*>(self)->queuePair0c_;
}

static mxo::sgi_tree::_Rb_tree_node_base* TreeHeaderBase(void* head) {
    return reinterpret_cast<mxo::sgi_tree::_Rb_tree_node_base*>(head);
}

static const mxo::sgi_tree::_Rb_tree_node_base* TreeHeaderBase(const void* head) {
    return reinterpret_cast<const mxo::sgi_tree::_Rb_tree_node_base*>(head);
}

static CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* AllocateEndpointTreeNode(
    const LTTCPEndpointKey_0x44b070& key,
    CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* payload) {
    auto* node = static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(
        std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode)));
    if (!node) {
        return nullptr;
    }

    std::memset(node, 0, sizeof(*node));
    node->_M_valptr()->first = key;
    node->_M_valptr()->second = payload;
    return node;
}

static CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* AllocateContextTreeNode(
    uint32_t key,
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* payload) {
    auto* node = static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(
        std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode)));
    if (!node) {
        return nullptr;
    }

    std::memset(node, 0, sizeof(*node));
    node->_M_valptr()->first = key;
    node->_M_valptr()->second = payload;
    return node;
}

// Narrow wrapper layer over the shared launcher RB-tree helper family.
// `compat/sgi_tree_compat.h` provides only the primitive node/rebalance operations.
// anchor family: launcher.exe:0x44b040 used by 0x42fdb0 / 0x4318f0 / 0x431240
// Ordering currently evidenced on `portNetworkOrder`, then `ipv4NetworkOrder`.
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

static CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* EndpointTreeEraseNodeAndReturnDetached(
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node) {
    if (!head || !node) {
        return nullptr;
    }
    return static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(
        mxo::sgi_tree::_Rb_tree_rebalance_for_erase(node, *TreeHeaderBase(head)));
}

static CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* ContextTreeEraseNodeAndReturnDetached(
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node) {
    if (!head || !node) {
        return nullptr;
    }
    return static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(
        mxo::sgi_tree::_Rb_tree_rebalance_for_erase(node, *TreeHeaderBase(head)));
}

// anchor: launcher.exe:0x42fdb0
static CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* EndpointTreeFindNode(
    const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    const LTTCPEndpointKey_0x44b070& key) {
    const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* candidate = nullptr;
    const mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    const auto* node = (header && header->_M_parent)
        ? static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header->_M_parent)
        : nullptr;
    while (node) {
        if (CompareEndpointTreeKeys(node->_M_valptr()->first, key) >= 0) {
            candidate = node;
            node = static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(node->_M_left);
        } else {
            node = static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(node->_M_right);
        }
    }
    return (candidate && CompareEndpointTreeKeys(candidate->_M_valptr()->first, key) == 0)
        ? const_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(candidate)
        : nullptr;
}

static CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* FindEngineEndpointPayload(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    const LTTCPEndpointKey_0x44b070& key) {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head = self
        ? *reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24**>(reinterpret_cast<uint8_t*>(self) + 0x80)
        : nullptr;
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
        EndpointTreeFindNode(head, key);
    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    return (node && node != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header))
        ? node->_M_valptr()->second
        : nullptr;
}

// anchor family: launcher.exe:0x4318f0 / 0x431240
static bool EndpointTreeInsertPlaceholder(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    const LTTCPEndpointKey_0x44b070& key,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head) {
        return false;
    }

    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* parent =
        reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header);
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
        header->_M_parent ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header->_M_parent) : nullptr;
    bool insertLeft = true;
    while (node) {
        parent = node;
        insertLeft = CompareEndpointTreeKeys(key, node->_M_valptr()->first) < 0;
        node = insertLeft
            ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(node->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(node->_M_right);
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* candidate = parent;
    if (!insertLeft) {
        if (candidate != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header)) {
            candidate = static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(
                mxo::sgi_tree::_Rb_tree_increment(candidate));
            if (candidate == reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header)) {
                candidate = nullptr;
            }
        }
    } else if (parent != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header) &&
               parent != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(header->_M_left)) {
        mxo::sgi_tree::_Rb_tree_node_base* previous = parent;
        while (previous->_M_left && previous == previous->_M_parent->_M_left) {
            previous = previous->_M_parent;
        }
        previous = previous->_M_parent;
        candidate = (previous != header)
            ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(previous)
            : nullptr;
    }

    if (candidate && CompareEndpointTreeKeys(candidate->_M_valptr()->first, key) == 0) {
        return true;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* insertedNode =
        AllocateEndpointTreeNode(key, nullptr);
    if (!insertedNode) {
        return false;
    }

    mxo::sgi_tree::_Rb_tree_insert_and_rebalance(
        insertLeft,
        insertedNode,
        reinterpret_cast<mxo::sgi_tree::_Rb_tree_node_base*>(parent),
        *header);
    ++(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x84));
    if (outInserted) {
        *outInserted = true;
    }
    return true;
}

static bool EndpointTreeAttachPayload(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    const LTTCPEndpointKey_0x44b070& key,
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> payload) {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head = self
        ? *reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24**>(reinterpret_cast<uint8_t*>(self) + 0x80)
        : nullptr;
    if (!self || !head || !payload) {
        return false;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
        EndpointTreeFindNode(head, key);
    if (!node || node == reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(TreeHeaderBase(head))) {
        return false;
    }

    node->_M_valptr()->second = payload.release();
    return true;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> EndpointTreeDetachPayloadByKey(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    const LTTCPEndpointKey_0x44b070& key) {
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head = self
        ? *reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24**>(reinterpret_cast<uint8_t*>(self) + 0x80)
        : nullptr;
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
        EndpointTreeFindNode(head, key);
    if (!node || node == reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(TreeHeaderBase(head))) {
        return nullptr;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> payload(node->_M_valptr()->second);
    node->_M_valptr()->second = nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* erased =
        EndpointTreeEraseNodeAndReturnDetached(head, node);
    std::free(erased);
    --(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x84));
    return payload;
}

// anchor: launcher.exe:0x42fe10
static CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* ContextTreeFindNode(
    const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    uint32_t key) {
    const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* candidate = nullptr;
    const mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    const auto* node = (header && header->_M_parent)
        ? static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(header->_M_parent)
        : nullptr;
    while (node) {
        if (CompareContextTreeKeys(node->_M_valptr()->first, key) >= 0) {
            candidate = node;
            node = static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(node->_M_left);
        } else {
            node = static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(node->_M_right);
        }
    }
    return (candidate && CompareContextTreeKeys(candidate->_M_valptr()->first, key) == 0)
        ? const_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(candidate)
        : nullptr;
}

static CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* FindEngineContextWorkerPayload(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    uint32_t key) {
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head = self
        ? *reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18**>(reinterpret_cast<uint8_t*>(self) + 0x8c)
        : nullptr;
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node =
        ContextTreeFindNode(head, key);
    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    return (node && node != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(header))
        ? node->_M_valptr()->second
        : nullptr;
}

// anchor family: launcher.exe:0x4196b0 / 0x420ba0 / 0x431ff0.
static CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* ContextTreeInsertUniqueWorkerNode(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    uint32_t key,
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> payload,
    bool* outInserted) {
    if (outInserted) {
        *outInserted = false;
    }
    if (!self || !head || !payload) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* existing = ContextTreeFindNode(head, key);
    if (existing && existing != reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(TreeHeaderBase(head))) {
        return existing->_M_valptr()->second;
    }

    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* parent =
        reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(header);
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node =
        header->_M_parent ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(header->_M_parent) : nullptr;
    bool insertLeft = true;
    while (node) {
        parent = node;
        insertLeft = key < node->_M_valptr()->first;
        node = insertLeft
            ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(node->_M_left)
            : static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(node->_M_right);
    }

    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = payload.get();
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* insertedNode =
        AllocateContextTreeNode(key, payload.release());
    if (!insertedNode) {
        return nullptr;
    }

    mxo::sgi_tree::_Rb_tree_insert_and_rebalance(
        insertLeft,
        insertedNode,
        reinterpret_cast<mxo::sgi_tree::_Rb_tree_node_base*>(parent),
        *header);
    ++(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x90));
    if (outInserted) {
        *outInserted = true;
    }
    return worker;
}

static std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> ContextTreeDetachPayloadByKey(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    uint32_t key) {
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head = self
        ? *reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18**>(reinterpret_cast<uint8_t*>(self) + 0x8c)
        : nullptr;
    if (!self || !head) {
        return nullptr;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node =
        ContextTreeFindNode(head, key);
    if (!node || node == reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(TreeHeaderBase(head))) {
        return nullptr;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> payload(node->_M_valptr()->second);
    node->_M_valptr()->second = nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* erased =
        ContextTreeEraseNodeAndReturnDetached(head, node);
    std::free(erased);
    --(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x90));
    return payload;
}

// Source-owned endpoint-tree erase adapter.
// Current source models the recovered outer layer as a plain endpoint-keyed map, so erase is
// handled by key-removal in `EndpointTreeDetachPayloadByKey` / direct `map::erase` cleanup.
static bool EndpointTreeEraseNode(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head,
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node) {
    if (!self || !head || !node) {
        return false;
    }
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* erased =
        EndpointTreeEraseNodeAndReturnDetached(head, node);
    std::free(erased);
    --(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x84));
    return true;
}

static bool ContextTreeEraseNode(
    CLTThreadPerClientTCPEngine_0x4b2768* self,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head,
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node) {
    if (!self || !head || !node) {
        return false;
    }
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* erased =
        ContextTreeEraseNodeAndReturnDetached(head, node);
    std::free(erased);
    --(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x90));
    return true;
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
static CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8*
CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate() {
    return static_cast<CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8*>(
        CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemPool_AllocateStorage(
            sizeof(CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8)));
}

// anchor: launcher.exe:0x435050
static CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8*
CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
    CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* self,
    uint32_t statusOrPayloadDword08) {
    if (!self) {
        return nullptr;
    }

    return new (self) CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8(
        statusOrPayloadDword08);
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
static CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00*
CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate() {
    return static_cast<CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00*>(
        CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemPool_AllocateStorage(
            sizeof(CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00)));
}

// anchor: launcher.exe:0x435070
static CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00*
CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
    CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00* self) {
    if (!self) {
        return nullptr;
    }

    return new (self) CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00();
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

}  // namespace

// anchor: launcher.exe:0x435050 / vtable `0x004b3df8`
CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8::CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8(
    uint32_t statusOrPayloadDword08)
    : workType04_(CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus)
    , statusOrPayloadDword08_(statusOrPayloadDword08) {}

// anchor: launcher.exe:0x435c30 / deleting dtor slot at vtable `0x004b3df8`
CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8::~CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8() = default;

uint32_t CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8::ReleaseSlot() {
    this->~CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8();
    SmallWorkItemPool_FreeStorageScaffold(&g_ConnectionStatusWorkItemPoolState, this);
    return 1u;
}

// anchor: launcher.exe:0x435070 / vtable `0x004b3e00`
CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00::CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00()
    : workType04_(CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose)
    , statusOrPayloadDword08_(0u) {}

// anchor: launcher.exe:0x435c80 / deleting dtor slot at vtable `0x004b3e00`
CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00::~CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00() = default;

uint32_t CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00::ReleaseSlot() {
    this->~CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00();
    SmallWorkItemPool_FreeStorageScaffold(&g_CloseWorkItemPoolState, this);
    return 1u;
}

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
        return 0;
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

// anchor: launcher.exe:0x452800
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

// anchor: launcher.exe:0x452800 / `_beginthreadex` start thunk
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
    if (wakeupSocketHandle_ != kInvalidSocketHandle) {
        closesocket(static_cast<SOCKET>(wakeupSocketHandle_));
        wakeupSocketHandle_ = kInvalidSocketHandle;
    }
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
    if (listenSocketHandle_ != kInvalidSocketHandle) {
        closesocket(static_cast<SOCKET>(listenSocketHandle_));
        listenSocketHandle_ = kInvalidSocketHandle;
    }
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
    if (wakeupSocketHandle_ != kInvalidSocketHandle) {
        closesocket(static_cast<SOCKET>(wakeupSocketHandle_));
        wakeupSocketHandle_ = kInvalidSocketHandle;
    }
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
            CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* statusWorkItem =
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                    workPayload);
            if (!statusWorkItem) {
                spdlog::warn(
                    "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread direct queue connect status alloc failed connection={} ownerContext={} payload=0x{:08x}",
                    fmt::ptr(connection),
                    fmt::ptr(connection->OwnerContext()),
                    static_cast<unsigned>(workPayload));
                return;
            }
            engine->EnqueueCompletedOperation(
                statusWorkItem,
                connection->RawDispatchObject(),
                /*useQueue34=*/false,
                connectStatusLabel);
            connectStatusQueued = true;
        };

    const auto queueClose = [&]() {
        if (closeQueued) {
            return;
        }
        CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00* closeWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
                CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate());
        if (!closeWorkItem) {
            spdlog::warn(
                "CLTThreadPerClientTCPEngine_0x4b2768::WorkerThread direct queue close alloc failed connection={} ownerContext={} state={}",
                fmt::ptr(connection),
                fmt::ptr(connection->OwnerContext()),
                static_cast<unsigned>(connection->State()));
            return;
        }
        engine->EnqueueCompletedOperation(
            closeWorkItem,
            connection->RawDispatchObject(),
            /*useQueue34=*/false,
            closeStatusLabel);
        closeQueued = true;
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
void CLTThreadPerClientTCPEngine_0x4b2768::Queue_Free(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord) {
    if (!queueRecord) {
        return;
    }

    if (queueRecord->slotArrayBase20) {
        uint32_t** slotsBase = static_cast<uint32_t**>(queueRecord->slotArrayBase20);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queueRecord->slotArrayCurrent0C);
        uint32_t** slotsLast = static_cast<uint32_t**>(queueRecord->slotArrayLast1C);
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

    std::memset(queueRecord, 0, sizeof(*queueRecord));
}

// anchor: launcher.exe:0x436340
bool CLTThreadPerClientTCPEngine_0x4b2768::Queue_Init(
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord,
    uint32_t initialSize) {
    if (!queueRecord) {
        return false;
    }

    Queue_Free(queueRecord);

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
            queueRecord->slotArrayBase20 = slotsBase;
            queueRecord->slotArrayCurrent0C = slotsBase + firstIndex;
            queueRecord->slotArrayLast1C = slotsBase + firstIndex + i;
            Queue_Free(queueRecord);
            return false;
        }
    }

    uint32_t** slotsCurrent = slotsBase + firstIndex;
    uint32_t** slotsLast = slotsCurrent + blockCount - 1;
    uint8_t* firstBlock = reinterpret_cast<uint8_t*>(*slotsCurrent);
    uint8_t* lastBlock = reinterpret_cast<uint8_t*>(*slotsLast);

    queueRecord->slotArrayBase20 = slotsBase;
    queueRecord->slotCapacity24 = slotCapacity;
    queueRecord->slotArrayCurrent0C = slotsCurrent;
    queueRecord->slotArrayLast1C = slotsLast;
    queueRecord->firstBlockBegin04 = firstBlock;
    queueRecord->firstBlockEnd08 = firstBlock ? (firstBlock + 0x80) : nullptr;
    queueRecord->readCursor00 = firstBlock;
    queueRecord->lastBlockBegin14 = lastBlock;
    queueRecord->lastBlockEnd18 = lastBlock ? (lastBlock + 0x80) : nullptr;
    queueRecord->writeCursor10 = lastBlock ? (lastBlock + ((initialSize & 0xfu) * 8u)) : nullptr;
    return true;
}

// anchor: launcher.exe:0x4361f0
static void Queue_RecenterOrGrowSlotArray(
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord,
    uint32_t additionalBlockCount,
    bool biasFrontExpansion) {
    if (!queueRecord) {
        return;
    }

    uint32_t** const slotsCurrent = static_cast<uint32_t**>(queueRecord->slotArrayCurrent0C);
    uint32_t** const slotsLast = static_cast<uint32_t**>(queueRecord->slotArrayLast1C);
    if (!slotsCurrent || !slotsLast) {
        return;
    }

    const int activeBlockCount = static_cast<int>(slotsLast - slotsCurrent) + 1;
    const int targetBlockCount = activeBlockCount + static_cast<int>(additionalBlockCount);
    const uint32_t slotCapacity = queueRecord->slotCapacity24;
    uint32_t** destination = nullptr;

    if (static_cast<uint32_t>(targetBlockCount * 2) < slotCapacity) {
        const uint32_t destinationIndex =
            ((slotCapacity - static_cast<uint32_t>(targetBlockCount)) >> 1) +
            (biasFrontExpansion ? additionalBlockCount : 0u);
        destination = static_cast<uint32_t**>(queueRecord->slotArrayBase20) + destinationIndex;
        if (destination < slotsCurrent) {
            if ((slotsLast + 1) != slotsCurrent) {
                std::memmove(destination, slotsCurrent, static_cast<size_t>((slotsLast + 1) - slotsCurrent) * sizeof(uint32_t*));
            }
        } else {
            const size_t moveBytes = static_cast<size_t>((slotsLast + 1) - slotsCurrent) * sizeof(uint32_t*);
            if (moveBytes != 0u) {
                std::memmove(
                    reinterpret_cast<uint8_t*>(destination) + (static_cast<size_t>(activeBlockCount) * sizeof(uint32_t*)) - moveBytes,
                    slotsCurrent,
                    moveBytes);
            }
        }
    } else {
        const uint32_t growthBase =
            (slotCapacity < additionalBlockCount) ? additionalBlockCount : slotCapacity;
        const uint32_t newCapacity = slotCapacity + 2u + growthBase;
        uint32_t** const newSlotsBase =
            static_cast<uint32_t**>(std::malloc(static_cast<size_t>(newCapacity) * sizeof(uint32_t*)));
        if (!newSlotsBase) {
            return;
        }

        const uint32_t destinationIndex =
            ((newCapacity - static_cast<uint32_t>(targetBlockCount)) >> 1) +
            (biasFrontExpansion ? additionalBlockCount : 0u);
        destination = newSlotsBase + destinationIndex;
        if ((slotsLast + 1) != slotsCurrent) {
            std::memmove(destination, slotsCurrent, static_cast<size_t>((slotsLast + 1) - slotsCurrent) * sizeof(uint32_t*));
        }

        if (queueRecord->slotArrayBase20) {
            std::free(queueRecord->slotArrayBase20);
        }
        queueRecord->slotArrayBase20 = newSlotsBase;
        queueRecord->slotCapacity24 = newCapacity;
    }

    queueRecord->slotArrayCurrent0C = destination;
    queueRecord->firstBlockBegin04 = *destination;
    queueRecord->firstBlockEnd08 = *destination
        ? (reinterpret_cast<uint8_t*>(*destination) + 0x80)
        : nullptr;
    queueRecord->slotArrayLast1C = destination + (activeBlockCount - 1);
    queueRecord->lastBlockBegin14 = destination[activeBlockCount - 1];
    queueRecord->lastBlockEnd18 = destination[activeBlockCount - 1]
        ? (reinterpret_cast<uint8_t*>(destination[activeBlockCount - 1]) + 0x80)
        : nullptr;
}

// anchor: launcher.exe:0x436450
static void Queue_AppendBlockAndCommitTailPair(
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord,
    const CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair& pair) {
    if (!queueRecord) {
        return;
    }

    uint32_t** slotsLast = static_cast<uint32_t**>(queueRecord->slotArrayLast1C);
    uint32_t** const slotsBase = static_cast<uint32_t**>(queueRecord->slotArrayBase20);
    if (!slotsLast || !slotsBase) {
        return;
    }

    if ((queueRecord->slotCapacity24 - static_cast<uint32_t>(slotsLast - slotsBase)) < 2u) {
        Queue_RecenterOrGrowSlotArray(queueRecord, 1u, false);
        slotsLast = static_cast<uint32_t**>(queueRecord->slotArrayLast1C);
        if (!slotsLast) {
            return;
        }
    }

    uint32_t* const newBlock = static_cast<uint32_t*>(std::malloc(0x80));
    if (!newBlock) {
        return;
    }

    slotsLast[1] = newBlock;
    if (uint32_t* const writeCursor = static_cast<uint32_t*>(queueRecord->writeCursor10)) {
        writeCursor[0] = pair.value0;
        writeCursor[1] = pair.value1;
    }

    queueRecord->slotArrayLast1C = slotsLast + 1;
    queueRecord->lastBlockBegin14 = newBlock;
    queueRecord->lastBlockEnd18 = static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80;
    queueRecord->writeCursor10 = newBlock;
}

// anchor: launcher.exe:0x436670 selected-queue push body reached from `0x436820`
void CLTThreadPerClientTCPEngine_0x4b2768::Queue_PushPair(
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord,
    uint32_t value0,
    uint32_t value1) {
    if (!queueRecord) {
        return;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair pair = {value0, value1};
    uint32_t* writeCursor = static_cast<uint32_t*>(queueRecord->writeCursor10);
    if (!writeCursor) {
        return;
    }

    const uint8_t* const lastPairInBlock =
        queueRecord->lastBlockEnd18 ? (static_cast<uint8_t*>(queueRecord->lastBlockEnd18) - 8) : nullptr;
    if (static_cast<const void*>(writeCursor) == static_cast<const void*>(lastPairInBlock)) {
        Queue_AppendBlockAndCommitTailPair(queueRecord, pair);
        return;
    }

    writeCursor[0] = value0;
    writeCursor[1] = value1;
    queueRecord->writeCursor10 = writeCursor + 2;
}

// anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
bool CLTThreadPerClientTCPEngine_0x4b2768::Queue_TryPopPair(
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord,
    CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair* outPair) {
    if (!queueRecord || !outPair || queueRecord->writeCursor10 == queueRecord->readCursor00) {
        return false;
    }

    uint32_t* readCursor = static_cast<uint32_t*>(queueRecord->readCursor00);
    outPair->value0 = readCursor[0];
    outPair->value1 = readCursor[1];

    uint8_t* lastPairInBlock =
        queueRecord->firstBlockEnd08 ? (static_cast<uint8_t*>(queueRecord->firstBlockEnd08) - 8) : nullptr;
    if (static_cast<void*>(queueRecord->readCursor00) != static_cast<void*>(lastPairInBlock)) {
        queueRecord->readCursor00 = readCursor + 2;
        return true;
    }

    uint32_t* oldBlock = static_cast<uint32_t*>(queueRecord->firstBlockBegin04);
    if (oldBlock) {
        std::free(oldBlock);
    }

    uint32_t** slotsCurrent = static_cast<uint32_t**>(queueRecord->slotArrayCurrent0C) + 1;
    queueRecord->slotArrayCurrent0C = slotsCurrent;
    uint32_t* newBlock = *slotsCurrent;
    queueRecord->firstBlockBegin04 = newBlock;
    queueRecord->firstBlockEnd08 = static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80;
    queueRecord->readCursor00 = newBlock;
    return true;
}

// anchor: launcher.exe:0x4816f0
// Helper reads `[workItem+0x04]`.
static uint32_t QueueWorkItem_GetType(const void* workItem) {
    if (!workItem) {
        return 0;
    }
    const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    return header->workType04;
}

// Current queue/runtime pass keeps launcher.exe as the source of truth and trims prior
// wrapper-era interpretation where concrete RE has now settled the behavior.

// anchor: launcher.exe:0x431c30
// vtable: launcher.exe:0x004b2768
// current constructor-layout anchors that should stay aligned with source comments/docs:
// - base ctor `0x4366f0`
// - queue-pair init `0x436610 -> 0x436340(size=0)` covering the `+0x0c` / `+0x34` pair
// - base helper roots at `+0x5c` / `+0x60`
// - CreateEventA result at `+0x7c`
// - derived list heads at `+0x80` (0x24 bytes) and `+0x8c` (0x18 bytes)
// - derived helper root at `+0x98`
CLTCriticalSectionHelper_0x4add70::CLTCriticalSectionHelper_0x4add70() {
    std::memset(&crit, 0, sizeof(crit));
    InitializeCriticalSection(&crit);
}

CLTCriticalSectionHelper_0x4add70::~CLTCriticalSectionHelper_0x4add70() {
    DeleteCriticalSection(&crit);
}

uint32_t CLTCriticalSectionHelper_0x4add70::Enter() {
    EnterCriticalSection(&crit);
    return 0u;
}

uint32_t CLTCriticalSectionHelper_0x4add70::Leave() {
    LeaveCriticalSection(&crit);
    return 0u;
}

CLTEventCriticalSectionHelper_0x4b3e20::CLTEventCriticalSectionHelper_0x4b3e20()
    : embeddedLockHelper04(),
      eventHandle20(CreateEventA(nullptr, FALSE, FALSE, nullptr)) {}

CLTEventCriticalSectionHelper_0x4b3e20::~CLTEventCriticalSectionHelper_0x4b3e20() {
    if (eventHandle20 != nullptr) {
        CloseHandle(eventHandle20);
        eventHandle20 = nullptr;
    }
}

uint32_t CLTEventCriticalSectionHelper_0x4b3e20::Signal() {
    return (eventHandle20 && SetEvent(eventHandle20)) ? 0u : 1u;
}

uint32_t CLTEventCriticalSectionHelper_0x4b3e20::Wait(int reasonMilliseconds) {
    embeddedLockHelper04.Leave();
    const DWORD waitResult = eventHandle20
        ? WaitForSingleObject(eventHandle20, static_cast<DWORD>(reasonMilliseconds))
        : WAIT_FAILED;
    if (waitResult == WAIT_OBJECT_0) {
        embeddedLockHelper04.Enter();
        return 0u;
    }
    if (waitResult == WAIT_TIMEOUT) {
        embeddedLockHelper04.Enter();
        return 3u;
    }
    return 1u;
}

void CLTThreadPerClientTCPEngine_0x4b2768::InitializeEndpointTreeHead24(
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head) {
    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    if (!header) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    header->_M_color = mxo::sgi_tree::_S_red;
    header->_M_parent = nullptr;
    header->_M_left = header;
    header->_M_right = header;
}

void CLTThreadPerClientTCPEngine_0x4b2768::InitializeContextTreeHead18(
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head) {
    mxo::sgi_tree::_Rb_tree_node_base* header = TreeHeaderBase(head);
    if (!header) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    header->_M_color = mxo::sgi_tree::_S_red;
    header->_M_parent = nullptr;
    header->_M_left = header;
    header->_M_right = header;
}

CLTBaseThreadPerClientTCPEngine_0x4b3e74::CLTBaseThreadPerClientTCPEngine_0x4b3e74()
    : field04_(0),
      field08_(nullptr),
      queuePair0c_(),
      queueWaitHelper5c_() {
    // anchor: launcher.exe:0x4366f0
    CLTThreadPerClientTCPEngine_0x4b2768::Queue_Init(&queuePair0c_.queue00, 0);
    CLTThreadPerClientTCPEngine_0x4b2768::Queue_Init(&queuePair0c_.queue28, 0);
}

// anchor: launcher.exe:0x436fd0 / deleting wrapper 0x437050
CLTBaseThreadPerClientTCPEngine_0x4b3e74::~CLTBaseThreadPerClientTCPEngine_0x4b3e74() {
    CLTThreadPerClientTCPEngine_0x4b2768::Queue_Free(&queuePair0c_.queue00);
    CLTThreadPerClientTCPEngine_0x4b2768::Queue_Free(&queuePair0c_.queue28);
}

CLTThreadPerClientTCPEngine_0x4b2768::CLTThreadPerClientTCPEngine_0x4b2768()
    : endpointTreeHead80_(nullptr),
      endpointCount84_(0),
      reserved88_(0),
      contextTreeHead8c_(nullptr),
      contextCount90_(0),
      reserved94_(0),
      cleanupLockHelper98_() {
    // anchor: launcher.exe:0x431c30 / base ctor 0x4366f0
    // Faithfulness restructuring:
    // - base now owns the recovered `+0x04..+0x7c` field family for real inheritance
    // - keep only the derived `+0x80..+0xb3` extension here
    (void)cleanupLockHelper98_;

    endpointTreeHead80_ =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24)));
    if (endpointTreeHead80_) {
        InitializeEndpointTreeHead24(endpointTreeHead80_);
    }

    contextTreeHead8c_ =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18)));
    if (contextTreeHead8c_) {
        InitializeContextTreeHead18(contextTreeHead8c_);
    }

    endpointCount84_ = 0u;
    contextCount90_ = 0u;

    // Original base ctor 0x4366f0 allocates queue-thread children only when the effective
    // ctor flag/count at +0x04 is non-zero. The current scaffold still enters through a
    // zero-count binder path, so keep the recovered child family in source but default it empty.
    CreateQueueThreadsForCtorCount(/*queueThreadCount=*/0);
}

// anchor: launcher.exe:0x40b389..0x40b404 teardown releases arg5 through vtable slot 0
// vtable: launcher.exe:0x004b2768
CLTThreadPerClientTCPEngine_0x4b2768::~CLTThreadPerClientTCPEngine_0x4b2768() {
    StopQueueThreads();

    while (true) {
        mxo::sgi_tree::_Rb_tree_node_base* endpointHeader = TreeHeaderBase(endpointTreeHead80_);
        CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* node =
            (endpointHeader && endpointHeader->_M_left != endpointHeader)
                ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(endpointHeader->_M_left)
                : nullptr;
        if (!node) {
            break;
        }
        std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> acceptThread(node->_M_valptr()->second);
        node->_M_valptr()->second = nullptr;
        StopAcceptThreadScaffold(acceptThread.get());
        (void)EndpointTreeEraseNode(this, endpointTreeHead80_, node);
    }

    while (true) {
        mxo::sgi_tree::_Rb_tree_node_base* contextHeader = TreeHeaderBase(contextTreeHead8c_);
        CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* node =
            (contextHeader && contextHeader->_M_left != contextHeader)
                ? static_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(contextHeader->_M_left)
                : nullptr;
        if (!node) {
            break;
        }
        CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread = node->_M_valptr()->second;
        if (workerThread) {
            workerThread->RequestExit();
            workerThread->SignalWakeup();
            if (workerThread->IsRunning()) {
                (void)workerThread->Wait();
            }
            if (CMessageConnection_0x4b7928* connection =
                    static_cast<CMessageConnection_0x4b7928*>(workerThread->ContextKey())) {
                connection->SetWorkerThreadScaffold(nullptr);
            }
            delete workerThread;
            node->_M_valptr()->second = nullptr;
        }
        (void)ContextTreeEraseNode(this, contextTreeHead8c_, node);
    }

    if (endpointTreeHead80_) {
        std::free(endpointTreeHead80_);
        endpointTreeHead80_ = nullptr;
    }
    if (contextTreeHead8c_) {
        std::free(contextTreeHead8c_);
        contextTreeHead8c_ = nullptr;
    }
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b3e74 slot +0x0c and 0x004b2768 slot +0x0c
uint32_t CLTBaseThreadPerClientTCPEngine_0x4b3e74::MonitorEphemeralUDPPort(
    uint16_t* outBoundPortHostOrder,
    void* contextKey,
    void* ipv4NetworkOrder) {
    // Current best static read: thin helper around slot 2 / UDPMonitorPort(port=0, ...)
    // followed by getsockname/ntohs to report the chosen local port.
    const uint32_t result = UDPMonitorPort(/*portHostOrder=*/0, contextKey, ipv4NetworkOrder);
    if (result == 0u && outBoundPortHostOrder) {
        *outBoundPortHostOrder = 0;
        CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(contextKey);
        if (connection && connection->SocketHandle() != kInvalidSocketHandle) {
            sockaddr_in boundAddr = {};
            int boundAddrSize = sizeof(boundAddr);
            if (getsockname(
                    static_cast<SOCKET>(connection->SocketHandle()),
                    reinterpret_cast<sockaddr*>(&boundAddr),
                    &boundAddrSize) == 0) {
                *outBoundPortHostOrder = ntohs(boundAddr.sin_port);
            }
        }
    }
    return result;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b3e74 slot +0x28 and 0x004b2768 slot +0x28
uint32_t CLTBaseThreadPerClientTCPEngine_0x4b3e74::Slot10_443810(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b3e74 slot +0x2c and 0x004b2768 slot +0x2c
uint32_t CLTBaseThreadPerClientTCPEngine_0x4b3e74::Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) {
    (void)arg1;
    if (out0) {
        *out0 = 0;
    }
    if (out1) {
        *out1 = 0;
    }
    return 0;
}

// anchor: launcher.exe:0x4319a0
// source-owned compatibility shim only:
// - launcher.exe `0x4319a0` is the deleting-dtor slot for vtable `0x004b2768`
// - this helper remains available for source paths that still speak in older `Release(...)`
//   terms, but it is not modeled as a virtual slot on the recovered class anymore
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
    if (!EndpointTreeInsertPlaceholder(this, endpointTreeHead80_, key, &inserted)) {
        return 1u;
    }
    if (!inserted) {
        return kResultAlreadyMonitored;
    }

    const uint32_t listenSocketHandle = CLTIPSocket_StaticAllocateSocket(
        SOCK_STREAM,
        IPPROTO_TCP,
        /*flags=*/0u);
    if (listenSocketHandle == kInvalidSocketHandle) {
        (void)EndpointTreeDetachPayloadByKey(this, key);
        return 1u;
    }

    SOCKET listenSocket = static_cast<SOCKET>(listenSocketHandle);
    sockaddr_in listenAddr = {};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(portHostOrder);
    listenAddr.sin_addr.s_addr = ipv4NetworkOrder;
    if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&listenAddr), sizeof(listenAddr)) == SOCKET_ERROR ||
        listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        (void)EndpointTreeDetachPayloadByKey(this, key);
        return 1u;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> acceptThread =
        std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread>(
            listenSocketHandle,
            ownerContext);
    if (!acceptThread) {
        uint32_t socketHandleToClose = listenSocketHandle;
        if (socketHandleToClose != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandleToClose));
            socketHandleToClose = kInvalidSocketHandle;
        }
        (void)EndpointTreeDetachPayloadByKey(this, key);
        return 1u;
    }
    if (!EndpointTreeAttachPayload(this, key, std::move(acceptThread))) {
        uint32_t socketHandleToClose = listenSocketHandle;
        if (socketHandleToClose != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandleToClose));
            socketHandleToClose = kInvalidSocketHandle;
        }
        (void)EndpointTreeDetachPayloadByKey(this, key);
        return 1u;
    }

    if (CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* payload = FindEngineEndpointPayload(this, key)) {
        (void)payload->Start(/*startPriority=*/2);
    }
    endpointCount84_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x84);
    return 0u;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ipv4NetworkOrder) {
    CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(contextKey);
    if (!connection || connection->State() != LTTCPEngineConnectionState::kClosed) {
        return 1u;
    }

    connection->SetEngine(this);

    const uint32_t socketHandle = CLTIPSocket_StaticAllocateSocket(
        SOCK_DGRAM,
        IPPROTO_UDP,
        /*flags=*/0u);
    if (socketHandle == kInvalidSocketHandle) {
        return 1u;
    }

    SOCKET udpSocket = static_cast<SOCKET>(socketHandle);
    BOOL reuseAddr = TRUE;
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(portHostOrder);
    bindAddr.sin_addr.s_addr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ipv4NetworkOrder));
    if (setsockopt(
            udpSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuseAddr),
            sizeof(reuseAddr)) == SOCKET_ERROR ||
        bind(udpSocket, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        closesocket(udpSocket);
        return 1u;
    }

    connection->SetSocketHandle(socketHandle);
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* worker = CreateAndInsertWorkerThread(
        connection,
        /*datagramMode=*/true,
        /*startThread=*/false);
    if (!worker) {
        uint32_t socketHandleToClose = socketHandle;
        if (socketHandleToClose != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandleToClose));
            socketHandleToClose = kInvalidSocketHandle;
        }
        connection->SetSocketHandle(kInvalidSocketHandle);
        return 1u;
    }

    connection->SetState(LTTCPEngineConnectionState::kUdpMonitorActive);
    (void)worker->Start(/*startPriority=*/2);
    contextCount90_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x90);
    return 0u;
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
    if (!FindEngineEndpointPayload(this, key)) {
        if (outOwnerContext) {
            *outOwnerContext = nullptr;
        }
        return kResultEndpointNotFound;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread> acceptThread =
        EndpointTreeDetachPayloadByKey(this, key);
    if (outOwnerContext) {
        *outOwnerContext = acceptThread ? acceptThread->OwnerContext() : nullptr;
    }
    StopAcceptThreadScaffold(acceptThread.get());
    acceptThread.reset();
    endpointCount84_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x84);
    if (endpointTreeHead80_ && endpointCount84_ == 0u) {
        InitializeEndpointTreeHead24(endpointTreeHead80_);
    }
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

    void* connectionDispatchObject = connection->RawDispatchObject();
    const LTTCPEndpointKey_0x44b070& remoteEndpoint = connection->remoteEndpoint_;
    if (connection->State() != LTTCPEngineConnectionState::kClosed) {
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect rejected connection={} state={} port={} ip=0x{:08x}",
            fmt::ptr(connection),
            static_cast<unsigned>(connection->State()),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder));

        CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectRejectedNotClosedPayload);
        (void)EnqueueCompletedOperation(
            connectionStatusWorkItem,
            connectionDispatchObject,
            /*useQueue34=*/false,
            "connect:not-closed");
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

        CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectImmediateFailurePayload);
        (void)EnqueueCompletedOperation(
            connectionStatusWorkItem,
            connectionDispatchObject,
            /*useQueue34=*/false,
            "connect:socket-failed");
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
        if (socketHandle != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandle));
            socketHandle = kInvalidSocketHandle;
        }
        connection->SetSocketHandle(socketHandle);
        spdlog::info(
            "CLTThreadPerClientTCPEngine_0x4b2768::Connect bind failed connection={} port={} ip=0x{:08x} wsaError={} ({})",
            fmt::ptr(connection),
            static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
            static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
            static_cast<unsigned>(wsaError),
            std::system_category().message(wsaError));

        CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* connectionStatusWorkItem =
            CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                kConnectImmediateFailurePayload);
        (void)EnqueueCompletedOperation(
            connectionStatusWorkItem,
            connectionDispatchObject,
            /*useQueue34=*/false,
            "connect:bind-failed");
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
            if (socketHandle != kInvalidSocketHandle) {
                closesocket(static_cast<SOCKET>(socketHandle));
                socketHandle = kInvalidSocketHandle;
            }
            connection->SetSocketHandle(socketHandle);
            spdlog::info(
                "CLTThreadPerClientTCPEngine_0x4b2768::Connect connect failed connection={} port={} ip=0x{:08x} wsaError={} ({})",
                fmt::ptr(connection),
                static_cast<unsigned>(ntohs(remoteEndpoint.portNetworkOrder)),
                static_cast<unsigned>(remoteEndpoint.ipv4NetworkOrder),
                static_cast<unsigned>(wsaError),
                std::system_category().message(wsaError));

            CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00* closeWorkItem =
                CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_ctor(
                    CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItem_Allocate());
            (void)EnqueueCompletedOperation(
                closeWorkItem,
                connectionDispatchObject,
                /*useQueue34=*/false,
                "connect:immediate-close");

            CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8* connectionStatusWorkItem =
                CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_ctor_withPayload(
                    CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItem_Allocate(),
                    kConnectImmediateFailurePayload);
            (void)EnqueueCompletedOperation(
                connectionStatusWorkItem,
                connectionDispatchObject,
                /*useQueue34=*/false,
                "connect:immediate-status");
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
        if (socketHandle != kInvalidSocketHandle) {
            closesocket(static_cast<SOCKET>(socketHandle));
            socketHandle = kInvalidSocketHandle;
        }
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
    contextCount90_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x90);
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

    CBaseConnection_0x4b8018* connection =
        static_cast<CBaseConnection_0x4b8018*>(contextKey);
    void* cleanupContextKey = static_cast<void*>(connection);
    bool touchedConnectionState = false;
    uint32_t result = 0u;
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> workerPayload;
    CMessageConnection_0x4b7928* cleanupConnection = nullptr;

    if (CMessageConnection_0x4b7928* connection = static_cast<CMessageConnection_0x4b7928*>(cleanupContextKey)) {
        cleanupConnection = connection;
        connection->SetState(LTTCPEngineConnectionState::kClosed);
        connection->SetSocketHandle(kInvalidSocketHandle);
        connection->SetWorkerThreadScaffold(nullptr);
        touchedConnectionState = true;
    }

    const uint32_t contextTreeKey = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cleanupContextKey));
    if (FindEngineContextWorkerPayload(this, contextTreeKey) != nullptr) {
        workerPayload = ContextTreeDetachPayloadByKey(this, contextTreeKey);
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

    if (cleanupConnection) {
        cleanupConnection->ReleasePendingSendQueueContentsScaffold();
    }

    (void)LeaveCleanupLockHelper();

    if (workerPayload) {
        // Keep the `0x4316a0` teardown shape explicit here instead of hiding the hot cleanup path
        // behind the broader source-only `StopWorkerThreadScaffold` helper.
        workerPayload->RequestExit();
        workerPayload->SignalWakeup();
        if (workerPayload->IsRunning()) {
            (void)workerPayload->Wait();
        }
        if (cleanupConnection) {
            cleanupConnection->SetWorkerThreadScaffold(nullptr);
        } else if (CMessageConnection_0x4b7928* connection =
                       static_cast<CMessageConnection_0x4b7928*>(workerPayload->ContextKey())) {
            connection->SetWorkerThreadScaffold(nullptr);
        }
        workerPayload.reset();
    }

    contextCount90_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x90);
    if (contextTreeHead8c_ && contextCount90_ == 0u) {
        InitializeContextTreeHead18(contextTreeHead8c_);
    }
    return result;
}

// anchor: launcher.exe:0x435f90
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::SignalQueueEventHelper() {
    return queueWaitHelper5c_.Signal();
}

// anchor: launcher.exe:0x435fa0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::WaitQueueEventHelper(int reasonMilliseconds) {
    return queueWaitHelper5c_.Wait(reasonMilliseconds);
}

// Shared helper-body evidence: launcher.exe:0x4147b0 / 0x4147c0.
// Queue-family collapse note:
// - arg5 +0x60 remains as ABI helper surface only
// - live queue-lock state now stays on the real engine object's recovered helper storage
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::EnterQueueLockHelper() {
    return queueWaitHelper5c_.embeddedLockHelper04.Enter();
}

uint32_t CLTThreadPerClientTCPEngine_0x4b2768::LeaveQueueLockHelper() {
    return queueWaitHelper5c_.embeddedLockHelper04.Leave();
}

uint32_t CLTThreadPerClientTCPEngine_0x4b2768::EnterCleanupLockHelper() {
    return cleanupLockHelper98_.Enter();
}

uint32_t CLTThreadPerClientTCPEngine_0x4b2768::LeaveCleanupLockHelper() {
    return cleanupLockHelper98_.Leave();
}

// anchor: launcher.exe:0x4364d0
uint32_t CLTThreadPerClientTCPEngine_0x4b2768::TryPopCompletedOperation(
    CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair* outPair,
    bool waitForSignal) {
    if (!outPair) {
        return 0x700000a;
    }
    if (field04_ != 0u) {
        outPair->value0 = 0u;
        outPair->value1 = 0u;
        return 0x7000006u;
    }

    CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610* activeQueuePair =
        ActiveQueuePairStorageScaffold(this);
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue0C =
        activeQueuePair ? &activeQueuePair->queue00 : nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue34 =
        activeQueuePair ? &activeQueuePair->queue28 : nullptr;
    HANDLE activeQueueSignalEvent = queueWaitHelper5c_.eventHandle20;
    (void)EnterQueueLockHelper();

    while (waitForSignal &&
           activeQueue0C->writeCursor10 == activeQueue0C->readCursor00 &&
           activeQueue34->writeCursor10 == activeQueue34->readCursor00) {
        if (!activeQueueSignalEvent) {
            (void)LeaveQueueLockHelper();
            outPair->value0 = 0u;
            outPair->value1 = 0u;
            return 0x700000au;
        }
        const uint32_t waitResult = WaitQueueEventHelper(INFINITE);
        if (waitResult != 0u && waitResult != 3u) {
            outPair->value0 = 0u;
            outPair->value1 = 0u;
            return 0x700000au;
        }
    }

    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* selectedQueue = nullptr;
    if (activeQueue34->writeCursor10 != activeQueue34->readCursor00) {
        selectedQueue = activeQueue34;
    } else if (activeQueue0C->writeCursor10 != activeQueue0C->readCursor00) {
        selectedQueue = activeQueue0C;
    }

    if (!selectedQueue) {
        (void)LeaveQueueLockHelper();
        outPair->value0 = 0u;
        outPair->value1 = 0u;
        return 0x700000au;
    }

    const bool popped = Queue_TryPopPair(selectedQueue, outPair);
    (void)LeaveQueueLockHelper();
    return popped ? 0u : 0x700000au;
}

// anchor: launcher.exe:0x436820
void CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperation(
    void* workItem,
    void* context,
    bool useQueue34,
    const char* label) {
    // Current best read of original `0x436820` / `0x436670`:
    // - `0x436820` itself returns `void`
    // - lock/order is:
    //   enter helper `+0x60` -> snapshot combined queue-pair emptiness -> push pair -> leave lock
    //   -> signal helper `+0x5c` only on pre-push empty -> non-empty transition
    // - no push-success result is surfaced back to callers; caller-side ownership/lifetime does not
    //   branch on queue-growth success/failure once this path is entered
    CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610* activeQueuePair =
        ActiveQueuePairStorageScaffold(this);
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue0C =
        activeQueuePair ? &activeQueuePair->queue00 : nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue34 =
        activeQueuePair ? &activeQueuePair->queue28 : nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* targetQueue = useQueue34 ? activeQueue34 : activeQueue0C;
    if (!targetQueue) {
        return;
    }

    (void)EnterQueueLockHelper();

    const bool queuePairWasEmpty =
        activeQueue0C->writeCursor10 == activeQueue0C->readCursor00 &&
        activeQueue34->writeCursor10 == activeQueue34->readCursor00;
    Queue_PushPair(
        targetQueue,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(workItem)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));

    (void)LeaveQueueLockHelper();

    if (queuePairWasEmpty) {
        (void)SignalQueueEventHelper();
    }

    LoggerForQueueLabel(label)->info(
        "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperation label={} queue=[{}] workItem=0x{:08x} context={} pairWasEmpty={:08x} lockHeld={:08x}",
        label ? label : "<null>",
        useQueue34 ? "queue34" : "queue0C",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(workItem)),
        fmt::ptr(context),
        queuePairWasEmpty ? 1u : 0u,
        0u);

    static uint32_t s_enqueueDiagnosticCount = 0u;
    ++s_enqueueDiagnosticCount;
    if (ShouldLogRepeatedQueueDiagnosticCount(s_enqueueDiagnosticCount)) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine_0x4b2768::EnqueueCompletedOperation raw queue state count={:08x} q0.read={} q0.write={} q34.read={} q34.write={} event={} field04=0x{:08x}",
            s_enqueueDiagnosticCount,
            fmt::ptr(activeQueue0C ? activeQueue0C->readCursor00 : nullptr),
            fmt::ptr(activeQueue0C ? activeQueue0C->writeCursor10 : nullptr),
            fmt::ptr(activeQueue34 ? activeQueue34->readCursor00 : nullptr),
            fmt::ptr(activeQueue34 ? activeQueue34->writeCursor10 : nullptr),
            fmt::ptr(queueWaitHelper5c_.eventHandle20),
            field04_);
    }
}

// anchor: launcher.exe:0x436b10
void CLTThreadPerClientTCPEngine_0x4b2768::RunCompletedOperationQueue(
    bool nonBlocking) {
    // Current bounded mirror of the shared launcher/client consumer family:
    // - prefer queue34, else queue0C
    // - nonBlocking=true matches the client poll form; false waits on the attached signal event
    // - null work item is the shutdown sentinel and cascades via the normal enqueue helper
    // - type-1 work runs slot-12-style cleanup before the later context callback
    // - callback runs before the later release tail
    // - on the type-1 path, conditional context auto-release precedes the final work-item release
    // - the release bodies themselves are still source-owned vtable-dispatch scaffolds
    // - queue selection/pop happens under the real engine queue lock helper family at `+0x60`
    CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610* activeQueuePair =
        ActiveQueuePairStorageScaffold(this);
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue0C =
        activeQueuePair ? &activeQueuePair->queue00 : nullptr;
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* activeQueue34 =
        activeQueuePair ? &activeQueuePair->queue28 : nullptr;
    HANDLE activeQueueSignalEvent = queueWaitHelper5c_.eventHandle20;
    while (true) {
        CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* selectedQueue = nullptr;
        while (true) {
            (void)EnterQueueLockHelper();
            if (activeQueue34->writeCursor10 != activeQueue34->readCursor00) {
                selectedQueue = activeQueue34;
                break;
            }
            if (activeQueue0C->writeCursor10 != activeQueue0C->readCursor00) {
                selectedQueue = activeQueue0C;
                break;
            }
            if (nonBlocking) {
                (void)LeaveQueueLockHelper();
                return;
            }
            if (!activeQueueSignalEvent) {
                (void)LeaveQueueLockHelper();
                return;
            }

            const uint32_t waitResult = WaitQueueEventHelper(INFINITE);
            if (waitResult != 0u && waitResult != 3u) {
                return;
            }
            if (activeQueue0C->writeCursor10 != activeQueue0C->readCursor00 ||
                activeQueue34->writeCursor10 != activeQueue34->readCursor00) {
                break;
            }
            (void)LeaveQueueLockHelper();
        }

        CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair pair = {};
        const bool popped = Queue_TryPopPair(selectedQueue, &pair);
        (void)LeaveQueueLockHelper();
        if (!popped) {
            return;
        }

        void* workItem = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value0));
        void* context = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value1));
        if (!workItem) {
            (void)EnqueueCompletedOperation(
                nullptr,
                nullptr,
                /*useQueue34=*/false,
                "RunCompletedOperationQueueShutdownCascade");
            return;
        }

        const uint32_t workType = QueueWorkItem_GetType(workItem);
        const bool isType1 = (workType == kWorkTypeClose);
        CBaseConnection_0x4b8018* connection =
            static_cast<CBaseConnection_0x4b8018*>(context);
        const bool shouldAutoReleaseConnection =
            isType1 && connection != nullptr &&
            (connection->AutoReleaseFlag04() != 0u);

        spdlog::debug(
            "CLTThreadPerClientTCPEngine_0x4b2768::RunCompletedOperationQueue consume queue=[{}] workItem={} workType=0x{:08x} context={} autoReleaseType1Context={}",
            (selectedQueue == activeQueue34) ? "queue34" : "queue0C",
            fmt::ptr(workItem),
            workType,
            fmt::ptr(context),
            shouldAutoReleaseConnection ? 1u : 0u);
        LogConnectionDispatchForensics(
            "RunCompletedOperationQueue",
            context,
            workItem,
            workType);

        if (connection && isType1) {
            CleanupConnection(connection);
        }

        if (connection) {
            (void)connection->OnOperationCompleted(workItem);
        }

        if (shouldAutoReleaseConnection) {
            (void)connection->QueueRelease();
        }
        (void)QueuedWorkItem_InvokeReleaseSlot(workItem);
    }
}

// anchor: launcher.exe:0x436920
void CLTThreadPerClientTCPEngine_0x4b2768::StopQueueThreads() {
    CLTThreadPerClientTCPEngine_0x4b2768_QueueThread** queueThreadArray =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_QueueThread**>(field08_);
    const uint32_t existingQueueThreadCount = field04_;
    if (existingQueueThreadCount == 0u) {
        CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair pair = {};
        while (TryPopCompletedOperation(&pair, /*waitForSignal=*/false) == 0u) {
            void* workItem = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value0));
            void* context = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value1));
            if (workItem != nullptr && context != nullptr) {
                const uint32_t workType = QueueWorkItem_GetType(workItem);
                CBaseConnection_0x4b8018* connection =
                    static_cast<CBaseConnection_0x4b8018*>(context);
                LogConnectionDispatchForensics(
                    "StopQueueThreads",
                    context,
                    workItem,
                    workType);
                if (workType == kWorkTypeClose && connection) {
                    CleanupConnection(connection);
                }
                if (connection) {
                    (void)connection->OnOperationCompleted(workItem);
                }
                const bool shouldAutoReleaseConnection =
                    workType == kWorkTypeClose && connection != nullptr &&
                    (connection->AutoReleaseFlag04() != 0u);
                if (shouldAutoReleaseConnection) {
                    (void)connection->QueueRelease();
                }
                (void)QueuedWorkItem_InvokeReleaseSlot(workItem);
            }
        }
        return;
    }

    (void)EnqueueCompletedOperation(
        nullptr,
        nullptr,
        /*useQueue34=*/false,
        "StopQueueThreadsShutdown");
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

    field08_ = nullptr;
    field04_ = 0u;
}

// Source-owned extraction of the queue-thread allocation/start tail embedded in ctor 0x4366f0.
void CLTThreadPerClientTCPEngine_0x4b2768::CreateQueueThreadsForCtorCount(uint32_t queueThreadCount) {
    if (queueThreadCount == 0u) {
        field08_ = nullptr;
        field04_ = 0u;
        return;
    }

    CLTThreadPerClientTCPEngine_0x4b2768_QueueThread** queueThreadArray =
        static_cast<CLTThreadPerClientTCPEngine_0x4b2768_QueueThread**>(
            std::calloc(queueThreadCount, sizeof(CLTThreadPerClientTCPEngine_0x4b2768_QueueThread*)));
    if (!queueThreadArray) {
        field08_ = nullptr;
        field04_ = 0u;
        return;
    }

    for (uint32_t i = 0; i < queueThreadCount; ++i) {
        queueThreadArray[i] = new CLTThreadPerClientTCPEngine_0x4b2768_QueueThread(this);
        (void)queueThreadArray[i]->Start(/*startPriority=*/2);
    }

    field08_ = queueThreadArray;
    field04_ = queueThreadCount;
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
    (void)key;
    // The recovered outer callsites now read best as a plain endpoint-keyed map with staged
    // placeholder insertion before accept-thread attachment. Keep this legacy node-returning helper
    // only as a boundary marker while active callers migrate to direct map-like helpers.
    return reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode*>(endpointTreeHead80_);
}

// anchor: launcher.exe:0x42fe10
CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* CLTThreadPerClientTCPEngine_0x4b2768::ContextTree_Find(uint32_t key) {
    (void)key;
    // The recovered outer callsites now read best as a plain map lookup keyed by the normalized
    // connection/context pointer. Source keeps this legacy node-returning helper only as a stub so
    // the anchored method boundary remains visible while callers move to direct map-like helpers.
    return reinterpret_cast<CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode*>(contextTreeHead8c_);
}

// anchor: launcher.exe:0x431ff0
CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* CLTThreadPerClientTCPEngine_0x4b2768::CreateAndInsertWorkerThread(
    CMessageConnection_0x4b7928* connection,
    bool datagramMode,
    bool startThread) {
    if (!connection) {
        return nullptr;
    }

    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread> worker =
        std::make_unique<CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread>(connection, datagramMode);
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* createdWorker = worker.get();
    connection->SetWorkerThreadScaffold(createdWorker);

    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* result = nullptr;
    const uint32_t key = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(connection));
    (void)EnterCleanupLockHelper();
    bool inserted = false;
    result = ContextTreeInsertUniqueWorkerNode(
        this,
        contextTreeHead8c_,
        key,
        std::move(worker),
        &inserted);
    (void)LeaveCleanupLockHelper();

    if (!result) {
        connection->SetWorkerThreadScaffold(nullptr);
        return nullptr;
    }
    connection->SetEngine(this);
    connection->SetWorkerThreadScaffold(result);
    if (startThread) {
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
