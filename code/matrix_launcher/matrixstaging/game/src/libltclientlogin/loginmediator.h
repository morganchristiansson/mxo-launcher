#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"
#include "../../../runtime/src/libltmessaging/messageconnection.h"
#include "../../../runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include "loginmediator_base.h"
#include "authbootstrap680_internal.h"

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
// - docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md
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
//   - `CLTLoginMediator_InitializeHelperDispatchTable` allocates / installs the mediator-owned
//     `CLTLoginState_*` dispatch objects
//   - many of those state-family vtables reuse shared slot-1 gate `0x438d80`
//   - older Ghidra label `LaunchPadClient_ProcessEvent0x17` is kept only as a cross-reference,
//     not as a class-ownership claim
//   - this is the launcher-side event handler system for auth/margin state transitions
// - recovered logging string anchors:
//   - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (logs "CLTLoginMediator::PostEvent(): Event# %d\n")
//   - launcher.exe:0x41d090 = CLTLoginMediator_PostError (calls PostError, logs "CLTLoginMediator::PostError(): Error# %d\n")
// - important current architectural split:
//   - keep low-level packet/crypto helpers under `src/auth/`
//   - keep launcher-owned auth/session state transitions here
//   - do not collapse `LaunchPadClient`-style pre-game account/subscription handling into the
//     direct auth TCP packet layer just because both live under `libltclientlogin`

class CLTLoginMediator : public ILTLoginMediator {
    // Source-ownership split note:
    // - the phase-2 auth/bootstrap child rooted at owner `+0x680` now has its own focused source
    //   home in `authbootstrap680.cpp`
    // - keep access narrow by granting that helper ops struct friendship instead of widening the
    //   mediator surface generically
    friend struct AuthBootstrap680Ops;

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
        // - slot 1 on many of those vtables reuses shared gate `0x438d80`
        // - InitializeHelperDispatchSlot15..Slot19 (0x420640..0x4209a0) populate the late
        //   `CLTLoginState_State15..State19` tail at 0x4f78a4..0x4f78b4
        //
        // Discovered function names from Ghidra renaming:
        // - launcher.exe:0x438d80 = shared `CLTLoginState_*` slot-1 gate
        //   - older Ghidra label: `LaunchPadClient_ProcessEvent0x17`
        // - launcher.exe:0x4816f0 = reused inline helper that returns a vtable offset
        //   - older Ghidra label: `LaunchPadClient_GetVtableOffset`
        // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
        // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
        // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
        //
        // Disassembly of 0x438d80 shows:
        //   - Calls reused inline helper `0x4816f0(this+8)` to get a vtable offset
        //   - Checks if event flag at [this+0x2c] is set
        //   - If event flag set, calls CLTLoginMediator_PostEvent(this, 1)
        //   - Otherwise calls vtable[+0x178]() and updates state at [this+0x80]
        //
        // Current highest-value slot anchors:
        // - slot 1 / `0x4f786c` / phase-code `1`
        //   - current best concrete state object: `CLTLoginState_State1` / vtable `0x4b4fc4`
        //   - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection starts auth connect through launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection
        // - slot 2 / `0x4f7870` / phase-code `2`
        //   - current best concrete state object: `CLTLoginState_AuthenticatePending` / vtable `0x4b5014`
        //   - launcher.exe:0x439210 is the strongest current earlier loginstate-owned handoff into
        //     the owner `+0x680` phase-2 auth/bootstrap child
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
        void* helper78A4 = nullptr;  // slot 15 / phase-code 15 / CLTLoginState_State15
        void* helper78A8 = nullptr;  // slot 16 / phase-code 16 / CLTLoginState_State16
        void* helper78AC = nullptr;  // slot 17 / phase-code 17 / CLTLoginState_State17
        void* helper78B0 = nullptr;  // slot 18 / phase-code 18 / CLTLoginState_State18
        void* helper78B4 = nullptr;  // slot 19 / phase-code 19 / CLTLoginState_State19
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

    struct AuthAddressListState {
        // Source-owned mirror of the auth-side iterator rooted at owner `+0x4c` plus the nearby
        // retry/attempt counter at owner `+0x28`.
        // Current best static read from `0x41d170 / 0x440bb0 / 0x4390b0`:
        // - owner `+0x4c` = begin pointer for a dword IPv4 list
        // - owner `+0x50` = end pointer for that list
        // - owner `+0x58` = current iterator cursor used by `0x440bb0`
        // - owner `+0x28` increments before each auth connect attempt
        // - state1 slot1 later compares `+0x28` against `((+0x50 - +0x4c) >> 2)` to decide
        //   retry-vs-error on the non-zero status side
        std::string resolvedHostName;
        std::vector<uint32_t> ipv4NetworkOrderList;
        size_t nextIndex = 0;
        uint32_t attemptCount28 = 0;
    };

    struct State9CallbackBlob18cSketch {
        // anchor: launcher.exe:0x41e690 / mediator vtable `+0x18c`
        // Focused active source home for this callback84/blob contract:
        // - `loginmediator_state9.cpp`
        // - `loginmediator_state9_submit_scaffold.h`
        // Current best fixed layout:
        // - `+0x00/+0x04` = current slot-record id low/high
        // - `+0x08/+0x0c` = caller args (`900, 0` on the client callback84 path)
        // - `+0x10..+0x1f` = 16-byte transform region
        //   - seeded from owner `+0xf18`
        //   - source material rooted at mediator `+0xd4 -> owner +0x1c + 0x85`
        //   - transformed in place through `0x41df60 / 0x44b190 / 0x44b570`
        // Current source-owned provenance answer for owner `+0xf18`:
        // - zero-init at `0x41ee60`
        // - non-init write at `0x440780` on state6 opcode-`9` success
        // Newer runtime+static tightening now closes the active one-block tail enough for a live
        // source-owned `+0x18c` path too:
        // - `AssemblyTwofish`
        // - `IV`
        // - `FeedbackSize`
        // - zero-IV one-block transform over `[ownerF18, 0, 0, 0]`
        uint32_t currentSlotIdLow00 = 0;
        uint32_t currentSlotIdHigh04 = 0;
        uint32_t callerArg08 = 0;
        uint32_t callerArg0c = 0;
        std::array<uint8_t, 16> transformedRegion10{};
    };

    struct State8SelectionContextSnapshotState {
        // owner writeback area filled by `0x41c1f0` on the active state `3(wait) -> 8` branch.
        // Keep the boundary explicit:
        // - state3 is just the current waiting helper at that stop
        // - the owner-side mediator method owns this writeback and helper advance
        // This is the persisted selection/config snapshot, not the later post-auth appearance/name block.
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

    struct State8PersistenceF1cSnapshot {
        std::array<char, 0x20> string00{};
        uint32_t field20 = 0;
        uint32_t field24 = 0;
        uint32_t field28 = 0;
        std::array<uint32_t, 8> header2c{};
        std::array<uint32_t, 8> secondary4c{};
        std::array<uint8_t, 0x465> body6c{};
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
        // - `+0x1b8` = Background source text
        //   - client wrapper-facing `+0x120` writer currently copies up to `0x400` bytes there
        //   - later state11 packet building still uses only the bounded prefix that fits the
        //     packet builder's own field limits
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
        std::array<uint8_t, 0x400> sourceBlock1b8{};     // `+0x1b8 ...` = Background source text

        // ========================================================================
        // Post-auth HandleLoadCharacterReply outputs (0x440320)
        // ========================================================================
        uint32_t worldListCountOrStatus80 = 0;           // `+0x80`

        // owner byte `+0xf14`; shared send gate used by the active state8 path and later state10.
        // Strongest current writer is state6 opcode-`9` success, which sets it alongside owner
        // `+0xf18 = parsed opcode-9 UDPSessionSecret / session-id dword`; later state9 success
        // clears it again at `0x41b420`.
        uint8_t state10SendGateFlagF14 = 0;             // `+0xf14`

        // Active state8 reply path prefers the current slot record (`+0x688[owner+0xcc8]`) as the
        // first-fragment seed for this name/world block, falling back to the older `+0x108` text.
        // Later state11 (`0x440320`) is tighter: its first fragment copies owner `+0x108`
        // directly and does not consult the current-slot record table.
        char characterNameBufferF1c[32] = {0};           // `+0xf1c .. +0xf3b`
        uint32_t characterReplyFieldF3c = 0;             // `+0xf3c`
        uint32_t characterReplyFieldF40 = 0;             // `+0xf40`
        uint32_t characterReplyFieldF44 = 0x1000;        // `+0xf44`; shared reset helper `0x438a50` seeds this before state8/state11 first-fragment materialization
        std::array<uint32_t, 8> characterFlagsF48{};     // `+0xf48 .. +0xf67`; original getter `0x41f170` / arg6 `+0xbc`
        std::array<uint32_t, 8> secondaryCharacterDataF68{}; // `+0xf68 .. +0xf87` (provisional world/status seed area)
        std::array<uint32_t, 10> characterRecordPointersF88{}; // post-auth/scaffold parsed subview
        std::array<char, 0x20> section0StringF8c{};      // post-auth/scaffold parsed subview
        std::array<char, 0x20> section0StringFac{};      // post-auth/scaffold parsed subview
        std::array<char, 0x20> section0StringFcc{};      // post-auth/scaffold parsed subview
        std::array<uint8_t, 0x465> state8Section0RawF88{}; // source-owned raw mirror of original owner `+0xf88 .. +0x13ec`; original getter `0x41f180` / arg6 `+0xc0`

