#pragma once

#include <winsock2.h>
#include <windows.h>

#include <bits/stl_tree.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "ilttcpengine.h"
#include "lttcpconnection.h"

namespace mxo::liblttcp {

class CMessageConnection_0x4b7928;
class CLTThreadPerClientTCPEngine_0x4b2768;
class CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread;
class CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread;
struct LTTCPEndpointKey_0x44b070;

using CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode =
    std::_Rb_tree_node<std::pair<LTTCPEndpointKey_0x44b070, CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread*>>;
using CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode =
    std::_Rb_tree_node<std::pair<uint32_t, CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread*>>;

// Recovered repeated queue-record layout used inside the base completed-operation queue-pair
// storage.
// Evidence chain:
// - launcher.exe:0x436610 zeros two consecutive 0x28 records and calls 0x436340 on each
// - launcher.exe:0x436670 selects either the first record or the pair-relative +0x28 record
// - launcher.exe:0x4364d0 treats base +0x0c as one queue-pair object and prefers the second
//   embedded record before the first
// Modeling note:
// - this is a recovered repeating record shape, not a proven original source-level class boundary
struct CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord {
    void* readCursor00;        // +0x00
    void* firstBlockBegin04;   // +0x04
    void* firstBlockEnd08;     // +0x08
    void* slotArrayCurrent0C;  // +0x0c
    void* writeCursor10;       // +0x10
    void* lastBlockBegin14;    // +0x14
    void* lastBlockEnd18;      // +0x18
    void* slotArrayLast1C;     // +0x1c
    void* slotArrayBase20;     // +0x20
    uint32_t slotCapacity24;   // +0x24
};

// Recovered queue-pair class/type boundary identified by OOAnalyzer and ctor/init helper
// launcher.exe:0x436610.
// This object is embedded inline at base +0x0c and spans engine/base +0x0c..+0x5b, so
// engine-relative +0x34 is the second embedded queue record (`queue28`), not a separate top-level
// sibling field.
struct CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610 {
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord queue00; // pair +0x00 / engine +0x0c
    CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord queue28; // pair +0x28 / engine +0x34
};

// Recovered queued pair shape from 0x436820 producer and 0x436d31..0x436ee7 consumer paths.
// Current best reading:
// - value0 = workItem pointer
// - value1 = context / owner pointer
struct CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair {
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
struct CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader {
    void** vtable; // +0x00
    uint32_t workType; // +0x04
    uint32_t statusOrPayloadDword08; // +0x08
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader) == 0x0c, "work-item header size mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader, workType) == 0x04, "work-item header workType offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader, statusOrPayloadDword08) == 0x08, "work-item header status/payload offset mismatch");

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord) == 0x28, "queue record size mismatch");
static_assert(sizeof(CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610) == 0x50, "queue pair storage size mismatch");
static_assert(offsetof(CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610, queue28) == 0x28, "queue pair second-record offset mismatch");

struct CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold {
    // anchor: launcher.exe:0x435050 / vtable `0x004b3df8`
    // Small queued type-2 work item used by the worker-thread connect-completion path.
    CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader header{};
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ConnectionStatusWorkItemScaffold) == 0x0c, "connection-status work-item size mismatch");

struct CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold {
    // anchor: launcher.exe:0x435070 / vtable `0x004b3e00`
    // Small queued type-1 work item used by the worker-thread close/peer-closed path.
    CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader header{};
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_CloseWorkItemScaffold) == 0x0c, "close work-item size mismatch");

// Recovered launcher-visible arg5 helper root at +0x5c.
// Current source note:
// - original shell still has to materialize this helper root in-place for client-visible `ecx`
//   identity
// - the target class now also owns a first-class surrogate for the helper family so wrapper code
//   can delegate helper semantics/mechanical mirror state into liblttcp instead of keeping all
//   arg5-only state bookkeeping in `src/launcher_network_object_abi.cpp`
struct CLTThreadPerClientTCPEngine_0x4b2768_WaitHelperScaffold {
    void** vtable;
};

// Recovered launcher-visible lock-helper family at +0x60 / +0x98.
// Faithfulness update:
// - original embeds a `CRITICAL_SECTION` immediately after the vtable root
// - keep that inline shape on the native class too so the class body can converge on the original
//   `0xb4` arg5 layout instead of carrying heap-backed helper storage baggage
struct CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold {
    void** vtable;
    CRITICAL_SECTION crit;
};

