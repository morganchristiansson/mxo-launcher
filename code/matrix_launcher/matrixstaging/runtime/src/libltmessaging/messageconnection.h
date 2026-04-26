#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <spdlog/spdlog.h>

#include <rsa.h>

// Forward declare CLTLoginMediator to avoid circular include
namespace mxo { namespace ltlogin { class CLTLoginMediator; } }
#include "../libltcrypto/auth_internal.h"
#include "../liblttcp/ltthreadperclienttcpengine.h"

// Forward declare crypto helper
class CryptoInitHelper_0x4b42bc;

// anchor: launcher.exe:0x004cb4d4
// Static lookup table for message offset calculations.
// Used by headerless message path to compute payload offset from descriptor byte.
// The descriptor byte at [messageStorage0c + 0xd] (= payloadBytes0c + 1) is split into two 3-bit indices:
// - high nibble: (descriptor >> 4) & 7
// - low nibble: descriptor & 7
// Payload size = lookup[high] + lookup[low] + 0x12
inline const uint32_t g_MessageOffsetLookupTable[8] = {
    0x00000011u,  // index 0
    0x00000004u,  // index 1
    0x00000010u,  // index 2
    0x0000000bu,  // index 3
    0x00000010u,  // index 4
    0x00000011u,  // index 5
    0x00000010u,  // index 6
    0x004AC434u,  // index 7 (pointer? unused in practice)
};





#ifdef DispatchMessage
#undef DispatchMessage
#endif

namespace mxo::liblttcp {

// Reimplementation note:
// This file mirrors the best current read of the queue0C context object family.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md

// Starter skeleton for the original CMessageConnection_0x4b7928-family object now visible in
// launcher.exe strings / vtable neighborhoods.
//
// Recovered source-file anchor:
// - `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`
//
// Current evidence-backed role:
// - `0x448b40` first constructs a base CLTTCPConnection-family object, then overwrites
//   its vtable to the CMessageConnection_0x4b7928-family vtable at `0x4b7928`
// - captures a CLTThreadPerClientTCPEngine_0x4b2768 pointer at +0x10
// - carries an endpoint copy at +0x24
// - carries the inherited parser pointer at +0x6c
// - Ghidra/OOAnalyzer fidelity note for the active receive path:
//   - keep `+0x00` as a real `vftptr_0x0` field, not an inline vtable struct member
//   - otherwise `0x4490c0` decompilation loses the inherited `remoteEndpoint24` field and starts
//     rendering endpoint copies as `&(this->vtable...).slot_36`-style artifacts instead of the
//     real connection `+0x24` endpoint block
// - participates in queue0C producer traffic by enqueueing (workItem, this, 0)
// - likely appears as the dequeued queue0C `context` on important consumer paths
// - string-backed methods include:
//   - CMessageConnection_0x4b7928::SendPacket
//   - CMessageConnection_0x4b7928::OnOperationCompleted
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
//     - current ctor-side proof from `0x41d170 / 0x41e500` now ties that `+0xa4` owner field
//       directly to the `CLTLoginMediator*`, not to a bridge/context record
//     - `+0x17c` itself is `0x41af80 = CLTLoginMediator_HandleAuthConnectionCompletionFallback`
//     - that fallback filters to owner `+0x18`, clears owner `+0x18` on type `1`, then re-enters
//       the current helper through raw vtable entry `+0x00` / slot 1
//     - practical auth-side consequence:
//       - type-2 connect status is not a separate slot-2/secondary-gate route here
//       - state1 slot 1 (`0x4390b0`) sees type `2`
//       - non-type-2 auth close work falls through the same callback into shared slot-1 gate
//         `0x438d80`
//     - so the concrete handling target still depends on the current helper selected through the
//       `0x4f7868` family and `0x41b450(...)`, not on one fixed owner-body function alone
//     - if that helper re-entry returns 0, `0x449a70` falls through to `0x448a60`, which is
//       string-backed only as a generic `Got unhandled op of type %d with status %s` logger
//     - only after that handled/unhandled decision does `0x449a70` test work type `1` and call
//       the deleting teardown path through vtable[0](1)
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
//   - vtable slot 10 / `0x448cf0` = `CMessageConnection_0x4b7928::SendPacket`
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
// └── CMessageConnection_0x4b7928-family base surface (`0x448b40`, `0x448cf0`, `0x4490c0`)
//     └── CBaseMarginConnection_0x4b64a8 (0x004b64a8)
//         └── CBasicMarginConnection (0x004afef0)
// Key leaf-only rows now relevant here:
// - 0x004afef0 = 0x0041cf50 scalar-deleting-dtor wrapper
// - 0x004aff00 = 0x00449a70 leaf `OnOperationCompleted` override
// - 0x004aff1c = 0x00449a30 leaf `DispatchMessage` override
// - 0x004aff34 = 0x0041ce80 connection `+0x98` reply-copy helper
// The source file does not yet model `CBasicMarginConnection` as its own class, so do not treat
// those leaf rows as base `CMessageConnection_0x4b7928` methods.

// ============================================================
// VTable 0x004aff38 - CMarginConnection
// ============================================================
// Later leaf on top of:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection_0x4b7928-family base surface
//     └── CBaseMarginConnection_0x4b64a8 (0x004b64a8)
//         └── CMarginConnection (0x004aff38)
// Key leaf-only rows:
// - 0x004aff38 = 0x0041cf80 scalar-deleting-dtor wrapper
// - 0x004aff48 = 0x0044af60 `OnOperationCompleted` override
// - 0x004aff64 = 0x0044af20 `DispatchMessage` override

// Important current limitation for this starter skeleton:
// - the recovered original engine entry on this path is more connection-object-oriented
//   than the placeholder engine signatures currently model
// - keep the names stable, but treat the exact live method signatures as still provisional

class CMessageConnectionLocalRefCountedBase {
public:
    virtual ~CMessageConnectionLocalRefCountedBase() = default;

    // anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
    virtual uint32_t AddRef() = 0;
    // anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
    virtual uint32_t Release() = 0;
    // The original heap families return objects to pools here. These internal source objects stay
    // stack/local, so derived classes keep this hook but use a non-deleting implementation.
    virtual void FinalRelease() = 0;
    // anchor: launcher.exe:0x42f880 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x10`
    virtual void ResetRefCount() = 0;
    // anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
    virtual void SetRefCountFromPtr(const volatile long* refCountSource) = 0;
};

class CMessageConnectionMessageStorage_0x4ba208 : public CMessageConnectionLocalRefCountedBase {
public:
    // anchor: launcher.exe vtable `0x004ba208`
    // Inner payload-storage leaf allocated by `0x455bd0` and retained through the outer message-ref
    // object at `+0x0c`.
    static constexpr uint16_t kMaxPayloadByteCount = 0x1000u;
    static constexpr uint16_t kBuilderReservedBytes08 = 0x002bu;  // anchor: launcher.exe:0x455bd0

    volatile long referenceCount04 = 0;
    uint16_t reservedBytes08 = kBuilderReservedBytes08;
    uint8_t payloadLengthHigh0a = 0;
    uint8_t payloadLengthLow0b = 0;
    // FIDELITY: Original assembly at 0x439840 computes:
    //   nopatchLauncherVersionValue04 = *(messageRef08 + 0xc) + 0xc
    // Where *(messageRef08 + 0xc) loads messageStorage0c pointer.
    // So payload starts at messageStorage0c + 0xc - inline array at offset 0xc.
    std::array<uint8_t, 0x1000> payloadBytes0c{};  // Inline array at offset 0xc

    uint32_t AddRef() override;
    uint32_t Release() override;
    // anchor: launcher.exe:0x455ad0 / vtable `0x004ba208 +0x0c`
    void FinalRelease() override;
    void ResetRefCount() override;
    void SetRefCountFromPtr(const volatile long* refCountSource) override;

    // anchor: launcher.exe:0x455bd0 inner-storage setup before outer `+0x0c` stores/AddRefs it
    void ResetForPacketBuilder();
    // UNANCHORED: source-owned deterministic reset helper that zero-fills the newly selected span
    // before committing the raw payload-length header bytes.
    void ResetPayloadByteCount(uint16_t payloadByteCount);
    // anchor: launcher.exe:0x4557b0 / shared raw length write behavior with `0x41bb60`
    void SetPayloadByteCountRaw(uint16_t payloadByteCount);
    // anchor: launcher.exe:0x4557b0
    uint16_t GrowPayloadByteCount(uint16_t additionalByteCount);
    // UNANCHORED: source-owned raw payload-length decoder from `+0x0a/+0x0b`, clamped to the
    // recovered `0x1000` payload ceiling.
    uint16_t PayloadByteCount() const;
    // UNANCHORED: source-owned capacity helper over the recovered raw payload-storage layout.
    uint16_t RemainingAppendableByteCount() const;
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    uint8_t* PayloadBase();
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    const uint8_t* PayloadBase() const;
};

class CMessageConnectionMessageRefBase_0x4ba220 : public CMessageConnectionLocalRefCountedBase {
public:
    // anchor: launcher.exe vtable `0x004ba220`
    // Reset-time/base outer message-ref table installed by `0x455bd0` before `0x455c60`
    // retables the created object to `0x004ba23c`.
    volatile long referenceCount04 = 0;
    uint32_t field08 = 0;
    CMessageConnectionMessageStorage_0x4ba208* messageStorage0c = nullptr;

    uint32_t AddRef() override;
    uint32_t Release() override;
    // `0x004ba220 +0x0c` is `purecall`, so this base class remains abstract at the final-release
    // slot just like the original reset-time/base table.
    void FinalRelease() override = 0;
    void ResetRefCount() override;
    void SetRefCountFromPtr(const volatile long* refCountSource) override;

    // anchor: launcher.exe:0x455bd0
    // Resets the base outer state, materializes a fresh inner storage object, and retains it
    // through `messageStorage0c`.
    void ResetBaseForPacketBuilder(uint32_t field08 = 0u);
    // anchor: launcher.exe:0x4557b0 / outer vtable `0x004ba220 +0x18`
    uint16_t GrowPayloadByteCount(uint16_t additionalByteCount);
    // anchor: launcher.exe:0x439820
    uint8_t* PayloadAppendPointer();
    // anchor: launcher.exe:0x41bb60
    bool SetPayloadByteCount(uint32_t payloadByteCount);
    // UNANCHORED: source-owned convenience wrapper over the inner payload-byte-count header.
    uint16_t PayloadByteCount() const;

protected:
    CMessageConnectionMessageStorage_0x4ba208 ownedMessageStorage_{};
};

class CMessageConnectionMessageRef_0x4ba23c : public CMessageConnectionMessageRefBase_0x4ba220 {
public:
    // anchor: launcher.exe vtable `0x004ba23c`
    // Live outer message-ref table produced by `0x455c60/0x455cd0`.
    uint8_t headerless10 = 0;
    uint8_t padding11_13[3] = {0u, 0u, 0u};
    uint32_t messageContext14 = 0;
    uint32_t field18 = 0;
    uint32_t field1c = 0;
    uint32_t field20 = 0;

