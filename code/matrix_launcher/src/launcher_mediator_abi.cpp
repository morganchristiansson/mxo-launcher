#include "diagnostics.h"
#include "diagnostics_auth.h"
#include "launcher_mediator_abi_shared.h"
#include "loginmediator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include <spdlog/spdlog.h>

// Broad ILTLoginMediator.Default ABI shell:
// - keep startup-selection and general arg6 surface here
// - keep late-login state9-only slots (`+0xd4`, `+0x124`, `+0x18c`) in
//   `src/launcher_mediator_state9_abi.cpp`

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

static MinimalLoginMediatorStub g_LoginMediatorStub = {};
static DiagnosticMediatorResolverNode g_DiagnosticMediatorResolver = {};
static DiagnosticBinderRegistry g_DiagnosticBinderRegistry = {};
static DiagnosticBinderWrapper g_DiagnosticBinderWrapper = {};
void* g_LoginMediatorVtable[104] = {0};
static const char g_MediatorStringA[] = "resurrections";
static const char g_MediatorStringC[] = "standalone";
static const char g_MediatorEmptyString[] = "";

static constexpr size_t kDiagnosticSelectionContextSize = 0xb4; // from client.dll:6211d3e0 zero-init of the +0xec handoff object

// UNANCHORED: diagnostic masking helper for auth/password log surfaces.
const char* MaskedSensitiveValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

// UNANCHORED: sidecar-model accessor for the replacement arg6 ABI shell.
mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel() {
    return dynamic_cast<mxo::ltlogin::CLTLoginMediator*>(mxo::ltlogin::ILTLoginMediator::Default);
}

static mxo::ltlogin::CLTLoginMediator* DiagnosticGetActiveMediatorForCharacterState() {
    if (mxo::ltlogin::CLTLoginMediator* loginController = DiagnosticAuthGetLoginController()) {
        return loginController;
    }
    return DiagnosticEnsureMediatorModel();
}

// UNANCHORED: trivial accessors into the recovered CLTLoginMediator sidecar model.
static const char* DiagnosticMediatorMappedSelectionName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6MappedSelectionName() : g_MediatorStringC;
}

static const char* DiagnosticMediatorProfileName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6ProfileName() : g_MediatorStringA;
}

// Observer state accessors (moved from g_MediatorRuntimeState to CLTLoginMediator):
uint32_t DiagnosticMediatorObserverRegisterCount() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->ObserverRegisterCount() : 0u;
}

uint32_t DiagnosticMediatorObserverUnregisterCount() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->ObserverUnregisterCount() : 0u;
}

const char* DiagnosticMediatorAuthName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthName() : DiagnosticMediatorProfileName();
}

const char* DiagnosticMediatorAuthPassword() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthPassword() : g_MediatorEmptyString;
}

static const char* NonEmptyOrPlaceholder(const char* value) {
    return (value && value[0]) ? value : "<empty>";
}

bool IsProfilePathBuilderCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    return address >= 0x62195ff0u && address <= 0x62196121u;
}

static bool IsMcdPersistenceCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    return address >= 0x62197830u && address <= 0x621983d0u;
}

const char* DescribeMediatorCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    if (address >= 0x62170b00u && address <= 0x62170fbbu) {
        return "client.dll:FUN_62170b00 init/selection family";
    }
    if (IsProfilePathBuilderCaller(returnAddress)) {
        return "client.dll:profile-path builder family";
    }
    if (IsMcdPersistenceCaller(returnAddress)) {
        return "client.dll:mcd.cfg persistence family";
    }
    if (address >= 0x620f1c60u && address <= 0x620f202fu) {
        return "client.dll:character-info dialog family";
    }
    if (address >= 0x620547c0u && address <= 0x62054eacu) {
        return "client.dll:loading-character family";
    }
    if (address >= 0x62056500u && address <= 0x62056600u) {
        return "client.dll:observer/late-runtime family";
    }
    if (address >= 0x625c86d0u && address <= 0x625c8700u) {
        return "client.dll:RCC/bootstrap family";
    }
    return "client.dll:<unclassified>";
}

