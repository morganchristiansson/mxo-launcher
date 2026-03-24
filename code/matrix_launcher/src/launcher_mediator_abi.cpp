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
DiagnosticMediatorRuntimeState g_MediatorRuntimeState = {};
void* g_LoginMediatorVtable[104] = {0};
static const char g_MediatorStringA[] = "resurrections";
static const char g_MediatorStringC[] = "standalone";
static const char g_MediatorEmptyString[] = "";

static constexpr size_t kDiagnosticSelectionContextSize = 0xb4; // from client.dll:6211d3e0 zero-init of the +0xec handoff object
struct DiagnosticMediatorSelectionContextCopy {
    unsigned char bytes[kDiagnosticSelectionContextSize];
};

static DiagnosticMediatorSelectionContextCopy g_MediatorSelectionContextCopy = {};
static bool g_MediatorSelectionContextCopyValid = false;

struct DiagnosticSmallStringLike {
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
};

struct DiagnosticVectorLike {
    const void* begin = nullptr;
    const void* current = nullptr;
    const void* capacity = nullptr;
};

static std::string g_MediatorRouteDescriptor10cOwned;
static DiagnosticSmallStringLike g_MediatorRouteDescriptor10c = {};
static DiagnosticVectorLike g_MediatorLateEntryList118 = {};

// UNANCHORED: diagnostic masking helper for auth/password log surfaces.
const char* MaskedSensitiveValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

// UNANCHORED: sidecar-model accessor for the replacement arg6 ABI shell.
mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel() {
    if (!mxo::ltlogin::ILTLoginMediator::Default) {
        mxo::ltlogin::ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();
    }
    return dynamic_cast<mxo::ltlogin::CLTLoginMediator*>(mxo::ltlogin::ILTLoginMediator::Default);
}

static mxo::ltlogin::CLTLoginMediator* DiagnosticGetActiveMediatorForCharacterState() {
    if (mxo::ltlogin::CLTLoginMediator* loginController = DiagnosticAuthGetLoginController()) {
        return loginController;
    }
    return DiagnosticEnsureMediatorModel();
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

const char* DiagnosticMediatorAuthName() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthName() : DiagnosticMediatorProfileName();
}

const char* DiagnosticMediatorAuthPassword() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->Arg6AuthPassword() : g_MediatorEmptyString;
}

static void DiagnosticMirrorSelectionContextIntoMediatorModel(const void* selectionContext) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator || !selectionContext) return;

    mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch input = {};
    std::memcpy(&input, selectionContext, sizeof(input));
    mediator->PersistSelectionContextForState8(input);
    spdlog::info(
        "DIAGNOSTIC: mirrored arg6 +0xec selection context into CLTLoginMediator state3->8 snapshot slot=0x{:02x} firstBlock04=0x{:08x} lastBlockA4=0x{:08x}",
        (unsigned)(input.slotOrSelectionIndex00 & 0xffu),
        (unsigned)input.block04[0],
        (unsigned)input.blockA4[3]);
}

static const char* NonEmptyOrPlaceholder(const char* value) {
    return (value && value[0]) ? value : "<empty>";
}

static bool IsProfilePathBuilderCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    return address >= 0x62195ff0u && address <= 0x62196121u;
}

static bool IsMcdPersistenceCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    return address >= 0x62197830u && address <= 0x621983d0u;
}

