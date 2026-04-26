#pragma once

#include <cstdint>

#include <randpool.h>

namespace mxo::liblttcp {

using byte = uint8_t;

// anchor: launcher.exe:0x4f7bf4 / outer wrapper vtable 0x4b695c
// Static-RE maps the original helper's +0x04 subobject (vtable 0x4b68a8) to the old
// Crypto++ RandomPool family. Source now models that semantic object directly with the
// modern compatibility type CryptoPP::OldRandomPool.
class CryptoInitHelperGlobal_0x4f7bf4 {
public:
    CryptoInitHelperGlobal_0x4f7bf4();

    CryptoPP::OldRandomPool& RandomPoolSubobject04() { return randomPoolSubobject04_; }
    const CryptoPP::OldRandomPool& RandomPoolSubobject04() const { return randomPoolSubobject04_; }

private:
    CryptoPP::OldRandomPool randomPoolSubobject04_;
};

// Global crypto context wrapper around the recovered launcher helper.
// anchor: launcher.exe:0x4f7bf4
extern CryptoInitHelperGlobal_0x4f7bf4 g_CryptoInitHelper_0x4f7bf4;

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
