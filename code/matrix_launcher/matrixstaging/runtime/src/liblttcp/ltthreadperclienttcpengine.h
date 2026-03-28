#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ilttcpengine.h"
#include "lttcpconnection.h"

namespace mxo::ltlogin {
class CLTLoginMediator;
struct CLTLoginMediatorConnectionContextScaffold;
}

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
    // UNANCHORED: source-owned wrapper mirroring `_StartAddress_00452800` thread-entry sequencing.
    uint32_t ExecuteThreadMainScaffold();
    // UNANCHORED: source-owned `_beginthreadex` entry thunk for ExecuteThreadMainScaffold.
    static unsigned __stdcall ThreadStartAddressScaffold(void* parameter);

    std::string threadName_;
    int startPriority_;
    int suspendDepth_;
    bool running_;
    uint32_t threadId_;
    uintptr_t threadHandle_;
    mutable std::mutex stateMutex_;
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
    // UNANCHORED: current vtable family keeps the shared CLTThread deleting dtor at slot +0x2c
    ~CLTThreadPerClientTCPEngine_QueueThread() override;

    // UNANCHORED: scaffold accessor for the recovered child +0x38 owner field
    CLTThreadPerClientTCPEngine* Owner() const;

protected:
    // anchor: launcher.exe:0x436fc0
    void Run() override;

private:
    CLTThreadPerClientTCPEngine* owner_;
};

// Recovered accept-thread child stored as the +0x80 endpoint-tree payload.
// Current high-confidence shape from 0x431ab0 / 0x431b30:
// - inherits generic CLTThread-style base state
// - stores owner/context-like state at +0x38
// - stores the listening socket handle at +0x3c
// - owns a wakeup socket helper at +0x40
class CLTThreadPerClientTCPEngine_AcceptThread : public CLTThread {
public:
    // anchor: launcher.exe:0x431ab0
    CLTThreadPerClientTCPEngine_AcceptThread(uint32_t listenSocketHandle, void* ownerContext);
    // anchor: launcher.exe:0x431b30 deleting wrapper / +0x40 wakeup helper teardown
    ~CLTThreadPerClientTCPEngine_AcceptThread() override;

    // UNANCHORED: scaffold accessor for recovered child +0x38 owner/context field
    void* OwnerContext() const;
    // UNANCHORED: scaffold accessor for recovered child +0x3c listening socket field
    uint32_t ListenSocketHandle() const;
    // UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
    uint32_t WakeupSocketHandle() const;
    // anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
    void SignalWakeup();

protected:
    // anchor: launcher.exe:0x432070
    void Run() override;

private:
    void* ownerContext_;
    uint32_t listenSocketHandle_;
    uint32_t wakeupSocketHandle_;
};

// Recovered worker-thread child stored as the +0x8c context-tree payload.
// Current high-confidence shape from 0x431b60 / 0x431be0:
// - inherits generic CLTThread-style base state
// - stores the context/connection key at +0x38
// - stores a mode byte at +0x3c
// - owns a wakeup socket helper at +0x40
// - stores an exit-request flag at +0x44
class CLTThreadPerClientTCPEngine_WorkerThread : public CLTThread {
public:
    // anchor: launcher.exe:0x431b60
    CLTThreadPerClientTCPEngine_WorkerThread(void* contextKey, bool datagramMode);
    // anchor: launcher.exe:0x431be0 deleting wrapper / +0x40 wakeup helper teardown
    ~CLTThreadPerClientTCPEngine_WorkerThread() override;

    // UNANCHORED: scaffold accessor for recovered child +0x38 context/connection key field
    void* ContextKey() const;
    // UNANCHORED: scaffold accessor for recovered child +0x3c datagram-mode byte
    bool DatagramMode() const;
    // UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
    uint32_t WakeupSocketHandle() const;
    // UNANCHORED: scaffold accessor for recovered child +0x44 exit-request byte
    bool ExitRequested() const;
    // UNANCHORED: source-owned bridge for the recovered child +0x44 exit-request byte
    void RequestExit();
    // anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
    void SignalWakeup();

protected:
    // anchor: launcher.exe:0x42fe50
    void Run() override;

private:
    void* contextKey_;
    bool datagramMode_;
    uint32_t wakeupSocketHandle_;
    bool exitRequested_;
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

