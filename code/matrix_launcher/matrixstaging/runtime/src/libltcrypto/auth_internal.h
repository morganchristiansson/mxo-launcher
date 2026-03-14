#pragma once

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

namespace mxo::auth::internal {

// Internal helper note:
// - these helpers are transitional glue extracted from the earlier monolithic auth helper
// - exact original one-to-one function ownership is still incomplete
// - keep concrete caller/source anchors in the public .cpp files whenever known

static const char kServerPublicModulusB64[] =
    "qMIfEkrXWpRr44ecWMzJHV7Hjg9bnru2PZv3NydzOZ6uab52wET+RoHhIzv+zJb3"
    "zBhmETAtsrmNnBXiW7tfqPK0xf6lb9RbvupfnfYSHO5WaEcWEi0JjQRBevg9d8ql"
    "ETo9Hrfy9PEfpeK1T2WF+xxx73chvBTB12Paa7yT+Ik=";
static const char kServerPublicExponentB64[] = "EQ==";

inline std::vector<uint8_t> Base64Decode(const char* text) {
    std::string decoded;
    CryptoPP::StringSource source(
        text,
        true,
        new CryptoPP::Base64Decoder(
            new CryptoPP::StringSink(decoded)));
    return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

inline bool BuildPublicKeyFromBytes(
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

inline bool BuildServerPublicKey(CryptoPP::RSA::PublicKey* outKey) {
    const std::vector<uint8_t> modulusBytes = Base64Decode(kServerPublicModulusB64);
    const std::vector<uint8_t> exponentBytes = Base64Decode(kServerPublicExponentB64);
    return BuildPublicKeyFromBytes(modulusBytes, exponentBytes, outKey);
}

inline void AppendU16LE(std::vector<uint8_t>* outBytes, uint16_t value) {
    outBytes->push_back(static_cast<uint8_t>(value & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
}

inline void AppendU32LE(std::vector<uint8_t>* outBytes, uint32_t value) {
    outBytes->push_back(static_cast<uint8_t>(value & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    outBytes->push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

inline bool NormalizeFixed16(
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

inline bool BuildDefaultAuthHeaderBytes(
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

inline uint32_t CurrentUnixTimeU32() {
    return static_cast<uint32_t>(std::time(NULL));
}

inline bool TwofishCbcProcessNoPadding(
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

inline bool Md5DigestBytes(
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

inline bool TwofishCbcProcessWithIvNoPadding(
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

inline uint16_t ReadU16LE(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(bytes[1] << 8u);
}

inline uint32_t ReadU32LE(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) |
           (static_cast<uint32_t>(bytes[3]) << 24u);
}

inline uint64_t ReadU64LE(const uint8_t* bytes) {
    return static_cast<uint64_t>(ReadU32LE(bytes)) |
           (static_cast<uint64_t>(ReadU32LE(bytes + 4u)) << 32u);
}

inline std::string TrimFixedCString(const uint8_t* bytes, size_t size) {
    size_t length = 0u;
    while (length < size && bytes[length] != 0u) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

inline bool ParseMxoStringAtOffset(
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

}  // namespace mxo::auth::internal
