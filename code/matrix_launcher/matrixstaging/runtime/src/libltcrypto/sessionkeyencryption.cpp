// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp
//
// Transitional reimplementation note:
// This file currently carries the working low-level auth/session-key crypto path while the exact
// original class/function split is still being recovered.
//
// Address anchors:
// - phase-2 auth/bootstrap dispatcher: launcher.exe:0x448050
//   renamed in Ghidra as AuthBootstrap680_PrepareAndDispatch
// - raw 0x06 request builder/send path: launcher.exe:0x447eb0
//   strongest current AS_GetPublicKeyRequest candidate
// - raw 0x08 request builder/send path: launcher.exe:0x4474f0
//   strongest current AS_AuthRequest candidate
// - later challenge/material continuation anchor: launcher.exe:0x429b0
// - exact original raw 0x0a builder VA: [not yet isolated]
// - exact original auth-reply private-exponent decrypt/helper VA: [not yet isolated]

#include "auth_internal.h"

#include <cstdio>
#include <windows.h>

namespace mxo::auth {

bool BuildGetPublicKeyRequestPacket(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    using namespace internal;

    std::vector<uint8_t> payload;
    payload.reserve(9u);
    payload.push_back(0x06u);
    AppendU32LE(&payload, launcherVersion);
    AppendU32LE(&payload, currentPublicKeyId);
    return BuildVariableLengthPacket(payload.data(), payload.size(), frameMode, outPacket);
}

bool BuildAuthRequestBlobPlaintext(
    const std::string& username,
    const AuthBlobLayout& layout,
    std::vector<uint8_t>* outPlaintext,
    std::vector<uint8_t>* outTwofishKey,
    uint16_t* outUsernameLengthField) {
    using namespace internal;

    if (!outPlaintext || !outTwofishKey || !outUsernameLengthField || username.empty()) {
        return false;
    }

    std::vector<uint8_t> effectiveTwofishKey = layout.twofishKey;
    if (effectiveTwofishKey.empty()) {
        effectiveTwofishKey.resize(16u);
        CryptoPP::AutoSeededRandomPool rng;
        rng.GenerateBlock(effectiveTwofishKey.data(), effectiveTwofishKey.size());
    }
    if (effectiveTwofishKey.size() != 16u) {
        return false;
    }

    std::vector<uint8_t> usernameBytes(username.begin(), username.end());
    if (layout.includeUsernameNullTerminator) {
        usernameBytes.push_back(0u);
    }

    const int lengthField =
        static_cast<int>(usernameBytes.size()) + layout.usernameLengthAdjust;
    if (lengthField < 0 || lengthField > 0xffff) {
        return false;
    }

    outPlaintext->clear();
    outPlaintext->reserve(1u + 4u + 2u + 16u + 4u + 2u + usernameBytes.size());
    outPlaintext->push_back(layout.leadingByte);
    AppendU32LE(outPlaintext, layout.rsaMethod);
    AppendU16LE(outPlaintext, layout.someShort);
    outPlaintext->insert(
        outPlaintext->end(),
        effectiveTwofishKey.begin(),
        effectiveTwofishKey.end());
    AppendU32LE(outPlaintext, layout.embeddedTime);
    AppendU16LE(outPlaintext, static_cast<uint16_t>(lengthField));
    outPlaintext->insert(
        outPlaintext->end(),
        usernameBytes.begin(),
        usernameBytes.end());

    *outTwofishKey = effectiveTwofishKey;
    *outUsernameLengthField = static_cast<uint16_t>(lengthField);
    return true;
}

// Transitional low-level margin CERT/MS helper note:
// - wire/crypto shape here is intentionally shared runtime code
// - evidence source is open server/proxy code, especially:
//   - `../../../work/mxoemu/Reality/Source/MarginSocket.cpp`
//   - `../../../work/mxoemu/Proxy/Logging.cpp`
//   - `../../../work/mxoemu/Proxy/EncryptedPacket.cpp`
// - this is not a claim that final launcher-owned state progression should remain here

bool EncryptMarginPayloadPacket(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    using namespace internal;

    if (!payloadBytes || payloadSize == 0u || !outPacket || twofishKeyBytes.size() != 16u ||
        payloadSize > 0xffffu) {
        return false;
    }

    const uint16_t payloadLength = static_cast<uint16_t>(payloadSize);
    const uint32_t timestamp = CurrentUnixTimeU32();

    std::vector<uint8_t> crcInput;
    crcInput.reserve(2u + 4u + payloadSize);
    AppendU16LE(&crcInput, payloadLength);
    AppendU32LE(&crcInput, timestamp);
    crcInput.insert(crcInput.end(), payloadBytes, payloadBytes + payloadSize);

    uint8_t crcBytes[4] = {0};
    CryptoPP::CRC32 crc;
    crc.Update(crcInput.data(), crcInput.size());
    crc.Final(crcBytes);

    std::vector<uint8_t> plaintext;
    plaintext.reserve(4u + crcInput.size());
    plaintext.insert(plaintext.end(), crcBytes, crcBytes + sizeof(crcBytes));
    plaintext.insert(plaintext.end(), crcInput.begin(), crcInput.end());

    uint8_t ivBytes[16] = {0};
    std::vector<uint8_t> ciphertextBytes;
    try {
        CryptoPP::AutoSeededRandomPool rng;
        rng.GenerateBlock(ivBytes, sizeof(ivBytes));

        CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption cipher;
        cipher.SetKeyWithIV(twofishKeyBytes.data(), twofishKeyBytes.size(), ivBytes);

        std::string ciphertext;
        CryptoPP::StringSource source(
            plaintext.data(),
            plaintext.size(),
            true,
            new CryptoPP::StreamTransformationFilter(
                cipher,
                new CryptoPP::StringSink(ciphertext)));
        ciphertextBytes.assign(ciphertext.begin(), ciphertext.end());
    } catch (const CryptoPP::Exception&) {
        return false;
    }

    std::vector<uint8_t> encryptedPayload;
    encryptedPayload.reserve(sizeof(ivBytes) + ciphertextBytes.size());
    encryptedPayload.insert(encryptedPayload.end(), ivBytes, ivBytes + sizeof(ivBytes));
    encryptedPayload.insert(encryptedPayload.end(), ciphertextBytes.begin(), ciphertextBytes.end());
    return BuildVariableLengthPacket(encryptedPayload.data(), encryptedPayload.size(), frameMode, outPacket);
}

bool DecryptMarginPayloadPacket(
    const uint8_t* encryptedPayloadBytes,
    size_t encryptedPayloadSize,
    const std::vector<uint8_t>& twofishKeyBytes,
    std::vector<uint8_t>* outPayloadBytes) {
    using namespace internal;

    if (!encryptedPayloadBytes || !outPayloadBytes || encryptedPayloadSize <= 16u ||
        twofishKeyBytes.size() != 16u) {
        return false;
    }

    std::vector<uint8_t> decryptedBytes;
    try {
        CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption cipher;
        cipher.SetKeyWithIV(twofishKeyBytes.data(), twofishKeyBytes.size(), encryptedPayloadBytes);

        std::string decrypted;
        CryptoPP::StringSource source(
            encryptedPayloadBytes + 16u,
            encryptedPayloadSize - 16u,
            true,
            new CryptoPP::StreamTransformationFilter(
                cipher,
                new CryptoPP::StringSink(decrypted)));
        decryptedBytes.assign(decrypted.begin(), decrypted.end());
    } catch (const CryptoPP::Exception&) {
        outPayloadBytes->clear();
        return false;
    }

    if (decryptedBytes.size() < 10u) {
        outPayloadBytes->clear();
        return false;
    }

    const uint16_t payloadLength = ReadU16LE(decryptedBytes.data() + 4u);
    const size_t totalExpected = 4u + 2u + 4u + payloadLength;
    if (decryptedBytes.size() != totalExpected) {
        outPayloadBytes->clear();
        return false;
    }

    uint8_t expectedCrc[4] = {0};
    CryptoPP::CRC32 crc;
    crc.Update(decryptedBytes.data() + 4u, decryptedBytes.size() - 4u);
    crc.Final(expectedCrc);
    if (std::memcmp(expectedCrc, decryptedBytes.data(), sizeof(expectedCrc)) != 0) {
        outPayloadBytes->clear();
        return false;
    }

    outPayloadBytes->assign(decryptedBytes.begin() + 10u, decryptedBytes.end());
    return true;
}

}  // namespace mxo::auth
