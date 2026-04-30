#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <array>
#include <cstring>
#include <vector>

namespace mxo {
namespace ltlogin {

// Forward declarations
class CLTLoginMediator;
class LaunchPadClient_0x4b0e48;

// Forward declarations for message ref classes
namespace liblttcp {
class CMessageConnectionMessageRef_0x4ba23c;
}

// Type definitions used in pure virtual methods
struct SelectionSeedConfig;

// anchor: launcher.exe:0x004b5328 / vtable
// anchor: launcher.exe:0x4398b0 / ctor
// anchor: launcher.exe:0x439910 / dtor
// Character slot record class.
//
// Static-RE tightening from launcher.exe owner slot accessors and the wrapper-side payload readers:
// - `0x004b5328` is a real `Packet_0x4af2a4` subclass, not a synthetic composition shell
// - it reuses the inherited packet fields directly:
//   - `debugString14`     = character name string
//   - `payloadSize18`     = slot-specific small length/state field used by the reset helper
//   - `packetType1a`      = slot status byte
//   - `characterIdLow1c`  = current character id low dword
//   - `characterIdHigh20` = current character id high dword
//   - `worldId24`         = selected world id
// - its virtual surface follows the shared `Packet_0x4af2a4` shape with slot-specific overrides
//   for the debug-string/reset helpers at `0x43dc80` / `0x439940`
class Packet_AsAuthReply_0x4b5328 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    ~Packet_AsAuthReply_0x4b5328() override = default;
    void DebugString(int /*formatType*/ = 2) override {}
    void InitializePayloadSize() override {}

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* characterName) {
        if (characterName == nullptr || payloadSize18 != 0u) {
            return;
        }

        size_t characterNameLength = 0u;
        const char* scan = characterName;
        while (*scan++) {
            ++characterNameLength;
        }

        const uint16_t reservedByteCount =
            ReserveLengthPrefixedTail(static_cast<uint16_t>(characterNameLength + 1u));
        if (reservedByteCount != 0u && debugString14 != nullptr) {
            char* const reservedWritePointer = const_cast<char*>(debugString14);
            strncpy(reservedWritePointer, characterName, reservedByteCount - 1u);
            reservedWritePointer[reservedByteCount - 1u] = '\0';
        }
    }
};

static_assert(sizeof(Packet_AsAuthReply_0x4b5328) == sizeof(mxo::liblttcp::Packet_0x4af2a4),
              "Packet_AsAuthReply_0x4b5328 should currently be a pure Packet_0x4af2a4-derived view");

// anchor: launcher.exe:0x4b5378 / vtable
// Margin message reply view class - used in state6 slot6 for margin reply handling.
// Creates a view into decoded margin message. Inherits from Packet_0x4af2a4.
// Used when receiving:
// - opcode 0x07 = MS_ConnectChallenge
// - opcode 0x09 = ConnectChallengeResponse
//
// MBR layout (from decompiler):
// - mbr_0x8: source CMessageConnectionMessageRef* (set from workItem param)
// - mbr_0xc: opcode flag byte (0x01 = success, 0x00 = pending)
// - mbr_0x10: computed payload pointer (inherited from Packet_0x4af2a4 as payloadAlias10)
class Packet_MarginConnectReplyView_0x4b5378 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // +0x0c: flag (reuses createRefParam0c from parent)
    // virtual methods inherited from Packet_0x4af2a4
};

