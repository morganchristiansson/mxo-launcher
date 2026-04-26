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
namespace {

// anchor: launcher.exe:0x43d800 = GenerateClientChunkHashes
static bool GenerateClientChunkHashesScaffold(
    const MarginConnectChallenge& challenge,
    std::vector<std::array<uint8_t, 16>>* outChunkDigests) {
    using namespace internal;

    if (!outChunkDigests || challenge.chunkByteCount == 0u) {
        return false;
    }

    char executablePath[MAX_PATH] = {};
    const DWORD executablePathLength = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (executablePathLength == 0u || executablePathLength >= MAX_PATH) {
        return false;
    }

    const char* const filePaths[] = {"client.dll", executablePath};
    std::vector<uint8_t> chunkBytes;
    chunkBytes.resize(static_cast<size_t>(challenge.chunkByteCount));

    outChunkDigests->clear();
    for (const char* filePath : filePaths) {
        FILE* const file = std::fopen(filePath, "rb");
        if (file == nullptr) {
            outChunkDigests->clear();
            return false;
        }

        while (true) {
            const size_t bytesRead = std::fread(chunkBytes.data(), 1u, chunkBytes.size(), file);
            if (bytesRead != 0u) {
                std::vector<uint8_t> digestBytes;
                if (!Md5DigestBytes(
                        std::vector<uint8_t>(chunkBytes.begin(), chunkBytes.begin() + bytesRead),
                        &digestBytes) ||
                    digestBytes.size() != 16u) {
                    std::fclose(file);
                    outChunkDigests->clear();
                    return false;
                }

                std::array<uint8_t, 16> digestArray = {};
                std::copy(digestBytes.begin(), digestBytes.end(), digestArray.begin());
                outChunkDigests->push_back(digestArray);
            }

            if (bytesRead < chunkBytes.size()) {
                const bool readFailed = std::ferror(file) != 0;
                std::fclose(file);
                if (readFailed) {
                    outChunkDigests->clear();
                    return false;
                }
                break;
            }
        }
    }

    return true;
}

// anchor: launcher.exe:0x4566a0 folds the server seed and every 16-byte chunk digest through the
// same MD5 family used by 0x43d800.
static bool BuildClientChunkHashResponseDigestScaffold(
    const MarginConnectChallenge& challenge,
    const std::vector<std::array<uint8_t, 16>>& chunkDigests,
    std::array<uint8_t, 16>* outDigest) {
    using namespace internal;

    if (!outDigest) {
        return false;
    }

    std::vector<uint8_t> foldedBytes;
    foldedBytes.reserve(16u + chunkDigests.size() * 16u);
    foldedBytes.insert(
        foldedBytes.end(),
        challenge.seedBytes.begin(),
        challenge.seedBytes.end());
    for (const auto& digest : chunkDigests) {
        foldedBytes.insert(foldedBytes.end(), digest.begin(), digest.end());
    }

    std::vector<uint8_t> digestBytes;
    if (!Md5DigestBytes(foldedBytes, &digestBytes) || digestBytes.size() != 16u) {
        return false;
    }

    std::copy(digestBytes.begin(), digestBytes.end(), outDigest->begin());
    return true;
}

}  // namespace

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

bool EncryptAuthRequestBlob(
    const std::vector<uint8_t>& plaintext,
    std::vector<uint8_t>* outCiphertext) {
    using namespace internal;

    if (!outCiphertext || plaintext.empty()) {
        return false;
    }

    CryptoPP::RSA::PublicKey publicKey;
    if (!BuildServerPublicKey(&publicKey)) {
        return false;
    }

    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);
        std::string ciphertext;
        CryptoPP::StringSource source(
            plaintext.data(),
            plaintext.size(),
            true,
            new CryptoPP::PK_EncryptorFilter(
                rng,
                encryptor,
                new CryptoPP::StringSink(ciphertext)));
        outCiphertext->assign(ciphertext.begin(), ciphertext.end());
        return !outCiphertext->empty();
    } catch (const CryptoPP::Exception&) {
        outCiphertext->clear();
        return false;
    }
}

