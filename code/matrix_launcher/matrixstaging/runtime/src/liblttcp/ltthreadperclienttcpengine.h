#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "lttcpconnection.h"

namespace mxo::liblttcp {

class CMessageConnection;

// Reimplementation note:
// This file intentionally mirrors recovered original launcher.exe naming.
// Keep the names stable even where behavior is still placeholder-only.
// Canonical RE reference remains:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// Recovered source-file anchor:
// - `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

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

    struct AcceptThreadRecord {
        LTTCPEndpointKey endpoint;
        void* ownerContext = nullptr;
        uint32_t listenSocketHandle = 0xffffffffu;
        bool shouldRun = true;
    };

    struct WorkerThreadRecord {
        void* contextKey = nullptr;
        void* ownerContext = nullptr;
        uint32_t socketHandle = 0xffffffffu;
        LTTCPEngineConnectionState state = LTTCPEngineConnectionState::kClosed;
    };

    CLTThreadPerClientTCPEngine();
    ~CLTThreadPerClientTCPEngine();

    uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext);
    uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext = nullptr);
    uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext = nullptr);
    uint32_t Connect(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, void* contextKey, void* ownerContext = nullptr);
    uint32_t Connect(CLTTCPConnection* connection);
    uint32_t ConnectContext(void* contextKey);
    uint32_t Close(CLTTCPConnection* connection, bool graceful);
    uint32_t CloseContext(void* contextKey, bool graceful);
    uint32_t SendBuffer(CLTTCPConnection* connection, const void* buffer, uint32_t byteCount, void* completionContext = nullptr);
    uint32_t SendPacketContext(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext = nullptr);
    uint32_t CleanupConnection(void* contextKey);
    uint32_t UnmonitorPort(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, uint32_t* outSocketHandle);

    const std::vector<AcceptThreadRecord>& MonitoredPorts() const;
    const std::vector<WorkerThreadRecord>& WorkerThreads() const;

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

// Starter binding object used by the launcher scaffold while arg5 still enters through
// the dedicated launcher-network ABI layer in src/launcher_network_object_abi.cpp.
// This keeps owner/engine binding state out of src/diagnostics.cpp and on the
// recovered liblttcp side.
class CLTThreadPerClientTCPEngineBinding {
public:
    CLTThreadPerClientTCPEngineBinding();
    ~CLTThreadPerClientTCPEngineBinding();

    bool Bind(void* owner);
    void Reset();

    void* Owner() const;
    CLTThreadPerClientTCPEngine* Engine() const;
    bool HasEngine() const;
    bool HasMonitoredPorts() const;
    bool HasWorkerThreads() const;

private:
    void* owner_;
    std::unique_ptr<CLTThreadPerClientTCPEngine> engine_;
};

}  // namespace mxo::liblttcp
