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

// anchor: launcher.exe:DAT_004f79e0
static bool g_authBootstrap680State2AuthReplySuccessOneTimeGate = false;

// Keep non-layout ownership outside `AuthBootstrap680ChildSketch` so the child mirror can stay
// faithful to launcher field boundaries while source still owns heap-backed helper payloads.
struct AuthBootstrap680ChildOwnedState {
    uint32_t raw08PublicKeyWorkerPresenceMarker = 0u;
    uint32_t replyAuthDataValidatorPresenceMarker = 0u;
    std::unique_ptr<AuthBootstrapReplyCopyShadowF4Sketch> authReplyCopyShadowF4;
    std::vector<uint32_t> modulusBigIntB0OwnedDigits;
    std::vector<uint32_t> publicExponentBigIntC4OwnedDigits;
    std::vector<uint32_t> privateExponentBigIntD8OwnedDigits;
    std::vector<uint8_t> opaqueReplyBlob108Owned;
    std::vector<uint8_t> opaqueReplyBlob10COwned;
};

static std::unordered_map<const CLTLoginMediator*, AuthBootstrap680ChildOwnedState>
    g_authBootstrap680ChildOwnedStateByMediator;

static AuthBootstrap680ChildOwnedState& MutableAuthBootstrap680ChildOwnedState(
    const CLTLoginMediator* mediator) {
    return g_authBootstrap680ChildOwnedStateByMediator[mediator];
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

static uint32_t ReadU32LE(const uint8_t* bytes) {
    if (!bytes) {
        return 0u;
    }
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) |
           (static_cast<uint32_t>(bytes[3]) << 24u);
}

static constexpr uint32_t kAuthBootstrap680BigIntCapacityTable[9] = {
    2u, 2u, 2u, 4u, 4u, 8u, 8u, 8u, 8u,
};

static uint32_t RoundAuthBootstrap680BigIntCapacityWords(size_t requiredWordCount) {
    if (requiredWordCount < std::size(kAuthBootstrap680BigIntCapacityTable)) {
        return kAuthBootstrap680BigIntCapacityTable[requiredWordCount];
    }
    if (requiredWordCount < 0x11u) {
        return 0x10u;
    }
    if (requiredWordCount < 0x21u) {
        return 0x20u;
    }
    if (requiredWordCount < 0x41u) {
        return 0x40u;
    }

    uint32_t rounded = 1u;
    while (rounded < requiredWordCount && rounded < 0x80000000u) {
        rounded <<= 1u;
    }
    return rounded;
}

static void ResetAuthBootstrap680BigIntObject(
    AuthBootstrap680BigIntObject20Scaffold* outObject,
    std::vector<uint32_t>* ownedDigits) {
    if (!outObject || !ownedDigits) {
        return;
    }

    ownedDigits->assign(2u, 0u);
    outObject->vtable00 = 0x004ba50cu;
    outObject->reserved04 = 0u;
    outObject->capacityWords08 = 2u;
    outObject->digits0c = ownedDigits->data();
    outObject->sign10 = 0u;
}

static void ResetAuthBootstrap680ReplyMaterialization(
    AuthBootstrap680ChildSketch& child,
    AuthBootstrap680ChildOwnedState& ownedState) {
    child.authReplyCopyShadowF4 = nullptr;
    ownedState.authReplyCopyShadowF4.reset();
    ResetAuthBootstrap680BigIntObject(&child.modulusBigIntB0, &ownedState.modulusBigIntB0OwnedDigits);
    ResetAuthBootstrap680BigIntObject(
        &child.publicExponentBigIntC4,
        &ownedState.publicExponentBigIntC4OwnedDigits);
    ResetAuthBootstrap680BigIntObject(
        &child.privateExponentBigIntD8,
        &ownedState.privateExponentBigIntD8OwnedDigits);
}

static const uint8_t* AuthBootstrap680BigIntObjectBytes(
    const AuthBootstrap680BigIntObject20Scaffold& object) {
    return reinterpret_cast<const uint8_t*>(&object);
}

