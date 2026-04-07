#include "diagnostics.h"
#include "launcher_mediator_abi_shared.h"
#include "launcher_network_object_abi.h"
#include "loginmediator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include <spdlog/spdlog.h>

extern void* g_pLauncherObject6304;

// Broad ILTLoginMediator.Default ABI shell:
// - keep startup-selection and general arg6 surface here

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
    if (mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel()) {
        return const_cast<mxo::ltlogin::CLTLoginMediator*>(mediator->ResolveActiveStateSourceScaffold());
    }
    return mxo::ltlogin::CLTLoginMediator::ActiveStateSourceScaffold();
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

static const char* DescribeLateMediatorAbiCaller(void* returnAddress) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(returnAddress);
    if (address >= 0x621707e0u && address <= 0x62170af8u) {
        return "client.dll:ClientShell_LoginMediatorObserver_OnEvent";
    }
    if (address >= 0x621c6d90u && address <= 0x621c7427u) {
        return "client.dll:late-entry loading-area setup helper";
    }
    if (address >= 0x62017150u && address <= 0x62017278u) {
        return "client.dll:late-entry metric matcher";
    }
    if (address >= 0x62030d90u && address <= 0x6203134du) {
        return "client.dll:LoadingAreaCommonLayoutView ctor family";
    }
    if (address >= 0x620301e0u && address <= 0x620304ffu) {
        return "client.dll:LoadingAreaCommonLayoutView dtor family";
    }
    if (address >= 0x620557c0u && address <= 0x62056700u) {
        return "client.dll:RsiLayoutsView ctor family";
    }
    return DescribeMediatorCaller(returnAddress);
}

struct LateMediatorAbiCallLogState {
    void* caller = nullptr;
    uint32_t selectionIndex = 0xffffffffu;
    uint32_t result32 = 0xffffffffu;
    void* resultPtr = nullptr;
    std::string resultText;
    bool valid = false;
};

static std::set<std::string> g_GetProfileRootName38SeenSites;
static std::set<std::string> g_GetDefaultSelectionIndex3cSeenSites;
static std::set<std::string> g_GetSelectionDescriptor40SeenSites;

static const char* DescribeKnownMediatorObserver(void* observer) {
    switch (reinterpret_cast<uintptr_t>(observer)) {
        case 0x629ddfc8u:
            return "ClientShell login-mediator observer";
        case 0x6298a5e8u:
            return "LoadingAreaCommonLayoutView forwarder";
        case 0x6298a760u:
            return "RsiLayoutsView forwarder";
        default:
            return "unknown/static observer";
    }
}

static std::string DescribeRouteDescriptorText(
    const mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* descriptor) {
    if (!descriptor || !descriptor->begin || !descriptor->current || descriptor->current < descriptor->begin) {
        return "<empty>";
    }
    if (descriptor->current == descriptor->begin) {
        return "<empty>";
    }
    return std::string(descriptor->begin, descriptor->current);
}

void LogMediatorCharacterStateContext(const char* slotLabel, void* returnAddress) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticGetActiveMediatorForCharacterState();
    if (!mediator) {
        return;
    }

    const auto& ownerState = mediator->PostAuthMarginLoadingStateView();
    const auto characterState = mediator->DescribeOwnCharacterStateScaffold();
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
    const char* activeCharacterName = NonEmptyOrPlaceholder(characterState.characterName);
    const uint32_t activeCharacterIdLow = characterState.characterIdLow;
    const uint32_t activeCharacterIdHigh = characterState.characterIdHigh;

    spdlog::debug(
        "MediatorStub::{} caller={} [{}] context{{mappedWorld='{}' profile='{}' currentSlot='{}' source108='{}' f1c='{}' section0f8c='{}' section0fac='{}' section0fcc='{}' activeChar='{}' activeIdLow=0x{:08x} activeIdHigh=0x{:08x} activeIdLow16=0x{:04x} currentState={} worldId=0x{:04x} status=0x{:02x}}}",
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
        activeCharacterName,
        activeCharacterIdLow,
        activeCharacterIdHigh,
        static_cast<unsigned>(activeCharacterIdLow & 0xffffu),
        fmt::ptr(currentState),
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->worldId0c) : 0u,
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->status0b) : 0u);
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

