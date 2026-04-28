/**
 * AuthBootstrap680 - launcher-owned phase-2 auth/bootstrap child rooted at mediator owner +0x680.
 *
 * Keep this TU focused on the separate bootstrap child/module that state2 slot 3 hands off into
 * through `0x439210 -> 0x448050`, plus the later auth-reply shadow fields surfaced back through
 * mediator wrappers.
 *
 * Important ownership split:
 * - this file intentionally models the owner `+0x680` bootstrap child as its own source-owned
 *   child class boundary
 * - state/owner paths should call that child directly instead of routing through fake mediator
 *   auth-bootstrap wrappers
 * - do not treat this file as proof that the bootstrap child is literally the mediator class
 */

#include "loginmediator.h"
#include "../../../runtime/src/libltcrypto/auth_internal.h"
#include "authbootstrap680.h"
#include "loginstate_packet_builder_scaffold.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <random>
#include <unordered_map>

#include <integer.h>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

struct AuthBootstrap680RsaPublicKeyPairOwnedState {
    std::vector<uint32_t> modulus08OwnedDigits;
    std::vector<uint32_t> exponent1cOwnedDigits;
    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> exponentBytes;
};

// anchor: launcher.exe:DAT_004f79e0
bool g_authBootstrap680State2AuthReplySuccessOneTimeGate = false;

namespace {

// Keep non-layout ownership outside `AuthBootstrap680Child_0x441290` so the child mirror can stay
// faithful to launcher field boundaries while source still owns heap-backed helper payloads.
struct AuthBootstrap680Field54HelperOwnedState {
    std::vector<uint8_t> bufferedOutput14;
    std::vector<uint8_t> scratchPrefix20;
};

struct AuthBootstrap680LazyPubkeyDatValidatorOwnedState {
    std::unique_ptr<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch> object;
    AuthBootstrap680RsaPublicKeyPairOwnedState publicKeyPair0c;
};

struct AuthBootstrap680Raw08PublicKeyWorkerOwnedState {
    std::unique_ptr<AuthBootstrap680Raw08PublicKeyWorkerA8Sketch> object;
    AuthBootstrap680RsaPublicKeyPairOwnedState publicKeyPair0c;
};

struct AuthBootstrap680ReplyAuthDataValidatorOwnedState {
    std::unique_ptr<AuthBootstrap680ReplyAuthDataValidatorACSketch> object;
    AuthBootstrap680RsaPublicKeyPairOwnedState publicKeyPair0c;
};

struct AuthBootstrap680ChildOwnedState {
    AuthBootstrap680Field54HelperOwnedState field54Helper;
    AuthBootstrap680LazyPubkeyDatValidatorOwnedState lazyPubkeyDatValidatorA4;
    AuthBootstrap680Raw08PublicKeyWorkerOwnedState raw08PublicKeyWorkerA8;
    AuthBootstrap680ReplyAuthDataValidatorOwnedState replyAuthDataValidatorAC;
    std::unique_ptr<mxo::auth::internal::FeedbackSizeTransformAdapterLarge> feedbackTransformLarge94;
    std::unique_ptr<mxo::auth::internal::FeedbackSizeTransformAdapterSmall> feedbackTransformSmall98;
    std::unique_ptr<AuthBootstrap680AuthReplyParseObjectF0Sketch> authReplyParseObjectF0;
    std::vector<uint8_t> authReplyParsePacketBodyBytes;
    std::unique_ptr<AuthBootstrapReplyCopyShadowF4_0x44add0> authReplyCopyShadowF4;
    std::vector<uint32_t> modulusBigIntB0OwnedDigits;
    std::vector<uint32_t> publicExponentBigIntC4OwnedDigits;
    std::vector<uint32_t> privateExponentBigIntD8OwnedDigits;
    std::vector<uint8_t> opaqueReplyBlob108Owned;
    std::vector<uint8_t> opaqueReplyBlob10COwned;
    std::vector<uint8_t> stagedIncomingAuthPacketBytes;
    mxo::auth::GetPublicKeyReply cachedGetPublicKeyReply;
    mxo::auth::AuthRequestBuildResult cachedAuthRequestBuildResult;
    mxo::auth::AuthChallenge cachedAuthChallenge;
    mxo::auth::AuthReply cachedAuthReply;
};

constexpr uint32_t kIncomingAuthMessageLocatorPayloadOffsetTable[7] = {
    0x11u,
    0x04u,
    0x10u,
    0x0bu,
    0x10u,
    0x11u,
    0x10u,
};

struct IncomingAuthPayloadViewScaffold {
    const uint8_t* payloadBytes = nullptr;
    size_t payloadByteCount = 0u;
    uint8_t rawCode = 0u;
    bool headerless = false;
    bool usedHeaderlessLocatorDecode = false;
};

// anchor family: launcher.exe:0x41bc20 / 0x41bbb0
static bool ResolveIncomingAuthMessageCodePointerScaffold(
    const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c& incomingMessageRef,
    const uint8_t** outMessageCodePointer,
    bool* outUsedHeaderlessLocatorDecode) {
    if (outMessageCodePointer) {
        *outMessageCodePointer = nullptr;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = false;
    }

    const auto* messageStorage = incomingMessageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
    if (!payloadBytes || payloadByteCount == 0u) {
        return false;
    }

    if (incomingMessageRef.headerless10 == 0u) {
        if (outMessageCodePointer) {
            *outMessageCodePointer = payloadBytes;
        }
        return true;
    }

    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = true;
    }
    if (payloadByteCount < 2u) {
        return false;
    }

    const uint8_t locatorByte0d = payloadBytes[1];
    const uint8_t targetLocatorType = static_cast<uint8_t>(locatorByte0d & 0x07u);
    const uint8_t senderLocatorType = static_cast<uint8_t>((locatorByte0d >> 4) & 0x07u);
    if (targetLocatorType == 0u || targetLocatorType > 6u ||
        senderLocatorType == 0u || senderLocatorType > 6u) {
        return false;
    }

    const size_t payloadOffset =
        0x12u +
        static_cast<size_t>(kIncomingAuthMessageLocatorPayloadOffsetTable[targetLocatorType - 1u]) +
        static_cast<size_t>(kIncomingAuthMessageLocatorPayloadOffsetTable[senderLocatorType - 1u]);
    if (payloadOffset >= payloadByteCount) {
        return false;
    }

    if (outMessageCodePointer) {
        *outMessageCodePointer = payloadBytes + payloadOffset;
    }
    return true;
}

static bool BuildIncomingAuthPayloadViewScaffold(
    const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingAuthMessageRef,
    IncomingAuthPayloadViewScaffold* outView) {
    if (outView) {
        *outView = {};
    }
    if (!incomingAuthMessageRef || !outView) {
        return false;
    }

    const auto* messageStorage = incomingAuthMessageRef->messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
    if (!payloadBytes || payloadByteCount == 0u) {
        return false;
    }

    const uint8_t* logicalPayloadBytes = nullptr;
    bool usedHeaderlessLocatorDecode = false;
    if (!ResolveIncomingAuthMessageCodePointerScaffold(
            *incomingAuthMessageRef,
            &logicalPayloadBytes,
            &usedHeaderlessLocatorDecode) ||
        !logicalPayloadBytes) {
        return false;
    }

    const size_t logicalPayloadOffset =
        static_cast<size_t>(logicalPayloadBytes - payloadBytes);
    if (logicalPayloadOffset >= payloadByteCount) {
        return false;
    }

    outView->payloadBytes = logicalPayloadBytes;
    outView->payloadByteCount = static_cast<size_t>(payloadByteCount) - logicalPayloadOffset;
    outView->rawCode = logicalPayloadBytes[0];
    outView->headerless = (incomingAuthMessageRef->headerless10 != 0u);
    outView->usedHeaderlessLocatorDecode = usedHeaderlessLocatorDecode;
    return true;
}

// anchor: launcher.exe:DAT_004b6b38
static constexpr std::array<uint8_t, 0x100u> kAuthBootstrap680PubkeyDatFallbackModulus = {
    0xc7u, 0x48u, 0x18u, 0xfdu, 0x48u, 0xdcu, 0x8fu, 0x4eu, 0xecu, 0x35u, 0xe1u, 0xcfu,
    0x65u, 0xe6u, 0x72u, 0xb7u, 0x7du, 0xcau, 0x04u, 0x48u, 0x3bu, 0xa2u, 0x58u, 0xd6u,
    0x1au, 0x31u, 0x8fu, 0x2bu, 0x7du, 0xafu, 0xceu, 0xf7u, 0x65u, 0xf2u, 0x1fu, 0x9cu,
    0xb5u, 0xeau, 0xa0u, 0xdeu, 0xb3u, 0x13u, 0xa0u, 0x4eu, 0x06u, 0xd7u, 0xfbu, 0xdcu,
    0xefu, 0x94u, 0x74u, 0xceu, 0x3bu, 0x80u, 0xefu, 0x7bu, 0xbdu, 0xa4u, 0x3bu, 0x04u,
    0xc6u, 0x71u, 0x22u, 0xc8u, 0x88u, 0x0du, 0x47u, 0x1fu, 0x8eu, 0x76u, 0xbeu, 0xbau,
    0xddu, 0x5cu, 0xb5u, 0xe5u, 0x97u, 0xc0u, 0xb3u, 0xf5u, 0x0fu, 0x0fu, 0xa1u, 0x01u,
    0x08u, 0x0au, 0xf7u, 0x1bu, 0xfbu, 0x9bu, 0x3au, 0xabu, 0x84u, 0xb1u, 0x12u, 0xccu,
    0xb1u, 0xcdu, 0xaau, 0x1du, 0x9bu, 0x5fu, 0xfbu, 0x99u, 0x5eu, 0xb1u, 0xe8u, 0x8au,
    0x76u, 0xc0u, 0xb6u, 0x5bu, 0xe0u, 0x3au, 0x9au, 0xb0u, 0x28u, 0x52u, 0x95u, 0xe4u,
    0x65u, 0x51u, 0x53u, 0xbdu, 0x41u, 0xd7u, 0x27u, 0xe9u, 0x99u, 0x72u, 0xc9u, 0xe5u,
    0x4cu, 0x9au, 0xd6u, 0xdbu, 0x25u, 0xedu, 0x0fu, 0x7au, 0x0fu, 0x49u, 0x81u, 0x64u,
    0x05u, 0xe0u, 0x7fu, 0xb9u, 0xeau, 0x70u, 0x09u, 0xd8u, 0x96u, 0x7au, 0xacu, 0xa4u,
    0xabu, 0xc2u, 0x10u, 0x75u, 0x72u, 0x0bu, 0x04u, 0x10u, 0x3eu, 0x6cu, 0x27u, 0x81u,
    0xa7u, 0xbdu, 0xd5u, 0x61u, 0x7cu, 0x10u, 0xd6u, 0xa3u, 0x77u, 0x54u, 0x2bu, 0x2eu,
    0x1cu, 0x32u, 0x15u, 0x88u, 0xeeu, 0x7eu, 0x4au, 0xdau, 0x4du, 0x27u, 0x35u, 0xbfu,
    0xeau, 0xa6u, 0xdbu, 0xefu, 0x33u, 0x80u, 0x5du, 0x74u, 0x31u, 0x47u, 0x0cu, 0x90u,
    0x55u, 0x9cu, 0x45u, 0x74u, 0x3au, 0x0au, 0x3bu, 0x09u, 0xe0u, 0x2cu, 0x31u, 0x9eu,
    0x8bu, 0xfdu, 0xe0u, 0xb8u, 0xadu, 0xf1u, 0x26u, 0x6bu, 0x8bu, 0x8eu, 0x41u, 0xd2u,
    0xf2u, 0xf1u, 0xedu, 0xffu, 0xe6u, 0xcau, 0xc9u, 0xcbu, 0x1au, 0xedu, 0xf8u, 0xd1u,
    0xabu, 0xa8u, 0x69u, 0x8au, 0x76u, 0x33u, 0xdau, 0x2du, 0x57u, 0x00u, 0xffu, 0xcau,
    0x67u, 0x9cu, 0x71u, 0x73u,
};

static std::unordered_map<const AuthBootstrap680ChildBase_0x4b7134*, AuthBootstrap680ChildOwnedState>
 g_authBootstrap680ChildOwnedStateByChild;

