#include "lttcpconnection.h"

#include "../libltbase/ltresult.h"
#include "../libltmessaging/variablelengthprefixedtcpstreamparser.h"
#include "ltthreadperclienttcpengine.h"

#include <winsock2.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

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

namespace {

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_BaseConnectionQueueContextVtable[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static CLTTCPReadOperationFragmentVTable g_ReadOperationFragmentSourceVtable = {};

// UNANCHORED: source-owned queue-context release bridge for current non-byte-faithful C++ objects.
static uint32_t __thiscall BaseConnectionQueueContext_ReleaseScaffold(
    CBaseConnection_QueueContextScaffold* /*self*/) {
    return 1u;
}

// UNANCHORED: source-owned queue-context completion bridge for current non-byte-faithful C++ objects.
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

// anchor: launcher.exe:0x42fd50 / vtable 0x004b2300 +0x00
static void* __thiscall ReadOperationFragmentSource_DeletingDtorScaffold(
    CLTTCPReadOperationFragmentScaffold* self,
    uint8_t deleteFlag) {
    if (self && (deleteFlag & 1u) != 0u) {
        std::free(self);
    }
    return nullptr;
}

// anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
static void __thiscall ReadOperationFragmentSource_AddRefScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }
    (void)InterlockedIncrement(reinterpret_cast<volatile LONG*>(&self->referenceCount));
}

// anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
static void __thiscall ReadOperationFragmentSource_ReleaseScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }

    const LONG remaining =
        InterlockedDecrement(reinterpret_cast<volatile LONG*>(&self->referenceCount));
    if (remaining == 0 && self->vtable && self->vtable->deleteIfNonNull) {
        self->vtable->deleteIfNonNull(self);
    }
}

// anchor: launcher.exe:0x004199b0 / vtable 0x004b2300 +0x0c
static void __thiscall ReadOperationFragmentSource_DeleteIfNonNullScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self || !self->vtable || !self->vtable->deletingDtor) {
        return;
    }

    (void)self->vtable->deletingDtor(self, 1u);
}

// anchor: launcher.exe:0x42f880 / vtable 0x004b2300 +0x10
static void __thiscall ReadOperationFragmentSource_ResetRefCountScaffold(
    CLTTCPReadOperationFragmentScaffold* self) {
    if (!self) {
        return;
    }
    (void)InterlockedExchange(reinterpret_cast<volatile LONG*>(&self->referenceCount), 0);
}

// anchor: launcher.exe:0x42f890 / vtable 0x004b2300 +0x14
static void __thiscall ReadOperationFragmentSource_SetRefCountFromPtrScaffold(
    CLTTCPReadOperationFragmentScaffold* self,
    const long* value) {
    if (!self || !value) {
        return;
    }
    (void)InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&self->referenceCount),
        static_cast<LONG>(*value));
}

// anchor: launcher.exe:0x42fe50 TCP receive-path `CLTTCPReadOperation` allocation/setup
static CLTTCPReadOperationFragmentScaffold* AllocateReadOperationFragmentSourceScaffold() {
    constexpr size_t kPayloadCapacity = 0x1000u;
    constexpr size_t kAllocationSize = offsetof(CLTTCPReadOperationFragmentScaffold, bytes0C) + kPayloadCapacity;
    if (!g_ReadOperationFragmentSourceVtable.addRef) {
        g_ReadOperationFragmentSourceVtable.deletingDtor =
            &ReadOperationFragmentSource_DeletingDtorScaffold;
        g_ReadOperationFragmentSourceVtable.addRef = &ReadOperationFragmentSource_AddRefScaffold;
        g_ReadOperationFragmentSourceVtable.release = &ReadOperationFragmentSource_ReleaseScaffold;
        g_ReadOperationFragmentSourceVtable.deleteIfNonNull =
            &ReadOperationFragmentSource_DeleteIfNonNullScaffold;
        g_ReadOperationFragmentSourceVtable.resetRefCount =
            &ReadOperationFragmentSource_ResetRefCountScaffold;
        g_ReadOperationFragmentSourceVtable.setRefCountFromPtr =
            &ReadOperationFragmentSource_SetRefCountFromPtrScaffold;
    }

    CLTTCPReadOperationFragmentScaffold* fragment =
        static_cast<CLTTCPReadOperationFragmentScaffold*>(std::calloc(1, kAllocationSize));
    if (!fragment) {
        return nullptr;
    }

    fragment->vtable = &g_ReadOperationFragmentSourceVtable;
    fragment->referenceCount = 0;
    fragment->byteCount = 0u;
    return fragment;
}

