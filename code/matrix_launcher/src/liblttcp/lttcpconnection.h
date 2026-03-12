#pragma once

#include <cstdint>

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

// current best original identity:
// - base CLTTCPConnection family from launcher.exe string evidence
// - base vtable `0x4b8018` is now string-backed by nearby `CLTTCPConnection::OnReceive()` text
// - `CMessageConnection` currently looks like a derived message-layer object built on top of this base
// - used by engine slots Close / SendBuffer and by later worker/receive paths
class CLTTCPConnection {
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

private:
    void* ownerContext_;
    uint32_t socketHandle_;
    LTTCPEngineConnectionState state_;
    LTTCPEndpointKey remoteEndpoint_;
};

}  // namespace mxo::liblttcp
