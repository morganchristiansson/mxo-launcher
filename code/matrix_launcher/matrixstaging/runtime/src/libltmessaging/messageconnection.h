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
//   - consumes the retained outer message-ref object from local envelope `+0x08`, performs
//     packet-agenda filtering, then reaches the lower submit helper `0x448a00`
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
    // Current source now treats that raw `+0x0a/+0x0b/+0x0c..` front matter as the primary truth and
    // only materializes vector copies on demand from those bytes.
    static constexpr uint16_t kMaxPayloadByteCount = 0x1000u;
    static constexpr uint16_t kBuilderReservedBytes08 = 0x002bu;  // anchor: launcher.exe:0x455bd0

    void** vtable00 = nullptr;
    volatile long referenceCount04 = 0;
    uint16_t reservedBytes08 = kBuilderReservedBytes08;
    uint8_t payloadLengthHigh0a = 0;
    uint8_t payloadLengthLow0b = 0;
    std::array<uint8_t, 0x1000> payloadBytes0c{};

    // anchor: launcher.exe:0x455bd0 / 0x455c60 / 0x455cd0
    void ResetForPacketBuilderScaffold();
    // UNANCHORED: source-owned raw byte-count reset helper over recovered `+0x0a/+0x0b/+0x0c..` storage.
    void ResetPayloadByteCountScaffold(uint16_t payloadByteCount);
    // anchor: launcher.exe:0x4557b0
    uint16_t GrowPayloadByteCountScaffold(uint16_t additionalByteCount);
    // UNANCHORED: source-owned raw payload-length decoder from `+0x0a/+0x0b`; clamp, do not bit-mask.
    uint16_t PayloadByteCountScaffold() const;
    // UNANCHORED: source-owned capacity helper over the recovered raw payload-storage layout.
    uint16_t RemainingAppendableByteCountScaffold() const;
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    uint8_t* PayloadBaseScaffold();
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    const uint8_t* PayloadBaseScaffold() const;
};

static_assert(offsetof(CMessageConnectionMessageScaffold, payloadBytes0c) == 0x0c, "message storage payload offset mismatch");

struct CMessageConnectionReceivedMessageRefScaffold {
    // Recovered outer live message-ref object created by `0x455cd0/0x455c60`.
    // Current confirmed consumers now include both:
    // - receive-side helpers such as `0x4490c0`, `0x41bc20`, `0x41bbb0`, callback84, and the
    //   client-side raw-`0x38` wrapper path
    // - send-side packet builders that retain this object at local envelope `+0x08` before
    //   `0x41cf30 -> 0x448cf0`
    // Current best raw-layout read:
    // - `+0x00` = vtable (`0x4ba23c` on fresh builder-owned objects)
    // - `+0x04` = refcount dword used with shared `0x42f850/0x42f860`-style AddRef/Release
    // - `+0x08` = ctor/reset argument preserved by `0x455bd0`
    // - `+0x0c` = inner message-storage pointer
    // - `+0x10` = send-mode/headerless flag consumed by `0x448cf0` and written by `0x4490c0`
    // - `+0x14` = extra context dword passed as the second `0x455cd0` argument
    // - `+0x18/+0x1c/+0x20` = cleared on fresh create
    // Current source keeps one inline local inner-storage tail after the recovered raw `0x24`
    // front matter and rebinds raw `+0x0c` to that tail on the local scaffold instead of treating
    // a source-owned shared_ptr identity as the primary message-ref shape.
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
    CMessageConnectionMessageScaffold ownedMessageStorageScaffold24{};

    CMessageConnectionReceivedMessageRefScaffold() = default;
    // anchor: launcher.exe:0x455cd0 / 0x455c60
    void ResetForPacketBuilderScaffold(bool headerless, uint32_t messageContext14 = 0u);
    // Source note: keep the local outer scaffold noncopyable so raw `+0x0c` never silently keeps a
    // pointer into another instance's inline `+0x24` tail.
    CMessageConnectionReceivedMessageRefScaffold(const CMessageConnectionReceivedMessageRefScaffold&) = delete;
    CMessageConnectionReceivedMessageRefScaffold& operator=(const CMessageConnectionReceivedMessageRefScaffold&) = delete;
    CMessageConnectionReceivedMessageRefScaffold(CMessageConnectionReceivedMessageRefScaffold&&) = delete;
    CMessageConnectionReceivedMessageRefScaffold& operator=(CMessageConnectionReceivedMessageRefScaffold&&) = delete;
};

