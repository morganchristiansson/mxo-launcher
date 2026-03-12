#include "diagnostics.h"
#include "liblttcp/cmessageconnection.h"
#include "liblttcp/ltthreadperclienttcpengine.h"

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

static_assert(sizeof(DiagnosticLauncherLockHelper) == 0x1c, "launcher lock helper size mismatch");
static_assert(sizeof(MinimalLauncherObjectStub) == 0xb4, "launcher object scaffold size must match original allocation");

static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count);
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
    void* loadingState120;
    void* selectionContext0ec;
    void* selectionContext0ecCopy;
    void* runtimeObject148;
    void* runtimeObject174;
    void* runtimeDescriptor178;
    uint32_t attach170Count;
    uint32_t provide124Count;
    uint32_t loading120Count;
    uint32_t selection0ecCount;
    uint32_t profile0f4Count;
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
static MinimalLauncherObjectStub* g_DiagnosticLttcpOwner = NULL;
static mxo::liblttcp::CLTThreadPerClientTCPEngine* g_DiagnosticLttcpEngine = NULL;
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
static uint32_t g_MediatorWorldUpperBoundExclusive = 1;
static uint32_t g_MediatorVariantUpperBoundExclusive = 1;
static uint32_t g_MediatorSelectedWorldIndexLow24 = 0;
static uint32_t g_MediatorSelectedVariantIndexHigh8 = 0;
static uint32_t g_MediatorSelectedWorldType = 1;
static uint32_t g_MediatorSelectedVariantState = 0;
static uint32_t g_MediatorMappedSelectionId = 0;
static const char* g_MediatorMappedSelectionName = g_MediatorStringC;
static const char* g_MediatorMappedVariantName = g_MediatorStringC;
static const char* g_MediatorProfileName = g_MediatorStringA;
static const char* g_MediatorAuthName = g_MediatorStringA;

struct __attribute__((packed)) DiagnosticMediatorSelectionPacked {
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t reserved2;
    const char* mappedName; // read by client as dword at +0x03
    uint32_t selectionId;   // read by client as dword at +0x07
};

struct DiagnosticMediatorSelectionObject {
    uint8_t reserved[0x10];
    DiagnosticMediatorSelectionPacked* packed; // read by client as dword at +0x10
};

static constexpr size_t kDiagnosticSelectionContextSize = 0xb4; // from client.dll:6211d3e0 zero-init of the +0xec handoff object
struct DiagnosticMediatorSelectionContextCopy {
    unsigned char bytes[kDiagnosticSelectionContextSize];
};

static DiagnosticMediatorSelectionPacked g_MediatorSelectionPacked = {0, 0, 0, g_MediatorStringC, 0};
static DiagnosticMediatorSelectionObject g_MediatorSelectionObject = {};
static DiagnosticMediatorSelectionContextCopy g_MediatorSelectionContextCopy = {};
static bool g_MediatorSelectionContextCopyValid = false;

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

static void LogWordBuffer(const char* label, const void* ptr, uint32_t byteCount) {
    if (!label || !ptr || byteCount == 0) {
        return;
    }

    const uint32_t* words = static_cast<const uint32_t*>(ptr);
    const uint32_t wordCount = byteCount / 4;
    for (uint32_t i = 0; i < wordCount; i += 4) {
        Log(
            "%s @ %p [+0x%02x]=%08x [+0x%02x]=%08x [+0x%02x]=%08x [+0x%02x]=%08x",
            label,
            ptr,
            (unsigned)(i * 4),
            words[i + 0],
            (unsigned)((i + 1) * 4),
            (i + 1 < wordCount) ? words[i + 1] : 0,
            (unsigned)((i + 2) * 4),
            (i + 2 < wordCount) ? words[i + 2] : 0,
            (unsigned)((i + 3) * 4),
            (i + 3 < wordCount) ? words[i + 3] : 0);
    }
}

static bool IsMostlyPrintableAscii(const unsigned char* data, uint32_t length) {
    if (!data || length < 4) return false;
    for (uint32_t i = 0; i < length; ++i) {
        const unsigned char c = data[i];
        if (c < 0x20 || c > 0x7e) {
            return false;
        }
    }
    return true;
}