// UNANCHORED: resets the replacement mediator stub and clears stale active-state registration.
static void ResetMediatorObjectState() {
    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    mxo::ltlogin::CLTLoginMediator::UnregisterActiveStateSourceScaffold(
        dynamic_cast<mxo::ltlogin::CLTLoginMediator*>(mxo::ltlogin::ILTLoginMediator::Default));
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
// Return-shape note from launcher.exe:0x40a380:
// - the caller turns the raw slot result into `result < 1`
// - so this scaffold returns `0` for the non-null success case and `1` when the arg5 object
//   failed to materialize, which preserves the original success/failure sense more closely
static int __thiscall Mediator_SetNetworkEngine(MinimalLoginMediatorStub* self, void* object) {
    (void)self;

    mxo::ltlogin::ILTLoginMediator::Default->SetNetworkEngine(
        LauncherNetworkEngineFromAbiShell(object));
    return object ? 0 : 1;
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

// anchor: launcher.exe:0x408400 / sibling resolved slot 0x4d2734 vtable +0x30
// Raw-vtable clarification from the same submit pass:
// - launcher page-6 rich-edit submit helper `0x408400` calls resolved mediator slot `+0x30`
// - raw memory read of launcher mediator vtable family `0x004b01c8` shows `0x41ecd0` stored at
//   `0x004b01f8`, i.e. the same raw virtual displacement
// - practical consequence: the launcher dialog submit helper reaches
//   `CLTLoginMediator::ProcessLoginRequest` through the resolved `ILTLoginMediator.Default`-style
//   surface rather than through a separate launcher-only credential API
static uint32_t __thiscall Mediator_ProcessLoginRequest30(
    MinimalLoginMediatorStub* self,
    const mxo::ltlogin::ProcessLoginRequestInputSketch* input) {
    (void)self;
    return input
        ? mxo::ltlogin::ILTLoginMediator::Default->ProcessLoginRequest(*input)
        : 0u;
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator.Default slot +0x2c
static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->IsConnected();
}

// anchor: client.dll profile-root formatting path uses arg6 +0x38 for Profiles\%s\... construction
// vtable: ILTLoginMediator.Default slot +0x38
static const char* __thiscall Mediator_GetProfileRootName38(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    const char* const result = mxo::ltlogin::ILTLoginMediator::Default->GetProfileRootName();
    const char* const normalizedResult = NonEmptyOrPlaceholder(result);
    const std::string siteKey = std::to_string(reinterpret_cast<uintptr_t>(returnAddress)) + "|" + normalizedResult;
    if (g_GetProfileRootName38SeenSites.insert(siteKey).second) {
        spdlog::info(
            "MediatorStub::GetProfileRootName38 caller={} [{}] result='{}'",
            fmt::ptr(returnAddress),
            DescribeLateMediatorAbiCaller(returnAddress),
            normalizedResult);
    }
    return result;
}

// anchor: client.dll fallback-selection path asks arg6 +0x3c for the default selection index when given 0xff
// vtable: ILTLoginMediator.Default slot +0x3c
static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    const uint32_t result = mxo::ltlogin::ILTLoginMediator::Default->GetDefaultSelectionIndex();
    const std::string siteKey = std::to_string(reinterpret_cast<uintptr_t>(returnAddress)) + "|" + std::to_string(result);
    if (g_GetDefaultSelectionIndex3cSeenSites.insert(siteKey).second) {
        spdlog::info(
            "MediatorStub::GetDefaultSelectionIndex3c caller={} [{}] result=0x{:08x}",
            fmt::ptr(returnAddress),
            DescribeLateMediatorAbiCaller(returnAddress),
            static_cast<unsigned>(result));
    }
    return result;
}

// UNANCHORED: C helper behind the recovered +0x40 ABI wrapper.
extern "C" void* Mediator_GetSelectionDescriptor40_Impl(
    MinimalLoginMediatorStub* self,
    uint32_t selectionIndex,
    void* returnAddress) {
    (void)self;
    void* const result = mxo::ltlogin::ILTLoginMediator::Default->GetArg6SelectionDescriptorObject40(
        selectionIndex);
    const std::string siteKey =
        std::to_string(reinterpret_cast<uintptr_t>(returnAddress)) + "|" +
        std::to_string(selectionIndex) + "|" +
        std::to_string(reinterpret_cast<uintptr_t>(result));
    if (g_GetSelectionDescriptor40SeenSites.insert(siteKey).second) {
        spdlog::info(
            "MediatorStub::GetSelectionDescriptor40 caller={} [{}] selectionIndex=0x{:08x} result={}",
            fmt::ptr(returnAddress),
            DescribeLateMediatorAbiCaller(returnAddress),
            static_cast<unsigned>(selectionIndex),
            fmt::ptr(result));
    }
    return result;
}

// anchor: client.dll:0x62170dc1..0x62170e59 later asks arg6 +0x40 with the scratch-shaped arg7 request
// vtable: ILTLoginMediator.Default slot +0x40
// Keep this wrapper-facing selection-descriptor family explicit instead of forcing the owner-side
// `0x004b01c8 +0x40/+0x44` slot-record accessor names onto it.
__attribute__((naked)) static void Mediator_GetSelectionDescriptor40() {
    __asm__ volatile(
        "mov (%%esp), %%edx\n\t"
        "mov 4(%%esp), %%eax\n\t"
        "push %%edx\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $12, %%esp\n\t"
        "ret $4\n\t"
        :
        : "i"(Mediator_GetSelectionDescriptor40_Impl)
        : "eax", "edx");
}

// UNANCHORED: C helper behind the recovered +0x44 ABI wrapper.
extern "C" void* Mediator_GetCurrentSlotRecordObject44_Impl(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetArg6CurrentSlotRecordObject44();
}

// anchor: launcher.exe:0x41f300
// vtable: ILTLoginMediator.Default slot +0x44
// Current wrapper-facing read from `0x4d2c58_ILTLoginMediator_Default.md`:
// - returns a current-slot record object on the later profile/save path
// - keep that split explicit from the owner-side `0x004b01c8 +0x44` family
__attribute__((naked)) static void Mediator_GetCurrentSlotRecordObject44() {
    __asm__ volatile(
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $4, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetCurrentSlotRecordObject44_Impl)
        : "eax");
}

// anchor: later client startup path calls arg6 +0x48 before the now-better-understood
// observer registration / startup-triple handoff sequence
// practical current note from client.dll profile-path work:
// - the broader client path later formats `Profiles\%s\%s_%X\`
// - and the middle `%s` is sourced from client-global `DAT_629de48c`
// - current replacement evidence points to the earlier +0x48-fed name path as the highest-value
//   narrow source to keep character-shaped instead of world-shaped on the active route
// vtable: ILTLoginMediator.Default slot +0x48
static const char* __thiscall Mediator_GetWorldOrSelectionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetWorldOrSelectionName();
}

// anchor: later client startup path calls arg6 +0x4c immediately after +0x48
// vtable: ILTLoginMediator.Default slot +0x4c
static const char* __thiscall Mediator_GetProfileOrSessionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetProfileOrSessionName();
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

// Focused selection cfg / mcd persistence note:
// - wrappers for `+0x68..+0xd0` now live inline here in slot order
// - keep their wrapper-facing ABI split explicit from owner-side helpers

// anchor: client.dll:0x62198670 / launcher.exe vtable +0x68 -> 0x41f0c0
// Exact current closed pair:
// - client helper `0x62198670` uses arg6 `+0x68`, then `+0x94`, for `hl.cfg`
// - original launcher `+0x68` returns owner byte `+0x140e`
// - original launcher `+0x94` returns owner pointer `+0x1408` and writes out-length `+0x140c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `6`
static uint32_t __thiscall Mediator_HasLiveCorpus68(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveHlCfg68();
}

