#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

namespace mxo::liblttcp {

using byte = uint8_t;

// Reference-counted base class
// anchor: launcher.exe:0x4b42b0 vtable (12 bytes = 3 slots)
// Constructor: 0x437900, Destructor: 0x41cda0
// Used as base at offset 0 and as embedded subobject at offset 4
class CLTReferenceCountedBase_0x4b42b0 {
public:
    // anchor: launcher.exe:0x437900
    CLTReferenceCountedBase_0x4b42b0(char initFlag);
    virtual ~CLTReferenceCountedBase_0x4b42b0() = default;
    
    // Virtual table at 0x4b42b0 has 3 slots:
    // +0x0: dtor ~CLTReferenceCountedBase_0x4b42b0 (0x41cda0)
    // +0x4: unknown method
    // +0x8: ResetUnknownString (0x41d880) - "auth string helper"
    
    // Anchor for vtable pointer storage (matches original offset 0 in subobject)
    // In original this is just the vtable pointer, no other members
};

// Second reference-counted subobject at offset 4
// anchor: launcher.exe:0x4b9fa0 / 0x4bace0 vtables at offset 4
// Same class type as CLTReferenceCountedBase_0x4b42b0 but installed at different offset
// with different vtable progression. Distinct C++ type for multiple inheritance layout.
class CLTReferenceCountedBase_0x4b9fa0 {
public:
    CLTReferenceCountedBase_0x4b9fa0(char initFlag);
    virtual ~CLTReferenceCountedBase_0x4b9fa0() = default;
};

// Child object base class for owner dispatch
// anchor: launcher.exe:0x4b41e0 vtable (12 bytes = 3 slots)
// Tiny class with no fields - dispatches calls to parent/owner at this+(-4)
// Installed at offset 8 (mbr_0x8) in CryptoInitHelper during construction
class CLTChildObjectBase_0x4b41e0 {
public:
    CLTChildObjectBase_0x4b41e0();
    virtual ~CLTChildObjectBase_0x4b41e0() = default;
    
    // Virtual table at 0x4b41e0 layout (3 slots):
    // +0x00: GetOwnerContext (0x4372a0) - calls owner vtable+0xa4, returns result
    // +0x04: NotifyOwner (0x4372c0) - calls owner vtable+0xa8, void return
    // +0x08: StubReturn0 (0x437b50) - returns 0
    
    // anchor: launcher.exe:0x4372a0 / vtable+0x00
    // Dispatches to owner: ((this-4)->vtable+0xa4)(), then indirect call through result
    virtual uint32_t GetOwnerContext();
    
    // anchor: launcher.exe:0x4372c0 / vtable+0x04
    // Dispatches to owner: ((this-4)->vtable+0xa8)(), then indirect call through result
    virtual void NotifyOwner();
};

// Crypto initialization helper class
// anchor: launcher.exe:0x4b42bc vtable (64 bytes)
// 
// FIDELITY: Original object layout from constructor (0x4686e0):
//   offset 0x00: cls_0x4b42b0 subobject #1 (initialized with flag 1)
//                vtable progression: 0x4b42bc -> 0x4bacbc
//   offset 0x04: cls_0x4b42b0 subobject #2 (initialized with flag 0)
//                vtable progression: 0x4b9fa0 -> 0x4bace0
//   offset 0x08: cls_0x4b41e0 vtable pointer
//                progression: 0x4b3e18 (briefly) -> 0x4b41e0
//   offset 0x10: mbr_0x10 (param_1, allocation size)
//   offset 0x14: mbr_0x14 (allocated buffer pointer)
//   offset 0x1c: mbr_0x1c (0x40 - second buffer size)
//   offset 0x20: mbr_0x20 (second allocated buffer)
//   offset 0x24: mbr_0x24 (0)
//   offset 0x28: mbr_0x28 (param_1 copy)
//
// C++ IMPLEMENTATION: Using multiple inheritance to match offsets:
//   - CLTReferenceCountedBase_0x4b42b0 at offset 0 (primary base, vtable 0x4b42b0)
//   - CLTReferenceCountedBase_0x4b9fa0 at offset 4 (second subobject, vtable 0x4b9fa0)
//   - CLTChildObjectBase_0x4b41e0 at offset 8 (third base, vtable 0x4b41e0)
class CryptoInitHelper_0x4b42bc 
    : public CLTReferenceCountedBase_0x4b42b0   // offset 0
    , public CLTReferenceCountedBase_0x4b9fa0   // offset 4
    , public CLTChildObjectBase_0x4b41e0 {      // offset 8
public:
    CryptoInitHelper_0x4b42bc(uint32_t param1);
    ~CryptoInitHelper_0x4b42bc();
    
    // Virtual methods from vtable
    virtual void virt_meth_0x468790(byte param1);
    virtual void virt_meth_0x4687a0();
    virtual void InitializeCryptoState(uint32_t param1, void** param2);
    
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
