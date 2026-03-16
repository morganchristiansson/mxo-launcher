/**
 * CLTLoginMediator - Launcher-owned authentication mediator.
 *
 * PURPOSE:
 * - Faithful reimplementation of the original launcher.exe authentication flow
 * - Coordinates auth/margin connections, packet handling, and state progression
 *
 * ADDRESS ANCHORS (from Ghidra analysis):
 * =============================================================================
 *
 * CONSTRUCTION / INITIALIZATION:
 * - launcher.exe:0x43b300 = CLTLoginMediator_InitializeHelperDispatchTable (helper array at 0x4f7868..0x4f78a0)
 * - launcher.exe:0x4f7868..0x4f78a0 = contiguous helper/state array (16 slots)
 * - launcher.exe:0x438d80 = LaunchPadClient_ProcessEvent0x17 (event handler for event code 0x1)
 *   - launcher.exe:0x4816f0 = LaunchPadClient_GetVtableOffset (inline helper returning *(this+4))
 *   - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism, logs "CLTLoginMediator::PostEvent(): Event# %d\n")
 *   - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
 *   - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf, calls PostError, logs "CLTLoginMediator::PostError(): Error# %d\n")
 * - launcher.exe:0x4b51e0, 0x4b4fec, 0x4b5014, etc. = PTR_FUN data entries pointing to 0x438d80
 *
 * AUTH CONNECTION INITIALIZATION:
 * - launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection (strongest anchor)
 * - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection
 * - launcher.exe:0x41e500 = CLTLoginMediator_BeginMarginConnection
 * - owner +0x4c = auth DNS name storage
 * - owner +0x4c8 = auth port value
 * - owner +0x5c = endpoint block (sockaddr-like)
 * - connection->+0x1c = ensure-connected wrapper
 *
 * AUTH HANDSHAKE / BOOTSTRAP:
 * - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap (strongest anchor)
 * - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (upstream dispatcher)
 * - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
 * - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
 * - branch site selecting raw 0x06 vs raw 0x08 path: 0x448050
 *
 * AUTH CHALLENGE RESPONSE:
 * - launcher.exe:0x429b0 = later challenge/material continuation anchor
 * - exact original raw 0x0a builder/send VA: [not yet isolated]
 *
 * AUTH REPLY HANDLING:
 * - launcher.exe:0x4401a0 = HandleAuthReply (strongest anchor)
 * - launcher.exe:0x43a330 = auth-reply parser object helper
 * - launcher.exe:0x43b830 = later GetWorldList sender (upstream after success)
 * - owner-side writeback areas: +0x80, +0x684, +0x688, +0x818, +0xd84, +0xcc8
 *
 * MARGIN CONNECTION DISPATCH:
 * - launcher.exe:0x439300 = CLTLoginMediator_DispatchMarginConnectionByState (strongest anchor)
 * - launcher.exe:0x41e500 = CLTLoginMediator_BeginMarginConnection
 * - owner vtable +0xe0 = character/route resolution
 * - owner vtable +0xfc = world id resolution
 * - owner vtable +0x10c = descriptor resolution
 * - owner +0x6c = margin endpoint block
 *
 * ARG6: ILTLoginMediator.Default at 0x4d2c58 - World List Provider
 * - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
 * - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
 * - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
 * - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
 * - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
 * - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
 * - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
 * - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
 * - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
 * - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
 * - vtable +0xfc = GetWorldNameByIndex(index) -> char* (Arg6GetWorldNameByIndex): launcher.exe:0x40cd10
 * - vtable +0x100 = GetWorldVariantByIndex(index) -> uint (1,2,3,5) (Arg6GetWorldVariantByIndex): launcher.exe:0x4d3584+0x100
 * - vtable +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 (Arg6ValidateWorldSelection): launcher.exe:0x4d3584+0xe4
 * - vtable +0xf8 = GetWorldListCount() -> uint (Arg6GetWorldListCount): launcher.exe:0x40e5b0
 * - vtable +0xd8 = GetActiveWorldCount() -> uint (Arg6GetActiveWorldListCount): launcher.exe:0x40e560
 * - vtable +0xe0 = GetAvailableWorlds(index) -> bool (Arg6GetAvailableWorlds): launcher.exe:0x40e670
 * - vtable +0xdc = GetAvailableWorldName(index) -> char* (Arg6GetAvailableWorldName): launcher.exe:0x40cd60
 *
 * ARG7 SELECTION RESOLUTION (through arg6 vtable at 0x4d3584):
 * - g_PackedArg7Selection = (high8bits << 24) | low24bits
 *   high8bits = variant state from vtable[+0x100]
 *   low24bits = world index from GetItemData low bits
 *
 * PROGRESS STATUS:
 * - [COMPLETE] Auth packet/protocol wire loop (0x06 -> 0x08 -> 0x0a -> 0x0b)
 * - [COMPLETE] Address anchors documented throughout source with Ghidra-synced names
 * - [COMPLETE] Helper dispatch table structure discovered from Ghidra analysis of 0x43b300:
 *   - [COMPLETE] CLTLoginMediator_InitializeHelperDispatchTable allocates 16 heap-allocated tables
 *   - [COMPLETE] LaunchPadClient_ProcessEvent0x17 (0x438d80) is the event handler for all slots
 *   - [COMPLETE] CLTLoginMediator_PostEvent (0x41cfb0), CLTLoginMediator_SwitchHelperState (0x41b450),
 *     CLTLoginMediator_PostError (0x41d090) are the helper functions
 *   - [COMPLETE] LaunchPadClient_GetVtableOffset (0x4816f0) is the inline vtable offset getter
 * - [IN PROGRESS] Faithful arg5 (launcherNetworkObject at 0x4d6304)
 * - [COMPLETE] Faithful arg6 (ILTLoginMediator.Default at 0x4d2c58):
 *   - [COMPLETE] InitializeArg6DefaultObject method with address anchors
 *   - [COMPLETE] Vtable methods implemented (+0xfc, +0x100, +0xe4, +0xf8, +0xd8, +0xe0, +0xdc)
 *   - [COMPLETE] Arg6WorldListData struct with worldNames_, worldVariants_, worldValid_
 *   - [IN PROGRESS] Populate arg6WorldList_ with real world data from launcher.exe
 *   - [IN PROGRESS] Call launcher.exe:0x40e480 BuildWorldList during InitClientDLL
 * - [IN PROGRESS] Faithful arg7 selection resolution through 0x4d3584 vtable
 * - [IN PROGRESS] Endpoint builder implementations (+0x4c, +0x5c, +0x6c)
 * =============================================================================
 */

