#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Forward declare CLTLoginMediator to avoid circular include
namespace mxo { namespace ltlogin { class CLTLoginMediator; } }
#include "../libltcrypto/auth_internal.h"
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
// - carries the inherited parser pointer at +0x6c
// - Ghidra/OOAnalyzer fidelity note for the active receive path:
//   - keep `+0x00` as a real `vftptr_0x0` field, not an inline vtable struct member
//   - otherwise `0x4490c0` decompilation loses the inherited `remoteEndpoint24` field and starts
//     rendering endpoint copies as `&(this->vtable...).slot_36`-style artifacts instead of the
//     real connection `+0x24` endpoint block
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
// - 0x004afef0 = 0x0041cf50 scalar-deleting-dtor wrapper
// - 0x004aff00 = 0x00449a70 leaf `OnOperationCompleted` override
// - 0x004aff1c = 0x00449a30 leaf `DispatchMessage` override
// - 0x004aff34 = 0x0041ce80 connection `+0x98` reply-copy helper
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

class CMessageConnectionMessageStorage : public CMessageConnectionLocalRefCountedBase {
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
    std::array<uint8_t, 0x1000> payloadBytes0c{};

    uint32_t AddRef() override;
    uint32_t Release() override;
    // anchor: launcher.exe:0x455ad0 / vtable `0x004ba208 +0x0c`
    void FinalRelease() override;
    void ResetRefCount() override;
    void SetRefCountFromPtr(const volatile long* refCountSource) override;

    // anchor: launcher.exe:0x455bd0 inner-storage setup before outer `+0x0c` stores/AddRefs it
    void ResetForPacketBuilderScaffold();
    // UNANCHORED: source-owned deterministic reset helper that zero-fills the newly selected span
    // before committing the raw payload-length header bytes.
    void ResetPayloadByteCountScaffold(uint16_t payloadByteCount);
    // anchor: launcher.exe:0x4557b0 / shared raw length write behavior with `0x41bb60`
    void SetPayloadByteCountRawScaffold(uint16_t payloadByteCount);
    // anchor: launcher.exe:0x4557b0
    uint16_t GrowPayloadByteCountScaffold(uint16_t additionalByteCount);
    // UNANCHORED: source-owned raw payload-length decoder from `+0x0a/+0x0b`, clamped to the
    // recovered `0x1000` payload ceiling.
    uint16_t PayloadByteCountScaffold() const;
    // UNANCHORED: source-owned capacity helper over the recovered raw payload-storage layout.
    uint16_t RemainingAppendableByteCountScaffold() const;
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    uint8_t* PayloadBaseScaffold();
    // UNANCHORED: source-owned accessor for raw inline payload base `+0x0c`.
    const uint8_t* PayloadBaseScaffold() const;
};

class CMessageConnectionMessageRefBase_0x4ba220 : public CMessageConnectionLocalRefCountedBase {
public:
    // anchor: launcher.exe vtable `0x004ba220`
    // Reset-time/base outer message-ref table installed by `0x455bd0` before `0x455c60`
    // retables the created object to `0x004ba23c`.
    volatile long referenceCount04 = 0;
    uint32_t field08 = 0;
    CMessageConnectionMessageStorage* messageStorage0c = nullptr;

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
    void ResetBaseForPacketBuilderScaffold(uint32_t field08 = 0u);
    // anchor: launcher.exe:0x4557b0 / outer vtable `0x004ba220 +0x18`
    uint16_t GrowPayloadByteCountScaffold(uint16_t additionalByteCount);
    // anchor: launcher.exe:0x439820
    uint8_t* PayloadAppendPointerScaffold();
    // anchor: launcher.exe:0x41bb60
    bool SetPayloadByteCountScaffold(uint32_t payloadByteCount);
    // UNANCHORED: source-owned convenience wrapper over the inner payload-byte-count header.
    uint16_t PayloadByteCountScaffold() const;

protected:
    CMessageConnectionMessageStorage ownedMessageStorage_{};
};

class CMessageConnectionMessageRef : public CMessageConnectionMessageRefBase_0x4ba220 {
public:
    // anchor: launcher.exe vtable `0x004ba23c`
    // Live outer message-ref table produced by `0x455c60/0x455cd0`.
    uint8_t headerless10 = 0;
    uint8_t padding11_13[3] = {0u, 0u, 0u};
    uint32_t messageContext14 = 0;
    uint32_t field18 = 0;
    uint32_t field1c = 0;
    uint32_t field20 = 0;

    CMessageConnectionMessageRef() = default;

    // anchor: launcher.exe:0x455b80 / vtable `0x004ba23c +0x0c`
    void FinalRelease() override;

    // anchor: launcher.exe:0x455cd0 / 0x455c60
    // Source-local live-object initializer mirroring `CreateRef(Create(...), messageContext14)`.
    void ResetForPacketBuilderScaffold(bool headerless, uint32_t messageContext14 = 0u);