static const char* DescribeMediatorCaller(void* returnAddress) {
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

static void LogMediatorCharacterStateContext(const char* slotLabel, void* returnAddress) {
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

    spdlog::info(
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

static void LogMediatorNameGetterDetails(
    const char* slotLabel,
    void* returnAddress,
    const char* returnedText) {
    spdlog::info(
        "MediatorStub::{} caller={} [{}] -> '{}'",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        NonEmptyOrPlaceholder(returnedText));
    LogMediatorCharacterStateContext(slotLabel, returnAddress);
}

// Focused source split for the broader ILTLoginMediator.Default ABI shell:
// - keep profile-path / current-slot / selection-descriptor work isolated
// - keep non-mcd selection cfg corpus fallback work isolated
// - keep mcd/persistence work isolated
#include "launcher_mediator_abi_profile_paths.cpp"
#include "launcher_mediator_abi_selection_cfg.cpp"
#include "launcher_mediator_abi_mcd.cpp"

// UNANCHORED: masks reflected password arguments in mediator-chain logs.
const char* MaskIfMediatorPassword(const char* value) {
    if (!value) return "<null>";
    const char* authPassword = DiagnosticMediatorAuthPassword();
    if (authPassword && authPassword[0] && std::strcmp(value, authPassword) == 0) {
        return "<provided>";
    }
    return value;
}

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

// UNANCHORED: shared diagnostic log-throttling helper.
static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
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
            spdlog::info("SelectionContext ascii candidate [+0x{:02x}] = '{}'", (unsigned)i, buffer);
            loggedAnyString = true;
            i = j + 1;
            continue;
        }

        ++i;
    }

    if (!loggedAnyString) {
        spdlog::info("SelectionContext ascii candidate scan: none");
    }
}

// UNANCHORED: resets the replacement mediator object and sidecar model to default state.
static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    std::memset(&g_MediatorSelectionObject, 0, sizeof(g_MediatorSelectionObject));
    std::memset(&g_MediatorCurrentSlotRecordObject, 0, sizeof(g_MediatorCurrentSlotRecordObject));
    std::memset(&g_MediatorSelectionContextCopy, 0, sizeof(g_MediatorSelectionContextCopy));
    g_MediatorProfileCharacterInfoF4 = {};
    g_MediatorState8PersistenceF1c = {};
    g_MediatorState8Overflow13f0.fill(0u);
    g_MediatorState8Overflow13f4 = 0u;
    g_MediatorState8Section11String1460Owned.clear();
    g_MediatorState8Section11String1460 = {};
    g_MediatorCurrentSlotRecordPayload = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    g_MediatorCurrentSlotRecordNameOwned.clear();
    g_MediatorSelectionContextCopyValid = false;
    delete mxo::ltlogin::ILTLoginMediator::Default;
    mxo::ltlogin::ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();
    g_MediatorSelectionPacked = DiagnosticMediatorSelectionPacked{0, 0, 0, 0u, 0u};
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
    // return mxo::ltlogin::ILTLoginMediator::Default->SetNetworkEngine();
    g_MediatorRuntimeState.registeredLauncherObject = object;
    if (self) {
        self->field04 = object;
    }
    spdlog::info("MediatorStub::SetNetworkEngine({})", object);
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
    if (!g_MediatorRuntimeState.lastNopatchValue1Ptr) {
        g_MediatorRuntimeState.lastNopatchValue1Ptr = value;
    } else {
        g_MediatorRuntimeState.lastNopatchValue2Ptr = value;
    }
    spdlog::info("MediatorStub::SetValue1({})", value);
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator.Default slot +0x2c
static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    static uint32_t s_IsConnectedCount = 0;
    ++s_IsConnectedCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(s_IsConnectedCount)) {
        spdlog::debug("MediatorStub::IsConnected() -> 1 [count={:08x} self={}, registeredEngine={}",
            s_IsConnectedCount,
            fmt::ptr(self),
            (self ? self->field04 : NULL));
    }
    return 1;
}

// anchor: client.dll:0x625c86d0 later calls arg6 +0x50 and converts null/non-null into flag 0x30
// vtable: ILTLoginMediator.Default slot +0x50
static void* __thiscall Mediator_GetBootstrapRaw08AuxHandle50(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    void* value = mediator ? mediator->BootstrapRaw08AuxHandle50() : nullptr;

    static bool loggedOnce = false;
    static void* lastValue = nullptr;
    if (!loggedOnce || lastValue != value) {
        spdlog::info(
            "MediatorStub::GetBootstrapRaw08AuxHandle(+0x50) -> {}{}",
            fmt::ptr(value),
            loggedOnce ? " [changed]" : " [first]");
        loggedOnce = true;
        lastValue = value;
    }

    return value;
}

