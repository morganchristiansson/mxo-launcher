#include "lttcpconnection.h"
#include "../../../../src/launcher_network_object_abi.h"

#include "../libltbase/ltresult.h"
#include "../libltmessaging/variablelengthprefixedtcpstreamparser.h"
#include "ltthreadperclienttcpengine.h"

#include <winsock2.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

namespace {

struct CLTTCPReadOperationPoolBackingBlock {
    CLTTCPReadOperationPoolBackingBlock* next;
};

struct CLTTCPReadOperationFreeListNode {
    CLTTCPReadOperationFreeListNode* next;
};

constexpr size_t kCLTTCPReadOperationStorageSize =
    sizeof(CLTTCPReadOperation) + CLTTCPReadOperation::kPayloadCapacity;

volatile LONG g_TrackedAllocationBytes = 0;
volatile LONG g_TrackedAllocationCount = 0;
uint32_t g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock = 0u;
uint32_t g_CLTTCPReadOperationFixedAllocatorBackingBlockCount = 0u;
size_t g_CLTTCPReadOperationFixedAllocatorBackingBlockBytes = 0u;
CLTTCPReadOperationPoolBackingBlock* g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead =
    nullptr;
CLTTCPReadOperationFreeListNode* g_CLTTCPReadOperationFixedAllocatorFreeListHead = nullptr;
size_t g_CLTTCPReadOperationFixedAllocatorExtraObjectBytes = 0u;
CRITICAL_SECTION g_CLTTCPReadOperationFixedAllocatorCriticalSection;
std::once_flag g_CLTTCPReadOperationFixedAllocatorInitOnce;
bool g_CLTTCPReadOperationFixedAllocatorInitialized = false;

static void CLTTCPReadOperationFixedAllocator_Init() noexcept;
static void CLTTCPReadOperationFixedAllocator_Shutdown() noexcept;

// UNANCHORED: source-owned startup bridge because current source does not run the original
// explicit init call tree that reaches `0x4a27e0` before the first read-operation allocation.
// The active pool body below now mirrors the original tracked-allocation updates and first backing
// block sizing heuristic, but this lazy-init entry still remains source-owned.
static void EnsureCLTTCPReadOperationFixedAllocatorInitializedScaffold() {
    std::call_once(
        g_CLTTCPReadOperationFixedAllocatorInitOnce,
        []() { CLTTCPReadOperationFixedAllocator_Init(); });
}

// UNANCHORED: source-owned direct critical-section lock helper standing in for the small lock
// object methods reached through `g_CLTTCPReadOperationFixedAllocatorLock` in launcher.exe.
static void CLTTCPReadOperationFixedAllocator_LockScaffold() {
    EnterCriticalSection(&g_CLTTCPReadOperationFixedAllocatorCriticalSection);
}

// UNANCHORED: source-owned direct critical-section unlock helper standing in for the same lock
// object method family.
static void CLTTCPReadOperationFixedAllocator_UnlockScaffold() {
    LeaveCriticalSection(&g_CLTTCPReadOperationFixedAllocatorCriticalSection);
}

// UNANCHORED: source-owned local equivalent of `0x434960`, which returns the cached system page
// size used by `0x452400` to seed the first backing-block sizing heuristic.
static uint32_t CLTTCPReadOperationFixedAllocator_SystemPageSizeScaffold() {
    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);
    return static_cast<uint32_t>(systemInfo.dwPageSize);
}

// anchor: launcher.exe:0x4a27e0
static void CLTTCPReadOperationFixedAllocator_Init() noexcept {
    InitializeCriticalSection(&g_CLTTCPReadOperationFixedAllocatorCriticalSection);
    g_CLTTCPReadOperationFixedAllocatorExtraObjectBytes = 0u;
    g_CLTTCPReadOperationFixedAllocatorInitialized = true;
    std::atexit(CLTTCPReadOperationFixedAllocator_Shutdown);
}

