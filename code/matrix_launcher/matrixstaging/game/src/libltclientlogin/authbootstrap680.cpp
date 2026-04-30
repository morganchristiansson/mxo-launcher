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

static void ResetAuthBootstrap680BigIntObject(
    CryptoPP::Integer* outObject) {
    if (!outObject) {
        return;
    }

    *outObject = CryptoPP::Integer::Zero();
}

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
static void ResetAuthBootstrap680RsaPublicKey(CryptoPP::RSA::PublicKey* outPublicKey) {
    if (!outPublicKey) {
        return;
    }

    *outPublicKey = CryptoPP::RSA::PublicKey();
}

static bool CopyAuthBootstrap680BigIntToBigEndianBytes(
    const CryptoPP::Integer& value,
    std::vector<uint8_t>* outBytes) {
    if (!outBytes || value.IsNegative()) {
        return false;
    }

    const size_t byteCount = static_cast<size_t>(value.ByteCount());
    if (byteCount == 0u) {
        outBytes->clear();
        return true;
    }

    outBytes->assign(byteCount, 0u);
    value.Encode(outBytes->data(), outBytes->size());
    return true;
}

// anchor: launcher.exe:0x447120 / 0x447020
static bool BuildAuthBootstrap680RsaPublicKeyFromReplyPublicKey(
    CryptoPP::RSA::PublicKey* outPublicKey,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger) {
    if (!outPublicKey || !ownedState || modulusInteger.IsNegative() ||
        publicExponentInteger.IsNegative() || publicExponentInteger.IsZero()) {
        return false;
    }

    ResetAuthBootstrap680RsaPublicKey(outPublicKey);
    ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);

    try {
        ownedState->publicKey.Initialize(modulusInteger, publicExponentInteger);
        *outPublicKey = ownedState->publicKey;
        if (!CopyAuthBootstrap680BigIntToBigEndianBytes(modulusInteger, &ownedState->modulusBytes) ||
            !CopyAuthBootstrap680BigIntToBigEndianBytes(publicExponentInteger, &ownedState->exponentBytes)) {
            ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
            return false;
        }
    } catch (const CryptoPP::Exception&) {
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
        return false;
    }

    return true;
}

static bool BuildAuthBootstrap680Raw08PublicKeyWorkerA8(
    std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Encryptor>* outWorker,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger) {
    if (!outWorker || !ownedState) {
        return false;
    }

    outWorker->reset();
    if (!BuildAuthBootstrap680RsaPublicKeyFromReplyPublicKey(
            &ownedState->publicKey,
            ownedState,
            modulusInteger,
            publicExponentInteger)) {
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
        return false;
    }

    try {
        *outWorker = std::make_unique<CryptoPP::RSAES_OAEP_SHA_Encryptor>(ownedState->publicKey);
    } catch (const CryptoPP::Exception&) {
        outWorker->reset();
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
        return false;
    }
    return true;
}

static bool BuildAuthBootstrap680ReplyAuthDataValidatorAC(
    std::unique_ptr<CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier>* outValidator,
    AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger) {
    if (!outValidator || !ownedState) {
        return false;
    }

    outValidator->reset();
    if (!BuildAuthBootstrap680RsaPublicKeyFromReplyPublicKey(
            &ownedState->publicKey,
            ownedState,
            modulusInteger,
            publicExponentInteger)) {
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
        return false;
    }

    try {
        *outValidator = std::make_unique<CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier>(ownedState->publicKey);
    } catch (const CryptoPP::Exception&) {
        outValidator->reset();
        ResetAuthBootstrap680RsaPublicKeyPairOwnedState(ownedState);
        return false;
    }
    return true;
}

namespace {

static void ResetAuthBootstrap680ReplyPublicKeyWorkers(
 AuthBootstrap680ChildBase_0x4b7134& child) {
 child.raw08PublicKeyWorkerA8 = nullptr;
 child.replyAuthDataValidatorAC = nullptr;

 child.raw08PublicKeyWorkerA8Owned_.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&child.raw08PublicKeyWorkerA8PublicKeyPair0c_);
 child.replyAuthDataValidatorACOwned_.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&child.replyAuthDataValidatorACPublicKeyPair0c_);
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
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject) {
    if (!parseObject || !parseObject->ReplyHeader10()) {
        return;
    }

    uint8_t* const replyHeader = const_cast<uint8_t*>(parseObject->ReplyHeader10());
    replyHeader[0x00u] = 0x0bu;
    *reinterpret_cast<uint32_t*>(replyHeader + 0x01u) = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x05u) = 0u;
    parseObject->SetStringField05Bytes14(nullptr);
    parseObject->SetStringField05Length18(0u);
    *reinterpret_cast<uint32_t*>(replyHeader + 0x07u) = 0u;
    *reinterpret_cast<uint16_t*>(replyHeader + 0x0bu) = 0u;
    parseObject->SetAuthDataBytes1c(nullptr);
    parseObject->SetAuthDataByteLength20(0u);
    parseObject->ClearAuthDataPadding22();
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
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject,
    size_t packetBodyByteCount,
    bool zeroTerminateStrings) {
    if (!parseObject || !parseObject->ReplyHeader10()) {
        return;
    }

    const uint8_t* const packetBody = parseObject->ReplyHeader10();
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

    const uint8_t* stringField05Bytes14 = nullptr;
    uint16_t stringField05Length18 = 0u;
    resolveField(0x05u, true, &stringField05Bytes14, &stringField05Length18);
    parseObject->SetStringField05Bytes14(stringField05Bytes14);
    parseObject->SetStringField05Length18(stringField05Length18);

    const uint8_t* authDataBytes1c = nullptr;
    uint16_t authDataByteLength20 = 0u;
    resolveField(0x0bu, false, &authDataBytes1c, &authDataByteLength20);
    parseObject->SetAuthDataBytes1c(authDataBytes1c);
    parseObject->SetAuthDataByteLength20(authDataByteLength20);
    parseObject->ClearAuthDataPadding22();
    resolveField(0x0du, false, &parseObject->encryptedPrivateExponentBytes24, &parseObject->encryptedPrivateExponentByteLength28);
    resolveField(0x0fu, false, &parseObject->opaqueField0fBytes2c, &parseObject->opaqueField0fByteLength30);
    resolveField(0x11u, false, &parseObject->opaqueField11Bytes34, &parseObject->opaqueField11ByteLength38);
    resolveField(0x13u, false, &parseObject->characterTempRecords3c, &parseObject->characterTempRecordCount40);
    resolveField(0x19u, false, &parseObject->worldTempRecords44, &parseObject->worldTempRecordCount48);
    resolveField(0x1bu, false, &parseObject->opaqueField1bBytes4c, &parseObject->opaqueField1bByteLength50);
    resolveField(0x1du, true, &parseObject->replyString1dBytes54, &parseObject->replyString1dByteLength58);
}

