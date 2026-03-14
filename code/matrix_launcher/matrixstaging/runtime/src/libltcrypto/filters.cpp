// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltcrypto\filters.cpp
//
// Transitional reimplementation note:
// This file currently hosts low-level auth text/packet parse helpers that are below the
// launcher-owned login state machine but not yet split to exact original class boundaries.
//
// Address anchors:
// - later incoming auth-reply owner handler: launcher.exe:0x4401a0
//   renamed in Ghidra as CLTLoginMediator_Helper10_HandleAuthReply
// - concrete auth-reply parser object helper: launcher.exe:0x43a330
// - auth opcode read helper used by that later path: launcher.exe:0x41bc20
// - exact original parser VAs for 0x07/0x09 packet bodies: [not yet isolated]

#include "auth_internal.h"

namespace mxo::auth {

const char* AuthOpcodeName(uint8_t rawCode) {
    switch (rawCode) {
        case 0x06: return "AS_GetPublicKeyRequest";
        case 0x07: return "AS_GetPublicKeyReply";
        case 0x08: return "AS_AuthRequest";
        case 0x09: return "AS_AuthChallenge";
        case 0x0a: return "AS_AuthChallengeResponse";
        case 0x0b: return "AS_AuthReply";
        case 0x35: return "AS_GetWorldListRequest";
        case 0x36: return "AS_GetWorldListReply";
        default: return "<unknown-auth-code>";
    }
}

std::string HexEncode(const uint8_t* data, size_t size) {
    static const char kHexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(size * 2u);
    for (size_t i = 0; i < size; ++i) {
        const uint8_t value = data[i];
        hex.push_back(kHexDigits[(value >> 4u) & 0x0fu]);
        hex.push_back(kHexDigits[value & 0x0fu]);
    }
    return hex;
}

std::string HexEncode(const std::vector<uint8_t>& bytes) {
    return bytes.empty() ? std::string() : HexEncode(bytes.data(), bytes.size());
}

bool HexDecode(const std::string& text, std::vector<uint8_t>* outBytes) {
    if (!outBytes || (text.size() & 1u) != 0u) {
        return false;
    }

    outBytes->assign(text.size() / 2u, 0u);
    for (size_t i = 0; i < outBytes->size(); ++i) {
        const char high = text[i * 2u];
        const char low = text[i * 2u + 1u];
        unsigned value = 0u;

        const char pair[2] = {high, low};
        for (size_t j = 0; j < 2u; ++j) {
            value <<= 4u;
            const char nibble = pair[j];
            if (nibble >= '0' && nibble <= '9') {
                value |= static_cast<unsigned>(nibble - '0');
            } else if (nibble >= 'a' && nibble <= 'f') {
                value |= static_cast<unsigned>(nibble - 'a' + 10);
            } else if (nibble >= 'A' && nibble <= 'F') {
                value |= static_cast<unsigned>(nibble - 'A' + 10);
            } else {
                outBytes->clear();
                return false;
            }
        }

        (*outBytes)[i] = static_cast<uint8_t>(value);
    }

    return true;
}

bool ParseGetPublicKeyReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    GetPublicKeyReply* outReply) {
    using namespace internal;

    if (!payloadBytes || !outReply || payloadSize < 14u || payloadBytes[0] != 0x07u) {
        return false;
    }

    GetPublicKeyReply reply;
    reply.valid = true;
    reply.payloadLength = static_cast<uint32_t>(payloadSize);
    reply.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);

    std::memcpy(&reply.status, payloadBytes + 1u, sizeof(uint32_t));
    std::memcpy(&reply.currentTime, payloadBytes + 5u, sizeof(uint32_t));
    std::memcpy(&reply.publicKeyId, payloadBytes + 9u, sizeof(uint32_t));
    reply.keySize = payloadBytes[13u];
    reply.tailBytes.assign(payloadBytes + 14u, payloadBytes + payloadSize);

    if (payloadSize >= 20u) {
        reply.reservedByte = payloadBytes[14u];
        reply.publicExponentByte = payloadBytes[15u];
        std::memcpy(&reply.unknownWord, payloadBytes + 16u, sizeof(uint16_t));
        std::memcpy(&reply.modulusLength, payloadBytes + 18u, sizeof(uint16_t));

        const size_t modulusStart = 20u;
        const size_t modulusEnd = modulusStart + reply.modulusLength;
        if (reply.modulusLength != 0u && modulusEnd + 2u <= payloadSize) {
            reply.modulusBytes.assign(payloadBytes + modulusStart, payloadBytes + modulusEnd);
            std::memcpy(&reply.signatureLength, payloadBytes + modulusEnd, sizeof(uint16_t));
            const size_t signatureStart = modulusEnd + 2u;
            const size_t signatureEnd = signatureStart + reply.signatureLength;
            if (signatureEnd <= payloadSize) {
                reply.signatureBytes.assign(payloadBytes + signatureStart, payloadBytes + signatureEnd);
                reply.hasEmbeddedPublicKey = !reply.modulusBytes.empty() && reply.publicExponentByte != 0u;
            }
        }
    }

    *outReply = reply;
    return true;
}

