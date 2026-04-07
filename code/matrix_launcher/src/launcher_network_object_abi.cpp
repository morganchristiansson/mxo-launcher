#include "launcher_network_object_abi.h"
#include "launcher_mediator_abi_shared.h"
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

using LauncherObjectQueue = mxo::liblttcp::CLTThreadPerClientTCPEngine_Queue;
using LauncherObjectListHead24 = mxo::liblttcp::CLTThreadPerClientTCPEngine_EndpointTreeHead24;
using LauncherObjectListHead18 = mxo::liblttcp::CLTThreadPerClientTCPEngine_ContextTreeHead18;
using LauncherObjectAbiAttachment = mxo::liblttcp::CLTThreadPerClientTCPEngine_LauncherAbiAttachment;

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
    uint32_t reserved88;         // +0x88 no ctor write recovered yet; current source zero-fills it
    void* list8C;                // +0x8c allocated 0x18 context-tree sentinel head
    uint32_t field90;            // +0x90 zeroed in derived ctor
    uint32_t reserved94;         // +0x94 no ctor write recovered yet; current source zero-fills it
    LauncherObjectLockHelper helper98; // +0x98..+0xb3 derived helper root + CRITICAL_SECTION
};

static_assert(sizeof(LauncherObjectLockHelper) == 0x1c, "launcher lock helper size mismatch");
static_assert(sizeof(LauncherObjectAbiShell) == 0xb4, "launcher object ABI shell size must match original allocation");

enum class LauncherObjectPrimaryDispatchMode {
    kWrapperTable,
    kMinGWNativeVptr,
};

#if defined(__MINGW32__) && !defined(MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR)
#define MXO_USE_MINGW_NATIVE_ARG5_VPTR 1
#else
#define MXO_USE_MINGW_NATIVE_ARG5_VPTR 0
#endif

static const char* LauncherObjectPrimaryDispatchModeName(LauncherObjectPrimaryDispatchMode mode) {
    switch (mode) {
        case LauncherObjectPrimaryDispatchMode::kWrapperTable:
            return "wrapper-table";
        case LauncherObjectPrimaryDispatchMode::kMinGWNativeVptr:
            return "mingw-native-vptr";
    }
    return "unknown";
}

static LauncherObjectPrimaryDispatchMode LauncherObjectPrimaryDispatchModeForBuild() {
#if MXO_USE_MINGW_NATIVE_ARG5_VPTR
    return LauncherObjectPrimaryDispatchMode::kMinGWNativeVptr;
#else
    return LauncherObjectPrimaryDispatchMode::kWrapperTable;
#endif
}

static void** LauncherObjectNativePrimaryAddressPointForBuild() {
#if MXO_USE_MINGW_NATIVE_ARG5_VPTR
    static void** const kNativeAddressPoint = []() -> void** {
        mxo::liblttcp::CLTThreadPerClientTCPEngine probe;
        return *reinterpret_cast<void***>(&probe);
    }();
    return kNativeAddressPoint;
#else
    return nullptr;
#endif
}

