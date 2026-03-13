#include "auth_crypto.h"

#include <cstring>
#include <ctime>

#ifndef CRYPTOPP_ENABLE_NAMESPACE_WEAK
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#endif

#include "base64.h"
#include "filters.h"
#include "md5.h"
#include "modes.h"
#include "osrng.h"
#include "oaep.h"
#include "rsa.h"
#include "sha.h"
#include "twofish.h"

namespace mxo {
namespace auth {

namespace {

static const char kServerPublicModulusB64[] =
    "qMIfEkrXWpRr44ecWMzJHV7Hjg9bnru2PZv3NydzOZ6uab52wET+RoHhIzv+zJb3"
    "zBhmETAtsrmNnBXiW7tfqPK0xf6lb9RbvupfnfYSHO5WaEcWEi0JjQRBevg9d8ql"
    "ETo9Hrfy9PEfpeK1T2WF+xxx73chvBTB12Paa7yT+Ik=";
static const char kServerPublicExponentB64[] = "EQ==";

static std::vector<uint8_t> Base64Decode(const char* text) {
    std::string decoded;
    CryptoPP::StringSource source(
        text,
        true,
        new CryptoPP::Base64Decoder(
            new CryptoPP::StringSink(decoded)));
    return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

static bool BuildPublicKeyFromBytes(
    const std::vector<uint8_t>& modulusBytes,
    const std::vector<uint8_t>& exponentBytes,
    CryptoPP::RSA::PublicKey* outKey) {
    if (!outKey || modulusBytes.empty() || exponentBytes.empty()) {
        return false;
    }

    CryptoPP::Integer modulus(modulusBytes.data(), modulusBytes.size());
    CryptoPP::Integer exponent(exponentBytes.data(), exponentBytes.size());
    outKey->Initialize(modulus, exponent);
    return true;
}

static bool BuildServerPublicKey(CryptoPP::RSA::PublicKey* outKey) {
    const std::vector<uint8_t> modulusBytes = Base64Decode(kServerPublicModulusB64);
    const std::vector<uint8_t> exponentBytes = Base64Decode(kServerPublicExponentB64);
    return BuildPublicKeyFromBytes(modulusBytes, exponentBytes, outKey);
}

static void AppendU16LE(std::vector<uint8_t>* outBytes, uint16_t value) {
    outBytes->push_back(static_cast<uint8_t>(value & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
}

static void AppendU32LE(std::vector<uint8_t>* outBytes, uint32_t value) {
    outBytes->push_back(static_cast<uint8_t>(value & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

static bool NormalizeFixed16(
    const std::vector<uint8_t>& source,
    std::vector<uint8_t>* outBytes) {
    if (!outBytes) {
        return false;
    }
    if (!source.empty() && source.size() != 16u) {
        return false;
    }

    outBytes->assign(16u, 0u);
    if (!source.empty()) {
        std::memcpy(outBytes->data(), source.data(), 16u);
    }
    return true;
}

static bool BuildDefaultAuthHeaderBytes(
    const AuthRequestLayout& requestLayout,
    std::vector<uint8_t>* outHeaderBytes,
    std::vector<uint8_t>* outKeyConfigBytes,
    std::vector<uint8_t>* outUiConfigBytes) {
    if (!outHeaderBytes || !outKeyConfigBytes || !outUiConfigBytes) {
        return false;
    }

    if (!NormalizeFixed16(requestLayout.keyConfigMd5, outKeyConfigBytes) ||
        !NormalizeFixed16(requestLayout.uiConfigMd5, outUiConfigBytes)) {
        return false;
    }

    outHeaderBytes->clear();
    outHeaderBytes->reserve(35u);
    outHeaderBytes->push_back(requestLayout.loginType);
    AppendU16LE(outHeaderBytes, requestLayout.reservedWord);
    outHeaderBytes->insert(
        outHeaderBytes->end(),
        outKeyConfigBytes->begin(),
        outKeyConfigBytes->end());
    outHeaderBytes->insert(
        outHeaderBytes->end(),
        outUiConfigBytes->begin(),
        outUiConfigBytes->end());
    return outHeaderBytes->size() == 35u;
}

static uint32_t CurrentUnixTimeU32() {
    return static_cast<uint32_t>(std::time(NULL));
}

static bool TwofishCbcProcessNoPadding(
    const std::vector<uint8_t>& inputBytes,
    const std::vector<uint8_t>& keyBytes,
    bool encrypt,
    std::vector<uint8_t>* outBytes) {
    if (!outBytes || keyBytes.size() != 16u || (inputBytes.size() % 16u) != 0u) {
        return false;
    }

    const uint8_t zeroIv[16] = {0};
    outBytes->assign(inputBytes.size(), 0u);
    if (inputBytes.empty()) {
        return true;
    }

    try {
        if (encrypt) {
            CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption cipher;
            cipher.SetKeyWithIV(keyBytes.data(), keyBytes.size(), zeroIv);
            cipher.ProcessData(outBytes->data(), inputBytes.data(), inputBytes.size());
        } else {
            CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption cipher;
            cipher.SetKeyWithIV(keyBytes.data(), keyBytes.size(), zeroIv);
            cipher.ProcessData(outBytes->data(), inputBytes.data(), inputBytes.size());
        }
        return true;
    } catch (const CryptoPP::Exception&) {
        outBytes->clear();
        return false;
    }
}

static bool Md5DigestBytes(
    const std::vector<uint8_t>& inputBytes,
    std::vector<uint8_t>* outDigestBytes) {
    if (!outDigestBytes) {
        return false;
    }

    outDigestBytes->assign(16u, 0u);
    CryptoPP::Weak::MD5 md5;
    if (!inputBytes.empty()) {
        md5.Update(inputBytes.data(), inputBytes.size());
    }
    md5.Final(outDigestBytes->data());
    return true;
}

static bool TwofishCbcProcessWithIvNoPadding(
    const std::vector<uint8_t>& inputBytes,
    const std::vector<uint8_t>& keyBytes,
    const std::vector<uint8_t>& ivBytes,
    bool encrypt,
    std::vector<uint8_t>* outBytes) {
    if (!outBytes || keyBytes.size() != 16u || ivBytes.size() != 16u ||
        (inputBytes.size() % 16u) != 0u) {
        return false;
    }

    outBytes->assign(inputBytes.size(), 0u);
    if (inputBytes.empty()) {
        return true;
    }

    try {
        if (encrypt) {
            CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption cipher;
            cipher.SetKeyWithIV(keyBytes.data(), keyBytes.size(), ivBytes.data());
            cipher.ProcessData(outBytes->data(), inputBytes.data(), inputBytes.size());
        } else {
            CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption cipher;
            cipher.SetKeyWithIV(keyBytes.data(), keyBytes.size(), ivBytes.data());
            cipher.ProcessData(outBytes->data(), inputBytes.data(), inputBytes.size());
        }
        return true;
    } catch (const CryptoPP::Exception&) {
        outBytes->clear();
        return false;
    }
}

static uint16_t ReadU16LE(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(bytes[1] << 8u);
}

static uint32_t ReadU32LE(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) |
           (static_cast<uint32_t>(bytes[3]) << 24u);
}

static uint64_t ReadU64LE(const uint8_t* bytes) {
    return static_cast<uint64_t>(ReadU32LE(bytes)) |
           (static_cast<uint64_t>(ReadU32LE(bytes + 4u)) << 32u);
}

static std::string TrimFixedCString(const uint8_t* bytes, size_t size) {
    size_t length = 0u;
    while (length < size && bytes[length] != 0u) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

static bool ParseMxoStringAtOffset(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    size_t offset,
    MxoString* outString) {
    if (!payloadBytes || !outString || offset + 2u > payloadSize) {
        return false;
    }

    MxoString value;
    value.length = ReadU16LE(payloadBytes + offset);
    const size_t stringStart = offset + 2u;
    const size_t stringEnd = stringStart + value.length;
    if (stringEnd > payloadSize) {
        return false;
    }

    value.rawBytes.assign(payloadBytes + offset, payloadBytes + stringEnd);
    size_t textSize = value.length;
    if (textSize != 0u && payloadBytes[stringStart + textSize - 1u] == 0u) {
        --textSize;
    }
    value.text.assign(
        reinterpret_cast<const char*>(payloadBytes + stringStart),
        textSize);
    *outString = value;
    return true;
}

}  // namespace

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

bool BuildVariableLengthPacket(
    const uint8_t* payload,
    size_t payloadSize,
    FrameMode mode,
    FramedPacket* outPacket) {
    if (!payload || !outPacket) {
        return false;
    }

    outPacket->headerBytes.clear();
    outPacket->payloadBytes.assign(payload, payload + payloadSize);
    outPacket->bytes.clear();

    if (mode == kFrameModeForceOneByte) {
        if (payloadSize > 0x7fu) {
            return false;
        }
        outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
    } else {
        if (payloadSize > 0x7fffu) {
            return false;
        }
        if (mode == kFrameModeForceTwoByte || payloadSize >= 0x80u) {
            outPacket->headerBytes.push_back(
                static_cast<uint8_t>(0x80u | ((payloadSize >> 8u) & 0x7fu)));
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize & 0xffu));
        } else {
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
        }
    }

    outPacket->bytes.reserve(outPacket->headerBytes.size() + outPacket->payloadBytes.size());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->headerBytes.begin(),
        outPacket->headerBytes.end());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->payloadBytes.begin(),
        outPacket->payloadBytes.end());
    return true;
}

bool ParseVariableLengthPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    FramedPacket* outPacket) {
    if (!packetBytes || !outPacket || packetSize < 2u) {
        return false;
    }

    size_t headerSize = 1u;
    size_t payloadSize = 0u;
    if (packetBytes[0] & 0x80u) {
        if (packetSize < 3u) {
            return false;
        }
        headerSize = 2u;
        payloadSize =
            (static_cast<size_t>(packetBytes[0] & 0x7fu) << 8u) |
            static_cast<size_t>(packetBytes[1]);
    } else {
        payloadSize = static_cast<size_t>(packetBytes[0]);
    }

    if (headerSize + payloadSize > packetSize) {
        return false;
    }

    outPacket->headerBytes.assign(packetBytes, packetBytes + headerSize);
    outPacket->payloadBytes.assign(
        packetBytes + headerSize,
        packetBytes + headerSize + payloadSize);
    outPacket->bytes.assign(
        packetBytes,
        packetBytes + headerSize + payloadSize);
    return true;
}

bool BuildGetPublicKeyRequestPacket(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    FrameMode frameMode,
    FramedPacket* outPacket) {
    std::vector<uint8_t> payload;
    payload.reserve(9u);
    payload.push_back(0x06u);
    AppendU32LE(&payload, launcherVersion);
    AppendU32LE(&payload, currentPublicKeyId);
    return BuildVariableLengthPacket(payload.data(), payload.size(), frameMode, outPacket);
}

bool ParseGetPublicKeyReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    GetPublicKeyReply* outReply) {
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

bool BuildAuthRequestBlobPlaintext(
    const std::string& username,
    const AuthBlobLayout& layout,
    std::vector<uint8_t>* outPlaintext,
    std::vector<uint8_t>* outTwofishKey,
    uint16_t* outUsernameLengthField) {
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

bool BuildAuthChallengeResponsePacket(
    const std::vector<uint8_t>& encryptedChallengeBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::string& password,
    const std::string& soePassword,
    const AuthChallengeResponseLayout& layout,
    FrameMode frameMode,
    AuthChallengeResponseBuildResult* outResult) {
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

bool ParseAuthReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthReply* outReply) {
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

bool DecryptAuthReplyPrivateExponent(
    const AuthReply& reply,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::vector<uint8_t>& challengeIvBytes,
    std::vector<uint8_t>* outPrivateExponentBytes) {
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

}  // namespace auth
}  // namespace mxo
