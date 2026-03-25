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
struct SessionCallbackHelper65cSketch {
    // Current best source-owned mirror of the lazy helper at owner `+0x65c`.
    // Concrete chain now in scope:
    // - owner vtable `+0x130` / `0x41f310` returns this helper
    // - owner vtable `+0x134` / `0x420d00` lazy-allocates it through `0x420ca0`
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
    // Current best recovered layout from launcher.exe:0x41f0a0 + launcher.exe:0x41ecd0 + launcher.exe:0x41eb80 + launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    // plus later owner-path uses like `0x43f300`, `0x41330`, `0x21a50`, and `0x20720`:
    // - `+0x00 .. +0x1f` = first inline 32-byte NUL-terminated string
    // - `+0x20 .. +0x3f` = second inline 32-byte NUL-terminated string
    // - `+0x40 .. +0x4f` = first copied 16-byte block
    // - `+0x50 .. +0x5f` = second copied 16-byte block
    // - `+0x60 .. +0x68` = embedded small-string object
    // - `+0x6c` = trailing byte/flag
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
    // Default branch copies this into owner `+0x94`, clears owner `+0xf4`, then moves to
    // helper/state `2`.
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
    // Active password-submit branch persists this `0xb4` selection/config snapshot and then
    // switches from state `3` to state `8`.
    uint32_t slotOrSelectionIndex00 = 0;            // input `+0x00`, must be `< 100`
    std::array<uint32_t, 4> block04{};             // input `+0x04 .. +0x13`
    std::array<uint32_t, 4> block14{};             // input `+0x14 .. +0x23`
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

struct ProcessLoginCredentialsInputSketch {
    // owner vtable `+0x120` / `0x41c3c0`
    // Real later branch-specific writer for the post-auth source block, but not the currently
    // proven default password-submit branch.
    // Current field read:
    // - `+0x00`  -> CharacterName (`owner +0x108`)
    // - `+0x24`  -> selected world-descriptor index / selector (`owner +0x12c`)
    // - `+0x2c .. +0x6f` -> 17 appearance/customization ids (`owner +0x134 .. +0x177`)
    // - `+0x70`  -> RealFirstName (`owner +0x178`)
    // - `+0x90`  -> RealLastName (`owner +0x198`)
    // - `+0xb0`  -> Background (`owner +0x1b8`)
    std::array<char, 0x20> string00{};              // input `+0x00 .. +0x1f` = CharacterName
    uint32_t field20 = 0;                           // input `+0x20`
    uint32_t field24 = 0;                           // input `+0x24`
    uint32_t field28 = 0;                           // input `+0x28`
    std::array<uint32_t, 8> dwords2c{};            // input `+0x2c .. +0x4b` = appearance ids 0..7
    std::array<uint32_t, 8> dwords4c{};            // input `+0x4c .. +0x6b` = appearance ids 8..15
    std::array<uint8_t, 4> bytes6c{};              // input `+0x6c .. +0x6f` = trailing appearance id 16
    std::array<char, 0x20> string70{};             // input `+0x70 .. +0x8f` = RealFirstName
    std::array<char, 0x20> string90{};             // input `+0x90 .. +0xaf` = RealLastName
    std::array<char, 0x20> stringB0{};             // input `+0xb0 .. +0xcf` = Background
};

// =============================================================================
// ILTLoginMediator - VTable 0x004b01c8 pure virtual interface
// =============================================================================
// This class provides a clean interface to the recovered launcher-side mediator
// structure based on vtable 0x004b01c8. It mirrors the original launcher behavior
// while keeping field names stable where they are string-backed or strongly implied
// by surrounding code.
//
// Canonical references:
// - ../../../../docs/launcher.exe/VTABLES/0x004b01c8.md
// =============================================================================

class ILTLoginMediator {
public:
    static ILTLoginMediator* Default;
    // virtual ~ILTLoginMediator();