    // Source note: keep the local outer scaffold noncopyable so raw `messageStorage0c` never
    // silently keeps a pointer into another instance's inline owned-storage tail.
    CMessageConnectionMessageRef(const CMessageConnectionMessageRef&) = delete;
    CMessageConnectionMessageRef& operator=(const CMessageConnectionMessageRef&) = delete;
    CMessageConnectionMessageRef(CMessageConnectionMessageRef&&) = delete;
    CMessageConnectionMessageRef& operator=(CMessageConnectionMessageRef&&) = delete;
};

// anchor: launcher.exe:0x41bc20 / CMessageConnectionMessageRef_DecodeMessageCode
// Exported decode helper used by CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84
// at launcher.exe:0x41c5c0. Decodes the message code from a message-ref payload,
// handling both headerless (locator-based) and non-headerless (direct) payload formats.
bool CMessageConnection_DecodeMessageCodeScaffold(
    const CMessageConnectionMessageRef& messageRef,
    uint16_t* outMessageCode,
    bool* outUsedHeaderlessLocatorDecode);

struct CMessageConnectionMessageRefHandleScaffold {
    // anchor family: launcher.exe:0x455cd0 / 0x4489d0
    // Tiny retained outer-message-ref handle helper used by packet-agenda read handoff and by
    // `CMessageConnection::OnOperationCompleted` stack locals.
    CMessageConnectionMessageRef* messageRef00 = nullptr; // +0x00
};

static_assert(sizeof(CMessageConnectionMessageRefHandleScaffold) == 0x04, "message-ref handle size mismatch");

struct CMessageConnectionCompletionHelperScaffold {
    // anchor family: launcher.exe:0x436080 / vtable `0x004b3e20`
    // Small event + embedded-lock helper reached by `CMessageConnection::OnOperationCompleted`
    // on work types `1` and `2` through connection `+0x7c/+0x80`.
    void** vtable00 = nullptr; // +0x00
    CLTThreadPerClientTCPEngine_LockHelperScaffold embeddedLockHelper04{}; // +0x04
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
    CMessageConnectionMessageRef* messageRef08 = nullptr;
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

static_assert(sizeof(CMessageConnectionPacketBuilderReservationScaffold) == 0x08, "packet-builder reservation scaffold size mismatch");

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

/**
 * VTable `0x004b6538` - CLTLoginMediatorPacketBuilderEnvelope for challenge response (packet 0x03)
 *
 * Derived local envelope used by `CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse`
 * (0x4429b0) to construct the CERT_ChallengeResponse packet.
 *
 * Layout mirrors CMessageConnectionPacketBuilderPayloadScaffold but with specific vtable dispatch:
 * - `vtable+0` = PacketBuilder_Destroy (0x443aa0)
 * - `vtable+1` = cls_0x4b6538 ctor (0x443220) - creates envelope with '\x01' flag
 * - `vtable+2` = virt_meth_0x442690 (debug/serialization)
 * - `vtable+3` = virt_meth_0x4419c0 (init/cleanup)
 * - `vtable+4` = EnsureStreamPacketEncryptionModule (0x441470)
 *
 * The constructor at 0x443220:
 * 1. Takes messageRef pointer and '\x01' flag
 * 2. Stores 16 bytes from decrypted challenge blob into mbr_0x10
 * 3. Extracts 4 dwords from mbr_0x10 at offsets +1/+5/+9/+0xd
 * 4. Writes to connection fields at this+0x85/0x89/0x8d/0x91
 *
 * Usage in 0x4429b0:
 * 1. Create cls_0x4b6538 envelope via cls_0x4b6538(&local_38, (int *)messageRef, '\x01')
 * 2. Extract 16 bytes from envelope.mbr_0x10 at offsets +1/+5/+9/+0xd
 * 3. Write to connection fields at this+0x85/0x89/0x8d/0x91
 * 4. Initialize packet builder envelope via CLTLoginMediatorPacketBuilderEnvelope_Initialize
 * 5. Set opcode 0x11 and copy challenge bytes from envelope.mbr_0x10 + 0x11/+0x15/+0x19/+0x1d
 * 6. Send via connection vtable+0x24
 */
class CLTLoginMediatorPacketBuilderEnvelope_0x4b6538 {
public:
    // anchor: launcher.exe vtable `0x004b6538`
    CMessageConnectionPacketBuilderPayloadScaffold builder00{};  // +0x00 (shared front matter)

