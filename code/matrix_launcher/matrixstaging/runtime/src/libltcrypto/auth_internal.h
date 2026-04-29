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
// Static-RE now identifies the launcher-side helper pair as real Crypto++ CBC wrappers:
// - small constructor path `0x41df60` -> encrypting `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption`
//   (Ghidra vftable anchor `CBC_Encryption_0x4b00b0`)
// - large constructor path `0x446d90` -> decrypting `CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption`
//   (Ghidra vftable anchor `CBC_Decryption_0x4b7500`)
//
// Source keeps the recovered launcher-facing adapter method names/boundaries, but the adapter
// internals now hold and configure the real Crypto++ CBC objects instead of a source-owned CBC
// reimplementation.
inline const std::array<uint8_t, 16>& FeedbackSizeTransformAdapterZeroIv() {
    // anchors: launcher.exe:DAT_004d4d50 / DAT_004f7c2c / DAT_004f7f88
    static const std::array<uint8_t, 16> kZeroIv = {};
    return kZeroIv;
}

template <typename TransformT>
class FeedbackSizeTransformAdapterBase {
public:
    FeedbackSizeTransformAdapterBase() = default;

    FeedbackSizeTransformAdapterBase(const FeedbackSizeTransformAdapterBase& other) {
        CopyFrom(other);
    }

    FeedbackSizeTransformAdapterBase& operator=(
        const FeedbackSizeTransformAdapterBase& other) {
        if (this != &other) {
            Reset();
            CopyFrom(other);
        }
        return *this;
    }

    FeedbackSizeTransformAdapterBase(FeedbackSizeTransformAdapterBase&& other) noexcept = default;
    FeedbackSizeTransformAdapterBase& operator=(
        FeedbackSizeTransformAdapterBase&& other) noexcept = default;

    ~FeedbackSizeTransformAdapterBase() {
        Reset();
    }

    uint32_t QueryBlockByteCount10() const {
        // anchors: launcher.exe:0x420320 / 0x41d6e0
        return 16u;
    }

    bool FeedbackSizeTransformAdapter_TransformBuffer(
        void* destinationBytes,
        const void* sourceBytes,
        uint32_t byteCount) {
        // anchor: launcher.exe:0x44b570
        if (!configured58_ || !destinationBytes || !sourceBytes || byteCount == 0u ||
            (byteCount % QueryBlockByteCount10()) != 0u) {
            return false;
        }

        try {
            // Reinitialize per call so the CBC chain always starts from the configured IV, which
            // matches the prior source behavior and the launcher call pattern that treats the
            // adapter as a reusable configured object rather than a continuously-advanced stream.
            transform58_ = std::make_unique<TransformT>();
            transform58_->SetKeyWithIV(
                keyBytes38_.data(),
                keyBytes38_.size(),
                ivBytes48_.data());

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

            transform58_->ProcessData(
                static_cast<CryptoPP::byte*>(destinationBytes),
                reinterpret_cast<const CryptoPP::byte*>(transformInput),
                byteCount);
            return true;
        } catch (const CryptoPP::Exception&) {
            return false;
        }
    }

protected:
    bool ConstructCommon(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes) {
        Reset();
        if (!sourceBytes || !ivBytes || sourceByteCount < 16u) {
            return false;
        }

        std::memcpy(keyBytes38_.data(), sourceBytes, keyBytes38_.size());
        std::memcpy(ivBytes48_.data(), ivBytes, ivBytes48_.size());

        try {
            transform58_ = std::make_unique<TransformT>();
            transform58_->SetKeyWithIV(
                keyBytes38_.data(),
                keyBytes38_.size(),
                ivBytes48_.data());
            configured58_ = true;
            rounds5a_ = 16u; // recovered `Rounds` query default consumed by the helper family
            return true;
        } catch (const CryptoPP::Exception&) {
            Reset();
            return false;
        }
    }

private:
    void Reset() {
        // anchor: launcher.exe:0x41e010 / 0x41e4b0
        transform58_.reset();
        keyBytes38_.fill(0u);
        ivBytes48_.fill(0u);
        configured58_ = false;
        rounds5a_ = 0u;
    }

    void CopyFrom(const FeedbackSizeTransformAdapterBase& other) {
        if (!other.configured58_) {
            return;
        }
        (void)ConstructCommon(
            other.keyBytes38_.data(),
            static_cast<uint32_t>(other.keyBytes38_.size()),
            other.ivBytes48_.data());
        rounds5a_ = other.rounds5a_;
    }

    std::array<uint8_t, 16> keyBytes38_{};  // launcher inner `AssemblyTwofish` seed bytes
    std::array<uint8_t, 16> ivBytes48_{};   // launcher named parameter `IV`
    std::unique_ptr<TransformT> transform58_;
    bool configured58_ = false;
    uint8_t rounds5a_ = 0u;
};

class FeedbackSizeTransformAdapterSmall final
    : public FeedbackSizeTransformAdapterBase<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption> {
public:
    bool FeedbackSizeTransformAdapter_ConstructSmall(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes,
        uint32_t /*unusedZero*/) {
        // anchor: launcher.exe:0x41df60
        return ConstructCommon(sourceBytes, sourceByteCount, ivBytes);
    }
};

class FeedbackSizeTransformAdapterLarge final
    : public FeedbackSizeTransformAdapterBase<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption> {
public:
    bool FeedbackSizeTransformAdapter_ConstructLarge(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes,
        uint32_t /*unusedZero*/) {
        // anchor: launcher.exe:0x446d90
        return ConstructCommon(sourceBytes, sourceByteCount, ivBytes);
    }
};

}  // namespace mxo::auth::internal
