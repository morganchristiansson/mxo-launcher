#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <vector>

namespace mxo {
namespace ltlogin {

// Type definitions used in pure virtual methods
struct Arg6SelectionConfig;
struct SlotRecordState004b5328 {
    // Current best source-owned mirror of the heap object allocated by `0x4398b0` and stored
    // under owner `+0x688[index]` by `0x4401a0`.
    // Address / vtable anchors:
    // - ctor/init: `0x4398b0`
    // - dtor: `0x439910`
    // - debug printer: `0x43dc80`
    // - payload reset/prepare: `0x439940`
    // - heap-string copy helper: `0x43aa80`
    // Current recovered fields of interest:
    // - object `+0x14` = heap string written by `0x43aa80`
    // - object `+0x10 + 0x03` = paired id low dword
    // - object `+0x10 + 0x07` = paired id high dword
    // - object `+0x10 + 0x0b` = status byte
    // - object `+0x10 + 0x0c` = world id word
    std::string heapString14;
    uint32_t globalCharacterIdLow03 = 0;
    uint32_t globalCharacterIdHigh07 = 0;
    uint8_t status0b = 0;
    uint16_t worldId0c = 0;
};

// Wrapper-facing `ILTLoginMediator.Default` profile-path/current-slot ABI family.
// Keep this split explicit from the owner-side `CLTLoginMediator` helpers documented under
// `0x004b01c8`:
// - wrapper `+0x40` returns a selection-descriptor object consumed by client profile-path code
// - wrapper `+0x44` returns a current-slot record object consumed by later save/profile code
// Those are not the same thing as the owner-side `+0x40/+0x44` slot-record table accessors.
struct __attribute__((packed)) Arg6SelectionDescriptor40PackedSketch {
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t reserved2;
    uint32_t field03;
    uint32_t field07;
};

struct Arg6SelectionDescriptor40ObjectSketch {
    void** vtable00;                           // +0x00 wrapper-owned tiny virtual surface
    void* bufferBase04;                        // +0x04 conservative object-local helper slot
    void* backingObject08;                     // +0x08 conservative object-local helper slot
    uint8_t flag0c;                            // +0x0c conservative presence/helper byte
    uint8_t padding0d[3];
    Arg6SelectionDescriptor40PackedSketch* packed; // +0x10
};

static_assert(offsetof(Arg6SelectionDescriptor40PackedSketch, field03) == 0x03);
static_assert(offsetof(Arg6SelectionDescriptor40PackedSketch, field07) == 0x07);
static_assert(offsetof(Arg6SelectionDescriptor40ObjectSketch, packed) == 0x10);
static_assert(sizeof(Arg6SelectionDescriptor40ObjectSketch) == 0x14);

struct __attribute__((packed)) Arg6CurrentSlotRecord44PayloadSketch {
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t reserved2;
    uint32_t characterIdLow03;
    uint32_t characterIdHigh07;
    uint8_t status0b;
    uint16_t worldId0c;
    uint8_t reserved0e;
    uint8_t reserved0f;
};

struct Arg6CurrentSlotRecord44ObjectSketch {
    void** vtable;
    void* bufferBase04;
    void* backingObject08;
    uint8_t flag0c;
    uint8_t padding0d[3];
    Arg6CurrentSlotRecord44PayloadSketch* payload10;
    const char* heapString14;
    uint16_t heapStringLen18;
    uint8_t padding1a[2];
};

static_assert(offsetof(Arg6CurrentSlotRecord44PayloadSketch, characterIdLow03) == 0x03);
static_assert(offsetof(Arg6CurrentSlotRecord44PayloadSketch, characterIdHigh07) == 0x07);
static_assert(offsetof(Arg6CurrentSlotRecord44PayloadSketch, status0b) == 0x0b);
static_assert(offsetof(Arg6CurrentSlotRecord44PayloadSketch, worldId0c) == 0x0c);
static_assert(offsetof(Arg6CurrentSlotRecord44ObjectSketch, payload10) == 0x10);
static_assert(offsetof(Arg6CurrentSlotRecord44ObjectSketch, heapString14) == 0x14);
static_assert(offsetof(Arg6CurrentSlotRecord44ObjectSketch, heapStringLen18) == 0x18);
static_assert(sizeof(Arg6CurrentSlotRecord44ObjectSketch) == 0x1c);

struct RouteDescriptor30SmallStringLikeSketch {
    // anchor: launcher.exe:0x41f2c0 / owner vtable `+0x10c`
    // Wrapper-facing late-runtime object shape returned through arg6 `+0x10c`.
    // Current client-side evidence only consumes the first two dwords as a small-string
    // begin/current pair, but keep the third dword explicit because the original getter returns a
    // full 3-dword string-like object at owner `+0x30`.
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
};

struct LateEntryList1470EntrySketch {
    // anchor: launcher.exe:0x41af50 / owner vtable `+0x118`
    // Newer tightening from `0x41f840 -> 0x41f640` plus client consumer `0x62017150`:
    // - owner `+0x1470` is a vector of 12-byte string-triple objects
    // - owner slot `+0x190 / 0x41f840` appends one entry by forwarding to
    //   `0x41f640 = StringTripleArray_Append`
    // - `0x41f640 / 0x41f3e0 / 0x41e410 / 0x41eb20` together show these entries own their copied
    //   character buffers rather than borrowing caller pointers
    // - later client code reads the first dword of each entry as a filename-like string and maps
    //   it through `FUN_622a9cf0` / `METR` metadata
    char* begin = nullptr;
    char* current = nullptr;
    char* capacity = nullptr;
};

struct LateEntryList1470VectorLikeSketch {
    // anchor: launcher.exe:0x41af50 / owner vtable `+0x118`
    // Wrapper-facing late-runtime object shape returned through arg6 `+0x118`.
    // The observer callback reads this as a begin/current/capacity triple over 12-byte entries.
    // Owner-side `+0x1470/+0x1474/+0x1478` is this same vector header.
    LateEntryList1470EntrySketch* begin = nullptr;
    LateEntryList1470EntrySketch* current = nullptr;
    LateEntryList1470EntrySketch* capacity = nullptr;
};

struct SessionCallbackHelper65cSketch {
    // Current best source-owned mirror of the lazy helper at owner `+0x65c`.
    // Concrete chain now in scope:
    // - owner vtable `+0x130` / `0x41f310` returns this helper
    // - owner vtable `+0x134` / `0x420d00` lazy-allocates it through `0x420ca0`
    // - owner vtable `+0x13c` / `0x4202c0` pumps helper vtable `+0x04` when present
    // - helper `+0x18` is a small-string object
    // - `0x421a50` refreshes that helper string from owner `+0x94 + 0x60`
    // - `0x420e70` then copies helper `+0x18` into owner `+0x664`
    void* owner10 = nullptr;            // helper `+0x10`
    std::string string18;               // helper `+0x18`
    uint32_t field24 = 0;               // helper `+0x24`
    uint32_t field28 = 0;               // helper `+0x28`
    uint8_t flag2C = 0;                 // helper `+0x2c`
    uint8_t flag2D = 0;                 // helper `+0x2d`
};

struct AuthBootstrapSelectedSource38Sketch {
    struct SmallStringLike60Sketch {
        // Same three-dword small-string family used by `0x407dd0`:
        // - `+0x00` = begin/data pointer
        // - `+0x04` = current/end pointer
        // - `+0x08` = capacity/end-of-storage pointer
        // launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch only consumes the first dword here as a raw `char*` for its arg9 path,
        // but `0x41eb80` proves the full embedded small-string object still lives here.
        const char* begin = nullptr;
        const char* current = nullptr;
        const char* capacity = nullptr;
    };

