/**
 * AuthBootstrap680 - launcher-owned phase-2 auth/bootstrap child rooted at mediator owner +0x680.
 *
 * Keep this TU focused on the separate bootstrap child/module that state2 slot 3 hands off into
 * through `0x439210 -> 0x448050`, plus the later auth-reply shadow fields surfaced back through
 * mediator wrappers.
 *
 * Important ownership split:
 * - this file intentionally models the owner `+0x680` bootstrap child as source-owned helper ops
 * - thin `CLTLoginMediator` wrappers remain elsewhere only so current callers/ABI do not churn
 * - do not treat this file as proof that the bootstrap child is literally the mediator class
 */

#include "loginmediator.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// UNANCHORED: source-owned sidecar storage for the owner `+0x680` bootstrap child mirrors that we
// do not want to inline into `CLTLoginMediator` layout yet.
struct AuthBootstrap680ChildSidecarState {
    AuthBootstrapReplyCopyShadowF4Sketch authReplyCopyShadowF4{};
    uint32_t raw08PublicKeyWorkerPresenceMarker = 0u;
    bool state2AuthReplySuccessOneTimeSideEffectsComplete = false;
    std::vector<uint8_t> opaqueReplyBlob108;
    std::vector<uint8_t> opaqueReplyBlob10C;
};

static std::unordered_map<const CLTLoginMediator*, std::unique_ptr<AuthBootstrap680ChildSidecarState>>
    g_authBootstrap680ChildSidecarByMediator;

static AuthBootstrap680ChildSidecarState* FindAuthBootstrap680ChildSidecar(const CLTLoginMediator* mediator) {
    const auto it = g_authBootstrap680ChildSidecarByMediator.find(mediator);
    return (it != g_authBootstrap680ChildSidecarByMediator.end() && it->second)
        ? it->second.get()
        : nullptr;
}

static AuthBootstrap680ChildSidecarState& MutableAuthBootstrap680ChildSidecar(const CLTLoginMediator* mediator) {
    std::unique_ptr<AuthBootstrap680ChildSidecarState>& slot =
        g_authBootstrap680ChildSidecarByMediator[mediator];
    if (!slot) {
        slot = std::make_unique<AuthBootstrap680ChildSidecarState>();
    }
    return *slot;
}

static std::array<uint8_t, 16> CopyPrefix16(const std::vector<uint8_t>& bytes) {
    std::array<uint8_t, 16> out = {};
    const size_t count = std::min(out.size(), bytes.size());
    std::copy_n(bytes.begin(), count, out.begin());
    return out;
}

static std::string BuildHexPreview(const void* bytes, size_t byteCount, size_t maxPreviewBytes) {
    if (!bytes || byteCount == 0u || maxPreviewBytes == 0u) {
        return "<empty>";
    }

    const uint8_t* p = static_cast<const uint8_t*>(bytes);
    const size_t previewCount = std::min(byteCount, maxPreviewBytes);
    std::string out;
    out.reserve(previewCount * 3u);
    static const char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < previewCount; ++i) {
        const uint8_t value = p[i];
        out.push_back(kHexDigits[(value >> 4) & 0x0fu]);
        out.push_back(kHexDigits[value & 0x0fu]);
        if (i + 1u != previewCount) {
            out.push_back(' ');
        }
    }
    return out;
}

static void ClearSmallStringMirror(AuthBootstrap680SmallStringMirror& mirror) {
    mirror.owned.clear();
    mirror.begin = nullptr;
    mirror.current = nullptr;
    mirror.capacity = nullptr;
}

static void AssignSmallStringMirror(
    AuthBootstrap680SmallStringMirror& mirror,
    const char* begin,
    const char* current) {
    ClearSmallStringMirror(mirror);
    if (!begin || !current || current <= begin) {
        return;
    }

    mirror.owned.assign(begin, current);
    mirror.begin = mirror.owned.c_str();
    mirror.current = mirror.begin + mirror.owned.size();
    mirror.capacity = mirror.current;
}

static size_t BoundedCStringLength(const char* text, size_t maxBytes) {
    if (!text || maxBytes == 0u) {
        return 0u;
    }

    size_t length = 0u;
    while (length < maxBytes && text[length] != '\0') {
        ++length;
    }
    return length;
}

static void AssignSmallStringMirror(AuthBootstrap680SmallStringMirror& mirror, const char* text) {
    if (!text) {
        ClearSmallStringMirror(mirror);
        return;
    }
    AssignSmallStringMirror(mirror, text, text + std::char_traits<char>::length(text));
}