// anchor: launcher.exe:0x4b533c / vtable
// anchor: launcher.exe:0x4399e0 / InitFromIncomingMessage
// anchor: launcher.exe:0x43c310 / InitializeFromTempRecord
// Recovered world-list packet/accessor object.
//
// Static-RE tightening from 0x4399e0 / 0x439b50 / 0x43c310 and the owner-side readers:
// - this is a real `Packet_0x4af2a4` child that reuses the inherited packet envelope fields
// - when used as a builder/reset object, `0x439b50` stamps payload opcode `0x35`
//   (`AS_GetWorldListRequest`) into inherited `payloadAlias10`
// - the mediator stores pointers to these packet-shaped descriptors in a fixed 100-entry table under owner `+0xd84`
//   and reads semantic fields decoded from payload offsets `+0x01..+0x1f`
class Packet_WorldList_0x4b533c : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    static constexpr uint8_t kPayloadTag35 = 0x35;
    static constexpr size_t kFixedByteCount = 0x06;

    // Recovered payload semantics from `0x43ded0` plus owner readers:
    // - payload + 0x01 = world id word
    // - payload + 0x03 = inline world-name string
    // - payload + 0x17 = world status byte
    // - payload + 0x18 = world type byte
    // - payload + 0x19 = server-version dword
    // - payload + 0x1d = server-language byte
    // - payload + 0x1e = private flag byte
    // - payload + 0x1f = population-level byte
    uint16_t worldId01 = 0;
    std::string inlineNamePlus03;
    uint8_t status17 = 0;
    uint8_t type18 = 0;
    uint32_t serverVersion19 = 0;
    uint8_t serverLanguage1d = 0;
    uint8_t privateFlag1e = 0;
    uint8_t populationLevel1f = 0;

    ~Packet_WorldList_0x4b533c() override = default;

    // anchor: launcher.exe:0x43ded0 / vtable +0x08
    void DebugString(int /*formatType*/ = 2) override {}

    // anchor: launcher.exe:0x439b50 / vtable +0x0c
    void InitializePayloadSize() override {
        payloadPtr04 = messageRef08 && messageRef08->messageStorage0c
            ? reinterpret_cast<uint32_t>(messageRef08->messageStorage0c->PayloadBase())
            : payloadPtr04;
        payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

        if (!messageRef08 || !messageRef08->messageStorage0c) {
            return;
        }

        messageRef08->GrowPayloadByteCount(kFixedByteCount);
        payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
        payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (!payload) {
            return;
        }

        payload[0] = kPayloadTag35;
        *reinterpret_cast<uint32_t*>(payload + 0x01) = 0u;
        payload[0x05] = 0u;
    }

    // anchor: launcher.exe:0x481760 / vtable +0x10
    void* GetPayloadBase() override { return payloadAlias10; }
};

static_assert(sizeof(Packet_WorldList_0x4b533c) >= sizeof(mxo::liblttcp::Packet_0x4af2a4),
              "Packet_WorldList_0x4b533c must retain the Packet_0x4af2a4 prefix");

// Wrapper-facing `ILTLoginMediator_0x4af2b8.Default` profile-path/current-slot ABI family.
// Keep this split explicit from the owner-side `CLTLoginMediator` helpers documented under
// Ghidra evidence currently points at the `0x403f90` family being old-MSVC2003
// `std::basic_string<char>` / `std::string`. Source therefore uses direct `std::string`
// storage and exposes semantic string views; the raw three-pointer layout view now lives only
// inside `src/launcher_mediator_abi.cpp` where the launcher/client ABI shim actually needs it.

inline const char* StringBeginOrNull(const std::string& value) {
    return value.empty() ? nullptr : value.c_str();
}

inline void StringResetAndAssignCString(std::string& value, const char* text) {
    // anchor: launcher.exe:0x403f20 = recovered basic_string reset/assign helper
    value = text ? text : "";
}

inline void StringAppendRange(std::string& value, const char* sourceBegin, const char* sourceEnd) {
    // anchor: launcher.exe:0x403dc0 = recovered append-range helper (Ghidra old name: meth_0x403dc0)
    if (sourceBegin == nullptr || sourceEnd == nullptr || sourceEnd < sourceBegin) {
        return;
    }
    value.append(sourceBegin, sourceEnd);
}

inline void StringAssignFromRange(std::string& value, const char* sourceBegin, const char* sourceEnd) {
    // anchor: launcher.exe:0x407dd0 = recovered basic_string assign-from-range helper
    if (sourceBegin == nullptr || sourceEnd == nullptr || sourceEnd < sourceBegin) {
        value.clear();
        return;
    }
    value.assign(sourceBegin, sourceEnd);
}

inline void StringReleaseStorage(std::string& value) {
    value.clear();
    value.shrink_to_fit();
}

