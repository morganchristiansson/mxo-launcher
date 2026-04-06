#pragma once

#include <winsock2.h>
#include <windows.h>

#include <cstddef>
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
// anchor family: launcher.exe:0x4816f0 / 0x434d00
// Current high-confidence anchors:
// - launcher helper `0x4816f0` returns `[workItem+0x04]`
// - launcher helper `0x434d00` returns `[workItem+0x08]`
// Current best read:
// - `+0x00` = vtable
// - `+0x04` = work-item type/code consumed by the queue consumer
// - `+0x08` = shared status/payload dword used by connection/completion consumers
struct CLTThreadPerClientTCPEngine_WorkItemHeader {
    void** vtable; // +0x00
    uint32_t workType; // +0x04
    uint32_t statusOrPayloadDword08; // +0x08
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_WorkItemHeader) == 0x0c, "work-item header size mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_WorkItemHeader, workType) == 0x04, "work-item header workType offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_WorkItemHeader, statusOrPayloadDword08) == 0x08, "work-item header status/payload offset mismatch");

// Recovered launcher-visible arg5 helper root at +0x5c.
// Current source note:
// - original shell still has to materialize this helper root in-place for client-visible `ecx`
//   identity
// - the target class now also owns a first-class surrogate for the helper family so wrapper code
//   can delegate helper semantics/mechanical mirror state into liblttcp instead of keeping all
//   arg5-only state bookkeeping in `src/launcher_network_object_abi.cpp`
struct CLTThreadPerClientTCPEngine_WaitHelperScaffold {
    void** vtable;
};

// Recovered launcher-visible lock-helper family at +0x60 / +0x98.
// Faithfulness update:
// - original embeds a `CRITICAL_SECTION` immediately after the vtable root
// - keep that inline shape on the native class too so the class body can converge on the original
//   `0xb4` arg5 layout instead of carrying heap-backed helper storage baggage
struct CLTThreadPerClientTCPEngine_LockHelperScaffold {
    void** vtable;
    CRITICAL_SECTION crit;
};

// Recovered allocated sentinel/tree head shape reached from arg5 +0x80.
struct CLTThreadPerClientTCPEngine_EndpointTreeHead24 {
    unsigned char colorOrFlag;
    unsigned char padding[3];
    void* root;
    void* first;
    void* last;
    unsigned char keyAndPayload[0x14];
};

// Recovered allocated sentinel/tree head shape reached from arg5 +0x8c.
struct CLTThreadPerClientTCPEngine_ContextTreeHead18 {
    unsigned char colorOrFlag;
    unsigned char padding[3];
    void* root;
    void* first;
    void* last;
    unsigned char keyAndPayload[0x8];
};

