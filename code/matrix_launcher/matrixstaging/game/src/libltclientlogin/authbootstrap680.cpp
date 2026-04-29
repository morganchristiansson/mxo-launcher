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
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <random>

#include <integer.h>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe:DAT_004f79e0
bool g_authBootstrap680State2AuthReplySuccessOneTimeGate = false;

namespace {

// Keep source-owned trailing storage on the concrete child/base objects themselves so the
// recovered launcher layout stays visible without a separate side map.

constexpr uint32_t kIncomingAuthMessageLocatorPayloadOffsetTable[7] = {
    0x11u,
    0x04u,
    0x10u,
    0x0bu,
    0x10u,
    0x11u,
    0x10u,
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

    ownedState->publicKey = CryptoPP::RSA::PublicKey();
    ownedState->modulusBytes.clear();
    ownedState->exponentBytes.clear();
}

// anchor: launcher.exe:0x4420f0
static void ResetAuthBootstrap680RsaPublicKeyPairSubobject(
    AuthBootstrap680RsaPublicKeyPairSubobject0cSketch* outSubobject) {
    if (!outSubobject) {
        return;
    }

    outSubobject->vtable00 = 0x004b6680u;
    outSubobject->helperVtable04 = 0x004b66acu;
    outSubobject->modulus08 = {};
    outSubobject->exponent1c = {};
    outSubobject->helperThunk30 = 0x004b630cu;
    outSubobject->helperThunk34 = 0x004b6348u;
    outSubobject->helperThunk38 = 0x004b6454u;
    outSubobject->helperVtable3c = 0x004b66a0u;
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

    ResetAuthBootstrap680RsaPublicKeyPairSubobject(outSubobject);

    try {
        ownedState->publicKey.Initialize(
            CryptoPP::Integer(modulusBytes, modulusByteCount),
            CryptoPP::Integer(&exponentByte, 1u));
    } catch (const CryptoPP::Exception&) {
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
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
    ResetAuthBootstrap680RsaPublicKeyPairSubobject(&outWorker->publicKeyPair0c);
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
    ResetAuthBootstrap680RsaPublicKeyPairSubobject(&outValidator->publicKeyPair0c);
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
 AuthBootstrap680ChildBase_0x4b7134& child) {
 child.raw08PublicKeyWorkerA8 = nullptr;
 child.replyAuthDataValidatorAC = nullptr;

 child.raw08PublicKeyWorkerA8OwnedState_.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&child.raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c);
 child.replyAuthDataValidatorACOwnedState_.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&child.replyAuthDataValidatorACOwnedState_.publicKeyPair0c);
}

static void ResetAuthBootstrap680FeedbackTransforms(
 AuthBootstrap680ChildBase_0x4b7134& child) {
 child.feedbackTransformLarge94 = nullptr;
 child.feedbackTransformSmall98 = nullptr;
 child.feedbackTransformLarge94Owned_.reset();
 child.feedbackTransformSmall98Owned_.reset();
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

constexpr uint16_t kAuthBootstrap680Raw08PlaintextFixedByteCount = 0x1bu;
constexpr uint16_t kAuthBootstrap680Raw08RequestFixedByteCount = 0x28u;

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
 AuthBootstrap680ChildBase_0x4b7134& child) {
    if (child.authReplyParseObjectF0 != nullptr) {
        std::free(child.authReplyParseObjectF0);
        child.authReplyParseObjectF0 = nullptr;
    }
    child.authReplyParsePacketBodyBytesOwned_.clear();
}

// anchor: launcher.exe:0x4436b0 = Packet_AsGetPublicKeyRequest_0x4b6c74::FUN_004436b0
// Ghidra may show a bogus free-function/int-parameter signature here, but assembly proves this is
// an ECX-receiver helper over the larger `0x4b6c74` auth-reply parse shell. Keep the source split
// at the same method-family boundary: this helper only zeros the writable/rebuild body rooted at
// parse-object `+0x10`, it does not perform field-view resolution.
static void AuthBootstrap680AuthReplyParseObject_ResetWritableBody_0x4436b0(
    AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject) {
    if (!parseObject || !parseObject->replyHeader10) {
        return;
    }

    uint8_t* const replyHeader = const_cast<uint8_t*>(parseObject->replyHeader10);
    replyHeader[0x00u] = 0x0bu;
    *reinterpret_cast<uint32_t*>(replyHeader + 0x01u) = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x05u) = 0u;
    parseObject->stringField05Bytes14 = nullptr;
    parseObject->stringField05Length18 = 0u;
    *reinterpret_cast<uint32_t*>(replyHeader + 0x07u) = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x0bu) = 0u;
    parseObject->authDataBytes1c = nullptr;
    parseObject->authDataByteLength20 = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x0du) = 0u;
    parseObject->encryptedPrivateExponentBytes24 = nullptr;
    parseObject->encryptedPrivateExponentByteLength28 = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x0fu) = 0u;
    parseObject->opaqueField0fBytes2c = nullptr;
    parseObject->opaqueField0fByteLength30 = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x11u) = 0u;
    parseObject->opaqueField11Bytes34 = nullptr;
    parseObject->opaqueField11ByteLength38 = 0u;
    *reinterpret_cast<uint32_t*>(replyHeader + 0x15u) = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x19u) = 0u;
    parseObject->characterTempRecords3c = nullptr;
    parseObject->characterTempRecordCount40 = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x1bu) = 0u;
    parseObject->worldTempRecords44 = nullptr;
    parseObject->worldTempRecordCount48 = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x1du) = 0u;
    parseObject->replyString1dBytes54 = nullptr;
    parseObject->replyString1dByteLength58 = 0u;
}

// anchor: launcher.exe:0x443470 = Packet_AsGetPublicKeyRequest_0x4b6c74::FUN_00443470
// Companion ECX-receiver helper for the same larger `0x4b6c74` parse shell. This method walks the
// reply-header offset table at `parseObject+0x10` and resolves the variable field views into the
// copied shell (`+0x14 .. +0x58`).
static void AuthBootstrap680AuthReplyParseObject_ResolveFieldViews_0x443470(
    AuthBootstrap680AuthReplyParseObjectF0Sketch* parseObject,
    size_t packetBodyByteCount,
    bool zeroTerminateStrings) {
    if (!parseObject || !parseObject->replyHeader10) {
        return;
    }

    const uint8_t* const packetBody = parseObject->replyHeader10;
    auto resolveField = [&](size_t replyHeaderOffset,
                            bool zeroTerminateLastByte,
                            const uint8_t** outFieldBytes,
                            uint16_t* outFieldLength) {
        if (outFieldBytes) {
            *outFieldBytes = nullptr;
        }
        if (outFieldLength) {
            *outFieldLength = 0u;
        }
        if (replyHeaderOffset + 2u > packetBodyByteCount) {
            return;
        }

        const uint16_t fieldOffset = ReadU16LE(packetBody + replyHeaderOffset);
        if (fieldOffset == 0u) {
            return;
        }

        const size_t lengthOffset = static_cast<size_t>(fieldOffset);
        if (lengthOffset + 2u > packetBodyByteCount) {
            return;
        }

        const uint16_t fieldLength = ReadU16LE(packetBody + lengthOffset);
        const size_t fieldDataOffset = lengthOffset + 2u;
        const size_t fieldDataEnd = fieldDataOffset + static_cast<size_t>(fieldLength);
        if (fieldDataEnd > packetBodyByteCount) {
            return;
        }

        if (zeroTerminateStrings && zeroTerminateLastByte && fieldLength != 0u) {
            const_cast<uint8_t*>(packetBody)[fieldDataEnd - 1u] = 0u;
        }
        if (outFieldBytes) {
            *outFieldBytes = packetBody + fieldDataOffset;
        }
        if (outFieldLength) {
            *outFieldLength = fieldLength;
        }
    };

    resolveField(0x05u, true, &parseObject->stringField05Bytes14, &parseObject->stringField05Length18);
    resolveField(0x0bu, false, &parseObject->authDataBytes1c, &parseObject->authDataByteLength20);
    resolveField(0x0du, false, &parseObject->encryptedPrivateExponentBytes24, &parseObject->encryptedPrivateExponentByteLength28);
    resolveField(0x0fu, false, &parseObject->opaqueField0fBytes2c, &parseObject->opaqueField0fByteLength30);
    resolveField(0x11u, false, &parseObject->opaqueField11Bytes34, &parseObject->opaqueField11ByteLength38);
    resolveField(0x13u, false, &parseObject->characterTempRecords3c, &parseObject->characterTempRecordCount40);
    resolveField(0x19u, false, &parseObject->worldTempRecords44, &parseObject->worldTempRecordCount48);
    resolveField(0x1bu, false, &parseObject->opaqueField1bBytes4c, &parseObject->opaqueField1bByteLength50);
    resolveField(0x1du, true, &parseObject->replyString1dBytes54, &parseObject->replyString1dByteLength58);
}