// anchor: launcher.exe:0x452350
static void ReadOperationFragmentSource_SetByteCountScaffold(
    CLTTCPReadOperationFragmentScaffold* fragment,
    uint32_t byteCount) {
    if (!fragment) {
        return;
    }
    fragment->byteCount = std::min<uint32_t>(byteCount, 0x1000u);
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
      engine_(nullptr),
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
      engine_(nullptr),
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
    : state_(initialState),
      queueContextScaffold_() {
    EnsureBaseConnectionQueueContextVtableInitialized();
    queueContextScaffold_.vtable = g_BaseConnectionQueueContextVtable;
    queueContextScaffold_.autoReleaseFlag = 0u;
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

// UNANCHORED: source-owned helper that recognizes the current queue-context bridge object and
// returns its owning `CBaseConnection` when present.
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

    CLTTCPReadOperationFragmentScaffold* readOperationFragment =
        AllocateReadOperationFragmentSourceScaffold();
    if (!readOperationFragment) {
        spdlog::warn(
            "CLTTCPConnection::ReceiveReadyReadOperationFragmentScaffold failed fragment allocation this={} remoteHost='{}'",
            fmt::ptr(this),
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
        return 0;
    }

    // anchor: launcher.exe:0x42fe50 TCP receive success path
    // Keep the same two worker-side fragment refs explicit here:
    // - one outer worker-owned ref immediately after allocation/setup
    // - one delivery-temp ref immediately before `OnReceive(fragment)`
    CLTTCPReadOperationFragment_AddRefScaffold(readOperationFragment);
    const int received = recv(
        static_cast<SOCKET>(socketHandle_),
        reinterpret_cast<char*>(readOperationFragment->bytes0C),
        0x1000,
        0);
    if (received <= 0) {
        CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
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

    ReadOperationFragmentSource_SetByteCountScaffold(
        readOperationFragment,
        static_cast<uint32_t>(received));
    CLTTCPReadOperationFragment_AddRefScaffold(readOperationFragment);
    OnReceive(readOperationFragment);
    CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
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
uint32_t CLTTCPConnection::Close(bool graceful) {
    if (state_ == LTTCPEngineConnectionState::kClosed) {
        return 0u;
    }

    return engine_
        ? engine_->CloseConnectionScaffold(this, graceful)
        : CloseSocketTransportScaffold(graceful);
}

// anchor: launcher.exe:0x449cd0
uint32_t CLTTCPConnection::Connect(const LTTCPEndpointKey& endpoint) {
    if (remoteEndpoint_.DiffersFrom(endpoint)) {
        (void)Close(false);
        endpoint.CopyTo(&remoteEndpoint_);
    }

    return engine_ ? engine_->ConnectConnectionScaffold(this) : 0u;
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
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    void* /*opaqueArg08*/,
    void* /*opaqueArg0c*/) {
    CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
}

// anchor: launcher.exe:0x449d40
void CLTTCPConnection::OnReceive(CLTTCPReadOperationFragmentScaffold* readOperationFragment) {
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

    CLTTCPReadOperationFragment_AddRefScaffold(readOperationFragment);
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
        (void)Close(false);
    }

    // `0x449d40` ends with a direct `readOperationFragment->Release()` on the outer OnReceive-held
    // temp ref. Keep that narrower than routing the final release back through the wider
    // `OnClose(fragment, opaqueArg08, opaqueArg0c)` callback wrapper.
    CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
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
