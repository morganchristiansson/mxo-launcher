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
#include "../../../runtime/src/libltcrypto/auth_internal.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <random>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

struct AuthBootstrap680RsaPublicKeyPairOwnedState {
    std::vector<uint32_t> modulus08OwnedDigits;
    std::vector<uint32_t> exponent1cOwnedDigits;
    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> exponentBytes;
};

namespace {

// anchor: launcher.exe:DAT_004f79e0
static bool g_authBootstrap680State2AuthReplySuccessOneTimeGate = false;

// Keep non-layout ownership outside `AuthBootstrap680ChildSketch` so the child mirror can stay
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
    std::unique_ptr<AuthBootstrap680AuthReplyParseObjectF0Sketch> authReplyParseObjectF0;
    std::vector<uint8_t> authReplyParsePacketBodyBytes;
    std::unique_ptr<AuthBootstrapReplyCopyShadowF4Sketch> authReplyCopyShadowF4;
    std::vector<uint32_t> modulusBigIntB0OwnedDigits;
    std::vector<uint32_t> publicExponentBigIntC4OwnedDigits;
    std::vector<uint32_t> privateExponentBigIntD8OwnedDigits;
    std::vector<uint8_t> opaqueReplyBlob108Owned;
    std::vector<uint8_t> opaqueReplyBlob10COwned;
};

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

static bool BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
    AuthBootstrap680BigIntObject20Scaffold* outObject,
    std::vector<uint32_t>* ownedDigits,
    const uint8_t* bigEndianBytes,
    size_t byteCount);
static bool BuildPositiveAuthBootstrap680BigIntFromUnsignedByte(
    AuthBootstrap680BigIntObject20Scaffold* outObject,
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

static void ResetAuthBootstrap680LazyPubkeyDatValidatorA4(
    AuthBootstrap680LazyPubkeyDatValidatorA4Sketch* outValidator,
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
    AuthBootstrap680ChildSketch& child,
    AuthBootstrap680ChildOwnedState& ownedState) {
    child.raw08PublicKeyWorkerA8 = nullptr;
    child.replyAuthDataValidatorAC = nullptr;

    ownedState.raw08PublicKeyWorkerA8.object.reset();
    ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.raw08PublicKeyWorkerA8.publicKeyPair0c);
    ownedState.replyAuthDataValidatorAC.object.reset();
    ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.replyAuthDataValidatorAC.publicKeyPair0c);
}