inline int StringCompareNoCase(const std::string& value, const char* other) {
#if defined(_WIN32)
    return _stricmp(value.c_str(), other ? other : "");
#else
    return strcasecmp(value.c_str(), other ? other : "");
#endif
}


// SessionCallbackHelper65cSketch is now LaunchPadClient_0x4b0e48 (see launchpad.h)
// The session callback helper at CLTLoginMediator +0x65c is a LaunchPadClient instance.

class SubmitLoginRequestInput_0x407d50 {
public:
    // anchor: launcher.exe:0x407d50 helper / launcher.exe:0x41ecd0 consumer
    // Best current identity:
    // - concrete launcher submit-input class/layout used by `0x408400`
    // - non-polymorphic data carrier with one recovered helper method at `0x407d50`
    // - Ghidra/OOAnalyzer class identity is worth preserving even though the object behaves more
    //   like a packed request block than a rich business object
    // Live input shape confirmed:
    // - `+0x00` = username
    // - `+0x20` = password
    // - `+0x40` = key-config md5 block
    // - `+0x50` = ui-config md5 block
    // - `+0x60` = session-token string triple
    // - `+0x6c` = request flag byte
    // - on the happy path this runs while current helper is still state0
    // - the owner path copies this into owner `+0x94`, clears owner `+0xf4`, then moves to
    //   helper/state `2`
    // So state2 is the first post-submit helper here, not the startup default helper.
    struct SessionTokenString60 {
        const char* begin = nullptr;
        const char* current = nullptr;
        const char* capacity = nullptr;

        bool Empty() const { return begin == current; }
        void Clear() {
            begin = nullptr;
            current = nullptr;
            capacity = nullptr;
        }
    };

    std::array<char, 0x20> submitUsername{};          // input `+0x00 .. +0x1f`
    std::array<char, 0x20> submitPassword{};          // input `+0x20 .. +0x3f`
    std::array<uint8_t, 16> submitKeyConfigMd5Block40{}; // input `+0x40 .. +0x4f`
    std::array<uint8_t, 16> submitUiConfigMd5Block50{};  // input `+0x50 .. +0x5f`
    SessionTokenString60 submitSessionTokenString{};  // input `+0x60 .. +0x6b`
    uint8_t submitRequestFlag6c = 0;                  // input `+0x6c`

    bool HasUsername() const { return submitUsername[0] != '\0'; }
    bool HasPassword() const { return submitPassword[0] != '\0'; }
    bool HasSessionToken() const { return !submitSessionTokenString.Empty(); }

    // anchor: launcher.exe:0x407d50
    // Fidelity note: the original helper frees pooled or heap-backed storage for the session-token
    // triple. Our source model treats this input as non-owning, so the safe equivalent is to clear
    // the pointer triple without attempting to free foreign storage.
    void ReleaseSubmitSessionTokenStringStorage() {
        submitSessionTokenString.Clear();
    }
};

static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitUsername) == 0x00);
static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitPassword) == 0x20);
static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitKeyConfigMd5Block40) == 0x40);
static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitUiConfigMd5Block50) == 0x50);
static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitSessionTokenString) == 0x60);
static_assert(offsetof(SubmitLoginRequestInput_0x407d50, submitRequestFlag6c) == 0x6c);

// anchor: launcher.exe:0x41eb80
// Owner-side auth/bootstrap source block at owner +0x94.
// Has a method CopyFromSubmitLoginRequestInput that copies from the input to this block.
// Also has a separate session token string at +0xf4 (block offset +0x60).
class OwnerAuthBootstrapSource94 {
public:
    // Layout matches owner+0x94 block
    std::array<char, 0x20> username00{};    // +0x00
    std::array<char, 0x20> password20{};     // +0x20
    std::array<uint8_t, 16> keyConfigMd540{}; // +0x40
    std::array<uint8_t, 16> uiConfigMd550{};  // +0x50
    // +0x60: session token string (SmallString60 - 12 bytes)
    struct SmallString60 {
        const char* begin = nullptr;
        const char* current = nullptr;
        const char* capacity = nullptr;
    } sessionToken60;
    uint8_t flag6C = 0;                     // +0x6c
    // Total block size: 0x70 bytes (112 bytes)

