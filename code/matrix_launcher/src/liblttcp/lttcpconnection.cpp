#include "lttcpconnection.h"

#include <winsock2.h>
#include <algorithm>

namespace mxo::liblttcp {

// This starter skeleton currently mirrors only the strongest state/behavior constraints
// recovered so far from launcher.exe string-backed engine paths.

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

CLTTCPConnection::~CLTTCPConnection() = default;

void CLTTCPConnection::SetOwnerContext(void* ownerContext) {
    ownerContext_ = ownerContext;
}

void* CLTTCPConnection::OwnerContext() const {
    return ownerContext_;
}

void CLTTCPConnection::SetSocketHandle(uint32_t socketHandle) {
    socketHandle_ = socketHandle;
}

uint32_t CLTTCPConnection::SocketHandle() const {
    return socketHandle_;
}

void CLTTCPConnection::SetState(LTTCPEngineConnectionState state) {
    state_ = state;
}

LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return state_;
}

void CLTTCPConnection::SetRemoteEndpoint(const LTTCPEndpointKey& endpoint) {
    remoteEndpoint_ = endpoint;
}

const LTTCPEndpointKey& CLTTCPConnection::RemoteEndpoint() const {
    return remoteEndpoint_;
}

void CLTTCPConnection::SetRemoteHostName(const char* hostName) {
    remoteHostName_ = hostName ? hostName : "";
}

const std::string& CLTTCPConnection::RemoteHostName() const {
    return remoteHostName_;
}

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

const std::vector<uint8_t>& CLTTCPConnection::ReceivedBytes() const {
    return receivedBytes_;
}

void CLTTCPConnection::ClearReceivedBytes() {
    receivedBytes_.clear();
}

uint32_t CLTTCPConnection::Close(bool /*graceful*/) {
    // Evidence-backed shape only:
    // - original engine treats states 1/2 as active
    // - Close writes state 4 before shutdown/closesocket cleanup
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

uint32_t CLTTCPConnection::OnReceive() {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    return 1;
}

}  // namespace mxo::liblttcp