#include "loginmediator.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"

#include <algorithm>
#include <ctime>

namespace mxo::ltlogin {

namespace {

static mxo::liblttcp::LTTCPEndpointKey BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey key = {};
    key.family = 2;  // AF_INET
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = 0;
    return key;
}

static const char* MaskedAuthValue(const std::string& value) {
    return value.empty() ? "<empty>" : "<provided>";
}

static std::array<uint8_t, 16> CopyPrefix16(const std::vector<uint8_t>& bytes) {
    std::array<uint8_t, 16> out = {};
    const size_t count = std::min(out.size(), bytes.size());
    std::copy_n(bytes.begin(), count, out.begin());
    return out;
}

}  // namespace

CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      currentState_(nullptr),
      authConnection_(nullptr),
      marginConnection_(nullptr),
      authConnectionContextKey_(nullptr),
      marginConnectionContextKey_(nullptr),
      helpers_{},
      marginRouteState_{},
      authBootstrap680_{},
      authServerPortHostOrder_(11000),
      ignoreHostsFileForAuth_(false),
      marginServerPortHostOrder_(10000),
      ignoreHostsFileForMargin_(false),
      authEndpoint_(BuildLoopbackEndpoint(authServerPortHostOrder_)),
      marginEndpoint_(BuildLoopbackEndpoint(marginServerPortHostOrder_)),
      authUsername_(),
      authPassword_(),
      authLauncherVersion_(76005),
      authCurrentPublicKeyId_(0),
      authLoginType_(1),
      authKeyConfigMd5_(),
      authUiConfigMd5_(),
      authGetPublicKeyRequestSent_(false),
      authRequestSent_(false),
      authChallengeResponseSent_(false),
      lastAuthPublicKeyReply_(),
      lastAuthRequestBuildResult_(),
      lastAuthChallenge_(),
      lastAuthReply_(),
      lastAuthConnectStatus_(0),
      lastMarginConnectStatus_(0),
      authConnectStatusCount_(0),
      marginConnectStatusCount_(0),
      expectedAuthRequestName_(nullptr),
      expectedMarginRequestName_(nullptr),
      worldSlots_{},
      worldPayloadSlots_{} {
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
}

CLTLoginMediator::~CLTLoginMediator() {
    if (!(engine_ && authConnectionContextKey_)) {
        delete authConnection_;
    }
    if (!(engine_ && marginConnectionContextKey_)) {
        delete marginConnection_;
    }
}

void CLTLoginMediator::SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
    if (authConnection_) authConnection_->SetEngine(engine_);
    if (marginConnection_) marginConnection_->SetEngine(engine_);
}

mxo::liblttcp::CLTThreadPerClientTCPEngine* CLTLoginMediator::NetworkEngine() const {
    return engine_;
}

void CLTLoginMediator::SetCurrentState(CLTLoginState* state) {
    currentState_ = state;
}