// anchor: client.dll early auth-name chain at 0x62001325..0x62001362 first calls arg6 +0x58
// vtable: ILTLoginMediator.Default slot +0x58
static const char* __thiscall Mediator_GetString0(MinimalLoginMediatorStub* self) {
    (void)self;
    spdlog::info("MediatorStub::GetString0(+0x58) -> '{}'", g_MediatorStringA);
    return g_MediatorStringA;
}

// UNANCHORED: C helper behind the caller-clean +0x60 ABI wrapper.
extern "C" const char* Mediator_GetString1_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    spdlog::info(
        "MediatorStub::GetString1(+0x60 value='{}') -> {}",
        value ? value : "<null>",
        MaskedSensitiveValue(DiagnosticMediatorAuthPassword()));
    return DiagnosticMediatorAuthPassword();
}

// anchor: client.dll early auth-name chain proves arg6 +0x60 is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x60
__attribute__((naked)) static void Mediator_GetString1() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $8, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetString1_Impl)
        : "eax");
}

// UNANCHORED: C helper behind the caller-clean +0x5c ABI wrapper.
extern "C" const char* Mediator_GetString2_Impl(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    spdlog::info(
        "MediatorStub::GetString2(+0x5c value='{}') -> '{}'",
        MaskIfMediatorPassword(value),
        DiagnosticMediatorAuthName());
    return DiagnosticMediatorAuthName();
}

// anchor: client.dll early auth-name chain proves arg6 +0x5c is caller-clean on this path
// vtable: ILTLoginMediator.Default slot +0x5c
__attribute__((naked)) static void Mediator_GetString2() {
    __asm__ volatile(
        "mov 4(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $8, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetString2_Impl)
        : "eax");
}

// anchor: client.dll:0x62170b00 gates arg7 high-byte selection flow through arg6 +0xd8
// vtable: ILTLoginMediator.Default slot +0xd8
static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    spdlog::info(
        "MediatorStub::GetArg7VariantUpperBoundExclusive(+0xd8) -> {}",
        (unsigned)DiagnosticMediatorVariantUpperBoundExclusive());
    return DiagnosticMediatorVariantUpperBoundExclusive();
}

// anchor: deeper client init maps arg7-derived selection names through arg6 +0xdc
// vtable: ILTLoginMediator.Default slot +0xdc
static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    const char* variantName = DiagnosticMediatorVariantNameForIndex(selectionHighByte);
    spdlog::info(
        "MediatorStub::MapSelectionName(selectionHighByte={}) -> '{}'",
        (unsigned)selectionHighByte,
        variantName ? variantName : "<null>");
    return variantName;
}

// anchor: launcher selection resolution checks arg6 +0x54 before accepting world types 2/5
// vtable: ILTLoginMediator.Default slot +0x54
static uint32_t __thiscall Mediator_IsLauncherSelectionTypeEnabled(MinimalLoginMediatorStub* self) {
    (void)self;
    spdlog::info("MediatorStub::IsLauncherSelectionTypeEnabled(+0x54) -> 1");
    return 1;
}

// anchor: arg7-selection resolution consults the sibling ILTLoginMediator surface through +0xe0
// vtable: ILTLoginMediator.Default slot +0xe0
static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
    if (g_GetSelectionCallCount % 5 == 0) {
        spdlog::info("DIAGNOSTIC: GetSelectionDescriptor count = {}", g_GetSelectionCallCount);
    }
    const char* worldName = DiagnosticMediatorWorldNameForIndex(DiagnosticMediatorSelectedWorldIndexLow24());
    if (!worldName ||
        variantIndex >= DiagnosticMediatorVariantUpperBoundExclusive() ||
        !DiagnosticMediatorVariantIndexMatchesConfiguredSelection(variantIndex)) {
        spdlog::info(
            "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> NULL (world='{}' configuredVariant=0x{:02x} variantUpperBoundExclusive={})",
            (unsigned)(variantIndex & 0xffu),
            worldName ? worldName : "<null>",
            DiagnosticMediatorSelectedVariantIndexHigh8(),
            (unsigned)DiagnosticMediatorVariantUpperBoundExclusive());
        return NULL;
    }

    spdlog::info(
        "MediatorStub::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> '{}'",
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
    spdlog::info(
        "MediatorStub::GetVariantState(+0xe4 variantIndex={}) -> {} (configuredVariant=0x{:02x} configuredState={})",
        variantIndex,
        state,
        DiagnosticMediatorSelectedVariantIndexHigh8(),
        DiagnosticMediatorSelectedVariantState());
    return state;
}

