#pragma once

#include <cstdint>

namespace mxo::ltlogin {
class CLTLoginMediator;
}

struct MinimalLoginMediatorStub {
    void** vtable;
    void* reserved04; // keep +0x04 layout placeholder; old wrapper-side registered-engine stash removed
    void* field08;
    void* field0C;
    void* field10;
    void* field14;
    void* field18;
    void* field1C;
    unsigned char payload[0x100];
};

struct DiagnosticMediatorRuntimeState {
    // Observer state moved to CLTLoginMediator instance:
    // - firstObserver170, latestObserver170, latestObserver174
    // - observerRegister170Count, observerUnregister174Count
    //
    // +0x124 state moved to CLTLoginMediator instance:
    // - provideStartupTripleNetShell_, provideStartupTripleNetMgr_, provideStartupTripleDistrObjExecutive_
    // - provideStartupTripleCount_
    void* loadingState120;
    uint32_t loading120Count;
};

extern DiagnosticMediatorRuntimeState g_MediatorRuntimeState;
extern void* g_LoginMediatorVtable[104];

mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel();
void LogPointerWords(const char* label, const void* ptr, uint32_t wordCount);
void LogWordBuffer(const char* label, const void* ptr, uint32_t byteCount);
void RegisterMediatorState9AbiSlots();

// Observer state accessors (moved from g_MediatorRuntimeState to CLTLoginMediator):
uint32_t DiagnosticMediatorObserverRegisterCount();
uint32_t DiagnosticMediatorObserverUnregisterCount();

const char g_MediatorName[] = "ILTLoginMediator.Default";
const char* DiagnosticMediatorAuthName();
const char* DiagnosticMediatorAuthPassword();
const char* MaskIfMediatorPassword(const char* value);
const char* MaskedSensitiveValue(const char* value);
