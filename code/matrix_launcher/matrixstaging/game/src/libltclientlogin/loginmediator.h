#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"
#include "../../../runtime/src/libltmessaging/messageconnection.h"
#include "../../../runtime/src/liblttcp/ltipaddresslist.h"
#include "../../../runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include "loginmediator_base.h"
#include "authbootstrap680_internal.h"

// Forward declaration - full class is defined in loginmediator_events.cpp
class LoginObserverTreeHelper674;

namespace mxo {
namespace ltlogin {

class CLTLoginState;
class CLTLoginState_State10;
class CLTLoginState_AuthenticatePending;
class CLTLoginState_WorldListPending;
class CLTLoginMediator;

// Fidelity note:
// - active launcher.exe worker/queue paths are now modeled as direct-connection flows
// - auth/margin connection ctors still store the owning `CLTLoginMediator*` directly at
//   connection `+0xa4`
// - the earlier mediator-owned bridge-context scaffolds have been retired from the active path

// owner `+0x674` listener tree sketch tightened from `0x41ddb0 / 0x41dde0 / 0x41cfb0 / 0x41d090`:
// - container object is an 8-byte pair `{ headerPtr, nodeCount }`
// - this matches the small non-vtable OOAnalyzer class at `0x419510`
// - header node is std::_Tree-like and self-referential when empty
//   - `header + 0x04` = root
//   - `header + 0x08` = leftmost/begin
//   - `header + 0x0c` = rightmost
// - concrete nodes compare/store the observer pointer at `+0x10`
struct LoginObserverTreeNode674 {
    uint32_t colorOrFlags00 = 0;                  // SGI/std::_Tree-like node color/flag field; exact bit meaning still unresolved
    LoginObserverTreeNode674* parent04 = nullptr;
    LoginObserverTreeNode674* left08 = nullptr;
    LoginObserverTreeNode674* right0c = nullptr;
    void* observerKey10 = nullptr;
};

struct LoginObserverTree674 {
    LoginObserverTreeNode674* header00 = nullptr;
    uint32_t nodeCount04 = 0;
};

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
    // - state2 `0x43f300` now also needs narrow access because early inbound auth staging/demux
    //   is source-owned there instead of in the mediator wrapper
    // - keep access narrow by granting focused friendship instead of widening the mediator
    //   surface generically
    friend struct AuthBootstrap680Child_0x441290;
    friend void AuthBootstrap680LogParsedAuthReply(
        const CLTLoginMediator& owner,
        const mxo::auth::AuthReply& reply);
    friend void AuthBootstrap680MaterializeReplyCopyShadowScaffold(
        AuthBootstrap680Child_0x441290& child,
        CLTLoginMediator& owner,
        const mxo::auth::AuthReply& reply);
    friend void AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
        AuthBootstrap680Child_0x441290& child,
        CLTLoginMediator& owner,
        const mxo::auth::AuthReply& reply);
    friend void AuthBootstrap680SyncState2AuthReplySuccessOneTimeScaffold(
        AuthBootstrap680Child_0x441290& child,
        CLTLoginMediator& owner,
        const mxo::auth::AuthReply& reply);
    friend class CLTLoginState_AuthenticatePending;
    friend class CLTLoginState_State4;
    friend class CLTLoginState_State6;
    friend class CLTLoginState_State7;
    friend class CLTLoginState_State8;
    friend class CLTLoginState_State10;
    friend class CLTLoginState_State11;
    friend class mxo::liblttcp::CAuthStartupConnection;
    friend class mxo::liblttcp::CMarginConnection;

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
        // launcher.exe:0x43b300 seeds the contiguous helper/state table rooted at `0x4f7868`,
        // immediately after `0x4f78b8 = esi`.
        //
        // From Ghidra decompilation of `CLTLoginMediator_InitializeHelperDispatchTable`:
        // - The recovered routine allocates the helper/state objects installed into `0x4f7868..0x4f78b4`
        //   in one contiguous pass covering slots `0..19`
        // - Those objects carry `CLTLoginState_*` vtables such as `0x4b4fc4`, `0x4b4fec`, `0x4b5014`
        // - slot 1 on many of those vtables reuses shared gate `0x438d80`
        // - `InitializeHelperDispatchTable()` now covers the earlier `0..14`
        //   registrations and keeps the late `15..19` tail in the same seed
        //
        // Discovered function names from Ghidra renaming:
        // - launcher.exe:0x438d80 = shared `CLTLoginState_*` slot-1 gate
        //   - older Ghidra label: `LaunchPadClient_ProcessEvent0x17`
        // - launcher.exe:0x4816f0 = reused inline helper that returns the queued work-item type
        //   - current Ghidra name: `CLTThreadPerClientTCPEngine_WorkItemHeader_GetWorkType`
        //   - older Ghidra label: `LaunchPadClient_GetVtableOffset`
        // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
        // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
        // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
        //
        // Disassembly of 0x438d80 shows:
        //   - Calls reused inline helper
        //     `CLTThreadPerClientTCPEngine_WorkItemHeader_GetWorkType(this+8)`
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
        //   - source material resolved through mediator `+0xd4`
        //   - original `+0xd4` body is the tiny live-pointer read `owner +0x1c + 0x85`
        //   - active replacement mirrors the recovered challenge key into that live field early
        //   - keep the older launcher-owned bootstrap sidecar as bounded fallback on `+0xd4`
        //     until runtime evidence is stable enough to prune it without risking game entry
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

    // Source implementation of the original shared owner-side class now named in Ghidra as
    // `CLTLoginMediatorCharacterPersistenceData_0x41d900`.
    // This is the canonical owner object behind the late getter family rooted at:
    // - `+0xbc -> +0xf48`
    // - `+0xc0 -> +0xf88`
    // - `+0xc4 -> +0x13f0/+0x13f4`
    // - `+0xf4 -> +0xf1c`
#pragma pack(push, 1)
    class CLTLoginMediatorCharacterPersistenceData {
    public:
        static constexpr size_t kBodySize = 0x465;

