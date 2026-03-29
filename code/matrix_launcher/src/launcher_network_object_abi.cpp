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
    void** vtable;               // +0x00 derived primary vtable after 0x431c30 completes
    uint32_t field04;            // +0x04 ctor arg / queue-thread count seed from 0x4366f0
    void* field08;               // +0x08 queue-thread pointer array (NULL on the current ctorFlags=0 path)
    LauncherObjectQueue queue0C; // +0x0c..+0x33 base queue state from 0x436610/0x436340
    LauncherObjectQueue queue34; // +0x34..+0x5b second base queue from 0x436610/0x436340
    void** subVtable5C;          // +0x5c base wait/event helper vtable (`0x4b3e20` final ctor state)
    LauncherObjectLockHelper helper60; // +0x60..+0x7b helper vtable + CRITICAL_SECTION
    HANDLE field7C;              // +0x7c CreateEventA(NULL,0,0,0)
    void* list80;                // +0x80 allocated 0x24 endpoint-tree sentinel head
    uint32_t field84;            // +0x84 zeroed in derived ctor
    uint32_t reserved88;         // +0x88 remains zero on the current ctorFlags=0 path
    void* list8C;                // +0x8c allocated 0x18 context-tree sentinel head
    uint32_t field90;            // +0x90 zeroed in derived ctor
    uint32_t reserved94;         // +0x94 remains zero on the current ctorFlags=0 path
    LauncherObjectLockHelper helper98; // +0x98..+0xb3 derived helper root + CRITICAL_SECTION
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
static_assert(sizeof(LauncherObjectAbiShell) == 0xb4, "launcher object ABI shell size must match original allocation");
static_assert(sizeof(LauncherObjectListHead24) == 0x24, "list80 ABI head size mismatch");
static_assert(sizeof(LauncherObjectListHead18) == 0x18, "list8C ABI head size mismatch");

static void SyncLauncherObjectShellState(LauncherObjectAbiShell* self);
// UNANCHORED: launcher-owned accessor for the single current arg5 sidecar binding.
static mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& LauncherObjectEngineBinding();
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

// UNANCHORED: launcher-owned accessor for the single current arg5 sidecar binding.
static mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& LauncherObjectEngineBinding() {
    static mxo::liblttcp::CLTThreadPerClientTCPEngineBinding binding;
    return binding;
}

// UNANCHORED: replacement arg5 owner/binding cleanup helper.
static void ResetLauncherObjectEngineSidecar(LauncherObjectAbiShell* owner) {
    mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& binding = LauncherObjectEngineBinding();
    if (owner && binding.Owner() != owner) {
        return;
    }

    binding.Reset(DiagnosticEnsureMediatorModel());
}

// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* GetOrCreateLauncherObjectEngineSidecar(
    LauncherObjectAbiShell* owner) {
    if (!owner) return NULL;

    mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& binding = LauncherObjectEngineBinding();
    if (binding.Owner() != owner) {
        if (!binding.Bind(owner, DiagnosticEnsureMediatorModel())) {
            spdlog::warn("launcher arg5 ABI shell failed to bind CLTThreadPerClientTCPEngine sidecar for {}", fmt::ptr(owner));
            return NULL;
        }
    }

    if (binding.Engine()) {
        binding.Engine()->AttachExternalQueuePair(
            &owner->queue0C,
            &owner->queue34,
            &owner->helper60.crit,
            owner->field7C);
        binding.Engine()->SetAttachedLauncherObjectStateSyncScaffold(
            owner,
            &SyncLauncherObjectShellStateTrampoline);
    }

    return binding.Engine();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::liblttcp::CLTThreadPerClientTCPEngine* LauncherNetworkEngineFromAbiShell(void* ownerPtr) {
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

    mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& binding = LauncherObjectEngineBinding();
    const bool hasMonitoredPorts = binding.HasMonitoredPorts();
    const bool hasWorkerThreads = binding.HasWorkerThreads();

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
    if (self->helper60.vtable) {
        DeleteCriticalSection(&self->helper60.crit);
        self->helper60.vtable = NULL;
    }
    if (self->helper98.vtable) {
        DeleteCriticalSection(&self->helper98.crit);
        self->helper98.vtable = NULL;
    }
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
    std::free(self);
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

// UNANCHORED: helper-to-owner backpointer used by the current arg5 embedded helper surfaces.
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
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            engine->PumpLauncherConnectionBridgeFromArg5HelperScaffold();
        }
    }
    return 0u;
}