// anchor: client.dll:0x62198770 / launcher.exe vtable +0x6c -> 0x41f0d0
// Exact current next narrow pair:
// - client helper `0x62198770` uses arg6 `+0x6c`, then `+0x98`, for `an.cfg`
// - original launcher `+0x6c` returns owner byte `+0x1416`
// - original launcher `+0x98` returns owner pointer `+0x1410` and writes out-length `+0x1414`
// - recovered state8 slot-6 producer writes that same owner family from section selector `7`
static uint32_t __thiscall Mediator_HasLiveCorpus6c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveAnCfg6c();
}

// anchor: client.dll:0x62198870 / launcher.exe vtable +0x70
// Exact current inventory/loadout-targeted pair:
// - client helper `0x62198870` uses arg6 `+0x70`, then `+0x9c`, for `pi.cfg`
// - original launcher `+0x70` returns owner byte `+0x141e`
// - original launcher `+0x9c` returns owner pointer `+0x1418` and writes out-length `+0x141c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `3`
static uint32_t __thiscall Mediator_HasLiveCorpus70(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLivePiCfg70();
}

// anchor: client.dll:0x62198970 / launcher.exe vtable +0x74 -> raw bytes 0x41f0f0 = owner byte +0x1426
// Exact current symptom-targeted pair:
// - client helper `0x62198970` uses arg6 `+0x74`, then `+0xa0`, for `ai.cfg`
// - original launcher `+0x74` returns owner byte `+0x1426`
// - original launcher `+0xa0` returns owner pointer `+0x1420` and writes out-length `+0x1424`
// - recovered state8 slot-6 producer writes that same owner family from section selector `4`
// - client live consumer `0x621e2310` directly calls `0x621e0b90(...)` and marks an availability byte at
//   `param_1 + 0x6e0 + actionId*8`, making this pair materially closer to the missing Actions-window symptom
static uint32_t __thiscall Mediator_HasLiveCorpus74(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveAiCfg74();
}

// anchor: client.dll:0x62198a70 / launcher.exe vtable +0x78 -> raw bytes 0x41f100 = owner byte +0x142e
// Exact current skills-targeted pair:
// - client helper `0x62198a70` uses arg6 `+0x78`, then `+0xa4`, for `cs.cfg`
// - original launcher `+0x78` returns owner byte `+0x142e`
// - original launcher `+0xa4` returns owner pointer `+0x1428` and writes out-length `+0x142c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `5`
// - client live adopter `0x621cd550` consumes compact 6-byte records and writes only the first dword of
//   8-byte entries at `param_1 + 0x6dc + index*8`; that explains why the live state8 payload can be
//   shorter than the later saved on-disk `cs.cfg` file emitted by `0x621966d0 -> 0x621c9e20`
// - newer client-side tightening also makes the saved second dword less scary than before:
//   `0x621ca0c0/0x621e0a10/0x621e3b50` only use its low byte as availability state, while the upper
//   24 bits currently read best as stack-carried noise from the client's `0x621e1a70` full-slot copy path
// - this is the same broader table family that `ai.cfg` availability processing touches at
//   `+0x6e0 + actionId*8`
static uint32_t __thiscall Mediator_HasLiveCorpus78(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveCsCfg78();
}

// anchor: client.dll:0x62198b70 / launcher.exe vtable +0x7c -> raw bytes 0x41f110 = owner byte +0x13fe
// Exact current bl.cfg pair:
// - client helper `0x62198b70` uses arg6 `+0x7c`, then `+0xa8`, for `bl.cfg`
// - original launcher `+0x7c` returns owner byte `+0x13fe`
// - original launcher `+0xa8` returns owner pointer `+0x13f8` and writes out-length `+0x13fc`
// - recovered state8 slot-6 producer writes that same owner family from section selector `1`
static uint32_t __thiscall Mediator_HasLiveCorpus7c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveBlCfg7c();
}

// anchor: client.dll:0x62198c60 / launcher.exe vtable +0x80 -> raw bytes 0x41f120 = owner byte +0x1406
// Exact current il.cfg pair:
// - client helper `0x62198c60` uses arg6 `+0x80`, then `+0xac`, for `il.cfg`
// - original launcher `+0x80` returns owner byte `+0x1406`
// - original launcher `+0xac` returns owner pointer `+0x1400` and writes out-length `+0x1404`
// - recovered state8 slot-6 producer writes that same owner family from section selector `2`
static uint32_t __thiscall Mediator_HasLiveCorpus80(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveIlCfg80();
}

// anchor: client.dll:0x62198d50 / launcher.exe vtable +0x84 -> raw bytes 0x41f130 = owner byte +0x1448
// Exact current rl.cfg pair:
// - client helper `0x62198d50` uses arg6 `+0x84`, then `+0xb0`, for `rl.cfg`
// - original launcher `+0x84` returns owner byte `+0x1448`
// - original launcher `+0xb0` returns owner pointer `+0x1440` and writes out-length `+0x1444`
// - recovered state8 slot-6 producer writes that same owner family from section selector `8`
static uint32_t __thiscall Mediator_HasLiveCorpus84(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveRlCfg84();
}

// anchor: client.dll:0x62198e50 / launcher.exe vtable +0x88 -> raw bytes 0x41f140 = owner byte +0x1452
// Exact current cl.cfg pair:
// - client helper `0x62198e50` uses arg6 `+0x88`, then `+0xb4`, for `cl.cfg`
// - original launcher `+0x88` returns owner byte `+0x1452`
// - original launcher `+0xb4` returns owner pointer `+0x144c` and writes out-length `+0x1450`
// - recovered state8 slot-6 producer writes that same owner family from section selector `9`
static uint32_t __thiscall Mediator_HasLiveCorpus88(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveClCfg88();
}

static_assert(
    sizeof(mxo::ltlogin::State3SelectionContextInputSketch) == kDiagnosticSelectionContextSize,
    "State3SelectionContextInputSketch must stay layout-compatible with the recovered arg6 +0xec 0xb4 snapshot");