        // state8 case `0x00` also has a one-shot overflow tail at `+0x13f0/+0x13f4` when the
        // incoming section exceeds `0x485` bytes. Keep that separate from the append families
        // below because the original only allocates it once on the case-0 path instead of using
        // the later generic append buffers.
        void* state8Section0OverflowBuffer13f0 = nullptr; // `+0x13f0` (state8 case 0x00 overflow tail); original getter `0x41aec0` / arg6 `+0xc4`
        uint16_t state8Section0OverflowLength13f4 = 0;    // `+0x13f4` out-length paired with the same `+0xc4` getter

        // Allocated buffer pointers for load-character fragment families.
        // Keep the owner offsets explicit because state8 (`0x43f930`) and state11 (`0x440320`)
        // both reuse this wider owner region with different section selectors.
        void* allocatedBuffer13f8 = nullptr;             // `+0x13f8` (state8 case 0x01); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x7c/+0xa8` / `bl.cfg`
        uint16_t allocatedBufferLength13fc = 0;         // `+0x13fc` out-length paired with the same arg6 `+0xa8` getter
        uint8_t flag13fe = 0;                            // `+0x13fe` bool gate paired with arg6 `+0x7c`

        void* allocatedBuffer1400 = nullptr;             // `+0x1400` (state8 case 0x02); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x80/+0xac` / `il.cfg`
        uint16_t allocatedBufferLength1404 = 0;         // `+0x1404` out-length paired with the same arg6 `+0xac` getter
        uint8_t flag1406 = 0;                            // `+0x1406` bool gate paired with arg6 `+0x80`

        void* allocatedBuffer1408 = nullptr;             // `+0x1408` (state8 case 0x06 / state11 case 0x06); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x68/+0x94` / `hl.cfg`
        uint16_t allocatedBufferLength140c = 0;         // `+0x140c` out-length paired with the same arg6 `+0x94` getter
        uint8_t allocatedBufferFlag140e = 0;             // `+0x140e` bool gate paired with arg6 `+0x68`

        void* allocatedBuffer1410 = nullptr;             // `+0x1410` (state8 case 0x07); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x6c/+0x98` / `an.cfg`
        uint16_t allocatedBufferLength1414 = 0;         // `+0x1414` out-length paired with the same arg6 `+0x98` getter
        uint8_t flag1416 = 0;                            // `+0x1416` bool gate paired with arg6 `+0x6c`

        void* allocatedBuffer1418 = nullptr;             // `+0x1418` (state8 case 0x03 / state11 case 0x03); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x70/+0x9c` / `pi.cfg`
        uint16_t allocatedBufferLength141c = 0;         // `+0x141c` out-length paired with the same arg6 `+0x9c` getter
        uint8_t allocatedBufferFlag141e = 0;             // `+0x141e` bool gate paired with arg6 `+0x70`

        void* allocatedBuffer1420 = nullptr;             // `+0x1420` (state8 case 0x04 / state11 case 0x04); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x74/+0xa0` / `ai.cfg`
        uint16_t allocatedBufferLength1424 = 0;         // `+0x1424` out-length paired with the same arg6 `+0xa0` getter
        uint8_t allocatedBufferFlag1426 = 0;             // `+0x1426` bool gate paired with arg6 `+0x74`

        void* allocatedBuffer1428 = nullptr;             // `+0x1428` (state8 case 0x05 / state11 case 0x05); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x78/+0xa4` / `cs.cfg`
        uint16_t allocatedBufferLength142c = 0;         // `+0x142c` out-length paired with the same arg6 `+0xa4` getter
        uint8_t allocatedBufferFlag142e = 0;             // `+0x142e` bool gate paired with arg6 `+0x78`

        void* allocatedBuffer1430 = nullptr;             // `+0x1430` (state8 case 0x0c)
        uint16_t allocatedBufferLength1434 = 0;         // `+0x1434`
        uint8_t flag1436 = 0;                            // `+0x1436`

        void* allocatedBuffer1438 = nullptr;             // `+0x1438` (state8 case 0x0d)
        uint16_t allocatedBufferLength143c = 0;         // `+0x143c`
        uint8_t flag143e = 0;                            // `+0x143e`

        void* allocatedBuffer1440 = nullptr;             // `+0x1440` (state8 case 0x08); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x84/+0xb0` / `rl.cfg`
        uint32_t allocatedBufferLength1444 = 0;         // `+0x1444` out-length paired with the same arg6 `+0xb0` getter
        uint8_t flag1448 = 0;                            // `+0x1448` bool gate paired with arg6 `+0x84`

        void* allocatedBuffer144c = nullptr;             // `+0x144c` (state8 case 0x09); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x88/+0xb4` / `cl.cfg`
        uint16_t allocatedBufferLength1450 = 0;         // `+0x1450` out-length paired with the same arg6 `+0xb4` getter
        uint8_t flag1452 = 0;                            // `+0x1452` bool gate paired with arg6 `+0x88`

        void* allocatedBuffer1454 = nullptr;             // `+0x1454` (state8 case 0x0a); exact current non-`mcd.cfg` live-corpus pair = arg6 `+0x90/+0xb8` / `cui.cfg`
        uint16_t allocatedBufferLength1458 = 0;         // `+0x1458` out-length paired with the same arg6 `+0xb8` getter
        uint8_t flag145a = 0;                            // `+0x145a` bool gate paired with arg6 `+0x90`
        uint32_t state8Section10ChunkBitmap = 0;         // source-owned mirror of the original `DAT_004f79e4` bitmap

        // state8/state11 case `0x0b` / `0x43f8c0` side effect:
        // - owner `+0x145c` = first dword of the section payload when byteCount > 4
        // - owner `+0x1460` = trailing small-string-like copy of the remaining payload bytes
        // - sibling original getters now anchored too:
        //   - `0x41f190` / arg6 `+0xc8` = bool-style test for non-zero `+0x145c`
        //   - `0x41f1a0` / arg6 `+0xcc` = return dword `+0x145c`
        //   - `0x41f1b0` / arg6 `+0xd0` = return small-string-like `+0x1460`
        uint32_t state8Section11Dword145c = 0;          // `+0x145c`
        std::string state8Section11String1460;          // `+0x1460` small-string-like mirror

        // Additional fields for character reply parsing:
        std::array<uint8_t, 8> replyParseBuffer{};       // `+0x13cc .. +0x13d3` scratch family
        uint32_t replySectionData13cc = 0;               // `+0x13cc`
        uint32_t replySectionData13d0 = 0;               // `+0x13d0`
        uint8_t section0Flag13f6 = 0;                    // `+0x13f6`; exact corrected mediator-backed `mcd.cfg` gate for arg6 `+0x8c`

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

    CLTLoginMediator();
    ~CLTLoginMediator();

    // ============================================================================
    // Migrated state fields (formerly in g_MediatorRuntimeState)
    // These fields are now instance-owned instead of global runtime state.
    // ============================================================================
    const void* lastNopatchValue1Ptr_ = nullptr;  // +0x1c / Mediator_SetValue1
    const void* lastNopatchValue2Ptr_ = nullptr;  // +0x20 / Mediator_SetValue2
    uint32_t lastStatus178_ = 0u;                 // +0x178 / Mediator_GetLastLoginStatus
    uint32_t statusQuery178Count_ = 0u;           // +0x178 / Mediator_GetLastLoginStatus
    mutable bool bootstrapRaw08AuxHandle50Logged_ = false; // +0x50 change-log state moved from ABI wrapper
    mutable void* lastBootstrapRaw08AuxHandle50_ = nullptr; // +0x50 last logged value moved from ABI wrapper
    mutable bool liveCuiCfgAbsentNoteLogged90_ = false;     // +0x90 one-shot caveat log moved from ABI wrapper

    // Accessors for migrated state fields (diagnostics only)
    const void* LastNopatchValue1Ptr() const;
    const void* LastNopatchValue2Ptr() const;
    uint32_t LastStatus178() const;
    uint32_t StatusQuery178Count() const;