static void LogSelectionContextDetails(const void* selectionContext, uint32_t byteCount) {
    if (!selectionContext || byteCount == 0) {
        return;
    }

    LogWordBuffer("SelectionContext words", selectionContext, byteCount);

    const unsigned char* bytes = static_cast<const unsigned char*>(selectionContext);
    bool loggedAnyString = false;
    for (uint32_t i = 0; i < byteCount;) {
        if (bytes[i] == '\0') {
            ++i;
            continue;
        }

        uint32_t j = i;
        while (j < byteCount && bytes[j] != '\0' && bytes[j] >= 0x20 && bytes[j] <= 0x7e) {
            ++j;
        }

        if (j > i && j < byteCount && IsMostlyPrintableAscii(bytes + i, j - i)) {
            char buffer[128] = {0};
            const uint32_t copyLength = ((j - i) < (sizeof(buffer) - 1)) ? (j - i) : (sizeof(buffer) - 1);
            std::memcpy(buffer, bytes + i, copyLength);
            buffer[copyLength] = '\0';
            Log(
                "SelectionContext ascii candidate [+0x%02x] = '%s'",
                (unsigned)i,
                buffer);
            loggedAnyString = true;
            i = j + 1;
            continue;
        }

        ++i;
    }

    if (!loggedAnyString) {
        Log("SelectionContext ascii candidate scan: none");
    }
}

static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    std::memset(&g_MediatorSelectionObject, 0, sizeof(g_MediatorSelectionObject));
    std::memset(&g_MediatorSelectionContextCopy, 0, sizeof(g_MediatorSelectionContextCopy));
    g_MediatorSelectionContextCopyValid = false;
    g_MediatorWorldUpperBoundExclusive = 1;
    g_MediatorVariantUpperBoundExclusive = 1;
    g_MediatorSelectedWorldIndexLow24 = 0;
    g_MediatorSelectedVariantIndexHigh8 = 0;
    g_MediatorSelectedWorldType = 1;
    g_MediatorSelectedVariantState = 0;
    g_MediatorMappedSelectionId = 0;
    g_MediatorMappedSelectionName = g_MediatorStringC;
    g_MediatorMappedVariantName = g_MediatorStringC;
    g_MediatorProfileName = g_MediatorStringA;
    g_MediatorAuthName = g_MediatorStringA;
    g_MediatorSelectionPacked = {0, 0, 0, g_MediatorStringC, 0};
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
    static uint32_t s_IsConnectedCount = 0;
    ++s_IsConnectedCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(s_IsConnectedCount)) {
        Log("MediatorStub::IsConnected() -> 1 [count=%u self=%p registeredEngine=%p]",
            (unsigned)s_IsConnectedCount,
            self,
            self ? self->field04 : NULL);
    }
    return 1;
}

static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetProfileRootName(+0x38) -> '%s'", g_MediatorProfileName);
    return g_MediatorProfileName;
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex);
static bool DiagnosticMediatorVariantIndexMatchesConfiguredSelection(uint32_t variantIndex);

static const char* DiagnosticMediatorWorldNameForIndex(uint32_t worldIndex) {
    if (worldIndex >= g_MediatorWorldUpperBoundExclusive) {
        return NULL;
    }
    if (!DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex)) {
        return NULL;
    }
    return g_MediatorMappedSelectionName;
}

static const char* DiagnosticMediatorVariantNameForIndex(uint32_t variantIndex) {
    if (variantIndex >= g_MediatorVariantUpperBoundExclusive) {
        return NULL;
    }
    if (!DiagnosticMediatorVariantIndexMatchesConfiguredSelection(variantIndex)) {
        return NULL;
    }
    return g_MediatorMappedVariantName;
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex) {
    return worldIndex == g_MediatorSelectedWorldIndexLow24;
}

static bool DiagnosticMediatorVariantIndexMatchesConfiguredSelection(uint32_t variantIndex) {
    return variantIndex == g_MediatorSelectedVariantIndexHigh8;
}

static uint32_t DiagnosticMediatorExpectedSelectionDescriptorScratchRequest() {
    // client.dll:62170dc1..62170e59 reuses the original arg7 stack slot as scratch,
    // then stores only BL back into its low byte before later reloading the full dword
    // for the +0x40 selection-descriptor path.
    const uint32_t variantHigh8 = (g_MediatorSelectedVariantIndexHigh8 & 0xffu) << 24;
    const uint32_t preservedMiddle16 = g_MediatorSelectedWorldIndexLow24 & 0x00ffff00u;
    const uint32_t lowByteOverwrittenWithVariant = g_MediatorSelectedVariantIndexHigh8 & 0xffu;
    return variantHigh8 | preservedMiddle16 | lowByteOverwrittenWithVariant;
}

static bool DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(uint32_t selectionIndex) {
    const uint32_t normalizedSelectionIndex = selectionIndex & 0xffffffffu;
    if ((normalizedSelectionIndex & 0x00ffffffu) == g_MediatorSelectedWorldIndexLow24) {
        return true;
    }
    return normalizedSelectionIndex == DiagnosticMediatorExpectedSelectionDescriptorScratchRequest();
}

static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetDefaultSelectionIndex() -> 0x%06x",
        (unsigned)g_MediatorSelectedWorldIndexLow24);
    return g_MediatorSelectedWorldIndexLow24;
}