    CMessageConnectionMessageRef_0x4ba23c() = default;

    // anchor: launcher.exe:0x455b80 / vtable `0x004ba23c +0x0c`
    void FinalRelease() override;

    // anchor: launcher.exe:0x455cd0 / 0x455c60
    // Source-local live-object initializer mirroring `CreateRef(Create(...), messageContext14)`.
    void ResetForPacketBuilder(bool headerless, uint32_t messageContext14 = 0u);

    // Source note: keep the local outer scaffold noncopyable so raw `messageStorage0c` never
    // silently keeps a pointer into another instance's inline owned-storage tail.
    CMessageConnectionMessageRef_0x4ba23c(const CMessageConnectionMessageRef_0x4ba23c&) = delete;
    CMessageConnectionMessageRef_0x4ba23c& operator=(const CMessageConnectionMessageRef_0x4ba23c&) = delete;
    CMessageConnectionMessageRef_0x4ba23c(CMessageConnectionMessageRef_0x4ba23c&&) = delete;
    CMessageConnectionMessageRef_0x4ba23c& operator=(CMessageConnectionMessageRef_0x4ba23c&&) = delete;
};

// anchor: launcher.exe:0x41bc20 / CMessageConnectionMessageRef_DecodeMessageCode
// Exported decode helper used by CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84
// at launcher.exe:0x41c5c0. Decodes the message code from a message-ref payload,
// handling both headerless (locator-based) and non-headerless (direct) payload formats.
bool CMessageConnection_0x4b7928_DecodeMessageCode(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    uint16_t* outMessageCode,
    bool* outUsedHeaderlessLocatorDecode);

// anchor: launcher.exe:0x41bc20 - CMessageConnectionMessageRef_DecodeMessageCode
// Non-headerless message code decoder
uint16_t CMessageConnectionMessageRef_DecodeMessageCode(
    CMessageConnectionMessageRef_0x4ba23c* messageRef);

// anchor: launcher.exe:0x41bbb0 - CMessageConnectionMessageRef_DecodeMessageCodeAlternate
// Headerless message code decoder (alternate version)
uint16_t CMessageConnectionMessageRef_DecodeMessageCodeAlternate(
    CMessageConnectionMessageRef_0x4ba23c* messageRef);

struct CMessageConnectionMessageRefHandleScaffold {
    // anchor family: launcher.exe:0x455cd0 / 0x4489d0
    // Tiny retained outer-message-ref handle helper used by packet-agenda read handoff and by
    // `CMessageConnection::OnOperationCompleted` stack locals.
    CMessageConnectionMessageRef_0x4ba23c* messageRef00 = nullptr; // +0x00
};

static_assert(sizeof(CMessageConnectionMessageRefHandleScaffold) == 0x04, "message-ref handle size mismatch");

struct CMessageConnectionCompletionHelperScaffold {
    // anchor family: launcher.exe:0x436080 / vtable `0x004b3e20`
    // Small event + embedded-lock helper reached by `CMessageConnection_0x4b7928::OnOperationCompleted`
    // on work types `1` and `2` through connection `+0x7c/+0x80`.
    void** vtable00 = nullptr; // +0x00
    CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold embeddedLockHelper04{}; // +0x04
    HANDLE eventHandle20 = nullptr; // +0x20

    CMessageConnectionCompletionHelperScaffold();
    ~CMessageConnectionCompletionHelperScaffold();

    void Signal();
    DWORD Wait(uint32_t timeoutMs) const;
};

static_assert(offsetof(CMessageConnectionCompletionHelperScaffold, embeddedLockHelper04) == 0x04, "completion helper lock offset mismatch");
static_assert(offsetof(CMessageConnectionCompletionHelperScaffold, eventHandle20) == 0x20, "completion helper event offset mismatch");
static_assert(sizeof(CMessageConnectionCompletionHelperScaffold) == 0x24, "completion helper size mismatch");

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
    CMessageConnectionMessageRef_0x4ba23c* messageRef08 = nullptr;
};

static_assert(offsetof(CMessageConnectionPacketBuilderEnvelope, messageRef08) == 0x08, "packet-builder envelope message-ref offset mismatch");

struct CMessageConnectionPacketBuilderReservationScaffold {
    // Recovered repeated reservation sidecar used by the local packet-builder helpers after
    // `0x43a230` / `0x43acf0` reserve `(requestedBytes + 2)` at the tail of the retained
    // message-ref payload:
    // - `+0x00` = concrete write pointer immediately after the little-endian reserved-length word
    // - `+0x04` = reserved content byte count (not including that 2-byte length field)
    uint8_t* writePointer00 = nullptr;
    uint16_t reservedContentByteCount04 = 0u;
    uint16_t reservedPadding06 = 0u;
};

static_assert(sizeof(CMessageConnectionPacketBuilderReservationScaffold) == 0x08, "packet-builder reservation size mismatch");

struct CMessageConnectionPacketBuilderPayloadScaffold {
    // Common active derived local packet-builder shape laid on top of the raw `0x439840` envelope
    // by families such as:
    // - `0x004b53b4` / packet `0x0a`
    // - `0x004b53f0` / packet `0x0d`
    // - `0x004b5418` / packet `0x0f`
    // - `0x004b5364` / packet `0x06`
    // - `0x004b6524` / packet `0x01 + length-prefixed blob`
    // - `0x004b6560` / packet `0x03 + 16-byte challenge response`
    // Current best common raw fields after the base envelope front matter:
    // - `+0x0c` = helper-local byte/flag cleared by the init/reset helpers
    // - `+0x10` = packet payload base pointer used by the fixed-field writers and reservation
    //   helpers as the offset base
    CMessageConnectionPacketBuilderEnvelope envelope00{};
    uint8_t builderFlag0c = 0u;
    uint8_t padding0d_0f[3] = {0u, 0u, 0u};
    uint8_t* packetPayload10 = nullptr;
};

// anchor: launcher.exe:0x004af2a4 / vtable
// anchor: launcher.exe:0x439840 / ctor
// Shared packet builder envelope base class. Uses CMessageConnectionMessage_CreateRef
// for internal message storage. This is the shared base pattern used by
// CLTLoginMediatorSlotRecord and potentially CLTLoginMediator.
// Note: Ghidra decompiler shows this as a component within SlotRecord_0x4b5328.
// VTable methods (inherited by derived classes):
// - +0x00: dtor / release retained outer message-ref (0x00443aa0)
// - +0x04: stub returns 0 (0x00437b50)
// - +0x08: append debug string (builder-specific)
// - +0x0c: reset/init helper (builder-specific)
// - +0x10: return builder +0x10 packet-payload base (0x00481760)
class Packet_0x4af2a4 {
public:
    // Shared packet builder envelope fields (no raw vtable ptr - uses C++ virtual):
    // +0x00: vtable pointer (C++ implicit)
    uint32_t payloadPtr04 = 0;           // +0x04: payload base pointer (was: nopatchLauncherVersionValue04)
    CMessageConnectionMessageRef_0x4ba23c* messageRef08 = nullptr;  // +0x08
    uint8_t createRefParam0c = 0;         // +0x0c: param to CreateRef (was: ownerReadyFlag0c)
    uint8_t padding0d_0f[3] = {0, 0, 0};  // +0x0d .. +0x0f

    // Payload pointers (set by derived classes):
    void* payloadAlias10 = nullptr;       // +0x10: alias of payloadPtr04 in some contexts (was: payloadBegin10)
    const char* debugString14 = nullptr;  // +0x14: heap-allocated string pointer (was: heapString14)
    uint16_t payloadSize18 = 0;           // +0x18: payload length in bytes (was: payloadLength18)
    uint8_t packetType1a = 0;             // +0x1a: status/packet type byte (was: statusByte1a)
    uint8_t padding1b = 0;                // +0x1b: alignment padding

    // Character slot fields (used by derived slot record):
    uint32_t characterIdLow1c = 0;         // +0x1c
    uint32_t characterIdHigh20 = 0;      // +0x20
    uint16_t worldId24 = 0;              // +0x24

public:
    // Virtual methods from vtable (4 slots at 0x004af2a4, 16 bytes):
    // anchor: launcher.exe:0x443aa0 / vtable +0x00 = PacketBuilder_Destroy
    virtual ~Packet_0x4af2a4() {
        // anchor: launcher.exe:0x443aa0
        // Original destructor releases the retained outer message-ref via vtable+8,
        // then optionally deletes the object itself (based on the flag in param_1).
        if (messageRef08) {
            messageRef08->Release();
        }
    }
    // anchor: launcher.exe:0x437b50 / vtable +0x04
    // Stub method returning 0 - inherited by all derived builders
    virtual uint32_t StubReturn0() { return 0; }
    // anchor: launcher.exe:0x4af2ac / vtable +0x08
    // Debug string method - builder-specific (e.g., "Certificate:..." output)
    // formatType: 2 = array size format, 3 = byte array format
    // Note: Original at 0x4425f0 returns void, not const char*
    virtual void DebugString(int /*formatType*/ = 2) {}
    // anchor: launcher.exe:0x41baf0 / vtable +0x0c
    // Initialize payload size from message ref, calls helper at 0x41bb30
    virtual void InitializePayloadSize() {}
    // anchor: launcher.exe:0x481760 / vtable +0x10
    // Returns payload base pointer (payloadAlias10 field)
    virtual void* GetPayloadBase() { return payloadAlias10; }

    // anchor: launcher.exe:0x43a230 - LocalPacketBuilder_ReserveLengthPrefixedTail
    // Virtual helper used by packet builder subclasses:
    // - CMarginConnection_SendStoredBootstrapReplyCopy98 (0x441f7d)
    // - CLTLoginMediatorPacket0x0a_SetCharacterName (0x43aaae)
    // - AuthBootstrap680_HandleInboundAuthMessage (0x448418)
    // Must be virtual because derived classes have different fields at +0x14/+0x18:
    // - Packet_0x4af2a4: debugString14 (+0x14), payloadSize18 (+0x18)
    // - Packet_CertConnectRequest_0x4b6524: reservationHeader14 (+0x14), reservedContentByteCount18 (+0x18)
    // Reserves space for a length-prefixed tail, updates bytes[1-2] with offset to header.
    // Returns content byte count (may be clamped to available space).
    virtual uint16_t ReserveLengthPrefixedTail(uint16_t contentByteCount);