    // +0x00
    const char* GetName() override;
    // +0x08
    void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
    // +0x0c
    void ClearEngine() override;
    // +0x10
    uint32_t IsReady() override;
    // +0x1c
    void SetValue1(void* value) override;
    // +0x20
    void SetValue2(void* value) override;
    // +0x2c
    uint32_t IsConnected() override;
    // +0x38
    // Current best wrapper-facing name from the client `Profiles\%s\...` builder.
    const char* GetProfileRootName() const override;
    // +0x3c
    uint32_t GetDefaultSelectionIndex() const override;
    // +0x48
    // Current best wrapper-facing name from the later `Profiles\%s\%s_%X\` builder.
    const char* GetWorldOrSelectionName() const override;
    // +0x4c
    const char* GetProfileOrSessionName() const override;
    // +0x54
    bool HasBootstrapRaw08AuxHandle54() const override;
    // +0x58
    uint8_t GetCrashReporterPromptForSecurId58() const override;
    // +0x5c
    const char* GetCrashReporterUsername5c(const void* chainedValueToken) override;
    // +0x60
    const char* GetCrashReporterPassword60(const void* chainedValueToken) override;
    // +0x68
    // client.dll:0x62198670 = `hl.cfg` live-corpus gate; launcher getter returns owner byte `+0x140e
    uint32_t HasLiveHlCfg68() const override;
    // +0x6c
    // client.dll:0x62198770 = `an.cfg` live-corpus gate; launcher getter returns owner byte `+0x1416`
    uint32_t HasLiveAnCfg6c() const override;
    // +0x70
    // client.dll:0x62198870 = `pi.cfg` live-corpus gate; launcher getter returns owner byte `+0x141e`
    uint32_t HasLivePiCfg70() const override;
    // +0x74
    // client.dll:0x62198970 = `ai.cfg` live-corpus gate; launcher getter returns owner byte `+0x1426`
    uint32_t HasLiveAiCfg74() const override;
    // +0x78
    // client.dll:0x62198a70 = `cs.cfg` live-corpus gate; launcher getter returns owner byte `+0x142e`
    uint32_t HasLiveCsCfg78() const override;
    // +0x7c
    // client.dll:0x62198b70 = `bl.cfg` live-corpus gate; launcher getter returns owner byte `+0x13fe`
    uint32_t HasLiveBlCfg7c() const override;
    // +0x80
    // client.dll:0x62198c60 = `il.cfg` live-corpus gate; launcher getter returns owner byte `+0x1406`
    uint32_t HasLiveIlCfg80() const override;
    // +0x84
    // client.dll:0x62198d50 = `rl.cfg` live-corpus gate; launcher getter returns owner byte `+0x1448`
    uint32_t HasLiveRlCfg84() const override;
    // +0x88
    // client.dll:0x62198e50 = `cl.cfg` live-corpus gate; launcher getter returns owner byte `+0x1452`
    uint32_t HasLiveClCfg88() const override;
    // +0x8c
    // client.dll:0x62198fa0 = `mcd.cfg` mediator-backed gate; launcher getter returns owner byte `+0x13f6`
    uint32_t HasState8PersistenceData8c() const override;
    // +0x90
    // client.dll:0x621993d0 = `cui.cfg` live-corpus gate; launcher getter returns owner byte `+0x145a`
    uint32_t HasLiveCuiCfg90() const override;
    // +0x94
    // client.dll:0x62198670 = `hl.cfg` live-corpus getter; launcher getter returns owner `+0x1408`, out-length `+0x140c`
    void* GetLiveHlCfg94(uint32_t* outLength) const override;
    // +0x98
    // client.dll:0x62198770 = `an.cfg` live-corpus getter; launcher getter returns owner `+0x1410`, out-length `+0x1414`
    void* GetLiveAnCfg98(uint32_t* outLength) const override;
    // +0x9c
    // client.dll:0x62198870 = `pi.cfg` live-corpus getter; launcher getter returns owner `+0x1418`, out-length `+0x141c`
    void* GetLivePiCfg9c(uint32_t* outLength) const override;
    // +0xa0
    // client.dll:0x62198970 = `ai.cfg` live-corpus getter; launcher getter returns owner `+0x1420`, out-length `+0x1424`
    void* GetLiveAiCfgA0(uint32_t* outLength) const override;
    // +0xa4
    // client.dll:0x62198a70 = `cs.cfg` live-corpus getter; launcher getter returns owner `+0x1428`, out-length `+0x142c`
    void* GetLiveCsCfgA4(uint32_t* outLength) const override;
    // +0xa8
    // client.dll:0x62198b70 = `bl.cfg` live-corpus getter; launcher getter returns owner `+0x13f8`, out-length `+0x13fc`
    void* GetLiveBlCfgA8(uint32_t* outLength) const override;
    // +0xac
    // client.dll:0x62198c60 = `il.cfg` live-corpus getter; launcher getter returns owner `+0x1400`, out-length `+0x1404`
    void* GetLiveIlCfgAc(uint32_t* outLength) const override;
    // +0xb0
    // client.dll:0x62198d50 = `rl.cfg` live-corpus getter; launcher getter returns owner `+0x1440`, out-length `+0x1444`
    void* GetLiveRlCfgB0(uint32_t* outLength) const override;
    // +0xb4
    // client.dll:0x62198e50 = `cl.cfg` live-corpus getter; launcher getter returns owner `+0x144c`, out-length `+0x1450`
    void* GetLiveClCfgB4(uint32_t* outLength) const override;
    // +0xb8
    // client.dll:0x621993d0 = `cui.cfg` live-corpus getter; launcher getter returns owner `+0x1454`, out-length `+0x1458`
    void* GetLiveCuiCfgB8(uint32_t* outLength) const override;
    // +0xbc
    // client.dll:0x62198fa0 = `mcd.cfg` mediator-backed header getter; launcher getter returns owner `+0xf48`
    const void* GetState8PersistenceHeaderBc() const override;
    // +0xc0
    // client.dll:0x62198fa0 = `mcd.cfg` mediator-backed body getter; launcher getter returns owner `+0xf88`
    const void* GetState8PersistenceBodyC0() const override;
    // +0xc4
    // client.dll:0x62198fa0 = `mcd.cfg` overflow-tail getter; launcher getter returns owner `+0x13f0`, out-length `+0x13f4`
    void* GetState8PersistenceOverflowC4(uint16_t* outLength) const override;
    // +0xc8
    // sibling state8 section-`0x0b` bool getter; launcher getter tests owner dword `+0x145c`
    uint32_t HasState8Section11Dword145c() const override;
    // +0xcc
    // sibling state8 section-`0x0b` dword getter; launcher getter returns owner dword `+0x145c`
    uint32_t GetState8Section11Dword145c() const override;
    // +0xd0
    // sibling state8 section-`0x0b` small-string-like getter; launcher getter returns owner `+0x1460`
    RouteDescriptor30SmallStringLikeSketch* GetState8Section11String1460() override;
    // +0xd8
    uint32_t GetArg7SelectionUpperBoundExclusive() const override;
    // +0xdc
    const char* MapSelectionName(uint32_t selectionHighByte) const override;
    // +0xe0
    const char* GetVariantWorldName(uint32_t variantIndex) override;
    // +0xe4
    uint8_t GetVariantState(int32_t variantIndex) const override;
    // +0x164
    bool RequestAuthConnectionCloseWaitEvent1() override;
    // +0x16c
    // Wrapper-facing split kept explicit from the owner-side state9 helper below.
    bool RequestMarginConnectionCloseWaitEvent0f() override;
    // +0x178
    uint32_t GetLastLoginStatus() override;

    void SetCurrentState(CLTLoginState* state);
    CLTLoginState* CurrentState() const;

    struct ActiveCharacterStateViewScaffold {
        const char* characterName = nullptr;
        uint32_t characterIdLow = 0;
        uint32_t characterIdHigh = 0;
        const char* realFirstName = nullptr;
        const char* realLastName = nullptr;
        const char* background = nullptr;
    };

    // Narrow active-state-source bridge for wrapper-facing/model code.
    // Current runtime still lets diagnostics register a separate live controller instance here,
    // but callers should depend on this generic hook instead of reaching back into diagnostics
    // globals so future ownership can move without a broad rewrite.
    static void RegisterActiveStateSourceScaffold(CLTLoginMediator* mediator);
    static bool UnregisterActiveStateSourceScaffold(const CLTLoginMediator* mediator);
    static CLTLoginMediator* ActiveStateSourceScaffold();
    const CLTLoginMediator* ResolveActiveStateSourceScaffold() const;
    ActiveCharacterStateViewScaffold DescribeOwnCharacterStateScaffold() const;
    ActiveCharacterStateViewScaffold DescribeActiveCharacterStateScaffold() const;

