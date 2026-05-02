#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"
#include "../../../runtime/src/libltmessaging/messageconnection.h"
#include "../../../runtime/src/liblttcp/ltipaddresslist.h"
#include "../../../runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include "loginmediator_base.h"
#include "authbootstrap680.h"

// Forward declaration - full class is defined in loginmediator_events.cpp
class LoginObserverTreeHelper674;

namespace mxo {
namespace ltlogin {

class CLTLoginState;
class CLTLoginState_State10_0x4b512c;
class CLTLoginState_AuthenticatePending_0x4b5014;
class CLTLoginState_WorldListPending_0x4b4fec;
class CLTLoginMediator;
class CLTLoginMediatorCharacterPersistenceData_0x41d900;

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
// - docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_0x4af2b8_Default.md
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
//   while `ILTLoginMediator_0x4af2b8.Default` remains the runtime interface slot passed into client.dll
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

// Source implementation of the original shared owner-side class now named in Ghidra as
// `CLTLoginMediatorCharacterPersistenceData_0x41d900`.
// This is the canonical owner object behind the late getter family rooted at:
// - `+0xbc -> +0xf48`
// - `+0xc0 -> +0xf88`
// - `+0xc4 -> +0x13f0/+0x13f4`
// - `+0xf4 -> +0xf1c`
#pragma pack(push, 1)
class CLTLoginMediatorCharacterPersistenceData_0x41d900 {
public:
    static constexpr size_t kBodySize = 0x465;

    CLTLoginMediatorCharacterPersistenceData_0x41d900();

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

static_assert(sizeof(CLTLoginMediatorCharacterPersistenceData_0x41d900) == 0x550);
#pragma pack(pop)

class CLTLoginMediator : public ILTLoginMediator_0x4af2b8 {
    // Source-ownership split note:
    // - the phase-2 auth/bootstrap child rooted at owner `+0x680` now has its own focused source
    //   home in `authbootstrap680.cpp`
    // - state2 `0x43f300` now also needs narrow access because early inbound auth staging/demux
    //   is source-owned there instead of in the mediator wrapper
    // - keep access narrow by granting focused friendship instead of widening the mediator
    //   surface generically
    friend struct AuthBootstrap680Child_0x441290;
    friend class CLTLoginState_AuthenticatePending_0x4b5014;
    friend class CLTLoginState_State4_0x4b503c;
    friend class CLTLoginState_State6_0x4b508c;
    friend class CLTLoginState_State7_0x4b50b4;
    friend class CLTLoginState_State8_0x4b5104;
    friend class CLTLoginState_State10_0x4b512c;
    friend class CLTLoginState_State11_0x4b5154;
    friend class mxo::liblttcp::CAuthStartupConnection_0x4afef0;
    friend class mxo::liblttcp::CBaseMarginConnection_0x4b64a8;
    friend class mxo::liblttcp::CMarginConnection_0x4aff38;

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
        //   - current Ghidra name: `CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader_GetWorkType`
        //   - older Ghidra label: `LaunchPadClient_GetVtableOffset`
        // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
        // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
        // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
        //
        // Disassembly of 0x438d80 shows:
        //   - Calls reused inline helper
        //     `CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader_GetWorkType(this+8)`
        //   - Checks if event flag at [this+0x2c] is set
        //   - If event flag set, calls CLTLoginMediator_PostEvent(this, 1)
        //   - Otherwise calls vtable[+0x178]() and updates state at [this+0x80]
        //
        // Current highest-value slot anchors:
        // - slot 1 / `0x4f786c` / phase-code `1`
        //   - current best concrete state object: `CLTLoginState_State1_0x4b4fc4` / vtable `0x4b4fc4`
        //   - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection starts auth connect through launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection
        // - slot 2 / `0x4f7870` / phase-code `2`
        //   - current best concrete state object: `CLTLoginState_AuthenticatePending_0x4b5014` / vtable `0x4b5014`
        //   - launcher.exe:0x439210 is the strongest current earlier loginstate-owned handoff into
        //     the owner `+0x680` phase-2 auth/bootstrap child
        //   - on the connected branch it reaches launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch, which then branches to:
        //     - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (builds/sends raw auth code 0x06)
        //       -> strongest current `AS_GetPublicKeyRequest` candidate
        //     - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (builds/sends raw auth code 0x08)
        //       -> strongest current `AS_AuthRequest` candidate
        // - slot 10 / `0x4f7890` / phase-code `10`
        //   - current best concrete state object: `CLTLoginState_State10_0x4b512c` / vtable `0x4b512c`
        //   - launcher.exe:0x4401a0 = later state-10 incoming `AS_AuthReply` handler
        // - slot 14 / `0x4f78a0` / phase-code `14`
        //   - current best concrete state object: `CLTLoginState_WorldListPending_0x4b4fec` / vtable `0x4b4fec`
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
        void* helper78A4 = nullptr;  // slot 15 / phase-code 15 / CLTLoginState_State15_0x4b0b88
        void* helper78A8 = nullptr;  // slot 16 / phase-code 16 / CLTLoginState_State16_0x4b0bb0
        void* helper78AC = nullptr;  // slot 17 / phase-code 17 / CLTLoginState_State17_0x4b0bd8
        void* helper78B0 = nullptr;  // slot 18 / phase-code 18 / CLTLoginState_State18_0x4b0c00
        void* helper78B4 = nullptr;  // slot 19 / phase-code 19 / CLTLoginState_State19_0x4b0c28
    };

