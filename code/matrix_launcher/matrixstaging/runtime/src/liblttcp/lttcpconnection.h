#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace mxo::liblttcp {

class CLTThreadPerClientTCPEngine;
class CLTThreadPerClientTCPEngine_WorkerThread;

// Reimplementation note:
// This is still a starter original-name skeleton.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/launcher.exe/VTABLES/0x004b8034.md

// State values recovered from original CLTThreadPerClientTCPEngine paths.
// Only the meanings marked in comments are evidence-backed so far.
enum class LTTCPEngineConnectionState : uint32_t {
    kUnknown = 0,
    kConnectActive = 1,     // written by Connect success path before worker-thread completion
    kUdpMonitorActive = 2,  // written by UDPMonitorPort success path and by TCP worker connect-complete success
    kClosing = 4,           // written by low-level close path before deferred shutdown / wakeup-only cleanup
    kClosed = 8,            // required by connection-wrapper and engine prechecks
};

class LTTCPEndpointKey {
public:
    // anchor: launcher.exe:0x44b070
    // Default endpoint-key constructor used by the connection / parser / worker families.
    // Original body zeros the full 16-byte block first, then writes `family = AF_INET`.
    LTTCPEndpointKey();

    // anchor: launcher.exe:0x44b020
    // Original helper compares the 16-byte key as four dwords (`repe cmpsd`) and returns true
    // when any dword differs.
    bool DiffersFrom(const LTTCPEndpointKey& other) const;

    // anchor: launcher.exe:0x44aff0
    // Original helper copies the 16-byte key as four dwords.
    void CopyTo(LTTCPEndpointKey* outEndpointKey) const;

    uint16_t family = 0; // AF_INET after ctor
    uint16_t portNetworkOrder = 0;
    uint32_t ipv4NetworkOrder = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

static_assert(sizeof(LTTCPEndpointKey) == 0x10, "endpoint key size mismatch");

// Recovered parser input fragment family consumed by connection `+0x6c`
// (`CVariableLengthPrefixedTCPStreamParser::Parse`).
// Current best family name comes from `CMessageConnection::OnOperationCompleted` logging
// ("Unused buffers were attached to CLTTCPReadOperation ...") plus the worker-thread receive path.
// This is a refcounted read-buffer fragment object, not a completed-packet work item.
// High-confidence fields / methods from `0x42fe50`, `0x42f820`, `0x42f850`, `0x42f860`,
// `0x42f880`, `0x42f890`, `0x452350`, `0x469bf0`, `0x435e60`, `0x4350c0`, and `0x435510`:
// - worker-thread receive path installs live leaf vtable `0x004b2300` and allocates `0x100c`
//   bytes for this family
// - deleting dtor `0x42fd50` collapses that live leaf back to shared low-level refcounted base
//   `0x004b211c` before final free
// - base `0x004b211c` owns the shared low-level refcount contract at `+0x00..+0x07`
// - derived `CLTTCPReadOperation` adds `+0x08 = byte count`
// - `+0x0c` = first payload byte in the variable-length inline tail allocation
// Important Ghidra/OOAnalyzer caution from the current RE pass:
// - tiny helpers `0x42f7e0`, `0x42f800`, and `0x42f810` are shared field-`+0x04` bodies reused by
//   the unrelated abstract helper cluster rooted at vtable `0x004c0540`
// - so the stray OOAnalyzer namespace around those helpers is not evidence that `0x004c0540`
//   itself is the read-operation base
// Source lockstep note:
// - this family is now modeled here as a real C++ class hierarchy instead of a manual struct plus
//   hand-built vtable record
// - the current pass also removes source-only payload/refcount wrapper helpers so parser and
//   consumer code operate on the recovered class layout directly
class CRefCountedReadOperationBase {
public:
    // anchor: launcher.exe:0x42f820 / vtable 0x004b211c +0x00
    virtual CRefCountedReadOperationBase* DeletingDtor(uint8_t deleteFlag);
    // anchor: launcher.exe:0x42f7e0 / vtable 0x004b211c +0x04
    virtual void AddRef();
    // anchor: launcher.exe:0x42f7f0 / vtable 0x004b211c +0x08
    virtual void Release();
    // anchor: launcher.exe:0x004199b0 / vtable 0x004b211c +0x0c
    virtual void DeleteIfNonNull();
    // anchor: launcher.exe:0x42f800 / vtable 0x004b211c +0x10
    virtual void ResetRefCount();
    // anchor: launcher.exe:0x42f810 / vtable 0x004b211c +0x14
    virtual void SetRefCountFromPtr(const long* value);