    // +0x00
    virtual const char* GetName() = 0;
    // +0x04
    void Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine* networkEngineOverride);
    // +0x08
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
    void UnknownSlot12();
    // +0x38
    void UnknownSlot13();
    // +0x3c
    void UnknownSlot14();
    // +0x40
    virtual const SlotRecordState004b5328* GetCurrentSlotRecord() const = 0;
    // +0x44
    void UnknownSlot16();
    // +0x48
    void UnknownSlot17();
    // +0x4c
    void UnknownSlot18();
    // +0x50
    virtual void* BootstrapRaw08AuxHandle50() const = 0;
    // +0x54
    bool UnknownSlot20();
    // +0x58
    uint8_t UnknownSlot21();
    // +0x5c
    virtual const char* GetString2(const char* value) = 0;
    // +0x60
    virtual const char* GetString1(const char* value) = 0;
    // +0x64
    void UnknownSlot24();
    // +0x68
    void UnknownSlot25();
    // +0x6c
    void UnknownSlot26();
    // +0x70
    void UnknownSlot27();
    // +0x74
    void UnknownSlot28();
    // +0x78
    uint8_t UnknownSlot29();
    // +0x7c
    uint8_t UnknownSlot30();
    // +0x80
    uint8_t UnknownSlot31();
    // +0x84
    uint8_t UnknownSlot32();
    // +0x88
    uint8_t UnknownSlot33();
    // +0x8c
    // virtual bool HasState8PersistenceData8c() const = 0;
    // +0x90
    uint8_t UnknownSlot35();
    // +0x94
    // virtual void GetLoginData(char* buffer, size_t* outLength) const = 0;
    // +0x98
    void UnknownSlot37();
    // +0x9c
    void UnknownSlot38();
    // +0xa0
    void UnknownSlot39();
    // +0xa4
    void UnknownSlot40();
    // +0xa8
    void UnknownSlot41();
    // +0xac
    void UnknownSlot42();
    // +0xb0
    void UnknownSlot43();
    // +0xb4
    void UnknownSlot44();
    // +0xb8
    uint8_t UnknownSlot45();
    // +0xbc
    // virtual bool HasState8Section11Dword145c() const = 0;
    // +0xc0
    // virtual uint32_t GetState8Section11Dword145c() const = 0;
    // +0xc4
    const char* UnknownSlot50();
    // +0xc8
    void* UnknownSlot51();
    // +0xcc
    uint8_t UnknownSlot52();
    // +0xd0
    char* UnknownSlot53();
    // +0xd4
    void* UnknownSlot54();
    // +0xd8
    void UnknownSlot55();
    // +0xdc
    // virtual const char* GetSlotRecordHeapStringByIndex(uint32_t index) const = 0;
    // +0xe0
    // virtual uint32_t GetRouteHostPrefixBySlot(uint32_t index) const = 0;
    // +0xe4
    virtual uint8_t GetVariantState(int32_t variantIndex) const = 0;
    // +0xe8
    void UnknownSlot59();
    // +0xec
    virtual uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) = 0;
    // +0xf0
    // virtual void SetSelectionIndexAndSwitchToState7(uint8_t selectionIndex) = 0;
    // +0xf4
    // virtual const char* GetState8PersistenceF1c() const = 0;
    // +0xf8
    void UnknownSlot65();
    // +0xfc
    virtual const char* GetWorldNameByIndex(uint32_t index) = 0;
    // +0x100
    // virtual uint8_t GetDescriptorField18ByIndex(uint32_t index) const = 0;
    // +0x104
    // virtual uint8_t GetDescriptorField19ByIndex(uint32_t index) const = 0;
    // +0x108
    // virtual uint8_t GetDescriptorLowNibble1fByIndex(uint32_t index) const = 0;
    // +0x10c
    // virtual void* GetRouteDescriptor30() const = 0;
    // +0x110
    void UnknownSlot70();
    // +0x114
    void UnknownSlot71();
    // +0x118
    // virtual void* GetLateEntryList1470() const = 0;
    // +0x11c
    void UnknownSlot73();
    // +0x120
    virtual uint32_t ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input) = 0;
    // +0x124
    // Wrapper-facing capture of the deeper-init startup triple; owner-side mirroring stays
    // explicit in `CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c`.
    virtual void ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) = 0;
    // +0x128
    void UnknownSlot76();
    // +0x12c
    virtual void SetState9CallbackObjectTriple84_88_8c(void* callback, void* object, void* object2) = 0;
    // +0x130
    virtual void* GetSessionCallbackHelper65c() const = 0;
    // +0x134
    virtual SessionCallbackHelper65cSketch* EnsureSessionCallbackHelper65c() = 0;
    // +0x138
    // virtual void HelperSlot138_State16Dispatch() = 0;
    // +0x13c
    // virtual void HelperSlot13c_InvokeSessionHelperVtable4() = 0;
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
    void UnknownSlot88();
    // +0x168
    void UnknownSlot89();
    // +0x16c
    // virtual uint32_t HandleState9Opcode11SuccessSideEffect() = 0;
    // +0x170
    virtual bool RegisterLoginObserver(void* observer) = 0;
    // +0x174
    virtual bool UnregisterLoginObserver(void* observer) = 0;
    // +0x178
    virtual uint32_t GetLastLoginStatus() = 0;
    // +0x17c
    void UnknownSlot93();
    // +0x180
    void UnknownSlot94();
    // +0x184
    void UnknownSlot95();
    // +0x188
    void UnknownSlot96();
    // +0x18c
    // virtual void FillState9CallbackBlob18c() = 0;
    // +0x190
    // virtual void AppendRouteHostStringTriple(uint32_t index, const char* begin, const char* current) = 0;
    // virtual ~ILTLoginMediator() {}
};

}  // namespace ltlogin
}  // namespace mxo