    // anchor: launcher.exe:0x443220 - cls_0x4b6538 constructor stores challenge bytes here
    // The original stores 16 bytes from decrypted challenge blob into mbr_0x10
    // Byte extraction at 0x443220 uses: *(dword *)(this + 0x85) = *(dword *)(mbr_0x10 + 1)
    //               *(dword *)(this + 0x89) = *(dword *)(mbr_0x10 + 5)
    //               *(dword *)(this + 0x8d) = *(dword *)(mbr_0x10 + 9)
    //               *(dword *)(this + 0x91) = *(dword *)(mbr_0x10 + 0xd)
    // Layout: 4 dwords stored at offsets 0x01, 0x05, 0x09, 0x0d within mbr_0x10 (16-byte region)
    uint8_t mbr_0x10[16] = {0u};

public:
    // Forward declarations for vtable methods
    virtual ~CLTLoginMediatorPacketBuilderEnvelope_0x4b6538() = default;
    virtual uint32_t dummyVTableMethod0() const { return 0; }
    virtual void dummyVTableMethod1() {}  // virt_meth_0x442690 (debug)
    virtual void dummyVTableMethod2() {}  // virt_meth_0x4419c0 (init/cleanup)

    // Allow default construction for stack-local envelope instances
    CLTLoginMediatorPacketBuilderEnvelope_0x4b6538();

    // anchor: launcher.exe:0x443220 -> cls_0x4b6538 constructor stores 16 bytes into mbr_0x10
    explicit CLTLoginMediatorPacketBuilderEnvelope_0x4b6538(
        const std::array<uint8_t, 16>& decryptedBytes);

    // anchor: launcher.exe:0x442ac6 -> envelope.mbr_0x10 +1/+5/+9/+0xd extracts to this+0x85/0x89/0x8d/0x91
    // Original at 0x442ac6-0x442ae0 extracts seed bytes via:
    //   *(dword *)(this + 0x85) = *(dword *)(local_38.mbr_0x10 + 1)
    //   *(dword *)(this + 0x89) = *(dword *)(local_38.mbr_0x10 + 5)
    //   *(dword *)(this + 0x8d) = *(dword *)(local_38.mbr_0x10 + 9)
    //   *(dword *)(this + 0x91) = *(dword *)(local_38.mbr_0x10 + 0xd)
    // Extract 16 bytes from envelope mbr_0x10 +1/+5/+9/+0xd for connection seed fields.
    // This is the first extraction pass - copies to connection this+0x85..+0x91.
    std::array<uint8_t, 16> ExtractChallengeBytes() const;

    // anchor: launcher.exe:0x442b18 -> copy from envelope.mbr_0x10 +0x11/+0x15/+0x19/+0x1d to packet+1/+5/+9/+0xd
    // Original at 0x442b18-0x442b28 copies response packet bytes via:
    //   *(dword *)(packet + 1) = *(dword *)(local_38.mbr_0x10 + 0x11)
    //   *(dword *)(packet + 5) = *(dword *)(local_38.mbr_0x10 + 0x15)
    //   *(dword *)(packet + 9) = *(dword *)(local_38.mbr_0x10 + 0x19)
    //   *(dword *)(packet + 0xd) = *(dword *)(local_38.mbr_0x10 + 0x1d)
    // Extract 16 bytes from envelope mbr_0x10 +0x11/+0x15/+0x19/+0x1d for response packet.
    // This is the second extraction pass - copies to response packet payload at offset +1.
    // Note: Different offsets than ExtractChallengeBytes - uses +0x11 instead of +1, etc.
    std::array<uint8_t, 16> ExtractForResponsePacket() const;
};

// Size: 0x28 (40 bytes) - shared front matter (0x10) + mbr_0x10 storage (0x10) + vtable (0x10) + padding (0x08)
static_assert(sizeof(CLTLoginMediatorPacketBuilderEnvelope_0x4b6538) == 0x28,
              "CLTLoginMediatorPacketBuilderEnvelope size mismatch");

/**
 * Implementation: Default constructor for stack-local envelope instances.
 * This mirrors the original cls_0x4b6538 constructor that takes messageRef and '\x01' flag,
 * but we use a simpler default-constructed version since we're using static helpers.
 */
inline CLTLoginMediatorPacketBuilderEnvelope_0x4b6538::CLTLoginMediatorPacketBuilderEnvelope_0x4b6538() {
    // Default initialization - in original, this would take messageRef and '\x01' flag
}

// anchor: launcher.exe:0x443220 -> cls_0x4b6538 constructor stores 16 bytes into mbr_0x10
inline CLTLoginMediatorPacketBuilderEnvelope_0x4b6538::CLTLoginMediatorPacketBuilderEnvelope_0x4b6538(
    const std::array<uint8_t, 16>& decryptedBytes) {
    // Store 16 bytes into mbr_0x10 for later extraction via two different offset patterns
    std::copy(decryptedBytes.begin(), decryptedBytes.end(), this->mbr_0x10);
}

// anchor: launcher.exe:0x443220
inline std::array<uint8_t, 16> CLTLoginMediatorPacketBuilderEnvelope_0x4b6538::ExtractChallengeBytes() const {
    std::array<uint8_t, 16> result{};
    // Extract 4 dwords from mbr_0x10 at offsets +1/+5/+9/+0xd
    std::memcpy(&result[0], this->mbr_0x10 + 1, 4);   // first dword
    std::memcpy(&result[4], this->mbr_0x10 + 5, 4);  // second dword
    std::memcpy(&result[8], this->mbr_0x10 + 9, 4);  // third dword
    std::memcpy(&result[12], this->mbr_0x10 + 13, 4); // fourth dword (0xd = 13)
    return result;
}

// anchor: launcher.exe:0x442b18 -> uses mbr_0x10 +0x11/+0x15/+0x19/+0x1d for response packet
inline std::array<uint8_t, 16> CLTLoginMediatorPacketBuilderEnvelope_0x4b6538::ExtractForResponsePacket() const {
    std::array<uint8_t, 16> result{};
    // Extract 4 dwords from mbr_0x10 at offsets +0x11/+0x15/+0x19/+0x1d
    std::memcpy(&result[0], this->mbr_0x10 + 0x11, 4);   // first dword
    std::memcpy(&result[4], this->mbr_0x10 + 0x15, 4);  // second dword
    std::memcpy(&result[8], this->mbr_0x10 + 0x19, 4);  // third dword
    std::memcpy(&result[12], this->mbr_0x10 + 0x1d, 4); // fourth dword
    return result;
}

static_assert(offsetof(CMessageConnectionPacketBuilderPayloadScaffold, packetPayload10) == 0x10, "packet-builder payload pointer offset mismatch");

struct CMessageConnectionPacketBuilderPayloadWithReservationScaffold {
    // Most common next step on top of `CMessageConnectionPacketBuilderPayloadScaffold`:
    // - one trailing reservation triplet rooted at raw `+0x14`
    CMessageConnectionPacketBuilderPayloadScaffold builder00{};
    CMessageConnectionPacketBuilderReservationScaffold reservation14{};
};

static_assert(offsetof(CMessageConnectionPacketBuilderPayloadWithReservationScaffold, reservation14) == 0x14, "packet-builder reservation offset mismatch");

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
    virtual void HandleOpaqueMessageRef(void* opaqueMessageRef) = 0;

