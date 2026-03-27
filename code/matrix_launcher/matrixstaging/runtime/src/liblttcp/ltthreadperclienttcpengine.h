#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ilttcpengine.h"
#include "lttcpconnection.h"

namespace mxo::liblttcp {

class CMessageConnection;
class CLTThreadPerClientTCPEngine;

// Recovered base-queue object from CLTBaseThreadPerClientTCPEngine.
// Current high-confidence field map comes from 0x436610 -> 0x436340 and later consumer paths.
// This matches the launcher arg5 queue storage shape at:
// - +0x0c..+0x33
// - +0x34..+0x5b
struct CLTThreadPerClientTCPEngine_Queue {
    void* current0;        // +0x00
    void* block0;          // +0x04
    void* end0;            // +0x08
    void* slotsCurrent;    // +0x0c
    void* current1;        // +0x10
    void* block1;          // +0x14
    void* end1;            // +0x18
    void* slotsLast;       // +0x1c
    void* slotsBase;       // +0x20
    uint32_t slotCapacity; // +0x24
};

// Recovered queued pair shape from 0x436820 producer and 0x436d31..0x436ee7 consumer paths.
// Current best reading:
// - value0 = workItem pointer
// - value1 = context / owner pointer
struct CLTThreadPerClientTCPEngine_QueuedPair {
    uint32_t value0;
    uint32_t value1;
};

// Recovered queued work-item header shape.
// Current high-confidence anchor:
// - launcher helper 0x4816f0 returns [workItem+0x04]
// Current best read:
// - +0x00 = vtable
// - +0x04 = work-item type/code consumed by the queue consumer
struct CLTThreadPerClientTCPEngine_WorkItemHeader {
    void** vtable;
    uint32_t workType;
};

// Recovered generic thread-base surface shared by several launcher worker objects.
// Current original anchors:
// - base ctor: 0x4319e0
// - GetNameString: 0x4319d0
// - Start: 0x4528d0
// - Resume: 0x4525d0
// - Stop: 0x452660
// - IsRunning: 0x431a60
// - Wait: 0x4526e0
// - Suspend: 0x452620
// - IsCurrentThread: 0x431a40
// - exit log hook: 0x452770
// - deleting dtor: 0x431a80
// Scaffold note:
// - this source model now tracks the recovered object layout / naming / ownership surface
// - but it still does NOT reproduce original Win32 force-terminate semantics faithfully yet
class CLTThread {
public:
    static constexpr uint32_t kStartSuccess = 0;
    static constexpr uint32_t kStartAlreadyRunning = 0x8000005;
    static constexpr uint32_t kStartFailure = 1;

    // anchor: launcher.exe:0x4319e0
    explicit CLTThread(const char* threadName);
    // anchor: launcher.exe:0x452950 / 0x431a80 deleting wrapper
    virtual ~CLTThread();

    // anchor: launcher.exe:0x4319d0
    const std::string& GetNameString() const;

    // anchor: launcher.exe:0x4528d0
    virtual uint32_t Start(int startPriority);
    // anchor: launcher.exe:0x4525d0
    virtual bool Resume();
    // anchor: launcher.exe:0x452660
    virtual int Stop(bool waitAfterTerminate);
    // anchor: launcher.exe:0x431a60
    virtual bool IsRunning() const;
    // anchor: launcher.exe:0x4526e0
    virtual uint32_t Wait();
    // anchor: launcher.exe:0x452620
    virtual void Suspend();
    // anchor: launcher.exe:0x431a40
    virtual bool IsCurrentThread() const;

protected:
    // anchor: launcher.exe:0x437b50 on the current shared base vtable family
    virtual uint32_t PreRun();
    // UNANCHORED scaffold base default; concrete derived thread classes override the thread-main slot
    virtual void Run();
    // anchor: launcher.exe:0x452770
    virtual void LogExit();

    std::string threadName_;
    int startPriority_;
    int suspendDepth_;
    bool running_;
    uint32_t threadId_;
};

// Recovered queue-thread child allocated by engine base ctor helper 0x4365a0.
// Current high-confidence shape:
// - inherits generic CLTThread-style base state
// - stores owner engine pointer at +0x38
// - overrides the thread-main slot to call owner RunCompletedOperationQueue(owner, 0)
class CLTThreadPerClientTCPEngine_QueueThread : public CLTThread {
public:
    // anchor: launcher.exe:0x4365a0
    explicit CLTThreadPerClientTCPEngine_QueueThread(CLTThreadPerClientTCPEngine* owner);
    // UNANCHORED scaffold dtor; current vtable/dtor mapping still reuses the shared CLTThread deleting dtor
    ~CLTThreadPerClientTCPEngine_QueueThread() override;

    // UNANCHORED scaffold accessor for the recovered child +0x38 owner field
    CLTThreadPerClientTCPEngine* Owner() const;

protected:
    // anchor: launcher.exe:0x436fc0
    void Run() override;

private:
    CLTThreadPerClientTCPEngine* owner_;
};

// Reimplementation note:
// This file intentionally mirrors recovered original launcher.exe naming.
// Keep the names stable even where behavior is still placeholder-only.
// Canonical RE reference remains:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// Recovered source-file anchor:
// - `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

// original class identity:
// - launcher global 0x4d6304
// - derived ctor 0x431c30
// - base ctor 0x4366f0
// - queue-pair init helper 0x436610 -> queue init helper 0x436340
// - queue-thread child ctor 0x4365a0
// - primary vtable 0x4b2768
// strongest current class string:
// - CLTThreadPerClientTCPEngine
class CLTThreadPerClientTCPEngine : public ILTTCPEngine {
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

