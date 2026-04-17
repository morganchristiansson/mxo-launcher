#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

namespace mxo::liblttcp {

using byte = uint8_t;

// Crypto initialization helper class
// anchor: launcher.exe:0x4b42bc vtable
// Used for static initialization of crypto context
class CryptoInitHelper_0x4b42bc {
public:
    CryptoInitHelper_0x4b42bc(uint32_t param1);
    ~CryptoInitHelper_0x4b42bc();
    
    // Virtual methods from vtable
    virtual void virt_meth_0x468790(byte param1);
    virtual void virt_meth_0x4687a0();
    virtual void virt_meth_0x468dc0(uint32_t param1, void** param2);
    
private:
    uint32_t mbr_0x10;
    uint32_t mbr_0x14;
    uint32_t mbr_0x1c;
    void* mbr_0x20;
    uint32_t mbr_0x24;
    uint32_t mbr_0x28;
};

// Global crypto context
// anchor: launcher.exe:0x4f7bf4
extern std::unique_ptr<CryptoInitHelper_0x4b42bc> g_CryptoContext_0x4f7bf4;

// Initialization flag
// anchor: launcher.exe:0x4f7c20
extern uint32_t g_CryptoInitializedFlag_0x4f7c20;

// Static initialization function
// anchor: launcher.exe:0x4429b0 static init block
void EnsureCryptoContextInitialized();

} // namespace mxo::liblttcp