// anchor: launcher.exe:0x452370
static void CLTTCPReadOperationFixedAllocator_ClearPool() noexcept {
    CLTTCPReadOperationFixedAllocator_LockScaffold();
    CLTTCPReadOperationPoolBackingBlock* backingBlock =
        g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead;
    while (backingBlock) {
        CLTTCPReadOperationPoolBackingBlock* nextBackingBlock = backingBlock->next;
        const size_t backingBlockBytes = static_cast<size_t>(_msize(backingBlock));
        (void)InterlockedExchangeAdd(
            &g_TrackedAllocationBytes,
            -static_cast<LONG>(backingBlockBytes));
        (void)InterlockedDecrement(&g_TrackedAllocationCount);
        std::free(backingBlock);
        backingBlock = nextBackingBlock;
    }
    g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead = nullptr;
    g_CLTTCPReadOperationFixedAllocatorFreeListHead = nullptr;
    g_CLTTCPReadOperationFixedAllocatorBackingBlockCount = 0u;
    CLTTCPReadOperationFixedAllocator_UnlockScaffold();
}

// anchor: launcher.exe:0x4a7660
static void CLTTCPReadOperationFixedAllocator_Shutdown() noexcept {
    if (g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead) {
        CLTTCPReadOperationFixedAllocator_ClearPool();
    }
    if (g_CLTTCPReadOperationFixedAllocatorInitialized) {
        DeleteCriticalSection(&g_CLTTCPReadOperationFixedAllocatorCriticalSection);
        g_CLTTCPReadOperationFixedAllocatorInitialized = false;
    }
}

// anchor: launcher.exe:0x452400
static void* CLTTCPReadOperationFixedAllocator_AllocateStorage() noexcept {
    const size_t objectStride =
        g_CLTTCPReadOperationFixedAllocatorExtraObjectBytes + kCLTTCPReadOperationStorageSize;
    CLTTCPReadOperationFixedAllocator_LockScaffold();

    CLTTCPReadOperationFreeListNode* storage = g_CLTTCPReadOperationFixedAllocatorFreeListHead;
    if (!storage) {
        if (!g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead) {
            if (g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock == 0u) {
                g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock = 1u;
            }

            size_t backingPayloadBytes =
                static_cast<size_t>(g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock) *
                objectStride;
            const size_t preferredBackingPayloadBytes =
                (static_cast<size_t>(CLTTCPReadOperationFixedAllocator_SystemPageSizeScaffold()) >> 1u) -
                sizeof(CLTTCPReadOperationPoolBackingBlock);
            if (backingPayloadBytes < preferredBackingPayloadBytes) {
                g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock =
                    static_cast<uint32_t>(preferredBackingPayloadBytes / objectStride);
                backingPayloadBytes =
                    static_cast<size_t>(g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock) *
                    objectStride;
            }
            g_CLTTCPReadOperationFixedAllocatorBackingBlockBytes =
                backingPayloadBytes + sizeof(CLTTCPReadOperationPoolBackingBlock);
        }

        const LONG trackedBackingBlockBytes =
            static_cast<LONG>(g_CLTTCPReadOperationFixedAllocatorBackingBlockBytes);
        (void)InterlockedExchangeAdd(&g_TrackedAllocationBytes, trackedBackingBlockBytes);
        (void)InterlockedIncrement(&g_TrackedAllocationCount);
        CLTTCPReadOperationPoolBackingBlock* backingBlock =
            static_cast<CLTTCPReadOperationPoolBackingBlock*>(
                std::malloc(g_CLTTCPReadOperationFixedAllocatorBackingBlockBytes));
        if (!backingBlock) {
            CLTTCPReadOperationFixedAllocator_UnlockScaffold();
            return nullptr;
        }

        backingBlock->next = g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead;
        ++g_CLTTCPReadOperationFixedAllocatorBackingBlockCount;
        uint8_t* objectStorage = reinterpret_cast<uint8_t*>(backingBlock + 1);
        CLTTCPReadOperationFreeListNode* previousFreeListHead =
            g_CLTTCPReadOperationFixedAllocatorFreeListHead;
        g_CLTTCPReadOperationFixedAllocatorBackingBlockListHead = backingBlock;
        for (uint32_t remaining = g_CLTTCPReadOperationFixedAllocatorObjectCountPerBackingBlock;
             remaining != 0u;
             --remaining) {
            CLTTCPReadOperationFreeListNode* freeListNode =
                reinterpret_cast<CLTTCPReadOperationFreeListNode*>(objectStorage);
            freeListNode->next = previousFreeListHead;
            previousFreeListHead = freeListNode;
            objectStorage += objectStride;
        }
        storage = previousFreeListHead;
    }

    g_CLTTCPReadOperationFixedAllocatorFreeListHead = storage
        ? storage->next
        : nullptr;
    CLTTCPReadOperationFixedAllocator_UnlockScaffold();
    return storage;
}

