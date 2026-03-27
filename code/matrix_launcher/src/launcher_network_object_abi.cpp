#include "diagnostics.h"
#include "launcher_mediator_abi_shared.h"
#include "launcher_network_object_abi.h"
#include "loginmediator.h"
#include "../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void LogLauncherObjectPointerWords(const char* label, const void* ptr, uint32_t wordCount) {
    if (!ptr || !wordCount) {
        spdlog::debug("{}: <null>", label ? label : "PointerWords");
        return;
    }

    const uint32_t* words = static_cast<const uint32_t*>(ptr);
    spdlog::debug("{} @ {} [+0x00]=0x{:08x} [+0x04]=0x{:08x} [+0x08]=0x{:08x} [+0x0c]=0x{:08x}",
        label,
        fmt::ptr(ptr),
        words[0],
        (wordCount > 1) ? words[1] : 0,
        (wordCount > 2) ? words[2] : 0,
        (wordCount > 3) ? words[3] : 0);
    if (wordCount > 4) {
        spdlog::debug("{} @ {} [+0x10]=0x{:08x} [+0x14]=0x{:08x} [+0x18]=0x{:08x} [+0x1c]=0x{:08x}",
            label,
            fmt::ptr(ptr),
            words[4],
            (wordCount > 5) ? words[5] : 0,
            (wordCount > 6) ? words[6] : 0,
            (wordCount > 7) ? words[7] : 0);
    }
}

using DiagnosticLauncherQueue = mxo::liblttcp::CLTThreadPerClientTCPEngine_Queue;

struct DiagnosticLauncherLockHelper {
    void** vtable;          // +0x00
    CRITICAL_SECTION crit;  // +0x04..+0x1b
};

struct MinimalLauncherObjectStub {
    void** vtable;              // +0x00
    uint32_t field04;           // +0x04 ctor arg in original (0 from 0x40a380)
    void* field08;              // +0x08 pointer array in base ctor (NULL when field04==0)
    DiagnosticLauncherQueue queue0C; // +0x0c..+0x33 base queue state from 0x436610/0x436340
    DiagnosticLauncherQueue queue34; // +0x34..+0x5b second base queue from 0x436610/0x436340
    void** subVtable5C;         // +0x5c base wait/event helper vtable
    DiagnosticLauncherLockHelper helper60; // +0x60..+0x7b vtable + CRITICAL_SECTION from 0x4add70/0x4147b0/0x4147c0
    HANDLE field7C;             // +0x7c CreateEventA(NULL,0,0,0)
    void* list80;               // +0x80 allocated 0x24 list head
    uint32_t field84;           // +0x84 zeroed in derived ctor
    uint32_t field88;           // +0x88 left generic for now
    void* list8C;               // +0x8c allocated 0x18 list head
    uint32_t field90;           // +0x90 zeroed in derived ctor
    uint32_t field94;           // +0x94 left generic for now
    DiagnosticLauncherLockHelper helper98; // +0x98..+0xb3 derived lock helper root + CRITICAL_SECTION
};

struct DiagnosticIntrusiveListHead {
    unsigned char colorOrFlag;  // +0x00 RB-tree/list sentinel byte
    unsigned char padding[3];
    void* root;                 // +0x04 root node pointer (NULL in ctor)
    void* first;                // +0x08 first/list-next sentinel link (self in ctor)
    void* last;                 // +0x0c last/list-prev sentinel link (self in ctor)
    unsigned char keyAndPayload[0x14];
};

struct DiagnosticIntrusiveListHeadSmall {
    unsigned char colorOrFlag;  // +0x00 RB-tree/list sentinel byte
    unsigned char padding[3];
    void* root;                 // +0x04 root node pointer (NULL in ctor)
    void* first;                // +0x08 first/list-next sentinel link (self in ctor)
    void* last;                 // +0x0c last/list-prev sentinel link (self in ctor)
    unsigned char keyAndPayload[0x8];
};

struct DiagnosticLauncherObjectBuildState {
    MinimalLauncherObjectStub* currentObject;
    uint32_t buildGeneration;
    uint32_t subobject5CSlot0CallCount;
    uint32_t subobject5CSlot1CallCount;
    uint32_t subobject60Slot0CallCount;
    uint32_t subobject60Slot1CallCount;
    uint32_t subobject98Slot0CallCount;
    uint32_t subobject98Slot1CallCount;
};

