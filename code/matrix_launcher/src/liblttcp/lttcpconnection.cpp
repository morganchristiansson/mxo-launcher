#include "lttcpconnection.h"

namespace mxo::liblttcp {

// This starter skeleton currently mirrors only the strongest state/behavior constraints
// recovered so far from launcher.exe string-backed engine paths.

CLTTCPConnection::CLTTCPConnection()
    : ownerContext_(nullptr),
      socketHandle_(0xffffffffu),
      state_(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_() {}

CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : ownerContext_(ownerContext),
      socketHandle_(0xffffffffu),
      state_(LTTCPEngineConnectionState::kClosed),
      remoteEndpoint_() {}

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

uint32_t CLTTCPConnection::Close(bool /*graceful*/) {
    // Evidence-backed shape only:
    // - original engine treats states 1/2 as active
    // - Close writes state 4 before shutdown/closesocket cleanup
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    state_ = LTTCPEngineConnectionState::kClosing;
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

    return 1;
}

uint32_t CLTTCPConnection::OnReceive() {
    if (state_ != LTTCPEngineConnectionState::kConnectActive &&
        state_ != LTTCPEngineConnectionState::kUdpMonitorActive) {
        return 0;
    }

    return 1;
}

}  // namespace mxo::liblttcp
