/**
 * CLTLoginMediator early auth/state-entry split.
 *
 * Keep this TU narrow:
 * - scaffold registration/accessors used by diagnostics auth bringup
 * - auth connection entry and state1 connect-status bridging
 * - adjacent startup connection-config helpers that cluster with that path
 *
 * Intentionally exclude:
 * - later auth packet handlers / broader bootstrap ownership
 * - state8/state9 and other late runtime continuations
 */

#include "loginmediator.h"

#include "loginstate.h"
#include "../../../runtime/src/liblttcp/ltipaddresslist.h"
#include <spdlog/spdlog.h>

#include <cstdlib>

namespace mxo::ltlogin {
namespace {

// Keep the early auth-entry split self-contained so loginmediator.cpp no longer needs the
// auth-credential logging helper just for state1/connect-status bringup.
// UNANCHORED: no original launcher.exe anchor assigned yet.
static void AssignOwnedSmallStringForAuthEntry(
    AuthBootstrapSelectedSource38Sketch& dest,
    const char* begin,
    const char* current) {
    dest.string60Owned.clear();
    dest.string60 = {};

    if (!begin || !current || current <= begin) {
        return;
    }

    dest.string60Owned.assign(begin, current);
    dest.string60.begin = dest.string60Owned.c_str();
    dest.string60.current = dest.string60.begin + dest.string60Owned.size();
    dest.string60.capacity = dest.string60.current;
}

struct BuiltinScaffoldStates {
    CLTLoginState_State0 state0 = {};
    CLTLoginState_State1 state1 = {};
    CLTLoginState_AuthenticatePending authenticatePending = {};
    CLTLoginState_State3 state3 = {};
    CLTLoginState_State4 state4 = {};
    CLTLoginState_State5 state5 = {};
    CLTLoginState_State6 state6 = {};
    CLTLoginState_State7 state7 = {};
    CLTLoginState_State8 state8 = {};
    CLTLoginState_State9 state9 = {};
    CLTLoginState_State10 state10 = {};
    CLTLoginState_State11 state11 = {};
    CLTLoginState_State12 state12 = {};
    CLTLoginState_State13 state13 = {};
    CLTLoginState_WorldListPending worldListPending = {};
    CLTLoginState_State15 state15 = {};
    CLTLoginState_State16 state16 = {};
    CLTLoginState_State17 state17 = {};
    CLTLoginState_State18 state18 = {};
    CLTLoginState_State19 state19 = {};
};

// UNANCHORED: no original launcher.exe anchor assigned yet.
static BuiltinScaffoldStates& GetBuiltinScaffoldStates() {
    static BuiltinScaffoldStates states = {};
    return states;
}

static CLTLoginState*& GlobalLoginHelperDispatchSlotByIdScaffold(uint32_t helperStateId) {
    return reinterpret_cast<CLTLoginState**>(&g_LoginHelperDispatchTableScaffold.helper7868)[helperStateId];
}

}  // namespace

CLTLoginMediator::ConnectionHelperFamily g_LoginHelperDispatchTableScaffold = {};

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState0(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(0u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState1(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(1u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState2(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(2u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState3(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(3u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState4(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(4u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState5(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(5u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState6(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(6u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState7(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(7u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState8(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(8u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState9(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(9u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState10(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(10u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState11(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(11u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState12(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(12u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState13(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(13u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState14(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(14u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState15(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(15u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState16(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(16u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState17(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(17u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState18(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(18u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState19(CLTLoginState* state) {
    GlobalLoginHelperDispatchSlotByIdScaffold(19u) = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::LoginHelperStateByIdScaffold(uint32_t helperStateId) const {
    return (helperStateId < 20u) ? const_cast<CLTLoginState*&>(GlobalLoginHelperDispatchSlotByIdScaffold(helperStateId)) : nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InstallInitialState0Scaffold() {
    if (LoginHelperStateByIdScaffold(0u) == nullptr) {
        spdlog::info(
            "ROUTE CHECKPOINT: startup initial state0 scaffold missing currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
        return;
    }

    // Fresh happy-path proof keeps startup ownership split explicit:
    // - mediator init installs state0 as the initial idle/start helper
    // - state0 itself does not own the first submit transition because its slot 3 is the shared
    //   no-op stub
    // - `ProcessLoginRequest` later performs the first happy-path state switch (`state0 -> state2`)
    currentState_ = LoginHelperStateByIdScaffold(0u);
    spdlog::info(
        "ROUTE CHECKPOINT: startup installed initial helper state0 currentState={} role=idle/start anchor=(launcher.exe:0x41b160 -> owner+0x10 = helper0 / 0x4f7868)",
        currentState_->DebugName());
}

// anchor: launcher.exe:0x43b300 / full helper-dispatch table seed
void CLTLoginMediator::InitializeHelperDispatchTable() {
    auto& states = GetBuiltinScaffoldStates();

    RegisterScaffoldState0(&states.state0);
    RegisterScaffoldState1(&states.state1);
    RegisterScaffoldState2(&states.authenticatePending);
    RegisterScaffoldState3(&states.state3);
    RegisterScaffoldState4(&states.state4);
    RegisterScaffoldState5(&states.state5);
    RegisterScaffoldState6(&states.state6);
    RegisterScaffoldState7(&states.state7);
    RegisterScaffoldState8(&states.state8);
    RegisterScaffoldState9(&states.state9);
    RegisterScaffoldState10(&states.state10);
    RegisterScaffoldState11(&states.state11);
    RegisterScaffoldState12(&states.state12);
    RegisterScaffoldState13(&states.state13);
    RegisterScaffoldState14(&states.worldListPending);
    RegisterScaffoldState15(&states.state15);
    RegisterScaffoldState16(&states.state16);
    RegisterScaffoldState17(&states.state17);
    RegisterScaffoldState18(&states.state18);
    RegisterScaffoldState19(&states.state19);

    if (currentState_ == nullptr) {
        InstallInitialState0Scaffold();
    }
}

uint32_t CLTLoginMediator::BeginLauncherMarginConnectionScaffold() {
    if (engine_ == nullptr) {
        spdlog::info(
            "CLTLoginMediator::BeginLauncherMarginConnectionScaffold skipped engine={}",
            fmt::ptr(engine_));
        return 0u;
    }

    marginPeerCloseQueuedScaffold_ = false;

    const uint32_t result = BeginMarginConnectionViaState4Scaffold();
    const std::string marginHost = ResolvedMarginHostName();

    engine_->SyncAttachedLauncherObjectStateScaffold();

    spdlog::info(
        "CLTLoginMediator::BeginLauncherMarginConnectionScaffold marginHost='{}' -> 0x{:08x}",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}



// UNANCHORED: source-owned config setter for the auth host/port scaffold feeding owner `+0x4c/+0x5c`.
void CLTLoginMediator::SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile) {
    authServerDnsName_ = dnsName ? dnsName : "";
    authServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForAuth_ = ignoreHostsFile;
    ResetAuthConnectRetryStateScaffold();
    BuildAuthEndpoint();
}

// UNANCHORED: source-owned reset for the auth-side owner `+0x28/+0x4c/+0x50/+0x58` retry/iterator family.
void CLTLoginMediator::ResetAuthConnectRetryStateScaffold() {
    authAddressListResolvedHostName4c_ = authServerDnsName_;
    authAddressList4c_.Reset();
    authConnectAttemptCount28_ = 0;
}

// UNANCHORED: source-owned getter for the mirrored auth connect-attempt counter at owner `+0x28`.
uint32_t CLTLoginMediator::AuthConnectAttemptCountScaffold() const {
    return authConnectAttemptCount28_;
}

// UNANCHORED: source-owned getter for the mirrored auth candidate count derived from owner `+0x4c/+0x50`.
uint32_t CLTLoginMediator::AuthConnectCandidateCountScaffold() const {
    return static_cast<uint32_t>(authAddressList4c_.Count());
}

// UNANCHORED: source-owned predicate matching the `0x4390b0` owner `+0x28` vs `(+0x50 - +0x4c) >> 2` retry gate.
bool CLTLoginMediator::HasAuthConnectRetryCandidateRemainingScaffold() const {
    return authConnectAttemptCount28_ < AuthConnectCandidateCountScaffold();
}

// UNANCHORED: source-owned host-resolution mirror for the auth-side dword IPv4 list rooted at owner `+0x4c`.
void CLTLoginMediator::RefreshAuthAddressListForCurrentHostScaffold() {
    const bool hostChanged = (authAddressListResolvedHostName4c_ != authServerDnsName_);
    if (!hostChanged && !authAddressList4c_.Empty()) {
        return;
    }

    authAddressListResolvedHostName4c_ = authServerDnsName_;

    if (authServerDnsName_.empty()) {
        authAddressList4c_.Reset();
        return;
    }

    uint32_t flags = mxo::liblttcp::CLTIPAddressList::kFlagShuffle;
    if (ignoreHostsFileForAuth_) {
        flags |= mxo::liblttcp::CLTIPAddressList::kFlagIgnoreHostsFile;
    }

    (void)authAddressList4c_.Reinit(authServerDnsName_.c_str(), flags);
}

// UNANCHORED: source-owned iterator step mirroring the `0x440bb0/0x44b090` auth endpoint prep family.
void CLTLoginMediator::PrepareNextAuthEndpointForConnectAttemptScaffold() {
    RefreshAuthAddressListForCurrentHostScaffold();

    const uint32_t nextIpv4 = authAddressList4c_.GetNextAddress(/*wrap=*/true);
    if (nextIpv4 != 0u) {
        authEndpoint_.ipv4NetworkOrder = nextIpv4;
    }

    ++authConnectAttemptCount28_;
}

// UNANCHORED: source-owned config setter for the margin suffix/port scaffold feeding owner `+0x30/+0x6c`.
void CLTLoginMediator::SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile) {
    marginServerDnsSuffix_ = dnsSuffix ? dnsSuffix : "";
    marginServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForMargin_ = ignoreHostsFile;
    marginSelectedIpv4_7c_ = 0u;
    BuildMarginEndpoint();
    if (!ResolvedMarginHostName().empty() && RebuildMarginAddressList() && SelectMarginEndpointIpv4()) {
        BuildMarginEndpoint();
    }
}

// UNANCHORED: source-owned route-text resolver for the reconstructed margin-host scaffold.
std::string CLTLoginMediator::ResolvedMarginHostName() const {
    if (!marginRouteState_.exactMarginHostName.empty()) {
        return marginRouteState_.exactMarginHostName;
    }
    if (!marginRouteState_.routeHostPrefix.empty() && !marginServerDnsSuffix_.empty()) {
        return marginRouteState_.routeHostPrefix + marginServerDnsSuffix_;
    }
    return std::string();
}

// UNANCHORED: source-owned accessor for the auth `CMessageConnection` child mirrored from owner `+0x18`.
mxo::liblttcp::CMessageConnection* CLTLoginMediator::AuthConnection() const {
    return authConnection_;
}

// UNANCHORED: source-owned accessor for the margin `CMessageConnection` child mirrored from owner `+0x1c`.
mxo::liblttcp::CMessageConnection* CLTLoginMediator::MarginConnection() const {
    return marginConnection_;
}

// UNANCHORED: source-owned setter for the reconstructed margin route-host prefix.
void CLTLoginMediator::SetMarginRouteHostPrefix(const char* routeHostPrefix) {
    marginRouteState_.routeHostPrefix = routeHostPrefix ? routeHostPrefix : "";
    marginSelectedIpv4_7c_ = 0u;
    if (!ResolvedMarginHostName().empty() && RebuildMarginAddressList() && SelectMarginEndpointIpv4()) {
        BuildMarginEndpoint();
    }
}

// UNANCHORED: source-owned setter for the reconstructed exact margin host name.
void CLTLoginMediator::SetExactMarginHostName(const char* exactMarginHostName) {
    marginRouteState_.exactMarginHostName = exactMarginHostName ? exactMarginHostName : "";
    marginSelectedIpv4_7c_ = 0u;
    if (!ResolvedMarginHostName().empty() && RebuildMarginAddressList() && SelectMarginEndpointIpv4()) {
        BuildMarginEndpoint();
    }
}

// UNANCHORED: source-owned accessor for the reconstructed margin route-state mirror.
const CLTLoginMediator::MarginRouteState& CLTLoginMediator::CurrentMarginRouteState() const {
    return marginRouteState_;
}


// anchor: launcher.exe:0x41b490
bool CLTLoginMediator::HasReadyAuthConnectionState2() const {
    return authConnection_ != nullptr &&
           authConnection_->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// anchor: launcher.exe:0x41ecd0
uint32_t CLTLoginMediator::ProcessLoginRequest(const ProcessLoginRequestInputSketch& input) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 1u:
        case 2u:
        case 3u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000006",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000006u;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000000",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000000u;
        case 12u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000007",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000007u;
        default:
            break;
    }

    const bool string60Empty = (input.string60.current == input.string60.begin);
    if ((input.inlineString00[0] == '\0' || input.inlineString20[0] == '\0') && string60Empty) {
        spdlog::info(
            "ROUTE CHECKPOINT: ProcessLoginRequest rejected empty credentials currentState={} -> 0x00000004",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 4u;
    }

    authBootstrapSource38_.inlineString00 = input.inlineString00;
    authBootstrapSource38_.inlineString20 = input.inlineString20;
    authBootstrapSource38_.block40 = input.block40;
    authBootstrapSource38_.block50 = input.block50;
    authBootstrapSource38_.flag6C = input.flag6C;
    AssignOwnedSmallStringForAuthEntry(authBootstrapSource38_, input.string60.begin, input.string60.current);

    spdlog::info(
        "CLTLoginMediator::ProcessLoginRequest copied owner+0x94 username='{}' password='{}' string60Len={} currentState={} stateCode={} launchPadGateState16State18AltPath={} helper65cPresent={} submitOwnership=owner",
        authBootstrapSource38_.inlineString00[0] ? authBootstrapSource38_.inlineString00.data() : "<empty>",
        authBootstrapSource38_.inlineString20[0] ? authBootstrapSource38_.inlineString20.data() : "<empty>",
        static_cast<unsigned>(authBootstrapSource38_.string60Owned.size()),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(stateCode),
        0u,
        sessionCallbackHelper65c_ ? 1u : 0u);

    CLTLoginState* const upstreamState = currentState_;
    if (stateCode == 0u) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth ProcessLoginRequest from state0 currentState={} string60Empty={} launchPadGateState16State18Scaffold={} helper65cPresent={}",
            upstreamState ? upstreamState->DebugName() : "<null>",
            string60Empty ? 1u : 0u,
            0u,
            sessionCallbackHelper65c_ ? 1u : 0u);
    }
    if (true) {
        // Static + runtime now line up on the default happy path at `0x41ecd0`:
        // - after copying the input block into owner `+0x94`, the code tests
        //   `g_LaunchPadGateState16State18`
        // - on `g_LaunchPadGateState16State18 == 0`, it clears owner `+0xf4`
        //   (`+0x94 + 0x60`) through `0x407dd0`
        // - then it calls `0x41b450(2)` while the current helper is still state0
        // - the next state-owned body is therefore `0x439210` on helper/state 2 with upstream
        //   state0, and state2 owns the ready/not-ready handoff into the owner `+0x680`
        //   bootstrap child
        // This is the exact favored happy path and keeps submit ownership on the mediator/owner,
        // not on state0.
        AssignOwnedSmallStringForAuthEntry(authBootstrapSource38_, nullptr, nullptr);
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state0 -> state2 via owner ProcessLoginRequest (favored g_LaunchPadGateState16State18==0 happy path) upstreamState={} clearedOwnerF4=1",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (LoginHelperStateByIdScaffold(2u) != nullptr) {
            const uint32_t state2EntryResult = SwitchHelperStateByIdScaffold(2u);
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest default state2 entry upstreamState={} -> slot3Result=0x{:08x}",
                upstreamState ? upstreamState->DebugName() : "<null>",
                static_cast<unsigned>(state2EntryResult));
        } else {
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest has no registered state2 scaffold; leaving currentState={} after owner+0xf4 clear",
                currentState_ ? currentState_->DebugName() : "<null>");
        }
        return 0u;
    }

    // Default-off source-owned scaffolds for the alternate
    // `g_LaunchPadGateState16State18 != 0` family.
    // Static `0x41ecd0` now narrows that split more concretely than before:
    // - if string60 is non-empty and helper65c is absent, switch to state16
    // - if string60 is non-empty and helper65c is present, switch back to state2
    // - if string60 is empty, optionally refresh owner `+0xf4` from helper65c `+0x18`, then
    //   switch to state16
    // Keep that state16/state18 family explicit but default-off so the proven
    // `g_LaunchPadGateState16State18 == 0` happy path remains the exact favored route.
    // That alternate family is separate from the active state2 -> owner+0x680 bootstrap-child
    // handoff.
    if (!string60Empty) {
        if (sessionCallbackHelper65c_ == nullptr) {
            spdlog::info(
                "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state16 (string60 non-empty, helper65c absent) upstreamState={}",
                upstreamState ? upstreamState->DebugName() : "<null>");
            if (LoginHelperStateByIdScaffold(16u) != nullptr) {
                (void)SwitchHelperStateByIdScaffold(16u);
            } else {
                spdlog::info(
                    "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate no-helper65c branch currentState={}",
                    currentState_ ? currentState_->DebugName() : "<null>");
            }
            return 0u;
        }

        spdlog::info(
            "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state2 (string60 non-empty, helper65c present) upstreamState={}",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (LoginHelperStateByIdScaffold(2u) != nullptr) {
            (void)SwitchHelperStateByIdScaffold(2u);
        } else {
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest missing registered state2 scaffold for alternate helper65c-present branch currentState={}",
                currentState_ ? currentState_->DebugName() : "<null>");
        }
        return 0u;
    }

    if (sessionCallbackHelper65c_ != nullptr) {
        const char* helperString = sessionCallbackHelper65cState_.string18.c_str();
        AssignOwnedSmallStringForAuthEntry(
            authBootstrapSource38_,
            helperString,
            helperString + sessionCallbackHelper65cState_.string18.size());
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest alternate g_LaunchPadGateState16State18!=0 branch refreshed owner+0xf4 from helper65c string18='{}'",
            sessionCallbackHelper65cState_.string18.empty() ? "<empty>" : sessionCallbackHelper65cState_.string18.c_str());
    }

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state16 (string60 empty) upstreamState={} helper65cPresent={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        sessionCallbackHelper65c_ ? 1u : 0u);
    if (LoginHelperStateByIdScaffold(16u) != nullptr) {
        (void)SwitchHelperStateByIdScaffold(16u);
    } else {
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate string60-empty branch currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    if (LoginHelperStateByIdScaffold(15u) == nullptr) {
        RegisterScaffoldState15(&GetBuiltinScaffoldStates().state15);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    if (LoginHelperStateByIdScaffold(16u) == nullptr) {
        RegisterScaffoldState16(&GetBuiltinScaffoldStates().state16);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    if (LoginHelperStateByIdScaffold(17u) == nullptr) {
        RegisterScaffoldState17(&GetBuiltinScaffoldStates().state17);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    if (LoginHelperStateByIdScaffold(18u) == nullptr) {
        RegisterScaffoldState18(&GetBuiltinScaffoldStates().state18);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    if (LoginHelperStateByIdScaffold(19u) == nullptr) {
        RegisterScaffoldState19(&GetBuiltinScaffoldStates().state19);
    }
}

// anchor: launcher.exe:0x41d170
uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Address anchors:
    // - launcher.exe:0x41d170 = strongest current BeginAuthConnection implementation
    // - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection (upstream)
    // - launcher.exe:0x440bb0 = auth-side dword-list iterator used from owner `+0x4c`
    // - launcher.exe:0x44b090 = endpoint builder fed by the selected IPv4 + auth port
    //
    // Current tightened launcher path:
    // - this is reached only after owner-owned submit has already handed off from state0 -> state2
    //   -> state1 on the happy path
    // - clear owner byte `+0x2c`
    // - increment owner dword `+0x28`
    // - pull the next auth IPv4 dword from the iterator rooted at owner `+0x4c`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)` (ensure-connected wrapper)
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    authConnectionFlag2c_ = 0u;
    // Fidelity improvement for launcher retry:
    // - `0x41d170` allocates a fresh auth connection child at owner `+0x18`, but current static RE
    //   does not show it clearing the broader owner `+0x680` bootstrap child or cached public-key
    //   state on every retry
    // - preserve the cached AS_GetPublicKeyReply / currentPublicKeyId / child worker family across
    //   retries so later compact raw-`0x07` replies with no embedded key material can still reuse
    //   the previously validated key when the publicKeyId matches
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    postAuthMarginAutoBeginAttemptedScaffold_ = false;
    ResetMarginBootstrapState();
    EnsureAuthBootstrapChild680Scaffold();
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ = nullptr;
    BuildAuthEndpoint();
    PrepareNextAuthEndpointForConnectAttemptScaffold();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) {
        return 0u;
    }

    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);

    spdlog::info(
        "CLTLoginMediator::BeginAuthConnection host='{}' attemptCount28={} candidateCount={} selectedIpv4=0x{:08x} currentState={} authFlag2c={} -> EnsureConnected()",
        authServerDnsName_.empty() ? "<empty>" : authServerDnsName_.c_str(),
        static_cast<unsigned>(AuthConnectAttemptCountScaffold()),
        static_cast<unsigned>(AuthConnectCandidateCountScaffold()),
        static_cast<unsigned>(authEndpoint_.ipv4NetworkOrder),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(authConnectionFlag2c_));
    return connection->EnsureConnected();
}

// UNANCHORED: source-owned early-auth helper that stages state1 then dispatches slot 3.
uint32_t CLTLoginMediator::BeginAuthConnectionViaState1Scaffold() {
    CLTLoginState* const state1 = LoginHelperStateByIdScaffold(1u);
    if (state1 == nullptr) {
        spdlog::warn(
            "CLTLoginMediator::BeginAuthConnectionViaState1Scaffold missing registered state1 scaffold currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    ResetAuthConnectRetryStateScaffold();

    CLTLoginState* const upstreamState = currentState_;
    spdlog::info(
        "ROUTE CHECKPOINT: early-auth entering state1 auth-connect upstreamState={} currentStateBeforeSwitch={} resetAuthRetryState=1 attemptCount28={} candidateCount={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(AuthConnectAttemptCountScaffold()),
        static_cast<unsigned>(AuthConnectCandidateCountScaffold()));
    const uint32_t result = SwitchHelperStateByIdScaffold(1u);
    spdlog::info(
        "CLTLoginMediator::BeginAuthConnectionViaState1Scaffold upstreamState={} currentState={} -> result=0x{:08x}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::BeginMarginConnectionViaState4Scaffold() {
    CLTLoginState* const state4 = LoginHelperStateByIdScaffold(4u);
    if (state4 == nullptr) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnectionViaState4Scaffold missing registered state4 scaffold currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    CLTLoginState* const upstreamState = currentState_;
    const uint32_t result = SwitchHelperStateByIdScaffold(4u);
    const std::string marginHost = ResolvedMarginHostName();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnectionViaState4Scaffold upstreamState={} currentState={} marginHost='{}' -> result=0x{:08x}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: source-owned status replay helper that re-enters the active helper after caching type-2 auth connect status.
uint32_t CLTLoginMediator::ContinueRecordedAuthConnectStatusScaffold() {
    // Keep the status-recording contract explicit:
    // - `HandleAuthConnectStatus` must cache the type-2 payload on the mediator first
    // - the natural auth leaf now routes that same work through `0x449a70 -> 0x41af80 -> slot1`
    // - this replay helper therefore synthesizes only the same narrow type-2 work-item shape when
    //   state1 is still active, while non-state1 callers retain the older live-success fallback
    if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 1u) {
        mxo::liblttcp::CLTThreadPerClientTCPEngine_ConnectionStatusWorkItemScaffold statusWorkItem = {};
        statusWorkItem.header.workType =
            mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus;
        statusWorkItem.header.statusOrPayloadDword08 = lastAuthConnectStatus_;
        // There is no separate launcher owner wrapper for this tail re-entry beyond
        // `0x41af80`; this source-only replay intentionally mirrors only the final current-helper
        // slot-1 dispatch on the synthesized type-2 work-item shape.
        return currentState_->Slot1_HandlePrimaryGate(&statusWorkItem, this);
    }

    return (lastAuthConnectStatus_ == kConnectStatusSuccess)
        ? AuthBootstrapChild680().PrepareAndDispatch(*this)
        : 0u;
}

// UNANCHORED: source-owned owner-side cache/update entry for auth type-2 connect-status work.
uint32_t CLTLoginMediator::HandleAuthConnectStatus(uint32_t workResultCode) {
    lastAuthConnectStatus_ = workResultCode;
    ++authConnectStatusCount_;
    if (authConnection_ != nullptr &&
        ((workResultCode == 0u &&
          authConnection_->State() == mxo::liblttcp::LTTCPEngineConnectionState::kConnectActive) ||
         workResultCode == kConnectStatusSuccess)) {
        // Current source now re-enters helper/state 2 on auth connect success, so mirror the
        // owner-side `+0x18 -> +0x34 == 2` ready state before state2 runs its `0x41b490` gate.
        authConnection_->SetState(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);
    }
    return ContinueRecordedAuthConnectStatusScaffold();
}

// UNANCHORED: source-owned builder for the auth endpoint mirror rooted at owner `+0x5c`.
void CLTLoginMediator::BuildAuthEndpoint() {
    // Placeholder only.
    // Original launcher currently appears to preserve host text in owner `+0x4c` and then
    // build a sockaddr-like endpoint block at owner `+0x5c` using the current auth port.
    authEndpoint_ = {};
    authEndpoint_.family = 2;
    authEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((authServerPortHostOrder_ << 8) | (authServerPortHostOrder_ >> 8));
    authEndpoint_.ipv4NetworkOrder = 0;
}

// UNANCHORED: source-owned builder for the margin endpoint mirror rooted at owner `+0x6c`.
void CLTLoginMediator::BuildMarginEndpoint() {
    marginEndpoint_ = {};
    marginEndpoint_.family = 2;
    marginEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((marginServerPortHostOrder_ << 8) | (marginServerPortHostOrder_ >> 8));
    marginEndpoint_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
}

// UNANCHORED: source-owned auth-connection child materializer mirroring owner `+0x18` construction.
mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureAuthConnectionObject() {
    mxo::liblttcp::CAuthStartupConnection* authConnection =
        dynamic_cast<mxo::liblttcp::CAuthStartupConnection*>(authConnection_);
    if (!authConnection) {
        if (authConnectionOwnedByMediator_ && authConnection_ != nullptr) {
            delete authConnection_;
        }
        authConnection = new mxo::liblttcp::CAuthStartupConnection(engine_);
        if (!authConnection) {
            authConnection_ = nullptr;
            authConnectionOwnedByMediator_ = false;
            return nullptr;
        }

        authConnection_ = authConnection;
        authConnectionOwnedByMediator_ = true;
    }

    authConnection->SetEngine(engine_);
    authConnection->SetOwnerContext(this);

    // anchor: launcher.exe:0x41d170 / vtable `0x004afef0`
    // Current bounded source correction:
    // - auth startup no longer materializes only a base `CMessageConnection` here
    // - source now keeps the auth-side leaf completion wrapper explicit so type-2 status and the
    //   later receive-drain proxy can re-enter through the nearer connection callback surface
    authConnection->ConfigurePacketNameFamily(
        mxo::liblttcp::CMessageConnectionPacketNameFamily::kAuth,
        /*packetizedMessagesEnabled=*/true);
    return authConnection_;
}

}  // namespace mxo::ltlogin