    // anchor: launcher.exe:0x439840 - Packet_0x4af2a4_DefaultCtor
    // Default constructor - creates message ref, sets up payload base at +0xc offset
    //
    // DECOMPILER ARTIFACT NOTE (Ghidra 0x439840):
    // The decompiler incorrectly shows:
    //   messageRefHelper_local.messageRef00 = (CMessageConnectionMessageRef_0x4ba23c *)this;
    // before calling CMessageConnectionMessage_CreateRef(&messageRefHelper_local, 0).
    // Actual behavior: messageRefHelper_local is an uninitialized 4-byte stack local.
    // CMessageConnectionMessage_CreateRef creates a NEW message ref and stores it in
    // messageRefHelper_local.messageRef00, then the AddRef/Release dance updates this->messageRef08.
    Packet_0x4af2a4() {
        // Create message ref as per original 0x439840
        // Note: Uses CMessageConnectionMessage_CreateRef helper which:
        // 1. Calls CMessageConnectionMessage_Create() to allocate new ref
        // 2. Sets messageContext14 = 0
        // 3. Stores in helper->messageRef00
        // 4. Calls AddRef on the new ref
        messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
        if (messageRef08) {
            messageRef08->AddRef();
            messageRef08->ResetForPacketBuilder(false, 0);
            if (messageRef08->messageStorage0c) {
                // FIDELITY: Original at 0x439840 computes:
                //   payloadPtr04 = *(messageRef08 + 0xc) + 0xc
                // Where *(messageRef08 + 0xc) is messageStorage0c.
                // So payloadPtr04 = messageStorage0c + 0xc = &payloadBytes0c[0]
                uint8_t* payloadBase = messageRef08->messageStorage0c->payloadBytes0c.data();
                payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase);
                payloadAlias10 = payloadBase;
            }
        }
    }
};

/**
 * Packet_CertChallenge_0x4b6538 - Challenge response packet builder (packet 0x03)
 * anchor: launcher.exe vtable `0x004b6538`
 *
 * Derived packet builder used by CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse
 * (0x4429b0) to construct the CERT_ChallengeResponse packet with SessionKey and Secret fields.
 *
 * VTable layout at 0x4b6538 (5 slots, inherits from Packet_0x4af2a4):
 * - Slot 0 (+0x00): 0x443aa0 - destructor (inherited)
 * - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited, returns 0)
 * - Slot 2 (+0x08): 0x442690 - DebugString (OVERRIDDEN - "SessionKey:" and "Secret:" output)
 * - Slot 3 (+0x0c): 0x4419c0 - InitializePayloadSize (OVERRIDDEN - packet 0x11 setup, size 0x21)
 * - Slot 4 (+0x10): 0x441470 - EnsureStreamPacketEncryptionModule (OVERRIDDEN)
 */
class Packet_CertChallenge_0x4b6538 : public Packet_0x4af2a4 {
public:
    // Additional field for challenge response extraction (follows base class fields)
    // packetPayloadPtr points to payload for extracting SessionKey/Secret bytes
    // at offsets +0x11/+0x15/+0x19/+0x1d for response packet construction
    uint8_t* packetPayloadPtr = nullptr;  // +0x28 (after base's worldId24 at +0x24)

    // anchor: launcher.exe:0x443aa0 / vtable slot 0 (inherited destructor)
    ~Packet_CertChallenge_0x4b6538() override = default;

    // anchor: launcher.exe:0x437b50 / vtable slot 1 (inherited, returns 0)
    // StubReturn0() inherited from base

    // anchor: launcher.exe:0x442690 / vtable slot 2 (OVERRIDDEN)
    // DebugString - outputs "SessionKey:" and "Secret:" debug info
    // Original at 0x442690:
    // - formatType 2: "SessionKey:(Array of size 0x10) Secret:(Array of size 0x10) "
    // - formatType 3: "SessionKey:[byte0,...,] Secret:[byte0,...,] "
    void DebugString(int formatType = 2) override {
        if (formatType == 2) {
            spdlog::debug("CLTLoginMediatorPacketBuilderEnvelope: SessionKey:(Array of size 0x10) Secret:(Array of size 0x10)");
        } else if (formatType == 3 && payloadAlias10) {
            std::string sessionKeyBytes, secretBytes;
            for (int i = 0; i < 16; ++i) {
                if (i > 0) { sessionKeyBytes += ","; secretBytes += ","; }
                sessionKeyBytes += fmt::format("0x{:02x}", static_cast<uint8_t*>(payloadAlias10)[i + 1]);
                secretBytes += fmt::format("0x{:02x}", static_cast<uint8_t*>(payloadAlias10)[i + 0x11]);
            }
            spdlog::debug("CLTLoginMediatorPacketBuilderEnvelope: SessionKey:[{}] Secret:[{}]",
                         sessionKeyBytes, secretBytes);
        }
    }

    // anchor: launcher.exe:0x4419c0 / vtable slot 3 (OVERRIDDEN) -> InitializePayloadSize
    // InitializePayloadSize - setup for packet 0x11 (challenge response)
    // Original at 0x4419c0 uses g_MessageOffsetLookupTable with payload[13] nibbles:
    //   offset = g_MessageOffsetLookupTable[(payload[13] >> 4) & 7] + 0x12 + g_MessageOffsetLookupTable[payload[13] & 7]
    // Then allocates 0x21 bytes and sets opcode byte to 0x00
    // FIDELITY TODO: Source stub - should implement lookup table logic from binary
    void InitializePayloadSize() override {
        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->ResetPayloadByteCount(0x21);
            packetPayloadPtr = static_cast<uint8_t*>(payloadAlias10);
            if (packetPayloadPtr) {
                *packetPayloadPtr = 0x00;  // Opcode byte
            }
        }
    }

    // Allow default construction for stack-local envelope instances
    Packet_CertChallenge_0x4b6538();

    // anchor: launcher.exe:0x441920 -> CLTLoginMediatorPacketBuilderEnvelope_ctor_MessageRefAndFlag
    // Original signature: cls_0x4b6538::cls_0x4b6538(&local_38, (int *)messageRef, '\x01')
    // FIDELITY: Proper constructor matching original:
    //   this->messageRef08 = messageRef;
    //   this->nopatchLauncherVersionValue04 = (flag == '\0') ? *(messageRef+0xc)+0xc : computed value
    //   this->mbr_0x10 = this->nopatchLauncherVersionValue04
    Packet_CertChallenge_0x4b6538(
        CMessageConnectionMessageRef_0x4ba23c* messageRef,
        uint8_t flag);

    // REMOVED: No matching original ctor - this overload is source-only convenience
    explicit Packet_CertChallenge_0x4b6538(
        const std::array<uint8_t, 16>& decryptedBytes);

    // anchor: launcher.exe:0x442ac6 -> envelope.mbr_0x10 +1/+5/+9/+0xd extracts to this+0x85/0x89/0x8d/0x91
    // Original at 0x442ac6-0x442ae0 extracts seed bytes via:
    //   *(dword *)(this + 0x85) = *(dword *)(local_38.mbr_0x10 + 1)
    //   *(dword *)(this + 0x89) = *(dword *)(local_38.mbr_0x10 + 5)

};

// Size: 0x18 (24 bytes) - vftptr(4) + nopatchLauncherVersionValue04(4) + messageRef08(4) + ownerReadyFlag0c(1) + padding(3) + packetPayloadPtr(4) = 20 bytes (actual may be padded to 24)
static_assert(sizeof(Packet_CertChallenge_0x4b6538) == 0x2c,
              "CLTLoginMediatorPacketBuilderEnvelope size mismatch");

// Default constructor is inherited from base Packet_0x4af2a4 (anchor: launcher.exe:0x439840)

// anchor: launcher.exe:0x441920 -> CLTLoginMediatorPacketBuilderEnvelope_ctor_MessageRefAndFlag
// Packet_CertChallenge_0x4b6538 ctor with message ref and flag
// Original uses different offset calculation based on flag:
//   flag == 0: packetPayloadPtr = payloadBase + 0xc
//   flag != 0: packetPayloadPtr uses lookup table (see Ghidra decompile 0x441959-0x44197d)
// The lookup table at 0x004cb4d4 is indexed by payload[13] nibbles
// For now, use simplified payload base since both ExtractChallengeBytes (+1)
// and ExtractForResponsePacket (+0x11) read relative offsets that work from base
inline Packet_CertChallenge_0x4b6538::Packet_CertChallenge_0x4b6538(
    CMessageConnectionMessageRef_0x4ba23c* messageRef,
    uint8_t flag) {
    (void)flag;  // Flag affects offset calculation in original
    messageRef08 = messageRef;

    if (messageRef && messageRef->messageStorage0c) {
        const uint8_t* payload = messageRef->messageStorage0c->payloadBytes0c.data();
        // Simple offset: packetPayloadPtr points to payload base
        // For full fidelity, would use lookup table per original decompile
        payloadPtr04 = reinterpret_cast<uint32_t>(payload);
        packetPayloadPtr = const_cast<uint8_t*>(payload);
    } else {
        payloadPtr04 = 0;
        packetPayloadPtr = nullptr;
    }

    createRefParam0c = flag;
}

// anchor: launcher.exe:0x443aa0 -> PacketBuilder_CertChallengeResponse_Dtor (shared dtor)
// UNUSED in main flow - array constructor would be at different address if it existed
inline Packet_CertChallenge_0x4b6538::Packet_CertChallenge_0x4b6538(
    const std::array<uint8_t, 16>& decryptedBytes) {
    // Not used in main flow - the messageRef constructor is used instead
    (void)decryptedBytes;
    packetPayloadPtr = nullptr;
}

static_assert(offsetof(CMessageConnectionPacketBuilderPayloadScaffold, packetPayload10) == 0x10, "packet-builder payload pointer offset mismatch");

// anchor: launcher.exe:0x4b6560 -> Packet_CertChallengeResponse_0x4b6560
// FIDELITY: Separate class for response envelope (vtable 0x4b6560)
// The original manually switches vtable from base PacketBuilder (0x4af2a4) to this (0x4b6560)
// at offset 0x442afd after default construction.
// This class is used specifically for building the CERT challenge response packet (opcode 0x03).
// The vtable at 0x4b6560 is a copy of MarginConnectionChallengeParsedResult_0x4b654c vtable
// but used in a different context for response packet building.
class Packet_CertChallengeResponse_0x4b6560 : public Packet_0x4af2a4 {
public:
    // FIDELITY: Default constructor mirrors original at 0x439840
    // Creates new message ref, sets up payload base at +0xc offset
    // anchor: launcher.exe:0x439840 - PacketBuilder_0x4af2a4_DefaultCtor (base class ctor)
    Packet_CertChallengeResponse_0x4b6560();

