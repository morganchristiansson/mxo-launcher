#include "lttcpconnection.h"

#include "ltthreadperclienttcpengine.h"

#include <winsock2.h>
#include <algorithm>
#include <cstdint>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

namespace {

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;

// UNANCHORED: source-owned endpoint-key comparison helper for the current connection wrapper.
static bool EndpointKeysDiffer(const LTTCPEndpointKey& lhs, const LTTCPEndpointKey& rhs) {
    return lhs.family != rhs.family ||
        lhs.portNetworkOrder != rhs.portNetworkOrder ||
        lhs.ipv4NetworkOrder != rhs.ipv4NetworkOrder ||
        lhs.reserved0 != rhs.reserved0 ||
        lhs.reserved1 != rhs.reserved1;
}

// UNANCHORED: current source-owned callback shim for the `param_1->+0x04(outWorkItem)` step
// visible at the start of `CLTTCPConnection::OnReceive`.
static void ConnectionReceiveCallback_PrepareWorkItem(void* callbackContext, void** outWorkItem) {
    if (!callbackContext || !outWorkItem) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(callbackContext);
    if (!vtable || !vtable[1]) {
        return;
    }

    typedef void (__thiscall *PrepareFn)(void*, void**);
    PrepareFn fn = reinterpret_cast<PrepareFn>(vtable[1]);
    fn(callbackContext, outWorkItem);
}

// UNANCHORED: current source-owned callback shim for the `param_1->+0x08()` step
// visible at the end of `CLTTCPConnection::OnClose` / `CLTTCPConnection::OnReceive`.
static void ConnectionReceiveCallback_Finalize(void* callbackContext) {
    if (!callbackContext) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(callbackContext);
    if (!vtable || !vtable[2]) {
        return;
    }

    typedef void (__thiscall *FinalizeFn)(void*);
    FinalizeFn fn = reinterpret_cast<FinalizeFn>(vtable[2]);
    fn(callbackContext);
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

// UNANCHORED: source-owned convenience ctor for the current replacement-side connection model.
CLTTCPConnection::CLTTCPConnection()
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      engine_(nullptr),
      ownerContext_(nullptr),
      socketHandle_(kInvalidSocketHandle),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

// UNANCHORED: source-owned convenience ctor that seeds owner-context state.
CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      engine_(nullptr),
      ownerContext_(ownerContext),
      socketHandle_(kInvalidSocketHandle),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

// anchor: launcher.exe:0x44ac40
CLTTCPConnection::~CLTTCPConnection() = default;

// UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
bool CBaseConnection::IsConnected() const {
    return static_cast<uint32_t>(state_) != static_cast<uint32_t>(LTTCPEngineConnectionState::kClosed);
}

// UNANCHORED: source-owned base-field initializer for the recovered state slot.
CBaseConnection::CBaseConnection(LTTCPEngineConnectionState initialState)
    : state_(initialState) {}

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

// UNANCHORED: source-owned nonblocking socket poll helper used by the launcher bridge scaffolds.
int CLTTCPConnection::PollReceiveNonBlocking() {
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
                "CLTTCPConnection::PollReceiveNonBlocking peer closed socket=0x{:08x} remoteHost='{}'",
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
                "CLTTCPConnection::PollReceiveNonBlocking recv(MSG_PEEK) failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
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
                "CLTTCPConnection::PollReceiveNonBlocking recv returned EOF socket=0x{:08x} remoteHost='{}'",
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
            "CLTTCPConnection::PollReceiveNonBlocking recv failed socket=0x{:08x} remoteHost='{}' wsaError={} -> closing",
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

// UNANCHORED: source-owned diagnostic accessor over the buffered receive bytes.
const std::vector<uint8_t>& CLTTCPConnection::ReceivedBytes() const {
    return receivedBytes_;
}

// UNANCHORED: source-owned buffered receive reset helper.
void CLTTCPConnection::ClearReceivedBytes() {
    receivedBytes_.clear();
}

// UNANCHORED: source-owned buffered receive prefix-consumption helper.
void CLTTCPConnection::ConsumeReceivedBytesPrefix(size_t byteCount) {
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
void CLTTCPConnection::OnClose(void* callbackContext) {
    ConnectionReceiveCallback_Finalize(callbackContext);
}

// anchor: launcher.exe:0x449d40
uint32_t CLTTCPConnection::OnReceive(void* callbackContext) {
    // Current best read:
    // - `workItem` is the parser-emitted completed packet/work object yielded through
    //   connection `+0x6c` (`CVariableLengthPrefixedTCPStreamParser::Parse`)
    // - current parser-side allocator path now narrows that emitted object to the same
    //   `0x2c` / vtable-`0x4b3e08` family built by `0x435db0 -> 0x435090`
    // - the exact role of the initial callback `+0x04(&slot)` step is still narrower than our
    //   current source model, so keep treating this local as a conservative shared out-slot
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem = nullptr;
    ConnectionReceiveCallback_PrepareWorkItem(
        callbackContext,
        reinterpret_cast<void**>(&workItem));

    uint32_t result = pollReceive(callbackContext, &workItem);
    while (result == 0u) {
        pushCompletedOperation(workItem, this, /*useQueue34=*/false);
        workItem = nullptr;
        result = pollReceive(nullptr, &workItem);
    }

    if (static_cast<int32_t>(result) > 0 && result != 0x7000000u) {
        // Current source scaffolds still do not reconstruct the original `+0x24` endpoint-copy
        // logging helper family used here before close. Keep the control-flow shape faithful first.
        (void)Close(false);
    }

    OnClose(callbackContext);
    return 1u;
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

// UNANCHORED: source-owned mirror of the connection `+0x6c` parser call shape seen in `0x449d40`.
uint32_t CLTTCPConnection::pollReceive(
    void* callbackContext,
    CLTTCPConnection_ParsedPacketWorkItemScaffold** outWorkItem) {
    // Current best static read of `0x449d40`:
    // - connection `+0x6c` is now narrowed to a
    //   `CVariableLengthPrefixedTCPStreamParser`-family helper
    // - original callee is `CVariableLengthPrefixedTCPStreamParser::Parse` at `0x469bf0`
    // - parser vtable slot `+0x10` / `0x469b40` allocates the emitted completed-packet object via
    //   `0x435db0 -> 0x435090`, i.e. the same `0x2c` / vtable-`0x4b3e08` work-item family already
    //   seen in queue producer xrefs
    // - first receive pass reaches it with the current callback/read-operation fragment and `&workItem`
    //   - current best fragment shape from `0x469bf0`:
    //     - dword `+0x08` = byte count
    //     - bytes begin at `+0x0c`
    //     - vtable `+0x04` / `+0x08` = retain/release-style lifetime hooks
    // - later drain passes reach it with `(0, &workItem)` until the parser stops yielding packets
    // The faithful parser/read-operation object family is not reconstructed yet, so keep this
    // source-owned scaffold conservative and side-effect-free.
    (void)callbackContext;
    if (outWorkItem) {
        *outWorkItem = nullptr;
    }
    return 1u;
}

// UNANCHORED: source-owned mirror of the queue-enqueue helper call shape seen in `0x449d40`.
void CLTTCPConnection::pushCompletedOperation(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    void* context,
    bool useQueue34) {
    // Current best static read of `0x449d40`:
    // - queued submission shape is `(engine+0x10, workItem, this, false)` through `0x436820`
    // - current source path can now forward that enqueue into the engine-side queue helper when
    //   the connection already has an attached engine sidecar
    if (!Engine()) {
        return;
    }

    (void)Engine()->EnqueueCompletedOperationFromConnectionScaffold(
        workItem,
        context,
        useQueue34,
        "CLTTCPConnection::OnReceive");
}

}  // namespace mxo::liblttcp
