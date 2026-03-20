#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"
#include "../../../runtime/src/libltmessaging/messageconnection.h"
#include "../../../runtime/src/liblttcp/ltthreadperclienttcpengine.h"

namespace mxo {
namespace ltlogin {

class CLTLoginState;
class CLTLoginState_AuthenticatePending;
class CLTLoginState_WorldListPending;

// Reimplementation note:
// This file is intended to mirror the concrete launcher-side login/controller structure
// now being recovered around global `0x4f78b8`.
// Keep names stable where they are string-backed or strongly implied by surrounding code,
// but keep uncertain field meanings clearly labeled in comments.
// Canonical runtime/cross-component references remain:
// - docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md
//
// Current best identity:
// - concrete launcher-side owner/controller rooted at global `0x4f78b8`
// - strong nearby string family: `CLTLoginMediator`, `CLTLoginMediator::PostEvent()`,
//   `CLTLoginMediator::PostError()`
// - recovered source-file string anchor: `\matrixstaging\game\src\libltclientlogin\loginmediator.cpp`
// - nearby recovered sibling file anchors:
//   - `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`
//   - `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`
// - current best read: this object owns the launcher-side auth/margin connection flow,
//   while `ILTLoginMediator.Default` remains the runtime interface slot passed into client.dll
// - discovered helper dispatch structure from Ghidra analysis of 0x43b300:
//   - `CLTLoginMediator_InitializeHelperDispatchTable` allocates 16 heap-allocated dispatch tables
//   - each table stores a function pointer to `LaunchPadClient_ProcessEvent0x17` (0x438d80)
//   - keep that symbol name as an analysis anchor only: it is a shared launcher-side event gate,
//     not evidence that the mediator object itself is a `LaunchPadClient`
//   - this is the launcher-side event handler system for auth/margin state transitions
// - recovered logging string anchors:
//   - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (logs "CLTLoginMediator::PostEvent(): Event# %d\n")
//   - launcher.exe:0x41d090 = CLTLoginMediator_PostError (calls PostError, logs "CLTLoginMediator::PostError(): Error# %d\n")
// - important current architectural split:
//   - keep low-level packet/crypto helpers under `src/auth/`
//   - keep launcher-owned auth/session state transitions here
//   - do not collapse `LaunchPadClient`-style pre-game account/subscription handling into the
//     direct auth TCP packet layer just because both live under `libltclientlogin`
class CLTLoginMediator {
public:
    static constexpr uint32_t kRecoveredWorldSlotCapacity = 100;

    // String-backed config anchors recovered from launcher/client registration code.
    static constexpr const char* kConfigQsAuthServerDnsName = "qsAuthServerDNSName";
    static constexpr const char* kConfigAuthServerPort = "AuthServerPort";
    static constexpr const char* kConfigMarginServerDnsSuffix = "MarginServerDNSSuffix";
    static constexpr const char* kConfigMarginServerPort = "MarginServerPort";
    static constexpr const char* kConfigIgnoreHostsFileForAuth = "IgnoreHostsFileForAuth";
    static constexpr const char* kConfigIgnoreHostsFileForMargin = "IgnoreHostsFileForMargin";

    // String-backed network/auth message anchors near the same owner paths.
    static constexpr const char* kMessageAsRouteToAuthServer = "AS_RouteToAuthServer";
    static constexpr const char* kMessageAsGetPublicKeyRequest = "AS_GetPublicKeyRequest";
    static constexpr const char* kMessageAsGetPublicKeyReply = "AS_GetPublicKeyReply";
    static constexpr const char* kMessageAsAuthRequest = "AS_AuthRequest";
    static constexpr const char* kMessageAsAuthReply = "AS_AuthReply";
    static constexpr const char* kMessageAsGetWorldListRequest = "AS_GetWorldListRequest";
    static constexpr const char* kMessageMsConnectRequest = "MS_ConnectRequest";

    // Current high-value raw auth-code anchors on the launcher-owned helper path:
    // - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (builds/sends raw code 0x06)
    //   -> strongest current `AS_GetPublicKeyRequest` candidate
    // - live diagnostic reply parsing now also confirms raw `0x07`
    //   -> `AS_GetPublicKeyReply`
    // - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (builds/sends raw code 0x08)
    //   -> strongest current `AS_AuthRequest` candidate
    // - launcher.exe:0x43b830 = CLTLoginMediator_Helper14_SendGetWorldListRequest (builds/sends raw code 0x35)
    //   -> later `AS_GetWorldListRequest`
    static constexpr uint8_t kAuthRawCodeGetPublicKeyRequest = 0x06;
    static constexpr uint8_t kAuthRawCodeGetPublicKeyReply = 0x07;
    static constexpr uint8_t kAuthRawCodeAuthRequest = 0x08;
    static constexpr uint8_t kAuthRawCodeGetWorldListRequest = 0x35;
    static constexpr const char* kMessageMsConnectReply = "MS_ConnectReply";
    static constexpr const char* kMessageMsLoadCharacterReply = "MS_LoadCharacterReply";

    // Recovered logging string anchors from launcher.exe:
    // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent
    //   logs: "CLTLoginMediator::PostEvent(): Event# %d\n"
    static constexpr const char* kLogPrefixPostEvent = "CLTLoginMediator::PostEvent():";
    // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (calls PostError)
    //   logs: "CLTLoginMediator::PostError(): Error# %d\n"
    static constexpr const char* kLogPrefixPostError = "CLTLoginMediator::PostError():";

    struct ConnectionHelperFamily {
        // launcher.exe:0x43b300 initializes a contiguous 15-slot helper/state array rooted at
        // `0x4f7868`, immediately after `0x4f78b8 = esi`.
        //
        // From Ghidra decompilation of 0x43b300 (CLTLoginMediator_InitializeHelperDispatchTable):
        // - Allocates the helper/state objects installed into 0x4f7868..0x4f78b4
        // - Those objects carry `CLTLoginState_*` vtables such as `0x4b4fc4`, `0x4b4fec`, `0x4b5014`
        // - slot 1 on many of those vtables points to `0x438d80`
        // - Helper functions InitializeHelperDispatchSlot15..Slot19 (0x420640..0x4209a0)
        //   set up PTR references to the event handler for additional slots at 0x4f78a4..0x4f78b4
        //
        // Discovered function names from Ghidra renaming:
        // - launcher.exe:0x438d80 = current shared launcher-side event gate symbolized as
        //   `LaunchPadClient_ProcessEvent0x17`
        // - launcher.exe:0x4816f0 = reused inline helper symbolized as
        //   `LaunchPadClient_GetVtableOffset` (anchor only; not a mediator-class identity claim)
        // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
        // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
        // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
        //
        // Disassembly of 0x438d80 shows:
        //   - Calls the helper currently named `LaunchPadClient_GetVtableOffset(this+8)` to get a
        //     vtable offset
        //   - Checks if event flag at [this+0x2c] is set
        //   - If event flag set, calls CLTLoginMediator_PostEvent(this, 1)
        //   - Otherwise calls vtable[+0x178]() and updates state at [this+0x80]
        //
        // Current highest-value slot anchors:
        // - slot 1 / `0x4f786c` / phase-code `1`
        //   - current best concrete state object: `CLTLoginState_State1` / vtable `0x4b4fc4`
        //   - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection starts auth connect through launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection
        // - slot 2 / `0x4f7870` / phase-code `2`
        //   - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap is the strongest current earlier credential/bootstrap auth lead
        //   - on the connected branch it reaches launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch, which then branches to:
        //     - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (builds/sends raw auth code 0x06)
        //       -> strongest current `AS_GetPublicKeyRequest` candidate
        //     - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (builds/sends raw auth code 0x08)
        //       -> strongest current `AS_AuthRequest` candidate
        // - slot 10 / `0x4f7890` / phase-code `10`
        //   - current best concrete state object: `CLTLoginState_State10` / vtable `0x4b512c`
        //   - launcher.exe:0x4401a0 = later state-10 incoming `AS_AuthReply` handler
        // - slot 14 / `0x4f78a0` / phase-code `14`
        //   - current best concrete state object: `CLTLoginState_WorldListPending` / vtable `0x4b4fec`
        //   - launcher.exe:0x43b830 = later `AS_GetWorldListRequest` sender
        void* helper7868 = nullptr;  // slot 0 / phase-code 0
        void* helper786C = nullptr;  // slot 1 / phase-code 1
        void* helper7870 = nullptr;  // slot 2 / phase-code 2
        void* helper7874 = nullptr;  // slot 3 / phase-code 3
        void* helper7878 = nullptr;  // slot 4 / phase-code 4
        void* helper787C = nullptr;  // slot 5 / phase-code 5
        void* helper7880 = nullptr;  // slot 6 / phase-code 6
        void* helper7884 = nullptr;  // slot 7 / phase-code 7
        void* helper7888 = nullptr;  // slot 8 / phase-code 8
        void* helper788C = nullptr;  // slot 9 / phase-code 9
        void* helper7890 = nullptr;  // slot 10 / phase-code 10
        void* helper7894 = nullptr;  // slot 11 / phase-code 11
        void* helper7898 = nullptr;  // slot 12 / phase-code 12
        void* helper789C = nullptr;  // slot 13 / phase-code 13
        void* helper78A0 = nullptr;  // slot 14 / phase-code 14
        void* helper78A4 = nullptr;  // slot 15 / phase-code 15 (FUN_00420640)
        void* helper78A8 = nullptr;  // slot 16 / phase-code 16 (FUN_004206e0)
        void* helper78AC = nullptr;  // slot 17 / phase-code 17 (FUN_00420850)
        void* helper78B0 = nullptr;  // slot 18 / phase-code 18 (FUN_00420920)
        void* helper78B4 = nullptr;  // slot 19 / phase-code 19 (FUN_004209a0)
    };

