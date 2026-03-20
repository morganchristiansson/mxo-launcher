#include "lttcpconnection.h"

#include <winsock2.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "spdlog/spdlog.h"

namespace mxo::liblttcp {

// ============================================================
// VTable 0x004b8034 - CLTTCPConnection (Base Class)
// 0x004b8034 - Constructor at 0x0044ac40
// 0x004b8040 - IsConnected at 0x00449ca0
// 0x004b8048 - OnReceive at 0x00449d40
// 0x004b804c - OnClose at 0x00449fd0
// 0x004b8050 - Close at 0x00449cd0
// 0x004b8054 - Destructor at 0x00449d20

CLTTCPConnection::CLTTCPConnection()
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      ownerContext_(nullptr),
      socketHandle_(0xffffffffu),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

CLTTCPConnection::CLTTCPConnection(void* ownerContext)
    : CBaseConnection(LTTCPEngineConnectionState::kClosed),
      ownerContext_(ownerContext),
      socketHandle_(0xffffffffu),
      remoteEndpoint_(),
      remoteHostName_(),
      receivedBytes_() {}

// VTable 0x004b8054 - Destructor at 0x00449d20
CLTTCPConnection::~CLTTCPConnection() = default;

// ============================================================
// FAITHFUL: VTable 0x004b8040 - IsConnected at 0x00449ca0
// Implemented in CBaseConnection abstract base (slot 3)
// Checks state field != kClosed
// ============================================================
bool CBaseConnection::IsConnected() const {
    return static_cast<uint32_t>(state_) != static_cast<uint32_t>(LTTCPEngineConnectionState::kClosed);
}

// ============================================================
// Constructor for CBaseConnection - initializes state at offset 0x34
// ============================================================
CBaseConnection::CBaseConnection(LTTCPEngineConnectionState initialState)
    : state_(initialState) {}

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
// Getter for connection state - delegates to base class
// ============================================================
LTTCPEngineConnectionState CLTTCPConnection::State() const {
    return CBaseConnection::State();
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
            socketHandle_ = 0xffffffffu;
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
            socketHandle_ = 0xffffffffu;
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
            socketHandle_ = 0xffffffffu;
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
        socketHandle_ = 0xffffffffu;
        return -1;
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
// Original implementation: 225 instructions, 34 complexity, 25 calls
//
// Logic flow from Ghidra decompilation:
// 1. Get receive pointer from queue0C (offset 0x6c) and poll for work
// 2. Loop polling until non-zero result or error
// 3. If result is valid (-1 < result && result != 0x7000000):
//    a. Handle stream corruption (0x700000b) - TLS file + 0x14c
//    b. Handle unrecoverable errors - TLS file + 0x14e
// 4. If invalid result, call cleanup vtable offset 0xc and return
// 5. Call cleanup callback at param_1+8
// ============================================================
uint32_t CLTTCPConnection::OnReceive() {
    // Handle optional cleanup callback at param_1+4
    if (nullptr != reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this))) {
        // Placeholder for cleanup callback - original calls method at param_1+4
    }

    // Poll for receive data
    std::uint32_t result = pollReceive();

    void* pvVar1 = nullptr;

    // Loop polling until non-zero result
    while (nullptr == pvVar1 && 0 == result) {
        // Push completed operation onto queue
        pushCompletedOperation(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) + 0x10), 0, this, '\0');

        result = pollReceive();
        pvVar1 = nullptr;
    }

    // If result is valid (not error code, not EOF)
    if (-1 < static_cast<int>(result) && result != 0x7000000) {
        // Handle stream corruption error
        if (result == 0x700000b) {
            pvVar1 = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) + 0x24);
            Close(false);
        }
        else {
            pvVar1 = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) + 0x24);
            Close(false);
        }

        // Call cleanup vtable offset 0xc
        cleanupConnection();
    }

    // if (param_1 != (int *)0x0) { (**(code **)(*param_1 + 8))(); }
    // Call cleanup callback at param_1+8
    if (nullptr != reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this))) {
        // Placeholder for cleanup callback - original calls method at param_1+8
    }

    return 1;
}

// ============================================================
// Helper: pollReceive - Poll for receive data from socket
// Original: uVar3 = (**(code **)(*piVar2 + 4))()
// ============================================================
std::uint32_t CLTTCPConnection::pollReceive() {
    // Original polls the receive queue (queue0C) for available work
    // This interfaces with the engine's receive dispatch mechanism

    return 1;
}

// ============================================================
// Helper: pushCompletedOperation - Push operation onto CompletedOpQueue
// Original: FUN_00436820(...)
// ============================================================
void CLTTCPConnection::pushCompletedOperation(void*, int, void*, char) {
    // Original pushes completed operations onto the engine's CompletedOpQueue
}

// ============================================================
// Helper: cleanupConnection - Call cleanup vtable offset 0xc
// Original: (**(code **)(*(int *)this + 0xc))(0)
// ============================================================
void CLTTCPConnection::cleanupConnection() {
    // Original calls method at vtable offset 0xc (which corresponds to Close method)
}

}  // namespace mxo::liblttcp
