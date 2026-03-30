#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../liblttcp/ltthreadperclienttcpengine.h"

#ifdef DispatchMessage
#undef DispatchMessage
#endif

namespace mxo::liblttcp {

// Reimplementation note:
// This file mirrors the best current read of the queue0C context object family.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md

// Starter skeleton for the original CMessageConnection-family object now visible in
// launcher.exe strings / vtable neighborhoods.
//
// Recovered source-file anchor:
// - `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`
//
// Current evidence-backed role:
// - `0x448b40` first constructs a base CLTTCPConnection-family object, then overwrites
//   its vtable to the CMessageConnection-family vtable at `0x4b7928`
// - captures a CLTThreadPerClientTCPEngine pointer at +0x10
// - carries an endpoint copy at +0x24
// - participates in queue0C producer traffic by enqueueing (workItem, this, 0)
// - likely appears as the dequeued queue0C `context` on important consumer paths
// - string-backed methods include:
//   - CMessageConnection::SendPacket
//   - CMessageConnection::OnOperationCompleted
//
// Current best virtual mapping from launcher.exe:
// - base vtable +0x10 / 0x4490c0 -> likely OnOperationCompleted(workItem)
//   - processes work item types via [workItem+0x04]
//   - string-backed receive/completion/error handling lives on this path
// - newer startup-side narrowing now also shows important **derived** leaf families on top of this
//   base object:
//   - later leaf vtable `0x4afef0`
//     - older notes called this `CBasicMarginConnection`
//     - current focused startup read instead proves it is the auth-side startup leaf reached from
//       `0x41d170` and wrapped by `0x449a70`
//   - later leaf vtable `0x4aff38` (`CMarginConnection`)
//   - those families wrap base completion through `0x449a70` / `0x44af60`
//   - later leaf `0x449a70` is now narrowed one step further:
//     - after base `0x4490c0` returns 0, it calls owner `[self+0xa4]->+0x17c`
//     - that owner surface is now resolved as thunk `0x41f260`, which forwards to the
//       owner's current helper/state object at `owner+0x10`, then jumps to helper vtable `+0x14`
//     - so the concrete handling target depends on the current helper selected through the
//       `0x4f7868` family and `0x41b450(...)`, not on one fixed owner-body function alone
//     - important correction: later body `0x4401a0` is **not** the generic owner `+0x17c`
//       target by itself; it is helper `0x4f7890` (`CLTLoginState_State10`, vtable `0x4b512c`) slot `+0x14`
//     - that helper body only meaningfully handles later raw auth code `0x0b`
//       (`AS_AuthReply`), then updates owner state and reaches
//       `0x41b450` + `CLTLoginMediator::PostEvent()` / `PostError()`
//     - if the current helper `+0x14` target returns 0, `0x449a70` falls through to
//       `0x448a60`, which is string-backed only as a generic
//       `Got unhandled op of type %d with status %s` logger
//   - important nuance: that owner/helper/fallback chain is therefore a later derived-leaf
//     incoming packet/owner-handling anchor, not direct proof of the first outbound request
//     after connect
// - connection fields `+0x7c` / `+0x80` are no longer just anonymous mystery pointers:
//   - ctor helper `0x436080` builds a `0x24` event+critical-section helper object there
//   - helper shape now reads as:
//     - main helper methods: `SetEvent()` and `WaitForSingleObject(timeout)`
//     - embedded lock helper rooted at `+0x04` (shared `0x4add70` family)
//     - event handle at `+0x20`
//   - `0x4490c0` uses them as completion notifiers:
//     - work type `2` -> signal `self+0x7c`
//     - work type `1` -> signal `self+0x80`
//   - current best subtype read:
//     - `+0x7c` = type-2 status/connect-style completion helper
//     - `+0x80` = type-1 close/terminal completion helper, supported by `0x448af0`
//       waiting on `+0x80` until connection state `+0x34` returns to `8`
//   - crucial current narrowing: startup auth/margin derived objects built through
//     `0x41d170 / 0x41e500 -> 0x4417e0 -> 0x448b40(flag=0)` leave both `+0x7c` and `+0x80`
//     as null on that path, so current auth/margin type-2 connect-status handling falls
//     through to owner callbacks instead of using these helper objects
// - inherited send path now tightens better as:
//   - vtable slot 10 / `0x448cf0` = `CMessageConnection::SendPacket`
//   - consumes a message/envelope object, performs packet-agenda filtering, then reaches the
//     lower submit helper `0x448a00`
//   - that lower helper forwards final byte pointer/size together with `self` to engine `+0x20`
//   - current best engine mapping there is slot-8 / SendBuffer
// - vtable +0x1c / 0x449cd0 -> likely endpoint-update / ensure-connected wrapper
//   - updates stored endpoint at +0x24 and then calls engine +0x18 with `self`
//   - current best engine mapping there is slot-6 / Connect
// - vtable +0x0c / 0x449ca0 -> likely close/abort wrapper
//   - calls engine +0x1c with `self`
//   - current best engine mapping there is slot-7 / Close
//
// ============================================================
// VTable 0x004afef0 - CBasicMarginConnection
// ============================================================
// Later leaf on top of the shared message-connection surface:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection-family base surface (`0x448b40`, `0x448cf0`, `0x4490c0`)
//     └── CBaseMarginConnection (0x004b64a8)
//         └── CBasicMarginConnection (0x004afef0)
// Key leaf-only rows now relevant here:
// - 0x004afef0 = 0x0041cf50 ctor / leaf init
// - 0x004aff00 = 0x00449a70 leaf `OnOperationCompleted` override
// - 0x004aff1c = 0x00449a30 leaf `DispatchMessage` override
// - 0x004aff34 = 0x0041ce80 deleting-dtor / cleanup family
// The source file does not yet model `CBasicMarginConnection` as its own class, so do not treat
// those leaf rows as base `CMessageConnection` methods.

// ============================================================
// VTable 0x004aff38 - CMarginConnection
// ============================================================
// Later leaf on top of:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection-family base surface
//     └── CBaseMarginConnection (0x004b64a8)
//         └── CMarginConnection (0x004aff38)
// Key leaf-only rows:
// - 0x004aff38 = 0x0041cf80 ctor / leaf init
// - 0x004aff48 = 0x0044af60 `OnOperationCompleted` override
// - 0x004aff64 = 0x0044af20 `DispatchMessage` override

// Important current limitation for this starter skeleton:
// - the recovered original engine entry on this path is more connection-object-oriented
//   than the placeholder engine signatures currently model
// - keep the names stable, but treat the exact live method signatures as still provisional

struct CMessageConnectionMessageScaffold {
    // Recovered inner message-storage object allocated at size `0x100c` under
    // `0x455bd0 -> 0x455c60` and reached through outer message-ref `+0x0c`.
    // Current best raw-layout read:
    // - `+0x00` = vtable (`0x4ba208` on fresh builder-owned objects)
    // - `+0x04` = refcount dword used with shared `0x42f850/0x42f860`-style AddRef/Release
    // - `+0x08` = reserved word initialized to `0x2b`
    // - `+0x0a/+0x0b` = payload-byte-count prefix/header bytes
    // - `+0x0c .. +0x100b` = inline payload storage
    // Source keeps one trailing mirror vector only for convenience when higher-level C++ code wants
    // a `std::vector` view; the raw first `0x100c` bytes stay aligned with the current static RE.
    static constexpr uint16_t kMaxPayloadByteCount = 0x1000u;
    static constexpr uint16_t kBuilderReservedBytes08 = 0x002bu;  // anchor: launcher.exe:0x455bd0