void LogMediatorCharacterStateContext(const char* slotLabel, void* returnAddress) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    if (!mediator) {
        return;
    }

    const auto& ownerState = mediator->PostAuthMarginLoadingStateView();
    const auto* currentState = mediator->CurrentState();
    const auto* currentSlotRecord = mediator->GetCurrentSlotRecord();
    const char* currentSlotName =
        (currentSlotRecord && !currentSlotRecord->heapString14.empty())
            ? currentSlotRecord->heapString14.c_str()
            : "<empty>";
    const char* sourceLeadString108 = ownerState.sourceLeadString108[0]
        ? ownerState.sourceLeadString108.data()
        : "<empty>";
    const char* characterNameBufferF1c = ownerState.characterNameBufferF1c[0]
        ? ownerState.characterNameBufferF1c
        : "<empty>";
    const char* section0StringF8c = ownerState.section0StringF8c[0]
        ? ownerState.section0StringF8c.data()
        : "<empty>";
    const char* section0StringFac = ownerState.section0StringFac[0]
        ? ownerState.section0StringFac.data()
        : "<empty>";
    const char* section0StringFcc = ownerState.section0StringFcc[0]
        ? ownerState.section0StringFcc.data()
        : "<empty>";
    const char* authCharacterName = NonEmptyOrPlaceholder(DiagnosticAuthCurrentCharacterName());
    const uint32_t authCharacterIdLow = DiagnosticAuthCurrentCharacterIdLow();
    const uint32_t authCharacterIdHigh = DiagnosticAuthCurrentCharacterIdHigh();

    spdlog::debug(
        "MediatorStub::{} caller={} [{}] context{{mappedWorld='{}' profile='{}' currentSlot='{}' source108='{}' f1c='{}' section0f8c='{}' section0fac='{}' section0fcc='{}' authChar='{}' authIdLow=0x{:08x} authIdHigh=0x{:08x} authIdLow16=0x{:04x} currentState={} worldId=0x{:04x} status=0x{:02x}}}",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        NonEmptyOrPlaceholder(DiagnosticMediatorMappedSelectionName()),
        NonEmptyOrPlaceholder(DiagnosticMediatorProfileName()),
        currentSlotName,
        sourceLeadString108,
        characterNameBufferF1c,
        section0StringF8c,
        section0StringFac,
        section0StringFcc,
        authCharacterName,
        authCharacterIdLow,
        authCharacterIdHigh,
        static_cast<unsigned>(authCharacterIdLow & 0xffffu),
        fmt::ptr(currentState),
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->worldId0c) : 0u,
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->status0b) : 0u);
}

// Focused source split for the broader ILTLoginMediator.Default ABI shell:
// - keep profile-path / current-slot / selection-descriptor work isolated
// - keep non-mcd selection cfg corpus fallback work isolated
// - keep mcd/persistence work isolated
#include "launcher_mediator_abi_profile_paths.cpp"
#include "launcher_mediator_abi_selection_cfg.cpp"
#include "launcher_mediator_abi_mcd.cpp"

// UNANCHORED: generic pointer-word dumper used by mediator diagnostics.
void LogPointerWords(const char* label, const void* ptr, uint32_t wordCount) {
    if (!ptr || !wordCount) {
        spdlog::info("{}: <null>", label ? label : "PointerWords");
        return;
    }

    const uint32_t* words = static_cast<const uint32_t*>(ptr);
    spdlog::info("{} @ {} [+0x00]=0x{:08x} [+0x04]=0x{:08x} [+0x08]=0x{:08x} [+0x0c]=0x{:08x}",
        label,
        fmt::ptr(ptr),
        words[0],
        (wordCount > 1) ? words[1] : 0,
        (wordCount > 2) ? words[2] : 0,
        (wordCount > 3) ? words[3] : 0);
    if (wordCount > 4) {
        spdlog::info("{} @ {} [+0x10]=0x{:08x} [+0x14]=0x{:08x} [+0x18]=0x{:08x} [+0x1c]=0x{:08x}",
            label,
            fmt::ptr(ptr),
            words[4],
            (wordCount > 5) ? words[5] : 0,
            (wordCount > 6) ? words[6] : 0,
            (wordCount > 7) ? words[7] : 0);
    }
}

// UNANCHORED: generic dword-buffer logger for copied mediator state blobs.
void LogWordBuffer(const char* label, const void* ptr, uint32_t byteCount) {
    if (!label || !ptr || byteCount == 0) {
        return;
    }

    const uint32_t* words = static_cast<const uint32_t*>(ptr);
    const uint32_t wordCount = byteCount / 4;
    for (uint32_t i = 0; i < wordCount; i += 4) {
        spdlog::info(
            "{} @ {} [+0x{:02x}]={:08x} [+0x{:02x}]={:08x} [+0x{:02x}]={:08x} [+0x{:02x}]={:08x}",
            label,
            fmt::ptr(ptr),
            i * 4,
            words[i + 0],
            (i + 1) * 4,
            (i + 1 < wordCount) ? words[i + 1] : 0,
            ((i + 2) * 4),
            (i + 2 < wordCount) ? words[i + 2] : 0,
            ((i + 3) * 4),
            (i + 3 < wordCount) ? words[i + 3] : 0);
    }
}

// UNANCHORED: resets the replacement mediator object and sidecar model to default state.
static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    g_MediatorState8Section11String1460Owned.clear();
    g_MediatorState8Section11String1460 = {};
    delete mxo::ltlogin::ILTLoginMediator::Default;
    mxo::ltlogin::ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();
    g_LoginMediatorStub.vtable = g_LoginMediatorVtable;
}

// anchor: launcher.exe dynamic initializer uses the registration string at 0x4ab34c for ILTLoginMediator.Default
// vtable: ILTLoginMediator.Default slot +0x00
static const char* __thiscall Mediator_GetName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetName();
}