// anchor: launcher.exe:0x447260 / 0x447c10
// Current bounded source mirror keeps the concrete lazy child `+0xa4` validator object typed and
// seeded from the recovered fallback modulus at `g_AuthBootstrap680PubkeyDatFallbackModulus100`.
// Static `0x447eb0 -> 0x447c10` now proves the non-fallback branch opens `qspubkey.dat`, reads a
// cached `(publicKeyId, modulus big-int, exponent big-int, 0x100-byte signature)` family, and
// then re-enters `0x447780 = AuthBootstrap680_RebuildReplyPublicKeyWorkers` with that same call
// shape. Current live runs also show the file absent in the shipped runtime directory used here,
// so launcher behavior on this path currently falls back to the embedded modulus as expected. The
// exact file-reader/object stack and on-disk field framing are still not recovered tightly enough
// to replace the current fallback-only source model.
static bool EnsureAuthBootstrap680LazyPubkeyDatValidatorA4FallbackScaffold(
    CLTLoginMediator& mediator,
    AuthBootstrap680ChildSketch& child) {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    if (child.lazyPubkeyDatValidatorA4 != nullptr && ownedState.lazyPubkeyDatValidatorA4.object) {
        return true;
    }

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
    return true;
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
    AuthBootstrap680ChildSketch& child,
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
    CLTLoginMediator& mediator,
    AuthBootstrap680ChildSketch& child,
    const std::vector<uint8_t>& stagedBytes) {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
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

// anchor: launcher.exe:0x44ae40
static void BuildAuthBootstrapReplyCopyShadowF4SignedDataMd5Digest10Scaffold(
    const AuthBootstrapReplyCopyShadowF4Sketch& copyShadow,
    std::array<uint8_t, 16>* outDigest10) {
    if (!outDigest10) {
        return;
    }

    CryptoPP::Weak::MD5 md5;
    md5.Update(copyShadow.signedData80.data(), copyShadow.signedData80.size());
    md5.Final(outDigest10->data());
}

// anchor: launcher.exe:0x468f80
static bool VerifyAuthBootstrap680ReplyPublicKeyAgainstLazyPubkeyDatValidatorScaffold(
    CLTLoginMediator& mediator,
    AuthBootstrap680ChildSketch& child,
    const mxo::auth::GetPublicKeyReply& reply) {
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    if (!EnsureAuthBootstrap680LazyPubkeyDatValidatorA4FallbackScaffold(mediator, child) ||
        child.lazyPubkeyDatValidatorA4 == nullptr) {
        return false;
    }
    if (reply.modulusBytes.size() != 0x80u || reply.signatureBytes.empty() || reply.publicExponentByte == 0u) {
        return false;
    }

    // `0x468f80` now closes the caller-side byte span tightly:
    // - convert the reply modulus big-int to exactly `0x80` bytes
    // - append the exponent low byte read from bit offset `0`
    // - pass the reply signature buffer to validator vtable `+0x2c` with fixed byte count `0x100`
    // Combined with the recovered finalize path, source now mirrors this exactly as:
    // - worker MD5 over those `0x81` bytes
    // - EMSA-PKCS1-v1_5(MD5) block build
    // - byte-for-byte compare against the RSA-decoded representative
    // Current live fallback-key runs now pass through this source-side validator path.
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

// anchor: launcher.exe:0x44aec0 / 0x44add0
static bool VerifyAuthBootstrap680ReplyCopyShadowF4WithValidatorScaffold(
    const AuthBootstrap680ChildSketch& child,
    const AuthBootstrap680ReplyAuthDataValidatorOwnedState& validatorOwnedState,
    const AuthBootstrapReplyCopyShadowF4Sketch& copyShadow) {
    const std::time_t now = std::time(nullptr);
    const uint32_t currentAuthServerTime =
        (now > static_cast<std::time_t>(child.authServerTimeBias80))
            ? static_cast<uint32_t>(now - static_cast<std::time_t>(child.authServerTimeBias80))
            : 0u;
    const uint32_t expiryTimeAc = ReadU32LE(copyShadow.signedData80.data() + 0x2cu);
    if (currentAuthServerTime >= expiryTimeAc) {
        return false;
    }

    // `0x44ae40/0x44aec0` now closes the caller-side bytes tightly too:
    // - build MD5 over copied signed-data `this+0x80 .. +0x135` (`0xb6` bytes)
    // - then call validator vtable `+0x2c(md5Digest10, 0x10, this, 0x80)`
    // Combined with the recovered finalize path, source now mirrors this exactly as a second MD5
    // stage inside the worker before the EMSA-PKCS1-v1_5(MD5) compare. Current live runs now pass
    // this source-side validator path too, so the copied `0xb6`-byte span and `0x80`-byte
    // signature slice are now enforced instead of bypassed.
    std::array<uint8_t, 16> md5Digest10{};
    BuildAuthBootstrapReplyCopyShadowF4SignedDataMd5Digest10Scaffold(copyShadow, &md5Digest10);
    return child.replyAuthDataValidatorAC->VerifySignatureRecoveredFinalizeScaffold(
        validatorOwnedState.publicKeyPair0c,
        md5Digest10.data(),
        md5Digest10.size(),
        copyShadow.authSignature00.data(),
        copyShadow.authSignature00.size());
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

static uint32_t ReadAuthBootstrap680AuthReplyParseHeaderDword(
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject,
    size_t headerOffset) {
    if (!parseObject || !parseObject->replyHeader10) {
        return 0u;
    }
    return ReadU32LE(parseObject->replyHeader10 + headerOffset);
}

static std::string CopyAuthBootstrap680ReplyParseString(
    const uint8_t* bytes,
    uint16_t byteLength) {
    if (!bytes || byteLength == 0u) {
        return {};
    }

    const char* text = reinterpret_cast<const char*>(bytes);
    return std::string(text, BoundedCStringLength(text, byteLength));
}

static void CopyAuthBootstrap680ParseFieldToOwnedBytes(
    std::vector<uint8_t>& ownedBytes,
    const uint8_t* bytes,
    uint16_t byteLength) {
    if (!bytes || byteLength == 0u) {
        ownedBytes.clear();
        return;
    }
    ownedBytes.assign(bytes, bytes + byteLength);
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
    // Preserve the recovered owner+0x94 -> child copy flow as the primary model.
    // The original `0x439210 -> 0x448050` call shape only feeds owner `+0x94`; any fallback to
    // wrapper-facing auth strings remains a bounded replacement-only escape hatch for paths that
    // still reach this child without first passing through `0x41ecd0 = ProcessLoginRequest`.
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
        mediator.Arg6AuthName(),
        mediator.Arg6AuthPassword(),
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

            // Keep child `+0x9c` on the *previous* public-key id while mirroring `0x447f50`.
            // Static RE compares the incoming raw-`0x07` id against the existing child field and
            // only rebuilds `+0xa8/+0xac` when that comparison differs. So do not pre-write
            // owner/child current-public-key mirrors here before the rebuild helper runs.
            child.authServerTimeBias80 = static_cast<uint32_t>(
                std::time(nullptr) - static_cast<std::time_t>(reply.currentTime));
            const uint32_t workerResult =
                mediator.SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(reply);
            if (workerResult == 0u) {
                mediator.authCurrentPublicKeyId_ = child.currentPublicKeyId9C;
            } else {
                child.inboundAuthStatusEc = workerResult;
            }

            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status={} currentTime={} publicKeyId={} keySize={} modulusLength={} signatureLength={} exponentByte=0x{:02x} hasEmbeddedPublicKey={} workerResult=0x{:08x} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={}",
                static_cast<unsigned>(reply.status),
                static_cast<unsigned>(reply.currentTime),
                static_cast<unsigned>(reply.publicKeyId),
                static_cast<unsigned>(reply.keySize),
                static_cast<unsigned>(reply.modulusLength),
                static_cast<unsigned>(reply.signatureLength),
                static_cast<unsigned>(reply.publicExponentByte),
                reply.hasEmbeddedPublicKey ? 1u : 0u,
                static_cast<unsigned>(workerResult),
                fmt::ptr(child.lazyPubkeyDatValidatorA4),
                fmt::ptr(child.raw08PublicKeyWorkerA8),
                fmt::ptr(child.replyAuthDataValidatorAC));

            if (workerResult != 0u) {
                return kAuthBootstrap680InboundGetPublicKeyWorkerError;
            }

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

            const bool storedParseObjectF0 =
                StoreAuthBootstrap680AuthReplyParseObjectFromStagedPacket(
                    mediator,
                    child,
                    stagedBytes);
            const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject = child.authReplyParseObjectF0;

            mediator.lastAuthReply_ = reply;
            mediator.expectedAuthRequestName_ = nullptr;
            child.inboundAuthStatusEc =
                storedParseObjectF0
                    ? ReadAuthBootstrap680AuthReplyParseHeaderDword(parseObject, 0x01u)
                    : (reply.isErrorReply ? reply.errorCode : 0u);

            spdlog::debug(
                "AuthBootstrap680Ops::HandleInboundAuthMessage stored child+0xf0 parse copy={} status=0x{:08x} authDataLen=0x{:04x} encryptedPrivateExponentLen=0x{:04x} characterCount={} worldCount={} replyString1dLen=0x{:04x}",
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

            // Tightened source-owned mirror of the original `0x44aec0` gate:
            // - `0x448140` requires copied auth-data field length `0x136`
            // - `0x44add0` rejects stale signed-data blocks using child `+0x80`
            // - `0x44ae40` builds the MD5 digest of signed-data `+0x80 .. +0x135`
            // - `0x44aec0` then calls child `+0xac` worker vtable `+0x2c` with
            //   `(md5Digest10, 0x10, copyShadowF4, 0x80)`
            const uint16_t authDataByteLength =
                parseObject ? parseObject->authDataByteLength20
                            : static_cast<uint16_t>(reply.authDataBytes.size());
            if (!reply.valid || !reply.signedData.valid ||
                authDataByteLength != sizeof(AuthBootstrapReplyCopyShadowF4Sketch) ||
                child.replyAuthDataValidatorAC == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrapReplyCopyShadowF4Sketch copyShadowCandidate = {};
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

            AuthBootstrap680ChildOwnedState& ownedState =
                MutableAuthBootstrap680ChildOwnedState(&mediator);
            const bool replyAuthDataValidatorAccepted =
                VerifyAuthBootstrap680ReplyCopyShadowF4WithValidatorScaffold(
                    child,
                    ownedState.replyAuthDataValidatorAC,
                    copyShadowCandidate);
            if (!replyAuthDataValidatorAccepted) {
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
    const bool ensuredLazyPubkeyDatValidatorA4 =
        EnsureAuthBootstrap680LazyPubkeyDatValidatorA4FallbackScaffold(mediator, mediator.authBootstrapChild680_);

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
        "DIAGNOSTIC: launcher-owned auth send via 0x447eb0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} ensuredLazyPubkeyDatValidatorA4={} childLazyPubkeyDatValidatorA4={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsGetPublicKeyRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        fmt::ptr(child.sendTarget50),
        ensuredLazyPubkeyDatValidatorA4 ? 1u : 0u,
        fmt::ptr(child.lazyPubkeyDatValidatorA4),
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
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
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

    child.currentPublicKeyId9C = reply.publicKeyId;

    FillAuthBootstrap680Field54SeedBytesScaffold(child.feedbackSeedHelper54, child.feedbackSeed84);

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = static_cast<uint8_t>(child.loginType28 & 0xffu);
    requestLayout.keyConfigMd5.assign(child.block30.begin(), child.block30.end());
    requestLayout.uiConfigMd5.assign(child.block40.begin(), child.block40.end());

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
            BuildHexPreview(child.feedbackSeed84.data(), child.feedbackSeed84.size(), child.feedbackSeed84.size()),
            static_cast<unsigned>(child.feedbackSeedHelper54.nextBufferedOutputByte28));
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
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);

    child.authServerTimeBias80 = 0u;
    child.sendTarget50 = nullptr;
    ResetAuthBootstrap680Field54Helper(&child.feedbackSeedHelper54, &ownedState.field54Helper);
    std::fill(child.feedbackSeed84.begin(), child.feedbackSeed84.end(), 0u);
    child.feedbackTransformLarge94 = nullptr;
    child.feedbackTransformSmall98 = nullptr;
    child.authRequestReadyA0 = 0u;
    child.paddingA1ToA3 = {};
    child.lazyPubkeyDatValidatorA4 = nullptr;
    ResetAuthBootstrap680ReplyPublicKeyWorkers(child, ownedState);
    ownedState.lazyPubkeyDatValidatorA4.object.reset();
    ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&ownedState.lazyPubkeyDatValidatorA4.publicKeyPair0c);

    ResetAuthBootstrap680ReplyParseObject(child, ownedState);
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

// anchor: launcher.exe:0x447f50 / 0x447780 / 0x447260 / 0x447c10
uint32_t AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::GetPublicKeyReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);

    // Static `0x447f50` only rebuilds the child `+0xa8/+0xac` worker pair when the incoming
    // public-key id differs from child `+0x9c`; otherwise it just sets child `+0xa0 = 1`.
    if (reply.publicKeyId == child.currentPublicKeyId9C) {
        child.authRequestReadyA0 = 1u;
        return 0u;
    }

    // `0x447780` first enforces a recovered modulus-byte-size gate (`0x80`) before consulting the
    // lazy `qspubkey.dat` validator at child `+0xa4` through `0x468f80`.
    if (!reply.hasEmbeddedPublicKey || reply.modulusBytes.size() != 0x80u || reply.publicExponentByte == 0u) {
        child.authRequestReadyA0 = 0u;
        return 1u;
    }
    const bool lazyPubkeyDatValidatorAccepted =
        VerifyAuthBootstrap680ReplyPublicKeyAgainstLazyPubkeyDatValidatorScaffold(mediator, child, reply);
    if (!lazyPubkeyDatValidatorAccepted) {
        child.authRequestReadyA0 = 0u;
        return 1u;
    }

    ResetAuthBootstrap680ReplyPublicKeyWorkers(child, ownedState);
    child.currentPublicKeyId9C = reply.publicKeyId;

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

    child.authRequestReadyA0 = 1u;
    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold rebuilt child+0xa8/+0xac from raw0x07 publicKeyId={} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} modulusLen={} signatureLen={} exponentByte=0x{:02x}",
        static_cast<unsigned>(reply.publicKeyId),
        fmt::ptr(child.lazyPubkeyDatValidatorA4),
        fmt::ptr(child.raw08PublicKeyWorkerA8),
        fmt::ptr(child.replyAuthDataValidatorAC),
        static_cast<unsigned>(reply.modulusBytes.size()),
        static_cast<unsigned>(reply.signatureBytes.size()),
        static_cast<unsigned>(reply.publicExponentByte));
    return 0u;
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
    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject = child.authReplyParseObjectF0;
    ResetAuthBootstrap680ReplyMaterialization(child, ownedState);

    if (reply.isErrorReply || !reply.valid) {
        return;
    }

    ownedState.authReplyCopyShadowF4 = std::make_unique<AuthBootstrapReplyCopyShadowF4Sketch>();
    AuthBootstrapReplyCopyShadowF4Sketch& copyShadow = *ownedState.authReplyCopyShadowF4;
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
        "CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold materialized owner+0x680+0xf4 copyShadow bytes=0x{:03x} replyAuthDataBytes=0x{:03x} parseObjectAuthDataLen=0x{:04x} signaturePrefix00='{}' signedDataExpiryAc=0x{:08x} modulusPrefixD2='{}' authServerTimeBias80=0x{:08x} builtBlockB0={} builtBlockC4={} builtBlockD8={} blockB0Words={} blockC4Words={} blockD8Words={} parseObjectF0={}",
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
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject = child.authReplyParseObjectF0;

    std::string promptPassword = mediator.authBootstrapSource38_.inlineString20.data();
    const bool promptForSecurId = HasTrailingSlashSixDigitSuffix(promptPassword);
    if (promptForSecurId && promptPassword.size() >= 7u) {
        promptPassword.resize(promptPassword.size() - 7u);
    }
    AssignSmallStringMirror(child.stringF8, promptPassword.c_str());
    child.crashReporterPromptForSecurId104 = promptForSecurId ? 1u : 0u;
    child.authReplySuccessHeaderDword07_110 =
        parseObject != nullptr
            ? ReadAuthBootstrap680AuthReplyParseHeaderDword(parseObject, 0x07u)
            : reply.successHeaderUnknownDword07;

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessPregateScaffold childStringF8Len={} promptForSecurId={} childField110=0x{:08x} parseObjectF0={}",
        static_cast<unsigned>(SmallStringMirrorLength(child.stringF8)),
        static_cast<unsigned>(child.crashReporterPromptForSecurId104),
        static_cast<unsigned>(child.authReplySuccessHeaderDword07_110),
        fmt::ptr(parseObject));
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
// Current source now keeps these gated consequences explicit through the copied `+0xf0`
// auth-reply parse-object family:
// - `0x441260 = AuthBootstrap680_StoreField114AndTimestamp118`
// - owner vtable `+0x150` fed from `0x43d480 = AuthBootstrap680_CopyReplyString54`
// - `0x441170 = AuthBootstrap680_CopyOpaqueReplyBlobs108_10c`
void AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold(
    CLTLoginMediator& mediator,
    const mxo::auth::AuthReply& reply) {
    AuthBootstrap680ChildSketch& child = mediator.authBootstrapChild680_;
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject = child.authReplyParseObjectF0;

    child.authReplySuccessField15_114 =
        parseObject != nullptr
            ? ReadAuthBootstrap680AuthReplyParseHeaderDword(parseObject, 0x15u)
            : reply.unknown3;
    child.authReplySuccessField15Timestamp118 = static_cast<uint32_t>(std::time(nullptr));

    std::string replyString1d = parseObject != nullptr
        ? CopyAuthBootstrap680ReplyParseString(
            parseObject->replyString1dBytes54,
            parseObject->replyString1dByteLength58)
        : std::string();
    if (!replyString1d.empty()) {
        mediator.SetLaunchPadSourceBlock94FirstString(replyString1d.c_str());
    } else if (!reply.username.text.empty()) {
        mediator.SetLaunchPadSourceBlock94FirstString(reply.username.text.c_str());
    }

    AuthBootstrap680ChildOwnedState& ownedState = MutableAuthBootstrap680ChildOwnedState(&mediator);
    if (parseObject != nullptr) {
        CopyAuthBootstrap680ParseFieldToOwnedBytes(
            ownedState.opaqueReplyBlob108Owned,
            parseObject->opaqueField0fBytes2c,
            parseObject->opaqueField0fByteLength30);
        CopyAuthBootstrap680ParseFieldToOwnedBytes(
            ownedState.opaqueReplyBlob10COwned,
            parseObject->opaqueField11Bytes34,
            parseObject->opaqueField11ByteLength38);
    } else {
        ownedState.opaqueReplyBlob108Owned = reply.authSignatureBytes;
        ownedState.opaqueReplyBlob10COwned = reply.encryptedPrivateExponentBytes;
    }
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob108, ownedState.opaqueReplyBlob108Owned);
    PointOpaqueBlobPointerAtOwnedBytes(child.opaqueReplyBlob10C, ownedState.opaqueReplyBlob10COwned);

    spdlog::info(
        "AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold childField114=0x{:08x} childField118=0x{:08x} ownerSource94FirstString='{}' opaqueBlob108Len={} opaqueBlob10CLen={} opaqueBlob108={} opaqueBlob10C={} parseObjectF0={}",
        static_cast<unsigned>(child.authReplySuccessField15_114),
        static_cast<unsigned>(child.authReplySuccessField15Timestamp118),
        mediator.authBootstrapSource38_.inlineString00[0] != '\0'
            ? mediator.authBootstrapSource38_.inlineString00.data()
            : "<empty>",
        static_cast<unsigned>(ownedState.opaqueReplyBlob108Owned.size()),
        static_cast<unsigned>(ownedState.opaqueReplyBlob10COwned.size()),
        fmt::ptr(child.opaqueReplyBlob108),
        fmt::ptr(child.opaqueReplyBlob10C),
        fmt::ptr(parseObject));
}

}  // namespace mxo::ltlogin