        std::array<char, 0x20> characterName00{};     // `+0x00`
        uint32_t replyField20 = 0;                    // `+0x20`
        uint32_t selectedWorldField24 = 0;            // `+0x24`
        uint32_t field28_1000 = 0x1000;               // `+0x28`
        std::array<uint32_t, 8> header2c{};           // `+0x2c .. +0x4b`
        std::array<uint32_t, 8> secondary4c{};        // `+0x4c .. +0x6b`
        uint32_t bodyWord6c = 0x1000;                 // `+0x6c`; ctor seeds this to `0x1000`
        std::array<char, 0x20> realFirstName70{};     // `+0x70 .. +0x8f`
        std::array<char, 0x20> realLastName90{};      // `+0x90 .. +0xaf`
        std::array<char, 0x400> backgroundB0{};       // `+0xb0 .. +0x4af`
        uint32_t replySectionData4b0 = 0;             // `+0x4b0`
        uint32_t replySectionData4b4 = 0;             // `+0x4b4`
        std::array<uint8_t, 0x19> tail4b8{{1u}};      // `+0x4b8 .. +0x4d0`; ctor seeds byte `+0x4b8 = 1`
        std::array<uint8_t, 3> gap4d1{};              // `+0x4d1 .. +0x4d3`
        void* section0OverflowBuffer4d4 = nullptr;    // `+0x4d4`
        uint16_t section0OverflowLength4d8 = 0;       // `+0x4d8`
        uint8_t section0PresentFlag4da = 0;           // `+0x4da`
        uint8_t pad4db = 0;                           // `+0x4db`
        void* section01Buffer4dc = nullptr;           // `+0x4dc`
        uint16_t section01Length4e0 = 0;              // `+0x4e0`
        uint8_t section01PresentFlag4e2 = 0;          // `+0x4e2`
        uint8_t pad4e3 = 0;                           // `+0x4e3`
        void* section02Buffer4e4 = nullptr;           // `+0x4e4`
        uint16_t section02Length4e8 = 0;              // `+0x4e8`
        uint8_t section02PresentFlag4ea = 0;          // `+0x4ea`
        uint8_t pad4eb = 0;                           // `+0x4eb`
        void* section06Buffer4ec = nullptr;           // `+0x4ec`
        uint16_t section06Length4f0 = 0;              // `+0x4f0`
        uint8_t section06PresentFlag4f2 = 0;          // `+0x4f2`
        uint8_t pad4f3 = 0;                           // `+0x4f3`
        void* section07Buffer4f4 = nullptr;           // `+0x4f4`
        uint16_t section07Length4f8 = 0;              // `+0x4f8`
        uint8_t section07PresentFlag4fa = 0;          // `+0x4fa`
        uint8_t pad4fb = 0;                           // `+0x4fb`
        void* section03Buffer4fc = nullptr;           // `+0x4fc`
        uint16_t section03Length500 = 0;              // `+0x500`
        uint8_t section03PresentFlag502 = 0;          // `+0x502`
        uint8_t pad503 = 0;                           // `+0x503`
        void* section04Buffer504 = nullptr;           // `+0x504`
        uint16_t section04Length508 = 0;              // `+0x508`
        uint8_t section04PresentFlag50a = 0;          // `+0x50a`
        uint8_t pad50b = 0;                           // `+0x50b`
        void* section05Buffer50c = nullptr;           // `+0x50c`
        uint16_t section05Length510 = 0;              // `+0x510`
        uint8_t section05PresentFlag512 = 0;          // `+0x512`
        uint8_t pad513 = 0;                           // `+0x513`
        void* section0cBuffer514 = nullptr;           // `+0x514`
        uint16_t section0cLength518 = 0;              // `+0x518`
        uint8_t section0cPresentFlag51a = 0;          // `+0x51a`
        uint8_t pad51b = 0;                           // `+0x51b`
        void* section0dBuffer51c = nullptr;           // `+0x51c`
        uint16_t section0dLength520 = 0;              // `+0x520`
        uint8_t section0dPresentFlag522 = 0;          // `+0x522`
        uint8_t pad523 = 0;                           // `+0x523`
        void* section08Buffer524 = nullptr;           // `+0x524`
        uint32_t section08Length528 = 0;              // `+0x528`
        uint8_t section08PresentFlag52c = 0;          // `+0x52c`
        std::array<uint8_t, 3> pad52d{};              // `+0x52d .. +0x52f`
        void* section09Buffer530 = nullptr;           // `+0x530`
        uint16_t section09Length534 = 0;              // `+0x534`
        uint8_t section09PresentFlag536 = 0;          // `+0x536`
        uint8_t pad537 = 0;                           // `+0x537`
        void* section0aChunkedBuffer538 = nullptr;    // `+0x538`
        uint16_t section0aChunkedLength53c = 0;       // `+0x53c`
        uint8_t section0aPresentFlag53e = 0;          // `+0x53e`
        uint8_t pad53f = 0;                           // `+0x53f`
        uint32_t section11Dword540 = 0;               // `+0x540`
        char* section11StringBegin544 = nullptr;      // `+0x544`
        char* section11StringCurrent548 = nullptr;    // `+0x548`
        char* section11StringCapacity54c = nullptr;   // `+0x54c`
    };

    static_assert(sizeof(CLTLoginMediatorCharacterPersistenceData) == 0x550);
#pragma pack(pop)

    class PostAuthMarginLoadingState {
    public:
        // Source-owned owner layout projection over the original shared
        // `CLTLoginMediatorCharacterPersistenceData_0x41d900` family.
        // owner post-auth margin/loading block shared by the active state8 path and the later
        // state10/state11 path.
        // Current high-value field read:
        // - `+0x108 .. +0x5d8` = earlier shared `CLTLoginMediatorCharacterPersistenceData`
        //   source block reused by the create-character / helper11 path
        //   - `characterName00` = owner `+0x108`
        //   - `selectedWorldField24` = owner `+0x12c`
        //   - `header2c[0..16]` = owner `+0x134 .. +0x174`
        //   - `realFirstName70` = owner `+0x178`
        //   - `realLastName90` = owner `+0x198`
        //   - `backgroundB0` = owner `+0x1b8`
        //   - client wrapper-facing `+0x120` writer currently copies up to `0x400` bytes there
        //   - later state11 packet building still uses only the bounded prefix that fits the
        //     packet builder's own field limits
        // - `+0xf1c ...` = later load-character reply materialization area
        CLTLoginMediatorCharacterPersistenceData createCharacterData108{}; // owner `+0x108 .. +0x5d8`

        // ========================================================================
        // Post-auth HandleLoadCharacterReply outputs (0x440320)
        // ========================================================================

        // owner byte `+0xf14`; shared send gate used by the active state8 path and later state10.
        // Strongest current writer is state6 opcode-`9` success, which sets it alongside owner
        // `+0xf18 = parsed opcode-9 UDPSessionSecret / session-id dword`; later clears are now
        // anchored both at state9 success `0x41b420` and margin-completion work-type-1 tail
        // `0x44af60`.
        uint8_t state10SendGateFlagF14 = 0;             // `+0xf14`

        // Canonical owner `+0xf1c` object returned by the original `+0xf4/+0xbc/+0xc0/+0xc4`
        // getter family.
        CLTLoginMediatorCharacterPersistenceData state8PersistenceDataF1c{}; // owner `+0xf1c .. +0x146b`

        // Legacy decomposed mirrors still kept while neighboring source is migrated onto the
        // canonical `state8PersistenceDataF1c` object.
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
        // Source-owned mirror of owner `+0x818`.
        // Faithful current read from `0x43f300 / 0x4401a0 / 0x41b260`:
        // - original storage is the common 3-dword string-triple family (`begin/current/capacity`)
        // - `+0x818` stores the copied world-descriptor inline-name string (`payload + 0x03`)
        // - wrapper-facing arg6 `+0xe0` returns that same begin pointer when the slot is live
        // - state-7/8/0x0d margin-route consumers then reuse that copied descriptor text as their
        //   route/world string input instead of a replacement-only lowercase host-prefix variant
        // - `+0x1470` remains a separate later late-entry string-triple vector
        std::string owned;
        const char* begin = nullptr;
        const char* current = nullptr;
        const char* capacity = nullptr;

        RouteHostStringTripleState() { SyncFromOwned(); }

        RouteHostStringTripleState(const RouteHostStringTripleState& other)
            : owned(other.owned) {
            SyncFromOwned();
        }

        RouteHostStringTripleState& operator=(const RouteHostStringTripleState& other) {
            if (this != &other) {
                owned = other.owned;
                SyncFromOwned();
            }
            return *this;
        }

        RouteHostStringTripleState(RouteHostStringTripleState&& other) noexcept
            : owned(std::move(other.owned)) {
            SyncFromOwned();
            other.SyncFromOwned();
        }