    struct MarginRouteState {
        // Current concrete inputs recovered from launcher `0x439300`:
        // - owner byte `+0xcc8`
        // - owner dword `+0x12c`
        // - owner dword `+0x104`
        // - owner vtable surfaces `+0xe0 / +0xfc / +0x10c`
        //
        // Keep the two dword names provisional for now:
        // the active disassembly only proves that `0x439300` forwards them into owner vtable
        // `+0xfc`, not that they are semantically settled world ids on every branch.
        uint8_t currentCharacterOrRouteIndex = 0;
        uint32_t pendingWorldId = 0;
        int32_t currentWorldId = -1;
        std::string routeHostPrefix;
        std::string exactMarginHostName;
    };

    struct MarginAddressListState {
        // Source-owned mirror of the `0x41e500` host-resolution family rooted at owner `+0x3c`.
        // Current best decompile-backed read:
        // - owner `+0x30` stores the current route/prefix string
        // - owner `+0x3c` owns the resolved IPv4 list object rebuilt through `0x440d80`
        // - owner `+0x7c` holds the transient selected IPv4 used to build `+0x6c`
        std::string resolvedHostName;
        std::vector<uint32_t> ipv4NetworkOrderList;
        size_t nextIndex = 0;
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

    struct AuthBootstrapState680Sketch {
        // Current best read of the extra owner child allocated through `0x41290` and stored at
        // owner `+0x680` by `0x41b160`.
        //
        // High-value phase-2 auth/bootstrap anchors:
        // - base ctor `0x45500`, size `0x11c`
        // - preparation/fill helper launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch
        // - branch condition at `0x44811e`: low-byte null test on dword field `+0xa0`
        //   - later `0x429b0` still uses that same field as a helper/pointer object via `+0x1c`
        //   - current best read therefore remains a helper/pointer family at `+0xa0`
        // - later challenge/crypto continuation `0x429b0`:
        //   - writes 16-byte material to `+0x85`
        //   - derives / caches the current/public key id at `+0x9c` via `0x41470`
        //
        // Current field sketch from launcher.exe:0x45500 + launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch + launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest + launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest:
        std::string string04;               // `+0x04`
        std::string string10;               // `+0x10`
        std::string string1C;               // `+0x1c`
        uint32_t loginType28 = 0;           // `+0x28`
        uint32_t launcherVersion2C = 0;     // `+0x2c`
        std::array<uint8_t, 16> block30{};  // `+0x30 .. +0x3f`
        std::array<uint8_t, 16> block40{};  // `+0x40 .. +0x4f`
        void* sendTarget50 = nullptr;       // `+0x50`
        uint32_t timestamp80 = 0;           // `+0x80`
        std::array<uint8_t, 16> material85{}; // `+0x85 .. +0x94`
        void* sideObject94 = nullptr;       // `+0x94`
        void* sideObject98 = nullptr;       // `+0x98`
        uint32_t currentPublicKeyId9C = 0;  // `+0x9c`
        void* helperA0 = nullptr;           // `+0xa0`
        void* lazyRaw06StateA4 = nullptr;   // `+0xa4`
        void* raw08AuxHandleA8 = nullptr;   // `+0xa8`
        uint32_t fieldAC = 0;               // `+0xac`
        uint32_t stateFlagEC = 1;           // `+0xec` from base ctor `0x45500`
        void* fieldF0 = nullptr;            // `+0xf0`
        void* fieldF4 = nullptr;            // `+0xf4`
        void* fieldF8 = nullptr;            // `+0xf8`
        void* fieldFC = nullptr;            // `+0xfc`
        void* field100 = nullptr;           // `+0x100`
        uint32_t field108 = 0;              // `+0x108`
        uint32_t field10C = 0;              // `+0x10c`
        uint32_t field110 = 0;              // `+0x110`
        uint32_t field114 = 0;              // `+0x114`
        uint32_t field118 = 0;              // `+0x118`
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

    struct ProcessLoginCredentialsInputSketch {
        // owner vtable `+0x120` / `0x41c3c0`
        // Real later branch-specific writer for the helper11 source block, but not the currently
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

    struct State8SelectionContextSnapshotState {
        // owner writeback area filled by `0x41c1f0` on the active state `3 -> 8` branch.
        // This is the persisted selection/config snapshot, not the helper11 appearance/name block.
        uint8_t slotOrSelectionIndexCc8 = 0;           // `+0xcc8`
        std::array<uint8_t, 7> paddingCc9{};           // `+0xcc9 .. +0xccf`
        std::array<uint32_t, 4> blockCd0{};            // `+0xcd0 .. +0xcdf`
        std::array<uint32_t, 4> blockCe0{};            // `+0xce0 .. +0xcef`
        std::array<uint32_t, 4> blockCf0{};            // `+0xcf0 .. +0xcff`
        std::array<uint32_t, 4> blockD00{};            // `+0xd00 .. +0xd0f`
        std::array<uint32_t, 4> blockD10{};            // `+0xd10 .. +0xd1f`
        std::array<uint32_t, 4> blockD20{};            // `+0xd20 .. +0xd2f`
        std::array<uint32_t, 4> blockD30{};            // `+0xd30 .. +0xd3f`
        std::array<uint32_t, 4> blockD40{};            // `+0xd40 .. +0xd4f`
        std::array<uint32_t, 4> blockD50{};            // `+0xd50 .. +0xd5f`
        std::array<uint32_t, 4> blockD60{};            // `+0xd60 .. +0xd6f`
        std::array<uint32_t, 4> blockD70{};            // `+0xd70 .. +0xd7f`
    };

    struct PostAuthMarginLoadingState {
        // owner post-auth margin/loading block shared by the active state8 path and the later
        // state10/state11 path.
        // Current high-value field read:
        // - `+0x108` = CharacterName
        // - `+0x12c` = selected world-descriptor index / selector
        // - `+0x134 .. +0x177` = appearance/customization ids
        // - `+0x178` = RealFirstName
        // - `+0x198` = RealLastName
        // - `+0x1b8` = Background
        // - `+0xf1c ...` = load-character reply materialization area
        std::array<char, 0x20> sourceLeadString108{};    // `+0x108 .. +0x127` = CharacterName
        uint32_t sourceField128 = 0;                     // `+0x128`
        uint32_t sourceField12c = 0;                     // `+0x12c` = selected world-descriptor index / selector on the active branch
        uint32_t sourceField130 = 0;                     // `+0x130`