// Launcher-visible shell attachment map used by the current wrapper boundary.
// This intentionally keeps the raw arg5 shell outside liblttcp while letting the target class own
// more of the constructor/runtime-visible state that the shell mirrors.
struct CLTThreadPerClientTCPEngine_LauncherAbiAttachment {
    uint32_t* field04CtorFlags = nullptr;
    void** field08QueueThreadArray = nullptr;
    CLTThreadPerClientTCPEngine_Queue* queue0C = nullptr;
    CLTThreadPerClientTCPEngine_Queue* queue34 = nullptr;
    void* queueLock = nullptr;
    void* queueSignalEvent = nullptr;
    void* cleanupLock = nullptr;
    void** list80EndpointTreeHead = nullptr;
    uint32_t* field84EndpointCount = nullptr;
    void** list8CContextTreeHead = nullptr;
    uint32_t* field90ContextCount = nullptr;
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_WaitHelperScaffold) == 0x04, "wait helper size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_LockHelperScaffold) == 0x1c, "lock helper size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_EndpointTreeHead24) == 0x24, "endpoint head size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_ContextTreeHead18) == 0x18, "context head size mismatch");

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
//   - current best evidence: `0x431840` writes `[payload+0x38]` to the caller out-pointer on the
//     endpoint-removal path, so this still reads best as the owner/context pointer rather than the
//     listening socket handle
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
    // UNANCHORED: source-owned bridge for the original external closesocket([payload+0x3c]) seam.
    void CloseListenSocketScaffold();
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
// Keep the names stable even where behavior is still scaffold-first or only partially recovered.
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
    // Source note:
    // - original slot return conventions are not uniform
    // - e.g. `MonitorPort`, `UDPMonitorPort`, and `UnmonitorPort` use `0` on success in the
    //   current static read, while some source-owned helper paths still use `1` as the local
    //   success sentinel
    static constexpr uint32_t kResultSuccess = 1;
    static constexpr uint32_t kResultAlreadyMonitored = 0x7000003;
    static constexpr uint32_t kResultEndpointNotFound = 0x7000004;

    // Recovered launcher arg5 body offsets derived from the real class layout.
    static constexpr size_t OffsetField04();
    static constexpr size_t OffsetField08();
    static constexpr size_t OffsetQueue0C();
    static constexpr size_t OffsetQueue34();
    static constexpr size_t OffsetWaitHelper5C();
    static constexpr size_t OffsetQueueLockHelper60();
    static constexpr size_t OffsetQueueSignalEvent7C();
    static constexpr size_t OffsetEndpointTreeHead80();
    static constexpr size_t OffsetEndpointCount84();
    static constexpr size_t OffsetReserved88();
    static constexpr size_t OffsetContextTreeHead8C();
    static constexpr size_t OffsetContextCount90();
    static constexpr size_t OffsetReserved94();
    static constexpr size_t OffsetCleanupLockHelper98();

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

    // Current best static read of the `+0x80` / `+0x8c` tree families:
    // - endpoint node payload `[node+0x20]` is the direct `AcceptThread` object
    // - context node payload `[node+0x14]` is the direct `WorkerThread` object
    // - there is no extra launcher-side wrapper record around either payload family

    // anchor: launcher.exe:0x431c30 / base ctor 0x4366f0
    CLTThreadPerClientTCPEngine();
    // anchor: launcher.exe:0x40b389..0x40b404 teardown release path; current C++ body remains scaffold-only
    ~CLTThreadPerClientTCPEngine();

    // anchor: launcher.exe:0x4319a0
    int Release(uint32_t flags) override;
    // anchor: launcher.exe:0x431ce0
    uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) override;
    // anchor: launcher.exe:0x4325d0
    uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ipv4NetworkOrder = nullptr) override;
    // anchor: launcher.exe:0x436000
    uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ipv4NetworkOrder = nullptr) override;
    // anchor: launcher.exe:0x42f7c0
    uint32_t Slot4_42F7C0(void* arg1) override;
    // anchor: launcher.exe:0x431840
    uint32_t UnmonitorPort(uint16_t portHostOrder, void** outOwnerContext, uint32_t ipv4NetworkOrder) override;
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
        void* unusedArg3 = nullptr);
    // UNANCHORED: source-side connection-object bridge into ConnectResolvedEndpointScaffold.
    uint32_t ConnectConnectionScaffold(CLTTCPConnection* connection);
    // UNANCHORED: source-side connection-object bridge into the recovered Close slot family.
    uint32_t CloseConnectionScaffold(CLTTCPConnection* connection, bool graceful);
    // UNANCHORED: source-side connection-object bridge into the recovered SendBuffer slot family.
    // Active `0x448a00 -> vtable +0x20` callers currently pass ownership-mode `1` in the fourth
    // slot; keep the older `completionContext` name only as a source-compatibility parameter name
    // until wider send-family prototypes are fully retyped.
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

    // UNANCHORED: scaffold bridge because the current liblttcp engine still lives beside the raw
    // launcher ABI shell.
    //
    // Ownership note after the current arg5 structural pass:
    // - the target class now owns first-class source-level surrogates for the recovered ctor/
    //   helper/container state
    // - the wrapper still provides the raw embedded shell addresses consumed by original code
    // - this attachment map tells the class which live shell surfaces should override the owned
    //   fallback queue/event/lock state while also exposing which count/head fields on the shell
    //   should mirror class-owned state directly
    void AttachLauncherAbiSurfaceScaffold(
        const CLTThreadPerClientTCPEngine_LauncherAbiAttachment& attachment);
    // UNANCHORED: explicit detach/reset helper for the launcher ABI bridge.
    void DetachLauncherAbiSurfaceScaffold();
    // UNANCHORED: current auth/margin begin wrappers and launcher ABI helper thunks still call this
    // so the raw arg5 object shell can mirror class-owned state changes.
    void SyncAttachedLauncherObjectStateScaffold();

    // Helper-family bodies now source-own the recovered +0x5c/+0x60/+0x98 semantics on the
    // target class side; launcher ABI wrappers only route raw helper entrypoints here.
    // Faithfulness split after the current helper pass:
    // - these class methods now mirror only the original helper bodies themselves
    // - the extra launcher-bridge pump side effect remains on the arg5 `+0x60` shell slot-0
    //   wrapper, not in the pure enter-helper body
    // anchor: launcher.exe:0x435f90
    uint32_t SignalQueueEventHelper();
    // anchor: launcher.exe:0x435fa0
    uint32_t WaitQueueEventHelper(int reasonMilliseconds);
    // anchor family: launcher.exe:0x4147b0
    uint32_t EnterQueueLockHelper();
    // anchor family: launcher.exe:0x4147c0
    uint32_t LeaveQueueLockHelper();
    // anchor family: launcher.exe:0x4147b0
    uint32_t EnterCleanupLockHelper();
    // anchor family: launcher.exe:0x4147c0
    uint32_t LeaveCleanupLockHelper();

    // UNANCHORED: current replacement seam still keeps loginmediator-owned high-level auth/margin
    // handlers, but the arg5-side queue-context allocation/vtable, nonblocking producer/push path,
    // and helper-owned pump now live on the engine side instead of inside loginmediator.cpp or
    // launcher_network_object_abi.cpp.
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* EnsureLauncherConnectionContextScaffold(
        mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold** slot,
        mxo::ltlogin::CLTLoginMediator* mediator,
        const char* label,
        bool isMarginConnection);
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
    // Current bounded source mirror:
    // - nonBlocking=true still models the client-side poll form (`0x62531c10(1)`)
    // - nonBlocking=false now also waits on the attached arg5 queue-signal event when both queues
    //   are empty, matching the recovered `+0x5c` wait-helper role more closely than the older
    //   immediate-return scaffold
    void RunCompletedOperationQueue(bool nonBlocking, bool preferType1CallbackBeforeCleanup = false);

    // anchor family: launcher.exe:0x4366f0 / 0x436920
    // Current source helper owns the real `+0x04/+0x08` queue-thread array/count fields directly
    // and mirrors the constructor/stop-thread family without pretending to be one exact body.
    void RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount);
    // Recovered field-backed accessor over real object `+0x04`.
    size_t QueueThreadCount() const;

    // Source-owned connection resolver. Current faithful preference order is:
    // - queue-context owner
    // - launcher bridge sidecar resolved from the connection's direct mediator owner at `+0xa4`
    // - active worker/context-tree payloads keyed by the direct connection object
    // - no generic synthetic fallback allocation
    CMessageConnection* FindMessageConnection(void* contextKey);

