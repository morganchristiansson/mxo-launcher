#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace mxo::liblttcp {

class CLTThreadPerClientTCPEngine_0x4b2768;
class CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread;

// Reimplementation note:
// This is still a starter original-name skeleton.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/launcher.exe/VTABLES/0x004b8034.md

// State values recovered from original CLTThreadPerClientTCPEngine_0x4b2768 paths.
// Only the meanings marked in comments are evidence-backed so far.
enum class LTTCPEngineConnectionState : uint32_t {
    kUnknown = 0,
    kConnectActive = 1,     // written by Connect success path before worker-thread completion
    kUdpMonitorActive = 2,  // written by UDPMonitorPort success path and by TCP worker connect-complete success
    kClosing = 4,           // written by low-level close path before deferred shutdown / wakeup-only cleanup
    kClosed = 8,            // required by connection-wrapper and engine prechecks
};

class LTTCPEndpointKey_0x44b070 {
public:
    // anchor: launcher.exe:0x44b070
    // Default endpoint-key constructor used by the connection / parser / worker families.
    // Original body zeros the full 16-byte block first, then writes `family = AF_INET`.
    LTTCPEndpointKey_0x44b070();

    // anchor: launcher.exe:0x44b090
    // Parameterized constructor: zeroes all 16 bytes, sets family=AF_INET, stores ipv4,
    // and converts port to network order via htons (IAT slot at 0x004a9818).
    LTTCPEndpointKey_0x44b070(uint32_t ipv4NetworkOrder, uint16_t portHostOrder);

    // anchor: launcher.exe:0x44b020
    // Original helper compares the 16-byte key as four dwords (`repe cmpsd`) and returns true
    // when any dword differs.
    bool DiffersFrom(const LTTCPEndpointKey_0x44b070& other) const;

    // anchor: launcher.exe:0x44aff0
    // Original helper copies the 16-byte key as four dwords.
    void CopyTo(LTTCPEndpointKey_0x44b070* outEndpointKey) const;

    uint16_t family = 0; // AF_INET after ctor
    uint16_t portNetworkOrder = 0;
    uint32_t ipv4NetworkOrder = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

static_assert(sizeof(LTTCPEndpointKey_0x44b070) == 0x10, "endpoint key size mismatch");

// Recovered parser input fragment family consumed by connection `+0x6c`
// (`CVariableLengthPrefixedTCPStreamParser::Parse`).
// Current best family name comes from `CMessageConnection_0x4b7928::OnOperationCompleted` logging
// ("Unused buffers were attached to CLTTCPReadOperation ...") plus the worker-thread receive path.
// This is a refcounted read-buffer fragment object, not a completed-packet work item.
// High-confidence fields / methods from `0x42fe50`, `0x42f820`, `0x42f850`, `0x42f860`,
// `0x42f880`, `0x42f890`, `0x452350`, `0x452400`, `0x452520`, `0x452560`, `0x469bf0`,
// `0x435e60`, `0x4350c0`, and `0x435510`:
// - worker-thread receive path installs live leaf vtable `0x004b2300` and allocates `0x100c`
//   bytes for this family
// - deleting dtor `0x42fd50` collapses that live leaf back to shared low-level refcounted base
//   `0x004b211c` before returning storage through `0x452520`
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

    // UNANCHORED: source-side operator wrappers over the recovered fixed-size fragment allocator
    // helpers `0x452560` / `0x452520`.
    static void* operator new(std::size_t requestedSize, const std::nothrow_t&) noexcept;
    static void operator delete(void* storage) noexcept;
    static void operator delete(void* storage, const std::nothrow_t&) noexcept;

    // anchor: launcher.exe:0x452350
    void SetByteCount(uint32_t byteCount);

    uint32_t byteCount08; // +0x08
};

static_assert(sizeof(CLTTCPReadOperation) == 0x0c, "read-operation fragment prefix size mismatch");