// UNANCHORED: compile-time launcher arg5 primary vtable table replacing the old mutable seed step.
static void** LauncherObjectPrimaryVtable() {
    static void* const kPrimaryVtable[13] = {
        (void*)LauncherObject_Release,                 // 0x4319a0
        (void*)LauncherObject_MonitorPort,             // 0x431ce0
        (void*)LauncherObject_UDPMonitorPort,          // 0x4325d0
        (void*)LauncherObject_MonitorEphemeralUDPPort, // 0x436000
        (void*)LauncherObject_Slot4_42F7C0,            // 0x42f7c0
        (void*)LauncherObject_UnmonitorPort,           // 0x431840
        (void*)LauncherObject_Connect,                 // 0x4328a0
        (void*)LauncherObject_Close,                   // 0x42f970
        (void*)LauncherObject_SendBuffer,              // 0x42fbd0
        (void*)LauncherObject_Slot9_42FD10,            // 0x42fd10
        (void*)LauncherObject_Slot10_443810,          // 0x443810
        (void*)LauncherObject_Slot11_431670,          // 0x431670
        (void*)LauncherObject_CleanupConnection,      // 0x4316a0
    };
    return const_cast<void**>(kPrimaryVtable);
}

// UNANCHORED: compile-time launcher arg5 helper table for the final `+0x5c` ctor state.
static void** LauncherObjectSubVtable5C() {
    static void* const kSubVtable5C[2] = {
        (void*)LauncherObject_Subobject5C_Slot0, // 0x435f90
        (void*)LauncherObject_Subobject5C_Slot1, // 0x435fa0
    };
    return const_cast<void**>(kSubVtable5C);
}

// UNANCHORED: compile-time launcher arg5 helper table for the final `+0x60` ctor state.
static void** LauncherObjectSubVtable60() {
    static void* const kSubVtable60[2] = {
        (void*)LauncherObject_Subobject60_Slot0, // 0x4147b0
        (void*)LauncherObject_LockHelper_Slot1,  // 0x4147c0
    };
    return const_cast<void**>(kSubVtable60);
}

// UNANCHORED: compile-time launcher arg5 helper table for the final `+0x98` ctor state.
static void** LauncherObjectSubVtable98() {
    static void* const kSubVtable98[2] = {
        (void*)LauncherObject_LockHelper_Slot0, // 0x4147b0
        (void*)LauncherObject_LockHelper_Slot1, // 0x4147c0
    };
    return const_cast<void**>(kSubVtable98);
}

// UNANCHORED: bounded source mirror of base ctor `0x4366f0` for the current ctorFlags=0 path.
static bool InitializeLauncherNetworkEngineAbiShellBaseCtorLike4366F0(LauncherObjectAbiShell* object) {
    if (!object) {
        return false;
    }

    object->field04 = 0; // 0x40a380 passes ctor arg 0 into 0x431c30 -> 0x4366f0.
    object->field08 = NULL;
    object->subVtable5C = LauncherObjectSubVtable5C();
    object->helper60.vtable = LauncherObjectSubVtable60();
    InitializeCriticalSection(&object->helper60.crit);
    object->field7C = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!object->field7C) {
        spdlog::warn("launcher arg5 ABI shell failed to create +0x7c event ({})", (unsigned long)GetLastError());
        return false;
    }

    if (!mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Init(&object->queue0C, 0) ||
        !mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Init(&object->queue34, 0)) {
        return false;
    }

    return true;
}

// UNANCHORED: bounded source mirror of derived ctor `0x431c30` after the base ctor completes.
static bool InitializeLauncherNetworkEngineAbiShellDerivedCtorLike431C30(LauncherObjectAbiShell* object) {
    if (!object) {
        return false;
    }

    object->vtable = LauncherObjectPrimaryVtable();

    LauncherObjectListHead24* list80 =
        static_cast<LauncherObjectListHead24*>(std::malloc(sizeof(LauncherObjectListHead24)));
    if (!list80) {
        spdlog::warn("launcher arg5 ABI shell failed to allocate +0x80 list head");
        return false;
    }
    InitializeLauncherObjectListHead24(list80);
    object->list80 = list80;
    object->field84 = 0;

    LauncherObjectListHead18* list8C =
        static_cast<LauncherObjectListHead18*>(std::malloc(sizeof(LauncherObjectListHead18)));
    if (!list8C) {
        spdlog::warn("launcher arg5 ABI shell failed to allocate +0x8c list head");
        return false;
    }
    InitializeLauncherObjectListHead18(list8C);
    object->list8C = list8C;
    object->field90 = 0;

    object->helper98.vtable = LauncherObjectSubVtable98();
    InitializeCriticalSection(&object->helper98.crit);
    return true;
}

