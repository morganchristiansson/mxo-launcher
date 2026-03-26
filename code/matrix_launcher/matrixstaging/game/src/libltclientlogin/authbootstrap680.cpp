/**
 * AuthBootstrap680 - launcher-owned phase-2 auth/bootstrap child rooted at mediator owner +0x680.
 *
 * Keep this TU focused on the extra bootstrap object/module that state2 slot 3 enters through
 * `0x439210 -> 0x448050`, plus the later auth-reply shadow fields surfaced back through mediator
 * wrappers.
 *
 * Important ownership split:
 * - this file intentionally models the owner `+0x680` bootstrap child as source-owned helper ops
 * - thin `CLTLoginMediator` wrappers remain elsewhere only so current callers/ABI do not churn
 * - do not treat this file as proof that the bootstrap child is literally the mediator class
 */

#include "authbootstrap680_internal.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// UNANCHORED: source-owned sidecar storage for the owner `+0x680` bootstrap child mirrors that we
// do not want to inline into `CLTLoginMediator` layout yet.
struct RecoveredAuthBootstrapSidecarState {
    CLTLoginMediator::AuthBootstrapReplyShadowF4Sketch fieldF4Shadow{};
    uint32_t raw08AuxHandleAvailabilityMarker = 0u;
};

static std::unordered_map<const CLTLoginMediator*, std::unique_ptr<RecoveredAuthBootstrapSidecarState>>
    g_recoveredAuthBootstrapSidecarByMediator;

static RecoveredAuthBootstrapSidecarState* FindRecoveredAuthBootstrapSidecar(const CLTLoginMediator* mediator) {
    const auto it = g_recoveredAuthBootstrapSidecarByMediator.find(mediator);
    return (it != g_recoveredAuthBootstrapSidecarByMediator.end() && it->second)
        ? it->second.get()
        : nullptr;
}