// anchor: launcher.exe:0x40a3e9..0x40a3fe hands the freshly built 0x4d6304 object into arg6 before InitClientDLL
// vtable: ILTLoginMediator.Default slot +0x08
static int __thiscall Mediator_SetNetworkEngine(MinimalLoginMediatorStub* self, void* object) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator::Default->SetNetworkEngine(
        static_cast<mxo::liblttcp::CLTThreadPerClientTCPEngine*>(object));
    return 1;
}

// anchor: client.dll early InitClientDLL readiness gate on arg6 +0x10
// vtable: ILTLoginMediator.Default slot +0x10
static uint32_t __thiscall Mediator_IsReady(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->IsReady();
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 clears the registered launcher object through arg6
// vtable: ILTLoginMediator.Default slot +0x0c
static void __thiscall Mediator_ClearEngine(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->ClearEngine();
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
static void __thiscall Mediator_SetValue1(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator::Default->SetValue1(value);
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
static void __thiscall Mediator_SetValue2(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator::Default->SetValue2(value);
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator.Default slot +0x2c
static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->IsConnected();
}

// anchor: client.dll:0x625c86d0 later calls arg6 +0x50 and converts null/non-null into flag 0x30
// vtable: ILTLoginMediator.Default slot +0x50
static void* __thiscall Mediator_GetBootstrapRaw08AuxHandle50(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->BootstrapRaw08AuxHandle50();
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
// vtable: ILTLoginMediator.Default slot +0x54
static uint32_t __thiscall Mediator_HasBootstrapRaw08AuxHandle54(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasBootstrapRaw08AuxHandle54() ? 1u : 0u;
}

// anchor: client.dll:0x62001325..0x62001362 passes the low byte from arg6 +0x58 into
// `FUN_6236fa40(..., flag)`; launcher.exe:0x409250..0x409254 also stores that low byte into the
// crashreporter `PromptForSecurId` global.
// vtable: ILTLoginMediator.Default slot +0x58
static uint32_t __thiscall Mediator_GetCrashReporterPromptForSecurId58(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetCrashReporterPromptForSecurId58();
}

// UNANCHORED: C helper behind the caller-clean +0x60 ABI wrapper.
// Keep the chained value opaque here:
// - client `InitClientDLL` threads the previous return value through this slot
// - launcher crashreporter seeding calls the same slot with no stack argument on its path
extern "C" const char* Mediator_GetCrashReporterPassword60_Impl(
    MinimalLoginMediatorStub* self,
    const void* chainedValueToken) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetCrashReporterPassword60(chainedValueToken);
}

// anchor: client.dll early auth-name chain proves arg6 +0x60 is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x60
__attribute__((naked)) static void Mediator_GetCrashReporterPassword60() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $8, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetCrashReporterPassword60_Impl)
        : "eax");
}

// UNANCHORED: C helper behind the caller-clean +0x5c ABI wrapper.
// Keep the chained value opaque here for the same launcher/client split as `+0x60`.
extern "C" const char* Mediator_GetCrashReporterUsername5c_Impl(
    MinimalLoginMediatorStub* self,
    const void* chainedValueToken) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetCrashReporterUsername5c(chainedValueToken);
}

// anchor: client.dll early auth-name chain proves arg6 +0x5c is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x5c
__attribute__((naked)) static void Mediator_GetCrashReporterUsername5c() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $8, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetCrashReporterUsername5c_Impl)
        : "eax");
}

// anchor: client.dll:0x62170b00 gates arg7 high-byte selection flow through arg6 +0xd8
// vtable: ILTLoginMediator.Default slot +0xd8
static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetArg7SelectionUpperBoundExclusive();
}

// anchor: deeper client init maps arg7-derived selection names through arg6 +0xdc
// vtable: ILTLoginMediator.Default slot +0xdc
static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->MapSelectionName(selectionHighByte);
}

// launcher.exe arg7-selection resolution still reuses arg6 `+0x54` as a generic bool gate when
// deciding whether to accept world-type values `2/5`, but the slot body itself is now anchored as
// the tiny `+0x50` truthiness wrapper at launcher.exe:0x41f0b0.

// anchor: arg7-selection resolution consults the sibling ILTLoginMediator surface through +0xe0
// vtable: ILTLoginMediator.Default slot +0xe0
static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetVariantWorldName(variantIndex);
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0xe4
// vtable: ILTLoginMediator.Default slot +0xe4
static uint32_t __thiscall Mediator_GetVariantState(MinimalLoginMediatorStub* self, int32_t variantIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetVariantState(variantIndex);
}

// Late-login arg6 ABI slots `+0xd4`, `+0x124`, and `+0x18c` now live in
// `src/launcher_mediator_state9_abi.cpp` so post-state9/state12 work no longer has to reread the
// broader startup-selection ABI surface in this TU.

// anchor: launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// vtable: launcher.exe:0x4d3584 slot +0xf8
static uint32_t __thiscall Mediator_GetWorldCount(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldCount();
}

