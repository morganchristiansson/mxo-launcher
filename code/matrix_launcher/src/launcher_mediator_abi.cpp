#include "diagnostics.h"
#include "diagnostics_auth.h"
#include "loginmediator.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

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

static MinimalLoginMediatorStub g_LoginMediatorStub = {};
static DiagnosticMediatorResolverNode g_DiagnosticMediatorResolver = {};
static DiagnosticBinderRegistry g_DiagnosticBinderRegistry = {};
static DiagnosticBinderWrapper g_DiagnosticBinderWrapper = {};
static DiagnosticMediatorRuntimeState g_MediatorRuntimeState = {};
static void* g_LoginMediatorVtable[96] = {0};
static const char g_MediatorName[] = "ILTLoginMediator.Default";
static const char g_MediatorStringA[] = "resurrections";
static const char g_MediatorStringC[] = "standalone";
static const char g_MediatorEmptyString[] = "";
static mxo::ltlogin::CLTLoginMediator* g_DiagnosticMediatorModel = NULL;

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

// UNANCHORED: diagnostic masking helper for auth/password log surfaces.
static const char* MaskedSensitiveValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

// UNANCHORED: sidecar-model accessor for the replacement arg6 ABI shell.
static mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel() {
    if (!g_DiagnosticMediatorModel) {
        g_DiagnosticMediatorModel = new mxo::ltlogin::CLTLoginMediator();
    }
    return g_DiagnosticMediatorModel;
}

// UNANCHORED: trivial accessors into the recovered CLTLoginMediator sidecar model.
static uint32_t DiagnosticMediatorWorldUpperBoundExclusive() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6WorldUpperBoundExclusive() : 1u;
}

static uint32_t DiagnosticMediatorVariantUpperBoundExclusive() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6VariantUpperBoundExclusive() : 1u;
}

static uint32_t DiagnosticMediatorSelectedWorldIndexLow24() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectedWorldIndexLow24() : 0u;
}

static uint32_t DiagnosticMediatorSelectedVariantIndexHigh8() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectedVariantIndexHigh8() : 0u;
}

static uint32_t DiagnosticMediatorSelectedWorldType() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectedWorldType() : 1u;
}

static uint32_t DiagnosticMediatorSelectedVariantState() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectedVariantState() : 0u;
}

static uint32_t DiagnosticMediatorMappedSelectionId() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6MappedSelectionId() : 0u;
}

static const char* DiagnosticMediatorMappedSelectionName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6MappedSelectionName() : g_MediatorStringC;
}

static const char* DiagnosticMediatorMappedVariantName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6MappedVariantName() : DiagnosticMediatorMappedSelectionName();
}

static const char* DiagnosticMediatorProfileName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6ProfileName() : g_MediatorStringA;
}

static const char* DiagnosticMediatorAuthName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthName() : DiagnosticMediatorProfileName();
}

static const char* DiagnosticMediatorAuthPassword() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthPassword() : g_MediatorEmptyString;
}

// UNANCHORED: masks reflected password arguments in mediator-chain logs.
static const char* MaskIfMediatorPassword(const char* value) {
    if (!value) return "<null>";
    const char* authPassword = DiagnosticMediatorAuthPassword();
    if (authPassword && authPassword[0] && std::strcmp(value, authPassword) == 0) {
        return "<provided>";
    }
    return value;
}

// UNANCHORED: generic pointer-word dumper used by mediator diagnostics.
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

// UNANCHORED: shared diagnostic log-throttling helper.
static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

// UNANCHORED: generic dword-buffer logger for copied mediator state blobs.
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

// UNANCHORED: heuristic ascii detector for selection-context dumps.
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

// UNANCHORED: expanded logger for the copied +0xec selection-context handoff.
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

// UNANCHORED: resets the replacement mediator object and sidecar model to default state.
static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    std::memset(&g_MediatorSelectionObject, 0, sizeof(g_MediatorSelectionObject));
    std::memset(&g_MediatorSelectionContextCopy, 0, sizeof(g_MediatorSelectionContextCopy));
    g_MediatorSelectionContextCopyValid = false;
    delete g_DiagnosticMediatorModel;
    g_DiagnosticMediatorModel = new mxo::ltlogin::CLTLoginMediator();
    g_MediatorSelectionPacked = {0, 0, 0, g_MediatorStringC, 0};
    g_LoginMediatorStub.vtable = g_LoginMediatorVtable;
}