// anchor: launcher.exe:0x452520
static void CLTTCPReadOperation_FreeStorage(void* storage) noexcept {
    if (!storage) {
        return;
    }

    CLTTCPReadOperationFixedAllocator_LockScaffold();
    CLTTCPReadOperationFreeListNode* freeListNode =
        static_cast<CLTTCPReadOperationFreeListNode*>(storage);
    freeListNode->next = g_CLTTCPReadOperationFixedAllocatorFreeListHead;
    g_CLTTCPReadOperationFixedAllocatorFreeListHead = freeListNode;
    CLTTCPReadOperationFixedAllocator_UnlockScaffold();
}

// anchor: launcher.exe:0x452560
static void* CLTTCPReadOperation_AllocateStorage() noexcept {
    EnsureCLTTCPReadOperationFixedAllocatorInitializedScaffold();
    return CLTTCPReadOperationFixedAllocator_AllocateStorage();
}

}  // namespace

// anchor: launcher.exe:0x44b070
LTTCPEndpointKey_0x44b070::LTTCPEndpointKey_0x44b070()
    : family(0),
      portNetworkOrder(0),
      ipv4NetworkOrder(0),
      reserved0(0),
      reserved1(0) {
    family = AF_INET;
}

// anchor: launcher.exe:0x44b090
LTTCPEndpointKey_0x44b070::LTTCPEndpointKey_0x44b070(uint32_t ipv4NetOrder, uint16_t portHostOrder)
    : family(0),
      portNetworkOrder(0),
      ipv4NetworkOrder(0),
      reserved0(0),
      reserved1(0) {
    family = AF_INET;
    ipv4NetworkOrder = ipv4NetOrder;
    portNetworkOrder = htons(portHostOrder);
}

// anchor: launcher.exe:0x44b020
bool LTTCPEndpointKey_0x44b070::DiffersFrom(const LTTCPEndpointKey_0x44b070& other) const {
    return std::memcmp(this, &other, sizeof(*this)) != 0;
}

// anchor: launcher.exe:0x44aff0
void LTTCPEndpointKey_0x44b070::CopyTo(LTTCPEndpointKey_0x44b070* outEndpointKey) const {
    std::memcpy(outEndpointKey, this, sizeof(*this));
}

// anchor: launcher.exe:0x42f820 / vtable 0x004b211c +0x00
CRefCountedReadOperationBase* CRefCountedReadOperationBase::DeletingDtor(
    uint8_t deleteFlag) {
    if ((deleteFlag & 1u) != 0u) {
        std::free(this);
    }
    return this;
}

// anchor: launcher.exe:0x42f7e0 / vtable 0x004b211c +0x04
void CRefCountedReadOperationBase::AddRef() {
    ++referenceCount04;
}

// anchor: launcher.exe:0x42f7f0 / vtable 0x004b211c +0x08
void CRefCountedReadOperationBase::Release() {
    if (--referenceCount04 == 0) {
        DeleteIfNonNull();
    }
}

// anchor: launcher.exe:0x004199b0 / vtable 0x004b211c +0x0c
void CRefCountedReadOperationBase::DeleteIfNonNull() {
    (void)DeletingDtor(1u);
}

// anchor: launcher.exe:0x42f800 / vtable 0x004b211c +0x10
void CRefCountedReadOperationBase::ResetRefCount() {
    referenceCount04 = 0;
}

// anchor: launcher.exe:0x42f810 / vtable 0x004b211c +0x14
void CRefCountedReadOperationBase::SetRefCountFromPtr(const long* value) {
    referenceCount04 = *value;
}

// anchor: launcher.exe:0x42fd50 / vtable 0x004b2300 +0x00
CRefCountedReadOperationBase* CLTTCPReadOperation::DeletingDtor(
    uint8_t deleteFlag) {
    const long referenceCountCopy = referenceCount04;
    (void)new (static_cast<CRefCountedReadOperationBase*>(this))
        CRefCountedReadOperationBase;
    referenceCount04 = referenceCountCopy;
    if ((deleteFlag & 1u) != 0u) {
        CLTTCPReadOperation_FreeStorage(this);
    }
    return reinterpret_cast<CRefCountedReadOperationBase*>(this);
}

// anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
void CLTTCPReadOperation::AddRef() {
    (void)InterlockedIncrement(reinterpret_cast<volatile LONG*>(&referenceCount04));
}

// anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
void CLTTCPReadOperation::Release() {
    const LONG remaining =
        InterlockedDecrement(reinterpret_cast<volatile LONG*>(&referenceCount04));
    if (remaining == 0) {
        DeleteIfNonNull();
    }
}

// anchor: launcher.exe:0x42f880 / vtable 0x004b2300 +0x10
void CLTTCPReadOperation::ResetRefCount() {
    (void)InterlockedExchange(reinterpret_cast<volatile LONG*>(&referenceCount04), 0);
}

// anchor: launcher.exe:0x42f890 / vtable 0x004b2300 +0x14
void CLTTCPReadOperation::SetRefCountFromPtr(const long* value) {
    (void)InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&referenceCount04),
        static_cast<LONG>(*value));
}

void* CLTTCPReadOperation::operator new(
    std::size_t /*requestedSize*/,
    const std::nothrow_t&) noexcept {
    return CLTTCPReadOperation_AllocateStorage();
}

void CLTTCPReadOperation::operator delete(void* storage) noexcept {
    CLTTCPReadOperation_FreeStorage(storage);
}

void CLTTCPReadOperation::operator delete(
    void* storage,
    const std::nothrow_t&) noexcept {
    CLTTCPReadOperation_FreeStorage(storage);
}

// anchor: launcher.exe:0x452350
void CLTTCPReadOperation::SetByteCount(uint32_t byteCount) {
    if (kPayloadCapacity < byteCount) {
        byteCount = kPayloadCapacity;
    }
    byteCount08 = byteCount;
}

namespace {

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;

// UNANCHORED: source-owned endpoint formatting helper that mirrors the byte/port extraction shape
// used by `CLTTCPConnection::OnReceive` terminal parser-error logging.
static unsigned EndpointIpv4OctetForOnReceiveLogScaffold(
    const LTTCPEndpointKey_0x44b070& endpoint,
    unsigned byteIndex) {
    return static_cast<unsigned>((endpoint.ipv4NetworkOrder >> (byteIndex * 8u)) & 0xffu);
}

}  // namespace