static uint32_t g_GetSelectionCallCount = 0;
static void* __thiscall Mediator_GetSelectionDescriptor(MinimalLoginMediatorStub* self, uint32_t selectionIndex) {
    (void)self;

    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = DiagnosticMediatorExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(selectionIndex);
    const char* worldName = matchedConfiguredRequest ? g_MediatorMappedSelectionName : NULL;

    if (!worldName) {
        Log(
            "MediatorStub::GetSelectionDescriptor(selectionIndex=0x%08x low24=0x%06x high8=0x%02x) -> NULL (configuredWorld=0x%06x configuredVariant=0x%02x expectedScratchRequest=0x%08x worldUpperBoundExclusive=%u)",
            (unsigned)selectionIndex,
            (unsigned)low24,
            (unsigned)high8,
            (unsigned)g_MediatorSelectedWorldIndexLow24,
            (unsigned)g_MediatorSelectedVariantIndexHigh8,
            (unsigned)expectedScratchRequest,
            (unsigned)g_MediatorWorldUpperBoundExclusive);
        return NULL;
    }

    g_MediatorSelectionPacked.mappedName = worldName;
    g_MediatorSelectionPacked.selectionId = g_MediatorMappedSelectionId;
    g_MediatorSelectionObject.packed = &g_MediatorSelectionPacked;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        ((low24 == g_MediatorSelectedWorldIndexLow24) ? "low24-world-match" : "other-match");
    Log(
        "MediatorStub::GetSelectionDescriptor(selectionIndex=0x%08x low24=0x%06x high8=0x%02x) -> %p (matchMode=%s mappedName='%s' packedSelectionId=0x%06x configuredWorld=0x%06x configuredVariant=0x%02x expectedScratchRequest=0x%08x)",
        (unsigned)selectionIndex,
        (unsigned)low24,
        (unsigned)high8,
        &g_MediatorSelectionObject,
        matchMode,
        worldName,
        (unsigned)g_MediatorSelectionPacked.selectionId,
        (unsigned)g_MediatorSelectedWorldIndexLow24,
        (unsigned)g_MediatorSelectedVariantIndexHigh8,
        (unsigned)expectedScratchRequest);
    return &g_MediatorSelectionObject;
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
    Log("MediatorStub::GetString0(+0x58) -> '%s'", g_MediatorStringA);
    return g_MediatorStringA;
}

extern "C" const char* Mediator_GetString1_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    Log(
        "MediatorStub::GetString1(+0x60 value='%s') -> '%s'",
        value ? value : "<null>",
        g_MediatorStringB);
    return g_MediatorStringB;
}

__attribute__((naked)) static void Mediator_GetString1() {
    __asm__ volatile(
        "push %%ebx\n\t"
        "mov 8(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "call *%%ebx\n\t"
        "add $8, %%esp\n\t"
        "pop %%ebx\n\t"
        "ret\n\t"
        :
        : "b"(Mediator_GetString1_Impl)
        : "eax");
}

extern "C" const char* Mediator_GetString2_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    Log(
        "MediatorStub::GetString2(+0x5c value='%s') -> '%s'",
        value ? value : "<null>",
        g_MediatorAuthName);
    return g_MediatorAuthName;
}

__attribute__((naked)) static void Mediator_GetString2() {
    __asm__ volatile(
        "push %%ebx\n\t"
        "mov 8(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "call *%%ebx\n\t"
        "add $8, %%esp\n\t"
        "pop %%ebx\n\t"
        "ret\n\t"
        :
        : "b"(Mediator_GetString2_Impl)
        : "eax");
}

static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetArg7VariantUpperBoundExclusive(+0xd8) -> %u",
        (unsigned)g_MediatorVariantUpperBoundExclusive);
    return g_MediatorVariantUpperBoundExclusive;
}

static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    const char* variantName = DiagnosticMediatorVariantNameForIndex(selectionHighByte);
    Log(
        "MediatorStub::MapSelectionName(selectionHighByte=%u) -> '%s'",
        (unsigned)selectionHighByte,
        variantName ? variantName : "<null>");
    return variantName;
}

static uint32_t __thiscall Mediator_IsLauncherSelectionTypeEnabled(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsLauncherSelectionTypeEnabled(+0x54) -> 1");
    return 1;
}

static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
  if (g_GetSelectionCallCount % 5 == 0) { Log("DIAGNOSTIC: GetSelectionDescriptor count = %u", g_GetSelectionCallCount); }
  const char* worldName = DiagnosticMediatorWorldNameForIndex(g_MediatorSelectedWorldIndexLow24);
    if (!worldName ||
        variantIndex >= g_MediatorVariantUpperBoundExclusive ||
        !DiagnosticMediatorVariantIndexMatchesConfiguredSelection(variantIndex)) {
        Log(
            "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x%02x) -> NULL (world='%s' configuredVariant=0x%02x variantUpperBoundExclusive=%u)",
            (unsigned)(variantIndex & 0xffu),
            worldName ? worldName : "<null>",
            (unsigned)g_MediatorSelectedVariantIndexHigh8,
            (unsigned)g_MediatorVariantUpperBoundExclusive);
        return NULL;
    }

    Log(
        "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x%02x) -> '%s'",
        (unsigned)(variantIndex & 0xffu),
        worldName);
    return worldName;
}