static size_t SmallStringMirrorLength(const AuthBootstrap680SmallStringMirror& mirror) {
    return (mirror.begin && mirror.current && mirror.current >= mirror.begin)
        ? static_cast<size_t>(mirror.current - mirror.begin)
        : 0u;
}

static const char* SmallStringMirrorDataOrEmpty(const AuthBootstrap680SmallStringMirror& mirror) {
    return mirror.begin ? mirror.begin : "";
}

static void PointOpaqueBlobPointerAtOwnedBytes(void*& dstPointer, std::vector<uint8_t>& ownedBytes) {
    dstPointer = ownedBytes.empty() ? nullptr : ownedBytes.data();
}

static bool HasTrailingSlashSixDigitSuffix(const std::string& text) {
    const size_t slash = text.find('/');
    if (slash == std::string::npos || slash + 7u != text.size()) {
        return false;
    }

    for (size_t i = slash + 1u; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch < static_cast<unsigned char>('0') || ch > static_cast<unsigned char>('9')) {
            return false;
        }
    }
    return true;
}

struct AuthBootstrap680PrepareCallShape {
    const AuthBootstrapSelectedSource38Sketch* ownerSource94 = nullptr;
    const char* sourceString00 = nullptr;
    const char* sourceString20 = nullptr;
    const char* sourceString60Begin = nullptr;
    bool usedFallbackString00 = false;
    bool usedFallbackString20 = false;
    uint32_t write28 = 1u;
    uint32_t write2C = 0u;
    const std::array<uint8_t, 16>* sourceBlock40 = nullptr;
    const std::array<uint8_t, 16>* sourceBlock50 = nullptr;
    void* sendTarget50 = nullptr;
};

static AuthBootstrap680PrepareCallShape BuildAuthBootstrap680PrepareCallShape(
    const AuthBootstrapSelectedSource38Sketch& ownerSource94,
    const char* fallbackUsername,
    const char* fallbackPassword,
    uint32_t write2C,
    void* sendTarget50) {
    AuthBootstrap680PrepareCallShape callShape;
    callShape.ownerSource94 = &ownerSource94;
    // Preserve the recovered owner+0x94 -> child copy flow as the primary model, but keep the
    // current live state8 auto-begin path working when it reaches `0x448050` without an original
    // ProcessLoginRequest-populated owner+0x94 inline-string pair yet.
    callShape.sourceString00 = ownerSource94.inlineString00[0] ? ownerSource94.inlineString00.data() : fallbackUsername;
    callShape.sourceString20 = ownerSource94.inlineString20[0] ? ownerSource94.inlineString20.data() : fallbackPassword;
    callShape.sourceString60Begin = ownerSource94.string60.begin;
    callShape.usedFallbackString00 =
        (ownerSource94.inlineString00[0] == '\0') && fallbackUsername && fallbackUsername[0] != '\0';
    callShape.usedFallbackString20 =
        (ownerSource94.inlineString20[0] == '\0') && fallbackPassword && fallbackPassword[0] != '\0';
    callShape.write28 = 1u;
    // `0x439265` is now concrete enough to keep explicit: state2 calls the tiny owner getter at
    // vtable `+0x20` / `0x41f070`, which returns owner `+0x08`; the paired setter is
    // vtable `+0x1c` / `0x41f060` from the nopatch version-config path.
    callShape.write2C = write2C;
    callShape.sourceBlock40 = &ownerSource94.block40;
    callShape.sourceBlock50 = &ownerSource94.block50;
    callShape.sendTarget50 = sendTarget50;
    return callShape;
}

static void StageAuthBootstrap680ChildFromPrepareCallShape(
    AuthBootstrap680ChildSketch& child,
    const AuthBootstrap680PrepareCallShape& callShape) {
    AssignSmallStringMirror(
        child.string04,
        callShape.sourceString00,
        callShape.sourceString00 +
            BoundedCStringLength(
                callShape.sourceString00,
                callShape.ownerSource94 ? callShape.ownerSource94->inlineString00.size() : 0u));
    AssignSmallStringMirror(
        child.string10,
        callShape.sourceString20,
        callShape.sourceString20 +
            BoundedCStringLength(
                callShape.sourceString20,
                callShape.ownerSource94 ? callShape.ownerSource94->inlineString20.size() : 0u));
    AssignSmallStringMirror(
        child.string1C,
        callShape.sourceString60Begin ? callShape.sourceString60Begin : "");

    child.loginType28 = callShape.write28;
    child.launcherVersion2C = callShape.write2C;
    if (callShape.sourceBlock40) {
        child.block30 = *callShape.sourceBlock40;
    }
    if (callShape.sourceBlock50) {
        child.block40 = *callShape.sourceBlock50;
    }
    child.sendTarget50 = callShape.sendTarget50;
}

}  // namespace