        RouteHostStringTripleState& operator=(RouteHostStringTripleState&& other) noexcept {
            if (this != &other) {
                owned = std::move(other.owned);
                SyncFromOwned();
                other.SyncFromOwned();
            }
            return *this;
        }

        void Assign(const char* value) {
            owned = value ? value : "";
            SyncFromOwned();
        }

        void Assign(const std::string& value) {
            owned = value;
            SyncFromOwned();
        }

        void Clear() {
            if (!owned.empty()) {
                owned.clear();
            }
            SyncFromOwned();
        }

        void ReleaseStorage() {
            owned.clear();
            owned.shrink_to_fit();
            SyncFromOwned();
        }

        const char* BeginOrNull() const {
            return (begin != nullptr && begin != current) ? begin : nullptr;
        }

    private:
        void SyncFromOwned() {
            begin = owned.c_str();
            current = begin + owned.size();
            capacity = current;
        }
    };

    class CLTLoginMediatorSelectionRouteState {
    public:
        struct PersistedSelectionContext64c {
            // owner `+0x64c .. +0x6fb` inside `CLTLoginMediatorSelectionRouteState_0x41dba0`
            // which corresponds to mediator owner `+0xcd0 .. +0xd7f`
            std::array<uint32_t, 4> blockCd0{};
            std::array<uint32_t, 4> blockCe0{};
            std::array<uint32_t, 4> blockCf0{};
            std::array<uint32_t, 4> blockD00{};
            std::array<uint32_t, 4> blockD10{};
            std::array<uint32_t, 4> blockD20{};
            std::array<uint32_t, 4> blockD30{};
            std::array<uint32_t, 4> blockD40{};
            std::array<uint32_t, 4> blockD50{};
            std::array<uint32_t, 4> blockD60{};
            std::array<uint32_t, 4> blockD70{};
        };

        // anchor: launcher.exe:0x41dba0 / embedded owner subobject ctor
        CLTLoginMediatorSelectionRouteState();

        // anchor: launcher.exe:0x41d270 / embedded owner subobject reset
        void ResetSelectionRouteState();

        // anchor: launcher.exe:0x41dd00 / embedded owner subobject destroy/final release
        void DestroySelectionRouteState();

        uint8_t CurrentSlotOrSelectionIndex644() const {
            return currentSlotOrSelectionIndex644_;
        }

        void SetCurrentSlotOrSelectionIndex644(uint8_t slotIndex) {
            currentSlotOrSelectionIndex644_ = slotIndex;
        }

        // Source-owned mirrors of the original embedded helper layout:
        // - `+0x00`  = active slot-record count
        // - `+0x04`  = slot-record pointer table (mirrored here as value objects plus validity bits)
        // - `+0x194` = route-string triples
        // - `+0x644` = current slot / selection byte
        // - `+0x64c .. +0x6fb` = persisted state3(wait)->state8 snapshot body
        uint8_t slotRecordCount00_ = 0;
        std::array<SlotRecordState_0x4b5328, kRecoveredWorldSlotCapacity> slotRecordTable04_{};
        std::array<bool, kRecoveredWorldSlotCapacity> slotRecordValid04_{};
        std::array<RouteHostStringTripleState, kRecoveredWorldSlotCapacity> routeHostStringTriples194_{};
        uint8_t currentSlotOrSelectionIndex644_ = 0xffu;
        PersistedSelectionContext64c persistedSelectionContext64c_{};
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
    // Source-owned mirrors of the original owner dwords reached through the tiny setter/getter
    // family around `0x41f060/0x41f070` and `0x41f080/0x41f090`.
    uint32_t nopatchLauncherVersionValue08_ = 0;  // original owner `+0x08`
    uint32_t nopatchClientVersionValue0c_ = 0;    // original owner `+0x0c`
    mutable bool liveCuiCfgAbsentNoteLogged90_ = false;     // +0x90 one-shot caveat log moved from ABI wrapper

    // anchor: launcher.exe:0x41f070
    // Tiny owner getter reached from state2 `0x439210` through owner vtable `+0x20`.
    // Exact original body returns `owner + 0x08`; callers then dereference that dword.
    const uint32_t* GetNoPatchLauncherVersionValuePtr08() const;

    // source-owned narrow accessor for owner `+0x94 + 0x60` small-string begin pointer.
    // Current concrete state7 use from `0x43ba20`:
    // - owner vtable `+0x38` returns `this+0x94`
    // - state7 slot3 then reads the first dword at `+0x60` and threads it into the raw-`0x0d`
    //   delete-character packet's optional string field via `0x43aa80`
    const char* GetSourceBlock94SmallString60BeginScaffold() const;

    // anchor: launcher.exe:0x41f090
    // Tiny owner getter paired with `0x41f080`; exact original body returns `owner + 0x0c`.
    const uint32_t* GetNoPatchClientVersionValuePtr0c() const;

    // +0x00
    const char* GetName() override;
    // +0x08
    void Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine* networkEngineOverride) override;
    // UNANCHORED helper kept explicit from the real wrapper/owner vtable rows:
    // - wrapper `ILTLoginMediator.Default +0x08` currently forwards into `Initialize(...)`
    // - owner `CLTLoginMediator +0x0c` is `0x41f510 = ResetOwnedRuntimeState`, not a direct engine setter
    void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
    // +0x0c
    void ClearEngine() override;
    // +0x14
    uint32_t IsReady() override;
    // +0x18
    // void UnknownSlot5();
    // +0x1c
    // void UnknownSlot6();
    void SetValue1(void* value) override;
    // +0x24
    void SetValue2(void* value) override;
    // +0x2c
    uint32_t IsConnected() override;
    // +0x38
    // anchor: launcher.exe:0x41f0a0 / exact tiny body returns owner `+0x94`
    const char* GetUsername() const override;
    // +0x3c
    // anchor: launcher.exe:0x41f2d0 / exact tiny body returns owner byte `+0xcc8`
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
    // anchor: launcher.exe:0x41af00 / exact body returns owner byte `+0x684` only when the
    // current helper/state code is `>= 3`
    uint32_t GetArg7SelectionUpperBoundExclusive() const override;
    // +0xdc
    // anchor: launcher.exe:0x41b220 / same direct body as the slot-record heap-string reader over
    // owner `+0x688[index]`
    const char* MapSelectionName(uint32_t selectionHighByte) const override;
    // +0xe0
    // anchor: launcher.exe:0x41b260 / exact body is the state-gated owner `+0x818[index*3]`
    // string reader reused by the launcher page-`7` world-list path
    const char* GetVariantWorldName(uint32_t variantIndex) override;
    // +0xe4
    uint8_t GetVariantState(int32_t variantIndex) const override;
    // +0x164
    bool RequestAuthConnectionCloseWaitEvent1() override;

    // +0x34
    // anchor: launcher.exe:0x41c0d0
    // owner-side auth-close/state-reset helper used by the page-6 rich-edit observer failure path
    // before launcher retry/re-prompt.
    void RequestAuthCloseAndSwitchToState0() override;
    // +0x16c
    // Wrapper-facing split kept explicit from the owner-side state9 helper below.
    bool RequestMarginConnectionCloseWaitEvent0f() override;
    // +0x178
    uint32_t GetLastLoginStatus() override;

    void SetCurrentState(CLTLoginState* state);
    CLTLoginState* CurrentState() const;