static bool BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
    AuthBootstrap680BigIntObject20Scaffold* outObject,
    std::vector<uint32_t>* ownedDigits,
    const uint8_t* bigEndianBytes,
    size_t byteCount) {
    if (!outObject || !ownedDigits || !bigEndianBytes || byteCount == 0u) {
        return false;
    }

    const size_t requiredWordCount = (byteCount + 3u) / 4u;
    const uint32_t capacityWords = RoundAuthBootstrap680BigIntCapacityWords(requiredWordCount);
    ownedDigits->assign(static_cast<size_t>(capacityWords), 0u);
    for (size_t i = 0; i < byteCount; ++i) {
        const size_t reversedIndex = byteCount - 1u - i;
        const size_t wordIndex = reversedIndex / 4u;
        const size_t byteShift = (reversedIndex & 3u) * 8u;
        (*ownedDigits)[wordIndex] |= static_cast<uint32_t>(bigEndianBytes[i]) << byteShift;
    }

    outObject->vtable00 = 0x004ba50cu;
    outObject->reserved04 = 0u;
    outObject->capacityWords08 = capacityWords;
    outObject->digits0c = ownedDigits->empty() ? nullptr : ownedDigits->data();
    outObject->sign10 = 0u;
    return true;
}

static bool BuildPositiveAuthBootstrap680BigIntFromUnsignedByte(
    AuthBootstrap680BigIntObject20Scaffold* outObject,
    std::vector<uint32_t>* ownedDigits,
    uint8_t value) {
    if (!outObject || !ownedDigits) {
        return false;
    }

    ownedDigits->assign(2u, 0u);
    (*ownedDigits)[0] = static_cast<uint32_t>(value);

    outObject->vtable00 = 0x004ba50cu;
    outObject->reserved04 = 0u;
    outObject->capacityWords08 = 2u;
    outObject->digits0c = ownedDigits->data();
    outObject->sign10 = 0u;
    return true;
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

void AuthBootstrap680Ops::EraseOwnedState(const CLTLoginMediator* mediator) {
    g_authBootstrap680ChildOwnedStateByMediator.erase(mediator);
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
            child.authServerTimeBias80 = static_cast<uint32_t>(
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

            // Current source-owned validation still stops short of the original
            // `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`
            // signature-check worker call, but the launcher-recovered gate is tighter than the
            // older loose presence test:
            // - `0x448140` requires the parse-object auth-data field length to be exactly `0x136`
            // - `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` uses child `+0xac` and rejects
            //   expired signed-data blocks before the
            //   later `+0xf4` copy/materialization path runs
            const std::time_t now = std::time(nullptr);
            const uint32_t currentAuthServerTime =
                (now > static_cast<std::time_t>(child.authServerTimeBias80))
                    ? static_cast<uint32_t>(now - static_cast<std::time_t>(child.authServerTimeBias80))
                    : 0u;
            if (!reply.valid || !reply.signedData.valid ||
                reply.authDataBytes.size() != sizeof(AuthBootstrapReplyCopyShadowF4Sketch) ||
                child.replyAuthDataValidatorAC == nullptr ||
                currentAuthServerTime >= reply.signedData.expiryTime) {
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
    // Static `0x41f370` is now concrete: this wrapper returns owner `+0x680 -> +0xf4 -> +0xa8`
    // when the copied auth-data block is present, not the earlier child `+0xa8` public-key worker.
    const auto* copyShadow = static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(
        mediator.authBootstrapChild680_.authReplyCopyShadowF4);
    void* value = nullptr;
    if (copyShadow != nullptr) {
        value = reinterpret_cast<void*>(static_cast<uintptr_t>(ReadU32LE(copyShadow->signedData80.data() + 0x28u)));
    }

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
    const bool present = BootstrapRaw08AuxHandle50(mediator) != nullptr;
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

    if (!child.sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendGetPublicKeyRequest missing child+0x50 send target; recovered 0x447eb0 tail expects direct virtual send through that field");
        return 0u;
    }

    auto* sendTarget = static_cast<mxo::liblttcp::CBaseConnection*>(child.sendTarget50);
    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    const uint32_t sendResult = sendTarget->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via 0x447eb0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsGetPublicKeyRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        fmt::ptr(child.sendTarget50),
        static_cast<unsigned>(sendResult));
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
    if (!child.sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendAuthRequest missing child+0x50 send target; recovered 0x4474f0 tail expects direct virtual send through that field");
        return 0u;
    }

    auto* sendTarget = static_cast<mxo::liblttcp::CBaseConnection*>(child.sendTarget50);
    const uint8_t rawCode =
        buildResult.packet.payloadBytes.empty() ? 0u : buildResult.packet.payloadBytes[0];
    const uint32_t sendResult = sendTarget->SendPacket(
        buildResult.packet.bytes.data(),
        static_cast<uint32_t>(buildResult.packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via 0x4474f0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsAuthRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        buildResult.packet.headerBytes.size(),
        buildResult.packet.payloadBytes.size(),
        buildResult.packet.bytes.size(),
        fmt::ptr(child.sendTarget50),
        static_cast<unsigned>(sendResult));
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

// anchor: launcher.exe:0x44831c..0x448467 (raw `0x09` inbound case building/sending raw `0x0a`)
uint32_t AuthBootstrap680Ops::SendAuthChallengeResponse(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthChallenge& challenge) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    const char* password = SmallStringMirrorDataOrEmpty(child.string10);
    const char* soePassword = SmallStringMirrorDataOrEmpty(child.string1C);
    if (SmallStringMirrorLength(child.string10) == 0u) {
        spdlog::error(
            "launcher-owned auth received AS_AuthChallenge but child+0x10 password data is empty");
        return 0;
    }
    if (SmallStringMirrorLength(child.string1C) == 0u) {
        spdlog::warn(
            "launcher-owned auth raw0x0a using empty child+0x1c secondary password/station field while preserving the recovered 0x44831c field mapping");
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
            password,
            soePassword,
            layout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::error("launcher-owned auth failed to build AS_AuthChallengeResponse");
        return 0;
    }

    // Current source still uses a recovered packet-builder helper rather than reconstructing the
    // exact original temporary object family rooted at `0x44831c`, but the field mapping and the
    // child+0x50 direct-send tail now follow the static launcher.exe case closely.
    if (!child.sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680 raw0x0a challenge-response missing child+0x50 send target; refusing less-faithful fallback path");
        return 0u;
    }

    auto* sendTarget = static_cast<mxo::liblttcp::CBaseConnection*>(child.sendTarget50);
    const uint8_t rawCode =
        buildResult.packet.payloadBytes.empty() ? 0u : buildResult.packet.payloadBytes[0];
    const uint32_t sendResult = sendTarget->SendPacket(
        buildResult.packet.bytes.data(),
        static_cast<uint32_t>(buildResult.packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via child+0x50 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} -> sendResult=0x{:08x}",
        "AS_AuthChallengeResponse",
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        buildResult.packet.headerBytes.size(),
        buildResult.packet.payloadBytes.size(),
        buildResult.packet.bytes.size(),
        fmt::ptr(child.sendTarget50),
        static_cast<unsigned>(sendResult));
    mediator.authChallengeResponseSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(mediator, buildResult);
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthChallengeResponse passwordLengthField={} soePasswordLengthField={} plaintextLen={} ciphertextLen={} childString10Len={} childString1CLen={}",
            (unsigned)buildResult.passwordLengthField,
            (unsigned)buildResult.soePasswordLengthField,
            (unsigned)buildResult.plaintextBytes.size(),
            (unsigned)buildResult.ciphertextBytes.size(),
            static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
            static_cast<unsigned>(SmallStringMirrorLength(child.string1C)));
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
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount={} worldCount={} username='{}' successHeaderUnknownWord05=0x{:04x} successHeaderUnknownDword07=0x{:08x} unknown2=0x{:08x} unknown3=0x{:08x} authDataFieldLen=0x{:04x} signatureLen={} encryptedPrivateExponentLen={}",
        reply.characterCount,
        reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text,
        static_cast<unsigned>(reply.successHeaderUnknownWord05),
        static_cast<unsigned>(reply.successHeaderUnknownDword07),
        static_cast<unsigned>(reply.unknown2),
        static_cast<unsigned>(reply.unknown3),
        static_cast<unsigned>(reply.authDataFieldLength),
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
    child.authServerTimeBias80 = 0u;
    child.sendTarget50 = nullptr;
    std::fill(child.feedbackSeed84.begin(), child.feedbackSeed84.end(), 0u);
    child.feedbackTransformLarge94 = nullptr;
    child.feedbackTransformSmall98 = nullptr;
    child.authRequestReadyA0 = 0u;
    child.paddingA1ToA3 = {};
    child.lazyPubkeyDatStateA4 = nullptr;
    child.raw08PublicKeyWorkerA8 = nullptr;
    child.replyAuthDataValidatorAC = nullptr;
    child.authReplyParseObjectF0 = nullptr;

    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    ownedState.raw08PublicKeyWorkerPresenceMarker = 0u;
    ownedState.replyAuthDataValidatorPresenceMarker = 0u;
    ResetAuthBootstrap680ReplyMaterialization(child, ownedState);
    ClearSmallStringMirror(child.stringF8);
    ownedState.opaqueReplyBlob108Owned.clear();
    ownedState.opaqueReplyBlob10COwned.clear();
    child.opaqueReplyBlob108 = nullptr;
    child.opaqueReplyBlob10C = nullptr;
    child.authReplySuccessHeaderDword07_110 = 0u;
    child.authReplySuccessField15_114 = 0u;
    child.authReplySuccessField15Timestamp118 = 0u;
}

// UNANCHORED: source-owned owner+0x680 update after parsed `AS_GetPublicKeyReply`.
// Static `0x447f50 / 0x47780 / AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` prove the
// ready-side child keeps two distinct worker
// families at `+0xa8` and `+0xac`; current source still uses child-local presence-only stand-ins
// until those concrete classes are typed tightly enough to deserve their own models.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.currentPublicKeyId9C = reply.publicKeyId;
    child.authRequestReadyA0 = 1u;

    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    const uint32_t presencePublicKeyId = (reply.publicKeyId != 0u) ? reply.publicKeyId : 1u;
    ownedState.raw08PublicKeyWorkerPresenceMarker = presencePublicKeyId;
    ownedState.replyAuthDataValidatorPresenceMarker = presencePublicKeyId;
    child.raw08PublicKeyWorkerA8 = &ownedState.raw08PublicKeyWorkerPresenceMarker;
    child.replyAuthDataValidatorAC = &ownedState.replyAuthDataValidatorPresenceMarker;
}

// UNANCHORED: source-owned post-raw-`0x0a` hook.
// Static `0x44831c..0x448467` does not write back into child `+0x84 .. +0x93`; that seed block is
// produced earlier by `0x4474f0` through the child `+0x54` helper and then consumed indirectly via
// the `+0x94/+0x98` transform objects during raw `0x09` handling. So keep this as a no-op instead
// of the older less-faithful convenience write.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthChallengeResponseBuildResult& buildResult) {
    (void)mediator;
    spdlog::debug(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold observed raw0x0a send without child-state mutation decryptedChallengeBytes={} processedChallengeMd5Bytes={}",
        static_cast<unsigned>(buildResult.decryptedChallengeBytes.size()),
        static_cast<unsigned>(buildResult.processedChallengeMd5Bytes.size()));
}