    // anchor: launcher.exe:0x41b450
    // Recovered helper-state switcher:
    // - not just a raw assignment
    // - calls old helper vtable `+0x0c` with the new helper object
    // - installs the new helper from the dispatch table
    // - then calls new helper vtable `+0x08` with the old helper object
    // Direct vtable reads now tighten that to:
    // - old helper `+0x0c` -> slot 4
    // - new helper `+0x08` -> slot 3 / BeginOrContinue
    // Active post-state9 consequence: the state9 -> state12 pair maps both of those to tiny stubs,
    // so the immediate continuation stays the later explicit PostEvent/listener work.
    void SwitchHelperStateScaffold(uint32_t helperStateId, CLTLoginState* state);
    uint32_t LastSwitchedHelperStateScaffold() const { return lastSwitchedHelperStateScaffold_; }

    // Narrow source-owned scaffolds for the launcher.exe logging/event side effects at
    // `0x41cfb0` / `0x41d090`.
    // Current implementation now keeps lightweight event/error history together with a minimal
    // observer registration bridge for arg6/`ILTLoginMediator.Default` slots `+0x170/+0x174`.
    void PostEventScaffold(uint32_t eventId);
    void PostErrorScaffold(uint32_t errorId);
    bool RegisterLoginObserver(void* observer) override;
    // +0x174
    bool UnregisterLoginObserver(void* observer) override;
    // Observer count getters for wrapper diagnostics (moved from g_MediatorRuntimeState):
    uint32_t ObserverRegisterCount() const { return observerRegister170Count_; }
    uint32_t ObserverUnregisterCount() const { return observerUnregister174Count_; }
    void* FirstObserver() const { return firstObserver170_; }
    void* LatestObserver() const { return latestObserver170_; }
    uint32_t LastPostedEventScaffold() const { return lastPostedEventScaffold_; }
    uint32_t LastPostedErrorScaffold() const { return lastPostedErrorScaffold_; }
    const std::array<uint32_t, 8>& RecentPostedEventsScaffold() const { return recentPostedEventsScaffold_; }
    uint32_t RecentPostedEventCountScaffold() const { return recentPostedEventCountScaffold_; }

    // Narrow post-auth receive-boundary counters used only for short runtime discrimination:
    // - no packet arrived yet
    // - packet arrived but would be consumed by base margin dispatch before slot 6
    // - packet survived into current helper slot 6
    uint32_t MarginPacketReceiveCountScaffold() const { return marginPacketReceiveCountScaffold_; }
    uint32_t MarginPacketFilteredBeforeSlot6CountScaffold() const { return marginPacketFilteredBeforeSlot6CountScaffold_; }
    uint32_t MarginPacketSlot6DispatchCountScaffold() const { return marginPacketSlot6DispatchCountScaffold_; }
    uint16_t LastMarginPacketOpcodeScaffold() const { return lastMarginPacketOpcodeScaffold_; }
    uint32_t LastMarginPacketSizeScaffold() const { return lastMarginPacketSizeScaffold_; }