    void** vtable00 = nullptr;
    volatile long referenceCount04 = 0;
    uint16_t reservedBytes08 = kBuilderReservedBytes08;
    uint8_t payloadLengthHigh0a = 0;
    uint8_t payloadLengthLow0b = 0;
    std::array<uint8_t, 0x1000> payloadBytes0c{};

    // Source-owned mirror/view for higher-level helpers. Keep it synchronized with the raw inline
    // payload above instead of treating the vector as the primary object layout.
    std::vector<uint8_t> payloadBytesFrom0c;

    void InitializeRawLayoutScaffold();
    void SyncRawPayloadFromMirrorScaffold();
    void SyncMirrorPayloadFromRawScaffold();
    void ResetForPacketBuilderScaffold();
    void ResetPayloadByteCountScaffold(uint16_t payloadByteCount);
    uint16_t GrowPayloadByteCountScaffold(uint16_t additionalByteCount);
    uint16_t PayloadByteCountScaffold() const;
    uint16_t RemainingAppendableByteCountScaffold() const;
    uint8_t* PayloadBaseScaffold();
    const uint8_t* PayloadBaseScaffold() const;
    std::vector<uint8_t> BuildFramedBytesFrom0aScaffold() const;
};

static_assert(offsetof(CMessageConnectionMessageScaffold, payloadBytes0c) == 0x0c, "message storage payload offset mismatch");

struct CMessageConnectionReceivedMessageRefScaffold {
    // Recovered outer receive/message-ref object created by `0x455cd0/0x455c60` and consumed by
    // later helpers such as `0x41bc20`, `0x41bbb0`, callback84, and the client-side raw-`0x38`
    // wrapper path.
    // Current best raw-layout read:
    // - `+0x00` = vtable (`0x4ba23c` on fresh builder-owned objects)
    // - `+0x04` = refcount dword used with shared `0x42f850/0x42f860`-style AddRef/Release
    // - `+0x08` = ctor/reset argument preserved by `0x455bd0`
    // - `+0x0c` = inner message-storage pointer
    // - `+0x10` = headerless/non-headerless flag written by `0x4490c0`
    // - `+0x14` = extra context dword passed as the second `0x455cd0` argument
    // - `+0x18/+0x1c/+0x20` = cleared on fresh create
    // Source keeps ownership trailing the raw `0x24` front matter so the callback/client-visible
    // face can stay closer to the original object instead of inventing a separate crash-only shim.
    void** vtable00 = nullptr;
    volatile long referenceCount04 = 0;
    uint32_t field08 = 0;
    CMessageConnectionMessageScaffold* messageStorage0c = nullptr;
    uint8_t headerless10 = 0;
    uint8_t padding11_13[3] = {0u, 0u, 0u};
    uint32_t messageContext14 = 0;
    uint32_t field18 = 0;
    uint32_t field1c = 0;
    uint32_t field20 = 0;
    std::shared_ptr<CMessageConnectionMessageScaffold> messageStorageOwnerScaffold24;