// anchor: launcher.exe:0x41b500 -> 0x4435f0 -> 0x41ce80 / 0x443340
bool AuthBootstrap680Ops::PrepareState5MarginConnectionCopySendScaffold(
    CLTLoginMediator& mediator,
    mxo::liblttcp::CMarginConnection& marginConnection) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    const auto* copyShadow =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(child.authReplyCopyShadowF4);
    if (copyShadow == nullptr) {
        return false;
    }

    constexpr size_t kBootstrapPrepBlockByteCount = sizeof(AuthBootstrap680BigIntObject20Scaffold);
    const uint8_t* blockB0Bytes = AuthBootstrap680BigIntObjectBytes(child.modulusBigIntB0);
    const uint8_t* blockC4Bytes = AuthBootstrap680BigIntObjectBytes(child.publicExponentBigIntC4);
    const uint8_t* blockD8Bytes = AuthBootstrap680BigIntObjectBytes(child.privateExponentBigIntD8);

    const bool storedReplyCopy =
        marginConnection.StoreBootstrapReplyCopy98(copyShadow, sizeof(*copyShadow));
    const bool storedPrepState =
        marginConnection.StoreBootstrapPrepStateA0(
            blockB0Bytes,
            blockC4Bytes,
            blockD8Bytes,
            kBootstrapPrepBlockByteCount);

    spdlog::info(
        "AuthBootstrap680Ops::PrepareState5MarginConnectionCopySendScaffold staged owner+0x680 child for state5 copy/send copyShadowF4={} storedReplyCopy98={} storedPrepStateA0={} childBlockB0FirstDword=0x{:08x} childBlockC4FirstDword=0x{:08x} childBlockD8FirstDword=0x{:08x}",
        fmt::ptr(copyShadow),
        storedReplyCopy ? 1u : 0u,
        storedPrepState ? 1u : 0u,
        static_cast<unsigned>(ReadU32LE(blockB0Bytes)),
        static_cast<unsigned>(ReadU32LE(blockC4Bytes)),
        static_cast<unsigned>(ReadU32LE(blockD8Bytes)));
    return storedReplyCopy && storedPrepState;
}