    ~Packet_CertChallengeResponse_0x4b6560() override = default;
};

// Size matches base PacketBuilder (0x26 = 38 bytes)
static_assert(sizeof(Packet_CertChallengeResponse_0x4b6560) == sizeof(Packet_0x4af2a4),
              "Packet_CertChallengeResponse_0x4b6560 size mismatch");

inline Packet_CertChallengeResponse_0x4b6560::Packet_CertChallengeResponse_0x4b6560() {
    // FIDELITY: Mirror original default ctor at 0x439840
    // The original sets base vtable (0x4af2a4), creates message ref, then later
    // manually overwrites with 0x4b6560 at 0x442afd.
    // We directly initialize with the correct vtable concept.
    // anchor: launcher.exe:0x439840-0x4398a4: PacketBuilder_0x4af2a4_DefaultCtor creates message ref, sets payload ptr

    CMessageConnectionMessageRef_0x4ba23c* newMessageRef = new CMessageConnectionMessageRef_0x4ba23c();
    if (newMessageRef) {
        newMessageRef->AddRef();
        newMessageRef->ResetForPacketBuilder(false, 0);
        messageRef08 = newMessageRef;
        if (newMessageRef->messageStorage0c) {
            uint8_t* payloadBase = newMessageRef->messageStorage0c->payloadBytes0c.data();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase);
            payloadAlias10 = payloadBase;
        }
    }
}

// Packet_CertConnectRequest_0x4b6524 - CERT_ConnectRequest packet builder (opcode 0x01)
// anchor: launcher.exe vtable `0x004b6524`
// Faithful mirror of the local packet builder used in CMarginConnection_SendStoredBootstrapReplyCopy98
// to construct CERT_ConnectRequest packets with length-prefixed blob payloads.
//
// VTable layout at 0x4b6524 (5 slots, inherits from Packet_0x4af2a4):
// - Slot 0 (+0x00): 0x443aa0 - destructor (inherited)
// - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited, returns 0)
// - Slot 2 (+0x08): 0x4425f0 - DebugString (OVERRIDDEN - "Certificate:..." output)
// - Slot 3 (+0x0c): 0x4418a0 - InitializePayloadSize (OVERRIDDEN - packet 0x01 setup)
// - Slot 4 (+0x10): 0x481760 - GetPayloadBase (inherited, returns payloadAlias10)
// Include PacketBuilder base class for inheritance (after CMessageConnectionMessageRef is defined)

class Packet_CertConnectRequest_0x4b6524 : public Packet_0x4af2a4 {
public:
    // FIDELITY: No additional fields at +0x14/+0x18 - use base class fields:
    // - debugString14 at +0x14 is repurposed as reservationHeader14 (uint8_t* instead of const char*)
    // - payloadSize18 at +0x18 is repurposed as reservedContentByteCount18
    // This matches original binary layout where these offsets store the reservation state.

    Packet_CertConnectRequest_0x4b6524() {
        // anchor: launcher.exe:0x441f30 - constructor setup
        // C++ compiler automatically sets vptr to Packet_CertConnectRequest_0x4b6524 vtable
        // Base class Packet_0x4af2a4 constructor runs first:
        //   - Creates a fresh messageRef via CMessageConnectionMessage_CreateRef
        //   - Sets nopatchLauncherVersionValue04 = messageStorage->PayloadBase()
        //   - Sets payloadBegin10 = nopatchLauncherVersionValue04
        // Reservation fields are zero-initialized via base class:
        debugString14 = nullptr;  // Acts as reservationHeader14
        payloadSize18 = 0u;       // Acts as reservedContentByteCount18
    }

    // Virtual method overrides matching vtable 0x4b6524
    // anchor: launcher.exe:0x443aa0 / vtable slot 0 (inherited destructor)
    ~Packet_CertConnectRequest_0x4b6524() override = default;

    // anchor: launcher.exe:0x437b50 / vtable slot 1 (inherited, returns 0)
    // StubReturn0() inherited from base

    // anchor: launcher.exe:0x4425f0 / vtable slot 2 (OVERRIDDEN)
    // DebugString - outputs "Certificate" debug info via spdlog
    // Original at 0x4425f0: void function, outputs "Certificate:(Array of size X)" or "Certificate:[byte0,byte1,...]"
    // param_2 == 2: array size format; param_2 == 3: byte array format
    void DebugString(int formatType = 2) override {
        // Cast debugString14 (const char*) to uint8_t* for byte array access
        uint8_t* reservationHeader = reinterpret_cast<uint8_t*>(const_cast<char*>(debugString14));
        if (!reservationHeader) {
            spdlog::debug("CertConnectRequestPacketBuilder: Certificate reservation not initialized");
            return;
        }

        if (formatType == 2) {
            // Array size format: "Certificate:(Array of size X)"
            spdlog::debug("CertConnectRequestPacketBuilder: Certificate:(Array of size 0x{:04x})",
                         static_cast<unsigned>(payloadSize18));
        } else if (formatType == 3) {
            // Byte array format: "Certificate:[byte0,byte1,...,]"
            if (payloadSize18 > 0) {
                std::string byteStr;
                for (uint16_t i = 0; i < payloadSize18 && i < 32; ++i) {
                    if (i > 0) byteStr += ",";
                    byteStr += fmt::format("0x{:02x}", reservationHeader[i]);
                }
                if (payloadSize18 > 32) byteStr += ",...";
                spdlog::debug("CertConnectRequestPacketBuilder: Certificate:[{}]", byteStr);
            } else {
                spdlog::debug("CertConnectRequestPacketBuilder: Certificate:[]");
            }
        }
    }

    // anchor: launcher.exe:0x4418a0 / vtable slot 3 (OVERRIDDEN)
    // InitializePayloadSize - setup for packet 0x01
    // anchor: launcher.exe:0x4418a0
    // Original flow:
    // - reads descriptor byte at [messageRef->messageStorage0c + 0xd]
    // - computes payload size = lookup[high_nibble] + lookup[low_nibble] + 0x12
    // - sets payloadPtr04 = messageStorage + 0xc + payloadSize (END pointer)
    // - calls SetPayloadByteCount(messageRef08, 0) to zero messageRef length
    // - calls messageStorage->GrowPayloadByteCount(payloadSize) to grow storage
    // - sets payloadAlias10 = payloadPtr04 (END pointer)
    // - calls messageRef08->GrowPayloadByteCount(3) to grow messageRef by 3
    // - writes opcode 0x01 (CERT_ConnectRequest) and zeros word at payload+1
    // - clears reservation fields +0x14/+0x18
    void InitializePayloadSize() override {
        if (!messageRef08 || !messageRef08->messageStorage0c) {
            return;
        }

        auto* msgStorage = messageRef08->messageStorage0c;
        uint8_t* storageBase = reinterpret_cast<uint8_t*>(msgStorage);
        uint8_t descriptor = *(storageBase + 0xd);

        uint32_t offset1 = g_MessageOffsetLookupTable[(descriptor >> 4) & 7];
        uint32_t offset2 = g_MessageOffsetLookupTable[descriptor & 7];
        uint32_t payloadSize = offset1 + offset2 + 0x12;

        // Set payloadPtr04 to END of payload (storage base + 0xc + size)
        payloadPtr04 = reinterpret_cast<uint32_t>(storageBase + 0xc) + payloadSize;

        // Zero the messageRef payload length via SetPayloadByteCount
        messageRef08->SetPayloadByteCount(0);

        // Grow messageStorage by payloadSize (from 0 to payloadSize)
        msgStorage->GrowPayloadByteCount(static_cast<uint16_t>(payloadSize));

        // Set payloadAlias10 to END pointer (same as payloadPtr04)
        payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

        // Grow messageRef by 3 bytes (opcode + 2-byte field)
        messageRef08->GrowPayloadByteCount(3);

        // Write opcode and zero word to payload
        uint8_t* packetPayload = static_cast<uint8_t*>(payloadAlias10);
        if (packetPayload) {
            packetPayload[0] = 0x01u;  // CERT_ConnectRequest opcode
            *reinterpret_cast<uint16_t*>(packetPayload + 1) = 0u;  // Zero field
        }
        // Clear reservation fields (stored in base class debugString14/payloadSize18)
        debugString14 = nullptr;
        payloadSize18 = 0u;
    }

    // anchor: launcher.exe:0x481760 / vtable slot 4 (inherited)
    // GetPayloadBase() inherited from base, returns payloadAlias10
};

// Note: Modern C++ adds vptr (4 bytes). Packet_CertConnectRequest_0x4b6524 has no additional
// fields - it repurposes base class debugString14/payloadSize18 for reservation state.
// Size: 4 (vptr) + base fields (0x24) = 0x28 (40 bytes) on i686.
static_assert(sizeof(Packet_CertConnectRequest_0x4b6524) == 0x28,
              "Packet_CertConnectRequest_0x4b6524 size mismatch (expected 0x28 with vptr)");

class CStreamPacketEncryptionOwnerBase_0x4b81dc {
public:
    // anchor: launcher.exe vtable `0x004b81dc`
    // Source-owned full virtual C++ mirror of the owner-side base. This family is only used by
    // our own code, so we intentionally let the compiler own the C++ vptr instead of storing a
    // fake raw `vtable00` field.
    virtual ~CStreamPacketEncryptionOwnerBase_0x4b81dc() = default;
    virtual const char* ClassName() const { return "CStreamPacketEncryptionOwnerBase_0x4b81dc"; }
};

class CStreamPacketEncryptionHelperBase_0x4b81c8 {
public:
    // anchor: launcher.exe vtable `0x004b81c8`
    // Source-owned full virtual C++ mirror of the helper-side base shared by:
    // - module read helper `0x004b86f0`
    // - module write helper `0x004b8690`
    // - embedded agenda helper `0x004baf48`
    virtual ~CStreamPacketEncryptionHelperBase_0x4b81c8() = default;
    virtual void HandleOpaqueMessageRef(void* opaqueMessageRef) = 0;

    // Current best role for original helper `+0x04`:
    // - a downstream helper-family object link, not a message-ref or owner pointer
    // - read side: previous agenda read-chain head
    // - write side: next write helper, or finally the embedded agenda write helper
    CStreamPacketEncryptionHelperBase_0x4b81c8* nextHelper04 = nullptr;

protected:
    void ForwardToNextHelper(void* opaqueMessageRef);
};