    // Focused source home for this early auth/state-entry wiring:
    // - `loginmediator_auth_entry.cpp`
    // Source-owned scaffold registration for concrete CLTLoginState objects that live outside the
    // mediator header. This preserves the original helper-state ownership on the login-state
    // vtables while still letting the mediator switch between the active scaffold states.
    // Early concrete auth-side states with real current value now use the same mediator-owned
    // registration/access pattern too.
    // Current practical startup/auth focus:
    // - state 0  = initial idle/start current helper installed by mediator init
    // - state 1  = auth-connect pending
    // - state 2  = post-submit loginstate handoff into the owner `+0x680` phase-2
    //              auth/bootstrap child, entered from owner `ProcessLoginRequest` rather than
    //              installed as the startup default helper
    // - state 14 = world-list pending
    // Late Family-A states 15..19 stay non-happy / later-flow scaffolds for now, but keep
    // registration points source-owned so future work can switch to them without reopening the
    // generic mediator ownership split again.
    void RegisterScaffoldState0(CLTLoginState* state);
    void RegisterScaffoldState1(CLTLoginState* state);
    void RegisterScaffoldState2(CLTLoginState* state);
    void RegisterScaffoldState3(CLTLoginState* state);
    void RegisterScaffoldState4(CLTLoginState* state);
    void RegisterScaffoldState6(CLTLoginState* state);
    void RegisterScaffoldState8(CLTLoginState* state);
    void RegisterScaffoldState9(CLTLoginState* state);
    void RegisterScaffoldState10(CLTLoginState* state);
    void RegisterScaffoldState11(CLTLoginState* state);
    void RegisterScaffoldState12(CLTLoginState* state);
    void RegisterScaffoldState13(CLTLoginState* state);
    void RegisterScaffoldState14(CLTLoginState* state);
    void RegisterScaffoldState15(CLTLoginState* state);
    void RegisterScaffoldState16(CLTLoginState* state);
    void RegisterScaffoldState17(CLTLoginState* state);
    void RegisterScaffoldState18(CLTLoginState* state);
    void RegisterScaffoldState19(CLTLoginState* state);
    // anchor: launcher.exe:0x41b4f0 / arg6 vtable +0xd4
    // Late-login state9 callback-seed getter. Returns the same 16-byte margin bootstrap key
    // family consumed by the callback-blob fill path.
    const void* GetState9CallbackSeedPointer85D4() const override;
    // anchor: launcher.exe:0x41f1d0
    // Focused late-login source home:
    // - `loginmediator_state9.cpp`
    // - `loginmediator_state9_submit_scaffold.h`
    // Current best provenance read:
    // - deeper client init calls arg6 vtable `+0x124(netShell, netMgr, distrObjExecutive)`
    // - `0x41f1d0` stores that triple into owner `+0x84/+0x88/+0x8c`
    // - within the bounded active mediator/state9 scope, `0x41ee60` zero-inits the triple,
    //   `0x41f1d0` writes it, and `0x41de40` later reads it
    // Important callback84 correction:
    // - `netShell +0x38` is not a self-contained answer source; it re-enters resolved
    //   `ILTLoginMediator.Default +0x18c`
    // Wrapper-facing `+0x124` capture remains separate from the owner-side triple mirror.
    void ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) override;
    void SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c);
    static void CaptureDeferredState9CallbackObjectTriple84_88_8c_Scaffold(
        void* callback84,
        void* object88,
        void* object8c);
    // anchor: launcher.exe:0x41e690 / mediator vtable +0x18c
    uint32_t FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) override;
    uint32_t FillState9CallbackBlob18cScaffold(uint32_t* outDwords, uint32_t arg2, uint32_t arg3);
    CLTLoginState* ScaffoldState0() const;
    CLTLoginState* ScaffoldState1() const;
    CLTLoginState* ScaffoldState2() const;
    CLTLoginState* ScaffoldState3() const;
    CLTLoginState* ScaffoldState4() const;
    CLTLoginState* ScaffoldState6() const;
    CLTLoginState* ScaffoldState8() const;
    CLTLoginState* ScaffoldState9() const;
    CLTLoginState* ScaffoldState10() const;
    CLTLoginState* ScaffoldState11() const;
    CLTLoginState* ScaffoldState12() const;
    CLTLoginState* ScaffoldState13() const;
    CLTLoginState* ScaffoldState14() const;
    CLTLoginState* ScaffoldState15() const;
    CLTLoginState* ScaffoldState16() const;
    CLTLoginState* ScaffoldState17() const;
    CLTLoginState* ScaffoldState18() const;
    CLTLoginState* ScaffoldState19() const;
    // Installs the source-owned initial idle/start helper convention (`state0`) after
    // registration. This stays separate from owner-owned submit handling: state0 keeps the shared
    // slot-3 no-op stub, and `ProcessLoginRequest` performs the first happy-path switch to state2.
    void InstallInitialState0Scaffold();

    void SetAuthConnectionContextKey(void* contextKey);
    void SetMarginConnectionContextKey(void* contextKey);

    // Recovered config anchors:
    // - launcher `qsAuthServerDNSName` / `AuthServerPort`
    // - launcher `MarginServerDNSSuffix` / `MarginServerPort`
    // The replacement launcher should eventually populate these from the same launcher-owned
    // config path instead of treating connection setup as generic ad-hoc socket work.
    void SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile = false);
    void SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile = false);

    std::string ResolvedMarginHostName() const;

    mxo::liblttcp::CMessageConnection* AuthConnection() const;
    mxo::liblttcp::CMessageConnection* MarginConnection() const;

    void SetMarginRouteHostPrefix(const char* routeHostPrefix);
    void SetExactMarginHostName(const char* exactMarginHostName);
    const MarginRouteState& CurrentMarginRouteState() const;

    // launcher.exe:0x43b300
    // Current best read:
    // - allocates / initializes the mediator-owned helper/state dispatch array
    // - the earlier recovered block covers slots `0..14` at `0x4f7868 .. 0x4f78a0`
    // - the late recovered tail covers slots `15..19` at `0x4f78a4 .. 0x4f78b4` and is now
    //   concretely typed as `CLTLoginState_State15..State19`
    // - exact class names for most earlier helper objects are still being recovered
    void InitializeConnectionHelpers();

    // HELPER / STATE DISPATCH TABLE INITIALIZATION HELPERS (from Ghidra analysis of 0x43b300):
    // ==============================================================================
    // launcher.exe:0x4f7868..0x4f78a0 = recovered earlier helper/state block (`0..14`)
    // launcher.exe:0x420640..0x4209a0 = InitializeHelperDispatchSlot15..Slot19 for the late
    // `CLTLoginState_State15..State19` tail at `0x4f78a4..0x4f78b4`
    // ==============================================================================
    // launcher.exe:0x420640 = InitializeHelperDispatchSlot15 (slot at 0x4f78a4)
    //   Original: allocates 8 bytes, stores vtable 0x4b0b88 (`CLTLoginState_State15`)
    // launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16 (slot at 0x4f78a8)
    //   Original: allocates 4 bytes, stores vtable 0x4b0bb0 (`CLTLoginState_State16`)
    // launcher.exe:0x420850 = InitializeHelperDispatchSlot17 (slot at 0x4f78ac)
    //   Original: allocates 4 bytes, stores vtable 0x4b0bd8 (`CLTLoginState_State17`)
    // launcher.exe:0x420920 = InitializeHelperDispatchSlot18 (slot at 0x4f78b0)
    //   Original: allocates 8 bytes, stores vtable 0x4b0c00 (`CLTLoginState_State18`)
    // launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19 (slot at 0x4f78b4)
    //   Original: allocates 4 bytes, stores vtable 0x4b0c28 (`CLTLoginState_State19`)
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
    // launcher.exe:0x4f78a4 = slot 15 (`CLTLoginState_State15`, vtable `0x4b0b88`)
    // launcher.exe:0x4f78a8 = slot 16 (`CLTLoginState_State16`, vtable `0x4b0bb0`)
    // launcher.exe:0x4f78ac = slot 17 (`CLTLoginState_State17`, vtable `0x4b0bd8`)
    // launcher.exe:0x4f78b0 = slot 18 (`CLTLoginState_State18`, vtable `0x4b0c00`)
    // launcher.exe:0x4f78b4 = slot 19 (`CLTLoginState_State19`, vtable `0x4b0c28`)
    // ==============================================================================

    // Minimal placeholder accessors for recovered world/selection storage families.
    // These are not yet faithful data structures; they only preserve the currently recovered
    // slot-count / owner-shape in source instead of re-describing it in markdown.
    void* WorldSlot(uint32_t index) const;
    void* WorldPayloadSlot(uint32_t index) const;

    // launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (world list data provider)
    // Faithful implementation of arg6 world list provider for InitClientDLL
    // Vtable at offset +0xc from object pointer at 0x4d2c58
    // Focused source home for this surface:
    // - `loginmediator_arg6.cpp`
    void InitializeArg6DefaultObject();
    void ConfigureArg6Selection(
        uint32_t worldUpperBoundExclusive,
        uint32_t variantUpperBoundExclusive,
        const char* mappedSelectionName,
        const char* mappedVariantName,
        uint32_t selectedWorldIndexLow24,
        uint32_t selectedVariantIndexHigh8,
        uint32_t selectedSelectionGateByte100,
        uint32_t selectedVariantState);
    void SetArg6ProfileName(const char* profileName);
    void SetArg6AuthName(const char* authName);
    void SetArg6AuthPassword(const char* authPassword);
    uint32_t Arg6WorldUpperBoundExclusive() const;
    uint32_t Arg6VariantUpperBoundExclusive() const;
    uint32_t Arg6SelectedWorldIndexLow24() const;
    uint32_t Arg6SelectedVariantIndexHigh8() const;
    uint32_t Arg6SelectedSelectionGateByte100() const;
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

    // Wrapper-facing world-descriptor family (`+0xf8 .. +0x108`).
    // Keep the wrapper/owner split explicit:
    // - once the post-auth owner `+0xd84` table exists, these slots read that concrete
    //   world-descriptor state
    // - owner `+0x100 / 0x41b320` now reads descriptor byte `+0x17` (Status), but the earlier
    //   startup/arg7 path still only has a legacy synthetic gate byte for the same wrapper slot,
    //   so keep `+0x100` named as a wrapper-facing selection-gate byte instead of forcing a
    //   false owner/wrapper unification
    // - owner `+0x104 / 0x41b360` reads descriptor byte `+0x18` (Type)
    // - `+0x104/+0x108` do not have a proved startup-side synthetic answer, so they stay `0`
    //   until the real descriptor table is present
    uint32_t GetWorldCount() const override;
    const char* GetWorldNameByIndex(uint32_t index) override;
    uint8_t GetWorldSelectionGateByteByIndex(uint32_t index) const override;
    uint8_t GetWorldTypeByteByIndex(uint32_t index) const override;
    uint8_t GetWorldPopulationNibbleByIndex(uint32_t index) const override;

    // Startup-only arg6 selection/world-list helpers.
    uint8_t Arg6ValidateWorldSelection(uint8_t variant);
    uint32_t Arg6GetWorldListCount() const;
    uint32_t Arg6GetActiveWorldListCount() const;
    bool Arg6GetAvailableWorlds(uint32_t index) const;

    // Focused source home for auth-entry/connect-status scaffolding:
    // - `loginmediator_auth_entry.cpp`
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
    // Narrow mediator-owned auth-entry bridge for the current diagnostics auto-begin path:
    // - captures the current helper as state1's cached upstream input
    // - installs the registered state1 scaffold as the active helper
    // - then dispatches state1 slot 3 / `0x439090`
    uint32_t BeginAuthConnectionViaState1Scaffold();
    // Focused source-owned wrapper for the missing new-helper slot-3 callback side of
    // `0x41b450` on the early auth path. Keep this narrow instead of changing the generic
    // switch scaffold until more of the broader helper transition surface is source-owned.
    uint32_t SwitchHelperStateAndDispatchSlot3Scaffold(
        uint32_t helperStateId,
        CLTLoginState* state,
        void* upstreamOrArg,
        const char* reason = nullptr);
    // anchor: launcher.exe:0x41b490
    // Tiny auth transport-ready test used by state2 slot 3 before it reaches the bootstrap
    // dispatcher. Current best concrete read: auth connection exists and connection `+0x34 == 2`.
    bool HasReadyAuthConnectionState2() const;
    // Source-owned branch selector for the later `0x41ecd0`
    // `g_LaunchPadGateState16State18` / state16/state18 family.
    // Keep it default-off so the proven happy path stays on the observed
    // `g_LaunchPadGateState16State18 == 0` route, but make the alternate transition scaffold
    // explicit for future non-happy work.
    void SetProcessLoginRequestAlternateState16BranchScaffold(bool enabled);
    bool ProcessLoginRequestAlternateState16BranchScaffold() const {
        return processLoginRequestAlternateState16BranchScaffold_;
    }

    // Post-connect status handling is still owner/helper-state driven.
    // Current high-value summary:
    // - active replacement happy path still sees auth connect completion arrive as type-2 work
    //   `0x7000001`
    // - newer `0x4390b0` disassembly also proves the original state1 slot-1 body branches on the
    //   raw status dword as zero-vs-non-zero when deciding helper-switch/event-vs-retry/error
    // - current source therefore keeps the exact recovered helper-switch/event/error shape while
    //   still aliasing live replacement `0x7000001` into the success-side bridge so the active
    //   happy path remains intact until the producer/result semantics are fully source-owned
    // - auth and margin derived connection families fall through wrappers into owner callback /
    //   current-state dispatch rather than being fully handled by optional helper slots alone
    // - active default password-submit continuation is now tighter:
    //   `0x41ecd0 -> state2 -> state1 -> state2 -> state3(wait) -> 0x41c1f0(owner advance)
    //    -> 0x439300 -> 0x43bd20 / 0x43f930`
    // - state3 remains the waiting helper there; do not invent a state3-local slot-3 body to
    //   explain the `0x41c1f0` transition
    // - state11 remains a later real branch after auth-reply handling, not the first active
    //   branch to prioritize
    // - the earlier auth bootstrap/send lead is still the helper2 / `0x448050` family, not the
    //   later world-list sender
    // - `BeginAuthHandshake()` stays only as a thin wrapper name; the concrete owner+0x680 child
    //   prepare/write/branch logic lives in `AuthBootstrap680Ops::PrepareAndDispatch(...)`
    uint32_t HandleAuthConnectStatus(uint32_t workResultCode);
    uint32_t HandleMarginConnectStatus(uint32_t workResultCode);
    uint32_t BeginAuthHandshake();
    uint32_t BeginMarginHandshake();
    uint32_t LastAuthConnectStatus() const { return lastAuthConnectStatus_; }
    uint32_t AuthConnectStatusCount() const { return authConnectStatusCount_; }
    void ResetAuthConnectRetryStateScaffold();
    uint32_t AuthConnectAttemptCountScaffold() const;
    uint32_t AuthConnectCandidateCountScaffold() const;
    bool HasAuthConnectRetryCandidateRemainingScaffold() const;

    void SetAuthCredentials(const char* username, const char* password);
    void SetAuthBootstrapConfig(
        uint32_t launcherVersion,
        uint32_t currentPublicKeyId,
        uint8_t loginType,
        const std::vector<uint8_t>& keyConfigMd5,
        const std::vector<uint8_t>& uiConfigMd5);
    uint32_t HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize);
    // Newer `0x44af20 / 0x442d00 / 0x41f260` tightening now makes the later post-auth receive
    // boundary explicit in source too:
    // - decoded margin codes `2`, `4`, and `5` are consumed by base margin dispatch
    // - only other codes survive into owner `+0x184` / current helper slot 6
    // - practical consequence for that later path: the first real `MS_LoadCharacterReply` candidate must
    //   arrive as raw code `0x10` *after* that base-dispatch filter
    uint32_t HandleMarginPacketBytes(const uint8_t* packetBytes, size_t packetSize);

    // Narrow staged-packet access kept on the mediator for the concrete CLTLoginState slot-6
    // bodies.
    // Keep the packet/class ownership split explicit:
    // - state10 slot 6 / `0x4401a0` still uses the mediator's staged auth-reply wrapper
    // - state11 slot 6 / `0x440320` now owns the load-character reply transition directly
    // - mediator only keeps the staged bytes for that later margin path
    uint32_t HandleStagedAuthReplyPacketScaffold();
    const std::vector<uint8_t>& StagedIncomingMarginPacketBytes() const;

    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;

    // launcher.exe owner-vtable surfaces currently recovered from the state4 margin dispatcher.
    // Keep these helpers narrow and structural:
    // - `0x439300` decides which helper to call
    // - these mediator methods only mirror the owner-side route-string getters that feed `0x41e500`
    const char* ResolveMarginRouteFromCurrentCharacterSlot() const;            // current best anchor: recovered route-host helper `0x41b260`
    const char* ResolveMarginRouteFromDescriptorIndex(uint32_t descriptorIndex) const; // current best anchor: owner vtable +0xfc when fed owner `+0x12c`
    const char* ResolveMarginRouteFromWorldId(uint32_t worldId) const;         // provisional fallback helper for world-id keyed route recovery
    const char* ResolveMarginRouteDescriptor() const;                          // current best owner-side route-text resolver used to back arg6 `+0x10c`
    // anchor: launcher.exe:0x41f2c0 / ILTLoginMediator.Default slot +0x10c
    // Keep the wrapper-facing small-string object explicit instead of collapsing it into the
    // owner-side route-text helper family.
    RouteDescriptor30SmallStringLikeSketch* GetRouteDescriptor30() override;
    // anchor: launcher.exe:0x41af50 / ILTLoginMediator.Default slot +0x118
    // Keep the wrapper-facing late-entry vector-like object explicit; the current source-owned
    // replacement still exposes an empty list scaffold, but the ABI shape now lives on the owner.
    LateEntryList1470VectorLikeSketch* GetLateEntryList1470() override;
    // Wrapper-facing arg6 profile-path/current-slot ABI objects.
    // Keep this split explicit instead of forcing the owner-side `0x004b01c8 +0x40/+0x44`
    // slot-record helpers onto the wrapper-facing `ILTLoginMediator.Default +0x40/+0x44` object
    // shapes.
    Arg6SelectionDescriptor40ObjectSketch* GetArg6SelectionDescriptorObject40(
        uint32_t selectionIndex) override;
    Arg6CurrentSlotRecord44ObjectSketch* GetArg6CurrentSlotRecordObject44() override;

    // Post-auth slot/route families recovered around helper10 (`0x4401a0`) and the later
    // state-8 margin dispatcher (`0x439300`).
    // anchor: launcher.exe:0x41f2e0 / owner vtable +0x30
    const SlotRecordState004b5328* GetSlotRecordByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f300 / owner vtable +0x40
    const SlotRecordState004b5328* GetCurrentSlotRecord() const;
    // anchor: launcher.exe:0x41b220
    // Source-owned helper over the recovered slot-record table; do not treat this as the current
    // `ILTLoginMediator` vtable slot `+0xdc` name.
    const char* LookupSlotRecordHeapStringByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f320 / owner vtable +0x148
    const char* GetGameSessionId() const override;
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
    uint32_t InvokeSessionCallbackHelper65cVtable4IfPresent();
    // source-owned shared helper used by `CLTLoginState_State18` slot 3 / `0x421a50`
    uint32_t RefreshSessionHelperGameSessionId664FromSourceBlock94();
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    SessionCallbackHelper65cSketch* GetSessionCallbackHelper65c() const override;
    // anchor: launcher.exe:0x4202c0 / owner vtable +0x13c
    void HelperSlot13c_InvokeSessionHelperVtable4() override;
    // anchor: launcher.exe:0x41b260
    // Source-owned helper over the recovered route-host string table; do not treat this as the
    // current `ILTLoginMediator` vtable slot `+0xe0` name.
    const char* LookupRouteHostPrefixBySlot(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2a0 / owner vtable +0xe4? / current slot-record payload reader
    uint8_t GetSlotRecordStatusByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2e0 / owner vtable +0xfc
    const char* GetDescriptorInlineNameByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b320 / owner vtable +0x100
    uint8_t GetDescriptorStatusByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b360 / owner vtable +0x104
    uint8_t GetDescriptorTypeByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b3a0 / owner vtable +0x108
    uint8_t GetDescriptorPopulationNibbleByIndex(uint8_t slotIndex) const;

    // Wrapper-facing arg6 `+0x44` source picker / scratch builder.
    const SlotRecordState004b5328* ResolveArg6CurrentSlotRecord44Source() const;
    bool RefreshArg6CurrentSlotRecordObject44();

    // =============================================================================
    // Post-Auth Margin/Loading State (launcher.exe:0x4f78b8)
    // =============================================================================
    // Recovered from Ghidra analysis of launcher.exe state10/state11-era functions:
    // - 0x43c020 = CLTLoginState_State11 slot 3 send body
    //   Builds/sends margin packet with first payload byte 0x4d, posts event 0x15
    // - 0x440320 = CLTLoginState_State11 slot 6 reply body
    //   Handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
    //   posts event 0x16 on completion
    // =============================================================================

    // +0x24
    // anchor: launcher.exe:0x41ecd0
    uint32_t ProcessLoginRequest(const ProcessLoginRequestInputSketch& input);

    // +0xec
    // anchor: launcher.exe:0x41c1f0
    // Owner-side state3-wait advance: persists the selection/config snapshot and switches to state8.
    uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) override;
    const State8PersistenceF1cSnapshot& State8PersistenceF1cView() const;
    const void* GetState8PersistenceF1c() const override;
    const State3SelectionContextInputSketch& SelectionContext0ecCopy() const { return selectionContext0ecCopy_; }
    bool SelectionContext0ecCopyValid() const { return selectionContext0ecCopyValid_; }
    uint32_t SelectionContext0ecCount() const { return selection0ecCount_; }
    uint32_t Profile0f4Count() const { return profile0f4Count_; }
    void ResetSelectionContext0ecMirror();

    // +0x120
    // anchor: launcher.exe:0x41c3c0
    uint32_t ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input) override;
    // wrapper-facing arg6 `+0x120` entry used by `client.dll:0x62054d1d`
    // Keep the instance-role split explicit in source:
    // - the wrapper-facing `ILTLoginMediator.Default` mirror should capture the source block even
    //   when it is not the live owner/controller instance
    // - the live owner/controller still applies the real `0x41c3c0` state gate and helper-state
    //   transition to `10`
    uint32_t CaptureProcessLoginCredentialsArg6Slot120(
        const void* input120,
        void* returnAddress,
        bool applyOwnerSemantics);

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
    //   - source now also keeps the object88 branch split explicit there:
    //     - mode probe `(+0x44)->(+0x30)`
    //     - direct submit `+0x28`
    //     - managed submit `+0x1c`, `+0x18`, `+0x24`
    uint32_t State9SubmitFollowupScaffold(uint8_t helperByte4, uint16_t helperWord6);
    // Shared owner-side helper for the wrapper-facing `+0x16c` close/wait-event-`0x0f` surface
    // and the owner-anchored state9 success-side effect at `0x41b420`.
    bool PrepareMarginConnectionCloseWaitEvent0fScaffold(
        uint32_t* outConnectionState,
        bool* outWouldCallConnectionClose0c,
        bool clearState10SendGateF14);
    // - state9 slot 6 success side effect / launcher.exe:0x41b420 (owner vtable +0x16c)
    uint32_t HandleState9Opcode11SuccessSideEffect();
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
    // These methods expose the owner fields used by the later post-auth margin/loading path.
    // They are used to faithfully reconstruct the original launcher's margin packet building
    // and load character reply handling logic.
    // =============================================================================

    // State-3(wait) -> state-8 owner-side selection/config snapshot block (`0x41c1f0`):
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

    // Later post-auth source block (`0x43c020`, `0x440320`):
    const std::array<char, 0x20>& SourceLeadString108() const { return postAuthMarginLoadingState_.sourceLeadString108; }
    uint32_t SourceField12c() const { return postAuthMarginLoadingState_.sourceField12c; }
    const std::array<uint32_t, 17>& SourceDwords134() const { return postAuthMarginLoadingState_.sourceDwords134; }
    const std::array<uint8_t, 0x20>& SourceBlock178() const { return postAuthMarginLoadingState_.sourceBlock178; }
    const std::array<uint8_t, 0x20>& SourceBlock198() const { return postAuthMarginLoadingState_.sourceBlock198; }
    const std::array<uint8_t, 0x400>& SourceBlock1b8() const { return postAuthMarginLoadingState_.sourceBlock1b8; }

    // State-owned slot-6 bodies keep class ownership, but mutate this mediator-owned owner-state
    // block through a narrow explicit accessor instead of duplicating the storage elsewhere.
    PostAuthMarginLoadingState& MutablePostAuthMarginLoadingState() { return postAuthMarginLoadingState_; }
    const PostAuthMarginLoadingState& PostAuthMarginLoadingStateView() const { return postAuthMarginLoadingState_; }

    // Post-auth load-character reply outputs (0x440320) plus neighboring auth/margin send flags:
    uint32_t& WorldListCountOrStatus80() { return postAuthMarginLoadingState_.worldListCountOrStatus80; }
    // Caution: original `0x4390b0` writes owner `+0x2c` on its non-zero payload branch, while the
    // current replacement also reuses this byte as a narrow live auth-ready alias on the active
    // happy path. Keep the storage stable, but do not overstate the exact original semantic yet.
    uint8_t AuthConnectionFlag2c() const { return authConnectionFlag2c_; }
    uint8_t& AuthConnectionFlag2c() { return authConnectionFlag2c_; }
    uint8_t State10SendGateFlagF14() const { return postAuthMarginLoadingState_.state10SendGateFlagF14; }
    uint8_t& State10SendGateFlagF14() { return postAuthMarginLoadingState_.state10SendGateFlagF14; }
    uint32_t State6UdpSessionSecretF18() const;
    void SetState6UdpSessionSecretF18(uint32_t value);
    bool CopyMarginBootstrapTwofishKeyScaffold(std::array<uint8_t, 16>* outKey) const;
    // anchor: launcher.exe:0x41f370 / owner vtable +0x50
    // Later runtime uses the auth-reply-derived bootstrap `+0xf4` copy, not the earlier direct
    // child `+0xa8` worker slot.
    // Current tighter read:
    // - original child `+0xf4` = reply-derived copied `0x136` block materialized by `0x448140`
    // - current source keeps only a narrowed shadow of that copied block's exposed `+0x85/+0xa8`
    //   suffix family
    void* BootstrapRaw08AuxHandle50() const override;
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
    void ResetRecoveredAuthBootstrapDynamicStateScaffold();
    void SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(const mxo::auth::GetPublicKeyReply& reply);
    void SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
        const mxo::auth::AuthChallengeResponseBuildResult& buildResult);
    void SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(const mxo::auth::AuthReply& reply);
    void SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world);
    void SeedRecoveredCharacterSlotRecordFromAuthReply(uint8_t characterIndex, const mxo::auth::AuthCharacterEntry& character);
    int FindRecoveredWorldDescriptorIndexByWorldId(uint16_t worldId) const;
    void SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();
    void MirrorProcessLoginCredentialsSourceBlock120(const ProcessLoginCredentialsInputSketch& input);
    void AdoptAuthReplyIntoRecoveredMediatorState();

    uint32_t SendMarginFramedPacket(
        const mxo::auth::FramedPacket& packet,
        uint8_t plainRawCode,
        const char* stepLabel,
        bool encryptedTransport);
    uint32_t ContinueMarginBootstrapHandshake(
        const uint8_t* payloadBytes,
        size_t payloadSize,
        bool transportEncrypted);
    // ABI-safety note:
    // - margin CERT/MS bootstrap/session storage is intentionally kept in a sidecar keyed by the
    //   mediator pointer in `loginmediator.cpp`
    // - do not add those transient fields into the middle of `CLTLoginMediator`
    void ResetMarginBootstrapState();

    void BuildAuthEndpoint();
    void RefreshAuthAddressListForCurrentHostScaffold();
    void PrepareNextAuthEndpointForConnectAttemptScaffold();
    void BuildMarginEndpoint();
    bool RebuildMarginAddressList();
    bool SelectMarginEndpointIpv4();
    mxo::liblttcp::CMessageConnection* EnsureAuthConnectionObject();
    mxo::liblttcp::CMessageConnection* EnsureMarginConnectionObject();
    uint32_t ContinueRecordedAuthConnectStatusScaffold();

    // Condensed `0x4f78b8` owner sketch for the active branch:
    // - `+0x10` = current helper/state object
    //   - newer receive-side tightening now also makes two mediator wrappers around that field
    //     more concrete:
    //     - `0x41f260` re-enters current helper vtable `+0x14` (current best read: slot 6)
    //     - `0x41afc0` is the margin-completion fallback that re-enters current helper vtable
    //       `+0x04` (current best read: slot 2), and can also clear owner `+0x1c/+0x20` on
    //       work type `1`
    //     - newer late natural-original proof now places that fallback concretely on the later
    //       post-state9 tail: `0x41afc0 -> 0x438df0 -> 0x41cfb0(0x0f)`
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
    CLTLoginState* scaffoldState0_;
    CLTLoginState* scaffoldState1_;
    CLTLoginState* scaffoldState2_;
    CLTLoginState* scaffoldState3_;
    CLTLoginState* scaffoldState4_;
    CLTLoginState* scaffoldState6_;
    CLTLoginState* scaffoldState8_;
    CLTLoginState* scaffoldState9_;
    CLTLoginState* scaffoldState10_;
    CLTLoginState* scaffoldState11_;
    CLTLoginState* scaffoldState12_;
    CLTLoginState* scaffoldState13_;
    CLTLoginState* scaffoldState14_;
    CLTLoginState* scaffoldState15_;
    CLTLoginState* scaffoldState16_;
    CLTLoginState* scaffoldState17_;
    CLTLoginState* scaffoldState18_;
    CLTLoginState* scaffoldState19_;

    mxo::liblttcp::CMessageConnection* authConnection_;
    mxo::liblttcp::CMessageConnection* marginConnection_;
    bool authConnectionOwnedByMediator_ = false;
    bool marginConnectionOwnedByMediator_ = false;
    void* authConnectionContextKey_;
    void* marginConnectionContextKey_;

    ConnectionHelperFamily helpers_;
    MarginRouteState marginRouteState_;
    MarginAddressListState marginAddressList3c_{};
    // owner `+0x24/+0x2c/+0x2d/+0x7c` connection-routing/teardown family:
    // - `+0x24` = margin begin-count from `0x41e500`
    // - `+0x2c` = shared slot1 / event-`1` gate armed by vtable `+0x164 / 0x41b3f0`
    // - `+0x2d` = shared slot2 / event-`0x0f` gate armed by vtable `+0x16c / 0x41b420`
    // - `+0x7c` = selected IPv4 for margin endpoint materialization
    uint32_t marginBeginCount24_ = 0;
    uint8_t authConnectionFlag2c_ = 0;
    uint8_t marginConnectionFlag2d_ = 0;
    uint32_t marginSelectedIpv4_7c_ = 0;
    AuthBootstrapSelectedSource38Sketch authBootstrapSource38_;
    // Separate phase-2 auth/bootstrap child rooted at owner `+0x680`.
    // Keep this as a distinct child/module mirror rather than folding its fields back into the
    // mediator body comments or generic mediator helpers.
    // Current tightened high-value family inside that child:
    // - `+0xa0` = auth-request-ready byte set by successful raw `0x07` handling and tested by
    //   `0x448050`
    // - `+0xa4` = lazy `pubkey.dat`-backed state primed by `0x447260/0x447c10`
    // - `+0xa8` = raw `0x08` reply-public-key worker used by `0x4474f0`
    // - `+0xf4` = original reply-derived copied `0x136` block; current source keeps only a
    //   narrowed shadow of its later exposed `+0x85/+0xa8` suffix family
    AuthBootstrap680ChildSketch authBootstrapChild680_;
    // Source-owned mirror for owner `+0x65c`.
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    // Lazily allocated session callback helper whose `+0x18` string can later feed owner `+0x664`.
    SessionCallbackHelper65cSketch sessionCallbackHelper65cState_{};
    SessionCallbackHelper65cSketch* sessionCallbackHelper65c_ = nullptr;
    uint32_t sharedMarginPacketField660_ = 0;  // owner `+0x660`
    std::string gameSessionId664_;             // owner `+0x664`
    // Wrapper-facing arg6 object mirrors:
    // - `+0x40`  = selection-descriptor object for profile-path / arg7-derived selection work
    // - `+0x44`  = current-slot record object for later save/profile work
    // - `+0xd0`  = owner `+0x1460` small-string-like state8 section-11 view
    // - `+0x10c` = owner `+0x30` small-string-like route descriptor
    // - `+0x118` = owner `+0x1470` vector-like late-entry list
    Arg6SelectionDescriptor40PackedSketch arg6SelectionDescriptor40Packed_{};
    Arg6SelectionDescriptor40ObjectSketch arg6SelectionDescriptor40_{};
    Arg6CurrentSlotRecord44PayloadSketch arg6CurrentSlotRecord44Payload_{};
    Arg6CurrentSlotRecord44ObjectSketch arg6CurrentSlotRecord44_{};
    std::string arg6CurrentSlotRecord44NameOwned_;
    bool arg6CurrentSlotRecord44Present_ = false;
    std::string state8Section11String1460Owned_;
    RouteDescriptor30SmallStringLikeSketch state8Section11String1460_{};
    std::string routeDescriptor30Owned_;
    RouteDescriptor30SmallStringLikeSketch routeDescriptor30_{};
    std::vector<LateEntryList1470EntrySketch> lateEntryList1470Entries_{};
    LateEntryList1470VectorLikeSketch lateEntryList1470_{};
    // Narrow source-owned post-state9 / post-state12 owner collaborators from
    // `0x41f1d0` / `0x41de40` / `0x41c5c0` / `0x41c510`.
    // Strongest current origin: deeper client init owner/arg6 vtable `+0x124`
    // (`netShell, netMgr, distrObjExecutive`) copied directly into this triple by `0x41f1d0`.
    void* ownerCallback84_ = nullptr;          // owner `+0x84`; bounded active-scope writes now read as init-zero at `0x41ee60`, startup-triple store at `0x41f1d0`, then submit-side reads
    void* ownerObject88_ = nullptr;            // owner `+0x88`; natural-original object88 cross-checks as client `INetMgr.Default` wrapper and no later bounded active-scope launcher write is isolated yet beyond `0x41f1d0`
    void* ownerObject8c_ = nullptr;            // owner `+0x8c`
    // +0x124 wrapper-facing startup triple capture state (owner-side mirror remains explicit in
    // `SetState9CallbackObjectTriple84_88_8c`).
    void* provideStartupTripleNetShell_ = nullptr;         // +0x124 netShell
    void* provideStartupTripleNetMgr_ = nullptr;           // +0x124 netMgr
    void* provideStartupTripleDistrObjExecutive_ = nullptr; // +0x124 distrObjExecutive
    uint32_t provideStartupTripleCount_ = 0u;              // +0x124 call count
    const void* arg6ProcessLoginCredentialsInput120_ = nullptr; // wrapper-facing `+0x120` last raw input pointer
    uint32_t arg6ProcessLoginCredentialsCount120_ = 0u;         // wrapper-facing `+0x120` call count
    uint32_t ownerOptionalField90_ = 0;                  // owner `+0x90`, only forwarded when helper byte `+4 != 0`
    int32_t ownerCachedHandle147c_ = -1;       // owner `+0x147c`, managed-submit handle cached across `+0x1c` release / `+0x18` reacquire
    // launcher.exe:0x4f78b8 owner-side persisted selection/config snapshot (`0x41c1f0`).
    // This is filled by the owner-side advance out of state3-wait, not by a state3-local body.
    State8SelectionContextSnapshotState state8SelectionContextSnapshotState_;
    // +0xec / +0xf4 wrapper-owned mirrors now live on the mediator instance instead of in the
    // launcher ABI shell.
    State3SelectionContextInputSketch selectionContext0ecCopy_{};
    bool selectionContext0ecCopyValid_ = false;
    uint32_t selection0ecCount_ = 0;
    // Wrapper-facing contiguous `mcd.cfg` snapshot backing for arg6 `+0xbc/+0xc0/+0xf4`.
    mutable State8PersistenceF1cSnapshot state8PersistenceF1c_{};
    mutable uint32_t profile0f4Count_ = 0;
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

    // Observer/listener state for late-login arg6 +0x170/+0x174 bridge:
    // - +0x170 (RegisterLoginObserver) inserts into owner `+0x674` listener tree
    // - +0x174 (UnregisterLoginObserver) removes from the same tree
    // These fields mirror launcher.exe runtime state for diagnostic logging.
    void* firstObserver170_ = nullptr;          // owner `+0x674` first registered observer
    void* latestObserver170_ = nullptr;         // owner `+0x674` most recent register call
    void* latestObserver174_ = nullptr;         // owner `+0x674` most recent unregister call
    uint32_t observerRegister170Count_ = 0;     // owner `+0x674` register call count
    uint32_t observerUnregister174Count_ = 0;   // owner `+0x674` unregister call count

    // Source-owned default-off mirror for the alternate
    // `g_LaunchPadGateState16State18 != 0` state16/state18 family.
    bool processLoginRequestAlternateState16BranchScaffold_ = false;

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
    // ABI-safety note:
    // - recovered margin bootstrap/session state derived from auth reply material is stored in a
    //   sidecar keyed by `this` in `loginmediator.cpp`
    // - that keeps existing in-class field order stable while we still treat this storage as
    //   launcher-owned transitional state


    uint32_t lastAuthConnectStatus_;
    uint32_t lastMarginConnectStatus_;
    uint32_t authConnectStatusCount_;
    uint32_t marginConnectStatusCount_;
    const char* expectedAuthRequestName_;
    const char* expectedMarginRequestName_;

    std::array<void*, kRecoveredWorldSlotCapacity> worldSlots_;
    std::array<void*, kRecoveredWorldSlotCapacity> worldPayloadSlots_;

    // Append-only source-owned mirror of the auth-side owner `+0x28/+0x4c/+0x50/+0x58`
    // retry/iterator family recovered from `0x41d170 / 0x440bb0 / 0x4390b0`.
    AuthAddressListState authAddressList4c_{};

    // launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (world list data provider)
    // Faithful implementation of arg6 world list provider for InitClientDLL
    // Vtable at offset +0xc from object pointer at 0x4d2c58
    // =============================================================================
    // Address anchors for arg6 world list provider:
    // launcher.exe:0x4d3584 +0xc = vtable (ILTLoginMediator)
    // launcher.exe:0x4d3584 +0x10 = ILTLoginMediator_BuildWorldList()
    // launcher.exe:0x4d3584 +0x14 = GetWorldNameByIndex(char*)
    // launcher.exe:0x4d3584 +0x18 = startup-only gate-byte fallback feeding wrapper slot +0x100
    // launcher.exe:0x4d3584 +0x1c = Arg6ValidateWorldSelection(uint -> 0 or 7)
    // launcher.exe:0x4d3584 +0x20 = Arg6GetWorldListCount(uint)
    // launcher.exe:0x4d3584 +0x24 = Arg6GetActiveWorldListCount(uint)
    // launcher.exe:0x4d3584 +0x28 = Arg6GetAvailableWorlds(bool)
    // =============================================================================
    struct Arg6WorldListData {
        // launcher.exe:0x4d3584 +0xfc = GetWorldNameByIndex(index) -> char*
        std::array<std::string, 10> worldNames_ = {"Default", "Starter", "Classic", "Advanced", "Extreme"};

        // launcher.exe:0x4d3584 +0x100 = startup-only synthetic gate byte used before the
        // recovered owner `+0xd84` descriptor table exists
        std::array<uint8_t, 10> worldSelectionGateBytes100_ = {1, 2, 3, 5, 1};

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
        // Startup-only synthetic fallback for wrapper slot `+0x100` before the owner-side
        // descriptor table exists. Keep the split explicit: this is not yet a proved direct alias
        // for the later owner descriptor Status/Type bytes.
        uint32_t selectedSelectionGateByte100_ = 1;
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
    uint32_t arg6VariantWorldNameQueryCountE0_ = 0u; // wrapper-facing arg6 `+0xe0` query count

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