// anchor: launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex
// vtable: launcher.exe:0x4d3584 slot +0xfc
static const char* __thiscall Mediator_GetWorldNameByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldNameByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b320 / launcher.exe arg7-selection writer at 0x40d763..0x40d810
// vtable: launcher.exe:0x4d3584 slot +0x100
static uint32_t __thiscall Mediator_GetWorldSelectionGateByteByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldSelectionGateByteByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b360
// vtable: launcher.exe:0x4d3584 slot +0x104
static uint32_t __thiscall Mediator_GetWorldTypeByteByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldTypeByteByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b3a0
// vtable: launcher.exe:0x4d3584 slot +0x108
static uint32_t __thiscall Mediator_GetWorldPopulationNibbleByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldPopulationNibbleByIndex(worldIndex);
}

// ILTLoginMediator.Default wrapper minimization:
// - keep `g_LoginMediatorVtable` in the ABI shell
// - move wrapper-owned late-runtime state/scratch/logging into `CLTLoginMediator` when the owner
//   can keep it
// - keep wrapper-facing ABI object shapes explicit when they are not the same thing as the
//   higher-level owner-side helper semantics

// anchor: launcher.exe:0x41f2c0
// vtable: ILTLoginMediator.Default slot +0x10c
// Current best late-runtime read from the event-0x18 observer callback:
// - returns owner `+0x30`
// - client immediately consumes the first two dwords there as a small-string begin/current pair
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* __thiscall Mediator_GetRouteDescriptor10c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetRouteDescriptor30();
}

// anchor: launcher.exe:0x41af50
// vtable: ILTLoginMediator.Default slot +0x118
// Current best late-runtime read from the event-0x18 observer callback:
// - returns owner `+0x1470`
// - client reads it as a vector-like begin/current/capacity triple of 12-byte entries
static mxo::ltlogin::LateEntryList1470VectorLikeSketch* __thiscall Mediator_GetLateEntryList118(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLateEntryList1470();
}

// UNANCHORED: C helper behind the recovered +0xec ABI wrapper.
extern "C" void Mediator_ConsumeSelectionContext_Impl(
    MinimalLoginMediatorStub* self,
    void* selectionContext,
    void* returnAddress) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        return;
    }

    if (selectionContext) {
        mxo::ltlogin::State3SelectionContextInputSketch input = {};
        std::memcpy(&input, selectionContext, sizeof(input));
        mediator->PersistSelectionContextForState8(input);
        DiagnosticMirrorSelectionContextIntoLoginController(selectionContext, sizeof(input));
    } else {
        mediator->ResetSelectionContext0ecMirror();
    }

    // Keep the wrapper-facing arg6 `+0x1c` semantic split explicit.
    // The copy now lives on `CLTLoginMediator`, but the stub object still exposes a direct
    // pointer-shaped field that client-side code may read without going back through the owner
    // vtable family.
    if (self) {
        self->field1C = const_cast<mxo::ltlogin::State3SelectionContextInputSketch*>(
            &mediator->SelectionContext0ecCopy());
    }

    (void)returnAddress;
}

// anchor: client.dll:0x62170f48 consumes the assembled 0xb4 selection/config handoff through arg6 +0xec
// vtable: ILTLoginMediator.Default slot +0xec
__attribute__((naked)) static void Mediator_ConsumeSelectionContext() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "mov 0(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $12, %%esp\n\t"
        "ret $4\n\t"
        :
        : "i"(Mediator_ConsumeSelectionContext_Impl)
        : "eax", "edx");
}

// UNANCHORED: C helper behind the recovered +0x120 ABI wrapper.
// Evidence-backed slot decision:
// - client.dll:0x62054d1d builds a larger stack-local input object during the loading-character /
//   create-character transition
// - the populated offsets line up with owner `+0x120 / 0x41c3c0`
// - keep the slot meaning unified as `ProcessLoginCredentials`, but preserve the instance-role
//   split between the wrapper mirror and the live owner/controller
extern "C" uint32_t Mediator_ProcessLoginCredentials120_Impl(
    MinimalLoginMediatorStub* self,
    void* input120,
    void* returnAddress) {
    (void)self;

    uint32_t result = 1u;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    mxo::ltlogin::CLTLoginMediator* loginController = DiagnosticAuthGetLoginController();

    if (mediator) {
        const bool applyOwnerSemantics = (loginController == nullptr || loginController == mediator);
        result = mediator->CaptureProcessLoginCredentialsArg6Slot120(
            input120,
            returnAddress,
            applyOwnerSemantics);
    }

    if (loginController && loginController != mediator) {
        result = loginController->CaptureProcessLoginCredentialsArg6Slot120(
            input120,
            returnAddress,
            true);
    }

    return result;
}

// anchor: later loading-character path around client.dll:0x620547c0..0x62054eac passes the
// post-auth character-data source block to arg6 +0x120
// vtable: ILTLoginMediator.Default slot +0x120
__attribute__((naked)) static void Mediator_ProcessLoginCredentials120() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "mov 0(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $12, %%esp\n\t"
        "ret $4\n\t"
        :
        : "i"(Mediator_ProcessLoginCredentials120_Impl)
        : "eax", "edx");
}

