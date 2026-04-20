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

CLTLoginMediator::ConnectionHelperFamily g_LoginHelperDispatchTableScaffold = {};

// Inline state objects - replaces static BuiltinScaffoldStates singleton pattern
// anchor: launcher.exe:0x43b300 / dispatch table seed
static CLTLoginState_State0_0x4b51e0 g_State0 = {};
static CLTLoginState_State1_0x4b4fc4 g_State1 = {};
static CLTLoginState_AuthenticatePending_0x4b5014 g_AuthenticatePending = {};
static CLTLoginState_State3_0x4b5208 g_State3 = {};
static CLTLoginState_State4_0x4b503c g_State4 = {};
static CLTLoginState_State5_0x4b5064 g_State5 = {};
static CLTLoginState_State6_0x4b508c g_State6 = {};
static CLTLoginState_State7_0x4b50b4 g_State7 = {};
static CLTLoginState_State8_0x4b5104 g_State8 = {};
static CLTLoginState_State9_0x4b517c g_State9 = {};
static CLTLoginState_State10_0x4b512c g_State10 = {};
static CLTLoginState_State11_0x4b5154 g_State11 = {};
static CLTLoginState_State12_0x4b5230 g_State12 = {};
static CLTLoginState_State13_0x4b50dc g_State13 = {};
static CLTLoginState_WorldListPending_0x4b4fec g_WorldListPending = {};
static CLTLoginState_State15_0x4b0b88 g_State15_0x4b0b88 = {};
static CLTLoginState_State16_0x4b0bb0 g_State16_0x4b0bb0 = {};
static CLTLoginState_State17_0x4b0bd8 g_State17_0x4b0bd8 = {};
static CLTLoginState_State18_0x4b0c00 g_State18 = {};
static CLTLoginState_State19_0x4b0c28 g_State19 = {};

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
    g_LoginHelperDispatchTableScaffold.helper78A4 = &g_State15_0x4b0b88;
    g_LoginHelperDispatchTableScaffold.helper78A8 = &g_State16_0x4b0bb0;
    g_LoginHelperDispatchTableScaffold.helper78AC = &g_State17_0x4b0bd8;
    g_LoginHelperDispatchTableScaffold.helper78B0 = &g_State18;
    g_LoginHelperDispatchTableScaffold.helper78B4 = &g_State19;
}

// Auth endpoint iteration - anchor: launcher.exe:0x41d1e8-0x41d232
// Faithful: original uses global directly without any host-change cache.
// The original anchor at launcher.exe:0x41b1e8..0x41b1ef directly reads g_qsAuthServerDNSName.
// (inline refresh happens in Initialize and BeginAuthConnection)
//     BuildMarginEndpoint();
//     if (!ResolvedMarginHostName().empty() && RebuildMarginAddressList() && SelectMarginEndpointIpv4()) {
//         BuildMarginEndpoint();
//     }
// }

// Faithful: use global directly (original doesn't have this helper - just uses StringTriple result)
std::string CLTLoginMediator::ResolvedMarginHostName() const {
    if (!marginRouteState_.exactMarginHostName.empty()) {
        return marginRouteState_.exactMarginHostName;
    }
    if (!marginRouteState_.routeHostPrefix.empty() && g_marginServerDNSName && g_marginServerDNSName[0]) {
        return marginRouteState_.routeHostPrefix + g_marginServerDNSName;
    }
    return std::string();
}



