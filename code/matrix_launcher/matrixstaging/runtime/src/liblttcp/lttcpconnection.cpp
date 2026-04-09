#include "lttcpconnection.h"

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
LTTCPEndpointKey::LTTCPEndpointKey()
    : family(0),
      portNetworkOrder(0),
      ipv4NetworkOrder(0),
      reserved0(0),
      reserved1(0) {
    family = AF_INET;
}

// anchor: launcher.exe:0x44b020
bool LTTCPEndpointKey::DiffersFrom(const LTTCPEndpointKey& other) const {
    return std::memcmp(this, &other, sizeof(*this)) != 0;
}

// anchor: launcher.exe:0x44aff0
void LTTCPEndpointKey::CopyTo(LTTCPEndpointKey* outEndpointKey) const {
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
static void* g_BaseConnectionQueueContextVtable[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

// UNANCHORED: source-owned queue-dispatch ABI adapter release bridge for current non-byte-faithful
// C++ objects.
static uint32_t __thiscall BaseConnectionQueueContext_ReleaseScaffold(
    CBaseConnection_QueueContextScaffold* /*self*/) {
    return 1u;
}

// UNANCHORED: source-owned queue-dispatch ABI adapter completion bridge for raw client.dll queue
// consumers expecting original slot `+0x10` / `vtable[4]`.
static uint32_t __thiscall BaseConnectionQueueContext_OnOperationCompletedScaffold(
    CBaseConnection_QueueContextScaffold* self,
    void* workItem) {
    return (self && self->owner) ? self->owner->OnOperationCompleted(workItem) : 0u;
}

static void EnsureBaseConnectionQueueContextVtableInitialized() {
    if (!g_BaseConnectionQueueContextVtable[1]) {
        g_BaseConnectionQueueContextVtable[1] =
            reinterpret_cast<void*>(BaseConnectionQueueContext_ReleaseScaffold);
        g_BaseConnectionQueueContextVtable[4] =
            reinterpret_cast<void*>(BaseConnectionQueueContext_OnOperationCompletedScaffold);
    }
}

// UNANCHORED: source-owned endpoint formatting helper that mirrors the byte/port extraction shape
// used by `CLTTCPConnection::OnReceive` terminal parser-error logging.
static unsigned EndpointIpv4OctetForOnReceiveLogScaffold(
    const LTTCPEndpointKey& endpoint,
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
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      ownerContext_(nullptr),
      socketHandle_(kInvalidSocketHandle),
      workerThread08_(nullptr),
      remoteEndpoint_(),
      remoteHostName_(),
      sendQueueEmptyFlag38_(true),
      sendQueueMutex_(),
      sendQueue3c_(),
      parser06c_(new CVariableLengthPrefixedTCPStreamParser()) {}

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
// replacement-side owner-context scaffold.
CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      ownerContext_(ownerContext),
      socketHandle_(kInvalidSocketHandle),
      workerThread08_(nullptr),
      remoteEndpoint_(),
      remoteHostName_(),
      sendQueueEmptyFlag38_(true),
      sendQueueMutex_(),
      sendQueue3c_(),
      parser06c_(new CVariableLengthPrefixedTCPStreamParser()) {}

// anchor: launcher.exe:0x44ac40
CLTTCPConnection::~CLTTCPConnection() {
    delete parser06c_;
    parser06c_ = nullptr;
}

// UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
bool CBaseConnection::IsConnected() const {
    return static_cast<uint32_t>(state_) != static_cast<uint32_t>(LTTCPEngineConnectionState::kClosed);
}

// UNANCHORED: source-owned narrow mirror of the `0x44a9f0` base-ctor state write.
CBaseConnection::CBaseConnection(LTTCPEngineConnectionState initialState)
    : autoReleaseFlag04_(0u),
      padding05_07_{0u, 0u, 0u},
      engine_(nullptr),
      state_(initialState),
      queueContextScaffold_() {
    EnsureBaseConnectionQueueContextVtableInitialized();
    queueContextScaffold_.vtable = g_BaseConnectionQueueContextVtable;
    queueContextScaffold_.autoReleaseFlag = autoReleaseFlag04_;
    queueContextScaffold_.padding05[0] = 0u;
    queueContextScaffold_.padding05[1] = 0u;
    queueContextScaffold_.padding05[2] = 0u;
    queueContextScaffold_.owner = this;
}

// UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
void CLTTCPConnection::SetEngine(CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
}

// UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
CLTThreadPerClientTCPEngine* CLTTCPConnection::Engine() const {
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

// UNANCHORED: source-owned helper that recognizes the current queue-dispatch ABI adapter object
// and returns its owning `CBaseConnection` when present.
CBaseConnection* CBaseConnection_FromQueueContextScaffold(void* maybeQueueContext) {
    CBaseConnection_QueueContextScaffold* queueContext =
        static_cast<CBaseConnection_QueueContextScaffold*>(maybeQueueContext);
    if (!queueContext || queueContext->vtable != g_BaseConnectionQueueContextVtable) {
        return nullptr;
    }
    return queueContext->owner;
}

// UNANCHORED: source-owned helper for queue-consumer slot-12-style cleanup.
void* CBaseConnection_ResolveQueueCleanupContextKeyScaffold(void* maybeQueueContext) {
    CBaseConnection* owner = CBaseConnection_FromQueueContextScaffold(maybeQueueContext);
    return owner ? static_cast<void*>(owner) : maybeQueueContext;
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
    CLTThreadPerClientTCPEngine_WorkerThread* workerThread) {
    workerThread08_ = workerThread;
}

CLTThreadPerClientTCPEngine_WorkerThread* CLTTCPConnection::WorkerThreadScaffold() const {
    return workerThread08_;
}

// UNANCHORED: source-owned connection-state setter used by the current scaffolds.
void CLTTCPConnection::SetState(LTTCPEngineConnectionState state) {
    state_ = state;
}

// UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return CBaseConnection::State();
}

// UNANCHORED: source-owned endpoint setter over the recovered connection `+0x24` copy.
void CLTTCPConnection::SetRemoteEndpoint(const LTTCPEndpointKey& endpoint) {
    endpoint.CopyTo(&remoteEndpoint_);
}

// UNANCHORED: source-owned endpoint accessor over the recovered connection `+0x24` copy.
const LTTCPEndpointKey& CLTTCPConnection::RemoteEndpoint() const {
    return remoteEndpoint_;
}

// UNANCHORED: source-owned hostname setter used by the current resolver scaffold.
void CLTTCPConnection::SetRemoteHostName(const char* hostName) {
    remoteHostName_ = hostName ? hostName : "";
}

// UNANCHORED: source-owned hostname accessor used by the current resolver scaffold.
const std::string& CLTTCPConnection::RemoteHostName() const {
    return remoteHostName_;
}

bool CLTTCPConnection::QueueSendBufferScaffold(
    const void* buffer,
    uint32_t byteCount,
    uintptr_t ownershipMode) {
    if (!buffer || byteCount == 0u) {
        return false;
    }

    // Tightened worker/send-path read from `0x448a00 -> vtable +0x20(..., 1)` and
    // `0x42fbd0 -> 0x44ad80`:
    // - the active message-envelope send path reaches slot `8` with ownership mode `1`, i.e. the
    //   copied-byte queue path
    // - current source therefore keeps the active path faithful and safe by copying queued bytes
    //   into owned storage before the worker-thread write loop drains them
    // - other historical queue modes (`0` borrowed / `2` caller-owned pointer`) remain a later
    //   fidelity target if a live source path starts proving them
    CLTTCPConnection_SendQueueItemScaffold item = {};
    remoteEndpoint_.CopyTo(&item.remoteEndpoint);
    item.ownedBytes.assign(
        static_cast<const uint8_t*>(buffer),
        static_cast<const uint8_t*>(buffer) + byteCount);

    {
        std::lock_guard<std::mutex> lock(sendQueueMutex_);
        sendQueue3c_.push_back(std::move(item));
        sendQueueEmptyFlag38_ = false;
    }

    spdlog::debug(
        "CLTTCPConnection::QueueSendBufferScaffold queued copied send bytes ownershipMode={} this={} worker={} byteCount={} remoteHost='{}'",
        static_cast<unsigned>(ownershipMode),
        fmt::ptr(this),
        fmt::ptr(workerThread08_),
        byteCount,
        remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
    return true;
}

bool CLTTCPConnection::TryPopQueuedSendBufferScaffold(
    CLTTCPConnection_SendQueueItemScaffold* outItem) {
    if (!outItem) {
        return false;
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex_);
    if (sendQueue3c_.empty()) {
        sendQueueEmptyFlag38_ = true;
        return false;
    }

    *outItem = std::move(sendQueue3c_.front());
    sendQueue3c_.pop_front();
    sendQueueEmptyFlag38_ = sendQueue3c_.empty();
    return true;
}

bool CLTTCPConnection::SendQueueEmptyFlagScaffold() const {
    std::lock_guard<std::mutex> lock(sendQueueMutex_);
    return sendQueueEmptyFlag38_;
}

int CLTTCPConnection::ReceiveReadyReadOperationFragmentScaffold(
    uint32_t* outWsaError,
    bool* outPeerClosed) {
    if (outWsaError) {
        *outWsaError = 0u;
    }
    if (outPeerClosed) {
        *outPeerClosed = false;
    }

    if (socketHandle_ == kInvalidSocketHandle ||
        (state_ != LTTCPEngineConnectionState::kConnectActive &&
         state_ != LTTCPEngineConnectionState::kUdpMonitorActive &&
         state_ != LTTCPEngineConnectionState::kClosing)) {
        return -1;
    }

    CLTTCPReadOperation* readOperationFragment =
        new (std::nothrow) CLTTCPReadOperation;
    if (!readOperationFragment) {
        spdlog::warn(
            "CLTTCPConnection::ReceiveReadyReadOperationFragmentScaffold failed fragment allocation this={} remoteHost='{}'",
            fmt::ptr(this),
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
        return 0;
    }

    // anchor: launcher.exe:0x42fe50 TCP receive success path
    // `0x42fe50` does not call a read-operation ctor here.
    // After the fixed-size `0x100c` allocator returns, the worker writes the live leaf layout
    // explicitly as:
    // - `vtable = 0x004b2300`
    // - `referenceCount04 = 0`
    // - `byteCount08 = 0`
    // The C++ `new (std::nothrow) CLTTCPReadOperation` above is only used to materialize that
    // same live leaf vptr in source before these explicit field writes.
    readOperationFragment->referenceCount04 = 0;
    readOperationFragment->byteCount08 = 0u;

    // Keep the same two worker-side fragment refs explicit here:
    // - one outer worker-owned ref immediately after allocation/setup
    // - one delivery-temp ref immediately before `OnReceive(fragment)`
    readOperationFragment->AddRef();
    const int received = recv(
        static_cast<SOCKET>(socketHandle_),
        reinterpret_cast<char*>(readOperationFragment + 1),
        0x1000,
        0);
    if (received <= 0) {
        readOperationFragment->Release();
        if (received == 0) {
            if (outPeerClosed) {
                *outPeerClosed = true;
            }
            return -1;
        }

        const uint32_t wsaError = static_cast<uint32_t>(WSAGetLastError());
        if (outWsaError) {
            *outWsaError = wsaError;
        }
        return (wsaError == WSAEWOULDBLOCK) ? 0 : -1;
    }

    readOperationFragment->SetByteCount(static_cast<uint32_t>(received));
    readOperationFragment->AddRef();
    OnReceive(readOperationFragment);
    readOperationFragment->Release();
    return received;
}

// anchor: launcher.exe:0x42fe50 TCP receive subpath
int CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold() {
    if (socketHandle_ == kInvalidSocketHandle ||
        (state_ != LTTCPEngineConnectionState::kConnectActive &&
         state_ != LTTCPEngineConnectionState::kUdpMonitorActive)) {
        return 0;
    }

    SOCKET socket = static_cast<SOCKET>(socketHandle_);
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);
    timeval timeout = {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
    if (ready == SOCKET_ERROR) {
        const int wsaError = WSAGetLastError();
        spdlog::debug(
            "CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold select failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            wsaError);
        (void)CloseSocketTransportScaffold(/*graceful=*/false);
        return -1;
    }
    if (ready == 0 || !FD_ISSET(socket, &readSet)) {
        return 0;
    }

    uint32_t wsaError = 0u;
    bool peerClosed = false;
    const int received = ReceiveReadyReadOperationFragmentScaffold(&wsaError, &peerClosed);
    if (received >= 0) {
        return received;
    }

    if (peerClosed) {
        spdlog::info(
            "CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold recv returned EOF socket=0x{:08x} remoteHost='{}'",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
    } else {
        spdlog::warn(
            "CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold recv failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            wsaError);
    }

    // The legacy helper keeps its older source-owned transport-close side effect for the fallback
    // launcher bridge seam. The tighter worker-thread path now uses
    // `ReceiveReadyReadOperationFragmentScaffold()` directly so it can mirror `0x42fe50`
    // terminal ordering without forcing this older helper to own those later state transitions.
    (void)CloseSocketTransportScaffold(/*graceful=*/false);
    return -1;
}

// anchor: launcher.exe:0x449ca0
uint32_t CBaseConnection::Close(bool graceful) {
    if (state_ == LTTCPEngineConnectionState::kClosed) {
        return 0u;
    }

    if (engine_) {
        // `0x449ca0` forwards the base connection object itself into engine vtable slot `+0x1c`.
        return engine_->Close(this, graceful);
    }

    // Source-owned null-engine fallback kept only because the current C++ layout still permits
    // detached connection objects while the wider base-field reconstruction remains incomplete.
    return static_cast<CLTTCPConnection*>(this)->CloseSocketTransportScaffold(graceful);
}

// anchor: launcher.exe:0x449cd0
uint32_t CLTTCPConnection::Connect(const LTTCPEndpointKey& endpoint) {
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

    return engine_
        ? engine_->SendBufferConnectionScaffold(this, buffer, byteCount, completionContext)
        : SendRawSocketBufferScaffold(buffer, byteCount, completionContext);
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
    CLTTCPConnection_ParsedPacketWorkItemScaffold* completedPacketWorkItem = nullptr;

    if (readOperationFragment) {
        readOperationFragment->AddRef();
    }
    uint32_t parseResult = parser06c_->Parse(readOperationFragment, &completedPacketWorkItem);
    while (parseResult == 0u) {
        EnqueueCompletedPacketWorkItemScaffold(completedPacketWorkItem);
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
            const char* resultName = libltbase::CResultNameArrayItem_GetResultName(parseResult);
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
        (void)static_cast<CBaseConnection*>(this)->Close(false);
    }

    // `0x449d40` ends with a direct `readOperationFragment->+0x08()` on the outer OnReceive-held
    // temp ref. Keep that narrower than routing the final release back through the wider
    // `OnClose(fragment, opaqueArg08, opaqueArg0c)` callback wrapper or the generic helper.
    if (readOperationFragment) {
        readOperationFragment->Release();
    }
}

// UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
uint32_t CLTTCPConnection::CloseSocketTransportScaffold(bool /*graceful*/) {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    state_ = LTTCPEngineConnectionState::kClosing;
    if (socketHandle_ != kInvalidSocketHandle) {
        const SOCKET socket = static_cast<SOCKET>(socketHandle_);
        const int shutdownResult = shutdown(socket, SD_BOTH);
        if (shutdownResult == SOCKET_ERROR) {
            const int wsaError = WSAGetLastError();
            if (wsaError != WSAENOTCONN) {
                spdlog::debug(
                    "CLTTCPConnection::CloseSocketTransportScaffold shutdown failed socket=0x{:08x} remoteHost='{}' wsaError={}",
                    socketHandle_,
                    remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
                    wsaError);
            }
        }
        const int closeResult = closesocket(socket);
        if (closeResult == SOCKET_ERROR) {
            spdlog::debug(
                "CLTTCPConnection::CloseSocketTransportScaffold closesocket failed socket=0x{:08x} remoteHost='{}' wsaError={}",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
                WSAGetLastError());
        }
    }
    socketHandle_ = kInvalidSocketHandle;
    return 1u;
}

// UNANCHORED: low-level raw-socket send helper used beneath the anchored SendBuffer wrapper.
uint32_t CLTTCPConnection::SendRawSocketBufferScaffold(
    const void* buffer,
    uint32_t byteCount,
    void* /*completionContext*/) {
    if (!buffer || byteCount == 0u) {
        return 0u;
    }

    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        spdlog::debug(
            "CLTTCPConnection::SendRawSocketBufferScaffold rejected send because state={} socket=0x{:08x} remoteHost='{}' byteCount={}",
            static_cast<uint32_t>(state_),
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            byteCount);
        return 0u;
    }

    if (socketHandle_ == kInvalidSocketHandle) {
        spdlog::debug(
            "CLTTCPConnection::SendRawSocketBufferScaffold rejected send because socket is invalid state={} remoteHost='{}' byteCount={}",
            static_cast<uint32_t>(state_),
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            byteCount);
        return 0u;
    }

    const int sent = send(
        static_cast<SOCKET>(socketHandle_),
        static_cast<const char*>(buffer),
        static_cast<int>(byteCount),
        0);
    if (sent == SOCKET_ERROR) {
        const int wsaError = WSAGetLastError();
        spdlog::debug(
            "CLTTCPConnection::SendRawSocketBufferScaffold send failed socket=0x{:08x} remoteHost='{}' byteCount={} wsaError={}",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            byteCount,
            wsaError);
        if (wsaError != WSAEWOULDBLOCK) {
            (void)CloseSocketTransportScaffold(/*graceful=*/false);
        }
        return 0u;
    }
    if (sent != static_cast<int>(byteCount)) {
        spdlog::debug(
            "CLTTCPConnection::SendRawSocketBufferScaffold short send socket=0x{:08x} remoteHost='{}' requested={} sent={}",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            byteCount,
            sent);
        return 0u;
    }
    return 1u;
}

// UNANCHORED: source-owned mirror of the exact `0x449d8a` enqueue handoff.
void CLTTCPConnection::EnqueueCompletedPacketWorkItemScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    // Current best static read of `0x449d40` / `0x469bf0`:
    // - the queue handoff is exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    // - launcher.exe queues the direct connection object there, but the active replacement may
    //   project that identity through `QueueContextScaffold()` before raw client.dll consumers see
    //   it so MSVC slot assumptions do not hit the MinGW object vtable directly
    // - this receive path therefore always targets queue0C through `0x436820`
    // - original caller-side lifetime does not depend on enqueue success because `0x436820`
    //   returns `void`; once we reach this seam the completed parsed-packet work item is
    //   queue-owned / consumer-owned rather than connection-owned
    if (!Engine()) {
        return;
    }

    Engine()->EnqueueCompletedOperationFromConnectionScaffold(
        workItem,
        static_cast<CLTTCPConnection*>(this),
        "CLTTCPConnection::OnReceive");
}

}  // namespace mxo::liblttcp