static void LogLauncherObjectPrimaryDispatchConfigOnce() {
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    const LauncherObjectPrimaryDispatchMode mode = LauncherObjectPrimaryDispatchModeForBuild();
    spdlog::info(
        "launcher arg5 primary dispatch active={} mingwNativeDefault={} optOutDefined={}",
        LauncherObjectPrimaryDispatchModeName(mode),
#if defined(__MINGW32__)
        1,
#else
        0,
#endif
#if defined(MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR)
        1
#else
        0
#endif
    );

#if MXO_USE_MINGW_NATIVE_ARG5_VPTR
    void** const nativeAddressPoint = LauncherObjectNativePrimaryAddressPointForBuild();
    const uintptr_t* const vtableWords = reinterpret_cast<const uintptr_t*>(nativeAddressPoint);
    spdlog::info(
        "launcher arg5 native-vptr addressPoint={} rawBaseGuess={} offsetToTop=0x{:08x} typeinfo={} nativeSize=0x{:x} shellSize=0x{:x}",
        fmt::ptr(nativeAddressPoint),
        fmt::ptr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(nativeAddressPoint) - 8u)),
        static_cast<uint32_t>(vtableWords[-2]),
        fmt::ptr(reinterpret_cast<void*>(vtableWords[-1])),
        sizeof(mxo::liblttcp::CLTThreadPerClientTCPEngine),
        sizeof(LauncherObjectAbiShell));
    spdlog::info(
        "launcher arg5 native-vptr compile-time layout field04=0x{:x} field08=0x{:x} queue0C=0x{:x} queue34=0x{:x} wait5C=0x{:x} lock60=0x{:x} event=0x{:x} list80=0x{:x} count84=0x{:x} list8C=0x{:x} count90=0x{:x} cleanup98=0x{:x} lockHelperSize=0x{:x}",
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, field04),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, field08),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, queue0C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, queue34),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, waitHelper5C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, queueLockHelper60),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, queueSignalEvent7C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, endpointTreeHead80),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, endpointCount84),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, contextTreeHead8C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, contextCount90),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LayoutMirror, cleanupLockHelper98),
        sizeof(mxo::liblttcp::CLTThreadPerClientTCPEngine_LockHelperScaffold));
    spdlog::info(
        "launcher arg5 native-vptr note: live shell keeps wrapper-owned helper vtables at +0x5c/+0x60/+0x98 for direct helper dispatch"
    );
#endif
}

static void** LauncherObjectSelectedPrimaryVtable() {
#if MXO_USE_MINGW_NATIVE_ARG5_VPTR
    return LauncherObjectNativePrimaryAddressPointForBuild();
#else
    return nullptr;
#endif
}

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

    if (binding.Engine()) {
        binding.Engine()->DetachLauncherAbiSurfaceScaffold();
    }

    // Current fidelity correction from the latest queue/worker/context RE pass:
    // - original engine evidence stays connection-centric (`0x431ff0`, `0x4316a0`, `0x436820`,
    //   `0x436b10`, `0x449d40`)
    // - there is still no positive Ghidra evidence that mediator bind/reset belongs to the engine
    //   object itself
    // So keep mediator-side connection reset in the outer launcher/login seam, not in liblttcp binding.
    if (mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel()) {
        mediator->ResetLauncherConnectionsScaffold();
    }
    binding.Reset();
}

// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* GetOrCreateLauncherObjectEngineSidecar(
    LauncherObjectAbiShell* owner) {
    if (!owner) return NULL;

    mxo::liblttcp::CLTThreadPerClientTCPEngineBinding& binding = LauncherObjectEngineBinding();
    if (binding.Owner() != owner) {
        mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
        if (binding.Engine() && mediator) {
            mediator->ResetLauncherConnectionsScaffold();
        }
        if (!binding.Bind(owner)) {
            spdlog::warn("launcher arg5 ABI shell failed to bind CLTThreadPerClientTCPEngine sidecar for {}", fmt::ptr(owner));
            return NULL;
        }
        if (binding.Engine() && mediator) {
            mediator->BindLauncherConnectionsScaffold(binding.Engine());
        }
    }

    if (binding.Engine()) {
        void* priorList80 = owner->list80;
        void* priorList8C = owner->list8C;

        LauncherObjectAbiAttachment attachment = {};
        attachment.field04CtorFlags = &owner->field04;
        attachment.field08QueueThreadArray = &owner->field08;
        attachment.queue0C = &owner->queue0C;
        attachment.queue34 = &owner->queue34;
        attachment.queueLock = &owner->helper60.crit;
        attachment.queueSignalEvent = owner->field7C;
        attachment.cleanupLock = &owner->helper98.crit;
        attachment.list80EndpointTreeHead = &owner->list80;
        attachment.field84EndpointCount = &owner->field84;
        attachment.list8CContextTreeHead = &owner->list8C;
        attachment.field90ContextCount = &owner->field90;
        binding.Engine()->AttachLauncherAbiSurfaceScaffold(attachment);

        if (priorList80 && priorList80 != owner->list80) {
            std::free(priorList80);
        }
        if (priorList8C && priorList8C != owner->list8C) {
            std::free(priorList8C);
        }
    }

    return binding.Engine();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::liblttcp::CLTThreadPerClientTCPEngine* LauncherNetworkEngineFromAbiShell(void* ownerPtr) {
    return GetOrCreateLauncherObjectEngineSidecar(static_cast<LauncherObjectAbiShell*>(ownerPtr));
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
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->UDPMonitorPort(
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port)),
        contextKey,
        ipv4NetworkOrder);
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b2768 slot +0x0c
static uint32_t __thiscall LauncherObject_MonitorEphemeralUDPPort(
    LauncherObjectAbiShell* self,
    void* outBoundPortHostOrder,
    void* contextKey,
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    uint16_t boundPortHostOrder = 0;
    return engine->MonitorEphemeralUDPPort(
        outBoundPortHostOrder ? static_cast<uint16_t*>(outBoundPortHostOrder) : &boundPortHostOrder,
        contextKey,
        ipv4NetworkOrder);
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
    void** outOwnerContext,
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    if (!engine) {
        return 0;
    }

    return engine->UnmonitorPort(
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(port)),
        outOwnerContext,
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
    mxo::liblttcp::ILTTCPEngine* engine = GetOrCreateLauncherObjectEngineSidecar(self);
    return engine ? engine->CleanupConnection(contextKey) : 0u;
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

static LauncherObjectAbiShell* LauncherObjectShellFromLockHelper(void* helperSelf, size_t* outHelperOffset) {
    if (!helperSelf) {
        return NULL;
    }

    LauncherObjectAbiShell* owner60 = LauncherObjectShellFromHelper(helperSelf, 0x60);
    if (owner60 && (&owner60->helper60 == helperSelf)) {
        if (outHelperOffset) {
            *outHelperOffset = 0x60;
        }
        return owner60;
    }

    LauncherObjectAbiShell* owner98 = LauncherObjectShellFromHelper(helperSelf, 0x98);
    if (owner98 && (&owner98->helper98 == helperSelf)) {
        if (outHelperOffset) {
            *outHelperOffset = 0x98;
        }
        return owner98;
    }

    return NULL;
}

// UNANCHORED: shared lock-helper enter for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot0(void* self) {
    size_t helperOffset = 0;
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromLockHelper(self, &helperOffset)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            return (helperOffset == 0x98)
                ? engine->EnterCleanupLockHelper()
                : engine->EnterQueueLockHelper();
        }
    }

    if (CRITICAL_SECTION* crit = LauncherObjectCritFromHelper(self)) {
        EnterCriticalSection(crit);
    }
    return 0u;
}

// UNANCHORED: shared lock-helper leave for the current arg5 +0x60/+0x98 helper family.
static uint32_t __thiscall LauncherObject_LockHelper_Slot1(void* self) {
    size_t helperOffset = 0;
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromLockHelper(self, &helperOffset)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            return (helperOffset == 0x98)
                ? engine->LeaveCleanupLockHelper()
                : engine->LeaveQueueLockHelper();
        }
    }

    if (CRITICAL_SECTION* crit = LauncherObjectCritFromHelper(self)) {
        LeaveCriticalSection(crit);
    }
    return 0u;
}

// anchor: launcher.exe:0x435f90
// vtable: launcher.exe:arg5+0x5c helper slot +0x00
static uint32_t __thiscall LauncherObject_Subobject5C_Slot0(void* self) {
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromHelper(self, 0x5c)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            return engine->SignalQueueEventHelper();
        }
    }

    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    return (eventHandle && SetEvent(eventHandle)) ? 0u : 1u;
}