// anchor: launcher.exe:0x444390 = Packet_AsGetPublicKeyRequest_0x4b6c74::Packet_AsGetPublicKeyRequest_ctor
// The same `0x004b6c74` vtable backs two launcher shapes:
// - the compact 9-byte send builder used by `0x447eb0`
// - this larger `0x8c` incoming-message parse shell used by `0x448140` for opcode `0x0b`
// Keep the ctor-like init flow faithful here, then let the smaller `0x4436b0/0x443470`
// method-family helpers own the writable-body reset vs resolved-field branch.
static Packet_AsGetPublicKeyRequest_0x4b6c74* Packet_AsGetPublicKeyRequest_CtorFromIncomingMessage(
    Packet_AsGetPublicKeyRequest_0x4b6c74* thisPacket,
    const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingAuthMessageRef,
    uint8_t resolveFields) {
    auto* outParseObject = thisPacket;
    if (!incomingAuthMessageRef) {
        return nullptr;
    }

    const auto* messageStorage = incomingAuthMessageRef->messageStorage0c;
    if (!messageStorage) {
        return nullptr;
    }

    const uint16_t incomingMessagePayloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const incomingMessagePayloadBytes = messageStorage->PayloadBase();
    if (!incomingMessagePayloadBytes || incomingMessagePayloadByteCount == 0u) {
        return nullptr;
    }

    const uint8_t* payloadBytes = incomingMessagePayloadBytes;
    size_t payloadByteCount = incomingMessagePayloadByteCount;
    if (incomingAuthMessageRef->headerless10 != 0u) {
        if (incomingMessagePayloadByteCount < 2u) {
            return nullptr;
        }

        const uint8_t locatorByte0d = incomingMessagePayloadBytes[1];
        const uint8_t targetLocatorType = static_cast<uint8_t>(locatorByte0d & 0x07u);
        const uint8_t senderLocatorType = static_cast<uint8_t>((locatorByte0d >> 4) & 0x07u);
        if (targetLocatorType == 0u || targetLocatorType > 6u ||
            senderLocatorType == 0u || senderLocatorType > 6u) {
            return nullptr;
        }

        const size_t payloadOffset =
            0x12u +
            static_cast<size_t>(kIncomingAuthMessageLocatorPayloadOffsetTable[targetLocatorType - 1u]) +
            static_cast<size_t>(kIncomingAuthMessageLocatorPayloadOffsetTable[senderLocatorType - 1u]);
        if (payloadOffset >= incomingMessagePayloadByteCount) {
            return nullptr;
        }

        payloadBytes = incomingMessagePayloadBytes + payloadOffset;
        payloadByteCount = static_cast<size_t>(incomingMessagePayloadByteCount) - payloadOffset;
    }

    if (!payloadBytes || payloadByteCount == 0u) {
        return nullptr;
    }

    *outParseObject = {};
    outParseObject->SetPacketBody04(payloadBytes);
    outParseObject->SetIncomingMessage08(
        const_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(incomingAuthMessageRef));
    outParseObject->SetResolveFields0c(resolveFields);
    outParseObject->SetReplyHeader10(outParseObject->PacketBody04());

    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->worldDescriptorAccessor5c,
        0x004b533cu,
        outParseObject->PacketBody04(),
        outParseObject->ResolveFields0c());
    outParseObject->worldDescriptorAccessor5c.incomingMessage08 = outParseObject->IncomingMessage08();
    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->slotRecordAccessor70,
        0x004b5328u,
        outParseObject->PacketBody04(),
        outParseObject->ResolveFields0c());
    outParseObject->slotRecordAccessor70.incomingMessage08 = outParseObject->IncomingMessage08();

    outParseObject->currentWorldTempRecord6c = nullptr;
    outParseObject->currentCharacterTempRecord80 = nullptr;
    outParseObject->currentCharacterHandle84 = nullptr;
    outParseObject->currentCharacterHandleByteLength88 = 0u;

    if (outParseObject->ResolveFields0c() == 0u) {
        AuthBootstrap680AuthReplyParseObject_ResetWritableBody_0x4436b0(outParseObject);
    } else {
        AuthBootstrap680AuthReplyParseObject_ResolveFieldViews_0x443470(
            outParseObject,
            payloadByteCount,
            true);
    }
    return reinterpret_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(outParseObject);
}