// Recovered allocated sentinel/tree head shape reached from arg5 +0x80.
struct CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24 {
    unsigned char colorOrFlag;
    unsigned char padding[3];
    void* root;
    void* first;
    void* last;
    unsigned char keyAndPayload[0x14];
};

// Recovered allocated sentinel/tree head shape reached from arg5 +0x8c.
struct CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18 {
    unsigned char colorOrFlag;
    unsigned char padding[3];
    void* root;
    void* first;
    void* last;
    unsigned char keyAndPayload[0x8];
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_WaitHelperScaffold) == 0x04, "wait helper size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold) == 0x1c, "lock helper size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24) == 0x24, "endpoint head size mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18) == 0x18, "context head size mismatch");

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
    // anchor: launcher.exe:0x452800
    uint32_t ExecuteThreadMainScaffold();
    // anchor: launcher.exe:0x452800 / `_beginthreadex` start thunk
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
class CLTThreadPerClientTCPEngine_0x4b2768_QueueThread : public CLTThread {
public:
    // anchor: launcher.exe:0x4365a0
    explicit CLTThreadPerClientTCPEngine_0x4b2768_QueueThread(CLTThreadPerClientTCPEngine_0x4b2768* owner);
    // UNANCHORED: current vtable family keeps the shared CLTThread deleting dtor at slot +0x2c
    ~CLTThreadPerClientTCPEngine_0x4b2768_QueueThread() override;

    // UNANCHORED: scaffold accessor for the recovered child +0x38 owner field
    CLTThreadPerClientTCPEngine_0x4b2768* Owner() const;

protected:
    // anchor: launcher.exe:0x436fc0
    void Run() override;

private:
    CLTThreadPerClientTCPEngine_0x4b2768* owner_;
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
class CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread : public CLTThread {
public:
    // anchor: launcher.exe:0x431ab0
    CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread(uint32_t listenSocketHandle, void* ownerContext);
    // anchor: launcher.exe:0x431b30 deleting wrapper / +0x40 wakeup helper teardown
    ~CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread() override;

    // UNANCHORED: scaffold accessor for recovered child +0x38 owner/context field
    void* OwnerContext() const;
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
class CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread : public CLTThread {
public:
    // anchor: launcher.exe:0x431b60
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread(void* contextKey, bool datagramMode);
    // anchor: launcher.exe:0x431be0 deleting wrapper / +0x40 wakeup helper teardown
    ~CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread() override;

    // UNANCHORED: scaffold accessor for recovered child +0x38 context/connection key field
    void* ContextKey() const;
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
// - CLTThreadPerClientTCPEngine_0x4b2768
class CLTThreadPerClientTCPEngine_0x4b2768 : public ILTTCPEngine {
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
    static constexpr size_t OffsetQueuePair0C();
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
    // arg5 helper fallback:
    // - `1` = original close / cleanup family that reaches arg5 slot 12 before context callback
    // - `2` = original connect/status family
    // - `3` = original parsed-packet work item emitted by `CLTTCPConnection::OnReceive`
    // - `0x10000003` = source-owned receive-drain proxy only
    //   - this is intentionally *not* another original type-3 work item
    //   - it exists because source currently stages the copied parsed packet inside
    //     `CMessageConnection_0x4b7928::OnOperationCompleted`, but does not yet reimplement the later
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
    CLTThreadPerClientTCPEngine_0x4b2768();
    // anchor: launcher.exe:0x40b389..0x40b404 teardown release path; current C++ body remains scaffold-only
    ~CLTThreadPerClientTCPEngine_0x4b2768();

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
    // Original slot argument order is `(buffer, byteCount, connection, ownershipMode)`.
    uint32_t SendBuffer(const void* buffer, uint32_t byteCount, void* contextKey, void* completionContext = nullptr) override;
    // anchor: launcher.exe:0x42fd10
    // Recovered slot-9 argument order is `(buffer, byteCount, remoteEndpoint, connection,
    // ownershipMode)`; this is the explicit-endpoint sibling of slot `8` / `0x42fbd0`.
    uint32_t SendBufferWithEndpoint(
        void* buffer,
        uint32_t byteCount,
        LTTCPEndpointKey_0x44b070* remoteEndpoint,
        void* contextKey,
        void* ownershipMode) override;
    // anchor: launcher.exe:0x443810
    uint32_t Slot10_443810(void* arg1) override;
    // anchor: launcher.exe:0x431670
    uint32_t Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) override;
    // anchor: launcher.exe:0x4316a0
    uint32_t CleanupConnection(void* contextKey) override;