static uint32_t __thiscall Mediator_GetVariantState(MinimalLoginMediatorStub* self, int32_t variantIndex) {
    (void)self;
    uint32_t state = 3u;
    if (variantIndex >= 0) {
        const uint32_t unsignedVariantIndex = static_cast<uint32_t>(variantIndex);
        if (unsignedVariantIndex < g_MediatorVariantUpperBoundExclusive &&
            DiagnosticMediatorVariantIndexMatchesConfiguredSelection(unsignedVariantIndex)) {
            state = g_MediatorSelectedVariantState;
        }
    }
    Log(
        "MediatorStub::GetVariantState(+0xe4 variantIndex=%d) -> %u (configuredVariant=0x%02x configuredState=%u)",
        (int)variantIndex,
        (unsigned)state,
        (unsigned)g_MediatorSelectedVariantIndexHigh8,
        (unsigned)g_MediatorSelectedVariantState);
    return state;
}

static uint32_t __thiscall Mediator_GetWorldCount(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetWorldCount(+0xf8) -> %u",
        (unsigned)g_MediatorWorldUpperBoundExclusive);
    return g_MediatorWorldUpperBoundExclusive;
}

static const char* __thiscall Mediator_GetWorldNameByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
  if (g_GetSelectionCallCount % 5 == 0) { Log("DIAGNOSTIC: GetSelectionDescriptor count = %u", g_GetSelectionCallCount); }
  const char* worldName = DiagnosticMediatorWorldNameForIndex(worldIndex);
    Log(
        "MediatorStub::GetWorldNameByIndex(+0xfc worldIndex=0x%06x) -> %s (configuredWorld=0x%06x)",
        (unsigned)(worldIndex & 0x00ffffffu),
        worldName ? worldName : "<null>",
        (unsigned)g_MediatorSelectedWorldIndexLow24);
    return worldName;
}

static uint32_t __thiscall Mediator_GetWorldTypeByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const uint32_t worldType =
        (worldIndex < g_MediatorWorldUpperBoundExclusive &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? g_MediatorSelectedWorldType
            : 0u;
    Log(
        "MediatorStub::GetWorldTypeByIndex(+0x100 worldIndex=0x%06x) -> %u (configuredWorld=0x%06x configuredType=%u)",
        (unsigned)(worldIndex & 0x00ffffffu),
        (unsigned)worldType,
        (unsigned)g_MediatorSelectedWorldIndexLow24,
        (unsigned)g_MediatorSelectedWorldType);
    return worldType;
}