// anchor: launcher.exe dynamic initializer uses the registration string at 0x4ab34c for ILTLoginMediator.Default
// vtable: ILTLoginMediator.Default slot +0x00
static const char* __thiscall Mediator_GetName(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorName;
}

// anchor: launcher.exe:0x40a3e9..0x40a3fe hands the freshly built 0x4d6304 object into arg6 before InitClientDLL
// vtable: ILTLoginMediator.Default slot +0x08
static int __thiscall Mediator_RegisterEngine(MinimalLoginMediatorStub* self, void* object) {
    g_MediatorRuntimeState.registeredLauncherObject = object;
    if (self) {
        self->field04 = object;
    }
    Log("MediatorStub::RegisterEngine(%p)", object);
    return 1;
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 clears the registered launcher object through arg6
// vtable: ILTLoginMediator.Default slot +0x0c
static void __thiscall Mediator_ClearEngine(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::ClearEngine()");
}

// anchor: client.dll early InitClientDLL readiness gate on arg6 +0x10
// vtable: ILTLoginMediator.Default slot +0x10
static uint32_t __thiscall Mediator_IsReady(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsReady() -> 1");
    return 1;
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
static void __thiscall Mediator_SetValue1(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    if (!g_MediatorRuntimeState.lastNopatchValue1Ptr) {
        g_MediatorRuntimeState.lastNopatchValue1Ptr = value;
    } else {
        g_MediatorRuntimeState.lastNopatchValue2Ptr = value;
    }
    Log("MediatorStub::SetValue1(%p)", value);
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator.Default slot +0x2c
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

// anchor: client.dll profile-root formatting path uses arg6 +0x38 for Profiles\%s\... construction
// vtable: ILTLoginMediator.Default slot +0x38
static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetProfileRootName(+0x38) -> '%s'", DiagnosticMediatorProfileName());
    return DiagnosticMediatorProfileName();
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex);
static bool DiagnosticMediatorVariantIndexMatchesConfiguredSelection(uint32_t variantIndex);

// UNANCHORED: scaffold selection-resolution helpers layered over the CLTLoginMediator sidecar model.
static const char* DiagnosticMediatorWorldNameForIndex(uint32_t worldIndex) {
    if (worldIndex >= DiagnosticMediatorWorldUpperBoundExclusive()) {
        return NULL;
    }
    if (!DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex)) {
        return NULL;
    }
    return DiagnosticMediatorMappedSelectionName();
}

static const char* DiagnosticMediatorVariantNameForIndex(uint32_t variantIndex) {
    if (variantIndex >= DiagnosticMediatorVariantUpperBoundExclusive()) {
        return NULL;
    }
    if (!DiagnosticMediatorVariantIndexMatchesConfiguredSelection(variantIndex)) {
        return NULL;
    }
    return DiagnosticMediatorMappedVariantName();
}

static bool DiagnosticMediatorWorldIndexMatchesConfiguredSelection(uint32_t worldIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6WorldIndexMatchesSelection(worldIndex) : false;
}

static bool DiagnosticMediatorVariantIndexMatchesConfiguredSelection(uint32_t variantIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6VariantIndexMatchesSelection(variantIndex) : false;
}

static uint32_t DiagnosticMediatorExpectedSelectionDescriptorScratchRequest() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6ExpectedSelectionDescriptorScratchRequest() : 0u;
}

static bool DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(uint32_t selectionIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6SelectionDescriptorMatchesRequest(selectionIndex) : false;
}

// anchor: client.dll fallback-selection path asks arg6 +0x3c for the default selection index when given 0xff
// vtable: ILTLoginMediator.Default slot +0x3c
static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetDefaultSelectionIndex() -> 0x%06x",
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24());
    return DiagnosticMediatorSelectedWorldIndexLow24();
}