struct CLTTCPReadOperationRefHandle {
    // anchor: launcher.exe:0x434fa0
    // Tiny retained-fragment handle helper used by parser state (`0x469bf0` / `0x4725c0`) and by
    // `CMessageConnection_0x4b7928::OnOperationCompleted` stack locals.
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
//   - shared work-item root prefix `CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader`
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

struct CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08 {
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

static_assert(sizeof(CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08) == 0x2c, "parsed packet work item size mismatch");

// High-confidence original parser object at connection `+0x6c`:
// - vtable `0x004baf84`
// - concrete class `CVariableLengthPrefixedTCPStreamParser`
// - implementation owned by
//   `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
class CVariableLengthPrefixedTCPStreamParser;
class CBaseConnection_0x4b8018;

// Recovered worker/send family tightening from `0x44a9f0`,
// `TryPopQueuedSendBufferWithEndpoint (0x44aa70)`,
// `cls_0x44ac90::QueueSendBufferWithEndpoint (0x44ac90)`,
// `cls_0x44ac90::QueueSendBuffer (0x44ad80)`, and `0x42fe50`:
// - connection `+0x08` stores the direct worker-thread object pointer
// - connection `+0x38` is a byte flag flipped by send-queue push/pop helpers
//   - ctor seeds it to `1`
//   - send-buffer queue push clears it to `0`
//   - worker pop helper restores it to `1` when the queue empties
// - connection `+0x3c` roots the pending-send queue consumed by the worker-thread write path
//
// Faithfulness note:
// - the earlier source reduced this family to a bare scaffold struct plus `std::deque`
// - current source now lifts the recovered send-queue helpers into explicit classes so the
//   `0x44aa70 / 0x44ac90 / 0x44ad80` object seams remain visible in code instead of being flattened
//   into anonymous container plumbing
// - launcher.exe is still sharper than the current source container:
//   - queued item wrapper `0x44a500` allocates a `0x14` record
//     `{sendBufferStorageDescriptor*, endpointKey16}`
//   - queue node `0x44a310` allocates a separate `0x14` lock-free node whose payload lives at
//     node `+0x10`
//   - queue core `0x44a830 / 0x44a900 / 0x449ff0` is a versioned Michael-Scott-style enqueue /
//     dequeue family with queue-local retired-head cleanup, not a native `std::deque`
class CLTTCPConnection_QueuedSendBufferStorage {
public:
    CLTTCPConnection_QueuedSendBufferStorage();

    // anchor family: launcher.exe:0x44ac90 ownership-mode branch / launcher.exe:0x44a7c0 release
    // Current best ownership-mode read from launcher.exe:
    // - `0` = transfer caller-owned tracked heap buffer; release path uses tracked free
    // - `1` = copy bytes into a pooled `0x1000` send buffer block
    // - `2` = transfer an already-pooled `0x1000` send buffer block back to the same pool
    // Active launcher path still reaches mode `1`; current source keeps the storage flattened and
    // copied even when static RE proves the original mode-`0/2` ownership split.
    bool InitializeFromSendBuffer(const void* sendBuffer, uint32_t byteCount, uintptr_t ownershipMode);
    void Reset();

    const uint8_t* BufferBytes() const;
    uint32_t BufferByteCount() const;

private:
    uint8_t usesPooledBuffer00_;
    uint8_t padding01_03_[3];
    std::vector<uint8_t> bufferBytes04_;
    uint32_t bufferByteCount08_;
};

class CLTTCPConnection_QueuedSendBufferWithEndpoint {
public:
    // Source-owned flattened stand-in for the original two-object wrapper/storage split:
    // - launcher.exe queues a `0x14` wrapper that points at a separate `0x0c`
    //   `CLTTCPConnection_QueuedSendBufferStorage`-style descriptor
    // - current source embeds the storage descriptor directly to keep the active worker-send path
    //   simple while the helper-family meaning stays visible in comments/docs
    CLTTCPConnection_QueuedSendBufferStorage sendBufferStorage00;
    LTTCPEndpointKey_0x44b070 remoteEndpoint04;
};

class CLTTCPConnection_PendingSendQueue {
public:
    CLTTCPConnection_PendingSendQueue();

