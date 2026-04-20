#pragma once

#include <cstdint>
#include <cstddef>

namespace mxo::liblttcp {

using byte = uint8_t;

// Forward declarations for vtable function pointers
namespace vtables {
    // anchor: launcher.exe:0x4b695c - CryptoInitHelper primary vtable
    extern const void* const vtable_0x4b695c;
    
    // anchor: launcher.exe:0x4b68a8 - AuthBootstrap680ReplyAuthDataValidator method
    // This is a function pointer, not a vtable
    extern void* const methodPtr_0x4b68a8;
    
    // anchor: launcher.exe:0x4b41e0 - CLTChildObjectBase vtable
    extern const void* const vtable_0x4b41e0;
}

// anchor: launcher.exe:0x4f7bf4 - Global crypto context structure
// Original layout (flat memory, NOT C++ class):
//   +0x00: pointer to vtable 0x4b695c
//   +0x04: pointer to method 0x4b68a8  
//   +0x08: pointer to vtable 0x4b41e0
//   +0x0c: padding
//   +0x10: param_1 (allocation size, 0x180)
//   +0x14: pointer to allocated buffer
//   +0x18: padding/alignment
//   +0x1c: second buffer size (0x40)
//   +0x20: pointer to second buffer
//   +0x24: field_0x24 (0)
//   +0x28: copy of param_1
//   ... rest is zeroed/allocated buffer space
struct CryptoInitHelperGlobal_0x4f7bf4 {
    const void* vtablePtr_0x00;       // anchor: 0x4b695c
    void* methodPtr_0x04;              // anchor: 0x4b68a8
    const void* childVtablePtr_0x08;   // anchor: 0x4b41e0
    // +0x0c: padding
    uint32_t paramSize_0x10;           // anchor: 0x180
    void* bufferPtr_0x14;              // allocated buffer
    // +0x18: padding
    uint32_t secondBufferSize_0x1c;   // anchor: 0x40
    void* secondBufferPtr_0x20;       // second allocated buffer
    uint32_t field_0x24;               // anchor: 0
    uint32_t paramSizeCopy_0x28;       // anchor: 0x180
};

// Global crypto context - points to static buffer at exact original address
// anchor: launcher.exe:0x4f7bf4
extern CryptoInitHelperGlobal_0x4f7bf4 g_CryptoInitHelper_0x4f7bf4;

// Initialization flag  
// anchor: launcher.exe:0x4f7c20
// Bit 0: crypto context initialized
extern uint32_t g_CryptoInitializedFlag_0x4f7c20;

// Static initialization function
// anchor: launcher.exe:0x4429b0 static init block
// Replicates original disassembly:
//   0x4429cf: PUSH 0x180
//   0x4429d4: MOV ECX,0x4f7bf4
//   0x4429d9: CALL 0x4686e0                 ; CryptoInitHelper_0x4b42bc ctor
//   0x4429de: PUSH 0x20
//   0x4429e0: PUSH 0x0
//   0x4429e2: MOV ECX,0x4f7bf4
//   0x4429e7: MOV [0x4f7bf4],0x4b695c       ; vtable 0x4b695c at +0x00
//   0x4429f1: MOV [0x4f7bf8],0x4b68a8       ; method ptr 0x4b68a8 at +0x04
//   0x4429fb: MOV [0x4f7bfc],0x4b41e0       ; vtable 0x4b41e0 at +0x08
//   0x442a05: CALL 0x468dc0                 ; meth_0x468dc0(0, &DAT_00000020)
void EnsureCryptoContextInitialized();

} // namespace mxo::liblttcp
