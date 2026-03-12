#pragma once

#include <cstdint>
#include <vector>

#include "lttcpconnection.h"

namespace mxo::liblttcp {

class CMessageConnection;

// Reimplementation note:
// This file intentionally mirrors recovered original launcher.exe naming.
// Keep the names stable even where behavior is still placeholder-only.
// Canonical RE reference remains:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md

// original class identity:
// - launcher global 0x4d6304
// - ctor 0x431c30
// - primary vtable 0x4b2768
// strongest current class string:
// - CLTThreadPerClientTCPEngine
class CLTThreadPerClientTCPEngine {
public:
    static constexpr uint32_t kResultSuccess = 1;
    static constexpr uint32_t kResultAlreadyMonitored = 0x7000003;
    static constexpr uint32_t kResultEndpointNotFound = 0x7000004;

    // Current placeholder payload model for arg5 +0x80.
    // Original path now strongly suggests these are AcceptThread-style worker objects
    // created by MonitorPort / listen-side setup.
    struct AcceptThreadRecord {
        LTTCPEndpointKey endpoint;
        void* ownerContext = nullptr;
        uint32_t listenSocketHandle = 0xffffffffu;
        bool shouldRun = true;
    };

    // Current placeholder payload model for arg5 +0x8c.
    // Original path now strongly suggests these are WorkerThread-style worker objects
    // created by UDPMonitorPort / Connect helper paths and later consumed by
    // CleanupConnection through the pointer-keyed container.
    struct WorkerThreadRecord {
        // Current best read: this key is often a CLTTCPConnection/CMessageConnection-family
        // object pointer on the launcher paths currently recovered.
        // Helper `0x431ff0` now also strongly suggests the original +0x8c tree is keyed by
        // that raw connection/context pointer while storing a WorkerThread-style payload value.
        void* contextKey = nullptr;
        void* ownerContext = nullptr;
        uint32_t socketHandle = 0xffffffffu;
        LTTCPEngineConnectionState state = LTTCPEngineConnectionState::kClosed;
    };

    CLTThreadPerClientTCPEngine();
    ~CLTThreadPerClientTCPEngine();

    // Original-name placeholders started from current RE evidence.
    // Keep these signatures intentionally minimal until the active scaffold is
    // ready to route real behavior through them.
    //
    // original slot 1 / launcher.exe:0x431ce0
    // string-backed name: CLTThreadPerClientTCPEngine::MonitorPort
    // proven shape: socket(AF_INET, SOCK_STREAM, 0) -> bind -> listen -> +0x80 population
    // launcher.exe:0x431ce0
    // original name recovered from strings: MonitorPort
    // current best read:
    // - creates TCP listen socket
    // - bind + listen
    // - populates endpoint-keyed +0x80 with AcceptThread-style payload
    uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext);