// Keep the `0x4449c0`-like copy step intentionally thin: copy the already-initialized source
// shell, rebase it onto child-owned packet bytes, then reuse the same smaller receiver helpers.
static bool AuthBootstrap680AuthReplyParseObject_CopyToOwnedPacketBody(
    Packet_AsGetPublicKeyRequest_0x4b6c74* outParseObject,
    const Packet_AsGetPublicKeyRequest_0x4b6c74& sourceParseObject,
    std::vector<uint8_t>* packetBodyBytes) {
    if (!outParseObject || !packetBodyBytes) {
        return false;
    }

    *outParseObject = sourceParseObject;
    outParseObject->SetPacketBody04(packetBodyBytes->empty() ? nullptr : packetBodyBytes->data());
    outParseObject->SetIncomingMessage08(nullptr);
    outParseObject->SetReplyHeader10(outParseObject->PacketBody04());
    outParseObject->currentWorldTempRecord6c = nullptr;
    outParseObject->currentCharacterTempRecord80 = nullptr;
    outParseObject->currentCharacterHandle84 = nullptr;
    outParseObject->currentCharacterHandleByteLength88 = 0u;

    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->worldDescriptorAccessor5c,
        0x004b533cu,
        outParseObject->PacketBody04(),
        outParseObject->ResolveFields0c());
    ResetAuthBootstrap680AuthReplyParseAccessor(
        &outParseObject->slotRecordAccessor70,
        0x004b5328u,
        outParseObject->PacketBody04(),
        outParseObject->ResolveFields0c());

    if (outParseObject->ResolveFields0c() == 0u) {
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
    ResetAuthBootstrap680BigIntObject(&child.modulusBigIntB0);
    ResetAuthBootstrap680BigIntObject(&child.publicExponentBigIntC4);
    ResetAuthBootstrap680BigIntObject(&child.privateExponentBigIntD8);
}



static bool BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
    CryptoPP::Integer* outObject,
    const uint8_t* bigEndianBytes,
    size_t byteCount) {
    if (!outObject || !bigEndianBytes || byteCount == 0u) {
        return false;
    }

    // anchor family: launcher.exe:0x45d000 / 0x45de10 / data type `0x4ba50c`
    // Static RE now proves the preserved child-side object family is old `CryptoPP::Integer`,
    // so source stores the real integer directly instead of rebuilding the legacy digit array.
    *outObject = CryptoPP::Integer(bigEndianBytes, byteCount);
    return true;
}

// anchor: launcher.exe:0x4472f0
// anchor: launcher.exe:0x447390
// anchor: launcher.exe:0x447340
// anchor: launcher.exe:0x447380
// anchor: launcher.exe:0x468520
// anchor: launcher.exe:0x467ee0
// anchor: launcher.exe:0x468f80 / 0x44aec0 / 0x4680a0 / 0x468e20
// Static RE now closes the validator-family accumulator/orchestration split tightly enough to
// model it with real Crypto++ semantics instead of a fake `0x84` launcher-owned worker layout:
// - `0x4472f0` creates the temporary MD5-backed verification accumulator
// - `0x447390` constructs the accumulator base state
// - `0x447340` behaves like `PK_MessageAccumulatorBase::Update`
// - `0x447380` behaves like `AccessHash()` for the embedded MD5 object
// - `0x468520` loads the RSA-decoded signature representative bytes into accumulator-owned state
// - `0x467ee0` / `0x467f70` finalize against the outer verifier object
// - `0x445410` returns the 18-byte MD5 `DigestInfo` prefix at `0x004baefc`
// - `0x468e20` builds the exact EMSA-PKCS1-v1_5 encoded block:
//     [optional leading 0 when (modulusBitCount-1)&7 != 0]
//     0x01 || 0xff... || 0x00 || DigestInfoPrefix || accumulatorMd5Digest
// - `0x4680a0` compares that generated block byte-for-byte against the loaded decoded
//   signature bytes
//
// Source keeps the boundary separation visible even though the temporary worker struct is gone:
// - outer object: `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier`
// - temporary object: MD5-backed `PK_MessageAccumulator` semantics
//
// The remaining helper below is therefore a source-local mirror of the concrete launcher chain
// `0x468520 -> 0x447340 -> 0x467ee0 -> 0x4680a0 -> 0x468e20 -> 0x445410`, not a single recovered
// library leaf. On the child `+0xac` path this intentionally means a second MD5 stage over the
// caller's already-prebuilt 16-byte MD5 digest, because `0x44ae40` hashes signed-data first and
// the temporary accumulator then hashes those 16 bytes again before the final compare.
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
    CryptoPP::Weak1::MD5 md5;
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

static bool VerifyAuthBootstrap680ReplyAuthDataValidatorRecoveredFinalizeScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* signedBytes,
    size_t signedByteCount,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) {
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

static uint32_t QueryAuthBootstrap680Raw08PublicKeyWorkerCiphertextChunkByteCountScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) {
    return static_cast<uint32_t>(
        QueryAuthBootstrap680Raw08RecoveredCiphertextBlockByteCount(ownedState.publicKey));
}

static uint32_t QueryAuthBootstrap680Raw08PublicKeyWorkerPlaintextChunkByteCountScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) {
    CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(ownedState.publicKey);
    return static_cast<uint32_t>(encryptor.FixedMaxPlaintextLength());
}