// ============================================================
// VTable 0x004b8034 - CLTTCPConnection (Base Class)
// High-confidence recovered wrapper entries:
// - 0x004b8040 -> 0x00449ca0 = Close wrapper into engine slot +0x1c
// - 0x004b8048 -> 0x00449d40 = OnReceive
// - 0x004b804c -> 0x00449fd0 = OnClose callback forwarder
// - 0x004b8050 -> 0x00449cd0 = Connect wrapper into engine slot +0x18
// - 0x004b8054 -> 0x00449d20 = SendBuffer wrapper into engine slot +0x20
// ============================================================

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family.
CLTTCPConnection::CLTTCPConnection()
    : CBaseConnection_0x4b8018(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_(),
      ownerContext_(nullptr),
      socketHandle_(kInvalidSocketHandle),
      workerThread08_(nullptr),
      pendingSendQueueState38_(),
      parser06c_(new CVariableLengthPrefixedTCPStreamParser()) {}

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
// replacement-side owner-context scaffold.
CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection_0x4b8018(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_(),
      ownerContext_(ownerContext),
      socketHandle_(kInvalidSocketHandle),
      workerThread08_(nullptr),
      pendingSendQueueState38_(),
      parser06c_(new CVariableLengthPrefixedTCPStreamParser()) {}

// anchor: launcher.exe:0x44ac40
CLTTCPConnection::~CLTTCPConnection() {
    delete parser06c_;
    parser06c_ = nullptr;
}

// UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
bool CBaseConnection_0x4b8018::IsConnected() const {
    return static_cast<uint32_t>(state_) != static_cast<uint32_t>(LTTCPEngineConnectionState::kClosed);
}

// UNANCHORED: source-owned narrow mirror of the `0x44a9f0` base-ctor state write.
CBaseConnection_0x4b8018::CBaseConnection_0x4b8018(LTTCPEngineConnectionState initialState)
    : autoReleaseFlag04_(0u),
      padding05_07_{0u, 0u, 0u},
      engine_(nullptr),
      state_(initialState),
      queueContextScaffold_() {
    InitializeBaseConnectionQueueContextScaffold(&queueContextScaffold_, this, autoReleaseFlag04_);
}

// UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
void CLTTCPConnection::SetEngine(CLTThreadPerClientTCPEngine_0x4b2768* engine) {
    engine_ = engine;
}

// UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
CLTThreadPerClientTCPEngine_0x4b2768* CLTTCPConnection::Engine() const {
    return engine_;
}

// UNANCHORED: source-owned owner-context setter used by the current scaffolds.
void CLTTCPConnection::SetOwnerContext(void* ownerContext) {
    ownerContext_ = ownerContext;
}

// UNANCHORED: source-owned owner-context accessor used by the current scaffolds.
void* CLTTCPConnection::OwnerContext() const {
    return ownerContext_;
}

// UNANCHORED: source-owned helper for queue-consumer slot-12-style cleanup.
void* CBaseConnection_ResolveQueueCleanupContextKeyScaffold(void* maybeQueueContext) {
    CBaseConnection_0x4b8018* owner = CBaseConnection_FromQueueContextScaffold(maybeQueueContext);
    return owner ? static_cast<void*>(owner) : maybeQueueContext;
}

// UNANCHORED: source-owned ABI-dispatch wrapper for queued context completion callbacks.
uint32_t CBaseConnection_InvokeQueuedOnOperationCompletedScaffold(void* maybeQueueContext, void* workItem) {
    CBaseConnection_0x4b8018* owner = CBaseConnection_FromQueueContextScaffold(maybeQueueContext);
    if (owner) {
        return owner->OnOperationCompleted(workItem);
    }
    if (!maybeQueueContext) {
        return 0u;
    }

    CBaseConnection_0x4b8018* directConnection = static_cast<CBaseConnection_0x4b8018*>(maybeQueueContext);
    void** directVtable = *reinterpret_cast<void***>(directConnection);
    spdlog::warn(
        "DIAGNOSTIC: direct/native queued connection dispatch context={} vtable={} slot1={} slot4={} workItem={}",
        fmt::ptr(directConnection),
        fmt::ptr(directVtable),
        fmt::ptr(directVtable ? directVtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex] : nullptr),
        fmt::ptr(directVtable ? directVtable[CBaseConnection_QueueContextScaffold::kOnOperationCompletedSlotIndex] : nullptr),
        fmt::ptr(workItem));
    return directConnection->OnOperationCompleted(workItem);
}

// UNANCHORED: source-owned ABI-dispatch wrapper for generic queued work-item slot-`+0x04`
// release calls.
uint32_t QueuedWorkItem_InvokeReleaseSlotScaffold(void* object) {
    if (!object) {
        return 0u;
    }

    void** vtable = *reinterpret_cast<void***>(object);
    if (!vtable || !vtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex]) {
        return 0u;
    }

    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = reinterpret_cast<ReleaseFn>(
        vtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex]);
    return fn(object);
}

// UNANCHORED: source-owned ABI-dispatch wrapper for queued connection-context auto-release calls.
uint32_t QueuedConnectionContext_InvokeAutoReleaseScaffold(void* maybeQueueContext) {
    CBaseConnection_0x4b8018* owner = CBaseConnection_FromQueueContextScaffold(maybeQueueContext);
    if (!owner) {
        if (maybeQueueContext) {
            void** directVtable = *reinterpret_cast<void***>(maybeQueueContext);
            spdlog::warn(
                "DIAGNOSTIC: direct/native queued connection auto-release bypass context={} vtable={} slot1={} slot4={}",
                fmt::ptr(maybeQueueContext),
                fmt::ptr(directVtable),
                fmt::ptr(directVtable ? directVtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex] : nullptr),
                fmt::ptr(directVtable ? directVtable[CBaseConnection_QueueContextScaffold::kOnOperationCompletedSlotIndex] : nullptr));
        }
        return 0u;
    }

    CBaseConnection_QueueContextScaffold* queueContext =
        static_cast<CBaseConnection_QueueContextScaffold*>(maybeQueueContext);
    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = queueContext->vtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex]
        ? reinterpret_cast<ReleaseFn>(
              queueContext->vtable[CBaseConnection_QueueContextScaffold::kReleaseSlotIndex])
        : nullptr;
    return fn ? fn(queueContext) : 0u;
}

// UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
void CLTTCPConnection::SetSocketHandle(uint32_t socketHandle) {
    socketHandle_ = socketHandle;
}

// UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
uint32_t CLTTCPConnection::SocketHandle() const {
    return socketHandle_;
}

void CLTTCPConnection::SetWorkerThreadScaffold(
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread) {
    workerThread08_ = workerThread;
}

CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* CLTTCPConnection::WorkerThreadScaffold() const {
    return workerThread08_;
}

// UNANCHORED: source-owned connection-state setter used by the current scaffolds.
void CLTTCPConnection::SetState(LTTCPEngineConnectionState state) {
    state_ = state;
}

// UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return CBaseConnection_0x4b8018::State();
}

CLTTCPConnection_QueuedSendBufferStorage::CLTTCPConnection_QueuedSendBufferStorage()
    : usesPooledBuffer00_(0u),
      padding01_03_{0u, 0u, 0u},
      bufferBytes04_(),
      bufferByteCount08_(0u) {}

bool CLTTCPConnection_QueuedSendBufferStorage::InitializeFromSendBuffer(
    const void* sendBuffer,
    uint32_t byteCount,
    uintptr_t ownershipMode) {
    Reset();
    if (!sendBuffer || byteCount == 0u) {
        return false;
    }

    // Current launcher.exe ownership-mode split recovered from `0x44ac90` / `0x44a7c0`:
    // - `0` => transfer caller-owned tracked heap buffer; release path uses tracked free
    // - `1` => copy into a pooled `0x1000` send buffer block
    // - `2` => transfer an already-pooled `0x1000` send buffer block back to that pool
    // The active launcher path still reaches mode `1`. Current source keeps a bounded copied-byte
    // stand-in for every mode because the original tracked allocators / pooled raw-buffer transfer
    // contracts are not fully reconstructed here yet.
    (void)ownershipMode;
    usesPooledBuffer00_ = 1u;
    bufferBytes04_.assign(
        static_cast<const uint8_t*>(sendBuffer),
        static_cast<const uint8_t*>(sendBuffer) + byteCount);
    bufferByteCount08_ = byteCount;
    return true;
}

void CLTTCPConnection_QueuedSendBufferStorage::Reset() {
    usesPooledBuffer00_ = 0u;
    bufferBytes04_.clear();
    bufferByteCount08_ = 0u;
}

const uint8_t* CLTTCPConnection_QueuedSendBufferStorage::BufferBytes() const {
    return bufferBytes04_.empty() ? nullptr : bufferBytes04_.data();
}

uint32_t CLTTCPConnection_QueuedSendBufferStorage::BufferByteCount() const {
    return bufferByteCount08_;
}

CLTTCPConnection_PendingSendQueue::CLTTCPConnection_PendingSendQueue()
    : sendQueueEmptyFlag38_(1u),
      padding39_3b_{0u, 0u, 0u},
      pendingSendQueueMutex_(),
      pendingSendQueue3c_() {}

// anchor: launcher.exe:0x44ac90
bool CLTTCPConnection_PendingSendQueue::QueueSendBufferWithEndpoint(
    const void* sendBuffer,
    uint32_t byteCount,
    const LTTCPEndpointKey_0x44b070& remoteEndpoint,
    uintptr_t ownershipMode) {
    CLTTCPConnection_QueuedSendBufferWithEndpoint queuedSendBuffer = {};
    if (!queuedSendBuffer.sendBufferStorage00.InitializeFromSendBuffer(
            sendBuffer,
            byteCount,
            ownershipMode)) {
        return false;
    }

    remoteEndpoint.CopyTo(&queuedSendBuffer.remoteEndpoint04);
    {
        std::lock_guard<std::mutex> lock(pendingSendQueueMutex_);
        pendingSendQueue3c_.push_back(std::move(queuedSendBuffer));
        sendQueueEmptyFlag38_ = 0u;
    }
    return true;
}

// anchor: launcher.exe:0x44ad80
bool CLTTCPConnection_PendingSendQueue::QueueSendBuffer(
    const void* sendBuffer,
    uint32_t byteCount,
    uintptr_t ownershipMode) {
    LTTCPEndpointKey_0x44b070 defaultEndpoint;
    return QueueSendBufferWithEndpoint(sendBuffer, byteCount, defaultEndpoint, ownershipMode);
}