    long referenceCount04; // +0x04 plain dword in the non-interlocked base contract

    explicit CRefCountedReadOperationBase(long initialReferenceCount = 0);
};

static_assert(sizeof(CRefCountedReadOperationBase) == 0x08, "refcounted read-operation base size mismatch");

class CLTTCPReadOperation final : public CRefCountedReadOperationBase {
public:
    static constexpr uint32_t kPayloadCapacity = 0x1000u;

    // anchor: launcher.exe:0x42fd50 / vtable 0x004b2300 +0x00
    CRefCountedReadOperationBase* DeletingDtor(uint8_t deleteFlag) override;
    // anchor: launcher.exe:0x42f850 / vtable 0x004b2300 +0x04
    void AddRef() override;
    // anchor: launcher.exe:0x42f860 / vtable 0x004b2300 +0x08
    void Release() override;
    // anchor: launcher.exe:0x42f880 / vtable 0x004b2300 +0x10
    void ResetRefCount() override;
    // anchor: launcher.exe:0x42f890 / vtable 0x004b2300 +0x14
    void SetRefCountFromPtr(const long* value) override;

    // Source-owned ctor that mirrors the worker receive-path field initialization for the concrete
    // `CLTTCPReadOperation` leaf.
    CLTTCPReadOperation();

    // anchor: launcher.exe:0x452350
    void SetByteCount(uint32_t byteCount);

    uint32_t byteCount08; // +0x08
};

static_assert(sizeof(CLTTCPReadOperation) == 0x0c, "read-operation fragment prefix size mismatch");

struct CLTTCPReadOperationRefHandle {
    // anchor: launcher.exe:0x434fa0
    // Tiny retained-fragment handle helper used by parser state (`0x469bf0` / `0x4725c0`) and by
    // `CMessageConnection::OnOperationCompleted` stack locals.
    CLTTCPReadOperation* retainedFragment00 = nullptr; // +0x00
};

static_assert(sizeof(CLTTCPReadOperationRefHandle) == 0x04, "fragment ref handle size mismatch");

// Recovered `0x2c` parsed-packet work-item family built via
// `CVariableLengthPrefixedTCPStreamParser_AllocatePacketBuffer -> CParsedPacketWorkItem_ctor`.
// Current best parser-family read from `0x469bf0`, `0x4725c0`, `0x435e60`, `0x4355c0`,
// `0x4350c0`, `0x435510`, `0x435f30`, and `0x4490c0`:
// - vtable = `0x4b3e08`
// - strong inheritance signal from shared 12-byte work-item root `0x004b2134`
//   - ctor writes the shared `+0x04/+0x08` prefix first
//   - dtor retables back to `0x004b2134` before final free path
// - `+0x04` = work type `3`
// - `+0x08` = shared status/payload dword; parser-produced packets zero it
// - utility child/helper objects directly touched on this path now also include:
//   - `0x434fa0` = retained-fragment ref helper over a single `CLTTCPReadOperation*`
//   - shared work-item root prefix `CLTThreadPerClientTCPEngine_WorkItemHeader`
// - this same object family is used in two phases:
//   - parser-owned assembly state while stream bytes are being accumulated
//   - emitted completed packet object after `Parse(...)` returns `0`
// - after emit, `CVariableLengthPrefixedTCPStreamParser_ResetAfterPacket` allocates a fresh
//   replacement object and, when unread bytes remain in the tail fragment, carries forward that
//   tail fragment plus the new `currentCursor24`
struct CParsedPacketWorkItem_RetainedFragmentNodeScaffold {
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* next;   // +0x00
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* prev;   // +0x04
    CLTTCPReadOperation* retainedFragment08;    // +0x08 retained fragment reference
};

struct CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold {
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel; // +0x00 sentinel-headed list root
};

static_assert(sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold) == 0x0c, "parsed packet retained-fragment node size mismatch");
static_assert(sizeof(CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold) == 0x04, "parsed packet retained-fragment list owner size mismatch");