// anchor: launcher.exe:0x444390 = Packet_AsGetPublicKeyRequest_0x4b6c74::FUN_00444390
// Mirror the original source-view init shape first, then let the smaller `0x4436b0/0x443470`
// method-family helpers own the writable-body reset vs resolved-field branch.
static bool AuthBootstrap680AuthReplyParseObject_InitSourceView_0x444390(
    AuthBootstrap680AuthReplyParseObjectF0Sketch* outParseObject,
    const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingAuthMessageRef) {
    if (!outParseObject || !incomingAuthMessageRef) {
        return false;
    }

    const auto* messageStorage = incomingAuthMessageRef->messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t incomingMessagePayloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const incomingMessagePayloadBytes = messageStorage->PayloadBase();
    if (!incomingMessagePayloadBytes || incomingMessagePayloadByteCount == 0u) {
        return false;
    }

    const uint8_t* payloadBytes = incomingMessagePayloadBytes;
    size_t payloadByteCount = incomingMessagePayloadByteCount;
    if (incomingAuthMessageRef->headerless10 != 0u) {
        if (incomingMessagePayloadByteCount < 2u) {
            return false;
        }

        const uint8_t locatorByte0d = incomingMessagePayloadBytes[1];
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
        if (payloadOffset >= incomingMessagePayloadByteCount) {
            return false;
        }

        payloadBytes = incomingMessagePayloadBytes + payloadOffset;
        payloadByteCount = static_cast<size_t>(incomingMessagePayloadByteCount) - payloadOffset;
    }

    if (!payloadBytes || payloadByteCount == 0u) {
        return false;
    }

    *outParseObject = {};
    outParseObject->vtable00 = 0x004b6c74u;
    outParseObject->packetBody04 = payloadBytes;
    outParseObject->incomingMessage08 =
        const_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessageRef);
    outParseObject->resolveFields0c = 1u;
    outParseObject->replyHeader10 = outParseObject->packetBody04;

    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->worldDescriptorAccessor5c,
        0x004b533cu,
        outParseObject->packetBody04,
        outParseObject->resolveFields0c);
    outParseObject->worldDescriptorAccessor5c.incomingMessage08 = outParseObject->incomingMessage08;
    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->slotRecordAccessor70,
        0x004b5328u,
        outParseObject->packetBody04,
        outParseObject->resolveFields0c);
    outParseObject->slotRecordAccessor70.incomingMessage08 = outParseObject->incomingMessage08;

    outParseObject->currentWorldTempRecord6c = nullptr;
    outParseObject->currentCharacterTempRecord80 = nullptr;
    outParseObject->currentCharacterHandle84 = nullptr;
    outParseObject->currentCharacterHandleByteLength88 = 0u;

    if (outParseObject->resolveFields0c == 0u) {
        AuthBootstrap680AuthReplyParseObject_ResetWritableBody_0x4436b0(outParseObject);
    } else {
        AuthBootstrap680AuthReplyParseObject_ResolveFieldViews_0x443470(
            outParseObject,
            payloadByteCount,
            true);
    }
    return true;
}

// Keep the `0x4449c0`-like copy step intentionally thin: copy the already-initialized source
// shell, rebase it onto child-owned packet bytes, then reuse the same smaller receiver helpers.
static bool AuthBootstrap680AuthReplyParseObject_CopyToOwnedPacketBody(
    AuthBootstrap680AuthReplyParseObjectF0Sketch* outParseObject,
    const AuthBootstrap680AuthReplyParseObjectF0Sketch& sourceParseObject,
    std::vector<uint8_t>* packetBodyBytes) {
    if (!outParseObject || !packetBodyBytes) {
        return false;
    }

    *outParseObject = sourceParseObject;
    outParseObject->packetBody04 = packetBodyBytes->empty() ? nullptr : packetBodyBytes->data();
    outParseObject->incomingMessage08 = nullptr;
    outParseObject->replyHeader10 = outParseObject->packetBody04;
    outParseObject->currentWorldTempRecord6c = nullptr;
    outParseObject->currentCharacterTempRecord80 = nullptr;
    outParseObject->currentCharacterHandle84 = nullptr;
    outParseObject->currentCharacterHandleByteLength88 = 0u;

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

    if (outParseObject->resolveFields0c == 0u) {
        AuthBootstrap680AuthReplyParseObject_ResetWritableBody_0x4436b0(outParseObject);
    } else {
        AuthBootstrap680AuthReplyParseObject_ResolveFieldViews_0x443470(
            outParseObject,
            packetBodyBytes->size(),
            true);
    }
    return true;
}

static void ResetAuthBootstrap680ReplyMaterialization(
 AuthBootstrap680ChildBase_0x4b7134& child) {
    if (child.authReplyCopyShadowF4 != nullptr) {
        std::free(child.authReplyCopyShadowF4);
        child.authReplyCopyShadowF4 = nullptr;
    }
    ResetAuthBootstrap680BigIntObject(&child.modulusBigIntB0, &child.modulusBigIntB0OwnedDigits_);
    ResetAuthBootstrap680BigIntObject(
        &child.publicExponentBigIntC4,
        &child.publicExponentBigIntC4OwnedDigits_);
    ResetAuthBootstrap680BigIntObject(
        &child.privateExponentBigIntD8,
        &child.privateExponentBigIntD8OwnedDigits_);
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

    const CryptoPP::RSA::PublicKey& publicKey = ownedState.publicKey;
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
    const CryptoPP::RSA::PublicKey& publicKey = ownedState.publicKey;
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

uint32_t AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::QueryCiphertextChunkByteCountScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) const {
    return static_cast<uint32_t>(
        QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(ownedState.publicKey));
}

uint32_t AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::QueryPlaintextChunkByteCountScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) const {
    CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(ownedState.publicKey);
    return static_cast<uint32_t>(encryptor.FixedMaxPlaintextLength());
}

bool AuthBootstrap680Raw08PublicKeyWorkerA8Sketch::EncryptPlaintextChunkScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* plaintextBytes,
    size_t plaintextByteCount,
    uint8_t* ciphertextBytes,
    size_t ciphertextByteCapacity) const {
    if (!plaintextBytes || plaintextByteCount == 0u || !ciphertextBytes) {
        return false;
    }

    try {
        const CryptoPP::RSA::PublicKey& publicKey = ownedState.publicKey;
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);
        const size_t ciphertextChunkByteCount =
            QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(publicKey);
        if (ciphertextChunkByteCount == 0u || ciphertextByteCapacity < ciphertextChunkByteCount) {
            return false;
        }

        std::string ciphertextChunk;
        CryptoPP::StringSource source(
            plaintextBytes,
            plaintextByteCount,
            true,
            new CryptoPP::PK_EncryptorFilter(
                rng,
                encryptor,
                new CryptoPP::StringSink(ciphertextChunk)));
        if (ciphertextChunk.size() != ciphertextChunkByteCount) {
            return false;
        }

        std::memcpy(ciphertextBytes, ciphertextChunk.data(), ciphertextChunk.size());
        return true;
    } catch (const CryptoPP::Exception&) {
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

}  // namespace