static AuthBootstrap680ChildOwnedState& MutableAuthBootstrap680ChildOwnedState(
 const AuthBootstrap680ChildBase_0x4b7134* child) {
 return g_authBootstrap680ChildOwnedStateByChild[child];
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

static uint16_t ReadU16LE(const uint8_t* bytes) {
    if (!bytes) {
        return 0u;
    }
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8u);
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

static void ResetAuthBootstrap680BigIntObject(
    AuthBootstrap680BigIntObjects_0x4ba50c* outObject,
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

static bool BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
    AuthBootstrap680BigIntObjects_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    const uint8_t* bigEndianBytes,
    size_t byteCount);
static bool BuildPositiveAuthBootstrap680BigIntFromUnsignedByte(
    AuthBootstrap680BigIntObjects_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    uint8_t value);

static void ResetAuthBootstrap680RsaPublicKeyPairOwnedState(
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    if (!ownedState) {
        return;
    }

    ownedState->modulus08OwnedDigits.clear();
    ownedState->exponent1cOwnedDigits.clear();
    ownedState->modulusBytes.clear();
    ownedState->exponentBytes.clear();
}

// anchor: launcher.exe:0x4420f0
static void ResetAuthBootstrap680RsaPublicKeyPairSubobject(
    AuthBootstrap680RsaPublicKeyPairSubobject0cSketch* outSubobject,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    if (!outSubobject || !ownedState) {
        return;
    }

    outSubobject->vtable00 = 0x004b6680u;
    outSubobject->helperVtable04 = 0x004b66acu;
    ResetAuthBootstrap680BigIntObject(&outSubobject->modulus08, &ownedState->modulus08OwnedDigits);
    ResetAuthBootstrap680BigIntObject(&outSubobject->exponent1c, &ownedState->exponent1cOwnedDigits);
    outSubobject->helperThunk30 = 0x004b630cu;
    outSubobject->helperThunk34 = 0x004b6348u;
    outSubobject->helperThunk38 = 0x004b6454u;
    outSubobject->helperVtable3c = 0x004b66a0u;
    ownedState->modulusBytes.clear();
    ownedState->exponentBytes.clear();
}

// anchor: launcher.exe:0x447120 / 0x447020
static bool BuildAuthBootstrap680RsaPublicKeyPairSubobjectFromReplyPublicKey(
    AuthBootstrap680RsaPublicKeyPairSubobject0cSketch* outSubobject,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const uint8_t* modulusBytes,
    size_t modulusByteCount,
    uint8_t exponentByte) {
    if (!outSubobject || !ownedState || !modulusBytes || modulusByteCount == 0u || exponentByte == 0u) {
        return false;
    }

    ResetAuthBootstrap680RsaPublicKeyPairSubobject(outSubobject, ownedState);
    if (!BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
            &outSubobject->modulus08,
            &ownedState->modulus08OwnedDigits,
            modulusBytes,
            modulusByteCount)) {
        ResetAuthBootstrap680RsaPublicKeyPairSubobject(outSubobject, ownedState);
        return false;
    }
    if (!BuildPositiveAuthBootstrap680BigIntFromUnsignedByte(
            &outSubobject->exponent1c,
            &ownedState->exponent1cOwnedDigits,
            exponentByte)) {
        ResetAuthBootstrap680RsaPublicKeyPairSubobject(outSubobject, ownedState);
        return false;
    }

    ownedState->modulusBytes.assign(modulusBytes, modulusBytes + modulusByteCount);
    ownedState->exponentBytes.assign(1u, exponentByte);
    return true;
}

static void ResetAuthBootstrap680Raw08PublicKeyWorkerA8(
    AuthBootstrap680Raw08PublicKeyWorkerA8Sketch* outWorker,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    if (!outWorker || !ownedState) {
        return;
    }

    outWorker->vtable00 = 0x004b75e4u;
    outWorker->helperVtable04 = 0x004b7610u;
    outWorker->helperVtable08 = 0x004b7138u;
    ResetAuthBootstrap680RsaPublicKeyPairSubobject(&outWorker->publicKeyPair0c, ownedState);
    outWorker->helperThunk4c = 0x004b75e0u;
    outWorker->helperThunk50 = 0x004b6300u;
    outWorker->helperThunk54 = 0x004b3e18u;
    outWorker->helperVtable58 = 0x004b67a0u;
}

static void ResetAuthBootstrap680ReplyAuthDataValidatorAC(
    AuthBootstrap680ReplyAuthDataValidatorACSketch* outValidator,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    if (!outValidator || !ownedState) {
        return;
    }

    outValidator->vtable00 = 0x004b7580u;
    outValidator->helperVtable04 = 0x004b75ccu;
    outValidator->helperVtable08 = 0x004b7440u;
    ResetAuthBootstrap680RsaPublicKeyPairSubobject(&outValidator->publicKeyPair0c, ownedState);
    outValidator->helperThunk4c = 0x004b73c0u;
    outValidator->helperThunk50 = 0x004b6c44u;
}

}  // namespace

void AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::ResetAsRecoveredLeaf(
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    ResetAuthBootstrap680Raw08PublicKeyWorkerA8(this, ownedState);
}

bool AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::ConstructFromReplyPublicKey(
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const uint8_t* modulusBytes,
    size_t modulusByteCount,
    uint8_t exponentByte) {
    ResetAsRecoveredLeaf(ownedState);
    if (!BuildAuthBootstrap680RsaPublicKeyPairSubobjectFromReplyPublicKey(
            &publicKeyPair0c,
            ownedState,
            modulusBytes,
            modulusByteCount,
            exponentByte)) {
        ResetAsRecoveredLeaf(ownedState);
        return false;
    }
    return true;
}

void AuthBootstrap680ReplyAuthDataValidatorACSketch::ResetAsRecoveredLeaf(
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState) {
    ResetAuthBootstrap680ReplyAuthDataValidatorAC(this, ownedState);
}

bool AuthBootstrap680ReplyAuthDataValidatorACSketch::ConstructFromReplyPublicKey(
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const uint8_t* modulusBytes,
    size_t modulusByteCount,
    uint8_t exponentByte) {
    ResetAsRecoveredLeaf(ownedState);
    if (!BuildAuthBootstrap680RsaPublicKeyPairSubobjectFromReplyPublicKey(
            &publicKeyPair0c,
            ownedState,
            modulusBytes,
            modulusByteCount,
            exponentByte)) {
        ResetAsRecoveredLeaf(ownedState);
        return false;
    }
    return true;
}

namespace {

static void ResetAuthBootstrap680ReplyPublicKeyWorkers(
 AuthBootstrap680ChildBase_0x4b7134& child,
 AuthBootstrap680ChildOwnedState& ownedState) {
 child.raw08PublicKeyWorkerA8 = nullptr;
 child.replyAuthDataValidatorAC = nullptr;

 ownedState.raw08PublicKeyWorkerA8.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c);
 ownedState.replyAuthDataValidatorAC.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.replyAuthDataValidatorAC.publicKeyPair0c);
}

static void ResetAuthBootstrap680FeedbackTransforms(
 AuthBootstrap680ChildBase_0x4b7134& child,
 AuthBootstrap680ChildOwnedState& ownedState) {
 child.feedbackTransformLarge94 = nullptr;
 child.feedbackTransformSmall98 = nullptr;
 ownedState.feedbackTransformLarge94.reset();
 ownedState.feedbackTransformSmall98.reset();
}

static void ResetAuthBootstrap680Field54Helper(
    AuthBootstrap680Field54HelperSketch* outHelper,
    AuthBootstrap680Field54HelperOwnedState* ownedState) {
    if (!outHelper || !ownedState) {
        return;
    }

    ownedState->bufferedOutput14.assign(0x180u, 0u);
    ownedState->scratchPrefix20.assign(0x40u, 0u);

    outHelper->vtable00 = 0x004b695cu;
    outHelper->helperVtable04 = 0x004b68a8u;
    outHelper->helperVtable08 = 0x004b41e0u;
    outHelper->reserved0c = 0u;
    outHelper->bufferedOutputByteCount10 = 0x180u;
    outHelper->bufferedOutputBytes14 = ownedState->bufferedOutput14.data();
    outHelper->reserved18 = 0u;
    outHelper->scratchPrefixByteCount1c = 0x40u;
    outHelper->scratchPrefixBytes20 = ownedState->scratchPrefix20.data();
    outHelper->bufferedStreamState24 = 0u;
    outHelper->nextBufferedOutputByte28 = 0x180u;
}

static void FillAuthBootstrap680Field54SeedBytesScaffold(
    AuthBootstrap680Field54HelperSketch& helper,
    std::array<uint8_t, 16>& outSeed) {
    // Static RE proves `0x4474f0` calls helper vtable `+0x18 / 0x468640` and copies the emitted
    // `0x10` bytes into child `+0x84 .. +0x93`. The exact buffered generator behind
    // `0x468640 / 0x468b60 / 0x468d30` is not yet closed tightly enough to reimplement bit-for-bit,
    // so keep the recovered helper layout explicit while bounding the source-owned byte producer to
    // one fresh 16-byte block here.
    std::random_device rd;
    for (uint8_t& byte : outSeed) {
        byte = static_cast<uint8_t>(rd());
    }

    const uint32_t outputStart = helper.scratchPrefixByteCount1c;
    if (helper.bufferedOutputBytes14 != nullptr &&
        helper.bufferedOutputByteCount10 >= outputStart + outSeed.size()) {
        std::copy(outSeed.begin(), outSeed.end(), helper.bufferedOutputBytes14 + outputStart);
    }
    helper.bufferedStreamState24 = 0u;
    helper.nextBufferedOutputByte28 = outputStart + static_cast<uint32_t>(outSeed.size());
}

static void ResetAuthBootstrap680AuthReplyParseAccessor(
    AuthBootstrap680AuthReplyParseAccessor10Sketch* outAccessor,
    uint32_t vtable,
    const uint8_t* packetBody,
    uint8_t resolveFieldsNow) {
    if (!outAccessor) {
        return;
    }

    outAccessor->vtable00 = vtable;
    outAccessor->packetBody04 = packetBody;
    outAccessor->incomingMessage08 = nullptr;
    outAccessor->resolveFields0c = resolveFieldsNow;
    outAccessor->padding0d = {};
}

static void ResetAuthBootstrap680ReplyParseObject(
 AuthBootstrap680ChildBase_0x4b7134& child,
 AuthBootstrap680ChildOwnedState& ownedState) {
    child.authReplyParseObjectF0 = nullptr;
    ownedState.authReplyParseObjectF0.reset();
    ownedState.authReplyParsePacketBodyBytes.clear();
}

static void TryResolveAuthBootstrap680ReplyLengthPrefixedField(
    std::vector<uint8_t>& packetBodyBytes,
    size_t replyHeaderOffset,
    bool zeroTerminateLastByte,
    const uint8_t** outFieldBytes,
    uint16_t* outFieldLength) {
    if (outFieldBytes) {
        *outFieldBytes = nullptr;
    }
    if (outFieldLength) {
        *outFieldLength = 0u;
    }
    if (packetBodyBytes.empty() || replyHeaderOffset + 2u > packetBodyBytes.size()) {
        return;
    }

    const uint16_t fieldOffset = ReadU16LE(packetBodyBytes.data() + replyHeaderOffset);
    if (fieldOffset == 0u) {
        return;
    }

    const size_t lengthOffset = static_cast<size_t>(fieldOffset);
    if (lengthOffset + 2u > packetBodyBytes.size()) {
        return;
    }

    const uint16_t fieldLength = ReadU16LE(packetBodyBytes.data() + lengthOffset);
    const size_t fieldDataOffset = lengthOffset + 2u;
    const size_t fieldDataEnd = fieldDataOffset + static_cast<size_t>(fieldLength);
    if (fieldDataEnd > packetBodyBytes.size()) {
        return;
    }

    if (zeroTerminateLastByte && fieldLength != 0u) {
        packetBodyBytes[fieldDataEnd - 1u] = 0u;
    }

    if (outFieldBytes) {
        *outFieldBytes = packetBodyBytes.data() + fieldDataOffset;
    }
    if (outFieldLength) {
        *outFieldLength = fieldLength;
    }
}

