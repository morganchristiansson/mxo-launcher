#include "diagnostics.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

struct WindowTraceEntry {
    HWND hwnd;
    LONG style;
    LONG exStyle;
    RECT rect;
    BOOL visible;
    BOOL iconic;
};

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

struct DiagnosticLauncherQueue {
    void* current0;        // +0x00
    void* block0;          // +0x04
    void* end0;            // +0x08
    void* slotsCurrent;    // +0x0c
    void* current1;        // +0x10
    void* block1;          // +0x14
    void* end1;            // +0x18
    void* slotsLast;       // +0x1c
    void* slotsBase;       // +0x20
    uint32_t slotCapacity; // +0x24
};

struct MinimalLauncherObjectStub {
    void** vtable;              // +0x00
    uint32_t field04;           // +0x04 ctor arg in original (0 from 0x40a380)
    void* field08;              // +0x08 pointer array in base ctor (NULL when field04==0)
    DiagnosticLauncherQueue queue0C; // +0x0c..+0x33 base queue state from 0x436610/0x436340
    DiagnosticLauncherQueue queue34; // +0x34..+0x5b second base queue from 0x436610/0x436340
    void** subVtable5C;         // +0x5c base subobject vtable / state tag
    const void* field60;        // +0x60 constant table pointer in base ctor
    unsigned char subobject64[0x18]; // +0x64..+0x7b initialized by imported helper
    void* field7C;              // +0x7c handle/result from imported helper
    void* list80;               // +0x80 allocated 0x24 list head
    uint32_t field84;           // +0x84 zeroed in derived ctor
    uint32_t field88;           // +0x88 left generic for now
    void* list8C;               // +0x8c allocated 0x18 list head
    uint32_t field90;           // +0x90 zeroed in derived ctor
    uint32_t field94;           // +0x94 left generic for now
    const void* field98;        // +0x98 constant table pointer in derived ctor
    unsigned char subobject9C[0x18]; // +0x9c..+0xb3 initialized by imported helper
};

struct DiagnosticIntrusiveListHead {
    unsigned char firstByte;
    unsigned char padding[3];
    uint32_t count;
    void* next;
    void* prev;
    unsigned char payload[0x14];
};

struct DiagnosticIntrusiveListHeadSmall {
    unsigned char firstByte;
    unsigned char padding[3];
    uint32_t count;
    void* next;
    void* prev;
    unsigned char payload[0x8];
};

struct DiagnosticLauncherObjectBuildState {
    MinimalLauncherObjectStub* currentObject;
    uint32_t buildGeneration;
    uint32_t slot1CallCount;
    uint32_t slot2CallCount;
    uint32_t slot3CallCount;
    uint32_t slot4CallCount;
    uint32_t subobject5CSlot1CallCount;
    uint32_t subobject60Slot0CallCount;
    uint32_t subobject60Slot1CallCount;
    uint32_t subobject98Slot0CallCount;
    uint32_t subobject98Slot1CallCount;
};

struct DiagnosticMediatorResolverNode {
    DiagnosticMediatorResolverNode* next;
    const char* serviceName;
    void* resolvedObject;
};

struct DiagnosticBinderRegistry {
    void* reserved0;
    void* reserved4;
    void* reserved8;
    void* reservedC;
    void* reserved10;
    void* reserved14;
    DiagnosticMediatorResolverNode* resolverList; // mirrors interest in launcher.exe registry+0x18
};

struct DiagnosticBinderWrapper {
    const char* serviceName;
    uint32_t mode;
    void** outSlot;
    DiagnosticBinderRegistry* registry;
    DiagnosticMediatorResolverNode* lastResolvedNode;
};

static_assert(sizeof(MinimalLauncherObjectStub) == 0xb4, "launcher object scaffold size must match original allocation");
static_assert(sizeof(DiagnosticIntrusiveListHead) == 0x24, "list80 scaffold size mismatch");
static_assert(sizeof(DiagnosticIntrusiveListHeadSmall) == 0x18, "list8C scaffold size mismatch");

struct DiagnosticMediatorRuntimeState {
    void* registeredLauncherObject;
    const void* lastNopatchValue1Ptr;
    const void* lastNopatchValue2Ptr;
    void* firstContext170;
    void* latestContext170;
    void* netShell124;
    void* netMgr124;
    void* distrObjExecutive124;
    void* selectionContext0ec;
    void* runtimeObject148;
    void* runtimeObject174;
    void* runtimeDescriptor178;
    uint32_t attach170Count;
    uint32_t provide124Count;
    uint32_t selection0ecCount;
    uint32_t runtime148Count;
    uint32_t runtime174Count;
    uint32_t descriptor178Count;
};

static DWORD g_MainProcessId = 0;
static HANDLE g_hWindowTraceThread = NULL;
static volatile LONG g_WindowTraceRunning = 0;
static WindowTraceEntry g_WindowTraceEntries[32] = {};
static int g_WindowTraceEntryCount = 0;
static int g_LastWindowTraceCount = -1;

