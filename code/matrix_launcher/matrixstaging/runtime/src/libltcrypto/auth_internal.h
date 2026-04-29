#pragma once

#include "auth_crypto.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>

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

// Shared `FeedbackSize` / `AssemblyTwofish` helper family
// --------------------------------------------------------
// Source-owned C++ mirror of the static-RE-closed launcher helper rooted at:
// - launcher.exe:0x41df60 = FeedbackSizeTransformAdapter_ConstructSmall
// - launcher.exe:0x446d90 = FeedbackSizeTransformAdapter_ConstructLarge
// - launcher.exe:0x44b570 = FeedbackSizeTransformAdapter_TransformBuffer
// - launcher.exe:0x41c750 = zero + tracked-free helper used by the adapter dtors
//
// Current strongest static read:
// - small constructor paths (`0x41df60`) are the encrypting `AssemblyTwofish` worker family used by
//   state9 callback blob filling, auth-bootstrap send-side field `+0x98`, and margin write-helper
//   setup
// - large constructor paths (`0x446d90`) are the decrypting sibling used by auth-bootstrap field
//   `+0x94` and margin read-helper worker setup
// - both variants configure a 16-byte `AssemblyTwofish` state around a 16-byte source block and a
//   caller-provided `IV` byte span; all currently recovered callers pass a zero IV block
// - `0x44b570` then walks the caller span in 16-byte chunks with 4-byte-alignment scratch handling
//
// The original helper alloc/free pair (`0x41d2e0` / `0x41c750`) dispatch into tracked fixed-size
// bins for `<0x11`, `<0x21`, `<0x281`, `<0x501`, and `<0x1001` before falling back to the general
// tracked heap. Source keeps the same thresholds/comments and exact zero-before-free behavior, but
// uses `malloc/free` as the backing storage until the allocator family itself is source-owned.
inline const std::array<uint8_t, 16>& FeedbackSizeTransformAdapterZeroIv() {
    // anchors: launcher.exe:DAT_004d4d50 / DAT_004f7c2c / DAT_004f7f88
    static const std::array<uint8_t, 16> kZeroIv = {};
    return kZeroIv;
}

inline uint8_t* FeedbackSizeTransformAdapter_TrackedAllocateBuffer(uint32_t byteCount) {
    // anchor: launcher.exe:0x41d2e0
    if (byteCount == 0u) {
        return nullptr;
    }
    return static_cast<uint8_t*>(std::malloc(byteCount));
}

inline void FeedbackSizeTransformAdapter_SecureFreeTrackedBuffer(void* storage, uint32_t byteCount) {
    // anchor: launcher.exe:0x41c750
    if (!storage || byteCount == 0u) {
        return;
    }

    std::memset(storage, 0, byteCount);
    std::free(storage);
}


class FeedbackSizeTransformAdapterCommon {
public:
    FeedbackSizeTransformAdapterCommon() = default;

    FeedbackSizeTransformAdapterCommon(const FeedbackSizeTransformAdapterCommon& other) {
        CopyFrom(other);
    }

    FeedbackSizeTransformAdapterCommon& operator=(
        const FeedbackSizeTransformAdapterCommon& other) {
        if (this != &other) {
            Reset();
            CopyFrom(other);
        }
        return *this;
    }

    FeedbackSizeTransformAdapterCommon(FeedbackSizeTransformAdapterCommon&& other) noexcept {
        MoveFrom(&other);
    }

    FeedbackSizeTransformAdapterCommon& operator=(
        FeedbackSizeTransformAdapterCommon&& other) noexcept {
        if (this != &other) {
            Reset();
            MoveFrom(&other);
        }
        return *this;
    }

    ~FeedbackSizeTransformAdapterCommon() {
        Reset();
    }

    uint32_t QueryBlockByteCount10() const {
        // anchors: launcher.exe:0x420320 / 0x41d6e0
        return 16u;
    }