private:
    // UNANCHORED: starter helper mirroring the recovered endpoint-key shape.
    static LTTCPEndpointKey MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder);
    // anchor: launcher.exe:0x42fdb0 search shape over the endpoint-keyed `+0x80` tree family.
    CLTThreadPerClientTCPEngine_AcceptThread* FindMonitoredPort(const LTTCPEndpointKey& key);
    // anchor: launcher.exe:0x42fe10 search shape over the context-keyed `+0x8c` tree family.
    CLTThreadPerClientTCPEngine_WorkerThread* FindWorker(void* contextKey);
    // Source-owned shared engine-slot connection resolver.
    // Faithfulness rule: this no longer synthesizes generic engine-owned `CMessageConnection`
    // objects when original caller/object evidence is missing.
    CMessageConnection* ResolveConnectionForEngineSlotScaffold(void* contextKey);
    // UNANCHORED: source-owned helper shaped after launcher.exe:0x431ff0 worker creation/insertion.
    CLTThreadPerClientTCPEngine_WorkerThread* CreateAndInsertWorkerThreadScaffold(
        CMessageConnection* connection,
        bool datagramMode,
        bool startThread = false);
    // UNANCHORED: source-owned teardown helper for the direct `AcceptThread` payload stored at
    // `[endpointNode+0x20]`.
    void StopAcceptThreadScaffold(CLTThreadPerClientTCPEngine_AcceptThread* acceptThread);
    // UNANCHORED: source-owned teardown helper for the direct `WorkerThread` payload stored at
    // `[contextNode+0x14]`.
    void StopWorkerThreadScaffold(CLTThreadPerClientTCPEngine_WorkerThread* workerThread);
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
    // Current tighter destination correction:
    // - once a sidecar connection exists, launcher-bridge work items queue through the
    //   connection-family queue context for type-1/type-2/synthetic paths alike
    // - that keeps the queued `context` closer to the original direct-connection identity while
    //   still landing callback dispatch on the nearer `CMessageConnection`/leaf callback path
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
    // UNANCHORED: launcher-visible mirror refresh for the recovered +0x80/+0x8c sentinel heads and
    // count dwords.
    void RefreshOwnedLauncherMirrorStateScaffold();
    CLTThreadPerClientTCPEngine_Queue* ActiveQueue0CScaffold();
    CLTThreadPerClientTCPEngine_Queue* ActiveQueue34Scaffold();
    const CLTThreadPerClientTCPEngine_Queue* ActiveQueue0CScaffold() const;
    const CLTThreadPerClientTCPEngine_Queue* ActiveQueue34Scaffold() const;
    void* ActiveQueueLockScaffold() const;
    void* ActiveQueueSignalEventScaffold() const;
    void* ActiveCleanupLockScaffold() const;
    static void InitializeLockHelperScaffold(CLTThreadPerClientTCPEngine_LockHelperScaffold* helper);
    static void DeleteLockHelperScaffold(CLTThreadPerClientTCPEngine_LockHelperScaffold* helper);
    static void InitializeEndpointTreeHead24(CLTThreadPerClientTCPEngine_EndpointTreeHead24* head);
    static void InitializeContextTreeHead18(CLTThreadPerClientTCPEngine_ContextTreeHead18* head);

    uint32_t ctorFlagsField04_;
    void* queueThreadArrayField08_;
    CLTThreadPerClientTCPEngine_Queue ownedQueue0C_;
    CLTThreadPerClientTCPEngine_Queue ownedQueue34_;
    CLTThreadPerClientTCPEngine_WaitHelperScaffold ownedWaitHelper5C_;
    CLTThreadPerClientTCPEngine_LockHelperScaffold ownedQueueLockHelper60_;
    HANDLE ownedQueueSignalEvent7C_;
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* ownedEndpointTreeHead80_;
    uint32_t ownedEndpointCount84_;
    uint32_t reserved88_;
    CLTThreadPerClientTCPEngine_ContextTreeHead18* ownedContextTreeHead8C_;
    uint32_t ownedContextCount90_;
    uint32_t reserved94_;
    CLTThreadPerClientTCPEngine_LockHelperScaffold ownedCleanupLockHelper98_;
};