    // Current best role for original helper `+0x04`:
    // - a downstream helper-family object link, not a message-ref or owner pointer
    // - read side: previous agenda read-chain head
    // - write side: next write helper, or finally the embedded agenda write helper
    CStreamPacketEncryptionHelperBase* nextHelper04 = nullptr;

protected:
    void ForwardToNextHelper(void* opaqueMessageRef);
};

class CMessageConnection;
class CStreamPacketEncryptionModule;

class CMessageConnectionMessageRefOutputBuffer {
public:
    // Source-owned real C++ helper class for the recovered helper-local transformed-output family.
    // Canonical original sink vtables remain documented under:
    // - `0x004b8438`
    // - `0x004b84f0`
    CMessageConnectionMessageRef* messageRef = nullptr;
    bool hasValue = false;

    ~CMessageConnectionMessageRefOutputBuffer();
    void Reset();
    bool SetPayloadBytes(const uint8_t* payloadBytes, size_t payloadByteCount);
    CMessageConnectionMessageRef* MessageRef();
};

class CStreamPacketEncryptionModuleReadTransformWorker {
public:
    // Source-owned real C++ mirror of the worker family inserted into the read-helper collection
    // by `0x44d910`.
    // Original shape is the larger `FeedbackSizeTransformAdapter_ConstructLarge` branch reused by
    // the auth-bootstrap transform family. Source now keeps that recovered large/decrypting
    // `AssemblyTwofish` adapter object explicit instead of collapsing this worker down to raw seed
    // bytes only. `0x44d500` then wraps each stored worker in a `StreamTransformationFilter` and
    // passes a copied `LTTCPEndpointKey` peer block into `0x44bca0 = CPacketDecryptor_DecryptPacket`.
    std::array<uint8_t, 16> associatedSeedBytes{};
    mxo::auth::internal::FeedbackSizeTransformAdapterLarge feedbackTransform;
    bool hasConfiguredFeedbackTransform = false;

    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
    bool TryTransform(
        const CMessageConnectionMessageRef& inputMessageRef,
        CMessageConnectionMessageRefOutputBuffer* outputBuffer);
};

class CStreamPacketEncryptionModuleWriteTransformWorker {
public:
    // Source-owned real C++ mirror of the embedded write-side transform worker rooted at helper
    // `+0x0c` by `0x44d820` / worker vtable `0x004b86a8`.
    // Original shape is the smaller `FeedbackSizeTransformAdapter_ConstructSmall` branch.
    // Source now keeps that recovered small/encrypting `AssemblyTwofish` adapter object explicit
    // instead of only caching the 16-byte seed. `0x44d250` then resolves the parameter block fed
    // into `0x44c750 = CPacketEncryptor_EncryptPacket`, while the helper also snapshots
    // `configuredConnection10->+0x24 = LTTCPEndpointKey` for the same packet-crypto family.
    std::array<uint8_t, 16> associatedSeedBytes{};
    mxo::auth::internal::FeedbackSizeTransformAdapterSmall feedbackTransform;
    bool hasConfiguredFeedbackTransform = false;

    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
    bool TryTransform(
        const CMessageConnectionMessageRef& inputMessageRef,
        CMessageConnectionMessageRefOutputBuffer* outputBuffer);
};