    // anchor: launcher.exe:0x44ac90
    bool QueueSendBufferWithEndpoint(
        const void* sendBuffer,
        uint32_t byteCount,
        const LTTCPEndpointKey_0x44b070& remoteEndpoint,
        uintptr_t ownershipMode);
    // anchor: launcher.exe:0x44ad80
    bool QueueSendBuffer(const void* sendBuffer, uint32_t byteCount, uintptr_t ownershipMode);
    // anchor: launcher.exe:0x44aa70
    bool TryPopQueuedSendBufferWithEndpoint(CLTTCPConnection_QueuedSendBufferWithEndpoint* outItem);
    bool SendQueueEmptyFlag() const;
    // anchor: launcher.exe:0x44ab60 helper family consumed by CleanupConnection
    void ReleasePendingSendQueueContents();

private:
    uint8_t sendQueueEmptyFlag38_;
    uint8_t padding39_3b_[3];
    mutable std::mutex pendingSendQueueMutex_;
    // Source-owned bounded stand-in for the original versioned lock-free queue rooted at
    // connection `+0x3c`.
    std::deque<CLTTCPConnection_QueuedSendBufferWithEndpoint> pendingSendQueue3c_;
};

class CBaseConnection_0x4b8018;
struct CBaseConnection_QueueContextScaffold;

// UNANCHORED: launcher-owned queued connection-context ABI wrapper initializer.
// The active replacement still materializes a tiny MSVC2003-compatible queue callback surface for
// client.dll consumers; ownership of that ABI lie lives in `src/launcher_network_object_abi.cpp`.
void InitializeBaseConnectionQueueContextScaffold(
    CBaseConnection_QueueContextScaffold* queueContext,
    CBaseConnection_0x4b8018* owner,
    uint8_t autoReleaseFlag);
// UNANCHORED: launcher-owned helper that recognizes the queued connection-context ABI adapter
// object and returns its owning `CBaseConnection_0x4b8018` when present.
CBaseConnection_0x4b8018* CBaseConnection_FromQueueContextScaffold(void* maybeQueueContext);
// UNANCHORED: source-owned ABI-dispatch wrapper for generic queued work-item slot-`+0x04`
// release calls.
uint32_t QueuedWorkItem_InvokeReleaseSlotScaffold(void* object);

// Source-owned queue-dispatch ABI adapter compensating for the current MinGW-vs-MSVC C++ vtable
// mismatch when client.dll consumes queued connection contexts through raw slot `+0x10`
// (`vtable[4]`) and the optional type-1 auto-release slot `+0x04`.
// Current recovered slot contract at this seam:
// - `vtable[1]` / slot `+0x04` = optional queued-context auto-release entry used only by type-1
//   close work
// - `vtable[4]` / slot `+0x10` = `OnOperationCompleted(void*)`
// This is intentionally *not* the full `CBaseConnection_0x4b8018` native vtable family yet; it is
// the minimum slot surface the queue consumer needs.
struct CBaseConnection_QueueContextScaffold {
    static constexpr size_t kReleaseSlotIndex = 1;
    static constexpr size_t kOnOperationCompletedSlotIndex = 4;

    void** vtable;           // +0x00
    uint8_t autoReleaseFlag; // +0x04
    uint8_t padding05[3];    // +0x05..+0x07
    CBaseConnection_0x4b8018* owner;  // +0x08
};

static_assert(sizeof(CBaseConnection_QueueContextScaffold) == 0x0c, "queue-context scaffold size mismatch");
static_assert(offsetof(CBaseConnection_QueueContextScaffold, autoReleaseFlag) == 0x04, "queue-context scaffold autoReleaseFlag offset mismatch");
static_assert(offsetof(CBaseConnection_QueueContextScaffold, owner) == 0x08, "queue-context scaffold owner offset mismatch");

// Source-owned abstraction over the recovered connection family.
// Recovered base vtable `0x004b8018` currently reads as 7 rows under the MSVC ABI:
// - deleting-dtor pair synthesized from the virtual destructor
// - slot `+0x08` = purecall in the base, `0x437860` / return-true in `CLTTCPConnection`
// - slot `+0x0c` = `0x449ca0` Close(bool)
// - slot `+0x10` = `0x443810` / return-false in `CLTTCPConnection`, later `0x4490c0`
//   `OnOperationCompleted` in `CMessageConnection_0x4b7928`
// - slot `+0x14` = purecall in the base, later `0x449d40` `CLTTCPConnection::OnReceive`
// - slot `+0x18` = purecall in the base, later `0x449fd0` `CLTTCPConnection::OnClose`
class CBaseConnection_0x4b8018 {
 public:
  virtual ~CBaseConnection_0x4b8018() = default;

