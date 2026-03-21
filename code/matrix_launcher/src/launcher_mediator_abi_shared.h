#pragma once

#include <cstdint>

namespace mxo::ltlogin {
class CLTLoginMediator;
}

struct MinimalLoginMediatorStub {
    void** vtable;
    void* field04;
    void* field08;
    void* field0C;
    void* field10;
    void* field14;
    void* field18;
    void* field1C;
    unsigned char payload[0x100];
};

struct DiagnosticMediatorRuntimeState {
    void* registeredLauncherObject;
    const void* lastNopatchValue1Ptr;
    const void* lastNopatchValue2Ptr;
    void* firstObserver170;
    void* latestObserver170;
    void* netShell124;
    void* netMgr124;
    void* distrObjExecutive124;
    void* loadingState120;
    void* selectionContext0ec;
    void* selectionContext0ecCopy;
    void* runtimeObject148;
    void* latestObserver174;
    uint32_t lastStatus178;
    uint32_t observerRegister170Count;
    uint32_t provide124Count;
    uint32_t loading120Count;
    uint32_t selection0ecCount;
    uint32_t profile0f4Count;
    uint32_t runtime148Count;
    uint32_t observerUnregister174Count;
    uint32_t statusQuery178Count;
};

extern DiagnosticMediatorRuntimeState g_MediatorRuntimeState;
extern void* g_LoginMediatorVtable[104];

mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel();
void LogPointerWords(const char* label, const void* ptr, uint32_t wordCount);
void LogWordBuffer(const char* label, const void* ptr, uint32_t byteCount);
void RegisterMediatorState9AbiSlots();