bool ParseGetPublicKeyReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    GetPublicKeyReply* outReply) {
    if (!packetBytes || !outReply) {
        return false;
    }

    FramedPacket framed;
    if (!ParseVariableLengthPacket(packetBytes, packetSize, &framed)) {
        return false;
    }

    GetPublicKeyReply reply;
    if (!ParseGetPublicKeyReplyPayload(
            framed.payloadBytes.data(),
            framed.payloadBytes.size(),
            &reply)) {
        return false;
    }

    reply.headerBytes = framed.headerBytes;
    reply.bytes = framed.bytes;
    *outReply = reply;
    return true;
}

bool ParseAuthChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthChallenge* outChallenge) {
    if (!payloadBytes || !outChallenge || payloadSize != 17u || payloadBytes[0] != 0x09u) {
        return false;
    }

    AuthChallenge challenge;
    challenge.valid = true;
    challenge.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);
    challenge.encryptedChallengeBytes.assign(payloadBytes + 1u, payloadBytes + payloadSize);
    *outChallenge = challenge;
    return true;
}

bool ParseAuthChallengePacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthChallenge* outChallenge) {
    if (!packetBytes || !outChallenge) {
        return false;
    }

    FramedPacket framed;
    if (!ParseVariableLengthPacket(packetBytes, packetSize, &framed)) {
        return false;
    }

    AuthChallenge challenge;
    if (!ParseAuthChallengePayload(
            framed.payloadBytes.data(),
            framed.payloadBytes.size(),
            &challenge)) {
        return false;
    }

    challenge.headerBytes = framed.headerBytes;
    challenge.bytes = framed.bytes;
    *outChallenge = challenge;
    return true;
}