    // anchor: launcher.exe:0x436340
    static void Queue_Free(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord);
    // anchor: launcher.exe:0x436340
    static bool Queue_Init(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord, uint32_t initialSize);
    // anchor: launcher.exe:0x436670 selected-queue push body reached from `0x436820`
    // Original helper returns `void`; once the caller reaches this push path it does not receive
    // enqueue-success feedback.
    // `queueRecord` is one of the two repeated records inside the inline queue-pair storage.
    static void Queue_PushPair(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord, uint32_t value0, uint32_t value1);
    // anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
    static bool Queue_IsEmpty(const CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord);
    // anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
    static bool Queue_TryPopPair(CLTThreadPerClientTCPEngine_0x4b2768_QueueRecord* queueRecord, CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair* outPair);


    // Helper-family bodies now source-own the recovered +0x5c/+0x60/+0x98 semantics on the
    // target class side; launcher ABI wrappers only route raw helper entrypoints here.
    // Faithfulness split after the current helper pass:
    // - these class methods now mirror only the original helper bodies themselves
    // - the extra launcher-side no-worker pump side effect remains on the arg5 `+0x60` shell slot-0
    //   wrapper, not in the pure enter-helper body
    // anchor: launcher.exe:0x435f90
    uint32_t SignalQueueEventHelper();
    // anchor: launcher.exe:0x435fa0
    uint32_t WaitQueueEventHelper(int reasonMilliseconds);
    // Shared helper-body evidence: launcher.exe:0x4147b0 / 0x4147c0.
    // These are source-side lock-entry/exit wrappers over the recovered helper storage, not a
    // claim that all four methods inline to one original body.
    uint32_t EnterQueueLockHelper();
    uint32_t LeaveQueueLockHelper();
    uint32_t EnterCleanupLockHelper();
    uint32_t LeaveCleanupLockHelper();

    // UNANCHORED: connection-owned bridge for the recovered `0x449d8a -> 0x436820` handoff.
    // Current best original read for this specific receive path:
    // - argument order after engine `this` is `(workItem, connection, useQueue34)`
    // - `CLTTCPConnection::OnReceive` always reaches it as `(completedPacketWorkItem, self, false)`
    //   i.e. queue0C on the currently understood receive path
    // - ownership of `workItem` transfers to the queue/consumer here; original `0x436820` returns
    //   `void`, so caller-side lifetime does not depend on enqueue success/failure
    void EnqueueCompletedOperationFromConnectionScaffold(
        CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
        CLTTCPConnection* connection,
        const char* label = nullptr);

    // anchor: launcher.exe:0x436b10
    // Current bounded source mirror:
    // - nonBlocking=true still models the client-side poll form (`0x62531c10(1)`)
    // - nonBlocking=false now also waits on the attached arg5 queue-signal event when both queues
    //   are empty, matching the recovered `+0x5c` wait-helper role more closely than the older
    //   immediate-return scaffold
    void RunCompletedOperationQueue(bool nonBlocking);

    // anchor: launcher.exe:0x436920
    // Drains pending queue work when no queue threads exist; otherwise enqueues the shutdown
    // sentinel, waits for each queue thread to terminate, releases the array, and clears +0x04/+0x08.
    void StopQueueThreads();
    // Source-owned extraction of the queue-thread allocation/start tail embedded in ctor 0x4366f0.
    // Kept separate so we do not pretend ctor-time startup and stop-thread teardown are one body.
    void CreateQueueThreadsForCtorCount(uint32_t queueThreadCount);