// anchor: launcher.exe:0x4202c0
// vtable: ILTLoginMediator.Default slot +0x13c
// WaitForEvent uses this repeatedly while blocked on registered observer notifications.
static void __thiscall Mediator_InvokeSessionCallbackHelper13c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator::Default->HelperSlot13c_InvokeSessionHelperVtable4();
}

// UNANCHORED: C helper behind the recovered +0x170 observer-registration ABI wrapper.
// Wrapper now forwards to CLTLoginMediator::RegisterLoginObserver; all state/logging moved to owner.
extern "C" uint32_t Mediator_RegisterLoginObserver170_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;

    return mxo::ltlogin::ILTLoginMediator::Default->RegisterLoginObserver(observer);
}

// anchor: launcher.exe:0x41ddb0
// vtable: ILTLoginMediator.Default slot +0x170
__attribute__((naked)) static void Mediator_RegisterLoginObserver170() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "mov 0(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $12, %%esp\n\t"
        "ret $4\n\t"
        :
        : "i"(Mediator_RegisterLoginObserver170_Impl)
        : "eax", "edx");
}

// anchor: launcher.exe:0x41f1c0
// vtable: ILTLoginMediator.Default slot +0xf4
static void* __thiscall Mediator_GetState8PersistenceF1c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? const_cast<void*>(mediator->GetState8PersistenceF1c()) : nullptr;
}

// anchor: launcher.exe:0x41f320 / owner vtable +0x148
// vtable: ILTLoginMediator.Default slot +0x148
// Historical wrapper note:
// - this wrapper slot used to carry the stale local name `Mediator_AttachRuntimeObject148`
// - current source now aligns it with the concrete launcher-owned `GameSessionID` getter
static const char* __thiscall Mediator_GetGameSessionId(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetGameSessionId();
}

// UNANCHORED: C helper behind the recovered +0x174 observer-unregistration ABI wrapper.
// Wrapper now forwards to CLTLoginMediator::UnregisterLoginObserver; all state/logging moved to owner.
extern "C" uint32_t Mediator_UnregisterLoginObserver174_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;

    return mxo::ltlogin::ILTLoginMediator::Default->UnregisterLoginObserver(observer);
}

// anchor: launcher.exe:0x41dde0
// vtable: ILTLoginMediator.Default slot +0x174
__attribute__((naked)) static void Mediator_UnregisterLoginObserver174() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "mov 0(%%esp), %%edx\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $12, %%esp\n\t"
        "ret $4\n\t"
        :
        : "i"(Mediator_UnregisterLoginObserver174_Impl)
        : "eax", "edx");
}