    uint32_t QueryAlignmentQuantum() const {
        // anchors: launcher.exe:0x4686b0 / 0x44b570
        return 4u;
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

        if (workingBuffer14_ == nullptr || scratchBuffer20_ == nullptr ||
            (decryptLarge59_ && swapBuffer2c_ == nullptr)) {
            return false;
        }

        uint8_t* destinationCursor = static_cast<uint8_t*>(destinationBytes);
        const uint8_t* sourceCursor = static_cast<const uint8_t*>(sourceBytes);
        const uint32_t blockByteCount = QueryBlockByteCount10();
        const uintptr_t alignmentMask = QueryAlignmentQuantum() - 1u;

        try {
            if (!decryptLarge59_) {
                CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption cipher;
                cipher.SetKeyWithIV(keyBytes38_.data(), keyBytes38_.size(), ivBytes48_.data());
                for (uint32_t remaining = byteCount; remaining != 0u; remaining -= blockByteCount) {
                    const bool sourceAligned =
                        ((reinterpret_cast<uintptr_t>(sourceCursor) & alignmentMask) == 0u);
                    const bool destinationAligned =
                        ((reinterpret_cast<uintptr_t>(destinationCursor) & alignmentMask) == 0u);

                    const uint8_t* transformInput = sourceCursor;
                    if (!sourceAligned) {
                        std::memcpy(workingBuffer14_, sourceCursor, blockByteCount);
                        transformInput = workingBuffer14_;
                    }

                    uint8_t* transformOutput = destinationCursor;
                    if (!destinationAligned || destinationCursor == sourceCursor) {
                        transformOutput = scratchBuffer20_;
                    }

                    cipher.ProcessData(transformOutput, transformInput, blockByteCount);
                    if (transformOutput != destinationCursor) {
                        std::memcpy(destinationCursor, transformOutput, blockByteCount);
                    }

                    sourceCursor += blockByteCount;
                    destinationCursor += blockByteCount;
                }
                return true;
            }

            CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption cipher;
            cipher.SetKeyWithIV(keyBytes38_.data(), keyBytes38_.size(), ivBytes48_.data());
            for (uint32_t remaining = byteCount; remaining != 0u; remaining -= blockByteCount) {
                const bool sourceAligned =
                    ((reinterpret_cast<uintptr_t>(sourceCursor) & alignmentMask) == 0u);
                const bool destinationAligned =
                    ((reinterpret_cast<uintptr_t>(destinationCursor) & alignmentMask) == 0u);

                const uint8_t* transformInput = sourceCursor;
                if (!sourceAligned || destinationCursor == sourceCursor) {
                    std::memcpy(swapBuffer2c_, sourceCursor, blockByteCount);
                    transformInput = swapBuffer2c_;
                }

                uint8_t* transformOutput = destinationCursor;
                if (!destinationAligned || destinationCursor == sourceCursor) {
                    transformOutput = scratchBuffer20_;
                }

                cipher.ProcessData(transformOutput, transformInput, blockByteCount);
                if (transformOutput != destinationCursor) {
                    std::memcpy(destinationCursor, transformOutput, blockByteCount);
                }

                sourceCursor += blockByteCount;
                destinationCursor += blockByteCount;
            }
            return true;
        } catch (const CryptoPP::Exception&) {
            return false;
        }
    }

protected:
    bool ConstructCommon(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes,
        bool decryptLarge) {
        Reset();
        if (!sourceBytes || !ivBytes || sourceByteCount < 16u) {
            return false;
        }

        std::memcpy(keyBytes38_.data(), sourceBytes, keyBytes38_.size());
        std::memcpy(ivBytes48_.data(), ivBytes, ivBytes48_.size());

        configured58_ = true;
        decryptLarge59_ = decryptLarge;
        rounds5a_ = 16u; // recovered `Rounds` query default consumed by the helper family

        workingBufferByteCount10_ = QueryBlockByteCount10();
        workingBuffer14_ = FeedbackSizeTransformAdapter_TrackedAllocateBuffer(workingBufferByteCount10_);
        scratchByteCount1c_ = QueryBlockByteCount10();
        scratchBuffer20_ = FeedbackSizeTransformAdapter_TrackedAllocateBuffer(scratchByteCount1c_);
        if (decryptLarge59_) {
            swapByteCount28_ = QueryBlockByteCount10();
            swapBuffer2c_ = FeedbackSizeTransformAdapter_TrackedAllocateBuffer(swapByteCount28_);
        }

        if (workingBuffer14_ == nullptr || scratchBuffer20_ == nullptr ||
            (decryptLarge59_ && swapBuffer2c_ == nullptr)) {
            Reset();
            return false;
        }

        std::memset(workingBuffer14_, 0, workingBufferByteCount10_);
        std::memset(scratchBuffer20_, 0, scratchByteCount1c_);
        if (swapBuffer2c_ != nullptr) {
            std::memset(swapBuffer2c_, 0, swapByteCount28_);
        }
        return true;
    }

private:
    void Reset() {
        // anchor: launcher.exe:0x41e010 / 0x41e4b0
        FeedbackSizeTransformAdapter_SecureFreeTrackedBuffer(scratchBuffer20_, scratchByteCount1c_);
        FeedbackSizeTransformAdapter_SecureFreeTrackedBuffer(workingBuffer14_, workingBufferByteCount10_);
        FeedbackSizeTransformAdapter_SecureFreeTrackedBuffer(swapBuffer2c_, swapByteCount28_);

        workingBufferByteCount10_ = 0u;
        workingBuffer14_ = nullptr;
        scratchByteCount1c_ = 0u;
        scratchBuffer20_ = nullptr;
        swapByteCount28_ = 0u;
        swapBuffer2c_ = nullptr;
        keyBytes38_.fill(0u);
        ivBytes48_.fill(0u);
        configured58_ = false;
        decryptLarge59_ = false;
        rounds5a_ = 0u;
    }

