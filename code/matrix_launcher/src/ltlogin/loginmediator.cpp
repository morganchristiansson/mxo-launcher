#include "loginmediator.h"

#include "loginstates.h"

namespace mxo::ltlogin {

namespace {

static mxo::liblttcp::LTTCPEndpointKey BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey key = {};
    key.family = 2;  // AF_INET
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = 0;
    return key;
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
      authServerPortHostOrder_(11000),
      ignoreHostsFileForAuth_(false),
      marginServerPortHostOrder_(10000),
      ignoreHostsFileForMargin_(false),
      authEndpoint_(BuildLoopbackEndpoint(authServerPortHostOrder_)),
      marginEndpoint_(BuildLoopbackEndpoint(marginServerPortHostOrder_)),
      worldSlots_{},
      worldPayloadSlots_{} {}

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

void CLTLoginMediator::InitializeConnectionHelpers() {
    // Placeholder only.
    // Original launcher `0x43b300` initializes the helper/state family around
    // `0x4f7868 / 0x4f786c / 0x4f7870 / 0x4f78a0` immediately after `0x4f78b8 = esi`.
    // Keep those globals represented here so the source scaffold carries the structure.
    helpers_.helper7868 = reinterpret_cast<void*>(0x4f7868);
    helpers_.helper786C = reinterpret_cast<void*>(0x4f786c);
    helpers_.helper7870 = reinterpret_cast<void*>(0x4f7870);
    helpers_.helper78A0 = reinterpret_cast<void*>(0x4f78a0);
}

uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Current best launcher path:
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)`
    BuildAuthEndpoint();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) return 0;
    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);
    return connection->EnsureConnected();
}

uint32_t CLTLoginMediator::ResolveMarginRouteFromCurrentCharacterSlot() const {
    // Current best recovered launcher anchor: owner vtable `+0xe0`.
    // `0x439300` feeds this from owner byte `+0xcc8` and then passes the returned value to
    // the margin connection initializer. The exact semantic type of the returned value is not
    // settled yet; keep the source hook explicit.
    return marginRouteState_.currentCharacterOrRouteIndex;
}

uint32_t CLTLoginMediator::ResolveMarginRouteFromWorldId(uint32_t worldId) const {
    // Current best recovered launcher anchor: owner vtable `+0xfc`.
    // The dispatcher currently feeds it from owner dword `+0x12c` or fallback dword `+0x104`.
    return worldId;
}

uint32_t CLTLoginMediator::ResolveMarginRouteDescriptor() const {
    // Current best recovered launcher anchor: owner vtable `+0x10c`.
    // The original path then uses the returned object's first dword as the argument into the
    // margin-side connection initializer.
    return static_cast<uint32_t>(marginRouteState_.pendingWorldId);
}

uint32_t CLTLoginMediator::DispatchMarginConnectionByState() {
    // Current best launcher path:
    // - `0x439300` queries a separate owner-side state/helper object through vtable `+0x18`
    // - several cases then call owner vtable `+0xe0 / +0xfc / +0x10c`
    // - and finally route into `0x41e500`
    // - `0x41e500` builds margin endpoint at owner `+0x6c` and calls `connection->+0x1c(owner+0x6c)`
    //
    // This scaffold still does not claim the exact phase-code mapping, but it now preserves
    // the concrete route-resolution substeps in source instead of only in markdown.
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

void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    return (index < worldSlots_.size()) ? worldSlots_[index] : nullptr;
}

void* CLTLoginMediator::WorldPayloadSlot(uint32_t index) const {
    return (index < worldPayloadSlots_.size()) ? worldPayloadSlots_[index] : nullptr;
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
