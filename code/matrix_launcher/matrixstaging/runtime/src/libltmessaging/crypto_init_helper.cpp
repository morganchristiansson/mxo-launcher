#include "crypto_init_helper.h"

#include <array>

#include <osrng.h>
#include <spdlog/spdlog.h>

namespace mxo::liblttcp {

CryptoInitHelperWrapper_0x4f7bf4::CryptoInitHelperWrapper_0x4f7bf4()
    : randomPoolSubobject04_(0x180u) {
}

// anchor: launcher.exe:0x4f7bf4
CryptoInitHelperWrapper_0x4f7bf4 g_CryptoInitHelper_0x4f7bf4{};

// anchor: launcher.exe:0x4f7c20
uint32_t g_CryptoInitializedFlag_0x4f7c20 = 0;

// anchor: launcher.exe:0x4429b0 static init block
void EnsureCryptoContextInitialized() {
    if ((g_CryptoInitializedFlag_0x4f7c20 & 1u) != 0u) {
        return;
    }

    g_CryptoInitializedFlag_0x4f7c20 |= 1u;

    try {
        // anchor: launcher.exe:0x4429b0 / 0x468dc0 / MicrosoftCryptoProvider family
        // Prefer the direct Crypto++ OS entropy path over a transient AutoSeededRandomPool. The
        // recovered launcher helper seeds its embedded OldRandomPool from the old OS-provider
        // family before later callers consume bytes through the wrapper/random-pool subobjects.
        std::array<CryptoPP::byte, 0x20> entropySeed{};
        CryptoPP::OS_GenerateRandomBlock(false, entropySeed.data(), entropySeed.size());
        g_CryptoInitHelper_0x4f7bf4.RandomPoolSubobject04().IncorporateEntropy(
            entropySeed.data(), entropySeed.size());

        spdlog::debug(
            "EnsureCryptoContextInitialized: seeded CryptoPP::OldRandomPool helper "
            "poolSize=0x180 seedBytes=0x20 this={}",
            fmt::ptr(&g_CryptoInitHelper_0x4f7bf4));
    } catch (const CryptoPP::Exception& exception) {
        spdlog::warn(
            "EnsureCryptoContextInitialized: CryptoPP RNG seed failed: {}",
            exception.what());
    }
}

} // namespace mxo::liblttcp