// anchor: launcher.exe:0x445500
AuthBootstrap680ChildBase_0x4b7134::AuthBootstrap680ChildBase_0x4b7134() {
 // Compiler handles vtable at +0x00
 // +0x04, +0x08, +0x0c: small string mirrors cleared by UpdateExceptionState(8) pattern
 // +0x10, +0x14, +0x18: more small string mirrors cleared by UpdateExceptionState(8)
 // +0x1c, +0x20, +0x24: final small string mirrors cleared by UpdateExceptionState(8)

  ResetAuthBootstrap680Field54Helper(&feedbackSeedHelper54, &field54HelperOwnedState_);

 // +0x80, +0x94, +0x98, +0x9c, +0xa0: zeroed
 // +0xa4: lazy validator object reset below
 ResetAuthBootstrap680ReplyPublicKeyWorkers(*this);
 // +0xa8: raw08 worker reset in ReplyPublicKeyWorkers
 // +0xac: validator reset in ReplyPublicKeyWorkers

 ResetAuthBootstrap680FeedbackTransforms(*this);
 // +0x94: large transform nulled
 // +0x98: small transform nulled

 lazyPubkeyDatValidatorA4OwnedState_.object.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&lazyPubkeyDatValidatorA4OwnedState_.publicKeyPair0c);
 lazyPubkeyDatValidatorA4 = nullptr; // +0xa4 = 0

 ResetAuthBootstrap680ReplyParseObject(*this);
 // +0xf0: parse object nulled

 ResetAuthBootstrap680ReplyMaterialization(*this);
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
 // with InterlockedExchangeAdd tracked deallocation.
 // Source now mirrors the raw ownership shape directly (heap pointer fields at +0x108/+0x10c)
 // even though the global tracked-byte/count side effects still remain to be closed elsewhere.
 if (opaqueReplyBlob108 != nullptr) {
     std::free(opaqueReplyBlob108);
     opaqueReplyBlob108 = nullptr;
 }

 if (opaqueReplyBlob10C != nullptr) {
     std::free(opaqueReplyBlob10C);
     opaqueReplyBlob10C = nullptr;
 }

 // +0xf8: stringF8 - free if begin != current (FUN_00403c20 pattern)
 if (stringF8.begin != nullptr && stringF8.begin != stringF8.current) {
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
    const AuthBootstrap680AuthReplyParseObjectF0Sketch* const parseObject = authReplyParseObjectF0;

    if (opaqueReplyBlob108 != nullptr) {
        std::free(opaqueReplyBlob108);
        opaqueReplyBlob108 = nullptr;
    }
    if (opaqueReplyBlob10C != nullptr) {
        std::free(opaqueReplyBlob10C);
        opaqueReplyBlob10C = nullptr;
    }

    if (parseObject != nullptr && parseObject->opaqueField0fBytes2c != nullptr &&
        parseObject->opaqueField0fByteLength30 != 0u) {
        void* const blob108 = std::malloc(parseObject->opaqueField0fByteLength30);
        if (blob108 != nullptr) {
            std::memcpy(
                blob108,
                parseObject->opaqueField0fBytes2c,
                parseObject->opaqueField0fByteLength30);
            opaqueReplyBlob108 = blob108;
        }
    }
    if (parseObject != nullptr && parseObject->opaqueField11Bytes34 != nullptr &&
        parseObject->opaqueField11ByteLength38 != 0u) {
        void* const blob10C = std::malloc(parseObject->opaqueField11ByteLength38);
        if (blob10C != nullptr) {
            std::memcpy(
                blob10C,
                parseObject->opaqueField11Bytes34,
                parseObject->opaqueField11ByteLength38);
            opaqueReplyBlob10C = blob10C;
        }
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

static bool AuthBootstrap680_TryGetEmbeddedPublicKeyFromPayload(
    const uint8_t* payloadBytes,
    size_t payloadByteCount,
    const uint8_t** outModulusBytes,
    size_t* outModulusByteCount,
    uint8_t* outPublicExponentByte,
    const uint8_t** outSignatureBytes,
    size_t* outSignatureByteCount,
    uint16_t* outSignatureLength) {
    if (outModulusBytes) {
        *outModulusBytes = nullptr;
    }
    if (outModulusByteCount) {
        *outModulusByteCount = 0u;
    }
    if (outPublicExponentByte) {
        *outPublicExponentByte = 0u;
    }
    if (outSignatureBytes) {
        *outSignatureBytes = nullptr;
    }
    if (outSignatureByteCount) {
        *outSignatureByteCount = 0u;
    }
    if (outSignatureLength) {
        *outSignatureLength = 0u;
    }

    if (!payloadBytes || payloadByteCount < 20u) {
        return false;
    }

    const uint8_t publicExponentByte = payloadBytes[0x0fu];
    const uint16_t modulusLength = ReadU16LE(payloadBytes + 0x12u);
    const size_t modulusStart = 20u;
    const size_t modulusEnd = modulusStart + static_cast<size_t>(modulusLength);
    if (publicExponentByte == 0u || modulusLength == 0u || modulusEnd + 2u > payloadByteCount) {
        return false;
    }

    const uint16_t signatureLength = ReadU16LE(payloadBytes + modulusEnd);
    const size_t signatureStart = modulusEnd + 2u;
    const size_t signatureEnd = signatureStart + static_cast<size_t>(signatureLength);
    if (signatureEnd > payloadByteCount) {
        return false;
    }

    if (outModulusBytes) {
        *outModulusBytes = payloadBytes + modulusStart;
    }
    if (outModulusByteCount) {
        *outModulusByteCount = modulusLength;
    }
    if (outPublicExponentByte) {
        *outPublicExponentByte = publicExponentByte;
    }
    if (outSignatureBytes) {
        *outSignatureBytes = payloadBytes + signatureStart;
    }
    if (outSignatureByteCount) {
        *outSignatureByteCount = signatureLength;
    }
    if (outSignatureLength) {
        *outSignatureLength = signatureLength;
    }
    return true;
}

// anchor: launcher.exe:0x468f80
static bool AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(
    AuthBootstrap680ChildBase_0x4b7134& child,
    const uint8_t* modulusBytes,
    size_t modulusByteCount,
    uint8_t publicExponentByte,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) {
    if (mxo::ltlogin::g_SkipAuthPublicKeyReplyValidation != 0u) {
        return true;
    }

    if (child.lazyPubkeyDatValidatorA4 == nullptr || !child.lazyPubkeyDatValidatorA4OwnedState_.object) {
        child.lazyPubkeyDatValidatorA4OwnedState_.object =
            std::make_unique<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch>();
        if (!child.lazyPubkeyDatValidatorA4OwnedState_.object->ConstructFromReplyPublicKey(
                &child.lazyPubkeyDatValidatorA4OwnedState_.publicKeyPair0c,
                kAuthBootstrap680PubkeyDatFallbackModulus.data(),
                kAuthBootstrap680PubkeyDatFallbackModulus.size(),
                0x11u)) {
            child.lazyPubkeyDatValidatorA4OwnedState_.object.reset();
            child.lazyPubkeyDatValidatorA4 = nullptr;
            return false;
        }
        child.lazyPubkeyDatValidatorA4 = child.lazyPubkeyDatValidatorA4OwnedState_.object.get();
    }
    if (child.lazyPubkeyDatValidatorA4 == nullptr) {
        return false;
    }

    if (!modulusBytes || modulusByteCount != 0x80u || publicExponentByte == 0u ||
        !signatureBytes || signatureByteCount == 0u) {
        return false;
    }

    std::array<uint8_t, 0x81u> signedReplyPublicKeyBytes{};
    std::copy_n(modulusBytes, modulusByteCount, signedReplyPublicKeyBytes.begin());
    signedReplyPublicKeyBytes.back() = publicExponentByte;
    return child.lazyPubkeyDatValidatorA4->VerifySignatureRecoveredFinalizeScaffold(
        child.lazyPubkeyDatValidatorA4OwnedState_.publicKeyPair0c,
        signedReplyPublicKeyBytes.data(),
        signedReplyPublicKeyBytes.size(),
        signatureBytes,
        signatureByteCount);
}

// anchor: launcher.exe:0x445610
// Base dtor handles +0x00..+0xf4 (validators, helpers, big-ints, parse objects, copy shadow)
AuthBootstrap680ChildBase_0x4b7134::~AuthBootstrap680ChildBase_0x4b7134() {
 // +0x94: feedbackTransformLarge94 - original points at the large/decrypting CBC Twofish object
 if (feedbackTransformLarge94 != nullptr) {
 feedbackTransformLarge94 = nullptr;
 }

 // +0x98: feedbackTransformSmall98 - original points at the small/encrypting CBC Twofish object
 if (feedbackTransformSmall98 != nullptr) {
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
 // `0x444900` frees the tracked `+0xf4` heap block directly.
 std::free(authReplyCopyShadowF4);
 authReplyCopyShadowF4 = nullptr;
 }

 // +0xf0: authReplyParseObjectF0 - call dtor(1) if non-null
 if (authReplyParseObjectF0 != nullptr) {
 // `0x444900` calls the `+0xf0` object destructor wrapper, which ultimately frees the
 // tracked heap allocation backing the copied `0x8c` parse shell. Source now matches that
 // raw child-owned storage shape even though the parse body itself is still rebuilt locally.
 std::free(authReplyParseObjectF0);
 authReplyParseObjectF0 = nullptr;
 }
 authReplyParsePacketBodyBytesOwned_.clear();
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

// anchor: launcher.exe:0x448050
void AuthBootstrap680Child_0x441290::PrepareAndDispatch(
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
        child.SendAuthRequest();
        return;
    }

    mediator.expectedAuthRequestName_ = CLTLoginMediator::kMessageAsGetPublicKeyRequest;
    child.SendGetPublicKeyRequest();
}

// anchor: launcher.exe:0x448140
uint32_t AuthBootstrap680Child_0x441290::HandleInboundAuthMessage(
    const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingAuthMessage) {
    if (!incomingAuthMessage || !incomingAuthMessage->messageStorage0c) {
        return kAuthBootstrap680InboundUnhandled;
    }

    const uint8_t* const incomingMessagePayloadBytes =
        incomingAuthMessage->messageStorage0c->PayloadBase();
    const uint16_t incomingMessagePayloadByteCount =
        incomingAuthMessage->messageStorage0c->PayloadByteCount();
    if (!incomingMessagePayloadBytes || incomingMessagePayloadByteCount == 0u) {
        return kAuthBootstrap680InboundUnhandled;
    }

    // anchor: launcher.exe:0x44814e..0x448171 / 0x41bc20
    // The recovered child dispatches on the shared message-code decoder first, then materializes
    // the narrower stack packet/parse helpers only for raw 0x07/0x09/0x0b.
    const uint16_t incomingMessageCode = mxo::liblttcp::CMessageConnectionMessageRef_DecodeMessageCode(
        const_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessage));

    switch (incomingMessageCode) {
        case CLTLoginMediator::kAuthRawCodeGetPublicKeyReply: {
            // anchor: launcher.exe:0x4484d3 / 0x443910
            // The recovered raw 0x07 path materializes the local packet parse accessor on-stack
            // and then reads fixed fields directly from the packet body rooted at +0x10. Mirror
            // that here instead of bouncing through the lower-level shared parser helper.
            Packet_AsGetPublicKeyReply_0x4b6ca4 incomingReplyPacket;
            incomingReplyPacket.InitFromIncomingMessage(
                const_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessage),
                true);

            const uint8_t* const replyPayloadBytes =
                static_cast<const uint8_t*>(incomingReplyPacket.payloadAlias10);
            if (!replyPayloadBytes || replyPayloadBytes < incomingMessagePayloadBytes) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_GetPublicKeyReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            const size_t replyPayloadOffset =
                static_cast<size_t>(replyPayloadBytes - incomingMessagePayloadBytes);
            if (replyPayloadOffset >= incomingMessagePayloadByteCount) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_GetPublicKeyReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            const size_t payloadByteCount =
                static_cast<size_t>(incomingMessagePayloadByteCount) - replyPayloadOffset;
            if (payloadByteCount < 14u ||
                replyPayloadBytes[0] != CLTLoginMediator::kAuthRawCodeGetPublicKeyReply) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_GetPublicKeyReply");
                return kAuthBootstrap680InboundUnhandled;
            }

            const uint32_t replyStatus01 = ReadU32LE(replyPayloadBytes + 0x01u);
            const uint32_t replyCurrentTime05 = ReadU32LE(replyPayloadBytes + 0x05u);
            const uint32_t replyPublicKeyId09 = ReadU32LE(replyPayloadBytes + 0x09u);
            const uint8_t replyKeySize0d = replyPayloadBytes[0x0du];
            const uint8_t replyPublicExponentByte0f =
                (payloadByteCount >= 16u) ? replyPayloadBytes[0x0fu] : 0u;
            const uint16_t replyModulusLength12 =
                (payloadByteCount >= 20u) ? ReadU16LE(replyPayloadBytes + 0x12u) : 0u;
            uint16_t replySignatureLength = 0u;
            const bool hasEmbeddedPublicKey = AuthBootstrap680_TryGetEmbeddedPublicKeyFromPayload(
                replyPayloadBytes,
                payloadByteCount,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &replySignatureLength);

            inboundAuthStatusEc = replyStatus01;
            if (replyStatus01 != 0u) {
                return kAuthBootstrap680InboundGetPublicKeyReplyError;
            }

            authServerTimeBias80 = static_cast<uint32_t>(
                std::time(nullptr) - static_cast<std::time_t>(replyCurrentTime05));
            const uint32_t workerResult = HandleGetPublicKeyReply(incomingReplyPacket);
            if (workerResult != 0u) {
                inboundAuthStatusEc = workerResult;
            } else {
                // anchor: launcher.exe:0x448548
                // The original child falls straight from the successful `0x447f50` worker rebuild
                // into `0x4474f0`. The replacement keeps this one source-owned payload snapshot
                // only so `SendAuthRequest()` can recover the same reply-public-key header bytes.
                cachedGetPublicKeyReplyPayloadBytesOwned_.assign(
                    replyPayloadBytes,
                    replyPayloadBytes + payloadByteCount);
            }

            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status={} currentTime={} publicKeyId={} keySize={} modulusLength={} signatureLength={} exponentByte=0x{:02x} hasEmbeddedPublicKey={} workerResult=0x{:08x} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} helper={} module={} childBase={}",
                static_cast<unsigned>(replyStatus01),
                static_cast<unsigned>(replyCurrentTime05),
                static_cast<unsigned>(replyPublicKeyId09),
                static_cast<unsigned>(replyKeySize0d),
                static_cast<unsigned>(replyModulusLength12),
                static_cast<unsigned>(replySignatureLength),
                static_cast<unsigned>(replyPublicExponentByte0f),
                hasEmbeddedPublicKey ? 1u : 0u,
                static_cast<unsigned>(workerResult),
                fmt::ptr(lazyPubkeyDatValidatorA4),
                fmt::ptr(raw08PublicKeyWorkerA8),
                fmt::ptr(replyAuthDataValidatorAC),
                fmt::ptr(this),
                fmt::ptr(owner08),
                fmt::ptr(this));

            if (workerResult != 0u) {
                return kAuthBootstrap680InboundGetPublicKeyWorkerError;
            }

            // anchor: launcher.exe:0x448548
            // The recovered launcher tail ignores the raw 0x08 send result here: it issues the
            // send attempt and still returns 1 so the outer auth state machine keeps waiting.
            SendAuthRequest();
            return kAuthBootstrap680InboundHandledContinueWaiting;
        }

        case 0x09u: {
            // anchor: launcher.exe:0x443d90 / 0x44831c
            // The recovered raw 0x09 path materializes the local packet parse accessor and then
            // consumes the 16-byte encrypted challenge inline from payload+1.
            Packet_AsAuthChallenge_0x4b6ce0 incomingChallengePacket;
            incomingChallengePacket.InitFromIncomingMessage(
                const_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessage));

            const uint8_t* const challengePayloadBytes =
                static_cast<const uint8_t*>(incomingChallengePacket.payloadAlias10);
            if (!challengePayloadBytes || challengePayloadBytes < incomingMessagePayloadBytes) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthChallenge");
                return kAuthBootstrap680InboundUnhandled;
            }

            const size_t challengePayloadOffset =
                static_cast<size_t>(challengePayloadBytes - incomingMessagePayloadBytes);
            if (challengePayloadOffset >= incomingMessagePayloadByteCount) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthChallenge");
                return kAuthBootstrap680InboundUnhandled;
            }

            const size_t payloadByteCount =
                static_cast<size_t>(incomingMessagePayloadByteCount) - challengePayloadOffset;
            if (payloadByteCount != 17u || challengePayloadBytes[0] != 0x09u) {
                spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthChallenge");
                return kAuthBootstrap680InboundUnhandled;
            }

            cachedAuthChallengeCiphertextBytesOwned_.assign(
                challengePayloadBytes + 1u,
                challengePayloadBytes + payloadByteCount);
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen={}",
                cachedAuthChallengeCiphertextBytesOwned_.size());

            {
                const char* password = SmallStringMirrorDataOrEmpty(string10);
                const char* soePassword = SmallStringMirrorDataOrEmpty(string1C);
                if (SmallStringMirrorLength(string10) == 0u) {
                    spdlog::error(
                        "launcher-owned auth received AS_AuthChallenge but child+0x10 password data is empty");
                    return kAuthBootstrap680InboundUnhandled;
                }
                if (SmallStringMirrorLength(string1C) == 0u) {
                    spdlog::warn(
                        "launcher-owned auth raw0x0a using empty child+0x1c secondary password/station field while preserving the recovered 0x44831c field mapping");
                }
                if (cachedAuthRequestTwofishKeyBytesOwned_.size() != 16u) {
                    spdlog::error("launcher-owned auth missing cached AS_AuthRequest Twofish key bytes for AS_AuthChallenge response path");
                    return kAuthBootstrap680InboundUnhandled;
                }
                if (!sendTarget50) {
                    spdlog::warn(
                        "AuthBootstrap680 raw0x0a challenge-response missing child+0x50 send target; refusing less-faithful fallback path");
                    return kAuthBootstrap680InboundUnhandled;
                }

                mxo::auth::AuthChallengeResponseLayout layout;
                std::vector<uint8_t> decryptedChallengeBytes;
                std::vector<uint8_t> processedChallengeMd5Bytes;
                std::vector<uint8_t> plaintextBytes;
                std::vector<uint8_t> ciphertextBytes;

                // anchor: launcher.exe:0x44831c..0x448467
                // This raw 0x0a path is inline in the child-side inbound handler, not a separate
                // standalone launcher.exe helper. Keep the crypto/material assembly here so future
                // fidelity passes can compare directly against the packet-object staging below.
                decryptedChallengeBytes.assign(
                    cachedAuthChallengeCiphertextBytesOwned_.size(),
                    0u);
                if ((cachedAuthChallengeCiphertextBytesOwned_.size() % 16u) != 0u) {
                    spdlog::error("launcher-owned auth rejected misaligned AS_AuthChallenge ciphertext");
                    return kAuthBootstrap680InboundUnhandled;
                }
                if (!cachedAuthChallengeCiphertextBytesOwned_.empty()) {
                    static constexpr uint8_t kZeroIv[16] = {0};
                    try {
                        CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption challengeCipher;
                        challengeCipher.SetKeyWithIV(
                            cachedAuthRequestTwofishKeyBytesOwned_.data(),
                            cachedAuthRequestTwofishKeyBytesOwned_.size(),
                            kZeroIv);
                        challengeCipher.ProcessData(
                            decryptedChallengeBytes.data(),
                            cachedAuthChallengeCiphertextBytesOwned_.data(),
                            cachedAuthChallengeCiphertextBytesOwned_.size());
                    } catch (const CryptoPP::Exception&) {
                        spdlog::error("launcher-owned auth failed to decrypt AS_AuthChallenge ciphertext");
                        return kAuthBootstrap680InboundUnhandled;
                    }
                }

                processedChallengeMd5Bytes.assign(16u, 0u);
                {
                    CryptoPP::Weak::MD5 challengeMd5;
                    if (!decryptedChallengeBytes.empty()) {
                        challengeMd5.Update(
                            decryptedChallengeBytes.data(),
                            decryptedChallengeBytes.size());
                    }
                    challengeMd5.Final(processedChallengeMd5Bytes.data());
                }

                std::vector<uint8_t> passwordBytes(password, password + std::strlen(password));
                if (layout.includePasswordNullTerminator) {
                    passwordBytes.push_back(0u);
                }
                std::vector<uint8_t> soePasswordBytes(
                    soePassword,
                    soePassword + std::strlen(soePassword));
                if (layout.includeSoePasswordNullTerminator) {
                    soePasswordBytes.push_back(0u);
                }
                if (passwordBytes.size() > 0xffffu || soePasswordBytes.size() > 0xffffu) {
                    spdlog::error("launcher-owned auth rejected oversized raw0x0a password field");
                    return kAuthBootstrap680InboundUnhandled;
                }

                const uint16_t passwordLengthField = static_cast<uint16_t>(passwordBytes.size());
                const uint16_t soePasswordLengthField = static_cast<uint16_t>(soePasswordBytes.size());
                const uint16_t unknown2 = layout.usePasswordLengthForUnknown2
                    ? passwordLengthField
                    : layout.unknown2;
                const uint16_t unknown3 = layout.useSoePasswordLengthForUnknown3
                    ? soePasswordLengthField
                    : layout.unknown3;

                const size_t plaintextSizeWithoutPadding =
                    1u +
                    processedChallengeMd5Bytes.size() +
                    2u + 2u + 2u +
                    2u + passwordBytes.size() +
                    2u + soePasswordBytes.size() +
                    2u;
                const uint16_t paddingLengthField =
                    static_cast<uint16_t>(0x20u - (plaintextSizeWithoutPadding & 0x0fu));

                plaintextBytes.reserve(plaintextSizeWithoutPadding + paddingLengthField);
                plaintextBytes.push_back(layout.plaintextLeadingByte);
                plaintextBytes.insert(
                    plaintextBytes.end(),
                    processedChallengeMd5Bytes.begin(),
                    processedChallengeMd5Bytes.end());
                mxo::auth::internal::AppendU16LE(&plaintextBytes, layout.unknown1);
                mxo::auth::internal::AppendU16LE(&plaintextBytes, unknown2);
                mxo::auth::internal::AppendU16LE(&plaintextBytes, unknown3);
                mxo::auth::internal::AppendU16LE(&plaintextBytes, passwordLengthField);
                plaintextBytes.insert(
                    plaintextBytes.end(),
                    passwordBytes.begin(),
                    passwordBytes.end());
                mxo::auth::internal::AppendU16LE(&plaintextBytes, soePasswordLengthField);
                plaintextBytes.insert(
                    plaintextBytes.end(),
                    soePasswordBytes.begin(),
                    soePasswordBytes.end());
                mxo::auth::internal::AppendU16LE(&plaintextBytes, paddingLengthField);
                plaintextBytes.insert(
                    plaintextBytes.end(),
                    paddingLengthField,
                    layout.paddingByte);

                if ((plaintextBytes.size() % 16u) != 0u) {
                    spdlog::error("launcher-owned auth built misaligned AS_AuthChallengeResponse plaintext");
                    return kAuthBootstrap680InboundUnhandled;
                }
                ciphertextBytes.assign(plaintextBytes.size(), 0u);
                if (!plaintextBytes.empty()) {
                    static constexpr uint8_t kZeroIv[16] = {0};
                    try {
                        CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption responseCipher;
                        responseCipher.SetKeyWithIV(
                            cachedAuthRequestTwofishKeyBytesOwned_.data(),
                            cachedAuthRequestTwofishKeyBytesOwned_.size(),
                            kZeroIv);
                        responseCipher.ProcessData(
                            ciphertextBytes.data(),
                            plaintextBytes.data(),
                            plaintextBytes.size());
                    } catch (const CryptoPP::Exception&) {
                        spdlog::error("launcher-owned auth failed to encrypt AS_AuthChallengeResponse plaintext");
                        return kAuthBootstrap680InboundUnhandled;
                    }
                }

                Packet_AsAuthChallengeResponse_0x4b6cf4 plaintextPacket;
                plaintextPacket.ResetAndInitialize();

                if (processedChallengeMd5Bytes.size() >= 16u) {
                    uint8_t* payload = static_cast<uint8_t*>(plaintextPacket.payloadAlias10);
                    if (payload) {
                        std::copy_n(processedChallengeMd5Bytes.begin(), 16u, payload + 1u);
                    }
                }

                plaintextPacket.AppendEncryptedChallenge(password);
                plaintextPacket.AppendPassword(soePassword);
                plaintextPacket.ReserveFieldLength(0x20u);

                const size_t plaintextLen = plaintextBytes.size();
                // anchor: launcher.exe:0x4483ce
                // The launcher computes `0x20 - (len & 0x0f)` directly and forwards that raw
                // value to `Packet_AsAuthChallengeResponse_0x4b6cf4::SetPadding`, even when the
                // low nibble is already zero.
                const uint16_t paddingBytes =
                    static_cast<uint16_t>(0x20u - (plaintextLen & 0x0fu));
                plaintextPacket.SetPadding(paddingBytes);

                Packet_AsAuthChallengeResponse_0x4b6d08 encryptedPacket;
                encryptedPacket.InitializePayloadSize();
                encryptedPacket.ReserveLengthPrefixedTail(
                    static_cast<uint16_t>(ciphertextBytes.size()));
                if (ciphertextBytes.size() != 0u && encryptedPacket.debugString14 != nullptr) {
                    std::memcpy(
                        const_cast<char*>(encryptedPacket.debugString14),
                        ciphertextBytes.data(),
                        ciphertextBytes.size());
                }

                auto* sendTarget = static_cast<mxo::liblttcp::CMessageConnection_0x4b7928*>(sendTarget50);
                sendTarget->SendPacketMessageRef(*encryptedPacket.messageRef08);

                const uint32_t sendResult = 1u;

                spdlog::debug(
                    "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage observed raw0x0a send using Packet_AsAuthChallengeResponse_0x4b6cf4 + Packet_AsAuthChallengeResponse_0x4b6d08 decryptedChallengeBytes={} processedChallengeMd5Bytes={}",
                    static_cast<unsigned>(decryptedChallengeBytes.size()),
                    static_cast<unsigned>(processedChallengeMd5Bytes.size()));
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned auth built/sent AS_AuthChallengeResponse passwordLengthField={} soePasswordLengthField={} plaintextLen={} ciphertextLen={} childString10Len={} childString1CLen={} sendTarget50={} helper={} module={} childBase={}",
                    static_cast<unsigned>(passwordLengthField),
                    static_cast<unsigned>(soePasswordLengthField),
                    static_cast<unsigned>(plaintextBytes.size()),
                    static_cast<unsigned>(ciphertextBytes.size()),
                    static_cast<unsigned>(SmallStringMirrorLength(string10)),
                    static_cast<unsigned>(SmallStringMirrorLength(string1C)),
                    fmt::ptr(sendTarget50),
                    fmt::ptr(this),
                    fmt::ptr(owner08),
                    fmt::ptr(this));
                return sendResult != 0u
                    ? kAuthBootstrap680InboundHandledContinueWaiting
                    : kAuthBootstrap680InboundUnhandled;
            }
        }

        case 0x0bu: {
            ResetAuthBootstrap680ReplyParseObject(*this);

            bool storedParseObjectF0 = false;
            AuthBootstrap680AuthReplyParseObjectF0Sketch sourceParseObject = {};
            if (AuthBootstrap680AuthReplyParseObject_InitSourceView_0x444390(
                    &sourceParseObject,
                    incomingAuthMessage)) {
                const uint8_t* const payloadBytes = sourceParseObject.replyHeader10;
                if (payloadBytes == nullptr || payloadBytes < incomingMessagePayloadBytes) {
                    spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthReply");
                    return kAuthBootstrap680InboundUnhandled;
                }

                const size_t payloadOffset =
                    static_cast<size_t>(payloadBytes - incomingMessagePayloadBytes);
                if (payloadOffset >= incomingMessagePayloadByteCount) {
                    spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthReply");
                    return kAuthBootstrap680InboundUnhandled;
                }

                const size_t payloadByteCount =
                    static_cast<size_t>(incomingMessagePayloadByteCount) - payloadOffset;
                if (payloadByteCount < 11u || payloadBytes[0] != 0x0bu) {
                    spdlog::warn("DIAGNOSTIC: CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage failed to parse AS_AuthReply");
                    return kAuthBootstrap680InboundUnhandled;
                }

                authReplyParsePacketBodyBytesOwned_.assign(
                    payloadBytes,
                    payloadBytes + payloadByteCount);
                authReplyParseObjectF0 =
                    static_cast<AuthBootstrap680AuthReplyParseObjectF0Sketch*>(
                        std::malloc(sizeof(AuthBootstrap680AuthReplyParseObjectF0Sketch)));
                if (authReplyParseObjectF0 != nullptr) {
                    storedParseObjectF0 =
                        AuthBootstrap680AuthReplyParseObject_CopyToOwnedPacketBody(
                            authReplyParseObjectF0,
                            sourceParseObject,
                            &authReplyParsePacketBodyBytesOwned_);
                    if (!storedParseObjectF0) {
                        ResetAuthBootstrap680ReplyParseObject(*this);
                    }
                } else {
                    authReplyParsePacketBodyBytesOwned_.clear();
                }
            }
            const auto* parseObject = authReplyParseObjectF0;
            const uint32_t errorReplyStatus01 =
                (parseObject != nullptr && parseObject->replyHeader10 != nullptr)
                    ? ReadU32LE(parseObject->replyHeader10 + 0x01u)
                    : 0u;

            inboundAuthStatusEc =
                (storedParseObjectF0 && parseObject != nullptr && parseObject->replyHeader10 != nullptr)
                    ? ReadU32LE(parseObject->replyHeader10 + 0x01u)
                    : errorReplyStatus01;

            spdlog::debug(
                "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage stored child+0xf0 parse copy={} status=0x{:08x} authDataLen=0x{:04x} encryptedPrivateExponentLen=0x{:04x} characterCount={} worldCount={} replyString1dLen=0x{:04x}",
                storedParseObjectF0 ? 1u : 0u,
                static_cast<unsigned>(inboundAuthStatusEc),
                static_cast<unsigned>(parseObject ? parseObject->authDataByteLength20 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->encryptedPrivateExponentByteLength28 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->characterTempRecordCount40 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->worldTempRecordCount48 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->replyString1dByteLength58 : 0u));

            if (inboundAuthStatusEc != 0u) {
                return kAuthBootstrap680InboundAuthReplyError;
            }

            const uint16_t authDataByteLength =
                parseObject ? parseObject->authDataByteLength20 : 0u;
            if (!storedParseObjectF0 || parseObject == nullptr ||
                authDataByteLength != sizeof(AuthBootstrapReplyCopyShadowF4_0x44add0) ||
                replyAuthDataValidatorAC == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrapReplyCopyShadowF4_0x44add0 copyShadowCandidate = {};
            if (parseObject != nullptr && parseObject->authDataBytes1c != nullptr &&
                parseObject->authDataByteLength20 == sizeof(copyShadowCandidate)) {
                std::copy_n(
                    parseObject->authDataBytes1c,
                    sizeof(copyShadowCandidate),
                    reinterpret_cast<uint8_t*>(&copyShadowCandidate));
            } else {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            const bool replyAuthDataValidatorAccepted =
                copyShadowCandidate.VerifyWithValidator(
                    replyAuthDataValidatorACOwnedState_.object.get(),
                    replyAuthDataValidatorACOwnedState_.publicKeyPair0c,
                    authServerTimeBias80);
            if (!replyAuthDataValidatorAccepted) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            // anchor: launcher.exe:0x448200..0x4482cf
            // Keep the raw `0x0b` success tail inline in the inbound child handler: the original
            // allocates/copies child `+0xf4`, then rebuilds child `+0xb0/+0xc4/+0xd8` from that
            // copied shadow and the encrypted-private-exponent side field before returning `2`.
            ResetAuthBootstrap680ReplyMaterialization(*this);
            authReplyCopyShadowF4 = static_cast<AuthBootstrapReplyCopyShadowF4_0x44add0*>(
                std::malloc(sizeof(AuthBootstrapReplyCopyShadowF4_0x44add0)));
            if (authReplyCopyShadowF4 == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrapReplyCopyShadowF4_0x44add0& copyShadow = *authReplyCopyShadowF4;
            copyShadow = {};
            std::copy_n(
                parseObject->authDataBytes1c,
                sizeof(copyShadow),
                reinterpret_cast<uint8_t*>(&copyShadow));

            const CryptoPP::Integer modulusInteger(
                copyShadow.signedData80.data() + 0x52u,
                0x60u);
            const CryptoPP::Integer publicExponentInteger(
                static_cast<unsigned int>(copyShadow.signedData80[0x51u]));

            AuthBootstrap680BigIntObjects_0x4ba50c* const blockB0 = &modulusBigIntB0;
            AuthBootstrap680BigIntObjects_0x4ba50c* const blockC4 = &publicExponentBigIntC4;
            AuthBootstrap680BigIntObjects_0x4ba50c* const blockD8 = &privateExponentBigIntD8;

            const size_t modulusByteCount =
                std::max<size_t>(1u, static_cast<size_t>(modulusInteger.MinEncodedSize()));
            std::vector<uint8_t> modulusBytes(modulusByteCount, 0u);
            modulusInteger.Encode(modulusBytes.data(), modulusBytes.size());
            const bool builtBlockB0 = BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                blockB0,
                &modulusBigIntB0OwnedDigits_,
                modulusBytes.data(),
                modulusBytes.size());

            const size_t publicExponentByteCount =
                std::max<size_t>(1u, static_cast<size_t>(publicExponentInteger.MinEncodedSize()));
            std::vector<uint8_t> publicExponentBytes(publicExponentByteCount, 0u);
            publicExponentInteger.Encode(
                publicExponentBytes.data(),
                publicExponentBytes.size());
            const bool builtBlockC4 = BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                blockC4,
                &publicExponentBigIntC4OwnedDigits_,
                publicExponentBytes.data(),
                publicExponentBytes.size());

            std::vector<uint8_t> decryptedPrivateExponentBytes;
            bool decryptedPrivateExponent = false;
            if (parseObject->encryptedPrivateExponentBytes24 != nullptr &&
                cachedAuthRequestTwofishKeyBytesOwned_.size() == 16u &&
                cachedAuthChallengeCiphertextBytesOwned_.size() == 16u &&
                (parseObject->encryptedPrivateExponentByteLength28 % 16u) == 0u) {
                decryptedPrivateExponentBytes.assign(
                    parseObject->encryptedPrivateExponentByteLength28,
                    0u);
                decryptedPrivateExponent = true;
                if (parseObject->encryptedPrivateExponentByteLength28 != 0u) {
                    try {
                        CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption privateExponentCipher;
                        privateExponentCipher.SetKeyWithIV(
                            cachedAuthRequestTwofishKeyBytesOwned_.data(),
                            cachedAuthRequestTwofishKeyBytesOwned_.size(),
                            cachedAuthChallengeCiphertextBytesOwned_.data());
                        privateExponentCipher.ProcessData(
                            decryptedPrivateExponentBytes.data(),
                            parseObject->encryptedPrivateExponentBytes24,
                            parseObject->encryptedPrivateExponentByteLength28);
                    } catch (const CryptoPP::Exception&) {
                        decryptedPrivateExponent = false;
                        decryptedPrivateExponentBytes.clear();
                    }
                }
            }

            bool builtBlockD8 = false;
            if (decryptedPrivateExponent && decryptedPrivateExponentBytes.size() == 0x60u) {
                const CryptoPP::Integer privateExponentInteger(
                    decryptedPrivateExponentBytes.data(),
                    decryptedPrivateExponentBytes.size());
                const size_t privateExponentByteCount =
                    std::max<size_t>(1u, static_cast<size_t>(privateExponentInteger.MinEncodedSize()));
                std::vector<uint8_t> privateExponentBytes(privateExponentByteCount, 0u);
                privateExponentInteger.Encode(
                    privateExponentBytes.data(),
                    privateExponentBytes.size());
                builtBlockD8 = BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                    blockD8,
                    &privateExponentBigIntD8OwnedDigits_,
                    privateExponentBytes.data(),
                    privateExponentBytes.size());
            }

            spdlog::info(
                "HandleInboundAuthMessage materialized child+0xf4/+0xb0/+0xc4/+0xd8 from raw0x0b authDataBytes=0x{:03x} parseObjectAuthDataLen=0x{:04x} signaturePrefix00='{}' signedDataExpiryAc=0x{:08x} modulusPrefixD2='{}' authServerTimeBias80=0x{:08x} builtBlockB0={} builtBlockC4={} builtBlockD8={} blockB0Words={} blockC4Words={} blockD8Words={} parseObjectF0={}",
                static_cast<unsigned>(sizeof(copyShadow)),
                static_cast<unsigned>(parseObject->authDataByteLength20),
                BuildHexPreview(
                    copyShadow.authSignature00.data(),
                    16u,
                    16u),
                static_cast<unsigned>(ReadU32LE(copyShadow.signedData80.data() + 0x2cu)),
                BuildHexPreview(
                    copyShadow.signedData80.data() + 0x52u,
                    16u,
                    16u),
                static_cast<unsigned>(authServerTimeBias80),
                builtBlockB0 ? 1u : 0u,
                builtBlockC4 ? 1u : 0u,
                builtBlockD8 ? 1u : 0u,
                static_cast<unsigned>(blockB0->capacityWords08),
                static_cast<unsigned>(blockC4->capacityWords08),
                static_cast<unsigned>(blockD8->capacityWords08),
                fmt::ptr(parseObject));
            return kAuthBootstrap680InboundAuthReplySuccess;
        }

        default:
            break;
    }

    return kAuthBootstrap680InboundUnhandled;
}