static bool BuildAuthBootstrap680AuthReplyParseObjectFromPacketBody(
    AuthBootstrap680AuthReplyParseObjectF0Sketch* outParseObject,
    std::vector<uint8_t>* packetBodyBytes) {
    if (!outParseObject || !packetBodyBytes) {
        return false;
    }

    *outParseObject = {};
    // anchor: launcher.exe:0x004b6c74
    // The launcher reuses this same vtable address for the compact
    // `Packet_AsGetPublicKeyRequest_0x4b6c74` builder and the larger copied auth-reply parse shell.
    outParseObject->vtable00 = 0x004b6c74u;
    outParseObject->packetBody04 = packetBodyBytes->empty() ? nullptr : packetBodyBytes->data();
    outParseObject->incomingMessage08 = nullptr;
    outParseObject->resolveFields0c = 1u;
    outParseObject->replyHeader10 = outParseObject->packetBody04;

    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->worldDescriptorAccessor5c,
        0x004b533cu,
        outParseObject->packetBody04,
        outParseObject->resolveFields0c);
    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->slotRecordAccessor70,
        0x004b5328u,
        outParseObject->packetBody04,
        outParseObject->resolveFields0c);

    if (packetBodyBytes->empty()) {
        return true;
    }

    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x05u,
        true,
        &outParseObject->stringField05Bytes14,
        &outParseObject->stringField05Length18);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x0bu,
        false,
        &outParseObject->authDataBytes1c,
        &outParseObject->authDataByteLength20);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x0du,
        false,
        &outParseObject->encryptedPrivateExponentBytes24,
        &outParseObject->encryptedPrivateExponentByteLength28);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x0fu,
        false,
        &outParseObject->opaqueField0fBytes2c,
        &outParseObject->opaqueField0fByteLength30);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x11u,
        false,
        &outParseObject->opaqueField11Bytes34,
        &outParseObject->opaqueField11ByteLength38);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x13u,
        false,
        &outParseObject->characterTempRecords3c,
        &outParseObject->characterTempRecordCount40);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x19u,
        false,
        &outParseObject->worldTempRecords44,
        &outParseObject->worldTempRecordCount48);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x1bu,
        false,
        &outParseObject->opaqueField1bBytes4c,
        &outParseObject->opaqueField1bByteLength50);
    TryResolveAuthBootstrap680ReplyLengthPrefixedField(
        *packetBodyBytes,
        0x1du,
        true,
        &outParseObject->replyString1dBytes54,
        &outParseObject->replyString1dByteLength58);
    return true;
}

static bool StoreAuthBootstrap680AuthReplyParseObjectFromStagedPacket(
    CLTLoginMediator& /*mediator*/,
    AuthBootstrap680Child_0x441290& child,
    const std::vector<uint8_t>& stagedBytes) {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);
    ResetAuthBootstrap680ReplyParseObject(child, ownedState);

    if (stagedBytes.empty()) {
        return false;
    }

    ownedState.authReplyParsePacketBodyBytes = stagedBytes;
    ownedState.authReplyParseObjectF0 = std::make_unique<AuthBootstrap680AuthReplyParseObjectF0Sketch>();
    if (!BuildAuthBootstrap680AuthReplyParseObjectFromPacketBody(
            ownedState.authReplyParseObjectF0.get(),
            &ownedState.authReplyParsePacketBodyBytes)) {
        ResetAuthBootstrap680ReplyParseObject(child, ownedState);
        return false;
    }

    child.authReplyParseObjectF0 = ownedState.authReplyParseObjectF0.get();
    return true;
}

static void ResetAuthBootstrap680ReplyMaterialization(
 AuthBootstrap680ChildBase_0x4b7134& child,
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



static bool BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
    AuthBootstrap680BigIntObjects_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    const uint8_t* bigEndianBytes,
    size_t byteCount) {
    if (!outObject || !ownedDigits || !bigEndianBytes || byteCount == 0u) {
        return false;
    }

    const size_t requiredWordCount = (byteCount + 3u) / 4u;
    uint32_t capacityWords = 0u;
    // anchor: launcher.exe:0x45d340
    // Preserve the old `CryptoPP::Integer` ctor capacity policy only where the launcher child
    // still physically stores raw `0x4ba50c` objects. `0x45d340` rounds requested word counts
    // through `DAT_004ba310`, then 0x10/0x20/0x40, then the next power of two.
    if (requiredWordCount < std::size(kAuthBootstrap680BigIntCapacityTable)) {
        capacityWords = kAuthBootstrap680BigIntCapacityTable[requiredWordCount];
    } else if (requiredWordCount < 0x11u) {
        capacityWords = 0x10u;
    } else if (requiredWordCount < 0x21u) {
        capacityWords = 0x20u;
    } else if (requiredWordCount < 0x41u) {
        capacityWords = 0x40u;
    } else {
        capacityWords = 1u;
        while (capacityWords < requiredWordCount && capacityWords < 0x80000000u) {
            capacityWords <<= 1u;
        }
    }
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
    AuthBootstrap680BigIntObjects_0x4ba50c* outObject,
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

static CryptoPP::Integer AuthBootstrap680BigIntObjectToCryptoPPInteger(
    const AuthBootstrap680BigIntObjects_0x4ba50c& object) {
    const auto* digits = static_cast<const uint32_t*>(object.digits0c);
    if (!digits || object.capacityWords08 == 0u) {
        return CryptoPP::Integer::Zero();
    }

    size_t usedWordCount = object.capacityWords08;
    while (usedWordCount != 0u && digits[usedWordCount - 1u] == 0u) {
        --usedWordCount;
    }
    if (usedWordCount == 0u) {
        return CryptoPP::Integer::Zero();
    }

    // anchor family: launcher.exe:0x45d000 / 0x45de10 / data type `0x4ba50c`
    // The preserved child-side `0x14` object stores little-endian digit words. Modern
    // `CryptoPP::Integer(byte*, size)` expects big-endian bytes, so convert here once and then
    // keep direct `CryptoPP::Integer` semantics through the later `0x443340 -> 0x443220 -> 0x465d70`
    // margin-bootstrap prep path.
    std::vector<uint8_t> bigEndianBytes(usedWordCount * 4u);
    for (size_t wordIdx = 0; wordIdx < usedWordCount; ++wordIdx) {
        const uint32_t word = digits[wordIdx];
        const size_t destBase = (usedWordCount - 1u - wordIdx) * 4u;
        bigEndianBytes[destBase + 0] = static_cast<uint8_t>(word >> 24);
        bigEndianBytes[destBase + 1] = static_cast<uint8_t>(word >> 16);
        bigEndianBytes[destBase + 2] = static_cast<uint8_t>(word >> 8);
        bigEndianBytes[destBase + 3] = static_cast<uint8_t>(word);
    }

    return CryptoPP::Integer(bigEndianBytes.data(), bigEndianBytes.size());
}

static bool BuildAuthBootstrap680CryptoPublicKeyFromOwnedState(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    CryptoPP::RSA::PublicKey* outPublicKey) {
    return mxo::auth::internal::BuildPublicKeyFromBytes(
        ownedState.modulusBytes,
        ownedState.exponentBytes,
        outPublicKey);
}

// anchor: launcher.exe:0x468f80 / 0x44aec0 / 0x467ee0 / 0x4680a0 / 0x468e20
// Static RE now closes the common validator-family finalize path tightly enough to mirror the real
// launcher call shape instead of a generic RSA-signature guess:
// - `0x437ba0` allocates a temporary worker through validator vtable `+0x1c`
// - `0x468520` loads the signature by applying the outer RSA public-key path and storing the
//   encoded representative bytes into worker `+0x14/+0x18`
// - worker vtable `+0x0c / 0x447340` feeds caller bytes into the worker-local MD5 object
//   returned by worker vtable `+0x44 / 0x447380`
// - `0x445410` returns the 18-byte MD5 `DigestInfo` prefix at `0x004baefc`
// - `0x468e20` builds the exact EMSA-PKCS1-v1_5 encoded block:
//     [optional leading 0 when (modulusBitCount-1)&7 != 0]
//     0x01 || 0xff... || 0x00 || DigestInfoPrefix || workerMd5Digest
// - `0x4680a0` compares that generated block byte-for-byte against the worker's stored decoded
//   signature bytes
//
// So the remaining source-side logic below is now a direct mirror of the recovered launcher
// finalize semantics. Keep it source-local instead of pretending it is one recovered library
// function: this helper is a composite mirror of the concrete launcher chain
// `0x467ee0 -> 0x4680a0 -> 0x468e20 -> 0x445410`, not a single standalone leaf.
// On the child `+0xac` path this intentionally means a second MD5 stage over
// the caller's already-prebuilt 16-byte MD5 digest, because `0x44ae40` hashes signed-data first
// and the temporary worker then hashes those 16 bytes again before the final compare.
static bool VerifyAuthBootstrap680ValidatorFamilyRecoveredFinalizeScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* signedBytes,
    size_t signedByteCount,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) {
    if (!signedBytes || signedByteCount == 0u || !signatureBytes || signatureByteCount == 0u) {
        return false;
    }

    static constexpr std::array<uint8_t, 0x12u> kMd5DigestInfoPrefix = {
        0x30u, 0x20u, 0x30u, 0x0cu, 0x06u, 0x08u, 0x2au, 0x86u, 0x48u,
        0x86u, 0xf7u, 0x0du, 0x02u, 0x05u, 0x05u, 0x00u, 0x04u, 0x10u,
    };

    CryptoPP::RSA::PublicKey publicKey;
    if (!BuildAuthBootstrap680CryptoPublicKeyFromOwnedState(ownedState, &publicKey)) {
        return false;
    }

    const CryptoPP::Integer& modulus = publicKey.GetModulus();
    const size_t modulusBitCount = static_cast<size_t>(modulus.BitCount());
    if (modulusBitCount == 0u) {
        return false;
    }

    const size_t modulusBitCountMinusOne = modulusBitCount - 1u;
    const size_t representativeByteCount = (modulusBitCountMinusOne + 7u) >> 3u;
    if (representativeByteCount == 0u || signatureByteCount != representativeByteCount) {
        return false;
    }

    std::array<uint8_t, 16> workerMd5Digest{};
    CryptoPP::Weak::MD5 md5;
    md5.Update(signedBytes, signedByteCount);
    md5.Final(workerMd5Digest.data());

    const CryptoPP::Integer signatureInteger(signatureBytes, signatureByteCount);
    if (signatureInteger >= modulus) {
        return false;
    }

    const CryptoPP::Integer representativeInteger = publicKey.ApplyFunction(signatureInteger);
    std::vector<uint8_t> representativeBytes(representativeByteCount, 0u);
    representativeInteger.Encode(representativeBytes.data(), representativeBytes.size());

    std::vector<uint8_t> expectedEncodedBytes(representativeByteCount, 0u);
    uint8_t* outputCursor = expectedEncodedBytes.data();
    if ((modulusBitCountMinusOne & 7u) != 0u) {
        *outputCursor++ = 0x00u;
    }

    if (expectedEncodedBytes.size() <
        static_cast<size_t>(outputCursor - expectedEncodedBytes.data()) + 1u +
            kMd5DigestInfoPrefix.size() + workerMd5Digest.size() + 1u) {
        return false;
    }

    *outputCursor++ = 0x01u;
    uint8_t* const digestStart =
        expectedEncodedBytes.data() + expectedEncodedBytes.size() - workerMd5Digest.size();
    uint8_t* const digestInfoStart = digestStart - kMd5DigestInfoPrefix.size();
    if (digestInfoStart <= outputCursor) {
        return false;
    }

    std::fill(outputCursor, digestInfoStart - 1u, 0xffu);
    *(digestInfoStart - 1u) = 0x00u;
    std::copy(kMd5DigestInfoPrefix.begin(), kMd5DigestInfoPrefix.end(), digestInfoStart);
    std::copy(workerMd5Digest.begin(), workerMd5Digest.end(), digestStart);

    return representativeBytes == expectedEncodedBytes;
}

}  // namespace

bool AuthBootstrap680ReplyAuthDataValidatorACSketch::VerifySignatureRecoveredFinalizeScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* signedBytes,
    size_t signedByteCount,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) const {
    return VerifyAuthBootstrap680ValidatorFamilyRecoveredFinalizeScaffold(
        ownedState,
        signedBytes,
        signedByteCount,
        signatureBytes,
        signatureByteCount);
}