// UNANCHORED: source-owned owner+0x680 auth-reply copy-shadow update for later `+0xf4`
// consumers such as `0x433c0 -> 0x41b500 -> 0x41ce80 -> 0x441f30`.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    child.authReplyParseObjectF0 = nullptr;
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    ResetAuthBootstrap680ReplyMaterialization(child, ownedState);

    if (reply.isErrorReply || !reply.valid) {
        return;
    }

    ownedState.authReplyCopyShadowF4 = std::make_unique<AuthBootstrapReplyCopyShadowF4Sketch>();
    AuthBootstrapReplyCopyShadowF4Sketch& copyShadow = *ownedState.authReplyCopyShadowF4;
    copyShadow = {};

    // Prefer the exact length-prefixed `0x136` auth-data field recovered from
    // `0x443470 / 0x448140`: that field is the original copied child `+0xf4` material.
    if (reply.authDataBytes.size() == sizeof(copyShadow)) {
        std::copy(
            reply.authDataBytes.begin(),
            reply.authDataBytes.end(),
            reinterpret_cast<uint8_t*>(&copyShadow));
    } else {
        if (reply.authSignatureBytes.size() != copyShadow.authSignature00.size() ||
            reply.signedData.rawBytes.size() != copyShadow.signedData80.size()) {
            ownedState.authReplyCopyShadowF4.reset();
            return;
        }

        std::copy(
            reply.authSignatureBytes.begin(),
            reply.authSignatureBytes.end(),
            copyShadow.authSignature00.begin());
        std::copy(
            reply.signedData.rawBytes.begin(),
            reply.signedData.rawBytes.end(),
            copyShadow.signedData80.begin());
    }

    child.authReplyCopyShadowF4 = ownedState.authReplyCopyShadowF4.get();

    AuthBootstrap680BigIntObject20Scaffold* blockB0 = &child.modulusBigIntB0;
    AuthBootstrap680BigIntObject20Scaffold* blockC4 = &child.publicExponentBigIntC4;
    AuthBootstrap680BigIntObject20Scaffold* blockD8 = &child.privateExponentBigIntD8;

    const bool builtBlockB0 = BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
        blockB0,
        &ownedState.modulusBigIntB0OwnedDigits,
        copyShadow.signedData80.data() + 0x52u,
        0x60u);
    const bool builtBlockC4 = BuildPositiveAuthBootstrap680BigIntFromUnsignedByte(
        blockC4,
        &ownedState.publicExponentBigIntC4OwnedDigits,
        copyShadow.signedData80[0x51u]);

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    const bool decryptedPrivateExponent = mxo::auth::DecryptAuthReplyPrivateExponent(
        reply,
        mediator.lastAuthRequestBuildResult_.twofishKeyBytes,
        mediator.lastAuthChallenge_.encryptedChallengeBytes,
        &decryptedPrivateExponentBytes);
    const bool builtBlockD8 =
        decryptedPrivateExponent &&
        decryptedPrivateExponentBytes.size() == 0x60u &&
        BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
            blockD8,
            &ownedState.privateExponentBigIntD8OwnedDigits,
            decryptedPrivateExponentBytes.data(),
            decryptedPrivateExponentBytes.size());

    spdlog::info(
        "CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold materialized owner+0x680+0xf4 copyShadow bytes=0x{:03x} replyAuthDataBytes=0x{:03x} signaturePrefix00='{}' signedDataExpiryAc=0x{:08x} modulusPrefixD2='{}' authServerTimeBias80=0x{:08x} builtBlockB0={} builtBlockC4={} builtBlockD8={} blockB0Words={} blockC4Words={} blockD8Words={}",
        static_cast<unsigned>(sizeof(copyShadow)),
        static_cast<unsigned>(reply.authDataBytes.size()),
        BuildHexPreview(
            copyShadow.authSignature00.data(),
            16u,
            16u),
        static_cast<unsigned>(ReadU32LE(copyShadow.signedData80.data() + 0x2cu)),
        BuildHexPreview(
            copyShadow.signedData80.data() + 0x52u,
            16u,
            16u),
        static_cast<unsigned>(child.authServerTimeBias80),
        builtBlockB0 ? 1u : 0u,
        builtBlockC4 ? 1u : 0u,
        builtBlockD8 ? 1u : 0u,
        static_cast<unsigned>(blockB0->capacityWords08),
        static_cast<unsigned>(blockC4->capacityWords08),
        static_cast<unsigned>(blockD8->capacityWords08));
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
    child.authReplySuccessHeaderDword07_110 = reply.successHeaderUnknownDword07;

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessPregateScaffold childStringF8Len={} promptForSecurId={} childField110=0x{:08x}",
        static_cast<unsigned>(SmallStringMirrorLength(child.stringF8)),
        static_cast<unsigned>(child.crashReporterPromptForSecurId104),
        static_cast<unsigned>(child.authReplySuccessHeaderDword07_110));
}

