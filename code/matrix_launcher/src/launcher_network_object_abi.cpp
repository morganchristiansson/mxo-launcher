#include "diagnostics.h"
#include "launcher_mediator_abi_shared.h"
#include "launcher_network_object_abi.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

using LauncherObjectQueue = mxo::liblttcp::CLTThreadPerClientTCPEngine_Queue;

struct LauncherObjectLockHelper {
    void** vtable;          // +0x00
    CRITICAL_SECTION crit;  // +0x04..+0x1b
};

struct LauncherObjectAbiShell {
    void** vtable;              // +0x00
    uint32_t field04;           // +0x04 ctor arg in original (0 from 0x40a380)
    void* field08;              // +0x08 pointer array in base ctor (NULL when field04==0)
    LauncherObjectQueue queue0C; // +0x0c..+0x33 base queue state from 0x436610/0x436340
    LauncherObjectQueue queue34; // +0x34..+0x5b second base queue from 0x436610/0x436340
    void** subVtable5C;         // +0x5c base wait/event helper vtable
    LauncherObjectLockHelper helper60; // +0x60..+0x7b vtable + CRITICAL_SECTION from 0x4add70/0x4147b0/0x4147c0
    HANDLE field7C;             // +0x7c CreateEventA(NULL,0,0,0)
    void* list80;               // +0x80 allocated 0x24 list head
    uint32_t field84;           // +0x84 zeroed in derived ctor
    uint32_t field88;           // +0x88 left generic for now
    void* list8C;               // +0x8c allocated 0x18 list head
    uint32_t field90;           // +0x90 zeroed in derived ctor
    uint32_t field94;           // +0x94 left generic for now
    LauncherObjectLockHelper helper98; // +0x98..+0xb3 derived lock helper root + CRITICAL_SECTION
};

struct LauncherObjectListHead24 {
    unsigned char colorOrFlag;  // +0x00 RB-tree/list sentinel byte
    unsigned char padding[3];
    void* root;                 // +0x04 root node pointer (NULL in ctor)
    void* first;                // +0x08 first/list-next sentinel link (self in ctor)
    void* last;                 // +0x0c last/list-prev sentinel link (self in ctor)
    unsigned char keyAndPayload[0x14];
};

struct LauncherObjectListHead18 {
    unsigned char colorOrFlag;  // +0x00 RB-tree/list sentinel byte
    unsigned char padding[3];
    void* root;                 // +0x04 root node pointer (NULL in ctor)
    void* first;                // +0x08 first/list-next sentinel link (self in ctor)
    void* last;                 // +0x0c last/list-prev sentinel link (self in ctor)
    unsigned char keyAndPayload[0x8];
};

static_assert(sizeof(LauncherObjectLockHelper) == 0x1c, "launcher lock helper size mismatch");
static_assert(sizeof(LauncherObjectAbiShell) == 0xb4, "launcher object scaffold size must match original allocation");
static_assert(sizeof(LauncherObjectListHead24) == 0x24, "list80 scaffold size mismatch");
static_assert(sizeof(LauncherObjectListHead18) == 0x18, "list8C scaffold size mismatch");

static LauncherObjectAbiShell* g_CurrentLauncherObject = NULL;
static mxo::liblttcp::CLTThreadPerClientTCPEngineBinding* g_LauncherObjectEngineBinding = NULL;
static void* g_LauncherObjectVtable[13] = {0};
static void* g_LauncherObjectSubVtable5C[2] = {0};
static void* g_LauncherObjectSubVtable60[2] = {0};
static void* g_LauncherObjectSubVtable98[2] = {0};
static void SyncLauncherObjectShellState(LauncherObjectAbiShell* self);
// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* GetOrCreateLauncherObjectEngineSidecar(LauncherObjectAbiShell* owner);