namespace {

// anchor: launcher.exe:0x468ea0
// Static RE now closes the outer formula tightly even though the inner worker stack is still
// bounded in source: `0x468ea0` computes
//   ciphertextBlockBytes * ceil(plaintextByteCount / plaintextChunkBytes)
// where:
// - ciphertext block bytes come from worker vtable `+0x24 -> 0x41f090 -> this+0x0c -> modulus`
// - plaintext chunk bytes come from the helper family behind outer `this+4/+8`
//   (`0x468f00` re-queries that chunk bound each loop)
//
// Disassembly also proves `0x468f00` pushes four stack args into worker vtable `+0x1c`, and
// `0x468280` returns with `ret 0x10`; the decompiler undercounts that unless checked against the
// assembly. Source therefore now mirrors the outer formula more literally:
// - ciphertext block bytes are derived from the RSA modulus bit count like the launcher path
// - plaintext chunk bytes remain a bounded OAEP-backed stand-in for the still-unclosed helper
//   family behind outer `this+4/+8`
static size_t QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(
    const CryptoPP::RSA::PublicKey& publicKey) {
    const size_t modulusBitCount = static_cast<size_t>(publicKey.GetModulus().BitCount());
    if (modulusBitCount == 0u) {
        return 0u;
    }

    return ((modulusBitCount - 1u) + 7u) >> 3u;
}

static uint32_t QueryAuthBootstrap680Raw08PublicKeyWorkerEncryptedOutputLengthScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    size_t plaintextByteCount) {
    CryptoPP::RSA::PublicKey publicKey;
    if (!BuildAuthBootstrap680CryptoPublicKeyFromOwnedState(ownedState, &publicKey)) {
        return 0u;
    }

    CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);
    const size_t maxPlaintextChunkByteCount = encryptor.FixedMaxPlaintextLength();
    const size_t ciphertextChunkByteCount =
        QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(publicKey);
    if (maxPlaintextChunkByteCount == 0u || ciphertextChunkByteCount == 0u) {
        return 0u;
    }

    const size_t chunkCount =
        (plaintextByteCount + maxPlaintextChunkByteCount - 1u) / maxPlaintextChunkByteCount;
    return static_cast<uint32_t>(chunkCount * ciphertextChunkByteCount);
}

}  // namespace

uint32_t AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::QueryEncryptedOutputLengthScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    size_t plaintextByteCount) const {
    return QueryAuthBootstrap680Raw08PublicKeyWorkerEncryptedOutputLengthScaffold(
        ownedState,
        plaintextByteCount);
}

bool AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::EncryptPlaintextIntoCiphertextScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* plaintextBytes,
    size_t plaintextByteCount,
    std::vector<uint8_t>* outCiphertextBytes) const {
    if (!plaintextBytes || plaintextByteCount == 0u || !outCiphertextBytes) {
        return false;
    }

    CryptoPP::RSA::PublicKey publicKey;
    if (!BuildAuthBootstrap680CryptoPublicKeyFromOwnedState(ownedState, &publicKey)) {
        return false;
    }

    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);
        const size_t maxPlaintextChunkByteCount = encryptor.FixedMaxPlaintextLength();
        const size_t ciphertextChunkByteCount =
            QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(publicKey);
        if (maxPlaintextChunkByteCount == 0u || ciphertextChunkByteCount == 0u) {
            outCiphertextBytes->clear();
            return false;
        }

        outCiphertextBytes->clear();
        outCiphertextBytes->reserve(
            QueryEncryptedOutputLengthScaffold(ownedState, plaintextByteCount));

        for (size_t offset = 0u; offset < plaintextByteCount; offset += maxPlaintextChunkByteCount) {
            const size_t chunkByteCount = std::min(
                maxPlaintextChunkByteCount,
                plaintextByteCount - offset);
            std::string ciphertextChunk;
            CryptoPP::StringSource source(
                plaintextBytes + offset,
                chunkByteCount,
                true,
                new CryptoPP::PK_EncryptorFilter(
                    rng,
                    encryptor,
                    new CryptoPP::StringSink(ciphertextChunk)));
            if (ciphertextChunk.size() != ciphertextChunkByteCount) {
                outCiphertextBytes->clear();
                return false;
            }
            outCiphertextBytes->insert(
                outCiphertextBytes->end(),
                ciphertextChunk.begin(),
                ciphertextChunk.end());
        }
        return !outCiphertextBytes->empty();
    } catch (const CryptoPP::Exception&) {
        outCiphertextBytes->clear();
        return false;
    }
}

namespace {

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

static uint32_t ReadAuthBootstrap680AuthReplyParseHeaderDword(
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject,
    size_t headerOffset) {
    if (!parseObject || !parseObject->replyHeader10) {
        return 0u;
    }
    return ReadU32LE(parseObject->replyHeader10 + headerOffset);
}

}  // namespace

// anchor: launcher.exe:0x445500
AuthBootstrap680ChildBase_0x4b7134::AuthBootstrap680ChildBase_0x4b7134() {
 // Compiler handles vtable at +0x00
 // +0x04, +0x08, +0x0c: small string mirrors cleared by UpdateExceptionState(8) pattern
 // +0x10, +0x14, +0x18: more small string mirrors cleared by UpdateExceptionState(8)
 // +0x1c, +0x20, +0x24: final small string mirrors cleared by UpdateExceptionState(8)

 AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(this);
 ResetAuthBootstrap680Field54Helper(&feedbackSeedHelper54, &ownedState.field54Helper);

 // +0x80, +0x94, +0x98, +0x9c, +0xa0: zeroed
 // +0xa4: lazy validator object reset below
 ResetAuthBootstrap680ReplyPublicKeyWorkers(*this, ownedState);
 // +0xa8: raw08 worker reset in ReplyPublicKeyWorkers
 // +0xac: validator reset in ReplyPublicKeyWorkers

 ResetAuthBootstrap680FeedbackTransforms(*this, ownedState);
 // +0x94: large transform nulled
 // +0x98: small transform nulled

 ownedState.lazyPubkeyDatValidatorA4.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.lazyPubkeyDatValidatorA4.publicKeyPair0c);
 lazyPubkeyDatValidatorA4 = nullptr; // +0xa4 = 0

 ResetAuthBootstrap680ReplyParseObject(*this, ownedState);
 // +0xf0: parse object nulled

 ResetAuthBootstrap680ReplyMaterialization(*this, ownedState);
 // +0xb0, +0xc4, +0xd8: big-int objects reset

 inboundAuthStatusEc = 1u; // +0xec = 1

 // +0xf4: auth reply copy shadow pointer nulled in ResetAuthBootstrap680ReplyMaterialization
}

// anchor: launcher.exe:0x441290
AuthBootstrap680Child_0x441290::AuthBootstrap680Child_0x441290()
 : AuthBootstrap680ChildBase_0x4b7134() {

    // +0x108, +0x10c: opaque blob pointers
    opaqueReplyBlob108 = nullptr;
    opaqueReplyBlob10C = nullptr;

    // +0x110, +0x114, +0x118: success header/field/timestamp
    authReplySuccessHeaderDword07_110 = 0u;
    authReplySuccessField15_114 = 0u;
    authReplySuccessField15Timestamp118 = 0u;
}

// anchor: launcher.exe:0x445a40 (dtor in vtable, calls base dtor then optionally frees this)
// Handles +0xf8..+0x118 fields (opaque blobs, stringF8, success fields)
// The base class destructor (~AuthBootstrap680ChildBase_0x4b7134) handles +0x00..+0xf4
AuthBootstrap680Child_0x441290::~AuthBootstrap680Child_0x441290() {
 // meth_0x4410b0 (0x4410b0) handles +0x108, +0x10c opaque blob freeing
 // with InterlockedExchangeAdd tracked deallocation
 // +0xf8: stringF8 small string mirror (if allocated, call FUN_00403c20)
 // +0x110, +0x114, +0x118: success fields (no dynamic allocation)

 // Release opaque blob at +0x108 with tracked deallocation
 if (opaqueReplyBlob108 != nullptr) {
 // Tracked deallocation pattern: _msize, InterlockedExchangeAdd(-size), InterlockedDecrement, free
 void* blob = opaqueReplyBlob108;
 opaqueReplyBlob108 = nullptr;
 // TODO: implement tracked deallocation properly
 (void)blob;
 }

 // Release opaque blob at +0x10c with tracked deallocation
 if (opaqueReplyBlob10C != nullptr) {
 void* blob = opaqueReplyBlob10C;
 opaqueReplyBlob10C = nullptr;
 // TODO: implement tracked deallocation properly
 (void)blob;
 }

 // +0xf8: stringF8 - free if begin != current (FUN_00403c20 pattern)
 if (stringF8.begin != nullptr && stringF8.begin != stringF8.current) {
 // FUN_00403c20(begin, current - begin) to deallocate
 // Currently just clear in source
 stringF8 = {};
 }

 // Base class destructor handles +0x00..+0xf4
}

// anchor: launcher.exe:0x441330
void AuthBootstrap680Child_0x441290::SetPromptPasswordF8AndSecurIdFlag(
    const char* promptPasswordWithOptionalSecurId) {
    if (promptPasswordWithOptionalSecurId == nullptr) {
        stringF8.owned.clear();
        stringF8.begin = nullptr;
        stringF8.current = nullptr;
        stringF8.capacity = nullptr;
        crashReporterPromptForSecurId104 = 0u;
        return;
    }

    std::string promptPassword = promptPasswordWithOptionalSecurId;
    bool promptForSecurId = false;
    const size_t slashPos = promptPassword.find('/');
    if (slashPos != std::string::npos && slashPos + 7u == promptPassword.size()) {
        promptForSecurId = true;
        for (size_t i = slashPos + 1u; i < promptPassword.size(); ++i) {
            const unsigned char ch = static_cast<unsigned char>(promptPassword[i]);
            if (ch < static_cast<unsigned char>('0') ||
                ch > static_cast<unsigned char>('9')) {
                promptForSecurId = false;
                break;
            }
        }
    }
    if (promptForSecurId && promptPassword.size() >= 7u) {
        promptPassword.resize(promptPassword.size() - 7u);
    }

    stringF8.owned = std::move(promptPassword);
    stringF8.begin = stringF8.owned.c_str();
    stringF8.current = stringF8.begin + stringF8.owned.size();
    stringF8.capacity = stringF8.current;
    crashReporterPromptForSecurId104 = promptForSecurId ? 1u : 0u;
}

// anchor: launcher.exe:0x441260
void AuthBootstrap680Child_0x441290::StoreField114AndTimestamp118(uint32_t field114Value) {
    authReplySuccessField15_114 = field114Value;
    authReplySuccessField15Timestamp118 = static_cast<uint32_t>(std::time(nullptr));
}

// anchor: launcher.exe:0x441170
void AuthBootstrap680Child_0x441290::CopyOpaqueReplyBlobs108_10c() {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(this);
    ownedState.opaqueReplyBlob108Owned.clear();
    ownedState.opaqueReplyBlob10COwned.clear();
    opaqueReplyBlob108 = nullptr;
    opaqueReplyBlob10C = nullptr;

    const AuthBootstrap680AuthReplyParseObjectF0Sketch* const parseObject = authReplyParseObjectF0;
    if (parseObject != nullptr && parseObject->opaqueField0fBytes2c != nullptr &&
        parseObject->opaqueField0fByteLength30 != 0u) {
        ownedState.opaqueReplyBlob108Owned.assign(
            parseObject->opaqueField0fBytes2c,
            parseObject->opaqueField0fBytes2c + parseObject->opaqueField0fByteLength30);
        opaqueReplyBlob108 = ownedState.opaqueReplyBlob108Owned.data();
    }
    if (parseObject != nullptr && parseObject->opaqueField11Bytes34 != nullptr &&
        parseObject->opaqueField11ByteLength38 != 0u) {
        ownedState.opaqueReplyBlob10COwned.assign(
            parseObject->opaqueField11Bytes34,
            parseObject->opaqueField11Bytes34 + parseObject->opaqueField11ByteLength38);
        opaqueReplyBlob10C = ownedState.opaqueReplyBlob10COwned.data();
    }
}

// anchor: launcher.exe:0x43d480
std::string AuthBootstrap680Child_0x441290::CopyReplyString54_SOURCEOWNED() const {
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* const parseObject = authReplyParseObjectF0;
    if (parseObject == nullptr || parseObject->replyString1dBytes54 == nullptr ||
        parseObject->replyString1dByteLength58 == 0u) {
        return {};
    }

    const char* const replyStringBegin =
        reinterpret_cast<const char*>(parseObject->replyString1dBytes54);
    size_t replyStringLength = 0u;
    while (replyStringLength < parseObject->replyString1dByteLength58 &&
           replyStringBegin[replyStringLength] != '\0') {
        ++replyStringLength;
    }
    return std::string(replyStringBegin, replyStringLength);
}

