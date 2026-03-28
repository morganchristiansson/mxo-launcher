#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mxo::liblttcp {

class CLTThreadPerClientTCPEngine;

// Reimplementation note:
// This is still a starter original-name skeleton.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/launcher.exe/VTABLES/0x004b8034.md

// State values recovered from original CLTThreadPerClientTCPEngine paths.
// Only the meanings marked in comments are evidence-backed so far.
enum class LTTCPEngineConnectionState : uint32_t {
    kUnknown = 0,
    kConnectActive = 1,     // written by Connect success path
    kUdpMonitorActive = 2,  // written by UDPMonitorPort success path
    kClosing = 4,           // provisional: written by low-level close path
    kClosed = 8,            // required by connection-wrapper and engine prechecks
};

struct LTTCPEndpointKey {
    uint16_t family = 2;          // AF_INET
    uint16_t portNetworkOrder = 0;
    uint32_t ipv4NetworkOrder = 0;
    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

// Recovered parser input fragment prefix consumed by connection `+0x6c`
// (`CVariableLengthPrefixedTCPStreamParser::Parse`).
// Current best family name comes from `CMessageConnection::OnOperationCompleted` logging
// ("Unused buffers were attached to CLTTCPReadOperation ...") plus the worker-thread receive path.
// This is a refcounted read-buffer fragment object, not a completed-packet work item.
// High-confidence fields from `0x42fe50`, `0x42f850`, `0x42f860`, `0x42f880`, `0x42f890`,
// `0x452350`, `0x469bf0`, `0x435e60`, `0x4350c0`, and `0x435510`:
// - worker-thread receive path installs vtable `0x004b2300` and allocates `0x100c` bytes for this family
// - `+0x04` = interlocked refcount
// - `+0x08` = byte count
// - `+0x0c` = first payload byte
// - vtable `+0x00` = deleting-dtor-style entry (`this, deleteFlag`)
// - vtable `+0x04` = no-arg AddRef / retain on the fragment object itself
// - vtable `+0x08` = no-arg Release on the fragment object itself
// - vtable `+0x10` = reset refcount to zero
// - vtable `+0x14` = set refcount from pointed-to value
struct CLTTCPReadOperationFragmentScaffold;

struct CLTTCPReadOperationFragmentVTable {
    void* (__thiscall *deletingDtor)(CLTTCPReadOperationFragmentScaffold* self, uint8_t deleteFlag); // +0x00
    void (__thiscall *addRef)(CLTTCPReadOperationFragmentScaffold* self);                             // +0x04
    void (__thiscall *release)(CLTTCPReadOperationFragmentScaffold* self);                            // +0x08
    void (__thiscall *deleteIfNonNull)(CLTTCPReadOperationFragmentScaffold* self);                    // +0x0c
    void (__thiscall *resetRefCount)(CLTTCPReadOperationFragmentScaffold* self);                      // +0x10
    void (__thiscall *setRefCountFromPtr)(CLTTCPReadOperationFragmentScaffold* self, const long* value); // +0x14
};

struct CLTTCPReadOperationFragmentScaffold {
    CLTTCPReadOperationFragmentVTable* vtable; // +0x00
    volatile long referenceCount;              // +0x04 interlocked by AddRef / Release
    uint32_t byteCount;                        // +0x08
    uint8_t bytes0C[1];                        // +0x0c variable-length fragment bytes begin here
};

// Recovered `0x2c` parsed-packet work-item family built via
// `CVariableLengthPrefixedTCPStreamParser_AllocatePacketBuffer -> CParsedPacketWorkItem_ctor`.
// Current best parser-family read from `0x469bf0`, `0x4725c0`, `0x435e60`, `0x4355c0`,
// `0x4350c0`, `0x435510`, and `0x4490c0`:
// - vtable = `0x4b3e08`
// - `+0x04` = work type `3`
// - this same object family is used in two phases:
//   - parser-owned assembly state while stream bytes are being accumulated
//   - emitted completed packet object after `Parse(...)` returns `0`
// - after emit, `CVariableLengthPrefixedTCPStreamParser_ResetAfterPacket` allocates a fresh
//   replacement object and, when unread bytes remain in the tail fragment, carries forward that
//   tail fragment plus the new `currentCursor24`
struct CParsedPacketWorkItem_RetainedFragmentNodeScaffold {
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* next;   // +0x00
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* prev;   // +0x04
    CLTTCPReadOperationFragmentScaffold* retainedFragment08;    // +0x08 retained fragment reference
};

struct CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold {
    CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel; // +0x00 sentinel-headed list root
};

static_assert(sizeof(CParsedPacketWorkItem_RetainedFragmentNodeScaffold) == 0x0c, "parsed packet retained-fragment node size mismatch");
static_assert(sizeof(CParsedPacketWorkItem_RetainedFragmentListOwnerScaffold) == 0x04, "parsed packet retained-fragment list owner size mismatch");

struct CLTTCPConnection_ParsedPacketWorkItemScaffold {
    void** vtable; // +0x00
    uint32_t workType; // +0x04 = 3
    uint32_t field08; // +0x08 unresolved in the current parser-focused read
    uint32_t retainedFragmentCount0C; // +0x0c retained fragment count (`AppendFragment` / `GetTailFragment`)
    CLTTCPReadOperationFragmentScaffold* firstRetainedFragment10; // +0x10 first retained fragment
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
  virtual void OnReceive(void* callbackContext) = 0;
  // UNANCHORED: source-owned abstraction over the recovered completion callback surface.
  virtual uint32_t OnOperationCompleted(void* workItem) = 0;
  // UNANCHORED: source-owned abstraction over the recovered send callback surface.
  virtual uint32_t SendPacket(const void*, uint32_t, void*) = 0;

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
    // - receive into a `CLTTCPReadOperation`-family fragment
    // - take one worker-owned outer ref immediately after allocation/setup
    // - set fragment byte count
    // - take one more delivery-temp ref just before `OnReceive(readOperationFragment)`
    // - release only that delivery-temp ref after the callback returns
    int PollReceiveAndDeliverReadOperationFragmentsScaffold();