static void InitializeLauncherObjectListHead24(LauncherObjectListHead24* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->colorOrFlag = 0;
    head->root = NULL;
    head->first = head;
    head->last = head;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void InitializeLauncherObjectListHead18(LauncherObjectListHead18* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->colorOrFlag = 0;
    head->root = NULL;
    head->first = head;
    head->last = head;
}


// UNANCHORED: callback trampoline registered on the liblttcp sidecar so engine-owned paths can
// request owner-visible arg5 state refresh without pulling raw launcher-object layout knowledge
// back into loginmediator.cpp.
static void SyncLauncherObjectShellStateTrampoline(void* ownerPtr) {
    SyncLauncherObjectShellState(static_cast<LauncherObjectAbiShell*>(ownerPtr));
}

// UNANCHORED: replacement arg5 owner/binding cleanup helper.
static void ResetLauncherObjectEngineSidecar(LauncherObjectAbiShell* owner) {
    if (owner && g_LauncherObjectEngineBinding && g_LauncherObjectEngineBinding->Owner() != owner) {
        return;
    }

    if (g_LauncherObjectEngineBinding) {
        g_LauncherObjectEngineBinding->Reset(DiagnosticEnsureMediatorModel());
    }
    delete g_LauncherObjectEngineBinding;
    g_LauncherObjectEngineBinding = NULL;
}

// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* GetOrCreateLauncherObjectEngineSidecar(
    LauncherObjectAbiShell* owner) {
    if (!owner) return NULL;

    if (!g_LauncherObjectEngineBinding) {
        g_LauncherObjectEngineBinding = new mxo::liblttcp::CLTThreadPerClientTCPEngineBinding();
        if (!g_LauncherObjectEngineBinding) {
            spdlog::warn("launcher arg5 ABI shell failed to allocate CLTThreadPerClientTCPEngine binding for {}", fmt::ptr(owner));
            return NULL;
        }
    }

    if (g_LauncherObjectEngineBinding->Owner() != owner) {
        if (!g_LauncherObjectEngineBinding->Bind(owner, DiagnosticEnsureMediatorModel())) {
            spdlog::warn("launcher arg5 ABI shell failed to bind CLTThreadPerClientTCPEngine sidecar for {}", fmt::ptr(owner));
            return NULL;
        }
    }

    if (g_LauncherObjectEngineBinding->Engine()) {
        g_LauncherObjectEngineBinding->Engine()->AttachExternalQueuePair(
            &owner->queue0C,
            &owner->queue34,
            &owner->helper60.crit,
            owner->field7C);
        g_LauncherObjectEngineBinding->Engine()->SetAttachedLauncherObjectStateSyncScaffold(
            owner,
            &SyncLauncherObjectShellStateTrampoline);
    }

    return g_LauncherObjectEngineBinding->Engine();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetLauncherObjectEngine(void* ownerPtr) {
    return GetOrCreateLauncherObjectEngineSidecar(static_cast<LauncherObjectAbiShell*>(ownerPtr));
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void SetLauncherObjectListHead24Occupancy(LauncherObjectListHead24* head, bool nonEmpty) {
    if (!head) return;
    if (!nonEmpty) {
        InitializeLauncherObjectListHead24(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[4];
    head->last = &head->keyAndPayload[8];
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void SetLauncherObjectListHead18Occupancy(LauncherObjectListHead18* head, bool nonEmpty) {
    if (!head) return;
    if (!nonEmpty) {
        InitializeLauncherObjectListHead18(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[0];
    head->last = &head->keyAndPayload[4];
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void SyncLauncherObjectShellState(LauncherObjectAbiShell* self) {
    if (!self) return;

    const bool hasMonitoredPorts = g_LauncherObjectEngineBinding && g_LauncherObjectEngineBinding->HasMonitoredPorts();
    const bool hasWorkerThreads = g_LauncherObjectEngineBinding && g_LauncherObjectEngineBinding->HasWorkerThreads();

    SetLauncherObjectListHead24Occupancy(
        static_cast<LauncherObjectListHead24*>(self->list80),
        hasMonitoredPorts);
    SetLauncherObjectListHead18Occupancy(
        static_cast<LauncherObjectListHead18*>(self->list8C),
        hasWorkerThreads);
}

// UNANCHORED: replacement arg5 internal teardown helper.
static void FreeLauncherObjectAbiShellInternals(LauncherObjectAbiShell* self) {
    if (!self) return;
    ResetLauncherObjectEngineSidecar(self);
    mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Free(&self->queue0C);
    mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Free(&self->queue34);
    if (self->field7C) {
        CloseHandle(self->field7C);
        self->field7C = NULL;
    }
    DeleteCriticalSection(&self->helper60.crit);
    DeleteCriticalSection(&self->helper98.crit);
    if (self->list80) {
        std::free(self->list80);
        self->list80 = NULL;
    }
    if (self->list8C) {
        std::free(self->list8C);
        self->list8C = NULL;
    }
}

// anchor: launcher.exe:0x4319a0
// vtable: launcher.exe:0x004b2768 slot +0x00
static int __thiscall LauncherObject_Release(LauncherObjectAbiShell* self, uint32_t flags) {
    (void)flags;
    FreeLauncherObjectAbiShellInternals(self);
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
static uint32_t __thiscall LauncherObject_MonitorPort(
    LauncherObjectAbiShell* self,
    void* port,
    void* ownerContext,
    void* reservedArg3) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->MonitorPort(
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port)),
        ownerContext,
        reservedArg3);
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
static uint32_t __thiscall LauncherObject_UDPMonitorPort(
    LauncherObjectAbiShell* self,
    void* port,
    void* contextKey,
    void* ownerContext) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->UDPMonitorPort(
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port)),
        contextKey,
        ownerContext);
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b2768 slot +0x0c
static uint32_t __thiscall LauncherObject_MonitorEphemeralUDPPort(
    LauncherObjectAbiShell* self,
    void* outBoundPortHostOrder,
    void* contextKey,
    void* ownerContext) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    uint16_t boundPortHostOrder = 0;
    return engine->MonitorEphemeralUDPPort(
        outBoundPortHostOrder ? static_cast<uint16_t*>(outBoundPortHostOrder) : &boundPortHostOrder,
        contextKey,
        ownerContext);
}

// anchor: launcher.exe:0x42f7c0
// vtable: launcher.exe:0x004b2768 slot +0x10
static uint32_t __thiscall LauncherObject_Slot4_42F7C0(
    LauncherObjectAbiShell* self,
    void* arg1) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    return engine ? engine->Slot4_42F7C0(arg1) : 0u;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
static uint32_t __thiscall LauncherObject_UnmonitorPort(
    LauncherObjectAbiShell* self,
    void* port,
    uint32_t* outSocketHandle,
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->UnmonitorPort(
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port)),
        outSocketHandle,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ipv4NetworkOrder)));
}

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
static uint32_t __thiscall LauncherObject_Connect(
    LauncherObjectAbiShell* self,
    void* contextKey) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->Connect(contextKey);
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
static uint32_t __thiscall LauncherObject_Close(
    LauncherObjectAbiShell* self,
    void* contextKey,
    uint32_t graceful) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->Close(contextKey, graceful != 0u);
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
static uint32_t __thiscall LauncherObject_SendBuffer(
    LauncherObjectAbiShell* self,
    void* contextKey,
    void* buffer,
    void* byteCount,
    void* completionContext) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->SendBuffer(
        contextKey,
        buffer,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(byteCount)),
        completionContext);
}

