#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mxo::liblttcp {

// Reimplementation note:
// This is still only a starter original-name skeleton.
// Canonical RE reference remains:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md

// State values recovered from original CLTThreadPerClientTCPEngine paths.
// Only the meanings marked in comments are evidence-backed so far.
enum class LTTCPEngineConnectionState : uint32_t {
    kUnknown = 0,
    kConnectActive = 1,     // written by Connect success path
    kUdpMonitorActive = 2,  // written by UDPMonitorPort success path
    kClosing = 4,           // provisional: written by Close path
    kClosed = 8,            // required by MonitorPort / UDPMonitorPort / Connect prechecks
};

struct LTTCPEndpointKey {
    uint16_t family = 2;          // AF_INET
    uint16_t portNetworkOrder = 0;
    uint32_t ipv4NetworkOrder = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

// ============================================================
// VTable 0x004b8018 - CBaseConnection (Abstract Root)
// This is the abstract base class that serves as the ROOT of the connection hierarchy.
// Objects are created with this vtable but immediately transition to concrete subclass vtables.
//
// Memory Layout:
//   Offset 0x34: Connection state field (original binary layout)
// ============================================================
class CBaseConnection {
 public:
  // Virtual destructor at slot 0 - cleanup routine for abstract base (16 instructions)
  virtual ~CBaseConnection() = default;

  // Pure virtual methods at slots 4-6 define the contract for all derived classes
  // These are intentionally unimplemented in the abstract base
  virtual uint32_t OnReceive() = 0;           // slot 4 - pure virtual
  virtual uint32_t OnOperationCompleted(void*) = 0;  // slot 5 - pure virtual
  virtual uint32_t SendPacket(const void*, uint32_t, void*) = 0;  // slot 6 - pure virtual

  // Common virtual method at slot 3 - checks state field != kClosed
  // Original: static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this) + 0x34) & 0xff != 8
  virtual bool IsConnected() const;
  
  // Getter for state field
  // Original access: *(uint8_t*)((int)this + 0x34)
  LTTCPEngineConnectionState State() const {
    return static_cast<LTTCPEngineConnectionState>(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this) + 0x34) & 0xff);
  }

  // Constructor to initialize state at offset 0x34
  CBaseConnection(LTTCPEngineConnectionState initialState = LTTCPEngineConnectionState::kClosed);

 protected:
  // State field at offset 0x34 - matches original binary layout for faithful implementation
  LTTCPEngineConnectionState state_;
};

// ============================================================
// VTable 0x004b8034 - CLTTCPConnection (Base Class)
// Inherits from CBaseConnection and implements the abstract methods.
// ============================================================
class CLTTCPConnection : public CBaseConnection {
public:
    CLTTCPConnection();
    explicit CLTTCPConnection(void* ownerContext);
    ~CLTTCPConnection();

    void SetOwnerContext(void* ownerContext);
    void* OwnerContext() const;

    void SetSocketHandle(uint32_t socketHandle);
    uint32_t SocketHandle() const;

    void SetState(LTTCPEngineConnectionState state);
    LTTCPEngineConnectionState State() const;

    void SetRemoteEndpoint(const LTTCPEndpointKey& endpoint);
    const LTTCPEndpointKey& RemoteEndpoint() const;

    void SetRemoteHostName(const char* hostName);
    const std::string& RemoteHostName() const;

    int PollReceiveNonBlocking();
    const std::vector<uint8_t>& ReceivedBytes() const;
    void ClearReceivedBytes();

    // Placeholder reimplementation entry points.
    // These names follow original launcher/client strings, but behavior is still skeletal.
    //
    // original engine users now recovered around them:
    // - CLTThreadPerClientTCPEngine::Close gates on state 1/2 and then drives shutdown/closesocket cleanup
    // - CLTThreadPerClientTCPEngine::SendBuffer also gates on state 1/2
    // - client/launcher queue dispatch later passes work items back through connection/context callbacks
    uint32_t Close(bool graceful);
    uint32_t SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext);

    // Name kept intentionally generic for now.
    // We do not yet have a high-confidence direct original method name mapped onto the
    // connection-side receive processing entrypoint in this starter skeleton.
    uint32_t OnReceive();
    void OnClose();

    // ============================================================
    // FAITHFUL: Helper functions from OnReceive implementation at 0x00449d40
    // ============================================================

    std::uint32_t pollReceive();
    void pushCompletedOperation(void* thisPtr, int priority, void* connection, char opType);
    void cleanupConnection();

 private:
    void* ownerContext_;
    uint32_t socketHandle_;
    LTTCPEndpointKey remoteEndpoint_;
    std::string remoteHostName_;
    std::vector<uint8_t> receivedBytes_;
};

}  // namespace mxo::liblttcp