// anchor: launcher.exe:0x447eb0
void AuthBootstrap680Child_0x441290::SendGetPublicKeyRequest() {
    bool ensuredLazyPubkeyDatValidatorA4 = false;
    if (lazyPubkeyDatValidatorA4 != nullptr && lazyPubkeyDatValidatorA4OwnedState_.object) {
        ensuredLazyPubkeyDatValidatorA4 = true;
    } else {
        // anchor: launcher.exe:0x447260 / 0x447c10
        lazyPubkeyDatValidatorA4OwnedState_.object =
            std::make_unique<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch>();
        if (lazyPubkeyDatValidatorA4OwnedState_.object->ConstructFromReplyPublicKey(
                &lazyPubkeyDatValidatorA4OwnedState_.publicKeyPair0c,
                kAuthBootstrap680PubkeyDatFallbackModulus.data(),
                kAuthBootstrap680PubkeyDatFallbackModulus.size(),
                0x11u)) {
            lazyPubkeyDatValidatorA4 = lazyPubkeyDatValidatorA4OwnedState_.object.get();
            ensuredLazyPubkeyDatValidatorA4 = true;
        } else {
            lazyPubkeyDatValidatorA4OwnedState_.object.reset();
            lazyPubkeyDatValidatorA4 = nullptr;
        }
    }

    Packet_AsGetPublicKeyRequest_0x4b6c74 packetBuilder;
    packetBuilder.InitializePayloadSize();
    uint8_t* const getPublicKeyRequestPayload = packetBuilder.PayloadBase();
    if (!getPublicKeyRequestPayload) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to initialize Packet_AsGetPublicKeyRequest_0x4b6c74 payload");
        return;
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
        return;
    }

    if (!sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendGetPublicKeyRequest missing child+0x50 send target; recovered 0x447eb0 tail expects direct virtual send through that field");
        return;
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
    g_CurrentLoginMediator->authGetPublicKeyRequestSent_ = (sendResult != 0u);
}