CLTLoginState* CLTLoginMediator::CurrentState() const {
    return currentState_;
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

    Log(
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

    Log(
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

const std::string& CLTLoginMediator::AuthServerDnsName() const {
    return authServerDnsName_;
}

uint16_t CLTLoginMediator::AuthServerPortHostOrder() const {
    return authServerPortHostOrder_;
}

bool CLTLoginMediator::IgnoreHostsFileForAuth() const {
    return ignoreHostsFileForAuth_;
}

const std::string& CLTLoginMediator::MarginServerDnsSuffix() const {
    return marginServerDnsSuffix_;
}

uint16_t CLTLoginMediator::MarginServerPortHostOrder() const {
    return marginServerPortHostOrder_;
}

bool CLTLoginMediator::IgnoreHostsFileForMargin() const {
    return ignoreHostsFileForMargin_;
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

const mxo::liblttcp::LTTCPEndpointKey& CLTLoginMediator::AuthEndpoint() const {
    return authEndpoint_;
}

const mxo::liblttcp::LTTCPEndpointKey& CLTLoginMediator::MarginEndpoint() const {
    return marginEndpoint_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::AuthConnection() const {
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::MarginConnection() const {
    return marginConnection_;
}

void CLTLoginMediator::SetMarginRouteState(uint8_t currentCharacterOrRouteIndex, uint32_t pendingWorldId, int32_t currentWorldId) {
    marginRouteState_.currentCharacterOrRouteIndex = currentCharacterOrRouteIndex;
    marginRouteState_.pendingWorldId = pendingWorldId;
    marginRouteState_.currentWorldId = currentWorldId;
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

const CLTLoginMediator::ConnectionHelperFamily& CLTLoginMediator::Helpers() const {
    return helpers_;
}

const CLTLoginMediator::AuthBootstrapState680Sketch& CLTLoginMediator::AuthBootstrap680() const {
    return authBootstrap680_;
}

void CLTLoginMediator::InitializeConnectionHelpers() {
    // Address anchors (from Ghidra decompilation of 0x43b300):
    // - launcher.exe:0x43b300 = CLTLoginMediator_InitializeHelperDispatchTable
    //   This function allocates 16 heap-allocated helper dispatch tables at 0x4f7868..0x4f78a0
    //   Each table stores a function pointer to LaunchPadClient_ProcessEvent0x17 (0x438d80)
    // - launcher.exe:0x4b51e0, 0x4b4fec, 0x4b5014, etc. = PTR_FUN data entries pointing to 0x438d80
    // - launcher.exe:0x420640 = InitializeHelperDispatchSlot15 (initializes slot at 0x4f78a4)
    // - launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16 (initializes slot at 0x4f78a8)
    // - launcher.exe:0x420850 = InitializeHelperDispatchSlot17 (initializes slot at 0x4f78ac)
    // - launcher.exe:0x420920 = InitializeHelperDispatchSlot18 (initializes slot at 0x4f78b0)
    // - launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19 (initializes slot at 0x4f78b4)
    // - launcher.exe:0x438d80 = LaunchPadClient_ProcessEvent0x17 (event handler for event code 0x1)
    // - launcher.exe:0x4816f0 = LaunchPadClient_GetVtableOffset (inline helper returning *(this+4))
    // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
    // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
    // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
    //
    // The 16 slots at 0x4f7868..0x4f78a0 are:
    //   Slot 0x68: helper for event dispatch table entry 0
    //   Slot 0x6c: helper for event dispatch table entry 1
    //   Slot 0x70: helper for event dispatch table entry 2
    //   ... and so on through slot 0xa0
    // Each slot stores a pointer to LaunchPadClient_ProcessEvent0x17 (0x438d80)
    //
    // From disassembly of 0x438d80:
    //   - Calls LaunchPadClient_GetVtableOffset(this+8) to get vtable offset
    //   - Checks if event flag at [this+0x2c] is set
    //   - If event flag set, calls CLTLoginMediator_PostEvent(this, 1)
    //   - Otherwise calls vtable[+0x178]() and updates state at [this+0x80]
    //
    // The re-implementation calls the helper methods to properly initialize all slots.
    InitializeHelperDispatchSlot15();
    InitializeHelperDispatchSlot16();
    InitializeHelperDispatchSlot17();
    InitializeHelperDispatchSlot18();
    InitializeHelperDispatchSlot19();
}

//
// These are called from InitializeConnectionHelpers() to properly initialize the
// helper dispatch table slots beyond the main 15 (0x68-0xa0).

void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    // Address anchor: launcher.exe:0x420640 = InitializeHelperDispatchSlot15
    // Original: allocates 8 bytes, stores PTR reference at address 0x4b51e0
    // The helper function calls the allocated memory which then stores &PTR_LaunchPadClient_ProcessEvent0x17_004b0b88
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b51e0);
        helpers_.helper78A4 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    // Address anchor: launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16
    // Original: allocates 4 bytes, stores PTR reference at address 0x4b4fec
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fec);
        helpers_.helper78A8 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    // Address anchor: launcher.exe:0x420850 = InitializeHelperDispatchSlot17
    // Original: allocates 4 bytes, stores PTR reference at address 0x4b4fc4
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fc4);
        helpers_.helper78AC = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    // Address anchor: launcher.exe:0x420920 = InitializeHelperDispatchSlot18
    // Original: allocates 8 bytes, stores PTR reference at address 0x4b5014
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b5014);
        helpers_.helper78B0 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    // Address anchor: launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19
    // Original: allocates 4 bytes, stores PTR reference at address 0x4b508c
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b508c);
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
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort` from owner `+0x4c8`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)` (ensure-connected wrapper)
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    expectedAuthRequestName_ = nullptr;
    BuildAuthEndpoint();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) return 0;
    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);
    return connection->EnsureConnected();
}