class CMessageConnection_0x4b7928;
class CStreamPacketEncryptionModule_0x4b8704;

class CMessageConnectionMessageRefOutputBuffer {
public:
    // Source-owned real C++ helper class for the recovered helper-local transformed-output family.
    // Canonical original sink vtables remain documented under:
    // - `0x004b8438`
    // - `0x004b84f0`
    CMessageConnectionMessageRef_0x4ba23c* messageRef = nullptr;
    bool hasValue = false;

    ~CMessageConnectionMessageRefOutputBuffer();
    void Reset();
    bool SetPayloadBytes(const uint8_t* payloadBytes, size_t payloadByteCount);
    CMessageConnectionMessageRef_0x4ba23c* MessageRef();
};



class CStreamPacketEncryptionModuleReadTransformWorker_0x4b86f0 {
public:
    // Source-owned real C++ mirror of the worker family inserted into the read-helper collection
    // by `0x44d910`.
    // Original shape is the larger `FeedbackSizeTransformAdapter_ConstructLarge` branch reused by
    // the auth-bootstrap transform family. Source now keeps that recovered large/decrypting
    // `AssemblyTwofish` adapter object explicit instead of collapsing this worker down to raw seed
    // bytes only. `0x44d500` then wraps each stored worker in a `StreamTransformationFilter` and
    // passes a copied `LTTCPEndpointKey_0x44b070` peer block into `0x44bca0 = CPacketDecryptor_DecryptPacket`.
    std::array<uint8_t, 16> associatedSeedBytes{};
    mxo::auth::internal::FeedbackSizeTransformAdapterLarge feedbackTransform;
    bool hasConfiguredFeedbackTransform = false;

    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
    bool TryTransform(
        const CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
        CMessageConnectionMessageRefOutputBuffer* outputBuffer);
};

class CStreamPacketEncryptionModuleWriteTransformWorker_0x4b86a8 {
public:
    // Source-owned real C++ mirror of the embedded write-side transform worker rooted at helper
    // `+0x0c` by `0x44d820` / worker vtable `0x004b86a8`.
    // Original shape is the smaller `FeedbackSizeTransformAdapter_ConstructSmall` branch.
    // Source now keeps that recovered small/encrypting `AssemblyTwofish` adapter object explicit
    // instead of only caching the 16-byte seed. `0x44d250` then resolves the parameter block fed
    // into `0x44c750 = CPacketEncryptor_EncryptPacket`, while the helper also snapshots
    // `configuredConnection10->+0x24 = LTTCPEndpointKey_0x44b070` for the same packet-crypto family.
    std::array<uint8_t, 16> associatedSeedBytes{};
    mxo::auth::internal::FeedbackSizeTransformAdapterSmall feedbackTransform;
    bool hasConfiguredFeedbackTransform = false;

    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
    bool TryTransform(
        const CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
        CMessageConnectionMessageRefOutputBuffer* outputBuffer);
};

// Forward declaration: Packet_MarginChallenge_0x4b654c is defined later near
// CBaseMarginConnection_0x4b64a8 (see below).

// ============================================================
// Message Reference Helper - cls_0x4489d0
// ============================================================
// anchor: launcher.exe:0x4489d0
// Helper class that wraps message reference management for challenge processing.
// Forward declaration for the message ref helper class
class MessageConnectionMessageRefHelper_0x4489d0;

// Standalone function for creating message references (used in OnOperationCompleted)
// anchor: launcher.exe:0x455cd0
void CMessageConnectionMessage_CreateRef(
    MessageConnectionMessageRefHelper_0x4489d0* messageRefHelper,
    int messageContext);

// Corresponds to the local_8 variable in CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse (0x4429b0).
//
// This class provides RAII-style management of CMessageConnectionMessageRef_0x4ba23c objects,
// ensuring proper reference counting and cleanup. The original launcher.exe uses this
// pattern extensively for temporary message objects during packet processing.

class MessageConnectionMessageRefHelper_0x4489d0 {
public:
    CMessageConnectionMessageRef_0x4ba23c* messageRef00 = nullptr;  // +0x00 - retained message ref pointer

    // anchor: launcher.exe:0x4429b8 (implicit default constructor)
    // Default constructor - initializes message ref pointer to nullptr
    MessageConnectionMessageRefHelper_0x4489d0() = default;

    // anchor: launcher.exe:0x4429d0 (implicit destructor)
    // Destructor - automatically releases any retained message ref
    // This ensures proper cleanup even if the helper goes out of scope unexpectedly
    ~MessageConnectionMessageRefHelper_0x4489d0() {
        if (messageRef00) {
            messageRef00->Release();
            messageRef00 = nullptr;
        }
    }

    // anchor: launcher.exe:0x455cd0 - CMessageConnectionMessage_CreateRef
    // Creates a new message reference with the specified context.
    // Corresponds to the call at 0x4429b8 in the original code.
    //
    // Parameters:
    //   messageContext - Context value to store in message ref (field 0x14)
    //
    // Returns: void (messageRef00 is set to the new reference)
    void CreateRef(int messageContext) {
        // anchor: launcher.exe:0x455c60 - CMessageConnectionMessage_Create
        messageRef00 = new CMessageConnectionMessageRef_0x4ba23c();
        if (messageRef00) {
            // anchor: launcher.exe:0x455b80 - AddRef (vtable +0x04)
            messageRef00->AddRef();
            // Initialize the message storage (this is what the original does)
            messageRef00->ResetForPacketBuilder(false, static_cast<uint32_t>(messageContext));
        }
    }

    // anchor: launcher.exe:0x455ad0 (implicit Reset method)
    // Explicitly reset and clean up the current message ref.
    // This method provides the same cleanup as the destructor but can be
    // called explicitly when needed.
    void Reset() {
        if (messageRef00) {
            messageRef00->Release();
            messageRef00 = nullptr;
        }
    }

    // anchor: launcher.exe:0x4429c0 (validation pattern)
    // Validates that the message ref is properly initialized and has storage.
    // Corresponds to the null checks in the original code before using the message ref.
    //
    // Returns: true if message ref is valid and has storage, false otherwise
    bool IsValid() const {
        return messageRef00 != nullptr && messageRef00->messageStorage0c != nullptr;
    }

    // Delete copy constructor and assignment operator to prevent accidental copying
    // The original launcher.exe doesn't copy these helper objects
    MessageConnectionMessageRefHelper_0x4489d0(const MessageConnectionMessageRefHelper_0x4489d0&) = delete;
    MessageConnectionMessageRefHelper_0x4489d0& operator=(const MessageConnectionMessageRefHelper_0x4489d0&) = delete;

    // anchor: launcher.exe:0x4489d0 - Message ref handle assignment
    static void CMessageConnectionMessageRefHandle_AssignRetained(
        MessageConnectionMessageRefHelper_0x4489d0* targetHandle,
        CMessageConnectionMessageRef_0x4ba23c* sourceMessageRef);
};

// Static assertion to ensure the helper maintains minimal size
static_assert(sizeof(MessageConnectionMessageRefHelper_0x4489d0) <= 0x08,
              "MessageConnectionMessageRefHelper_0x4489d0 size unexpected");

class CStreamPacketEncryptionModuleHelper : public CStreamPacketEncryptionHelperBase_0x4b81c8 {
public:
    // Current common source-owned state on the two module child helpers.
    // `0x469740 = CMessageConnectionPacketAgenda_InstallStreamPacketEncryptionModule` gives the
    // tighter roles here:
    // - helper `+0x04` is the downstream helper-family link used by the agenda chains
    // - helper `+0x08` is the owning `CStreamPacketEncryptionModule*`
    //   - the transform bodies then read owner `+0x10 = configuredConnection10`
    //     and copy connection `+0x24 = LTTCPEndpointKey_0x44b070` through
    //     `0x44aff0 = LTTCPEndpointKey_0x44b070_Copy`
    //   - read side passes that 16-byte peer block into `0x44bca0 = CPacketDecryptor_DecryptPacket`
    //     for discard/expiry logging
    //   - write side likewise passes it into `0x44c750 = CPacketEncryptor_EncryptPacket`
    CStreamPacketEncryptionModule_0x4b8704* owner08 = nullptr;
};

class CStreamPacketEncryptionModuleReadHelper_0x4b86f0
    : public CStreamPacketEncryptionModuleHelper {
public:
    // anchor: launcher.exe vtable `0x004b86f0`
    // Current best recovered role:
    // - owner `+0x04`
    // - inserted at agenda `+0x40` by
    //   `0x469740 = CMessageConnectionPacketAgenda_InstallStreamPacketEncryptionModule`
    // - therefore the concrete module-side **read helper**
    // - original helper `+0x0c` is a small repeated-success control dword consumed by `0x44d2e0`
    // - original helper `+0x10/+0x14/+0x18` are the read-worker vector front matter consumed by
    //   `0x44d500 / 0x44d770 / 0x44d130`
    // - repeated success on worker 0 can collapse that collection back down to just worker 0
    // - source now models that as a real worker collection rather than raw begin/end pointers
    uint32_t collectionControl0c = 0;
    std::vector<CStreamPacketEncryptionModuleReadTransformWorker_0x4b86f0> transformWorkers;
    CMessageConnectionMessageRefOutputBuffer transformedOutput;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule_0x4b8704* owner);
    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
};

class CStreamPacketEncryptionModuleWriteHelper_0x4b8690
    : public CStreamPacketEncryptionModuleHelper {
public:
    // anchor: launcher.exe vtable `0x004b8690`
    // Current best recovered role:
    // - owner `+0x08`
    // - inserted at agenda `+0x44/+0x48` by
    //   `0x469740 = CMessageConnectionPacketAgenda_InstallStreamPacketEncryptionModule`
    // - therefore the concrete module-side **write helper**
    // - original `0x44d820` constructs a much larger embedded transform worker at helper `+0x0c`
    //   and retables it to `0x004b86a8`
    // - `0x44d390` can forward either a fresh transformed message-ref or a null discard through
    //   helper `+0x04`
    // - source now models that as a real worker class rather than raw placeholder pointers
    CStreamPacketEncryptionModuleWriteTransformWorker_0x4b86a8 transformWorker;
    bool hasTransformWorker = false;
    CMessageConnectionMessageRefOutputBuffer transformedOutput;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule_0x4b8704* owner);
    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
};

class PacketProcessingAgenda_0x4baf48 : public CStreamPacketEncryptionHelperBase_0x4b81c8 {
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
    CStreamPacketEncryptionHelperBase_0x4b81c8** downstreamHelperSlot14 = nullptr;

    // anchor: launcher.exe:0x44c680 / vtable `0x004baf48 +0x00`
    ~PacketProcessingAgenda_0x4baf48() override;