    // anchor: launcher.exe:0x431c30 / base ctor 0x4366f0
    CLTThreadPerClientTCPEngine();
    // anchor: launcher.exe:0x40b389..0x40b404 teardown release path; current C++ body remains scaffold-only
    ~CLTThreadPerClientTCPEngine();

    int Release(uint32_t flags) override;
    uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) override;
    uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext = nullptr) override;
    uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext = nullptr) override;
    uint32_t Slot4_42F7C0(void* arg1) override;
    uint32_t UnmonitorPort(uint16_t portHostOrder, uint32_t* outSocketHandle, uint32_t ipv4NetworkOrder) override;
    uint32_t Connect(void* contextKey) override;
    uint32_t Close(void* contextKey, bool graceful) override;
    uint32_t SendBuffer(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext = nullptr) override;
    uint32_t Slot9_42FD10(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) override;
    uint32_t Slot10_443810(void* arg1) override;
    uint32_t Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) override;
    uint32_t CleanupConnection(void* contextKey) override;

    // UNANCHORED source-side helpers used by the current connection scaffolding.
    uint32_t ConnectResolvedEndpointScaffold(
        uint16_t portHostOrder,
        uint32_t ipv4NetworkOrder,
        void* contextKey,
        void* ownerContext = nullptr);
    uint32_t ConnectConnectionScaffold(CLTTCPConnection* connection);
    uint32_t CloseConnectionScaffold(CLTTCPConnection* connection, bool graceful);
    uint32_t SendBufferConnectionScaffold(
        CLTTCPConnection* connection,
        const void* buffer,
        uint32_t byteCount,
        void* completionContext = nullptr);

    // Base queue helpers recovered from:
    // - 0x436340 = Queue_Init
    // - 0x436670 / 0x436820 = queue-pair push helpers
    // Current scaffold uses these helpers as the canonical queue-storage implementation even
    // while the live arg5 ABI object still owns the runtime-visible queue fields.
    static void Queue_Free(CLTThreadPerClientTCPEngine_Queue* queue);
    static bool Queue_Init(CLTThreadPerClientTCPEngine_Queue* queue, uint32_t initialSize);
    static bool Queue_PushPair(CLTThreadPerClientTCPEngine_Queue* queue, uint32_t value0, uint32_t value1);
    // anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
    static bool Queue_IsEmpty(const CLTThreadPerClientTCPEngine_Queue* queue);
    // anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
    static bool Queue_TryPopPair(CLTThreadPerClientTCPEngine_Queue* queue, CLTThreadPerClientTCPEngine_QueuedPair* outPair);

    // UNANCHORED scaffold bridge because the current liblttcp engine lives beside, not inside,
    // the launcher ABI object that still owns the runtime-visible +0x0c / +0x34 queue fields.
    void AttachExternalQueuePair(
        CLTThreadPerClientTCPEngine_Queue* queue0C,
        CLTThreadPerClientTCPEngine_Queue* queue34);

    // anchor: launcher.exe:0x436b10
    // Current shared consumer-family name recovered from the queue-thread child path.
    void RunCompletedOperationQueue(bool nonBlocking);

    // UNANCHORED scaffold helper used to mirror the recovered 0x4366f0 child-allocation shape in source.
    void RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount);
    // UNANCHORED scaffold accessor for source-side queue-thread child tracking.
    size_t QueueThreadCount() const;

    const std::vector<AcceptThreadRecord>& MonitoredPorts() const;
    const std::vector<WorkerThreadRecord>& WorkerThreads() const;

    CMessageConnection* FindMessageConnection(void* contextKey);
    CMessageConnection* GetOrCreateMessageConnection(void* contextKey);
    bool DropMessageConnection(void* contextKey);

private:
    static LTTCPEndpointKey MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder);
    AcceptThreadRecord* FindMonitoredPort(const LTTCPEndpointKey& key);
    WorkerThreadRecord* FindWorker(void* contextKey);

    CLTThreadPerClientTCPEngine_Queue* externalQueue0C_;
    CLTThreadPerClientTCPEngine_Queue* externalQueue34_;
    std::vector<std::unique_ptr<CLTThreadPerClientTCPEngine_QueueThread>> queueThreads_;
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
    // UNANCHORED starter binding helper.
    CLTThreadPerClientTCPEngineBinding();
    // UNANCHORED starter binding helper.
    ~CLTThreadPerClientTCPEngineBinding();

    // UNANCHORED starter binding helper.
    bool Bind(void* owner);
    // UNANCHORED starter binding helper.
    void Reset();

    // UNANCHORED starter binding helper.
    void* Owner() const;
    // UNANCHORED starter binding helper.
    CLTThreadPerClientTCPEngine* Engine() const;
    // UNANCHORED starter binding helper.
    bool HasEngine() const;
    // UNANCHORED starter binding helper.
    bool HasMonitoredPorts() const;
    // UNANCHORED starter binding helper.
    bool HasWorkerThreads() const;

private:
    void* owner_;
    std::unique_ptr<CLTThreadPerClientTCPEngine> engine_;
};

}  // namespace mxo::liblttcp
