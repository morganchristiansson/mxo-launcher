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
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>

namespace mxo::ltlogin {
namespace {

// Keep the early auth-entry split self-contained so loginmediator.cpp no longer needs the
// auth-credential logging helper just for state1/connect-status bringup.
static const char* MaskedAuthValue(const std::string& value) {
    return value.empty() ? "<empty>" : "<provided>";
}

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

void CLTLoginMediator::RegisterScaffoldState0(CLTLoginState* state) {
    scaffoldState0_ = state;
}

void CLTLoginMediator::RegisterScaffoldState1(CLTLoginState* state) {
    scaffoldState1_ = state;
}

void CLTLoginMediator::RegisterScaffoldState2(CLTLoginState* state) {
    scaffoldState2_ = state;
}

void CLTLoginMediator::RegisterScaffoldState3(CLTLoginState* state) {
    scaffoldState3_ = state;
}

void CLTLoginMediator::RegisterScaffoldState4(CLTLoginState* state) {
    scaffoldState4_ = state;
}

void CLTLoginMediator::RegisterScaffoldState6(CLTLoginState* state) {
    scaffoldState6_ = state;
}

void CLTLoginMediator::RegisterScaffoldState8(CLTLoginState* state) {
    scaffoldState8_ = state;
}

void CLTLoginMediator::RegisterScaffoldState9(CLTLoginState* state) {
    scaffoldState9_ = state;
}

void CLTLoginMediator::RegisterScaffoldState10(CLTLoginState* state) {
    scaffoldState10_ = state;
}

void CLTLoginMediator::RegisterScaffoldState11(CLTLoginState* state) {
    scaffoldState11_ = state;
}

void CLTLoginMediator::RegisterScaffoldState12(CLTLoginState* state) {
    scaffoldState12_ = state;
}

void CLTLoginMediator::RegisterScaffoldState13(CLTLoginState* state) {
    scaffoldState13_ = state;
}

void CLTLoginMediator::RegisterScaffoldState14(CLTLoginState* state) {
    scaffoldState14_ = state;
}

void CLTLoginMediator::RegisterScaffoldState15(CLTLoginState* state) {
    scaffoldState15_ = state;
}

void CLTLoginMediator::RegisterScaffoldState16(CLTLoginState* state) {
    scaffoldState16_ = state;
}

void CLTLoginMediator::RegisterScaffoldState17(CLTLoginState* state) {
    scaffoldState17_ = state;
}

void CLTLoginMediator::RegisterScaffoldState18(CLTLoginState* state) {
    scaffoldState18_ = state;
}

void CLTLoginMediator::RegisterScaffoldState19(CLTLoginState* state) {
    scaffoldState19_ = state;
}

CLTLoginState* CLTLoginMediator::ScaffoldState0() const {
    return scaffoldState0_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState1() const {
    return scaffoldState1_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState2() const {
    return scaffoldState2_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState3() const {
    return scaffoldState3_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState4() const {
    return scaffoldState4_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState6() const {
    return scaffoldState6_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState8() const {
    return scaffoldState8_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState9() const {
    return scaffoldState9_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState10() const {
    return scaffoldState10_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState11() const {
    return scaffoldState11_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState12() const {
    return scaffoldState12_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState13() const {
    return scaffoldState13_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState14() const {
    return scaffoldState14_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState15() const {
    return scaffoldState15_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState16() const {
    return scaffoldState16_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState17() const {
    return scaffoldState17_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState18() const {
    return scaffoldState18_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState19() const {
    return scaffoldState19_;
}

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

void CLTLoginMediator::SetAuthConnectionContextKey(void* contextKey) {
    authConnectionContextKey_ = contextKey;
}

void CLTLoginMediator::SetMarginConnectionContextKey(void* contextKey) {
    marginConnectionContextKey_ = contextKey;
}

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
    ResetMarginBootstrapState();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth credentials configured username='%s' password=%s",
        authUsername_.empty() ? "<empty>" : authUsername_.c_str(),
        MaskedAuthValue(authPassword_));
}

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
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth bootstrap configured launcherVersion=%u currentPublicKeyId=%u loginType=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
        (unsigned)authLauncherVersion_,
        (unsigned)authCurrentPublicKeyId_,
        (unsigned)authLoginType_,
        (unsigned)authKeyConfigMd5_.size(),
        (unsigned)authUiConfigMd5_.size());
}

void CLTLoginMediator::SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile) {
    authServerDnsName_ = dnsName ? dnsName : "";
    authServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForAuth_ = ignoreHostsFile;
    BuildAuthEndpoint();
}

void CLTLoginMediator::SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile) {
    marginServerDnsSuffix_ = dnsSuffix ? dnsSuffix : "";
    marginServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForMargin_ = ignoreHostsFile;
    BuildMarginEndpoint();
}

std::string CLTLoginMediator::ResolvedMarginHostName() const {
    if (!marginRouteState_.exactMarginHostName.empty()) {
        return marginRouteState_.exactMarginHostName;
    }
    if (!marginRouteState_.routeHostPrefix.empty() && !marginServerDnsSuffix_.empty()) {
        return marginRouteState_.routeHostPrefix + marginServerDnsSuffix_;
    }
    return std::string();
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::AuthConnection() const {
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::MarginConnection() const {
    return marginConnection_;
}

void CLTLoginMediator::SetMarginRouteHostPrefix(const char* routeHostPrefix) {
    marginRouteState_.routeHostPrefix = routeHostPrefix ? routeHostPrefix : "";
}

void CLTLoginMediator::SetExactMarginHostName(const char* exactMarginHostName) {
    marginRouteState_.exactMarginHostName = exactMarginHostName ? exactMarginHostName : "";
}

const CLTLoginMediator::MarginRouteState& CLTLoginMediator::CurrentMarginRouteState() const {
    return marginRouteState_;
}

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

bool CLTLoginMediator::HasReadyAuthConnectionState2() const {
    return authConnection_ != nullptr &&
           authConnection_->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

void CLTLoginMediator::SetProcessLoginRequestAlternateState16BranchScaffold(bool enabled) {
    processLoginRequestAlternateState16BranchScaffold_ = enabled;
    spdlog::info(
        "CLTLoginMediator::SetProcessLoginRequestAlternateState16BranchScaffold enabled={} (default-off scaffold for DAT_004d66ec!=0 alternate state16/session family; proven happy path remains DAT_004d66ec==0)",
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
        "CLTLoginMediator::ProcessLoginRequest copied owner+0x94 username='{}' password='{}' string60Len={} currentState={} stateCode={} altState16Branch={} helper65cPresent={} submitOwnership=owner",
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
            "ROUTE CHECKPOINT: early-auth ProcessLoginRequest from state0 currentState={} string60Empty={} alternateState16Scaffold={} helper65cPresent={}",
            upstreamState ? upstreamState->DebugName() : "<null>",
            string60Empty ? 1u : 0u,
            processLoginRequestAlternateState16BranchScaffold_ ? 1u : 0u,
            sessionCallbackHelper65c_ ? 1u : 0u);
    }
    if (!processLoginRequestAlternateState16BranchScaffold_) {
        // Static + runtime now line up on the default happy path at `0x41ecd0`:
        // - after copying the input block into owner `+0x94`, the code tests `DAT_004d66ec`
        // - on `DAT_004d66ec == 0`, it clears owner `+0xf4` (`+0x94 + 0x60`) through
        //   `0x407dd0`
        // - then it calls `0x41b450(2)` while the current helper is still state0
        // - the next state-owned body is therefore `0x439210` on helper/state 2 with upstream
        //   state0
        // This is the exact favored happy path and keeps submit ownership on the mediator/owner,
        // not on state0.
        AssignOwnedSmallStringForAuthEntry(authBootstrapSource38_, nullptr, nullptr);
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state0 -> state2 via owner ProcessLoginRequest (favored DAT_004d66ec==0 happy path) upstreamState={} clearedOwnerF4=1",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (scaffoldState2_ != nullptr) {
            const uint32_t state2EntryResult = SwitchHelperStateAndDispatchSlot3Scaffold(
                2u,
                scaffoldState2_,
                upstreamState,
                "ProcessLoginRequest default DAT_004d66ec==0 branch");
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

    // Default-off source-owned scaffolds for the alternate `DAT_004d66ec != 0` family.
    // Static `0x41ecd0` now narrows that split more concretely than before:
    // - if string60 is non-empty and helper65c is absent, switch to state16
    // - if string60 is non-empty and helper65c is present, switch back to state2
    // - if string60 is empty, optionally refresh owner `+0xf4` from helper65c `+0x18`, then
    //   switch to state16
    // Keep that family explicit but default-off so the proven `DAT_004d66ec == 0` happy path
    // remains the exact favored route.
    if (!string60Empty) {
        if (sessionCallbackHelper65c_ == nullptr) {
            spdlog::info(
                "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest alternate branch -> state16 (string60 non-empty, helper65c absent) upstreamState={}",
                upstreamState ? upstreamState->DebugName() : "<null>");
            if (scaffoldState16_ != nullptr) {
                (void)SwitchHelperStateAndDispatchSlot3Scaffold(
                    16u,
                    scaffoldState16_,
                    upstreamState,
                    "ProcessLoginRequest alternate DAT_004d66ec!=0 branch / no helper65c");
            } else {
                spdlog::info(
                    "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate no-helper65c branch currentState={}",
                    currentState_ ? currentState_->DebugName() : "<null>");
            }
            return 0u;
        }

        spdlog::info(
            "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest alternate branch -> state2 (string60 non-empty, helper65c present) upstreamState={}",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (scaffoldState2_ != nullptr) {
            (void)SwitchHelperStateAndDispatchSlot3Scaffold(
                2u,
                scaffoldState2_,
                upstreamState,
                "ProcessLoginRequest alternate DAT_004d66ec!=0 branch / helper65c present");
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
            "CLTLoginMediator::ProcessLoginRequest alternate DAT_004d66ec!=0 branch refreshed owner+0xf4 from helper65c string18='{}'",
            sessionCallbackHelper65cState_.string18.empty() ? "<empty>" : sessionCallbackHelper65cState_.string18.c_str());
    }

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest alternate branch -> state16 (string60 empty) upstreamState={} helper65cPresent={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        sessionCallbackHelper65c_ ? 1u : 0u);
    if (scaffoldState16_ != nullptr) {
        (void)SwitchHelperStateAndDispatchSlot3Scaffold(
            16u,
            scaffoldState16_,
            upstreamState,
            "ProcessLoginRequest alternate DAT_004d66ec!=0 branch / string60 empty");
    } else {
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate string60-empty branch currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return 0u;
}

void CLTLoginMediator::InitializeConnectionHelpers() {
    // anchor: launcher.exe:0x43b300
    // Initializes the helper/state dispatch table rooted at `0x4f7868..0x4f78b4`.
    // Early concrete states that now have source-owned bodies/scaffolds (including the initial
    // idle/start state0, plus state1, state2, and state14) are registered separately through
    // RegisterScaffoldState*.
    // This initializer therefore still only materializes the late
    // `CLTLoginState_State15..State19` tail recovered concretely so far
    // (`0x420640/0x4206e0/0x420850/0x420920/0x4209a0`).
    InitializeHelperDispatchSlot15();
    InitializeHelperDispatchSlot16();
    InitializeHelperDispatchSlot17();
    InitializeHelperDispatchSlot18();
    InitializeHelperDispatchSlot19();
}

void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    // Address anchor: launcher.exe:0x420640 = InitializeHelperDispatchSlot15
    // Original: allocates 8 bytes, stores vtable 0x4b0b88 (`CLTLoginState_State15`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0b88);
        helpers_.helper78A4 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    // Address anchor: launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16
    // Original: allocates 4 bytes, stores vtable 0x4b0bb0 (`CLTLoginState_State16`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0bb0);
        helpers_.helper78A8 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    // Address anchor: launcher.exe:0x420850 = InitializeHelperDispatchSlot17
    // Original: allocates 4 bytes, stores vtable 0x4b0bd8 (`CLTLoginState_State17`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0bd8);
        helpers_.helper78AC = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    // Address anchor: launcher.exe:0x420920 = InitializeHelperDispatchSlot18
    // Original: allocates 8 bytes, stores vtable 0x4b0c00 (`CLTLoginState_State18`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0c00);
        helpers_.helper78B0 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    // Address anchor: launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19
    // Original: allocates 4 bytes, stores vtable 0x4b0c28 (`CLTLoginState_State19`).
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b0c28);
        helpers_.helper78B4 = ptr;
    }
}

uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Address anchors:
    // - launcher.exe:0x41d170 = strongest current BeginAuthConnection implementation
    // - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection (upstream)
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best launcher path:
    // - this is reached only after owner-owned submit has already handed off from state0 -> state2
    //   -> state1 on the happy path
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort` from owner `+0x4c8`
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
    ResetMarginBootstrapState();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ = nullptr;
    BuildAuthEndpoint();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) return 0;
    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);
    return connection->EnsureConnected();
}

uint32_t CLTLoginMediator::BeginAuthConnectionViaState1Scaffold() {
    CLTLoginState* const state1 = scaffoldState1_;
    if (state1 == nullptr) {
        spdlog::warn(
            "CLTLoginMediator::BeginAuthConnectionViaState1Scaffold missing registered state1 scaffold currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    CLTLoginState* const upstreamState = currentState_;
    spdlog::info(
        "ROUTE CHECKPOINT: early-auth entering state1 auth-connect upstreamState={} currentStateBeforeSwitch={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>");
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

uint32_t CLTLoginMediator::ContinueRecordedAuthConnectStatusScaffold() {
    // Keep the status-recording contract explicit:
    // - `HandleAuthConnectStatus` must cache the type-2 payload on the mediator first
    // - state1 slot 1 then consumes that cached payload through `LastAuthConnectStatus()`
    // - non-state1 callers still use the narrow historical fallback while earlier startup
    //   ownership remains only partially source-owned
    if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 1u) {
        return currentState_->Slot1_HandlePrimaryGate(this);
    }

    return (lastAuthConnectStatus_ == kConnectStatusSuccess) ? BeginAuthHandshake() : 0u;
}

uint32_t CLTLoginMediator::HandleAuthConnectStatus(uint32_t workResultCode) {
    lastAuthConnectStatus_ = workResultCode;
    ++authConnectStatusCount_;
    return ContinueRecordedAuthConnectStatusScaffold();
}

uint32_t CLTLoginMediator::BeginAuthHandshake() {
    // Address anchors:
    // - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap / state2 slot 3
    // - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (ready-side dispatcher)
    // - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
    // - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    //
    // Static `0x439210` now narrows the ready-side ownership better:
    // - state2 slot 3, not the mediator owner, owns the ready/not-ready split
    // - on the ready side it gathers owner/bootstrap inputs and forwards them into `0x448050`
    // - the current source still keeps that dispatcher bridge narrow here while the deeper
    //   state2-owned bootstrap body is migrated out of mediator code incrementally
    // The standalone auth probe remains the current wire/reference implementation for the
    // launcher-owned auth loop, so reuse the shared low-level runtime-style auth helpers here
    // instead of keeping a second launcher-only packet path.
    expectedAuthRequestName_ = kMessageAsGetPublicKeyRequest;
    return SendAuthGetPublicKeyRequest();
}

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

void CLTLoginMediator::BuildMarginEndpoint() {
    marginEndpoint_ = {};
    marginEndpoint_.family = 2;
    marginEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((marginServerPortHostOrder_ << 8) | (marginServerPortHostOrder_ >> 8));
    marginEndpoint_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureAuthConnectionObject() {
    if (engine_ && authConnectionContextKey_) {
        authConnection_ = engine_->GetOrCreateMessageConnection(authConnectionContextKey_);
        authConnectionOwnedByMediator_ = false;
    } else if (!authConnection_) {
        authConnection_ = new mxo::liblttcp::CMessageConnection(engine_);
        authConnectionOwnedByMediator_ = true;
    }

    if (authConnection_) {
        // anchor: launcher.exe:0x448960 via `0x41d170`
        // Auth connection startup config writes `+0x78 = 1` and `+0x70 = FUN_0041ce00`.
        // Current source scaffold keeps the packet-name family / packetized-mode side of that
        // connection metadata explicit even though the callback body itself is still only used as
        // evidence for naming, not as a live function pointer.
        authConnection_->ConfigurePacketNameFamilyScaffold(
            mxo::liblttcp::CMessageConnectionPacketNameFamilyScaffold::kAuth,
            /*packetizedMessagesEnabled=*/true);
    }
    return authConnection_;
}

}  // namespace mxo::ltlogin
