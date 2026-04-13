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

}  // namespace

CLTLoginMediator::ConnectionHelperFamily g_LoginHelperDispatchTableScaffold = {};

// Inline state objects - replaces static BuiltinScaffoldStates singleton pattern
// anchor: launcher.exe:0x43b300 / dispatch table seed
static CLTLoginState_State0 g_State0 = {};
static CLTLoginState_State1 g_State1 = {};
static CLTLoginState_AuthenticatePending g_AuthenticatePending = {};
static CLTLoginState_State3 g_State3 = {};
static CLTLoginState_State4 g_State4 = {};
static CLTLoginState_State5 g_State5 = {};
static CLTLoginState_State6 g_State6 = {};
static CLTLoginState_State7 g_State7 = {};
static CLTLoginState_State8 g_State8 = {};
static CLTLoginState_State9 g_State9 = {};
static CLTLoginState_State10 g_State10 = {};
static CLTLoginState_State11 g_State11 = {};
static CLTLoginState_State12 g_State12 = {};
static CLTLoginState_State13 g_State13 = {};
static CLTLoginState_WorldListPending g_WorldListPending = {};
static CLTLoginState_State15 g_State15 = {};
static CLTLoginState_State16 g_State16 = {};
static CLTLoginState_State17 g_State17 = {};
static CLTLoginState_State18 g_State18 = {};
static CLTLoginState_State19 g_State19 = {};

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::LoginHelperStateByIdScaffold(uint32_t helperStateId) const {
    return (helperStateId < 20u) ? const_cast<CLTLoginState*&>(reinterpret_cast<CLTLoginState**>(&g_LoginHelperDispatchTableScaffold.helper7868)[helperStateId]) : nullptr;
}

// anchor: launcher.exe:0x43b300 / static helper-dispatch table seed
// Static method - does not use this pointer, only initializes global state dispatch table.
void CLTLoginMediator::InitializeHelperDispatchTable() {
    // Direct dispatch table slot assignments - matches static-RE pattern at 0x43b300
    // Original was a static method that allocated heap and stored vtable pointers to globals
    g_LoginHelperDispatchTableScaffold.helper7868 = &g_State0;
    g_LoginHelperDispatchTableScaffold.helper786C = &g_State1;
    g_LoginHelperDispatchTableScaffold.helper7870 = &g_AuthenticatePending;
    g_LoginHelperDispatchTableScaffold.helper7874 = &g_State3;
    g_LoginHelperDispatchTableScaffold.helper7878 = &g_State4;
    g_LoginHelperDispatchTableScaffold.helper787C = &g_State5;
    g_LoginHelperDispatchTableScaffold.helper7880 = &g_State6;
    g_LoginHelperDispatchTableScaffold.helper7884 = &g_State7;
    g_LoginHelperDispatchTableScaffold.helper7888 = &g_State8;
    g_LoginHelperDispatchTableScaffold.helper788C = &g_State9;
    g_LoginHelperDispatchTableScaffold.helper7890 = &g_State10;
    g_LoginHelperDispatchTableScaffold.helper7894 = &g_State11;
    g_LoginHelperDispatchTableScaffold.helper7898 = &g_State12;
    g_LoginHelperDispatchTableScaffold.helper789C = &g_State13;
    g_LoginHelperDispatchTableScaffold.helper78A0 = &g_WorldListPending;
    g_LoginHelperDispatchTableScaffold.helper78A4 = &g_State15;
    g_LoginHelperDispatchTableScaffold.helper78A8 = &g_State16;
    g_LoginHelperDispatchTableScaffold.helper78AC = &g_State17;
    g_LoginHelperDispatchTableScaffold.helper78B0 = &g_State18;
    g_LoginHelperDispatchTableScaffold.helper78B4 = &g_State19;
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