    // anchor: launcher.exe:0x41af80 / owner vtable `+0x17c`
    // Exact owner-body mirror: compare against owner `+0x18`, clear only that field on type-1
    // close, then jump straight back through current helper slot 1.
    uint32_t HandleAuthConnectionCompletionFallback(void* connection, void* workItem) override;
    // anchor: launcher.exe:0x41f250 / owner vtable `+0x180`
    // Exact tiny wrapper: reload current helper from owner `+0x10` and tail-jump to helper
    // vtable `+0x10` / current best read slot 5 (`AuthMessageDispatch`). No synthetic guard.
    uint32_t DispatchCurrentHelperAuthMessage(void* workItem) override;
    // anchor: launcher.exe:0x41f260 / owner vtable `+0x184`
    // Exact tiny wrapper: reload current helper from owner `+0x10` and tail-jump to helper
    // vtable `+0x14` / current best read slot 6. No synthetic guard.
    uint32_t DispatchCurrentHelperSlot6(void* workItem) override;
    // anchor: launcher.exe:0x41afc0 / owner vtable `+0x188`
    // Thin owner-body mirror: compare against owner `+0x1c`, clear the live margin-connection
    // field on type-1 close, then jump straight back through current helper slot 2.
    // launcher.exe also clears owner `+0x20` here; source still lacks a proven field home for
    // that word, so keep that discrepancy explicit instead of inventing one.
    uint32_t HandleMarginConnectionCompletionFallback(void* connection, void* workItem) override;

    struct ActiveCharacterStateViewScaffold {
        const char* characterName = nullptr;
        uint32_t characterIdLow = 0;
        uint32_t characterIdHigh = 0;
        const char* realFirstName = nullptr;
        const char* realLastName = nullptr;
        const char* background = nullptr;
    };

    // Narrow wrapper-facing bridge on top of the launcher global current-mediator pointer
    // (`0x4f78b8 = g_CurrentLoginMediator`). Keep callers on this hook instead of reaching into a
    // TU-local global directly.
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


    // Anchored launcher.exe logging/event side effects at `0x41cfb0` / `0x41d090`.
    // Current implementation keeps lightweight event/error history together with the recovered
    // observer registration bridge for arg6/`ILTLoginMediator.Default` slots `+0x170/+0x174`.
    void PostEvent(uint32_t eventId);
    void PostError(uint32_t errorId);
    bool RegisterLoginObserver(void* observer) override;
    // +0x174
    bool UnregisterLoginObserver(void* observer) override;
    // Observer count getters for wrapper diagnostics (moved from g_MediatorRuntimeState):

    // Narrow post-auth receive-boundary counters used only for short runtime discrimination:
    // - no packet arrived yet
    // - packet arrived but would be consumed by base margin dispatch before slot 6
    // - packet survived into current helper slot 6
    uint32_t MarginPacketReceiveCountScaffold() const { return marginPacketReceiveCountScaffold_; }
    uint32_t MarginPacketFilteredBeforeSlot6CountScaffold() const { return marginPacketFilteredBeforeSlot6CountScaffold_; }
    uint32_t MarginPacketSlot6DispatchCountScaffold() const { return marginPacketSlot6DispatchCountScaffold_; }