static_assert(offsetof(CMessageConnectionReceivedMessageRefScaffold, ownedMessageStorageScaffold24) == 0x24, "message-ref local-storage tail offset mismatch");

using CMessageConnectionMessageRefScaffold = CMessageConnectionReceivedMessageRefScaffold;

enum class CMessageConnectionPacketNameFamily : uint8_t {
    kUnknown = 0,
    kAuth = 1,
    kMargin = 2,
};

struct CMessageConnectionPacketBuilderEnvelope {
    // Recovered raw local packet-builder envelope front matter initialized by `0x439840` and
    // forwarded by `0x41af70 -> 0x41cf30`.
    // Current best material fields from `0x439840` / `0x41cf30`:
    // - `+0x00` = local helper vtable / derived builder vtable slot
    // - `+0x04` = payload base cached as `messageRef+0x0c -> inner+0x0c`
    // - `+0x08` = retained outer message-ref object consumed by `0x448cf0`
    void** vtable00 = nullptr;
    uint8_t* payloadBase04 = nullptr;
    CMessageConnectionMessageRefScaffold* messageRef08 = nullptr;
};

static_assert(offsetof(CMessageConnectionPacketBuilderEnvelope, messageRef08) == 0x08, "packet-builder envelope message-ref offset mismatch");

class CStreamPacketEncryptionOwnerBase {
public:
    // anchor: launcher.exe vtable `0x004b81dc`
    // Source-owned full virtual C++ mirror of the owner-side base. This family is only used by
    // our own code, so we intentionally let the compiler own the C++ vptr instead of storing a
    // fake raw `vtable00` field.
    virtual ~CStreamPacketEncryptionOwnerBase() = default;
    virtual const char* ClassName() const { return "CStreamPacketEncryptionOwnerBase"; }
};

class CStreamPacketEncryptionHelperBase {
public:
    // anchor: launcher.exe vtable `0x004b81c8`
    // Source-owned full virtual C++ mirror of the helper-side base shared by:
    // - module read helper `0x004b86f0`
    // - module write helper `0x004b8690`
    // - embedded agenda helper `0x004baf48`
    virtual ~CStreamPacketEncryptionHelperBase() = default;
    virtual const void* Descriptor() const {
        return reinterpret_cast<const void*>(0x004aafbbu);
    }
    virtual void HandleOpaqueMessageRef(void* opaqueMessageRef) = 0;

    CStreamPacketEncryptionHelperBase* nextHelper04 = nullptr;

protected:
    void ForwardToNextHelper(void* opaqueMessageRef);
};

class CStreamPacketEncryptionModule;

class CStreamPacketEncryptionModuleHelper : public CStreamPacketEncryptionHelperBase {
public:
    // Current common source-owned state on the two module child helpers.
    // `0x469740` gives the tighter role for `nextHelper04`:
    // - read helper (`0x004b86f0`) links to the previous agenda read-chain head there
    // - write helper (`0x004b8690`) links to the embedded/default agenda write helper there
    CStreamPacketEncryptionModule* owner08 = nullptr;
};

class CStreamPacketEncryptionModuleReadHelper
    : public CStreamPacketEncryptionModuleHelper {
public:
    // anchor: launcher.exe vtable `0x004b86f0`
    // Current best recovered role:
    // - owner `+0x04`
    // - inserted at agenda `+0x40` by `0x469740`
    // - therefore the concrete module-side **read helper**
    uint32_t collectionControl0c = 0;
    void* collectionBegin10 = nullptr;
    void* collectionEnd14 = nullptr;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule* owner);
};