// UNANCHORED: source-owned sidecar cleanup for the owner `+0x680` bootstrap child mirrors.
void AuthBootstrap680Ops::EraseSidecar(const CLTLoginMediator* mediator) {
    g_authBootstrap680ChildSidecarByMediator.erase(mediator);
}

// anchor: launcher.exe:0x448050
uint32_t AuthBootstrap680Ops::PrepareAndDispatch(CLTLoginMediator& mediator) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    if (mediator.authLoginType_ != 1u) {
        spdlog::warn(
            "AuthBootstrap680_PrepareAndDispatch current source config loginType={} differs from recovered 0x439210 immediate write28=1; ready-side staging still mirrors the original call shape",
            static_cast<unsigned>(mediator.authLoginType_));
    }

    void* sendTarget50 = mediator.AuthConnection();
    if (!sendTarget50) {
        sendTarget50 = mediator.EnsureAuthConnectionObject();
    }

    const uint32_t* recoveredLauncherVersionPtr = mediator.GetNoPatchLauncherVersionValuePtr08();
    const uint32_t recoveredLauncherVersion =
        (recoveredLauncherVersionPtr && *recoveredLauncherVersionPtr != 0u)
            ? *recoveredLauncherVersionPtr
            : mediator.authLauncherVersion_;

    const AuthBootstrap680PrepareCallShape callShape = BuildAuthBootstrap680PrepareCallShape(
        mediator.authBootstrapSource38_,
        mediator.authUsername_.c_str(),
        mediator.authPassword_.c_str(),
        recoveredLauncherVersion,
        sendTarget50);

    StageAuthBootstrap680ChildFromPrepareCallShape(child, callShape);
    child.currentPublicKeyId9C = mediator.authCurrentPublicKeyId_;

    const uint8_t authRequestReadyA0 = child.authRequestReadyA0;
    const bool sendAuthRequestBranch = authRequestReadyA0 != 0u;

    spdlog::info(
        "AuthBootstrap680Ops::PrepareAndDispatch staged owner+0x680 child (+0x04/+0x10/+0x1c/+0x28/+0x2c/+0x30..+0x4f/+0x50) from owner+0x94={} len04={} len10={} len1C={} fallback04={} fallback10={} write28={} write2C={} currentPublicKeyId9C={} sendTarget50={} authRequestReadyA0=0x{:02x} branch={}",
        fmt::ptr(callShape.ownerSource94),
        static_cast<unsigned>(SmallStringMirrorLength(child.string04)),
        static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
        static_cast<unsigned>(SmallStringMirrorLength(child.string1C)),
        callShape.usedFallbackString00 ? 1u : 0u,
        callShape.usedFallbackString20 ? 1u : 0u,
        static_cast<unsigned>(child.loginType28),
        static_cast<unsigned>(child.launcherVersion2C),
        static_cast<unsigned>(child.currentPublicKeyId9C),
        fmt::ptr(child.sendTarget50),
        static_cast<unsigned>(authRequestReadyA0),
        sendAuthRequestBranch ? "raw0x08/auth-request" : "raw0x06/get-public-key");

    if (sendAuthRequestBranch) {
        mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsAuthRequest;
        if (!mediator.lastAuthPublicKeyReply_.valid || !mediator.lastAuthPublicKeyReply_.hasEmbeddedPublicKey) {
            spdlog::warn(
                "AuthBootstrap680Ops::PrepareAndDispatch expected auth-request branch from owner+0x680 child but no valid cached AS_GetPublicKeyReply is present authRequestReadyA0=0x{:02x}",
                static_cast<unsigned>(authRequestReadyA0));
            return 0u;
        }
        return SendAuthRequestFromReply(mediator, mediator.lastAuthPublicKeyReply_);
    }

    mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsGetPublicKeyRequest;
    return SendAuthGetPublicKeyRequest(mediator);
}