    // original slot 2 / launcher.exe:0x4325d0
    // string-backed name: CLTThreadPerClientTCPEngine::UDPMonitorPort
    // proven shape: socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) -> setsockopt(SO_REUSEADDR) -> bind
    // then create WorkerThread payload in +0x8c with state 2
    // launcher.exe:0x4325d0
    // original name recovered from strings: UDPMonitorPort
    // current best read:
    // - creates UDP socket
    // - sets SO_REUSEADDR
    // - bind
    // - creates WorkerThread-style payload in +0x8c
    // - marks [worker+0x34] = 2 on success
    // - important newer wrapper-level narrowing: the recovered original path is likely
    //   driven by a connection/context object carrying state/endpoint fields, not only by
    //   raw primitive `(port, key, owner)` arguments
    uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext = nullptr);

    // original slot 3 / launcher.exe:0x436000
    // provisional helper name only: current static behavior calls UDPMonitorPort(port=0, ...)
    // then queries getsockname/ntohs to report the bound local port back to the caller.
    // This is not yet wired into the active scaffold.
    // launcher.exe:0x436000
    // direct string-backed name not yet recovered.
    // current best read:
    // - thin helper around UDPMonitorPort(port=0, ...)
    // - then getsockname / ntohs to report chosen local port
    uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext = nullptr);

    // original slot 6 / launcher.exe:0x4328a0
    // string-backed name: CLTThreadPerClientTCPEngine::Connect
    // proven shape: socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) -> bind -> connect
    // then create WorkerThread payload in +0x8c with state 1
    // launcher.exe:0x4328a0
    // original name recovered from strings: Connect
    // current best read:
    // - creates TCP socket
    // - local bind + connect
    // - creates WorkerThread-style payload in +0x8c
    // - marks [worker+0x34] = 1 on success
    // - newer `CMessageConnection` wrapper analysis now strongly suggests the live original
    //   entry is connection-object-based (`engine->Connect(self)`) after endpoint updates at
    //   connection `+0x24`
    uint32_t Connect(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, void* contextKey, void* ownerContext = nullptr);
    uint32_t Connect(CLTTCPConnection* connection);

    // original slot 7 / launcher.exe:0x42f970
    // string-backed name: CLTThreadPerClientTCPEngine::Close
    // launcher.exe:0x42f970
    // original name recovered from strings: Close
    // current best read:
    // - only active for connection states 1 / 2
    // - performs shutdown / closesocket style cleanup
    uint32_t Close(CLTTCPConnection* connection, bool graceful);

    // original slot 8 / launcher.exe:0x42fbd0
    // string-backed name: CLTThreadPerClientTCPEngine::SendBuffer
    // launcher.exe:0x42fbd0
    // original name recovered from strings: SendBuffer
    // current best read:
    // - only active for connection states 1 / 2
    // - failure string explicitly says not connected/connecting otherwise
    uint32_t SendBuffer(CLTTCPConnection* connection, const void* buffer, uint32_t byteCount, void* completionContext = nullptr);

    // original slot 12 / launcher.exe:0x4316a0
    // string-backed name: CLTThreadPerClientTCPEngine::CleanupConnection
    // proven consumer-side role: non-empty queue0C dispatch continuation
    // launcher.exe:0x4316a0
    // original name recovered from strings: CleanupConnection
    // current best read:
    // - non-empty queue0C consumer milestone
    // - looks up contextKey in pointer-keyed +0x8c
    // - consumes/removes payload there before later callback chain continues
    uint32_t CleanupConnection(void* contextKey);

    // original slot 5 / launcher.exe:0x431840
    // direct name not yet recovered; current best read is the endpoint-keyed
    // unmonitor / teardown / handle-extraction counterpart to MonitorPort.
    // launcher.exe:0x431840
    // direct original public name not yet recovered.
    // current best read:
    // - endpoint-keyed unmonitor / teardown / handle-extraction path
    // - miss returns 0x7000004
    // - likely the stop-monitoring counterpart to MonitorPort
    uint32_t UnmonitorPort(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, uint32_t* outSocketHandle);

    const std::vector<AcceptThreadRecord>& MonitoredPorts() const;
    const std::vector<WorkerThreadRecord>& WorkerThreads() const;

    // Starter ownership helpers used to keep connection-oriented sidecar logic out of
    // diagnostics.cpp while the raw arg5 ABI trampolines remain there.
    CMessageConnection* FindMessageConnection(void* contextKey);
    CMessageConnection* GetOrCreateMessageConnection(void* contextKey);
    bool DropMessageConnection(void* contextKey);

private:
    static LTTCPEndpointKey MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder);
    AcceptThreadRecord* FindMonitoredPort(const LTTCPEndpointKey& key);
    WorkerThreadRecord* FindWorker(void* contextKey);

    std::vector<AcceptThreadRecord> monitoredPorts_;
    std::vector<WorkerThreadRecord> workerThreads_;
    std::vector<CMessageConnection*> messageConnections_;
    uint32_t nextSyntheticSocketHandle_;
};

}  // namespace mxo::liblttcp