// anchor: launcher.exe:0x4474f0
void AuthBootstrap680Child_0x441290::SendAuthRequest() {
    auto* const childBase = static_cast<AuthBootstrap680ChildBase_0x4b7134*>(this);

    const char* const username = SmallStringMirrorDataOrEmpty(string04);
    if (raw08PublicKeyWorkerA8 == nullptr ||
        raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c.modulusBytes.empty() ||
        raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c.exponentBytes.empty()) {
        spdlog::warn(
            "AuthBootstrap680_SendAuthRequest missing child+0xa8 raw08 worker material; recovered 0x4474f0 consumes that worker through 0x468ea0/0x468f00");
        return;
    }
    if (!sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendAuthRequest missing child+0x50 send target; recovered 0x4474f0 tail expects direct virtual send through that field");
        return;
    }

    FillAuthBootstrap680Field54SeedBytesScaffold(feedbackSeedHelper54, feedbackSeed84);
    ResetAuthBootstrap680FeedbackTransforms(*this);

    auto feedbackTransformLarge94 =
        std::make_unique<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption>();
    try {
        feedbackTransformLarge94->SetKeyWithIV(
            feedbackSeed84.data(),
            static_cast<uint32_t>(feedbackSeed84.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data());
        this->feedbackTransformLarge94 = feedbackTransformLarge94.get();
        feedbackTransformLarge94Owned_ = std::move(feedbackTransformLarge94);
    } catch (const CryptoPP::Exception&) {
    }

    auto feedbackTransformSmall98 =
        std::make_unique<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption>();
    try {
        feedbackTransformSmall98->SetKeyWithIV(
            feedbackSeed84.data(),
            static_cast<uint32_t>(feedbackSeed84.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data());
        this->feedbackTransformSmall98 = feedbackTransformSmall98.get();
        feedbackTransformSmall98Owned_ = std::move(feedbackTransformSmall98);
    } catch (const CryptoPP::Exception&) {
    }

    // anchor: launcher.exe:0x447643
    // The recovered raw `0x08` path uses two stack-local packet builders, not the generic
    // runtime auth helpers:
    // - plaintext builder: fixed 0x1b-byte prefix with a length-prefixed username tail offset at
    //   payload `+0x05`
    // - outbound builder: fixed 0x28-byte prefix with a length-prefixed ciphertext tail offset at
    //   payload `+0x06`
    Packet_AsGetPublicKeyReply_0x4b6ca4 plaintextAuthBlob;
    if (!plaintextAuthBlob.messageRef08 || !plaintextAuthBlob.messageRef08->messageStorage0c) {
        spdlog::error("launcher-owned auth failed to initialize recovered 0x4474f0 plaintext packet builder");
        return;
    }
    auto* const plaintextMessageStorage = plaintextAuthBlob.messageRef08->messageStorage0c;
    plaintextMessageStorage->ResetPayloadByteCount(0u);
    plaintextMessageStorage->GrowPayloadByteCount(kAuthBootstrap680Raw08PlaintextFixedByteCount);

    uint8_t* const plaintextPayload = plaintextMessageStorage->PayloadBase();
    plaintextAuthBlob.payloadPtr04 = reinterpret_cast<uint32_t>(plaintextPayload);
    plaintextAuthBlob.payloadAlias10 = plaintextPayload;
    plaintextAuthBlob.debugString14 = nullptr;
    plaintextAuthBlob.payloadSize18 = 0u;
    if (plaintextPayload == nullptr) {
        spdlog::error("launcher-owned auth failed to initialize recovered 0x4474f0 plaintext packet builder");
        return;
    }
    std::memset(plaintextPayload, 0, kAuthBootstrap680Raw08PlaintextFixedByteCount);

    plaintextPayload[0] = 0x00u;
    *reinterpret_cast<uint32_t*>(plaintextPayload + 0x01u) = currentPublicKeyId9C;
    std::memcpy(plaintextPayload + 0x07u, feedbackSeed84.data(), feedbackSeed84.size());
    *reinterpret_cast<uint32_t*>(plaintextPayload + 0x17u) =
        static_cast<uint32_t>(std::time(nullptr)) - authServerTimeBias80;

    const uint16_t usernameLengthField =
        static_cast<uint16_t>(std::strlen(username) + 1u);
    uint8_t* const plaintextReservationHeader =
        plaintextPayload + plaintextMessageStorage->PayloadByteCount();
    plaintextMessageStorage->GrowPayloadByteCount(static_cast<uint16_t>(usernameLengthField + 2u));
    *reinterpret_cast<uint16_t*>(plaintextReservationHeader) = usernameLengthField;
    *reinterpret_cast<uint16_t*>(plaintextPayload + 0x05u) =
        static_cast<uint16_t>(plaintextReservationHeader - plaintextPayload);
    plaintextAuthBlob.debugString14 = reinterpret_cast<const char*>(plaintextReservationHeader + 2u);
    plaintextAuthBlob.payloadSize18 = usernameLengthField;
    if (plaintextAuthBlob.debugString14 == nullptr) {
        spdlog::error("launcher-owned auth lost recovered 0x4474f0 username tail pointer");
        return;
    }
    std::memcpy(
        const_cast<char*>(plaintextAuthBlob.debugString14),
        username,
        usernameLengthField - 1u);
    const_cast<char*>(plaintextAuthBlob.debugString14)[usernameLengthField - 1u] = '\0';

    const uint16_t plaintextPayloadByteCount =
        plaintextAuthBlob.messageRef08 ? plaintextAuthBlob.messageRef08->PayloadByteCount() : 0u;
    const uint32_t raw08WorkerExpectedBlobLen =
        raw08PublicKeyWorkerA8->QueryEncryptedOutputLengthScaffold(
            raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c,
            plaintextPayloadByteCount);
    if (raw08WorkerExpectedBlobLen == 0u || raw08WorkerExpectedBlobLen > 0xffffu) {
        spdlog::error(
            "launcher-owned auth rejected recovered 0x4474f0 ciphertext reservation len={}",
            raw08WorkerExpectedBlobLen);
        return;
    }

    Packet_AsGetPublicKeyReply_0x4b6ca4 authRequestPacket;
    if (!authRequestPacket.messageRef08 || !authRequestPacket.messageRef08->messageStorage0c) {
        spdlog::error("launcher-owned auth failed to initialize recovered 0x4474f0 outbound packet builder");
        return;
    }
    auto* const authRequestMessageStorage = authRequestPacket.messageRef08->messageStorage0c;
    authRequestMessageStorage->ResetPayloadByteCount(0u);
    authRequestMessageStorage->GrowPayloadByteCount(kAuthBootstrap680Raw08RequestFixedByteCount);

    uint8_t* const authRequestPayload = authRequestMessageStorage->PayloadBase();
    authRequestPacket.payloadPtr04 = reinterpret_cast<uint32_t>(authRequestPayload);
    authRequestPacket.payloadAlias10 = authRequestPayload;
    authRequestPacket.debugString14 = nullptr;
    authRequestPacket.payloadSize18 = 0u;
    if (authRequestPayload == nullptr) {
        spdlog::error("launcher-owned auth failed to initialize recovered 0x4474f0 outbound packet builder");
        return;
    }
    std::memset(authRequestPayload, 0, kAuthBootstrap680Raw08RequestFixedByteCount);

    authRequestPayload[0] = 0x08u;
    *reinterpret_cast<uint32_t*>(authRequestPayload + 0x01u) = currentPublicKeyId9C;
    authRequestPayload[0x05u] = static_cast<uint8_t>(loginType28 & 0xffu);
    std::memcpy(authRequestPayload + 0x08u, block30.data(), block30.size());
    std::memcpy(authRequestPayload + 0x18u, block40.data(), block40.size());

    uint8_t* const authRequestReservationHeader =
        authRequestPayload + authRequestMessageStorage->PayloadByteCount();
    authRequestMessageStorage->GrowPayloadByteCount(
        static_cast<uint16_t>(raw08WorkerExpectedBlobLen + 2u));
    *reinterpret_cast<uint16_t*>(authRequestReservationHeader) =
        static_cast<uint16_t>(raw08WorkerExpectedBlobLen);
    *reinterpret_cast<uint16_t*>(authRequestPayload + 0x06u) =
        static_cast<uint16_t>(authRequestReservationHeader - authRequestPayload);
    authRequestPacket.debugString14 = reinterpret_cast<const char*>(authRequestReservationHeader + 2u);
    authRequestPacket.payloadSize18 = static_cast<uint16_t>(raw08WorkerExpectedBlobLen);
    if (authRequestPacket.debugString14 == nullptr) {
        spdlog::error("launcher-owned auth lost recovered 0x4474f0 ciphertext tail pointer");
        return;
    }

    // anchor: launcher.exe:0x468f00
    // Keep the raw08 per-chunk worker loop inline in `0x4474f0`, but route the concrete chunk
    // sizing / encrypt work through recovered worker-shaped methods instead of driving Crypto++
    // directly from this send helper.
    const uint32_t plaintextChunkByteCount =
        raw08PublicKeyWorkerA8->QueryPlaintextChunkByteCountScaffold(
            raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c);
    const uint32_t ciphertextChunkByteCount =
        raw08PublicKeyWorkerA8->QueryCiphertextChunkByteCountScaffold(
            raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c);
    if (plaintextChunkByteCount == 0u || ciphertextChunkByteCount == 0u) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth failed to recover 0x4474f0 raw08 chunk sizing");
        return;
    }

    uint8_t* ciphertextCursor =
        reinterpret_cast<uint8_t*>(const_cast<char*>(authRequestPacket.debugString14));
    const uint8_t* plaintextCursor = plaintextPayload;
    size_t plaintextBytesRemaining = plaintextPayloadByteCount;
    size_t ciphertextBytesWritten = 0u;
    while (plaintextBytesRemaining != 0u) {
        const size_t currentChunkByteCount =
            std::min<size_t>(plaintextChunkByteCount, plaintextBytesRemaining);
        if (ciphertextBytesWritten + ciphertextChunkByteCount > raw08WorkerExpectedBlobLen ||
            !raw08PublicKeyWorkerA8->EncryptPlaintextChunkScaffold(
                raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c,
                plaintextCursor,
                currentChunkByteCount,
                ciphertextCursor + ciphertextBytesWritten,
                ciphertextChunkByteCount)) {
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth failed to encrypt recovered 0x4474f0 plaintext blob through child+0xa8 raw08 worker loop");
            return;
        }

        plaintextCursor += currentChunkByteCount;
        plaintextBytesRemaining -= currentChunkByteCount;
        ciphertextBytesWritten += ciphertextChunkByteCount;
    }
    if (ciphertextBytesWritten != raw08WorkerExpectedBlobLen) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth rejected recovered 0x4474f0 ciphertext byte count actual={} expected={}",
            ciphertextBytesWritten,
            raw08WorkerExpectedBlobLen);
        return;
    }

    cachedAuthRequestTwofishKeyBytesOwned_.assign(
        feedbackSeed84.begin(),
        feedbackSeed84.end());

    auto* sendTarget =
        static_cast<mxo::liblttcp::CMessageConnection_0x4b7928*>(sendTarget50);
    sendTarget->SendPacketMessageRef(*authRequestPacket.messageRef08);
    const uint32_t sendResult = 1u;

    const uint16_t authRequestPayloadByteCount =
        authRequestPacket.messageRef08 ? authRequestPacket.messageRef08->PayloadByteCount() : 0u;
    const uint32_t headerByteCount = authRequestPayloadByteCount > 0x7fu ? 2u : 1u;
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via 0x4474f0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} helper={} module={} childBase={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsAuthRequest,
        static_cast<unsigned>(authRequestPayload[0]),
        mxo::auth::AuthOpcodeName(authRequestPayload[0]),
        static_cast<unsigned>(headerByteCount),
        static_cast<unsigned>(authRequestPayloadByteCount),
        static_cast<unsigned>(headerByteCount + authRequestPayloadByteCount),
        fmt::ptr(sendTarget50),
        fmt::ptr(this),
        fmt::ptr(owner08),
        fmt::ptr(childBase),
        static_cast<unsigned>(sendResult));
    g_CurrentLoginMediator->authRequestSent_ = true;

    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId={} loginType={} blobLen={} raw08WorkerExpectedBlobLen={} usernameLengthField={} plaintextPayloadLen={} plaintextUsernameFieldOffset=0x{:04x} ciphertextFieldOffset=0x{:04x} childSendTarget50={} childRaw08PublicKeyWorkerA8={} childString04Len={} childString10Len={} childString1CLen={} feedbackSeed84='{}' helper54NextBufferedByte28=0x{:08x}",
        static_cast<unsigned>(currentPublicKeyId9C),
        static_cast<unsigned>(authRequestPayload[0x05u]),
        static_cast<unsigned>(raw08WorkerExpectedBlobLen),
        static_cast<unsigned>(raw08WorkerExpectedBlobLen),
        static_cast<unsigned>(usernameLengthField),
        static_cast<unsigned>(plaintextPayloadByteCount),
        static_cast<unsigned>(ReadU16LE(plaintextPayload + 0x05u)),
        static_cast<unsigned>(ReadU16LE(authRequestPayload + 0x06u)),
        fmt::ptr(sendTarget50),
        fmt::ptr(raw08PublicKeyWorkerA8),
        static_cast<unsigned>(SmallStringMirrorLength(string04)),
        static_cast<unsigned>(SmallStringMirrorLength(string10)),
        static_cast<unsigned>(SmallStringMirrorLength(string1C)),
        BuildHexPreview(feedbackSeed84.data(), feedbackSeed84.size(), feedbackSeed84.size()),
        static_cast<unsigned>(feedbackSeedHelper54.nextBufferedOutputByte28));
}