// anchor: launcher.exe:0x448140
uint32_t AuthBootstrap680Ops::HandleInboundAuthMessage(CLTLoginMediator& mediator) {
    const std::vector<uint8_t>& stagedBytes = mediator.stagedIncomingAuthPacketBytes_;
    if (stagedBytes.empty()) {
        return kAuthBootstrap680InboundUnhandled;
    }

    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    const uint8_t rawCode = stagedBytes[0];
    switch (rawCode) {
        case CLTLoginMediator::kAuthRawCodeGetPublicKeyReply: {
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &reply)) {
                spdlog::warn("DIAGNOSTIC: AuthBootstrap680Ops::HandleInboundAuthMessage failed to parse AS_GetPublicKeyReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            mediator.lastAuthPublicKeyReply_ = reply;
            child.inboundAuthStatusEc = reply.status;
            if (reply.status != 0u) {
                return kAuthBootstrap680InboundGetPublicKeyReplyError;
            }

            mediator.authCurrentPublicKeyId_ = reply.publicKeyId;
            mediator.SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
            mediator.SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(reply);
            child.timestamp80 = static_cast<uint32_t>(
                std::time(nullptr) - static_cast<std::time_t>(reply.currentTime));

            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status={} currentTime={} publicKeyId={} keySize={} modulusLength={} signatureLength={} exponentByte=0x{:02x} hasEmbeddedPublicKey={}",
                static_cast<unsigned>(reply.status),
                static_cast<unsigned>(reply.currentTime),
                static_cast<unsigned>(reply.publicKeyId),
                static_cast<unsigned>(reply.keySize),
                static_cast<unsigned>(reply.modulusLength),
                static_cast<unsigned>(reply.signatureLength),
                static_cast<unsigned>(reply.publicExponentByte),
                reply.hasEmbeddedPublicKey ? 1u : 0u);

            mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsAuthRequest;
            return SendAuthRequestFromReply(mediator, reply) != 0u
                ? kAuthBootstrap680InboundHandledContinueWaiting
                : kAuthBootstrap680InboundGetPublicKeyWorkerError;
        }

        case 0x09u: {
            mxo::auth::AuthChallenge challenge;
            if (!mxo::auth::ParseAuthChallengePayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &challenge)) {
                spdlog::warn("DIAGNOSTIC: AuthBootstrap680Ops::HandleInboundAuthMessage failed to parse AS_AuthChallenge");
                return kAuthBootstrap680InboundUnhandled;
            }

            mediator.lastAuthChallenge_ = challenge;
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen={}",
                challenge.encryptedChallengeBytes.size());
            mediator.expectedAuthRequestName_ = "AS_AuthChallengeResponse";
            return SendAuthChallengeResponse(mediator, challenge) != 0u
                ? kAuthBootstrap680InboundHandledContinueWaiting
                : kAuthBootstrap680InboundUnhandled;
        }

        case 0x0bu: {
            mxo::auth::AuthReply reply;
            if (!mxo::auth::ParseAuthReplyPayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &reply)) {
                spdlog::warn("DIAGNOSTIC: AuthBootstrap680Ops::HandleInboundAuthMessage failed to parse AS_AuthReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            mediator.lastAuthReply_ = reply;
            mediator.expectedAuthRequestName_ = nullptr;
            child.inboundAuthStatusEc = reply.isErrorReply ? reply.errorCode : 0u;

            if (reply.isErrorReply) {
                return kAuthBootstrap680InboundAuthReplyError;
            }

            // Current source-owned validation is narrower than the original `0x44aec0` path:
            // keep the marker/worker gate explicit while the full child `+0xac` validation family
            // is still not typed tightly enough.
            if (!reply.valid || !reply.hasAuthDataMarker || reply.authDataMarker != 0x0136u ||
                child.raw08PublicKeyWorkerA8 == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            mediator.SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(reply);
            return kAuthBootstrap680InboundAuthReplySuccess;
        }

        default:
            break;
    }

    return kAuthBootstrap680InboundUnhandled;
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
void* AuthBootstrap680Ops::BootstrapRaw08AuxHandle50(const CLTLoginMediator& mediator) {
    const auto* authReplyCopyShadowF4 =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(
            mediator.authBootstrapChild680_.authReplyCopyShadowF4);
    void* value = authReplyCopyShadowF4 ? authReplyCopyShadowF4->raw08PublicKeyWorkerA8 : nullptr;

    if (!mediator.bootstrapRaw08AuxHandle50Logged_ || mediator.lastBootstrapRaw08AuxHandle50_ != value) {
        spdlog::info(
            "CLTLoginMediator::BootstrapRaw08AuxHandle50(+0x50) -> {}{}",
            fmt::ptr(value),
            mediator.bootstrapRaw08AuxHandle50Logged_ ? " [changed]" : " [first]");
        mediator.bootstrapRaw08AuxHandle50Logged_ = true;
        mediator.lastBootstrapRaw08AuxHandle50_ = value;
    }

    return value;
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
bool AuthBootstrap680Ops::HasBootstrapRaw08AuxHandle54(const CLTLoginMediator& mediator) {
    const auto* authReplyCopyShadowF4 =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(
            mediator.authBootstrapChild680_.authReplyCopyShadowF4);
    const bool present =
        authReplyCopyShadowF4 && authReplyCopyShadowF4->raw08PublicKeyWorkerA8 != nullptr;
    spdlog::debug(
        "CLTLoginMediator::HasBootstrapRaw08AuxHandle54(+0x54) -> {}",
        present ? 1u : 0u);
    return present;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t AuthBootstrap680Ops::GetCrashReporterPromptForSecurId58(const CLTLoginMediator& mediator) {
    const uint8_t prompt = mediator.authBootstrapChild680_.crashReporterPromptForSecurId104;
    spdlog::debug(
        "CLTLoginMediator::GetCrashReporterPromptForSecurId58(+0x58) -> {}",
        static_cast<unsigned>(prompt));
    return prompt;
}

// anchor: launcher.exe:0x447eb0
uint32_t AuthBootstrap680Ops::SendAuthGetPublicKeyRequest(CLTLoginMediator& mediator) {
    const AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;

    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            child.launcherVersion2C,
            child.currentPublicKeyId9C,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_GetPublicKeyRequest");
        return 0;
    }

    const uint32_t sendResult =
        mediator.SendAuthFramedPacket(packet, CLTLoginMediator::kMessageAsGetPublicKeyRequest);
    mediator.authGetPublicKeyRequestSent_ = (sendResult != 0u);
    return sendResult;
}

// anchor: launcher.exe:0x4474f0
uint32_t AuthBootstrap680Ops::SendAuthRequestFromReply(
    CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    if (!reply.hasEmbeddedPublicKey) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth GetPublicKeyReply has no embedded public key material");
        return 0;
    }

    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    const char* username = SmallStringMirrorDataOrEmpty(child.string04);
    if (SmallStringMirrorLength(child.string04) == 0u) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without child+0x04 username data");
        return 0;
    }

    child.currentPublicKeyId9C = reply.publicKeyId;

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = static_cast<uint8_t>(child.loginType28 & 0xffu);
    requestLayout.keyConfigMd5.assign(child.block30.begin(), child.block30.end());
    requestLayout.uiConfigMd5.assign(child.block40.begin(), child.block40.end());
    requestLayout.rsaModulusBytes = reply.modulusBytes;
    requestLayout.rsaExponentBytes.assign(1u, reply.publicExponentByte);

    mxo::auth::AuthRequestBuildResult buildResult;
    if (!mxo::auth::BuildAuthRequestPacket(
            username,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest from child+0x04/+0x28/+0x30..+0x4f state");
        return 0;
    }

    mediator.lastAuthRequestBuildResult_ = buildResult;
    const uint32_t sendResult =
        mediator.SendAuthFramedPacket(buildResult.packet, CLTLoginMediator::kMessageAsAuthRequest);
    mediator.authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId={} loginType={} keySize={} blobLen={} usernameLengthField={} usedReplyPublicKey={} keyConfigMd5Len={} uiConfigMd5Len={} childSendTarget50={} childRaw08PublicKeyWorkerA8={} childString04Len={} childString10Len={} childString1CLen={}",
            static_cast<unsigned>(reply.publicKeyId),
            static_cast<unsigned>(requestLayout.loginType),
            static_cast<unsigned>(reply.keySize),
            static_cast<unsigned>(buildResult.blobCiphertextBytes.size()),
            static_cast<unsigned>(buildResult.usernameLengthField),
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            static_cast<unsigned>(buildResult.keyConfigMd5Bytes.size()),
            static_cast<unsigned>(buildResult.uiConfigMd5Bytes.size()),
            fmt::ptr(child.sendTarget50),
            fmt::ptr(child.raw08PublicKeyWorkerA8),
            static_cast<unsigned>(SmallStringMirrorLength(child.string04)),
            static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
            static_cast<unsigned>(SmallStringMirrorLength(child.string1C)));
    }
    return sendResult;
}