// anchor: launcher.exe:0x44aa70
bool CLTTCPConnection_PendingSendQueue::TryPopQueuedSendBufferWithEndpoint(
    CLTTCPConnection_QueuedSendBufferWithEndpoint* outItem) {
    if (!outItem) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pendingSendQueueMutex_);
    if (pendingSendQueue3c_.empty()) {
        sendQueueEmptyFlag38_ = 1u;
        return false;
    }

    *outItem = std::move(pendingSendQueue3c_.front());
    pendingSendQueue3c_.pop_front();
    sendQueueEmptyFlag38_ = pendingSendQueue3c_.empty() ? 1u : 0u;
    return true;
}

bool CLTTCPConnection_PendingSendQueue::SendQueueEmptyFlag() const {
    std::lock_guard<std::mutex> lock(pendingSendQueueMutex_);
    return sendQueueEmptyFlag38_ != 0u;
}

// anchor: launcher.exe:0x44ab60 helper family consumed by CleanupConnection
void CLTTCPConnection_PendingSendQueue::ReleasePendingSendQueueContents() {
    std::lock_guard<std::mutex> lock(pendingSendQueueMutex_);
    pendingSendQueue3c_.clear();
    sendQueueEmptyFlag38_ = 1u;
}

// anchor: launcher.exe:0x44ad80
bool CLTTCPConnection::QueueSendBuffer(
    const void* buffer,
    uint32_t byteCount,
    uintptr_t ownershipMode) {
    const bool queued = pendingSendQueueState38_.QueueSendBufferWithEndpoint(
        buffer,
        byteCount,
        remoteEndpoint_,
        ownershipMode);
    if (!queued) {
        return false;
    }

    spdlog::debug(
        "CLTTCPConnection::QueueSendBuffer queued copied send bytes ownershipMode={} this={} worker={} byteCount={}",
        static_cast<unsigned>(ownershipMode),
        fmt::ptr(this),
        fmt::ptr(workerThread08_),
        byteCount);
    return true;
}

// anchor: launcher.exe:0x44ac90
bool CLTTCPConnection::QueueSendBufferWithEndpoint(
    const void* buffer,
    uint32_t byteCount,
    const LTTCPEndpointKey_0x44b070& remoteEndpoint,
    uintptr_t ownershipMode) {
    return pendingSendQueueState38_.QueueSendBufferWithEndpoint(
        buffer,
        byteCount,
        remoteEndpoint,
        ownershipMode);
}

// anchor: launcher.exe:0x44aa70
bool CLTTCPConnection::TryPopQueuedSendBufferWithEndpoint(
    CLTTCPConnection_QueuedSendBufferWithEndpoint* outItem) {
    return pendingSendQueueState38_.TryPopQueuedSendBufferWithEndpoint(outItem);
}

bool CLTTCPConnection::SendQueueEmptyFlag() const {
    return pendingSendQueueState38_.SendQueueEmptyFlag();
}

// anchor: launcher.exe:0x44ab60 helper family consumed by CleanupConnection
void CLTTCPConnection::ReleasePendingSendQueueContentsScaffold() {
    pendingSendQueueState38_.ReleasePendingSendQueueContents();
}

// anchor: launcher.exe:0x449ca0
uint32_t CBaseConnection_0x4b8018::Close(bool graceful) {
    if (state_ == LTTCPEngineConnectionState::kClosed) {
        return 0u;
    }

    // `0x449ca0` forwards the base connection object itself into engine vtable slot `+0x1c`.
    return engine_ ? engine_->Close(this, graceful) : 0u;
}

// anchor: launcher.exe:0x449cd0
uint32_t CLTTCPConnection::Connect(const LTTCPEndpointKey_0x44b070& endpoint) {
    if (remoteEndpoint_.DiffersFrom(endpoint)) {
        (void)Close(false);
        endpoint.CopyTo(&remoteEndpoint_);
    }

    // `0x449cd0` forwards the direct connection object into engine slot `+0x18`; it does not
    // rebuild a synthetic `(port, ip, context)` helper call.
    return engine_ ? engine_->Connect(this) : 0u;
}

// anchor: launcher.exe:0x449d20
uint32_t CLTTCPConnection::SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext) {
    if (!buffer || byteCount == 0u) {
        return 0u;
    }

    // `0x449d20` forwards `(buffer, byteCount, this, completionContext)` into engine slot
    // `+0x20`; the current C++ interface now uses that recovered argument order directly.
    return engine_ ? engine_->SendBuffer(buffer, byteCount, this, completionContext) : 0u;
}