// anchor: launcher.exe:0x468f80
static bool AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(
    AuthBootstrap680ChildBase_0x4b7134& child,
    const mxo::auth::GetPublicKeyReply& reply) {
    if (mxo::ltlogin::g_SkipAuthPublicKeyReplyValidation != 0u) {
        return true;
    }

    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);
    if (child.lazyPubkeyDatValidatorA4 == nullptr || !ownedState.lazyPubkeyDatValidatorA4.object) {
        ownedState.lazyPubkeyDatValidatorA4.object =
            std::make_unique<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch>();
        if (!ownedState.lazyPubkeyDatValidatorA4.object->ConstructFromReplyPublicKey(
                &ownedState.lazyPubkeyDatValidatorA4.publicKeyPair0c,
                kAuthBootstrap680PubkeyDatFallbackModulus.data(),
                kAuthBootstrap680PubkeyDatFallbackModulus.size(),
                0x11u)) {
            ownedState.lazyPubkeyDatValidatorA4.object.reset();
            child.lazyPubkeyDatValidatorA4 = nullptr;
            return false;
        }
        child.lazyPubkeyDatValidatorA4 = ownedState.lazyPubkeyDatValidatorA4.object.get();
    }
    if (child.lazyPubkeyDatValidatorA4 == nullptr) {
        return false;
    }
    if (reply.modulusBytes.size() != 0x80u || reply.signatureBytes.empty() ||
        reply.publicExponentByte == 0u) {
        return false;
    }

    std::array<uint8_t, 0x81u> signedReplyPublicKeyBytes{};
    std::copy(reply.modulusBytes.begin(), reply.modulusBytes.end(), signedReplyPublicKeyBytes.begin());
    signedReplyPublicKeyBytes.back() = reply.publicExponentByte;
    return child.lazyPubkeyDatValidatorA4->VerifySignatureRecoveredFinalizeScaffold(
        ownedState.lazyPubkeyDatValidatorA4.publicKeyPair0c,
        signedReplyPublicKeyBytes.data(),
        signedReplyPublicKeyBytes.size(),
        reply.signatureBytes.data(),
        reply.signatureBytes.size());
}

// anchor: launcher.exe:0x445610
// Base dtor handles +0x00..+0xf4 (validators, helpers, big-ints, parse objects, copy shadow)
AuthBootstrap680ChildBase_0x4b7134::~AuthBootstrap680ChildBase_0x4b7134() {
 // +0x94: feedbackTransformLarge94 - call dtor(1) if non-null
 if (feedbackTransformLarge94 != nullptr) {
 // feedbackTransformLarge94->~FeedbackSizeTransformAdapterLarge();
 feedbackTransformLarge94 = nullptr;
 }

 // +0x98: feedbackTransformSmall98 - call dtor(1) if non-null
 if (feedbackTransformSmall98 != nullptr) {
 // feedbackTransformSmall98->~FeedbackSizeTransformAdapterSmall();
 feedbackTransformSmall98 = nullptr;
 }

 // +0xa4: lazyPubkeyDatValidatorA4 - call dtor(1) if non-null
 if (lazyPubkeyDatValidatorA4 != nullptr) {
 // lazyPubkeyDatValidatorA4->~AuthBootstrap680ReplyAuthDataValidatorACSketch();
 lazyPubkeyDatValidatorA4 = nullptr;
 }

 // +0xa8: raw08PublicKeyWorkerA8 - call dtor(1) if non-null
 if (raw08PublicKeyWorkerA8 != nullptr) {
 // raw08PublicKeyWorkerA8->~AuthBootstrap680Raw08PublicKeyWorkerA8Sketch();
 raw08PublicKeyWorkerA8 = nullptr;
 }

 // +0xac: replyAuthDataValidatorAC - call dtor(1) if non-null
 if (replyAuthDataValidatorAC != nullptr) {
 // replyAuthDataValidatorAC->~AuthBootstrap680ReplyAuthDataValidatorACSketch();
 replyAuthDataValidatorAC = nullptr;
 }

 // anchor: launcher.exe:0x444900 - called at this point in base dtor flow
 ClearReplyParseAndCopyShadowFields();

 // +0xb0, +0xc4, +0xd8: big-int objects reset (vtable reset, no heap)
 modulusBigIntB0 = {};
 publicExponentBigIntC4 = {};
 privateExponentBigIntD8 = {};

 // +0x54: feedbackSeedHelper54 - clear/reset (calls FUN_0041c750 pattern for each buffer)
 feedbackSeedHelper54 = {};

 // +0x04..+0x24: small string mirrors cleared (three groups of three pointers)
 string04 = {};
 string10 = {};
 string1C = {};

 // Erase owned state from global map
 g_authBootstrap680ChildOwnedStateByChild.erase(this);
}

// anchor: launcher.exe:0x444900
// Handles +0x04 (string04), +0x10 (string10), +0x28..+0x4c (block30, block40, sendTarget50, loginType28, launcherVersion2C)
// +0xf0 (authReplyParseObjectF0), +0xf4 (authReplyCopyShadowF4)
void AuthBootstrap680ChildBase_0x4b7134::ClearReplyParseAndCopyShadowFields() {
 // +0x04: string04 small string mirror - zero if begin != capacity
 if (string04.begin != nullptr && string04.begin != string04.capacity) {
 string04 = {};
 }

 // +0x10: string10 small string mirror - zero if begin != capacity
 if (string10.begin != nullptr && string10.begin != string10.capacity) {
 string10 = {};
 }

 // +0x28..+0x4c: zero these fields
 loginType28 = 0u;
 launcherVersion2C = 0u;
 // +0x30: block30 - already zeroed with ={}
 // +0x40: block40 - already zeroed with ={}
 // +0x50: sendTarget50 - zeroed below
 sendTarget50 = nullptr;

 // +0xf4: authReplyCopyShadowF4 - tracked deallocation
 if (authReplyCopyShadowF4 != nullptr) {
 // Tracked: _msize, InterlockedExchangeAdd(-size), InterlockedDecrement, free
 authReplyCopyShadowF4 = nullptr;
 }

 // +0xf0: authReplyParseObjectF0 - call dtor(1) if non-null
 if (authReplyParseObjectF0 != nullptr) {
 // authReplyParseObjectF0->~AuthBootstrap680AuthReplyParseObjectF0Sketch();
 authReplyParseObjectF0 = nullptr;
 }
}