    // Note: there's also a separate session token string at +0xf4 in the parent object
    // (owner +0x94 + 0x60 = owner +0xf4), cleared by the +0x30 path.

    // anchor: launcher.exe:0x41eb80
    // Copies from SubmitLoginRequestInput_0x407d50 to this block.
    // Uses delta-based byte copy matching the static-RE implementation.
    void CopyFromSubmitLoginRequestInput(const SubmitLoginRequestInput_0x407d50& input) {
        // Copy username (32 bytes)
        std::copy_n(input.submitUsername.begin(), 0x20, username00.begin());
        // Copy password (32 bytes)
        std::copy_n(input.submitPassword.begin(), 0x20, password20.begin());
        // Copy keyConfigMd5 block (16 bytes)
        std::copy_n(input.submitKeyConfigMd5Block40.begin(), 0x10, keyConfigMd540.begin());
        // Copy uiConfigMd5 block (16 bytes)
        std::copy_n(input.submitUiConfigMd5Block50.begin(), 0x10, uiConfigMd550.begin());
        // Copy session token string
        sessionToken60.begin = input.submitSessionTokenString.begin;
        sessionToken60.current = input.submitSessionTokenString.current;
        sessionToken60.capacity = input.submitSessionTokenString.capacity;
        // Copy flag
        flag6C = input.submitRequestFlag6c;
    }
};

// launcher.exe owner object `0x4f78b8` - active login / margin / load-character state.
// Keep only the source-owned field sketches needed by current code here; deeper evidence lives
// in the canonical docs.
struct State3SelectionContextInputSketch {
    // owner vtable `+0xec` / `0x41c1f0`
    // Active password-submit branch reaches this while the current helper is still state `3`.
    // Keep the ownership split explicit:
    // - state3 is the waiting helper on that branch
    // - the owner-side mediator method consumes this `0xb4` selection/config snapshot and then
    //   switches to state `8`
    // - do not model this as a state3-local slot-3 body
    //
    // Current bounded field recovery:
    // - `+0x00` is the launcher-selected high-8 selection index byte consumed directly by
    //   `0x41c390/0x41c1f0`
    //   - bounded evidence now lines up across both sides of the bridge:
    //     - launcher `0x40ec70` passes the selected row item-data high word to `+0xf0`
    //     - client `0x62170e2a..0x62170f48` stores arg7 high-8 into the first dword of the
    //       stack-local `0xb4` handoff immediately before arg6 `+0xec`
    // - the remaining `0xb0` bytes are still only partially understood globally
    // - `0x41c1f0` copies them monotonically into owner `+0xcd0..+0xd7f`, but state8 slot3 later
    //   serializes those owner blocks in a different packet order:
    //   - input `block04/block14/block24` -> packet `0x09/0x19/0x29`
    //   - input `block34/block44/block54/block64` -> packet `0x39/0x49/0x59/0x69`
    //   - input `block74/block84/block94/blockA4` -> packet `0x79/0x89/0x99/0xa9`
    // - negative result from `0x43bd20`: the fixed packet dwords at `0x01/0x05` do **not** come
    //   from `block04[0..1]`; state8 slot3 fetches those character id dwords separately from the
    //   current slot record through owner vtable `+0x44`
    // - newer direct-producer tightening from `client.dll:0x6211d3e0 + 0x62170e2a..0x62170f48` is
    //   stronger than the older bridge-side guess:
    //   - the client first zero-initializes the full `0xb4` handoff
    //   - then it only proves writes at `+0x00` and `+0x24..+0xa4`
    //   - practical consequence: the proven direct success-side path leaves both `block04` and
    //     `block14` zero
    // - current source-owned CLI bridge now mirrors that tighter direct-producer read:
    //   - `block04[0..3]` remain zero in the persisted snapshot
    //   - `block14[0..3]` remain zero in the persisted snapshot
    //   - recovered slot-record GCID/world/descriptor data are kept only as separate diagnostic
    //     shadow information, not as proven `+0xec` input semantics
    // - newer launcher/client bridge tightening narrows the remaining producer split further:
    //   - launcher-side selection UI now closes concretely through
    //     `0x40d6f0 = ILTLoginMediator_0x4af2b8_ResolveSelectionFromListCtrl`, which writes
    //     `CLauncher+0xa8/+0xac` (`0x4d3410/0x4d3414`) and persists `Last_WorldName`
    //   - the direct success-side caller into owner `+0xec` is then best read as
    //     `client.dll:0x62170e2a..0x62170f48 = InitClientDLL_BeginLoadingCharacterFlow`
    //     rather than a trivially xref-able launcher virtual call
    //   - that client branch zero-initializes a stack-local `0xb4` object, stores the arg7-derived
    //     high-8 selector in its first dword, fills later fields through selection-cfg loaders, and
    //     finally calls arg6 `+0xec`
    // - important negative result kept explicit:
    //   - row nodes built by `0x40e480` / repainted by `0x40e1c0` are only `0x48` bytes total
    //   - their payload covers packed low/high row indices, four visible strings, and one dword
    //     availability/sort field
    //   - that launcher-owned node layout therefore looks like UI display/sort state, not a direct
    //     in-place match for this mediator-owned `0xb4` commit block
    uint32_t slotOrSelectionIndex00 = 0;            // input `+0x00`, must be `< 100`
    std::array<uint32_t, 4> block04{};             // input `+0x04 .. +0x13`; zero on the proven direct client path
    std::array<uint32_t, 4> block14{};             // input `+0x14 .. +0x23`; zero on the proven direct client path
    std::array<uint32_t, 4> block24{};             // input `+0x24 .. +0x33`
    std::array<uint32_t, 4> block34{};             // input `+0x34 .. +0x43`
    std::array<uint32_t, 4> block44{};             // input `+0x44 .. +0x53`
    std::array<uint32_t, 4> block54{};             // input `+0x54 .. +0x63`
    std::array<uint32_t, 4> block64{};             // input `+0x64 .. +0x73`
    std::array<uint32_t, 4> block74{};             // input `+0x74 .. +0x83`
    std::array<uint32_t, 4> block84{};             // input `+0x84 .. +0x93`
    std::array<uint32_t, 4> block94{};             // input `+0x94 .. +0xa3`
    std::array<uint32_t, 4> blockA4{};             // input `+0xa4 .. +0xb3`
};

