#include "launcher_network_object_abi.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

using LauncherObjectQueuePair = mxo::liblttcp::CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610;
using LauncherObjectListHead24 = mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_EndpointTreeHead24;
using LauncherObjectListHead18 = mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_ContextTreeHead18;
using LauncherObjectAbiAttachment = mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LauncherAbiAttachment;

struct LauncherObjectLockHelper {
    void** vtable;          // +0x00
    CRITICAL_SECTION crit;  // +0x04..+0x1b
};

struct LauncherObjectAbiShell {
    void** vtable;                    // +0x00 derived primary vtable after 0x431c30 completes
    uint32_t field04;                 // +0x04 ctor arg / queue-thread count seed from 0x4366f0
    void* field08;                    // +0x08 queue-thread pointer array (NULL on the current ctorFlags=0 path)
    LauncherObjectQueuePair queuePair0C; // +0x0c..+0x5b inline queue-pair object from 0x436610
                                          // ABI-wrapper ownership note: this subobject is not
                                          // just decorative layout padding. Raw client.dll code
                                          // may touch these bytes/subobjects directly, so the
                                          // wrapper-owned shell remains the authoritative queue
                                          // storage surface whenever a shell is attached.
    void** subVtable5C;               // +0x5c base wait/event helper vtable (`0x4b3e20` final ctor state)
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
        mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768 probe;
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
        sizeof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768),
        sizeof(LauncherObjectAbiShell));
    spdlog::info(
        "launcher arg5 native-vptr compile-time layout field04=0x{:x} field08=0x{:x} queuePair0C=0x{:x} queuePair0C.queue28=0x{:x} wait5C=0x{:x} lock60=0x{:x} event=0x{:x} list80=0x{:x} count84=0x{:x} list8C=0x{:x} count90=0x{:x} cleanup98=0x{:x} lockHelperSize=0x{:x}",
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, field04),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, field08),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queuePair0C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queuePair0C.queue28),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, waitHelper5C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queueLockHelper60),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, queueSignalEvent7C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, endpointTreeHead80),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, endpointCount84),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, contextTreeHead8C),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, contextCount90),
        offsetof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LayoutMirror, cleanupLockHelper98),
        sizeof(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_LockHelperScaffold));
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
static mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding& LauncherObjectEngineBinding();
// UNANCHORED: registered arg5 sidecar resolver using the single current launcher binding.
// This is the non-synthetic path for later arg5 dispatch after launcher startup has already
// performed the one real create/store/register handoff at `0x40a380`.
static mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* ResolveLauncherObjectEngineSidecar(LauncherObjectAbiShell* owner);

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
static mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding& LauncherObjectEngineBinding() {
    static mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding binding;
    return binding;
}

// UNANCHORED: replacement arg5 owner/binding cleanup helper.
static void ResetLauncherObjectEngineSidecar(LauncherObjectAbiShell* owner) {
    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding& binding = LauncherObjectEngineBinding();
    if (owner && binding.Owner() != owner) {
        return;
    }

    if (binding.Engine()) {
        binding.Engine()->DetachLauncherAbiSurfaceScaffold();
    }

    // Current fidelity correction from the latest queue/worker/context RE pass:
    // - original engine evidence stays connection-centric (`0x431ff0`, `0x4316a0`, `0x436820`,
    //   `0x436b10`, `0x449d40`)
    // - launcher startup `0x40a380` owns the arg5 -> arg6 handoff
    // - there is still no positive Ghidra evidence that mediator bind/reset belongs to the engine
    //   object itself or to arbitrary later arg5 virtual dispatch
    // So keep mediator-side install/clear in the outer launcher/login seam instead of doing lazy
    // bind/reset here inside the arg5 sidecar accessor.
    binding.Reset();
}

// UNANCHORED: registered arg5 sidecar resolver using the single current launcher binding.
static mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* ResolveLauncherObjectEngineSidecar(
    LauncherObjectAbiShell* owner) {
    if (!owner) return NULL;

    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding& binding = LauncherObjectEngineBinding();
    if (binding.Owner() != owner || !binding.Engine()) {
        return NULL;
    }

    return binding.Engine();
}