    void InitializeRawLayoutScaffold(uint32_t field08Value = 0u);
    void SyncRawMessageStoragePointerScaffold();
};

static_assert(offsetof(CMessageConnectionReceivedMessageRefScaffold, messageStorageOwnerScaffold24) == 0x24, "message-ref ownership tail offset mismatch");

enum class CMessageConnectionPacketNameFamilyScaffold : uint8_t {
    kUnknown = 0,
    kAuth = 1,
    kMargin = 2,
};

struct CMessageConnectionEnvelopeScaffold {
    // Source-owned bridge for the original local envelope family forwarded by `0x41af70`
    // through `0x41cf30` into `0x448cf0`.
    // Current best narrowed material fields:
    // - original envelope `+0x08` = shared message object
    // - original envelope `+0x10` = headerless/send-mode flag consumed by `0x448cf0`
    std::shared_ptr<CMessageConnectionMessageScaffold> sharedMessage;
    uint8_t headerless10 = 0;
};

struct CMessageConnectionPacketAgendaScaffold {
    // Source-owned mirror of the lazy packet-processing agenda object rooted at original
    // connection `+0x74`.
    // Current best narrowed shape from `0x448980 -> 0x469b10 -> 0x469850 -> 0x469740`:
    // - object is lazy-created, not constructor-owned
    // - object has distinct read/write helper sides
    // - read side always has an embedded default pass-through helper at agenda `+0x0c`
    // - receive path `0x4490c0` consults the read side through `0x469930`, which returns agenda
    //   read-output slot `+0x08`, and then swaps that message-ref through `0x4489d0` before leaf
    //   dispatch
    // - send path `0x448cf0` consults the write side through `0x469950`, which returns agenda
    //   write-output slot `+0x24`, and may replace/discard the outgoing envelope when an active
    //   write helper exists at agenda `+0x44`
    bool created = false;
    bool hasEmbeddedDefaultReadPassThroughHelper0c = false;
    // Current source meaning of these counts:
    // - they track the known caller-installed helper pair forwarded through `0x469740`
    // - they do not count the embedded default read pass-through helper at agenda `+0x0c`
    uint32_t configuredReadHelperCount = 0;
    uint32_t configuredWriteHelperCount = 0;
    bool hasReadOutputSlot08 = false;
    CMessageConnectionReceivedMessageRefScaffold readOutputSlot08;
    bool hasWriteOutputSlot24 = false;
    CMessageConnectionEnvelopeScaffold writeOutputSlot24;
};

struct CMessageConnectionReceivedPacketScaffold {
    std::vector<uint8_t> payloadBytes;
    bool headerless = false;
};

class CMessageConnection : public CLTTCPConnection {
public:
    // UNANCHORED: source-owned narrow subset of `0x448b40` with a null engine and without the
    // optional `+0x7c/+0x80` completion-helper allocation path.
    CMessageConnection();
    // UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
    // Current source body only seeds the recovered base `+0x10` engine field and does not model
    // the original parser allocation or optional completion-helper ownership.
    explicit CMessageConnection(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned default destructor; the original family uses several concrete deleting-dtor paths.
    ~CMessageConnection();

    // UNANCHORED: source-owned compatibility pass-through over the recovered base-connection
    // `+0x10` engine field; no separate leaf-owned engine slot is evidenced here.
    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered base-connection `+0x10`
    // engine field; no separate leaf-owned engine slot is evidenced here.
    CLTThreadPerClientTCPEngine* Engine() const;

    // UNANCHORED: source-owned wrapper over base `CLTTCPConnection::Connect` / engine slot 6.
    // This keeps the connection-object-oriented call site out of diagnostics.cpp.
    uint32_t EnsureConnected();

    // UNANCHORED: source-owned payload-span submit wrapper beneath the original
    // `CMessageConnection::SendPacket` envelope family.
    // Current best original `0x448cf0` consumes a message/envelope object, performs packet-agenda
    // filtering, then reaches lower submit helper `0x448a00`.
    uint32_t SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext = nullptr);