static_assert(sizeof(DiagnosticLauncherLockHelper) == 0x1c, "launcher lock helper size mismatch");
static_assert(sizeof(MinimalLauncherObjectStub) == 0xb4, "launcher object scaffold size must match original allocation");
static_assert(sizeof(DiagnosticIntrusiveListHead) == 0x24, "list80 scaffold size mismatch");
static_assert(sizeof(DiagnosticIntrusiveListHeadSmall) == 0x18, "list8C scaffold size mismatch");

static DiagnosticLauncherObjectBuildState g_LauncherObjectBuildState = {};
static mxo::liblttcp::CLTThreadPerClientTCPEngineBinding* g_DiagnosticLttcpBinding = NULL;
static void* g_LauncherObjectVtable[16] = {0};
static void* g_LauncherObjectSubVtable5C[8] = {0};
static void* g_LauncherObjectSubVtable60[8] = {0};
static void* g_LauncherObjectSubVtable98[8] = {0};

static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count);
static void DiagnosticSyncLauncherObjectSidecarState(MinimalLauncherObjectStub* self);
// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetOrCreateLttcpEngine(MinimalLauncherObjectStub* owner);

static void InitializeDiagnosticIntrusiveListHead(DiagnosticIntrusiveListHead* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->colorOrFlag = 0;
    head->root = NULL;
    head->first = head;
    head->last = head;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void InitializeDiagnosticIntrusiveListHeadSmall(DiagnosticIntrusiveListHeadSmall* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->colorOrFlag = 0;
    head->root = NULL;
    head->first = head;
    head->last = head;
}

// UNANCHORED: launcher-side wrapper that now delegates queue storage teardown to the liblttcp scaffold.
static void DiagnosticFreeLauncherQueue(DiagnosticLauncherQueue* queue) {
    mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Free(queue);
}

// UNANCHORED: launcher-side wrapper that now delegates queue initialization to the liblttcp scaffold.
static bool DiagnosticInitializeLauncherQueue(DiagnosticLauncherQueue* queue, uint32_t initialSize) {
    const bool ok = mxo::liblttcp::CLTThreadPerClientTCPEngine::Queue_Init(queue, initialSize);
    if (!ok) {
        spdlog::info("DIAGNOSTIC: failed to initialize launcher queue via liblttcp Queue_Init(initialSize={})", initialSize);
    }
    return ok;
}

// UNANCHORED: callback trampoline registered on the liblttcp sidecar so engine-owned paths can
// request owner-visible arg5 state refresh without pulling raw launcher-object layout knowledge
// back into loginmediator.cpp.
static void DiagnosticLauncherObjectStateSyncTrampoline(void* ownerPtr) {
    DiagnosticSyncLauncherObjectSidecarState(static_cast<MinimalLauncherObjectStub*>(ownerPtr));
}

// UNANCHORED: replacement arg5 owner/binding cleanup helper.
static void DiagnosticClearLttcpBinding(MinimalLauncherObjectStub* owner) {
    if (owner && g_DiagnosticLttcpBinding && g_DiagnosticLttcpBinding->Owner() != owner) {
        return;
    }

    if (g_DiagnosticLttcpBinding) {
        g_DiagnosticLttcpBinding->Reset(DiagnosticEnsureMediatorModel());
    }
    delete g_DiagnosticLttcpBinding;
    g_DiagnosticLttcpBinding = NULL;
}

// UNANCHORED: replacement arg5 sidecar binder into liblttcp-owned engine state.
static mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetOrCreateLttcpEngine(
    MinimalLauncherObjectStub* owner) {
    if (!owner) return NULL;

    if (!g_DiagnosticLttcpBinding) {
        g_DiagnosticLttcpBinding = new mxo::liblttcp::CLTThreadPerClientTCPEngineBinding();
        if (!g_DiagnosticLttcpBinding) {
            spdlog::info("DIAGNOSTIC: failed to allocate CLTThreadPerClientTCPEngine binding for {}", fmt::ptr(owner));
            return NULL;
        }
    }

    if (g_DiagnosticLttcpBinding->Owner() != owner) {
        if (!g_DiagnosticLttcpBinding->Bind(owner, DiagnosticEnsureMediatorModel())) {
            spdlog::info("DIAGNOSTIC: failed to bind CLTThreadPerClientTCPEngine sidecar for {}", fmt::ptr(owner));
            return NULL;
        }
        spdlog::info("DIAGNOSTIC: created CLTThreadPerClientTCPEngine sidecar for launcher object {}", fmt::ptr(owner));
    }

    if (g_DiagnosticLttcpBinding->Engine()) {
        g_DiagnosticLttcpBinding->Engine()->SetAttachedLauncherObjectStateSyncScaffold(
            owner,
            &DiagnosticLauncherObjectStateSyncTrampoline);
    }

    return g_DiagnosticLttcpBinding->Engine();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetLauncherObjectEngine(void* ownerPtr) {
    return DiagnosticGetOrCreateLttcpEngine(static_cast<MinimalLauncherObjectStub*>(ownerPtr));
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void DiagnosticSetListHeadOccupancy(DiagnosticIntrusiveListHead* head, bool nonEmpty) {
    if (!head) return;
    if (!nonEmpty) {
        InitializeDiagnosticIntrusiveListHead(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[4];
    head->last = &head->keyAndPayload[8];
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void DiagnosticSetListHeadOccupancySmall(DiagnosticIntrusiveListHeadSmall* head, bool nonEmpty) {
    if (!head) return;
    if (!nonEmpty) {
        InitializeDiagnosticIntrusiveListHeadSmall(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[0];
    head->last = &head->keyAndPayload[4];
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void DiagnosticSyncLauncherObjectSidecarState(MinimalLauncherObjectStub* self) {
    if (!self) return;

    if (g_DiagnosticLttcpBinding && g_DiagnosticLttcpBinding->Owner() == self && g_DiagnosticLttcpBinding->Engine()) {
        // UNANCHORED scaffold bridge: current liblttcp engine sidecar is separate from the ABI object,
        // so hand the real launcher-visible queue-field addresses and paired helper/event surfaces
        // across explicitly.
        g_DiagnosticLttcpBinding->Engine()->AttachExternalQueuePair(
            &self->queue0C,
            &self->queue34,
            &self->helper60.crit,
            self->field7C);
    }

    const bool hasMonitoredPorts = g_DiagnosticLttcpBinding && g_DiagnosticLttcpBinding->HasMonitoredPorts();
    const bool hasWorkerThreads = g_DiagnosticLttcpBinding && g_DiagnosticLttcpBinding->HasWorkerThreads();

    DiagnosticSetListHeadOccupancy(
        static_cast<DiagnosticIntrusiveListHead*>(self->list80),
        hasMonitoredPorts);
    DiagnosticSetListHeadOccupancySmall(
        static_cast<DiagnosticIntrusiveListHeadSmall*>(self->list8C),
        hasWorkerThreads);
}

// UNANCHORED: replacement arg5 internal teardown helper.
static void DiagnosticFreeLauncherObjectInternals(MinimalLauncherObjectStub* self) {
    if (!self) return;
    DiagnosticClearLttcpBinding(self);
    DiagnosticFreeLauncherQueue(&self->queue0C);
    DiagnosticFreeLauncherQueue(&self->queue34);
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
static int __thiscall LauncherObject_Release(MinimalLauncherObjectStub* self, uint32_t flags) {
    (void)flags;
    DiagnosticFreeLauncherObjectInternals(self);
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
static uint32_t __thiscall LauncherObject_MonitorPort(
    MinimalLauncherObjectStub* self,
    void* port,
    void* ownerContext,
    void* reservedArg3) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
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
    MinimalLauncherObjectStub* self,
    void* port,
    void* contextKey,
    void* ownerContext) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
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
    MinimalLauncherObjectStub* self,
    void* outBoundPortHostOrder,
    void* contextKey,
    void* ownerContext) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
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
    MinimalLauncherObjectStub* self,
    void* arg1) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    return engine ? engine->Slot4_42F7C0(arg1) : 0u;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
static uint32_t __thiscall LauncherObject_UnmonitorPort(
    MinimalLauncherObjectStub* self,
    void* port,
    uint32_t* outSocketHandle,
    void* ipv4NetworkOrder) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
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
    MinimalLauncherObjectStub* self,
    void* contextKey) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    if (!engine) {
        return 0;
    }

    return engine->Connect(contextKey);
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
static uint32_t __thiscall LauncherObject_Close(
    MinimalLauncherObjectStub* self,
    void* contextKey,
    uint32_t graceful) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    if (!engine) {
        return 0;
    }

    return engine->Close(contextKey, graceful != 0u);
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
static uint32_t __thiscall LauncherObject_SendBuffer(
    MinimalLauncherObjectStub* self,
    void* contextKey,
    void* buffer,
    void* byteCount,
    void* completionContext) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
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
    MinimalLauncherObjectStub* self,
    void* arg1,
    void* arg2,
    void* arg3,
    void* arg4,
    void* arg5) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    return engine ? engine->Slot9_42FD10(arg1, arg2, arg3, arg4, arg5) : 0u;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
static uint32_t __thiscall LauncherObject_Slot10_443810(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    return engine ? engine->Slot10_443810(arg1) : 0u;
}

// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x60 / arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self);
// anchor: launcher.exe:0x4147c0
// vtable: launcher.exe:0x4add70-family helper slot +0x04 (arg5+0x60 / arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject60_Slot1(void* self);
// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject98_Slot0(void* self);
// anchor: launcher.exe:0x4147c0
// vtable: launcher.exe:0x4add70-family helper slot +0x04 (arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject98_Slot1(void* self);

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
static uint32_t __thiscall LauncherObject_Slot11_431670(
    MinimalLauncherObjectStub* self,
    void* arg1,
    uint32_t* out0,
    uint32_t* out1) {
    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    return engine ? engine->Slot11_431670(arg1, out0, out1) : 0u;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
static uint32_t __thiscall LauncherObject_CleanupConnection(
    MinimalLauncherObjectStub* self,
    void* contextKey) {
    LauncherObject_Subobject98_Slot0(&self->helper98);

    mxo::liblttcp::ILTTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    const uint32_t result = engine ? engine->CleanupConnection(contextKey) : 0u;

    LauncherObject_Subobject98_Slot1(&self->helper98);
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static CRITICAL_SECTION* DiagnosticLauncherCritFromHelper(void* self) {
    return self ? reinterpret_cast<CRITICAL_SECTION*>(static_cast<unsigned char*>(self) + 4) : NULL;
}

// UNANCHORED: diagnostic log-throttling helper for replacement arg5 runtime polling.
static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

// UNANCHORED: helper-to-owner backpointer used by the current arg5 subobject helper scaffolds.
static MinimalLauncherObjectStub* DiagnosticLauncherObjectFromHelper(void* helperSelf, size_t helperOffset) {
    return helperSelf
        ? reinterpret_cast<MinimalLauncherObjectStub*>(static_cast<unsigned char*>(helperSelf) - helperOffset)
        : NULL;
}

// UNANCHORED: diagnostic queue-state logger for the replacement arg5 runtime poll loop.
static void DiagnosticLogLauncherRuntimeQueueState(
    const char* source,
    MinimalLauncherObjectStub* object,
    uint32_t count) {
    if (!object) return;

    const bool queue0CursorEqual = (object->queue0C.current1 == object->queue0C.current0);
    const bool queue34CursorEqual = (object->queue34.current1 == object->queue34.current0);
    const bool queue0SameBlock = (object->queue0C.block0 == object->queue0C.block1);
    const bool queue34SameBlock = (object->queue34.block0 == object->queue34.block1);

    spdlog::debug(
        "LauncherObject runtime state[{}] count={}]: self={}, field04={}, field7C={}, q0(current0={}, current1={}, block0={}, block1={}, slotsCurrent={}, slotsLast={}, sameCursor={}, sameBlock={}) q34(current0={}, current1={}, block0={},",
        source,
        (unsigned)count,
        fmt::ptr(object),
        (unsigned)object->field04,
        fmt::ptr(object->field08),
        fmt::ptr(object->field7C),
        fmt::ptr(object->queue0C.current0),
        fmt::ptr(object->queue0C.current1),
        fmt::ptr(object->queue0C.block0),
        fmt::ptr(object->queue0C.block1),
        fmt::ptr(object->queue0C.slotsCurrent),
        fmt::ptr(object->queue0C.slotsLast),
        queue0CursorEqual ? 1u : 0u,
        queue0SameBlock ? 1u : 0u,
        fmt::ptr(object->queue34.current0),
        fmt::ptr(object->queue34.block1),
        fmt::ptr(object->queue34.slotsCurrent),
        fmt::ptr(object->queue34.slotsLast),
        queue34CursorEqual ? 1u : 0u,
        queue34SameBlock ? 1u : 0u);
}

// anchor: launcher.exe:0x435f90
// vtable: launcher.exe:arg5+0x5c helper slot +0x00
static uint32_t __thiscall LauncherObject_Subobject5C_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject5CSlot0CallCount;
    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    BOOL result = eventHandle ? SetEvent(eventHandle) : FALSE;
    spdlog::info(
        "LauncherObjectStub::Subobject5C::Slot0(self={} event={} SetEvent={}) [count={}]",
        fmt::ptr(self),
        fmt::ptr(eventHandle),
        (long)result,
        (unsigned)g_LauncherObjectBuildState.subobject5CSlot0CallCount);
    LogLauncherObjectPointerWords("LauncherObject subobject5C self", self, 8);
    return result ? 0u : 1u;
}

// anchor: launcher.exe:0x435fa0
// vtable: launcher.exe:arg5+0x5c helper slot +0x04
static uint32_t __thiscall LauncherObject_Subobject5C_Slot1(void* self, int reason) {
    ++g_LauncherObjectBuildState.subobject5CSlot1CallCount;

    void* helper60 = self ? static_cast<unsigned char*>(self) + 4 : NULL;
    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    if (helper60) {
        LauncherObject_Subobject60_Slot1(helper60);
    }

    DWORD waitResult = eventHandle ? WaitForSingleObject(eventHandle, static_cast<DWORD>(reason)) : WAIT_FAILED;
    uint32_t result = 1;
    if (waitResult == WAIT_OBJECT_0) {
        if (helper60) {
            LauncherObject_Subobject60_Slot0(helper60);
        }
        result = 0;
    } else if (waitResult == WAIT_TIMEOUT) {
        if (helper60) {
            LauncherObject_Subobject60_Slot0(helper60);
        }
        result = 3;
    }

    spdlog::info(
        "LauncherObjectStub::Subobject5C::Slot1(self={} reason={} event={} wait={} result={} [count={}]",
        fmt::ptr(self),
        reason,
        fmt::ptr(eventHandle),
        waitResult,
        result,
        g_LauncherObjectBuildState.subobject5CSlot1CallCount);
    LogLauncherObjectPointerWords("LauncherObject subobject5C self", self, 8);
    return result;
}

// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x60 / arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject60Slot0CallCount;
    CRITICAL_SECTION* crit = DiagnosticLauncherCritFromHelper(self);
    if (crit) {
        EnterCriticalSection(crit);
    }
    MinimalLauncherObjectStub* owner = DiagnosticLauncherObjectFromHelper(self, 0x60);
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetLauncherObjectEngine(owner)) {
        engine->PumpLauncherConnectionBridgeFromArg5HelperScaffold();
    }
    const uint32_t count = g_LauncherObjectBuildState.subobject60Slot0CallCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(count)) {
        spdlog::debug(
            "LauncherObjectStub::Subobject60::Slot0(self={} crit={} EnterCriticalSection) [count={}]",
            fmt::ptr(self),
            fmt::ptr(crit),
            count);
        LogLauncherObjectPointerWords("LauncherObject subobject60 self", self, 4);
        DiagnosticLogLauncherRuntimeQueueState(
            "sub60.enter",
            owner,
            count);
    }
    return 0;
}

// anchor: launcher.exe:0x4147c0
// vtable: launcher.exe:0x4add70-family helper slot +0x04 (arg5+0x60 / arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject60_Slot1(void* self) {
    ++g_LauncherObjectBuildState.subobject60Slot1CallCount;
    CRITICAL_SECTION* crit = DiagnosticLauncherCritFromHelper(self);
    if (crit) {
        LeaveCriticalSection(crit);
    }
    const uint32_t count = g_LauncherObjectBuildState.subobject60Slot1CallCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(count)) {
        spdlog::debug(
            "LauncherObjectStub::Subobject60::Slot1(self={} crit={} LeaveCriticalSection) [count={}]",
            fmt::ptr(self),
            fmt::ptr(crit),
            count);
        LogLauncherObjectPointerWords("LauncherObject subobject60 self", self, 4);
        DiagnosticLogLauncherRuntimeQueueState(
            "sub60.leave",
            DiagnosticLauncherObjectFromHelper(self, 0x60),
            count);
    }
    return 0;
}

// anchor: launcher.exe:0x4147b0
// vtable: launcher.exe:0x4add70-family helper slot +0x00 (arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject98_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject98Slot0CallCount;
    CRITICAL_SECTION* crit = DiagnosticLauncherCritFromHelper(self);
    if (crit) {
        EnterCriticalSection(crit);
    }
    spdlog::info(
        "LauncherObjectStub::Subobject98::Slot0(self={} crit={} EnterCriticalSection) [count={}]",
        fmt::ptr(self),
        fmt::ptr(crit),
        g_LauncherObjectBuildState.subobject98Slot0CallCount);
    LogLauncherObjectPointerWords("LauncherObject subobject98 self", self, 4);
    return 0;
}

// anchor: launcher.exe:0x4147c0
// vtable: launcher.exe:0x4add70-family helper slot +0x04 (arg5+0x98)
static uint32_t __thiscall LauncherObject_Subobject98_Slot1(void* self) {
    ++g_LauncherObjectBuildState.subobject98Slot1CallCount;
    CRITICAL_SECTION* crit = DiagnosticLauncherCritFromHelper(self);
    if (crit) {
        LeaveCriticalSection(crit);
    }
    spdlog::info(
        "LauncherObjectStub::Subobject98::Slot1(self={} crit={} LeaveCriticalSection) [count={}]",
        fmt::ptr(self),
        fmt::ptr(crit),
        g_LauncherObjectBuildState.subobject98Slot1CallCount);
    LogLauncherObjectPointerWords("LauncherObject subobject98 self", self, 4);
    return 0;
}

// UNANCHORED: seeds the replacement arg5 ABI vtables from recovered launcher.exe addresses.
static void InitializeLauncherObjectStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::memset(&g_LauncherObjectBuildState, 0, sizeof(g_LauncherObjectBuildState));
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
    g_LauncherObjectSubVtable60[1] = (void*)LauncherObject_Subobject60_Slot1; // base +0x60 helper slot 0x4147c0
    g_LauncherObjectSubVtable98[0] = (void*)LauncherObject_Subobject98_Slot0; // derived +0x98 helper slot
    g_LauncherObjectSubVtable98[1] = (void*)LauncherObject_Subobject98_Slot1; // derived +0x98 helper slot
}