bool ParseAuthReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthReply* outReply) {
    using namespace internal;

    static const size_t kAuthReplyHeaderSize = 33u;
    static const size_t kCharacterRecordSize = 14u;
    static const size_t kWorldRecordSize = 32u;
    static const size_t kSignedDataSize = 182u;

    if (!payloadBytes || !outReply || payloadSize < 11u || payloadBytes[0] != 0x0bu) {
        return false;
    }

    AuthReply reply;
    reply.valid = true;
    reply.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);

    if (payloadSize < kAuthReplyHeaderSize) {
        reply.isErrorReply = true;
        reply.errorCode = ReadU32LE(payloadBytes + 1u);
        if (payloadSize >= 9u) {
            reply.zeroDword = ReadU32LE(payloadBytes + 5u);
        }
        if (payloadSize >= 11u) {
            reply.trailingWord = ReadU16LE(payloadBytes + 9u);
        }
        *outReply = reply;
        return true;
    }

    reply.offsetAuthData = ReadU16LE(payloadBytes + 11u);
    reply.offsetEncryptedData = ReadU16LE(payloadBytes + 13u);
    reply.unknown2 = ReadU32LE(payloadBytes + 15u);
    reply.offsetCharData = ReadU16LE(payloadBytes + 19u);
    reply.unknown3 = ReadU32LE(payloadBytes + 21u);
    reply.offsetServerData = ReadU32LE(payloadBytes + 25u);
    reply.offsetUsername = ReadU32LE(payloadBytes + 29u);

    if (reply.offsetCharData + 2u <= payloadSize) {
        reply.characterCount = ReadU16LE(payloadBytes + reply.offsetCharData);
        const size_t charactersBase = reply.offsetCharData + 2u;
        const size_t charactersDataEnd = charactersBase +
            (static_cast<size_t>(reply.characterCount) * kCharacterRecordSize);
        if (charactersDataEnd <= payloadSize && charactersDataEnd <= reply.offsetServerData) {
            for (uint16_t i = 0; i < reply.characterCount; ++i) {
                const size_t recordOffset = charactersBase + (static_cast<size_t>(i) * kCharacterRecordSize);
                const uint8_t* record = payloadBytes + recordOffset;
                AuthCharacterEntry entry;
                entry.unknownByte = record[0];
                entry.handleStringOffset = ReadU16LE(record + 1u);
                entry.characterId = ReadU64LE(record + 3u);
                entry.status = record[11u];
                entry.worldId = ReadU16LE(record + 12u);
                const size_t handleOffset = recordOffset + entry.handleStringOffset;
                (void)ParseMxoStringAtOffset(payloadBytes, payloadSize, handleOffset, &entry.handle);
                reply.characters.push_back(entry);
            }
        }
    }

    if (reply.offsetServerData + 2u <= payloadSize) {
        reply.worldCount = ReadU16LE(payloadBytes + reply.offsetServerData);
        const size_t worldsBase = reply.offsetServerData + 2u;
        const size_t worldsDataEnd = worldsBase +
            (static_cast<size_t>(reply.worldCount) * kWorldRecordSize);
        if (worldsDataEnd <= payloadSize && worldsDataEnd <= reply.offsetAuthData) {
            for (uint16_t i = 0; i < reply.worldCount; ++i) {
                const size_t recordOffset = worldsBase + (static_cast<size_t>(i) * kWorldRecordSize);
                const uint8_t* record = payloadBytes + recordOffset;
                AuthWorldEntry world;
                world.unknownByte = record[0];
                world.worldId = ReadU16LE(record + 1u);
                world.worldName = TrimFixedCString(record + 3u, 20u);
                world.status = record[23u];
                world.type = record[24u];
                world.clientVersion = ReadU32LE(record + 25u);
                world.unknown4 = ReadU16LE(record + 29u);
                world.load = record[31u];
                reply.worlds.push_back(world);
            }
        }
    }

    if (reply.offsetAuthData + 2u <= payloadSize) {
        reply.authDataMarker = ReadU16LE(payloadBytes + reply.offsetAuthData);
        reply.hasAuthDataMarker = true;

        const size_t markerEnd = reply.offsetAuthData + 2u;
        if (reply.offsetEncryptedData >= markerEnd + kSignedDataSize && reply.offsetEncryptedData <= payloadSize) {
            const size_t signatureLen = reply.offsetEncryptedData - markerEnd - kSignedDataSize;
            if (signatureLen <= payloadSize && markerEnd + signatureLen + kSignedDataSize <= payloadSize) {
                reply.authSignatureBytes.assign(
                    payloadBytes + markerEnd,
                    payloadBytes + markerEnd + signatureLen);

                const uint8_t* signedDataBytes = payloadBytes + markerEnd + signatureLen;
                reply.signedData.valid = true;
                reply.signedData.rawBytes.assign(signedDataBytes, signedDataBytes + kSignedDataSize);
                reply.signedData.unknownByte = signedDataBytes[0];
                reply.signedData.userId1 = ReadU32LE(signedDataBytes + 1u);
                reply.signedData.userName = TrimFixedCString(signedDataBytes + 5u, 33u);
                reply.signedData.unknownShort = ReadU16LE(signedDataBytes + 38u);
                reply.signedData.padding1 = ReadU32LE(signedDataBytes + 40u);
                reply.signedData.expiryTime = ReadU32LE(signedDataBytes + 44u);
                reply.signedData.padding2.assign(signedDataBytes + 48u, signedDataBytes + 80u);
                reply.signedData.publicExponent =
                    static_cast<uint16_t>((signedDataBytes[80u] << 8u) | signedDataBytes[81u]);
                reply.signedData.modulusBytes.assign(signedDataBytes + 82u, signedDataBytes + 178u);
                reply.signedData.timeCreated = ReadU32LE(signedDataBytes + 178u);
            }
        }
    }

    if (reply.offsetEncryptedData + 2u <= payloadSize) {
        reply.encryptedPrivateExponentLength = ReadU16LE(payloadBytes + reply.offsetEncryptedData);
        const size_t privateExponentStart = reply.offsetEncryptedData + 2u;
        const size_t privateExponentEnd = privateExponentStart + reply.encryptedPrivateExponentLength;
        if (privateExponentEnd <= payloadSize && privateExponentEnd <= reply.offsetUsername) {
            reply.encryptedPrivateExponentBytes.assign(
                payloadBytes + privateExponentStart,
                payloadBytes + privateExponentEnd);
        }
    }

    if (reply.offsetUsername + 2u <= payloadSize) {
        ParseMxoStringAtOffset(payloadBytes, payloadSize, reply.offsetUsername, &reply.username);
    }

    *outReply = reply;
    return true;
}

bool ParseAuthReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthReply* outReply) {
    if (!packetBytes || !outReply) {
        return false;
    }

    FramedPacket framed;
    if (!ParseVariableLengthPacket(packetBytes, packetSize, &framed)) {
        return false;
    }

    AuthReply reply;
    if (!ParseAuthReplyPayload(
            framed.payloadBytes.data(),
            framed.payloadBytes.size(),
            &reply)) {
        return false;
    }

    reply.headerBytes = framed.headerBytes;
    reply.bytes = framed.bytes;
    *outReply = reply;
    return true;
}

}  // namespace mxo::auth