static uint32_t __thiscall Mediator_GetWorldFlag104(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const uint32_t flagValue =
        (worldIndex < g_MediatorWorldUpperBoundExclusive &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? 0u
            : 0u;
    Log(
        "MediatorStub::GetWorldFlag104(+0x104 worldIndex=0x%06x) -> %u",
        (unsigned)(worldIndex & 0x00ffffffu),
        (unsigned)flagValue);
    return flagValue;
}

static const char* __thiscall Mediator_GetWorldExtra108(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const char* value =
        (worldIndex < g_MediatorWorldUpperBoundExclusive &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? g_MediatorMappedVariantName
            : NULL;
    Log(
        "MediatorStub::GetWorldExtra108(+0x108 worldIndex=0x%06x) -> %s",
        (unsigned)(worldIndex & 0x00ffffffu),
        value ? value : "<null>");
    return value;
}

extern "C" void Mediator_ConsumeSelectionContext_Impl(
    MinimalLoginMediatorStub* self,
    void* selectionContext,
    void* returnAddress) {
    g_MediatorRuntimeState.selectionContext0ec = selectionContext;
    g_MediatorRuntimeState.selectionContext0ecCopy = &g_MediatorSelectionContextCopy;
    if (selectionContext) {
        std::memcpy(&g_MediatorSelectionContextCopy, selectionContext, sizeof(g_MediatorSelectionContextCopy));
        g_MediatorSelectionContextCopyValid = true;
    } else {
        std::memset(&g_MediatorSelectionContextCopy, 0, sizeof(g_MediatorSelectionContextCopy));
        g_MediatorSelectionContextCopyValid = false;
    }
    if (self) {
        self->field1C = &g_MediatorSelectionContextCopy;
    }
    ++g_MediatorRuntimeState.selection0ecCount;
    Log(
        "MediatorStub::ConsumeSelectionContext(%p) [count=%u caller=%p copied=%p size=0x%lx valid=%u configuredWorld=0x%06x configuredVariant=0x%02x profile='%s' world='%s']",
        selectionContext,
        (unsigned)g_MediatorRuntimeState.selection0ecCount,
        returnAddress,
        &g_MediatorSelectionContextCopy,
        (unsigned long)sizeof(g_MediatorSelectionContextCopy),
        g_MediatorSelectionContextCopyValid ? 1u : 0u,
        (unsigned)g_MediatorSelectedWorldIndexLow24,
        (unsigned)g_MediatorSelectedVariantIndexHigh8,
        g_MediatorProfileName,
        g_MediatorMappedSelectionName);
    LogPointerWords("ConsumeSelectionContext copied", &g_MediatorSelectionContextCopy, 8);
    const uint32_t* copiedWords = reinterpret_cast<const uint32_t*>(&g_MediatorSelectionContextCopy);
    Log(
        "DIAGNOSTIC: selectionContext[0]=0x%08x (configuredVariant=0x%02x configuredWorld=0x%06x)",
        (unsigned)copiedWords[0],
        (unsigned)g_MediatorSelectedVariantIndexHigh8,
        (unsigned)g_MediatorSelectedWorldIndexLow24);
    LogSelectionContextDetails(&g_MediatorSelectionContextCopy, sizeof(g_MediatorSelectionContextCopy));
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

extern "C" void Mediator_FillLoadingCharacterState120_Impl(
    MinimalLoginMediatorStub* self,
    void* loadingState,
    void* returnAddress) {
    g_MediatorRuntimeState.loadingState120 = loadingState;
    ++g_MediatorRuntimeState.loading120Count;
    Log(
        "MediatorStub::FillLoadingCharacterState(+0x120 out=%p self=%p) [count=%u caller=%p copiedFrom0ec=%u]",
        loadingState,
        self,
        (unsigned)g_MediatorRuntimeState.loading120Count,
        returnAddress,
        g_MediatorSelectionContextCopyValid ? 1u : 0u);
    LogPointerWords("FillLoadingCharacterState self", self, 8);
    LogPointerWords("FillLoadingCharacterState out(before/after stub)", loadingState, 8);
}

__attribute__((naked)) static void Mediator_FillLoadingCharacterState120() {
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
        : "b"(Mediator_FillLoadingCharacterState120_Impl)
        : "eax", "edx");
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

static void* __thiscall Mediator_GetSelectionContextSnapshot(MinimalLoginMediatorStub* self) {
    (void)self;
    ++g_MediatorRuntimeState.profile0f4Count;
    Log(
        "MediatorStub::GetSelectionContextSnapshot(+0xf4) -> %p [count=%u copiedFrom0ec=%u raw0ec=%p]",
        &g_MediatorSelectionContextCopy,
        (unsigned)g_MediatorRuntimeState.profile0f4Count,
        g_MediatorSelectionContextCopyValid ? 1u : 0u,
        g_MediatorRuntimeState.selectionContext0ec);
    LogPointerWords("GetSelectionContextSnapshot copy", &g_MediatorSelectionContextCopy, 8);
    LogSelectionContextDetails(&g_MediatorSelectionContextCopy, sizeof(g_MediatorSelectionContextCopy));
    return &g_MediatorSelectionContextCopy;
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

static bool DiagnosticPopLauncherQueue(DiagnosticLauncherQueue* queue, uint32_t* out0, uint32_t* out1) {
    if (!queue || !queue->current0 || !out0 || !out1) return false;

    uint32_t* current0 = static_cast<uint32_t*>(queue->current0);
    *out0 = current0[0];
    *out1 = current0[1];

    uint8_t* lastPairInBlock = queue->end0 ? (static_cast<uint8_t*>(queue->end0) - 8) : NULL;
    if (static_cast<void*>(current0) != static_cast<void*>(lastPairInBlock)) {
        queue->current0 = current0 + 2;
        return true;
    }

    if (queue->block0) {
        std::free(queue->block0);
    }

    uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
    ++slotsCurrent;
    queue->slotsCurrent = slotsCurrent;
    queue->block0 = *slotsCurrent;
    queue->end0 = queue->block0 ? (static_cast<uint8_t*>(queue->block0) + 0x80) : NULL;
    queue->current0 = queue->block0;
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

static void DiagnosticClearLttcpBinding(MinimalLauncherObjectStub* owner) {
    if (owner && g_DiagnosticLttcpOwner != owner) {
        return;
    }

    delete g_DiagnosticLttcpEngine;
    g_DiagnosticLttcpEngine = NULL;
    g_DiagnosticLttcpOwner = NULL;
}

static mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetOrCreateLttcpEngine(
    MinimalLauncherObjectStub* owner) {
    if (!owner) return NULL;

    if (g_DiagnosticLttcpOwner != owner) {
        DiagnosticClearLttcpBinding(NULL);
        g_DiagnosticLttcpEngine = new mxo::liblttcp::CLTThreadPerClientTCPEngine();
        if (!g_DiagnosticLttcpEngine) {
            Log("DIAGNOSTIC: failed to allocate CLTThreadPerClientTCPEngine sidecar for %p", owner);
            return NULL;
        }
        g_DiagnosticLttcpOwner = owner;
        Log("DIAGNOSTIC: created CLTThreadPerClientTCPEngine sidecar for launcher object %p", owner);
    }

    return g_DiagnosticLttcpEngine;
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

    DiagnosticSetListHeadOccupancy(
        static_cast<DiagnosticIntrusiveListHead*>(self->list80),
        g_DiagnosticLttcpEngine && !g_DiagnosticLttcpEngine->MonitoredPorts().empty());
    DiagnosticSetListHeadOccupancySmall(
        static_cast<DiagnosticIntrusiveListHeadSmall*>(self->list8C),
        g_DiagnosticLttcpEngine && !g_DiagnosticLttcpEngine->WorkerThreads().empty());
}

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
    // Future original-name wiring target:
    //
    // mxo::liblttcp::CLTThreadPerClientTCPEngine engine;
    // return engine.MonitorEphemeralUDPPort(
    //     /*outBoundPortHostOrder=*/static_cast<uint16_t*>(arg1),
    //     /*contextKey=*/arg2,
    //     /*ownerContext=*/arg3);
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
    // Keep slot4 as a logged placeholder for now.
    // This still needs stronger static naming/semantics before we route it into liblttcp.
    return 0;
}

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

    // Future original-name wiring target on the non-empty path:
    //
    // mxo::liblttcp::CLTThreadPerClientTCPEngine engine;
    // return engine.UnmonitorPort(
    //     /*portHostOrder=*/static_cast<uint16_t>(reinterpret_cast<uintptr_t>(arg1)),
    //     /*ipv4NetworkOrder=*/static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg3)),
    //     /*outSocketHandle=*/out0);
    Log("LauncherObjectStub::Slot5_431840 -> non-empty list80 not reconstructed yet, returning neutral 0");
    return 0;
}

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
        mxo::liblttcp::CMessageConnection* connection = engine->GetOrCreateMessageConnection(arg1);
        result = connection ? connection->EnsureConnected() : 0u;
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot6_4328A0 -> sidecar Connect result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
}

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
        mxo::liblttcp::CMessageConnection* connection = engine->GetOrCreateMessageConnection(arg1);
        result = connection ? connection->CloseConnection(/*graceful=*/(arg2 != 0u)) : 0u;
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot7_42F970 -> sidecar Close result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
}

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
        mxo::liblttcp::CMessageConnection* connection = engine->GetOrCreateMessageConnection(arg1);
        result = connection
            ? connection->SendPacket(
                /*packetData=*/arg2,
                /*packetByteCount=*/static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg3)),
                /*completionContext=*/arg4)
            : 0u;
        DiagnosticSyncLauncherObjectSidecarState(self);
    }

    Log("LauncherObjectStub::Slot8_42FBD0 -> sidecar SendPacket/SendBuffer result=0x%08x context=%p", (unsigned)result, arg1);
    return result;
}

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