// UNANCHORED: source-owned current raw `0x0a` builder/send bridge; exact original send VA is not
// yet isolated even though the surrounding challenge/material continuation is anchored later.
uint32_t AuthBootstrap680Ops::SendAuthChallengeResponse(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthChallenge& challenge) {
    if (mediator.authPassword_.empty()) {
        spdlog::error(
            "launcher-owned auth received AS_AuthChallenge but has no password to send in AS_AuthChallengeResponse");
        return 0;
    }
    if (mediator.lastAuthRequestBuildResult_.twofishKeyBytes.size() != 16u) {
        spdlog::error("launcher-owned auth missing Twofish key from AS_AuthRequest build result");
        return 0;
    }

    mxo::auth::AuthChallengeResponseLayout layout;
    mxo::auth::AuthChallengeResponseBuildResult buildResult;
    if (!mxo::auth::BuildAuthChallengeResponsePacket(
            challenge.encryptedChallengeBytes,
            mediator.lastAuthRequestBuildResult_.twofishKeyBytes,
            mediator.authPassword_,
            mediator.authPassword_,
            layout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::error("launcher-owned auth failed to build AS_AuthChallengeResponse");
        return 0;
    }

    const uint32_t sendResult =
        mediator.SendAuthFramedPacket(buildResult.packet, "AS_AuthChallengeResponse");
    mediator.authChallengeResponseSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(mediator, buildResult);
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthChallengeResponse passwordLengthField={} soePasswordLengthField={} plaintextLen={} ciphertextLen={}",
            (unsigned)buildResult.passwordLengthField,
            (unsigned)buildResult.soePasswordLengthField,
            (unsigned)buildResult.plaintextBytes.size(),
            (unsigned)buildResult.ciphertextBytes.size());
    }
    return sendResult;
}

