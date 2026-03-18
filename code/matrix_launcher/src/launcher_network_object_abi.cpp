#include "diagnostics.h"
#include "diagnostics_auth.h"
#include "../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

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
    uint32_t slot1CallCount;
    uint32_t slot2CallCount;
    uint32_t slot3CallCount;
    uint32_t slot4CallCount;
    uint32_t slot5CallCount;
    uint32_t slot6CallCount;
    uint32_t slot7CallCount;
    uint32_t slot8CallCount;
    uint32_t slot9CallCount;
    uint32_t slot10CallCount;
    uint32_t slot11CallCount;
    uint32_t slot12CallCount;
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

static void InitializeDiagnosticIntrusiveListHeadSmall(DiagnosticIntrusiveListHeadSmall* head) {
    if (!head) return;
    std::memset(head, 0, sizeof(*head));
    head->colorOrFlag = 0;
    head->root = NULL;
    head->first = head;
    head->last = head;
}

struct DiagnosticLauncherQueuePair {
    uint32_t value0;
    uint32_t value1;
};

static void DiagnosticFreeLauncherQueue(DiagnosticLauncherQueue* queue) {
    if (!queue) return;

    if (queue->slotsBase) {
        uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
        uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
        if (slotsCurrent && slotsLast && slotsCurrent <= slotsLast) {
            for (uint32_t** slot = slotsCurrent; slot <= slotsLast; ++slot) {
                if (*slot) {
                    std::free(*slot);
                    *slot = NULL;
                }
            }
        }
        std::free(slotsBase);
    }

    std::memset(queue, 0, sizeof(*queue));
}

static bool DiagnosticInitializeLauncherQueue(DiagnosticLauncherQueue* queue, uint32_t initialSize) {
    if (!queue) return false;
    DiagnosticFreeLauncherQueue(queue);

    uint32_t blockCount = (initialSize >> 4) + 1;
    uint32_t slotCapacity = blockCount + 2;
    if (slotCapacity < 8) slotCapacity = 8;

    uint32_t** slotsBase = static_cast<uint32_t**>(std::calloc(slotCapacity, sizeof(uint32_t*)));
    if (!slotsBase) {
        Log("DIAGNOSTIC: failed to allocate launcher queue slot array (capacity=%u)", (unsigned)slotCapacity);
        return false;
    }

    const uint32_t firstIndex = (slotCapacity - blockCount) >> 1;
    for (uint32_t i = 0; i < blockCount; ++i) {
        slotsBase[firstIndex + i] = static_cast<uint32_t*>(std::calloc(1, 0x80));
        if (!slotsBase[firstIndex + i]) {
            Log("DIAGNOSTIC: failed to allocate launcher queue block %u/%u", (unsigned)(i + 1), (unsigned)blockCount);
            queue->slotsBase = slotsBase;
            queue->slotsCurrent = slotsBase + firstIndex;
            queue->slotsLast = slotsBase + firstIndex + i;
            DiagnosticFreeLauncherQueue(queue);
            return false;
        }
    }

    uint32_t** slotsCurrent = slotsBase + firstIndex;
    uint32_t** slotsLast = slotsCurrent + blockCount - 1;
    uint8_t* block0 = reinterpret_cast<uint8_t*>(*slotsCurrent);
    uint8_t* block1 = reinterpret_cast<uint8_t*>(*slotsLast);

    queue->slotsBase = slotsBase;
    queue->slotCapacity = slotCapacity;
    queue->slotsCurrent = slotsCurrent;
    queue->slotsLast = slotsLast;
    queue->block0 = block0;
    queue->end0 = block0 ? (block0 + 0x80) : NULL;
    queue->current0 = block0;
    queue->block1 = block1;
    queue->end1 = block1 ? (block1 + 0x80) : NULL;
    queue->current1 = block1 ? (block1 + ((initialSize & 0xfu) * 8u)) : NULL;
    return true;
}