class CStreamPacketEncryptionModuleHelper : public CStreamPacketEncryptionHelperBase {
public:
    // Current common source-owned state on the two module child helpers.
    // `0x469740 = CMessageConnectionPacketAgenda_InstallStreamPacketEncryptionModule` gives the
    // tighter roles here:
    // - helper `+0x04` is the downstream helper-family link used by the agenda chains
    // - helper `+0x08` is the owning `CStreamPacketEncryptionModule*`
    //   - the transform bodies then read owner `+0x10 = configuredConnection10`
    //     and copy connection `+0x24 = LTTCPEndpointKey` through
    //     `0x44aff0 = LTTCPEndpointKey_Copy`
    //   - read side passes that 16-byte peer block into `0x44bca0 = CPacketDecryptor_DecryptPacket`
    //     for discard/expiry logging
    //   - write side likewise passes it into `0x44c750 = CPacketEncryptor_EncryptPacket`
    CStreamPacketEncryptionModule* owner08 = nullptr;
};

class CStreamPacketEncryptionModuleReadHelper
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
    std::vector<CStreamPacketEncryptionModuleReadTransformWorker> transformWorkers;
    CMessageConnectionMessageRefOutputBuffer transformedOutput;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule* owner);
    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
};

class CStreamPacketEncryptionModuleWriteHelper
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
    CStreamPacketEncryptionModuleWriteTransformWorker transformWorker;
    bool hasTransformWorker = false;
    CMessageConnectionMessageRefOutputBuffer transformedOutput;

    void HandleOpaqueMessageRef(void* opaqueMessageRef) override;
    void ResetForOwner(CStreamPacketEncryptionModule* owner);
    void ResetForSeed(const std::array<uint8_t, 16>& seedBytes);
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
    // - `configuredConnection10` = agenda `+0x00` connection pointer copied during install
    CStreamPacketEncryptionModuleReadHelper* readHelper04 = nullptr;
    CStreamPacketEncryptionModuleWriteHelper* writeHelper08 = nullptr;
    CStreamPacketEncryptionModule* nextConfiguredModule0c = nullptr;
    CMessageConnection* configuredConnection10 = nullptr;
    CStreamPacketEncryptionModuleReadHelper ownedReadHelper14{};
    CStreamPacketEncryptionModuleWriteHelper ownedWriteHelper2c{};
    std::array<uint8_t, 16> associatedSeedBytes40{};

    const char* ClassName() const override { return "CStreamPacketEncryptionModule"; }
    void InitializeForMarginConnectionSeed(const std::array<uint8_t, 16>& seedBytes85);
    void RefreshFromMarginConnectionSeed(const std::array<uint8_t, 16>& seedBytes85);
};

struct CMessageConnectionPacketAgenda {
    // Source-owned mirror of the lazy packet-processing agenda object rooted at original
    // connection `+0x74`.
    // This scaffold now keeps the recovered agenda front matter explicit where source can do so
    // faithfully while still representing the two embedded helper objects as internal C++ wrappers.
    CMessageConnection* connectionOwner00 = nullptr;
    CStreamPacketEncryptionModule* configuredModuleList04 = nullptr;
    CMessageConnectionMessageRef* readOutputSlot08 = nullptr;
    CStreamPacketEncryptionAgendaHelper embeddedReadHelper0c{};
    CMessageConnectionMessageRef* writeOutputSlot24 = nullptr;
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
    // Current source now also mirrors the optional `+0x7c/+0x80` completion-helper allocation
    // branch when explicitly requested.
    explicit CMessageConnection(
        CLTThreadPerClientTCPEngine* engine,
        bool allocateCompletionHelpers = false);
    // UNANCHORED: source-owned default destructor; the original family uses several concrete deleting-dtor paths.
    ~CMessageConnection();

    // UNANCHORED: source-owned compatibility pass-through over the recovered base-connection
    // `+0x10` engine field; no separate leaf-owned engine slot is evidenced here.
    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered base-connection `+0x10`
    // engine field; no separate leaf-owned engine slot is evidenced here.
    CLTThreadPerClientTCPEngine* Engine() const;