static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self);
static uint32_t __thiscall LauncherObject_Subobject60_Slot1(void* self);
static uint32_t __thiscall LauncherObject_Subobject98_Slot0(void* self);
static uint32_t __thiscall LauncherObject_Subobject98_Slot1(void* self);

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

static uint32_t __thiscall LauncherObject_Slot12_4316A0(
    MinimalLauncherObjectStub* self,
    void* arg1) {
    ++g_LauncherObjectBuildState.slot12CallCount;
    LauncherObject_Subobject98_Slot0(&self->helper98);

    bool droppedConnection = false;
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine = DiagnosticGetOrCreateLttcpEngine(self);
    const uint32_t cleanupResult = engine ? engine->CleanupConnection(/*contextKey=*/arg1) : 0u;
    if (cleanupResult != 0u && engine) {
        droppedConnection = engine->DropMessageConnection(arg1);
    }
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

static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

static MinimalLauncherObjectStub* DiagnosticLauncherObjectFromHelper(void* helperSelf, size_t helperOffset) {
    return helperSelf
        ? reinterpret_cast<MinimalLauncherObjectStub*>(static_cast<unsigned char*>(helperSelf) - helperOffset)
        : NULL;
}

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

static uint32_t __thiscall LauncherObject_Subobject60_Slot0(void* self) {
    ++g_LauncherObjectBuildState.subobject60Slot0CallCount;
    CRITICAL_SECTION* crit = DiagnosticLauncherCritFromHelper(self);
    if (crit) {
        EnterCriticalSection(crit);
    }
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
            DiagnosticLauncherObjectFromHelper(self, 0x60),
            count);
    }
    return 0;
}

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
    g_LoginMediatorVtable[15] = (void*)Mediator_GetDefaultSelectionIndex; // +0x3c
    g_LoginMediatorVtable[16] = (void*)Mediator_GetSelectionDescriptor; // +0x40
    g_LoginMediatorVtable[18] = (void*)Mediator_GetWorldOrSelectionName; // +0x48
    g_LoginMediatorVtable[19] = (void*)Mediator_GetProfileOrSessionName; // +0x4c
    g_LoginMediatorVtable[21] = (void*)Mediator_IsLauncherSelectionTypeEnabled; // +0x54
    g_LoginMediatorVtable[22] = (void*)Mediator_GetString0;      // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetString2;      // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetString1;      // +0x60
    g_LoginMediatorVtable[54] = (void*)Mediator_GetArg7SelectionUpperBoundExclusive; // +0xd8
    g_LoginMediatorVtable[55] = (void*)Mediator_MapSelectionName;     // +0xdc
    g_LoginMediatorVtable[56] = (void*)Mediator_GetVariantWorldName; // +0xe0
    g_LoginMediatorVtable[57] = (void*)Mediator_GetVariantState; // +0xe4
    g_LoginMediatorVtable[59] = (void*)Mediator_ConsumeSelectionContext; // +0xec
    g_LoginMediatorVtable[61] = (void*)Mediator_GetSelectionContextSnapshot; // +0xf4
    g_LoginMediatorVtable[62] = (void*)Mediator_GetWorldCount; // +0xf8
    g_LoginMediatorVtable[63] = (void*)Mediator_GetWorldNameByIndex; // +0xfc
    g_LoginMediatorVtable[64] = (void*)Mediator_GetWorldTypeByIndex; // +0x100
    g_LoginMediatorVtable[65] = (void*)Mediator_GetWorldFlag104; // +0x104
    g_LoginMediatorVtable[66] = (void*)Mediator_GetWorldExtra108; // +0x108
    g_LoginMediatorVtable[72] = (void*)Mediator_FillLoadingCharacterState120; // +0x120
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