// UNANCHORED: source-owned parsed-auth logging helper around the
// `0x4401a0 / State10AuthReplyParseObject_InitFromIncomingPacket` family.
void AuthBootstrap680Ops::LogParsedAuthReply(
    const CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    if (reply.isErrorReply) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply error errorCode=0x{:08x} zeroDword=0x{:08x} trailingWord=0x{:04x}",
            reply.errorCode,
            reply.zeroDword,
            reply.trailingWord);
        return;
    }

    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount={} worldCount={} username='{}' successHeaderUnknownWord05=0x{:04x} successHeaderUnknownDword07=0x{:08x} unknown2=0x{:08x} unknown3=0x{:08x} authDataMarker=0x{:04x} signatureLen={} encryptedPrivateExponentLen={}",
        reply.characterCount,
        reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text,
        static_cast<unsigned>(reply.successHeaderUnknownWord05),
        static_cast<unsigned>(reply.successHeaderUnknownDword07),
        static_cast<unsigned>(reply.unknown2),
        static_cast<unsigned>(reply.unknown3),
        reply.authDataMarker,
        reply.authSignatureBytes.size(),
        reply.encryptedPrivateExponentLength);

    for (size_t i = 0; i < reply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = reply.characters[i];
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth character[{}] handle='{}' characterId={} status={} worldId={}",
            static_cast<unsigned>(i),
            entry.handle.text.empty() ? "<empty>" : entry.handle.text.c_str(),
            static_cast<unsigned long long>(entry.characterId),
            static_cast<unsigned>(entry.status),
            static_cast<unsigned>(entry.worldId));
    }

    for (size_t i = 0; i < reply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = reply.worlds[i];
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth world[{}] id={} name='{}' status={} type={} clientVersion={} load='{}'",
            static_cast<unsigned>(i),
            static_cast<unsigned>(world.worldId),
            world.worldName.empty() ? "<empty>" : world.worldName.c_str(),
            static_cast<unsigned>(world.status),
            static_cast<unsigned>(world.type),
            static_cast<unsigned>(world.clientVersion),
            world.load ? static_cast<char>(world.load) : '?');
    }

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    if (mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            mediator.lastAuthRequestBuildResult_.twofishKeyBytes,
            mediator.lastAuthChallenge_.encryptedChallengeBytes,
            &decryptedPrivateExponentBytes)) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth decrypted AS_AuthReply private exponent length={}",
            static_cast<unsigned>(decryptedPrivateExponentBytes.size()));
    }
}

// UNANCHORED: source-owned fixed-field preseed for the owner `+0x680` bootstrap child.
// The ready-side `0x439210 -> 0x448050` path still rewrites child `+0x28/+0x2c` from its own
// recovered call shape before raw `0x06` / raw `0x08` dispatch.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig(CLTLoginMediator& mediator) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.loginType28 = mediator.authLoginType_;
    const uint32_t* launcherVersionPtr = mediator.GetNoPatchLauncherVersionValuePtr08();
    child.launcherVersion2C = (launcherVersionPtr && *launcherVersionPtr != 0u)
        ? *launcherVersionPtr
        : mediator.authLauncherVersion_;
    child.currentPublicKeyId9C = mediator.authCurrentPublicKeyId_;
}