static uint32_t g_GetSelectionCallCount = 0;
// anchor: client.dll:0x62170dc1..0x62170e59 later asks arg6 +0x40 with the scratch-shaped arg7 request
// vtable: ILTLoginMediator.Default slot +0x40
static void* __thiscall Mediator_GetSelectionDescriptor(MinimalLoginMediatorStub* self, uint32_t selectionIndex) {
    (void)self;

    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = DiagnosticMediatorExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = DiagnosticMediatorSelectionDescriptorMatchesConfiguredRequest(selectionIndex);
    const char* worldName = matchedConfiguredRequest ? DiagnosticMediatorMappedSelectionName() : NULL;

    if (!worldName) {
        Log(
            "MediatorStub::GetSelectionDescriptor(selectionIndex=0x%08x low24=0x%06x high8=0x%02x) -> NULL (configuredWorld=0x%06x configuredVariant=0x%02x expectedScratchRequest=0x%08x worldUpperBoundExclusive=%u)",
            (unsigned)selectionIndex,
            (unsigned)low24,
            (unsigned)high8,
            (unsigned)DiagnosticMediatorSelectedWorldIndexLow24(),
            (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
            (unsigned)expectedScratchRequest,
            (unsigned)DiagnosticMediatorWorldUpperBoundExclusive());
        return NULL;
    }

    g_MediatorSelectionPacked.mappedName = worldName;
    g_MediatorSelectionPacked.selectionId = DiagnosticMediatorMappedSelectionId();
    g_MediatorSelectionObject.packed = &g_MediatorSelectionPacked;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        ((low24 == DiagnosticMediatorSelectedWorldIndexLow24()) ? "low24-world-match" : "other-match");
    Log(
        "MediatorStub::GetSelectionDescriptor(selectionIndex=0x%08x low24=0x%06x high8=0x%02x) -> %p (matchMode=%s mappedName='%s' packedSelectionId=0x%06x configuredWorld=0x%06x configuredVariant=0x%02x expectedScratchRequest=0x%08x)",
        (unsigned)selectionIndex,
        (unsigned)low24,
        (unsigned)high8,
        &g_MediatorSelectionObject,
        matchMode,
        worldName,
        (unsigned)g_MediatorSelectionPacked.selectionId,
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24(),
        (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
        (unsigned)expectedScratchRequest);
    return &g_MediatorSelectionObject;
}

// anchor: later client startup path calls arg6 +0x48 before AttachStartupContext/ProvideStartupTriple
// vtable: ILTLoginMediator.Default slot +0x48
static const char* __thiscall Mediator_GetWorldOrSelectionName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetWorldOrSelectionName() -> '%s'", DiagnosticMediatorMappedSelectionName());
    return DiagnosticMediatorMappedSelectionName();
}

// anchor: later client startup path calls arg6 +0x4c immediately after +0x48
// vtable: ILTLoginMediator.Default slot +0x4c
static const char* __thiscall Mediator_GetProfileOrSessionName(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetProfileOrSessionName() -> '%s'", DiagnosticMediatorProfileName());
    return DiagnosticMediatorProfileName();
}

// anchor: client.dll early auth-name chain at 0x62001325..0x62001362 first calls arg6 +0x58
// vtable: ILTLoginMediator.Default slot +0x58
static const char* __thiscall Mediator_GetString0(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::GetString0(+0x58) -> '%s'", g_MediatorStringA);
    return g_MediatorStringA;
}

// UNANCHORED: C helper behind the caller-clean +0x60 ABI wrapper.
extern "C" const char* Mediator_GetString1_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    Log(
        "MediatorStub::GetString1(+0x60 value='%s') -> %s",
        value ? value : "<null>",
        MaskedSensitiveValue(DiagnosticMediatorAuthPassword()));
    return DiagnosticMediatorAuthPassword();
}

// anchor: client.dll early auth-name chain proves arg6 +0x60 is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x60
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

// UNANCHORED: C helper behind the caller-clean +0x5c ABI wrapper.
extern "C" const char* Mediator_GetString2_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    Log(
        "MediatorStub::GetString2(+0x5c value='%s') -> '%s'",
        MaskIfMediatorPassword(value),
        DiagnosticMediatorAuthName());
    return DiagnosticMediatorAuthName();
}

// anchor: client.dll early auth-name chain proves arg6 +0x5c is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x5c
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

// anchor: client.dll:0x62170b00 gates arg7 high-byte selection flow through arg6 +0xd8
// vtable: ILTLoginMediator.Default slot +0xd8
static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetArg7VariantUpperBoundExclusive(+0xd8) -> %u",
        (unsigned)DiagnosticMediatorVariantUpperBoundExclusive());
    return DiagnosticMediatorVariantUpperBoundExclusive();
}