    // Focused source home for this early auth/state-entry wiring:
    // - `loginmediator_auth_entry.cpp`
    // Source-owned scaffold registration for concrete CLTLoginState objects that live outside the
    // mediator header. Current source now mirrors the original storage shape more closely by
    // populating the process-global helper dispatch-table mirror rooted at `0x4f7868` instead of
    // mediator-owned per-slot fields.
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
    CLTLoginState* LoginHelperStateByIdScaffold(uint32_t helperStateId) const;
    // anchor: launcher.exe:0x41b450
    // Faithful by-id helper switch mirror for anchored call sites that statically prove the
    // original passes only the helper/state id into `0x41b450` and lets that body load the target
    // from the helper dispatch table rooted at `0x4f7868`.
    // Current source returns the new-helper slot-3 result for diagnostics even though the original
    // helper-switch body itself returns `void`.
    uint32_t SwitchHelperStateByIdScaffold(uint32_t helperStateId);
    // Installs the source-owned initial idle/start helper convention (`state0`) after
    // registration. This stays separate from owner-owned submit handling: state0 keeps the shared
    // slot-3 no-op stub, and `ProcessLoginRequest` performs the first happy-path switch to state2.
    // anchor: launcher.exe:0x43b300
    // Static helper-dispatch table seed. Does not use this pointer - only initializes
    // global state objects in g_LoginHelperDispatchTableScaffold. currentState_ init
    // should be done by caller.
    static void InitializeHelperDispatchTable();
    // anchor: launcher.exe:0x41b4f0 / arg6 vtable +0xd4
    // Late-login state9 callback-seed getter. Original body is the tiny live-pointer read
    // `owner +0x1c + 0x85`; current replacement still keeps an explicit launcher-owned
    // bootstrap-sidecar fallback on this slot until the live connection mirror is present at every
    // active caller timing.
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
    // anchor: launcher.exe:0x41e690 / mediator vtable +0x18c
    uint32_t FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) override;
    uint32_t FillState9CallbackBlob18cScaffold(uint32_t* outDwords, uint32_t arg2, uint32_t arg3);

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
    bool AuthPeerCloseQueuedScaffold() const { return authPeerCloseQueuedScaffold_; }
    bool MarginPeerCloseQueuedScaffold() const { return marginPeerCloseQueuedScaffold_; }
    void SetAuthPeerCloseQueuedScaffold(bool value) { authPeerCloseQueuedScaffold_ = value; }
    void SetMarginPeerCloseQueuedScaffold(bool value) { marginPeerCloseQueuedScaffold_ = value; }

    void SetMarginRouteHostPrefix(const char* routeHostPrefix);
    void SetExactMarginHostName(const char* exactMarginHostName);
    uint32_t MarginConnectAttemptCountScaffold() const { return marginBeginCount24_; }
    void ResetMarginConnectAttemptCountScaffold() { marginBeginCount24_ = 0u; }

    // launcher.exe:0x43b300 / full helper-dispatch table seed
    // Current best read:
    // - the decompiled routine seeds helper/state slots `0..19` in one contiguous pass
    // - source now keeps the entire seed in `InitializeHelperDispatchTable()`
    //
    // HELPER / STATE DISPATCH TABLE INITIALIZATION HELPERS (from Ghidra analysis of 0x43b300):
    // ==============================================================================
    // launcher.exe:0x4f7868..0x4f78b4 = contiguous helper/state table covering slots `0..19`
    // `InitializeHelperDispatchTable()` now performs the full seed inline
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

    // launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (world list data provider)
    // Active replacement note:
    // - the current no-GUI launcher path no longer keeps a synthetic pre-auth world-list sidecar
    // - selection now runs after auth success and consumes the recovered owner tables directly
    // - the narrower configured arg6 selection scratch below remains only for wrapper-facing
    //   descriptor/profile bridges still reached from launcher/client scaffolding
    void ConfigureArg6Selection(
        uint32_t worldUpperBoundExclusive,
        uint32_t variantUpperBoundExclusive,
        const char* mappedSelectionName,
        const char* mappedVariantName,
        uint32_t selectedWorldIndexLow24,
        uint32_t selectedVariantIndexHigh8,
        uint32_t selectedVariantState);
    // Source-owned arg6 bootstrap seed helpers.
    // These are replacement-side setup helpers, not recovered launcher.exe vtable slots.
    uint32_t Arg6WorldUpperBoundExclusive() const;
    uint32_t Arg6VariantUpperBoundExclusive() const;
    uint32_t Arg6SelectedWorldIndexLow24() const;
    uint32_t Arg6SelectedVariantIndexHigh8() const;
    uint32_t Arg6SelectedVariantState() const;
    const char* Arg6MappedSelectionName() const;
    const char* Arg6MappedVariantName() const;
    const char* Arg6ProfileName() const;
    const char* Arg6AuthName() const;
    const char* Arg6AuthPassword() const;
    bool Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const;
    uint32_t Arg6ExpectedSelectionDescriptorScratchRequest() const;
    bool Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const;

    // Wrapper-facing world-descriptor family (`+0xf8 .. +0x108`).
    // Keep the wrapper/owner split explicit:
    // - on the current active path these slots now read the recovered post-auth owner
    //   descriptor table directly
    // - owner `+0x100 / 0x41b320` reads descriptor byte `+0x17` (Status)
    // - owner `+0x104 / 0x41b360` reads descriptor byte `+0x18` (Type)
    // - `+0x104/+0x108` still stay `0` until the real descriptor table is present
    uint32_t GetWorldCount() const override;
    const char* GetWorldNameByIndex(uint32_t index) override;
    uint8_t GetWorldSelectionGateByteByIndex(uint32_t index) const override;
    uint8_t GetWorldTypeByteByIndex(uint32_t index) const override;
    uint8_t GetWorldPopulationNibbleByIndex(uint32_t index) const override;

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
    // Narrow mediator-owned margin-entry bridge for the current diagnostics auto-begin path:
    // - preserves the state4-owned `0x439300` case split instead of open-coding it in diagnostics
    // - dispatches the registered state4 slot 3 against the current helper as upstream input
    uint32_t BeginMarginConnectionViaState4Scaffold();
    // Launcher-owned arg5/auth connection migration:
    // - keep the direct auth/margin child ownership on `CLTLoginMediator`
    // - let `CLTThreadPerClientTCPEngine` own the current queue push / no-worker pump seam
    // - keep the launcher-object ABI shell thin and arg5-shaped
    void ResetLauncherConnectionsScaffold();
    uint32_t BeginLauncherMarginConnectionScaffold();
    // anchor: launcher.exe:0x41b490
    // Tiny auth transport-ready test used by state2 slot 3 before it reaches the bootstrap
    // dispatcher. Current best concrete read: auth connection exists and connection `+0x34 == 2`.
    bool HasReadyAuthConnectionState2() const;

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
    //   - auth `0x449a70 -> 0x41af80` re-enters current helper slot 1 / raw vtable `+0x00`
    //   - margin `0x44af60 -> 0x41afc0` re-enters current helper slot 2 / raw vtable `+0x04`
    // - active default password-submit continuation is now tighter:
    //   `0x41ecd0 -> state2 -> state1 -> state2 -> state3(wait) -> 0x41c1f0(owner advance)
    //    -> 0x439300 -> 0x43bd20 / 0x43f930`
    // - state3 remains the waiting helper there; do not invent a state3-local slot-3 body to
    //   explain the `0x41c1f0` transition
    // - state11 remains a later real branch after auth-reply handling, not the first active
    //   branch to prioritize
    // - the earlier auth bootstrap/send lead is still the helper2 / `0x448050` family, not the
    //   later world-list sender
    // - state1/state2 should therefore call the separate owner+0x680 child directly instead of
    //   routing through a fake mediator-owned auth-bootstrap method

    // Current source-owned lifecycle mirror for owner `+0x680`:
    // - original `0x41b160` allocates the child during mediator initialize
    // - original `0x41f510` frees it during reset/clear-owned-runtime-state
    // - source therefore allocates it lazily through this helper instead of constructing it in the
    //   C++ object ctor
    AuthBootstrap680Child_0x441290& EnsureAuthBootstrapChild680Scaffold();
    AuthBootstrap680Child_0x441290& AuthBootstrapChild680() { return EnsureAuthBootstrapChild680Scaffold(); }
    const AuthBootstrap680Child_0x441290& AuthBootstrapChild680() const {
        return const_cast<CLTLoginMediator*>(this)->EnsureAuthBootstrapChild680Scaffold();
    }
    void ResetAuthConnectRetryStateScaffold();
    // Current source-owned mirror of owner `+0x4c` auth address-list reinit used by startup
    // helper `0x41b160` after launcher config has already seeded the auth host name.
    void RefreshAuthAddressListForCurrentHostScaffold();
    uint32_t AuthConnectAttemptCountScaffold() const;
    uint32_t AuthConnectCandidateCountScaffold() const;
    bool HasAuthConnectRetryCandidateRemainingScaffold() const;


    // Newer `0x44af20 / 0x442d00 / 0x41f260` tightening now makes the later post-auth receive
    // boundary explicit in source too:
    // - decoded margin codes `2`, `4`, and `5` are consumed by base margin dispatch
    // - code `2`, code `4`, and code `5` now each have one bounded nearer source-owned step at
    //   the connection/leaf seam before any later fallback path
    // - only other codes survive into owner `+0x184` / current helper slot 6
    // - practical consequence for that later path: the first real `MS_LoadCharacterReply` candidate must
    //   arrive as raw code `0x10` *after* that base-dispatch filter
    // anchor: launcher.exe:0x442d00 -> 0x41bc20 / 0x441a30 / 0x4429b0
    // Narrow source-owned mirror of one consumed decoded-code-2 branch moved closer to the
    // connection/leaf dispatch seam.
    uint32_t HandleMarginConsumedCode2AtConnectionSeamScaffold(
        const uint8_t* packetBytes,
        size_t packetSize,
        bool transportEncrypted);
    // anchor: launcher.exe:0x442d00 -> 0x41bc20 / 0x441bc0 / 0x441850
    // Narrow source-owned mirror of one consumed decoded-code-4 branch moved closer to the
    // connection/leaf dispatch seam.
    // Current bounded split:
    // - source now mirrors the local `0x441850` type-`0x0b` work-item re-entry through
    //   connection vtable `+0x10`
    // - the older launcher-owned bootstrap continuation remains only as the fallback that keeps the
    //   current working path alive while later slot-2 resumption is still incomplete
    uint32_t HandleMarginConsumedCode4AtConnectionSeamScaffold(
        const uint8_t* packetBytes,
        size_t packetSize,
        bool transportEncrypted);

    // Narrow staged-packet access kept on the mediator for the concrete CLTLoginState slot-6
    // bodies.
    // Keep the packet/class ownership split explicit:
    // - state10 slot 6 / `0x4401a0` now owns the source-side staged auth-reply helpers,
    //   including the shared owner-state writeback reused by broader state2 slot 5 / `0x43f300`
    // - state11 slot 6 / `0x440320` owns the load-character reply transition directly
    // - mediator only keeps the staged bytes for those later paths
    const std::vector<uint8_t>& StagedIncomingMarginPacketBytes() const;

    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;
    static constexpr uint32_t kReceiveActionNone = 0u;
    static constexpr uint32_t kReceiveActionBeginMarginAfterAuthReply = 1u << 0;

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
    // Keep the wrapper-facing late-entry vector-like object explicit; the ABI shape lives directly
    // on the owner at `+0x1470/+0x1474/+0x1478`, so this getter can stay close to the original
    // tiny `lea eax,[ecx+0x1470]; ret`.
    LateEntryList1470VectorLikeSketch* GetLateEntryList1470() override;
    // anchor: launcher.exe:0x41f5f0 / owner helper clearing owner `+0x1470`
    void ClearLateEntryList1470Scaffold();
    // anchor: launcher.exe:0x41f840 / owner vtable +0x190
    // Keep this wrapper tiny like the original: it forwards owner `+0x1470` into the lower-level
    // string-triple array append helper.
    void AppendLateEntryStringTriple1470Scaffold(const LateEntryList1470EntrySketch* sourceEntry);
    // Wrapper-facing arg6 profile-path/current-slot ABI objects.
    // Keep this split explicit instead of forcing the owner-side `0x004b01c8 +0x40/+0x44`
    // slot-record helpers onto the wrapper-facing `ILTLoginMediator.Default +0x40/+0x44` object
    // shapes.
    // - arg6 `+0x40` = selection-descriptor object family
    // - arg6 `+0x44` = current-slot-record object family
    // - owner `+0x40 = 0x41f2e0` remains the separate `GetSlotRecordByIndex` accessor
    Arg6CurrentSlotRecord44ObjectSketch* GetArg6SelectionDescriptorObject40(
        uint32_t selectionIndex) override;
    Arg6CurrentSlotRecord44ObjectSketch* GetArg6CurrentSlotRecordObject44() override;

    // Post-auth slot/route families recovered around helper10 (`0x4401a0`) and the later
    // state-8 margin dispatcher (`0x439300`).
    // Source home for the focused route/descriptor/getter cluster:
    // - `matrixstaging/game/src/libltclientlogin/loginmediator_margin_route.cpp`
    // anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
    const SlotRecordState_0x4b5328* GetSlotRecordByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f300 / owner vtable +0x44
    const SlotRecordState_0x4b5328* GetCurrentSlotRecord() const;
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

    // Embedded owner `+0x684 .. +0xd7f` selection-route helper/class.
    // Current Ghidra class name is `CLTLoginMediatorSelectionRouteState_0x41dba0`, and the
    // recovered role is tighter now:
    // this is the mediator-owned selection/slot/route state island behind owner `+0x40/+0x44`
    // with current slot byte at `+0xcc8` and the persisted state3(wait)->state8 snapshot body at
    // `+0xcd0..+0xd7f`.
    // Current best read of its three methods:
    // - `0x41dba0` ctor: initializes slot-count / slot-table / route-string entries and sets the
    //   shared current-slot byte to `0xff`
    // - `0x41d270`: releases active slot records, clears active route strings, zeroes count, sets
    //   current-slot byte to `0xff`
    // - `0x41dd00`: calls `0x41d270`, then releases the backing storage for all 100 route-string
    //   entries
    // Source ownership note:
    // - original keeps this as one embedded helper spanning count + `+0x688` slot table + `+0x818`
    //   route strings + current-slot byte `+0xcc8` + snapshot body `+0xcd0..+0xd7f`
    // - replacement now models that ownership island as the nested
    //   `CLTLoginMediatorSelectionRouteState` class and keeps the anchored method boundaries there

    void SetCurrentCharacterRouteIndexCc8Scaffold(uint8_t slotIndex);

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

    // +0xe8
    // anchor: launcher.exe:0x41ec00
    uint32_t RemoveSlotRecordAndCompactRouteStateByIndex(uint32_t selectedSlotRecordIndex) override;

    // +0xf0
    // anchor: launcher.exe:0x41c390
    // Owner-side narrower state3-wait advance: stores selection index and switches to state7.
    uint32_t SetSelectionIndexAndSwitchToState7(uint32_t selectedSlotRecordIndex) override;

    // +0xec
    // anchor: launcher.exe:0x41c1f0
    // Owner-side state3-wait advance: persists the selection/config snapshot and switches to state8.
    uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) override;
    const void* GetState8PersistenceF1c() const override;
    const State3SelectionContextInputSketch& SelectionContext0ecCopy() const { return selectionContext0ecCopy_; }
    void ResetSelectionContext0ecMirror();

    // +0x120
    // anchor: launcher.exe:0x41c3c0
    uint32_t ProcessCreateCharacterInput120(const ProcessCreateCharacterInput120Sketch& input) override;
    // wrapper-facing arg6 `+0x120` entry used by `client.dll:0x62054d1d`
    // Keep the instance-role split explicit in source:
    // - the wrapper-facing `ILTLoginMediator.Default` mirror should capture the source block even
    //   when it is not the live owner/controller instance
    // - the live owner/controller still applies the real `0x41c3c0` state gate and helper-state
    //   transition to `10`
    uint32_t CaptureCreateCharacterInputArg6Slot120(
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
    // - that wrapper forwards envelope `+0x08` (the retained outer message-ref object) into
    //   `+0x28` = inherited `CMessageConnection::SendPacket` / `0x448cf0`
    // - `0x448cf0` consumes that message-ref object, not bare payload bytes
    // Current source helper is therefore intentionally narrower/scaffold-only:
    // - local builder front matter now stays raw-first (`+0x04` payload base, `+0x08` message-ref)
    // - `0x448cf0` consumption now happens on that retained message-ref object rather than a
    //   shared_ptr-owned convenience shell
    // - helper-side packet-agenda replacement/discard and the larger builder-local metadata tails
    //   beyond that front matter still remain incomplete
    uint32_t SendCurrentMarginPacketScaffold(
        mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& envelope);
    uint32_t SendCurrentMarginPacketScaffold(const void* packetBytes, uint32_t packetByteCount);

    // anchor: launcher.exe:0x41e500
    // Narrow reusable transport/init helper kept on the mediator after moving the `0x439300`
    // case split back into `CLTLoginState_State4::Slot3_BeginOrContinue`.
    // Preserved call contract from the original body:
    // - arg1 = route/prefix text used to refresh owner `+0x30`
    // - arg2 = cached non-zero selector that skips the route-refresh / address-list rebuild path
    uint32_t BeginMarginConnectionScaffold(const char* routeHostText, uint8_t cachedRouteSelector);

    // Post-Auth Margin/Loading State Accessors (launcher.exe:0x4f78b8)
    // =============================================================================
    // These methods expose the owner fields used by the later post-auth margin/loading path.
    // They are used to faithfully reconstruct the original launcher's margin packet building
    // and load character reply handling logic.
    // =============================================================================

    // Source-owned bridge over original owner byte `+0xcc8`.
    // Fidelity tightening from `0x41f300`:
    // - owner vtable `+0x44` directly reads the embedded selection-route helper byte
    // - current source now treats `selectionRouteState684_.currentSlotOrSelectionIndex644_`
    //   as the canonical mirror of that owner byte and keeps the later post-auth fields synced
    //   through `SetCurrentCharacterRouteIndexCc8Scaffold`
    uint8_t CurrentCharacterRouteIndexCc8Scaffold() const;
    const std::array<uint32_t, 4>& SelectionContextBlockCd0() const { return selectionRouteState684_.persistedSelectionContext64c_.blockCd0; }
    const std::array<uint32_t, 4>& SelectionContextBlockCe0() const { return selectionRouteState684_.persistedSelectionContext64c_.blockCe0; }
    const std::array<uint32_t, 4>& SelectionContextBlockCf0() const { return selectionRouteState684_.persistedSelectionContext64c_.blockCf0; }
    const std::array<uint32_t, 4>& SelectionContextBlockD00() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD00; }
    const std::array<uint32_t, 4>& SelectionContextBlockD10() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD10; }
    const std::array<uint32_t, 4>& SelectionContextBlockD20() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD20; }
    const std::array<uint32_t, 4>& SelectionContextBlockD30() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD30; }
    const std::array<uint32_t, 4>& SelectionContextBlockD40() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD40; }
    const std::array<uint32_t, 4>& SelectionContextBlockD50() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD50; }
    const std::array<uint32_t, 4>& SelectionContextBlockD60() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD60; }
    const std::array<uint32_t, 4>& SelectionContextBlockD70() const { return selectionRouteState684_.persistedSelectionContext64c_.blockD70; }

    // State-owned slot-6 bodies mutate this mediator-owned owner-state block directly.

    // Post-auth load-character reply outputs (0x440320) plus neighboring auth/margin send flags:
    // Caution: original `0x4390b0` writes owner `+0x2c` on its non-zero payload branch, while the
    // current replacement also reuses this byte as a narrow live auth-ready alias on the active
    // happy path. Keep the storage stable, but do not overstate the exact original semantic yet.

    bool MarginConnectionCloseWaitEvent0fGateArmedScaffold() const { return marginConnectionFlag2d_ != 0u; }
    void SetMarginConnectionCloseWaitEvent0fGateArmedScaffold(bool armed) {
        marginConnectionFlag2d_ = armed ? 1u : 0u;
    }
    uint32_t State6UdpSessionSecretF18() const;
    void SetState6UdpSessionSecretF18(uint32_t value);
    // anchor: launcher.exe:0x41f370 / owner vtable +0x50
    // Wrapper-facing helper related to the owner `+0x680` auth/bootstrap child.
    // Newer `0x448140` tightening now keeps the reply-derived `+0xf4` copy as wire-shaped bytes,
    // so current source leaves this accessor on the original child `+0xa8` worker slot instead of
    // inventing a pointer semantic inside the copied `0x136` blob.
    void* BootstrapRaw08AuxHandle50() const override;

    // anchor: launcher.exe:0x41b500 -> 0x4435f0 / 0x441f30
    uint32_t PrepareState5MarginConnectionCopySendScaffold();
    const void* AuthBootstrapReplyCopyShadowF4Scaffold() const;
    // anchor: launcher.exe:0x004433c0 / 0x0044add0
    // Returns whether the owner `+0x680 +0xf4` auth-reply-derived `0x136` copy block is present
    // and still fresh enough for the state5 copy/send path.
    bool HasValidState5ReplyCopyShadowF4Scaffold() const;