// UNANCHORED: replacement launcher builder mirroring launcher.exe:0x40a380 -> 0x431c30.
static MinimalLauncherObjectStub* DiagnosticBuildLauncherObjectLike40A380() {
    InitializeLauncherObjectStub();

    if (g_LauncherObjectBuildState.currentObject) {
        spdlog::info(
            "DIAGNOSTIC: replacing prior launcher object scaffold generation={} ptr={}",
            (unsigned)g_LauncherObjectBuildState.buildGeneration,
            fmt::ptr(g_LauncherObjectBuildState.currentObject));
        DiagnosticFreeLauncherObjectInternals(g_LauncherObjectBuildState.currentObject);
        std::free(g_LauncherObjectBuildState.currentObject);
        g_LauncherObjectBuildState.currentObject = NULL;
    }

    MinimalLauncherObjectStub* object =
        static_cast<MinimalLauncherObjectStub*>(std::malloc(sizeof(MinimalLauncherObjectStub)));
    if (!object) {
        spdlog::info(
            "DIAGNOSTIC: failed to allocate launcher object scaffold (size=0x{:x})",
            static_cast<size_t>(sizeof(MinimalLauncherObjectStub)));
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
        spdlog::info("DIAGNOSTIC: failed to create launcher object +0x7c event ({})", (unsigned long)GetLastError());
        DiagnosticFreeLauncherObjectInternals(object);
        std::free(object);
        return NULL;
    }
    if (!DiagnosticInitializeLauncherQueue(&object->queue0C, 0) ||
        !DiagnosticInitializeLauncherQueue(&object->queue34, 0)) {
        spdlog::info("DIAGNOSTIC: failed to initialize launcher object base queues");
        DiagnosticFreeLauncherObjectInternals(object);
        std::free(object);
        return NULL;
    }

    DiagnosticIntrusiveListHead* list80 =
        static_cast<DiagnosticIntrusiveListHead*>(std::malloc(sizeof(DiagnosticIntrusiveListHead)));
    if (!list80) {
        spdlog::info("DIAGNOSTIC: failed to allocate launcher object +0x80 list head");
        std::free(object);
        return NULL;
    }
    InitializeDiagnosticIntrusiveListHead(list80);
    object->list80 = list80;

    DiagnosticIntrusiveListHeadSmall* list8C =
        static_cast<DiagnosticIntrusiveListHeadSmall*>(std::malloc(sizeof(DiagnosticIntrusiveListHeadSmall)));
    if (!list8C) {
        spdlog::info("DIAGNOSTIC: failed to allocate launcher object +0x8c list head");
        std::free(list80);
        std::free(object);
        return NULL;
    }
    InitializeDiagnosticIntrusiveListHeadSmall(list8C);
    object->list8C = list8C;

    ++g_LauncherObjectBuildState.buildGeneration;
    g_LauncherObjectBuildState.currentObject = object;

    spdlog::info(
        "DIAGNOSTIC: built launcher object scaffold like 0x40a380/0x431c30 ptr={} size={} generation={}",
        fmt::ptr(object),
        sizeof(MinimalLauncherObjectStub),
        g_LauncherObjectBuildState.buildGeneration);
    spdlog::info(
        "DIAGNOSTIC: launcher object scaffold notes: field04=0 field08=NULL +0x0c/+0x34 faithful queue skeletons initialized +0x80/+0x8c intrusive heads allocated +0x5c/+0x60/+0x98 seeded to faithful placeholders, full primary 13-slot vtable surface now exposed; slot5 models the proven empty-list80 miss path and slot10 matches the original zero-return stub");
    LogLauncherObjectPointerWords("LauncherObject self", object, 8);
    LogLauncherObjectPointerWords("LauncherObject queue0C", &object->queue0C, 8);
    LogLauncherObjectPointerWords("LauncherObject queue34", &object->queue34, 8);
    LogLauncherObjectPointerWords("LauncherObject +0x80 list", object->list80, 4);
    LogLauncherObjectPointerWords("LauncherObject +0x8c list", object->list8C, 4);

    return object;
}