  // anchor: launcher.exe:0x004b8020 / `0x437860` on the `0x004b8034` row
  virtual bool UsesTcpConnectionVtableShape() const = 0;
  // anchor: launcher.exe:0x449ca0
  // vtable: launcher.exe:0x004b8024
  virtual uint32_t Close(bool graceful);
  // anchor: launcher.exe:0x004b8028 / `0x443810` on the `0x004b8034` row, later overridden by
  // `CMessageConnection_0x4b7928::OnOperationCompleted` at `0x4490c0`.
  virtual uint32_t OnOperationCompleted(void* workItem) {
    (void)workItem;
    return 0u;
  }
  // anchor: launcher.exe:0x004b802c / `0x449d40` on the `0x004b8034` row
  virtual void OnReceive(CLTTCPReadOperation* readOperationFragment) = 0;
  // anchor: launcher.exe:0x004b8030 / `0x449fd0` on the `0x004b8034` row
  virtual void OnClose(
      CLTTCPReadOperation* readOperationFragment,
      void* opaqueArg08 = nullptr,
      void* opaqueArg0c = nullptr) = 0;

  // UNANCHORED: source-owned utility accessor over the recovered `+0x34` state field.
  bool IsConnected() const;

  // UNANCHORED: source-owned accessor over the recovered base `+0x04` auto-release byte tested by
  // `0x436d31..0x436ee7` before the later queued-context `+0x04` release call on type-1 work.
  uint8_t AutoReleaseFlag04() const { return autoReleaseFlag04_; }
  // UNANCHORED: source-owned setter for the same recovered base `+0x04` byte.
  void SetAutoReleaseFlag04(uint8_t autoReleaseFlag) {
    autoReleaseFlag04_ = autoReleaseFlag;
    queueContextScaffold_.autoReleaseFlag = autoReleaseFlag;
  }

  // EXPERIMENTAL: queued contexts now cross this seam directly as native connection-family
  // objects again so the current logs can show exactly where scaffold assumptions still survive.
  void* QueueContextScaffold() { return this; }

  // UNANCHORED: source-owned accessor over the recovered `+0x34` state field.
  LTTCPEngineConnectionState State() const {
    return state_;
  }

  // UNANCHORED: source-owned narrow mirror of the `0x44a9f0` base-ctor state write.
  // The original base ctor initializes much more of the eventual full object than this reduced
  // source-side base class owns.
  CBaseConnection_0x4b8018(
      LTTCPEngineConnectionState initialState = LTTCPEngineConnectionState::kClosed);

 protected:
  uint8_t autoReleaseFlag04_;
  uint8_t padding05_07_[3];
  // Source-owned compatibility mirror of the recovered base connection `+0x10` engine field.
  CLTThreadPerClientTCPEngine_0x4b2768* engine_;
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
class CLTTCPConnection : public CBaseConnection_0x4b8018 {
public:
    // UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family.
    // Current source ctor seeds only the replacement-side fields we model explicitly and does not
    // reconstruct the original parser-argument construction path.
    CLTTCPConnection();
    // UNANCHORED: source-owned narrow subset of the `0x44aad0` ctor family that also seeds the
    // replacement-side owner-context scaffold.
    explicit CLTTCPConnection(void* ownerContext);
    // anchor: launcher.exe:0x44ac40
    ~CLTTCPConnection() override;

    // anchor: launcher.exe:0x004b803c / `0x437860`
    bool UsesTcpConnectionVtableShape() const override { return true; }
    // anchor: launcher.exe:0x004b8044 / `0x443810`
    uint32_t OnOperationCompleted(void* workItem) override {
        (void)workItem;
        return 0u;
    }