// anchor: launcher.exe:0x42fd10
// vtable: launcher.exe:0x004b2768 slot +0x24
static uint32_t __thiscall LauncherObject_Slot9_42FD10(
    LauncherObjectAbiShell* self,
    void* arg1,
    void* arg2,
    void* arg3,
    void* arg4,
    void* arg5) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    return engine ? engine->Slot9_42FD10(arg1, arg2, arg3, arg4, arg5) : 0u;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
static uint32_t __thiscall LauncherObject_Slot10_443810(
    LauncherObjectAbiShell* self,
    void* arg1) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    return engine ? engine->Slot10_443810(arg1) : 0u;
}

// UNANCHORED: shared lock-helper enter for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot0(void* self);
// UNANCHORED: shared lock-helper leave for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot1(void* self);
// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x60)
static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self);

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
static uint32_t __thiscall LauncherObject_Slot11_431670(
    LauncherObjectAbiShell* self,
    void* arg1,
    uint32_t* out0,
    uint32_t* out1) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    return engine ? engine->Slot11_431670(arg1, out0, out1) : 0u;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
static uint32_t __thiscall LauncherObject_CleanupConnection(
    LauncherObjectAbiShell* self,
    void* contextKey) {
    LauncherObject_LockHelper_Slot0(&self->helper98);

    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    const uint32_t result = engine ? engine->CleanupConnection(contextKey) : 0u;

    LauncherObject_LockHelper_Slot1(&self->helper98);
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static CRITICAL_SECTION* LauncherObjectCritFromHelper(void* self) {
    return self ? reinterpret_cast<CRITICAL_SECTION*>(static_cast<unsigned char*>(self) + 4) : NULL;
}

// UNANCHORED: helper-to-owner backpointer used by the current arg5 subobject helper scaffolds.
static LauncherObjectAbiShell* LauncherObjectShellFromHelper(void* helperSelf, size_t helperOffset) {
    return helperSelf
        ? reinterpret_cast<LauncherObjectAbiShell*>(static_cast<unsigned char*>(helperSelf) - helperOffset)
        : NULL;
}

// UNANCHORED: shared lock-helper enter for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot0(void* self) {
    if (CRITICAL_SECTION* crit = LauncherObjectCritFromHelper(self)) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// UNANCHORED: shared lock-helper leave for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot1(void* self) {
    if (CRITICAL_SECTION* crit = LauncherObjectCritFromHelper(self)) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// anchor: launcher.exe:0x435f90
// vtable: launcher.exe:arg5+0x5c helper slot +0x00
static uint32_t __thiscall LauncherObject_Subobject5C_Slot0(void* self) {
    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    return (eventHandle && SetEvent(eventHandle)) ? 0u : 1u;
}

// anchor: launcher.exe:0x435fa0
// vtable: launcher.exe:arg5+0x5c helper slot +0x04
static uint32_t __thiscall LauncherObject_Subobject5C_Slot1(void* self, int reason) {
    void* helper60 = self ? static_cast<unsigned char*>(self) + 4 : NULL;
    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    if (helper60) {
        LauncherObject_LockHelper_Slot1(helper60);
    }

    const DWORD waitResult = eventHandle ? WaitForSingleObject(eventHandle, static_cast<DWORD>(reason)) : WAIT_FAILED;
    if (waitResult == WAIT_OBJECT_0) {
        if (helper60) {
            LauncherObject_Subobject60_Slot0(helper60);
        }
        return 0u;
    }
    if (waitResult == WAIT_TIMEOUT) {
        if (helper60) {
            LauncherObject_Subobject60_Slot0(helper60);
        }
        return 3u;
    }
    return 1u;
}

// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x60)
static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self) {
    LauncherObject_LockHelper_Slot0(self);
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromHelper(self, 0x60)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetLauncherObjectEngine(owner)) {
            engine->PumpLauncherConnectionBridgeFromArg5HelperScaffold();
        }
    }
    return 0u;
}