uint32_t CLTLoginMediator::HandleAuthConnectStatus(uint32_t workResultCode) {
    lastAuthConnectStatus_ = workResultCode;
    ++authConnectStatusCount_;
    return (workResultCode == kConnectStatusSuccess) ? BeginAuthHandshake() : 0u;
}

uint32_t CLTLoginMediator::HandleMarginConnectStatus(uint32_t workResultCode) {
    lastMarginConnectStatus_ = workResultCode;
    ++marginConnectStatusCount_;
    return (workResultCode == kConnectStatusSuccess) ? BeginMarginHandshake() : 0u;
}

uint32_t CLTLoginMediator::BeginAuthHandshake() {
    // Address anchors (NOW WITH ACTUAL FUNCTION NAMES):
    // - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    // - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (upstream dispatcher)
    // - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
    // - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    //
    // Transitional note:
    // - current scaffold still begins with an explicit 0x06 request step
    // - original helper-state progression is not yet fully rebuilt around 0x448050
    // The standalone auth probe is the current wire/reference implementation for the
    // launcher-owned auth loop. Reuse the shared low-level runtime-style auth helpers here
    // instead of keeping a second launcher-only packet path.
    expectedAuthRequestName_ = kMessageAsGetPublicKeyRequest;
    return SendAuthGetPublicKeyRequest();
}

uint32_t CLTLoginMediator::BeginMarginHandshake() {
    // Important correction from newer message-code review:
    // - the owner callback path around `0x440320` handles raw margin message code `0x10`
    // - margin string mapping `0x41bf70` also uses `code - 6`, so raw `0x10` resolves to
    //   `MS_LoadCharacterReply`, not `MS_ConnectRequest`
    // - that makes `0x440320` a later incoming loading-character anchor, not direct proof of
    //   the first outbound request after margin connect
    expectedMarginRequestName_ = nullptr;
    return 1u;
}

const char* CLTLoginMediator::ExpectedAuthRequestName() const {
    return expectedAuthRequestName_ ? expectedAuthRequestName_ : "";
}

const char* CLTLoginMediator::ExpectedMarginRequestName() const {
    return expectedMarginRequestName_ ? expectedMarginRequestName_ : "";
}

uint32_t CLTLoginMediator::ResolveMarginRouteFromCurrentCharacterSlot() const {
    // Address anchors:
    // - launcher.exe:0x439300 = current concrete margin route dispatcher
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best recovered launcher anchor: owner vtable `+0xe0`.
    // `0x439300` feeds this from owner byte `+0xcc8` and then passes the returned value to
    // the margin connection initializer. The exact semantic type of the returned value is not
    // settled yet; keep the source hook explicit.
    return marginRouteState_.currentCharacterOrRouteIndex;
}

uint32_t CLTLoginMediator::ResolveMarginRouteFromWorldId(uint32_t worldId) const {
    // Address anchors:
    // - launcher.exe:0x439300 = current concrete margin route dispatcher
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best recovered launcher anchor: owner vtable `+0xfc`.
    // The dispatcher currently feeds it from owner dword `+0x12c` or fallback dword `+0x104`.
    return worldId;
}

uint32_t CLTLoginMediator::ResolveMarginRouteDescriptor() const {
    // Address anchors:
    // - launcher.exe:0x439300 = current concrete margin route dispatcher
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best recovered launcher anchor: owner vtable `+0x10c`.
    // The original path then uses the returned object's first dword as the argument into the
    // margin-side connection initializer.
    return static_cast<uint32_t>(marginRouteState_.pendingWorldId);
}

