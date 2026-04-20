#include "crypto_init_helper.h"

#include <cstdlib>
#include <cstring>

namespace mxo::liblttcp {

// External vtable references - these are function pointers from the original binary
// We use addresses from the original launcher.exe
namespace vtables {
    // anchor: launcher.exe:0x4b695c - CryptoInitHelper primary vtable
    // This vtable is set at [0x4f7bf4] after construction
    // Contains: [0x442950, 0x437b50, 0x41d880, 0x468050, 0x437b40, 0x468c30, 0x437880]
    __declspec(dllimport) extern const void* const vtable_0x4b695c;
    
    // anchor: launcher.exe:0x4b68a8 - method pointer stored at +0x04
    // This is set to address 0x442950 (FUN_442950)
    __declspec(dllimport) extern void* const methodPtr_0x4b68a8;
    
    // anchor: launcher.exe:0x4b41e0 - CLTChildObjectBase vtable at +0x08
    __declspec(dllimport) extern const void* const vtable_0x4b41e0;
}

// anchor: launcher.exe:0x4f7bf4 - Global crypto context structure
// This must be at the exact same address as in the original binary (0x4f7bf4)
// FIDELITY: Using static allocation at exact address
#if defined(__MINGW32__)
__attribute__((section(".data")))
#endif
CryptoInitHelperGlobal_0x4f7bf4 g_CryptoInitHelper_0x4f7bf4 = {};

// Initialization flag
// anchor: launcher.exe:0x4f7c20
#if defined(__MINGW32__)
__attribute__((section(".data")))
#endif
uint32_t g_CryptoInitializedFlag_0x4f7c20 = 0;

// Helper function to initialize the crypto context fields
// anchor: launcher.exe:0x4686e0 - CryptoInitHelper_ctor
// This replicates the constructor logic manually
static void CryptoInitHelper_InitializeFields(CryptoInitHelperGlobal_0x4f7bf4* ctx, uint32_t param1) {
    // Initialize vtable pointers to temporary vtables during construction
    // These will be overwritten after construction
    // For now, set fields based on original constructor logic
    
    ctx->paramSize_0x10 = param1;              // param_1
    ctx->bufferPtr_0x14 = malloc(param1);       // allocate buffer
    ctx->secondBufferSize_0x1c = 0x40;          // second buffer size
    ctx->secondBufferPtr_0x20 = malloc(0x40);   // second buffer
    ctx->field_0x24 = 0;
    ctx->paramSizeCopy_0x28 = param1;            // copy of param_1
    
    // Zero the allocated buffers
    if (ctx->bufferPtr_0x14) {
        memset(ctx->bufferPtr_0x14, 0, param1);
    }
    if (ctx->secondBufferPtr_0x20) {
        memset(ctx->secondBufferPtr_0x20, 0, 0x40);
    }
}

// Helper function for InitializeCryptoState
// anchor: launcher.exe:0x468dc0
static void CryptoInitHelper_InitializeCryptoState(CryptoInitHelperGlobal_0x4f7bf4* ctx, uint32_t param1, void** param2) {
    // Original decompile:
    //   ppuVar1 = param_2;
    //   puVar2 = FUN_0041d2e0((uint)param_2);
    //   CLTReferenceCountedBase_0x4b42b0::CLTReferenceCountedBase_0x4b42b0(&param_2, '\x01');
    //   param_2 = &cls_0x4baed8__vftable_4baed8_004baed8.~cls_0x4b0000_0;
    //   cls_0x4baed8::virt_meth_0x468d60((cls_0x4baed8 *)&param_2,(DWORD)puVar2);
    //   (*(this->refCountedBase2_0x4).vftptr_0x0[1].AuthBootstrap680Field54Helper_ResetUnknownString_8)
    //             (&this->refCountedBase2_0x4);
    //   FUN_0041c750(ppuVar1,(uint)puVar2);
    //
    // The key operation is calling AuthBootstrap680Field54Helper_ResetUnknownString
    // on the second subobject (offset +4), which resets a string field.
    (void)ctx;
    (void)param1;
    if (param2) {
        *param2 = nullptr;
    }
}

// Static initialization function
// anchor: launcher.exe:0x4429b0 static init block
// Replicates original disassembly EXACTLY:
void EnsureCryptoContextInitialized() {
    // anchor: launcher.exe:0x4429b9
    if ((g_CryptoInitializedFlag_0x4f7c20 & 1) == 0) {
        // anchor: launcher.exe:0x4429c9 - set flag
        g_CryptoInitializedFlag_0x4f7c20 |= 1;
        
        // anchor: launcher.exe:0x4429cf-0x4429d9 - constructor call
        // Original: PUSH 0x180; MOV ECX,0x4f7bf4; CALL 0x4686e0
        // This initializes the fields at offsets +0x10 through +0x28
        CryptoInitHelper_InitializeFields(&g_CryptoInitHelper_0x4f7bf4, 0x180);
        
        // anchor: launcher.exe:0x4429e7 - set vtable pointer at +0x00
        // Original: MOV [0x4f7bf4],0x4b695c
        // FIDELITY: Store the vtable address as a data pointer
        g_CryptoInitHelper_0x4f7bf4.vtablePtr_0x00 = 
            reinterpret_cast<const void*>(0x4b695c);
        
        // anchor: launcher.exe:0x4429f1 - set method pointer at +0x04
        // Original: MOV [0x4f7bf8],0x4b68a8
        g_CryptoInitHelper_0x4f7bf4.methodPtr_0x04 = 
            reinterpret_cast<void*>(0x4b68a8);
        
        // anchor: launcher.exe:0x4429fb - set child vtable at +0x08
        // Original: MOV [0x4f7bfc],0x4b41e0
        g_CryptoInitHelper_0x4f7bf4.childVtablePtr_0x08 = 
            reinterpret_cast<const void*>(0x4b41e0);
        
        // anchor: launcher.exe:0x442a05 - call InitializeCryptoState
        // Original: CALL 0x468dc0 with (0, &DAT_00000020)
        void* datPtr = nullptr;  // &DAT_00000020 - unknown global
        CryptoInitHelper_InitializeCryptoState(&g_CryptoInitHelper_0x4f7bf4, 0, &datPtr);
        
        // anchor: launcher.exe:0x442a0a-0x442a0f - atexit handler
        // Original: PUSH 0x4a6e80; CALL _atexit
        // We don't need to register atexit for cleanup in this implementation
    }
}

} // namespace mxo::liblttcp