// anchor: launcher.exe:0x41b490
bool CLTLoginMediator::HasReadyAuthConnectionState2() const {
    return authConnection_ != nullptr &&
           authConnection_->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// anchor: launcher.exe:0x41d170
uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Address anchors:
    // - launcher.exe:0x41d170 = BeginAuthConnection implementation
    // - launcher.exe:0x4417e0 = CLTLoginMediator_Helper1_StartAuthConnection (ctor for auth connection)
    // - launcher.exe:0x440bb0 = CLTIPAddressList_GetNextAddress
    // - launcher.exe:0x44b090 = endpoint builder helper (port from 0x004f7a50, ipv4 from iterator)
    //
    // Call sequence:
    // 1. vtable+0x164 (RequestAuthConnectionCloseWaitEvent1)
    // 2. Allocation tracking via g_TrackedAllocationBytes/Count
    // 3. malloc(0xa8) for auth connection
    // 4. CLTLoginMediator_Helper1_StartAuthConnection(connection, engine)
    // 5. Set vtable to 0x4afef0
    // 6. Set owner context at offset 0xa4 to this
    // 7. CMessageConnection_ConfigurePacketNameCallback(connection, 1, 0x41ce00)
    // 8. Store connection at this+0x18
    // 9. Clear byte at this+0x2c
    // 10. Get next address from authAddressList at +0x4c
    // 11. Build endpoint at +0x5c using helper at 0x44b090 with port from DAT_004f7a50
    // 12. Increment counter at this+0x28
    // 13. Call vtable+0x1c (EnsureConnected) with endpoint at +0x5c

    // Call vtable slot 0x164 - close wait event before new connection
    // anchor: launcher.exe:0x41d17c / vtable+0x164
    RequestAuthConnectionCloseWaitEvent1();

    // Inline allocation and initialization - replaces EnsureAuthConnectionObject()
    // anchor: launcher.exe:0x41d1a9 / malloc(0xa8)
    // anchor: launcher.exe:0x41d1c0 / CLTLoginMediator_Helper1_StartAuthConnection
    // anchor: launcher.exe:0x41d1d1 / vftptr = 0x4afef0
    // anchor: launcher.exe:0x41d1d7 / owner = this at offset 0xa4
    // anchor: launcher.exe:0x41d1dd / ConfigurePacketNameFamily in original
    // Free existing connection first (like static RE - fresh allocation every time)
    if (authConnectionOwnedByMediator_ && authConnection_ != nullptr) {
        delete authConnection_;
    }
    authConnection_ = new mxo::liblttcp::CAuthStartupConnection_0x4afef0(engine_);
    if (!authConnection_) {
        return 0u;
    }
    authConnectionOwnedByMediator_ = true;
    authConnection_->SetOwnerContext(this);
    // Configure packet naming using high-level API (ConfigurePacketNameFamily handles
    // the callback pointer internally - original used ConfigurePacketNameCallback with
    // a binary-specific callback at launcher.exe:0x41ce00)
    authConnection_->ConfigurePacketNameFamily(
        mxo::liblttcp::CMessageConnectionPacketNameFamily::kAuth,
        true);

    auto* connection = authConnection_;
    if (!connection) {
        return 0u;
    }

    // Clear flag byte at +0x2c - anchor: launcher.exe:0x41d1ee
    // anchor: launcher.exe:0x41d1ee / byte ptr [ESI + 0x2c] = 0
    authConnectionFlag2c_ = 0u;

    // Inline: refresh address list and get next IPv4 - anchor: launcher.exe:0x41d1f2
    // anchor: launcher.exe:0x41d1f2 / call CLTIPAddressList_GetNextAddress(ESI+0x4c, 1)
    // First refresh the address list if empty - original directly uses global without cache
    if (authAddressList4c_.Empty()) {
        if (g_qsAuthServerDNSName != nullptr && g_qsAuthServerDNSName[0] != '\0') {
            uint32_t flags = mxo::liblttcp::CLTIPAddressList::kFlagShuffle;
            if (ignoreHostsFileForAuth_) {
                flags |= mxo::liblttcp::CLTIPAddressList::kFlagIgnoreHostsFile;
            }
            (void)authAddressList4c_.Reinit(g_qsAuthServerDNSName, flags);
        } else {
            authAddressList4c_.Reset();
        }
    }
    // Get next IPv4
    const uint32_t nextIpv4 = authAddressList4c_.GetNextAddress(/*wrap=*/true);
    if (nextIpv4 != 0u) {
        authEndpoint_.ipv4NetworkOrder = nextIpv4;
    }
    ++authConnectAttemptCount28_;

    // Build endpoint at +0x5c using helper at 0x44b090 - anchor: launcher.exe:0x41d205
    // The helper takes the next IPv4 and port from DAT_004f7a50
    // anchor: launcher.exe:0x41d1f7 / XOR ECX, ECX
    // anchor: launcher.exe:0x41d1f9 / MOV CX, word ptr [0x004f7a50] - port from global
    // anchor: launcher.exe:0x41d201 / PUSH EAX - next IPv4
    // anchor: launcher.exe:0x41d202 / LEA ECX, [EBP-0x14] - local endpoint
    // anchor: launcher.exe:0x41d205 / CALL 0x44b090
    // anchor: launcher.exe:0x41d20a-0x41d225 / copy endpoint fields to this+0x5c
    // Just ensure port is set correctly from config
    authEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((authServerPortHostOrder_ << 8) | (authServerPortHostOrder_ >> 8));

    // Set connection endpoint and call EnsureConnected - anchor: launcher.exe:0x41d228-0x41d232
    // anchor: launcher.exe:0x41d228 / MOV ECX, dword ptr [ESI+0x18] - load connection
    // anchor: 0x41d22f / MOV EAX, dword ptr [ECX] - get vtable
    // anchor: 0x41d231 / PUSH EDX - endpoint
    // anchor: 0x41d232 / CALL dword ptr [EAX+0x1c] - EnsureConnected
    connection->remoteEndpoint_ = authEndpoint_;
    connection->SetRemoteHostName(g_qsAuthServerDNSName);

    spdlog::info(
        "CLTLoginMediator::BeginAuthConnection host='{}' attemptCount28={} candidateCount={} selectedIpv4=0x{:08x} currentState={} authFlag2c={} -> EnsureConnected()",
        g_qsAuthServerDNSName ? g_qsAuthServerDNSName : "<empty>",
        static_cast<unsigned>(authConnectAttemptCount28_),
        static_cast<unsigned>(authAddressList4c_.Count()),
        static_cast<unsigned>(authEndpoint_.ipv4NetworkOrder),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(authConnectionFlag2c_));

    // Call EnsureConnected via vtable+0x1c - anchor: launcher.exe:0x41d232
    // anchor: launcher.exe:0x41d232 / call dword ptr [ECX + 0x1c]
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

    // Inline reset - original resets address list directly
    authAddressList4c_.Reset();
    authConnectAttemptCount28_ = 0;

    CLTLoginState* const upstreamState = currentState_;
    spdlog::info(
        "ROUTE CHECKPOINT: early-auth entering state1 auth-connect upstreamState={} currentStateBeforeSwitch={} resetAuthRetryState=1 attemptCount28={} candidateCount={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(authConnectAttemptCount28_),
        static_cast<unsigned>(authAddressList4c_.Count()));
    const uint32_t result = SetCurrentState(1u);
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
    const uint32_t result = SetCurrentState(4u);
    const std::string marginHost = ResolvedMarginHostName();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnectionViaState4Scaffold upstreamState={} currentState={} marginHost='{}' -> result=0x{:08x}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}

}  // namespace mxo::ltlogin