struct ProcessCreateCharacterInput120Sketch {
    // owner vtable `+0x120` / `0x41c3c0`
    // Current best read: create-character source-block submit.
    // Same semantic slot is reached from the wrapper-facing arg6 `+0x120` client call at
    // `client.dll:0x62054d1d`, even though that caller builds a larger stack-local object during
    // the loading-character/create-character transition.
    // Current concrete alignment between the client writer and owner reader:
    // - client initializes `+0x24`, `+0x2c..+0x6f`, `+0x70`, `+0x90`, and `+0xb0`
    // - owner `0x41c3c0` consumes exactly that subset, then switches helper state to `10`
    // - owner does not currently read `+0x20` or `+0x28`
    // Current field read:
    // - `+0x00`  -> CharacterName (`owner +0x108`)
    // - `+0x24`  -> selected world-descriptor index / selector (`owner +0x12c`)
    // - `+0x2c .. +0x6f` -> 17 appearance/customization ids (`owner +0x134 .. +0x177`)
    //   - newer `0x43c020` sender tightening gives the current best names/order:
    //     SkinToneID, BodyID, HeadID, HairID, HairColorID, TattooID, FacialHairID,
    //     FacialHairColorID, StartingHat, StartingGlasses, StartingShirt, StartingGloves,
    //     StartingCoat, StartingPants, StartingTights, StartingShoes, TraitID
    // - `+0x70`  -> RealFirstName (`owner +0x178`)
    // - `+0x90`  -> RealLastName (`owner +0x198`)
    // - `+0xb0`  -> Background (`owner +0x1b8`), with the current client caller copying up to
    //               `0x400` bytes here before the owner consumes it as a NUL-terminated string
    std::array<char, 0x20> string00{};              // input `+0x00 .. +0x1f` = CharacterName
    uint32_t field20 = 0;                           // input `+0x20`; currently not read by owner `0x41c3c0`
    uint32_t field24 = 0;                           // input `+0x24` = selected world-descriptor index / selector
    uint32_t field28 = 0;                           // input `+0x28`; currently not read by owner `0x41c3c0`
    std::array<uint32_t, 8> dwords2c{};            // input `+0x2c .. +0x4b` = appearance ids 0..7
    std::array<uint32_t, 8> dwords4c{};            // input `+0x4c .. +0x6b` = appearance ids 8..15
    std::array<uint8_t, 4> bytes6c{};              // input `+0x6c .. +0x6f` = trailing appearance id 16
    std::array<char, 0x20> string70{};             // input `+0x70 .. +0x8f` = RealFirstName
    std::array<char, 0x20> string90{};             // input `+0x90 .. +0xaf` = RealLastName
    std::array<char, 0x400> stringB0{};            // input `+0xb0 .. +0x4af` = Background source text
};

