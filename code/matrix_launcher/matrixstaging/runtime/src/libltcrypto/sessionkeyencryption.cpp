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

bool EncryptAuthRequestBlobWithKeyMaterial(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& modulusBytes,
    const std::vector<uint8_t>& exponentBytes,
    std::vector<uint8_t>* outCiphertext) {
    using namespace internal;

    if (!outCiphertext || plaintext.empty()) {
        return false;
    }

    CryptoPP::RSA::PublicKey publicKey;
    if (!BuildPublicKeyFromBytes(modulusBytes, exponentBytes, &publicKey)) {
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

bool BuildAuthRequestPacket(
    const std::string& username,
    const AuthBlobLayout& blobLayout,
    const AuthRequestLayout& requestLayout,
    FrameMode frameMode,
    AuthRequestBuildResult* outResult) {
    using namespace internal;

    if (!outResult) {
        return false;
    }

    AuthRequestBuildResult result;
    result.includedUsernameNullTerminator = blobLayout.includeUsernameNullTerminator;
    result.usedFixedHeaderOverride = !requestLayout.fixedHeaderBytes.empty();

    if (!BuildAuthRequestBlobPlaintext(
            username,
            blobLayout,
            &result.blobPlaintextBytes,
            &result.twofishKeyBytes,
            &result.usernameLengthField)) {
        return false;
    }

    result.usedProvidedPublicKey =
        !requestLayout.rsaModulusBytes.empty() && !requestLayout.rsaExponentBytes.empty();
    const bool encrypted = result.usedProvidedPublicKey
        ? EncryptAuthRequestBlobWithKeyMaterial(
            result.blobPlaintextBytes,
            requestLayout.rsaModulusBytes,
            requestLayout.rsaExponentBytes,
            &result.blobCiphertextBytes)
        : EncryptAuthRequestBlob(result.blobPlaintextBytes, &result.blobCiphertextBytes);
    if (!encrypted) {
        return false;
    }

    if (requestLayout.fixedHeaderBytes.empty()) {
        if (!BuildDefaultAuthHeaderBytes(
                requestLayout,
                &result.authHeaderBytes,
                &result.keyConfigMd5Bytes,
                &result.uiConfigMd5Bytes)) {
            return false;
        }
    } else {
        if (requestLayout.fixedHeaderBytes.size() != 35u) {
            return false;
        }
        result.authHeaderBytes = requestLayout.fixedHeaderBytes;
        result.keyConfigMd5Bytes.assign(
            result.authHeaderBytes.begin() + 3u,
            result.authHeaderBytes.begin() + 19u);
        result.uiConfigMd5Bytes.assign(
            result.authHeaderBytes.begin() + 19u,
            result.authHeaderBytes.end());
    }

    if (result.blobCiphertextBytes.size() > 0xffffu) {
        return false;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1u + 4u + 35u + 2u + result.blobCiphertextBytes.size());
    payload.push_back(0x08u);
    AppendU32LE(&payload, requestLayout.publicKeyId);
    payload.insert(
        payload.end(),
        result.authHeaderBytes.begin(),
        result.authHeaderBytes.end());
    AppendU16LE(&payload, static_cast<uint16_t>(result.blobCiphertextBytes.size()));
    payload.insert(
        payload.end(),
        result.blobCiphertextBytes.begin(),
        result.blobCiphertextBytes.end());

    if (!BuildVariableLengthPacket(payload.data(), payload.size(), frameMode, &result.packet)) {
        return false;
    }

    *outResult = result;
    return true;
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

std::vector<uint8_t> BuildAuthRequestBlob(
    const char* username,
    uint32_t embeddedTime,
    uint32_t rsaMethod,
    uint16_t someShort) {
    AuthBlobLayout layout;
    layout.embeddedTime = embeddedTime;
    layout.rsaMethod = rsaMethod;
    layout.someShort = someShort;

    std::vector<uint8_t> plaintext;
    std::vector<uint8_t> twofishKey;
    uint16_t usernameLengthField = 0;
    if (!BuildAuthRequestBlobPlaintext(
            username ? std::string(username) : std::string(),
            layout,
            &plaintext,
            &twofishKey,
            &usernameLengthField)) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> ciphertext;
    if (!EncryptAuthRequestBlob(plaintext, &ciphertext)) {
        return std::vector<uint8_t>();
    }
    return ciphertext;
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

std::string BuildAuthRequestBlobHex(
    const char* username,
    uint32_t rsaMethod,
    uint16_t someShort) {
    const std::vector<uint8_t> blob =
        BuildAuthRequestBlobEx(username, rsaMethod, someShort, NULL);
    return HexEncode(blob);
}

}  // namespace mxo::auth
