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

// UNANCHORED: source-owned shim for the explicit fragment `+0x04` virtual at the start of
// `CLTTCPConnection::OnReceive` / `CVariableLengthPrefixedTCPStreamParser::Parse`.
// Current best static read from the assembly:
// - this is a no-arg AddRef / retain on the fragment object itself
// - the apparent stack values around the call belong to the immediately following parser / append
//   call, not to the fragment virtual
static void ReadOperationFragment_AddRef(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->addRef) {
        return;
    }

    fragment->vtable->addRef(fragment);
}

// UNANCHORED: source-owned release shim for the read-operation fragment object passed to
// `CLTTCPConnection::OnReceive` / `CLTTCPConnection::OnClose`.
// Current best static read of the concrete `CLTTCPReadOperation` family:
// - decrements interlocked refcount at `+0x04`
// - zero-count path dispatches vtable `+0x0c`, which then reaches the deleting-dtor-style slot `+0x00`
static void ReadOperationFragment_Release(CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!fragment || !fragment->vtable || !fragment->vtable->release) {
        return;
    }

    fragment->vtable->release(fragment);
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
void CLTTCPConnection::OnClose(
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    void* /*opaqueArg08*/,
    void* /*opaqueArg0c*/) {
    ReadOperationFragment_Release(readOperationFragment);
}

// anchor: launcher.exe:0x449d40
void CLTTCPConnection::OnReceive(void* readOperationFragment) {
    // Current best static read of `0x449d40`:
    // - the explicit arg is a refcounted `CLTTCPReadOperation`-family buffer fragment
    // - the early fragment `+0x04` call is a no-arg AddRef / retain on that fragment only
    //   - the stack values visible around that call belong to the immediately following parser
    //     call, not to the fragment virtual itself
    // - connection `+0x6c` is a `CVariableLengthPrefixedTCPStreamParser` family object
    //   - current recovered source-side parser prefix now includes:
    //     - `+0x04` retained current-cursor fragment
    //     - `+0x08` next unread buffered byte pointer
    //     - `+0x0c` unread buffered byte count
    //     - `+0x10` provisional advanced-byte-count state
    //     - `+0x14` current parser-owned work item
    // - first parser handoff is `Parse(fragment, &completedPacketWorkItem)`
    // - later drain handoffs are `Parse(nullptr, &completedPacketWorkItem)`
    // - successful emits then hand off exactly `(engine+0x10, completedPacketWorkItem, this, false)`
    //   through `0x436820`; this path does not use queue34 and does not branch on enqueue success
    // - parser-emitted `completedPacketWorkItem` is the same `0x2c` / vtable-`0x4b3e08`
    //   `CParsedPacketWorkItem` family built by `0x435db0 -> 0x435090`
    // - that object family is both:
    //   - the parser-owned assembly state while bytes are still buffered, and
    //   - the completed packet object once `Parse(...)` returns `0`
    // - after each successful emit, `ResetAfterPacket` allocates a fresh replacement work item and
    //   may carry the tail fragment / cursor forward when unread stream bytes remain buffered
    // - the final fragment `+0x08` here releases only the outer OnReceive-held fragment reference
    CLTTCPReadOperationFragmentScaffold* fragment =
        static_cast<CLTTCPReadOperationFragmentScaffold*>(readOperationFragment);
    CLTTCPConnection_ParsedPacketWorkItemScaffold* completedPacketWorkItem = nullptr;

    ReadOperationFragment_AddRef(fragment);
    uint32_t parseResult = ParseReadOperationFragmentScaffold(fragment, &completedPacketWorkItem);
    while (parseResult == 0u) {
        pushCompletedOperation(completedPacketWorkItem);
        completedPacketWorkItem = nullptr;
        parseResult = ParseReadOperationFragmentScaffold(nullptr, &completedPacketWorkItem);
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

// UNANCHORED: source-owned mirror of the connection `+0x6c` parser call shape seen in `0x449d40`.
uint32_t CLTTCPConnection::ParseReadOperationFragmentScaffold(
    CLTTCPReadOperationFragmentScaffold* readOperationFragment,
    CLTTCPConnection_ParsedPacketWorkItemScaffold** outWorkItem) {
    // Current best static read of `0x449d40` / `0x469bf0`:
    // - connection `+0x6c` is a `CVariableLengthPrefixedTCPStreamParser` family object
    // - original callee is `CVariableLengthPrefixedTCPStreamParser::Parse` at `0x469bf0`
    // - current recovered parser prefix at connection `+0x6c` is:
    //   - `+0x04` retained fragment currently containing parser cursor `+0x08`
    //   - `+0x08` next unread buffered byte pointer
    //   - `+0x0c` total unread buffered byte count across retained fragments
    //   - `+0x10` provisional byte-count accumulator advanced by `0x472660`
    //   - `+0x14` current parser-owned `CParsedPacketWorkItem`
    // - first receive pass reaches it as `Parse(readOperationFragment, &completedPacketWorkItem)`
    //   immediately after a no-arg fragment AddRef in `0x449d40`
    // - later drain passes reach it as `Parse(nullptr, &completedPacketWorkItem)` until the parser
    //   stops yielding complete packets
    // - parser slot `+0x10` / `0x469b40` allocates the current `CParsedPacketWorkItem` object via
    //   `0x435db0 -> 0x435090`, i.e. the same `0x2c` / vtable-`0x4b3e08` work-item family already
    //   seen in queue producer xrefs
    // - that current work item starts as parser-owned assembly state and becomes the emitted
    //   completed packet object when `Parse(...)` returns `0`
    // - current best static read of `0x469bf0` also narrows the emit contract further:
    //   - `Parse(...) == 0` writes `*outWorkItem = parser+0x14` before `ResetAfterPacket`
    //   - no evidence currently supports an intentional `Parse(...) == 0` / `*outWorkItem == nullptr`
    //     result on this receive path; null work items belong to later lifecycle/shutdown producers
    // - `0x4725c0` / `ResetAfterPacket` then allocates a fresh replacement object and, when unread
    //   bytes remain in the old tail fragment, carries that tail fragment plus the new cursor into
    //   the replacement work item
    // - `Parse` itself also uses fragment `+0x04` / `+0x08` as no-arg AddRef / Release hooks while
    //   transferring retained fragment ownership into the completed work item
    // The faithful parser/read-operation object family is not reconstructed yet, so keep this
    // source-owned scaffold conservative and side-effect-free.
    (void)readOperationFragment;
    if (outWorkItem) {
        *outWorkItem = nullptr;
    }
    return 1u;
}

// UNANCHORED: source-owned mirror of the exact `0x449d8a` enqueue handoff.
void CLTTCPConnection::pushCompletedOperation(CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
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
        this,
        "CLTTCPConnection::OnReceive");
}

}  // namespace mxo::liblttcp