// UNANCHORED: seeds the replacement arg5 ABI vtables from recovered launcher.exe addresses.
static void InitializeLauncherObjectStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::memset(g_LauncherObjectVtable, 0, sizeof(g_LauncherObjectVtable));
    std::memset(g_LauncherObjectSubVtable5C, 0, sizeof(g_LauncherObjectSubVtable5C));
    std::memset(g_LauncherObjectSubVtable60, 0, sizeof(g_LauncherObjectSubVtable60));
    std::memset(g_LauncherObjectSubVtable98, 0, sizeof(g_LauncherObjectSubVtable98));
    g_LauncherObjectVtable[0] = (void*)LauncherObject_Release;                   // 0x4319a0
    g_LauncherObjectVtable[1] = (void*)LauncherObject_MonitorPort;               // 0x431ce0
    g_LauncherObjectVtable[2] = (void*)LauncherObject_UDPMonitorPort;            // 0x4325d0
    g_LauncherObjectVtable[3] = (void*)LauncherObject_MonitorEphemeralUDPPort;   // 0x436000
    g_LauncherObjectVtable[4] = (void*)LauncherObject_Slot4_42F7C0;              // 0x42f7c0
    g_LauncherObjectVtable[5] = (void*)LauncherObject_UnmonitorPort;             // 0x431840
    g_LauncherObjectVtable[6] = (void*)LauncherObject_Connect;                   // 0x4328a0
    g_LauncherObjectVtable[7] = (void*)LauncherObject_Close;                     // 0x42f970
    g_LauncherObjectVtable[8] = (void*)LauncherObject_SendBuffer;                // 0x42fbd0
    g_LauncherObjectVtable[9] = (void*)LauncherObject_Slot9_42FD10;              // 0x42fd10
    g_LauncherObjectVtable[10] = (void*)LauncherObject_Slot10_443810;            // 0x443810
    g_LauncherObjectVtable[11] = (void*)LauncherObject_Slot11_431670;            // 0x431670
    g_LauncherObjectVtable[12] = (void*)LauncherObject_CleanupConnection;        // 0x4316a0
    g_LauncherObjectSubVtable5C[0] = (void*)LauncherObject_Subobject5C_Slot0; // base +0x5c helper slot 0x435f90
    g_LauncherObjectSubVtable5C[1] = (void*)LauncherObject_Subobject5C_Slot1; // base +0x5c helper slot 0x435fa0
    g_LauncherObjectSubVtable60[0] = (void*)LauncherObject_Subobject60_Slot0; // base +0x60 helper slot 0x4147b0
    g_LauncherObjectSubVtable60[1] = (void*)LauncherObject_LockHelper_Slot1;  // base +0x60 helper slot 0x4147c0
    g_LauncherObjectSubVtable98[0] = (void*)LauncherObject_LockHelper_Slot0;  // derived +0x98 helper slot 0x4147b0
    g_LauncherObjectSubVtable98[1] = (void*)LauncherObject_LockHelper_Slot1;  // derived +0x98 helper slot 0x4147c0
}