struct CLTThreadPerClientTCPEngine_LayoutMirror {
    void** vtable;
    uint32_t field04;
    void* field08;
    CLTThreadPerClientTCPEngine_Queue queue0C;
    CLTThreadPerClientTCPEngine_Queue queue34;
    CLTThreadPerClientTCPEngine_WaitHelperScaffold waitHelper5C;
    CLTThreadPerClientTCPEngine_LockHelperScaffold queueLockHelper60;
    HANDLE queueSignalEvent7C;
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* endpointTreeHead80;
    uint32_t endpointCount84;
    uint32_t reserved88;
    CLTThreadPerClientTCPEngine_ContextTreeHead18* contextTreeHead8C;
    uint32_t contextCount90;
    uint32_t reserved94;
    CLTThreadPerClientTCPEngine_LockHelperScaffold cleanupLockHelper98;
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_LayoutMirror) == 0xb4, "layout mirror size mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, field04) == 0x04, "field04 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, field08) == 0x08, "field08 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, queue0C) == 0x0c, "queue0C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, queue34) == 0x34, "queue34 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, waitHelper5C) == 0x5c, "waitHelper5C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, queueLockHelper60) == 0x60, "queueLockHelper60 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, queueSignalEvent7C) == 0x7c, "queueSignalEvent7C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, endpointTreeHead80) == 0x80, "endpointTreeHead80 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, endpointCount84) == 0x84, "endpointCount84 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, reserved88) == 0x88, "reserved88 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, contextTreeHead8C) == 0x8c, "contextTreeHead8C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, contextCount90) == 0x90, "contextCount90 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, reserved94) == 0x94, "reserved94 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_LayoutMirror, cleanupLockHelper98) == 0x98, "cleanupLockHelper98 offset mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine) == sizeof(CLTThreadPerClientTCPEngine_LayoutMirror), "CLTThreadPerClientTCPEngine size must match launcher arg5");

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
    // Faithfulness note from the current Ghidra pass:
    // - launcher.exe shows the engine object itself as connection/queue/worker-owned state
    // - no positive evidence puts mediator bind/reset lifecycle inside the original engine class
    // So this binding now owns only owner<->engine pairing; outer launcher/login seams perform any
    // mediator-specific bridge reset/bind around it.
    bool Bind(void* owner);
    // UNANCHORED: starter binding helper.
    void Reset();

    // UNANCHORED: starter binding helper.
    void* Owner() const;
    // UNANCHORED: starter binding helper.
    CLTThreadPerClientTCPEngine* Engine() const;
    // UNANCHORED: starter binding helper.
    bool HasEngine() const;

private:
    void* owner_;
    std::unique_ptr<CLTThreadPerClientTCPEngine> engine_;
};

}  // namespace mxo::liblttcp