// anchor: launcher.exe:0x41f240
// vtable: ILTLoginMediator.Default slot +0x178
static uint32_t __thiscall Mediator_GetLastLoginStatus(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLastLoginStatus();
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

    std::memset(g_LoginMediatorVtable, 0, sizeof(g_LoginMediatorVtable));
    // Forward to ILTLoginMediator::Default vtable slot 0 (original Mediator_GetName)
    g_LoginMediatorVtable[0] = (void*)Mediator_GetName;          // +0x00
    g_LoginMediatorVtable[2] = (void*)Mediator_SetNetworkEngine; // +0x08
    g_LoginMediatorVtable[3] = (void*)Mediator_ClearEngine;      // +0x0c
    g_LoginMediatorVtable[4] = (void*)Mediator_IsReady;          // +0x10
    g_LoginMediatorVtable[7] = (void*)Mediator_SetValue1;        // +0x1c
    g_LoginMediatorVtable[9] = (void*)Mediator_SetValue2;        // +0x24
    g_LoginMediatorVtable[11] = (void*)Mediator_IsConnected;     // +0x2c
    g_LoginMediatorVtable[14] = (void*)Mediator_GetProfileRootName38;  // +0x38
    g_LoginMediatorVtable[15] = (void*)Mediator_GetDefaultSelectionIndex; // +0x3c
    g_LoginMediatorVtable[16] = (void*)Mediator_GetSelectionDescriptor40; // +0x40
    g_LoginMediatorVtable[17] = (void*)Mediator_GetCurrentSlotRecordObject44; // +0x44
    g_LoginMediatorVtable[18] = (void*)Mediator_GetWorldOrSelectionName; // +0x48
    g_LoginMediatorVtable[19] = (void*)Mediator_GetProfileOrSessionName; // +0x4c
    g_LoginMediatorVtable[20] = (void*)Mediator_GetBootstrapRaw08AuxHandle50; // +0x50
    g_LoginMediatorVtable[21] = (void*)Mediator_HasBootstrapRaw08AuxHandle54; // +0x54
    g_LoginMediatorVtable[22] = (void*)Mediator_GetCrashReporterPromptForSecurId58; // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetCrashReporterUsername5c; // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetCrashReporterPassword60; // +0x60
    g_LoginMediatorVtable[26] = (void*)Mediator_HasLiveCorpus68; // +0x68
    g_LoginMediatorVtable[27] = (void*)Mediator_HasLiveCorpus6c; // +0x6c
    g_LoginMediatorVtable[28] = (void*)Mediator_HasLiveCorpus70; // +0x70
    g_LoginMediatorVtable[29] = (void*)Mediator_HasLiveCorpus74; // +0x74
    g_LoginMediatorVtable[30] = (void*)Mediator_HasLiveCorpus78; // +0x78
    g_LoginMediatorVtable[31] = (void*)Mediator_HasLiveCorpus7c; // +0x7c
    g_LoginMediatorVtable[32] = (void*)Mediator_HasLiveCorpus80; // +0x80
    g_LoginMediatorVtable[33] = (void*)Mediator_HasLiveCorpus84; // +0x84
    g_LoginMediatorVtable[34] = (void*)Mediator_HasLiveCorpus88; // +0x88
    g_LoginMediatorVtable[35] = (void*)Mediator_HasState8PersistenceData8c; // +0x8c
    g_LoginMediatorVtable[36] = (void*)Mediator_HasLiveCorpus90; // +0x90
    g_LoginMediatorVtable[37] = (void*)Mediator_GetLiveCorpus94; // +0x94
    g_LoginMediatorVtable[38] = (void*)Mediator_GetLiveCorpus98; // +0x98
    g_LoginMediatorVtable[39] = (void*)Mediator_GetLiveCorpus9c; // +0x9c
    g_LoginMediatorVtable[40] = (void*)Mediator_GetLiveCorpusA0; // +0xa0
    g_LoginMediatorVtable[41] = (void*)Mediator_GetLiveCorpusA4; // +0xa4
    g_LoginMediatorVtable[42] = (void*)Mediator_GetLiveCorpusA8; // +0xa8
    g_LoginMediatorVtable[43] = (void*)Mediator_GetLiveCorpusAc; // +0xac
    g_LoginMediatorVtable[44] = (void*)Mediator_GetLiveCorpusB0; // +0xb0
    g_LoginMediatorVtable[45] = (void*)Mediator_GetLiveCorpusB4; // +0xb4
    g_LoginMediatorVtable[46] = (void*)Mediator_GetLiveCorpusB8; // +0xb8
    g_LoginMediatorVtable[47] = (void*)Mediator_GetState8PersistenceHeaderBc; // +0xbc
    g_LoginMediatorVtable[48] = (void*)Mediator_GetState8PersistenceBodyC0; // +0xc0
    g_LoginMediatorVtable[49] = (void*)Mediator_GetState8PersistenceOverflowC4; // +0xc4
    g_LoginMediatorVtable[50] = (void*)Mediator_HasState8Section11DataC8; // +0xc8
    g_LoginMediatorVtable[51] = (void*)Mediator_GetState8Section11DwordCc; // +0xcc
    g_LoginMediatorVtable[52] = (void*)Mediator_GetState8Section11StringD0; // +0xd0
    RegisterMediatorState9AbiSlots();
    g_LoginMediatorVtable[54] = (void*)Mediator_GetArg7SelectionUpperBoundExclusive; // +0xd8
    g_LoginMediatorVtable[55] = (void*)Mediator_MapSelectionName;     // +0xdc
    g_LoginMediatorVtable[56] = (void*)Mediator_GetVariantWorldName; // +0xe0
    g_LoginMediatorVtable[57] = (void*)Mediator_GetVariantState; // +0xe4
    g_LoginMediatorVtable[59] = (void*)Mediator_ConsumeSelectionContext; // +0xec
    g_LoginMediatorVtable[61] = (void*)Mediator_GetState8PersistenceF1c; // +0xf4
    g_LoginMediatorVtable[62] = (void*)Mediator_GetWorldCount; // +0xf8
    g_LoginMediatorVtable[63] = (void*)Mediator_GetWorldNameByIndex; // +0xfc
    g_LoginMediatorVtable[64] = (void*)Mediator_GetWorldSelectionGateByteByIndex; // +0x100
    g_LoginMediatorVtable[65] = (void*)Mediator_GetWorldTypeByteByIndex; // +0x104
    g_LoginMediatorVtable[66] = (void*)Mediator_GetWorldPopulationNibbleByIndex; // +0x108
    g_LoginMediatorVtable[67] = (void*)Mediator_GetRouteDescriptor10c; // +0x10c
    g_LoginMediatorVtable[70] = (void*)Mediator_GetLateEntryList118; // +0x118
    g_LoginMediatorVtable[72] = (void*)Mediator_ProcessLoginCredentials120; // +0x120
    g_LoginMediatorVtable[79] = (void*)Mediator_InvokeSessionCallbackHelper13c; // +0x13c
    g_LoginMediatorVtable[82] = (void*)Mediator_GetGameSessionId; // +0x148
    g_LoginMediatorVtable[89] = (void*)Mediator_ShouldExportA;   // +0x164
    g_LoginMediatorVtable[91] = (void*)Mediator_ShouldExportB;   // +0x16c
    g_LoginMediatorVtable[92] = (void*)Mediator_RegisterLoginObserver170; // +0x170
    g_LoginMediatorVtable[93] = (void*)Mediator_UnregisterLoginObserver174; // +0x174
    g_LoginMediatorVtable[94] = (void*)Mediator_GetLastLoginStatus; // +0x178

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
        spdlog::info(
            "DIAGNOSTIC: registry node {} service='{}' object={} next={}",
            fmt::ptr(node),
            node->serviceName ? node->serviceName : "<null>",
            fmt::ptr(node->resolvedObject),
            fmt::ptr(node->next));

        if (node->serviceName && std::strcmp(node->serviceName, serviceName) == 0) {
            return node;
        }
    }

    return NULL;
}