// anchor: launcher.exe:0x41f150
// vtable: ILTLoginMediator.Default slot +0x8c
// Live original `client.dll:0x62198fa0` mcd.cfg family uses this as the mediator-backed/live-data gate.
// Exact corrected original getter proof from launcher disassembly:
// - `0x41f150` returns owner byte `+0x13f6`
// - `+0x1452` is instead the neighboring `cl.cfg` gate used by arg6 `+0x88`
static uint32_t __thiscall Mediator_HasState8PersistenceData8c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasState8PersistenceData8c();
}

// anchor: client.dll:0x621993d0 / launcher.exe vtable +0x90 -> raw bytes 0x41f160 = owner byte +0x145a
// Exact current cui.cfg pair:
// - client helper `0x621993d0` uses arg6 `+0x90`, then `+0xb8`, for `cui.cfg`
// - original launcher `+0x90` returns owner byte `+0x145a`
// - original launcher `+0xb8` returns owner pointer `+0x1454` and writes out-length `+0x1458`
// - recovered state8 slot-6 producer writes that same owner family from section selector `10`
// - newer client-side tightening matters for the current remaining narrow mismatch:
//   - if `+0x90` is false, `0x621993d0` only tries to load an existing on-disk `cui.cfg`
//   - later direct-save helper `0x62198490` can still call `0x62197050` and write `cui.cfg`
//     from the client-owned `0x629e05bc` object during shutdown-side persistence
//   - current bounded replacement route still emits on-disk `cui.cfg` that way even though the
//     live mediator pair stays absent, while the bounded original route still omits the file
static uint32_t __thiscall Mediator_HasLiveCorpus90(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasLiveCuiCfg90();
}