// UNANCHORED: source-owned dynamic-state reset for the owner `+0x680` bootstrap child.
void AuthBootstrap680Ops::ResetRecoveredAuthBootstrapDynamicStateScaffold(CLTLoginMediator& mediator) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.timestamp80 = 0u;
    child.sendTarget50 = nullptr;
    std::fill(child.challengeMaterial85.begin(), child.challengeMaterial85.end(), 0u);
    child.feedbackTransformLarge94 = nullptr;
    child.feedbackTransformSmall98 = nullptr;
    child.authRequestReadyA0 = 0u;
    child.paddingA1ToA3 = {};
    child.lazyPubkeyDatStateA4 = nullptr;
    child.raw08PublicKeyWorkerA8 = nullptr;
    child.fieldAC = nullptr;
    child.fieldF0 = nullptr;
    child.authReplyCopyShadowF4 = nullptr;
    ClearSmallStringMirror(child.stringF8);
    child.fieldFC = nullptr;
    child.field100 = nullptr;
    child.opaqueReplyBlob108 = nullptr;
    child.opaqueReplyBlob10C = nullptr;
    child.field110 = 0u;
    child.field114 = 0u;
    child.field118 = 0u;
    EraseSidecar(&mediator);
}

// UNANCHORED: source-owned owner+0x680 update after parsed `AS_GetPublicKeyReply`.
// Current source sets the original `+0xa0` ready byte directly and uses one sidecar-backed non-zero
// marker as a stand-in for the still-untyped original `+0xa8/+0xac` worker family.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.currentPublicKeyId9C = reply.publicKeyId;
    child.authRequestReadyA0 = 1u;

    AuthBootstrap680ChildSidecarState& sidecar = MutableAuthBootstrap680ChildSidecar(&mediator);
    sidecar.raw08PublicKeyWorkerPresenceMarker =
        (reply.publicKeyId != 0u) ? reply.publicKeyId : 1u;
    child.raw08PublicKeyWorkerA8 = &sidecar.raw08PublicKeyWorkerPresenceMarker;
    child.fieldAC = &sidecar.raw08PublicKeyWorkerPresenceMarker;
}

// UNANCHORED: source-owned owner+0x680 challenge-material update after raw `0x0a` build/send.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthChallengeResponseBuildResult& buildResult) {
    mediator.authBootstrapChild680_.challengeMaterial85 =
        CopyPrefix16(buildResult.decryptedChallengeBytes);
}

// UNANCHORED: source-owned owner+0x680 auth-reply copy-shadow update for later `+0x50/+0x5c`
// exposure. Original child `+0xf4` points at a reply-derived copied `0x136` block; current source
// keeps only the narrower exposed `+0x85/+0xa8` suffix family shadow there.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.authReplyCopyShadowF4 = nullptr;

    AuthBootstrap680ChildSidecarState* sidecar = FindAuthBootstrap680ChildSidecar(&mediator);
    if (sidecar) {
        sidecar->authReplyCopyShadowF4 = {};
    }

    if (reply.isErrorReply || !reply.valid || !reply.hasAuthDataMarker ||
        reply.authDataMarker != 0x0136u || child.raw08PublicKeyWorkerA8 == nullptr) {
        return;
    }

    AuthBootstrap680ChildSidecarState& materializedSidecar =
        MutableAuthBootstrap680ChildSidecar(&mediator);
    materializedSidecar.authReplyCopyShadowF4.material85 = child.challengeMaterial85;
    materializedSidecar.authReplyCopyShadowF4.raw08PublicKeyWorkerA8 = child.raw08PublicKeyWorkerA8;
    child.authReplyCopyShadowF4 = &materializedSidecar.authReplyCopyShadowF4;

    spdlog::info(
        "CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold challengeMaterial85='{}' raw08PublicKeyWorker={} authDataMarker=0x{:04x} childStringF8Begin={} childStringF8Len={}",
        BuildHexPreview(
            materializedSidecar.authReplyCopyShadowF4.material85.data(),
            materializedSidecar.authReplyCopyShadowF4.material85.size(),
            materializedSidecar.authReplyCopyShadowF4.material85.size()),
        fmt::ptr(materializedSidecar.authReplyCopyShadowF4.raw08PublicKeyWorkerA8),
        static_cast<unsigned>(reply.authDataMarker),
        fmt::ptr(SmallStringMirrorDataOrEmpty(child.stringF8)),
        static_cast<unsigned>(SmallStringMirrorLength(child.stringF8)));
}