// UNANCHORED: binder-scaffold resolver that writes the materialized arg6 pointer into the caller slot.
static bool DiagnosticResolveBinderWrapper(DiagnosticBinderWrapper* wrapper) {
    if (!wrapper || !wrapper->outSlot || !wrapper->registry) return false;

    spdlog::info(
        "DIAGNOSTIC: binder wrapper lookup(service='{}', mode={:08x}, outSlot={})",
        wrapper->serviceName ? wrapper->serviceName : "<null>",
        (unsigned)wrapper->mode,
        fmt::ptr(wrapper->outSlot));
    spdlog::info(
        "DIAGNOSTIC: binder registry={} resolverList(registry+0x18)={}",
        fmt::ptr(wrapper->registry),
        fmt::ptr(wrapper->registry->resolverList));

    DiagnosticMediatorResolverNode* node =
        DiagnosticLookupResolverNode(wrapper->registry, wrapper->serviceName);
    wrapper->lastResolvedNode = node;
    if (!node) {
        spdlog::info("DIAGNOSTIC: binder lookup failed for '{}'", wrapper->serviceName ? wrapper->serviceName : "<null>");
        return false;
    }

    *wrapper->outSlot = node->resolvedObject;
    spdlog::info(
        "DIAGNOSTIC: binder resolved '{}' via node {} -> wrote {} to slot {}",
        wrapper->serviceName ? wrapper->serviceName : "<null>",
        fmt::ptr(node),
        fmt::ptr(node->resolvedObject),
        fmt::ptr(wrapper->outSlot));
    return true;
}

// UNANCHORED: diagnostic binder-backed installer for the replacement arg6 mediator stub.
void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr) {
    DiagnosticInitializeBinderScaffold(outMediatorPtr);

    spdlog::info(
        "DIAGNOSTIC: binder scaffold prepared wrapper={} registry={} resolver={} targetSlot={}",
        fmt::ptr(&g_DiagnosticBinderWrapper),
        fmt::ptr(&g_DiagnosticBinderRegistry),
        fmt::ptr(&g_DiagnosticMediatorResolver),
        fmt::ptr(outMediatorPtr));

    if (!DiagnosticResolveBinderWrapper(&g_DiagnosticBinderWrapper)) {
        spdlog::info("DIAGNOSTIC: binder scaffold failed to materialize arg6");
        return;
    }

    spdlog::info("DIAGNOSTIC: binder scaffold materialized arg6 as {}", fmt::ptr(outMediatorPtr ? *outMediatorPtr : NULL));
}