static bool DiagnosticRecenterLauncherQueueSlots(
    DiagnosticLauncherQueue* queue,
    uint32_t additionalBlocks,
    bool biasTowardTail) {
    if (!queue || !queue->slotsBase || !queue->slotsCurrent || !queue->slotsLast) return false;

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t activeBlocks = static_cast<uint32_t>((slotsLast - slotsCurrent) + 1);
    const uint32_t neededBlocks = activeBlocks + additionalBlocks;

    uint32_t newCapacity = queue->slotCapacity;
    uint32_t** newSlotsBase = slotsBase;

    if (queue->slotCapacity <= (neededBlocks * 2u)) {
        const uint32_t growthBase = (queue->slotCapacity >= additionalBlocks) ? queue->slotCapacity : additionalBlocks;
        newCapacity = queue->slotCapacity + growthBase + 2u;
        newSlotsBase = static_cast<uint32_t**>(std::calloc(newCapacity, sizeof(uint32_t*)));
        if (!newSlotsBase) {
            Log("DIAGNOSTIC: failed to grow launcher queue slot array (%u -> %u)",
                (unsigned)queue->slotCapacity,
                (unsigned)newCapacity);
            return false;
        }
    }

    uint32_t newIndex = (newCapacity - neededBlocks) >> 1;
    if (biasTowardTail) newIndex += additionalBlocks;

    uint32_t** newSlotsCurrent = newSlotsBase + newIndex;
    std::memmove(newSlotsCurrent, slotsCurrent, activeBlocks * sizeof(uint32_t*));

    if (newSlotsBase != slotsBase) {
        std::free(slotsBase);
        queue->slotsBase = newSlotsBase;
        queue->slotCapacity = newCapacity;
    }

    queue->slotsCurrent = newSlotsCurrent;
    queue->block0 = *newSlotsCurrent;
    queue->end0 = queue->block0 ? (static_cast<uint8_t*>(queue->block0) + 0x80) : NULL;

    uint32_t** newSlotsLast = newSlotsCurrent + activeBlocks - 1;
    queue->slotsLast = newSlotsLast;
    queue->block1 = *newSlotsLast;
    queue->end1 = queue->block1 ? (static_cast<uint8_t*>(queue->block1) + 0x80) : NULL;
    return true;
}

static bool DiagnosticGrowLauncherQueue(
    DiagnosticLauncherQueue* queue,
    const DiagnosticLauncherQueuePair* pendingPair) {
    if (!queue || !queue->slotsLast) return false;

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t tailFreeSlots = queue->slotCapacity - static_cast<uint32_t>(slotsLast - slotsBase);
    if (tailFreeSlots < 2u) {
        if (!DiagnosticRecenterLauncherQueueSlots(queue, 1, false)) {
            return false;
        }
        slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    }

    uint32_t* newBlock = static_cast<uint32_t*>(std::calloc(1, 0x80));
    if (!newBlock) {
        Log("DIAGNOSTIC: failed to allocate launcher queue growth block");
        return false;
    }

    slotsLast[1] = newBlock;

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    if (current1 && pendingPair) {
        current1[0] = pendingPair->value0;
        current1[1] = pendingPair->value1;
    }

    queue->slotsLast = slotsLast + 1;
    queue->block1 = newBlock;
    queue->end1 = static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80;
    queue->current1 = newBlock;
    return true;
}

static bool DiagnosticPushLauncherQueue(
    DiagnosticLauncherQueue* queue,
    uint32_t value0,
    uint32_t value1) {
    if (!queue || !queue->current1) return false;

    uint8_t* lastPairInBlock = queue->end1 ? (static_cast<uint8_t*>(queue->end1) - 8) : NULL;
    if (static_cast<void*>(queue->current1) == static_cast<void*>(lastPairInBlock)) {
        DiagnosticLauncherQueuePair pair = {value0, value1};
        return DiagnosticGrowLauncherQueue(queue, &pair);
    }

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    current1[0] = value0;
    current1[1] = value1;
    queue->current1 = current1 + 2;
    return true;
}

// UNANCHORED: auth-side diagnostics bridge into the replacement arg5 queue0C scaffold.
bool DiagnosticAuthBridgePushQueue0C(void* ownerPtr, uint32_t value0, uint32_t value1) {
    MinimalLauncherObjectStub* owner = static_cast<MinimalLauncherObjectStub*>(ownerPtr);
    return owner ? DiagnosticPushLauncherQueue(&owner->queue0C, value0, value1) : false;
}

// UNANCHORED: auth-side diagnostics bridge for refreshing replacement arg5 sidecar state.
void DiagnosticAuthBridgeSyncOwnerState(void* ownerPtr) {
    DiagnosticSyncLauncherObjectSidecarState(static_cast<MinimalLauncherObjectStub*>(ownerPtr));
}