static void* __thiscall Mediator_GetLiveCorpus94(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveHlCfg94(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus98(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveAnCfg98(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus9c(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLivePiCfg9c(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveAiCfgA0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveCsCfgA4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveBlCfgA8(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusAc(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveIlCfgAc(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveRlCfgB0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveClCfgB4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetLiveCuiCfgB8(outLength);
}

// anchor: launcher.exe:0x41f170
// vtable: ILTLoginMediator.Default slot +0xbc
// Live original `client.dll:0x62198fa0` copies 0x20 bytes from this pointer into DAT_629ea67c.
static void* __thiscall Mediator_GetState8PersistenceHeaderBc(MinimalLoginMediatorStub* self) {
    (void)self;
    return const_cast<void*>(mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceHeaderBc());
}

// anchor: launcher.exe:0x41f180
// vtable: ILTLoginMediator.Default slot +0xc0
// Live original `client.dll:0x62198fa0` copies 0x465 bytes from this pointer into DAT_629ea648-backed state.
// Post-event-0x18 continuation note:
// - event `0x0b` later reads byte `+0x464` from this returned block into client global
//   `DAT_629e689d`
// - `0x621704a0` then uses that same global as an early state-0 branch gate before any possible
//   `ClientViewFactory_GetOrCreateViewById(0x67)` / `0x6298a5e8` observer-registration path
static void* __thiscall Mediator_GetState8PersistenceBodyC0(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    void* const body = const_cast<void*>(mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceBodyC0());
    const uint8_t byte464 = body ? *(reinterpret_cast<const uint8_t*>(body) + 0x464u) : 0u;
    spdlog::info(
        "MediatorStub::GetState8PersistenceBodyC0 caller={} [{}] result={} byte464=0x{:02x}",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(body),
        static_cast<unsigned>(byte464));
    return body;
}

// anchor: launcher.exe:0x41aec0
// vtable: ILTLoginMediator.Default slot +0xc4
// Live original `client.dll:0x62198fa0` asks for the optional overflow tail pointer plus out-length.
static void* __thiscall Mediator_GetState8PersistenceOverflowC4(MinimalLoginMediatorStub* self, uint16_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8PersistenceOverflowC4(outLength);
}

// anchor: launcher.exe:0x41f190
// vtable: ILTLoginMediator.Default slot +0xc8
static uint32_t __thiscall Mediator_HasState8Section11DataC8(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->HasState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1a0
// vtable: ILTLoginMediator.Default slot +0xcc
static uint32_t __thiscall Mediator_GetState8Section11DwordCc(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1b0
// vtable: ILTLoginMediator.Default slot +0xd0
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* __thiscall Mediator_GetState8Section11StringD0(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState8Section11String1460();
}

// anchor: launcher.exe:0x41b4f0 / arg6 vtable +0xd4
// Current active client-side state9 use from `0x620065e0`:
// - returns the 16-byte source pointer then consumed with size `0x10` by `0x62530630`
// - practical current read is the same launcher-owned Twofish key/seed family reused by `+0x18c`
static const void* __thiscall Mediator_GetState9CallbackSeedPointer85D4(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetState9CallbackSeedPointer85D4();
}

// anchor: client.dll:0x62170b00 gates arg7 high-byte selection flow through arg6 +0xd8
// vtable: ILTLoginMediator.Default slot +0xd8
// Tighter launcher page-`7` read now keeps this high byte aligned with the active selection-entry
// count, and on the auth-valid path that is currently better modeled through owner slot records.
static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetArg7SelectionUpperBoundExclusive();
}

// anchor: deeper client init maps arg7-derived selection names through arg6 +0xdc
// vtable: ILTLoginMediator.Default slot +0xdc
// Tighter launcher page-`7` read now keeps this closer to the active selection-entry display text
// (auth-valid path: slot-record / character-entry name by selected-row high word).
static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->MapSelectionName(selectionHighByte);
}

// launcher.exe arg7-selection resolution still reuses arg6 `+0x54` as a generic bool gate when
// deciding whether to accept world-type values `2/5`, but the slot body itself is now anchored as
// the tiny `+0x50` truthiness wrapper at launcher.exe:0x41f0b0.

// anchor: arg7-selection resolution consults the sibling ILTLoginMediator surface through +0xe0
// vtable: ILTLoginMediator.Default slot +0xe0
// Tighter launcher page-`7` read now keeps this closer to the active selection-entry world-match
// string (auth-valid path: slot-record worldId -> world-descriptor inline name).
static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetVariantWorldName(variantIndex);
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0xe4
// vtable: ILTLoginMediator.Default slot +0xe4
// Tighter launcher page-`7` read now keeps this high-word consumer aligned with active
// selection-entry status, i.e. slot-record status on the auth-valid path.
static uint32_t __thiscall Mediator_GetVariantState(MinimalLoginMediatorStub* self, int32_t variantIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->GetVariantState(variantIndex);
}


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
// - wrapper also logs the exact client return address so the successful post-0x18 route can prove
//   whether the current run actually executed the event-0x18 body or skipped it on observer byte
//   `this+0xcc`
static mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* __thiscall Mediator_GetRouteDescriptor10c(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    mxo::ltlogin::RouteDescriptor30SmallStringLikeSketch* const descriptor =
        mxo::ltlogin::ILTLoginMediator::Default->GetRouteDescriptor30();
    const std::string descriptorText = DescribeRouteDescriptorText(descriptor);
    spdlog::info(
        "MediatorStub::GetRouteDescriptor10c caller={} [{}] result={} begin={} current={} text='{}'",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(descriptor),
        fmt::ptr(descriptor ? descriptor->begin : nullptr),
        fmt::ptr(descriptor ? descriptor->current : nullptr),
        descriptorText);
    return descriptor;
}

// anchor: launcher.exe:0x41af50
// vtable: ILTLoginMediator.Default slot +0x118
// Current best late-runtime read:
// - returns owner `+0x1470`
// - client reads it as a vector-like begin/current/capacity triple of 12-byte string-triple entries
// - immediate event-0x18 helper `0x621c6d90` and later consumer `0x62017150` both use this slot
// - wrapper logs the exact client return address so successful runs can show whether only the
//   immediate event-0x18 helper fired or the later metric-matcher path also ran
static mxo::ltlogin::LateEntryList1470VectorLikeSketch* __thiscall Mediator_GetLateEntryList118(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    mxo::ltlogin::LateEntryList1470VectorLikeSketch* const list =
        mxo::ltlogin::ILTLoginMediator::Default->GetLateEntryList1470();
    size_t entryCount = 0u;
    const char* firstEntry = "<empty>";
    if (list && list->begin && list->current && list->current >= list->begin) {
        entryCount = static_cast<size_t>(list->current - list->begin);
        if (entryCount != 0u && list->begin->begin && list->begin->begin[0] != '\0') {
            firstEntry = list->begin->begin;
        }
    }
    spdlog::info(
        "MediatorStub::GetLateEntryList118 caller={} [{}] result={} begin={} current={} capacity={} entryCount={} firstEntry='{}'",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(list),
        fmt::ptr(list ? list->begin : nullptr),
        fmt::ptr(list ? list->current : nullptr),
        fmt::ptr(list ? list->capacity : nullptr),
        entryCount,
        firstEntry);
    return list;
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
        // Current bounded client-side proof:
        // - `ClientShell_OnEngineInitialized` (`0x6216f060`) earlier pushes a direct visible status
        //   sequence including:
        //   - `Initializing Client Data Cache`
        //   - `Initializing Inventory Manager`
        //   - `Initializing Shortcut Manager`
        //   - `Initializing Game Object Manager`
        //   - `Initializing Character Animations`
        //   - `Initializing Rules Subsystem`
        //   - `Initializing Animation Tables`
        //   - `Initializing Chat Manager`
        //   - `Initializing Abilities`
        //   - `Initializing FX`
        //   - `Initializing Metro World`
        // - `InitClientDLL_BeginLoadingCharacterFlow` then sets visible text `"Loading Character"`
        //   at `0x62170f2a`
        // - it then immediately calls arg6 `+0xec` at `0x62170f48`
        // Practical replacement stance:
        // - keep exact late text mirrors where we have exact caller-side boundaries (`Loading Character`)
        // - for the earlier engine-init text family, emit a one-shot retrospective mirror here once
        //   the path is proven to have passed through `0x6216f060`, without patching client.dll.
        DiagnosticLogKnownClientEngineInitStatusTextsOnce(
            "client.dll:ClientShell_OnEngineInitialized retrospective mirror before arg6 +0xec");
        DiagnosticLogClientLoadingStateText(
            "Loading Character",
            "client.dll:InitClientDLL_BeginLoadingCharacterFlow before arg6 +0xec");
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
// - current best read is create-character source-block submit, but preserve the instance-role
//   split between the wrapper mirror and whichever mediator currently owns the active state source
extern "C" uint32_t Mediator_ProcessCreateCharacterInput120_Impl(
    MinimalLoginMediatorStub* self,
    void* input120,
    void* returnAddress) {
    (void)self;

    uint32_t result = 1u;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    mxo::ltlogin::CLTLoginMediator* activeStateSource =
        mxo::ltlogin::CLTLoginMediator::ActiveStateSourceScaffold();

    if (mediator) {
        const bool applyOwnerSemantics = (activeStateSource == nullptr || activeStateSource == mediator);
        result = mediator->CaptureCreateCharacterInputArg6Slot120(
            input120,
            returnAddress,
            applyOwnerSemantics);
    }

    if (activeStateSource && activeStateSource != mediator) {
        result = activeStateSource->CaptureCreateCharacterInputArg6Slot120(
            input120,
            returnAddress,
            true);
    }

    return result;
}

// anchor: later loading-character path around client.dll:0x620547c0..0x62054eac passes the
// post-auth create-character source block to arg6 +0x120
// vtable: ILTLoginMediator.Default slot +0x120
__attribute__((naked)) static void Mediator_ProcessCreateCharacterInput120() {
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
        : "i"(Mediator_ProcessCreateCharacterInput120_Impl)
        : "eax", "edx");
}

// UNANCHORED: near-direct C helper behind the recovered wrapper-facing +0x124 ABI slot.
extern "C" void Mediator_ProvideStartupTriple_Impl(
    MinimalLoginMediatorStub* self,
    void* pNetShell,
    void* pNetMgr,
    void* pDistrObjExecutive,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;
    if (mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel()) {
        mediator->ProvideStartupTriple(pNetShell, pNetMgr, pDistrObjExecutive);
    }
}

// anchor: deeper client init hands netShell/netMgr/distrObjExecutive to arg6 +0x124
// vtable: ILTLoginMediator.Default slot +0x124
// Important later state9-submit tightening from newer client.dll + launcher evidence:
// - the captured `netShell` object is not a self-contained callback84-side answer source
// - `ClientNetShell` vtable `+0x38` / `0x62006580` later re-enters the client-side resolved
//   `ILTLoginMediator.Default` global at `0x629df7f0`, calls its `+0x18c` writer, and only then
//   returns pair `(&DAT_629e0284, 0x20)`
// - but newer bounded original-launcher lifecycle proof now also shows owner `+0x84/+0x88/+0x8c`
//   are really zero-init -> `0x41f1d0` startup store -> later submit-side reads, with owner
//   `+0x88` unchanged through `0x439780 -> 0x41de40 -> 0x43c180`
// - so the live replacement path now preserves the same-run startup `+0x124` triple directly,
//   while pairing that with a source-owned `+0x18c` implementation instead of cross-run object
//   transplant or hardcoded callback bytes
__attribute__((naked)) static void Mediator_ProvideStartupTriple() {
    __asm__ volatile(
        "mov 0(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $20, %%esp\n\t"
        "ret $12\n\t"
        :
        : "i"(Mediator_ProvideStartupTriple_Impl)
        : "eax");
}

// anchor: launcher.exe:0x4202c0
// vtable: ILTLoginMediator.Default slot +0x13c
// WaitForEvent uses this repeatedly while blocked on registered observer notifications.
static void __thiscall Mediator_InvokeSessionCallbackHelper13c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator::Default->HelperSlot13c_InvokeSessionHelperVtable4();
}

// UNANCHORED: C helper behind the recovered +0x170 observer-registration ABI wrapper.
// Wrapper now forwards to CLTLoginMediator::RegisterLoginObserver; owner keeps the tree state while
// the ABI shell logs the exact client callsite (`InitClientDLL`, `LoadingAreaCommonLayoutView_ctor`,
// `RsiLayoutsView_ctor`, etc.) so post-event-0x18 runs can prove which observer registrations did
// or did not happen.
extern "C" uint32_t Mediator_RegisterLoginObserver170_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    (void)self;

    const uint32_t result = mxo::ltlogin::ILTLoginMediator::Default->RegisterLoginObserver(observer);
    spdlog::info(
        "MediatorStub::RegisterLoginObserver170 caller={} [{}] observer={} ({}) -> returnValue={}",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(observer),
        DescribeKnownMediatorObserver(observer),
        result);
    return result;
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
// Wrapper now forwards to CLTLoginMediator::UnregisterLoginObserver; owner keeps the tree state
// while the ABI shell logs exact client callsites so paired view ctor/dtor observer lifetimes stay
// visible during late-runtime investigation.
extern "C" uint32_t Mediator_UnregisterLoginObserver174_Impl(
    MinimalLoginMediatorStub* self,
    void* observer,
    void* returnAddress) {
    (void)self;

    const uint32_t result = mxo::ltlogin::ILTLoginMediator::Default->UnregisterLoginObserver(observer);
    spdlog::info(
        "MediatorStub::UnregisterLoginObserver174 caller={} [{}] observer={} ({}) -> returnValue={}",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(observer),
        DescribeKnownMediatorObserver(observer),
        result);
    return result;
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

// UNANCHORED: C helper behind the recovered +0x18c ABI wrapper.
extern "C" uint32_t Mediator_FillState9CallbackBlob18c_Impl(
    MinimalLoginMediatorStub* self,
    void* outBuffer,
    uint32_t arg2,
    uint32_t arg3,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator || !outBuffer) {
        return 1u;
    }

    return mediator->FillState9CallbackBlob18c(
        static_cast<uint32_t*>(outBuffer),
        arg2,
        arg3);
}

// anchor: launcher.exe:0x41e690 / arg6 vtable +0x18c
__attribute__((naked)) static void Mediator_FillState9CallbackBlob18c() {
    __asm__ volatile(
        "mov 0(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "mov 16(%%esp), %%eax\n\t"
        "push %%eax\n\t"
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $20, %%esp\n\t"
        "ret $12\n\t"
        :
        : "i"(Mediator_FillState9CallbackBlob18c_Impl)
        : "eax");
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 conditionally checks arg6 +0x164
// vtable: ILTLoginMediator.Default slot +0x164
// Wrapper-minimization note:
// - keep the wrapper-facing teardown meaning explicit here (`WaitForEvent(1)` predicate)
// - owner-side state/logging lives on `CLTLoginMediator`
static uint32_t __thiscall Mediator_RequestAuthConnectionCloseWaitEvent1(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->RequestAuthConnectionCloseWaitEvent1() ? 1u : 0u;
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 conditionally checks arg6 +0x16c
// vtable: ILTLoginMediator.Default slot +0x16c
// Keep the wrapper-facing split explicit:
// - teardown uses this as the `WaitForEvent(0x0f)` predicate
// - owner-side state9 success still has its own method name on `CLTLoginMediator`
static uint32_t __thiscall Mediator_RequestMarginConnectionCloseWaitEvent0f(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator::Default->RequestMarginConnectionCloseWaitEvent0f() ? 1u : 0u;
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
    g_LoginMediatorVtable[12] = (void*)Mediator_ProcessLoginRequest30; // +0x30
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
    g_LoginMediatorVtable[53] = (void*)Mediator_GetState9CallbackSeedPointer85D4; // +0xd4
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
    g_LoginMediatorVtable[72] = (void*)Mediator_ProcessCreateCharacterInput120; // +0x120
    g_LoginMediatorVtable[73] = (void*)Mediator_ProvideStartupTriple; // +0x124
    g_LoginMediatorVtable[79] = (void*)Mediator_InvokeSessionCallbackHelper13c; // +0x13c
    g_LoginMediatorVtable[82] = (void*)Mediator_GetGameSessionId; // +0x148
    g_LoginMediatorVtable[89] = (void*)Mediator_RequestAuthConnectionCloseWaitEvent1;   // +0x164
    g_LoginMediatorVtable[91] = (void*)Mediator_RequestMarginConnectionCloseWaitEvent0f;   // +0x16c
    g_LoginMediatorVtable[92] = (void*)Mediator_RegisterLoginObserver170; // +0x170
    g_LoginMediatorVtable[93] = (void*)Mediator_UnregisterLoginObserver174; // +0x174
    g_LoginMediatorVtable[94] = (void*)Mediator_GetLastLoginStatus; // +0x178
    g_LoginMediatorVtable[99] = (void*)Mediator_FillState9CallbackBlob18c; // +0x18c

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
// Current tighter page-`7` read:
// - `requestedWorldIndexLow24` mirrors the selected row low word (`0x40e480` total-world index)
// - `requestedSelectionIndexHighWord` mirrors the selected row high word (`0x40e480` active-entry
//   index, currently tighter on the auth-valid path as the slot-record / character-entry index)
// - `0x40d6f0` sign-extends that high word before writing `CLauncher+0xa8`
bool DiagnosticResolveLauncherSelectionFromMediator(
    void* mediatorPtr,
    uint32_t requestedWorldIndexLow24,
    uint32_t requestedSelectionIndexHighWord,
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
    // - launcher arg7-selection resolution reuses wrapper slot `+0x54` only on the exact
    //   selection-gate-byte `2/5` branch recovered through `0x40cdb0`
    // - gate byte `1` is accepted unconditionally there
    // - any other gate byte is rejected before the final `0x4d3410/0x4d3414` writeback
    // - the slot body itself is now named from the owner-side evidence: a tiny `+0x50`
    //   truthiness wrapper over the auth/bootstrap raw08 aux-handle family
    // - on the auth-valid launcher page-`7` path, `+0xe4` is now tighter as slot-record status by
    //   selected-row high word rather than a generic world-variant code
    NoArgUIntFn hasBootstrapRaw08AuxHandle54Fn = (NoArgUIntFn)vtable[21]; // +0x54
    IndexStringFn worldNameFn = (IndexStringFn)vtable[63];         // +0xfc
    IndexUIntFn selectionGateByte100Fn = (IndexUIntFn)vtable[64];  // +0x100
    SignedIndexUIntFn variantStateFn = (SignedIndexUIntFn)vtable[57]; // +0xe4

    const uint32_t worldIndexLow24 = requestedWorldIndexLow24 & 0x00ffffffu;
    const uint32_t selectionIndexHighWord = requestedSelectionIndexHighWord & 0xffffu;
    const char* worldName = worldNameFn(mediatorPtr, worldIndexLow24);
    const uint32_t selectionGateByte100 = selectionGateByte100Fn(mediatorPtr, worldIndexLow24);

    bool selectionGateAccepted = false;
    if (selectionGateByte100 == 1u) {
        selectionGateAccepted = true;
    } else if (selectionGateByte100 == 2u || selectionGateByte100 == 5u) {
        selectionGateAccepted = hasBootstrapRaw08AuxHandle54Fn(mediatorPtr) != 0;
    }

    const int32_t signedSelectionIndex = static_cast<int16_t>(selectionIndexHighWord);
    const uint32_t variantState = variantStateFn(mediatorPtr, signedSelectionIndex);
    const bool variantAccepted = (variantState == 0u || variantState == 7u);

    if (!worldName || !selectionGateAccepted || !variantAccepted) {
        spdlog::info(
            "DIAGNOSTIC: launcher selection resolve failed worldIndexLow24=0x{:06x} selectionIndexHighWord=0x{:04x} signedSelectionIndex={} worldName={} selectionGateByte100={} selectionGateAccepted={} variantState={} variantAccepted={}",
            worldIndexLow24,
            selectionIndexHighWord,
            signedSelectionIndex,
            worldName ? worldName : "<null>",
            selectionGateByte100,
            selectionGateAccepted ? 1u : 0u,
            variantState,
            variantAccepted ? 1u : 0u);
        return false;
    }

    *outFieldA8 = static_cast<uint32_t>(signedSelectionIndex);
    *outFieldAC = worldIndexLow24;

    if (outWorldName && outWorldNameCapacity) {
        outWorldName[0] = '\0';
        std::strncpy(outWorldName, worldName, outWorldNameCapacity - 1);
        outWorldName[outWorldNameCapacity - 1] = '\0';
    }

    spdlog::info(
        "DIAGNOSTIC: launcher-style selection resolve via mediator worldIndexLow24=0x{:06x} selectionIndexHighWord=0x{:04x} signedSelectionIndex={} -> worldName='{}' selectionGateByte100={} variantState={} a8=0x{:08x} ac=0x{:08x} packed=0x{:08x}",
        worldIndexLow24,
        selectionIndexHighWord,
        signedSelectionIndex,
        worldName,
        selectionGateByte100,
        variantState,
        *outFieldA8,
        *outFieldAC,
        (*outFieldAC & 0x00ffffffu) | ((*outFieldA8 & 0xffu) << 24));
    return true;
}

void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: auth credential configure skipped (no installed CLTLoginMediator)");
        return;
    }

    mediator->SetAuthCredentials(authName, authPassword);
}

void DiagnosticConfigureLoginControllerNetwork(
    const char* authDnsName,
    uint16_t authPortHostOrder,
    bool ignoreHostsFileForAuth,
    const char* marginDnsSuffix,
    uint16_t marginPortHostOrder,
    bool ignoreHostsFileForMargin,
    const char* marginRouteHostPrefix,
    const char* exactMarginHostName) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: login controller network configure skipped (no installed CLTLoginMediator)");
        return;
    }

    mediator->SetAuthServerConfig(
        authDnsName,
        authPortHostOrder,
        ignoreHostsFileForAuth);
    mediator->SetMarginServerConfig(
        marginDnsSuffix,
        marginPortHostOrder,
        ignoreHostsFileForMargin);
    mediator->SetMarginRouteHostPrefix(marginRouteHostPrefix);
    mediator->SetExactMarginHostName(exactMarginHostName);
    spdlog::info(
        "DIAGNOSTIC: login controller network configured auth='{}' port={} marginSuffix='{}' marginPort={} marginRoutePrefix='{}' exactMarginHost='{}' ignoreAuthHosts={} ignoreMarginHosts={}",
        authDnsName && authDnsName[0] ? authDnsName : "<empty>",
        (unsigned)authPortHostOrder,
        marginDnsSuffix && marginDnsSuffix[0] ? marginDnsSuffix : "<empty>",
        (unsigned)marginPortHostOrder,
        marginRouteHostPrefix && marginRouteHostPrefix[0] ? marginRouteHostPrefix : "<empty>",
        exactMarginHostName && exactMarginHostName[0] ? exactMarginHostName : "<empty>",
        ignoreHostsFileForAuth ? 1u : 0u,
        ignoreHostsFileForMargin ? 1u : 0u);
}

void DiagnosticConfigureLoginControllerCharacterSeed(
    const char* characterName,
    const char* gameSessionId,
    uint32_t selectedWorldIndexLow24) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: login-controller character seed configure skipped (no installed CLTLoginMediator)");
        return;
    }

    const uint32_t normalizedWorldIndex = selectedWorldIndexLow24 & 0x00ffffffu;
    const uint32_t seedResult =
        mediator->MirrorCharacterSeedIntoCreateCharacterInput120Scaffold(characterName, normalizedWorldIndex);
    if (gameSessionId && gameSessionId[0]) {
        mediator->SetGameSessionId664(gameSessionId);
    }

    spdlog::info(
        "DIAGNOSTIC: login-controller character seed configured character='{}' session='{}' selectedWorldIndexLow24=0x{:06x} mirrorResult=0x{:08x} (mirror-only source-block seed; original upstream producer still unresolved)",
        (characterName && characterName[0]) ? characterName : "<empty>",
        (gameSessionId && gameSessionId[0]) ? gameSessionId : "<empty>",
        static_cast<unsigned>(normalizedWorldIndex),
        static_cast<unsigned>(seedResult));
}

bool DiagnosticCanBeginAuthConnection() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->CanBeginLauncherAuthConnectionScaffold() : false;
}

bool DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface() {
    return g_LoginMediatorStub.vtable != nullptr && g_LoginMediatorStub.vtable[12] != nullptr;
}

uint32_t DiagnosticSubmitLoginRequestViaResolvedMediatorSurface(
    const mxo::ltlogin::ProcessLoginRequestInputSketch& input) {
    using SubmitFn = uint32_t (__thiscall*)(
        MinimalLoginMediatorStub*,
        const mxo::ltlogin::ProcessLoginRequestInputSketch*);

    if (!DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface()) {
        return 0u;
    }

    SubmitFn submitFn = reinterpret_cast<SubmitFn>(g_LoginMediatorStub.vtable[12]);
    return submitFn(&g_LoginMediatorStub, &input);
}

uint32_t DiagnosticBeginAuthConnection() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: installed CLTLoginMediator unavailable for auth connection");
        return 0u;
    }
    return mediator->BeginLauncherAuthConnectionScaffold();
}

uint32_t DiagnosticBeginMarginConnection() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        spdlog::info("DIAGNOSTIC: installed CLTLoginMediator unavailable for margin connection");
        return 0u;
    }
    return mediator->BeginLauncherMarginConnectionScaffold();
}