bool BuildAuthChallengeResponsePacket(
    const std::vector<uint8_t>& encryptedChallengeBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::string& password,
    const std::string& soePassword,
    const AuthChallengeResponseLayout& layout,
    FrameMode frameMode,
    AuthChallengeResponseBuildResult* outResult) {
    using namespace internal;

    if (!outResult || encryptedChallengeBytes.size() != 16u || twofishKeyBytes.size() != 16u) {
        return false;
    }

    AuthChallengeResponseBuildResult result;
    result.encryptedChallengeBytes = encryptedChallengeBytes;

    if (!TwofishCbcProcessNoPadding(
            encryptedChallengeBytes,
            twofishKeyBytes,
            false,
            &result.decryptedChallengeBytes)) {
        return false;
    }
    if (!Md5DigestBytes(result.decryptedChallengeBytes, &result.processedChallengeMd5Bytes)) {
        return false;
    }

    std::vector<uint8_t> passwordBytes(password.begin(), password.end());
    if (layout.includePasswordNullTerminator) {
        passwordBytes.push_back(0u);
    }
    std::vector<uint8_t> soePasswordBytes(soePassword.begin(), soePassword.end());
    if (layout.includeSoePasswordNullTerminator) {
        soePasswordBytes.push_back(0u);
    }
    if (passwordBytes.size() > 0xffffu || soePasswordBytes.size() > 0xffffu) {
        return false;
    }

    result.passwordLengthField = static_cast<uint16_t>(passwordBytes.size());
    result.soePasswordLengthField = static_cast<uint16_t>(soePasswordBytes.size());
    const uint16_t unknown2 = layout.usePasswordLengthForUnknown2
        ? result.passwordLengthField
        : layout.unknown2;
    const uint16_t unknown3 = layout.useSoePasswordLengthForUnknown3
        ? result.soePasswordLengthField
        : layout.unknown3;

    const size_t plaintextSizeWithoutPadding =
        1u +
        result.processedChallengeMd5Bytes.size() +
        2u + 2u + 2u +
        2u + passwordBytes.size() +
        2u + soePasswordBytes.size() +
        2u;
    result.paddingLengthField = static_cast<uint16_t>(
        (16u - (plaintextSizeWithoutPadding % 16u)) % 16u);

    result.plaintextBytes.clear();
    result.plaintextBytes.reserve(plaintextSizeWithoutPadding + result.paddingLengthField);
    result.plaintextBytes.push_back(layout.plaintextLeadingByte);
    result.plaintextBytes.insert(
        result.plaintextBytes.end(),
        result.processedChallengeMd5Bytes.begin(),
        result.processedChallengeMd5Bytes.end());
    AppendU16LE(&result.plaintextBytes, layout.unknown1);
    AppendU16LE(&result.plaintextBytes, unknown2);
    AppendU16LE(&result.plaintextBytes, unknown3);
    AppendU16LE(&result.plaintextBytes, result.passwordLengthField);
    result.plaintextBytes.insert(
        result.plaintextBytes.end(),
        passwordBytes.begin(),
        passwordBytes.end());
    AppendU16LE(&result.plaintextBytes, result.soePasswordLengthField);
    result.plaintextBytes.insert(
        result.plaintextBytes.end(),
        soePasswordBytes.begin(),
        soePasswordBytes.end());
    AppendU16LE(&result.plaintextBytes, result.paddingLengthField);
    result.plaintextBytes.insert(
        result.plaintextBytes.end(),
        result.paddingLengthField,
        layout.paddingByte);

    if (!TwofishCbcProcessNoPadding(
            result.plaintextBytes,
            twofishKeyBytes,
            true,
            &result.ciphertextBytes)) {
        return false;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1u + 2u + 2u + result.ciphertextBytes.size());
    payload.push_back(0x0au);
    AppendU16LE(&payload, layout.packetSomeShort);
    AppendU16LE(&payload, static_cast<uint16_t>(result.ciphertextBytes.size()));
    payload.insert(
        payload.end(),
        result.ciphertextBytes.begin(),
        result.ciphertextBytes.end());

    if (!BuildVariableLengthPacket(payload.data(), payload.size(), frameMode, &result.packet)) {
        return false;
    }

    *outResult = result;
    return true;
}

bool DecryptAuthReplyPrivateExponent(
    const AuthReply& reply,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::vector<uint8_t>& challengeIvBytes,
    std::vector<uint8_t>* outPrivateExponentBytes) {
    using namespace internal;

    if (!outPrivateExponentBytes || reply.encryptedPrivateExponentBytes.empty()) {
        return false;
    }
    return TwofishCbcProcessWithIvNoPadding(
        reply.encryptedPrivateExponentBytes,
        twofishKeyBytes,
        challengeIvBytes,
        false,
        outPrivateExponentBytes);
}