    // anchor family: launcher.exe:0x449cd0
    // Source-owned bool-return wrapper over the inherited `CLTTCPConnection::Connect` body that
    // reads connection `remoteEndpoint_` directly and then calls engine slot `+0x18`.
    uint32_t EnsureConnected();

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
    uint32_t SendPacketMessageRef(CMessageConnectionMessageRef& messageRef);

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
        CStreamPacketEncryptionModule* streamPacketEncryptionModule = nullptr);
    // anchor family: launcher.exe:0x448980 -> connection `+0x74`
    // Source-owned accessor for the recovered lazy packet-agenda pointer.
    const CMessageConnectionPacketAgenda* PacketAgenda() const;

    // anchor: launcher.exe:0x4490c0
    // string-backed original name: CMessageConnection::OnOperationCompleted
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
        CMessageConnectionMessageRef& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x30`
    // Non-zero-flag protocol-`5` receive seam beneath the same callback tail.
    // Original call shape is `this->+0x30(messageRef)`.
    // Current startup auth/margin leaf tables keep this row on `0x441790`, so the default source
    // implementation mirrors that as a locally consumed no-op.
    virtual uint32_t DispatchPacketizedProtocol5MessageRefScaffold(
        CMessageConnectionMessageRef& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x34`
    // Non-zero-flag protocol-`7` receive seam beneath the same callback tail.
    // Original call shape is `this->+0x34(messageRef)`.
    // Current startup auth/margin leaf tables also keep this row on `0x441790`, so the default
    // source implementation mirrors that as a locally consumed no-op.
    virtual uint32_t DispatchPacketizedProtocol7MessageRefScaffold(
        CMessageConnectionMessageRef& messageRef);
    // anchor: launcher.exe:0x4490c0 -> vtable `+0x38`
    // Pre-dispatch receive hook reached after the optional read-agenda handoff and before the
    // later `+0x2c/+0x30/+0x34` branch.
    // Original call shape is `this->+0x38(messageRef)`.
    // Current startup auth/margin/base tables keep this row on `0x441790`, so the default source
    // implementation remains a no-op.
    virtual void PreDispatchMessageRefScaffold(
        CMessageConnectionMessageRef& messageRef);

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
    CMessageConnectionMessageRef* ApplySendPacketAgenda(
        CMessageConnectionMessageRef& inputMessageRef,
        bool* outAgendaTouched);
    // anchor: launcher.exe:0x448a00
    // Lower submit helper beneath `0x448cf0`; source computes the final byte pointer/size directly
    // from raw inner `+0x0a/+0x0b/+0x0c..` storage.
    uint32_t SubmitMessageRefBytes(const CMessageConnectionMessageRef& messageRef);

public:
    CMessageConnectionPacketNameFamily packetNameFamily_ = CMessageConnectionPacketNameFamily::kUnknown;
    uintptr_t packetNameCallback_ = 0;
    bool packetizedMessagesEnabled_ = false;
    std::unique_ptr<CMessageConnectionCompletionHelperScaffold> connectCompletionHelper7c_;
    std::unique_ptr<CMessageConnectionCompletionHelperScaffold> closeCompletionHelper80_;
    std::unique_ptr<CMessageConnectionPacketAgenda> packetAgenda_;
    std::vector<uint8_t> lastReceivedPacketBodyBytesScaffold_;
    bool lastReceivedPacketHeaderlessScaffold_ = false;
    // Legacy fallback queue retained only as dormant compatibility scaffolding now that the
    // tightened `0x4490c0` receive tail consumes the auth/margin packet path locally.
};

struct CMarginConnectionLocalCompletionWorkItemScaffold;
struct CMarginConnectionBootstrapPrepStateA0Scaffold;
class CMarginConnectionBootstrapPrepStateOwner_0x443340;

// ============================================================
// CBaseMarginConnection class declaration
// ============================================================
// Current recovered intermediate base between `CMessageConnection` and the auth/margin startup
// leaf families.
class CBaseMarginConnection : public CMessageConnection {
public:
    // UNANCHORED: source-owned narrow intermediate-base ctor.
    CBaseMarginConnection();
    // UNANCHORED: source-owned narrow intermediate-base ctor that only seeds the recovered engine.
    explicit CBaseMarginConnection(CLTThreadPerClientTCPEngine* connectionEngine);
    // UNANCHORED: source-owned default intermediate-base destructor.
    ~CBaseMarginConnection() override;

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
    uint32_t SendStoredBootstrapReplyCopy98();
    // anchor: launcher.exe:0x4429b0 / 0x439840 / 0x41cf30
    // Source-owned mirror of the consumed decoded-code-2 CERT challenge-response send.
    uint32_t SendCertChallengeResponseFromChallengeBytes(
        const std::array<uint8_t, 16>& challengeBytes);
    // anchor: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
    // Narrow source-owned mirror of the consumed decoded-code-2/5 seed-byte writeback.
    void SetMessageCode5SeedBytes85(const std::array<uint8_t, 16>& value);
    // anchor family: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
    // Source-owned raw-pointer readback of the same recovered seed-byte block.
    const uint8_t* MessageCode5SeedBytes85Pointer() const;

    // anchor: launcher.exe:0x442d00
    // Intermediate base dispatch router shared by the auth and margin startup leaf families.
    // Current source now keeps the nearer helper-wrapper step explicit too:
    // - resolve the logical payload span from the incoming message object through the
    //   `0x41bc20/0x41bbb0`-style seam first
    // - then route decoded code `2/4/5` through the recovered direct helper families before the
    //   later connection/owner continuations
    virtual uint32_t DispatchMessage(void* messageRef);