    // Current best queue work-type split from `0x4490c0`, `0x436d31..0x436ee7`, and the current
    // launcher bridge:
    // - `1` = original close / cleanup family that reaches arg5 slot 12 before context callback
    // - `2` = original connect/status family
    // - `3` = original parsed-packet work item emitted by `CLTTCPConnection::OnReceive`
    // - `0x10000003` = source-owned receive-drain proxy only
    //   - this is intentionally *not* another original type-3 work item
    //   - it exists because source currently stages the copied parsed packet inside
    //     `CMessageConnection::OnOperationCompleted`, but does not yet reimplement the later
    //     original message-object / dispatch / owner-callback tail reached from that same callback
    static constexpr uint32_t kWorkTypeClose = 1u;
    static constexpr uint32_t kWorkTypeConnectionStatus = 2u;
    static constexpr uint32_t kWorkTypeParsedPacket = 3u;
    static constexpr uint32_t kWorkTypeSyntheticReceiveDrain = 0x10000003u;

    struct AcceptThreadRecord {
        LTTCPEndpointKey endpoint;
        void* ownerContext = nullptr;
        uint32_t listenSocketHandle = 0xffffffffu;
        std::unique_ptr<CLTThreadPerClientTCPEngine_AcceptThread> thread;
    };

    struct WorkerThreadRecord {
        void* contextKey = nullptr;
        void* ownerContext = nullptr;
        uint32_t socketHandle = 0xffffffffu;
        LTTCPEngineConnectionState state = LTTCPEngineConnectionState::kClosed;
        std::unique_ptr<CLTThreadPerClientTCPEngine_WorkerThread> thread;
    };

    // anchor: launcher.exe:0x431c30 / base ctor 0x4366f0
    CLTThreadPerClientTCPEngine();
    // anchor: launcher.exe:0x40b389..0x40b404 teardown release path; current C++ body remains scaffold-only
    ~CLTThreadPerClientTCPEngine();