static RecoveredAuthBootstrapSidecarState& MutableRecoveredAuthBootstrapSidecar(const CLTLoginMediator* mediator) {
    std::unique_ptr<RecoveredAuthBootstrapSidecarState>& slot =
        g_recoveredAuthBootstrapSidecarByMediator[mediator];
    if (!slot) {
        slot = std::make_unique<RecoveredAuthBootstrapSidecarState>();
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

}  // namespace

// UNANCHORED: source-owned sidecar cleanup for the owner `+0x680` bootstrap child mirrors.
void AuthBootstrap680Ops::EraseSidecar(const CLTLoginMediator* mediator) {
    g_recoveredAuthBootstrapSidecarByMediator.erase(mediator);
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
void* AuthBootstrap680Ops::BootstrapRaw08AuxHandle50(const CLTLoginMediator& mediator) {
    const auto* fieldF4 =
        static_cast<const CLTLoginMediator::AuthBootstrapReplyShadowF4Sketch*>(mediator.authBootstrap680_.fieldF4);
    void* value = fieldF4 ? fieldF4->raw08AuxHandleA8 : nullptr;

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
    const auto* fieldF4 =
        static_cast<const CLTLoginMediator::AuthBootstrapReplyShadowF4Sketch*>(mediator.authBootstrap680_.fieldF4);
    const bool present = fieldF4 && fieldF4->raw08AuxHandleA8 != nullptr;
    spdlog::debug(
        "CLTLoginMediator::HasBootstrapRaw08AuxHandle54(+0x54) -> {}",
        present ? 1u : 0u);
    return present;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t AuthBootstrap680Ops::GetCrashReporterPromptForSecurId58(const CLTLoginMediator& mediator) {
    const uint8_t prompt = mediator.authBootstrap680_.crashReporterPromptForSecurId104;
    spdlog::debug(
        "CLTLoginMediator::GetCrashReporterPromptForSecurId58(+0x58) -> {}",
        static_cast<unsigned>(prompt));
    return prompt;
}

// anchor: launcher.exe:0x447eb0
uint32_t AuthBootstrap680Ops::SendAuthGetPublicKeyRequest(CLTLoginMediator& mediator) {
    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            mediator.authLauncherVersion_,
            mediator.authCurrentPublicKeyId_,
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
    if (mediator.authUsername_.empty()) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without a username");
        return 0;
    }
    if (!reply.hasEmbeddedPublicKey) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth GetPublicKeyReply has no embedded public key material");
        return 0;
    }

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = mediator.authLoginType_;
    requestLayout.keyConfigMd5 = mediator.authKeyConfigMd5_;
    requestLayout.uiConfigMd5 = mediator.authUiConfigMd5_;
    requestLayout.rsaModulusBytes = reply.modulusBytes;
    requestLayout.rsaExponentBytes.assign(1u, reply.publicExponentByte);

    mxo::auth::AuthRequestBuildResult buildResult;
    if (!mxo::auth::BuildAuthRequestPacket(
            mediator.authUsername_,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest");
        return 0;
    }

    mediator.lastAuthRequestBuildResult_ = buildResult;
    const uint32_t sendResult =
        mediator.SendAuthFramedPacket(buildResult.packet, CLTLoginMediator::kMessageAsAuthRequest);
    mediator.authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId=%u loginType=%u keySize=%u blobLen=%u usernameLengthField=%u usedReplyPublicKey=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
            (unsigned)reply.publicKeyId,
            (unsigned)mediator.authLoginType_,
            (unsigned)reply.keySize,
            (unsigned)buildResult.blobCiphertextBytes.size(),
            (unsigned)buildResult.usernameLengthField,
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            (unsigned)buildResult.keyConfigMd5Bytes.size(),
            (unsigned)buildResult.uiConfigMd5Bytes.size());
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

// UNANCHORED: source-owned parsed-auth logging helper around the `0x4401a0 / 0x43a330` family.
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
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount={} worldCount={} username='{}' authDataMarker=0x{:04x} signatureLen={} encryptedPrivateExponentLen={}",
        reply.characterCount,
        reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text,
        reply.authDataMarker,
        reply.authSignatureBytes.size(),
        reply.encryptedPrivateExponentLength);

    for (size_t i = 0; i < reply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = reply.characters[i];
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth character[%u] handle='%s' characterId=%llu status=%u worldId=%u",
            (unsigned)i,
            entry.handle.text.empty() ? "<empty>" : entry.handle.text.c_str(),
            static_cast<unsigned long long>(entry.characterId),
            (unsigned)entry.status,
            (unsigned)entry.worldId);
    }

    for (size_t i = 0; i < reply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = reply.worlds[i];
        spdlog::info(
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
            mediator.lastAuthRequestBuildResult_.twofishKeyBytes,
            mediator.lastAuthChallenge_.encryptedChallengeBytes,
            &decryptedPrivateExponentBytes)) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth decrypted AS_AuthReply private exponent length=%u",
            (unsigned)decryptedPrivateExponentBytes.size());
    }
}

// UNANCHORED: source-owned fixed-field sync for the owner `+0x680` bootstrap child.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig(CLTLoginMediator& mediator) {
    mediator.authBootstrap680_.loginType28 = mediator.authLoginType_;
    mediator.authBootstrap680_.launcherVersion2C = mediator.authLauncherVersion_;
    mediator.authBootstrap680_.block30 = CopyPrefix16(mediator.authKeyConfigMd5_);
    mediator.authBootstrap680_.block40 = CopyPrefix16(mediator.authUiConfigMd5_);
    mediator.authBootstrap680_.currentPublicKeyId9C = mediator.authCurrentPublicKeyId_;
}