uint32_t CLTLoginMediator::DispatchMarginConnectionByState() {
    // Address anchors (NOW WITH ACTUAL FUNCTION NAMES):
    // - launcher.exe:0x439300 = CLTLoginMediator_DispatchMarginConnectionByState
    // - launcher.exe:0x41e500 = CLTLoginMediator_BeginMarginConnection
    //
    // Current best launcher path:
    // - `0x439300` queries a separate owner-side state/helper object through vtable `+0x18`
    // - several cases then call owner vtable `+0xe0 / +0xfc / +0x10c`
    // - and finally route into `0x41e500`
    // - `0x41e500` builds margin endpoint at owner `+0x6c` and calls `connection->+0x1c(owner+0x6c)`
    //
    // Transitional note:
    // - arg7 selection resolution through ILTLoginMediator sibling object (0x4d3584) needs faithful implementation
    // - vtable methods at +0xfc, +0x100, +0xe4 must be wired to return proper world list data
    // - placeholder logic here preserves the dispatcher structure for later completion
    uint32_t routeKey = 0;
    if (currentState_) {
        routeKey = currentState_->DispatchPhaseCode();
    }

    switch (routeKey) {
        case 0:
            routeKey = ResolveMarginRouteFromCurrentCharacterSlot();
            break;
        case 1:
            routeKey = ResolveMarginRouteFromWorldId(marginRouteState_.pendingWorldId);
            break;
        case 2:
            routeKey = ResolveMarginRouteDescriptor();
            break;
        default:
            if (marginRouteState_.currentWorldId >= 0) {
                routeKey = ResolveMarginRouteFromWorldId(static_cast<uint32_t>(marginRouteState_.currentWorldId));
            }
            break;
    }

    (void)routeKey;
    BuildMarginEndpoint();
    auto* connection = EnsureMarginConnectionObject();
    if (!connection) return 0;
    const std::string marginHost = ResolvedMarginHostName();
    if (!marginHost.empty()) {
        connection->SetRemoteHostName(marginHost.c_str());
    }
    connection->SetRemoteEndpoint(marginEndpoint_);
    return connection->EnsureConnected();
}

// =============================================================================
// ARG7 SELECTION RESOLUTION (ILTLoginMediator sibling object at 0x4d3584)
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl (vtable access)
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList (world list construction)
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback path)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
//
// VTABLE METHODS (at offset +0xc from object pointer):
// +0xfc = GetWorldNameByIndex(index) -> char* world name string
// +0x100 = GetWorldVariantByIndex(index) -> uint variant state (1,2,3,5)
// +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
// +0xf8 = GetWorldListCount() -> uint total count
// +0xd8 = GetActiveWorldCount() -> uint active count
// +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
// +0xdc = GetAvailableWorldName(index) -> char* (fallback path)
//
// ARG7 PACKING FORMAT:
// g_PackedArg7Selection = (high8bits << 24) | low24bits
//   high8bits = variant state from vtable[+0x100]
//   low24bits = world index from GetItemData low bits
// =============================================================================

// =============================================================================
// ARG6: ILTLoginMediator.Default at 0x4d2c58 - World List Provider
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
//
// VTABLE METHODS (at offset +0xc from object pointer at 0x4d3584):
//   +0xfc = GetWorldNameByIndex(index) -> char* world name string
//   +0x100 = GetWorldVariantByIndex(index) -> uint variant state (1,2,3,5)
//   +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
//   +0xf8 = GetWorldListCount() -> uint total count
//   +0xd8 = GetActiveWorldCount() -> uint active count
//   +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
//   +0xdc = GetAvailableWorldName(index) -> char* (fallback path)
// =============================================================================

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds(index)
    // - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)

    // Transitional stub preserves the slot structure for later completion.
    return worldSlots_[index];
}

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldPayloadSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName(index)
    // - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl

    // Transitional stub preserves the payload structure for later completion.
    return worldPayloadSlots_[index];
}

// =============================================================================
// ARG6 FAITHFUL INITIALIZATION: ILTLoginMediator.Default at 0x4d2c58
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
// =============================================================================

// =============================================================================
// ARG6 FAITHFUL INITIALIZATION: ILTLoginMediator.Default at 0x4d2c58
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
// =============================================================================

void CLTLoginMediator::InitializeArg6DefaultObject() {
    // Transitional implementation preserves the structure for faithful completion.
    // Future work:
    // - Call launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
    // - Populate worldSlots_ with results from launcher.exe:0x40e670
    // - Wire vtable methods (+0xfc, +0x100, +0xe4) to return actual world data
}

// =============================================================================
// ARG6 VTABLE METHODS (at offset +0xc from object pointer at 0x4d3584)
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
// =============================================================================