// UNANCHORED: replacement launcher builder mirroring launcher.exe:0x40a380 -> 0x431c30.
static LauncherObjectAbiShell* BuildLauncherObjectAbiShellLike40A380() {
    InitializeLauncherObjectStub();

    if (g_CurrentLauncherObject) {
        FreeLauncherObjectAbiShellInternals(g_CurrentLauncherObject);
        std::free(g_CurrentLauncherObject);
        g_CurrentLauncherObject = NULL;
    }

    LauncherObjectAbiShell* object =
        static_cast<LauncherObjectAbiShell*>(std::malloc(sizeof(LauncherObjectAbiShell)));
    if (!object) {
        spdlog::warn(
            "launcher arg5 ABI shell failed to allocate object (size=0x{:x})",
            static_cast<size_t>(sizeof(LauncherObjectAbiShell)));
        return NULL;
    }

    std::memset(object, 0, sizeof(*object));
    object->vtable = g_LauncherObjectVtable;
    object->field04 = 0; // 0x40a380 passes ctor arg 0
    object->field08 = NULL;
    object->subVtable5C = g_LauncherObjectSubVtable5C;
    object->helper60.vtable = g_LauncherObjectSubVtable60;
    InitializeCriticalSection(&object->helper60.crit);
    object->field7C = CreateEventA(NULL, FALSE, FALSE, NULL);
    object->field84 = 0;
    object->field88 = 0;
    object->field90 = 0;
    object->field94 = 0;
    object->helper98.vtable = g_LauncherObjectSubVtable98;
    InitializeCriticalSection(&object->helper98.crit);
    if (!object->field7C) {
        spdlog::warn("launcher arg5 ABI shell failed to create +0x7c event ({})", (unsigned long)GetLastError());
        FreeLauncherObjectAbiShellInternals(object);
        std::free(object);
        return NULL;
    }
    if (!mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Init(&object->queue0C, 0) ||
        !mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Init(&object->queue34, 0)) {
        FreeLauncherObjectAbiShellInternals(object);
        std::free(object);
        return NULL;
    }

    LauncherObjectListHead24* list80 =
        static_cast<LauncherObjectListHead24*>(std::malloc(sizeof(LauncherObjectListHead24)));
    if (!list80) {
        spdlog::warn("launcher arg5 ABI shell failed to allocate +0x80 list head");
        FreeLauncherObjectAbiShellInternals(object);
        std::free(object);
        return NULL;
    }
    InitializeLauncherObjectListHead24(list80);
    object->list80 = list80;

    LauncherObjectListHead18* list8C =
        static_cast<LauncherObjectListHead18*>(std::malloc(sizeof(LauncherObjectListHead18)));
    if (!list8C) {
        spdlog::warn("launcher arg5 ABI shell failed to allocate +0x8c list head");
        object->list80 = list80;
        FreeLauncherObjectAbiShellInternals(object);
        std::free(object);
        return NULL;
    }
    InitializeLauncherObjectListHead18(list8C);
    object->list8C = list8C;

    g_CurrentLauncherObject = object;
    return object;
}

// UNANCHORED: replacement helper that mirrors the mediator +0x08 handoff used after launcher.exe:0x40a380.
static void RegisterLauncherObjectWithMediator(void* mediatorPtr, void* launcherObjectPtr) {
    if (!launcherObjectPtr || !mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[2]) {
        return;
    }

    typedef int (__thiscall *RegisterEngineFn)(void*, void*);
    RegisterEngineFn fn = (RegisterEngineFn)vtable[2];
    (void)fn(mediatorPtr, launcherObjectPtr);
}

// UNANCHORED: public replacement-launcher entrypoint that installs the arg5 scaffold.
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr) {
    LauncherObjectAbiShell* object = BuildLauncherObjectAbiShellLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }

    if (object) {
        GetOrCreateLauncherObjectEngineSidecar(object);
        SyncLauncherObjectShellState(object);
    }

    if (mediatorPtr && object) {
        RegisterLauncherObjectWithMediator(mediatorPtr, object);
    }
}