void DiagnosticConfigureMediatorSelection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedWorldType,
    uint32_t selectedVariantState) {
    g_MediatorWorldUpperBoundExclusive = worldUpperBoundExclusive ? worldUpperBoundExclusive : 1;
    g_MediatorVariantUpperBoundExclusive = variantUpperBoundExclusive ? variantUpperBoundExclusive : 1;
    g_MediatorSelectedWorldIndexLow24 = selectedWorldIndexLow24 & 0x00ffffffu;
    g_MediatorSelectedVariantIndexHigh8 = selectedVariantIndexHigh8 & 0xffu;
    g_MediatorSelectedWorldType = selectedWorldType;
    g_MediatorSelectedVariantState = selectedVariantState;
    g_MediatorMappedSelectionId = g_MediatorSelectedWorldIndexLow24;
    g_MediatorMappedSelectionName =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : g_MediatorStringC;
    g_MediatorMappedVariantName =
        (mappedVariantName && mappedVariantName[0]) ? mappedVariantName : g_MediatorMappedSelectionName;
    g_MediatorSelectionPacked.mappedName = g_MediatorMappedSelectionName;
    g_MediatorSelectionPacked.selectionId = g_MediatorMappedSelectionId;

    Log(
        "DIAGNOSTIC: mediator selection configured worldUpperBoundExclusive=%u variantUpperBoundExclusive=%u worldName='%s' variantName='%s' selectedWorldLow24=0x%06x selectedVariantHigh8=0x%02x selectedWorldType=%u selectedVariantState=%u",
        (unsigned)g_MediatorWorldUpperBoundExclusive,
        (unsigned)g_MediatorVariantUpperBoundExclusive,
        g_MediatorMappedSelectionName,
        g_MediatorMappedVariantName,
        (unsigned)g_MediatorSelectedWorldIndexLow24,
        (unsigned)g_MediatorSelectedVariantIndexHigh8,
        (unsigned)g_MediatorSelectedWorldType,
        (unsigned)g_MediatorSelectedVariantState);
}