void DiagnosticPumpLauncherNetwork(bool nonBlocking) {
    LauncherPumpNetworkEngineAbiShell(g_pLauncherObject6304, nonBlocking);
}

void DiagnosticResetPostedLoginResult() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    if (mediator) {
        mediator->ResetPostedLoginResultScaffold();
    }
}

bool DiagnosticHasSuccessfulPreClientAuthState() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator && mediator->LastPostedEventScaffold() == 5u && mediator->RecoveredCharacterCountScaffold() != 0u;
}

uint32_t DiagnosticLastLoginEvent() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->LastPostedEventScaffold() : 0u;
}

uint32_t DiagnosticLastLoginError() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->LastPostedErrorScaffold() : 0u;
}

uint32_t DiagnosticRecoveredCharacterCount() {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->RecoveredCharacterCountScaffold() : 0u;
}

bool DiagnosticRecoveredCharacterName(uint32_t slotIndex, char* outName, uint32_t outNameCapacity) {
    if (!outName || outNameCapacity == 0u) {
        return false;
    }
    outName[0] = '\0';

    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    const mxo::ltlogin::SlotRecordState004b5328* slotRecord =
        mediator ? mediator->RecoveredCharacterByIndexScaffold(slotIndex) : nullptr;
    if (!slotRecord || slotRecord->heapString14.empty()) {
        return false;
    }

    std::strncpy(outName, slotRecord->heapString14.c_str(), outNameCapacity - 1u);
    outName[outNameCapacity - 1u] = '\0';
    return true;
}

bool DiagnosticSelectRecoveredCharacter(uint32_t slotIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->SelectRecoveredCharacterByIndexScaffold(slotIndex) : false;
}

bool DiagnosticAdoptRecoveredCharacterSelectionForLauncher(
    uint32_t slotIndex,
    char* outCharacterName,
    uint32_t outCharacterNameCapacity,
    char* outWorldName,
    uint32_t outWorldNameCapacity,
    uint32_t* outDescriptorIndex) {
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->AdoptRecoveredCharacterSelectionForLauncherScaffold(
                          slotIndex,
                          outCharacterName,
                          outCharacterNameCapacity,
                          outWorldName,
                          outWorldNameCapacity,
                          outDescriptorIndex)
                    : false;
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