// UNANCHORED: diagnostic selection configurator for the replacement arg6 sidecar model.
void DiagnosticConfigureMediatorSelection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedSelectionGateByte100,
    uint32_t selectedVariantState) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: mediator selection configuration skipped (no scaffold model)");
        return;
    }

    mediator->ConfigureArg6Selection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        mappedSelectionName,
        mappedVariantName,
        selectedWorldIndexLow24,
        selectedVariantIndexHigh8,
        selectedSelectionGateByte100,
        selectedVariantState);

    spdlog::info(
        "DIAGNOSTIC: mediator selection configured worldUpperBoundExclusive={} variantUpperBoundExclusive={} worldName='{}' variantName='{}' selectedWorldLow24=0x{:06x} selectedVariantHigh8=0x{:02x} selectedSelectionGateByte100={} selectedVariantState={}",
        mediator->Arg6WorldUpperBoundExclusive(),
        mediator->Arg6VariantUpperBoundExclusive(),
        mediator->Arg6MappedSelectionName(),
        mediator->Arg6MappedVariantName(),
        mediator->Arg6SelectedWorldIndexLow24(),
        mediator->Arg6SelectedVariantIndexHigh8(),
        mediator->Arg6SelectedSelectionGateByte100(),
        mediator->Arg6SelectedVariantState());
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
        spdlog::info("DIAGNOSTIC: launcher selection resolve skipped (mediator={})", fmt::ptr(mediatorPtr));
        return false;
    }

    void** vtable = *(void***)mediatorPtr;
    if (!vtable || !vtable[21] || !vtable[57] || !vtable[63] || !vtable[64]) {
        spdlog::info("DIAGNOSTIC: launcher selection resolve missing mediator slots (+0x54/+0xe4/+0xfc/+0x100)");
        return false;
    }

    typedef uint32_t (__thiscall *NoArgUIntFn)(void*);
    typedef const char* (__thiscall *IndexStringFn)(void*, uint32_t);
    typedef uint32_t (__thiscall *IndexUIntFn)(void*, uint32_t);
    typedef uint32_t (__thiscall *SignedIndexUIntFn)(void*, int32_t);

    // Keep the semantic split explicit here:
    // - launcher arg7-selection resolution reuses wrapper slot `+0x54` as a generic bool gate for
    //   accepting world-type values `2/5`
    // - the slot body itself is now named from the owner-side evidence: a tiny `+0x50`
    //   truthiness wrapper over the auth/bootstrap raw08 aux-handle family
    NoArgUIntFn hasBootstrapRaw08AuxHandle54Fn = (NoArgUIntFn)vtable[21]; // +0x54
    IndexStringFn worldNameFn = (IndexStringFn)vtable[63];         // +0xfc
    IndexUIntFn selectionGateByte100Fn = (IndexUIntFn)vtable[64];  // +0x100
    SignedIndexUIntFn variantStateFn = (SignedIndexUIntFn)vtable[57]; // +0xe4

    const uint32_t worldIndexLow24 = requestedWorldIndexLow24 & 0x00ffffffu;
    const uint32_t variantIndexHigh8 = requestedVariantIndexHigh8 & 0xffu;
    const char* worldName = worldNameFn(mediatorPtr, worldIndexLow24);
    const uint32_t selectionGateByte100 = selectionGateByte100Fn(mediatorPtr, worldIndexLow24);

    bool selectionGateAccepted = false;
    if (selectionGateByte100 == 1u) {
        selectionGateAccepted = true;
    } else if (selectionGateByte100 == 2u || selectionGateByte100 == 5u) {
        selectionGateAccepted = hasBootstrapRaw08AuxHandle54Fn(mediatorPtr) != 0;
    }

    const int32_t signedVariantIndex = static_cast<int32_t>(variantIndexHigh8);
    const uint32_t variantState = variantStateFn(mediatorPtr, signedVariantIndex);
    const bool variantAccepted = (variantState == 0u || variantState == 7u);

    if (!worldName || !selectionGateAccepted || !variantAccepted) {
        spdlog::info(
            "DIAGNOSTIC: launcher selection resolve failed worldIndexLow24=0x{:06x} variantIndexHigh8=0x{:02x} worldName={} selectionGateByte100={} selectionGateAccepted={} variantState={} variantAccepted={}",
            worldIndexLow24,
            variantIndexHigh8,
            worldName ? worldName : "<null>",
            selectionGateByte100,
            selectionGateAccepted ? 1u : 0u,
            variantState,
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

    spdlog::info(
        "DIAGNOSTIC: launcher-style selection resolve via mediator worldIndexLow24=0x{:06x} variantIndexHigh8=0x{:02x} -> worldName='{}' selectionGateByte100={} variantState={} a8=0x{:08x} ac=0x{:08x} packed=0x{:08x}",
        worldIndexLow24,
        variantIndexHigh8,
        worldName,
        selectionGateByte100,
        variantState,
        *outFieldA8,
        *outFieldAC,
        (*outFieldAC & 0x00ffffffu) | ((*outFieldA8 & 0xffu) << 24));
    return true;
}

// UNANCHORED: diagnostic profile/session-name configurator for arg6.
void DiagnosticConfigureMediatorProfileName(const char* profileName) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6ProfileName(profileName);
    }

    spdlog::info("DIAGNOSTIC: mediator profile/session name configured as '{}'", DiagnosticMediatorProfileName());
}

// UNANCHORED: diagnostic auth-name configurator for arg6 +0x5c.
void DiagnosticConfigureMediatorAuthName(const char* authName) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6AuthName(authName);
    }

    spdlog::info("DIAGNOSTIC: mediator auth-name chain (+0x5c) configured as '{}'", DiagnosticMediatorAuthName());
    DiagnosticAuthSetMediatorCredentials(DiagnosticMediatorAuthName(), DiagnosticMediatorAuthPassword());
}

// UNANCHORED: diagnostic auth-password configurator for arg6 +0x60.
void DiagnosticConfigureMediatorAuthPassword(const char* authPassword) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->SetArg6AuthPassword(authPassword);
    }

    spdlog::info(
        "DIAGNOSTIC: mediator auth-password chain (+0x60) configured as {}",
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
        spdlog::info("DIAGNOSTIC: mediator nopatch slots unavailable");
        return;
    }

    typedef void (__thiscall *SetValueFn)(void*, void*);
    SetValueFn setValue1 = (SetValueFn)vtable[7];
    SetValueFn setValue2 = (SetValueFn)vtable[9];

    setValue1(mediatorPtr, &parsedNoPatchValue);
    spdlog::info("DIAGNOSTIC: applied default nopatch mediator +0x1c with value 0x{:08x}", parsedNoPatchValue);

    setValue2(mediatorPtr, &clientVersionValue);
    spdlog::info("DIAGNOSTIC: applied default nopatch mediator +0x24 with value 0x{:08x}", clientVersionValue);
}