// UNANCHORED: source-owned narrower mirror of the pre-gate `0x43f300` neighboring helper call
// `0x441330`, plus the direct child `+0x110` write that occurs immediately before it.
// Current source keeps only the parts now tight enough to model confidently:
// - copy owner `+0x94 + 0x20` into child `+0xf8`
// - set child `+0x104` from the original slash+6-digit SecurID-tail test and strip that tail
//   from child `+0xf8` when present
// - mirror child `+0x110` from the parsed success-header dword at payload offset `0x07`
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessPregateScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;

    std::string promptPassword = mediator.authBootstrapSource38_.inlineString20.data();
    const bool promptForSecurId = HasTrailingSlashSixDigitSuffix(promptPassword);
    if (promptForSecurId && promptPassword.size() >= 7u) {
        promptPassword.resize(promptPassword.size() - 7u);
    }
    AssignSmallStringMirror(child.stringF8, promptPassword.c_str());
    child.crashReporterPromptForSecurId104 = promptForSecurId ? 1u : 0u;
    child.field110 = reply.successHeaderUnknownDword07;

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessPregateScaffold childStringF8Len={} promptForSecurId={} childField110=0x{:08x}",
        static_cast<unsigned>(SmallStringMirrorLength(child.stringF8)),
        static_cast<unsigned>(child.crashReporterPromptForSecurId104),
        static_cast<unsigned>(child.field110));
}

// UNANCHORED: source-owned one-time gate mirror for the `DAT_004f79e0` success-side block inside
// `0x43f300`.
bool AuthBootstrap680Ops::ConsumeState2AuthReplySuccessOneTimeGateScaffold(CLTLoginMediator& mediator) {
    AuthBootstrap680ChildSidecarState& sidecar = MutableAuthBootstrap680ChildSidecar(&mediator);
    if (sidecar.state2AuthReplySuccessOneTimeSideEffectsComplete) {
        return false;
    }
    sidecar.state2AuthReplySuccessOneTimeSideEffectsComplete = true;
    return true;
}

// UNANCHORED: source-owned narrower mirror of the gated neighboring `0x43f300` success-side
// helper subset after the world/character arrays are built.
// Current source now keeps these gated consequences explicit:
// - `0x441260 = AuthBootstrap680_StoreField114AndTimestamp118`
// - owner vtable `+0x150` fed from `0x43d480 = AuthBootstrap680_CopyReplyString54`
// - `0x441170 = AuthBootstrap680_CopyOpaqueReplyBlobs108_10c`
// The exact semantics of the two copied blob families are still only provisionally mapped in
// source to the parsed auth-signature and encrypted-private-exponent byte vectors.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    AuthBootstrap680ChildSidecarState& sidecar = MutableAuthBootstrap680ChildSidecar(&mediator);

    child.field114 = reply.unknown3;
    child.field118 = static_cast<uint32_t>(std::time(nullptr));

    if (!reply.username.text.empty()) {
        mediator.SetLaunchPadSourceBlock94FirstString(reply.username.text.c_str());
    }

    sidecar.opaqueReplyBlob108 = reply.authSignatureBytes;
    sidecar.opaqueReplyBlob10C = reply.encryptedPrivateExponentBytes;
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob108, sidecar.opaqueReplyBlob108);
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob10C, sidecar.opaqueReplyBlob10C);

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold childField114=0x{:08x} childField118=0x{:08x} ownerSource94FirstString='{}' opaqueBlob108Len={} opaqueBlob10CLen={} opaqueBlob108={} opaqueBlob10C={}",
        static_cast<unsigned>(child.field114),
        static_cast<unsigned>(child.field118),
        mediator.authBootstrapSource38_.inlineString00[0] != '\0'
            ? mediator.authBootstrapSource38_.inlineString00.data()
            : "<empty>",
        static_cast<unsigned>(sidecar.opaqueReplyBlob108.size()),
        static_cast<unsigned>(sidecar.opaqueReplyBlob10C.size()),
        fmt::ptr(child.opaqueReplyBlob108),
        fmt::ptr(child.opaqueReplyBlob10C));
}

}  // namespace mxo::ltlogin