    // Source-owned launcher-only bridge for the narrowed state8/state10/state11 send-authenticity
    // gap. It preserves the now-recovered envelope/message split from
    // `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00` without pretending the full original shared
    // message-object shape is already recovered.
    // Important current narrowing from original-launcher WineDbg at the natural first state8 send:
    // - margin connection `+0x70` was live (`0x41ce40`)
    // - connection `+0x74` was still null on that send
    // - submitted payload length there was `0x13b`, not the current replacement `0x0bb`
    // So the remaining blocker is no longer best framed as agenda presence alone; the stronger
    // current gap is the richer original message-object content that our scaffold still does not
    // build.
    // UNANCHORED: source-owned launcher-only bridge for the local envelope builder seam.
    static CMessageConnectionEnvelopeScaffold BuildPacketBuilderEnvelopeScaffold(bool headerless = false);
    // UNANCHORED: source-owned launcher-only bridge that wraps raw payload bytes in the local envelope scaffold.
    static CMessageConnectionEnvelopeScaffold BuildPayloadEnvelopeScaffold(
        const void* packetData,
        uint32_t packetByteCount,
        bool headerless = false);
    // anchor: launcher.exe:0x448cf0
    // Narrow source-owned mirror of the envelope-based `CMessageConnection::SendPacket` family.
    // Original input is a local envelope / shared-message object, not a bare byte span.
    // Current bounded send-side tightening also preserves the nearer packet-agenda write seam:
    // - if connection `+0x74` exists, `0x448cf0` consults `0x469950` before submit
    // - source now keeps that handoff shape explicit even though helper-side replacement/discard is
    //   still pass-through-only in source
    uint32_t SendPacketEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope);