    // anchor: launcher.exe:0x44bb60 / vtable `0x004baf48 +0x04`
    virtual void VirtualMethod1_0x44bb60();

    // anchor: launcher.exe:0x481750 / vtable `0x004baf48 +0x08`
    virtual uint32_t VirtualMethod2_0x481750();

    // anchor: launcher.exe:0x469980 / vtable `0x004baf48 +0x0c`
    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;

    // anchor: launcher.exe:0x469720 / vtable `0x004baf48 +0x10`
    virtual void DelegateToChainedHelper();

    // anchor: launcher.exe:0x469980
    // Store opaque message ref with proper ref counting
    void StoreOpaqueMessageRef(CMessageConnectionMessageRef_0x4ba23c* messageRef);
};

class CStreamPacketEncryptionModule_0x4b8704 : public CStreamPacketEncryptionOwnerBase_0x4b81dc {
public:
    // anchor: launcher.exe:0x44da00 / vtable `0x004b8704`
    // Named owner/aggregator recovered from:
    // - `0x44dab0 = CStreamPacketEncryptionModule_GetClassName`
    // - literal string `"CStreamPacketEncryptionModule"`
    // Current tighter integration from `0x469740`:
    // - `readHelper04` = module **read helper** rooted at `0x004b86f0`
    // - `writeHelper08` = module **write helper** rooted at `0x004b8690`
    // - `nextConfiguredModule0c` = next configured module in the agenda-owned module list
    // - `configuredConnection10` = agenda `+0x00` connection pointer copied during install
    CStreamPacketEncryptionModuleReadHelper_0x4b86f0* readHelper04 = nullptr;
    CStreamPacketEncryptionModuleWriteHelper_0x4b8690* writeHelper08 = nullptr;
    CStreamPacketEncryptionModule_0x4b8704* nextConfiguredModule0c = nullptr;
    CMessageConnection_0x4b7928* configuredConnection10 = nullptr;
    CStreamPacketEncryptionModuleReadHelper_0x4b86f0 ownedReadHelper14{};
    CStreamPacketEncryptionModuleWriteHelper_0x4b8690 ownedWriteHelper2c{};
    std::array<uint8_t, 16> associatedSeedBytes40{};

    const char* ClassName() const override { return "CStreamPacketEncryptionModule"; }
    void InitializeForMarginConnectionSeed(const std::array<uint8_t, 16>& seedBytes85);
    void RefreshFromMarginConnectionSeed(const std::array<uint8_t, 16>& seedBytes85);
};

struct PacketProcessingAgenda_0x469850 {
    // Source-owned mirror of the lazy packet-processing agenda object rooted at original
    // connection `+0x74`.
    // This scaffold now keeps the recovered agenda front matter explicit where source can do so
    // faithfully while still representing the two embedded helper objects as internal C++ wrappers.
    CMessageConnection_0x4b7928* connectionOwner00 = nullptr;
    CStreamPacketEncryptionModule_0x4b8704* configuredModuleList04 = nullptr;
    CMessageConnectionMessageRef_0x4ba23c* readOutputSlot08 = nullptr;
    PacketProcessingAgenda_0x4baf48 embeddedReadHelper0c{};
    CMessageConnectionMessageRef_0x4ba23c* writeOutputSlot24 = nullptr;
    PacketProcessingAgenda_0x4baf48 embeddedWriteHelper28{};
    // faithful raw-field naming from the recovered agenda object:
    // - `readHelperChainHead40` mirrors original agenda `+0x40`
    // - `writeHelperChainHead44` mirrors original agenda `+0x44`
    // - `writeHelperChainTail48` mirrors original agenda `+0x48`
    // Ghidra evidence from `0x469850 / 0x469740` shows `+0x44/+0x48` are real write-chain
    // head/tail pointers, not source-invented convenience state.
    CStreamPacketEncryptionHelperBase_0x4b81c8* readHelperChainHead40 = nullptr;
    CStreamPacketEncryptionHelperBase_0x4b81c8* writeHelperChainHead44 = nullptr;
    CStreamPacketEncryptionHelperBase_0x4b81c8* writeHelperChainTail48 = nullptr;
    uint16_t configuredModuleCount4c = 0;
    uint16_t reserved4e = 0;
    bool created = false;
    CStreamPacketEncryptionModule_0x4b8704* configuredStreamPacketEncryptionModule = nullptr;

    // anchor: launcher.exe:0x469850
    // Constructor that mirrors the original initialization logic
    explicit PacketProcessingAgenda_0x469850(CMessageConnection_0x4b7928* connectionOwner);

    // anchor: launcher.exe:0x469740
    // Install stream packet encryption module into the agenda
    uint16_t InstallStreamPacketEncryptionModule(
        CStreamPacketEncryptionModule_0x4b8704* streamPacketEncryptionModule);

    // anchor: launcher.exe:0x469950 / 0x469930
    // Apply packet agenda processing to message refs
    CMessageConnectionMessageRef_0x4ba23c* ApplySendPacketAgenda(
        CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
        bool* outAgendaTouched = nullptr);
    CMessageConnectionMessageRef_0x4ba23c* ApplyReceivePacketAgenda(
        CMessageConnectionMessageRef_0x4ba23c* inputMessageRef,
        bool* outAgendaTouched = nullptr);


};

struct CMessageConnectionReceivedPacketScaffold {
    std::vector<uint8_t> payloadBytes;
    bool headerless = false;
};

// Forward declaration for the packet agenda struct
struct PacketProcessingAgenda_0x469850;

class CMessageConnection_0x4b7928 : public CLTTCPConnection {
public:
    // UNANCHORED: source-owned narrow subset of `0x448b40` with a null engine and without the
    // optional `+0x7c/+0x80` completion-helper allocation path.
    CMessageConnection_0x4b7928();
    // UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
    // Current source now also mirrors the optional `+0x7c/+0x80` completion-helper allocation
    // branch when explicitly requested.
    explicit CMessageConnection_0x4b7928(
        CLTThreadPerClientTCPEngine_0x4b2768* engine,
        bool allocateCompletionHelpers = false);
    // UNANCHORED: source-owned default destructor; the original family uses several concrete deleting-dtor paths.
    ~CMessageConnection_0x4b7928();

