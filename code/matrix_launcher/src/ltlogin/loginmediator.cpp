#include "loginmediator.h"

#include "../diagnostics.h"
#include "loginstates.h"

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

void CLTLoginMediator::InitializeConnectionHelpers() {
    // Placeholder only.
    // Original launcher `0x43b300` initializes a contiguous helper/state array at
    // `0x4f7868 .. 0x4f78a0` immediately after `0x4f78b8 = esi`.
    // Keep the full recovered slot layout represented here so the source scaffold carries the
    // structure, even while most per-slot class names/behavior remain provisional.
    helpers_.helper7868 = reinterpret_cast<void*>(0x4f7868);
    helpers_.helper786C = reinterpret_cast<void*>(0x4f786c);
    helpers_.helper7870 = reinterpret_cast<void*>(0x4f7870);
    helpers_.helper7874 = reinterpret_cast<void*>(0x4f7874);
    helpers_.helper7878 = reinterpret_cast<void*>(0x4f7878);
    helpers_.helper787C = reinterpret_cast<void*>(0x4f787c);
    helpers_.helper7880 = reinterpret_cast<void*>(0x4f7880);
    helpers_.helper7884 = reinterpret_cast<void*>(0x4f7884);
    helpers_.helper7888 = reinterpret_cast<void*>(0x4f7888);
    helpers_.helper788C = reinterpret_cast<void*>(0x4f788c);
    helpers_.helper7890 = reinterpret_cast<void*>(0x4f7890);
    helpers_.helper7894 = reinterpret_cast<void*>(0x4f7894);
    helpers_.helper7898 = reinterpret_cast<void*>(0x4f7898);
    helpers_.helper789C = reinterpret_cast<void*>(0x4f789c);
    helpers_.helper78A0 = reinterpret_cast<void*>(0x4f78a0);
}

uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Current best launcher path:
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)`
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
    // The standalone auth probe is the current wire/reference implementation for the
    // launcher-owned auth loop. Reuse src/auth/auth_crypto.* here instead of keeping a second
    // launcher-only packet path.
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

uint32_t CLTLoginMediator::HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    mxo::auth::FramedPacket framedPacket;
    if (!packetBytes || !mxo::auth::ParseVariableLengthPacket(packetBytes, packetSize, &framedPacket) ||
        framedPacket.payloadBytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = framedPacket.payloadBytes[0];
    switch (rawCode) {
        case kAuthRawCodeGetPublicKeyReply: {
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPacket(packetBytes, packetSize, &reply)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_GetPublicKeyReply");
                return 0;
            }

            lastAuthPublicKeyReply_ = reply;
            authCurrentPublicKeyId_ = reply.publicKeyId;
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
            mxo::auth::AuthReply reply;
            if (!mxo::auth::ParseAuthReplyPacket(packetBytes, packetSize, &reply)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthReply");
                return 0;
            }

            lastAuthReply_ = reply;
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
