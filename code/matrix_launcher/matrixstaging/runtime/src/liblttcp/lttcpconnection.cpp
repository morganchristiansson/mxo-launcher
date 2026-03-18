#include "lttcpconnection.h"

#include <winsock2.h>
#include <algorithm>

namespace mxo::liblttcp {

// VTable 0x004b8034 - CLTTCPConnection (Base Class)
// 0x004b8034 - Constructor at 0x0044ac40
// 0x004b8040 - IsConnected at 0x00449ca0
// 0x004b8048 - OnReceive at 0x00449d40
// 0x004b804c - OnClose at 0x00449fd0
// 0x004b8050 - Close at 0x00449cd0
// 0x004b8054 - Destructor at 0x00449d20

CLTTCPConnection::CLTTCPConnection()
    : ownerContext_(nullptr),
      socketHandle_(0xffffffffu),
      state_(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : ownerContext_(ownerContext),
      socketHandle_(0xffffffffu),
      state_(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

// VTable 0x004b8054 - Destructor at 0x00449d20
CLTTCPConnection::~CLTTCPConnection() = default;

// ============================================================
// FAITHFUL: VTable 0x004b8040 - IsConnected at 0x00449ca0
// ============================================================
bool CLTTCPConnection::IsConnected() const {
    return static_cast<uint32_t>(state_) != 8;
}

// ============================================================
// FAITHFUL: VTable 0x004b804c - OnClose at 0x00449fd0
// Helper/utility function for close operations (9 instructions)
// ============================================================
void CLTTCPConnection::OnClose() {
    // Placeholder - original is a helper/utility function
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for owner context management
// ============================================================
void CLTTCPConnection::SetOwnerContext(void* ownerContext) {
    ownerContext_ = ownerContext;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for owner context
// ============================================================
void* CLTTCPConnection::OwnerContext() const {
    return ownerContext_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for socket handle management
// ============================================================
void CLTTCPConnection::SetSocketHandle(uint32_t socketHandle) {
    socketHandle_ = socketHandle;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for socket handle
// ============================================================
uint32_t CLTTCPConnection::SocketHandle() const {
    return socketHandle_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for state management
// ============================================================
void CLTTCPConnection::SetState(LTTCPEngineConnectionState state) {
    state_ = state;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for connection state
// ============================================================
LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return state_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for remote endpoint management
// ============================================================
void CLTTCPConnection::SetRemoteEndpoint(const LTTCPEndpointKey& endpoint) {
    remoteEndpoint_ = endpoint;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for remote endpoint
// ============================================================
const LTTCPEndpointKey& CLTTCPConnection::RemoteEndpoint() const {
    return remoteEndpoint_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for remote hostname management
// ============================================================
void CLTTCPConnection::SetRemoteHostName(const char* hostName) {
    remoteHostName_ = hostName ? hostName : "";
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for remote hostname
// ============================================================
const std::string& CLTTCPConnection::RemoteHostName() const {
    return remoteHostName_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder receive polling entry point
// ============================================================
int CLTTCPConnection::PollReceiveNonBlocking() {
    if (socketHandle_ == 0xffffffffu ||
        (state_ != LTTCPEngineConnectionState::kConnectActive &&
         state_ != LTTCPEngineConnectionState::kUdpMonitorActive)) {
        return 0;
    }

    u_long available = 0;
    if (ioctlsocket(static_cast<SOCKET>(socketHandle_), FIONREAD, &available) != 0 || available == 0) {
        return 0;
    }

    const int toRead = static_cast<int>(std::min<u_long>(available, 4096));
    if (toRead <= 0) {
        return 0;
    }

    const size_t oldSize = receivedBytes_.size();
    receivedBytes_.resize(oldSize + static_cast<size_t>(toRead));
    const int received = recv(
        static_cast<SOCKET>(socketHandle_),
        reinterpret_cast<char*>(receivedBytes_.data() + oldSize),
        toRead,
        0);
    if (received <= 0) {
        receivedBytes_.resize(oldSize);
        return 0;
    }

    receivedBytes_.resize(oldSize + static_cast<size_t>(received));
    return received;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for received bytes
// ============================================================
const std::vector<uint8_t>& CLTTCPConnection::ReceivedBytes() const {
    return receivedBytes_;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Clear received bytes buffer
// ============================================================
void CLTTCPConnection::ClearReceivedBytes() {
    receivedBytes_.clear();
}

// ============================================================
// FAITHFUL: VTable 0x004b8050 - Close at 0x00449cd0
// Evidence-backed shape only:
// - original engine treats states 1/2 as active
// - Close writes state 4 before shutdown/closesocket cleanup
// ============================================================
uint32_t CLTTCPConnection::Close(bool /*graceful*/) {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    state_ = LTTCPEngineConnectionState::kClosing;
    if (socketHandle_ != 0xffffffffu) {
        closesocket(static_cast<SOCKET>(socketHandle_));
    }
    socketHandle_ = 0xffffffffu;
    return 1;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder send buffer entry point
// ============================================================
uint32_t CLTTCPConnection::SendBuffer(const void* buffer, uint32_t byteCount, void* /*completionContext*/) {
    if (!buffer || byteCount == 0) {
        return 0;
    }

    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    if (socketHandle_ == 0xffffffffu) {
        return 0;
    }

    const int sent = send(
        static_cast<SOCKET>(socketHandle_),
        static_cast<const char*>(buffer),
        static_cast<int>(byteCount),
        0);
    return (sent == static_cast<int>(byteCount)) ? 1u : 0u;
}

// ============================================================
// FAITHFUL: VTable 0x004b8048 - OnReceive at 0x00449d40
// Note: Original implementation is 225 instructions, 34 complexity, 25 calls
// This skeleton is a placeholder
// ============================================================
uint32_t CLTTCPConnection::OnReceive() {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    return 1;
}

}  // namespace mxo::liblttcp