static MinimalLoginMediatorStub g_LoginMediatorStub = {};
static DiagnosticLauncherObjectBuildState g_LauncherObjectBuildState = {};
static DiagnosticMediatorResolverNode g_DiagnosticMediatorResolver = {};
static DiagnosticBinderRegistry g_DiagnosticBinderRegistry = {};
static DiagnosticBinderWrapper g_DiagnosticBinderWrapper = {};
static DiagnosticMediatorRuntimeState g_MediatorRuntimeState = {};
static void* g_LoginMediatorVtable[96] = {0};
static void* g_LauncherObjectVtable[16] = {0};
static void* g_LauncherObjectSubVtable5C[8] = {0};
static void* g_LauncherObjectSubVtable60[8] = {0};
static void* g_LauncherObjectSubVtable98[8] = {0};
static const char g_LauncherObjectConstTable[] = "diagnostic-launcher-object";
static const char g_MediatorName[] = "ILTLoginMediator.Default";
static const char g_MediatorStringA[] = "resurrections";
static const char g_MediatorStringB[] = "nopatch";
static const char g_MediatorStringC[] = "standalone";
static uint32_t g_MediatorSelectionHighByteFloor = 0;
static const char* g_MediatorMappedSelectionName = g_MediatorStringC;
static const char* g_MediatorProfileName = g_MediatorStringA;

static void LogPointerWords(const char* label, const void* ptr, uint32_t wordCount) {
    if (!ptr || !wordCount) {
        Log("%s: <null>", label ? label : "PointerWords");
        return;
    }

    const uint32_t* words = static_cast<const uint32_t*>(ptr);
    Log("%s @ %p [+0x00]=%08x [+0x04]=%08x [+0x08]=%08x [+0x0c]=%08x",
        label,
        ptr,
        words[0],
        (wordCount > 1) ? words[1] : 0,
        (wordCount > 2) ? words[2] : 0,
        (wordCount > 3) ? words[3] : 0);
    if (wordCount > 4) {
        Log("%s @ %p [+0x10]=%08x [+0x14]=%08x [+0x18]=%08x [+0x1c]=%08x",
            label,
            ptr,
            words[4],
            (wordCount > 5) ? words[5] : 0,
            (wordCount > 6) ? words[6] : 0,
            (wordCount > 7) ? words[7] : 0);
    }
}

static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    g_LoginMediatorStub.vtable = g_LoginMediatorVtable;
}

static const char* __thiscall Mediator_GetName(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorName;
}

static int __thiscall Mediator_RegisterEngine(MinimalLoginMediatorStub* self, void* object) {
    g_MediatorRuntimeState.registeredLauncherObject = object;
    if (self) {
        self->field04 = object;
    }
    Log("MediatorStub::RegisterEngine(%p)", object);
    return 1;
}

static void __thiscall Mediator_ClearEngine(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::ClearEngine()");
}

static uint32_t __thiscall Mediator_IsReady(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsReady() -> 1");
    return 1;
}

static void __thiscall Mediator_SetValue1(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    if (!g_MediatorRuntimeState.lastNopatchValue1Ptr) {
        g_MediatorRuntimeState.lastNopatchValue1Ptr = value;
    } else {
        g_MediatorRuntimeState.lastNopatchValue2Ptr = value;
    }
    Log("MediatorStub::SetValue1(%p)", value);
}

static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsConnected() -> 1");
    return 1;
}

static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorStringA;
}

static const char* __thiscall Mediator_GetWorldOrSelectionName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetWorldOrSelectionName() -> '%s'", g_MediatorMappedSelectionName);
    return g_MediatorMappedSelectionName;
}

static const char* __thiscall Mediator_GetProfileOrSessionName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetProfileOrSessionName() -> '%s'", g_MediatorProfileName);
    return g_MediatorProfileName;
}

static const char* __thiscall Mediator_GetString0(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorStringA;
}

static const char* __thiscall Mediator_GetString1(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    (void)value;
    return g_MediatorStringB;
}

static const char* __thiscall Mediator_GetString2(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    (void)value;
    return g_MediatorMappedSelectionName;
}

static uint32_t __thiscall Mediator_GetArg7HighByteFloor(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetArg7HighByteFloor() -> %u", (unsigned)g_MediatorSelectionHighByteFloor);
    return g_MediatorSelectionHighByteFloor;
}

static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    Log(
        "MediatorStub::MapSelectionName(selectionHighByte=%u) -> '%s'",
        (unsigned)selectionHighByte,
        g_MediatorMappedSelectionName);
    return g_MediatorMappedSelectionName;
}

extern "C" void Mediator_ConsumeSelectionContext_Impl(
    MinimalLoginMediatorStub* self,
    void* selectionContext,
    void* returnAddress) {
    g_MediatorRuntimeState.selectionContext0ec = selectionContext;
    if (self) {
        self->field1C = selectionContext;
    }
    ++g_MediatorRuntimeState.selection0ecCount;
    Log(
        "MediatorStub::ConsumeSelectionContext(%p) [count=%u caller=%p]",
        selectionContext,
        (unsigned)g_MediatorRuntimeState.selection0ecCount,
        returnAddress);
}

__attribute__((naked)) static void Mediator_ConsumeSelectionContext() {
    __asm__ volatile(
        "push %%ebx\n\t"
        "mov 8(%%esp), %%eax\n\t"
        "mov 4(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "call *%%ebx\n\t"
        "add $12, %%esp\n\t"
        "pop %%ebx\n\t"
        "ret $4\n\t"
        :
        : "b"(Mediator_ConsumeSelectionContext_Impl)
        : "eax", "edx");
}