    // Fidelity note: the old source-only `PostAuthMarginLoadingState` wrapper has been retired.
    // Static-RE now shows this region is just ordinary `CLTLoginMediator` storage, with the
    // canonical state8 persistence object living directly at owner `+0xf1c`.

    class CLTLoginMediatorSelectionRouteState_0x41dba0 {
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
        CLTLoginMediatorSelectionRouteState_0x41dba0();

        // anchor: launcher.exe:0x41d270 / embedded owner subobject reset
        void ResetSelectionRouteState();

        // anchor: launcher.exe:0x41dd00 / embedded owner subobject destroy/final release
        void DestroySelectionRouteState();

        // Source-owned mirrors of the original embedded helper layout:
        // - `+0x00`  = active slot-record count
        // - `+0x04`  = slot-record pointer table (mirrored here as value objects plus validity bits)
        // - `+0x194` = route-host `std::string` array, matching the recovered old-MSVC2003
        //              `std::basic_string<char>` family at `0x403f90`
        // - `+0x644` = current slot / selection byte
        // - `+0x64c .. +0x6fb` = persisted state3(wait)->state8 snapshot body
        uint8_t slotRecordCount00_ = 0;
        std::array<Packet_AsAuthReply_0x4b5328, kRecoveredWorldSlotCapacity> slotRecordTable04_{};
        std::array<bool, kRecoveredWorldSlotCapacity> slotRecordValid04_{};
        std::array<std::string, kRecoveredWorldSlotCapacity> routeHostStrings194_{};
        uint8_t currentSlotOrSelectionIndex644_ = 0xffu;
        PersistedSelectionContext64c persistedSelectionContext64c_{};
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
    uint8_t unknownByte05_ = 0;                // original owner `+0x05` (GetUnknownByte05)
    mutable bool liveCuiCfgAbsentNoteLogged90_ = false;     // +0x90 one-shot caveat log moved from ABI wrapper
    // Tiny owner getter reached from state2 `0x439210` through owner vtable `+0x20`.
    // Exact original body returns `owner + 0x08`; callers then dereference that dword.
    const uint32_t* GetNoPatchLauncherVersionValuePtr08() const;

    // anchor: launcher.exe:0x41f050 = vtable +0x18
    // Returns owner `+0x05` (mbr_0x5). Default 0, set to 1 by SetUnknownByte05().
    // This byte is written to MS_ConnectRequest offset 0x22.
    uint8_t GetUnknownByte05() const;

    // source-owned narrow accessor for owner `+0x94 + 0x60` small-string begin pointer.
    // Current concrete state7 use from `0x43ba20`:
    // - owner vtable `+0x38` returns `this+0x94`
    // - state7 slot3 then reads the first dword at `+0x60` and threads it into the raw-`0x0d`
    //   delete-character packet's optional string field via `0x43aa80`


    // anchor: launcher.exe:0x41f090
    // Tiny owner getter paired with `0x41f080`; exact original body returns `owner + 0x0c`.
    const uint32_t* GetNoPatchClientVersionValuePtr0c() const;