    // anchor: launcher.exe:0x4319a0
    int Release(uint32_t flags) override;
    // anchor: launcher.exe:0x431ce0
    uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) override;
    // anchor: launcher.exe:0x4325d0
    uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext = nullptr) override;
    // anchor: launcher.exe:0x436000
    uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext = nullptr) override;
    // anchor: launcher.exe:0x42f7c0
    uint32_t Slot4_42F7C0(void* arg1) override;
    // anchor: launcher.exe:0x431840
    uint32_t UnmonitorPort(uint16_t portHostOrder, uint32_t* outSocketHandle, uint32_t ipv4NetworkOrder) override;
    // anchor: launcher.exe:0x4328a0
    uint32_t Connect(void* contextKey) override;
    // anchor: launcher.exe:0x42f970
    uint32_t Close(void* contextKey, bool graceful) override;
    // anchor: launcher.exe:0x42fbd0
    uint32_t SendBuffer(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext = nullptr) override;
    // anchor: launcher.exe:0x42fd10
    uint32_t Slot9_42FD10(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) override;
    // anchor: launcher.exe:0x443810
    uint32_t Slot10_443810(void* arg1) override;
    // anchor: launcher.exe:0x431670
    uint32_t Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) override;
    // anchor: launcher.exe:0x4316a0
    uint32_t CleanupConnection(void* contextKey) override;

    // UNANCHORED: source-side helper used by the current connection scaffolding.
    uint32_t ConnectResolvedEndpointScaffold(
        uint16_t portHostOrder,
        uint32_t ipv4NetworkOrder,
        void* contextKey,
        void* ownerContext = nullptr);
    // UNANCHORED: source-side connection-object bridge into ConnectResolvedEndpointScaffold.
    uint32_t ConnectConnectionScaffold(CLTTCPConnection* connection);
    // UNANCHORED: source-side connection-object bridge into the recovered Close slot family.
    uint32_t CloseConnectionScaffold(CLTTCPConnection* connection, bool graceful);
    // UNANCHORED: source-side connection-object bridge into the recovered SendBuffer slot family.
    uint32_t SendBufferConnectionScaffold(
        CLTTCPConnection* connection,
        const void* buffer,
        uint32_t byteCount,
        void* completionContext = nullptr);

    // anchor: launcher.exe:0x436340
    static void Queue_Free(CLTThreadPerClientTCPEngine_Queue* queue);
    // anchor: launcher.exe:0x436340
    static bool Queue_Init(CLTThreadPerClientTCPEngine_Queue* queue, uint32_t initialSize);
    // anchor: launcher.exe:0x436670 selected-queue push body reached from `0x436820`
    // Original helper returns `void`; once the caller reaches this push path it does not receive
    // enqueue-success feedback.
    static void Queue_PushPair(CLTThreadPerClientTCPEngine_Queue* queue, uint32_t value0, uint32_t value1);
    // anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
    static bool Queue_IsEmpty(const CLTThreadPerClientTCPEngine_Queue* queue);
    // anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
    static bool Queue_TryPopPair(CLTThreadPerClientTCPEngine_Queue* queue, CLTThreadPerClientTCPEngine_QueuedPair* outPair);

    // UNANCHORED: scaffold bridge because the current liblttcp engine lives beside, not inside,
    // the launcher ABI object that still owns the runtime-visible +0x0c / +0x34 queue fields,
    // the paired +0x60 lock helper, and the +0x7c queue signal event.
    void AttachExternalQueuePair(
        CLTThreadPerClientTCPEngine_Queue* queue0C,
        CLTThreadPerClientTCPEngine_Queue* queue34,
        void* queueLock = nullptr,
        void* queueSignalEvent = nullptr);
    // UNANCHORED: current sidecar still needs an ABI-shell callback to refresh owner-visible
    // arg5 list-head/queue attachment state after engine-side changes reached through connection
    // wrappers instead of direct arg5 primary-vtable calls.
    void SetAttachedLauncherObjectStateSyncScaffold(
        void* owner,
        void (*syncFn)(void*) = nullptr);
    // UNANCHORED: current auth/margin begin wrappers still call this after engine-side connect work
    // so the raw arg5 object shell can mirror sidecar-owned state changes.
    void SyncAttachedLauncherObjectStateScaffold();

    // UNANCHORED: current replacement seam still keeps loginmediator-owned high-level auth/margin
    // handlers, but the arg5-side queue-context allocation/vtable, nonblocking producer/push path,
    // and helper-owned pump now live on the engine side instead of inside loginmediator.cpp or
    // launcher_network_object_abi.cpp.
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* EnsureLauncherConnectionContextScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold** slot,
        mxo::ltlogin::CLTLoginMediator* mediator,
        const char* label,
        bool isMarginConnection);
    void AttachLauncherConnectionBridgeContextsScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* authContext,
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* marginContext);
    bool EnqueueLauncherConnectionStatusWorkItemScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
        uint32_t workType,
        uint32_t workPayload,
        const char* label);
    // UNANCHORED: connection-owned bridge for the recovered `0x449d8a -> 0x436820` handoff.
    // Current best original read for this specific receive path:
    // - argument order after engine `this` is `(workItem, connection, useQueue34)`
    // - `CLTTCPConnection::OnReceive` always reaches it as `(completedPacketWorkItem, self, false)`
    //   i.e. queue0C on the currently understood receive path
    // - ownership of `workItem` transfers to the queue/consumer here; original `0x436820` returns
    //   `void`, so caller-side lifetime does not depend on enqueue success/failure
    void EnqueueCompletedOperationFromConnectionScaffold(
        CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
        CLTTCPConnection* connection,
        const char* label = nullptr);
    void PumpLauncherConnectionBridgeFromArg5HelperScaffold();

    // anchor: launcher.exe:0x436b10
    void RunCompletedOperationQueue(bool nonBlocking);

    // UNANCHORED: scaffold helper used to mirror the recovered 0x4366f0 child-allocation shape in source.
    void RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount);
    // UNANCHORED: scaffold accessor for source-side queue-thread child tracking.
    size_t QueueThreadCount() const;

    // UNANCHORED: starter accessor exposing source-owned monitored-port state.
    const std::vector<AcceptThreadRecord>& MonitoredPorts() const;
    // UNANCHORED: starter accessor exposing source-owned worker-thread state.
    const std::vector<WorkerThreadRecord>& WorkerThreads() const;

    // UNANCHORED: starter helper keeping connection lookup out of diagnostics.cpp.
    CMessageConnection* FindMessageConnection(void* contextKey);
    // UNANCHORED: starter helper keeping connection creation out of diagnostics.cpp.
    CMessageConnection* GetOrCreateMessageConnection(void* contextKey);
    // UNANCHORED: starter helper keeping connection destruction out of diagnostics.cpp.
    bool DropMessageConnection(void* contextKey);