        // raw `0x4d` sender field names from `0x43e540`:
        //  0: SkinToneID
        //  1: BodyID
        //  2: HeadID
        //  3: HairID
        //  4: HairColorID
        //  5: TattooID
        //  6: FacialHairID
        //  7: FacialHairColorID
        //  8: StartingHat
        //  9: StartingGlasses
        // 10: StartingShirt
        // 11: StartingGloves
        // 12: StartingCoat
        // 13: StartingPants
        // 14: StartingTights
        // 15: StartingShoes
        // 16: TraitID
        // `0x440320` also copies the first 8 dwords of this same span into `+0xf48`.
        std::array<uint32_t, 17> sourceDwords134{};      // `+0x134 .. +0x174`

        std::array<uint8_t, 0x20> sourceBlock178{};      // `+0x178 .. +0x197` = RealFirstName
        std::array<uint8_t, 0x20> sourceBlock198{};      // `+0x198 .. +0x1b7` = RealLastName
        std::array<uint8_t, 0x20> sourceBlock1b8{};      // `+0x1b8 .. +0x1d7` = Background

        // ========================================================================
        // Helper11 HandleLoadCharacterReply outputs (0x440320)
        // ========================================================================
        uint32_t worldListCountOrStatus80 = 0;           // `+0x80`

        // owner byte `+0xf14`; shared send gate used by the active state8 path and later state10.
        uint8_t state10SendGateFlagF14 = 1;             // `+0xf14`

        // Active state8 reply path prefers the current slot record (`+0x688[owner+0xcc8]`) as the
        // first-fragment seed for this name/world block, falling back to the older `+0x108` text.
        char characterNameBufferF1c[32] = {0};           // `+0xf1c .. +0xf3b`
        uint32_t characterReplyFieldF3c = 0;             // `+0xf3c`
        uint32_t characterReplyFieldF40 = 0;             // `+0xf40`
        std::array<uint32_t, 8> characterFlagsF48{};     // `+0xf48 .. +0xf67`
        std::array<uint32_t, 8> secondaryCharacterDataF68{}; // `+0xf68 .. +0xf87` (provisional world/status seed area)
        std::array<uint32_t, 10> characterRecordPointersF88{}; // helper11/scaffold parsed subview
        std::array<char, 0x20> section0StringF8c{};      // helper11/scaffold parsed subview
        std::array<char, 0x20> section0StringFac{};      // helper11/scaffold parsed subview
        std::array<char, 0x20> section0StringFcc{};      // helper11/scaffold parsed subview
        std::array<uint8_t, 0x465> state8Section0RawF88{}; // source-owned raw mirror of state8 `0x43f930` section-0 copy span

        // state8 case `0x00` also has a one-shot overflow tail at `+0x13f0/+0x13f4` when the
        // incoming section exceeds `0x485` bytes. Keep that separate from the append families
        // below because the original only allocates it once on the case-0 path instead of using
        // the later generic append buffers.
        void* state8Section0OverflowBuffer13f0 = nullptr; // `+0x13f0` (state8 case 0x00 overflow tail)
        uint16_t state8Section0OverflowLength13f4 = 0;    // `+0x13f4`

        // Allocated buffer pointers for load-character fragment families.
        // Keep the owner offsets explicit because state8 (`0x43f930`) and state11 (`0x440320`)
        // both reuse this wider owner region with different section selectors.
        void* allocatedBuffer13f8 = nullptr;             // `+0x13f8` (state8 case 0x01)
        uint16_t allocatedBufferLength13fc = 0;         // `+0x13fc`
        uint8_t flag13fe = 0;                            // `+0x13fe`

        void* allocatedBuffer1400 = nullptr;             // `+0x1400` (state8 case 0x02)
        uint16_t allocatedBufferLength1404 = 0;         // `+0x1404`
        uint8_t flag1406 = 0;                            // `+0x1406`

        void* allocatedBuffer1408 = nullptr;             // `+0x1408` (state8 case 0x06 / state11 case 0x06)
        uint16_t allocatedBufferLength140c = 0;         // `+0x140c`
        uint8_t allocatedBufferFlag140e = 0;             // `+0x140e`

        void* allocatedBuffer1410 = nullptr;             // `+0x1410` (state8 case 0x07)
        uint16_t allocatedBufferLength1414 = 0;         // `+0x1414`
        uint8_t flag1416 = 0;                            // `+0x1416`

        void* allocatedBuffer1418 = nullptr;             // `+0x1418` (state8 case 0x03 tail / state11 case 0x03)
        uint16_t allocatedBufferLength141c = 0;         // `+0x141c`
        uint8_t allocatedBufferFlag141e = 0;             // `+0x141e`

        void* allocatedBuffer1420 = nullptr;             // `+0x1420` (state8 case 0x04 / state11 case 0x04)
        uint16_t allocatedBufferLength1424 = 0;         // `+0x1424`
        uint8_t allocatedBufferFlag1426 = 0;             // `+0x1426`

        void* allocatedBuffer1428 = nullptr;             // `+0x1428` (state8 case 0x05 / state11 case 0x05)
        uint16_t allocatedBufferLength142c = 0;         // `+0x142c`
        uint8_t allocatedBufferFlag142e = 0;             // `+0x142e`

        void* allocatedBuffer1430 = nullptr;             // `+0x1430` (state8 case 0x0c)
        uint16_t allocatedBufferLength1434 = 0;         // `+0x1434`
        uint8_t flag1436 = 0;                            // `+0x1436`

        void* allocatedBuffer1438 = nullptr;             // `+0x1438` (state8 case 0x0d)
        uint16_t allocatedBufferLength143c = 0;         // `+0x143c`
        uint8_t flag143e = 0;                            // `+0x143e`

        void* allocatedBuffer1440 = nullptr;             // `+0x1440` (state8 case 0x08)
        uint32_t allocatedBufferLength1444 = 0;         // `+0x1444`
        uint8_t flag1448 = 0;                            // `+0x1448`

        void* allocatedBuffer144c = nullptr;             // `+0x144c` (state8 case 0x09)
        uint16_t allocatedBufferLength1450 = 0;         // `+0x1450`
        uint8_t flag1452 = 0;                            // `+0x1452`

        void* allocatedBuffer1454 = nullptr;             // `+0x1454` (state8 case 0x0a)
        uint16_t allocatedBufferLength1458 = 0;         // `+0x1458`
        uint8_t flag145a = 0;                            // `+0x145a`
        uint32_t state8Section10ChunkBitmap = 0;         // source-owned mirror of the original `DAT_004f79e4` bitmap

        // state8 case `0x0b` / `0x43f8c0` side effect:
        // - owner `+0x145c` = first dword of the section payload when byteCount > 4
        // - owner `+0x1460` = trailing small-string-like copy of the remaining payload bytes
        uint32_t state8Section11Dword145c = 0;          // `+0x145c`
        std::string state8Section11String1460;          // `+0x1460` small-string-like mirror

        // Additional fields for character reply parsing:
        std::array<uint8_t, 8> replyParseBuffer{};       // `+0x13cc .. +0x13d3` scratch family
        uint32_t replySectionData13cc = 0;               // `+0x13cc`
        uint32_t replySectionData13d0 = 0;               // `+0x13d0`
        uint8_t section0Flag13f6 = 0;                    // `+0x13f6`

        // +0xcc8 = character/route index byte (mirrored from auth reply)
        uint8_t characterRouteIndexCc8 = 0;              // `+0xcc8`
    };

    struct RouteHostStringTripleState {
        // Current best source-owned mirror of the owner `+0x818` family.
        // Evidence chain:
        // - owner vtable `+0xe0` / `0x41b260` returns the first dword of each 0x0c-byte slot
        //   when begin != current
        // - owner writer `0x41f840 = CLTLoginMediator_AppendRouteHostStringTriple`
        //   forwards into `0x41f640 = StringTripleArray_Append`
        // - `0x41f640` either copies one 3-dword string object directly or grows the backing
        //   array through `0x41f3e0 = StringTripleArray_GrowAndAppend`
        // Current best semantic read:
        // - one per-slot route/host prefix string consumed by the state-7/8/0x0d margin path.
        std::string text;
    };