// UNANCHORED: replacement arg5 owner/binding cleanup helper.
static void DiagnosticClearLttcpBinding(MinimalLauncherObjectStub* owner) {
    if (owner && g_DiagnosticLttcpBinding && g_DiagnosticLttcpBinding->Owner() != owner) {
        return;
    }

    DiagnosticAuthResetState();
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
            Log("DIAGNOSTIC: failed to allocate CLTThreadPerClientTCPEngine binding for %p", owner);
            return NULL;
        }
    }

    if (g_DiagnosticLttcpBinding->Owner() != owner) {
        DiagnosticAuthResetState();
        if (!g_DiagnosticLttcpBinding->Bind(owner)) {
            Log("DIAGNOSTIC: failed to bind CLTThreadPerClientTCPEngine sidecar for %p", owner);
            return NULL;
        }
        DiagnosticAuthInitializeForEngine(owner, g_DiagnosticLttcpBinding->Engine());
        Log("DIAGNOSTIC: created CLTThreadPerClientTCPEngine sidecar for launcher object %p", owner);
    }

    return g_DiagnosticLttcpBinding->Engine();
}

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

static void DiagnosticSyncLauncherObjectSidecarState(MinimalLauncherObjectStub* self) {
    if (!self) return;

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
    Log("LauncherObjectStub::Release(flags=%u self=%p)", flags, self);
    DiagnosticFreeLauncherObjectInternals(self);
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
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

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->MonitorPort(
            /*portHostOrder=*/static_cast<uint16_t>(reinterpret_cast<uintptr_t>(arg1)),
            /*ownerContext=*/arg2);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot1_431CE0 -> sidecar MonitorPort result=0x%08x", (unsigned)result);
    (void)arg3;
    return result;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
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

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->UDPMonitorPort(
            /*portHostOrder=*/static_cast<uint16_t>(reinterpret_cast<uintptr_t>(arg1)),
            /*contextKey=*/arg2,
            /*ownerContext=*/arg3);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot2_4325D0 -> sidecar UDPMonitorPort result=0x%08x", (unsigned)result);
    return result;
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b2768 slot +0x0c
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

    uint32_t result = 0;
    uint16_t boundPortHostOrder = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->MonitorEphemeralUDPPort(
            /*outBoundPortHostOrder=*/arg1 ? static_cast<uint16_t*>(arg1) : &boundPortHostOrder,
            /*contextKey=*/arg2,
            /*ownerContext=*/arg3);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log(
        "LauncherObjectStub::Slot3_436000 -> sidecar MonitorEphemeralUDPPort result=0x%08x boundPort=%u context=%p",
        (unsigned)result,
        (unsigned)(arg1 ? *static_cast<uint16_t*>(arg1) : boundPortHostOrder),
        arg2);
    return result;
}

// anchor: launcher.exe:0x42f7c0
// vtable: launcher.exe:0x004b2768 slot +0x10
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
    // Keep slot4 as a logged placeholder for now.
    // This still needs stronger static naming/semantics before we route it into liblttcp.
    return 0;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