    // UNANCHORED: source-owned compatibility pass-through over the recovered base-connection
    // `+0x10` engine field; no separate leaf-owned engine slot is evidenced here.
    void SetEngine(CLTThreadPerClientTCPEngine_0x4b2768* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered base-connection `+0x10`
    // engine field; no separate leaf-owned engine slot is evidenced here.
    CLTThreadPerClientTCPEngine_0x4b2768* Engine() const;

    // anchor family: launcher.exe:0x449d20
    // Source-owned raw-byte wrapper over the inherited `CLTTCPConnection::SendBuffer` path.
    // Keep this distinct from the local packet-builder / message-ref send family rooted at:
    // - `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00`
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
    void SendPacketMessageRef(CMessageConnectionMessageRef_0x4ba23c& messageRef);

    // anchor: launcher.exe:0x448960
    // Narrow source-owned wrapper over the per-connection packet-name callback configuration:
    // - original writes connection `+0x78 = enabled`
    // - when enabled, original writes connection `+0x70 = callback`
    void ConfigurePacketNameFamily(
        CMessageConnectionPacketNameFamily family,
        bool packetizedMessagesEnabled);

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
    // installed named module pointer explicit, and now also routes helper-side
    // replacement/discard through source-owned real C++ worker classes for the newly recovered
    // module family.
    void ConfigurePacketAgenda(
        CStreamPacketEncryptionModule_0x4b8704* streamPacketEncryptionModule = nullptr);
    // anchor family: launcher.exe:0x448980 -> connection `+0x74`
    // Source-owned accessor for the recovered lazy packet-agenda pointer.
    const PacketProcessingAgenda_0x469850* PacketAgenda() const;

    // anchor: launcher.exe:0x4490c0
    // string-backed original name: CMessageConnection_0x4b7928::OnOperationCompleted
    // current best read:
    // - main completion/receive-side bridge back into engine/queue handling
    // - the initial dispatch on `workItem+0x04` only directly handles work types `1`, `2`, and `3`
    //   - type `1` = optional close-completion helper `+0x80`
    //   - type `2` = optional connect-completion helper `+0x7c`
    //   - source now carries the raw helper layout explicitly and mirrors the original signal
    //     branch when those helpers exist
    //   - current auth/margin startup construction still passes `allocateCompletionHelpers=0`, so
    //     both helper pointers remain null on the active path and the original false-ish return
    //     boundary is preserved there
    //   - every other non-type-3 work item falls straight back to the later leaf wrappers instead
    //     of being consumed or generically logged by base `0x4490c0`
    // - work type `3` first checks the shared `workItem+0x08` status/payload dword (`0x434d00`)
    //   and returns handled immediately when that dword is non-zero
    // - otherwise it copies packet-body bytes out of the retained-fragment-backed
    //   `CParsedPacketWorkItem` via `+0x24/+0x28` into a local receive/message-ref scaffold
    //   built on the same outer-ref/inner-storage split used by `0x455cd0/0x455c60`
    // - later inside that same callback, the tighter current tail is now:
    //   - keep the original headerless locator-id validity gate on that receive/message-ref
    //   - optional read-agenda handoff `connection+0x74 -> 0x469930 -> 0x4489d0`
    //   - always hit the vtable `+0x38(messageRef)` pre-dispatch hook first
    //   - when the local message-ref keeps the original non-zero `+0x10` flag, the callback then
    //     branches on `inner+0x0f & 0x07`
    //     - protocol `5` -> vtable `+0x30(messageRef)`
    //     - protocol `7` -> vtable `+0x34(messageRef)`
    //     - otherwise -> vtable `+0x2c(messageRef)`
    //   - the simpler zero-flag path goes straight to vtable `+0x2c(messageRef)`
    // - current startup auth/margin leaf tables keep `+0x30/+0x34/+0x38` on `0x441790` no-op, so
    //   those protocol-`5/7` branches are still consumed locally before any later owner fallback
    // - current source now mirrors the nearer local `+0x2c` auth/margin destinations too:
    //   - auth: local message-ref/base-filter step -> `0x449a30 -> owner+0x180 / 0x41f250`
    //   - margin: `0x44af20 -> 0x442d00 -> owner+0x184 / 0x41f260`
    // - remaining source gap kept explicit:
    //   - packet-name callback / log-name side effects are still only partially mirrored
    //   - exact heap/refcount lifetime on the original message-object tail remains narrower than
    //     the current local scaffolds
    // - practical boundary consequence of the current correction:
    //   - once this in-callback tail reaches the virtual dispatch family, source now treats the
    //     packet as consumed locally just like original `0x4490c0`
    //   - source-owned synthetic receive-drain and local type-`0x0b` continuations are therefore
    //     outside the original base body and remain only as later compatibility / owner-fallback
    //     scaffolding
    uint32_t OnOperationCompleted(void* workItem);

    // UNANCHORED: source-owned helper mirroring the current queue producer context-key shape.
    // Current best reading: queue0C often receives (workItem, this, 0) from this class.
    void* ContextKey() { return this; }

protected:
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x2c`
    // Source-owned post-copy dispatch seam beneath the narrowed type-3 receive path.
    // Current bounded use:
    // - base `0x4490c0` now still owns the copied-packet extraction, locator gate, agenda handoff,
    //   and the later packetized protocol branch that chooses `+0x2c/+0x30/+0x34`
    // - leaf families receive a nearer outer-ref/inner-storage scaffold instead of a naked byte
    //   vector, matching the real `0x455cd0 -> 0x41bc20/0x41bbb0` seam more closely
    // - this local seam stands in for the original one-argument `+0x2c(messageRef)` body only
    // - unlike earlier source-owned fallback logic, original `0x4490c0` still consumes the packet
    //   locally after calling this seam even when the callee returns false-ish
    virtual uint32_t DispatchCopiedParsedPacketTailScaffold(
        CMessageConnectionMessageRef_0x4ba23c& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x30`
    // Non-zero-flag protocol-`5` receive seam beneath the same callback tail.
    // Original call shape is `this->+0x30(messageRef)`.
    // Current startup auth/margin leaf tables keep this row on `0x441790`, so the default source
    // implementation mirrors that as a locally consumed no-op.
    virtual uint32_t DispatchPacketizedProtocol5MessageRefScaffold(
        CMessageConnectionMessageRef_0x4ba23c& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x34`
    // Non-zero-flag protocol-`7` receive seam beneath the same callback tail.
    // Original call shape is `this->+0x34(messageRef)`.
    // Current startup auth/margin leaf tables also keep this row on `0x441790`, so the default
    // source implementation mirrors that as a locally consumed no-op.
    virtual uint32_t DispatchPacketizedProtocol7MessageRefScaffold(
        CMessageConnectionMessageRef_0x4ba23c& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x38`
    // Pre-dispatch receive hook reached after the optional read-agenda handoff and before the
    // later `+0x2c/+0x30/+0x34` branch.
    // Original call shape is `this->+0x38(messageRef)`.
    // Current startup auth/margin/base tables keep this row on `0x441790`, so the default source
    // implementation remains a no-op.
    virtual void PreDispatchMessageRefScaffold(
        CMessageConnectionMessageRef_0x4ba23c& messageRef);

private:
    // UNANCHORED: source-owned diagnostic stringifier for the recovered packet-name family enum.
    static const char* PacketNameFamilyToString(CMessageConnectionPacketNameFamily family);
    // anchor: launcher.exe:0x469950
    // Source-owned send-side packet-agenda handoff helper.
    // Current bounded model preserves the nearer
    // `0x469950 = CMessageConnectionPacketAgenda_DispatchWriteHelperChain` shape:
    // - no active write helper => keep the original message-ref pointer
    // - active write helper => return agenda `+0x24` exactly after the helper chain runs
    // - original returns that slot after dispatch without pre-clearing it first
    // Source now models that chain with real internal worker classes, so helper-side
    // replacement/discard remains visible at the same seam as the original.
    CMessageConnectionMessageRef_0x4ba23c* ApplySendPacketAgenda(
        CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
        bool* outAgendaTouched);
    // anchor: launcher.exe:0x448a00
    // Lower submit helper beneath `0x448cf0`; source computes the final byte pointer/size directly
    // from raw inner `+0x0a/+0x0b/+0x0c..` storage.
    uint32_t SubmitMessageRefBytes(const CMessageConnectionMessageRef_0x4ba23c& messageRef);

public:
    CMessageConnectionPacketNameFamily packetNameFamily_ = CMessageConnectionPacketNameFamily::kUnknown;
    uintptr_t packetNameCallback_ = 0;
    bool packetizedMessagesEnabled_ = false;
    std::unique_ptr<CMessageConnectionCompletionHelperScaffold> connectCompletionHelper7c_;
    std::unique_ptr<CMessageConnectionCompletionHelperScaffold> closeCompletionHelper80_;
    std::unique_ptr<PacketProcessingAgenda_0x469850> packetAgenda_;
    std::vector<uint8_t> lastReceivedPacketBodyBytesScaffold_;
    bool lastReceivedPacketHeaderlessScaffold_ = false;
    // Legacy fallback queue retained only as dormant compatibility scaffolding now that the
    // tightened `0x4490c0` receive tail consumes the auth/margin packet path locally.
};

struct CMarginConnectionLocalCompletionWorkItemScaffold;
class CMarginConnectionBootstrapPrepStateOwner_0x443340;

// ============================================================
// CBaseMarginConnection_0x4b64a8 class declaration
// ============================================================
// Current recovered auth-bootstrap decryptor state stored at connection `+0xa0`.
// Source wraps real Crypto++ types but preserves the launcher entry points around them.
class CMarginConnectionAuthBootstrapState_0x443220;

// Scaffold structures for parsed message results
struct CBaseMarginConnection_0x4b64a8_ParsedPayloadSpanScaffold {
    const uint8_t* logicalPayloadBytes00 = nullptr;
    size_t logicalPayloadByteCount04 = 0u;
    bool headerless08 = false;
    bool usedHeaderlessLocatorDecode09 = false;
};

// ============================================================
// Packet_MarginChallenge_0x4b654c - Code-2 challenge packet builder
// ============================================================
// anchor: launcher.exe vtable `0x004b654c`
// Proper packet builder subclass of Packet_0x4af2a4 used for code-2 (challenge) message
// handling.  VTable at 0x4b654c has 5 slots inheriting from Packet_0x4af2a4 (0x4af2a4):
// - Slot 0 (+0x00): 0x443aa0 - destructor (inherited)
// - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited)
// - Slot 2 (+0x08): 0x442790 - DebugString (OVERRIDDEN - "EncryptedBlob:...")
// - Slot 3 (+0x0c): 0x441ad0 - InitializePayloadSize (OVERRIDDEN - opcode 0x02)
// - Slot 4 (+0x10): 0x481760 - GetPayloadBase (inherited)
//
// Field layout reuses inherited Packet_0x4af2a4 fields:
// - debugString14 (+0x14) stores the encrypted blob pointer
// - payloadSize18 (+0x18) stores the encrypted blob size
class Packet_MarginChallenge_0x4b654c : public Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x441a30 / CBaseMarginConnection_OnMessageCode2
    // Constructor that initializes from an existing message ref, mirroring original manual
    // vtable+field init.  Releases the base-allocated message ref and replaces it with the
    // provided external ref (with AddRef).
    Packet_MarginChallenge_0x4b654c(
        CMessageConnectionMessageRef_0x4ba23c* messageRef,
        bool isHeaderless);

    ~Packet_MarginChallenge_0x4b654c() override = default;

    // anchor: launcher.exe:0x442790 / vtable slot 2 (OVERRIDDEN)
    // DebugString - outputs "EncryptedBlob:(Array of size X)" or "EncryptedBlob:[...]"
    void DebugString(int formatType = 2) override;

    // anchor: launcher.exe:0x441ad0 / vtable slot 3 (OVERRIDDEN)
    // InitializePayloadSize - setup for opcode 0x02 packet
    void InitializePayloadSize() override;

    // anchor: launcher.exe:0x4416d0 / MarginConnectionChallengeParsedResult_0x4b654c::meth_0x4416d0
    // Extracts encrypted blob pointer and size from the payload based on headerless flag.
    void ExtractEncryptedBlobFromPayload(bool isHeaderless);

    // FIDELITY: Original accesses debugString14 (+0x14) and payloadSize18 (+0x18)
    // directly as encryptedBlobPtr and encryptedBlobSize; no accessor methods exist.
};

struct CBaseMarginConnection_0x4b64a8_Code4MessageScaffold {
    CBaseMarginConnection_0x4b64a8_ParsedPayloadSpanScaffold parsedPayload00{};
    uint32_t statusOrPayload0c = 0u;
};

struct CBaseMarginConnection_0x4b64a8_Code5MessageScaffold {
    CBaseMarginConnection_0x4b64a8_ParsedPayloadSpanScaffold parsedPayload00{};
    std::array<uint8_t, 16> seedBytes0c{};
};

class CBaseMarginConnection_0x4b64a8 : public CMessageConnection_0x4b7928 {
public:
    // UNANCHORED: source-owned narrow intermediate-base ctor.
    CBaseMarginConnection_0x4b64a8();
    // UNANCHORED: source-owned narrow intermediate-base ctor that only seeds the recovered engine.
    explicit CBaseMarginConnection_0x4b64a8(CLTThreadPerClientTCPEngine_0x4b2768* connectionEngine);
    // UNANCHORED: source-owned default intermediate-base destructor.
    ~CBaseMarginConnection_0x4b64a8() override;

    // anchor: launcher.exe:0x441850
    // Narrow source-owned mirror of the consumed decoded-code-4 side effect that sets connection
    // byte `+0x84` when the inner status dword is zero.
    void SetMessageCode4SuccessFlag84(bool value);
    // anchor family: launcher.exe:0x441850 / 0x44af20 -> connection `+0x84`
    // Source-owned readback of the same recovered success-side connection byte.
    bool MessageCode4SuccessFlag84() const;
    // anchor: launcher.exe:0x441850
    // Source-owned local type-`0x0b` continuation scaffold routed back through connection vtable
    // `+0x10` / `OnOperationCompleted(workItem)`.
    uint32_t DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus);
    // anchor: launcher.exe:0x41ce80 -> connection `+0x98`
    // Source-owned mirror of the connection helper that stores a copied `0x136` auth-reply shadow
    // block for the later raw type-1 state5/state6 send path.
    bool StoreBootstrapReplyCopy98(const void* bytes, size_t byteCount);
    // anchor: launcher.exe:0x41f30
    // Source-owned mirror of the later raw type-1 send that now keeps the nearer original local
    // builder shape explicit too.
    void SendStoredBootstrapReplyCopy98();
    // anchor: launcher.exe:0x442d00
    // Intermediate base dispatch router shared by the auth and margin startup leaf families.
    // Current source now keeps the nearer helper-wrapper step explicit too:
    // - resolve the logical payload span from the incoming message object through the
    //   `0x41bc20/0x41bbb0`-style seam first
    // - then route decoded code `2/4/5` through the recovered direct helper families before the
    //   later connection/owner continuations
    virtual uint32_t DispatchMessage(void* messageRef);