// anchor: launcher.exe:DAT_004f79e0 / 0x43f300 success-side global one-time gate
bool AuthBootstrap680Ops::ConsumeState2AuthReplySuccessOneTimeGateScaffold(CLTLoginMediator& mediator) {
    (void)mediator;
    if (g_authBootstrap680State2AuthReplySuccessOneTimeGate) {
        return false;
    }
    g_authBootstrap680State2AuthReplySuccessOneTimeGate = true;
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

    child.authReplySuccessField15_114 = reply.unknown3;
    child.authReplySuccessField15Timestamp118 = static_cast<uint32_t>(std::time(nullptr));

    if (!reply.username.text.empty()) {
        mediator.SetLaunchPadSourceBlock94FirstString(reply.username.text.c_str());
    }

    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    ownedState.opaqueReplyBlob108Owned = reply.authSignatureBytes;
    ownedState.opaqueReplyBlob10COwned = reply.encryptedPrivateExponentBytes;
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob108, ownedState.opaqueReplyBlob108Owned);
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob10C, ownedState.opaqueReplyBlob10COwned);

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold childField114=0x{:08x} childField118=0x{:08x} ownerSource94FirstString='{}' opaqueBlob108Len={} opaqueBlob10CLen={} opaqueBlob108={} opaqueBlob10C={}",
        static_cast<unsigned>(child.authReplySuccessField15_114),
        static_cast<unsigned>(child.authReplySuccessField15Timestamp118),
        mediator.authBootstrapSource38_.inlineString00[0] != '\0'
            ? mediator.authBootstrapSource38_.inlineString00.data()
            : "<empty>",
        static_cast<unsigned>(ownedState.opaqueReplyBlob108Owned.size()),
        static_cast<unsigned>(ownedState.opaqueReplyBlob10COwned.size()),
        fmt::ptr(child.opaqueReplyBlob108),
        fmt::ptr(child.opaqueReplyBlob10C));
}

}  // namespace mxo::ltlogin