// UNANCHORED: source-owned dynamic-state reset for the owner `+0x680` bootstrap child.
void AuthBootstrap680Ops::ResetRecoveredAuthBootstrapDynamicStateScaffold(CLTLoginMediator& mediator) {
    mediator.authBootstrap680_.timestamp80 = 0u;
    mediator.authBootstrap680_.sendTarget50 = nullptr;
    std::fill(mediator.authBootstrap680_.material85.begin(), mediator.authBootstrap680_.material85.end(), 0u);
    mediator.authBootstrap680_.sideObject94 = nullptr;
    mediator.authBootstrap680_.sideObject98 = nullptr;
    mediator.authBootstrap680_.helperA0 = nullptr;
    mediator.authBootstrap680_.lazyRaw06StateA4 = nullptr;
    mediator.authBootstrap680_.raw08AuxHandleA8 = nullptr;
    mediator.authBootstrap680_.fieldAC = nullptr;
    mediator.authBootstrap680_.fieldF0 = nullptr;
    mediator.authBootstrap680_.fieldF4 = nullptr;
    mediator.authBootstrap680_.fieldF8 = nullptr;
    mediator.authBootstrap680_.fieldFC = nullptr;
    mediator.authBootstrap680_.field100 = nullptr;
    mediator.authBootstrap680_.field108 = 0u;
    mediator.authBootstrap680_.field10C = 0u;
    mediator.authBootstrap680_.field110 = 0u;
    mediator.authBootstrap680_.field114 = 0u;
    mediator.authBootstrap680_.field118 = 0u;
    EraseSidecar(&mediator);
}

// UNANCHORED: source-owned owner+0x680 update after parsed `AS_GetPublicKeyReply`.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    mediator.authBootstrap680_.currentPublicKeyId9C = reply.publicKeyId;
    RecoveredAuthBootstrapSidecarState& sidecar = MutableRecoveredAuthBootstrapSidecar(&mediator);
    sidecar.raw08AuxHandleAvailabilityMarker = (reply.publicKeyId != 0u) ? reply.publicKeyId : 1u;
    mediator.authBootstrap680_.helperA0 = &sidecar.raw08AuxHandleAvailabilityMarker;
    mediator.authBootstrap680_.raw08AuxHandleA8 = &sidecar.raw08AuxHandleAvailabilityMarker;
    mediator.authBootstrap680_.fieldAC = &sidecar.raw08AuxHandleAvailabilityMarker;
}

// UNANCHORED: source-owned owner+0x680 challenge-material update after raw `0x0a` build/send.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthChallengeResponseBuildResult& buildResult) {
    mediator.authBootstrap680_.material85 = CopyPrefix16(buildResult.decryptedChallengeBytes);
}

// UNANCHORED: source-owned owner+0x680 auth-reply shadow update for later `+0x50/+0x5c` exposure.
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    mediator.authBootstrap680_.fieldF4 = nullptr;

    RecoveredAuthBootstrapSidecarState* sidecar = FindRecoveredAuthBootstrapSidecar(&mediator);
    if (sidecar) {
        sidecar->fieldF4Shadow = {};
    }

    if (reply.isErrorReply || !reply.valid || !reply.hasAuthDataMarker ||
        reply.authDataMarker != 0x0136u || mediator.authBootstrap680_.raw08AuxHandleA8 == nullptr) {
        return;
    }

    RecoveredAuthBootstrapSidecarState& materializedSidecar = MutableRecoveredAuthBootstrapSidecar(&mediator);
    materializedSidecar.fieldF4Shadow.material85 = mediator.authBootstrap680_.material85;
    materializedSidecar.fieldF4Shadow.raw08AuxHandleA8 = mediator.authBootstrap680_.raw08AuxHandleA8;
    mediator.authBootstrap680_.fieldF4 = &materializedSidecar.fieldF4Shadow;

    spdlog::info(
        "CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold material85='{}' raw08AuxHandle={} authDataMarker=0x{:04x}",
        BuildHexPreview(
            materializedSidecar.fieldF4Shadow.material85.data(),
            materializedSidecar.fieldF4Shadow.material85.size(),
            materializedSidecar.fieldF4Shadow.material85.size()),
        fmt::ptr(materializedSidecar.fieldF4Shadow.raw08AuxHandleA8),
        static_cast<unsigned>(reply.authDataMarker));
}

}  // namespace mxo::ltlogin