    // +0x00
    const char* GetName() override;
    // +0x08
    // anchor: launcher.exe:0x41b160
    // Return: 0x12000001 if auth address list is empty, 0 if it has entries
    uint32_t Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* networkEngineOverride) override;
    // UNANCHORED helper kept explicit from the real wrapper/owner vtable rows:
    // - wrapper `ILTLoginMediator_0x4af2b8.Default +0x08` currently forwards into `Initialize(...)`
    // - owner `CLTLoginMediator +0x0c` is `0x41f510 = ResetOwnedRuntimeState`, not a direct engine setter
    void SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine);
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
    // anchor: launcher.exe:0x41f350 / exact tiny body returns child `+0x108`
    // Current wrapper-facing names remain provisional; the original body is only a direct child-field read.
    const char* GetWorldOrSelectionName() const override;
    // +0x4c
    // anchor: launcher.exe:0x41f360 / exact tiny body returns child `+0x10c`
    const char* GetProfileOrSessionName() const override;
    // +0x54
    bool HasBootstrapRaw08AuxHandle54() const override;
    // +0x58
    // anchor: launcher.exe:0x41f390 / exact tiny body returns child byte `+0x104` in AL
    uint8_t GetCrashReporterPromptForSecurId58() const override;
    // +0x5c
    // anchor: launcher.exe:0x41f3a0 / tiny getter returns child `+0xf4+0x85` or pooled empty string
    const char* GetCrashReporterUsername5c(const void* chainedValueToken) override;
    // +0x60
    // anchor: launcher.exe:0x41f3c0 / exact tiny body returns child `+0xf8` begin pointer
    const char* GetCrashReporterPassword60(const void* chainedValueToken) override;
    // +0x64
    // anchor: launcher.exe:0x41f2b0 / exact tiny body returns owner+0x680+0x110
    uint32_t GetBootstrapSuccessHeaderDword64() const override;
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
    std::string_view GetState8Section11String1460() const override;
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
    // anchor: launcher.exe:0x41b2a0
    uint8_t GetSlotRecordStatusBySelectionIndex(int32_t selectionIndex) const override;
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
    uint32_t DispatchCurrentHelperSlot6(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;
    // anchor: launcher.exe:0x41afc0 / owner vtable `+0x188`
    // Thin owner-body mirror: compare against owner `+0x1c`, clear the live margin-connection
    // field on type-1 close, then jump straight back through current helper slot 2.
    // launcher.exe also clears owner `+0x20` here; source still lacks a proven field home for
    // that word, so keep that discrepancy explicit instead of inventing one.
    uint32_t HandleMarginConnectionCompletionFallback(void* connection, void* workItem) override;

    // Anchored launcher.exe logging/event side effects at `0x41cfb0` / `0x41d090`.
    // Current implementation keeps lightweight event/error history together with the recovered
    // observer registration bridge for arg6/`ILTLoginMediator_0x4af2b8.Default` slots `+0x170/+0x174`.
    void PostEvent(uint32_t eventId);
    void PostError(uint32_t errorId);
    bool RegisterLoginObserver(void* observer) override;
    // +0x174
    bool UnregisterLoginObserver(void* observer) override;
    // Observer count getters for wrapper diagnostics (moved from g_MediatorRuntimeState):


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
    // anchor: launcher.exe:0x41b450
    uint32_t SetCurrentState(uint32_t helperStateId);
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
    //   `ILTLoginMediator_0x4af2b8.Default +0x18c`
    // Wrapper-facing `+0x124` capture remains separate from the owner-side triple mirror.
    void ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) override;
    void SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* startupDistrObjExecutive8c);
    // anchor: launcher.exe:0x41f210 / mediator vtable +0x12c
    // Current best type recovery: returns the owner `+0x8c` collaborator copied from the client
    // startup triple's third argument, matching the client-side `g_StartupDistrObjExecutive124`.
    void* GetStartupDistrObjExecutive8c() const;
    // anchor: launcher.exe:0x41e690 / mediator vtable +0x18c
    uint32_t FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) override;

    // Recovered config anchors:
    // - launcher `qsAuthServerDNSName` / `AuthServerPort`
    // - launcher `MarginServerDNSSuffix` / `MarginServerPort`
    // The replacement launcher should eventually populate these from the same launcher-owned
    // config path instead of treating connection setup as generic ad-hoc socket work.

    bool AuthPeerCloseQueuedScaffold() const { return authPeerCloseQueuedScaffold_; }
    bool MarginPeerCloseQueuedScaffold() const { return marginPeerCloseQueuedScaffold_; }
    void SetAuthPeerCloseQueuedScaffold(bool value) { authPeerCloseQueuedScaffold_ = value; }
    void SetMarginPeerCloseQueuedScaffold(bool value) { marginPeerCloseQueuedScaffold_ = value; }

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
    //   Original: allocates 8 bytes, stores vtable 0x4b0b88 (`CLTLoginState_State15_0x4b0b88`)
    // launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16 (slot at 0x4f78a8)
    //   Original: allocates 4 bytes, stores vtable 0x4b0bb0 (`CLTLoginState_State16_0x4b0bb0`)
    // launcher.exe:0x420850 = InitializeHelperDispatchSlot17 (slot at 0x4f78ac)
    //   Original: allocates 4 bytes, stores vtable 0x4b0bd8 (`CLTLoginState_State17`)
    // launcher.exe:0x420920 = InitializeHelperDispatchSlot18 (slot at 0x4f78b0)
    //   Original: allocates 8 bytes, stores vtable 0x4b0c00 (`CLTLoginState_State18_0x4b0c00`)
    // launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19 (slot at 0x4f78b4)
    //   Original: allocates 4 bytes, stores vtable 0x4b0c28 (`CLTLoginState_State19_0x4b0c28`)
    // Slot anchors from Ghidra decompilation:
    // launcher.exe:0x4f7868 = slot 0, launcher.exe:0x4f78a0 = slot 1, launcher.exe:0x4f786c = slot 2,
    // launcher.exe:0x4f7870 = slot 3, launcher.exe:0x4f7874 = slot 4, launcher.exe:0x4f7878 = slot 5,
    // launcher.exe:0x4f787c = slot 6, launcher.exe:0x4f7880 = slot 7, launcher.exe:0x4f7884 = slot 8,
    // launcher.exe:0x4f7888 = slot 9, launcher.exe:0x4f7890 = slot 10, launcher.exe:0x4f7894 = slot 11,
    // launcher.exe:0x4f788c = slot 12, launcher.exe:0x4f7898 = slot 13, launcher.exe:0x4f789c = slot 14,
    // launcher.exe:0x4f78a4 = slot 15 (`CLTLoginState_State15_0x4b0b88`, vtable `0x4b0b88`)
    // launcher.exe:0x4f78a8 = slot 16 (`CLTLoginState_State16_0x4b0bb0`, vtable `0x4b0bb0`)
    // launcher.exe:0x4f78ac = slot 17 (`CLTLoginState_State17_0x4b0bd8`, vtable `0x4b0bd8`)
    // launcher.exe:0x4f78b0 = slot 18 (`CLTLoginState_State18_0x4b0c00`, vtable `0x4b0c00`)
    // launcher.exe:0x4f78b4 = slot 19 (`CLTLoginState_State19_0x4b0c28`, vtable `0x4b0c28`)
    // ==============================================================================

    // launcher.exe:0x4d3584 = ILTLoginMediator_0x4af2b8_SiblingObject (world list data provider)
    // Active replacement note:
    // - the current no-GUI launcher path no longer keeps a synthetic pre-auth world-list sidecar
    // - selection now runs after auth success and consumes the recovered owner tables directly

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
    // - constructs auth-side CMessageConnection_0x4b7928 child at owner `+0x18`
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
    // - let `CLTThreadPerClientTCPEngine_0x4b2768` own the current queue push / no-worker pump seam
    // - keep the launcher-object ABI shell thin and arg5-shaped
    void ResetLauncherConnectionsScaffold();
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

    // Newer `0x44af20 / 0x442d00 / 0x41f260` tightening now makes the later post-auth receive
    // boundary explicit in source too:
    // - decoded margin codes `2`, `4`, and `5` are consumed by base margin dispatch
    // - code `2`, code `4`, and code `5` now each have one bounded nearer source-owned step at
    //   the connection/leaf seam before any later fallback path
    // - only other codes survive into owner `+0x184` / current helper slot 6
    // - practical consequence for that later path: the first real `MS_LoadCharacterReply` candidate must
    //   arrive as raw code `0x10` *after* that base-dispatch filter



    static constexpr uint32_t kConnectStatusSuccess = 0x7000001u;
    static constexpr uint32_t kReceiveActionNone = 0u;
    static constexpr uint32_t kReceiveActionBeginMarginAfterAuthReply = 1u << 0;

    // launcher.exe owner-vtable surfaces currently recovered from the state4 margin dispatcher.
    // Keep the recovered owner slots explicit instead of adding state4-specific bridge helpers.
    // anchor: launcher.exe:0x41f2c0 / ILTLoginMediator_0x4af2b8.Default slot +0x10c
    // Keep the ABI-compatibility small-string object explicit instead of collapsing it into the
    // owner-side route-text helper family.
    std::string_view GetRouteDescriptor30() const override;
    // anchor: launcher.exe:0x41af50 / ILTLoginMediator_0x4af2b8.Default slot +0x118
    // Keep the wrapper-facing late-entry vector-like object explicit; the ABI shape lives directly
    // on the owner at `+0x1470/+0x1474/+0x1478`, so this getter can stay close to the original
    // tiny `lea eax,[ecx+0x1470]; ret`.
    const std::vector<std::string>& GetLateEntryList1470() const override;
    // anchor: launcher.exe:0x41f5f0 / owner helper clearing owner `+0x1470`
    void ClearLateEntryList1470Scaffold();
    // anchor: launcher.exe:0x41f840 / owner vtable +0x190
    // Keep this wrapper tiny like the original: it appends one recovered late-entry string.
    void AppendLateEntryStringTriple1470Scaffold(std::string_view sourceEntry);
    // Wrapper-facing selection profile-path/current-slot ABI objects.
    // Keep this split explicit instead of forcing the owner-side `0x004b01c8 +0x40/+0x44`
    // slot-record helpers onto the wrapper-facing `ILTLoginMediator_0x4af2b8.Default +0x40/+0x44` object
    // shapes.
    // - selection `+0x40` = selection-descriptor object family
    // - selection `+0x44` = current-slot-record object family
    // - owner `+0x40 = 0x41f2e0` remains the separate `GetSlotRecordByIndex` accessor
    Packet_AsAuthReply_0x4b5328* GetAuthReplyPacketByIndex40(
        uint32_t selectionIndex) override;
    Packet_AsAuthReply_0x4b5328* GetCurrentAuthReplyPacket44() override;

    // Post-auth slot/route families recovered around helper10 (`0x4401a0`) and the later
    // state-8 margin dispatcher (`0x439300`).
    // Source home for the focused route/descriptor/getter cluster:
    // - `matrixstaging/game/src/libltclientlogin/loginmediator_margin_route.cpp`
    // anchor: launcher.exe:0x41b220
    // Source-owned helper over the recovered slot-record table; do not treat this as the current
    // `ILTLoginMediator_0x4af2b8` vtable slot `+0xdc` name.
    const char* LookupSlotRecordHeapStringByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41f320 / owner vtable +0x148
    const char* GetGameSessionId() const override;
    // Removed: SetGameSessionId664 - now using direct public access to gameSessionId664_
    // anchor: launcher.exe:0x41f270 / owner vtable +0x150
    void SetLaunchPadSourceBlock94FirstString(const char* value);
    // anchor: launcher.exe:0x41f330 / owner vtable +0x14c
    void SetSharedMarginPacketField660(uint32_t value);
    // anchor: launcher.exe:0x420d00 / owner vtable +0x134
    LaunchPadClient_0x4b0e48* EnsureLaunchPadClient65c();
    // anchor: launcher.exe:0x420e70
    void CommitSessionCallbackHelperGameSessionId664();
    // source-owned shared helper used by `CLTLoginState_State18_0x4b0c00` slot 3 / `0x421a50`
    void RefreshSessionHelperGameSessionId664FromSourceBlock94();
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    LaunchPadClient_0x4b0e48* GetLaunchPadClient65c() const override;
    // anchor: launcher.exe:0x4202c0 / owner vtable +0x13c
    void HelperSlot13c_InvokeSessionHelperVtable4() override;
    // anchor: launcher.exe:0x41b260
    // Source-owned helper over the recovered route-host string table; do not treat this as the
    // current `ILTLoginMediator_0x4af2b8` vtable slot `+0xe0` name.
    const char* LookupRouteHostPrefixBySlot(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2a0 / owner vtable +0xe4? / current slot-record payload reader
    uint8_t GetSlotRecordStatusByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b2e0 / owner helper over `mbr_0xd84[index]` packet payload +0x03
    const char* GetWorldListNameByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b320 / owner helper over `mbr_0xd84[index]` packet payload +0x17
    uint8_t GetWorldListStatusByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b360 / owner helper over `mbr_0xd84[index]` packet payload +0x18
    uint8_t GetWorldListTypeByIndex(uint8_t slotIndex) const;
    // anchor: launcher.exe:0x41b3a0 / owner helper over `mbr_0xd84[index]` packet payload +0x1f low nibble
    uint8_t GetWorldListPopulationByIndex(uint8_t slotIndex) const;

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
    //   `CLTLoginMediatorSelectionRouteState_0x41dba0` class and keeps the anchored method boundaries there

    // =============================================================================
    // Post-Auth Margin/Loading State (launcher.exe:0x4f78b8)
    // =============================================================================
    // Recovered from Ghidra analysis of launcher.exe state10/state11-era functions:
    // - 0x43c020 = CLTLoginState_State11_0x4b5154 slot 3 send body
    //   Builds/sends margin packet with first payload byte 0x4d, posts event 0x15
    // - 0x440320 = CLTLoginState_State11_0x4b5154 slot 6 reply body
    //   Handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
    //   posts event 0x16 on completion
    // =============================================================================

    // +0x24
    // anchor: launcher.exe:0x41ecd0
    uint32_t ProcessLoginRequest(const SubmitLoginRequestInput_0x407d50& input);

    // +0xe8
    // anchor: launcher.exe:0x41ec00
    uint32_t RemoveSlotRecordAndCompactRouteStateByIndex(uint32_t selectedSlotRecordIndex) override;

    // +0xf0
    // anchor: launcher.exe:0x41c390
    // Owner-side narrower state3-wait advance: stores selection index and switches to state7.
    uint32_t SetSelectionIndexAndSwitchToState7(uint32_t selectedSlotRecordIndex) override;

  // +0xec / +0xf4 wrapper-owned mirrors
  // anchor: launcher.exe:0x41c1f0 (PersistSelectionContextForState8)
  // anchor: launcher.exe:0x41ecd0 (ResetSelectionContext0ecMirror)
  // anchor: launcher.exe:0x41f1c0 (GetState8PersistenceF1c)
  // Owner-side state3-wait advance: persists the selection/config snapshot and switches to state8.
  uint32_t PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) override;
  void ResetSelectionContext0ecMirror();
  const void* GetState8PersistenceF1c() const override;

  // +0x120
    // anchor: launcher.exe:0x41c3c0
    uint32_t ProcessCreateCharacterInput120(const ProcessCreateCharacterInput120Sketch& input) override;
    // wrapper-facing arg6 `+0x120` entry used by `client.dll:0x62054d1d`
    // Keep the instance-role split explicit in source:
    // - the wrapper-facing `ILTLoginMediator_0x4af2b8.Default` mirror should capture the source block even
    //   when it is not the live owner/controller instance
    // - the live owner/controller still applies the real `0x41c3c0` state gate and helper-state
    //   transition to `10`
    uint32_t CaptureCreateCharacterInputSlot120(
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
    uint32_t State9SubmitFollowup(uint8_t helperByte4, uint16_t helperWord6);
    // - state9 slot 6 success side effect / launcher.exe:0x41b420 (owner vtable +0x16c)
    uint32_t HandleState9Opcode11SuccessSideEffect();
    // anchor: launcher.exe:0x41b4b0
    // State10 slot-3 precheck: owner `+0x1c` must exist and connection state `+0x34 == 2`.
    bool State10HasReadyConnectionState2() const;

    // anchor: launcher.exe:0x41af70
    // Original call shape is mediator-thiscall plus one stack-local `Packet_0x4af2a4`-family
    // object built by helpers like `0x43ac10/0x43ada0`, then a tail jump to current margin
    // connection vtable `+0x24`.
    // Newer Ghidra tightening now makes that two-step downstream bridge more concrete:
    // - `+0x24` = `0x41cf30 = CMessageConnection_ForwardPacketBuilderToSendPacket`
    // - that wrapper forwards packet-builder `+0x08` (the retained outer message-ref object)
    //   into `+0x28` = inherited `CMessageConnection_0x4b7928::SendPacket` / `0x448cf0`
    // - `0x448cf0` consumes that message-ref object, not bare payload bytes
    // - caller passes the real stack-local packet-builder object, not a synthetic wrapper
    void SendCurrentMarginPacket(
        mxo::liblttcp::Packet_0x4af2a4& packetBuilder);

    // anchor: launcher.exe:0x41e500
    // Narrow reusable transport/init helper kept on the mediator after moving the `0x439300`
    // case split back into `CLTLoginState_State4_0x4b503c::Slot3_BeginOrContinue`.
    // Preserved call contract from the original body:
    // - arg1 = route/prefix text used to refresh owner `+0x30`
    // - arg2 = cached non-zero selector that skips the route-refresh / address-list rebuild path
    uint32_t BeginMarginConnection(const char* routeHostText, uint8_t cachedRouteSelector);

    // Post-Auth Margin/Loading State Accessors (launcher.exe:0x4f78b8)
    // =============================================================================
    // These methods expose the owner fields used by the later post-auth margin/loading path.
    // They are used to faithfully reconstruct the original launcher's margin packet building
    // and load character reply handling logic.
    // =============================================================================

 // State-owned slot-6 bodies mutate this mediator-owned owner-state block directly.

    // Post-auth load-character reply outputs (0x440320) plus neighboring auth/margin send flags:
    // Caution: original `0x4390b0` writes owner `+0x2c` on its non-zero payload branch, while the
    // current replacement also reuses this byte as a narrow live auth-ready alias on the active
    // happy path. Keep the storage stable, but do not overstate the exact original semantic yet.

    // anchor: launcher.exe:0x41f370 / owner vtable +0x50
    // Wrapper-facing helper related to the owner `+0x680` auth/bootstrap child.
    // Newer `0x448140` tightening now keeps the reply-derived `+0xf4` copy as wire-shaped bytes,
    // so current source leaves this accessor on the original child `+0xa8` worker slot instead of
    // inventing a pointer semantic inside the copied `0x136` blob.
    void* BootstrapRaw08AuxHandle50() const override;

    // anchor: launcher.exe:0x41b500 -> 0x4435f0 / 0x441f30
    void PrepareState5MarginConnectionCopySend();

private:
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

    // ABI-safety note:
    // - margin CERT/MS bootstrap/session storage is intentionally kept in a sidecar keyed by the
    //   mediator pointer in `loginmediator.cpp`
    // - do not add those transient fields into the middle of `CLTLoginMediator`
    void FreeLateEntryList1470StorageScaffold();

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
    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine_;
    // anchor: launcher.exe:0x41b1cd - owner byte +0x04 set to 1 during Initialize()
    uint8_t ownerReadyFlag04_ = 0;


public:
    mxo::liblttcp::CMessageConnection_0x4b7928* authConnection_;
    mxo::liblttcp::CMessageConnection_0x4b7928* marginConnection_;

    // anchor: launcher.exe:0x4b01c8 +0x10 (current helper/state object)
    CLTLoginState* currentState_;

    bool authPeerCloseQueuedScaffold_ = false;
    bool marginPeerCloseQueuedScaffold_ = false;

    // owner `+0x80` - world list count or auth/status (used by state4 slot2)
    uint32_t worldListCountOrStatus80 = 0;

    // owner `+0x104`; `0x439300/0x4393a4` compare against `-1`, then forward through owner
    // vtable `+0xfc(index)` on the default state4 branch. Keep the role provisional but keep the
    // field on the mediator itself, not in a synthetic wrapper.
    int32_t marginCurrentWorldId104_ = -1;
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
    // - `+0xa4` = lazy `qspubkey.dat` `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier*`
    //   primed by `0x447260/0x447c10`
    // - `+0xa8` = raw `0x08` `CryptoPP::RSAES_OAEP_SHA_Encryptor*` rebuilt by `0x447780`
    //   and consumed by `0x4474f0` through `0x468ea0/0x468f00`
    // - `+0xac` = sibling `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier*` rebuilt by
    //   `0x447780` and consumed by `0x44aec0 = AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`
    // - `+0xf4` = original reply-derived copied `0x136` block; source now keeps that block as a
    //   wire-faithful shadow so later wrapper/state5 users can read the original `+0x85/+0xa8`
    //   suffix family without re-inventing a semantic object model
    // Replacement source now also keeps the ownership boundary explicit by storing this as a
    // separate child object instead of flattening it into the mediator body.
    std::unique_ptr<AuthBootstrap680Child_0x441290> authBootstrapChild680_;
    // Source-owned mirror for owner `+0x65c`.
    // anchor: launcher.exe:0x41f310 / owner vtable +0x130
    // Lazily allocated session callback helper (a LaunchPadClient_0x4b0e48)
    // whose `+0x18` string can later feed owner `+0x664`.
    LaunchPadClient_0x4b0e48* launchPadClient65c_ = nullptr;
    uint32_t sharedMarginPacketField660_ = 0;  // owner `+0x660`
public:
    std::string gameSessionId664_;             // owner `+0x664` (public for direct access)
    // Wrapper-facing selection object mirrors:
    // - `+0x40`  = selection-descriptor object for profile-path / arg7-derived selection work
    // - `+0x44`  = current-slot record object for later save/profile work
    // Those fake outer objects now live in `src/launcher_mediator_abi.cpp` instead of the
    // launcher-owned mediator model so the owner-side slot-record family can stay aligned with
    // `0x41f2e0 / 0x41f300`.
    // - `+0xd0`  = owner `+0x1460` state8 section-11 string view
    // - `+0x10c` = owner `+0x30` route descriptor, modeled directly as `std::string`
    // - `+0x118` = owner `+0x1470` vector-like late-entry list of 12-byte string-triple entries
    std::string routeDescriptor30_{};
    // owner `+0x1470` / arg6 `+0x118` late-entry family:
    // semantic model is `std::vector<std::string>`; the old-MSVC vector/string layout view is now
    // synthesized only inside `src/launcher_mediator_abi.cpp`.
    std::vector<std::string> lateEntryList1470_{};
    // Narrow source-owned post-state9 / post-state12 owner collaborators from
    // `0x41f1d0` / `0x41de40` / `0x41c5c0` / `0x41c510`.
    // Strongest current origin: deeper client init owner/arg6 vtable `+0x124`
    // (`netShell, netMgr, distrObjExecutive`) copied directly into this triple by `0x41f1d0`.
    void* ownerCallback84_ = nullptr;          // owner `+0x84`; bounded active-scope writes now read as init-zero at `0x41ee60`, startup-triple store at `0x41f1d0`, then submit-side reads
    void* ownerObject88_ = nullptr;            // owner `+0x88`; natural-original object88 cross-checks as client `INetMgr.Default` wrapper and no later bounded active-scope launcher write is isolated yet beyond `0x41f1d0`
    void* startupDistrObjExecutive8c_ = nullptr; // owner `+0x8c`; current best match is the client startup triple's ILTDistrObjExecutive-like third object
    uint32_t ownerOptionalField90_ = 0;                  // owner `+0x90`, only forwarded when helper byte `+4 != 0`
    // launcher.exe:0x41f0a0 / owner vtable +0x38: returns pointer to this block at +0x94
    // anchor: launcher.exe:0x41eb80 copies from submit input into this block
    // Layout: +0x00=username[32], +0x20=password[32], +0x40=keyConfigMd5[16], +0x50=uiConfigMd5[16],
    //         +0x60=sessionTokenString, +0x6c=flag6c. Total 112 bytes (0x70).
    // Note: there's also a separate session token string at +0xf4 (+0x94 + 0x60) cleared by +0x30 path.
    OwnerAuthBootstrapSource94_0x41eb80 ownerAuthBootstrapSource94_{};
    int32_t ownerCachedHandle147c_ = -1;       // owner `+0x147c`, managed-submit handle cached across `+0x1c` release / `+0x18` reacquire
    // launcher.exe owner `+0x684 .. +0xd7f` embedded selection-route helper/class
    // (`CLTLoginMediatorSelectionRouteState_0x41dba0` in current Ghidra).
  CLTLoginMediatorSelectionRouteState_0x41dba0 selectionRouteState684_{};
    // Canonical owner `+0xf1c` object returned by the original `+0xf4/+0xbc/+0xc0/+0xc4`
    // getter family.
    CLTLoginMediatorCharacterPersistenceData_0x41d900 state8PersistenceDataF1c{}; // anchor: launcher.exe owner `+0xf1c .. +0x146b`
    // launcher.exe owner `+0x108 .. +0x5d8`; earlier shared persistence block reused by the
    // create-character / helper11 path.
    CLTLoginMediatorCharacterPersistenceData_0x41d900 createCharacterData108{};

    // owner byte `+0xf14`; shared send gate used by the active state8 path and later state10.
    uint8_t state10SendGateFlagF14 = 0;

    // Remaining direct owner-side storage adjacent to the canonical `+0xf1c` persistence object.
    // Keep only fields that are still represented directly in source instead of the packed
    // `CLTLoginMediatorCharacterPersistenceData_0x41d900` blob.
    char characterNameBufferF1c[32] = {0};
    std::array<uint32_t, 10> characterRecordPointersF88{};
    std::array<char, 0x20> section0StringF8c{};
    std::array<char, 0x20> section0StringFac{};
    std::array<char, 0x20> section0StringFcc{};
    std::array<uint8_t, 0x465> state8Section0RawF88{};
    void* state8Section0OverflowBuffer13f0 = nullptr;
    uint16_t state8Section0OverflowLength13f4 = 0;
    void* allocatedBuffer13f8 = nullptr;
    uint16_t allocatedBufferLength13fc = 0;
    uint8_t flag13fe = 0;
    void* allocatedBuffer1400 = nullptr;
    uint16_t allocatedBufferLength1404 = 0;
    uint8_t flag1406 = 0;
    void* allocatedBuffer1408 = nullptr;
    uint16_t allocatedBufferLength140c = 0;
    uint8_t allocatedBufferFlag140e = 0;
    void* allocatedBuffer1410 = nullptr;
    uint16_t allocatedBufferLength1414 = 0;
    uint8_t flag1416 = 0;
    void* allocatedBuffer1418 = nullptr;
    uint16_t allocatedBufferLength141c = 0;
    uint8_t allocatedBufferFlag141e = 0;
    void* allocatedBuffer1420 = nullptr;
    uint16_t allocatedBufferLength1424 = 0;
    uint8_t allocatedBufferFlag1426 = 0;
    void* allocatedBuffer1428 = nullptr;
    uint16_t allocatedBufferLength142c = 0;
    uint8_t allocatedBufferFlag142e = 0;
    void* allocatedBuffer1430 = nullptr;
    uint16_t allocatedBufferLength1434 = 0;
    uint8_t flag1436 = 0;
    void* allocatedBuffer1438 = nullptr;
    uint16_t allocatedBufferLength143c = 0;
    uint8_t flag143e = 0;
    void* allocatedBuffer1440 = nullptr;
    uint32_t allocatedBufferLength1444 = 0;
    uint8_t flag1448 = 0;
    void* allocatedBuffer144c = nullptr;
    uint16_t allocatedBufferLength1450 = 0;
    uint8_t flag1452 = 0;
    void* allocatedBuffer1454 = nullptr;
    uint16_t allocatedBufferLength1458 = 0;
    uint8_t flag145a = 0;
    uint32_t state8Section11Dword145c = 0;
    std::string state8Section11String1460;
    std::array<uint8_t, 8> replyParseBuffer{};
    uint8_t section0Flag13f6 = 0;
    uint32_t state6UdpSessionSecretF18_ = 0;  // owner +0xf18
    // launcher.exe:0x4f78b8 owner-side world-list packet pointer table (`+0xd84`).
    // Static-RE from `0x41ee60` ctor plus owner readers `0x41b2e0/0x41b320/0x41b360/0x41b3a0` shows:
    // - fixed 100-entry pointer table
    // - count tracked separately at `+0xd80`
    std::array<Packet_WorldList_0x4b533c*, kRecoveredWorldSlotCapacity> worldListPacketsD84_{};
    uint8_t worldListPacketCountD80_ = 0;

    // Observer/listener state for late-login arg6 +0x170/+0x174 bridge:
    // - +0x170 (RegisterLoginObserver) inserts into owner `+0x674`
    // - +0x174 (UnregisterLoginObserver) removes from owner `+0x674`
    // Keep the container shape explicit instead of flattening it into a vector because the
    // original `0x41cfb0 / 0x41d090` traversals are std::_Tree-like in-order walks over this data.
    LoginObserverTree674 observerTree674_{};
    LoginObserverTreeNode674 observerTreeHeader674_{};

    uint16_t authServerPortHostOrder_;

    uint16_t marginServerPortHostOrder_;
    bool ignoreHostsFileForMargin_;

    // owner `+0x5c .. +0x6b`; `0x41d170` materializes a temporary endpoint and copies all four
    // dwords here before calling connection vtable `+0x1c` with `&owner+0x5c`.
    mxo::liblttcp::LTTCPEndpointKey_0x44b070 authEndpoint5c_{};
    // owner `+0x6c .. +0x7b`; `0x41e500` materializes a temporary endpoint and copies all four
    // dwords here before calling connection vtable `+0x1c` with `&owner+0x6c`.
    mxo::liblttcp::LTTCPEndpointKey_0x44b070 marginEndpoint6c_{};

    std::string authUsername_;
    std::string authPassword_;
    uint32_t authLauncherVersion_;
    std::vector<uint8_t> authKeyConfigMd5_;
    std::vector<uint8_t> authUiConfigMd5_;


    // Original non-virtual `CLTIPAddressList` helper rooted at owner `+0x4c`.
    // Recovered original in-object layout:
    // - `+0x4c` = begin pointer
    // - `+0x50` = end pointer
    // - `+0x54` = capacity pointer
    // - `+0x58` = current iterator pointer
    // Nearby owner-side state that is not part of the helper object itself remains explicit:
    // - owner `+0x28` = auth connect attempt count / retry gate state
    mxo::liblttcp::CLTIPAddressList authAddressList4c_{};
    uint32_t authConnectAttemptCount28_ = 0;

    // Current active replacement path no longer uses a synthetic startup world-list sidecar.
    // The launcher selection menu in `src/textmode_launcher_flow.cpp` now runs after auth success
    // and consumes the recovered owner tables (`+0xd84/+0x688/+0x818`) directly.
    // Keep only the narrower configured arg6 selection scratch that is still used by the
    // wrapper-facing descriptor/profile bridges.
    uint32_t arg6VariantWorldNameQueryCountE0_ = 0u; // wrapper-facing arg6 `+0xe0` query count
};