    struct WorldDescriptorState004b533c {
        // Current best source-owned mirror of the `0x14`-byte heap object allocated by the
        // broader auth writer `0x43f300` and stored under owner `+0xd84[index]`.
        // Concrete class/vtable family now recovered from the ctor/reset/debug path:
        // - init from temp/source object: `0x43c310`
        // - dtor: `0x443aa0`
        // - debug printer: `0x43ded0`
        // - payload reset/prepare: `0x439a70`
        // Recovered payload semantics from `0x43ded0` plus owner readers:
        // - `payload + 0x01` = world id word
        // - `payload + 0x03` = inline world-name string
        // - `payload + 0x17` = world status byte
        // - `payload + 0x18` = world type byte
        // - `payload + 0x19` = server-version dword
        // - `payload + 0x1d` = server-language byte
        // - `payload + 0x1e` = private flag byte
        // - `payload + 0x1f` = population-level byte
        // Important broader-writer relation from `0x43f300`:
        // - this `+0xd84` world-descriptor table is built first from auth world data
        // - later `+0x688` character-slot records are built from auth character data
        // - then `+0x818` route-host strings are seeded by joining character `worldId0c`
        //   against descriptor `worldId01` and copying descriptor `inlineNamePlus03`
        uint16_t worldId01 = 0;
        std::string inlineNamePlus03;
        uint8_t status17 = 0;
        uint8_t type18 = 0;
        uint32_t serverVersion19 = 0;
        uint8_t serverLanguage1d = 0;
        uint8_t privateFlag1e = 0;
        uint8_t populationLevel1f = 0;
    };

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

    CLTLoginMediator();
    ~CLTLoginMediator();

    void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
    mxo::liblttcp::CLTThreadPerClientTCPEngine* NetworkEngine() const;

    void SetCurrentState(CLTLoginState* state);
    CLTLoginState* CurrentState() const;

    // anchor: launcher.exe:0x41b450
    // Recovered helper-state switcher:
    // - not just a raw assignment
    // - notifies the old helper with the new helper object
    // - installs the new helper from the dispatch table
    // - then notifies the new helper with the old helper object
    // Current source scaffold keeps the same transition boundary explicit even though the exact
    // old/new helper notification slots are still unresolved.
    void SwitchHelperStateScaffold(uint32_t helperStateId, CLTLoginState* state);
    uint32_t LastSwitchedHelperStateScaffold() const { return lastSwitchedHelperStateScaffold_; }

    // Narrow source-owned scaffolds for the launcher.exe logging/event side effects at
    // `0x41cfb0` / `0x41d090`.
    // Current implementation keeps these as lightweight source-owned event/error markers instead of
    // attempting the original listener container at owner `+0x674`.
    void PostEventScaffold(uint32_t eventId);
    void PostErrorScaffold(uint32_t errorId);
    uint32_t LastPostedEventScaffold() const { return lastPostedEventScaffold_; }
    uint32_t LastPostedErrorScaffold() const { return lastPostedErrorScaffold_; }
    const std::array<uint32_t, 8>& RecentPostedEventsScaffold() const { return recentPostedEventsScaffold_; }
    uint32_t RecentPostedEventCountScaffold() const { return recentPostedEventCountScaffold_; }

    // Narrow helper11 receive-boundary counters used only for short runtime discrimination:
    // - no packet arrived yet
    // - packet arrived but would be consumed by base margin dispatch before slot 6
    // - packet survived into current helper slot 6
    uint32_t MarginPacketReceiveCountScaffold() const { return marginPacketReceiveCountScaffold_; }
    uint32_t MarginPacketFilteredBeforeSlot6CountScaffold() const { return marginPacketFilteredBeforeSlot6CountScaffold_; }
    uint32_t MarginPacketSlot6DispatchCountScaffold() const { return marginPacketSlot6DispatchCountScaffold_; }
    uint16_t LastMarginPacketOpcodeScaffold() const { return lastMarginPacketOpcodeScaffold_; }
    uint32_t LastMarginPacketSizeScaffold() const { return lastMarginPacketSizeScaffold_; }

    // Source-owned scaffold registration for concrete CLTLoginState objects that live outside the
    // mediator header. This preserves the original helper-state ownership on the login-state
    // vtables while still letting the mediator switch between the active scaffold states.
    void RegisterScaffoldState3(CLTLoginState* state);
    void RegisterScaffoldState4(CLTLoginState* state);
    void RegisterScaffoldState6(CLTLoginState* state);
    void RegisterScaffoldState8(CLTLoginState* state);
    void RegisterScaffoldState9(CLTLoginState* state);
    void RegisterScaffoldState10(CLTLoginState* state);
    void RegisterScaffoldState11(CLTLoginState* state);
    void RegisterScaffoldState12(CLTLoginState* state);
    void RegisterScaffoldState13(CLTLoginState* state);
    // anchor: launcher.exe:0x41f1d0
    void SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c);
    CLTLoginState* ScaffoldState3() const;
    CLTLoginState* ScaffoldState4() const;
    CLTLoginState* ScaffoldState6() const;
    CLTLoginState* ScaffoldState8() const;
    CLTLoginState* ScaffoldState9() const;
    CLTLoginState* ScaffoldState10() const;
    CLTLoginState* ScaffoldState11() const;
    CLTLoginState* ScaffoldState12() const;
    CLTLoginState* ScaffoldState13() const;

    void SetAuthConnectionContextKey(void* contextKey);
    void SetMarginConnectionContextKey(void* contextKey);

    // Recovered config anchors:
    // - launcher `qsAuthServerDNSName` / `AuthServerPort`
    // - launcher `MarginServerDNSSuffix` / `MarginServerPort`
    // The replacement launcher should eventually populate these from the same launcher-owned
    // config path instead of treating connection setup as generic ad-hoc socket work.
    void SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile = false);
    void SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile = false);

    const std::string& AuthServerDnsName() const;
    uint16_t AuthServerPortHostOrder() const;
    bool IgnoreHostsFileForAuth() const;

    const std::string& MarginServerDnsSuffix() const;
    uint16_t MarginServerPortHostOrder() const;
    bool IgnoreHostsFileForMargin() const;
    std::string ResolvedMarginHostName() const;

    const mxo::liblttcp::LTTCPEndpointKey& AuthEndpoint() const;
    const mxo::liblttcp::LTTCPEndpointKey& MarginEndpoint() const;

    mxo::liblttcp::CMessageConnection* AuthConnection() const;
    mxo::liblttcp::CMessageConnection* MarginConnection() const;

    void SetMarginRouteState(uint8_t currentCharacterOrRouteIndex, uint32_t pendingWorldId, int32_t currentWorldId);
    void SetMarginRouteHostPrefix(const char* routeHostPrefix);
    void SetExactMarginHostName(const char* exactMarginHostName);
    const MarginRouteState& CurrentMarginRouteState() const;

    const ConnectionHelperFamily& Helpers() const;
    const AuthBootstrapState680Sketch& AuthBootstrap680() const;

    // launcher.exe:0x43b300
    // Current best read:
    // - allocates / initializes a contiguous 15-slot launcher-global helper/state array
    //   rooted at `0x4f7868 .. 0x4f78a0`
    // - this happens immediately after `0x4f78b8 = esi`
    // - the slot index and each helper's vtable `+0x18` phase-code getter now match across
    //   the recovered table (`0..14`)
    // - exact class names for most helper objects are still being recovered
    void InitializeConnectionHelpers();