// anchor: deeper client init maps arg7-derived selection names through arg6 +0xdc
// vtable: ILTLoginMediator.Default slot +0xdc
static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    const char* variantName = DiagnosticMediatorVariantNameForIndex(selectionHighByte);
    Log(
        "MediatorStub::MapSelectionName(selectionHighByte=%u) -> '%s'",
        (unsigned)selectionHighByte,
        variantName ? variantName : "<null>");
    return variantName;
}

// anchor: launcher selection resolution checks arg6 +0x54 before accepting world types 2/5
// vtable: ILTLoginMediator.Default slot +0x54
static uint32_t __thiscall Mediator_IsLauncherSelectionTypeEnabled(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsLauncherSelectionTypeEnabled(+0x54) -> 1");
    return 1;
}

// anchor: arg7-selection resolution consults the sibling ILTLoginMediator surface through +0xe0
// vtable: ILTLoginMediator.Default slot +0xe0
static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
    if (g_GetSelectionCallCount % 5 == 0) { Log("DIAGNOSTIC: GetSelectionDescriptor count = %u", g_GetSelectionCallCount); }
    const char* worldName = DiagnosticMediatorWorldNameForIndex(DiagnosticMediatorSelectedWorldIndexLow24());
    if (!worldName ||
        variantIndex >= DiagnosticMediatorVariantUpperBoundExclusive() ||
        !DiagnosticMediatorVariantIndexMatchesConfiguredSelection(variantIndex)) {
        Log(
            "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x%02x) -> NULL (world='%s' configuredVariant=0x%02x variantUpperBoundExclusive=%u)",
            (unsigned)(variantIndex & 0xffu),
            worldName ? worldName : "<null>",
            (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
            (unsigned)DiagnosticMediatorVariantUpperBoundExclusive());
        return NULL;
    }

    Log(
        "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x%02x) -> '%s'",
        (unsigned)(variantIndex & 0xffu),
        worldName);
    return worldName;
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0xe4
// vtable: ILTLoginMediator.Default slot +0xe4
static uint32_t __thiscall Mediator_GetVariantState(MinimalLoginMediatorStub* self, int32_t variantIndex) {
    (void)self;
    uint32_t state = 3u;
    if (variantIndex >= 0) {
        const uint32_t unsignedVariantIndex = static_cast<uint32_t>(variantIndex);
        if (unsignedVariantIndex < DiagnosticMediatorVariantUpperBoundExclusive() &&
            DiagnosticMediatorVariantIndexMatchesConfiguredSelection(unsignedVariantIndex)) {
            state = DiagnosticMediatorSelectedVariantState();
        }
    }
    Log(
        "MediatorStub::GetVariantState(+0xe4 variantIndex=%d) -> %u (configuredVariant=0x%02x configuredState=%u)",
        (int)variantIndex,
        (unsigned)state,
        (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
        (unsigned)DiagnosticMediatorSelectedVariantState());
    return state;
}

// anchor: launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// vtable: launcher.exe:0x4d3584 slot +0xf8
static uint32_t __thiscall Mediator_GetWorldCount(MinimalLoginMediatorStub* self) {
    (void)self;
    Log(
        "MediatorStub::GetWorldCount(+0xf8) -> %u",
        (unsigned)DiagnosticMediatorWorldUpperBoundExclusive());
    return DiagnosticMediatorWorldUpperBoundExclusive();
}

// anchor: launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex
// vtable: launcher.exe:0x4d3584 slot +0xfc
static const char* __thiscall Mediator_GetWorldNameByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
    if (g_GetSelectionCallCount % 5 == 0) { Log("DIAGNOSTIC: GetSelectionDescriptor count = %u", g_GetSelectionCallCount); }
    const char* worldName = DiagnosticMediatorWorldNameForIndex(worldIndex);
    Log(
        "MediatorStub::GetWorldNameByIndex(+0xfc worldIndex=0x%06x) -> %s (configuredWorld=0x%06x)",
        (unsigned)(worldIndex & 0x00ffffffu),
        worldName ? worldName : "<null>",
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24());
    return worldName;
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0x100
// vtable: launcher.exe:0x4d3584 slot +0x100
static uint32_t __thiscall Mediator_GetWorldTypeByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const uint32_t worldType =
        (worldIndex < DiagnosticMediatorWorldUpperBoundExclusive() &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? DiagnosticMediatorSelectedWorldType()
            : 0u;
    Log(
        "MediatorStub::GetWorldTypeByIndex(+0x100 worldIndex=0x%06x) -> %u (configuredWorld=0x%06x configuredType=%u)",
        (unsigned)(worldIndex & 0x00ffffffu),
        (unsigned)worldType,
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24(),
        (unsigned)DiagnosticMediatorSelectedWorldType());
    return worldType;
}

// anchor: later client/runtime world-descriptor consumers read additional sibling-object fields after +0x100
// vtable: launcher.exe:0x4d3584 slot +0x104
static uint32_t __thiscall Mediator_GetWorldFlag104(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const uint32_t flagValue =
        (worldIndex < DiagnosticMediatorWorldUpperBoundExclusive() &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? 0u
            : 0u;
    Log(
        "MediatorStub::GetWorldFlag104(+0x104 worldIndex=0x%06x) -> %u",
        (unsigned)(worldIndex & 0x00ffffffu),
        (unsigned)flagValue);
    return flagValue;
}

// anchor: later client/runtime world-descriptor consumers read additional sibling-object fields after +0x100
// vtable: launcher.exe:0x4d3584 slot +0x108
static const char* __thiscall Mediator_GetWorldExtra108(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    const char* value =
        (worldIndex < DiagnosticMediatorWorldUpperBoundExclusive() &&
         DiagnosticMediatorWorldIndexMatchesConfiguredSelection(worldIndex))
            ? DiagnosticMediatorMappedVariantName()
            : NULL;
    Log(
        "MediatorStub::GetWorldExtra108(+0x108 worldIndex=0x%06x) -> %s",
        (unsigned)(worldIndex & 0x00ffffffu),
        value ? value : "<null>");
    return value;
}

// UNANCHORED: C helper behind the recovered +0xec ABI wrapper.
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
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24(),
        (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
        DiagnosticMediatorProfileName(),
        DiagnosticMediatorMappedSelectionName());
    LogPointerWords("ConsumeSelectionContext copied", &g_MediatorSelectionContextCopy, 8);
    const uint32_t* copiedWords = reinterpret_cast<const uint32_t*>(&g_MediatorSelectionContextCopy);
    Log(
        "DIAGNOSTIC: selectionContext[0]=0x%08x (configuredVariant=0x%02x configuredWorld=0x%06x)",
        (unsigned)copiedWords[0],
        (unsigned)DiagnosticMediatorSelectedVariantIndexHigh8(),
        (unsigned)DiagnosticMediatorSelectedWorldIndexLow24());
    LogSelectionContextDetails(&g_MediatorSelectionContextCopy, sizeof(g_MediatorSelectionContextCopy));
}

// anchor: client.dll:0x62170f48 consumes the assembled 0xb4 selection/config handoff through arg6 +0xec
// vtable: ILTLoginMediator.Default slot +0xec
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

// UNANCHORED: C helper behind the recovered +0x124 ABI wrapper.
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

// anchor: deeper client init hands netShell/netMgr/distrObjExecutive to arg6 +0x124
// vtable: ILTLoginMediator.Default slot +0x124
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

// UNANCHORED: C helper behind the recovered +0x120 ABI wrapper.
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

// anchor: later loading-character path around client.dll:0x620547c0..0x62054eac passes a large state object to arg6 +0x120
// vtable: ILTLoginMediator.Default slot +0x120
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

// UNANCHORED: C helper behind the recovered +0x170 ABI wrapper.
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

// anchor: deeper client init attaches startup context to arg6 through +0x170 before/after +0x124
// vtable: ILTLoginMediator.Default slot +0x170
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

// anchor: later runtime/config paths treat arg6 +0xf4 as a persisted selection/config snapshot
// vtable: ILTLoginMediator.Default slot +0xf4
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

// anchor: later runtime setup uses arg6 +0x148 and +0x174 for runtime-object handoff
// vtable: ILTLoginMediator.Default slots +0x148 and +0x174
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

// anchor: later runtime setup uses arg6 +0x178 for runtime-descriptor handoff
// vtable: ILTLoginMediator.Default slot +0x178
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

// anchor: launcher.exe teardown path 0x40b360..0x40b409 conditionally checks arg6 +0x164
// vtable: ILTLoginMediator.Default slot +0x164
static uint32_t __thiscall Mediator_ShouldExportA(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 conditionally checks arg6 +0x16c
// vtable: ILTLoginMediator.Default slot +0x16c
static uint32_t __thiscall Mediator_ShouldExportB(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

// UNANCHORED: seeds the replacement ILTLoginMediator.Default ABI vtable from recovered slot usage.
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

// UNANCHORED: diagnostic binder scaffold that mimics launcher-side interface materialization for arg6.
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

// UNANCHORED: binder-scaffold lookup over the replacement resolver list.
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

// UNANCHORED: binder-scaffold resolver that writes the materialized arg6 pointer into the caller slot.
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

// UNANCHORED: direct diagnostic installer for the replacement arg6 mediator stub.
void DiagnosticInstallMediatorStub(void** outMediatorPtr) {
    InitializeMediatorStub();
    if (outMediatorPtr) {
        *outMediatorPtr = &g_LoginMediatorStub;
    }
    Log("DIAGNOSTIC: using MinimalLoginMediatorStub for arg6 (%p)", &g_LoginMediatorStub);
}

// UNANCHORED: diagnostic binder-backed installer for the replacement arg6 mediator stub.
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

// UNANCHORED: diagnostic selection configurator for the replacement arg6 sidecar model.
void DiagnosticConfigureMediatorSelection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedWorldType,
    uint32_t selectedVariantState) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        Log("DIAGNOSTIC: mediator selection configuration skipped (no scaffold model)");
        return;
    }

    mediator->ConfigureArg6Selection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        mappedSelectionName,
        mappedVariantName,
        selectedWorldIndexLow24,
        selectedVariantIndexHigh8,
        selectedWorldType,
        selectedVariantState);
    g_MediatorSelectionPacked.mappedName = mediator->Arg6MappedSelectionName();
    g_MediatorSelectionPacked.selectionId = mediator->Arg6MappedSelectionId();

    Log(
        "DIAGNOSTIC: mediator selection configured worldUpperBoundExclusive=%u variantUpperBoundExclusive=%u worldName='%s' variantName='%s' selectedWorldLow24=0x%06x selectedVariantHigh8=0x%02x selectedWorldType=%u selectedVariantState=%u",
        (unsigned)mediator->Arg6WorldUpperBoundExclusive(),
        (unsigned)mediator->Arg6VariantUpperBoundExclusive(),
        mediator->Arg6MappedSelectionName(),
        mediator->Arg6MappedVariantName(),
        (unsigned)mediator->Arg6SelectedWorldIndexLow24(),
        (unsigned)mediator->Arg6SelectedVariantIndexHigh8(),
        (unsigned)mediator->Arg6SelectedWorldType(),
        (unsigned)mediator->Arg6SelectedVariantState());
}