    // anchor: launcher.exe:0x442d00 -> 0x442d9e -> 0x4429b0 (CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse)
    // Original signature: void __thiscall CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse(void *this, int param_1)
    //   param_1 = parsed message result object with message context at +0x14 (dword) and +0x18 (word)
    // Original: Decrypts challenge via prep object vtable+0x1c (0x437810 -> 0x468130),
    //   extracts 16 bytes via cls_0x4b6538 envelope, writes to this+0x85..0x91,
    //   ensures stream encryption module, sends response packet opcode 0x11 via vtable+0x24.
    // SOURCE DIVERGENCE: Current source takes raw bytes instead of message object,
    //   uses static helper instead of vtable dispatch, lacks cls_0x4b6538 envelope construction,
    //   uses helper methods instead of inline field writes and packet building.
    // See implementation in .cpp for full fidelity notes and TODOs.
    uint32_t HandleCode2ForBootstrap(
        const uint8_t* packetBytes,
        size_t packetSize);

    // Handle decoded code 4: set success flag, synthesize local completion work item, continue bootstrap.
    // anchor: launcher.exe:0x441850 - takes 2 params (this, parsed message object)
    uint32_t HandleCode4ForBootstrap(
        const uint8_t* packetBytes,
        size_t packetSize);

protected:
    // anchor: launcher.exe:0x442d00 -> vtable `+0x2c`
    // The recovered intermediate-base table still owns the `+0x2c(messageRef)` row. Keep the
    // post-copy seam thin here by forwarding the local message-ref scaffold straight into the
    // concrete base/leaf `DispatchMessage` body.
    uint32_t DispatchCopiedParsedPacketTailScaffold(
        CMessageConnectionMessageRef& messageRef) override;

private:
    friend class CMarginConnectionBootstrapPrepStateOwner_0x443340;

    // anchor: launcher.exe:0x441470 / 0x44da00 / 0x44daf0
    // Source-owned mirror of the lazy connection `+0x9c` packet-agenda module install/refresh
    // reached from the consumed code-2 challenge path after `+0x85..+0x94` is available.
    void EnsureStreamPacketEncryptionModuleFromSeed85();

    bool messageCode4SuccessFlag84_ = false;
    bool hasBootstrapReplyCopy98_ = false;
    std::array<uint8_t, 0x136> bootstrapReplyCopy98_{};
    std::unique_ptr<CMarginConnectionBootstrapPrepStateA0Scaffold> bootstrapPrepStateA0_; // original connection `+0xa0`; standalone helper `0x443340` allocates/stores the `0xe0` prep object here, and the first later original consumer is `0x4429b0 -> +0x1c / 0x437810`
    bool hasMessageCode5SeedBytes85_ = false;
    std::array<uint8_t, 16> messageCode5SeedBytes85_{};
    std::unique_ptr<CStreamPacketEncryptionModule> streamPacketEncryptionModule9c_;
};

// ============================================================
// CAuthStartupConnection class declaration
// ============================================================
// Source-owned leaf mirror of the auth-side startup child built at `0x41d170` and assigned
// vtable `0x004afef0` before the initial `connection->+0x1c(owner+0x5c)` call.
// Current constructor-side proof also keeps the owner split explicit here:
// - connection `+0xa4` = direct `CLTLoginMediator*`
// - extra launcher-bridge records remain separate source-owned sidecars only
// Keep the class name conservative in source for now:
// - the surrounding canonical docs still carry older naming on `0x004afef0`
// - but current static RE is strong that this is the auth-side leaf completion wrapper reached
//   through `0x449a70`, not just a generic base `CMessageConnection`
class CAuthStartupConnection : public CBaseMarginConnection {
public:
    // UNANCHORED: source-owned narrow leaf ctor.
    CAuthStartupConnection();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CAuthStartupConnection(CLTThreadPerClientTCPEngine* authEngine);
    ~CAuthStartupConnection();

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
    // Thin auth-side leaf override on top of `CBaseMarginConnection::DispatchMessage`:
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
    CLTThreadPerClientTCPEngine_WorkItemHeader header{};
};

static_assert(sizeof(CMarginConnectionLocalCompletionWorkItemScaffold) == 0x0c, "margin code-4 local completion work-item size mismatch");

class CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c {
public:
    // anchor family: launcher.exe:0x45de10 / 0x45d000 / data type `cls_0x4ba50c`
    uint32_t vftptr_0x0 = 0u;
    uint32_t mbr_0x4 = 0u;
    uint32_t mbr_0x8 = 0u;
    void* mbr_0xc = nullptr;
    uint32_t mbr_0x10 = 0u;
};
static_assert(sizeof(CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c) == 0x14, "bootstrap prep big-int object size mismatch");

class CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70 {
public:
    // anchor: launcher.exe:0x465d70
    void InitializeFromBootstrapBlocks(
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_1,
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_2,
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_3);