    // HELPER DISPATCH TABLE INITIALIZATION HELPERS (from Ghidra analysis of 0x43b300):
    // ==============================================================================
    // launcher.exe:0x4f7868..0x4f78a0 = contiguous helper/state array (16 slots)
    // launcher.exe:0x420640..0x4209a0 = InitializeHelperDispatchSlot15..Slot19 for additional slots at 0x4f78a4..0x4f78b4
    // ==============================================================================
    // launcher.exe:0x420640 = InitializeHelperDispatchSlot15 (slot at 0x4f78a4)
    //   Original: allocates 8 bytes, stores vtable 0x4b51e0 (`CLTLoginState_State0`)
    // launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16 (slot at 0x4f78a8)
    //   Original: allocates 4 bytes, stores vtable 0x4b4fec (`CLTLoginState_WorldListPending`)
    // launcher.exe:0x420850 = InitializeHelperDispatchSlot17 (slot at 0x4f78ac)
    //   Original: allocates 4 bytes, stores vtable 0x4b4fc4 (`CLTLoginState_State1`)
    // launcher.exe:0x420920 = InitializeHelperDispatchSlot18 (slot at 0x4f78b0)
    //   Original: allocates 8 bytes, stores vtable 0x4b5014 (`CLTLoginState_AuthenticatePending`)
    // launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19 (slot at 0x4f78b4)
    //   Original: allocates 4 bytes, stores vtable 0x4b508c (`CLTLoginState_State6`)
    void InitializeHelperDispatchSlot15();
    void InitializeHelperDispatchSlot16();
    void InitializeHelperDispatchSlot17();
    void InitializeHelperDispatchSlot18();
    void InitializeHelperDispatchSlot19();
    // Slot anchors from Ghidra decompilation:
    // launcher.exe:0x4f7868 = slot 0, launcher.exe:0x4f78a0 = slot 1, launcher.exe:0x4f786c = slot 2,
    // launcher.exe:0x4f7870 = slot 3, launcher.exe:0x4f7874 = slot 4, launcher.exe:0x4f7878 = slot 5,
    // launcher.exe:0x4f787c = slot 6, launcher.exe:0x4f7880 = slot 7, launcher.exe:0x4f7884 = slot 8,
    // launcher.exe:0x4f7888 = slot 9, launcher.exe:0x4f7890 = slot 10, launcher.exe:0x4f7894 = slot 11,
    // launcher.exe:0x4f788c = slot 12, launcher.exe:0x4f7898 = slot 13, launcher.exe:0x4f789c = slot 14,
    // launcher.exe:0x4f78a4 = slot 15 (`CLTLoginState_State0`, vtable `0x4b51e0`)
    // launcher.exe:0x4f78a8 = slot 16 (`CLTLoginState_WorldListPending`, vtable `0x4b4fec`)
    // launcher.exe:0x4f78ac = slot 17 (`CLTLoginState_State1`, vtable `0x4b4fc4`)
    // launcher.exe:0x4f78b0 = slot 18 (`CLTLoginState_AuthenticatePending`, vtable `0x4b5014`)
    // launcher.exe:0x4f78b4 = slot 19 (`CLTLoginState_State6`, vtable `0x4b508c`)
    // ==============================================================================

    // Minimal placeholder accessors for recovered world/selection storage families.
    // These are not yet faithful data structures; they only preserve the currently recovered
    // slot-count / owner-shape in source instead of re-describing it in markdown.
    void* WorldSlot(uint32_t index) const;
    void* WorldPayloadSlot(uint32_t index) const;

    // launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (world list data provider)
    // Faithful implementation of arg6 world list provider for InitClientDLL
    // Vtable at offset +0xc from object pointer at 0x4d2c58
    void InitializeArg6DefaultObject();
    void ConfigureArg6Selection(
        uint32_t worldUpperBoundExclusive,
        uint32_t variantUpperBoundExclusive,
        const char* mappedSelectionName,
        const char* mappedVariantName,
        uint32_t selectedWorldIndexLow24,
        uint32_t selectedVariantIndexHigh8,
        uint32_t selectedWorldType,
        uint32_t selectedVariantState);
    void SetArg6ProfileName(const char* profileName);
    void SetArg6AuthName(const char* authName);
    void SetArg6AuthPassword(const char* authPassword);
    uint32_t Arg6WorldUpperBoundExclusive() const;
    uint32_t Arg6VariantUpperBoundExclusive() const;
    uint32_t Arg6SelectedWorldIndexLow24() const;
    uint32_t Arg6SelectedVariantIndexHigh8() const;
    uint32_t Arg6SelectedWorldType() const;
    uint32_t Arg6SelectedVariantState() const;
    uint32_t Arg6MappedSelectionId() const;
    const char* Arg6MappedSelectionName() const;
    const char* Arg6MappedVariantName() const;
    const char* Arg6ProfileName() const;
    const char* Arg6AuthName() const;
    const char* Arg6AuthPassword() const;
    bool Arg6WorldIndexMatchesSelection(uint32_t worldIndex) const;
    bool Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const;
    uint32_t Arg6ExpectedSelectionDescriptorScratchRequest() const;
    bool Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const;
    const char* Arg6GetWorldNameByIndex(uint32_t index);
    uint8_t Arg6GetWorldVariantByIndex(uint32_t index);
    uint8_t Arg6ValidateWorldSelection(uint8_t variant);
    uint32_t Arg6GetWorldListCount() const;
    uint32_t Arg6GetActiveWorldListCount() const;
    bool Arg6GetAvailableWorlds(uint32_t index) const;

    // Current best auth-side connection-init path:
    // - launcher `0x43909f -> 0x41d170`
    // - copies auth DNS into owner `+0x4c`
    // - reads auth port from recovered config state
    // - builds endpoint into owner `+0x5c`
    // - constructs auth-side CMessageConnection child at owner `+0x18`
    // - then calls `connection->+0x1c(owner+0x5c)`
    // Current best method mapping still treats that virtual `+0x1c` as the connection-
    // oriented ensure-connected / engine-Connect wrapper.
    uint32_t BeginAuthConnection();

    // Post-connect status handling is still owner/helper-state driven.
    // Current high-value summary:
    // - successful connect completion arrives as type-2 work `0x7000001`
    // - auth and margin derived connection families fall through wrappers into owner callback /
    //   current-state dispatch rather than being fully handled by optional helper slots alone
    // - active default password-submit continuation is now:
    //   `0x41ecd0 -> 0x41c1f0 -> 0x439300 -> 0x43bd20 / 0x43f930`
    // - helper11/state11 remains a later real branch after auth-reply handling, not the first
    //   active branch to prioritize
    // - the earlier auth bootstrap/send lead is still the helper2 / `0x448050` family, not the
    //   later world-list sender
    uint32_t HandleAuthConnectStatus(uint32_t workResultCode);
    uint32_t HandleMarginConnectStatus(uint32_t workResultCode);
    uint32_t BeginAuthHandshake();
    uint32_t BeginMarginHandshake();

    void SetAuthCredentials(const char* username, const char* password);
    void SetAuthBootstrapConfig(
        uint32_t launcherVersion,
        uint32_t currentPublicKeyId,
        uint8_t loginType,
        const std::vector<uint8_t>& keyConfigMd5,
        const std::vector<uint8_t>& uiConfigMd5);
    uint32_t HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize);
    // Newer `0x44af20 / 0x442d00 / 0x41f260` tightening now makes the helper11 receive boundary
    // explicit in source too:
    // - decoded margin codes `2`, `4`, and `5` are consumed by base margin dispatch
    // - only other codes survive into owner `+0x184` / current helper slot 6
    // - practical helper11 consequence: the first real `MS_LoadCharacterReply` candidate must
    //   arrive as raw code `0x10` *after* that base-dispatch filter
    uint32_t HandleMarginPacketBytes(const uint8_t* packetBytes, size_t packetSize);

    // Narrow owner-side parsers/stagers used by the concrete CLTLoginState slot-6 bodies.
    // Keep the packet/class ownership split explicit:
    // - state10 slot 6 / `0x4401a0` owns the auth-reply transition
    // - state11 slot 6 / `0x440320` owns the load-character reply transition
    // - mediator only keeps the staged bytes plus owner-state writeback helpers
    uint32_t HandleStagedAuthReplyPacketScaffold();
    uint32_t HandleStagedMarginLoadCharacterReplyPacketScaffold();
    const std::vector<uint8_t>& StagedIncomingAuthPacketBytes() const;
    const std::vector<uint8_t>& StagedIncomingMarginPacketBytes() const;

    const char* ExpectedAuthRequestName() const;
    const char* ExpectedMarginRequestName() const;

    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;

    // launcher.exe owner-vtable surfaces currently recovered from the state4 margin dispatcher.
    // Keep these helpers narrow and structural:
    // - `0x439300` decides which helper to call
    // - these mediator methods only mirror the owner-side route-string getters that feed `0x41e500`
    const char* ResolveMarginRouteFromCurrentCharacterSlot() const;            // current best anchor: owner vtable +0xe0
    const char* ResolveMarginRouteFromDescriptorIndex(uint32_t descriptorIndex) const; // current best anchor: owner vtable +0xfc when fed owner `+0x12c`
    const char* ResolveMarginRouteFromWorldId(uint32_t worldId) const;         // provisional fallback helper for world-id keyed route recovery
    const char* ResolveMarginRouteDescriptor() const;                          // current best anchor: owner vtable +0x10c first-dword fetch