    friend class CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread;

private:
    // UNANCHORED: starter helper mirroring the recovered endpoint-key shape.
    static LTTCPEndpointKey_0x44b070 MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder);
    // anchor: launcher.exe:0x42fdb0 search shape over the endpoint-keyed `+0x80` tree family.
    // Faithfulness note: unlike the older source helper, the original returns the matching tree
    // node or the tree-head sentinel, not the `AcceptThread*` payload.
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeNode* EndpointTree_Find(const LTTCPEndpointKey_0x44b070& key);
    // anchor: launcher.exe:0x42fe10 search shape over the context-keyed `+0x8c` tree family.
    // Faithfulness note: like the endpoint-tree finder, the original returns the matching tree
    // node or the tree-head sentinel, not the `WorkerThread*` payload.
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeNode* ContextTree_Find(uint32_t key);
    // anchor: launcher.exe:0x431ff0 worker creation/insertion helper.
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* CreateAndInsertWorkerThread(
        CMessageConnection_0x4b7928* connection,
        bool datagramMode,
        bool startThread = false);
    // UNANCHORED: source-owned teardown helper for the direct `AcceptThread` payload stored at
    // `[endpointNode+0x20]`.
    void StopAcceptThreadScaffold(CLTThreadPerClientTCPEngine_0x4b2768_AcceptThread* acceptThread);
    // UNANCHORED: source-owned teardown helper for the direct `WorkerThread` payload stored at
    // `[contextNode+0x14]`.
    // anchor: launcher.exe:0x4364d0
    // Pop one completed-operation pair from the inline queue-pair storage, preferring the second
    // repeated record (pair +0x28 / engine +0x34) before the first record (pair +0x00 / engine
    // +0x0c). Returns launcher-style result codes and writes a null pair on empty/non-threaded
    // early-return cases.
    uint32_t TryPopCompletedOperation(
        CLTThreadPerClientTCPEngine_0x4b2768_QueuedPair* outPair,
        bool waitForSignal);
    // UNANCHORED: current sidecar enqueue helper preserving original `0x436820` lock/order shape.
    // Unlike older source scaffolds, this keeps the original `void`-return shape once the caller
    // has already committed to queue ownership transfer.
    void EnqueueCompletedOperationScaffold(
        void* workItem,
        void* context,
        bool useQueue34,
        const char* label);
    static void InitializeLockHelperScaffold(CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold* helper);
    static void DeleteLockHelperScaffold(CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold* helper);
    static void InitializeEndpointTreeHead24(CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* head);
    static void InitializeContextTreeHead18(CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* head);

    uint32_t ctorFlagsField04_;
    void* queueThreadArrayField08_;
    CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610 ownedQueuePair0C_;
    CLTThreadPerClientTCPEngine_0x4b2768_WaitHelperScaffold ownedWaitHelper5C_;
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold ownedQueueLockHelper60_;
    HANDLE ownedQueueSignalEvent7C_;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* ownedEndpointTreeHead80_;
    uint32_t ownedEndpointCount84_;
    uint32_t reserved88_;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* ownedContextTreeHead8C_;
    uint32_t ownedContextCount90_;
    uint32_t reserved94_;
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold ownedCleanupLockHelper98_;
};

struct CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror {
    void** vtable;
    uint32_t field04;
    void* field08;
    CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610 queuePair0C;
    CLTThreadPerClientTCPEngine_0x4b2768_WaitHelperScaffold waitHelper5C;
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold queueLockHelper60;
    HANDLE queueSignalEvent7C;
    CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24* endpointTreeHead80;
    uint32_t endpointCount84;
    uint32_t reserved88;
    CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18* contextTreeHead8C;
    uint32_t contextCount90;
    uint32_t reserved94;
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold cleanupLockHelper98;
};

static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror) == 0xb4, "layout mirror size mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, field04) == 0x04, "field04 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, field08) == 0x08, "field08 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queuePair0C) == 0x0c, "queuePair0C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queuePair0C.queue28) == 0x34, "queuePair0C.queue28 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, waitHelper5C) == 0x5c, "waitHelper5C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queueLockHelper60) == 0x60, "queueLockHelper60 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queueSignalEvent7C) == 0x7c, "queueSignalEvent7C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, endpointTreeHead80) == 0x80, "endpointTreeHead80 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, endpointCount84) == 0x84, "endpointCount84 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, reserved88) == 0x88, "reserved88 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, contextTreeHead8C) == 0x8c, "contextTreeHead8C offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, contextCount90) == 0x90, "contextCount90 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, reserved94) == 0x94, "reserved94 offset mismatch");
static_assert(offsetof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, cleanupLockHelper98) == 0x98, "cleanupLockHelper98 offset mismatch");
static_assert(sizeof(CLTThreadPerClientTCPEngine_0x4b2768) == sizeof(CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror), "CLTThreadPerClientTCPEngine_0x4b2768 size must match launcher arg5");

// Starter binding object used by the launcher scaffold while arg5 still enters through
// the dedicated launcher-network ABI layer in src/launcher_network_object_abi.cpp.
// This keeps owner/engine binding state out of src/diagnostics.cpp and on the
// recovered liblttcp side.
class CLTThreadPerClientTCPEngine_0x4b2768Binding {
public:
    // UNANCHORED: starter binding helper.
    CLTThreadPerClientTCPEngine_0x4b2768Binding();
    // UNANCHORED: starter binding helper.
    ~CLTThreadPerClientTCPEngine_0x4b2768Binding();

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
    CLTThreadPerClientTCPEngine_0x4b2768* Engine() const;

private:
    void* owner_;
    std::unique_ptr<CLTThreadPerClientTCPEngine_0x4b2768> engine_;
};

}  // namespace mxo::liblttcp