class CStreamPacketEncryptionModuleWriteHelper
    : public CStreamPacketEncryptionModuleHelper {
public:
    // anchor: launcher.exe vtable `0x004b8690`
    // Current best recovered role:
    // - owner `+0x08`
    // - inserted at agenda `+0x44/+0x48` by `0x469740`
    // - therefore the concrete module-side **write helper**
    void* embeddedTransformAdapter0c = nullptr;
    void* embeddedTransformAdapterMeta10 = nullptr;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule* owner);
};

class CStreamPacketEncryptionAgendaHelper : public CStreamPacketEncryptionHelperBase {
public:
    // anchor: launcher.exe vtable `0x004baf48`
    // Source-owned full virtual C++ mirror of the embedded agenda helper object materialized twice
    // inside the packet agenda by `0x469850`:
    // - agenda `+0x0c` = embedded/default read helper
    // - agenda `+0x28` = embedded/default write helper
    uint32_t field04 = 0;
    uint32_t field08 = 0;
    const char* helperLabel0c = nullptr;
    void** outputSlotAddress10 = nullptr;
    CStreamPacketEncryptionHelperBase** downstreamHelperSlot14 = nullptr;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForAgenda(
        const char* helperLabel,
        void** outputSlotAddress,
        CStreamPacketEncryptionHelperBase** downstreamHelperSlot);
};

class CStreamPacketEncryptionModule : public CStreamPacketEncryptionOwnerBase {
public:
    // anchor: launcher.exe:0x44da00 / vtable `0x004b8704`
    // Named owner/aggregator recovered from:
    // - `0x44dab0 = CStreamPacketEncryptionModule_GetClassName`
    // - literal string `"CStreamPacketEncryptionModule"`
    // Current tighter integration from `0x469740`:
    // - `readHelper04` = module **read helper** rooted at `0x004b86f0`
    // - `writeHelper08` = module **write helper** rooted at `0x004b8690`
    // - `nextConfiguredModule0c` = next configured module in the agenda-owned module list
    // - `configuredAgendaIdentity10` = source-owned agenda identity copied during installation
    CStreamPacketEncryptionModuleReadHelper* readHelper04 = nullptr;
    CStreamPacketEncryptionModuleWriteHelper* writeHelper08 = nullptr;
    CStreamPacketEncryptionModule* nextConfiguredModule0c = nullptr;
    const void* configuredAgendaIdentity10 = nullptr;
    CStreamPacketEncryptionModuleReadHelper ownedReadHelper14{};
    CStreamPacketEncryptionModuleWriteHelper ownedWriteHelper2c{};
    std::array<uint8_t, 16> associatedSeedBytes40{};

    const char* ClassName() const override { return "CStreamPacketEncryptionModule"; }
    void ResetForMarginConnectionSeed(const std::array<uint8_t, 16>& seedBytes85);
};

struct CMessageConnectionPacketAgenda {
    // Source-owned mirror of the lazy packet-processing agenda object rooted at original
    // connection `+0x74`.
    // The original raw offsets remain documented in the canonical docs, but because the embedded
    // helper/module family is now modeled as full internal-only C++ classes, this source mirror is
    // intentionally behavioral rather than raw-layout-exact.
    CStreamPacketEncryptionModule* configuredModuleList04 = nullptr;
    CMessageConnectionReceivedMessageRefScaffold* readOutputSlot08 = nullptr;
    CStreamPacketEncryptionAgendaHelper embeddedReadHelper0c{};
    CMessageConnectionMessageRefScaffold* writeOutputSlot24 = nullptr;
    CStreamPacketEncryptionAgendaHelper embeddedWriteHelper28{};
    // faithful raw-field naming from the recovered agenda object:
    // - `readHelperChainHead40` mirrors original agenda `+0x40`
    // - `writeHelperChainHead44` mirrors original agenda `+0x44`
    // - `writeHelperChainTail48` mirrors original agenda `+0x48`
    // Ghidra evidence from `0x469850 / 0x469740` shows `+0x44/+0x48` are real write-chain
    // head/tail pointers, not source-invented convenience state.
    CStreamPacketEncryptionHelperBase* readHelperChainHead40 = nullptr;
    CStreamPacketEncryptionHelperBase* writeHelperChainHead44 = nullptr;
    CStreamPacketEncryptionHelperBase* writeHelperChainTail48 = nullptr;
    uint16_t configuredModuleCount4c = 0;
    uint16_t reserved4e = 0;
    bool created = false;
    CStreamPacketEncryptionModule* configuredStreamPacketEncryptionModule = nullptr;
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