// =============================================================================
// ILTLoginMediator_0x4af2b8 - current source-owned arg6 surface
// =============================================================================
// Important fidelity note:
// - this header models the launcher-resolved `ILTLoginMediator_0x4af2b8.Default` surface consumed through
//   the runtime pointer slot at `0x4d2c58`
// - many slots align with the concrete owner object documented under vtable `0x004b01c8`, but they
//   should not be treated as a one-to-one synonym
// - newer destructor/raw-vtable RE now gives the abstraction itself a better static anchor too:
//   `0x004af2b8` is a purecall-backed abstract base candidate immediately above
//   `0x004b01c8 = CLTLoginMediator`, which strongly fits the source-side `ILTLoginMediator_0x4af2b8` role
// - in particular, launcher startup `0x40a380` calls the resolved arg6 slot `+0x08`, while owner
//   vtable `0x004b01c8 +0x0c` is currently recovered as `0x41f510` reset/clear logic rather than a
//   simple engine setter
//
// Canonical references:
// - ../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_0x4af2b8_Default.md
// - ../../../../docs/launcher.exe/VTABLES/0x004af2b8.md
// - ../../../../docs/launcher.exe/VTABLES/0x004b01c8.md
// =============================================================================

class ILTLoginMediator_0x4af2b8 {
public:
    static ILTLoginMediator_0x4af2b8* Default;
    // Important wrapper/owner split correction:
    // - this abstract base models the resolved `ILTLoginMediator_0x4af2b8.Default` arg6 surface
    // - do not project owner-only `CLTLoginMediator` slots back onto this interface just because
    //   nearby concrete rows share related data
    // - in particular, owner `+0x40 = 0x41f2e0 = GetSlotRecordByIndex` is **not** the abstract
    //   selection `+0x40` row; the wrapper-facing selection `+0x40` slot remains the separate
    //   selection-descriptor object family consumed by the client profile-path builder