// Late-login arg6 ABI slots `+0xd4`, `+0x124`, and `+0x18c` now live in
// `src/launcher_mediator_state9_abi.cpp` so post-state9/state12 work no longer has to reread the
// broader startup-selection ABI surface in this TU.

// anchor: launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// vtable: launcher.exe:0x4d3584 slot +0xf8
static uint32_t __thiscall Mediator_GetWorldCount(MinimalLoginMediatorStub* self) {
    (void)self;
    spdlog::info(
        "MediatorStub::GetWorldCount(+0xf8) -> {}",
        (unsigned)DiagnosticMediatorWorldUpperBoundExclusive());
    return DiagnosticMediatorWorldUpperBoundExclusive();
}

// anchor: launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex
// vtable: launcher.exe:0x4d3584 slot +0xfc
static const char* __thiscall Mediator_GetWorldNameByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    ++g_GetSelectionCallCount;
    if (g_GetSelectionCallCount % 5 == 0) {
        spdlog::info("DIAGNOSTIC: GetSelectionDescriptor count = {}", g_GetSelectionCallCount);
    }
    const char* worldName = DiagnosticMediatorWorldNameForIndex(worldIndex);
    spdlog::info(
        "MediatorStub::GetWorldNameByIndex(+0xfc worldIndex=0x{:06x}) -> '{}' (configuredWorld=0x{:06x})",
        (unsigned)(worldIndex & 0x00ffffffu),
        worldName ? worldName : "<null>",
        DiagnosticMediatorSelectedWorldIndexLow24());
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
    spdlog::info(
        "MediatorStub::GetWorldTypeByIndex(+0x100 worldIndex=0x{:06x}) -> {} (configuredWorld=0x{:06x} configuredType={})",
        (unsigned)(worldIndex & 0x00ffffffu),
        (unsigned)worldType,
        DiagnosticMediatorSelectedWorldIndexLow24(),
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
    spdlog::info(
        "MediatorStub::GetWorldFlag104(+0x104 worldIndex=0x{:06x}) -> {}",
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
    spdlog::info(
        "MediatorStub::GetWorldExtra108(+0x108 worldIndex=0x{:06x}) -> '{}'",
        (unsigned)(worldIndex & 0x00ffffffu),
        value ? value : "<null>");
    return value;
}

// anchor: launcher.exe:0x41f2c0
// vtable: ILTLoginMediator.Default slot +0x10c
// Current best late-runtime read from the event-0x18 observer callback:
// - returns owner `+0x30`
// - client immediately consumes the first two dwords there as a small-string begin/current pair
static DiagnosticSmallStringLike* __thiscall Mediator_GetRouteDescriptor10c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    const char* routeDescriptor = mediator ? mediator->ResolveMarginRouteDescriptor() : nullptr;

    g_MediatorRouteDescriptor10cOwned = routeDescriptor ? routeDescriptor : std::string();
    g_MediatorRouteDescriptor10c.begin = g_MediatorRouteDescriptor10cOwned.c_str();
    g_MediatorRouteDescriptor10c.current =
        g_MediatorRouteDescriptor10cOwned.c_str() + g_MediatorRouteDescriptor10cOwned.size();
    g_MediatorRouteDescriptor10c.capacity = g_MediatorRouteDescriptor10c.current;

    spdlog::info(
        "MediatorStub::GetRouteDescriptor10c(+0x10c) -> begin={} current={} text='{}'",
        fmt::ptr(g_MediatorRouteDescriptor10c.begin),
        fmt::ptr(g_MediatorRouteDescriptor10c.current),
        g_MediatorRouteDescriptor10cOwned.empty() ? "<empty>" : g_MediatorRouteDescriptor10cOwned.c_str());
    return &g_MediatorRouteDescriptor10c;
}