private:
    void RecoverAuthReplyPrivateExponentIntoMarginBootstrapState(const mxo::auth::AuthReply& reply);
    void SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world);
    void SeedRecoveredCharacterSlotRecordFromAuthReply(uint8_t characterIndex, const mxo::auth::AuthCharacterEntry& character);
    int FindRecoveredWorldDescriptorIndexByWorldId(uint16_t worldId) const;
    void SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();
    void MirrorCreateCharacterInput120SourceBlock(const ProcessCreateCharacterInput120Sketch& input);
    void PersistCharactersIniFromRecoveredAuthStateScaffold() const;

    void InitializeObserverTree674();
    void ClearObserverTree674();
    LoginObserverTreeNode674* ObserverTreeBegin674() const;
    LoginObserverTreeNode674* ObserverTreeEnd674() const;
    LoginObserverTreeNode674* FindObserverNode674(void* observer) const;
    void EqualRangeObserver674(
        void* observer,
        LoginObserverTreeNode674** outLowerBound,
        LoginObserverTreeNode674** outUpperBound) const;
    bool InsertObserverNode674(void* observer);
    void EraseObserverRange674(
        LoginObserverTreeNode674* first,
        LoginObserverTreeNode674* last);

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
    void FreeLateEntryList1470StorageScaffold();

    void BuildAuthEndpoint();
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
    //   - auth leaf fallback `0x41af80` compares its incoming connection against `+0x18`, clears
    //     `+0x18` on work type `1`, then re-enters current helper slot 1 / raw vtable `+0x00`
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
    uint32_t marginPacketReceiveCountScaffold_ = 0;
    uint32_t marginPacketFilteredBeforeSlot6CountScaffold_ = 0;
    uint32_t marginPacketSlot6DispatchCountScaffold_ = 0;
    uint16_t lastMarginPacketOpcodeScaffold_ = 0;
    uint32_t lastMarginPacketSizeScaffold_ = 0;
    mxo::liblttcp::CMessageConnection* authConnection_;
    mxo::liblttcp::CMessageConnection* marginConnection_;
    bool authConnectionOwnedByMediator_ = false;
    bool marginConnectionOwnedByMediator_ = false;
    bool authPeerCloseQueuedScaffold_ = false;
    bool marginPeerCloseQueuedScaffold_ = false;