    // Post-auth slot/route families recovered around helper10 (`0x4401a0`) and the later
    // state-8 margin dispatcher (`0x439300`).
    // anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
    const SlotRecordState004b5328* GetSlotRecordByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f300 / owner vtable +0x44
    const SlotRecordState004b5328* GetCurrentSlotRecord() const;
    // anchor: launcher.exe:0x41b220 / owner vtable +0xdc
    const char* GetSlotRecordHeapStringByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    void* GetSessionCallbackHelper65c() const;
    // anchor: launcher.exe:0x41f320 / owner vtable +0x148
    const char* GetGameSessionId664() const;
    // UNANCHORED: source-owned owner-field setter used by separate LaunchPad/session callback paths
    void SetGameSessionId664(const char* value);
    // anchor: launcher.exe:0x41f270 / owner vtable +0x150
    void SetLaunchPadSourceBlock94FirstString(const char* value);
    // anchor: launcher.exe:0x41f330 / owner vtable +0x14c
    void SetSharedMarginPacketField660(uint32_t value);
    // anchor: launcher.exe:0x420d00 / owner vtable +0x134
    SessionCallbackHelper65cSketch* EnsureSessionCallbackHelper65c();
    // anchor: launcher.exe:0x420e70
    uint32_t CommitSessionCallbackHelperGameSessionId664();
    // source-owned shared helper used by `CLTLoginState_State18` slot 3 / `0x421a50`
    uint32_t RefreshSessionHelperGameSessionId664FromSourceBlock94();
    // anchor: launcher.exe:0x41b260 / owner vtable +0xe0
    const char* GetRouteHostPrefixBySlot(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2a0 / owner vtable +0xe4? / current slot-record payload reader
    uint8_t GetSlotRecordStatusByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2e0 / owner vtable +0xfc
    const char* GetDescriptorInlineNameByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b320 / owner vtable +0x100
    uint8_t GetDescriptorField18ByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b360 / owner vtable +0x104
    uint8_t GetDescriptorField19ByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b3a0 / owner vtable +0x108
    uint8_t GetDescriptorLowNibble1fByIndex(uint8_t slotIndex) const;

    // =============================================================================
    // HELPER11: Post-Auth Margin/Loading State (launcher.exe:0x4f78b8)
    // =============================================================================
    // Recovered from Ghidra analysis of launcher.exe helper/state11 functions:
    // - 0x43c020 = CLTLoginState_State11 slot 3 send body
    //   Builds/sends margin packet with first payload byte 0x4d, posts event 0x15
    // - 0x440320 = CLTLoginState_State11 slot 6 reply body
    //   Handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
    //   posts event 0x16 on completion
    // =============================================================================

    // anchor: launcher.exe:0x41ecd0
    uint32_t ProcessLoginRequest(const ProcessLoginRequestInputSketch& input);

    // anchor: launcher.exe:0x41c1f0
    uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input);

    // anchor: launcher.exe:0x41c3c0
    uint32_t ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input);

    // Internal source-owned scaffolds for active CLTLoginState vtable bodies:
    // - owner callback84 secondary-message bridge shared by state8/state9 fallbacks and state12
    //   slot 6 / launcher.exe:0x41c5c0
    uint32_t DispatchSecondaryMessageToOwnerCallback84(void* workItem);
    // - state12-gated owner helper that stores `+0x90` and switches to state13 /
    //   launcher.exe:0x41c510
    uint32_t SetState9OptionalField90AndSwitchToState13(uint32_t field90Value);
    // - state9 slot 3 / launcher.exe:0x41de40 + 0x439780
    //   - newer natural-original WineDbg now proves `0x439780 -> 0x41de40`
    //   - representative live state there: helper byte `+4 = 0`, helper word `+6 = 0x2710`
    uint32_t State9SubmitFollowupScaffold(uint8_t helperByte4, uint16_t helperWord6);
    // - state9 slot 6 success side effect / launcher.exe:0x41b420 (owner vtable +0x16c)
    uint32_t HandleState9Opcode11SuccessSideEffect();
    // - state11 slot 6 / launcher.exe:0x440320
    uint32_t State11HandleLoadCharacterReplyScaffold(const uint8_t* packetBytes, size_t packetSize);

    // anchor: launcher.exe:0x41b4b0
    // State10 slot-3 precheck: owner `+0x1c` must exist and connection state `+0x34 == 2`.
    bool State10HasReadyConnectionState2() const;

    // anchor: launcher.exe:0x41af70
    // Original call shape is mediator-thiscall plus one stack-local packet-envelope object built by
    // helpers like `0x43ac10/0x43ada0`, then a tail jump to current margin connection vtable
    // `+0x24`.
    // Newer Ghidra tightening now makes that two-step downstream bridge more concrete:
    // - `+0x24` = `0x41cf30 = CMessageConnection_ForwardEnvelopeToSendPacket`
    // - that wrapper forwards the envelope's shared packet/message object into
    //   `+0x28` = inherited `CMessageConnection::SendPacket` / `0x448cf0`
    // - `0x448cf0` consumes a message/envelope object, not bare payload bytes
    // Current source helper is therefore intentionally narrower/scaffold-only: it now wraps the
    // recovered payload bytes in a source-owned envelope/message scaffold that matches the
    // original `0x448a00` byte-derivation shape more closely, but it still does not reconstruct
    // the original packet-envelope metadata / agenda semantics that sit between `0x43bd20` and a
    // later natural `0x43f930`.
    uint32_t SendCurrentMarginPacketScaffold(
        const mxo::liblttcp::CMessageConnectionEnvelopeScaffold& envelope);
    uint32_t SendCurrentMarginPacketScaffold(const void* packetBytes, uint32_t packetByteCount);

    // anchor: launcher.exe:0x41e500
    // Narrow reusable transport/init helper kept on the mediator after moving the `0x439300`
    // case split back into `CLTLoginState_State4::Slot3_BeginOrContinue`.
    // Preserved call contract from the original body:
    // - arg1 = route/prefix text used to refresh owner `+0x30`
    // - arg2 = cached non-zero selector that skips the route-refresh / address-list rebuild path
    uint32_t BeginMarginConnectionScaffold(const char* routeHostText, uint8_t cachedRouteSelector);
    const char* Arg6GetAvailableWorldName(uint32_t index);

    // Post-Auth Margin/Loading State Accessors (launcher.exe:0x4f78b8)
    // =============================================================================
    // These methods expose the owner fields recovered from Ghidra analysis of helper11.
    // They are used to faithfully reconstruct the original launcher's margin packet building
    // and load character reply handling logic.
    // =============================================================================

    // State-3 -> state-8 selection/config snapshot block (`0x41c1f0`):
    uint8_t SelectionContextSlotOrSelectionIndexCc8() const { return state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8; }
    const std::array<uint32_t, 4>& SelectionContextBlockCd0() const { return state8SelectionContextSnapshotState_.blockCd0; }
    const std::array<uint32_t, 4>& SelectionContextBlockCe0() const { return state8SelectionContextSnapshotState_.blockCe0; }
    const std::array<uint32_t, 4>& SelectionContextBlockCf0() const { return state8SelectionContextSnapshotState_.blockCf0; }
    const std::array<uint32_t, 4>& SelectionContextBlockD00() const { return state8SelectionContextSnapshotState_.blockD00; }
    const std::array<uint32_t, 4>& SelectionContextBlockD10() const { return state8SelectionContextSnapshotState_.blockD10; }
    const std::array<uint32_t, 4>& SelectionContextBlockD20() const { return state8SelectionContextSnapshotState_.blockD20; }
    const std::array<uint32_t, 4>& SelectionContextBlockD30() const { return state8SelectionContextSnapshotState_.blockD30; }
    const std::array<uint32_t, 4>& SelectionContextBlockD40() const { return state8SelectionContextSnapshotState_.blockD40; }
    const std::array<uint32_t, 4>& SelectionContextBlockD50() const { return state8SelectionContextSnapshotState_.blockD50; }
    const std::array<uint32_t, 4>& SelectionContextBlockD60() const { return state8SelectionContextSnapshotState_.blockD60; }
    const std::array<uint32_t, 4>& SelectionContextBlockD70() const { return state8SelectionContextSnapshotState_.blockD70; }