    // UNANCHORED: source-owned compatibility wrapper over the recovered connection `+0x10` engine field.
    void SetEngine(CLTThreadPerClientTCPEngine_0x4b2768* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered connection `+0x10` engine field.
    CLTThreadPerClientTCPEngine_0x4b2768* Engine() const;

    // UNANCHORED: source-owned owner-context setter used by the current scaffolds.
    void SetOwnerContext(void* ownerContext);
    // UNANCHORED: source-owned owner-context accessor used by the current scaffolds.
    void* OwnerContext() const;

    // UNANCHORED: source-owned socket-handle setter used by the current scaffolds.
    void SetSocketHandle(uint32_t socketHandle);
    // UNANCHORED: source-owned socket-handle accessor used by the current scaffolds.
    uint32_t SocketHandle() const;

    // Recovered direct connection `+0x08` worker pointer from `0x431ff0` / `0x42fbd0` / `0x42fe50`.
    void SetWorkerThreadScaffold(CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread);
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* WorkerThreadScaffold() const;

    // UNANCHORED: source-owned connection-state setter used by the current scaffolds.
    void SetState(LTTCPEngineConnectionState state);
    // UNANCHORED: source-owned connection-state accessor used by the current scaffolds.
    LTTCPEngineConnectionState State() const;

    // anchor: launcher.exe:0x42fe50 TCP receive subpath
    // Narrow source-owned mirror of the worker-thread receive delivery shape:
    // - each recv iteration allocates a `CLTTCPReadOperation`-family fragment
    // - recv lands directly into fragment `+0x0c`
    // anchor: launcher.exe:0x449ca0
    // vtable: launcher.exe:0x004b8040
    // Inherited shared `CBaseConnection_0x4b8018::Close(bool)` body; this table does not add a
    // distinct `CLTTCPConnection::Close` override.

    // anchor: launcher.exe:0x449d40
    // vtable: launcher.exe:0x004b8048
    // Current best read: retain one typed `CLTTCPReadOperation`-family fragment, hand it to the
    // parser at connection `+0x6c` as `Parse(fragment, &completedPacketWorkItem)`, enqueue each
    // parser-emitted completed packet work item as the exact `0x449d8a -> 0x436820` handoff
    // `(engine+0x10, completedPacketWorkItem, this, false)`, then branch through the original
    // endpoint-based terminal-error log split before releasing the outer fragment reference.
    // Active source now queues the direct connection object pointer, matching the original
    // `(engine+0x10, completedPacketWorkItem, this, false)` handoff from `0x449d8a -> 0x436820`.
    void OnReceive(CLTTCPReadOperation* readOperationFragment) override;

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
        void* opaqueArg0c = nullptr) override;

    // anchor: launcher.exe:0x449cd0
    // vtable: launcher.exe:0x004b8050
    // This wrapper is first introduced on the concrete `CLTTCPConnection` table, not on
    // `CBaseConnection_0x4b8018`.
    virtual uint32_t Connect(const LTTCPEndpointKey_0x44b070& endpoint);

    // anchor: launcher.exe:0x449d20
    // vtable: launcher.exe:0x004b8054
    // Active `0x448a00` callers currently reach this wrapper with ownership-mode `1` in the
    // fourth stack slot, not an arbitrary callback pointer.
    virtual uint32_t SendBuffer(const void* buffer, uint32_t byteCount, void* completionContext);

    // Recovered send-queue seam beneath slot `8` / `0x42fbd0`.
    // Current best helper names from Ghidra/source lockstep:
    // - `0x44ad80 = cls_0x44ac90::QueueSendBuffer`
    // - `0x44ac90 = cls_0x44ac90::QueueSendBufferWithEndpoint`
    // - `0x44aa70 = cls_0x449ad0::TryPopQueuedSendBufferWithEndpoint`
    // anchor: launcher.exe:0x44ad80
    bool QueueSendBuffer(const void* buffer, uint32_t byteCount, uintptr_t ownershipMode = 1u);
    // anchor: launcher.exe:0x44ac90
    bool QueueSendBufferWithEndpoint(
        const void* buffer,
        uint32_t byteCount,
        const LTTCPEndpointKey_0x44b070& remoteEndpoint,
        uintptr_t ownershipMode);
    // anchor: launcher.exe:0x44aa70
    bool TryPopQueuedSendBufferWithEndpoint(CLTTCPConnection_QueuedSendBufferWithEndpoint* outItem);
    bool SendQueueEmptyFlag() const;
    // anchor: launcher.exe:0x44ab60 helper family consumed by CleanupConnection
    void ReleasePendingSendQueueContentsScaffold();


    // anchor: launcher.exe:0x41de6a - endpoint at connection +0x24
    LTTCPEndpointKey_0x44b070 remoteEndpoint_;

private:
    void* ownerContext_;
    uint32_t socketHandle_;
    CLTThreadPerClientTCPEngine_0x4b2768_WorkerThread* workerThread08_;
    CLTTCPConnection_PendingSendQueue pendingSendQueueState38_;
    // High-confidence original seam: `CLTTCPConnection_ctor` stores a concrete
    // `CVariableLengthPrefixedTCPStreamParser` object pointer at connection `+0x6c`.
    CVariableLengthPrefixedTCPStreamParser* parser06c_;
};

}  // namespace mxo::liblttcp