static uint32_t __thiscall LauncherObject_Slot5_431840(
    MinimalLauncherObjectStub* self,
    void* arg1,
    uint32_t* out0,
    void* arg3) {
    ++g_LauncherObjectBuildState.slot5CallCount;
    if (out0) *out0 = 0;

    const DiagnosticIntrusiveListHead* list80 =
        self ? static_cast<const DiagnosticIntrusiveListHead*>(self->list80) : NULL;
    const bool listLooksEmpty =
        !self || !list80 || !list80->root || list80->first == list80;

    Log(
        "LauncherObjectStub::Slot5_431840(self=%p arg1=%p out0=%p arg3=%p root=%p first=%p last=%p empty=%u) [count=%u]",
        self,
        arg1,
        out0,
        arg3,
        list80 ? list80->root : NULL,
        list80 ? list80->first : NULL,
        list80 ? list80->last : NULL,
        listLooksEmpty ? 1u : 0u,
        (unsigned)g_LauncherObjectBuildState.slot5CallCount);
    LogPointerWords("LauncherObject slot5 self", self, 8);

    if (listLooksEmpty) {
        Log("LauncherObjectStub::Slot5_431840 -> faithful empty-list80 miss path (return 0x7000004, out0=0)");
        return 0x7000004u;
    }

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->UnmonitorPort(
            /*portHostOrder=*/static_cast<uint16_t>(reinterpret_cast<uintptr_t>(arg1)),
            /*ipv4NetworkOrder=*/static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg3)),
            /*outSocketHandle=*/out0);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log(
        "LauncherObjectStub::Slot5_431840 -> sidecar UnmonitorPort result=0x%08x outSocketHandle=0x%08x",
        (unsigned)result,
        (unsigned)(out0 ? *out0 : 0u));
    return result;
}

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
static uint32_t __thiscall LauncherObject_Slot6_4328A0(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    ++g_LauncherObjectBuildState.slot6CallCount;
    Log(
        "LauncherObjectStub::Slot6_4328A0(self=%p arg1=%p) [count=%u]",
        self,
        arg1,
        (unsigned)g_LauncherObjectBuildState.slot6CallCount);
    LogPointerWords("LauncherObject slot6 self", self, 8);

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->ConnectContext(arg1);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot6_4328A0 -> sidecar Connect result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
static uint32_t __thiscall LauncherObject_Slot7_42F970(
    MinimalLauncherObjectStub* self,
    void* arg1,
    uint32_t arg2) {
    ++g_LauncherObjectBuildState.slot7CallCount;
    Log(
        "LauncherObjectStub::Slot7_42F970(self=%p arg1=%p arg2=0x%08x) [count=%u]",
        self,
        arg1,
        (unsigned)arg2,
        (unsigned)g_LauncherObjectBuildState.slot7CallCount);
    LogPointerWords("LauncherObject slot7 self", self, 8);

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->CloseContext(arg1, /*graceful=*/(arg2 != 0u));
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot7_42F970 -> sidecar Close result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
static uint32_t __thiscall LauncherObject_Slot8_42FBD0(
    MinimalLauncherObjectStub* self,
    void* arg1,
    void* arg2,
    void* arg3,
    void* arg4) {
    ++g_LauncherObjectBuildState.slot8CallCount;
    Log(
        "LauncherObjectStub::Slot8_42FBD0(self=%p arg1=%p arg2=%p arg3=%p arg4=%p) [count=%u]",
        self,
        arg1,
        arg2,
        arg3,
        arg4,
        (unsigned)g_LauncherObjectBuildState.slot8CallCount);
    LogPointerWords("LauncherObject slot8 self", self, 8);

    uint32_t result = 0;
    if (mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self)) {
        result = engine->SendPacketContext(
            /*contextKey=*/arg1,
            /*buffer=*/arg2,
            /*byteCount=*/static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg3)),
            /*completionContext=*/arg4);
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot8_42FBD0 -> sidecar SendPacket/SendBuffer result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
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
    ++g_LauncherObjectBuildState.slot9CallCount;
    Log(
        "LauncherObjectStub::Slot9_42FD10(self=%p arg1=%p arg2=%p arg3=%p arg4=%p arg5=%p) [count=%u]",
        self,
        arg1,
        arg2,
        arg3,
        arg4,
        arg5,
        (unsigned)g_LauncherObjectBuildState.slot9CallCount);
    LogPointerWords("LauncherObject slot9 self", self, 8);
    return 0;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
static uint32_t __thiscall LauncherObject_Slot10_443810(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    ++g_LauncherObjectBuildState.slot10CallCount;
    Log(
        "LauncherObjectStub::Slot10_443810(self=%p arg1=%p) [count=%u]",
        self,
        arg1,
        (unsigned)g_LauncherObjectBuildState.slot10CallCount);
    LogPointerWords("LauncherObject slot10 self", self, 8);
    return 0;
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
    ++g_LauncherObjectBuildState.slot11CallCount;
    if (out0) *out0 = 0;
    if (out1) *out1 = 0;
    Log(
        "LauncherObjectStub::Slot11_431670(self=%p arg1=%p out0=%p out1=%p) [count=%u]",
        self,
        arg1,
        out0,
        out1,
        (unsigned)g_LauncherObjectBuildState.slot11CallCount);
    LogPointerWords("LauncherObject slot11 self", self, 8);
    return 0;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
static uint32_t __thiscall LauncherObject_Slot12_4316A0(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    ++g_LauncherObjectBuildState.slot12CallCount;
    LauncherObject_Subobject98_Slot0(&self->helper98);

    bool droppedConnection = false;
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    const uint32_t cleanupResult = engine ? engine->CleanupConnection(/*contextKey=*/arg1) : 0u;
    // Keep the connection/context sidecar alive across the later context->+0x10 callback.
    // Original launcher consumer order is slot12(context) first, then context->+0x10(workItem).
    // Dropping the sidecar here would make the current diagnostic context callback less useful.
    DiagnosticSyncLauncherObjectSidecarState(self);

    const DiagnosticIntrusiveListHeadSmall* list8C =
        self ? static_cast<const DiagnosticIntrusiveListHeadSmall*>(self->list8C) : NULL;
    const bool listLooksEmpty =
        !self || !list8C || !list8C->root || list8C->first == list8C;

    Log(
        "LauncherObjectStub::Slot12_4316A0(self=%p arg1=%p root=%p first=%p last=%p empty=%u cleanupResult=0x%08x droppedConnection=%u) [count=%u]",
        self,
        arg1,
        list8C ? list8C->root : NULL,
        list8C ? list8C->first : NULL,
        list8C ? list8C->last : NULL,
        listLooksEmpty ? 1u : 0u,
        (unsigned)cleanupResult,
        droppedConnection ? 1u : 0u,
        (unsigned)g_LauncherObjectBuildState.slot12CallCount);
    LogPointerWords("LauncherObject slot12 self", self, 8);

    if (listLooksEmpty) {
        Log("LauncherObjectStub::Slot12_4316A0 -> sidecar CleanupConnection now leaves list8C empty");
    } else {
        Log("LauncherObjectStub::Slot12_4316A0 -> sidecar CleanupConnection left list8C non-empty");
    }

    LauncherObject_Subobject98_Slot1(&self->helper98);
    return cleanupResult;
}

static CRITICAL_SECTION* DiagnosticLauncherCritFromHelper(void* self) {
    return self ? reinterpret_cast<CRITICAL_SECTION*>(static_cast<unsigned char*>(self) + 4) : NULL;
}

// UNANCHORED: diagnostic log-throttling helper for replacement arg5 runtime polling.
static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

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

    Log(
        "LauncherObject runtime state[%s count=%u]: self=%p field04=%u field08=%p field7C=%p q0(current0=%p current1=%p block0=%p block1=%p slotsCurrent=%p slotsLast=%p sameCursor=%u sameBlock=%u) q34(current0=%p current1=%p block0=%p block1=%p slotsCurrent=%p slotsLast=%p sameCursor=%u sameBlock=%u)",
        source,
        (unsigned)count,
        object,
        (unsigned)object->field04,
        object->field08,
        object->field7C,
        object->queue0C.current0,
        object->queue0C.current1,
        object->queue0C.block0,
        object->queue0C.block1,
        object->queue0C.slotsCurrent,
        object->queue0C.slotsLast,
        queue0CursorEqual ? 1u : 0u,
        queue0SameBlock ? 1u : 0u,
        object->queue34.current0,
        object->queue34.current1,
        object->queue34.block0,
        object->queue34.block1,
        object->queue34.slotsCurrent,
        object->queue34.slotsLast,
        queue34CursorEqual ? 1u : 0u,
        queue34SameBlock ? 1u : 0u);
}

// anchor: launcher.exe:0x435f90
// vtable: launcher.exe:arg5+0x5c helper slot +0x00
static uint32_t __thiscall LauncherObject_Subobject5C_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject5CSlot0CallCount;
    HANDLE eventHandle = self ? *reinterpret_cast<HANDLE*>(static_cast<unsigned char*>(self) + 0x20) : NULL;
    BOOL result = eventHandle ? SetEvent(eventHandle) : FALSE;
    Log(
        "LauncherObjectStub::Subobject5C::Slot0(self=%p event=%p SetEvent=%ld) [count=%u]",
        self,
        eventHandle,
        (long)result,
        (unsigned)g_LauncherObjectBuildState.subobject5CSlot0CallCount);
    LogPointerWords("LauncherObject subobject5C self", self, 8);
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

    Log(
        "LauncherObjectStub::Subobject5C::Slot1(self=%p reason=%d event=%p wait=%lu result=%u) [count=%u]",
        self,
        reason,
        eventHandle,
        (unsigned long)waitResult,
        (unsigned)result,
        (unsigned)g_LauncherObjectBuildState.subobject5CSlot1CallCount);
    LogPointerWords("LauncherObject subobject5C self", self, 8);
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
    DiagnosticAuthPollLiveConnectionTraffic(owner);
    const uint32_t count = g_LauncherObjectBuildState.subobject60Slot0CallCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(count)) {
        Log(
            "LauncherObjectStub::Subobject60::Slot0(self=%p crit=%p EnterCriticalSection) [count=%u]",
            self,
            crit,
            (unsigned)count);
        LogPointerWords("LauncherObject subobject60 self", self, 4);
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
        Log(
            "LauncherObjectStub::Subobject60::Slot1(self=%p crit=%p LeaveCriticalSection) [count=%u]",
            self,
            crit,
            (unsigned)count);
        LogPointerWords("LauncherObject subobject60 self", self, 4);
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
    Log(
        "LauncherObjectStub::Subobject98::Slot0(self=%p crit=%p EnterCriticalSection) [count=%u]",
        self,
        crit,
        (unsigned)g_LauncherObjectBuildState.subobject98Slot0CallCount);
    LogPointerWords("LauncherObject subobject98 self", self, 4);
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
    Log(
        "LauncherObjectStub::Subobject98::Slot1(self=%p crit=%p LeaveCriticalSection) [count=%u]",
        self,
        crit,
        (unsigned)g_LauncherObjectBuildState.subobject98Slot1CallCount);
    LogPointerWords("LauncherObject subobject98 self", self, 4);
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
    g_LauncherObjectVtable[0] = (void*)LauncherObject_Release;        // 0x4319a0
    g_LauncherObjectVtable[1] = (void*)LauncherObject_Slot1_431CE0;   // 0x431ce0
    g_LauncherObjectVtable[2] = (void*)LauncherObject_Slot2_4325D0;   // 0x4325d0
    g_LauncherObjectVtable[3] = (void*)LauncherObject_Slot3_436000;   // 0x436000
    g_LauncherObjectVtable[4] = (void*)LauncherObject_Slot4_42F7C0;   // 0x42f7c0
    g_LauncherObjectVtable[5] = (void*)LauncherObject_Slot5_431840;   // 0x431840
    g_LauncherObjectVtable[6] = (void*)LauncherObject_Slot6_4328A0;   // 0x4328a0
    g_LauncherObjectVtable[7] = (void*)LauncherObject_Slot7_42F970;   // 0x42f970
    g_LauncherObjectVtable[8] = (void*)LauncherObject_Slot8_42FBD0;   // 0x42fbd0
    g_LauncherObjectVtable[9] = (void*)LauncherObject_Slot9_42FD10;   // 0x42fd10
    g_LauncherObjectVtable[10] = (void*)LauncherObject_Slot10_443810; // 0x443810
    g_LauncherObjectVtable[11] = (void*)LauncherObject_Slot11_431670; // 0x431670
    g_LauncherObjectVtable[12] = (void*)LauncherObject_Slot12_4316A0; // 0x4316a0
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
        Log("DIAGNOSTIC: failed to create launcher object +0x7c event (%lu)", (unsigned long)GetLastError());
        DiagnosticFreeLauncherObjectInternals(object);
        std::free(object);
        return NULL;
    }
    if (!DiagnosticInitializeLauncherQueue(&object->queue0C, 0) ||
        !DiagnosticInitializeLauncherQueue(&object->queue34, 0)) {
        Log("DIAGNOSTIC: failed to initialize launcher object base queues");
        DiagnosticFreeLauncherObjectInternals(object);
        std::free(object);
        return NULL;
    }

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
        "DIAGNOSTIC: launcher object scaffold notes: field04=0 field08=NULL +0x0c/+0x34 faithful queue skeletons initialized +0x80/+0x8c intrusive heads allocated +0x5c/+0x60/+0x98 seeded to faithful placeholders, full primary 13-slot vtable surface now exposed; slot5 models the proven empty-list80 miss path and slot10 matches the original zero-return stub");
    LogPointerWords("LauncherObject self", object, 8);
    LogPointerWords("LauncherObject queue0C", &object->queue0C, 8);
    LogPointerWords("LauncherObject queue34", &object->queue34, 8);
    LogPointerWords("LauncherObject +0x80 list", object->list80, 4);
    LogPointerWords("LauncherObject +0x8c list", object->list8C, 4);

    return object;
}

// UNANCHORED: replacement helper that mirrors the mediator +0x08 handoff used after launcher.exe:0x40a380.
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

// UNANCHORED: public replacement-launcher entrypoint that installs the arg5 scaffold.
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr) {
    MinimalLauncherObjectStub* object = DiagnosticBuildLauncherObjectLike40A380();
    if (outLauncherObjectPtr) {
        *outLauncherObjectPtr = object;
    }

    Log(
        "DIAGNOSTIC: using launcher object scaffold for arg5 (%p, size=0x%zx)",
        object,
        sizeof(MinimalLauncherObjectStub));

    if (object) {
        DiagnosticGetOrCreateLttcpEngine(object);
        DiagnosticSyncLauncherObjectSidecarState(object);
    }

    if (mediatorPtr && object) {
        Log("DIAGNOSTIC: mirroring original handoff: registering arg5 through arg6 before InitClientDLL");
        DiagnosticRegisterLauncherObjectWithMediator(mediatorPtr, object);
    }
}