    // Helper11 source block (`0x43c020`, `0x440320`):
    const std::array<char, 0x20>& SourceLeadString108() const { return postAuthMarginLoadingState_.sourceLeadString108; }
    uint32_t SourceField12c() const { return postAuthMarginLoadingState_.sourceField12c; }
    const std::array<uint32_t, 17>& SourceDwords134() const { return postAuthMarginLoadingState_.sourceDwords134; }
    const std::array<uint8_t, 0x20>& SourceBlock178() const { return postAuthMarginLoadingState_.sourceBlock178; }
    const std::array<uint8_t, 0x20>& SourceBlock198() const { return postAuthMarginLoadingState_.sourceBlock198; }
    const std::array<uint8_t, 0x20>& SourceBlock1b8() const { return postAuthMarginLoadingState_.sourceBlock1b8; }

    // State-owned slot-6 bodies keep class ownership, but mutate this mediator-owned owner-state
    // block through a narrow explicit accessor instead of duplicating the storage elsewhere.
    PostAuthMarginLoadingState& MutablePostAuthMarginLoadingState() { return postAuthMarginLoadingState_; }
    const PostAuthMarginLoadingState& PostAuthMarginLoadingStateView() const { return postAuthMarginLoadingState_; }

    // Helper11 HandleLoadCharacterReply outputs (0x440320):
    uint32_t& WorldListCountOrStatus80() { return postAuthMarginLoadingState_.worldListCountOrStatus80; }
    uint8_t State10SendGateFlagF14() const { return postAuthMarginLoadingState_.state10SendGateFlagF14; }
    uint8_t& State10SendGateFlagF14() { return postAuthMarginLoadingState_.state10SendGateFlagF14; }
    const char* CharacterNameBufferF1c() { return postAuthMarginLoadingState_.characterNameBufferF1c; }
    const std::array<uint32_t, 8>& CharacterFlagsF48() { return postAuthMarginLoadingState_.characterFlagsF48; }
    const std::array<uint32_t, 8>& SecondaryCharacterDataF68() { return postAuthMarginLoadingState_.secondaryCharacterDataF68; }
    const std::array<uint32_t, 10>& CharacterRecordPointersF88() { return postAuthMarginLoadingState_.characterRecordPointersF88; }

    void* AllocatedBuffer1418() { return postAuthMarginLoadingState_.allocatedBuffer1418; }
    uint16_t& AllocatedBufferLength141c() { return postAuthMarginLoadingState_.allocatedBufferLength141c; }
    uint8_t& AllocatedBufferFlag141e() { return postAuthMarginLoadingState_.allocatedBufferFlag141e; }

    void* AllocatedBuffer1420() { return postAuthMarginLoadingState_.allocatedBuffer1420; }
    uint16_t& AllocatedBufferLength1424() { return postAuthMarginLoadingState_.allocatedBufferLength1424; }
    uint8_t& AllocatedBufferFlag1426() { return postAuthMarginLoadingState_.allocatedBufferFlag1426; }

    void* AllocatedBuffer1428() { return postAuthMarginLoadingState_.allocatedBuffer1428; }
    uint16_t& AllocatedBufferLength142c() { return postAuthMarginLoadingState_.allocatedBufferLength142c; }
    uint8_t& AllocatedBufferFlag142e() { return postAuthMarginLoadingState_.allocatedBufferFlag142e; }

    void* AllocatedBuffer1408() { return postAuthMarginLoadingState_.allocatedBuffer1408; }
    uint16_t& AllocatedBufferLength140c() { return postAuthMarginLoadingState_.allocatedBufferLength140c; }
    uint8_t& AllocatedBufferFlag140e() { return postAuthMarginLoadingState_.allocatedBufferFlag140e; }