private:
    // UNANCHORED: starter helper mirroring the recovered endpoint-key shape.
    static LTTCPEndpointKey MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder);
    // UNANCHORED: starter helper for the endpoint-keyed +0x80 accept-thread container.
    AcceptThreadRecord* FindMonitoredPort(const LTTCPEndpointKey& key);
    // UNANCHORED: starter helper for the context-keyed +0x8c worker-thread container.
    WorkerThreadRecord* FindWorker(void* contextKey);
    // UNANCHORED: source-owned helper shaped after launcher.exe:0x431ff0 worker creation/insertion.
    WorkerThreadRecord* CreateOrReplaceWorkerThreadScaffold(
        void* contextKey,
        void* ownerContext,
        uint32_t socketHandle,
        LTTCPEngineConnectionState state,
        bool datagramMode);
    // UNANCHORED: source-owned teardown helper for recovered AcceptThread-style payloads.
    void StopAcceptThreadScaffold(AcceptThreadRecord* record);
    // UNANCHORED: source-owned teardown helper for recovered WorkerThread-style payloads.
    void StopWorkerThreadScaffold(WorkerThreadRecord* record);
    // UNANCHORED: current sidecar enqueue helper preserving original `0x436820` lock/order shape
    // while still returning `false` to synthetic callers when no attached target queue exists.
    // Unlike original `0x436820`, this source-owned helper exposes queue availability to the
    // synthetic launcher-bridge allocation path; once a target queue exists, it follows the
    // original enter-lock -> empty-snapshot -> push -> leave-lock -> signal ordering and does not
    // surface a push-success result.
    bool EnqueueCompletedOperationScaffold(
        void* workItem,
        void* context,
        bool useQueue34,
        const char* label,
        bool queueLockAlreadyHeld);
    // UNANCHORED: engine-owned launcher-bridge work-item builder used by both direct enqueue sites
    // and the arg5 helper-owned nonblocking pump.
    // Current work-type split:
    // - original connection-status items still use type `2`
    // - source-owned receive-drain proxies use `kWorkTypeSyntheticReceiveDrain`
    //   because original type `3` is already consumed by the parsed-packet queue items emitted from
    //   `CLTTCPConnection::OnReceive`
    // Current bounded destination correction:
    // - synthetic receive-drain items now prefer the connection-family queue context when one is
    //   available, so queue dispatch lands on `CMessageConnection::OnOperationCompleted` before any
    //   later mediator-owned drain handling
    bool EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
        uint32_t workType,
        uint32_t workPayload,
        const char* label,
        bool queueLockAlreadyHeld);
    // UNANCHORED: narrow per-context nonblocking receive pump used by the current arg5 helper seam.
    // Current source-owned pacing split:
    // - `CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold()` stays the narrow
    //   one-fragment recv->fragment->OnReceive seam
    // - this bridge pump may re-enter that helper repeatedly within one arg5 helper poll so the
    //   current source-owned `AuthReceivePacket` / `MarginReceivePacket` receive-drain proxies can
    //   stay aligned once per successful recv fragment instead of once per whole pump
    // - those proxies are intentionally not queued as original type `3`; original parsed-packet
    //   type `3` work is already in queue0C before this helper enqueues the later drain proxy
    // - when the sidecar connection is present, that later drain proxy now targets the
    //   connection-family queue callback path first rather than the mediator bridge context
    void PumpLauncherConnectionContextScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
        const char* receiveLabel);

    CLTThreadPerClientTCPEngine_Queue* externalQueue0C_;
    CLTThreadPerClientTCPEngine_Queue* externalQueue34_;
    void* externalQueueLock_;
    void* externalQueueSignalEvent_;
    void* attachedLauncherObjectOwnerScaffold_;
    void (*attachedLauncherObjectStateSyncScaffold_)(void*);
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* authBridgeContextScaffold_;
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* marginBridgeContextScaffold_;
    std::vector<std::unique_ptr<CLTThreadPerClientTCPEngine_QueueThread>> queueThreads_;
    std::vector<AcceptThreadRecord> monitoredPorts_;
    std::vector<WorkerThreadRecord> workerThreads_;
    std::vector<CMessageConnection*> messageConnections_;
};

// Starter binding object used by the launcher scaffold while arg5 still enters through
// the dedicated launcher-network ABI layer in src/launcher_network_object_abi.cpp.
// This keeps owner/engine binding state out of src/diagnostics.cpp and on the
// recovered liblttcp side.
class CLTThreadPerClientTCPEngineBinding {
public:
    // UNANCHORED: starter binding helper.
    CLTThreadPerClientTCPEngineBinding();
    // UNANCHORED: starter binding helper.
    ~CLTThreadPerClientTCPEngineBinding();

    // UNANCHORED: starter binding helper.
    bool Bind(void* owner, mxo::ltlogin::CLTLoginMediator* mediator = nullptr);
    // UNANCHORED: starter binding helper.
    void Reset(mxo::ltlogin::CLTLoginMediator* mediator = nullptr);

    // UNANCHORED: starter binding helper.
    void* Owner() const;
    // UNANCHORED: starter binding helper.
    CLTThreadPerClientTCPEngine* Engine() const;
    // UNANCHORED: starter binding helper.
    bool HasEngine() const;
    // UNANCHORED: starter binding helper.
    bool HasMonitoredPorts() const;
    // UNANCHORED: starter binding helper.
    bool HasWorkerThreads() const;

private:
    void* owner_;
    std::unique_ptr<CLTThreadPerClientTCPEngine> engine_;
};

}  // namespace mxo::liblttcp