// anchor: launcher.exe:0x41af50
// vtable: ILTLoginMediator.Default slot +0x118
// Current best late-runtime read from the event-0x18 observer callback:
// - returns owner `+0x1470`
// - client reads it as a vector-like begin/current/capacity triple of 12-byte entries
static DiagnosticVectorLike* __thiscall Mediator_GetLateEntryList118(MinimalLoginMediatorStub* self) {
    (void)self;
    spdlog::info(
        "MediatorStub::GetLateEntryList118(+0x118) -> begin={} current={} capacity={} (empty scaffold)",
        fmt::ptr(g_MediatorLateEntryList118.begin),
        g_MediatorLateEntryList118.current,
        g_MediatorLateEntryList118.capacity);
    return &g_MediatorLateEntryList118;
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
        DiagnosticMirrorSelectionContextIntoMediatorModel(&g_MediatorSelectionContextCopy);
        DiagnosticMirrorSelectionContextIntoLoginController(&g_MediatorSelectionContextCopy, sizeof(g_MediatorSelectionContextCopy));
    } else {
        std::memset(&g_MediatorSelectionContextCopy, 0, sizeof(g_MediatorSelectionContextCopy));
        g_MediatorSelectionContextCopyValid = false;
    }
    if (self) {
        self->field1C = &g_MediatorSelectionContextCopy;
    }
    ++g_MediatorRuntimeState.selection0ecCount;
    spdlog::info(
        "MediatorStub::ConsumeSelectionContext({}) [count={} caller={} copied={}] size=0x{} valid={} configuredWorld=0x{} configuredVariant=0x{} profile='{}' world='{}'",
        fmt::ptr(selectionContext),
        (unsigned)g_MediatorRuntimeState.selection0ecCount,
        fmt::ptr(returnAddress),
        fmt::ptr(&g_MediatorSelectionContextCopy),
        (unsigned long)sizeof(g_MediatorSelectionContextCopy),
        g_MediatorSelectionContextCopyValid ? 1u : 0u,
        DiagnosticMediatorSelectedWorldIndexLow24(),
        DiagnosticMediatorSelectedVariantIndexHigh8(),
        DiagnosticMediatorProfileName(),
        DiagnosticMediatorMappedSelectionName());
    LogPointerWords("ConsumeSelectionContext copied", &g_MediatorSelectionContextCopy, 8);
    const uint32_t* copiedWords = reinterpret_cast<const uint32_t*>(&g_MediatorSelectionContextCopy);
    spdlog::info(
        "DIAGNOSTIC: selectionContext[0]=0x{:08x} (configuredVariant=0x{:02x} configuredWorld=0x{:06x})",
        copiedWords[0],
        DiagnosticMediatorSelectedVariantIndexHigh8(),
        DiagnosticMediatorSelectedWorldIndexLow24());
    LogSelectionContextDetails(&g_MediatorSelectionContextCopy, sizeof(g_MediatorSelectionContextCopy));
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
extern "C" void Mediator_FillLoadingCharacterState120_Impl(
    MinimalLoginMediatorStub* self,
    void* loadingState,
    void* returnAddress) {
    g_MediatorRuntimeState.loadingState120 = loadingState;
    ++g_MediatorRuntimeState.loading120Count;
    spdlog::info(
        "MediatorStub::FillLoadingCharacterState(+0x120 out={} self={}) [count={:08x} caller={} copiedFrom0ec={}]}",
        fmt::ptr(loadingState),
        fmt::ptr(self),
        (unsigned)g_MediatorRuntimeState.loading120Count,
        fmt::ptr(returnAddress),
        g_MediatorSelectionContextCopyValid ? 1u : 0u);
    LogPointerWords("FillLoadingCharacterState self", self, 8);
    LogPointerWords("FillLoadingCharacterState out(before/after stub)", loadingState, 8);
}

// anchor: later loading-character path around client.dll:0x620547c0..0x62054eac passes a large state object to arg6 +0x120
// vtable: ILTLoginMediator.Default slot +0x120
__attribute__((naked)) static void Mediator_FillLoadingCharacterState120() {
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
        : "i"(Mediator_FillLoadingCharacterState120_Impl)
        : "eax", "edx");
}