    // anchor: launcher.exe:0x448960
    // Narrow source-owned wrapper over the per-connection packet-name callback configuration:
    // - original writes connection `+0x78 = enabled`
    // - when enabled, original writes connection `+0x70 = callback`
    // Current source API still accepts a family enum, but immediately maps that family to the
    // currently known callback bodies (`0x41ce00` auth, `0x41ce40` margin).
    void ConfigurePacketNameFamilyScaffold(
        CMessageConnectionPacketNameFamilyScaffold family,
        bool packetizedMessagesEnabled);
    // UNANCHORED: source-owned diagnostic family view derived from the callback-address scaffold.
    CMessageConnectionPacketNameFamilyScaffold PacketNameFamilyScaffold() const;
    // UNANCHORED: source-owned accessor for the packetized-messages enable scaffold byte.
    bool PacketizedMessagesEnabledScaffold() const;

    // anchor: launcher.exe:0x448980
    // Narrow source-owned mirror of the lazy packet-agenda install/configure helper at
    // connection `+0x74`.
    // Current best read of the original helper:
    // - lazily allocate the `+0x74` agenda object on first use
    // - always forward the caller-supplied agenda/config pointer into the helper family rooted at
    //   `0x469740`
    // Current source model keeps only the lazy creation/ownership fact explicit.
    void ConfigurePacketAgendaScaffold(const void* agendaConfiguration = nullptr);
    // UNANCHORED: source-owned accessor for the lazy packet-agenda scaffold pointer.
    const CMessageConnectionPacketAgendaScaffold* PacketAgendaScaffold() const;

    // anchor: launcher.exe:0x4490c0
    // string-backed original name: CMessageConnection::OnOperationCompleted
    // current best read:
    // - main completion/receive-side bridge back into engine/queue handling
    // - work type `2` first tries optional completion helper `+0x7c`; on the launcher startup path
    //   that then falls through into the leaf owner-callback wrapper
    // - work type `3` copies packet-body bytes out of the retained-fragment-backed
    //   `CParsedPacketWorkItem` via `+0x24/+0x28` into a local receive/message-ref scaffold
    //   built on the same outer-ref/inner-storage split used by `0x455cd0/0x455c60`
    // - source now also mirrors one narrower original post-copy step there:
    //   - keep the original headerless locator-id validity gate on that receive/message-ref
    //   - read message codes through the nearer `0x41bc20/0x41bbb0`-style object path before leaf dispatch
    // - generic fallback logger on this path is helper `0x448a60`
    // Current source gap kept explicit:
    // - source now owns the copied-packet staging subset and two later leaf post-copy destinations:
    //   - auth: optional `connection+0x74 -> 0x469930 -> 0x4489d0` pass-through handoff,
    //     then local message-ref/base-filter step -> `0x449a30 -> owner+0x180 / 0x41f250`
    //   - margin: optional `connection+0x74 -> 0x469930 -> 0x4489d0` pass-through handoff,
    //     then `0x44af20 -> 0x442d00 -> owner+0x184 / 0x41f260`
    // - later original agenda helper transformation/discard behavior from this same callback is
    //   still incomplete, so the launcher bridge keeps one extra synthetic receive-drain proxy item
    //   for the remaining unconsumed paths
    uint32_t OnOperationCompleted(void* workItem);

    // UNANCHORED: source-owned accessor exposing the current copied packet-body bytes from the
    // narrowed `0x4490c0` type-3 path.
    const std::vector<uint8_t>& LastReceivedPacketBodyBytesScaffold() const;
    // UNANCHORED: source-owned accessor exposing the current headerless/packetized split narrowed
    // from the `0x4490c0` message-ref flag write.
    bool LastReceivedPacketHeaderlessScaffold() const;
    // UNANCHORED: source-owned queue-drain helper exposing copied parsed-packet bodies from the
    // narrowed `0x4490c0` type-3 path.
    bool TakeNextReceivedPacketScaffold(
        std::vector<uint8_t>* outPayloadBytes,
        bool* outHeaderless = nullptr);

