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
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <cstdlib>

namespace mxo::ltlogin {
namespace {

// Keep the early auth-entry split self-contained so loginmediator.cpp no longer needs the
// auth-credential logging helper just for state1/connect-status bringup.
// UNANCHORED: no original launcher.exe anchor assigned yet.
static const char* MaskedAuthValue(const std::string& value) {
    return value.empty() ? "<empty>" : "<provided>";
}

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

// anchor: launcher.exe:0x41ecd0 / raw owner vtable +0x30
// Current upstream launcher dialog tightening around the same input shape:
// - manual nopatch login on page state `6` routes keystrokes through
//   `0x408840 = LauncherLoginRichEdit_HandlePromptInputCharacter`
// - its submit helper `0x408400 = LauncherLoginRichEdit_SubmitCredentialsViaResolvedLoginService`
//   builds the same stack block layout now used here:
//   - `+0x00` username
//   - `+0x20` password
//   - zeroed `block40/block50`
//   - small-string at `+0x60`
// - that helper then submits through sibling resolved slot `0x4d2734` vtable `+0x30`
// - newer raw-vtable clarification closes that bridge directly:
//   - raw mediator vtable memory at `0x004b01f8` stores `0x41ecd0`
//   - so the launcher page-`6` helper's `call [eax+0x30]` is the same recovered
//     `CLTLoginMediator::ProcessLoginRequest` boundary
// - current replacement keeps that same owner boundary but can now mirror the launcher submit
//   surface through the binder-backed raw `+0x30` slot instead of only by calling the owner method
//   directly
static bool BuildConfiguredProcessLoginRequestInput(
    const CLTLoginMediator& mediator,
    ProcessLoginRequestInputSketch* outInput,
    const char** outUsernameSource,
    const char** outPasswordSource) {
    if (!outInput) {
        return false;
    }

    *outInput = {};
    if (outUsernameSource) {
        *outUsernameSource = "<none>";
    }
    if (outPasswordSource) {
        *outPasswordSource = "<none>";
    }

    const char* username = mediator.Arg6AuthName();
    const char* password = mediator.Arg6AuthPassword();
    if (outUsernameSource) {
        *outUsernameSource = "arg6 +0x5c";
    }
    if (outPasswordSource) {
        *outPasswordSource = "arg6 +0x60";
    }

    if (!username || username[0] == '\0' || !password || password[0] == '\0') {
        return false;
    }

    const auto copyBoundedString = [](auto& dest, const char* src) {
        std::fill(dest.begin(), dest.end(), '\0');
        if (!src || src[0] == '\0') {
            return;
        }
        const size_t copyCount = std::min(
            std::char_traits<char>::length(src),
            dest.size() - 1u);
        std::copy_n(src, copyCount, dest.begin());
        dest[copyCount] = '\0';
    };

    copyBoundedString(outInput->inlineString00, username);
    copyBoundedString(outInput->inlineString20, password);
    return true;
}

struct BuiltinScaffoldStates {
    CLTLoginState_State0 state0 = {};
    CLTLoginState_State1 state1 = {};
    CLTLoginState_AuthenticatePending authenticatePending = {};
    CLTLoginState_State3 state3 = {};
    CLTLoginState_State4 state4 = {};
    CLTLoginState_State5 state5 = {};
    CLTLoginState_State6 state6 = {};
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

}  // namespace

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState0(CLTLoginState* state) {
    scaffoldState0_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState1(CLTLoginState* state) {
    scaffoldState1_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState2(CLTLoginState* state) {
    scaffoldState2_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState3(CLTLoginState* state) {
    scaffoldState3_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState4(CLTLoginState* state) {
    scaffoldState4_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState5(CLTLoginState* state) {
    scaffoldState5_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState6(CLTLoginState* state) {
    scaffoldState6_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState8(CLTLoginState* state) {
    scaffoldState8_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState9(CLTLoginState* state) {
    scaffoldState9_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState10(CLTLoginState* state) {
    scaffoldState10_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState11(CLTLoginState* state) {
    scaffoldState11_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState12(CLTLoginState* state) {
    scaffoldState12_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState13(CLTLoginState* state) {
    scaffoldState13_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState14(CLTLoginState* state) {
    scaffoldState14_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState15(CLTLoginState* state) {
    scaffoldState15_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState16(CLTLoginState* state) {
    scaffoldState16_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState17(CLTLoginState* state) {
    scaffoldState17_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState18(CLTLoginState* state) {
    scaffoldState18_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RegisterScaffoldState19(CLTLoginState* state) {
    scaffoldState19_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState0() const {
    return scaffoldState0_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState1() const {
    return scaffoldState1_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState2() const {
    return scaffoldState2_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState3() const {
    return scaffoldState3_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState4() const {
    return scaffoldState4_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState5() const {
    return scaffoldState5_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState6() const {
    return scaffoldState6_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState8() const {
    return scaffoldState8_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState9() const {
    return scaffoldState9_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState10() const {
    return scaffoldState10_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState11() const {
    return scaffoldState11_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState12() const {
    return scaffoldState12_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState13() const {
    return scaffoldState13_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState14() const {
    return scaffoldState14_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState15() const {
    return scaffoldState15_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState16() const {
    return scaffoldState16_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState17() const {
    return scaffoldState17_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState18() const {
    return scaffoldState18_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::ScaffoldState19() const {
    return scaffoldState19_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InstallInitialState0Scaffold() {
    if (scaffoldState0_ == nullptr) {
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
    currentState_ = scaffoldState0_;
    spdlog::info(
        "ROUTE CHECKPOINT: startup installed initial helper state0 currentState={} role=idle/start anchor=(launcher.exe:0x41b160 -> owner+0x10 = helper0 / 0x4f7868)",
        currentState_->DebugName());
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::EnsureBuiltinScaffoldStatesRegistered() {
    auto& states = GetBuiltinScaffoldStates();

    RegisterScaffoldState0(&states.state0);
    RegisterScaffoldState1(&states.state1);
    RegisterScaffoldState2(&states.authenticatePending);
    RegisterScaffoldState3(&states.state3);
    RegisterScaffoldState4(&states.state4);
    RegisterScaffoldState5(&states.state5);
    RegisterScaffoldState6(&states.state6);
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
    InitializeConnectionHelpers();

    if (currentState_ == nullptr) {
        InstallInitialState0Scaffold();
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::BindLauncherConnectionBridgeScaffold(
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    if (!engine) {
        return;
    }

    SetNetworkEngine(engine);
    EnsureBuiltinScaffoldStatesRegistered();
    RegisterActiveStateSourceScaffold(this);
    spdlog::info(
        "CLTLoginMediator::BindLauncherConnectionBridgeScaffold engine={} currentState={}",
        fmt::ptr(engine),
        currentState_ ? currentState_->DebugName() : "<null>");
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::CanBeginLauncherAuthConnectionScaffold() const {
    return engine_ != nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::BeginLauncherAuthConnectionScaffold() {
    if (!CanBeginLauncherAuthConnectionScaffold()) {
        spdlog::info(
            "CLTLoginMediator::BeginLauncherAuthConnectionScaffold skipped engine={}",
            fmt::ptr(engine_));
        return 0u;
    }

    CLTLoginMediatorConnectionContextScaffold* context =
        engine_->EnsureLauncherConnectionContextScaffold(
            &authConnectionContextScaffold_,
            this,
            "AuthConnection",
            /*isMarginConnection=*/false);
    if (context) {
        context->peerCloseQueued = false;
        context->sidecarConnection = EnsureAuthConnectionObject();
    }

    // Current replacement-side fidelity correction:
    // - original launcher password submit does not jump straight into `0x439210` with an
    //   out-of-band username/password store
    // - upstream dialog tightening now shows the manual nopatch page-`6` rich-edit host collecting
    //   username then password and building the exact `0x41ecd0`-style stack block before handing
    //   it to sibling resolved slot `0x4d2734` vtable `+0x30`
    // - raw mediator-vtable clarification now closes that callsite more tightly:
    //   `0x41ecd0` lives at raw vtable entry `0x004b01f8`, so the launcher helper's
    //   `call [eax+0x30]` reaches `CLTLoginMediator::ProcessLoginRequest`
    // - current replacement mirrors that same submit surface when the binder-backed raw `+0x30`
    //   slot is available, then falls back to the direct owner call only when the raw surface is
    //   unavailable
    // - keep the exact trusted owner boundary on `0x41ecd0` anyway: it is the earlier anchored API
    //   that accepts the username/password block, copies it into owner `+0x94`, clears owner
    //   `+0xf4` on the happy path, and then performs the observed
    //   `state0 -> state2 -> state1 -> state2` chain feeding the owner `+0x680` bootstrap child
    // - preserve the older direct state2 fallback only when we are no longer in the initial idle
    //   helper or when no launcher-configured auth block is available to mirror through
    //   `ProcessLoginRequest`
    const uint32_t currentPhaseCode = currentState_ ? currentState_->DispatchPhaseCode() : 0xffu;
    ProcessLoginRequestInputSketch submitInput = {};
    const char* usernameSource = "<none>";
    const char* passwordSource = "<none>";
    const bool haveConfiguredSubmitInput = BuildConfiguredProcessLoginRequestInput(
        *this,
        &submitInput,
        &usernameSource,
        &passwordSource);

    uint32_t result = 0u;
    if (currentPhaseCode == 0u && haveConfiguredSubmitInput) {
        const bool haveResolvedSubmitSurface =
            DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface();
        spdlog::info(
            "ROUTE CHECKPOINT: launcher bridge auto-begin auth via {} currentState={} usernameSource={} passwordSource={} ownerSource94Empty={}",
            haveResolvedSubmitSurface
                ? "resolved ILTLoginMediator.Default-style raw +0x30 ProcessLoginRequest surface"
                : "direct owner ProcessLoginRequest fallback",
            currentState_ ? currentState_->DebugName() : "<null>",
            usernameSource,
            passwordSource,
            (authBootstrapSource38_.inlineString00[0] == '\0' && authBootstrapSource38_.inlineString20[0] == '\0') ? 1u : 0u);
        result = haveResolvedSubmitSurface
            ? DiagnosticSubmitLoginRequestViaResolvedMediatorSurface(submitInput)
            : ProcessLoginRequest(submitInput);
    } else {
        CLTLoginState* const upstreamState = currentState_;
        CLTLoginState* const state2 = scaffoldState2_;
        spdlog::info(
            "ROUTE CHECKPOINT: launcher bridge auto-begin auth via state2 fallback currentState={} upstreamState={} state2Registered={} currentPhaseCode={} haveConfiguredSubmitInput={}",
            currentState_ ? currentState_->DebugName() : "<null>",
            upstreamState ? upstreamState->DebugName() : "<null>",
            state2 ? 1u : 0u,
            static_cast<unsigned>(currentPhaseCode),
            haveConfiguredSubmitInput ? 1u : 0u);
        result = state2 != nullptr
            ? SwitchHelperStateAndDispatchSlot3Scaffold(
                  2u,
                  state2,
                  upstreamState,
                  "BeginLauncherAuthConnectionScaffold -> helper2/auth-bootstrap continuation")
            : BeginAuthConnectionViaState1Scaffold();
    }
    if (context) {
        context->sidecarConnection = AuthConnection();
    }

    engine_->SyncAttachedLauncherObjectStateScaffold();

    spdlog::info(
        "CLTLoginMediator::BeginLauncherAuthConnectionScaffold -> 0x{:08x}",
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::BeginLauncherMarginConnectionScaffold() {
    if (engine_ == nullptr) {
        spdlog::info(
            "CLTLoginMediator::BeginLauncherMarginConnectionScaffold skipped engine={}",
            fmt::ptr(engine_));
        return 0u;
    }

    CLTLoginMediatorConnectionContextScaffold* context =
        engine_->EnsureLauncherConnectionContextScaffold(
            &marginConnectionContextScaffold_,
            this,
            "MarginConnection",
            /*isMarginConnection=*/true);
    if (context) {
        context->peerCloseQueued = false;
        context->sidecarConnection = EnsureMarginConnectionObject();
    }

    const uint32_t result = BeginMarginConnectionViaState4Scaffold();
    const std::string marginHost = ResolvedMarginHostName();
    if (context) {
        context->sidecarConnection = MarginConnection();
    }

    engine_->SyncAttachedLauncherObjectStateScaffold();

    spdlog::info(
        "CLTLoginMediator::BeginLauncherMarginConnectionScaffold marginHost='{}' -> 0x{:08x}",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: replacement-owned reset/config shim.
// Keep this separate from the recovered password-submit API at `0x41ecd0`:
// - launcher.exe feeds submitted username/password through `ProcessLoginRequest`
// - this shim now only preserves diagnostic mirrors and resets transient auth/bootstrap state
void CLTLoginMediator::SetAuthCredentials(const char* username, const char* password) {
    authUsername_ = username ? username : "";
    authPassword_ = password ? password : "";
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    postAuthMarginAutoBeginAttemptedScaffold_ = false;
    ResetMarginBootstrapState();
    authBootstrapChild680_ = std::make_unique<AuthBootstrap680Child>();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth reset/config shim updated username='{}' password={} (live submit path should still enter through 0x41ecd0 / ProcessLoginRequest)",
        authUsername_.empty() ? "<empty>" : authUsername_.c_str(),
        MaskedAuthValue(authPassword_));
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetAuthBootstrapConfig(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    uint8_t loginType,
    const std::vector<uint8_t>& keyConfigMd5,
    const std::vector<uint8_t>& uiConfigMd5) {
    authLauncherVersion_ = launcherVersion;
    authCurrentPublicKeyId_ = currentPublicKeyId;
    authLoginType_ = loginType;
    authKeyConfigMd5_ = keyConfigMd5;
    authUiConfigMd5_ = uiConfigMd5;

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth bootstrap configured launcherVersion={} currentPublicKeyId={} loginType={} keyConfigMd5Len={} uiConfigMd5Len={}",
        static_cast<unsigned>(authLauncherVersion_),
        static_cast<unsigned>(authCurrentPublicKeyId_),
        static_cast<unsigned>(authLoginType_),
        static_cast<unsigned>(authKeyConfigMd5_.size()),
        static_cast<unsigned>(authUiConfigMd5_.size()));
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

// UNANCHORED: source-owned launcher-bridge sidecar resolver.
// Current static-RE split:
// - auth/margin connection constructors store the owning mediator directly at connection `+0xa4`
// - the extra bridge record remains only as source-owned helper-pump state keyed by the mediator's
//   auth/margin child pointers rather than as the connection's actual owner field
CLTLoginMediatorConnectionContextScaffold* CLTLoginMediator::ResolveConnectionBridgeContextScaffold(
    const mxo::liblttcp::CMessageConnection* connection) const {
    if (connection == authConnection_) {
        return authConnectionContextScaffold_;
    }
    if (connection == marginConnection_) {
        return marginConnectionContextScaffold_;
    }
    return nullptr;
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

// UNANCHORED: source-owned wrapper over `0x41b450` plus explicit new-helper slot-3 dispatch.
uint32_t CLTLoginMediator::SwitchHelperStateAndDispatchSlot3Scaffold(
    uint32_t helperStateId,
    CLTLoginState* state,
    void* upstreamOrArg,
    const char* reason) {
    const CLTLoginState* const oldState = currentState_;
    SwitchHelperStateScaffold(helperStateId, state);
    if (state == nullptr) {
        spdlog::info(
            "CLTLoginMediator::SwitchHelperStateAndDispatchSlot3Scaffold helperState=0x{:02x} reason='{}' oldState={} upstream={} upstreamPhaseCode={} -> skipped (no target state)",
            static_cast<unsigned>(helperStateId),
            reason ? reason : "<unspecified>",
            oldState ? oldState->DebugName() : "<null>",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(RecoverCachedUpstreamPhaseCode(upstreamOrArg)));
        return 0u;
    }

    const uint32_t result = state->Slot3_BeginOrContinue(upstreamOrArg, this);
    spdlog::info(
        "CLTLoginMediator::SwitchHelperStateAndDispatchSlot3Scaffold helperState=0x{:02x} reason='{}' oldState={} newState={} upstream={} upstreamPhaseCode={} -> slot3Result=0x{:08x}",
        static_cast<unsigned>(helperStateId),
        reason ? reason : "<unspecified>",
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName(),
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(RecoverCachedUpstreamPhaseCode(upstreamOrArg)),
        static_cast<unsigned>(result));
    return result;
}

// anchor: launcher.exe:0x41b490
bool CLTLoginMediator::HasReadyAuthConnectionState2() const {
    return authConnection_ != nullptr &&
           authConnection_->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// UNANCHORED: source-owned switch for the default-off `g_LaunchPadGateState16State18 != 0` branch scaffold.
void CLTLoginMediator::SetProcessLoginRequestAlternateState16BranchScaffold(bool enabled) {
    processLoginRequestAlternateState16BranchScaffold_ = enabled;
    spdlog::info(
        "CLTLoginMediator::SetProcessLoginRequestAlternateState16BranchScaffold enabled={} (default-off scaffold mirroring g_LaunchPadGateState16State18!=0 alternate state16/state18 family; proven happy path remains g_LaunchPadGateState16State18==0)",
        enabled ? 1u : 0u);
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
        processLoginRequestAlternateState16BranchScaffold_ ? 1u : 0u,
        sessionCallbackHelper65c_ ? 1u : 0u);

    CLTLoginState* const upstreamState = currentState_;
    if (stateCode == 0u) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth ProcessLoginRequest from state0 currentState={} string60Empty={} launchPadGateState16State18Scaffold={} helper65cPresent={}",
            upstreamState ? upstreamState->DebugName() : "<null>",
            string60Empty ? 1u : 0u,
            processLoginRequestAlternateState16BranchScaffold_ ? 1u : 0u,
            sessionCallbackHelper65c_ ? 1u : 0u);
    }
    if (!processLoginRequestAlternateState16BranchScaffold_) {
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
        if (scaffoldState2_ != nullptr) {
            const uint32_t state2EntryResult = SwitchHelperStateAndDispatchSlot3Scaffold(
                2u,
                scaffoldState2_,
                upstreamState,
                "ProcessLoginRequest default g_LaunchPadGateState16State18==0 branch");
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
            if (scaffoldState16_ != nullptr) {
                (void)SwitchHelperStateAndDispatchSlot3Scaffold(
                    16u,
                    scaffoldState16_,
                    upstreamState,
                    "ProcessLoginRequest alternate g_LaunchPadGateState16State18!=0 branch / no helper65c");
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
        if (scaffoldState2_ != nullptr) {
            (void)SwitchHelperStateAndDispatchSlot3Scaffold(
                2u,
                scaffoldState2_,
                upstreamState,
                "ProcessLoginRequest alternate g_LaunchPadGateState16State18!=0 branch / helper65c present");
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
    if (scaffoldState16_ != nullptr) {
        (void)SwitchHelperStateAndDispatchSlot3Scaffold(
            16u,
            scaffoldState16_,
            upstreamState,
            "ProcessLoginRequest alternate g_LaunchPadGateState16State18!=0 branch / string60 empty");
    } else {
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate string60-empty branch currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeConnectionHelpers() {
    // anchor: launcher.exe:0x43b300
    // Initializes the helper/state dispatch table rooted at `0x4f7868..0x4f78b4`.
    // Early concrete states that now have source-owned bodies/scaffolds (including the initial
    // idle/start state0, plus state1, state2, and state14) are registered separately through
    // RegisterScaffoldState*.
    // This initializer therefore still only materializes the late
    // `CLTLoginState_State15..State19` tail recovered concretely so far
    // (`0x420640/0x4206e0/0x420850/0x420920/0x4209a0`).
    if (helpers_.helper78A4 != nullptr || helpers_.helper78A8 != nullptr ||
        helpers_.helper78AC != nullptr || helpers_.helper78B0 != nullptr ||
        helpers_.helper78B4 != nullptr) {
        return;
    }

    InitializeHelperDispatchSlot15();
    InitializeHelperDispatchSlot16();
    InitializeHelperDispatchSlot17();
    InitializeHelperDispatchSlot18();
    InitializeHelperDispatchSlot19();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    // Address anchor: launcher.exe:0x420640 = InitializeHelperDispatchSlot15
    // Original: allocates 8 bytes, stores vtable 0x4b0b88 (`CLTLoginState_State15`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0b88);
        helpers_.helper78A4 = ptr;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    // Address anchor: launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16
    // Original: allocates 4 bytes, stores vtable 0x4b0bb0 (`CLTLoginState_State16`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0bb0);
        helpers_.helper78A8 = ptr;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    // Address anchor: launcher.exe:0x420850 = InitializeHelperDispatchSlot17
    // Original: allocates 4 bytes, stores vtable 0x4b0bd8 (`CLTLoginState_State17`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0bd8);
        helpers_.helper78AC = ptr;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    // Address anchor: launcher.exe:0x420920 = InitializeHelperDispatchSlot18
    // Original: allocates 8 bytes, stores vtable 0x4b0c00 (`CLTLoginState_State18`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0c00);
        helpers_.helper78B0 = ptr;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    // Address anchor: launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19
    // Original: allocates 4 bytes, stores vtable 0x4b0c28 (`CLTLoginState_State19`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0c28);
        helpers_.helper78B4 = ptr;
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
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    postAuthMarginAutoBeginAttemptedScaffold_ = false;
    ResetMarginBootstrapState();
    authBootstrapChild680_ = std::make_unique<AuthBootstrap680Child>();
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

// UNANCHORED: source-owned early-auth bridge that stages state1 then dispatches slot 3.
uint32_t CLTLoginMediator::BeginAuthConnectionViaState1Scaffold() {
    CLTLoginState* const state1 = scaffoldState1_;
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
    const uint32_t result = SwitchHelperStateAndDispatchSlot3Scaffold(
        1u,
        state1,
        upstreamState,
        "BeginAuthConnectionViaState1Scaffold");
    spdlog::info(
        "CLTLoginMediator::BeginAuthConnectionViaState1Scaffold upstreamState={} currentState={} -> result=0x{:08x}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::BeginMarginConnectionViaState4Scaffold() {
    CLTLoginState* const state4 = scaffoldState4_;
    if (state4 == nullptr) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnectionViaState4Scaffold missing registered state4 scaffold currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    CLTLoginState* const upstreamState = currentState_;
    const uint32_t result = SwitchHelperStateAndDispatchSlot3Scaffold(
        4u,
        state4,
        upstreamState,
        "BeginMarginConnectionViaState4Scaffold");
    const std::string marginHost = ResolvedMarginHostName();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnectionViaState4Scaffold upstreamState={} currentState={} marginHost='{}' -> result=0x{:08x}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}

// UNANCHORED: source-owned status replay bridge that re-enters the active helper after caching type-2 auth connect status.
uint32_t CLTLoginMediator::ContinueRecordedAuthConnectStatusScaffold() {
    // Keep the status-recording contract explicit:
    // - `HandleAuthConnectStatus` must cache the type-2 payload on the mediator first
    // - the natural auth leaf now routes that same work through `0x449a70 -> 0x41af80 -> slot1`
    // - this replay helper therefore synthesizes only the same narrow type-2 work-item shape when
    //   state1 is still active, while non-state1 callers retain the older live-success fallback
    if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 1u) {
        CLTLoginMediatorQueuedWorkItemScaffold statusWorkItem = {};
        statusWorkItem.header.workType =
            mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus;
        statusWorkItem.workPayload = lastAuthConnectStatus_;
        statusWorkItem.debugLabel = "Synthetic auth connect-status replay";
        return DispatchCurrentHelperPrimaryGateScaffold(&statusWorkItem);
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
        if (authConnectionOwnedByMediator_) {
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
    if (authConnectionContextScaffold_ != nullptr) {
        authConnectionContextScaffold_->sidecarConnection = authConnection;
    }

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