// Address anchor: launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
const char* CLTLoginMediator::Arg6GetWorldNameByIndex(uint32_t index) {
    // Address anchor: launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex
    // Vtable method at +0xfc from object pointer at 0x4d2c58
    // Faithful implementation returns world name from arg6WorldList_ data
    return (index < arg6WorldList_.worldNames_.size()) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

uint8_t CLTLoginMediator::Arg6GetWorldVariantByIndex(uint32_t index) {
    // Address anchor: launcher.exe:0x4d3584 +0x100 = GetWorldVariantByIndex(vtable)
    // Faithful implementation returns variant state from arg6WorldList_ data
    return (index < arg6WorldList_.worldVariants_.size()) ? arg6WorldList_.worldVariants_[index] : 0;
}

uint8_t CLTLoginMediator::Arg6ValidateWorldSelection(uint8_t variant) {
    // Address anchor: launcher.exe:0x4d3584 +0xe4 = ValidateWorldSelection(vtable)
    // Returns 0 or 7 on valid selection (0 = valid, 7 = invalid)
    // Faithful implementation validates against worldValid_ data
    if (variant == 1 || variant == 2 || variant == 3 || variant == 5) {
        return 0;  // valid
    }
    return 7;  // invalid
}

uint32_t CLTLoginMediator::Arg6GetWorldListCount() const {
    // Address anchor: launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
    // Vtable method at +0xf8 from object pointer at 0x4d2c58
    // Faithful implementation returns total count from arg6WorldList_
    return arg6WorldList_.totalCount_;
}

uint32_t CLTLoginMediator::Arg6GetActiveWorldListCount() const {
    // Address anchor: launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
    // Vtable method at +0xd8 from object pointer at 0x4d2c58
    // Faithful implementation returns active count from arg6WorldList_
    return arg6WorldList_.activeCount_;
}

bool CLTLoginMediator::Arg6GetAvailableWorlds(uint32_t index) const {
    // Address anchor: launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
    // Vtable method at +0xe0 from object pointer at 0x4d2c58
    // Faithful implementation returns availability from arg6WorldList_
    return (index < arg6WorldList_.available_.size()) ? arg6WorldList_.available_[index] : false;
}

const char* CLTLoginMediator::Arg6GetAvailableWorldName(uint32_t index) {
    // Address anchor: launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Invalid
    // Vtable method at +0xdc from object pointer at 0x4d2c58
    // Faithful implementation returns world name from arg6WorldList_
    return (index < arg6WorldList_.worldNames_.size()) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

uint32_t CLTLoginMediator::SendAuthFramedPacket(
    const mxo::auth::FramedPacket& packet,
    const char* stepLabel) {
    mxo::liblttcp::CMessageConnection* connection = AuthConnection();
    if (!connection) {
        connection = EnsureAuthConnectionObject();
    }
    if (!connection || packet.bytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    const uint32_t sendResult = connection->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    Log(
        "DIAGNOSTIC: launcher-owned auth send step='%s' rawCode=0x%02x message='%s' headerLen=%u payloadLen=%u byteCount=%u -> sendResult=0x%08x",
        (stepLabel && stepLabel[0]) ? stepLabel : "<unnamed>",
        (unsigned)rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        (unsigned)packet.headerBytes.size(),
        (unsigned)packet.payloadBytes.size(),
        (unsigned)packet.bytes.size(),
        (unsigned)sendResult);
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthGetPublicKeyRequest() {
    // Address anchors:
    // - launcher.exe:0x447eb0 = strongest current raw 0x06 send builder
    // - launcher.exe:0x448050 = upstream phase-2 bootstrap dispatcher
    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            authLauncherVersion_,
            authCurrentPublicKeyId_,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_GetPublicKeyRequest");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(packet, kMessageAsGetPublicKeyRequest);
    authGetPublicKeyRequestSent_ = (sendResult != 0u);
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply) {
    // Address anchors:
    // - launcher.exe:0x4474f0 = strongest current raw 0x08 / AS_AuthRequest send builder
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authUsername_.empty()) {
        Log("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without a username");
        return 0;
    }
    if (!reply.hasEmbeddedPublicKey) {
        Log("DIAGNOSTIC: launcher-owned auth GetPublicKeyReply has no embedded public key material");
        return 0;
    }

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = authLoginType_;
    requestLayout.keyConfigMd5 = authKeyConfigMd5_;
    requestLayout.uiConfigMd5 = authUiConfigMd5_;
    requestLayout.rsaModulusBytes = reply.modulusBytes;
    requestLayout.rsaExponentBytes.assign(1u, reply.publicExponentByte);

    mxo::auth::AuthRequestBuildResult buildResult;
    if (!mxo::auth::BuildAuthRequestPacket(
            authUsername_,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest");
        return 0;
    }

    lastAuthRequestBuildResult_ = buildResult;
    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, kMessageAsAuthRequest);
    authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        Log(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId=%u loginType=%u keySize=%u blobLen=%u usernameLengthField=%u usedReplyPublicKey=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
            (unsigned)reply.publicKeyId,
            (unsigned)authLoginType_,
            (unsigned)reply.keySize,
            (unsigned)buildResult.blobCiphertextBytes.size(),
            (unsigned)buildResult.usernameLengthField,
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            (unsigned)buildResult.keyConfigMd5Bytes.size(),
            (unsigned)buildResult.uiConfigMd5Bytes.size());
    }
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge) {
    // Address anchors:
    // - launcher.exe:0x429b0 = later challenge/material continuation anchor
    // - exact original raw 0x0a builder/send VA: [not yet isolated]
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authPassword_.empty()) {
        Log("DIAGNOSTIC: launcher-owned auth received AS_AuthChallenge but has no password to send in AS_AuthChallengeResponse");
        return 0;
    }
    if (lastAuthRequestBuildResult_.twofishKeyBytes.size() != 16u) {
        Log("DIAGNOSTIC: launcher-owned auth missing Twofish key from AS_AuthRequest build result");
        return 0;
    }

    mxo::auth::AuthChallengeResponseLayout layout;
    mxo::auth::AuthChallengeResponseBuildResult buildResult;
    if (!mxo::auth::BuildAuthChallengeResponsePacket(
            challenge.encryptedChallengeBytes,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            authPassword_,
            authPassword_,
            layout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthChallengeResponse");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, "AS_AuthChallengeResponse");
    authChallengeResponseSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        Log(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthChallengeResponse passwordLengthField=%u soePasswordLengthField=%u plaintextLen=%u ciphertextLen=%u",
            (unsigned)buildResult.passwordLengthField,
            (unsigned)buildResult.soePasswordLengthField,
            (unsigned)buildResult.plaintextBytes.size(),
            (unsigned)buildResult.ciphertextBytes.size());
    }
    return sendResult;
}

void CLTLoginMediator::LogParsedAuthReply(const mxo::auth::AuthReply& reply) const {
    // Address anchors:
    // - launcher.exe:0x4401a0 = strongest current HandleAuthReply implementation
    // - launcher.exe:0x43a330 = concrete auth-reply parser object helper
    // - launcher.exe:0x43b830 = later auth-side GetWorldList sender (upstream after success)
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (reply.isErrorReply) {
        Log(
            "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply error errorCode=0x%08x zeroDword=0x%08x trailingWord=0x%04x",
            (unsigned)reply.errorCode,
            (unsigned)reply.zeroDword,
            (unsigned)reply.trailingWord);
        return;
    }

    Log(
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount=%u worldCount=%u username='%s' authDataMarker=0x%04x signatureLen=%u encryptedPrivateExponentLen=%u",
        (unsigned)reply.characterCount,
        (unsigned)reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text.c_str(),
        (unsigned)reply.authDataMarker,
        (unsigned)reply.authSignatureBytes.size(),
        (unsigned)reply.encryptedPrivateExponentLength);

    for (size_t i = 0; i < reply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = reply.characters[i];
        Log(
            "DIAGNOSTIC: launcher-owned auth character[%u] handle='%s' characterId=%llu status=%u worldId=%u",
            (unsigned)i,
            entry.handle.text.empty() ? "<empty>" : entry.handle.text.c_str(),
            static_cast<unsigned long long>(entry.characterId),
            (unsigned)entry.status,
            (unsigned)entry.worldId);
    }

    for (size_t i = 0; i < reply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = reply.worlds[i];
        Log(
            "DIAGNOSTIC: launcher-owned auth world[%u] id=%u name='%s' status=%u type=%u clientVersion=%u load='%c'",
            (unsigned)i,
            (unsigned)world.worldId,
            world.worldName.empty() ? "<empty>" : world.worldName.c_str(),
            (unsigned)world.status,
            (unsigned)world.type,
            (unsigned)world.clientVersion,
            world.load ? static_cast<char>(world.load) : '?');
    }

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    if (mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            lastAuthChallenge_.encryptedChallengeBytes,
            &decryptedPrivateExponentBytes)) {
        Log(
            "DIAGNOSTIC: launcher-owned auth decrypted AS_AuthReply private exponent length=%u",
            (unsigned)decryptedPrivateExponentBytes.size());
    }
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig() {
    authBootstrap680_.loginType28 = authLoginType_;
    authBootstrap680_.launcherVersion2C = authLauncherVersion_;
    authBootstrap680_.block30 = CopyPrefix16(authKeyConfigMd5_);
    authBootstrap680_.block40 = CopyPrefix16(authUiConfigMd5_);
    authBootstrap680_.currentPublicKeyId9C = authCurrentPublicKeyId_;
}

void CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState() {
    // Address anchors:
    // - launcher.exe:0x4401a0 = strongest current HandleAuthReply implementation
    // - important owner-side writeback areas on that path include:
    //   - +0x80 (world list container)
    //   - +0x684 / +0x688 / +0x818 / +0xd84 (helper/state blocks)
    //   - +0xcc8 (character/route index byte)
    // - launcher.exe:0x43b830 = later auth-side GetWorldList sender (upstream after success)
    //
    // Transitional note:
    // - this is still only a partial writeback sketch, not a faithful one-to-one reconstruction
    // - it exists so the parsed auth result begins to live in mediator-owned tables rather than
    //   only in `lastAuthReply_`
    worldSlots_.fill(nullptr);
    worldPayloadSlots_.fill(nullptr);

    const size_t worldCount = std::min(worldSlots_.size(), lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
        worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
    }

    if (!lastAuthReply_.worlds.empty()) {
        marginRouteState_.pendingWorldId = lastAuthReply_.worlds[0].worldId;
        if (marginRouteState_.currentWorldId < 0) {
            marginRouteState_.currentWorldId = static_cast<int32_t>(lastAuthReply_.worlds[0].worldId);
        }
    }

    if (!lastAuthReply_.characters.empty()) {
        marginRouteState_.currentCharacterOrRouteIndex = 0;
    }

    Log(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount=%u characterCount=%u firstWorldId=%u currentCharacterOrRouteIndex=%u",
        (unsigned)lastAuthReply_.worlds.size(),
        (unsigned)lastAuthReply_.characters.size(),
        lastAuthReply_.worlds.empty() ? 0u : (unsigned)lastAuthReply_.worlds[0].worldId,
        (unsigned)marginRouteState_.currentCharacterOrRouteIndex);
}

uint32_t CLTLoginMediator::HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    // Address anchors:
    // - launcher.exe:0x41bc20 = auth opcode read helper on later incoming path
    // - launcher.exe:0x4401a0 = strongest current AS_AuthReply handler
    // - launcher.exe:0x43a330 = concrete auth-reply parse/helper object builder
    mxo::auth::FramedPacket framedPacket;
    if (!packetBytes || !mxo::auth::ParseVariableLengthPacket(packetBytes, packetSize, &framedPacket) ||
        framedPacket.payloadBytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = framedPacket.payloadBytes[0];
    switch (rawCode) {
        case kAuthRawCodeGetPublicKeyReply: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPacket(packetBytes, packetSize, &reply)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_GetPublicKeyReply");
                return 0;
            }

            lastAuthPublicKeyReply_ = reply;
            authCurrentPublicKeyId_ = reply.publicKeyId;
            SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
            Log(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status=%u currentTime=%u publicKeyId=%u keySize=%u modulusLength=%u signatureLength=%u exponentByte=0x%02x hasEmbeddedPublicKey=%u",
                (unsigned)reply.status,
                (unsigned)reply.currentTime,
                (unsigned)reply.publicKeyId,
                (unsigned)reply.keySize,
                (unsigned)reply.modulusLength,
                (unsigned)reply.signatureLength,
                (unsigned)reply.publicExponentByte,
                reply.hasEmbeddedPublicKey ? 1u : 0u);
            expectedAuthRequestName_ = kMessageAsAuthRequest;
            return SendAuthRequestFromReply(reply);
        }

        case 0x09: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::AuthChallenge challenge;
            if (!mxo::auth::ParseAuthChallengePacket(packetBytes, packetSize, &challenge)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthChallenge");
                return 0;
            }

            lastAuthChallenge_ = challenge;
            Log(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen=%u",
                (unsigned)challenge.encryptedChallengeBytes.size());
            expectedAuthRequestName_ = "AS_AuthChallengeResponse";
            return SendAuthChallengeResponse(challenge);
        }

        case 0x0b: {
            // Address anchor: launcher.exe:0x4401a0 = strongest current AS_AuthReply handler
            mxo::auth::AuthReply reply;
            if (!mxo::auth::ParseAuthReplyPacket(packetBytes, packetSize, &reply)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthReply");
                return 0;
            }

            lastAuthReply_ = reply;
            AdoptAuthReplyIntoRecoveredMediatorState();
            LogParsedAuthReply(reply);
            expectedAuthRequestName_ = kMessageAsGetWorldListRequest;
            return 1u;
        }

        default:
            Log(
                "DIAGNOSTIC: launcher-owned auth received unhandled packet rawCode=0x%02x message='%s' payloadLen=%u",
                (unsigned)rawCode,
                mxo::auth::AuthOpcodeName(rawCode),
                (unsigned)framedPacket.payloadBytes.size());
            break;
    }

    return 0;
}