// anchor: launcher.exe:0x4202c0
// vtable: ILTLoginMediator.Default slot +0x13c
// WaitForEvent uses this repeatedly while blocked on registered observer notifications.
static void __thiscall Mediator_InvokeSessionCallbackHelper13c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    (void)mediator;
}

// UNANCHORED: C helper behind the recovered +0x170 observer-registration ABI wrapper.
extern "C" uint32_t Mediator_RegisterLoginObserver170_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    if (!g_MediatorRuntimeState.firstObserver170) {
        g_MediatorRuntimeState.firstObserver170 = observer;
    }
    g_MediatorRuntimeState.latestObserver170 = observer;
    ++g_MediatorRuntimeState.observerRegister170Count;

    const bool inserted = DiagnosticEnsureMediatorModel()
        ? DiagnosticEnsureMediatorModel()->RegisterLoginObserverScaffold(observer)
        : false;

    spdlog::info(
        "MediatorStub::RegisterLoginObserver(+0x170 observer={} self={}) [count={} inserted={} first={} latest124=({}, {}, {}) caller={}]",
        fmt::ptr(observer),
        fmt::ptr(self),
        (unsigned)g_MediatorRuntimeState.observerRegister170Count,
        inserted ? "1" : "0",
        fmt::ptr(g_MediatorRuntimeState.firstObserver170),
        fmt::ptr(g_MediatorRuntimeState.netShell124),
        fmt::ptr(g_MediatorRuntimeState.netMgr124),
        fmt::ptr(g_MediatorRuntimeState.distrObjExecutive124),
        fmt::ptr(returnAddress));
    LogPointerWords("RegisterLoginObserver self", self, 8);
    LogPointerWords("RegisterLoginObserver observer", observer, 4);
    return inserted ? 1u : 0u;
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
// Original getter is tiny: returns owner `+0xf1c`; the real producer is the earlier state8
// load-character reply path (`0x43f930`) that materializes the broader `+0xf1c/+0xf48/+0xf88/+0x13f0`
// family later consumed by client `mcd.cfg` and character-info paths.
static void* __thiscall Mediator_GetSelectionContextSnapshot(MinimalLoginMediatorStub* self) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    ++g_MediatorRuntimeState.profile0f4Count;
    PopulateMediatorState8PersistenceF1c();

    const char* firstName = reinterpret_cast<const char*>(g_MediatorState8PersistenceF1c.body6c.data() + 0x04);
    const char* lastName = reinterpret_cast<const char*>(g_MediatorState8PersistenceF1c.body6c.data() + 0x24);
    const char* background = reinterpret_cast<const char*>(g_MediatorState8PersistenceF1c.body6c.data() + 0x44);

    spdlog::info(
        "MediatorStub::GetSelectionContextSnapshot(+0xf4) caller={} [{}] -> {} [count={} copiedFrom0ec={} raw0ec={} char='{}' first='{}' last='{}' background='{}' field24=0x{:08x} overflow13f4=0x{:04x}]",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(&g_MediatorState8PersistenceF1c),
        static_cast<unsigned>(g_MediatorRuntimeState.profile0f4Count),
        g_MediatorSelectionContextCopyValid ? 1u : 0u,
        fmt::ptr(g_MediatorRuntimeState.selectionContext0ec),
        NonEmptyOrPlaceholder(g_MediatorState8PersistenceF1c.string00.data()),
        NonEmptyOrPlaceholder(firstName),
        NonEmptyOrPlaceholder(lastName),
        NonEmptyOrPlaceholder(background),
        static_cast<unsigned>(g_MediatorState8PersistenceF1c.field24),
        static_cast<unsigned>(g_MediatorState8Overflow13f4));
    LogMediatorState8PersistenceSummary("GetSelectionContextSnapshot(+0xf4)", returnAddress);
    LogMediatorCharacterStateContext("GetSelectionContextSnapshot(+0xf4)", returnAddress);
    LogPointerWords("GetSelectionContextSnapshot ownerF1c", &g_MediatorState8PersistenceF1c, 8);
    return &g_MediatorState8PersistenceF1c;
}