    // UNANCHORED: source-owned raw-byte send wrapper used by current auth/bootstrap direct-send
    // callsites.
    // Keep this distinct from the local packet-builder / message-ref path:
    // - `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00`
    // That original higher-level path consumes a message-ref object, not a bare byte span.
    uint32_t SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext = nullptr);

    // anchor: launcher.exe:0x41cf30
    // Wrapper immediately beneath mediator send helper `0x41af70`.
    // Original body extracts envelope `+0x08` and forwards that retained message-ref object into
    // vtable `+0x28` / `0x448cf0`.
    uint32_t ForwardPacketBuilderEnvelopeToSendPacket(
        CMessageConnectionPacketBuilderEnvelope& envelope);

    // anchor: launcher.exe:0x448cf0
    // Narrow source-owned mirror of the message-ref-based send path.
    // Current bounded send-side tightening preserves three concrete original facts:
    // - original input is the retained outer message-ref object, not the local packet-builder shell
    // - if connection `+0x74` exists, `0x448cf0` consults `0x469950` before submit
    // - the send-mode/headerless branch mutates raw inner bytes around `+0x12/+0x16/+0x17`, then
    //   later clears the first payload byte high bit on the original input object after the
    //   agenda/submit branch
    uint32_t SendPacketMessageRef(CMessageConnectionMessageRefScaffold& messageRef);

    // anchor: launcher.exe:0x448960
    // Narrow source-owned wrapper over the per-connection packet-name callback configuration:
    // - original writes connection `+0x78 = enabled`
    // - when enabled, original writes connection `+0x70 = callback`
    // Current source API still accepts a family enum, but immediately maps that family to the
    // currently known callback bodies (`0x41ce00` auth, `0x41ce40` margin).
    void ConfigurePacketNameFamily(
        CMessageConnectionPacketNameFamily family,
        bool packetizedMessagesEnabled);
    // UNANCHORED: source-owned diagnostic family view derived from the callback-address scaffold.
    CMessageConnectionPacketNameFamily PacketNameFamily() const;
    // UNANCHORED: source-owned accessor for the packetized-messages enable scaffold byte.
    bool PacketizedMessagesEnabled() const;

    // anchor: launcher.exe:0x448980
    // Narrow source-owned mirror of the lazy packet-agenda install/configure helper at
    // connection `+0x74`.
    // Current best read of the original helper:
    // - lazily allocate the `+0x74` agenda object on first use
    // - always forward the caller-supplied agenda/config pointer into the helper family rooted at
    //   `0x469740`
    // Current best named caller-owned config object on the margin code-2 path is now:
    // - `0x44da00 / vtable 0x004b8704 = CStreamPacketEncryptionModule`
    // - `0x469740` installs owner `+0x04` as the agenda read helper and owner `+0x08` as the
    //   agenda write helper
    // Current source model now keeps the recovered raw agenda/helper front matter plus the
    // installed named module pointer explicit, while still leaving helper-side
    // transformation/discard behavior conservative.
    void ConfigurePacketAgenda(
        CStreamPacketEncryptionModule* streamPacketEncryptionModule = nullptr);
    // UNANCHORED: source-owned accessor for the lazy packet-agenda scaffold pointer.
    const CMessageConnectionPacketAgenda* PacketAgenda() const;

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
    static const char* PacketNameFamilyToString(CMessageConnectionPacketNameFamily family);
    // UNANCHORED: source-owned helper that maps the diagnostic family enum onto the currently known
    // original callback bodies stored at connection `+0x70`.
    static uintptr_t PacketNameCallbackAddressScaffold(CMessageConnectionPacketNameFamily family);
    // UNANCHORED: source-owned send-side packet-agenda handoff helper.
    // Current bounded model preserves the nearer `0x469950` shape:
    // - no active write helper => keep the original message-ref pointer
    // - active write helper => preserve the agenda `+0x24` pointer handoff, but currently return
    //   the same message-ref pointer until helper-side transformation/discard is recovered
    CMessageConnectionMessageRefScaffold* ApplySendPacketAgenda(
        CMessageConnectionMessageRefScaffold& inputMessageRef,
        bool* outAgendaTouched);
    // UNANCHORED: source-owned lower submit helper beneath `0x448cf0`.
    // Current best original helper is `0x448a00`; source now computes the final byte pointer/size
    // directly from raw inner `+0x0a/+0x0b/+0x0c..` storage.
    uint32_t SubmitMessageRefBytes(const CMessageConnectionMessageRefScaffold& messageRef);

    uintptr_t packetNameCallback_ = 0;
    bool packetizedMessagesEnabled_ = false;
    std::unique_ptr<CMessageConnectionPacketAgenda> packetAgenda_;
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
    void SetMessageCode4SuccessFlag84(bool value);
    // UNANCHORED: source-owned diagnostic accessor for the same narrowed `+0x84` mirror.
    bool MessageCode4SuccessFlag84() const;
    // anchor: launcher.exe:0x441850
    // Source-owned local type-`0x0b` continuation scaffold routed back through connection vtable
    // `+0x10` / `OnOperationCompleted(workItem)`.
    uint32_t DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus);
    // anchor: launcher.exe:0x41ce80 -> connection `+0x98`
    // Source-owned mirror of the margin connection helper that stores a copied `0x136` auth-reply
    // shadow block for the later state5/state6 raw type-1 send path.
    bool StoreBootstrapReplyCopy98(const void* bytes, size_t byteCount);
    // anchor: launcher.exe:0x443340 -> connection `+0xa0`
    // Source-owned mirror of the owner `+0x680` prep-object adoption from child
    // `+0xb0/+0xc4/+0xd8` that `0x41b500` performs before the later raw type-1 send.
    bool StoreBootstrapPrepStateA0(
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
    uint32_t SendStoredBootstrapReplyCopy98();
    // anchor: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
    // Narrow source-owned mirror of the consumed decoded-code-2/5 seed-byte writeback.
    // Current tighter integration now also mirrors the neighboring lazy `+0x9c`
    // `CStreamPacketEncryptionModule` install/refresh trigger that the original code reaches from
    // the earlier code-2 path (`0x4429b0 -> 0x441470`).
    void SetMessageCode5SeedBytes85(const std::array<uint8_t, 16>& value);
    // UNANCHORED: source-owned copy-out accessor for the same narrowed code-5 writeback.
    bool CopyMessageCode5SeedBytes85(std::array<uint8_t, 16>* outValue) const;
    // UNANCHORED: source-owned raw-pointer accessor for the same narrowed code-5 writeback.
    const uint8_t* MessageCode5SeedBytes85Pointer() const;

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
    // anchor: launcher.exe:0x441470 / 0x44da00 / 0x44daf0
    // Source-owned mirror of the lazy connection `+0x9c` packet-agenda module install/refresh
    // reached from the consumed code-2 challenge path after `+0x85..+0x94` is available.
    void EnsureStreamPacketEncryptionModuleFromSeed85();

    bool messageCode4SuccessFlag84_ = false;
    bool hasBootstrapReplyCopy98_ = false;
    std::array<uint8_t, 0x136> bootstrapReplyCopy98_{};
    std::unique_ptr<CMarginConnectionBootstrapPrepStateA0Scaffold> bootstrapPrepStateA0_;
    bool hasMessageCode5SeedBytes85_ = false;
    std::array<uint8_t, 16> messageCode5SeedBytes85_{};
    std::unique_ptr<CStreamPacketEncryptionModule> streamPacketEncryptionModule9c_;
};

}  // namespace mxo::liblttcp
