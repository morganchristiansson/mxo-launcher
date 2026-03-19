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
        // - launcher.exe:0x438d80 = LaunchPadClient_ProcessEvent0x17 (event handler for event code 0x1)
        // - launcher.exe:0x4816f0 = LaunchPadClient_GetVtableOffset (inline helper returning *(this+4))
        // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
        // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
        // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
        //
        // Disassembly of 0x438d80 shows:
        //   - Calls LaunchPadClient_GetVtableOffset(this+8) to get vtable offset
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
        uint8_t currentCharacterOrRouteIndex = 0;
        uint32_t pendingWorldId = 0;
        int32_t currentWorldId = -1;
        std::string routeHostPrefix;
        std::string exactMarginHostName;
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
        //
        // Current best semantic read is therefore stronger than a generic auth blob but still
        // deliberately provisional on exact original class name:
        // an owner-side **station/launchpad-flavored phase-2 auth/bootstrap source block**
        // that feeds launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch and later session/bootstrap helpers.
        std::array<char, 0x20> inlineString00{};
        std::array<char, 0x20> inlineString20{};
        std::array<uint8_t, 16> block40{};
        std::array<uint8_t, 16> block50{};
        SmallStringLike60Sketch string60;
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

    // =============================================================================
    // LAUNCHER.EXE OWNER OBJECT (0x4f78b8) - Post-Auth Margin/Loading State
    // =============================================================================
    // Recovered from Ghidra analysis of helper11 functions:
    // - 0x43c020 = CLTLoginMediator_Helper11_SendPostAuthMarginPacket0x4d
    //   reads scattered fields and builds margin packet with first payload byte 0x4d
    // - 0x440320 = CLTLoginMediator_Helper11_HandleLoadCharacterReply
    //   handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
    //   posts event 0x16 on completion
    //
    // Field layout derived from:
    // - helper11 SendPostAuthMarginPacket (`0x43c020`) disassembly shows:
    //   - `ESI = owner + 0x108`
    //   - reads dwords from `owner + 0x134 .. +0x174`
    //   - passes `owner + 0x178`, `owner + 0x198`, and `owner + 0x1b8` to packet-builder helpers
    // - helper11 HandleLoadCharacterReply (`0x440320`) shows:
    //   - first-fragment path copies leading owner `+0x108` string-ish data into `owner + 0xf1c`
    //   - separately reads owner dword `+0x12c`
    //   - copies 8 dwords from `owner + 0x134` into `owner + 0xf48`
    //   - accumulates later reply fragments under `owner + 0xf1c`
    // - later sibling load-character path `0x43f930` proves a different seed source for the same
    //   `+0xf1c` family:
    //   - owner vtable `+0x44` / `0x41f300` returns current record `owner + 0x688[owner+0xcc8]`
    //   - that path seeds the `+0xf1c` name/world fields from the current cached record table
    // - helper10 HandleAuthReply (`0x4401a0`) also proves:
    //   - `owner + 0x80` is updated from parsed auth-reply status/result
    //   - `owner + 0xcc8` mirrors the current slot/index byte
    //   - `FUN_0043aa80(newRecord, owner + 0x108)` copies the owner `+0x108` string into a
    //     newly allocated per-slot record before helper11 becomes active
    struct State3SelectionContextInputSketch {
        // Current best evidence-backed input layout for owner vtable `+0xec`
        // / `0x41c1f0`.
        //
        // New stronger runtime+static read:
        // - live original `matrix.exe` password confirmation hits owner `+0xec` / `0x41ecd0`
        // - that branch then transitions `0 -> 2 -> 3 -> 8`
        // - while current helper vtable is `0x004b5208` (state `3`), the launcher reaches the
        //   `0x41c1f0` family and switches helper state to `8`
        // - the consumed object size/layout matches the already recovered arg6 `+0xec` copied
        //   selection/config handoff object size (`0xb4` bytes) closely:
        //   first dword selector/index + eleven 16-byte blocks
        //
        // Keep names structural for now, but the current best practical read is that this is a
        // launcher-owned persisted **selection/config snapshot** used on the active post-password
        // launch path before later state-8 work.
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
        // Current best evidence-backed input layout for owner vtable `+0x120`
        // / `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials`.
        //
        // That function directly writes the helper11 owner source block then switches helper state
        // to `10`.
        // Important runtime narrowing from live original `matrix.exe` under WineDbg:
        // - confirmed password/launch progression does hit owner `+0xec` / `0x41ecd0`
        // - the observed live state path then went `0 -> 2 -> 3 -> 8`
        // - `0x41c3c0` did **not** fire on that observed branch before later game loading
        // So keep this as a real branch-specific writer, but do not currently treat it as the
        // proven default post-password progression on the active launch path.
        //
        // Structural writes when it *does* run:
        // - compares input `+0x24` against owner vtable `+0xf8`
        // - writes owner `+0x12c` from input `+0x24`
        // - copies 8 dwords from input `+0x2c` -> owner `+0x134`
        // - copies 8 dwords from input `+0x4c` -> owner `+0x154`
        // - copies 4 bytes from input `+0x6c` -> owner `+0x174`
        // - copies strings from input `+0x00/+0x70/+0x90/+0xb0`
        //   -> owner `+0x108/+0x178/+0x198/+0x1b8`
        //
        // Exact semantic names for these fields are still unsettled; keep them as structural input.
        std::array<char, 0x20> string00{};              // input `+0x00 .. +0x1f`
        uint32_t field20 = 0;                           // input `+0x20`
        uint32_t field24 = 0;                           // input `+0x24`
        uint32_t field28 = 0;                           // input `+0x28`
        std::array<uint32_t, 8> dwords2c{};            // input `+0x2c .. +0x4b`
        std::array<uint32_t, 8> dwords4c{};            // input `+0x4c .. +0x6b`
        std::array<uint8_t, 4> bytes6c{};              // input `+0x6c .. +0x6f`
        std::array<char, 0x20> string70{};             // input `+0x70 .. +0x8f`
        std::array<char, 0x20> string90{};             // input `+0x90 .. +0xaf`
        std::array<char, 0x20> stringB0{};             // input `+0xb0 .. +0xcf`
    };

    struct State8SelectionContextSnapshotState {
        // Owner writeback area filled by `0x41c1f0` on the observed live `state 3 -> 8` branch.
        // Disassembly writes:
        // - owner byte `+0xcc8` from input `+0x00`
        // - owner `+0xcd0 .. +0xd7f` from input `+0x04 .. +0xb3`
        // - then switches helper state to `8`
        //
        // Keep this grouped as the persisted selection/config snapshot sibling to the arg6 `+0xec`
        // handoff object until stronger field names are anchored.
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
        // ========================================================================
        // Owner source block consumed by helper11 send/load paths
        // ========================================================================
        // `0x440320` copies a NUL-terminated byte/string field from the start of this owner block
        // to `+0xf1c` on first fragment, but the entire `+0x108 .. +0x133` range is not proven to
        // be one flat string. The same first-fragment path also reads the dword at relative `+0x24`
        // (`owner +0x12c`) separately.
        std::array<char, 0x20> sourceLeadString108{};    // `+0x108 .. +0x127`
        uint32_t sourceField128 = 0;                     // `+0x128`
        uint32_t sourceField12c = 0;                     // `+0x12c`
        uint32_t sourceField130 = 0;                     // `+0x130`

        // `0x43c020` reads these 17 dwords into the outgoing raw-0x4d margin packet.
        // `0x440320` also copies the first 8 dwords of this same span into `+0xf48`.
        // Keep this as an opaque owner-side dword/object span for now.
        // A nearby `0x440d80` lead turned out to be ambiguous and is not yet strong enough to
        // claim a concrete object identity for `owner +0x134`.
        std::array<uint32_t, 17> sourceDwords134{};      // `+0x134 .. +0x174`

        // `0x43c020` passes these three adjacent owner blocks to packet-builder helpers.
        // Keep them as opaque byte blocks until those helper parameter types are recovered.
        std::array<uint8_t, 0x20> sourceBlock178{};      // `+0x178 .. +0x197`
        std::array<uint8_t, 0x20> sourceBlock198{};      // `+0x198 .. +0x1b7`
        std::array<uint8_t, 0x20> sourceBlock1b8{};      // `+0x1b8 .. +0x1d7`

        // ========================================================================
        // Helper11 HandleLoadCharacterReply outputs (0x440320)
        // ========================================================================
        uint32_t worldListCountOrStatus80 = 0;           // `+0x80`

        char characterNameBufferF1c[32] = {0};           // `+0xf1c .. +0xf3f`
        std::array<uint32_t, 8> characterFlagsF48{};     // `+0xf48 .. +0xf67`
        std::array<uint32_t, 8> secondaryCharacterDataF68{}; // `+0xf68 .. +0xf87`
        std::array<uint32_t, 10> characterRecordPointersF88{}; // `+0xf88 ..`

        // Allocated buffer pointers for character data fragments:
        void* allocatedBuffer1418 = nullptr;             // `+0x1418` (case 0x03)
        uint16_t allocatedBufferLength141c = 0;         // `+0x141c`
        uint8_t allocatedBufferFlag141e = 0;             // `+0x141e`

        void* allocatedBuffer1420 = nullptr;             // `+0x1420` (case 0x04)
        uint16_t allocatedBufferLength1424 = 0;         // `+0x1424`
        uint8_t allocatedBufferFlag1426 = 0;             // `+0x1426`

        void* allocatedBuffer1428 = nullptr;             // `+0x1428` (case 0x05)
        uint16_t allocatedBufferLength142c = 0;         // `+0x142c`
        uint8_t allocatedBufferFlag142e = 0;             // `+0x142e`

        void* allocatedBuffer1408 = nullptr;             // `+0x1408` (case 0x06)
        uint16_t allocatedBufferLength140c = 0;         // `+0x140c`
        uint8_t allocatedBufferFlag140e = 0;             // `+0x140e`

        // Additional fields for character reply parsing:
        std::array<uint8_t, 8> replyParseBuffer{};       // `+0x13cc .. +0x13d3` scratch family
        uint32_t replySectionData13cc = 0;               // `+0x13cc`
        uint32_t replySectionData13d0 = 0;               // `+0x13d0`

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

    // Current best post-connect status/result anchors:
    // - original engine `Connect` success path `0x4329b9..0x4329cc` builds `0x435050(0x7000001)`
    //   which is a type-2 work item enqueued as `(workItem, connection, 0)`
    // - auth-side derived connection family (launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection, vtable `0x4afef0`) later reaches
    //   owner-side packet handling through wrapper `0x449a70`
    // - margin-side derived connection family (`0x41e500`, vtable `0x4aff38`) later reaches
    //   owner-side packet handling through wrapper `0x44af60`
    // - newer helper-object review now narrows one important negative detail:
    //   - startup auth/margin derived objects come through `0x4417e0 -> 0x448b40(flag=0)`
    //   - so connection helper slots `+0x7c / +0x80` stay null on that path
    //   - type-2 connect-status completion therefore falls through `0x449a70 / 0x44af60`
    //     into the owner callback / fallback chain instead of being fully handled by those
    //     optional helper objects alone
    // - important auth-side fallback-chain narrowing from `0x449a70 -> owner +0x17c -> 0x448a60`:
    //   - owner `+0x17c` is now resolved as thunk `0x41f260`
    //   - `0x41f260` forwards to the owner's current helper/state object at `+0x10`, then jumps
    //     to helper vtable `+0x14`
    //   - so the concrete auth-side body depends on the current helper selected through the
    //     `0x4f7868` family via `0x41b450(...)`
    //   - important correction: launcher.exe:0x4401a0 = CLTLoginMediator_Helper10_HandleAuthReply is one **later helper-state** `+0x14` body
    //     (`0x4f7890` / vtable `0x4b512c`), not the generic owner `+0x17c` target itself
    //   - that later helper body only meaningfully handles raw auth code `0x0b` (`AS_AuthReply`)
    //   - on success it parses that reply via `0x43a330`, updates owner `+0x80`, appends a
    //     small owner record under `+0x684`, mirrors the current index to owner byte `+0xcc8`,
    //     then reaches `0x41b450(0x0b)` and string-backed `CLTLoginMediator::PostEvent(0x14)`
    //   - newer helper-side follow-up now makes that immediate post-`AS_AuthReply`
    //     continuation much more concrete:
    //     - it selects helper `0x4f7894` (vtable `0x4b5154`)
    //     - helper `+0x8` / `0x43c020`
    //       (`CLTLoginMediator_Helper11_SendPostAuthMarginPacket0x4d`) builds a larger
    //       margin-side packet whose first payload byte is raw `0x4d`, sends it through
    //       `CLTLoginMediator_SendCurrentMarginPacket` (`0x41af70`), then posts event `0x15`
    //     - helper `+0x14` / `0x440320`
    //       (`CLTLoginMediator_Helper11_HandleLoadCharacterReply`) handles raw `0x10`
    //       / `MS_LoadCharacterReply`, accumulates reply fragments into owner `+0xf1c`, and on
    //       completion switches helper state to `9` then posts event `0x16`
    //   - this now makes the post-auth gap narrower than a generic "later world-list send":
    //     the immediate original continuation is helper11-driven margin/loading progression
    //   - if the current helper `+0x14` body returns 0, `0x448a60` only logs
    //     `Got unhandled op of type %d with status %s`
    // - important current nuance: those later packet handlers are not themselves proof of the
    //   first outbound request after connect; current best read is now stronger than that:
    //   this owner/helper/fallback chain is **not** where the first faithful outbound auth
    //   request begins, even though it remains important later incoming-path evidence
    // - newer helper-family tracing now gives a stronger earlier bootstrap lead than only the
    //   later world-list sender:
    //   - `0x41b450(1)` selects helper `0x4f786c`
    //   - helper launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection starts auth connect through launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection
    //   - direct code xrefs to auth wrapper `0x41af60` still only tie down the later helper
    //     launcher.exe:0x4f78a0 +0x08 / launcher.exe:0x43b830 = CLTLoginMediator_Helper14_SendGetWorldListRequest
    //   - that later auth-side wrapper path remains:
    //       - `0x41af60`
    //       - auth connection `+0x24 / 0x41cf30`
    //       - auth connection `+0x28 / 0x448cf0`
    //       - send helper `0x448a00`
    //       - connection `+0x20 / 0x449d20`
    //       - engine `+0x20` / current best `SendBuffer`
    //     with raw code `0x35` -> `AS_GetWorldListRequest`
    // - current strongest earlier credential/bootstrap auth lead is now helper
    //   `0x4f7870` selected through `0x41b450(2)`:
    //   - helper launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    //   - if auth is not connected yet, it falls back to `0x41b450(1)`
    //   - on the connected branch it gathers launcher-owned owner data through:
    //     - owner `+0x168`
    //     - owner `+0x20`
    //     - owner `+0x38`
    //   - then calls launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch, which is currently only xref'd from launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    //   - Ghidra-backed callsite layout now narrows the selected source object materially:
    //     - owner vtable `+0x38` getter `0x41f0a0` returns embedded owner block `this + 0x94`
    //     - owner vtable `+0x30` / `0x41ecd0` copies/consumes the same family via `0x41eb80`
    //     - source `+0x00 .. +0x1f` = first inline 32-byte NUL-terminated string
    //     - source `+0x20 .. +0x3f` = second inline 32-byte NUL-terminated string
    //     - source `+0x40 .. +0x4f` = first copied 16-byte block
    //     - source `+0x50 .. +0x5f` = second copied 16-byte block
    //     - source `+0x60 .. +0x68` = embedded `0x407dd0`-style small-string object
    //       whose first dword is passed into launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch as the raw third-string pointer
    //     - source `+0x6c` = trailing byte/flag
    //   - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch then branches into two launcher-owned outbound packet builders that both
    //     send indirectly through a bootstrap object send-target at `object + 0x50`
    //     via virtual `+0x24`, rather than through another simple direct `0x41af60` callsite:
    //     - `0x447eb0`
    //       - builds/sends raw code `0x06`
    //       - strongest current `AS_GetPublicKeyRequest` candidate
    //     - `0x4474f0`
    //       - builds/sends raw code `0x08`
    //       - strongest current `AS_AuthRequest` candidate
    //       - also builds/sends a later auxiliary raw `0x1b` packet on that same indirect path
    //   - the branch there is now best read as a low-byte null test on dword helper field `+0xa0`
    // - important channel-specific correction from the latest packet-code pass:
    //   - do not confuse this auth-side bootstrap lead with margin-side wrapper traffic like
    //     `0x41af70`, where raw code `0x06` maps through the margin table to
    //     `MS_GetClientIPRequest`
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

    const char* ExpectedAuthRequestName() const;
    const char* ExpectedMarginRequestName() const;

    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;

    // launcher.exe owner-vtable surfaces currently recovered from the margin-state dispatcher.
    // These names remain provisional, but keeping them in source is more useful than leaving
    // the recovered callsites as anonymous `+0xe0/+0xfc/+0x10c` notes in markdown.
    uint32_t ResolveMarginRouteFromCurrentCharacterSlot() const;   // current best anchor: owner vtable +0xe0
    uint32_t ResolveMarginRouteFromWorldId(uint32_t worldId) const; // current best anchor: owner vtable +0xfc
    uint32_t ResolveMarginRouteDescriptor() const;                  // current best anchor: owner vtable +0x10c

    // Post-auth slot/route families recovered around helper10 (`0x4401a0`) and the later
    // state-8 margin dispatcher (`0x439300`).
    // anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
    const SlotRecordState004b5328* GetSlotRecordByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f300 / owner vtable +0x44
    const SlotRecordState004b5328* GetCurrentSlotRecord() const;
    // anchor: launcher.exe:0x41b220 / owner vtable +0xdc
    const char* GetSlotRecordHeapStringByIndex(uint8_t slotIndex) const;
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
    // Recovered from Ghidra analysis of launcher.exe helper functions:
    // - 0x43c020 = CLTLoginMediator_Helper11_SendPostAuthMarginPacket0x4d
    //   Builds/sends margin packet with first payload byte 0x4d, posts event 0x15
    // - 0x440320 = CLTLoginMediator_Helper11_HandleLoadCharacterReply
    //   Handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
    //   posts event 0x16 on completion
    // =============================================================================

    // anchor: launcher.exe:0x41c1f0
    uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input);

    // anchor: launcher.exe:0x41c3c0
    uint32_t ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input);

    // anchor: launcher.exe:0x43c020
    uint32_t CLTLoginMediator_Helper11_SendPostAuthMarginPacket0x4d();
    
    // anchor: launcher.exe:0x440320
    uint32_t CLTLoginMediator_Helper11_HandleLoadCharacterReply(const uint8_t* packetBytes, size_t packetSize);

    // Current best margin-side connection-init dispatcher:
    // - launcher `0x439300`
    // - consults `[owner+4]` current state object through vtable `+0x18`
    // - dispatches several cases into `0x41e500`
    // - concrete currently recovered case inputs include owner state vtable surfaces
    //   `+0xe0 / +0xfc / +0x10c` and owner fields `+0xcc8 / +0x12c / +0x104`
    // - `0x41e500` then constructs the margin-side CMessageConnection child at owner `+0x1c`,
    //   builds endpoint state into owner `+0x6c`, and calls `connection->+0x1c(owner+0x6c)`
    uint32_t DispatchMarginConnectionByState();
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

    // Helper11 HandleLoadCharacterReply outputs (0x440320):
    uint32_t& WorldListCountOrStatus80() { return postAuthMarginLoadingState_.worldListCountOrStatus80; }
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
    void AdoptAuthReplyIntoRecoveredMediatorState();

    void BuildAuthEndpoint();
    void BuildMarginEndpoint();
    mxo::liblttcp::CMessageConnection* EnsureAuthConnectionObject();
    mxo::liblttcp::CMessageConnection* EnsureMarginConnectionObject();

    // Current best field sketch for the `0x4f78b8` owner object:
    // - `+0x10` = current state/helper object used heavily by owner-state dispatch paths
    // - `+0x18` = auth-side CMessageConnection child
    // - `+0x1c` = margin-side CMessageConnection child
    // - `+0x4c` = auth DNS / route string staging area
    // - `+0x5c` = auth endpoint block consumed by auth-side `connection->+0x1c(...)`
    // - `+0x6c` = margin endpoint block consumed by margin-side `connection->+0x1c(...)`
    // - `+0x94` = embedded station/launchpad-flavored phase-2 auth/bootstrap source block
    //   returned by owner vtable `+0x38` (`0x41f0a0`), copied/consumed by owner vtable `+0x30`
    //   (`0x41ecd0 -> 0x41eb80`), and also written through owner vtable `+0x150` (`0x41f270`)
    // - `+0x680` = extra heap child built during owner initialization; current best read is
    //   the phase-2 auth/bootstrap object sketched above (`0x41290` / `0x45500` family)
    // - `+0x688` = current-slot character-record pointer table
    //   - owner vtable `+0x40` / `0x41f2e0` returns `owner + 0x688[index]`
    //   - owner vtable `+0x44` / `0x41f300` returns the current entry via owner byte `+0xcc8`
    //   - broader writer `0x43f300` seeds this from auth character data
    //   - helper10 (`0x4401a0`) later reuses the same concrete `0x1c`-byte class for selected-slot
    //     writeback / refresh
    //   - current best concrete record class is the object rooted at vtable `0x004b5328`
    //     (`0x4398b0`, `0x43aa80`, `0x43d430`, `0x43dc80`)
    // - `+0x818` = parallel per-character copied route-host string triple family
    //   keyed by the same slot byte as `+0x688`
    //   - owner vtable `+0xe0` / `0x41b260` returns the first dword of each 0x0c-byte slot when
    //     begin != current
    //   - owner writer `0x41f840 = CLTLoginMediator_AppendRouteHostStringTriple` forwards into
    //     `0x41f640 = StringTripleArray_Append` and grows through
    //     `0x41f3e0 = StringTripleArray_GrowAndAppend`
    //   - broader writer `0x43f300` seeds this by matching each `+0x688` character record's
    //     world id against the `+0xd84` world-descriptor table and copying the descriptor name
    // - `+0xd84` = separate world-descriptor pointer table family
    //   - owner vtable `+0xfc/+0x100/+0x104/+0x108` read fields from the pointed payload at
    //     `+0x10`, including world-name `payload+3`, type `payload+0x18`, low byte of
    //     server-version `payload+0x19`, and `payload+0x1f & 0xf`
    //   - current best concrete descriptor class is the `0x14`-byte object rooted at vtable
    //     `0x004b533c` (`0x43c310`, `0x443aa0`, `0x43ded0`, `0x439a70`)
    //   - `0x43f300 = CLTLoginState_AuthenticatePending_AuthMessageDispatch` now looks like the
    //     broader writer/validator over this world-descriptor family before later helper10
    //     selected-slot adoption reads from it
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine_;
    CLTLoginState* currentState_;

    mxo::liblttcp::CMessageConnection* authConnection_;
    mxo::liblttcp::CMessageConnection* marginConnection_;
    void* authConnectionContextKey_;
    void* marginConnectionContextKey_;

    ConnectionHelperFamily helpers_;
    MarginRouteState marginRouteState_;
    AuthBootstrapSelectedSource38Sketch authBootstrapSource38_;
    AuthBootstrapState680Sketch authBootstrap680_;
    // launcher.exe:0x4f78b8 owner-side persisted selection/config snapshot written on the
    // observed live state-3 -> state-8 branch by `0x41c1f0`.
    State8SelectionContextSnapshotState state8SelectionContextSnapshotState_;
    // launcher.exe:0x4f78b8 owner-side post-auth margin/loading writeback area recovered from
    // helper11 (`0x43c020`, `0x440320`). Keep this as source-owned field layout evidence.
    PostAuthMarginLoadingState postAuthMarginLoadingState_;
    // launcher.exe:0x4f78b8 owner-side current-slot character record table (`+0x688`).
    // `0x43f300` seeds this from auth character data, then matches each character's world id
    // against the separate `+0xd84` world-descriptor table to populate the parallel `+0x818`
    // route-host string triple family.
    std::array<SlotRecordState004b5328, kRecoveredWorldSlotCapacity> slotRecords688_;
    std::array<bool, kRecoveredWorldSlotCapacity> slotRecordValid688_{};
    uint8_t slotRecordCount684_ = 0;
    // launcher.exe:0x4f78b8 owner-side copied route/host string triple family (`+0x818`).
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