    // +0x00
    virtual const char* GetName() = 0;
    // +0x04
    // virtual ~ILTLoginMediator_0x4af2b8();
    // +0x08
    // anchor: launcher.exe:0x41b160
    // Return: status dword (0x12000001 if auth address list empty, 0 if has entries)
    virtual uint32_t Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* networkEngineOverride) = 0;
    // +0x0c
    virtual void ClearEngine() = 0;
    // +0x10
    virtual uint32_t IsReady() = 0;
    // +0x14
    void UnknownSlot5();
    // +0x18 anchor: launcher.exe:0x41f050 = vtable slot +0x18
    virtual uint8_t GetUnknownByte05() const = 0;
    // +0x1c
    virtual void SetValue1(void* value) = 0;
    // +0x20
    void UnknownSlot8();
    // +0x24
    virtual void SetValue2(void* value) = 0;
    // +0x28
    void UnknownSlot10();
    // +0x2c
    virtual uint32_t IsConnected() = 0;
    // +0x30
    virtual uint32_t ProcessLoginRequest(const SubmitLoginRequestInput_0x407d50& input) = 0;
    // +0x34
    virtual void RequestAuthCloseAndSwitchToState0() = 0;
    // +0x38
    virtual const char* GetUsername() const = 0;
    // +0x3c
    virtual uint32_t GetDefaultSelectionIndex() const = 0;
    // +0x40
    virtual Packet_AsAuthReply_0x4b5328* GetAuthReplyPacketByIndex40(
        uint32_t selectionIndex) = 0;
    // +0x44
    virtual Packet_AsAuthReply_0x4b5328* GetCurrentAuthReplyPacket44() = 0;
    // +0x48
    virtual const char* GetWorldOrSelectionName() const = 0;
    // +0x4c
    virtual const char* GetProfileOrSessionName() const = 0;
    // +0x50
    virtual void* BootstrapRaw08AuxHandle50() const = 0;
    // +0x54
    virtual bool HasBootstrapRaw08AuxHandle54() const = 0;
    // +0x58
    virtual uint8_t GetCrashReporterPromptForSecurId58() const = 0;
    // +0x5c
    virtual const char* GetCrashReporterUsername5c(const void* chainedValueToken) = 0;
    // +0x60
    virtual const char* GetCrashReporterPassword60(const void* chainedValueToken) = 0;
    // +0x64
    virtual uint32_t GetBootstrapSuccessHeaderDword64() const = 0;
    // +0x68
    virtual uint32_t HasLiveHlCfg68() const = 0;
    // +0x6c
    virtual uint32_t HasLiveAnCfg6c() const = 0;
    // +0x70
    virtual uint32_t HasLivePiCfg70() const = 0;
    // +0x74
    virtual uint32_t HasLiveAiCfg74() const = 0;
    // +0x78
    virtual uint32_t HasLiveCsCfg78() const = 0;
    // +0x7c
    virtual uint32_t HasLiveBlCfg7c() const = 0;
    // +0x80
    virtual uint32_t HasLiveIlCfg80() const = 0;
    // +0x84
    virtual uint32_t HasLiveRlCfg84() const = 0;
    // +0x88
    virtual uint32_t HasLiveClCfg88() const = 0;
    // +0x8c
    virtual uint32_t HasState8PersistenceData8c() const = 0;
    // +0x90
    virtual uint32_t HasLiveCuiCfg90() const = 0;
    // +0x94
    virtual void* GetLiveHlCfg94(uint32_t* outLength) const = 0;
    // +0x98
    virtual void* GetLiveAnCfg98(uint32_t* outLength) const = 0;
    // +0x9c
    virtual void* GetLivePiCfg9c(uint32_t* outLength) const = 0;
    // +0xa0
    virtual void* GetLiveAiCfgA0(uint32_t* outLength) const = 0;
    // +0xa4
    virtual void* GetLiveCsCfgA4(uint32_t* outLength) const = 0;
    // +0xa8
    virtual void* GetLiveBlCfgA8(uint32_t* outLength) const = 0;
    // +0xac
    virtual void* GetLiveIlCfgAc(uint32_t* outLength) const = 0;
    // +0xb0
    virtual void* GetLiveRlCfgB0(uint32_t* outLength) const = 0;
    // +0xb4
    virtual void* GetLiveClCfgB4(uint32_t* outLength) const = 0;
    // +0xb8
    virtual void* GetLiveCuiCfgB8(uint32_t* outLength) const = 0;
    // +0xbc
    virtual const void* GetState8PersistenceHeaderBc() const = 0;
    // +0xc0
    virtual const void* GetState8PersistenceBodyC0() const = 0;
    // +0xc4
    virtual void* GetState8PersistenceOverflowC4(uint16_t* outLength) const = 0;
    // +0xc8
    virtual uint32_t HasState8Section11Dword145c() const = 0;
    // +0xcc
    virtual uint32_t GetState8Section11Dword145c() const = 0;
    // +0xd0
    virtual std::string_view GetState8Section11String1460() const = 0;
    // +0xd4
    virtual const void* GetState9CallbackSeedPointer85D4() const = 0;
    // +0xd8
    virtual uint32_t GetArg7SelectionUpperBoundExclusive() const = 0;
    // +0xdc
    virtual const char* MapSelectionName(uint32_t selectionHighByte) const = 0;
    // +0xe0
    virtual const char* GetVariantWorldName(uint32_t variantIndex) = 0;
    // +0xe4
    // anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 and page-7 row builder at 0x40e480
    // High-word active-selection / slot-record status reader; valid matched rows map directly to
    // owner-side slot-record status, while negative / fallback indices stay wrapper-defined.
    virtual uint8_t GetSlotRecordStatusBySelectionIndex(int32_t selectionIndex) const = 0;
    // +0xe8
    virtual uint32_t RemoveSlotRecordAndCompactRouteStateByIndex(uint32_t selectedSlotRecordIndex) = 0;
    // +0xec
    virtual uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) = 0;
    // +0xf0
    virtual uint32_t SetSelectionIndexAndSwitchToState7(uint32_t selectedSlotRecordIndex) = 0;
    // +0xf4
    virtual const void* GetState8PersistenceF1c() const = 0;
    // +0xf8
    virtual uint32_t GetWorldCount() const = 0;
    // +0xfc
    virtual const char* GetWorldNameByIndex(uint32_t index) = 0;
    // +0x100
    virtual uint8_t GetWorldSelectionGateByteByIndex(uint32_t index) const = 0;
    // +0x104
    virtual uint8_t GetWorldTypeByteByIndex(uint32_t index) const = 0;
    // +0x108
    virtual uint8_t GetWorldPopulationNibbleByIndex(uint32_t index) const = 0;
    // +0x10c
    virtual std::string_view GetRouteDescriptor30() const = 0;
    // +0x110
    void UnknownSlot68();
    // +0x114
    void UnknownSlot69();
    // +0x118
    virtual const std::vector<std::string>& GetLateEntryList1470() const = 0;
    // +0x11c
    void UnknownSlot71();
    // +0x120
    virtual uint32_t ProcessCreateCharacterInput120(const ProcessCreateCharacterInput120Sketch& input) = 0;
    // +0x124
    virtual void ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) = 0;
    // +0x128
    void UnknownSlot74();
    // +0x12c
    void UnknownSlot75();
    // +0x130
    virtual LaunchPadClient_0x4b0e48* GetLaunchPadClient65c() const = 0;
    // +0x134
    virtual LaunchPadClient_0x4b0e48* EnsureLaunchPadClient65c() = 0;
    // +0x138
    void UnknownSlot78();
    // +0x13c
    virtual void HelperSlot13c_InvokeSessionHelperVtable4() = 0;
    // +0x140
    void UnknownSlot80();
    // +0x144
    void UnknownSlot81();
    // +0x148
    virtual const char* GetGameSessionId() const = 0;
    // +0x14c
    virtual void SetSharedMarginPacketField660(uint32_t value) = 0;
    // +0x150
    void UnknownSlot84();
    // +0x154
    void UnknownSlot85();
    // +0x158
    virtual uint32_t SetState9OptionalField90AndSwitchToState13(uint32_t value) = 0;
    // +0x15c
    void UnknownSlot87();
    // +0x160
    void UnknownSlot88();
    // +0x164
    virtual bool RequestAuthConnectionCloseWaitEvent1() = 0;
    // +0x168
    void UnknownSlot90();
    // +0x16c
    virtual bool RequestMarginConnectionCloseWaitEvent0f() = 0;
    // +0x170
    virtual bool RegisterLoginObserver(void* observer) = 0;
    // +0x174
    virtual bool UnregisterLoginObserver(void* observer) = 0;
    // +0x178
    virtual uint32_t GetLastLoginStatus() = 0;
    // +0x17c
    virtual uint32_t HandleAuthConnectionCompletionFallback(void* connection, void* workItem) = 0;
    // +0x180
    virtual uint32_t DispatchCurrentHelperAuthMessage(void* workItem) = 0;
    // +0x184
    virtual uint32_t DispatchCurrentHelperSlot6(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) = 0;
    // +0x188
    virtual uint32_t HandleMarginConnectionCompletionFallback(void* connection, void* workItem) = 0;
    // +0x18c
    virtual uint32_t FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) = 0;
    // concrete owner-only `CLTLoginMediator` continues with `+0x190`
};

}  // namespace ltlogin
}  // namespace mxo