extern "C" void Mediator_ProvideStartupTriple_Impl(
    MinimalLoginMediatorStub* self,
    void* pNetShell,
    void* pNetMgr,
    void* pDistrObjExecutive,
    void* returnAddress) {
    g_MediatorRuntimeState.netShell124 = pNetShell;
    g_MediatorRuntimeState.netMgr124 = pNetMgr;
    g_MediatorRuntimeState.distrObjExecutive124 = pDistrObjExecutive;
    if (self) {
        self->field0C = pNetShell;
        self->field10 = pNetMgr;
        self->field14 = pDistrObjExecutive;
    }
    ++g_MediatorRuntimeState.provide124Count;
    Log(
        "MediatorStub::ProvideStartupTriple(netShell=%p netMgr=%p distrObjExecutive=%p self=%p) [count=%u caller=%p]",
        pNetShell,
        pNetMgr,
        pDistrObjExecutive,
        self,
        (unsigned)g_MediatorRuntimeState.provide124Count,
        returnAddress);
    LogPointerWords("ProvideStartupTriple self", self, 8);
    LogPointerWords("ProvideStartupTriple netShell", pNetShell, 8);
    LogPointerWords("ProvideStartupTriple netMgr", pNetMgr, 8);
    LogPointerWords("ProvideStartupTriple distrObjExecutive", pDistrObjExecutive, 8);
}

__attribute__((naked)) static void Mediator_ProvideStartupTriple() {
    __asm__ volatile(
        "push %%ebx\n\t"
        "mov 4(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 20(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 20(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 20(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "call *%%ebx\n\t"
        "add $20, %%esp\n\t"
        "pop %%ebx\n\t"
        "ret $12\n\t"
        :
        : "b"(Mediator_ProvideStartupTriple_Impl)
        : "eax");
}

extern "C" void Mediator_AttachStartupContext_Impl(
    MinimalLoginMediatorStub* self,
    void* startupContext,
    void* returnAddress) {
    if (!g_MediatorRuntimeState.firstContext170) {
        g_MediatorRuntimeState.firstContext170 = startupContext;
    }
    g_MediatorRuntimeState.latestContext170 = startupContext;
    if (self) {
        if (!self->field08) {
            self->field08 = startupContext;
        }
        self->field18 = startupContext;
    }
    ++g_MediatorRuntimeState.attach170Count;

    const bool sawTriple =
        g_MediatorRuntimeState.netShell124 ||
        g_MediatorRuntimeState.netMgr124 ||
        g_MediatorRuntimeState.distrObjExecutive124;
    const char* relation = "before-124";
    if (sawTriple) {
        relation = (startupContext == g_MediatorRuntimeState.firstContext170) ? "repeat-first-after-124" : "post-124";
    }

    Log(
        "MediatorStub::AttachStartupContext(%p self=%p) [count=%u relation=%s first=%p latest124=(%p,%p,%p) caller=%p]",
        startupContext,
        self,
        (unsigned)g_MediatorRuntimeState.attach170Count,
        relation,
        g_MediatorRuntimeState.firstContext170,
        g_MediatorRuntimeState.netShell124,
        g_MediatorRuntimeState.netMgr124,
        g_MediatorRuntimeState.distrObjExecutive124,
        returnAddress);
    LogPointerWords("AttachStartupContext self", self, 8);
    LogPointerWords("AttachStartupContext context", startupContext, 8);
}

__attribute__((naked)) static void Mediator_AttachStartupContext() {
    __asm__ volatile(
        "push %%ebx\n\t"
        "mov 8(%%esp), %%eax\n\t"
        "mov 4(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "call *%%ebx\n\t"
        "add $12, %%esp\n\t"
        "pop %%ebx\n\t"
        "ret $4\n\t"
        :
        : "b"(Mediator_AttachStartupContext_Impl)
        : "eax", "edx");
}

static const char* __thiscall Mediator_GetProfilePathComponent(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetProfilePathComponent() -> '%s'", g_MediatorProfileName);
    return g_MediatorProfileName;
}

static void __thiscall Mediator_AttachRuntimeObject(MinimalLoginMediatorStub* self, void* runtimeObject) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    if (g_MediatorRuntimeState.provide124Count == 0) {
        g_MediatorRuntimeState.runtimeObject148 = runtimeObject;
        ++g_MediatorRuntimeState.runtime148Count;
        Log(
            "MediatorStub::AttachRuntimeObject(+0x148 guess=%p) [count=%u caller=%p]",
            runtimeObject,
            (unsigned)g_MediatorRuntimeState.runtime148Count,
            returnAddress);
        return;
    }

    g_MediatorRuntimeState.runtimeObject174 = runtimeObject;
    ++g_MediatorRuntimeState.runtime174Count;
    Log(
        "MediatorStub::AttachRuntimeObject(+0x174 guess=%p) [count=%u caller=%p]",
        runtimeObject,
        (unsigned)g_MediatorRuntimeState.runtime174Count,
        returnAddress);
}