struct CLTTCPConnection_ParsedPacketWorkItemScaffold {
    void** vtable; // +0x00
    uint32_t workType; // +0x04 = 3
    uint32_t statusOrPayloadDword08; // +0x08 shared work-item-root status/payload dword; zero on parser-produced packets
    uint32_t retainedFragmentCount0C; // +0x0c retained fragment count (`AppendFragment` / `GetTailFragment`)
    CLTTCPReadOperation* firstRetainedFragment10; // +0x10 first retained fragment
    CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold* retainedFragmentListOwner14; // +0x14 optional wrapper for additional retained fragments beyond `+0x10`
    uint8_t directFragmentTraversalPhase18; // +0x18 traversal flag used by `BeginFragmentTraversal` / `GetNextFragment`
    uint8_t unknown19_1b[3]; // +0x19..+0x1b
    uint32_t fragmentTraversalIndex1C; // +0x1c traversal-only counter/reset state
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* fragmentTraversalNode20; // +0x20 current retained-fragment list node during traversal
    uint8_t* currentCursor24; // +0x24 cursor inside the retained fragments; packet-body start on emitted packets, unread stream cursor on carry-over assembly state
    uint32_t assembledByteCount28; // +0x28 assembled packet-body byte count once prefix decode succeeds
};

static_assert(sizeof(CLTTCPConnection_ParsedPacketWorkItemScaffold) == 0x2c, "parsed packet work item size mismatch");

// High-confidence original parser object at connection `+0x6c`:
// - vtable `0x004baf84`
// - concrete class `CVariableLengthPrefixedTCPStreamParser`
// - implementation owned by
//   `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
class CVariableLengthPrefixedTCPStreamParser;
class CBaseConnection;

// Recovered worker/send family tightening from `0x44a9f0`, `0x44aa70`, `0x44ac90`, `0x44ad80`,
// and `0x42fe50`:
// - connection `+0x08` stores the direct worker-thread object pointer
// - connection `+0x38` is a byte flag flipped by send-queue push/pop helpers
//   - ctor seeds it to `1`
//   - send-buffer queue push clears it to `0`
//   - worker pop helper restores it to `1` when the queue empties
// - connection `+0x3c` roots the pending-send queue consumed by the worker-thread write path
// Current source-owned queue item keeps only the active send-path facts explicit:
// - queued byte storage
// - remote endpoint snapshot used by the datagram sendto path
struct CLTTCPConnection_SendQueueItemScaffold {
    LTTCPEndpointKey remoteEndpoint;
    std::vector<uint8_t> ownedBytes;
};

// UNANCHORED: source-owned helper that recognizes the current queue-context bridge object and
// returns its owning `CBaseConnection` when present.
CBaseConnection* CBaseConnection_FromQueueContextScaffold(void* maybeQueueContext);
// UNANCHORED: source-owned helper for queue-consumer slot-12-style cleanup.
// Original queue consumers dequeue a real connection-family object as `context`; current source may
// instead carry the queue-context bridge object that dispatches back into `vtable[4]`.
// This helper narrows that gap by resolving the cleanup/search key back to the owning connection
// object when the bridge object is what was actually queued.
void* CBaseConnection_ResolveQueueCleanupContextKeyScaffold(void* maybeQueueContext);

// Source-owned queue-context bridge compensating for the current non-byte-faithful C++ vtable
// layout of `CBaseConnection` / `CLTTCPConnection` while queue consumers still dispatch through
// original-style slot `+0x10` (`vtable[4]`).
struct CBaseConnection_QueueContextScaffold {
    void** vtable;           // +0x00
    uint8_t autoReleaseFlag; // +0x04
    uint8_t padding05[3];    // +0x05..+0x07
    CBaseConnection* owner;  // +0x08
};

// Source-owned abstraction over the recovered connection family.
// Important current limitation:
// - this C++ base keeps the recovered state/virtual relationships useful to the replacement
//   launcher, but it is not yet a byte-faithful class-layout reconstruction of the original
//   `CBaseConnection` / `CLTTCPConnection` hierarchy.
class CBaseConnection {
 public:
  // UNANCHORED: source-owned abstract base for the recovered connection family.
  virtual ~CBaseConnection() = default;

  // UNANCHORED: source-owned abstraction over the recovered receive entry surface.
  virtual void OnReceive(CLTTCPReadOperation* readOperationFragment) = 0;
  // UNANCHORED: source-owned abstraction over the recovered completion callback surface.
  virtual uint32_t OnOperationCompleted(void* workItem) = 0;
  // UNANCHORED: source-owned abstraction over the recovered send callback surface.
  virtual uint32_t SendPacket(const void*, uint32_t, void*) = 0;
  // UNANCHORED: source-owned abstraction over the recovered shared close wrapper.
  // Original `0x449ca0` lives in the base connection slot family and `0x449d40` reaches it
  // through the inherited virtual dispatch at `this->+0x0c(false)`.
  virtual uint32_t Close(bool graceful) = 0;

  // UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
  virtual bool IsConnected() const;

  // UNANCHORED: source-owned queue-context bridge accessor used where queue consumers still expect
  // original-style context vtable slot `+0x10`.
  void* QueueContextScaffold() { return &queueContextScaffold_; }

  // UNANCHORED: source-owned accessor over the recovered `+0x34` state field.
  LTTCPEngineConnectionState State() const {
    return state_;
  }

  // UNANCHORED: source-owned narrow mirror of the `0x44a9f0` base-ctor state write.
  // The original base ctor initializes much more of the eventual full object than this reduced
  // source-side base class owns.
  CBaseConnection(LTTCPEngineConnectionState initialState = LTTCPEngineConnectionState::kClosed);

 protected:
  LTTCPEngineConnectionState state_;
  CBaseConnection_QueueContextScaffold queueContextScaffold_;
};

// Recovered CLTTCPConnection-family wrapper surface.
// Current high-confidence anchors from launcher.exe vtable `0x004b8034`:
// - `0x00449ca0` = Close wrapper into engine slot `+0x1c`
// - `0x00449d40` = OnReceive
// - `0x00449fd0` = OnClose callback-forwarder
// - `0x00449cd0` = Connect wrapper that updates endpoint `+0x24` then calls engine slot `+0x18`
// - `0x00449d20` = SendBuffer wrapper into engine slot `+0x20`
class CLTTCPConnection : public CBaseConnection {
public:
    // UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family.
    // Current source ctor seeds only the replacement-side fields we model explicitly and does not
    // reconstruct the original parser-argument construction path.
    CLTTCPConnection();
    // UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
    // replacement-side owner-context scaffold.
    explicit CLTTCPConnection(void* ownerContext);
    // anchor: launcher.exe:0x44ac40
    ~CLTTCPConnection();

    // UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
    CLTThreadPerClientTCPEngine* Engine() const;

    // UNANCHORED: source-owned owner-context setter used by the current scaffolds.
    void SetOwnerContext(void* ownerContext);
    // UNANCHORED: source-owned owner-context accessor used by the current scaffolds.
    void* OwnerContext() const;

    // UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
    void SetSocketHandle(uint32_t socketHandle);
    // UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
    uint32_t SocketHandle() const;

    // Recovered direct connection `+0x08` worker pointer from `0x431ff0` / `0x42fbd0` / `0x42fe50`.
    void SetWorkerThreadScaffold(CLTThreadPerClientTCPEngine_WorkerThread* workerThread);
    CLTThreadPerClientTCPEngine_WorkerThread* WorkerThreadScaffold() const;

    // UNANCHORED: source-owned connection-state setter used by the current scaffolds.
    void SetState(LTTCPEngineConnectionState state);
    // UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
    LTTCPEngineConnectionState State() const;

    // UNANCHORED: source-owned endpoint setter over the recovered connection `+0x24` copy.
    void SetRemoteEndpoint(const LTTCPEndpointKey& endpoint);
    // UNANCHORED: source-owned endpoint accessor over the recovered connection `+0x24` copy.
    const LTTCPEndpointKey& RemoteEndpoint() const;

    // UNANCHORED: source-owned hostname setter used by the current resolver scaffold.
    void SetRemoteHostName(const char* hostName);
    // UNANCHORED: source-owned hostname accessor used by the current resolver scaffold.
    const std::string& RemoteHostName() const;

    // anchor: launcher.exe:0x42fe50 TCP receive subpath
    // Narrow source-owned mirror of the worker-thread receive delivery shape:
    // - each recv iteration allocates a `CLTTCPReadOperation`-family fragment
    // - recv lands directly into fragment `+0x0c`
    // - take one worker-owned outer ref immediately after allocation/setup
    // - set fragment byte count
    // - take one more delivery-temp ref just before `OnReceive(readOperationFragment)`
    // - release only that delivery-temp ref after the callback returns
    // This helper still models one successful recv/OnReceive iteration per call.
    // Current source may re-enter it repeatedly from the launcher bridge within one arg5 helper
    // poll, but the original `0x42fe50` same-poll recv-drain loop is still not reconstructed here
    // inside `CLTTCPConnection` itself.
    int PollReceiveAndDeliverReadOperationFragmentsScaffold();
    // Lower recv/fragment seam used by the worker-thread select loop after readability is already
    // known. Unlike the legacy poll helper above, this does not run its own select and does not
    // synthesize transport close side effects on terminal recv outcomes.
    int ReceiveReadyReadOperationFragmentScaffold(uint32_t* outWsaError = nullptr, bool* outPeerClosed = nullptr);