// UNANCHORED: launcher-style selection resolver used to model the current arg7 reconstruction path.
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

// UNANCHORED: diagnostic profile/session-name configurator for arg6.
void DiagnosticConfigureMediatorProfileName(const char* profileName) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6ProfileName(profileName);
    }

    Log("DIAGNOSTIC: mediator profile/session name configured as '%s'", DiagnosticMediatorProfileName());
}

// UNANCHORED: diagnostic auth-name configurator for arg6 +0x5c.
void DiagnosticConfigureMediatorAuthName(const char* authName) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6AuthName(authName);
    }

    Log("DIAGNOSTIC: mediator auth-name chain (+0x5c) configured as '%s'", DiagnosticMediatorAuthName());
    DiagnosticAuthSetMediatorCredentials(DiagnosticMediatorAuthName(), DiagnosticMediatorAuthPassword());
}

// UNANCHORED: diagnostic auth-password configurator for arg6 +0x60.
void DiagnosticConfigureMediatorAuthPassword(const char* authPassword) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6AuthPassword(authPassword);
    }

    Log(
        "DIAGNOSTIC: mediator auth-password chain (+0x60) configured as %s",
        MaskedSensitiveValue(DiagnosticMediatorAuthPassword()));
    DiagnosticAuthSetMediatorCredentials(DiagnosticMediatorAuthName(), DiagnosticMediatorAuthPassword());
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures ILTLoginMediator.Default before InitClientDLL
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
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