static bool EncryptAuthBootstrap680Raw08PlaintextChunkScaffold(
    const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
    const uint8_t* plaintextBytes,
    size_t plaintextByteCount,
    uint8_t* ciphertextBytes,
    size_t ciphertextByteCapacity) {
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

        // Preserve the raw-0x08 layering recovered from launcher.exe even though we no longer keep
        // a dedicated `0x48` helper layout struct in source:
        // - outer worker family here is `CryptoPP::RSAES_OAEP_SHA_Encryptor`
        // - inner per-chunk helper is the old `PK_DefaultEncryptionFilter`
        // - that helper owns a `ByteQueue` plaintext accumulator with `NodeSize = 0x100`
        //
        // anchor: launcher.exe:0x438120
        // anchor: launcher.exe:0x438320
        // anchor: launcher.exe:0x4382c0
        // anchor: launcher.exe:0x454f10
        // Mirror the message-end semantics of `CryptoPP_PK_DefaultEncryptionFilter_Put2` instead of
        // erasing the boundary into a one-shot convenience encrypt call:
        // 1. queue plaintext into a `ByteQueue`
        // 2. query queued size
        // 3. drain queued plaintext into a temporary contiguous buffer
        // 4. feed that plaintext through a `PK_EncryptorFilter`-equivalent emission step
        CryptoPP::ByteQueue plaintextQueue;
        plaintextQueue.Put(plaintextBytes, plaintextByteCount);
        const size_t queuedPlaintextByteCount = plaintextQueue.CurrentSize();
        if (queuedPlaintextByteCount != plaintextByteCount) {
            return false;
        }

        std::string queuedPlaintext(queuedPlaintextByteCount, '\0');
        plaintextQueue.Get(
            reinterpret_cast<CryptoPP::byte*>(queuedPlaintext.data()),
            queuedPlaintextByteCount);

        std::string ciphertextChunk;
        CryptoPP::PK_EncryptorFilter encryptFilter(
            rng,
            encryptor,
            new CryptoPP::StringSink(ciphertextChunk));
        encryptFilter.Put(
            reinterpret_cast<const CryptoPP::byte*>(queuedPlaintext.data()),
            queuedPlaintext.size());
        encryptFilter.MessageEnd();
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

static void ClearSmallStringMirror(std::string& mirror) {
    mirror.clear();
}

static void AssignSmallStringMirror(
    std::string& mirror,
    const char* begin,
    const char* current) {
    ClearSmallStringMirror(mirror);
    if (!begin || !current || current <= begin) {
        return;
    }

    mirror.assign(begin, current);
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

static void AssignSmallStringMirror(std::string& mirror, const char* text) {
    if (!text) {
        ClearSmallStringMirror(mirror);
        return;
    }
    AssignSmallStringMirror(mirror, text, text + std::char_traits<char>::length(text));
}

static size_t SmallStringMirrorLength(const std::string& mirror) {
    return mirror.size();
}

static const char* SmallStringMirrorDataOrEmpty(const std::string& mirror) {
    return mirror.empty() ? "" : mirror.c_str();
}

}  // namespace
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

 lazyPubkeyDatValidatorA4Owned_.reset();
 ResetAuthBootstrap680RsaPublicKeyPairOwnedState(&lazyPubkeyDatValidatorA4PublicKeyPair0c_);
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

 // +0xf8: stringF8 - source now models this directly as std::string
 stringF8.clear();

 // Base class destructor handles +0x00..+0xf4
}