    uint32_t vftptr_0x0 = 0u;
    uint32_t field_0x4 = 0u;
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c field_0x8{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c field_0x1c{};
    std::array<uint8_t, 0x0c> gap_0x30{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c field_0x3c{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c field_0x50{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c field_0x64{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c mbr_0x78{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c mbr_0x8c{};
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c mbr_0xa0{};
    uint32_t field_0xb4 = 0u;
    uint32_t field_0xb8 = 0u;
    std::array<uint8_t, 0x08> gap_0xbc{};
};

static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0x8) == 0x08);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0x1c) == 0x1c);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0x3c) == 0x3c);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0x50) == 0x50);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0x64) == 0x64);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, mbr_0x78) == 0x78);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, mbr_0x8c) == 0x8c);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, mbr_0xa0) == 0xa0);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0xb4) == 0xb4);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70, field_0xb8) == 0xb8);
static_assert(sizeof(CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70) == 0xc4, "bootstrap prep state subobject +0x0c size mismatch");

class CMarginConnectionBootstrapPrepStateA0Scaffold {
public:
    // anchor: launcher.exe:0x443220 / object size `0xe0`
    CMarginConnectionBootstrapPrepStateA0Scaffold(
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_1,
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_2,
        const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_3,
        int param_4);
    // anchor: launcher.exe:0x443390
    ~CMarginConnectionBootstrapPrepStateA0Scaffold();
    // Later original use of the stored connection `+0xa0` object starts at
    // `0x4429b0`, which loads that pointer and calls prep-object vtable `+0x1c /
    // 0x437810`; current source deliberately stops at ctor/dtor/materialization
    // until that later consumer path is reimplemented from static-RE.

    uint32_t vftptr_0x0 = 0u;
    uint32_t mbr_0x4 = 0u;
    uint32_t mbr_0x8 = 0u;
    CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70 field_0xc{};
    uint32_t field_0xd0 = 0u;
    uint32_t field_0xd4 = 0u;
    uint32_t cls_0x4b3e18 = 0u;
    uint32_t field_0xdc = 0u;

    CMarginConnectionBootstrapPrepStateA0Scaffold(const CMarginConnectionBootstrapPrepStateA0Scaffold&) = delete;
    CMarginConnectionBootstrapPrepStateA0Scaffold& operator=(const CMarginConnectionBootstrapPrepStateA0Scaffold&) = delete;
};

static_assert(offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, field_0xc) == 0x0c);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, field_0xd0) == 0xd0);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, field_0xd4) == 0xd4);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, cls_0x4b3e18) == 0xd8);
static_assert(offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, field_0xdc) == 0xdc);
static_assert(sizeof(CMarginConnectionBootstrapPrepStateA0Scaffold) == 0xe0, "bootstrap prep state +0xa0 object size mismatch");

class CMarginConnectionBootstrapPrepStateOwner_0x443340 {
public:
    explicit CMarginConnectionBootstrapPrepStateOwner_0x443340(CBaseMarginConnection& connection)
        : connection_(connection) {}

    // anchor: launcher.exe:0x443340
    // Direct-call helper that allocates a fresh `0xe0` prep object from three child-side `0x14`
    // byte blocks and stores it at connection `+0xa0`.
    void StoreBootstrapPrepStateA0(
        const void* blockB0,
        const void* blockC4,
        const void* blockD8);

private:
    CBaseMarginConnection& connection_;
};

class CMarginConnection : public CBaseMarginConnection {
public:
    // UNANCHORED: source-owned narrow leaf ctor.
    CMarginConnection();
    // UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
    explicit CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine);
    // UNANCHORED: source-owned default destructor.
    // Current tighter static-RE split:
    // - live leaf teardown is through scalar-deleting-dtor wrappers at `0x41cf50/0x41cf80`
    // - `0x41ce80` is the separate connection `+0x98` reply-copy helper
    ~CMarginConnection();

    // anchor: launcher.exe:0x44af60
    // Later leaf override on top of the base `CMessageConnection::OnOperationCompleted` family.
    // Current best original order:
    // - call base `0x4490c0`
    // - if base returns 0, call owner `+0x188(this, workItem)`
    // - if that also returns 0, fall through to `0x448a60`
    // - only then read `workItem+0x04`
    // - if work type == 1, clear owner byte `+0xf14` then tear down through the connection object
    // - no leaf-local type-2 split; connect-status also flows through owner `+0x188`
    uint32_t OnOperationCompleted(void* workItem) override;

    // anchor: launcher.exe:0x44af20
    // Later leaf dispatch override on top of `CBaseMarginConnection::DispatchMessage`.
    // Current best original order:
    // - call `CBaseMarginConnection::DispatchMessage(this, messageRef)` (`0x442d00`)
    // - if that returns 0, call owner `+0x184(messageRef)`
    uint32_t DispatchMessage(void* messageRef) override;
};

}  // namespace mxo::liblttcp
