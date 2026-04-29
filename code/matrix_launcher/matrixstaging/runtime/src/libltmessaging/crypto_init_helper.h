#pragma once

#include <cstdint>

#include <randpool.h>

namespace mxo::liblttcp {

using byte = uint8_t;

// anchor: launcher.exe:0x4f7bf4 / outer wrapper vtable 0x4b695c
// Source-owned wrapper for the launcher global `CryptoInitHelper_0x4b42bc` object.
// Static-RE currently closes the embedded Crypto++ pieces as:
// - wrapper `+0x04` / vtable `0x4b68a8` -> old `CryptoPP::RandomPool`
//   (`CryptoPP::OldRandomPool` in the modern tree)
// - wrapper `+0x08` / vtable `0x4b41e0` -> `CryptoPP::BufferedTransformation`
//
// Source models the semantic RNG object directly and keeps the outer launcher wrapper boundary
// explicit so callers still pass around the same helper concept seen at `0x4429b0`, `0x44557a`,
// and `0x44d27a`.
class CryptoInitHelperWrapper_0x4f7bf4 {
public:
    CryptoInitHelperWrapper_0x4f7bf4();

    CryptoPP::OldRandomPool& RandomPoolSubobject04() { return randomPoolSubobject04_; }
    const CryptoPP::OldRandomPool& RandomPoolSubobject04() const { return randomPoolSubobject04_; }

private:
    CryptoPP::OldRandomPool randomPoolSubobject04_;
};

// Global crypto context wrapper around the recovered launcher helper.
// anchor: launcher.exe:0x4f7bf4
extern CryptoInitHelperWrapper_0x4f7bf4 g_CryptoInitHelper_0x4f7bf4;

// Initialization flag
// anchor: launcher.exe:0x4f7c20
// Bit 0: crypto context initialized
extern uint32_t g_CryptoInitializedFlag_0x4f7c20;

// Static initialization function
// anchor: launcher.exe:0x4429b0 static init block
// Original flow:
//   - construct CryptoInitHelper_0x4b42bc / RNG wrapper with pool size 0x180
//   - seed it through 0x468dc0 using 0x20 bytes of entropy
// Source keeps the original one-time init boundary but uses actual Crypto++ classes.
void EnsureCryptoContextInitialized();

} // namespace mxo::liblttcp