    // anchor: launcher.exe:0x449ca0
    // vtable: launcher.exe:0x004b8040
    // Shared inherited close wrapper body also present in `CBaseConnection` vtable `0x004b8018`.
    uint32_t Close(bool graceful) override;

    // anchor: launcher.exe:0x449cd0
    // vtable: launcher.exe:0x004b8050
    uint32_t Connect(const LTTCPEndpointKey& endpoint);

    // anchor: launcher.exe:0x449d20
    // vtable: launcher.exe:0x004b8054
    // Active `0x448a00` callers currently reach this wrapper with ownership-mode `1` in the
    // fourth stack slot, not an arbitrary callback pointer.
    uint32_t SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext);

    // anchor: launcher.exe:0x449fd0
    // vtable: launcher.exe:0x004b804c
    // Current best original ABI is wider than the source-owned wrapper logic:
    // - `ecx` is ignored
    // - function returns with `ret 0xc`
    // - current concrete caller shape from `0x42fe50` prepares:
    //   `(readOperationFragment, peerAddressBlob16Ptr, 0x004b2118)`
    // Current recovered semantic effect is still only:
    // - if `readOperationFragment != nullptr`, call `readOperationFragment->+0x08()`
    void OnClose(
        CLTTCPReadOperation* readOperationFragment,
        void* opaqueArg08 = nullptr,
        void* opaqueArg0c = nullptr);

    // anchor: launcher.exe:0x449d40
    // vtable: launcher.exe:0x004b8048
    // Current best read: retain one typed `CLTTCPReadOperation`-family fragment, hand it to the
    // parser at connection `+0x6c` as `Parse(fragment, &completedPacketWorkItem)`, enqueue each
    // parser-emitted completed packet work item as the exact `0x449d8a -> 0x436820` handoff
    // `(engine+0x10, completedPacketWorkItem, this, false)`, then branch through the original
    // endpoint-based terminal-error log split before releasing the outer fragment reference.
    void OnReceive(CLTTCPReadOperation* readOperationFragment) override;

    // Recovered send-queue seam beneath slot `8` / `0x42fbd0`.
    // Current bounded source mirror keeps the active `0x448a00 -> vtable +0x20(...,1)` copied-byte
    // path explicit while still using source-owned `std::deque` storage under the hood.
    bool QueueSendBufferScaffold(const void* buffer, uint32_t byteCount, uintptr_t ownershipMode = 1u);
    bool TryPopQueuedSendBufferScaffold(CLTTCPConnection_SendQueueItemScaffold* outItem);
    bool SendQueueEmptyFlagScaffold() const;

    // UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
    uint32_t CloseSocketTransportScaffold(bool graceful);
    // UNANCHORED: low-level raw-socket send helper used beneath the anchored SendBuffer wrapper.
    uint32_t SendRawSocketBufferScaffold(const void* buffer, uint32_t byteCount, void* completionContext);

    // UNANCHORED: source-owned mirror of the exact `0x449d8a` enqueue handoff.
    // Current best original read:
    // - argument order after engine `this` is `(workItem, connection, useQueue34)`
    // - this `OnReceive` path always uses `(completedPacketWorkItem, this, false)`
    // - the queued context is the direct connection object itself, not the source-owned
    //   queue-context bridge that remains only as a consumer-side compatibility fallback
    // - ownership transfers to the engine queue here; caller-side lifetime does not branch on an
    //   enqueue success result because original `0x436820` returns `void`
    void EnqueueCompletedPacketWorkItemScaffold(CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);

private:
    CLTThreadPerClientTCPEngine* engine_;
    void* ownerContext_;
    uint32_t socketHandle_;
    CLTThreadPerClientTCPEngine_WorkerThread* workerThread08_;
    LTTCPEndpointKey remoteEndpoint_;
    std::string remoteHostName_;
    bool sendQueueEmptyFlag38_;
    mutable std::mutex sendQueueMutex_;
    std::deque<CLTTCPConnection_SendQueueItemScaffold> sendQueue3c_;
    // High-confidence original seam: `CLTTCPConnection_ctor` stores a concrete
    // `CVariableLengthPrefixedTCPStreamParser` object pointer at connection `+0x6c`.
    CVariableLengthPrefixedTCPStreamParser* parser06c_;
};

}  // namespace mxo::liblttcp