// anchor: launcher.exe:0x447780
uint32_t AuthBootstrap680Child_0x441290::RebuildReplyPublicKeyWorkers(
    uint32_t replyPublicKeyId09,
    const uint8_t* modulusBytes,
    size_t modulusByteCount,
    uint8_t publicExponentByte,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) {
    auto& child = *this;

    if (modulusByteCount != 0x80u) {
        return 1u;
    }

    if (lazyPubkeyDatValidatorA4 == nullptr || !lazyPubkeyDatValidatorA4OwnedState_.object) {
        lazyPubkeyDatValidatorA4OwnedState_.object =
            std::make_unique<AuthBootstrap680LazyPubkeyDatValidatorA4Sketch>();
        if (lazyPubkeyDatValidatorA4OwnedState_.object->ConstructFromReplyPublicKey(
                &lazyPubkeyDatValidatorA4OwnedState_.publicKeyPair0c,
                kAuthBootstrap680PubkeyDatFallbackModulus.data(),
                kAuthBootstrap680PubkeyDatFallbackModulus.size(),
                0x11u)) {
            lazyPubkeyDatValidatorA4 = lazyPubkeyDatValidatorA4OwnedState_.object.get();
        } else {
            lazyPubkeyDatValidatorA4OwnedState_.object.reset();
            lazyPubkeyDatValidatorA4 = nullptr;
        }
    }

    if (lazyPubkeyDatValidatorA4 == nullptr ||
        !AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(
            child,
            modulusBytes,
            modulusByteCount,
            publicExponentByte,
            signatureBytes,
            signatureByteCount)) {
        return 0x19000004u;
    }

    ResetAuthBootstrap680ReplyPublicKeyWorkers(child);
    currentPublicKeyId9C = replyPublicKeyId09;

    child.raw08PublicKeyWorkerA8OwnedState_.object =
        std::make_unique<AuthBootstrap680Raw08PublicKeyWorkerA8Sketch>();
    if (child.raw08PublicKeyWorkerA8OwnedState_.object->ConstructFromReplyPublicKey(
            &child.raw08PublicKeyWorkerA8OwnedState_.publicKeyPair0c,
            modulusBytes,
            modulusByteCount,
            publicExponentByte)) {
        child.raw08PublicKeyWorkerA8 = child.raw08PublicKeyWorkerA8OwnedState_.object.get();
    } else {
        child.raw08PublicKeyWorkerA8OwnedState_.object.reset();
        child.raw08PublicKeyWorkerA8 = nullptr;
    }

    child.replyAuthDataValidatorACOwnedState_.object =
        std::make_unique<AuthBootstrap680ReplyAuthDataValidatorACSketch>();
    if (child.replyAuthDataValidatorACOwnedState_.object->ConstructFromReplyPublicKey(
            &child.replyAuthDataValidatorACOwnedState_.publicKeyPair0c,
            modulusBytes,
            modulusByteCount,
            publicExponentByte)) {
        child.replyAuthDataValidatorAC = child.replyAuthDataValidatorACOwnedState_.object.get();
    } else {
        child.replyAuthDataValidatorACOwnedState_.object.reset();
        child.replyAuthDataValidatorAC = nullptr;
    }

    return 0u;
}