// UNANCHORED: launcher startup bind helper for the single current arg5 object.
// This is only for the real `0x40a380` create/store/register handoff path; later arg5 dispatch
// must resolve the already-registered binding rather than lazily creating/rebinding it.
mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* LauncherNetworkEngineFromAbiShell(void* ownerPtr) {
    LauncherObjectAbiShell* owner = static_cast<LauncherObjectAbiShell*>(ownerPtr);
    if (!owner) {
        return NULL;
    }

    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768Binding& binding = LauncherObjectEngineBinding();
    if (binding.Owner() != owner) {
        if (!binding.Bind(owner)) {
            spdlog::warn(
                "launcher arg5 ABI shell failed to bind CLTThreadPerClientTCPEngine_0x4b2768 sidecar for {}",
                fmt::ptr(owner));
            return NULL;
        }
    }

    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine = binding.Engine();
    if (!engine) {
        return NULL;
    }

    LauncherObjectAbiAttachment attachment = {};
    attachment.launcherObjectShell = owner;
    engine->AttachLauncherAbiSurfaceScaffold(attachment);

    return engine;
}

// UNANCHORED: replacement arg5 internal teardown helper.
static void FreeLauncherObjectAbiShellInternals(LauncherObjectAbiShell* self) {
    if (!self) return;
    ResetLauncherObjectEngineSidecar(self);
    std::memset(&self->queuePair0C, 0, sizeof(self->queuePair0C));
    self->field7C = NULL;
    self->helper60.vtable = NULL;
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
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
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
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
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
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
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
    return ResolveLauncherObjectEngineSidecar(self)->Slot4_42F7C0(arg1);
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
static uint32_t __thiscall LauncherObject_UnmonitorPort(
    LauncherObjectAbiShell* self,
    void* port,
    void** outOwnerContext,
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
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
    return ResolveLauncherObjectEngineSidecar(self)->Connect(contextKey);
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
static uint32_t __thiscall LauncherObject_Close(
    LauncherObjectAbiShell* self,
    void* contextKey,
    uint32_t graceful) {
    return ResolveLauncherObjectEngineSidecar(self)->Close(contextKey, graceful != 0u);
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
static uint32_t __thiscall LauncherObject_SendBuffer(
    LauncherObjectAbiShell* self,
    void* buffer,
    void* byteCount,
    void* contextKey,
    void* completionContext) {
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
    return engine->SendBuffer(
        buffer,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(byteCount)),
        contextKey,
        completionContext);
}

// anchor: launcher.exe:0x42fd10
// vtable: launcher.exe:0x004b2768 slot +0x24
static uint32_t __thiscall LauncherObject_SendBufferWithEndpoint(
    LauncherObjectAbiShell* self,
    void* buffer,
    void* byteCount,
    void* remoteEndpoint,
    void* contextKey,
    void* ownershipMode) {
    mxo::liblttcp::ILTTCPEngine* engine = ResolveLauncherObjectEngineSidecar(self);
    return engine->SendBufferWithEndpoint(
        buffer,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(byteCount)),
        static_cast<mxo::liblttcp::LTTCPEndpointKey_0x44b070*>(remoteEndpoint),
        contextKey,
        ownershipMode);
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
static uint32_t __thiscall LauncherObject_Slot10_443810(
    LauncherObjectAbiShell* self,
    void* arg1) {
    return ResolveLauncherObjectEngineSidecar(self)->Slot10_443810(arg1);
}

// UNANCHORED: queue-lock helper wrappers for arg5 +0x60 now forward into the real engine-owned
// storage rather than using shell-local critical-section state.
static uint32_t __thiscall LauncherObject_QueueLockHelper_Slot0(void* self);
static uint32_t __thiscall LauncherObject_QueueLockHelper_Slot1(void* self);
// UNANCHORED: cleanup-lock helper wrappers for arg5 +0x98 now also forward into the real
// engine-owned cleanup lock. Keep the shell-local CRITICAL_SECTION initialized as inert ABI
// backing in case client/runtime code still probes the embedded bytes directly.
static uint32_t __thiscall LauncherObject_CleanupLockHelper_Slot0(void* self);
static uint32_t __thiscall LauncherObject_CleanupLockHelper_Slot1(void* self);

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
static uint32_t __thiscall LauncherObject_Slot11_431670(
    LauncherObjectAbiShell* self,
    void* arg1,
    uint32_t* out0,
    uint32_t* out1) {
    return ResolveLauncherObjectEngineSidecar(self)->Slot11_431670(arg1, out0, out1);
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
static uint32_t __thiscall LauncherObject_CleanupConnection(
    LauncherObjectAbiShell* self,
    void* contextKey) {
    return ResolveLauncherObjectEngineSidecar(self)->CleanupConnection(contextKey);
}

static LauncherObjectAbiShell* LauncherObjectFromSubobject(void* self, size_t offset) {
    return self ? reinterpret_cast<LauncherObjectAbiShell*>(static_cast<unsigned char*>(self) - offset) : NULL;
}

// UNANCHORED: queue-lock helper enter for arg5 +0x60.
static uint32_t __thiscall LauncherObject_QueueLockHelper_Slot0(void* self) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x60))) {
        return engine->EnterQueueLockHelper();
    }
    return 1u;
}