// anchor: launcher.exe:0x435fa0
// vtable: launcher.exe:arg5+0x5c helper slot +0x04
static uint32_t __thiscall LauncherObject_Subobject5C_Slot1(void* self, int reason) {
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromHelper(self, 0x5c)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            return engine->WaitQueueEventHelper(reason);
        }
    }

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

// anchor family: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x60)
// Faithfulness note:
// - the original helper body itself is just the `EnterCriticalSection` wrapper from `0x4147b0`
// - the extra launcher-side no-worker pump remains a current shell-owned side effect layered on
//   top of that pure enter-helper body for the arg5 `+0x60` slot-0 path only
static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self) {
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromHelper(self, 0x60)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            const uint32_t result = engine->EnterQueueLockHelper();
            engine->PumpLauncherConnectionsFromArg5HelperScaffold();
            return result;
        }
    }

    LauncherObject_LockHelper_Slot0(self);
    if (LauncherObjectAbiShell* owner = LauncherObjectShellFromHelper(self, 0x60)) {
        if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(owner)) {
            engine->PumpLauncherConnectionsFromArg5HelperScaffold();
        }
    }
    return 0u;
}


// UNANCHORED: compile-time launcher arg5 primary vtable table replacing the old mutable seed step.
// 2026-03-28 ABI-boundary note:
// - do not replace this wholesale with the native GCC C++ vtable from
//   `mxo::liblttcp::CLTThreadPerClientTCPEngine`
// - the launcher-visible arg5 object is still the `0xb4` shell carrying client-consumed fields and
//   embedded helper subobjects at `+0x5c/+0x60/+0x98`
// - the current native sidecar object is smaller and uses a separate GNU C++ object/vptr model
// - only per-slot thinning via explicit shell->sidecar trampolines is a plausible future reduction
// Canonical doc:
// - ../../docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
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
// Important faithfulness note from current static RE:
// - original `0x40a380` allocates with `malloc(0xb4)` and does NOT zero the full object first
// - recovered ctors currently do not write `+0x88` or `+0x94`
// - current source now seeds only cleanup-sensitive / currently-read fields before ctor-shaped init,
//   and leaves any stronger meaning for `+0x88/+0x94` deliberately unresolved
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

    if (void** nativeVptr = LauncherObjectSelectedPrimaryVtable()) {
        object->vtable = nativeVptr;
    } else {
        object->vtable = LauncherObjectPrimaryVtable();
    }

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