    // anchor: launcher.exe:0x442d00 -> 0x442d9e -> 0x4429b0 (CBaseMarginConnection_0x4b64a8_HandleCode2CertChallengeAndSendResponse)
    // Original signature: uint __thiscall CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse
    //                     (CBaseMarginConnection_0x4b64a8 *this, Packet_MarginChallenge_0x4b654c *parsedMessageResult)
    // Handle decoded code 2: decrypt challenge blob, extract seed/response bytes, send response.
    // The parsedMessageResult object is a Packet_MarginChallenge_0x4b654c (vtable 0x4b654c)
    // constructed by its constructor (mirroring original OnMessageCode2 at 0x441a30).
    uint32_t HandleCode2ForBootstrap(
        Packet_MarginChallenge_0x4b654c* parsedMessageResult);

    // Handle decoded code 4: set success flag, synthesize local completion work item, continue bootstrap.
    // anchor: launcher.exe:0x441850 - takes 2 params (this, parsed message object)
    uint32_t HandleCode4ForBootstrap(
        const uint8_t* packetBytes,
        size_t packetSize);

    // anchor family: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
    // Recovered original field at connection+0x85. Kept public because original
    // consumers (e.g., login mediator +0xd4 callback) access it directly.
    std::array<uint8_t, 16> messageCode5SeedBytes85_{};

protected:
    // anchor: launcher.exe:0x442d00 -> vtable `+0x2c`
    // The recovered intermediate-base table still owns the `+0x2c(messageRef)` row. Keep the
    // post-copy seam thin here by forwarding the local message-ref scaffold straight into the
    // concrete base/leaf `DispatchMessage` body.
    uint32_t DispatchCopiedParsedPacketTailScaffold(
        CMessageConnectionMessageRef_0x4ba23c& messageRef) override;

private:
    friend class CMarginConnectionBootstrapPrepStateOwner_0x443340;

    // anchor: launcher.exe:0x441470 / 0x44da00 / 0x44daf0
    // Source-owned mirror of the lazy connection `+0x9c` packet-agenda module install/refresh
    // reached from the consumed code-2 challenge path after `+0x85..+0x94` is available.
    void EnsureStreamPacketEncryptionModuleFromSeed85();

    bool messageCode4SuccessFlag84_ = false;
    bool hasBootstrapReplyCopy98_ = false;
    std::array<uint8_t, 0x136> bootstrapReplyCopy98_{};
    std::unique_ptr<CMarginConnectionAuthBootstrapState_0x443220> bootstrapPrepStateA0_; // original connection `+0xa0`; standalone helper `0x443340` allocates/stores the auth-bootstrap decryptor state here, and the first later original consumer is `0x4429b0 -> 0x437810`
    std::unique_ptr<CStreamPacketEncryptionModule_0x4b8704> streamPacketEncryptionModule9c_;
};

// ============================================================
// CAuthStartupConnection_0x4afef0 class declaration
// ============================================================
// Source-owned leaf mirror of the auth-side startup child built at `0x41d170` and assigned
// vtable `0x004afef0` before the initial `connection->+0x1c(owner+0x5c)` call.
// Current constructor-side proof also keeps the owner split explicit here:
// - connection `+0xa4` = direct `CLTLoginMediator*`
// - extra launcher-bridge records remain separate source-owned sidecars only
// Keep the class name conservative in source for now:
// - the surrounding canonical docs still carry older naming on `0x004afef0`
// - but current static RE is strong that this is the auth-side leaf completion wrapper reached
//   through `0x449a70`, not just a generic base `CMessageConnection_0x4b7928`
class CAuthStartupConnection_0x4afef0 : public CBaseMarginConnection_0x4b64a8 {
public:
    // UNANCHORED: source-owned narrow leaf ctor.
    CAuthStartupConnection_0x4afef0();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CAuthStartupConnection_0x4afef0(CLTThreadPerClientTCPEngine_0x4b2768* authEngine);
    ~CAuthStartupConnection_0x4afef0();

    // anchor: launcher.exe:0x449a70
    // Current best original order:
    // - call base `0x4490c0`
    // - if base returns 0, call owner `+0x17c(this, workItem)`
    // - if that also returns 0, fall through to `0x448a60`
    // - only then read `workItem+0x04`
    // - if work type == 1, tears down through the connection object
    // - no leaf-local type-2 split; connect-status also flows through owner `+0x17c`
    uint32_t OnOperationCompleted(void* workItem) override;

    // anchor: launcher.exe:0x449a30
    // Thin auth-side leaf override on top of `CBaseMarginConnection_0x4b64a8::DispatchMessage`:
    // - call base `0x442d00` first
    // - if that returns 0, call owner `+0x180(messageRef)`
    // Replacement-only auth payload staging, when still needed by deeper state2/`0x448140`
    // helpers, no longer belongs here.
    uint32_t DispatchMessage(void* messageRef) override;
};

// ============================================================
// CMarginConnection class declaration
// ============================================================
// anchor: launcher.exe:cls_0x4baa00 (0x464870)
// Minimal local stack work-item shape recovered for the `0x441850` continuation.
// This mirrors cls_0x4baa00 - the work item wrapper class.
// Layout: vtable(+0x00), workType(+0x04), statusOrPayload(+0x08), extra data(+0x10,+0x14)
class CMarginConnectionLocalCompletionWorkItemScaffold {
public:
    CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader header{};
};

static_assert(sizeof(CMarginConnectionLocalCompletionWorkItemScaffold) == 0x0c, "margin code-4 local completion work-item size mismatch");

// anchor: launcher.exe:0x465d70 / original subobject at decryptor+0x0c
// This helper converts the recovered old-Crypto++ `Integer` inputs into a real
// `CryptoPP::RSA::PrivateKey` while preserving the original `0x465d70`
// key-loader boundary.
//
// Static-RE interpretation:
// - the original `0x4ba50c` family is old `CryptoPP::Integer`
// - the original `cls_0x4b659c` subobject really behaves like embedded
//   `CryptoPP::RSA::PrivateKey` / `CryptoPP::InvertibleRSAFunction` state
// So source now uses direct Crypto++ types here instead of a launcher-local big-int stand-in.

// anchor: launcher.exe:0x443220 / complete-object ctor reached from `0x443340`
// This launcher wrapper owns the real Crypto++ objects that static-RE identifies under the
// original MI-heavy decryptor family:
// - root `0x4b42b0`  -> older CryptoPP::Algorithm (now represented by the real Crypto++ members)
// - subobject `0x442b70` -> CryptoPP::RSAES_OAEP_SHA_Decryptor
// - helper `0x465d70` -> direct initialization of embedded CryptoPP::RSA::PrivateKey
class CMarginConnectionAuthBootstrapState_0x443220 {
public:
    CMarginConnectionAuthBootstrapState_0x443220(
        const CryptoPP::Integer& modulus,
        const CryptoPP::Integer& publicExponent,
        const CryptoPP::Integer& privateExponent,
        int constructVirtualBaseStateFlag);

    // anchor: launcher.exe:0x437810
    void* DecryptChallenge(
        void* outputBuffer,
        const void* cryptoContext,
        uint32_t encryptedBlobPtr,
        uint16_t expectedOutputSize,
        void* localBufferPtr);

    // anchor: launcher.exe:0x468130
    void* PerformRSADecryption(
        void* outputBuffer,
        const void* cryptoContext,
        uint32_t encryptedBlobPtr,
        void* localBufferPtr);

    bool HasBootstrapPrivateKey() const { return bootstrapPrivateKeyInitialized_; }
    const CryptoPP::RSA::PrivateKey& BootstrapPrivateKey() const { return bootstrapPrivateKey_0x0c; }

private:
    CryptoPP::RSA::PrivateKey bootstrapPrivateKey_0x0c{};
    bool bootstrapPrivateKeyInitialized_ = false;
    CryptoPP::RSAES_OAEP_SHA_Decryptor cryptoPPDecryptor_0x442b70{};
};

class CMarginConnectionBootstrapPrepStateOwner_0x443340 {
public:
    explicit CMarginConnectionBootstrapPrepStateOwner_0x443340(CBaseMarginConnection_0x4b64a8& connection)
        : connection_(connection) {}

    // anchor: launcher.exe:0x443340
    // Original direct-call helper allocates a fresh `0xe0` prep object from three child-side
    // old-Crypto++ `Integer` objects and stores it at connection `+0xa0`.
    // Source converts those preserved child-side raw `0x14` objects before this seam and stores a
    // wrapper around the identified real Crypto++ decryptor/key objects here.
    void StoreBootstrapPrepStateA0(
        const CryptoPP::Integer& modulus,
        const CryptoPP::Integer& publicExponent,
        const CryptoPP::Integer& privateExponent);

private:
    CBaseMarginConnection_0x4b64a8& connection_;
};

class CMarginConnection_0x4aff38 : public CBaseMarginConnection_0x4b64a8 {
public:
    // UNANCHORED: source-owned narrow leaf ctor.
    CMarginConnection_0x4aff38();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CMarginConnection_0x4aff38(CLTThreadPerClientTCPEngine_0x4b2768* marginEngine);
    // UNANCHORED: source-owned default destructor.
    // Current tighter static-RE split:
    // - live leaf teardown is through scalar-deleting-dtor wrappers at `0x41cf50/0x41cf80`
    // - `0x41ce80` is the separate connection `+0x98` reply-copy helper
    ~CMarginConnection_0x4aff38();

    // anchor: launcher.exe:0x44af60
    // Later leaf override on top of the base `CMessageConnection_0x4b7928::OnOperationCompleted` family.
    // Current best original order:
    // - call base `0x4490c0`
    // - if base returns 0, call owner `+0x188(this, workItem)`
    // - if that also returns 0, fall through to `0x448a60`
    // - only then read `workItem+0x04`
    // - if work type == 1, clear owner byte `+0xf14` then tear down through the connection object
    // - no leaf-local type-2 split; connect-status also flows through owner `+0x188`
    uint32_t OnOperationCompleted(void* workItem) override;

    // anchor: launcher.exe:0x44af20
    // Later leaf dispatch override on top of `CBaseMarginConnection_0x4b64a8::DispatchMessage`.
    // Current best original order:
    // - call `CBaseMarginConnection_0x4b64a8::DispatchMessage(this, messageRef)` (`0x442d00`)
    // - if that returns 0, call owner `+0x184(messageRef)`
    uint32_t DispatchMessage(void* messageRef) override;
};

}  // namespace mxo::liblttcp