// UNANCHORED: queue-lock helper leave for arg5 +0x60.
static uint32_t __thiscall LauncherObject_QueueLockHelper_Slot1(void* self) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x60))) {
        return engine->LeaveQueueLockHelper();
    }
    return 1u;
}

// UNANCHORED: cleanup-lock helper enter for arg5 +0x98.
static uint32_t __thiscall LauncherObject_CleanupLockHelper_Slot0(void* self) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x98))) {
        return engine->EnterCleanupLockHelper();
    }
    return 1u;
}

// UNANCHORED: cleanup-lock helper leave for arg5 +0x98.
static uint32_t __thiscall LauncherObject_CleanupLockHelper_Slot1(void* self) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x98))) {
        return engine->LeaveCleanupLockHelper();
    }
    return 1u;
}

// anchor: launcher.exe:0x435f90
// vtable: launcher.exe:arg5+0x5c helper slot +0x00
static uint32_t __thiscall LauncherObject_Subobject5C_Slot0(void* self) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x5c))) {
        return engine->SignalQueueEventHelper();
    }
    return 1u;
}

// anchor: launcher.exe:0x435fa0
// vtable: launcher.exe:arg5+0x5c helper slot +0x04
static uint32_t __thiscall LauncherObject_Subobject5C_Slot1(void* self, int reason) {
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine =
            ResolveLauncherObjectEngineSidecar(LauncherObjectFromSubobject(self, 0x5c))) {
        return engine->WaitQueueEventHelper(reason);
    }
    return 1u;
}


// UNANCHORED: compile-time launcher arg5 primary vtable table replacing the old mutable seed step.
// 2026-03-28 ABI-boundary note:
// - do not replace this wholesale with the native GCC C++ vtable from
//   `mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768`
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
        (void*)LauncherObject_SendBufferWithEndpoint,  // 0x42fd10
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
        (void*)LauncherObject_QueueLockHelper_Slot0, // 0x4147b0 wrapper -> engine queue lock
        (void*)LauncherObject_QueueLockHelper_Slot1, // 0x4147c0 wrapper -> engine queue lock
    };
    return const_cast<void**>(kSubVtable60);
}

// UNANCHORED: compile-time launcher arg5 helper table for the final `+0x98` ctor state.
static void** LauncherObjectSubVtable98() {
    static void* const kSubVtable98[2] = {
        (void*)LauncherObject_CleanupLockHelper_Slot0, // 0x4147b0 shell-local cleanup lock
        (void*)LauncherObject_CleanupLockHelper_Slot1, // 0x4147c0 shell-local cleanup lock
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
    object->field7C = NULL;
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

    // Tree-family pruning step:
    // - the shell no longer pre-allocates fake +0x80/+0x8c tree heads
    // - unlike queuePair0C, these later tree/count bytes are only published from the real engine
    //   for raw direct field readers via AttachLauncherAbiSurfaceScaffold/
    //   SyncAttachedLauncherObjectStateScaffold
    object->list80 = NULL;
    object->field84 = 0;
    object->list8C = NULL;
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
    std::memset(&object->queuePair0C, 0, sizeof(object->queuePair0C));
    object->subVtable5C = NULL;
    object->helper60.vtable = NULL;
    std::memset(&object->helper60.crit, 0, sizeof(object->helper60.crit));
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
    ResolveLauncherObjectEngineSidecar(static_cast<LauncherObjectAbiShell*>(launcherObjectPtr))
        ->RunCompletedOperationQueue(nonBlocking);
}

// UNANCHORED: public replacement-launcher entrypoint for the original 0x40a380 allocation + ctor
// step only. Keep the later store-to-`0x4d6304` and arg6 wrapper `+0x08` call in
// `CLauncher::InitializeThreadPerClientTCPEngine()` so the anchored launcher method owns that
// sequence directly.
void* LauncherCreateNetworkEngineAbiShell() {
    LogLauncherObjectPrimaryDispatchConfigOnce();

    LauncherObjectAbiShell* object = CreateLauncherNetworkEngineAbiShellLike40A380();
    if (object) {
        LauncherLogNetworkEngineAbiShellDispatchState(object, "post-create");
    }
    return object;
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