// Transitional low-level margin CERT/MS helper note:
// - wire/crypto shape here is intentionally shared runtime code
// - evidence source is open server/proxy code, especially:
//   - `../../../work/mxoemu/Reality/Source/MarginSocket.cpp`
//   - `../../../work/mxoemu/Proxy/Logging.cpp`
//   - `../../../work/mxoemu/Proxy/EncryptedPacket.cpp`
// - this is not a claim that final launcher-owned state progression should remain here

bool ParseMarginCertChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    const AuthSignedData& signedData,
    const std::vector<uint8_t>& privateExponentBytes,
    MarginCertChallenge* outChallenge) {
    using namespace internal;

    if (!payloadBytes || !outChallenge || payloadSize < 5u || payloadBytes[0] != 0x02u ||
        !signedData.valid || signedData.modulusBytes.empty() || privateExponentBytes.empty()) {
        return false;
    }

    const uint16_t firstNumber = ReadU16LE(payloadBytes + 1u);
    const uint16_t blobSize = ReadU16LE(payloadBytes + 3u);
    if (firstNumber != 3u || payloadSize != 5u + blobSize) {
        return false;
    }

    std::vector<uint8_t> exponentBytes;
    if ((signedData.publicExponent >> 8u) != 0u) {
        exponentBytes.push_back(static_cast<uint8_t>((signedData.publicExponent >> 8u) & 0xffu));
    }
    exponentBytes.push_back(static_cast<uint8_t>(signedData.publicExponent & 0xffu));

    CryptoPP::RSA::PrivateKey privateKey;
    if (!BuildPrivateKeyFromBytes(
            signedData.modulusBytes,
            exponentBytes,
            privateExponentBytes,
            &privateKey)) {
        return false;
    }

    std::vector<uint8_t> decryptedBytes;
    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Decryptor decryptor(privateKey);
        std::string plaintext;
        CryptoPP::StringSource source(
            payloadBytes + 5u,
            blobSize,
            true,
            new CryptoPP::PK_DecryptorFilter(
                rng,
                decryptor,
                new CryptoPP::StringSink(plaintext)));
        decryptedBytes.assign(plaintext.begin(), plaintext.end());
    } catch (const CryptoPP::Exception&) {
        return false;
    }

    if (decryptedBytes.size() != 33u || decryptedBytes[0] != 0u) {
        return false;
    }

    MarginCertChallenge challenge;
    challenge.valid = true;
    challenge.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);
    challenge.bytes = challenge.payloadBytes;
    challenge.encryptedBlobBytes.assign(payloadBytes + 5u, payloadBytes + 5u + blobSize);
    challenge.twofishKeyBytes.assign(decryptedBytes.begin() + 1u, decryptedBytes.begin() + 17u);
    challenge.challengeBytes.assign(decryptedBytes.begin() + 17u, decryptedBytes.begin() + 33u);
    *outChallenge = challenge;
    return true;
}

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

bool BuildMarginCertChallengeResponsePacket(
    const std::vector<uint8_t>& challengeBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    if (challengeBytes.size() != 16u) {
        return false;
    }

    std::vector<uint8_t> payload;
    payload.reserve(17u);
    payload.push_back(0x03u);
    payload.insert(payload.end(), challengeBytes.begin(), challengeBytes.end());
    return EncryptMarginPayloadPacket(payload.data(), payload.size(), twofishKeyBytes, frameMode, outPacket);
}

bool BuildMarginMsConnectRequestPacket(
    uint32_t matrixVersion,
    uint32_t clientDllVersion,
    const std::array<uint8_t, 16>& weirdSequenceBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    using namespace internal;

    std::vector<uint8_t> payload;
    payload.reserve(1u + 4u + 4u + 9u + weirdSequenceBytes.size() + 1u);
    payload.push_back(0x06u);
    AppendU32LE(&payload, matrixVersion);
    AppendU32LE(&payload, clientDllVersion);
    payload.insert(payload.end(), 9u, 0u);
    payload.insert(payload.end(), weirdSequenceBytes.begin(), weirdSequenceBytes.end());
    payload.push_back(0u);
    return EncryptMarginPayloadPacket(payload.data(), payload.size(), twofishKeyBytes, frameMode, outPacket);
}

bool BuildMarginConnectRequestPacket(
    uint32_t matrixVersion,
    uint32_t clientDllVersion,
    const std::array<uint8_t, 16>& weirdSequenceBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    return BuildMarginMsConnectRequestPacket(
        matrixVersion,
        clientDllVersion,
        weirdSequenceBytes,
        twofishKeyBytes,
        frameMode,
        outPacket);
}