// UNANCHORED: replacement helper that mirrors the mediator +0x08 handoff used after launcher.exe:0x40a380.
static void DiagnosticRegisterLauncherObjectWithMediator(void* mediatorPtr, void* launcherObjectPtr) {
    if (!launcherObjectPtr || !mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[2]) {
        spdlog::info("DIAGNOSTIC: mediator register slot unavailable for launcher object handoff");
        return;
    }

    typedef int (__thiscall *RegisterEngineFn)(void*, void*);
    RegisterEngineFn fn = (RegisterEngineFn)vtable[2];
    int result = fn(mediatorPtr, launcherObjectPtr);
    spdlog::info(
        "DIAGNOSTIC: mediator +0x08 register launcher object({}) -> {}",
        fmt::ptr(launcherObjectPtr),
        result);
}

// UNANCHORED: public replacement-launcher entrypoint that installs the arg5 scaffold.
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr) {
    MinimalLauncherObjectStub* object = DiagnosticBuildLauncherObjectLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }

    spdlog::info(
        "DIAGNOSTIC: using launcher object scaffold for arg5 ptr={} size={}",
        fmt::ptr(object),
        sizeof(MinimalLauncherObjectStub));

    if (object) {
        DiagnosticGetOrCreateLttcpEngine(object);
        DiagnosticSyncLauncherObjectSidecarState(object);
    }

    if (mediatorPtr && object) {
        spdlog::info("DIAGNOSTIC: mirroring original handoff: registering arg5 through arg6 before InitClientDLL");
        DiagnosticRegisterLauncherObjectWithMediator(mediatorPtr, object);
    }
}