const mxo::auth::AuthReply& AuthBootstrap680Child_0x441290::CachedAuthReply_SOURCEOWNED() const {
    return MutableAuthBootstrap680ChildOwnedState(this).cachedAuthReply;
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
void* AuthBootstrap680Child_0x441290::BootstrapRaw08AuxHandle50() const {
    // Static `0x41f370` is now concrete: this wrapper returns owner `+0x680 -> +0xf4 -> +0xa8`
    // when the copied auth-data block is present, not the earlier child `+0xa8` public-key worker.
    const auto* copyShadow =
        static_cast<const AuthBootstrapReplyCopyShadowF4_0x44add0*>(authReplyCopyShadowF4);
    void* value = nullptr;
    if (copyShadow != nullptr) {
        value = reinterpret_cast<void*>(static_cast<uintptr_t>(ReadU32LE(copyShadow->signedData80.data() + 0x28u)));
    }
    return value;
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
bool AuthBootstrap680Child_0x441290::HasBootstrapRaw08AuxHandle54() const {
    return BootstrapRaw08AuxHandle50() != nullptr;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t AuthBootstrap680Child_0x441290::GetCrashReporterPromptForSecurId58() const {
    return crashReporterPromptForSecurId104;
}

// anchor: launcher.exe:0x44add0
bool AuthBootstrapReplyCopyShadowF4_0x44add0::IsFresh(int timeBias) const {
    // Ghidra: return (bool)('\x01' - ((uint)((int)time(NULL) - param_1) < this->mbr_0xac));
    // mbr_0xac is at class offset +0xac = signedData80 + 0x2c
    const time_t now = time(nullptr);
    const uint32_t expiry = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(this) + 0xac);
    return static_cast<bool>('\x01' - (static_cast<uint32_t>(static_cast<int>(now) - timeBias) < expiry));
}

// anchor: launcher.exe:0x44ae40
void AuthBootstrapReplyCopyShadowF4_0x44add0::BuildSignedDataMd5Digest(std::array<uint8_t, 16>* outDigest) const {
    if (!outDigest) {
        return;
    }
    // Uses same logic as BuildAuthBootstrapReplyCopyShadowF4SignedDataMd5Digest10Scaffold
    CryptoPP::Weak::MD5 md5;
    md5.Update(signedData80.data(), signedData80.size());
    md5.Final(outDigest->data());
}

// anchor: launcher.exe:0x44aec0
// Full fidelity: builds MD5 of signedData80 and delegates to validator
uint32_t AuthBootstrapReplyCopyShadowF4_0x44add0::VerifyWithValidator(
    AuthBootstrap680ReplyAuthDataValidatorACSketch* validator,
    const AuthBootstrap680RsaPublicKeyPairOwnedState& publicKeyPair,
    int timeBias) const {
    // Calculate current auth server time
    const std::time_t now = std::time(nullptr);
    const uint32_t currentAuthServerTime =
        (now > static_cast<std::time_t>(timeBias))
            ? static_cast<uint32_t>(now - static_cast<std::time_t>(timeBias))
            : 0u;

    // Read expiry from signedData80 + 0x2c (= +0xac in class)
    const uint32_t expiryTimeAc = *reinterpret_cast<const uint32_t*>(signedData80.data() + 0x2c);
    if (currentAuthServerTime >= expiryTimeAc) {
        return 0;
    }

    // Build MD5 over signed data (0xb6 bytes from +0x80)
    std::array<uint8_t, 16> md5Digest10{};
    BuildSignedDataMd5Digest(&md5Digest10);

    // Call validator verify with public key pair
    return validator->VerifySignatureRecoveredFinalizeScaffold(
        publicKeyPair,
        md5Digest10.data(),
        md5Digest10.size(),
        authSignature00.data(),
        authSignature00.size());
}

// anchor: launcher.exe:0x4435f0
void AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0::PrepareState5MarginConnectionCopySend(
    mxo::liblttcp::CMarginConnection_0x4aff38& marginConnection) {
    AuthBootstrap680Child_0x441290& child = child_;
    const auto* copyShadow =
        static_cast<const AuthBootstrapReplyCopyShadowF4_0x44add0*>(child.authReplyCopyShadowF4);
    if (copyShadow == nullptr) {
        spdlog::warn(
            "AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0::PrepareState5MarginConnectionCopySend missing child+0xf4 copy shadow marginConnection={}",
            fmt::ptr(&marginConnection));
        return;
    }

    const auto* blockB0 = &child.modulusBigIntB0;
    const auto* blockC4 = &child.publicExponentBigIntC4;
    const auto* blockD8 = &child.privateExponentBigIntD8;
    const CryptoPP::Integer modulus = AuthBootstrap680BigIntObjectToCryptoPPInteger(*blockB0);
    const CryptoPP::Integer publicExponent = AuthBootstrap680BigIntObjectToCryptoPPInteger(*blockC4);
    const CryptoPP::Integer privateExponent = AuthBootstrap680BigIntObjectToCryptoPPInteger(*blockD8);

    const bool storedReplyCopy =
        marginConnection.StoreBootstrapReplyCopy98(copyShadow, sizeof(*copyShadow));
    mxo::liblttcp::CMarginConnectionBootstrapPrepStateOwner_0x443340(marginConnection)
        .StoreBootstrapPrepStateA0(modulus, publicExponent, privateExponent);

    spdlog::info(
        "AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0::PrepareState5MarginConnectionCopySend staged owner+0x680 child for state5 copy/send copyShadowF4={} storedReplyCopy98={} childBlockB0Cap={} childBlockC4Cap={} childBlockD8Cap={} modulusBits={} publicExponentBits={} privateExponentBits={}",
        fmt::ptr(copyShadow),
        storedReplyCopy ? 1u : 0u,
        blockB0->capacityWords08,
        blockC4->capacityWords08,
        blockD8->capacityWords08,
        static_cast<unsigned>(modulus.BitCount()),
        static_cast<unsigned>(publicExponent.BitCount()),
        static_cast<unsigned>(privateExponent.BitCount()));
}

// Source-owned shared auth-reply materialization bridge for later child `+0xf4` consumers such as
// `0x433c0 -> 0x41b500 -> 0x41ce80 -> 0x441f30`.
// Bridge anchors:
// - launcher.exe:0x448140 raw `0x0b` success copies the validated `0x136` auth-data block into
//   child `+0xf4` and rebuilds child `+0xb0/+0xc4/+0xd8`
// - launcher.exe:0x43f300 consumes that child-side result on the broader state2 path
// - launcher.exe:0x4401a0 is the later state10 selected-slot auth-reply handler and does not call
//   `0x448140`
// The shared replacement helper is therefore a deliberate bridge between anchored original owners,
// not a newly claimed standalone launcher method.
void AuthBootstrap680MaterializeReplyCopyShadowScaffold(
    AuthBootstrap680Child_0x441290& child,
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    (void)mediator;
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject = child.authReplyParseObjectF0;
    ResetAuthBootstrap680ReplyMaterialization(child, ownedState);

    if (reply.isErrorReply || !reply.valid) {
        return;
    }

    ownedState.authReplyCopyShadowF4 = std::make_unique<AuthBootstrapReplyCopyShadowF4_0x44add0>();
    AuthBootstrapReplyCopyShadowF4_0x44add0& copyShadow = *ownedState.authReplyCopyShadowF4;
    copyShadow = {};

    // Prefer the exact copied parse-object auth-data field recovered from
    // `0x443470 / 0x448140`: that `0x136` span is the original child `+0xf4` material.
    if (parseObject != nullptr && parseObject->authDataBytes1c != nullptr &&
        parseObject->authDataByteLength20 == sizeof(copyShadow)) {
        std::copy_n(
            parseObject->authDataBytes1c,
            sizeof(copyShadow),
            reinterpret_cast<uint8_t*>(&copyShadow));
    } else if (reply.authDataBytes.size() == sizeof(copyShadow)) {
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

    AuthBootstrap680BigIntObjects_0x4ba50c* blockB0 = &child.modulusBigIntB0;
    AuthBootstrap680BigIntObjects_0x4ba50c* blockC4 = &child.publicExponentBigIntC4;
    AuthBootstrap680BigIntObjects_0x4ba50c* blockD8 = &child.privateExponentBigIntD8;

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
        ownedState.cachedAuthRequestBuildResult.twofishKeyBytes,
        ownedState.cachedAuthChallenge.encryptedChallengeBytes,
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
        "AuthBootstrap680MaterializeReplyCopyShadowScaffold materialized owner+0x680+0xf4 copyShadow bytes=0x{:03x} replyAuthDataBytes=0x{:03x} parseObjectAuthDataLen=0x{:04x} signaturePrefix00='{}' signedDataExpiryAc=0x{:08x} modulusPrefixD2='{}' authServerTimeBias80=0x{:08x} builtBlockB0={} builtBlockC4={} builtBlockD8={} blockB0Words={} blockC4Words={} blockD8Words={} parseObjectF0={}",
        static_cast<unsigned>(sizeof(copyShadow)),
        static_cast<unsigned>(reply.authDataBytes.size()),
        static_cast<unsigned>(parseObject ? parseObject->authDataByteLength20 : 0u),
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
        static_cast<unsigned>(blockD8->capacityWords08),
        fmt::ptr(parseObject));
}


// anchor: launcher.exe:0x448050
uint32_t AuthBootstrap680Child_0x441290::PrepareAndDispatch(
    CLTLoginMediator& mediator,
    void* sendTarget,
    const char* sessionTokenBegin) {
    auto& child = *this;

    child.loginType28 = 1u;

    const uint32_t* recoveredLauncherVersionPtr = mediator.GetNoPatchLauncherVersionValuePtr08();
    const uint32_t recoveredLauncherVersion =
        (recoveredLauncherVersionPtr && *recoveredLauncherVersionPtr != 0u)
            ? *recoveredLauncherVersionPtr
            : mediator.authLauncherVersion_;
    child.launcherVersion2C = recoveredLauncherVersion;

    const char* username = mediator.ownerAuthBootstrapSource94_.username00.data();
    const char* password = mediator.ownerAuthBootstrapSource94_.password20.data();
    const char* sessionToken = sessionTokenBegin ? sessionTokenBegin : "";

    AssignSmallStringMirror(
        child.string04,
        username,
        username + BoundedCStringLength(
            username,
            mediator.ownerAuthBootstrapSource94_.username00.size()));
    AssignSmallStringMirror(
        child.string10,
        password,
        password + BoundedCStringLength(
            password,
            mediator.ownerAuthBootstrapSource94_.password20.size()));
    AssignSmallStringMirror(child.string1C, sessionToken);

    if (!mediator.ownerAuthBootstrapSource94_.keyConfigMd540.empty()) {
        child.block30 = mediator.ownerAuthBootstrapSource94_.keyConfigMd540;
    }
    if (!mediator.ownerAuthBootstrapSource94_.uiConfigMd550.empty()) {
        child.block40 = mediator.ownerAuthBootstrapSource94_.uiConfigMd550;
    }

    child.sendTarget50 = sendTarget;

    const uint8_t authRequestReadyA0Value = child.authRequestReadyA0;
    const bool sendAuthRequestBranch = authRequestReadyA0Value != 0u;

    spdlog::info(
        "AuthBootstrap680Child_0x441290::PrepareAndDispatch staged owner+0x680 child (+0x04/+0x10/+0x1c/+0x28/+0x2c/+0x30..+0x4f/+0x50) from owner+0x94 len04={} len10={} len1C={} write28={} write2C={} currentPublicKeyId9C={} sendTarget50={} authRequestReadyA0=0x{:02x} branch={} child={} childBase={}",
        static_cast<unsigned>(SmallStringMirrorLength(child.string04)),
        static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
        static_cast<unsigned>(SmallStringMirrorLength(child.string1C)),
        static_cast<unsigned>(child.loginType28),
        static_cast<unsigned>(child.launcherVersion2C),
        static_cast<unsigned>(child.currentPublicKeyId9C),
        fmt::ptr(child.sendTarget50),
        static_cast<unsigned>(authRequestReadyA0Value),
        sendAuthRequestBranch ? "raw0x08/auth-request" : "raw0x06/get-public-key",
        fmt::ptr(this),
        fmt::ptr(static_cast<AuthBootstrap680ChildBase_0x4b7134*>(this)));

    if (sendAuthRequestBranch) {
        mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsAuthRequest;
        return child.SendAuthRequest(mediator);
    }

    mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsGetPublicKeyRequest;
    return child.SendGetPublicKeyRequest(mediator);
}

}  // namespace mxo::ltlogin

namespace mxo::liblttcp {

// anchor: launcher.exe:0x448140
uint32_t CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage(
    void* incomingAuthMessage,
    mxo::ltlogin::CLTLoginMediator& mediator) {
    using namespace mxo::ltlogin;

    auto* const module = owner08;
    AuthBootstrap680ChildBase_0x4b7134* childBase =
        module ? module->authBootstrapChildBase_SOURCEONLY : nullptr;
    if (childBase == nullptr) {
        childBase = &AuthBootstrapChildFromWriteHelper(*this);
    }
    if (childBase == nullptr) {
        return kAuthBootstrap680InboundUnhandled;
    }

    auto& child = *static_cast<AuthBootstrap680Child_0x441290*>(childBase);
    auto& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);

    IncomingAuthPayloadViewScaffold incomingPayload = {};
    if (!BuildIncomingAuthPayloadViewScaffold(
            static_cast<const CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessage),
            &incomingPayload)) {
        ownedState.stagedIncomingAuthPacketBytes.clear();
        return kAuthBootstrap680InboundUnhandled;
    }

    ownedState.stagedIncomingAuthPacketBytes.assign(
        incomingPayload.payloadBytes,
        incomingPayload.payloadBytes + incomingPayload.payloadByteCount);
    const std::vector<uint8_t>& stagedBytes = ownedState.stagedIncomingAuthPacketBytes;

    const uint8_t rawCode = incomingPayload.rawCode;
    switch (rawCode) {
        case CLTLoginMediator::kAuthRawCodeGetPublicKeyReply: {
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &reply)) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_GetPublicKeyReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            const mxo::auth::GetPublicKeyReply cachedPublicKeyReplyBeforeUpdate =
                ownedState.cachedGetPublicKeyReply;
            child.inboundAuthStatusEc = reply.status;
            if (reply.status != 0u) {
                ownedState.cachedGetPublicKeyReply = reply;
                return kAuthBootstrap680InboundGetPublicKeyReplyError;
            }

            child.authServerTimeBias80 = static_cast<uint32_t>(
                std::time(nullptr) - static_cast<std::time_t>(reply.currentTime));
            const uint32_t workerResult =
                HandleGetPublicKeyReply(mediator, reply);
            if (workerResult != 0u) {
                child.inboundAuthStatusEc = workerResult;
            }

            mxo::auth::GetPublicKeyReply effectiveReplyForSend = reply;
            const bool reusedCachedEmbeddedPublicKey =
                !reply.hasEmbeddedPublicKey &&
                cachedPublicKeyReplyBeforeUpdate.valid &&
                cachedPublicKeyReplyBeforeUpdate.hasEmbeddedPublicKey &&
                cachedPublicKeyReplyBeforeUpdate.publicKeyId == reply.publicKeyId &&
                currentPublicKeyId9C == reply.publicKeyId;
            if (reusedCachedEmbeddedPublicKey) {
                effectiveReplyForSend = cachedPublicKeyReplyBeforeUpdate;
                effectiveReplyForSend.status = reply.status;
                effectiveReplyForSend.currentTime = reply.currentTime;
                effectiveReplyForSend.publicKeyId = reply.publicKeyId;
            }
            ownedState.cachedGetPublicKeyReply = effectiveReplyForSend;

            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status={} currentTime={} publicKeyId={} keySize={} modulusLength={} signatureLength={} exponentByte=0x{:02x} hasEmbeddedPublicKey={} reusedCachedEmbeddedPublicKey={} workerResult=0x{:08x} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} helper={} module={} childBase={}",
                static_cast<unsigned>(reply.status),
                static_cast<unsigned>(reply.currentTime),
                static_cast<unsigned>(reply.publicKeyId),
                static_cast<unsigned>(reply.keySize),
                static_cast<unsigned>(reply.modulusLength),
                static_cast<unsigned>(reply.signatureLength),
                static_cast<unsigned>(reply.publicExponentByte),
                reply.hasEmbeddedPublicKey ? 1u : 0u,
                reusedCachedEmbeddedPublicKey ? 1u : 0u,
                static_cast<unsigned>(workerResult),
                fmt::ptr(child.lazyPubkeyDatValidatorA4),
                fmt::ptr(child.raw08PublicKeyWorkerA8),
                fmt::ptr(child.replyAuthDataValidatorAC),
                fmt::ptr(this),
                fmt::ptr(module),
                fmt::ptr(childBase));

            if (workerResult != 0u) {
                return kAuthBootstrap680InboundGetPublicKeyWorkerError;
            }

            return SendAuthRequest(mediator) != 0u
                ? kAuthBootstrap680InboundHandledContinueWaiting
                : kAuthBootstrap680InboundGetPublicKeyWorkerError;
        }

        case 0x09u: {
            mxo::auth::AuthChallenge challenge;
            if (!mxo::auth::ParseAuthChallengePayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &challenge)) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthChallenge");
                return kAuthBootstrap680InboundUnhandled;
            }

            ownedState.cachedAuthChallenge = challenge;
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen={}",
                challenge.encryptedChallengeBytes.size());

            {
                const char* password = SmallStringMirrorDataOrEmpty(child.string10);
                const char* soePassword = SmallStringMirrorDataOrEmpty(child.string1C);
                if (SmallStringMirrorLength(child.string10) == 0u) {
                    spdlog::error(
                        "launcher-owned auth received AS_AuthChallenge but child+0x10 password data is empty");
                    return kAuthBootstrap680InboundUnhandled;
                }
                if (SmallStringMirrorLength(child.string1C) == 0u) {
                    spdlog::warn(
                        "launcher-owned auth raw0x0a using empty child+0x1c secondary password/station field while preserving the recovered 0x44831c field mapping");
                }
                if (ownedState.cachedAuthRequestBuildResult.twofishKeyBytes.size() != 16u) {
                    spdlog::error("launcher-owned auth missing Twofish key from AS_AuthRequest build result");
                    return kAuthBootstrap680InboundUnhandled;
                }
                if (!child.sendTarget50) {
                    spdlog::warn(
                        "AuthBootstrap680 raw0x0a challenge-response missing child+0x50 send target; refusing less-faithful fallback path");
                    return kAuthBootstrap680InboundUnhandled;
                }

                mxo::auth::AuthChallengeResponseLayout layout;
                mxo::auth::AuthChallengeResponseBuildResult buildResult;
                if (!mxo::auth::BuildAuthChallengeResponsePacket(
                        challenge.encryptedChallengeBytes,
                        ownedState.cachedAuthRequestBuildResult.twofishKeyBytes,
                        password,
                        soePassword,
                        layout,
                        mxo::auth::kFrameModeAuto,
                        &buildResult)) {
                    spdlog::error("launcher-owned auth failed to build AS_AuthChallengeResponse");
                    return kAuthBootstrap680InboundUnhandled;
                }

                Packet_MsClaimCharacterNameRequest_0x4b6cf4 plaintextPacket;
                plaintextPacket.ResetAndInitialize();

                if (buildResult.processedChallengeMd5Bytes.size() >= 16u) {
                    uint8_t* payload = static_cast<uint8_t*>(plaintextPacket.payloadAlias10);
                    if (payload) {
                        std::copy_n(buildResult.processedChallengeMd5Bytes.begin(), 16u, payload + 1u);
                    }
                }

                plaintextPacket.AppendEncryptedChallenge(password);
                plaintextPacket.AppendPassword(soePassword);
                plaintextPacket.ReserveFieldLength(0x20u);

                const size_t plaintextLen = buildResult.plaintextBytes.size();
                uint16_t paddingBytes = static_cast<uint16_t>(0x20u - (plaintextLen & 0x0fu));
                if (paddingBytes == 0x20u) {
                    paddingBytes = 0u;
                }
                plaintextPacket.SetPadding(paddingBytes);

                Packet_MsClaimCharacterNameRequest_0x4b6d08 encryptedPacket;
                encryptedPacket.InitializePayloadSize();
                encryptedPacket.ReserveLengthPrefixedTail(
                    static_cast<uint16_t>(buildResult.ciphertextBytes.size()));
                if (buildResult.ciphertextBytes.size() != 0u && encryptedPacket.debugString14 != nullptr) {
                    std::memcpy(
                        const_cast<char*>(encryptedPacket.debugString14),
                        buildResult.ciphertextBytes.data(),
                        buildResult.ciphertextBytes.size());
                }

                auto* sendTarget = static_cast<CMessageConnection_0x4b7928*>(child.sendTarget50);
                sendTarget->SendPacketMessageRef(*encryptedPacket.messageRef08);

                const uint32_t sendResult = 1u;

                spdlog::debug(
                    "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage observed raw0x0a send using Packet_MsClaimCharacterNameRequest_0x4b6cf4 + Packet_MsClaimCharacterNameRequest_0x4b6d08 decryptedChallengeBytes={} processedChallengeMd5Bytes={}",
                    static_cast<unsigned>(buildResult.decryptedChallengeBytes.size()),
                    static_cast<unsigned>(buildResult.processedChallengeMd5Bytes.size()));
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned auth built/sent AS_AuthChallengeResponse passwordLengthField={} soePasswordLengthField={} plaintextLen={} ciphertextLen={} childString10Len={} childString1CLen={} sendTarget50={} helper={} module={} childBase={}",
                    static_cast<unsigned>(buildResult.passwordLengthField),
                    static_cast<unsigned>(buildResult.soePasswordLengthField),
                    static_cast<unsigned>(buildResult.plaintextBytes.size()),
                    static_cast<unsigned>(buildResult.ciphertextBytes.size()),
                    static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
                    static_cast<unsigned>(SmallStringMirrorLength(child.string1C)),
                    fmt::ptr(child.sendTarget50),
                    fmt::ptr(this),
                    fmt::ptr(module),
                    fmt::ptr(childBase));
                return sendResult != 0u
                    ? kAuthBootstrap680InboundHandledContinueWaiting
                    : kAuthBootstrap680InboundUnhandled;
            }
        }

        case 0x0bu: {
            mxo::auth::AuthReply reply;
            if (!mxo::auth::ParseAuthReplyPayload(
                    stagedBytes.data(),
                    stagedBytes.size(),
                    &reply)) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            const bool storedParseObjectF0 =
                StoreAuthBootstrap680AuthReplyParseObjectFromStagedPacket(
                    mediator,
                    child,
                    stagedBytes);
            const auto* parseObject = child.authReplyParseObjectF0;

            ownedState.cachedAuthReply = reply;
            child.inboundAuthStatusEc =
                storedParseObjectF0
                    ? ReadAuthBootstrap680AuthReplyParseHeaderDword(parseObject, 0x01u)
                    : (reply.isErrorReply ? reply.errorCode : 0u);

            spdlog::debug(
                "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage stored child+0xf0 parse copy={} status=0x{:08x} authDataLen=0x{:04x} encryptedPrivateExponentLen=0x{:04x} characterCount={} worldCount={} replyString1dLen=0x{:04x}",
                storedParseObjectF0 ? 1u : 0u,
                static_cast<unsigned>(child.inboundAuthStatusEc),
                static_cast<unsigned>(parseObject ? parseObject->authDataByteLength20 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->encryptedPrivateExponentByteLength28 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->characterTempRecordCount40 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->worldTempRecordCount48 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->replyString1dByteLength58 : 0u));

            if (reply.isErrorReply) {
                return kAuthBootstrap680InboundAuthReplyError;
            }

            const uint16_t authDataByteLength =
                parseObject ? parseObject->authDataByteLength20
                            : static_cast<uint16_t>(reply.authDataBytes.size());
            if (!reply.valid || !reply.signedData.valid ||
                authDataByteLength != sizeof(AuthBootstrapReplyCopyShadowF4_0x44add0) ||
                child.replyAuthDataValidatorAC == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrapReplyCopyShadowF4_0x44add0 copyShadowCandidate = {};
            if (parseObject != nullptr && parseObject->authDataBytes1c != nullptr &&
                parseObject->authDataByteLength20 == sizeof(copyShadowCandidate)) {
                std::copy_n(
                    parseObject->authDataBytes1c,
                    sizeof(copyShadowCandidate),
                    reinterpret_cast<uint8_t*>(&copyShadowCandidate));
            } else if (reply.authDataBytes.size() == sizeof(copyShadowCandidate)) {
                std::copy(
                    reply.authDataBytes.begin(),
                    reply.authDataBytes.end(),
                    reinterpret_cast<uint8_t*>(&copyShadowCandidate));
            } else {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            const bool replyAuthDataValidatorAccepted =
                copyShadowCandidate.VerifyWithValidator(
                    ownedState.replyAuthDataValidatorAC.object.get(),
                    ownedState.replyAuthDataValidatorAC.publicKeyPair0c,
                    child.authServerTimeBias80);
            if (!replyAuthDataValidatorAccepted) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrap680MaterializeReplyCopyShadowScaffold(child, mediator, reply);
            return kAuthBootstrap680InboundAuthReplySuccess;
        }

        default:
            break;
    }

    return kAuthBootstrap680InboundUnhandled;
}

}  // namespace mxo::liblttcp

