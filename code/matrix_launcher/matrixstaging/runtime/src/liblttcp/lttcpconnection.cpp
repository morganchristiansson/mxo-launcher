#include "lttcpconnection.h"

#include "../libltmessaging/variablelengthprefixedtcpstreamparser.h"
#include "ltthreadperclienttcpengine.h"

#include <winsock2.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

namespace {

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_BaseConnectionQueueContextVtable[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static CLTTCPReadOperationFragmentVTable g_ReadOperationFragmentSourceVtable = {};

// UNANCHORED: source-owned endpoint-key comparison helper for the current connection wrapper.
static bool EndpointKeysDiffer(const LTTCPEndpointKey& lhs, const LTTCPEndpointKey& rhs) {
    return lhs.family != rhs.family ||
        lhs.portNetworkOrder != rhs.portNetworkOrder ||
        lhs.ipv4NetworkOrder != rhs.ipv4NetworkOrder ||
        lhs.reserved0 != rhs.reserved0 ||
        lhs.reserved1 != rhs.reserved1;
}

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
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_(),
      parser06c_(new CVariableLengthPrefixedTCPStreamParser()) {}

// UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
// replacement-side owner-context scaffold.
CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      engine_(nullptr),
      ownerContext_(ownerContext),
      socketHandle_(kInvalidSocketHandle),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_(),
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

// UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
void CLTTCPConnection::SetSocketHandle(uint32_t socketHandle) {
    socketHandle_ = socketHandle;
}

// UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
uint32_t CLTTCPConnection::SocketHandle() const {
    return socketHandle_;
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
    remoteEndpoint_ = endpoint;
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

// UNANCHORED: internal socket-read helper that fills the connection-owned buffered-byte staging
// used by the faithful `0x42fe50 -> 0x449d40 -> 0x469bf0` receive seam.
int CLTTCPConnection::ReceiveBufferedSocketBytesNonBlockingScaffold() {
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
    if (ready <= 0 || !FD_ISSET(socket, &readSet)) {
        return 0;
    }

    u_long available = 0;
    if (ioctlsocket(socket, FIONREAD, &available) != 0) {
        return 0;
    }

    if (available == 0) {
        char probeByte = 0;
        const int peekResult = recv(socket, &probeByte, 1, MSG_PEEK);
        if (peekResult == 0) {
            spdlog::info(
                "CLTTCPConnection::ReceiveBufferedSocketBytesNonBlockingScaffold peer closed socket=0x{:08x} remoteHost='{}'",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }
        if (peekResult == SOCKET_ERROR) {
            const int wsaError = WSAGetLastError();
            if (wsaError == WSAEWOULDBLOCK) {
                return 0;
            }
            spdlog::warn(
                "CLTTCPConnection::ReceiveBufferedSocketBytesNonBlockingScaffold recv(MSG_PEEK) failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
                wsaError);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }
        available = 1;
    }

    const int toRead = static_cast<int>(std::min<u_long>(available, 4096));
    if (toRead <= 0) {
        return 0;
    }

    const size_t oldSize = receivedBytes_.size();
    receivedBytes_.resize(oldSize + static_cast<size_t>(toRead));
    const int received = recv(
        socket,
        reinterpret_cast<char*>(receivedBytes_.data() + oldSize),
        toRead,
        0);
    if (received <= 0) {
        receivedBytes_.resize(oldSize);
        if (received == 0) {
            spdlog::info(
                "CLTTCPConnection::ReceiveBufferedSocketBytesNonBlockingScaffold recv returned EOF socket=0x{:08x} remoteHost='{}'",
                socketHandle_,
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            state_ = LTTCPEngineConnectionState::kClosed;
            closesocket(socket);
            socketHandle_ = kInvalidSocketHandle;
            return -1;
        }

        const int wsaError = WSAGetLastError();
        if (wsaError == WSAEWOULDBLOCK) {
            return 0;
        }
        spdlog::warn(
            "CLTTCPConnection::ReceiveBufferedSocketBytesNonBlockingScaffold recv failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
            socketHandle_,
            remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_,
            wsaError);
        state_ = LTTCPEngineConnectionState::kClosed;
        closesocket(socket);
        socketHandle_ = kInvalidSocketHandle;
        return -1;
    }

    receivedBytes_.resize(oldSize + static_cast<size_t>(received));
    return received;
}

// anchor: launcher.exe:0x42fe50 TCP receive subpath
int CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold() {
    const int received = ReceiveBufferedSocketBytesNonBlockingScaffold();
    if (received <= 0) {
        return received;
    }

    size_t consumedBytes = 0u;
    while (consumedBytes < receivedBytes_.size()) {
        const size_t chunkByteCount = std::min<size_t>(receivedBytes_.size() - consumedBytes, 0x1000u);
        CLTTCPReadOperationFragmentScaffold* readOperationFragment =
            AllocateReadOperationFragmentSourceScaffold();
        if (!readOperationFragment) {
            spdlog::warn(
                "CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold failed fragment allocation this={} bufferedBytes={} remoteHost='{}'",
                fmt::ptr(this),
                static_cast<unsigned>(receivedBytes_.size() - consumedBytes),
                remoteHostName_.empty() ? std::string("<empty>") : remoteHostName_);
            break;
        }

        std::memcpy(
            readOperationFragment->bytes0C,
            receivedBytes_.data() + consumedBytes,
            chunkByteCount);
        ReadOperationFragmentSource_SetByteCountScaffold(
            readOperationFragment,
            static_cast<uint32_t>(chunkByteCount));
        CLTTCPReadOperationFragment_AddRefScaffold(readOperationFragment);
        OnReceive(readOperationFragment);
        CLTTCPReadOperationFragment_ReleaseScaffold(readOperationFragment);
        consumedBytes += chunkByteCount;
    }

    if (consumedBytes != 0u) {
        ConsumeBufferedSocketBytesPrefixScaffold(consumedBytes);
    }
    return received;
}

// UNANCHORED: internal buffered-byte prefix-consumption helper used after staged fragment
// delivery drains bytes out of the connection-owned socket-read staging.
void CLTTCPConnection::ConsumeBufferedSocketBytesPrefixScaffold(size_t byteCount) {
    if (byteCount == 0u) {
        return;
    }
    if (byteCount >= receivedBytes_.size()) {
        receivedBytes_.clear();
        return;
    }
    receivedBytes_.erase(receivedBytes_.begin(), receivedBytes_.begin() + static_cast<std::ptrdiff_t>(byteCount));
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
    if (EndpointKeysDiffer(remoteEndpoint_, endpoint)) {
        (void)Close(false);
        remoteEndpoint_ = endpoint;
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
void CLTTCPConnection::OnReceive(void* readOperationFragment) {
    // Current best static read of `0x449d40`:
    // - the explicit arg is a refcounted `CLTTCPReadOperation`-family buffer fragment
    // - the early fragment `+0x04` call is a no-arg AddRef / retain on that fragment only
    // - connection `+0x6c` is a concrete `CVariableLengthPrefixedTCPStreamParser` object
    //   at vtable `0x004baf84`, not an anonymous helper blob
    // - first parser handoff is `parser->Parse(fragment, &completedPacketWorkItem)`
    // - later drain handoffs are `parser->Parse(nullptr, &completedPacketWorkItem)`
    // - successful emits then hand off exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    //   through `0x436820`
    CLTTCPReadOperationFragmentScaffold* fragment =
        static_cast<CLTTCPReadOperationFragmentScaffold*>(readOperationFragment);
    CLTTCPConnection_ParsedPacketWorkItemScaffold* completedPacketWorkItem = nullptr;

    CLTTCPReadOperationFragment_AddRefScaffold(fragment);
    uint32_t parseResult = parser06c_
        ? parser06c_->Parse(fragment, &completedPacketWorkItem)
        : 1u;
    while (parseResult == 0u) {
        EnqueueCompletedPacketWorkItemScaffold(completedPacketWorkItem);
        completedPacketWorkItem = nullptr;
        parseResult = parser06c_
            ? parser06c_->Parse(nullptr, &completedPacketWorkItem)
            : 1u;
    }

    if (static_cast<int32_t>(parseResult) > 0 && parseResult != 0x7000000u) {
        // Current source scaffolds still do not reconstruct the original `+0x24` endpoint-copy
        // logging helper family used here before close. Keep the control-flow shape faithful first.
        (void)Close(false);
    }

    OnClose(fragment);
}

// UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
uint32_t CLTTCPConnection::CloseSocketTransportScaffold(bool /*graceful*/) {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0u;
    }

    state_ = LTTCPEngineConnectionState::kClosing;
    if (socketHandle_ != kInvalidSocketHandle) {
        closesocket(static_cast<SOCKET>(socketHandle_));
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
        return 0u;
    }

    if (socketHandle_ == kInvalidSocketHandle) {
        return 0u;
    }

    const int sent = send(
        static_cast<SOCKET>(socketHandle_),
        static_cast<const char*>(buffer),
        static_cast<int>(byteCount),
        0);
    return (sent == static_cast<int>(byteCount)) ? 1u : 0u;
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