static void __thiscall Mediator_ConsumeRuntimeDescriptor(MinimalLoginMediatorStub* self, void* runtimeDescriptor) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    g_MediatorRuntimeState.runtimeDescriptor178 = runtimeDescriptor;
    ++g_MediatorRuntimeState.descriptor178Count;
    Log(
        "MediatorStub::ConsumeRuntimeDescriptor(%p) [count=%u caller=%p]",
        runtimeDescriptor,
        (unsigned)g_MediatorRuntimeState.descriptor178Count,
        returnAddress);
}

static uint32_t __thiscall Mediator_ShouldExportA(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

static uint32_t __thiscall Mediator_ShouldExportB(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

static void InitializeDiagnosticIntrusiveListHead(DiagnosticIntrusiveListHead* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->firstByte = 0;
    head->count = 0;
    head->next = head;
    head->prev = head;
}

static void InitializeDiagnosticIntrusiveListHeadSmall(DiagnosticIntrusiveListHeadSmall* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->firstByte = 0;
    head->count = 0;
    head->next = head;
    head->prev = head;
}

static void DiagnosticFreeLauncherObjectInternals(MinimalLauncherObjectStub* self) {
    if (!self) return;
    if (self->list80) {
        std::free(self->list80);
        self->list80 = NULL;
    }
    if (self->list8C) {
        std::free(self->list8C);
        self->list8C = NULL;
    }
}

static int __thiscall LauncherObject_Release(MinimalLauncherObjectStub* self, uint32_t flags) {
    Log("LauncherObjectStub::Release(flags=%u self=%p)", flags, self);
    DiagnosticFreeLauncherObjectInternals(self);
    return 1;
}

static uint32_t __thiscall LauncherObject_Slot1_431CE0(
    MinimalLauncherObjectStub* self,
    void* arg1,
    void* arg2,
    void* arg3) {
    ++g_LauncherObjectBuildState.slot1CallCount;
    Log(
        "LauncherObjectStub::Slot1_431CE0(self=%p arg1=%p arg2=%p arg3=%p) [count=%u]",
        self,
        arg1,
        arg2,
        arg3,
        (unsigned)g_LauncherObjectBuildState.slot1CallCount);
    LogPointerWords("LauncherObject slot1 self", self, 8);
    return 0;
}

static uint32_t __thiscall LauncherObject_Slot2_4325D0(
    MinimalLauncherObjectStub* self,
    void* arg1,
    void* arg2,
    void* arg3) {
    ++g_LauncherObjectBuildState.slot2CallCount;
    Log(
        "LauncherObjectStub::Slot2_4325D0(self=%p arg1=%p arg2=%p arg3=%p) [count=%u]",
        self,
        arg1,
        arg2,
        arg3,
        (unsigned)g_LauncherObjectBuildState.slot2CallCount);
    LogPointerWords("LauncherObject slot2 self", self, 8);
    return 0;
}

static uint32_t __thiscall LauncherObject_Slot3_436000(
    MinimalLauncherObjectStub* self,
    void* arg1,
    void* arg2,
    void* arg3) {
    ++g_LauncherObjectBuildState.slot3CallCount;
    Log(
        "LauncherObjectStub::Slot3_436000(self=%p arg1=%p arg2=%p arg3=%p) [count=%u]",
        self,
        arg1,
        arg2,
        arg3,
        (unsigned)g_LauncherObjectBuildState.slot3CallCount);
    LogPointerWords("LauncherObject slot3 self", self, 8);
    return 0;
}

static uint32_t __thiscall LauncherObject_Slot4_42F7C0(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    ++g_LauncherObjectBuildState.slot4CallCount;
    Log(
        "LauncherObjectStub::Slot4_42F7C0(self=%p arg1=%p) [count=%u]",
        self,
        arg1,
        (unsigned)g_LauncherObjectBuildState.slot4CallCount);
    LogPointerWords("LauncherObject slot4 self", self, 8);
    return 0;
}

static uint32_t __thiscall LauncherObject_Subobject5C_Slot1(void* self, int reason) {
    ++g_LauncherObjectBuildState.subobject5CSlot1CallCount;
    Log(
        "LauncherObjectStub::Subobject5C::Slot1(self=%p reason=%d) [count=%u]",
        self,
        reason,
        (unsigned)g_LauncherObjectBuildState.subobject5CSlot1CallCount);
    LogPointerWords("LauncherObject subobject5C self", self, 4);
    return 0;
}

static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject60Slot0CallCount;
    Log(
        "LauncherObjectStub::Subobject60::Slot0(self=%p) [count=%u]",
        self,
        (unsigned)g_LauncherObjectBuildState.subobject60Slot0CallCount);
    LogPointerWords("LauncherObject subobject60 self", self, 4);
    return 0;
}

static uint32_t __thiscall LauncherObject_Subobject60_Slot1(void* self) {
    ++g_LauncherObjectBuildState.subobject60Slot1CallCount;
    Log(
        "LauncherObjectStub::Subobject60::Slot1(self=%p) [count=%u]",
        self,
        (unsigned)g_LauncherObjectBuildState.subobject60Slot1CallCount);
    LogPointerWords("LauncherObject subobject60 self", self, 4);
    return 0;
}

static uint32_t __thiscall LauncherObject_Subobject98_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject98Slot0CallCount;
    Log(
        "LauncherObjectStub::Subobject98::Slot0(self=%p) [count=%u]",
        self,
        (unsigned)g_LauncherObjectBuildState.subobject98Slot0CallCount);
    LogPointerWords("LauncherObject subobject98 self", self, 4);
    return 0;
}