// UNANCHORED: bounded source pre-ctor initialization for safe partial cleanup on the current
// malloc-backed create path. Unlike the older whole-object memset, this only seeds the fields that
// current source cleanup or the current ctor-shaped steps may read before full initialization.
static void InitializeLauncherNetworkEngineAbiShellPreCtorState(LauncherObjectAbiShell* object) {
    if (!object) {
        return;
    }

    object->vtable = NULL;
    object->field04 = 0;
    object->field08 = NULL;
    std::memset(&object->queue0C, 0, sizeof(object->queue0C));
    std::memset(&object->queue34, 0, sizeof(object->queue34));
    object->subVtable5C = NULL;
    object->helper60.vtable = NULL;
    object->field7C = NULL;
    object->list80 = NULL;
    object->field84 = 0;
    object->reserved88 = 0;
    object->list8C = NULL;
    object->field90 = 0;
    object->reserved94 = 0;
    object->helper98.vtable = NULL;
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

    // Current bounded stability choice:
    // - original `0x40a380` reaches `malloc(0xb4)` and then ctor writes only known fields
    // - source no longer zero-fills the entire shell up front
    // - instead it seeds only the fields that partial cleanup / current ctor-shaped init may read
    InitializeLauncherNetworkEngineAbiShellPreCtorState(object);
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
    if (!mediatorPtr) return 1;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[2]) {
        return 1;
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

void LauncherLogNetworkEngineAbiShellDispatchState(void* launcherObjectPtr, const char* phase) {
    LogLauncherObjectPrimaryDispatchConfigOnce();

    const LauncherObjectPrimaryDispatchMode mode = LauncherObjectPrimaryDispatchModeForBuild();
    void* storedVptr = launcherObjectPtr ? *reinterpret_cast<void**>(launcherObjectPtr) : nullptr;
    const bool vptrMatchesNativeAddressPoint =
        storedVptr != nullptr && storedVptr == LauncherObjectNativePrimaryAddressPointForBuild();
    spdlog::info(
        "launcher arg5 dispatch state phase='{}' object={} storedVptr={} active={} nativeAddressPointMatch={}",
        (phase && phase[0]) ? phase : "<unspecified>",
        fmt::ptr(launcherObjectPtr),
        fmt::ptr(storedVptr),
        LauncherObjectPrimaryDispatchModeName(mode),
        vptrMatchesNativeAddressPoint ? 1 : 0);
}

// UNANCHORED: launcher-owned poll helper for pre-client auth/selection sequencing.
void LauncherPumpNetworkEngineAbiShell(void* launcherObjectPtr, bool nonBlocking) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = LauncherNetworkEngineFromAbiShell(launcherObjectPtr)) {
        engine->PumpLauncherConnectionsFromArg5HelperScaffold();
        engine->RunCompletedOperationQueue(nonBlocking);
        engine->PumpLauncherConnectionsFromArg5HelperScaffold();
    }
}

// UNANCHORED: public replacement-launcher entrypoint that installs the arg5 ABI shell.
int LauncherInstallNetworkEngineAbiShell(void** outLauncherObjectPtr, void* mediatorPtr) {
    if (outLauncherObjectPtr && *outLauncherObjectPtr) {
        LauncherReleaseNetworkEngineAbiShell(outLauncherObjectPtr, mediatorPtr);
    }
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = NULL;
    }

    LogLauncherObjectPrimaryDispatchConfigOnce();

    LauncherObjectAbiShell* object = CreateLauncherNetworkEngineAbiShellLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }
    if (object) {
        LauncherLogNetworkEngineAbiShellDispatchState(object, "post-create");
    }

    const int registerResult = RegisterLauncherNetworkEngineWithMediator(mediatorPtr, object);
    if (registerResult >= 1) {
        spdlog::warn(
            "launcher arg5 ABI shell mediator registration returned {} for {}",
            registerResult,
            fmt::ptr(object));
    }
    return registerResult;
}

// UNANCHORED: public replacement-launcher entrypoint that releases the arg5 ABI shell.
void LauncherReleaseNetworkEngineAbiShell(void** launcherObjectPtr, void* mediatorPtr) {
    if (!launcherObjectPtr || !*launcherObjectPtr) {
        return;
    }

    LauncherObjectAbiShell* object = static_cast<LauncherObjectAbiShell*>(*launcherObjectPtr);
    LauncherLogNetworkEngineAbiShellDispatchState(object, "pre-release");

    if (LauncherObjectPrimaryDispatchModeForBuild() == LauncherObjectPrimaryDispatchMode::kMinGWNativeVptr) {
        spdlog::info(
            "launcher arg5 native-vptr mode using manual shell release for {} instead of object->vtable[0]",
            fmt::ptr(object));
        LauncherObject_Release(object, 1u);
    } else {
        typedef int (__thiscall *ReleaseFn)(LauncherObjectAbiShell*, uint32_t);
        ReleaseFn release = object->vtable ? reinterpret_cast<ReleaseFn>(object->vtable[0]) : nullptr;
        if (release) {
            release(object, 1u);
        }
    }
    *launcherObjectPtr = NULL;

    ClearLauncherNetworkEngineFromMediator(mediatorPtr);
}

