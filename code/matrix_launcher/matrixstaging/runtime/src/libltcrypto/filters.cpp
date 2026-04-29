// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltcrypto\filters.cpp
//
// Transitional reimplementation note:
// This file currently hosts low-level auth text/packet parse helpers that are below the
// launcher-owned login state machine but not yet split to exact original class boundaries.
//
// Address anchors:
// - later incoming auth-reply owner/helper handler: launcher.exe:0x4401a0
//   - current best `CLTLoginState` mapping: `CLTLoginState_State10_0x4b512c` vtable `0x004b512c` slot 6
//   - older Ghidra label: `CLTLoginMediator_Helper10_HandleAuthReply`
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

    reply.successHeaderUnknownWord05 = ReadU16LE(payloadBytes + 5u);
    reply.successHeaderUnknownDword07 = ReadU32LE(payloadBytes + 7u);
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

    auto parseSignedDataFromCopyShadow = [&](const uint8_t* signedDataBytes) {
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
    };

    if (reply.offsetAuthData + 2u <= payloadSize) {
        static const size_t kAuthDataCopyShadowSize = 0x136u;
        static const size_t kAuthDataSignatureOffset = 0x00u;
        static const size_t kAuthDataSignatureSize = 0x80u;
        static const size_t kAuthDataSignedDataOffset = 0x80u;

        const uint16_t authDataFieldLength = ReadU16LE(payloadBytes + reply.offsetAuthData);
        const size_t authDataFieldStart = reply.offsetAuthData + 2u;
        const size_t authDataFieldEnd = authDataFieldStart + authDataFieldLength;
        reply.authDataFieldLength = authDataFieldLength;

        if (authDataFieldLength != 0u && authDataFieldEnd <= payloadSize) {
            reply.authDataBytes.assign(payloadBytes + authDataFieldStart, payloadBytes + authDataFieldEnd);
        }

        // Exact launcher parse-object consequence from `0x443470 / 0x448140`:
        // `offsetAuthData` points at a u16 length prefix, and the bytes after that prefix are the
        // copied `0x136` block later adopted at child `+0xf4`.
        // Static suffix-offset use now tightens that copied block itself to:
        // - `+0x00 .. +0x7f` = 128-byte signature span
        // - `+0x80 .. +0x135` = `0xb6` signed-data span
        if (authDataFieldLength == kAuthDataCopyShadowSize &&
            authDataFieldEnd <= payloadSize) {
            const uint8_t* copyShadowBytes = payloadBytes + authDataFieldStart;
            reply.authDataFirstWord = 0u;
            reply.authDataMarker = 0u;
            reply.hasAuthDataMarker = false;
            reply.authSignatureBytes.assign(
                copyShadowBytes + kAuthDataSignatureOffset,
                copyShadowBytes + kAuthDataSignatureOffset + kAuthDataSignatureSize);
            parseSignedDataFromCopyShadow(copyShadowBytes + kAuthDataSignedDataOffset);
        } else {
            // Narrow fallback for older loose parser behavior while keeping the exact
            // length-prefixed `0x136` block as the preferred active-path model.
            reply.authDataMarker = ReadU16LE(payloadBytes + reply.offsetAuthData);
            reply.hasAuthDataMarker = true;

            const size_t markerEnd = reply.offsetAuthData + 2u;
            if (reply.offsetEncryptedData >= markerEnd + kSignedDataSize && reply.offsetEncryptedData <= payloadSize) {
                const size_t signatureLen = reply.offsetEncryptedData - markerEnd - kSignedDataSize;
                if (signatureLen <= payloadSize && markerEnd + signatureLen + kSignedDataSize <= payloadSize) {
                    reply.authSignatureBytes.assign(
                        payloadBytes + markerEnd,
                        payloadBytes + markerEnd + signatureLen);
                    parseSignedDataFromCopyShadow(payloadBytes + markerEnd + signatureLen);
                }
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

}  // namespace mxo::auth