static uint32_t __thiscall LauncherObject_Subobject98_Slot1(void* self) {
    ++g_LauncherObjectBuildState.subobject98Slot1CallCount;
    Log(
        "LauncherObjectStub::Subobject98::Slot1(self=%p) [count=%u]",
        self,
        (unsigned)g_LauncherObjectBuildState.subobject98Slot1CallCount);
    LogPointerWords("LauncherObject subobject98 self", self, 4);
    return 0;
}

static void InitializeMediatorStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::memset(&g_MediatorRuntimeState, 0, sizeof(g_MediatorRuntimeState));
    std::memset(g_LoginMediatorVtable, 0, sizeof(g_LoginMediatorVtable));
    g_LoginMediatorVtable[0] = (void*)Mediator_GetName;          // +0x00
    g_LoginMediatorVtable[2] = (void*)Mediator_RegisterEngine;   // +0x08
    g_LoginMediatorVtable[3] = (void*)Mediator_ClearEngine;      // +0x0c
    g_LoginMediatorVtable[4] = (void*)Mediator_IsReady;          // +0x10
    g_LoginMediatorVtable[7] = (void*)Mediator_SetValue1;        // +0x1c
    g_LoginMediatorVtable[9] = (void*)Mediator_SetValue1;        // +0x24
    g_LoginMediatorVtable[11] = (void*)Mediator_IsConnected;     // +0x2c
    g_LoginMediatorVtable[14] = (void*)Mediator_GetDisplayName;  // +0x38
    g_LoginMediatorVtable[18] = (void*)Mediator_GetWorldOrSelectionName; // +0x48
    g_LoginMediatorVtable[19] = (void*)Mediator_GetProfileOrSessionName; // +0x4c
    g_LoginMediatorVtable[22] = (void*)Mediator_GetString0;      // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetString2;      // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetString1;      // +0x60
    g_LoginMediatorVtable[54] = (void*)Mediator_GetArg7HighByteFloor; // +0xd8
    g_LoginMediatorVtable[55] = (void*)Mediator_MapSelectionName;     // +0xdc
    g_LoginMediatorVtable[59] = (void*)Mediator_ConsumeSelectionContext; // +0xec
    g_LoginMediatorVtable[61] = (void*)Mediator_GetProfilePathComponent; // +0xf4
    g_LoginMediatorVtable[73] = (void*)Mediator_ProvideStartupTriple; // +0x124
    g_LoginMediatorVtable[82] = (void*)Mediator_AttachRuntimeObject; // +0x148
    g_LoginMediatorVtable[89] = (void*)Mediator_ShouldExportA;   // +0x164
    g_LoginMediatorVtable[91] = (void*)Mediator_ShouldExportB;   // +0x16c
    g_LoginMediatorVtable[92] = (void*)Mediator_AttachStartupContext; // +0x170
    g_LoginMediatorVtable[93] = (void*)Mediator_AttachRuntimeObject; // +0x174
    g_LoginMediatorVtable[94] = (void*)Mediator_ConsumeRuntimeDescriptor; // +0x178

    ResetMediatorObjectState();
}

static void InitializeLauncherObjectStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::memset(&g_LauncherObjectBuildState, 0, sizeof(g_LauncherObjectBuildState));
    std::memset(g_LauncherObjectVtable, 0, sizeof(g_LauncherObjectVtable));
    std::memset(g_LauncherObjectSubVtable5C, 0, sizeof(g_LauncherObjectSubVtable5C));
    std::memset(g_LauncherObjectSubVtable60, 0, sizeof(g_LauncherObjectSubVtable60));
    std::memset(g_LauncherObjectSubVtable98, 0, sizeof(g_LauncherObjectSubVtable98));
    g_LauncherObjectVtable[0] = (void*)LauncherObject_Release;        // 0x4319a0
    g_LauncherObjectVtable[1] = (void*)LauncherObject_Slot1_431CE0;   // 0x431ce0
    g_LauncherObjectVtable[2] = (void*)LauncherObject_Slot2_4325D0;   // 0x4325d0
    g_LauncherObjectVtable[3] = (void*)LauncherObject_Slot3_436000;   // 0x436000
    g_LauncherObjectVtable[4] = (void*)LauncherObject_Slot4_42F7C0;   // 0x42f7c0
    g_LauncherObjectSubVtable5C[1] = (void*)LauncherObject_Subobject5C_Slot1; // base +0x5c helper slot
    g_LauncherObjectSubVtable60[0] = (void*)LauncherObject_Subobject60_Slot0; // base +0x60 helper slot 0x4147b0
    g_LauncherObjectSubVtable60[1] = (void*)LauncherObject_Subobject60_Slot1; // base +0x60 helper slot 0x4147c0
    g_LauncherObjectSubVtable98[0] = (void*)LauncherObject_Subobject98_Slot0; // derived +0x98 helper slot
    g_LauncherObjectSubVtable98[1] = (void*)LauncherObject_Subobject98_Slot1; // derived +0x98 helper slot
}