namespace mxo::ltlogin {

// anchor: launcher.exe:0x447eb0
uint32_t AuthBootstrap680Child_0x441290::SendGetPublicKeyRequest(
    CLTLoginMediator& mediator) {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(this);
    bool ensuredLazyPubkeyDatValidatorA4 = false;
    if (lazyPubkeyDatValidatorA4 != nullptr && ownedState.lazyPubkeyDatValidatorA4.object) {
        ensuredLazyPubkeyDatValidatorA4 = true;
    } else {
        // anchor: launcher.exe:0x447260 / 0x447c10
        ownedState.lazyPubkeyDatValidatorA4.object =
            std::make_unique<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch>();
        if (ownedState.lazyPubkeyDatValidatorA4.object->ConstructFromReplyPublicKey(
                &ownedState.lazyPubkeyDatValidatorA4.publicKeyPair0c,
                kAuthBootstrap680PubkeyDatFallbackModulus.data(),
                kAuthBootstrap680PubkeyDatFallbackModulus.size(),
                0x11u)) {
            lazyPubkeyDatValidatorA4 = ownedState.lazyPubkeyDatValidatorA4.object.get();
            ensuredLazyPubkeyDatValidatorA4 = true;
        } else {
            ownedState.lazyPubkeyDatValidatorA4.object.reset();
            lazyPubkeyDatValidatorA4 = nullptr;
        }
    }

    Packet_AsGetPublicKeyRequest_0x4b6c74 packetBuilder;
    packetBuilder.InitializePayloadSize();
    uint8_t* const getPublicKeyRequestPayload = packetBuilder.PayloadBase();
    if (!getPublicKeyRequestPayload) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to initialize Packet_AsGetPublicKeyRequest_0x4b6c74 payload");
        return 0;
    }

    getPublicKeyRequestPayload[0] = Packet_AsGetPublicKeyRequest_0x4b6c74::kPayloadTag06;
    *reinterpret_cast<uint32_t*>(
        getPublicKeyRequestPayload + Packet_AsGetPublicKeyRequest_0x4b6c74::kLauncherVersionOffset) =
        launcherVersion2C;
    *reinterpret_cast<uint32_t*>(
        getPublicKeyRequestPayload + Packet_AsGetPublicKeyRequest_0x4b6c74::kCurrentPublicKeyIdOffset) =
        currentPublicKeyId9C;

    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildVariableLengthPacket(
            getPublicKeyRequestPayload,
            Packet_AsGetPublicKeyRequest_0x4b6c74::kFixedByteCount,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to frame Packet_AsGetPublicKeyRequest_0x4b6c74");
        return 0;
    }

    if (!sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendGetPublicKeyRequest missing child+0x50 send target; recovered 0x447eb0 tail expects direct virtual send through that field");
        return 0u;
    }

    auto* sendTarget = static_cast<mxo::liblttcp::CBaseConnection*>(sendTarget50);
    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    const uint32_t sendResult = sendTarget->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via 0x447eb0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} ensuredLazyPubkeyDatValidatorA4={} childLazyPubkeyDatValidatorA4={} child={} helperOwner={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsGetPublicKeyRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        fmt::ptr(sendTarget50),
        ensuredLazyPubkeyDatValidatorA4 ? 1u : 0u,
        fmt::ptr(lazyPubkeyDatValidatorA4),
        fmt::ptr(this),
        fmt::ptr(owner08),
        static_cast<unsigned>(sendResult));
    mediator.authGetPublicKeyRequestSent_ = (sendResult != 0u);
    return sendResult;
}

}  // namespace mxo::ltlogin

namespace mxo::liblttcp {

// anchor: launcher.exe:0x4474f0
uint32_t CStreamPacketEncryptionModuleWriteHelper_0x4b8690::SendAuthRequest(
    mxo::ltlogin::CLTLoginMediator& mediator) {
    using namespace mxo::ltlogin;

    AuthBootstrap680ChildBase_0x4b7134* childBase =
        owner08 ? owner08->authBootstrapChildBase_SOURCEONLY : nullptr;
    if (childBase == nullptr) {
        childBase = &AuthBootstrapChildFromWriteHelper(*this);
    }
    auto& child = *static_cast<AuthBootstrap680Child_0x441290*>(childBase);
    auto& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);
    const mxo::auth::GetPublicKeyReply& reply = ownedState.cachedGetPublicKeyReply;
    if (!reply.valid || !reply.hasEmbeddedPublicKey) {
        spdlog::warn(
            "AuthBootstrap680_SendAuthRequest missing cached valid embedded-public-key reply at child-owned state while authRequestReadyA0=0x{:02x}",
            static_cast<unsigned>(authRequestReadyA0));
        return 0u;
    }