// anchor: launcher.exe:0x447f50 / 0x447780 / 0x447260 / 0x447c10
uint32_t AuthBootstrap680Child_0x441290::HandleGetPublicKeyReply(
    const Packet_AsGetPublicKeyReply_0x4b6ca4& replyPacket) {
    auto* const childBase = static_cast<AuthBootstrap680ChildBase_0x4b7134*>(this);
    auto& child = *this;

    const uint8_t* const replyPayloadBytes =
        static_cast<const uint8_t*>(replyPacket.payloadAlias10);
    size_t replyPayloadByteCount = 0x12u;
    if (replyPacket.debugString14 != nullptr && replyPacket.payloadSize18 != 0u) {
        const auto* modulusFieldBytes =
            reinterpret_cast<const uint8_t*>(replyPacket.debugString14);
        if (modulusFieldBytes >= replyPayloadBytes) {
            replyPayloadByteCount = std::max(
                replyPayloadByteCount,
                static_cast<size_t>(modulusFieldBytes - replyPayloadBytes) +
                    static_cast<size_t>(replyPacket.payloadSize18));
        }
    }
    if (replyPacket.characterIdLow1c != 0u && replyPacket.characterIdHigh20 != 0u) {
        const auto* signatureFieldBytes =
            reinterpret_cast<const uint8_t*>(replyPacket.characterIdLow1c);
        if (signatureFieldBytes >= replyPayloadBytes) {
            replyPayloadByteCount = std::max(
                replyPayloadByteCount,
                static_cast<size_t>(signatureFieldBytes - replyPayloadBytes) +
                    static_cast<size_t>(replyPacket.characterIdHigh20));
        }
    }

    const uint32_t replyPublicKeyId09 = ReadU32LE(replyPayloadBytes + 0x09u);

    const uint8_t* modulusBytes = nullptr;
    size_t modulusByteCount = 0u;
    uint8_t publicExponentByte = 0u;
    const uint8_t* signatureBytes = nullptr;
    size_t signatureByteCount = 0u;
    const bool hasEmbeddedPublicKey = AuthBootstrap680_TryGetEmbeddedPublicKeyFromPayload(
        replyPayloadBytes,
        replyPayloadByteCount,
        &modulusBytes,
        &modulusByteCount,
        &publicExponentByte,
        &signatureBytes,
        &signatureByteCount,
        nullptr);

    if (replyPublicKeyId09 == currentPublicKeyId9C) {
        authRequestReadyA0 = 1u;
        return 0u;
    }

    if (g_SkipAuthPublicKeyReplyValidation == 0u) {
        if (!hasEmbeddedPublicKey || modulusByteCount != 0x80u || publicExponentByte == 0u) {
            authRequestReadyA0 = 0u;
            return 1u;
        }
    } else {
        if (!hasEmbeddedPublicKey || modulusByteCount == 0u || publicExponentByte == 0u) {
            authRequestReadyA0 = 0u;
            return 1u;
        }
        spdlog::info(
            "DIAGNOSTIC: skipping AS_GetPublicKeyReply modulus size validation (g_SkipAuthPublicKeyReplyValidation=1) modulusBytes={}",
            modulusByteCount);
    }
    const uint32_t rebuildResult = RebuildReplyPublicKeyWorkers(
        replyPublicKeyId09,
        modulusBytes,
        modulusByteCount,
        publicExponentByte,
        signatureBytes,
        signatureByteCount);
    if (rebuildResult != 0u) {
        authRequestReadyA0 = 0u;
        return rebuildResult;
    }

    authRequestReadyA0 = 1u;
    spdlog::info(
        "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleGetPublicKeyReply rebuilt child+0xa4/+0xa8/+0xac from raw0x07 publicKeyId={} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} modulusLen={} signatureLen={} exponentByte=0x{:02x} helper={} module={} childBase={}",
        static_cast<unsigned>(replyPublicKeyId09),
        fmt::ptr(child.lazyPubkeyDatValidatorA4),
        fmt::ptr(child.raw08PublicKeyWorkerA8),
        fmt::ptr(child.replyAuthDataValidatorAC),
        static_cast<unsigned>(modulusByteCount),
        static_cast<unsigned>(signatureByteCount),
        static_cast<unsigned>(publicExponentByte),
        fmt::ptr(this),
        fmt::ptr(owner08),
        fmt::ptr(childBase));
    return 0u;
}

}  // namespace mxo::ltlogin