static void DiagnosticInitializeBinderScaffold(void** outMediatorPtr) {
    InitializeMediatorStub();

    std::memset(&g_DiagnosticMediatorResolver, 0, sizeof(g_DiagnosticMediatorResolver));
    std::memset(&g_DiagnosticBinderRegistry, 0, sizeof(g_DiagnosticBinderRegistry));
    std::memset(&g_DiagnosticBinderWrapper, 0, sizeof(g_DiagnosticBinderWrapper));

    g_DiagnosticMediatorResolver.serviceName = g_MediatorName;
    g_DiagnosticMediatorResolver.resolvedObject = &g_LoginMediatorStub;

    g_DiagnosticBinderRegistry.resolverList = &g_DiagnosticMediatorResolver;

    g_DiagnosticBinderWrapper.serviceName = g_MediatorName;
    g_DiagnosticBinderWrapper.mode = 0;
    g_DiagnosticBinderWrapper.outSlot = outMediatorPtr;
    g_DiagnosticBinderWrapper.registry = &g_DiagnosticBinderRegistry;
}

static DiagnosticMediatorResolverNode* DiagnosticLookupResolverNode(
    DiagnosticBinderRegistry* registry,
    const char* serviceName) {
    if (!registry || !serviceName) return NULL;

    for (DiagnosticMediatorResolverNode* node = registry->resolverList; node; node = node->next) {
        Log(
            "DIAGNOSTIC: registry node %p service='%s' object=%p next=%p",
            node,
            node->serviceName ? node->serviceName : "<null>",
            node->resolvedObject,
            node->next);

        if (node->serviceName && std::strcmp(node->serviceName, serviceName) == 0) {
            return node;
        }
    }

    return NULL;
}

static bool DiagnosticResolveBinderWrapper(DiagnosticBinderWrapper* wrapper) {
    if (!wrapper || !wrapper->outSlot || !wrapper->registry) return false;

    Log(
        "DIAGNOSTIC: binder wrapper lookup(service='%s', mode=%u, outSlot=%p)",
        wrapper->serviceName ? wrapper->serviceName : "<null>",
        (unsigned)wrapper->mode,
        wrapper->outSlot);
    Log(
        "DIAGNOSTIC: binder registry=%p resolverList(registry+0x18)=%p",
        wrapper->registry,
        wrapper->registry->resolverList);

    DiagnosticMediatorResolverNode* node =
        DiagnosticLookupResolverNode(wrapper->registry, wrapper->serviceName);
    wrapper->lastResolvedNode = node;
    if (!node) {
        Log("DIAGNOSTIC: binder lookup failed for '%s'", wrapper->serviceName ? wrapper->serviceName : "<null>");
        return false;
    }

    *wrapper->outSlot = node->resolvedObject;
    Log(
        "DIAGNOSTIC: binder resolved '%s' via node %p -> wrote %p to slot %p",
        wrapper->serviceName,
        node,
        node->resolvedObject,
        wrapper->outSlot);
    return true;
}

static MinimalLauncherObjectStub* DiagnosticBuildLauncherObjectLike40A380() {
    InitializeLauncherObjectStub();

    if (g_LauncherObjectBuildState.currentObject) {
        Log(
            "DIAGNOSTIC: replacing prior launcher object scaffold generation=%u ptr=%p",
            (unsigned)g_LauncherObjectBuildState.buildGeneration,
            g_LauncherObjectBuildState.currentObject);
        DiagnosticFreeLauncherObjectInternals(g_LauncherObjectBuildState.currentObject);
        std::free(g_LauncherObjectBuildState.currentObject);
        g_LauncherObjectBuildState.currentObject = NULL;
    }

    MinimalLauncherObjectStub* object =
        static_cast<MinimalLauncherObjectStub*>(std::malloc(sizeof(MinimalLauncherObjectStub)));
    if (!object) {
        Log("DIAGNOSTIC: failed to allocate launcher object scaffold (size=0x%zx)", sizeof(MinimalLauncherObjectStub));
        return NULL;
    }

    std::memset(object, 0, sizeof(*object));
    object->vtable = g_LauncherObjectVtable;
    object->field04 = 0; // 0x40a380 passes ctor arg 0
    object->field08 = NULL;
    object->subVtable5C = g_LauncherObjectSubVtable5C;
    object->field60 = g_LauncherObjectSubVtable60;
    object->field7C = NULL;
    object->field84 = 0;
    object->field88 = 0;
    object->field90 = 0;
    object->field94 = 0;
    object->field98 = g_LauncherObjectSubVtable98;

    DiagnosticIntrusiveListHead* list80 =
        static_cast<DiagnosticIntrusiveListHead*>(std::malloc(sizeof(DiagnosticIntrusiveListHead)));
    if (!list80) {
        Log("DIAGNOSTIC: failed to allocate launcher object +0x80 list head");
        std::free(object);
        return NULL;
    }
    InitializeDiagnosticIntrusiveListHead(list80);
    object->list80 = list80;

    DiagnosticIntrusiveListHeadSmall* list8C =
        static_cast<DiagnosticIntrusiveListHeadSmall*>(std::malloc(sizeof(DiagnosticIntrusiveListHeadSmall)));
    if (!list8C) {
        Log("DIAGNOSTIC: failed to allocate launcher object +0x8c list head");
        std::free(list80);
        std::free(object);
        return NULL;
    }
    InitializeDiagnosticIntrusiveListHeadSmall(list8C);
    object->list8C = list8C;

    ++g_LauncherObjectBuildState.buildGeneration;
    g_LauncherObjectBuildState.currentObject = object;

    Log(
        "DIAGNOSTIC: built launcher object scaffold like 0x40a380/0x431c30 ptr=%p size=0x%zx generation=%u",
        object,
        sizeof(MinimalLauncherObjectStub),
        (unsigned)g_LauncherObjectBuildState.buildGeneration);
    Log(
        "DIAGNOSTIC: launcher object scaffold notes: field04=0 field08=NULL +0x80/+0x8c intrusive heads allocated +0x5c/+0x60/+0x98 seeded to faithful placeholders, vtable slots 0-4 now mapped to logging probes with original function indices");
    LogPointerWords("LauncherObject self", object, 8);
    LogPointerWords("LauncherObject +0x80 list", object->list80, 4);
    LogPointerWords("LauncherObject +0x8c list", object->list8C, 4);

    return object;
}