    void CopyFrom(const FeedbackSizeTransformAdapterCommon& other) {
        if (!other.configured58_) {
            return;
        }
        if (!ConstructCommon(
                other.keyBytes38_.data(),
                static_cast<uint32_t>(other.keyBytes38_.size()),
                other.ivBytes48_.data(),
                other.decryptLarge59_)) {
            return;
        }

        if (workingBuffer14_ && other.workingBuffer14_) {
            std::memcpy(workingBuffer14_, other.workingBuffer14_, workingBufferByteCount10_);
        }
        if (scratchBuffer20_ && other.scratchBuffer20_) {
            std::memcpy(scratchBuffer20_, other.scratchBuffer20_, scratchByteCount1c_);
        }
        if (swapBuffer2c_ && other.swapBuffer2c_) {
            std::memcpy(swapBuffer2c_, other.swapBuffer2c_, swapByteCount28_);
        }
        rounds5a_ = other.rounds5a_;
    }

    void MoveFrom(FeedbackSizeTransformAdapterCommon* other) {
        workingBufferByteCount10_ = other->workingBufferByteCount10_;
        workingBuffer14_ = other->workingBuffer14_;
        scratchByteCount1c_ = other->scratchByteCount1c_;
        scratchBuffer20_ = other->scratchBuffer20_;
        swapByteCount28_ = other->swapByteCount28_;
        swapBuffer2c_ = other->swapBuffer2c_;
        keyBytes38_ = other->keyBytes38_;
        ivBytes48_ = other->ivBytes48_;
        configured58_ = other->configured58_;
        decryptLarge59_ = other->decryptLarge59_;
        rounds5a_ = other->rounds5a_;

        other->workingBufferByteCount10_ = 0u;
        other->workingBuffer14_ = nullptr;
        other->scratchByteCount1c_ = 0u;
        other->scratchBuffer20_ = nullptr;
        other->swapByteCount28_ = 0u;
        other->swapBuffer2c_ = nullptr;
        other->keyBytes38_.fill(0u);
        other->ivBytes48_.fill(0u);
        other->configured58_ = false;
        other->decryptLarge59_ = false;
        other->rounds5a_ = 0u;
    }

    uint32_t workingBufferByteCount10_ = 0u; // launcher adapter `+0x10`
    uint8_t* workingBuffer14_ = nullptr;      // launcher adapter `+0x14`
    uint32_t scratchByteCount1c_ = 0u;       // launcher adapter `+0x1c`
    uint8_t* scratchBuffer20_ = nullptr;     // launcher adapter `+0x20`
    uint32_t swapByteCount28_ = 0u;          // launcher large-adapter `+0x28`
    uint8_t* swapBuffer2c_ = nullptr;        // launcher large-adapter `+0x2c`
    std::array<uint8_t, 16> keyBytes38_{};   // launcher inner `AssemblyTwofish` seed bytes
    std::array<uint8_t, 16> ivBytes48_{};    // launcher named parameter `IV`
    bool configured58_ = false;
    bool decryptLarge59_ = false;
    uint8_t rounds5a_ = 0u;
};

class FeedbackSizeTransformAdapterSmall final : public FeedbackSizeTransformAdapterCommon {
public:
    bool FeedbackSizeTransformAdapter_ConstructSmall(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes,
        uint32_t /*unusedZero*/) {
        // anchor: launcher.exe:0x41df60
        return ConstructCommon(sourceBytes, sourceByteCount, ivBytes, /*decryptLarge=*/false);
    }
};

class FeedbackSizeTransformAdapterLarge final : public FeedbackSizeTransformAdapterCommon {
public:
    bool FeedbackSizeTransformAdapter_ConstructLarge(
        const void* sourceBytes,
        uint32_t sourceByteCount,
        const void* ivBytes,
        uint32_t /*unusedZero*/) {
        // anchor: launcher.exe:0x446d90
        return ConstructCommon(sourceBytes, sourceByteCount, ivBytes, /*decryptLarge=*/true);
    }
};

}  // namespace mxo::auth::internal