    // UNANCHORED: source-owned helper mirroring the current queue producer context-key shape.
    // Current best reading: queue0C often receives (workItem, this, 0) from this class.
    void* ContextKey() { return this; }

protected:
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x2c`
    // Source-owned post-copy dispatch seam beneath the narrowed type-3 receive path.
    // Current bounded use:
    // - base `0x4490c0` now still owns the copied-packet extraction
    // - leaf families now receive a nearer outer-ref/inner-storage scaffold instead of a naked
    //   byte vector, matching the real `0x455cd0 -> 0x41bc20/0x41bbb0` seam more closely
    // - leaf families can optionally source-own one later dispatch destination without pretending
    //   the full original message-object / agenda tail is already reconstructed
    // - returning non-zero means the leaf consumed this copied packet without needing the pending
    //   copied-packet queue / later synthetic receive-drain proxy
    virtual uint32_t DispatchCopiedParsedPacketTailScaffold(
        void* workItem,
        CMessageConnectionReceivedMessageRefScaffold& messageRef);

private:
    // UNANCHORED: source-owned packet-family name helper for current diagnostics.
    static const char* PacketNameFamilyToString(CMessageConnectionPacketNameFamilyScaffold family);
    // UNANCHORED: source-owned helper that maps the diagnostic family enum onto the currently known
    // original callback bodies stored at connection `+0x70`.
    static uintptr_t PacketNameCallbackAddressScaffold(CMessageConnectionPacketNameFamilyScaffold family);
    // UNANCHORED: source-owned send-side packet-agenda handoff helper.
    // Current bounded model preserves the nearer `0x469950` shape:
    // - no active write helper => keep the original envelope
    // - active write helper => preserve the handoff/swap seam but currently pass the same envelope
    //   through until helper-side transformation/discard is recovered
    bool ApplySendPacketAgendaScaffold(
        const CMessageConnectionEnvelopeScaffold& inputEnvelope,
        CMessageConnectionEnvelopeScaffold* outputEnvelope,
        bool* outAgendaTouched);
    // UNANCHORED: source-owned lower submit helper beneath SendPacketEnvelopeScaffold.
    // Current best original helper is `0x448a00`.
    uint32_t SubmitEnvelopeBytesScaffold(const CMessageConnectionEnvelopeScaffold& envelope);

    uintptr_t packetNameCallbackScaffold_ = 0;
    bool packetizedMessagesEnabledScaffold_ = false;
    std::unique_ptr<CMessageConnectionPacketAgendaScaffold> packetAgendaScaffold_;
    std::vector<uint8_t> lastReceivedPacketBodyBytesScaffold_;
    bool lastReceivedPacketHeaderlessScaffold_ = false;
    std::vector<CMessageConnectionReceivedPacketScaffold> pendingReceivedPacketsScaffold_;
};

// ============================================================
// CAuthStartupConnection class declaration
// ============================================================
// Source-owned leaf mirror of the auth-side startup child built at `0x41d170` and assigned
// vtable `0x004afef0` before the initial `connection->+0x1c(owner+0x5c)` call.
// Keep the class name conservative in source for now:
// - the surrounding canonical docs still carry older naming on `0x004afef0`
// - but current static RE is strong that this is the auth-side leaf completion wrapper reached
//   through `0x449a70`, not just a generic base `CMessageConnection`
class CAuthStartupConnection : public CMessageConnection {
public:
    // anchor: launcher.exe:0x41cf50
    CAuthStartupConnection();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CAuthStartupConnection(CLTThreadPerClientTCPEngine* authEngine);
    ~CAuthStartupConnection();

    // anchor: launcher.exe:0x449a70
    // Current best original order:
    // - call base `0x4490c0`
    // - if base returns 0, call owner `+0x17c(this, workItem)`
    // - if that also returns 0, fall through to `0x448a60`
    // - if work type == 1, tears down through the connection object
    uint32_t OnOperationCompleted(void* workItem) override;

protected:
    // anchor: launcher.exe:0x449a30 -> owner vtable `+0x180` / `0x41f250`
    // Current bounded auth-side correction:
    // - after base `0x4490c0` finishes the parsed-packet copy, the auth leaf now receives the
    //   nearer local receive/message-ref scaffold and runs the narrow `0x442d00` consumed-code
    //   filter through the same `0x41bc20/0x41bbb0`-style object read path
    // - only the surviving auth path then re-enters the current helper's slot-5
    //   `AuthMessageDispatch` path through owner `+0x180`
    // - source still does not materialize the full original refcounted message object / agenda tail
    uint32_t DispatchCopiedParsedPacketTailScaffold(
        void* workItem,
        CMessageConnectionReceivedMessageRefScaffold& messageRef) override;
};

// ============================================================
// CMarginConnection class declaration
// ============================================================
struct CMarginConnectionLocalCompletionWorkItemScaffold {
    // anchor: launcher.exe:0x434ce0 -> 0x464870 / 0x4444e0
    // Minimal local stack work-item shape recovered for the `0x441850` continuation:
    // - `+0x00` = vtable pointer (type-specific local completion object)
    // - `+0x04` = work-item type / LaunchPadClient_GetVtableOffset() result
    // - `+0x08` = status / payload dword read back through `0x434d00`
    CLTThreadPerClientTCPEngine_WorkItemHeader header{};
    uint32_t workPayload = 0u;
};

static_assert(sizeof(CMarginConnectionLocalCompletionWorkItemScaffold) == 0x0c, "margin code-4 local completion work-item size mismatch");

struct CMarginConnectionBootstrapPrepStateA0Scaffold {
    // anchor: launcher.exe:0x443340 -> helper object stored at connection `+0xa0`
    // Bounded source-owned mirror of the owner `+0x680` prep object payload seeded from child
    // `+0xb0/+0xc4/+0xd8` before the later raw type-1 state5 send.
    std::array<uint8_t, 0x14> blockB0{};
    std::array<uint8_t, 0x14> blockC4{};
    std::array<uint8_t, 0x14> blockD8{};
};

class CMarginConnection : public CMessageConnection {
public:
    // UNANCHORED: source-owned narrow leaf ctor over the `0x41cf80 -> 0x448b40` family.
    CMarginConnection();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine);
    // UNANCHORED: source-owned default destructor; the original family uses `0x41ce80` cleanup
    // after restoring the shared base-margin vtable.
    ~CMarginConnection();

    // UNANCHORED: source-owned compatibility pass-through over the recovered base `+0x10` engine
    // field; no separate `CMarginConnection` engine slot is evidenced.
    void SetMarginEngine(CLTThreadPerClientTCPEngine* marginEngine);
    // UNANCHORED: source-owned compatibility accessor over the recovered base `+0x10` engine
    // field; no separate `CMarginConnection` engine slot is evidenced.
    CLTThreadPerClientTCPEngine* MarginEngine() const;

    // anchor: launcher.exe:0x441850
    // Narrow source-owned mirror of the consumed decoded-code-4 side effect that sets connection
    // byte `+0x84` when the inner status dword is zero.
    void SetMessageCode4SuccessFlag84Scaffold(bool value);
    // UNANCHORED: source-owned diagnostic accessor for the same narrowed `+0x84` mirror.
    bool MessageCode4SuccessFlag84Scaffold() const;
    // anchor: launcher.exe:0x441850
    // Source-owned local type-`0x0b` continuation scaffold routed back through connection vtable
    // `+0x10` / `OnOperationCompleted(workItem)`.
    uint32_t DispatchMessageCode4LocalCompletionWorkItemScaffold(uint32_t workPayloadStatus);
    // anchor: launcher.exe:0x41ce80 -> connection `+0x98`
    // Source-owned mirror of the margin connection helper that stores a copied `0x136` auth-reply
    // shadow block for the later state5/state6 raw type-1 send path.
    bool StoreBootstrapReplyCopy98Scaffold(const void* bytes, size_t byteCount);
    // anchor: launcher.exe:0x443340 -> connection `+0xa0`
    // Source-owned mirror of the owner `+0x680` prep-object adoption from child
    // `+0xb0/+0xc4/+0xd8` that `0x41b500` performs before the later raw type-1 send.
    bool StoreBootstrapPrepStateA0Scaffold(
        const void* blockB0,
        const void* blockC4,
        const void* blockD8,
        size_t blockByteCount);
    // anchor: launcher.exe:0x41f30
    // Source-owned mirror of the later raw type-1 send that:
    // - writes prefix bytes `01 00 00`
    // - then reserves the copied reply span through the same `0x43a230(0x136)` helper shape used
    //   by the original local envelope builder
    // - then copies the stored `+0x98` reply-derived `0x136` block into that returned tail span
    // - then forwards the completed envelope through connection vtable `+0x24`
    uint32_t SendStoredBootstrapReplyCopy98Scaffold();
    // anchor: launcher.exe:0x442d00 code-5 branch -> connection `+0x85 .. +0x94`
    // Narrow source-owned mirror of the consumed decoded-code-5 16-byte writeback.
    void SetMessageCode5SeedBytes85Scaffold(const std::array<uint8_t, 16>& value);
    // UNANCHORED: source-owned copy-out accessor for the same narrowed code-5 writeback.
    bool CopyMessageCode5SeedBytes85Scaffold(std::array<uint8_t, 16>* outValue) const;
    // UNANCHORED: source-owned raw-pointer accessor for the same narrowed code-5 writeback.
    const uint8_t* MessageCode5SeedBytes85PointerScaffold() const;

    // anchor: launcher.exe:0x44af60
    // Later leaf override on top of the base `CMessageConnection::OnOperationCompleted` family.
    // Current best original order:
    // - call base `0x4490c0`
    // - if base returns 0, call owner `+0x188(this, workItem)`
    // - if that also returns 0, fall through to `0x448a60`
    // - if work type == 1, clear owner byte `+0xf14` then tear down through the connection object
    uint32_t OnOperationCompleted(void* workItem) override;

    // anchor: launcher.exe:0x44af20
    // Later leaf dispatch override on top of the unmodeled `CBaseMarginConnection` dispatch family.
    // Current best original order:
    // - call `CBaseMarginConnection::DispatchMessage(this, messageRef)` (`0x442d00`)
    // - if that returns 0, call owner `+0x184(messageRef)`
    uint32_t DispatchMessage(void* messageRef);

protected:
    // anchor: launcher.exe:0x44af20 -> 0x442d00 -> owner vtable `+0x184` / `0x41f260`
    // Current bounded margin-side correction:
    // - after base `0x4490c0` finishes the parsed-packet copy, the margin leaf can now re-enter
    //   the nearer base-dispatch/current-helper-slot6 path directly from the connection callback
    // - consumed `0x442d00` branches are now pulled one step closer too:
    //   - decoded code `2` can now re-enter the launcher-owned bootstrap continuation directly at
    //     the connection/leaf seam
    //   - decoded code `4` mirrors the narrower `0x441850` side effect locally before that same
    //     continuation
    // - current source still does not materialize the full original message-ref object / agenda
    //   tail, but this seam now receives the nearer local outer-ref/inner-storage scaffold instead
    //   of a naked payload vector before mirroring `0x44af20`
    uint32_t DispatchCopiedParsedPacketTailScaffold(
        void* workItem,
        CMessageConnectionReceivedMessageRefScaffold& messageRef) override;

private:
    bool messageCode4SuccessFlag84Scaffold_ = false;
    bool hasBootstrapReplyCopy98Scaffold_ = false;
    std::array<uint8_t, 0x136> bootstrapReplyCopy98Scaffold_{};
    std::unique_ptr<CMarginConnectionBootstrapPrepStateA0Scaffold> bootstrapPrepStateA0Scaffold_;
    bool hasMessageCode5SeedBytes85Scaffold_ = false;
    std::array<uint8_t, 16> messageCode5SeedBytes85Scaffold_{};
};

}  // namespace mxo::liblttcp