    // Current best concrete family returned by owner vtable `+0x38`:
    // - getter is tiny function `0x41f0a0 = lea eax,[ecx+0x94] ; ret`
    // - so this is an **embedded owner subobject at `0x4f78b8 + 0x94`**, not a separate heap object
    // - owner vtable `+0x30` / `0x41ecd0` then acts as the corresponding setter/consumer and
    //   uses `0x41eb80` to copy the same family into owner `+0x94`
    // - owner vtable `+0x150` / `0x41f270` is now also a direct first-string writer for the
    //   same block and copies up to `0x20` bytes into owner `+0x94`
    //
    // Current best recovered layout from launcher.exe:0x41f0a0 + launcher.exe:0x41ecd0
    // + launcher.exe:0x41eb80 + launcher.exe:0x439210, where state2 owns the handoff into the
    // owner `+0x680` bootstrap child, plus later owner-path uses like `0x43f300`, `0x41330`,
    // `0x21a50`, and `0x20720`:
    // - `+0x00 .. +0x1f` = first inline 32-byte NUL-terminated string
    // - `+0x20 .. +0x3f` = second inline 32-byte NUL-terminated string
    // - `+0x40 .. +0x4f` = first copied 16-byte block
    // - `+0x50 .. +0x5f` = second copied 16-byte block
    // - `+0x60 .. +0x68` = embedded small-string object
    // - `+0x6c` = trailing byte/flag
    //
    // Newer direct `0x439210 -> 0x448050` call-shape tightening now closes the active use of
    // this owner block too:
    // - arg1 = owner `+0x94 + 0x00`
    // - arg2 = owner `+0x94 + 0x20`
    // - arg5 = owner `+0x94 + 0x40`
    // - arg6 = owner `+0x94 + 0x50`
    // - arg8 = first dword / begin pointer from owner `+0x94 + 0x60`
    //
    // Newer semantic anchors on individual fields:
    // - `+0x00` first string:
    //   - owner vtable `+0x150` / `0x41f270` writes it directly
    //   - later auth-reply path `0x43f300 -> owner +0x150` feeds it from `0x43d480(...)`
    // - `+0x20` second string:
    //   - later copied into bootstrap `+0xf8` by `0x41330`
    //   - `0x41330 -> 0x456c40` validates it against a concrete slash+6-digit shape
    // - `+0x60` embedded small string:
    //   - empty case falls through a literal `"STATION"` default path in `0x489bc0`
    //   - non-empty case is later copied into owner `+0x65c + 0x18` through `0x21a50`
    //   - that same helper-local string is then copied onward into owner `+0x664`
    //     (`GameSessionID`) by `0x420e70` when helper flag `+0x2d` is clear
    //
    // Current best semantic read is therefore stronger than a generic auth blob but still
    // deliberately provisional on exact original class name:
    // an owner-side **station/bootstrap phase-2 auth source block** that later separate
    // LaunchPad/session helpers also touch.
    // Keep this on the mediator owner because it is shared state, not because the mediator and
    // LaunchPadClient are the same class.
    std::array<char, 0x20> inlineString00{};
    std::array<char, 0x20> inlineString20{};
    std::array<uint8_t, 16> block40{};
    std::array<uint8_t, 16> block50{};
    SmallStringLike60Sketch string60;
    std::string string60Owned;         // source-owned backing storage for the copied `+0x60` small-string mirror
    uint8_t flag6C = 0;
};

struct ProcessLoginRequestInputSketch {
    // owner vtable `+0x30` / `0x41ecd0 = CLTLoginMediator::ProcessLoginRequest`
    // Live input shape confirmed:
    // - `+0x00` = username
    // - `+0x20` = password
    // - on the happy path this runs while current helper is still state0
    // - the owner path copies this into owner `+0x94`, clears owner `+0xf4`, then moves to
    //   helper/state `2`
    // So state2 is the first post-submit helper here, not the startup default helper.
    std::array<char, 0x20> inlineString00{};     // input `+0x00 .. +0x1f` = username
    std::array<char, 0x20> inlineString20{};     // input `+0x20 .. +0x3f` = password
    std::array<uint8_t, 16> block40{};           // input `+0x40 .. +0x4f`
    std::array<uint8_t, 16> block50{};           // input `+0x50 .. +0x5f`
    AuthBootstrapSelectedSource38Sketch::SmallStringLike60Sketch string60; // input `+0x60 .. +0x68`
    uint8_t flag6C = 0;                          // input `+0x6c`
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
    //     `0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl`, which writes
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
// ILTLoginMediator - current source-owned arg6 surface
// =============================================================================
// Important fidelity note:
// - this header models the launcher-resolved `ILTLoginMediator.Default` surface consumed through
//   the runtime pointer slot at `0x4d2c58`
// - many slots align with the owner object documented under vtable `0x004b01c8`, but they should
//   not be treated as a one-to-one synonym
// - in particular, launcher startup `0x40a380` calls the resolved arg6 slot `+0x08`, while owner
//   vtable `0x004b01c8 +0x08` is currently recovered as `0x41f510` reset/clear logic rather than a
//   simple engine setter
//
// Canonical references:
// - ../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md
// - ../../../../docs/launcher.exe/VTABLES/0x004b01c8.md
// =============================================================================

class ILTLoginMediator {
public:
    static ILTLoginMediator* Default;
    // virtual ~ILTLoginMediator();