// anchor: launcher.exe:0x43b300 / 0x41b450 / 0x4f7868..0x4f78b4
// Source-owned mirror of the contiguous global login-helper dispatch table.
extern CLTLoginMediator::ConnectionHelperFamily g_LoginHelperDispatchTableScaffold;

// anchor: launcher.exe:0x4f78b8
// Source-owned mirror of the launcher global current mediator pointer consumed by the helper family.
extern CLTLoginMediator* g_CurrentLoginMediator;

// External globals from launcher.exe used by faithful Initialize implementation.
// anchor: launcher.exe:0x4d6304
extern void* g_pThreadPerClientTCPEngine;
// anchor: launcher.exe:0x4f7b14
extern const char* g_qsAuthServerDNSName;
// anchor: launcher.exe:0x4d6780
extern uint32_t g_IgnoreHostsFileForAuth;
// anchor: launcher.exe:0x4f7a50
extern uint16_t g_AuthServerPort;
// Server RSA public key (base64 encoded) - from selected server config
extern const char* g_ServerPublicModulusB64;
extern const char* g_ServerPublicExponentB64;
// Skip AS_GetPublicKeyReply embedded key validation (for non-standard key sizes like 2048-bit)
extern uint32_t g_SkipAuthPublicKeyReplyValidation;
// Margin server globals - analogous to auth globals, for faithful server-selection flow.
extern const char* g_marginServerDNSName;
extern uint16_t g_marginServerPort;

}  // namespace mxo::ltlogin

}  // namespace mxo