// UNANCHORED: replacement launcher builder mirroring launcher.exe:0x40a380 -> 0x431c30.
static LauncherObjectAbiShell* CreateLauncherNetworkEngineAbiShellLike40A380() {
    LauncherObjectAbiShell* object =
        static_cast<LauncherObjectAbiShell*>(std::malloc(sizeof(LauncherObjectAbiShell)));
    if (!object) {
        spdlog::warn(
            "launcher arg5 ABI shell failed to allocate object (size=0x{:x})",
            static_cast<size_t>(sizeof(LauncherObjectAbiShell)));
        return NULL;
    }

    std::memset(object, 0, sizeof(*object));
    if (!InitializeLauncherNetworkEngineAbiShellBaseCtorLike4366F0(object) ||
        !InitializeLauncherNetworkEngineAbiShellDerivedCtorLike431C30(object)) {
        FreeLauncherObjectAbiShellInternals(object);
        std::free(object);
        return NULL;
    }

    return object;
}

// UNANCHORED: replacement helper that mirrors the mediator +0x08 handoff used after launcher.exe:0x40a380.
static int RegisterLauncherNetworkEngineWithMediator(void* mediatorPtr, void* launcherObjectPtr) {
    if (!launcherObjectPtr || !mediatorPtr) return 0;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[2]) {
        return 0;
    }

    typedef int (__thiscall *RegisterEngineFn)(void*, void*);
    RegisterEngineFn fn = (RegisterEngineFn)vtable[2];
    return fn(mediatorPtr, launcherObjectPtr);
}

// UNANCHORED: replacement helper that mirrors the mediator +0x0c clear handoff used during
// launcher.exe cleanup after the arg5 release.
static void ClearLauncherNetworkEngineFromMediator(void* mediatorPtr) {
    if (!mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[3]) {
        return;
    }

    typedef void (__thiscall *ClearEngineFn)(void*);
    ClearEngineFn fn = (ClearEngineFn)vtable[3];
    fn(mediatorPtr);
}

// UNANCHORED: public replacement-launcher entrypoint that installs the arg5 ABI shell.
void LauncherInstallNetworkEngineAbiShell(void** outLauncherObjectPtr, void* mediatorPtr) {
    if (outLauncherObjectPtr && *outLauncherObjectPtr) {
        LauncherReleaseNetworkEngineAbiShell(outLauncherObjectPtr, mediatorPtr);
    }
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = NULL;
    }

    LauncherObjectAbiShell* object = CreateLauncherNetworkEngineAbiShellLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }

    if (mediatorPtr && object) {
        const int registerResult = RegisterLauncherNetworkEngineWithMediator(mediatorPtr, object);
        if (registerResult < 1) {
            spdlog::warn(
                "launcher arg5 ABI shell mediator registration returned {} for {}",
                registerResult,
                fmt::ptr(object));
        }
    }
}

// UNANCHORED: public replacement-launcher entrypoint that releases the arg5 ABI shell.
void LauncherReleaseNetworkEngineAbiShell(void** launcherObjectPtr, void* mediatorPtr) {
    if (!launcherObjectPtr || !*launcherObjectPtr) {
        return;
    }

    LauncherObjectAbiShell* object = static_cast<LauncherObjectAbiShell*>(*launcherObjectPtr);
    typedef int (__thiscall *ReleaseFn)(LauncherObjectAbiShell*, uint32_t);
    ReleaseFn release = object->vtable ? reinterpret_cast<ReleaseFn>(object->vtable[0]) : nullptr;
    if (release) {
        release(object, 1u);
    }
    *launcherObjectPtr = NULL;

    ClearLauncherNetworkEngineFromMediator(mediatorPtr);
}