    // +0x00
    virtual const char* GetName() = 0;
    // +0x04
    // anchor: launcher.exe:0x41b160 / owner vtable +0x04
    // Current source-owned wrapper helper mirrors the highest-confidence startup effects of owner
    // initialize while keeping the resolved arg6 wrapper handoff distinct from owner vtable slot
    // numbering.
    void Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine* networkEngineOverride);
    // +0x08
    // Wrapper-facing startup handoff from `launcher.exe:0x40a380`; keep this distinct from owner
    // vtable `0x004b01c8 +0x08` while the wrapper/owner relation is still being tightened.
    virtual void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) = 0;
    // +0x0c
    virtual void ClearEngine() = 0;
    // +0x10
    virtual uint32_t IsReady() = 0;
    // +0x14
    void UnknownSlot5();
    // +0x18
    void UnknownSlot6();
    // +0x1c
    virtual void SetValue1(void* value) = 0;
    // +0x20
    virtual void SetValue2(void* value) = 0;
    // +0x24
    virtual uint32_t ProcessLoginRequest(const ProcessLoginRequestInputSketch& input) = 0;
    // +0x28
    void UnknownSlot9();
    // +0x2c
    virtual uint32_t IsConnected() = 0;
    // +0x30
    virtual const SlotRecordState004b5328* GetSlotRecordByIndex(uint8_t index) const = 0;
    // +0x34
    virtual void RequestAuthCloseAndSwitchToState0() = 0;
    // +0x38
    virtual const char* GetProfileRootName() const = 0;
    // +0x3c
    virtual uint32_t GetDefaultSelectionIndex() const = 0;
    // +0x40
    virtual Arg6SelectionDescriptor40ObjectSketch* GetArg6SelectionDescriptorObject40(uint32_t selectionIndex) = 0;
    // +0x44
    virtual Arg6CurrentSlotRecord44ObjectSketch* GetArg6CurrentSlotRecordObject44() = 0;
    // +0x48
    virtual const char* GetWorldOrSelectionName() const = 0;
    // +0x4c
    virtual const char* GetProfileOrSessionName() const = 0;
    // +0x50
    virtual void* BootstrapRaw08AuxHandle50() const = 0;
    // +0x54
    // launcher.exe:0x41f0b0 is a tiny bool wrapper over `+0x50`.
    virtual bool HasBootstrapRaw08AuxHandle54() const = 0;
    // +0x58
    virtual uint8_t GetCrashReporterPromptForSecurId58() const = 0;
    // +0x5c
    virtual const char* GetCrashReporterUsername5c(const void* chainedValueToken) = 0;
    // +0x60
    virtual const char* GetCrashReporterPassword60(const void* chainedValueToken) = 0;
    // +0x64
    void UnknownSlot24();
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
    virtual RouteDescriptor30SmallStringLikeSketch* GetState8Section11String1460() = 0;
    // +0xd4
    virtual const void* GetState9CallbackSeedPointer85D4() const = 0;
    // +0xd8
    virtual uint32_t GetArg7SelectionUpperBoundExclusive() const = 0;
    // +0xdc
    virtual const char* MapSelectionName(uint32_t selectionHighByte) const = 0;
    // +0xe0
    virtual const char* GetVariantWorldName(uint32_t variantIndex) = 0;
    // +0xe4
    virtual uint8_t GetVariantState(int32_t variantIndex) const = 0;
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
    virtual RouteDescriptor30SmallStringLikeSketch* GetRouteDescriptor30() = 0;
    // +0x110
    void UnknownSlot70();
    // +0x114
    void UnknownSlot71();
    // +0x118
    virtual LateEntryList1470VectorLikeSketch* GetLateEntryList1470() = 0;
    // +0x11c
    void UnknownSlot73();
    // +0x120
    virtual uint32_t ProcessCreateCharacterInput120(const ProcessCreateCharacterInput120Sketch& input) = 0;
    // +0x124
    virtual void ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) = 0;
    // +0x128
    void UnknownSlot76();
    // +0x12c
    virtual void SetState9CallbackObjectTriple84_88_8c(void* callback, void* object, void* object2) = 0;
    // +0x130
    virtual SessionCallbackHelper65cSketch* GetSessionCallbackHelper65c() const = 0;
    // +0x134
    virtual SessionCallbackHelper65cSketch* EnsureSessionCallbackHelper65c() = 0;
    // +0x138
    // virtual void SwitchToState18IfLaunchPadGateState16State18Set() = 0;
    // +0x13c
    virtual void HelperSlot13c_InvokeSessionHelperVtable4() = 0;
    // +0x140
    // virtual void HelperSlot140_CreateStationLoginSideEffect() = 0;
    // +0x144
    void UnknownSlot80();
    // +0x148
    // anchor: launcher.exe:0x41f320 / owner vtable +0x148
    // Current replacement-wrapper cleanup now treats this as the shared `GameSessionID` getter.
    // Older wrapper-side `AttachRuntimeObject148` naming was a stale low-confidence guess.
    virtual const char* GetGameSessionId() const = 0;
    // +0x14c
    virtual void SetSharedMarginPacketField660(uint32_t value) = 0;
    // +0x150
    // virtual void ProcessCallbackData(const char* data) = 0;
    // +0x154
    void UnknownSlot84();
    // +0x158
    virtual uint32_t SetState9OptionalField90AndSwitchToState13(uint32_t value) = 0;
    // +0x15c
    void UnknownSlot86();
    // +0x160
    // virtual void ForwardThroughReadyMarginConnection24(void* workItem) = 0;
    // +0x164
    virtual bool RequestAuthConnectionCloseWaitEvent1() = 0;
    // +0x168
    void UnknownSlot89();
    // +0x16c
    virtual bool RequestMarginConnectionCloseWaitEvent0f() = 0;
    // +0x170
    virtual bool RegisterLoginObserver(void* observer) = 0;
    // +0x174
    virtual bool UnregisterLoginObserver(void* observer) = 0;
    // +0x178
    virtual uint32_t GetLastLoginStatus() = 0;
    // +0x17c
    // anchor: launcher.exe:0x41af80
    virtual uint32_t HandleAuthConnectionCompletionFallback(void* connection, void* workItem) = 0;
    // +0x180
    // anchor: launcher.exe:0x41f250
    virtual uint32_t DispatchCurrentHelperAuthMessage(void* workItem) = 0;
    // +0x184
    // anchor: launcher.exe:0x41f260
    virtual uint32_t DispatchCurrentHelperSlot6(void* workItem) = 0;
    // +0x188
    virtual uint32_t HandleMarginConnectionCompletionFallback(void* connection, void* workItem) = 0;
    // +0x18c
    virtual uint32_t FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) = 0;
    // +0x190
    // virtual void AppendLateEntryStringTriple1470(const LateEntryList1470EntrySketch* entry) = 0;
    // virtual ~ILTLoginMediator() {}
};

}  // namespace ltlogin
}  // namespace mxo