static void DiagnosticRegisterLauncherObjectWithMediator(void* mediatorPtr, void* launcherObjectPtr) {
    if (!launcherObjectPtr || !mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[2]) {
        Log("DIAGNOSTIC: mediator register slot unavailable for launcher object handoff");
        return;
    }

    typedef int (__thiscall *RegisterEngineFn)(void*, void*);
    RegisterEngineFn fn = (RegisterEngineFn)vtable[2];
    int result = fn(mediatorPtr, launcherObjectPtr);
    Log(
        "DIAGNOSTIC: mediator +0x08 register launcher object(%p) -> %d",
        launcherObjectPtr,
        result);
}

void DiagnosticInstallMediatorStub(void** outMediatorPtr) {
    InitializeMediatorStub();
    if (outMediatorPtr) {
        *outMediatorPtr = &g_LoginMediatorStub;
    }
    Log("DIAGNOSTIC: using MinimalLoginMediatorStub for arg6 (%p)", &g_LoginMediatorStub);
}

void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr) {
    DiagnosticInitializeBinderScaffold(outMediatorPtr);

    Log(
        "DIAGNOSTIC: binder scaffold prepared wrapper=%p registry=%p resolver=%p targetSlot=%p",
        &g_DiagnosticBinderWrapper,
        &g_DiagnosticBinderRegistry,
        &g_DiagnosticMediatorResolver,
        outMediatorPtr);

    if (!DiagnosticResolveBinderWrapper(&g_DiagnosticBinderWrapper)) {
        Log("DIAGNOSTIC: binder scaffold failed to materialize arg6");
        return;
    }

    Log("DIAGNOSTIC: binder scaffold materialized arg6 as %p", outMediatorPtr ? *outMediatorPtr : NULL);
}

void DiagnosticConfigureMediatorSelection(uint32_t highByteFloor, const char* mappedSelectionName) {
    g_MediatorSelectionHighByteFloor = highByteFloor;
    g_MediatorMappedSelectionName =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : g_MediatorStringC;

    Log(
        "DIAGNOSTIC: mediator selection configured highByteFloor=%u mappedSelection='%s'",
        (unsigned)g_MediatorSelectionHighByteFloor,
        g_MediatorMappedSelectionName);
}

void DiagnosticConfigureMediatorProfileName(const char* profileName) {
    g_MediatorProfileName =
        (profileName && profileName[0]) ? profileName : g_MediatorStringA;

    Log("DIAGNOSTIC: mediator profile/session name configured as '%s'", g_MediatorProfileName);
}

void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr) {
    if (!mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[7] || !vtable[9]) {
        Log("DIAGNOSTIC: mediator nopatch slots unavailable");
        return;
    }

    const uint32_t parsedNoPatchValue = 0x3dcccccd; // diagnostic placeholder for parsed "0.1"
    const uint32_t clientVersionValue = 0x3dcccccd; // original source value still unresolved

    typedef void (__thiscall *SetValueFn)(void*, void*);
    SetValueFn setValue1 = (SetValueFn)vtable[7];
    SetValueFn setValue2 = (SetValueFn)vtable[9];

    setValue1(mediatorPtr, (void*)&parsedNoPatchValue);
    Log("DIAGNOSTIC: applied default nopatch mediator +0x1c with placeholder 0x%08x", parsedNoPatchValue);

    setValue2(mediatorPtr, (void*)&clientVersionValue);
    Log("DIAGNOSTIC: applied default nopatch mediator +0x24 with placeholder 0x%08x", clientVersionValue);
}

void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr) {
    MinimalLauncherObjectStub* object = DiagnosticBuildLauncherObjectLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }

    Log(
        "DIAGNOSTIC: using launcher object scaffold for arg5 (%p, size=0x%zx)",
        object,
        sizeof(MinimalLauncherObjectStub));

    if (mediatorPtr && object) {
        Log("DIAGNOSTIC: mirroring original handoff: registering arg5 through arg6 before InitClientDLL");
        DiagnosticRegisterLauncherObjectWithMediator(mediatorPtr, object);
    }
}

static void LogCurrentDisplayMode(const char* prefix) {
    DEVMODEA mode = {};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
        Log("%s: EnumDisplaySettingsA failed", prefix);
        return;
    }

    Log(
        "%s: %lux%lu %lu-bpp @%luHz",
        prefix,
        (unsigned long)mode.dmPelsWidth,
        (unsigned long)mode.dmPelsHeight,
        (unsigned long)mode.dmBitsPerPel,
        (unsigned long)mode.dmDisplayFrequency);
}