// anchor: launcher.exe:0x441330
void AuthBootstrap680Child_0x441290::SetPromptPasswordF8AndSecurIdFlag(
    const char* promptPasswordWithOptionalSecurId) {
    if (promptPasswordWithOptionalSecurId == nullptr) {
        stringF8.clear();
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

    stringF8 = std::move(promptPassword);
    crashReporterPromptForSecurId104 = promptForSecurId ? 1u : 0u;
}

// anchor: launcher.exe:0x441260
void AuthBootstrap680Child_0x441290::StoreField114AndTimestamp118(uint32_t field114Value) {
    authReplySuccessField15_114 = field114Value;
    authReplySuccessField15Timestamp118 = static_cast<uint32_t>(std::time(nullptr));
}

// anchor: launcher.exe:0x441170
void AuthBootstrap680Child_0x441290::CopyOpaqueReplyBlobs108_10c() {
    const Packet_AsGetPublicKeyRequest_0x4b6c74* const parseObject = authReplyParseObjectF0;

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
    const Packet_AsGetPublicKeyRequest_0x4b6c74* const parseObject = authReplyParseObjectF0;
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

static bool EnsureAuthBootstrap680LazyPubkeyDatValidator(
    AuthBootstrap680ChildBase_0x4b7134& child) {
    if (child.lazyPubkeyDatValidatorA4 != nullptr && child.lazyPubkeyDatValidatorA4Owned_) {
        return true;
    }

    static const CryptoPP::Integer kFallbackModulus(
        kAuthBootstrap680PubkeyDatFallbackModulus.data(),
        kAuthBootstrap680PubkeyDatFallbackModulus.size());
    static const CryptoPP::Integer kFallbackPublicExponent(0x11u);
    if (!BuildAuthBootstrap680ReplyAuthDataValidatorAC(
            &child.lazyPubkeyDatValidatorA4Owned_,
            &child.lazyPubkeyDatValidatorA4PublicKeyPair0c_,
            kFallbackModulus,
            kFallbackPublicExponent)) {
        child.lazyPubkeyDatValidatorA4Owned_.reset();
        child.lazyPubkeyDatValidatorA4 = nullptr;
        return false;
    }

    child.lazyPubkeyDatValidatorA4 = child.lazyPubkeyDatValidatorA4Owned_.get();
    return child.lazyPubkeyDatValidatorA4 != nullptr;
}

// anchor: launcher.exe:0x468f80
static bool AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(
    AuthBootstrap680ChildBase_0x4b7134& child,
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger,
    const uint8_t* signatureBytes,
    size_t signatureByteCount) {
    if (mxo::ltlogin::g_SkipAuthPublicKeyReplyValidation != 0u) {
        return true;
    }

    if (!EnsureAuthBootstrap680LazyPubkeyDatValidator(child)) {
        return false;
    }

    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> publicExponentBytes;
    if (!CopyAuthBootstrap680BigIntToBigEndianBytes(modulusInteger, &modulusBytes) ||
        !CopyAuthBootstrap680BigIntToBigEndianBytes(publicExponentInteger, &publicExponentBytes) ||
        modulusBytes.size() != 0x80u || publicExponentBytes.size() != 1u ||
        publicExponentBytes[0] == 0u || !signatureBytes || signatureByteCount == 0u) {
        return false;
    }

    std::array<uint8_t, 0x81u> signedReplyPublicKeyBytes{};
    std::copy_n(modulusBytes.data(), modulusBytes.size(), signedReplyPublicKeyBytes.begin());
    signedReplyPublicKeyBytes.back() = publicExponentBytes[0];
    return VerifyAuthBootstrap680ReplyAuthDataValidatorRecoveredFinalizeScaffold(
        child.lazyPubkeyDatValidatorA4PublicKeyPair0c_,
        signedReplyPublicKeyBytes.data(),
        signedReplyPublicKeyBytes.size(),
        signatureBytes,
        signatureByteCount);
}

// anchor: launcher.exe:0x447dd0
static uint32_t AuthBootstrap680_RecordReplyPublicKeyToPubkeyDat(
    uint32_t replyPublicKeyId09,
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger,
    const uint8_t* signatureBytes) {
    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> publicExponentBytes;
    if (!CopyAuthBootstrap680BigIntToBigEndianBytes(modulusInteger, &modulusBytes) ||
        !CopyAuthBootstrap680BigIntToBigEndianBytes(publicExponentInteger, &publicExponentBytes) ||
        signatureBytes == nullptr) {
        return 0u;
    }

    // `0x447dd0` constructs a launcher-owned output sink configured with:
    // - OutputFileName = "pubkey.dat"
    // - OutputBinaryMode = true
    // It then serializes the reply-public-key record into that sink by emitting:
    // - replyPublicKeyId09 via `0x437ef0`
    // - modulus integer object via virtual `+0x08`
    // - public exponent integer object via virtual `+0x08`
    // - a trailing NUL byte via `0x444640`
    // - a fixed 0x100-byte blob from `signatureBytes` via sink virtual `+0x14`
    //
    // The surrounding sink/file-object family is still unresolved, so keep the boundary explicit
    // and model just the exact serialized record shape rather than pretending this helper is a
    // direct std::ofstream append.
    std::vector<uint8_t> serializedRecord;
    serializedRecord.reserve(
        sizeof(replyPublicKeyId09) + modulusBytes.size() + publicExponentBytes.size() + 1u + 0x100u);
    serializedRecord.push_back(static_cast<uint8_t>(replyPublicKeyId09 & 0xffu));
    serializedRecord.push_back(static_cast<uint8_t>((replyPublicKeyId09 >> 8u) & 0xffu));
    serializedRecord.push_back(static_cast<uint8_t>((replyPublicKeyId09 >> 16u) & 0xffu));
    serializedRecord.push_back(static_cast<uint8_t>((replyPublicKeyId09 >> 24u) & 0xffu));
    serializedRecord.insert(serializedRecord.end(), modulusBytes.begin(), modulusBytes.end());
    serializedRecord.insert(serializedRecord.end(), publicExponentBytes.begin(), publicExponentBytes.end());
    serializedRecord.push_back(0u);
    serializedRecord.insert(serializedRecord.end(), signatureBytes, signatureBytes + 0x100u);
    return 1u;
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

 // +0xa4: lazyPubkeyDatValidatorA4 - original calls dtor(1) on the verifier-family object
 if (lazyPubkeyDatValidatorA4 != nullptr) {
 lazyPubkeyDatValidatorA4 = nullptr;
 }

 // +0xa8: raw08PublicKeyWorkerA8 - original calls dtor(1) on the encryptor-family object
 if (raw08PublicKeyWorkerA8 != nullptr) {
 raw08PublicKeyWorkerA8 = nullptr;
 }

 // +0xac: replyAuthDataValidatorAC - original calls dtor(1) on the verifier-family object
 if (replyAuthDataValidatorAC != nullptr) {
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

 // +0x04..+0x24: source now models these directly as std::string values
 string04.clear();
 string10.clear();
 string1C.clear();

 // Erase owned state from global map

}

// anchor: launcher.exe:0x444900
// Handles +0x04 (string04), +0x10 (string10), +0x28..+0x4c (block30, block40, sendTarget50, loginType28, launcherVersion2C)
// +0xf0 (authReplyParseObjectF0), +0xf4 (authReplyCopyShadowF4)
void AuthBootstrap680ChildBase_0x4b7134::ClearReplyParseAndCopyShadowFields() {
 // +0x04/+0x10: source now models these directly as std::string values.
 string04.clear();
 string10.clear();

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

// anchor: launcher.exe:0x4433c0
bool AuthBootstrap680Child_0x441290::State5ReplyCopyShadowMissingOrStale() const {
    const auto* copyShadow = static_cast<const AuthBootstrapReplyCopyShadowF4_0x44add0*>(authReplyCopyShadowF4);
    if (copyShadow == nullptr) {
        return true;
    }
    return copyShadow->IsFresh(static_cast<int>(authServerTimeBias80));
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
    CryptoPP::Weak1::MD5 md5;
    md5.Update(signedData80.data(), signedData80.size());
    md5.Final(outDigest->data());
}

// anchor: launcher.exe:0x44aec0
// Full fidelity: builds MD5 of signedData80 and delegates to validator
uint32_t AuthBootstrapReplyCopyShadowF4_0x44add0::VerifyWithValidator(
    const CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier* validator,
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
    return validator != nullptr &&
                   VerifyAuthBootstrap680ReplyAuthDataValidatorRecoveredFinalizeScaffold(
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
    const CryptoPP::Integer& modulus = *blockB0;
    const CryptoPP::Integer& publicExponent = *blockC4;
    const CryptoPP::Integer& privateExponent = *blockD8;

    const bool storedReplyCopy =
        marginConnection.StoreBootstrapReplyCopy98(copyShadow, sizeof(*copyShadow));
    mxo::liblttcp::CMarginConnectionBootstrapPrepStateOwner_0x443340(marginConnection)
        .StoreBootstrapPrepStateA0(modulus, publicExponent, privateExponent);

    spdlog::info(
        "AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0::PrepareState5MarginConnectionCopySend staged owner+0x680 child for state5 copy/send copyShadowF4={} storedReplyCopy98={} modulusBytes={} publicExponentBytes={} privateExponentBytes={} modulusBits={} publicExponentBits={} privateExponentBits={}",
        fmt::ptr(copyShadow),
        storedReplyCopy ? 1u : 0u,
        static_cast<unsigned>(modulus.ByteCount()),
        static_cast<unsigned>(publicExponent.ByteCount()),
        static_cast<unsigned>(privateExponent.ByteCount()),
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
        child.SendAuthRequest();
        return;
    }

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
                    CryptoPP::Weak1::MD5 challengeMd5;
                    if (!decryptedChallengeBytes.empty()) {
                        challengeMd5.Update(
                            decryptedChallengeBytes.data(),
                            decryptedChallengeBytes.size());
                    }
                    challengeMd5.Final(processedChallengeMd5Bytes.data());
                }

                // anchor: launcher.exe:0x44831c..0x448467
                // Ghidra confirms the raw 0x0a path constructs a real
                // `Packet_AsAuthChallengeResponse_0x4b6cf4` inline and then calls its small field
                // appenders directly:
                // - `0x444040` appends password string
                // - `0x444140` appends secondary/station string
                // - `0x4441a0(0x20)` writes the fixed little-endian word 0x0020
                // - `0x443660(0x20 - (payloadLen & 0x0f))` appends zero padding
                // So the old source-owned `AuthChallengeResponseLayout` knob bag was an infidel.
                static constexpr uint8_t kAuthChallengeResponseLeadingByte = 0x00;
                static constexpr uint16_t kAuthChallengeResponseUnknown1 = 23;
                static constexpr uint8_t kAuthChallengeResponsePaddingByte = 0x00;

                std::vector<uint8_t> passwordBytes(password, password + std::strlen(password));
                passwordBytes.push_back(0u);
                std::vector<uint8_t> soePasswordBytes(
                    soePassword,
                    soePassword + std::strlen(soePassword));
                soePasswordBytes.push_back(0u);
                if (passwordBytes.size() > 0xffffu || soePasswordBytes.size() > 0xffffu) {
                    spdlog::error("launcher-owned auth rejected oversized raw0x0a password field");
                    return kAuthBootstrap680InboundUnhandled;
                }

                const uint16_t passwordLengthField = static_cast<uint16_t>(passwordBytes.size());
                const uint16_t soePasswordLengthField = static_cast<uint16_t>(soePasswordBytes.size());

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
                plaintextBytes.push_back(kAuthChallengeResponseLeadingByte);
                plaintextBytes.insert(
                    plaintextBytes.end(),
                    processedChallengeMd5Bytes.begin(),
                    processedChallengeMd5Bytes.end());
                mxo::auth::internal::AppendU16LE(&plaintextBytes, kAuthChallengeResponseUnknown1);
                mxo::auth::internal::AppendU16LE(&plaintextBytes, passwordLengthField);
                mxo::auth::internal::AppendU16LE(&plaintextBytes, soePasswordLengthField);
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
                    kAuthChallengeResponsePaddingByte);

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
            Packet_AsGetPublicKeyRequest_0x4b6c74 sourceParseObject = {};
            auto* sourcePacket = reinterpret_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(
                &sourceParseObject);
            // anchor: launcher.exe:0x4481aa / 0x444390
            // The local `0x8c` packet view is built by the same ctor/vtable family as the raw
            // `0x06` request builder; the incoming-message branch just walks the larger parse shell.
            if ((sourcePacket = Packet_AsGetPublicKeyRequest_CtorFromIncomingMessage(
                     sourcePacket,
                     incomingAuthMessage,
                     true)) != nullptr) {
                const uint8_t* const payloadBytes = sourceParseObject.ReplyHeader10();
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
                    static_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(
                        std::malloc(sizeof(Packet_AsGetPublicKeyRequest_0x4b6c74)));
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
                (parseObject != nullptr && parseObject->ReplyHeader10() != nullptr)
                    ? ReadU32LE(parseObject->ReplyHeader10() + 0x01u)
                    : 0u;

            inboundAuthStatusEc =
                (storedParseObjectF0 && parseObject != nullptr && parseObject->ReplyHeader10() != nullptr)
                    ? ReadU32LE(parseObject->ReplyHeader10() + 0x01u)
                    : errorReplyStatus01;

            spdlog::debug(
                "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleInboundAuthMessage stored child+0xf0 parse copy={} status=0x{:08x} authDataLen=0x{:04x} encryptedPrivateExponentLen=0x{:04x} characterCount={} worldCount={} replyString1dLen=0x{:04x}",
                storedParseObjectF0 ? 1u : 0u,
                static_cast<unsigned>(inboundAuthStatusEc),
                static_cast<unsigned>(parseObject ? parseObject->AuthDataByteLength20() : 0u),
                static_cast<unsigned>(parseObject ? parseObject->encryptedPrivateExponentByteLength28 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->characterTempRecordCount40 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->worldTempRecordCount48 : 0u),
                static_cast<unsigned>(parseObject ? parseObject->replyString1dByteLength58 : 0u));

            if (inboundAuthStatusEc != 0u) {
                return kAuthBootstrap680InboundAuthReplyError;
            }

            const uint16_t authDataByteLength =
                parseObject ? parseObject->AuthDataByteLength20() : 0u;
            if (!storedParseObjectF0 || parseObject == nullptr ||
                authDataByteLength != sizeof(AuthBootstrapReplyCopyShadowF4_0x44add0) ||
                replyAuthDataValidatorAC == nullptr) {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            AuthBootstrapReplyCopyShadowF4_0x44add0 copyShadowCandidate = {};
            if (parseObject != nullptr && parseObject->AuthDataBytes1c() != nullptr &&
                parseObject->AuthDataByteLength20() == sizeof(copyShadowCandidate)) {
                std::copy_n(
                    parseObject->AuthDataBytes1c(),
                    sizeof(copyShadowCandidate),
                    reinterpret_cast<uint8_t*>(&copyShadowCandidate));
            } else {
                return kAuthBootstrap680InboundAuthReplyValidationError;
            }

            const bool replyAuthDataValidatorAccepted =
                copyShadowCandidate.VerifyWithValidator(
                    replyAuthDataValidatorACOwned_.get(),
                    replyAuthDataValidatorACPublicKeyPair0c_,
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
                parseObject->AuthDataBytes1c(),
                sizeof(copyShadow),
                reinterpret_cast<uint8_t*>(&copyShadow));

            const CryptoPP::Integer modulusInteger(
                copyShadow.signedData80.data() + 0x52u,
                0x60u);
            const CryptoPP::Integer publicExponentInteger(
                static_cast<unsigned int>(copyShadow.signedData80[0x51u]));

            CryptoPP::Integer* const blockB0 = &modulusBigIntB0;
            CryptoPP::Integer* const blockC4 = &publicExponentBigIntC4;
            CryptoPP::Integer* const blockD8 = &privateExponentBigIntD8;

            const size_t modulusByteCount =
                std::max<size_t>(1u, static_cast<size_t>(modulusInteger.MinEncodedSize()));
            std::vector<uint8_t> modulusBytes(modulusByteCount, 0u);
            modulusInteger.Encode(modulusBytes.data(), modulusBytes.size());
            const bool builtBlockB0 = BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                blockB0,
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
                    privateExponentBytes.data(),
                    privateExponentBytes.size());
            }

            spdlog::info(
                "HandleInboundAuthMessage materialized child+0xf4/+0xb0/+0xc4/+0xd8 from raw0x0b authDataBytes=0x{:03x} parseObjectAuthDataLen=0x{:04x} signaturePrefix00='{}' signedDataExpiryAc=0x{:08x} modulusPrefixD2='{}' authServerTimeBias80=0x{:08x} builtBlockB0={} builtBlockC4={} builtBlockD8={} blockB0Bytes={} blockC4Bytes={} blockD8Bytes={} parseObjectF0={}",
                static_cast<unsigned>(sizeof(copyShadow)),
                static_cast<unsigned>(parseObject->AuthDataByteLength20()),
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
                static_cast<unsigned>(blockB0->ByteCount()),
                static_cast<unsigned>(blockC4->ByteCount()),
                static_cast<unsigned>(blockD8->ByteCount()),
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
    // anchor: launcher.exe:0x447260 / 0x447c10
    const bool ensuredLazyPubkeyDatValidatorA4 = EnsureAuthBootstrap680LazyPubkeyDatValidator(*this);

    Packet_AsGetPublicKeyRequest_0x4b6c74 packetBuilder;
    // anchor: launcher.exe:0x447ec3..0x447ef3
    // The send path uses the default/base ctor shape first, then retables the local packet object
    // to `0x004b6c74` before filling the fixed 9-byte payload.
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

    std::vector<uint8_t> packetBytes;
    size_t packetHeaderByteCount = 0u;
    if (!mxo::auth::BuildVariableLengthPacket(
            getPublicKeyRequestPayload,
            Packet_AsGetPublicKeyRequest_0x4b6c74::kFixedByteCount,
            mxo::auth::kFrameModeAuto,
            &packetBytes,
            &packetHeaderByteCount)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to frame Packet_AsGetPublicKeyRequest_0x4b6c74");
        return;
    }

    if (!sendTarget50) {
        spdlog::warn(
            "AuthBootstrap680_SendGetPublicKeyRequest missing child+0x50 send target; recovered 0x447eb0 tail expects direct virtual send through that field");
        return;
    }

    auto* sendTarget = static_cast<mxo::liblttcp::CBaseConnection*>(sendTarget50);
    const uint8_t rawCode = getPublicKeyRequestPayload[0];
    const uint32_t sendResult = sendTarget->SendPacket(
        packetBytes.data(),
        static_cast<uint32_t>(packetBytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send via 0x447eb0 child+0x50->vtable+0x24 step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} sendTarget50={} ensuredLazyPubkeyDatValidatorA4={} childLazyPubkeyDatValidatorA4={} child={} helperOwner={} -> sendResult=0x{:08x}",
        CLTLoginMediator::kMessageAsGetPublicKeyRequest,
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        packetHeaderByteCount,
        Packet_AsGetPublicKeyRequest_0x4b6c74::kFixedByteCount,
        packetBytes.size(),
        fmt::ptr(sendTarget50),
        ensuredLazyPubkeyDatValidatorA4 ? 1u : 0u,
        fmt::ptr(lazyPubkeyDatValidatorA4),
        fmt::ptr(this),
        fmt::ptr(owner08),
        static_cast<unsigned>(sendResult));
}

// anchor: launcher.exe:0x4474f0
void AuthBootstrap680Child_0x441290::SendAuthRequest() {
    auto* const childBase = static_cast<AuthBootstrap680ChildBase_0x4b7134*>(this);

    const char* const username = SmallStringMirrorDataOrEmpty(string04);
    if (raw08PublicKeyWorkerA8 == nullptr ||
        raw08PublicKeyWorkerA8PublicKeyPair0c_.modulusBytes.empty() ||
        raw08PublicKeyWorkerA8PublicKeyPair0c_.exponentBytes.empty()) {
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
        QueryAuthBootstrap680Raw08PublicKeyWorkerEncryptedOutputLengthScaffold(
            raw08PublicKeyWorkerA8PublicKeyPair0c_,
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
        QueryAuthBootstrap680Raw08PublicKeyWorkerPlaintextChunkByteCountScaffold(
            raw08PublicKeyWorkerA8PublicKeyPair0c_);
    const uint32_t ciphertextChunkByteCount =
        QueryAuthBootstrap680Raw08PublicKeyWorkerCiphertextChunkByteCountScaffold(
            raw08PublicKeyWorkerA8PublicKeyPair0c_);
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
            !EncryptAuthBootstrap680Raw08PlaintextChunkScaffold(
                raw08PublicKeyWorkerA8PublicKeyPair0c_,
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
    const CryptoPP::Integer& modulusInteger,
    const CryptoPP::Integer& publicExponentInteger,
    const uint8_t* signatureBytes) {
    if (modulusInteger.ByteCount() != 0x80u) {
        return 1u;
    }

    if (!EnsureAuthBootstrap680LazyPubkeyDatValidator(*this) ||
        !AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator_SOURCEOWNED(
            *this,
            modulusInteger,
            publicExponentInteger,
            signatureBytes,
            0x80u)) {
        return 0x19000004u;
    }

    ResetAuthBootstrap680ReplyPublicKeyWorkers(*this);
    currentPublicKeyId9C = replyPublicKeyId09;

    if (BuildAuthBootstrap680Raw08PublicKeyWorkerA8(
            &raw08PublicKeyWorkerA8Owned_,
            &raw08PublicKeyWorkerA8PublicKeyPair0c_,
            modulusInteger,
            publicExponentInteger)) {
        raw08PublicKeyWorkerA8 = raw08PublicKeyWorkerA8Owned_.get();
    } else {
        raw08PublicKeyWorkerA8Owned_.reset();
        raw08PublicKeyWorkerA8 = nullptr;
    }

    if (BuildAuthBootstrap680ReplyAuthDataValidatorAC(
            &replyAuthDataValidatorACOwned_,
            &replyAuthDataValidatorACPublicKeyPair0c_,
            modulusInteger,
            publicExponentInteger)) {
        replyAuthDataValidatorAC = replyAuthDataValidatorACOwned_.get();
    } else {
        replyAuthDataValidatorACOwned_.reset();
        replyAuthDataValidatorAC = nullptr;
    }

    return 0u;
}

// anchor: launcher.exe:0x447f50
uint32_t AuthBootstrap680Child_0x441290::HandleGetPublicKeyReply(
    const Packet_AsGetPublicKeyReply_0x4b6ca4& replyPacket) {
    const uint8_t* const replyPayloadBytes =
        static_cast<const uint8_t*>(replyPacket.payloadAlias10);
    if (!replyPayloadBytes) {
        return 1u;
    }

    const uint32_t replyStatus01 = ReadU32LE(replyPayloadBytes + 0x01u);
    if (static_cast<int32_t>(replyStatus01) >= 1) {
        return replyStatus01;
    }

    const uint32_t replyPublicKeyId09 = ReadU32LE(replyPayloadBytes + 0x09u);
    if (replyPublicKeyId09 != currentPublicKeyId9C) {
        const uint8_t* const modulusBytes =
            reinterpret_cast<const uint8_t*>(replyPacket.debugString14);
        const size_t modulusByteCount = static_cast<size_t>(replyPacket.payloadSize18);
        const uint8_t publicExponentByte = replyPayloadBytes[0x0fu];
        const uint8_t* const signatureBytes =
            reinterpret_cast<const uint8_t*>(replyPacket.characterIdLow1c);
        const size_t signatureByteCount = static_cast<size_t>(replyPacket.characterIdHigh20);
        CryptoPP::Integer modulusInteger;
        CryptoPP::Integer publicExponentInteger;
        if (!BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                &modulusInteger,
                modulusBytes,
                modulusByteCount) ||
            !BuildPositiveAuthBootstrap680BigIntFromBigEndianBytes(
                &publicExponentInteger,
                &publicExponentByte,
                1u)) {
            authRequestReadyA0 = 0u;
            return 1u;
        }

        const uint32_t rebuildResult = RebuildReplyPublicKeyWorkers(
            replyPublicKeyId09,
            modulusInteger,
            publicExponentInteger,
            signatureBytes);
        if (rebuildResult != 0u) {
            authRequestReadyA0 = 0u;
            return rebuildResult;
        }

        AuthBootstrap680_RecordReplyPublicKeyToPubkeyDat(
            replyPublicKeyId09,
            modulusInteger,
            publicExponentInteger,
            signatureBytes);
        spdlog::info(
            "CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleGetPublicKeyReply rebuilt child+0xa4/+0xa8/+0xac from raw0x07 publicKeyId={} childLazyPubkeyDatValidatorA4={} childRaw08PublicKeyWorkerA8={} childReplyAuthDataValidatorAC={} modulusLen={} signatureLen={} exponentByte=0x{:02x} helper={} module={} childBase={}",
            static_cast<unsigned>(replyPublicKeyId09),
            fmt::ptr(lazyPubkeyDatValidatorA4),
            fmt::ptr(raw08PublicKeyWorkerA8),
            fmt::ptr(replyAuthDataValidatorAC),
            static_cast<unsigned>(modulusByteCount),
            static_cast<unsigned>(signatureByteCount),
            static_cast<unsigned>(publicExponentByte),
            fmt::ptr(this),
            fmt::ptr(owner08),
            fmt::ptr(static_cast<AuthBootstrap680ChildBase_0x4b7134*>(this)));
    }

    authRequestReadyA0 = 1u;
    return 0u;
}

}  // namespace mxo::ltlogin