bool ParseMarginMsConnectChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    MarginConnectChallenge* outChallenge) {
    using namespace internal;

    if (!payloadBytes || !outChallenge || payloadSize < 21u || payloadBytes[0] != 0x07u) {
        return false;
    }

    MarginConnectChallenge challenge;
    challenge.valid = true;
    challenge.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);
    std::copy_n(payloadBytes + 1u, challenge.seedBytes.size(), challenge.seedBytes.begin());
    challenge.chunkByteCount = ReadU32LE(payloadBytes + 17u);
    if (challenge.chunkByteCount == 0u) {
        return false;
    }

    *outChallenge = challenge;
    return true;
}

bool ParseMarginConnectChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    MarginConnectChallenge* outChallenge) {
    return ParseMarginMsConnectChallengePayload(payloadBytes, payloadSize, outChallenge);
}

bool BuildMarginMsConnectChallengeResponsePacket(
    const MarginConnectChallenge& challenge,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    std::vector<std::array<uint8_t, 16>> chunkDigests;
    if (!challenge.valid ||
        !GenerateClientChunkHashesScaffold(challenge, &chunkDigests)) {
        return false;
    }

    std::array<uint8_t, 16> responseDigest = {};
    if (!BuildClientChunkHashResponseDigestScaffold(
            challenge,
            chunkDigests,
            &responseDigest)) {
        return false;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1u + responseDigest.size());
    payload.push_back(0x08u);
    payload.insert(payload.end(), responseDigest.begin(), responseDigest.end());
    return EncryptMarginPayloadPacket(payload.data(), payload.size(), twofishKeyBytes, frameMode, outPacket);
}

bool BuildMarginConnectChallengeResponsePacket(
    const MarginConnectChallenge& challenge,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    return BuildMarginMsConnectChallengeResponsePacket(
        challenge,
        twofishKeyBytes,
        frameMode,
        outPacket);
}

bool ParseMarginMsConnectReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    MarginConnectReply* outReply) {
    using namespace internal;

    // Older emulator/proxy evidence shows a 23-byte `MS_ConnectReply` core body.
    // Current live deliberate runs now show at least one server replying with a longer decrypted
    // raw `0x09` payload while still preserving that same leading field family.
    // Treat the first 23 bytes as the stable prefix and preserve the full payload on the reply.
    if (!payloadBytes || !outReply || payloadSize < 23u || payloadBytes[0] != 0x09u) {
        return false;
    }

    MarginConnectReply reply;
    reply.valid = true;
    reply.payloadBytes.assign(payloadBytes, payloadBytes + payloadSize);
    reply.bytes = reply.payloadBytes;
    reply.status0 = ReadU32LE(payloadBytes + 1u);
    reply.status1 = ReadU32LE(payloadBytes + 5u);
    reply.sessionId = ReadU32LE(payloadBytes + 9u);
    reply.field0d = ReadU16LE(payloadBytes + 13u);
    reply.field0f = ReadU16LE(payloadBytes + 15u);
    reply.field11 = ReadU16LE(payloadBytes + 17u);
    reply.field13 = ReadU16LE(payloadBytes + 19u);
    reply.field15 = ReadU16LE(payloadBytes + 21u);
    *outReply = reply;
    return true;
}

bool ParseMarginConnectReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    MarginConnectReply* outReply) {
    return ParseMarginMsConnectReplyPayload(payloadBytes, payloadSize, outReply);
}

std::vector<uint8_t> BuildAuthRequestBlobEx(
    const char* username,
    uint32_t rsaMethod,
    uint16_t someShort,
    const uint8_t* twofishKey) {
    using namespace internal;

    AuthBlobLayout layout;
    layout.embeddedTime = CurrentUnixTimeU32();
    layout.rsaMethod = rsaMethod;
    layout.someShort = someShort;
    if (twofishKey) {
        layout.twofishKey.assign(twofishKey, twofishKey + 16u);
    }

    std::vector<uint8_t> plaintext;
    std::vector<uint8_t> effectiveTwofishKey;
    uint16_t usernameLengthField = 0;
    if (!BuildAuthRequestBlobPlaintext(
            username ? std::string(username) : std::string(),
            layout,
            &plaintext,
            &effectiveTwofishKey,
            &usernameLengthField)) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> ciphertext;
    if (!EncryptAuthRequestBlob(plaintext, &ciphertext)) {
        return std::vector<uint8_t>();
    }
    return ciphertext;
}

}  // namespace mxo::auth