bool DiagnosticResolveLauncherSelectionFromMediator(
    void* mediatorPtr,
    uint32_t requestedWorldIndexLow24,
    uint32_t requestedVariantIndexHigh8,
    uint32_t* outFieldA8,
    uint32_t* outFieldAC,
    char* outWorldName,
    uint32_t outWorldNameCapacity) {
    if (!mediatorPtr || !outFieldA8 || !outFieldAC) {
        Log("DIAGNOSTIC: launcher selection resolve skipped (mediator=%p outA8=%p outAC=%p)", mediatorPtr, outFieldA8, outFieldAC);
        return false;
    }

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[21] || !vtable[57] || !vtable[63] || !vtable[64]) {
        Log("DIAGNOSTIC: launcher selection resolve missing mediator slots (+0x54/+0xe4/+0xfc/+0x100)");
        return false;
    }

    typedef uint32_t (__thiscall *NoArgUIntFn)(void*);
    typedef const char* (__thiscall *IndexStringFn)(void*, uint32_t);
    typedef uint32_t (__thiscall *IndexUIntFn)(void*, uint32_t);
    typedef uint32_t (__thiscall *SignedIndexUIntFn)(void*, int32_t);

    NoArgUIntFn allowSpecialTypeFn = (NoArgUIntFn)vtable[21];       // +0x54
    IndexStringFn worldNameFn = (IndexStringFn)vtable[63];         // +0xfc
    IndexUIntFn worldTypeFn = (IndexUIntFn)vtable[64];             // +0x100
    SignedIndexUIntFn variantStateFn = (SignedIndexUIntFn)vtable[57]; // +0xe4

    const uint32_t worldIndexLow24 = requestedWorldIndexLow24 & 0x00ffffffu;
    const uint32_t variantIndexHigh8 = requestedVariantIndexHigh8 & 0xffu;
    const char* worldName = worldNameFn(mediatorPtr, worldIndexLow24);
    const uint32_t worldType = worldTypeFn(mediatorPtr, worldIndexLow24);

    bool typeAccepted = false;
    if (worldType == 1u) {
        typeAccepted = true;
    } else if (worldType == 2u || worldType == 5u) {
        typeAccepted = allowSpecialTypeFn(mediatorPtr) != 0;
    }

    const int32_t signedVariantIndex = static_cast<int32_t>(variantIndexHigh8);
    const uint32_t variantState = variantStateFn(mediatorPtr, signedVariantIndex);
    const bool variantAccepted = (variantState == 0u || variantState == 7u);

    if (!worldName || !typeAccepted || !variantAccepted) {
        Log(
            "DIAGNOSTIC: launcher selection resolve failed worldIndexLow24=0x%06x variantIndexHigh8=0x%02x worldName=%s worldType=%u typeAccepted=%u variantState=%u variantAccepted=%u",
            (unsigned)worldIndexLow24,
            (unsigned)variantIndexHigh8,
            worldName ? worldName : "<null>",
            (unsigned)worldType,
            typeAccepted ? 1u : 0u,
            (unsigned)variantState,
            variantAccepted ? 1u : 0u);
        return false;
    }

    *outFieldA8 = variantIndexHigh8;
    *outFieldAC = worldIndexLow24;

    if (outWorldName && outWorldNameCapacity) {
        outWorldName[0] = '\0';
        std::strncpy(outWorldName, worldName, outWorldNameCapacity - 1);
        outWorldName[outWorldNameCapacity - 1] = '\0';
    }

    Log(
        "DIAGNOSTIC: launcher-style selection resolve via mediator worldIndexLow24=0x%06x variantIndexHigh8=0x%02x -> worldName='%s' worldType=%u variantState=%u a8=0x%08x ac=0x%08x packed=0x%08x",
        (unsigned)worldIndexLow24,
        (unsigned)variantIndexHigh8,
        worldName,
        (unsigned)worldType,
        (unsigned)variantState,
        (unsigned)*outFieldA8,
        (unsigned)*outFieldAC,
        (unsigned)((*outFieldAC & 0x00ffffffu) | ((*outFieldA8 & 0xffu) << 24)));
    return true;
}

void DiagnosticConfigureMediatorProfileName(const char* profileName) {
    g_MediatorProfileName =
        (profileName && profileName[0]) ? profileName : g_MediatorStringA;

    Log("DIAGNOSTIC: mediator profile/session name configured as '%s'", g_MediatorProfileName);
}

void DiagnosticConfigureMediatorAuthName(const char* authName) {
    g_MediatorAuthName =
        (authName && authName[0]) ? authName : g_MediatorProfileName;

    Log("DIAGNOSTIC: mediator auth-name chain (+0x5c) configured as '%s'", g_MediatorAuthName);
}

void DiagnosticApplyDefaultNopatchMediatorConfig(
    void* mediatorPtr,
    uint32_t parsedNoPatchValue,
    uint32_t clientVersionValue) {
    if (!mediatorPtr) return;

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[7] || !vtable[9]) {
        Log("DIAGNOSTIC: mediator nopatch slots unavailable");
        return;
    }

    typedef void (__thiscall *SetValueFn)(void*, void*);
    SetValueFn setValue1 = (SetValueFn)vtable[7];
    SetValueFn setValue2 = (SetValueFn)vtable[9];

    setValue1(mediatorPtr, (void*)&parsedNoPatchValue);
    Log("DIAGNOSTIC: applied default nopatch mediator +0x1c with value 0x%08x", parsedNoPatchValue);

    setValue2(mediatorPtr, (void*)&clientVersionValue);
    Log("DIAGNOSTIC: applied default nopatch mediator +0x24 with value 0x%08x", clientVersionValue);
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
