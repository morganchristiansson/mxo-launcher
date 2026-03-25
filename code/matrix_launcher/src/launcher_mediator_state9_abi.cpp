#include "launcher_mediator_abi_shared.h"

#include "diagnostics.h"
#include "diagnostics_auth.h"
#include "loginmediator.h"


// Focused late-login arg6 ABI split:
// - keep the state9-only ILTLoginMediator.Default slots out of the broad startup-selection ABI TU
// - active surface here is deliberately narrow:
//   - +0xd4
//   - +0x124
//   - +0x18c
// - canonical docs:
//   - ../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md
//   - ../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md

// anchor: launcher.exe:0x41b4f0 / arg6 vtable +0xd4
// Current active client-side state9 use from `0x620065e0`:
// - returns the 16-byte source pointer then consumed with size `0x10` by `0x62530630`
// - practical current read is the same launcher-owned Twofish key/seed family reused by `+0x18c`
static const void* __thiscall Mediator_GetState9CallbackSeedPointer85D4(MinimalLoginMediatorStub* self) {
    (void)self;
    return DiagnosticGetState9CallbackSeedPointer85D4();
}

// UNANCHORED: near-direct C helper behind the recovered wrapper-facing +0x124 ABI slot.
extern "C" void Mediator_ProvideStartupTriple_Impl(
    MinimalLoginMediatorStub* self,
    void* pNetShell,
    void* pNetMgr,
    void* pDistrObjExecutive,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;
    if (mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel()) {
        mediator->ProvideStartupTriple(pNetShell, pNetMgr, pDistrObjExecutive);
    }
}

// anchor: deeper client init hands netShell/netMgr/distrObjExecutive to arg6 +0x124
// vtable: ILTLoginMediator.Default slot +0x124
// Important later state9-submit tightening from newer client.dll + launcher evidence:
// - the captured `netShell` object is not a self-contained callback84-side answer source
// - `ClientNetShell` vtable `+0x38` / `0x62006580` later re-enters the client-side resolved
//   `ILTLoginMediator.Default` global at `0x629df7f0`, calls its `+0x18c` writer, and only then
//   returns pair `(&DAT_629e0284, 0x20)`
// - but newer bounded original-launcher lifecycle proof now also shows owner `+0x84/+0x88/+0x8c`
//   are really zero-init -> `0x41f1d0` startup store -> later submit-side reads, with owner
//   `+0x88` unchanged through `0x439780 -> 0x41de40 -> 0x43c180`
// - so the live replacement path now preserves the same-run startup `+0x124` triple directly,
//   while pairing that with a source-owned `+0x18c` implementation instead of cross-run object
//   transplant or hardcoded callback bytes
__attribute__((naked)) static void Mediator_ProvideStartupTriple() {
    __asm__ volatile(
        "mov 0(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $20, %%esp\n\t"
        "ret $12\n\t"
        :
        : "i"(Mediator_ProvideStartupTriple_Impl)
        : "eax");
}

// UNANCHORED: C helper behind the recovered +0x18c ABI wrapper.
extern "C" uint32_t Mediator_FillState9CallbackBlob18c_Impl(
    MinimalLoginMediatorStub* self,
    void* outBuffer,
    uint32_t arg2,
    uint32_t arg3,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;
    return DiagnosticFillState9CallbackBlob18c(outBuffer, arg2, arg3);
}

// anchor: launcher.exe:0x41e690 / arg6 vtable +0x18c
__attribute__((naked)) static void Mediator_FillState9CallbackBlob18c() {
    __asm__ volatile(
        "mov 0(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $20, %%esp\n\t"
        "ret $12\n\t"
        :
        : "i"(Mediator_FillState9CallbackBlob18c_Impl)
        : "eax");
}

void RegisterMediatorState9AbiSlots() {
    g_LoginMediatorVtable[53] = (void*)Mediator_GetState9CallbackSeedPointer85D4; // +0xd4
    g_LoginMediatorVtable[73] = (void*)Mediator_ProvideStartupTriple;              // +0x124 wrapper-facing ProvideStartupTriple
    g_LoginMediatorVtable[99] = (void*)Mediator_FillState9CallbackBlob18c;         // +0x18c
}