public:
    // owner `+0x80` - world list count or auth/status (used by state4 slot2)
    uint32_t worldListCountOrStatus80 = 0;

    MarginRouteState marginRouteState_;
    // Original non-virtual `CLTIPAddressList` helper rooted at owner `+0x3c`.
    // Recovered original in-object layout:
    // - `+0x3c` = begin pointer
    // - `+0x40` = end pointer
    // - `+0x44` = capacity pointer
    // - `+0x48` = current iterator pointer
    // Companion source-owned state kept adjacent to the object only where launcher RE proves it is
    // outside the helper itself:
    // - current resolved host string used for source-side route-change detection
    mxo::liblttcp::CLTIPAddressList marginAddressList3c_{};
    std::string marginAddressListResolvedHostName3c_;
    // owner `+0x24/+0x2c/+0x2d/+0x7c` connection-routing/teardown family:
    // - `+0x24` = margin begin-count from `0x41e500`
    // - `+0x2c` = shared slot1 / event-`1` gate armed by vtable `+0x164 / 0x41b3f0`
    // - `+0x2d` = shared slot2 / event-`0x0f` gate armed by vtable `+0x16c / 0x41b420`
    // - `+0x7c` = selected IPv4 for margin endpoint materialization
    uint32_t marginBeginCount24_ = 0;
    uint8_t authConnectionFlag2c_ = 0;
    uint8_t marginConnectionFlag2d_ = 0;
    uint32_t marginSelectedIpv4_7c_ = 0;
    // Separate phase-2 auth/bootstrap child rooted at owner `+0x680`.
    // Keep this as a distinct child/module mirror rather than folding its fields back into the
    // mediator body comments or generic mediator helpers.
    // Current tightened high-value family inside that child:
    // - `+0xa0` = auth-request-ready byte set by successful raw `0x07` handling and tested by
    //   `0x448050`
    // - `+0xa4` = lazy `qspubkey.dat` validator-family pointer primed by `0x447260/0x447c10`
    // - `+0xa8` = raw `0x08` reply-public-key worker rebuilt by `0x447780` and consumed by
    //   `0x4474f0` through `0x468ea0/0x468f00`
    // - `+0xac` = sibling reply validator rebuilt by `0x447780` and consumed by
    //   `0x44aec0 = AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`
    // - `+0xf4` = original reply-derived copied `0x136` block; source now keeps that block as a
    //   wire-faithful shadow so later wrapper/state5 users can read the original `+0x85/+0xa8`
    //   suffix family without re-inventing a semantic object model
    // Replacement source now also keeps the ownership boundary explicit by storing this as a
    // separate child object instead of flattening it into the mediator body.
    std::unique_ptr<AuthBootstrap680Child_0x441290> authBootstrapChild680_;
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
    // - `+0x118` = owner `+0x1470` vector-like late-entry list of 12-byte string-triple entries
    Arg6CurrentSlotRecord44ObjectSketch arg6SelectionDescriptor40_{};
    Arg6CurrentSlotRecord44ObjectSketch arg6CurrentSlotRecord44_{};
    std::string arg6CurrentSlotRecord44NameOwned_;
    RouteDescriptor30SmallStringLikeSketch state8Section11String1460_{};
    std::string routeDescriptor30Owned_;
    RouteDescriptor30SmallStringLikeSketch routeDescriptor30_{};
    // owner `+0x1470` / arg6 `+0x118` late-entry family:
    // - owner `+0x1470/+0x1474/+0x1478` is the real vector header returned by `0x41af50`
    // - entries are 12-byte owned string-triples copied by `0x41f640`
    // - growth / destruction helpers around this header now mirror the original array semantics
    //   directly instead of routing through STL containers
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
    const void* arg6CreateCharacterInput120_ = nullptr; // wrapper-facing `+0x120` last raw input pointer
    uint32_t arg6CreateCharacterInputCount120_ = 0u;     // wrapper-facing `+0x120` call count
    uint32_t ownerOptionalField90_ = 0;                  // owner `+0x90`, only forwarded when helper byte `+4 != 0`
    // launcher.exe:0x41f0a0 / owner vtable +0x38: returns pointer to this block at +0x94
    // anchor: launcher.exe:0x41eb80 copies from submit input into this block
    // Layout: +0x00=username[32], +0x20=password[32], +0x40=keyConfigMd5[16], +0x50=uiConfigMd5[16],
    //         +0x60=sessionTokenString, +0x6c=flag6c. Total 112 bytes (0x70).
    // Note: there's also a separate session token string at +0xf4 (+0x94 + 0x60) cleared by +0x30 path.
    OwnerAuthBootstrapSource94 ownerAuthBootstrapSource94_{};
    int32_t ownerCachedHandle147c_ = -1;       // owner `+0x147c`, managed-submit handle cached across `+0x1c` release / `+0x18` reacquire
    // launcher.exe owner `+0x684 .. +0xd7f` embedded selection-route helper/class
    // (`CLTLoginMediatorSelectionRouteState_0x41dba0` in current Ghidra).
    CLTLoginMediatorSelectionRouteState selectionRouteState684_{};
    // +0xec / +0xf4 wrapper-owned mirrors now live on the mediator instance instead of in the
    // launcher ABI shell.
    State3SelectionContextInputSketch selectionContext0ecCopy_{};
    bool selectionContext0ecCopyValid_ = false;
    uint32_t selection0ecCount_ = 0;
    mutable uint32_t profile0f4Count_ = 0;
    // launcher.exe:0x4f78b8 owner-side post-auth margin/loading area used by state8/state10/state11.
    PostAuthMarginLoadingState postAuthMarginLoadingState_;
    // launcher.exe:0x4f78b8 owner-side world-descriptor table (`+0xd84`).
    std::array<WorldDescriptorState004b533c, kRecoveredWorldSlotCapacity> worldDescriptorsD84_;
    std::array<bool, kRecoveredWorldSlotCapacity> worldDescriptorValidD84_{};
    uint8_t worldDescriptorCountD80_ = 0;

    // Observer/listener state for late-login arg6 +0x170/+0x174 bridge:
    // - +0x170 (RegisterLoginObserver) inserts into owner `+0x674`
    // - +0x174 (UnregisterLoginObserver) removes from owner `+0x674`
    // Keep the container shape explicit instead of flattening it into a vector because the
    // original `0x41cfb0 / 0x41d090` traversals are std::_Tree-like in-order walks over this data.
    LoginObserverTree674 observerTree674_{};
    LoginObserverTreeNode674 observerTreeHeader674_{};
    void* latestObserver170_ = nullptr;         // most recent register call observer
    void* latestObserver174_ = nullptr;         // most recent unregister call observer

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
    // Replacement-only compatibility auth payload copy:
    // - auth leaf/base dispatch now keeps `0x449a30` thin again
    // - when current source still needs raw auth payload bytes, state2 / owner+0x680 child copies
    //   the logical payload span there after resolving the incoming auth-message object
    // - later raw-`0x0b` selected-slot handling and state9 callback84 opcode fallback still
    //   consult this storage
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
    bool postAuthMarginAutoBeginAttemptedScaffold_ = false;
    const char* expectedAuthRequestName_;
    const char* expectedMarginRequestName_;

    std::array<void*, kRecoveredWorldSlotCapacity> worldSlots_;
    std::array<void*, kRecoveredWorldSlotCapacity> worldPayloadSlots_;

    // Original non-virtual `CLTIPAddressList` helper rooted at owner `+0x4c`.
    // Recovered original in-object layout:
    // - `+0x4c` = begin pointer
    // - `+0x50` = end pointer
    // - `+0x54` = capacity pointer
    // - `+0x58` = current iterator pointer
    // Nearby owner-side state that is not part of the helper object itself remains explicit:
    // - owner `+0x28` = auth connect attempt count / retry gate state
    // - source-side resolved-host cache is kept separately for rebuild-change detection
    mxo::liblttcp::CLTIPAddressList authAddressList4c_{};
    std::string authAddressListResolvedHostName4c_;
    uint32_t authConnectAttemptCount28_ = 0;

    // Current active replacement path no longer uses a synthetic startup world-list sidecar.
    // The launcher selection menu in `src/textmode_launcher_flow.cpp` now runs after auth success
    // and consumes the recovered owner tables (`+0xd84/+0x688/+0x818`) directly.
    // Keep only the narrower configured arg6 selection scratch that is still used by the
    // wrapper-facing descriptor/profile bridges.
    struct Arg6SelectionConfig {
        uint32_t worldUpperBoundExclusive_ = 1;
        uint32_t variantUpperBoundExclusive_ = 1;
        uint32_t selectedWorldIndexLow24_ = 0;
        // Current tighter page-`7` read: this high byte is the selected row's high-word active
        // entry index. On the auth-valid launcher selection path that is currently better modeled
        // as the slot-record / character-entry index rather than a free-standing world variant id.
        uint32_t selectedVariantIndexHigh8_ = 0;
        uint32_t selectedVariantState_ = 0;
        uint32_t mappedSelectionId_ = 0;
        std::string mappedSelectionName_ = "standalone";
        std::string mappedVariantName_ = "standalone";
        std::string profileName_ = "resurrections";
        std::string authName_ = "resurrections";
        std::string authPassword_;
    };

    Arg6SelectionConfig arg6Selection_;
    uint32_t arg6VariantWorldNameQueryCountE0_ = 0u; // wrapper-facing arg6 `+0xe0` query count
};

// anchor: launcher.exe:0x43b300 / 0x41b450 / 0x4f7868..0x4f78b4
// Source-owned mirror of the contiguous global login-helper dispatch table.
extern CLTLoginMediator::ConnectionHelperFamily g_LoginHelperDispatchTableScaffold;

// anchor: launcher.exe:0x4f78b8
// Source-owned mirror of the launcher global current mediator pointer consumed by the helper family.
extern CLTLoginMediator* g_CurrentLoginMediator;

}  // namespace mxo::ltlogin

}  // namespace mxo