    const std::array<uint8_t, 8>& ReplyParseBuffer() { return postAuthMarginLoadingState_.replyParseBuffer; }
    uint32_t& ReplySectionData13cc() { return postAuthMarginLoadingState_.replySectionData13cc; }
    uint32_t& ReplySectionData13d0() { return postAuthMarginLoadingState_.replySectionData13d0; }
    uint8_t& CharacterRouteIndexCc8() { return postAuthMarginLoadingState_.characterRouteIndexCc8; }

private:
    uint32_t SendAuthFramedPacket(const mxo::auth::FramedPacket& packet, const char* stepLabel);
    uint32_t SendAuthGetPublicKeyRequest();
    uint32_t SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply);
    uint32_t SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge);
    void LogParsedAuthReply(const mxo::auth::AuthReply& reply) const;
    void SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
    void SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world);
    void SeedRecoveredCharacterSlotRecordFromAuthReply(uint8_t characterIndex, const mxo::auth::AuthCharacterEntry& character);
    int FindRecoveredWorldDescriptorIndexByWorldId(uint16_t worldId) const;
    void SeedHelper11SourceBlockFromRecoveredPostAuthStateIfUnset();
    void AdoptAuthReplyIntoRecoveredMediatorState();

    void BuildAuthEndpoint();
    void BuildMarginEndpoint();
    bool RebuildMarginAddressList();
    bool SelectMarginEndpointIpv4();
    mxo::liblttcp::CMessageConnection* EnsureAuthConnectionObject();
    mxo::liblttcp::CMessageConnection* EnsureMarginConnectionObject();

    // Condensed `0x4f78b8` owner sketch for the active branch:
    // - `+0x10` = current helper/state object
    //   - newer receive-side tightening now also makes two mediator wrappers around that field
    //     more concrete:
    //     - `0x41f260` re-enters current helper vtable `+0x14` (current best read: slot 6)
    //     - `0x41afc0` is the margin-completion fallback that re-enters current helper vtable
    //       `+0x04` (current best read: slot 2), and can also clear owner `+0x1c/+0x20` on
    //       work type `1`
    // - `+0x18` = auth connection, `+0x1c` = margin connection
    // - `+0x4c/+0x5c` = auth route + endpoint
    // - `+0x6c` = margin endpoint
    // - `+0x94` = auth/bootstrap source block (`0x41ecd0` consumer family)
    // - `+0x65c` = session callback helper
    // - `+0x660/+0x664` = shared GameSessionID family
    // - `+0x688` = current-slot character-record table
    // - `+0x818` = per-slot route-host string family
    // - `+0xd84` = world-descriptor table
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine_;
    CLTLoginState* currentState_;
    uint32_t lastSwitchedHelperStateScaffold_ = 0;
    uint32_t lastPostedEventScaffold_ = 0;
    uint32_t lastPostedErrorScaffold_ = 0;
    std::array<uint32_t, 8> recentPostedEventsScaffold_{};
    uint32_t recentPostedEventCountScaffold_ = 0;
    uint32_t marginPacketReceiveCountScaffold_ = 0;
    uint32_t marginPacketFilteredBeforeSlot6CountScaffold_ = 0;
    uint32_t marginPacketSlot6DispatchCountScaffold_ = 0;
    uint16_t lastMarginPacketOpcodeScaffold_ = 0;
    uint32_t lastMarginPacketSizeScaffold_ = 0;
    CLTLoginState* scaffoldState3_;
    CLTLoginState* scaffoldState4_;
    CLTLoginState* scaffoldState6_;
    CLTLoginState* scaffoldState8_;
    CLTLoginState* scaffoldState9_;
    CLTLoginState* scaffoldState10_;
    CLTLoginState* scaffoldState11_;
    CLTLoginState* scaffoldState12_;
    CLTLoginState* scaffoldState13_;

    mxo::liblttcp::CMessageConnection* authConnection_;
    mxo::liblttcp::CMessageConnection* marginConnection_;
    bool authConnectionOwnedByMediator_ = false;
    bool marginConnectionOwnedByMediator_ = false;
    void* authConnectionContextKey_;
    void* marginConnectionContextKey_;

    ConnectionHelperFamily helpers_;
    MarginRouteState marginRouteState_;
    MarginAddressListState marginAddressList3c_{};
    uint32_t marginBeginCount24_ = 0;
    uint8_t marginConnectionFlag2d_ = 0;
    uint32_t marginSelectedIpv4_7c_ = 0;
    AuthBootstrapSelectedSource38Sketch authBootstrapSource38_;
    AuthBootstrapState680Sketch authBootstrap680_;
    // Source-owned mirror for owner `+0x65c`.
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    // Lazily allocated session callback helper whose `+0x18` string can later feed owner `+0x664`.
    SessionCallbackHelper65cSketch sessionCallbackHelper65cState_{};
    void* sessionCallbackHelper65c_ = nullptr;
    uint32_t sharedMarginPacketField660_ = 0;  // owner `+0x660`
    std::string gameSessionId664_;             // owner `+0x664`
    // Narrow source-owned post-state9 / post-state12 owner collaborators from
    // `0x41f1d0` / `0x41de40` / `0x41c5c0` / `0x41c510`.
    void* ownerCallback84_ = nullptr;          // owner `+0x84`
    void* ownerObject88_ = nullptr;            // owner `+0x88`
    void* ownerObject8c_ = nullptr;            // owner `+0x8c`
    uint32_t ownerOptionalField90_ = 0;        // owner `+0x90`, only forwarded when helper byte `+4 != 0`
    int32_t ownerCachedHandle147c_ = -1;       // owner `+0x147c`, reused on one branch before reacquire
    // launcher.exe:0x4f78b8 owner-side persisted selection/config snapshot (`0x41c1f0`).
    State8SelectionContextSnapshotState state8SelectionContextSnapshotState_;
    // launcher.exe:0x4f78b8 owner-side post-auth margin/loading area used by state8/state10/state11.
    PostAuthMarginLoadingState postAuthMarginLoadingState_;
    // launcher.exe:0x4f78b8 owner-side current-slot character record table (`+0x688`).
    // Seeded from auth character data; later joined against `+0xd84` to populate `+0x818`.
    std::array<SlotRecordState004b5328, kRecoveredWorldSlotCapacity> slotRecords688_;
    std::array<bool, kRecoveredWorldSlotCapacity> slotRecordValid688_{};
    uint8_t slotRecordCount684_ = 0;
    // launcher.exe:0x4f78b8 owner-side per-slot route/host string family (`+0x818`).
    std::array<RouteHostStringTripleState, kRecoveredWorldSlotCapacity> routeHostStrings818_;
    // launcher.exe:0x4f78b8 owner-side world-descriptor table (`+0xd84`).
    std::array<WorldDescriptorState004b533c, kRecoveredWorldSlotCapacity> worldDescriptorsD84_;
    std::array<bool, kRecoveredWorldSlotCapacity> worldDescriptorValidD84_{};
    uint8_t worldDescriptorCountD80_ = 0;

    std::string authServerDnsName_;
    uint16_t authServerPortHostOrder_;
    bool ignoreHostsFileForAuth_;

    std::string marginServerDnsSuffix_;
    uint16_t marginServerPortHostOrder_;
    bool ignoreHostsFileForMargin_;

    mxo::liblttcp::LTTCPEndpointKey authEndpoint_;
    mxo::liblttcp::LTTCPEndpointKey marginEndpoint_;

    std::string authUsername_;
    std::string authPassword_;
    uint32_t authLauncherVersion_;
    uint32_t authCurrentPublicKeyId_;
    uint8_t authLoginType_;
    std::vector<uint8_t> authKeyConfigMd5_;
    std::vector<uint8_t> authUiConfigMd5_;
    bool authGetPublicKeyRequestSent_;
    bool authRequestSent_;
    bool authChallengeResponseSent_;
    mxo::auth::GetPublicKeyReply lastAuthPublicKeyReply_;
    mxo::auth::AuthRequestBuildResult lastAuthRequestBuildResult_;
    mxo::auth::AuthChallenge lastAuthChallenge_;
    mxo::auth::AuthReply lastAuthReply_;
    std::vector<uint8_t> stagedIncomingAuthPacketBytes_;
    std::vector<uint8_t> stagedIncomingMarginPacketBytes_;

    uint32_t lastAuthConnectStatus_;
    uint32_t lastMarginConnectStatus_;
    uint32_t authConnectStatusCount_;
    uint32_t marginConnectStatusCount_;
    const char* expectedAuthRequestName_;
    const char* expectedMarginRequestName_;

    std::array<void*, kRecoveredWorldSlotCapacity> worldSlots_;
    std::array<void*, kRecoveredWorldSlotCapacity> worldPayloadSlots_;

    // launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (world list data provider)
    // Faithful implementation of arg6 world list provider for InitClientDLL
    // Vtable at offset +0xc from object pointer at 0x4d2c58
    // =============================================================================
    // Address anchors for arg6 world list provider:
    // launcher.exe:0x4d3584 +0xc = vtable (ILTLoginMediator)
    // launcher.exe:0x4d3584 +0x10 = ILTLoginMediator_BuildWorldList()
    // launcher.exe:0x4d3584 +0x14 = Arg6GetWorldNameByIndex(char*)
    // launcher.exe:0x4d3584 +0x18 = Arg6GetWorldVariantByIndex(uint)
    // launcher.exe:0x4d3584 +0x1c = Arg6ValidateWorldSelection(uint -> 0 or 7)
    // launcher.exe:0x4d3584 +0x20 = Arg6GetWorldListCount(uint)
    // launcher.exe:0x4d3584 +0x24 = Arg6GetActiveWorldListCount(uint)
    // launcher.exe:0x4d3584 +0x28 = Arg6GetAvailableWorlds(bool)
    // =============================================================================
    struct Arg6WorldListData {
        // launcher.exe:0x4d3584 +0xfc = GetWorldNameByIndex(index) -> char*
        std::array<std::string, 10> worldNames_ = {"Default", "Starter", "Classic", "Advanced", "Extreme"};

        // launcher.exe:0x4d3584 +0x100 = GetWorldVariantByIndex(index) -> uint (1,2,3,5)
        std::array<uint8_t, 10> worldVariants_ = {1, 2, 3, 5, 1};

        // launcher.exe:0x4d3584 +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
        std::array<bool, 10> worldValid_ = {true, true, true, true, true, false, false, false, false, false};

        // launcher.exe:0x4d3584 +0xf8 = GetWorldListCount() -> uint total count
        uint32_t totalCount_ = 5;

        // launcher.exe:0x4d3584 +0xd8 = GetActiveWorldCount() -> uint active count
        uint32_t activeCount_ = 5;

        // launcher.exe:0x4d3584 +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
        std::array<bool, 10> available_ = {true, true, true, true, true, false, false, false, false, false};
    };

    struct Arg6SelectionConfig {
        uint32_t worldUpperBoundExclusive_ = 1;
        uint32_t variantUpperBoundExclusive_ = 1;
        uint32_t selectedWorldIndexLow24_ = 0;
        uint32_t selectedVariantIndexHigh8_ = 0;
        uint32_t selectedWorldType_ = 1;
        uint32_t selectedVariantState_ = 0;
        uint32_t mappedSelectionId_ = 0;
        std::string mappedSelectionName_ = "standalone";
        std::string mappedVariantName_ = "standalone";
        std::string profileName_ = "resurrections";
        std::string authName_ = "resurrections";
        std::string authPassword_;
    };

    Arg6WorldListData arg6WorldList_;
    Arg6SelectionConfig arg6Selection_;

    // launcher.dll export that populates the client.dll's world list view for InitClientDLL
    // This should be called before InitClientDLL so client.dll receives populated world data
    // Address anchor: launcher.exe:0x4d3584 +0xc = vtable (ILTLoginMediator)
    // Note: The original launcher initializes arg6WorldList_ inline when the object is created,
    // not through a separate method call. We keep these methods for future reference/testing.
public:
    // void BuildWorldList();  // Removed - original launcher doesn't use separate method

private:
    // Populates the client's world list view with launcher-provided data
    void PopulateClientWorldView();  // Kept for reference/testing
};

}  // namespace mxo::ltlogin

}  // namespace mxo