    // anchor: launcher.exe:0x449ca0
    // vtable: launcher.exe:0x004b8040
    uint32_t Close(bool graceful);

    // anchor: launcher.exe:0x449cd0
    // vtable: launcher.exe:0x004b8050
    uint32_t Connect(const LTTCPEndpointKey& endpoint);

    // anchor: launcher.exe:0x449d20
    // vtable: launcher.exe:0x004b8054
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
        CLTTCPReadOperationFragmentScaffold* readOperationFragment,
        void* opaqueArg08 = nullptr,
        void* opaqueArg0c = nullptr);

    // anchor: launcher.exe:0x449d40
    // vtable: launcher.exe:0x004b8048
    // Current best read: retain one `CLTTCPReadOperation`-family fragment, hand it to the parser at
    // connection `+0x6c` as `Parse(fragment, &completedPacketWorkItem)`, enqueue each parser-emitted
    // completed packet work item as the exact `0x449d8a -> 0x436820` handoff
    // `(engine+0x10, completedPacketWorkItem, this, false)`, then release the outer fragment
    // reference.
    void OnReceive(void* readOperationFragment) override;

    // UNANCHORED: low-level socket close helper used beneath the anchored Close wrapper.
    uint32_t CloseSocketTransportScaffold(bool graceful);
    // UNANCHORED: low-level raw-socket send helper used beneath the anchored SendBuffer wrapper.
    uint32_t SendRawSocketBufferScaffold(const void* buffer, uint32_t byteCount, void* completionContext);

    // UNANCHORED: source-owned mirror of the exact `0x449d8a` enqueue handoff.
    // Current best original read:
    // - argument order after engine `this` is `(workItem, connection, useQueue34)`
    // - this `OnReceive` path always uses `(completedPacketWorkItem, this, false)`
    // - ownership transfers to the engine queue here; caller-side lifetime does not branch on an
    //   enqueue success result because original `0x436820` returns `void`
    void EnqueueCompletedPacketWorkItemScaffold(CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem);

private:
    // UNANCHORED: internal socket-read helper that fills the connection-owned buffered-byte staging
    // used by the faithful `0x42fe50 -> 0x449d40 -> 0x469bf0` receive seam.
    int ReceiveBufferedSocketBytesNonBlockingScaffold();
    // UNANCHORED: internal buffered-byte prefix-consumption helper used after staged fragment
    // delivery drains bytes out of the connection-owned socket-read staging.
    void ConsumeBufferedSocketBytesPrefixScaffold(size_t byteCount);

    CLTThreadPerClientTCPEngine* engine_;
    void* ownerContext_;
    uint32_t socketHandle_;
    LTTCPEndpointKey remoteEndpoint_;
    std::string remoteHostName_;
    std::vector<uint8_t> receivedBytes_;
    // High-confidence original seam: `CLTTCPConnection_ctor` stores a concrete
    // `CVariableLengthPrefixedTCPStreamParser` object pointer at connection `+0x6c`.
    CVariableLengthPrefixedTCPStreamParser* parser06c_;
};

}  // namespace mxo::liblttcp