static void UpsertWindowTraceEntry(
    HWND hwnd,
    LONG style,
    LONG exStyle,
    const RECT& rect,
    BOOL visible,
    BOOL iconic,
    const char* className,
    const char* title) {

    int index = -1;
    for (int i = 0; i < g_WindowTraceEntryCount; ++i) {
        if (g_WindowTraceEntries[i].hwnd == hwnd) {
            index = i;
            break;
        }
    }

    const bool isNew = (index < 0);
    WindowTraceEntry previous = {};
    if (!isNew) previous = g_WindowTraceEntries[index];

    const bool changed = isNew ||
        previous.style != style ||
        previous.exStyle != exStyle ||
        previous.rect.left != rect.left ||
        previous.rect.top != rect.top ||
        previous.rect.right != rect.right ||
        previous.rect.bottom != rect.bottom ||
        previous.visible != visible ||
        previous.iconic != iconic;

    if (changed) {
        Log(
            "WindowTrace hwnd=%p visible=%d iconic=%d class='%s' title='%s' style=0x%08lx exStyle=0x%08lx rect=(%ld,%ld)-(%ld,%ld)",
            hwnd,
            visible ? 1 : 0,
            iconic ? 1 : 0,
            className,
            title,
            (unsigned long)style,
            (unsigned long)exStyle,
            (long)rect.left,
            (long)rect.top,
            (long)rect.right,
            (long)rect.bottom);
    }

    if (isNew) {
        if (g_WindowTraceEntryCount >= (int)(sizeof(g_WindowTraceEntries) / sizeof(g_WindowTraceEntries[0]))) {
            return;
        }
        index = g_WindowTraceEntryCount++;
    }

    g_WindowTraceEntries[index].hwnd = hwnd;
    g_WindowTraceEntries[index].style = style;
    g_WindowTraceEntries[index].exStyle = exStyle;
    g_WindowTraceEntries[index].rect = rect;
    g_WindowTraceEntries[index].visible = visible;
    g_WindowTraceEntries[index].iconic = iconic;
}

static BOOL CALLBACK WindowTraceEnumProc(HWND hwnd, LPARAM lParam) {
    int* count = reinterpret_cast<int*>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != g_MainProcessId) return TRUE;

    ++(*count);

    char className[256] = {0};
    char title[256] = {0};
    GetClassNameA(hwnd, className, sizeof(className));
    GetWindowTextA(hwnd, title, sizeof(title));

    RECT rect = {};
    GetWindowRect(hwnd, &rect);

    UpsertWindowTraceEntry(
        hwnd,
        GetWindowLongA(hwnd, GWL_STYLE),
        GetWindowLongA(hwnd, GWL_EXSTYLE),
        rect,
        IsWindowVisible(hwnd),
        IsIconic(hwnd),
        className,
        title);

    return TRUE;
}

static DWORD WINAPI WindowTraceThreadProc(LPVOID) {
    DWORD lastWidth = 0;
    DWORD lastHeight = 0;
    DWORD lastBpp = 0;
    DWORD lastHz = 0;

    Log("WindowTrace: started for pid %lu", (unsigned long)g_MainProcessId);
    LogCurrentDisplayMode("WindowTrace display mode");

    while (InterlockedCompareExchange(&g_WindowTraceRunning, 0, 0) != 0) {
        DEVMODEA mode = {};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
            if (mode.dmPelsWidth != lastWidth ||
                mode.dmPelsHeight != lastHeight ||
                mode.dmBitsPerPel != lastBpp ||
                mode.dmDisplayFrequency != lastHz) {
                lastWidth = mode.dmPelsWidth;
                lastHeight = mode.dmPelsHeight;
                lastBpp = mode.dmBitsPerPel;
                lastHz = mode.dmDisplayFrequency;
                LogCurrentDisplayMode("WindowTrace display mode");
            }
        }

        int count = 0;
        EnumWindows(WindowTraceEnumProc, reinterpret_cast<LPARAM>(&count));
        if (count != g_LastWindowTraceCount) {
            g_LastWindowTraceCount = count;
            Log("WindowTrace top-level window count: %d", count);
        }

        Sleep(250);
    }

    Log("WindowTrace: stopped");
    return 0;
}

void DiagnosticStartWindowTrace() {
    if (g_hWindowTraceThread) return;

    g_MainProcessId = GetCurrentProcessId();
    InterlockedExchange(&g_WindowTraceRunning, 1);
    g_hWindowTraceThread = CreateThread(NULL, 0, WindowTraceThreadProc, NULL, 0, NULL);
    if (!g_hWindowTraceThread) {
        InterlockedExchange(&g_WindowTraceRunning, 0);
        Log("WindowTrace: CreateThread failed (%lu)", (unsigned long)GetLastError());
    }
}

void DiagnosticStopWindowTrace() {
    if (!g_hWindowTraceThread) return;
    InterlockedExchange(&g_WindowTraceRunning, 0);
    WaitForSingleObject(g_hWindowTraceThread, 1000);
    CloseHandle(g_hWindowTraceThread);
    g_hWindowTraceThread = NULL;
}