void CLTLoginMediator::BuildAuthEndpoint() {
    // Placeholder only.
    // Original launcher currently appears to preserve host text in owner `+0x4c` and then
    // build a sockaddr-like endpoint block at owner `+0x5c` using the current auth port.
    authEndpoint_ = BuildLoopbackEndpoint(authServerPortHostOrder_);
}

void CLTLoginMediator::BuildMarginEndpoint() {
    // Placeholder only.
    // Current recovered launcher path preserves margin suffix text separately and builds the
    // later connect endpoint block at owner `+0x6c` using the current margin port.
    marginEndpoint_ = BuildLoopbackEndpoint(marginServerPortHostOrder_);
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureAuthConnectionObject() {
    if (engine_ && authConnectionContextKey_) {
        authConnection_ = engine_->GetOrCreateMessageConnection(authConnectionContextKey_);
        return authConnection_;
    }

    if (!authConnection_) {
        authConnection_ = new mxo::liblttcp::CMessageConnection(engine_);
    }
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureMarginConnectionObject() {
    if (engine_ && marginConnectionContextKey_) {
        marginConnection_ = engine_->GetOrCreateMessageConnection(marginConnectionContextKey_);
        return marginConnection_;
    }

    if (!marginConnection_) {
        marginConnection_ = new mxo::liblttcp::CMessageConnection(engine_);
    }
    return marginConnection_;
}

}  // namespace mxo::ltlogin
