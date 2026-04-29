#pragma once

#include "auth_crypto.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#ifndef CRYPTOPP_ENABLE_NAMESPACE_WEAK
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#endif

#include "base64.h"
#include "crc.h"
#include "filters.h"
#include "md5.h"
#include "modes.h"
#include "osrng.h"
#include "oaep.h"
#include "rsa.h"
#include "sha.h"
#include "twofish.h"

namespace mxo::ltlogin {
extern const char* g_ServerPublicModulusB64;
extern const char* g_ServerPublicExponentB64;
} // namespace mxo::ltlogin

namespace mxo::auth::internal {

// Internal helper note:
// - these helpers are transitional glue extracted from the earlier monolithic auth helper
// - exact original one-to-one function ownership is still incomplete
// - keep concrete caller/source anchors in the public .cpp files whenever known

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

// Shared `FeedbackSize` / `AssemblyTwofish` helper family
// --------------------------------------------------------
// Static-RE now identifies the launcher-side helper pair as direct Crypto++ CBC objects:
// - small constructor path `0x41df60` -> `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption`
// - large constructor path `0x446d90` -> `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption`
// - shared transform row `0x44b570` -> direct `ProcessData(...)` over a configured CBC object
//
// Source therefore uses the Crypto++ classes directly and preserves the launcher method boundaries
// with helper functions instead of source-owned adapter class shells.
inline const std::array<uint8_t, 16>& FeedbackSizeTransformAdapterZeroIv() {
    // anchors: launcher.exe:DAT_004d4d50 / DAT_004f7c2c / DAT_004f7f88
    static const std::array<uint8_t, 16> kZeroIv = {};
    return kZeroIv;
}

template <typename TransformT>
inline bool ConfigureFeedbackSizeTransform(
    TransformT* transform,
    const void* sourceBytes,
    uint32_t sourceByteCount,
    const void* ivBytes) {
    if (!transform || !sourceBytes || !ivBytes || sourceByteCount < 16u) {
        return false;
    }

    try {
        transform->SetKeyWithIV(
            static_cast<const CryptoPP::byte*>(sourceBytes),
            sourceByteCount,
            static_cast<const CryptoPP::byte*>(ivBytes));
        return true;
    } catch (const CryptoPP::Exception&) {
        return false;
    }
}

template <typename TransformT>
inline bool ProcessFeedbackSizeTransform(
    TransformT* transform,
    void* destinationBytes,
    const void* sourceBytes,
    uint32_t byteCount) {
    // anchor: launcher.exe:0x44b570
    if (!transform || !destinationBytes || !sourceBytes || byteCount == 0u ||
        (byteCount % 16u) != 0u) {
        return false;
    }

    try {
        const uint8_t* transformInput = static_cast<const uint8_t*>(sourceBytes);
        std::vector<uint8_t> overlapScratch;
        const uint8_t* destinationBegin = static_cast<const uint8_t*>(destinationBytes);
        const uint8_t* destinationEnd = destinationBegin + byteCount;
        const bool overlaps =
            (transformInput < destinationEnd) &&
            (destinationBegin < (transformInput + byteCount));
        if (overlaps) {
            overlapScratch.assign(transformInput, transformInput + byteCount);
            transformInput = overlapScratch.data();
        }

        transform->ProcessData(
            static_cast<CryptoPP::byte*>(destinationBytes),
            reinterpret_cast<const CryptoPP::byte*>(transformInput),
            byteCount);
        return true;
    } catch (const CryptoPP::Exception&) {
        return false;
    }
}

}  // namespace mxo::auth::internal