// anchor: later runtime setup uses arg6 +0x148 for runtime-object handoff
// vtable: ILTLoginMediator.Default slot +0x148
static void __thiscall Mediator_AttachRuntimeObject148(MinimalLoginMediatorStub* self, void* runtimeObject) {
    (void)self;
    void* returnAddress = __builtin_return_address(0);
    g_MediatorRuntimeState.runtimeObject148 = runtimeObject;
    ++g_MediatorRuntimeState.runtime148Count;
    spdlog::info(
        "MediatorStub::AttachRuntimeObject(+0x148 guess={}) [count={} caller={}]",
        fmt::ptr(runtimeObject),
        (unsigned)g_MediatorRuntimeState.runtime148Count,
        fmt::ptr(returnAddress));
}

// UNANCHORED: C helper behind the recovered +0x174 observer-unregistration ABI wrapper.
extern "C" uint32_t Mediator_UnregisterLoginObserver174_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    g_MediatorRuntimeState.latestObserver174 = observer;
    ++g_MediatorRuntimeState.observerUnregister174Count;

    const bool removed = DiagnosticEnsureMediatorModel()
        ? DiagnosticEnsureMediatorModel()->UnregisterLoginObserverScaffold(observer)
        : false;

    spdlog::info(
        "MediatorStub::UnregisterLoginObserver(+0x174 observer={} self={}) [count={} removed={} caller={}]",
        fmt::ptr(observer),
        fmt::ptr(self),
        g_MediatorRuntimeState.observerUnregister174Count,
        removed ? 1u : 0u,
        returnAddress);
    LogPointerWords("UnregisterLoginObserver observer", observer, 4);
    return removed ? 1u : 0u;
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
static uint32_t __thiscall Mediator_GetLastLoginStatus178(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    const uint32_t status = mediator ? mediator->WorldListCountOrStatus80() : 0u;
    g_MediatorRuntimeState.lastStatus178 = status;
    ++g_MediatorRuntimeState.statusQuery178Count;
    spdlog::info(
        "MediatorStub::GetLastLoginStatus(+0x178) -> 0x{:08x} [count={}]",
        status,
        g_MediatorRuntimeState.statusQuery178Count);
    return status;
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
    std::memset(g_MediatorCurrentSlotRecordVtable, 0, sizeof(g_MediatorCurrentSlotRecordVtable));
    g_MediatorCurrentSlotRecordVtable[0] = (void*)MediatorCurrentSlotRecord_Destroy;
    g_MediatorCurrentSlotRecordVtable[1] = (void*)MediatorCurrentSlotRecord_TinyGetter;
    g_MediatorCurrentSlotRecordVtable[2] = (void*)MediatorCurrentSlotRecord_AppendDebugString;
    g_MediatorCurrentSlotRecordVtable[3] = (void*)MediatorCurrentSlotRecord_ResetPayloadForSourceDescriptor;
    g_MediatorCurrentSlotRecordVtable[4] = (void*)MediatorCurrentSlotRecord_TinyHelper;
    // Forward to ILTLoginMediator::Default vtable slot 0 (original Mediator_GetName)
    g_LoginMediatorVtable[0] = (void*)Mediator_GetName;          // +0x00
    g_LoginMediatorVtable[2] = (void*)Mediator_SetNetworkEngine; // +0x08
    g_LoginMediatorVtable[3] = (void*)Mediator_ClearEngine;      // +0x0c
    g_LoginMediatorVtable[4] = (void*)Mediator_IsReady;          // +0x10
    g_LoginMediatorVtable[7] = (void*)Mediator_SetValue1;        // +0x1c
    g_LoginMediatorVtable[9] = (void*)Mediator_SetValue1;        // +0x24
    g_LoginMediatorVtable[11] = (void*)Mediator_IsConnected;     // +0x2c
    g_LoginMediatorVtable[14] = (void*)Mediator_GetDisplayName;  // +0x38
    g_LoginMediatorVtable[15] = (void*)Mediator_GetDefaultSelectionIndex; // +0x3c
    g_LoginMediatorVtable[16] = (void*)Mediator_GetSelectionDescriptor; // +0x40
    g_LoginMediatorVtable[17] = (void*)Mediator_GetCurrentSlotRecord44; // +0x44
    g_LoginMediatorVtable[18] = (void*)Mediator_GetWorldOrSelectionName; // +0x48
    g_LoginMediatorVtable[19] = (void*)Mediator_GetProfileOrSessionName; // +0x4c
    g_LoginMediatorVtable[20] = (void*)Mediator_GetBootstrapRaw08AuxHandle50; // +0x50
    g_LoginMediatorVtable[21] = (void*)Mediator_IsLauncherSelectionTypeEnabled; // +0x54
    g_LoginMediatorVtable[22] = (void*)Mediator_GetString0;      // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetString2;      // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetString1;      // +0x60
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
    g_LoginMediatorVtable[61] = (void*)Mediator_GetSelectionContextSnapshot; // +0xf4
    g_LoginMediatorVtable[62] = (void*)Mediator_GetWorldCount; // +0xf8
    g_LoginMediatorVtable[63] = (void*)Mediator_GetWorldNameByIndex; // +0xfc
    g_LoginMediatorVtable[64] = (void*)Mediator_GetWorldTypeByIndex; // +0x100
    g_LoginMediatorVtable[65] = (void*)Mediator_GetWorldFlag104; // +0x104
    g_LoginMediatorVtable[66] = (void*)Mediator_GetWorldExtra108; // +0x108
    g_LoginMediatorVtable[67] = (void*)Mediator_GetRouteDescriptor10c; // +0x10c
    g_LoginMediatorVtable[70] = (void*)Mediator_GetLateEntryList118; // +0x118
    g_LoginMediatorVtable[72] = (void*)Mediator_FillLoadingCharacterState120; // +0x120
    g_LoginMediatorVtable[79] = (void*)Mediator_InvokeSessionCallbackHelper13c; // +0x13c
    g_LoginMediatorVtable[82] = (void*)Mediator_AttachRuntimeObject148; // +0x148
    g_LoginMediatorVtable[89] = (void*)Mediator_ShouldExportA;   // +0x164
    g_LoginMediatorVtable[91] = (void*)Mediator_ShouldExportB;   // +0x16c
    g_LoginMediatorVtable[92] = (void*)Mediator_RegisterLoginObserver170; // +0x170
    g_LoginMediatorVtable[93] = (void*)Mediator_UnregisterLoginObserver174; // +0x174
    g_LoginMediatorVtable[94] = (void*)Mediator_GetLastLoginStatus178; // +0x178

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
    uint32_t selectedWorldType,
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
        selectedWorldType,
        selectedVariantState);
    g_MediatorSelectionPacked.field03 =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mediator->Arg6MappedSelectionName()));
    g_MediatorSelectionPacked.field07 = mediator->Arg6MappedSelectionId();

    spdlog::info(
        "DIAGNOSTIC: mediator selection configured worldUpperBoundExclusive={} variantUpperBoundExclusive={} worldName='{}' variantName='{}' selectedWorldLow24=0x{:06x} selectedVariantHigh8=0x{:02x} selectedWorldType={} selectedVariantState={}",
        mediator->Arg6WorldUpperBoundExclusive(),
        mediator->Arg6VariantUpperBoundExclusive(),
        mediator->Arg6MappedSelectionName(),
        mediator->Arg6MappedVariantName(),
        mediator->Arg6SelectedWorldIndexLow24(),
        mediator->Arg6SelectedVariantIndexHigh8(),
        mediator->Arg6SelectedWorldType(),
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
        spdlog::info(
            "DIAGNOSTIC: launcher selection resolve failed worldIndexLow24=0x{:06x} variantIndexHigh8=0x{:02x} worldName={} worldType={} typeAccepted={} variantState={} variantAccepted={}",
            worldIndexLow24,
            variantIndexHigh8,
            worldName ? worldName : "<null>",
            worldType,
            typeAccepted ? 1u : 0u,
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
        "DIAGNOSTIC: launcher-style selection resolve via mediator worldIndexLow24=0x{:06x} variantIndexHigh8=0x{:02x} -> worldName='{}' worldType={} variantState={} a8=0x{:08x} ac=0x{:08x} packed=0x{:08x}",
        worldIndexLow24,
        variantIndexHigh8,
        worldName,
        worldType,
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