// anchor: launcher.exe:0x449fd0
void CLTTCPConnection::OnClose(
    CLTTCPReadOperation* readOperationFragment,
    void* /*opaqueArg08*/,
    void* /*opaqueArg0c*/) {
    if (readOperationFragment) {
        readOperationFragment->Release();
    }
}

// anchor: launcher.exe:0x449d40
void CLTTCPConnection::OnReceive(CLTTCPReadOperation* readOperationFragment) {
    // Current best static read of `0x449d40`:
    // - the explicit arg is a typed, refcounted `CLTTCPReadOperation`-family buffer fragment
    // - the early fragment `+0x04` call is a no-arg AddRef / retain on that fragment only
    // - connection `+0x6c` is a concrete `CVariableLengthPrefixedTCPStreamParser` object
    //   at vtable `0x004baf84`, not an anonymous helper blob
    // - first parser handoff is `parser->Parse(fragment, &completedPacketWorkItem)`
    // - later drain handoffs are `parser->Parse(nullptr, &completedPacketWorkItem)`
    // - successful emits then hand off exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    //   through `0x436820`
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* completedPacketWorkItem = nullptr;

    if (readOperationFragment) {
        readOperationFragment->AddRef();
    }
    uint32_t parseResult = parser06c_->Parse(readOperationFragment, &completedPacketWorkItem);
    while (parseResult == 0u) {
        CLTThreadPerClientTCPEngine_0x4b2768* const engine = Engine();
        if (engine) {
            engine->EnqueueCompletedOperation(
                completedPacketWorkItem,
                QueueContextScaffold(),
                /*useQueue34=*/false,
                "CLTTCPConnection::OnReceive");
        }
        completedPacketWorkItem = nullptr;
        parseResult = parser06c_->Parse(nullptr, &completedPacketWorkItem);
    }

    if (static_cast<int32_t>(parseResult) > 0 && parseResult != 0x7000000u) {
        const unsigned ipv4Byte0 = EndpointIpv4OctetForOnReceiveLogScaffold(remoteEndpoint_, 0u);
        const unsigned ipv4Byte1 = EndpointIpv4OctetForOnReceiveLogScaffold(remoteEndpoint_, 1u);
        const unsigned ipv4Byte2 = EndpointIpv4OctetForOnReceiveLogScaffold(remoteEndpoint_, 2u);
        const unsigned ipv4Byte3 = EndpointIpv4OctetForOnReceiveLogScaffold(remoteEndpoint_, 3u);
        const unsigned portHostOrder = static_cast<unsigned>(ntohs(remoteEndpoint_.portNetworkOrder));

        // anchor: launcher.exe:0x449dbc / 0x449eb7 terminal parser-error logging split
        // `0x449d40` special-cases `0x7000000b` as a stream-corruption log and otherwise emits the
        // generic unrecoverable-error text plus `CResultNameArrayItem_GetResultName(parseResult)`.
        // The helper strips the original `LT` / `LT_` prefix family just like `0x417650`.
        if (parseResult == 0x700000bu) {
            spdlog::warn(
                "CLTTCPConnection::OnReceive(): Stream corrupted on connection to {}.{}.{}.{}:{}!  Closing connection!",
                ipv4Byte0,
                ipv4Byte1,
                ipv4Byte2,
                ipv4Byte3,
                portHostOrder);
        } else {
            const char* resultName = CResultNameArrayItem_GetResultName(parseResult);
            spdlog::warn(
                "CLTTCPConnection::OnReceive(): An unrecoverable error ({}) occurred on connection to {}.{}.{}.{}:{}!  Closing connection!",
                resultName,
                ipv4Byte0,
                ipv4Byte1,
                ipv4Byte2,
                ipv4Byte3,
                portHostOrder);
        }
        // `0x449fac..0x449fb2` reaches the inherited base close slot through virtual dispatch.
        (void)static_cast<CBaseConnection_0x4b8018*>(this)->Close(false);
    }

    // `0x449d40` ends with a direct `readOperationFragment->+0x08()` on the outer OnReceive-held
    // temp ref. Keep that narrower than routing the final release back through the wider
    // `OnClose(fragment, opaqueArg08, opaqueArg0c)` callback wrapper or the generic helper.
    if (readOperationFragment) {
        readOperationFragment->Release();
    }
}


}  // namespace mxo::liblttcp