    const char* username = SmallStringMirrorDataOrEmpty(child.string04);
    if (SmallStringMirrorLength(child.string04) == 0u) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without child+0x04 username data");
        return 0;
    }
    if (child.raw08PublicKeyWorkerA8 == nullptr ||
        ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c.modulusBytes.empty() ||
        ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c.exponentBytes.empty()) {
        spdlog::warn(
            "AuthBootstrap680_SendAuthRequest missing child+0xa8 raw08 worker material; recovered 0x4474f0 consumes that worker through 0x468ea0/0x468f00");
        return 0u;
    }

    currentPublicKeyId9C = reply.publicKeyId;

    FillAuthBootstrap680Field54SeedBytesScaffold(child.feedbackSeedHelper54, feedbackSeed84);
    ResetAuthBootstrap680FeedbackTransforms(child, ownedState);

    auto feedbackTransformLarge94 =
        std::make_unique<mxo::auth::internal::FeedbackSizeTransformAdapterLarge>();
    if (feedbackTransformLarge94->FeedbackSizeTransformAdapter_ConstructLarge(
            feedbackSeed84.data(),
            static_cast<uint32_t>(feedbackSeed84.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data(),
            0u)) {
        child.feedbackTransformLarge94 = feedbackTransformLarge94.get();
        ownedState.feedbackTransformLarge94 = std::move(feedbackTransformLarge94);
    }

    auto feedbackTransformSmall98 =
        std::make_unique<mxo::auth::internal::FeedbackSizeTransformAdapterSmall>();
    if (feedbackTransformSmall98->FeedbackSizeTransformAdapter_ConstructSmall(
            feedbackSeed84.data(),
            static_cast<uint32_t>(feedbackSeed84.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data(),
            0u)) {
        child.feedbackTransformSmall98 = feedbackTransformSmall98.get();
        ownedState.feedbackTransformSmall98 = std::move(feedbackTransformSmall98);
    }

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = static_cast<uint8_t>(child.loginType28 & 0xffu);
    requestLayout.keyConfigMd5.assign(block30.begin(), block30.end());
    requestLayout.uiConfigMd5.assign(block40.begin(), block40.end());

    mxo::auth::AuthRequestBuildResult buildResult;
    buildResult.includedUsernameNullTerminator = blobLayout.includeUsernameNullTerminator;
    buildResult.usedFixedHeaderOverride = !requestLayout.fixedHeaderBytes.empty();
    buildResult.usedProvidedPublicKey = true;

    if (!mxo::auth::BuildAuthRequestBlobPlaintext(
            username,
            blobLayout,
            &buildResult.blobPlaintextBytes,
            &buildResult.twofishKeyBytes,
            &buildResult.usernameLengthField)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest plaintext blob from child+0x04/+0x28/+0x30..+0x4f state");
        return 0;
    }

    if (requestLayout.fixedHeaderBytes.empty()) {
        if (!mxo::auth::internal::BuildDefaultAuthHeaderBytes(
                requestLayout,
                &buildResult.authHeaderBytes,
                &buildResult.keyConfigMd5Bytes,
                &buildResult.uiConfigMd5Bytes)) {
            spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest fixed auth header bytes");
            return 0;
        }
    } else {
        if (requestLayout.fixedHeaderBytes.size() != 35u) {
            spdlog::info("DIAGNOSTIC: launcher-owned auth rejected AS_AuthRequest fixed header override size={}", requestLayout.fixedHeaderBytes.size());
            return 0;
        }
        buildResult.authHeaderBytes = requestLayout.fixedHeaderBytes;
        buildResult.keyConfigMd5Bytes.assign(
            buildResult.authHeaderBytes.begin() + 3u,
            buildResult.authHeaderBytes.begin() + 19u);
        buildResult.uiConfigMd5Bytes.assign(
            buildResult.authHeaderBytes.begin() + 19u,
            buildResult.authHeaderBytes.end());
    }

    if (!child.raw08PublicKeyWorkerA8->EncryptPlaintextIntoCiphertextScaffold(
            ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c,
            buildResult.blobPlaintextBytes.data(),
            buildResult.blobPlaintextBytes.size(),
            &buildResult.blobCiphertextBytes)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to encrypt AS_AuthRequest blob through child+0xa8 raw08 worker scaffold");
        return 0;
    }
    if (buildResult.blobCiphertextBytes.size() > 0xffffu) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth rejected oversized AS_AuthRequest ciphertext len={}", buildResult.blobCiphertextBytes.size());
        return 0;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1u + 4u + buildResult.authHeaderBytes.size() + 2u + buildResult.blobCiphertextBytes.size());
    payload.push_back(0x08u);
    mxo::auth::internal::AppendU32LE(&payload, requestLayout.publicKeyId);
    payload.insert(
        payload.end(),
        buildResult.authHeaderBytes.begin(),
        buildResult.authHeaderBytes.end());
    mxo::auth::internal::AppendU16LE(
        &payload,
        static_cast<uint16_t>(buildResult.blobCiphertextBytes.size()));
    payload.insert(
        payload.end(),
        buildResult.blobCiphertextBytes.begin(),
        buildResult.blobCiphertextBytes.end());
    if (!mxo::auth::BuildVariableLengthPacket(
            payload.data(),
            payload.size(),
            mxo::auth::kFrameModeAuto,
            &buildResult.packet)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to frame AS_AuthRequest packet bytes");
        return 0;
    }

    const uint32_t raw08WorkerExpectedBlobLen =
        child.raw08PublicKeyWorkerA8->QueryEncryptedOutputLengthScaffold(
            ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c,
            buildResult.blobPlaintextBytes.size());

    ownedState.cachedAuthRequestBuildResult = buildResult;
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
        "DIAGNOSTIC: launcher-owned auth send via 0x4474f0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} helper={} module={} childBase={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsAuthRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        buildResult.packet.headerBytes.size(),
        buildResult.packet.payloadBytes.size(),
        buildResult.packet.bytes.size(),
        fmt::ptr(child.sendTarget50),
        fmt::ptr(this),
        fmt::ptr(owner08),
        fmt::ptr(childBase),
        static_cast<unsigned>(sendResult));
    mediator.authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId={} loginType={} keySize={} blobLen={} raw08WorkerExpectedBlobLen={} usernameLengthField={} usedChildRaw08PublicKeyWorker={} keyConfigMd5Len={} uiConfigMd5Len={} childSendTarget50={} childRaw08PublicKeyWorkerA8={} childString04Len={} childString10Len={} childString1CLen={} feedbackSeed84='{}' helper54NextBufferedByte28=0x{:08x}",
            static_cast<unsigned>(reply.publicKeyId),
            static_cast<unsigned>(requestLayout.loginType),
            static_cast<unsigned>(reply.keySize),
            static_cast<unsigned>(buildResult.blobCiphertextBytes.size()),
            static_cast<unsigned>(raw08WorkerExpectedBlobLen),
            static_cast<unsigned>(buildResult.usernameLengthField),
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            static_cast<unsigned>(buildResult.keyConfigMd5Bytes.size()),
            static_cast<unsigned>(buildResult.uiConfigMd5Bytes.size()),
            fmt::ptr(child.sendTarget50),
            fmt::ptr(child.raw08PublicKeyWorkerA8),
            static_cast<unsigned>(SmallStringMirrorLength(child.string04)),
            static_cast<unsigned>(SmallStringMirrorLength(child.string10)),
            static_cast<unsigned>(SmallStringMirrorLength(child.string1C)),
            BuildHexPreview(feedbackSeed84.data(), feedbackSeed84.size(), feedbackSeed84.size()),
            static_cast<unsigned>(child.feedbackSeedHelper54.nextBufferedOutputByte28));
    }
    return sendResult;
}

// anchor: launcher.exe:0x447780
uint32_t CStreamPacketEncryptionModuleWriteHelper_0x4b8690::RebuildReplyPublicKeyWorkers(
    const mxo::auth::GetPublicKeyReply& reply) {
    using namespace mxo::ltlogin;

    AuthBootstrap680ChildBase_0x4b7134* childBase =
        owner08 ? owner08->authBootstrapChildBase_SOURCEONLY : nullptr;
    if (childBase == nullptr) {
        childBase = &AuthBootstrapChildFromWriteHelper(*this);
    }
    auto& child = *static_cast<AuthBootstrap680Child_0x441290*>(childBase);
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&child);

    ResetAuthBootstrap680ReplyPublicKeyWorkers(child, ownedState);
    currentPublicKeyId9C = reply.publicKeyId;

    ownedState.raw08PublicKeyWorkerA8.object =
        std::make_unique<AuthBootstrap680Raw08PublicKeyWorkerA8Sketch>();
    if (ownedState.raw08PublicKeyWorkerA8.object->ConstructFromReplyPublicKey(
            &ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c,
            reply.modulusBytes.data(),
            reply.modulusBytes.size(),
            reply.publicExponentByte)) {
        child.raw08PublicKeyWorkerA8 = ownedState.raw08PublicKeyWorkerA8.object.get();
    } else {
        ownedState.raw08PublicKeyWorkerA8.object.reset();
    }

    ownedState.replyAuthDataValidatorAC.object =
        std::make_unique<AuthBootstrap680ReplyAuthDataValidatorACSketch>();
    if (ownedState.replyAuthDataValidatorAC.object->ConstructFromReplyPublicKey(
            &ownedState.replyAuthDataValidatorAC.publicKeyPair0c,
            reply.modulusBytes.data(),
            reply.modulusBytes.size(),
            reply.publicExponentByte)) {
        child.replyAuthDataValidatorAC = ownedState.replyAuthDataValidatorAC.object.get();
    } else {
        ownedState.replyAuthDataValidatorAC.object.reset();
    }

    return (child.raw08PublicKeyWorkerA8 != nullptr && child.replyAuthDataValidatorAC != nullptr)
               ? 0u
               : 1u;
}

// anchor: launcher.exe:0x447f50 / 0x447780 / 0x447260 / 0x447c10
uint32_t CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleGetPublicKeyReply(
    mxo::ltlogin::CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    using namespace mxo::ltlogin;

    AuthBootstrap680ChildBase_0x4b7134* childBase =
        owner08 ? owner08->authBootstrapChildBase_SOURCEONLY : nullptr;
    if (childBase == nullptr) {
        childBase = &AuthBootstrapChildFromWriteHelper(*this);
    }
    auto& child = *static_cast<AuthBootstrap680Child_0x441290*>(childBase);

    if (reply.publicKeyId == currentPublicKeyId9C) {
        authRequestReadyA0 = 1u;
        return 0u;
    }

    if (g_SkipAuthPublicKeyReplyValidation == 0u) {
        if (!reply.hasEmbeddedPublicKey || reply.modulusBytes.size() != 0x80u || reply.publicExponentByte == 0u) {
            authRequestReadyA0 = 0u;
            return 1u;
        }
    } else {
        if (!reply.hasEmbeddedPublicKey || reply.modulusBytes.empty() || reply.publicExponentByte == 0u) {
            authRequestReadyA0 = 0u;
            return 1u;
        }
        spdlog::info(
            "DIAGNOSTIC: skipping AS_GetPublicKeyReply modulus size validation (g_SkipAuthPublicKeyReplyValidation=1) modulusBytes={}",
            reply.modulusBytes.size());
    }
    const bool lazyPubkeyDatValidatorAccepted =
        AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(child, reply);
    if (!lazyPubkeyDatValidatorAccepted) {
        authRequestReadyA0 = 0u;
        return 1u;
    }

    const uint32_t rebuildResult = RebuildReplyPublicKeyWorkers(reply);
    if (rebuildResult != 0u) {
        authRequestReadyA0 = 0u;
        return rebuildResult;
    }

    authRequestReadyA0 = 1u;
    spdlog::info(
        "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleGetPublicKeyReply rebuilt child+0xa8/+0xac from raw0x07 publicKeyId={} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} modulusLen={} signatureLen={} exponentByte=0x{:02x} helper={} module={} childBase={}",
        static_cast<unsigned>(reply.publicKeyId),
        fmt::ptr(child.lazyPubkeyDatValidatorA4),
        fmt::ptr(child.raw08PublicKeyWorkerA8),
        fmt::ptr(child.replyAuthDataValidatorAC),
        static_cast<unsigned>(reply.modulusBytes.size()),
        static_cast<unsigned>(reply.signatureBytes.size()),
        static_cast<unsigned>(reply.publicExponentByte),
        fmt::ptr(this),
        fmt::ptr(owner08),
        fmt::ptr(childBase));
    return 0u;
}

}  // namespace mxo::liblttcp
