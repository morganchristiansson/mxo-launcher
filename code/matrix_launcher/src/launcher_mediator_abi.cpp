#include "diagnostics.h"
#include "launcher_mediator_abi.h"
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include <spdlog/spdlog.h>

extern void* g_pLauncherObject6304;

// Broad wrapper-facing arg6 ABI shell:
// - keep startup-selection and general mediator surface here
// - the historical `ILTLoginMediator_0x4af2b8::Default` spell is treated as naming baggage for the
//   resolved arg6 surface, not as proof of a distinct startup-owned singleton object

MinimalLoginMediatorStub g_LoginMediatorStub = {};
void* g_LoginMediatorVtable[104] = {0};

static constexpr size_t kDiagnosticSelectionContextSize = 0xb4; // from client.dll:6211d3e0 zero-init of the +0xec handoff object

// UNANCHORED: diagnostic masking helper for auth/password log surfaces.
const char* MaskedSensitiveValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

// UNANCHORED: sidecar-model accessor for the replacement arg6 ABI shell.
mxo::ltlogin::CLTLoginMediator* DiagnosticEnsureMediatorModel() {
    return mxo::ltlogin::g_CurrentLoginMediator;
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

namespace mxo::ltlogin {
struct CurrentSlotRecord44ObjectSketch {
    void** vtable;
    void* bufferBase04;
    void* backingObject08;
    uint8_t flag0c;
    uint8_t padding0d[3];
    Packet_AsAuthReply_0x4b5328* payload10;
    const char* debugString14;
    uint16_t debugStringLen18;
    uint8_t padding1a[2];
};
static_assert(offsetof(CurrentSlotRecord44ObjectSketch, payload10) == 0x10);
static_assert(offsetof(CurrentSlotRecord44ObjectSketch, debugString14) == 0x14);
static_assert(offsetof(CurrentSlotRecord44ObjectSketch, debugStringLen18) == 0x18);
static_assert(sizeof(CurrentSlotRecord44ObjectSketch) == 0x1c);
}  // namespace mxo::ltlogin

static mxo::ltlogin::CurrentSlotRecord44ObjectSketch g_MediatorSelectionDescriptor40{};

static void DiagnosticLogPacketAbiValidationOnce() {
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    using mxo::liblttcp::Packet_0x4af2a4;
    using mxo::ltlogin::Packet_AsAuthReply_0x4b5328;
    using mxo::ltlogin::CurrentSlotRecord44ObjectSketch;

    const size_t packetBaseSize = sizeof(Packet_0x4af2a4);
    const size_t authReplySize = sizeof(Packet_AsAuthReply_0x4b5328);
    const size_t currentSlotViewSize = sizeof(CurrentSlotRecord44ObjectSketch);

    spdlog::info(
        "DIAGNOSTIC: packet ABI validation Packet_0x4af2a4 sizeof=0x{:x} payloadPtr04=0x{:x} messageRef08=0x{:x} createRefParam0c=0x{:x} payloadAlias10=0x{:x} debugString14=0x{:x} payloadSize18=0x{:x}",
        static_cast<unsigned>(packetBaseSize),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, payloadPtr04)),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, messageRef08)),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, createRefParam0c)),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, payloadAlias10)),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, debugString14)),
        static_cast<unsigned>(offsetof(Packet_0x4af2a4, payloadSize18)));

    spdlog::info(
        "DIAGNOSTIC: packet ABI validation Packet_AsAuthReply_0x4b5328 sizeof=0x{:x} debugString14=0x{:x} payloadSize18=0x{:x}",
        static_cast<unsigned>(authReplySize),
        static_cast<unsigned>(offsetof(Packet_AsAuthReply_0x4b5328, debugString14)),
        static_cast<unsigned>(offsetof(Packet_AsAuthReply_0x4b5328, payloadSize18)));

    spdlog::info(
        "DIAGNOSTIC: packet ABI validation CurrentSlotRecord44ObjectSketch sizeof=0x{:x} payload10=0x{:x} debugString14=0x{:x} debugStringLen18=0x{:x}",
        static_cast<unsigned>(currentSlotViewSize),
        static_cast<unsigned>(offsetof(CurrentSlotRecord44ObjectSketch, payload10)),
        static_cast<unsigned>(offsetof(CurrentSlotRecord44ObjectSketch, debugString14)),
        static_cast<unsigned>(offsetof(CurrentSlotRecord44ObjectSketch, debugStringLen18)));

    if (packetBaseSize != 0x1cu || authReplySize != 0x1cu) {
        spdlog::critical(
            "DIAGNOSTIC: packet ABI mismatch vs launcher.exe/client.dll expectation: Packet_0x4af2a4 sizeof=0x{:x}, Packet_AsAuthReply_0x4b5328 sizeof=0x{:x}, expected both 0x1c from launcher.exe 0x4398b0/0x4401ec TrackedMalloc(0x1c)",
            static_cast<unsigned>(packetBaseSize),
            static_cast<unsigned>(authReplySize));
    }
}

static uint32_t __thiscall MediatorSelectionObject_Destroy(
    mxo::ltlogin::CurrentSlotRecord44ObjectSketch* self) {
    return self ? 1u : 0u;
}

static uint32_t __thiscall MediatorSelectionObject_GetStateId(
    mxo::ltlogin::CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

static uint32_t __thiscall MediatorSelectionObject_AppendDebugString(
    mxo::ltlogin::CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall MediatorSelectionObject_ResetPayloadForSourceDescriptor(
    mxo::ltlogin::CurrentSlotRecord44ObjectSketch* self) {
    if (!self) {
        return 0u;
    }
    self->backingObject08 = nullptr;
    self->flag0c = (self->payload10 != nullptr) ? 1u : 0u;
    return 1u;
}

static uint32_t __thiscall MediatorSelectionObject_GetPayload10(
    mxo::ltlogin::CurrentSlotRecord44ObjectSketch* self) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self ? self->payload10 : nullptr));
}

static void** MediatorSelectionObjectVtable() {
    static void* vtable[5] = {
        reinterpret_cast<void*>(MediatorSelectionObject_Destroy),
        reinterpret_cast<void*>(MediatorSelectionObject_GetStateId),
        reinterpret_cast<void*>(MediatorSelectionObject_AppendDebugString),
        reinterpret_cast<void*>(MediatorSelectionObject_ResetPayloadForSourceDescriptor),
        reinterpret_cast<void*>(MediatorSelectionObject_GetPayload10),
    };
    return vtable;
}

static mxo::ltlogin::CurrentSlotRecord44ObjectSketch* BuildMediatorSelectionDescriptorObject40AbiShim(
    const mxo::ltlogin::Packet_AsAuthReply_0x4b5328* currentSlotRecord,
    uint32_t selectionIndex,
    uint32_t defaultSelectionIndex) {
    static uint32_t s_logCount = 0u;
    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;

    if (!currentSlotRecord) {
        if (++s_logCount <= 4u) {
            spdlog::debug(
                "ILTLoginMediator_0x4af2b8.Default(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x}) -> NULL [currentSlotIndex=0x{:02x}] [count=0x{:08x}]",
                static_cast<unsigned>(selectionIndex),
                static_cast<unsigned>(low24),
                static_cast<unsigned>(high8),
                static_cast<unsigned>(defaultSelectionIndex & 0xffu),
                s_logCount);
        }
        return nullptr;
    }

    g_MediatorSelectionDescriptor40 = {};
    g_MediatorSelectionDescriptor40.vtable = MediatorSelectionObjectVtable();
    g_MediatorSelectionDescriptor40.payload10 = const_cast<mxo::ltlogin::Packet_AsAuthReply_0x4b5328*>(currentSlotRecord);
    g_MediatorSelectionDescriptor40.flag0c = 1u;

    if (++s_logCount <= 4u) {
        spdlog::debug(
            "ILTLoginMediator_0x4af2b8.Default(+0x40 selectionIndex=0x{:08x}) -> {} [currentSlotIndex=0x{:02x} slotName='{}' payload={}] [count=0x{:08x}]",
            static_cast<unsigned>(selectionIndex),
            fmt::ptr(&g_MediatorSelectionDescriptor40),
            static_cast<unsigned>(defaultSelectionIndex & 0xffu),
            currentSlotRecord->debugString14 ? currentSlotRecord->debugString14 : "<empty>",
            fmt::ptr(g_MediatorSelectionDescriptor40.payload10),
            s_logCount);
    }
    return &g_MediatorSelectionDescriptor40;
}


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

namespace {
struct Msvc2003StdStringView {
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
};

static Msvc2003StdStringView BuildMsvc2003StdStringView(std::string_view text) {
    Msvc2003StdStringView view{};
    view.begin = text.data();
    view.current = text.data() + text.size();
    view.capacity = view.current;
    return view;
}

static std::string DescribeRouteDescriptorText(const Msvc2003StdStringView* descriptor) {
    if (!descriptor || !descriptor->begin || !descriptor->current || descriptor->current < descriptor->begin) {
        return "<empty>";
    }
    if (descriptor->current == descriptor->begin) {
        return "<empty>";
    }
    return std::string(descriptor->begin, descriptor->current);
}
}  // namespace

// anchor: launcher.exe dynamic initializer uses the registration string at 0x4ab34c for ILTLoginMediator_0x4af2b8.Default
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x00
static const char* __thiscall Mediator_GetName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetName();
}

// anchor: launcher.exe:0x40a3e9..0x40a3fe hands the freshly built 0x4d6304 object into arg6 before InitClientDLL
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x08
// Fidelity note:
// - this resolved arg6 wrapper slot is the launcher-owned startup handoff used from `0x40a380`
// - do not conflate it with owner vtable `0x004b01c8 +0x0c`; current Ghidra now assigns that owner
//   slot to `0x41f510`, which looks like reset/clear logic rather than a simple engine setter
// - current source therefore keeps the wrapper handoff explicit here, routes it into
//   `CLTLoginMediator::Initialize(...)`, and limits arg5 sidecar accessors to owner<->engine
//   pairing only, without lazy mediator bind/reset
// ABI boundary warning from the current queue-subobject pass:
// - client.dll does not treat arg5 as a clean abstract interface object
// - `client.dll:0x62531c10` directly reads/writes the inline completed-operation `QueuePair`
//   subobject and also drives the embedded helper roots at `+0x5c/+0x60`
// - because of that, a fully wrapped/separate network-engine shell is not a clean long-term model
//   on the current MinGW/MSVC2003 bridge; the live engine object itself must remain layout-valid
//   for those direct cross-module subobject accesses
// - on the current native-object build, arg5 itself is the live engine object, so shell writes are
//   not the goal; however, engine-owned bookkeeping around tree/count state still needs to stay
//   faithful, so only the detached-shell publication half can be considered optional
// - keep this arg6 registration seam thin and treat later wrapper work as call/ABI adaptation,
//   not as permission to virtualize the whole arg5 object behind detached copied state
// Return-shape note from launcher.exe:0x40a380:
// - the caller turns the raw slot result into `result < 1`
// - so this scaffold returns `0` for the non-null success case and `1` when the arg5 object
//   failed to materialize, which preserves the original success/failure sense more closely
static int __thiscall Mediator_RegisterLauncherNetworkEngineObject08(
    MinimalLoginMediatorStub* self,
    void* object) {
    (void)self;

    mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* const engine =
        static_cast<mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768*>(object);
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->Initialize(engine);
    return object ? 0 : 1;
}

// anchor: client.dll early InitClientDLL readiness gate on arg6 +0x10
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x10
static uint32_t __thiscall Mediator_IsReady(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->IsReady();
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 clears the registered launcher object through arg6
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x0c
static void __thiscall Mediator_ClearEngine(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->ClearEngine();
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator_0x4af2b8.Default slots +0x1c and +0x24
static void __thiscall Mediator_SetValue1(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->SetValue1(value);
}

// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator_0x4af2b8.Default slots +0x1c and +0x24
static void __thiscall Mediator_SetValue2(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->SetValue2(value);
}

// anchor: launcher.exe:0x408400 / sibling resolved slot 0x4d2734 vtable +0x30
// Raw-vtable clarification from the same submit pass:
// - launcher page-6 rich-edit submit helper `0x408400` calls resolved mediator slot `+0x30`
// - raw memory read of launcher mediator vtable family `0x004b01c8` shows `0x41ecd0` stored at
//   `0x004b01f8`, i.e. the same raw virtual displacement
// - practical consequence: the launcher dialog submit helper reaches
//   `CLTLoginMediator::ProcessLoginRequest` through the resolved `ILTLoginMediator_0x4af2b8.Default`-style
//   surface rather than through a separate launcher-only credential API
static uint32_t __thiscall Mediator_ProcessLoginRequest30(
    MinimalLoginMediatorStub* self,
    const mxo::ltlogin::SubmitLoginRequestInput_0x407d50* input) {
    (void)self;
    return input
        ? mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->ProcessLoginRequest(*input)
        : 0u;
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x2c
static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->IsConnected();
}

// anchor: launcher.exe:0x41c0d0 / wrapper-facing arg6 slot +0x34
static void __thiscall Mediator_RequestAuthCloseAndSwitchToState0(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->RequestAuthCloseAndSwitchToState0();
}

// anchor: launcher.exe:0x41f050 / wrapper-facing arg6 slot +0x18
static uint32_t __thiscall Mediator_GetUnknownByte05(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetUnknownByte05();
}

// anchor: client.dll profile-root formatting path uses arg6 +0x38 for Profiles\%s\... construction
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x38
static const char* __thiscall Mediator_GetUsername38(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetUsername();
}

// anchor: client.dll fallback-selection path asks arg6 +0x3c for the default selection index when given 0xff
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x3c
static uint32_t __thiscall Mediator_GetDefaultSelectionIndex(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetDefaultSelectionIndex();
}

// anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
// Wrapper-facing bridge: preserve the fake outer object shape the client expects, but keep the
// owner-side lookup faithful to the direct slot-index reader.
extern "C" void* Mediator_GetSelectionDescriptor40_Impl(
    MinimalLoginMediatorStub* self,
    uint32_t selectionIndex,
    void* returnAddress) {
    (void)self;
    (void)returnAddress;
    mxo::ltlogin::CLTLoginMediator* const mediator = DiagnosticEnsureMediatorModel();
    if (!mediator) {
        return nullptr;
    }

    return BuildMediatorSelectionDescriptorObject40AbiShim(
        mediator->GetAuthReplyPacketByIndex40(selectionIndex),
        selectionIndex,
        mediator->GetDefaultSelectionIndex());
}

// anchor: client.dll:0x62170dc1..0x62170e59 later asks arg6 +0x40 with the scratch-shaped arg7 request
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x40
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

// anchor: launcher.exe:0x41f300 / owner vtable +0x44
// Thin the wrapper to the raw owner-side packet pointer.
// The previous sketch layer copied packet-facing fields into a fake outer object, but the
// recovered owner body is already just a direct slot-record read and the interface prototype is
// `Packet_AsAuthReply_0x4b5328*`. Keep the wrapper only as an ABI thunk.
extern "C" void* Mediator_GetCurrentAuthReplyPacket44_Impl(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* const mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->GetCurrentAuthReplyPacket44() : nullptr;
}

// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x44
// Current wrapper-facing read from `0x4d2c58_ILTLoginMediator_0x4af2b8_Default.md`:
// - returns the current slot record on the later profile/save path
// - this slot is now intentionally thinner than +0x40: we publish the raw `0x004b5328`
//   packet/view directly instead of re-wrapping it into `CurrentSlotRecord44ObjectSketch`
// - keep that split explicit from +0x40, which still preserves the separate wrapper-facing
//   descriptor sketch for the scratch-shaped selection request path
__attribute__((naked)) static void Mediator_GetCurrentAuthReplyPacket44() {
    __asm__ volatile(
        "push %%ecx\n\t"
        "mov %0, %%eax\n\t"
        "call *%%eax\n\t"
        "add $4, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(Mediator_GetCurrentAuthReplyPacket44_Impl)
        : "eax");
}

// anchor: later client startup path calls arg6 +0x48 before the now-better-understood
// observer registration / startup-triple handoff sequence
// practical current note from client.dll profile-path work:
// - the broader client path later formats `Profiles\%s\%s_%X\`
// - and the middle `%s` is sourced from client-global `DAT_629de48c`
// - current replacement evidence points to the earlier +0x48-fed name path as the highest-value
//   narrow source to keep character-shaped instead of world-shaped on the active route
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x48
static const char* __thiscall Mediator_GetWorldOrSelectionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldOrSelectionName();
}

// anchor: later client startup path calls arg6 +0x4c immediately after +0x48
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x4c
static const char* __thiscall Mediator_GetProfileOrSessionName(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetProfileOrSessionName();
}

// anchor: client.dll:0x625c86d0 later calls arg6 +0x50 and converts null/non-null into flag 0x30
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x50
static void* __thiscall Mediator_GetBootstrapRaw08AuxHandle50(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->BootstrapRaw08AuxHandle50();
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x54
static uint32_t __thiscall Mediator_HasBootstrapRaw08AuxHandle54(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasBootstrapRaw08AuxHandle54() ? 1u : 0u;
}

// anchor: client.dll:0x62001325..0x62001362 passes the low byte from arg6 +0x58 into
// `FUN_6236fa40(..., flag)`; launcher.exe:0x409250..0x409254 also stores that low byte into the
// crashreporter `PromptForSecurId` global.
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x58
static uint32_t __thiscall Mediator_GetCrashReporterPromptForSecurId58(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetCrashReporterPromptForSecurId58();
}

// UNANCHORED: C helper behind the caller-clean +0x60 ABI wrapper.
// Keep the chained value opaque here:
// - client `InitClientDLL` threads the previous return value through this slot
// - launcher crashreporter seeding calls the same slot with no stack argument on its path
extern "C" const char* Mediator_GetCrashReporterPassword60_Impl(
    MinimalLoginMediatorStub* self,
    const void* chainedValueToken) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetCrashReporterPassword60(chainedValueToken);
}

// anchor: client.dll early auth-name chain proves arg6 +0x60 is caller-clean on this path
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x60
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

// anchor: launcher.exe:0x41f2b0 / owner vtable +0x64
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x64
static uint32_t __thiscall Mediator_GetBootstrapSuccessHeaderDword64(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetBootstrapSuccessHeaderDword64();
}

// UNANCHORED: C helper behind the caller-clean +0x5c ABI wrapper.
// Keep the chained value opaque here for the same launcher/client split as `+0x60`.
extern "C" const char* Mediator_GetCrashReporterUsername5c_Impl(
    MinimalLoginMediatorStub* self,
    const void* chainedValueToken) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetCrashReporterUsername5c(chainedValueToken);
}

// anchor: client.dll early auth-name chain proves arg6 +0x5c is caller-clean on this path
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x5c
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
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveHlCfg68();
}

// anchor: client.dll:0x62198770 / launcher.exe vtable +0x6c -> 0x41f0d0
// Exact current next narrow pair:
// - client helper `0x62198770` uses arg6 `+0x6c`, then `+0x98`, for `an.cfg`
// - original launcher `+0x6c` returns owner byte `+0x1416`
// - original launcher `+0x98` returns owner pointer `+0x1410` and writes out-length `+0x1414`
// - recovered state8 slot-6 producer writes that same owner family from section selector `7`
static uint32_t __thiscall Mediator_HasLiveCorpus6c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveAnCfg6c();
}

// anchor: client.dll:0x62198870 / launcher.exe vtable +0x70
// Exact current inventory/loadout-targeted pair:
// - client helper `0x62198870` uses arg6 `+0x70`, then `+0x9c`, for `pi.cfg`
// - original launcher `+0x70` returns owner byte `+0x141e`
// - original launcher `+0x9c` returns owner pointer `+0x1418` and writes out-length `+0x141c`
// - recovered state8 slot-6 producer writes that same owner family from section selector `3`
static uint32_t __thiscall Mediator_HasLiveCorpus70(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLivePiCfg70();
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
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveAiCfg74();
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
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveCsCfg78();
}

// anchor: client.dll:0x62198b70 / launcher.exe vtable +0x7c -> raw bytes 0x41f110 = owner byte +0x13fe
// Exact current bl.cfg pair:
// - client helper `0x62198b70` uses arg6 `+0x7c`, then `+0xa8`, for `bl.cfg`
// - original launcher `+0x7c` returns owner byte `+0x13fe`
// - original launcher `+0xa8` returns owner pointer `+0x13f8` and writes out-length `+0x13fc`
// - recovered state8 slot-6 producer writes that same owner family from section selector `1`
static uint32_t __thiscall Mediator_HasLiveCorpus7c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveBlCfg7c();
}

// anchor: client.dll:0x62198c60 / launcher.exe vtable +0x80 -> raw bytes 0x41f120 = owner byte +0x1406
// Exact current il.cfg pair:
// - client helper `0x62198c60` uses arg6 `+0x80`, then `+0xac`, for `il.cfg`
// - original launcher `+0x80` returns owner byte `+0x1406`
// - original launcher `+0xac` returns owner pointer `+0x1400` and writes out-length `+0x1404`
// - recovered state8 slot-6 producer writes that same owner family from section selector `2`
static uint32_t __thiscall Mediator_HasLiveCorpus80(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveIlCfg80();
}

// anchor: client.dll:0x62198d50 / launcher.exe vtable +0x84 -> raw bytes 0x41f130 = owner byte +0x1448
// Exact current rl.cfg pair:
// - client helper `0x62198d50` uses arg6 `+0x84`, then `+0xb0`, for `rl.cfg`
// - original launcher `+0x84` returns owner byte `+0x1448`
// - original launcher `+0xb0` returns owner pointer `+0x1440` and writes out-length `+0x1444`
// - recovered state8 slot-6 producer writes that same owner family from section selector `8`
static uint32_t __thiscall Mediator_HasLiveCorpus84(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveRlCfg84();
}

// anchor: client.dll:0x62198e50 / launcher.exe vtable +0x88 -> raw bytes 0x41f140 = owner byte +0x1452
// Exact current cl.cfg pair:
// - client helper `0x62198e50` uses arg6 `+0x88`, then `+0xb4`, for `cl.cfg`
// - original launcher `+0x88` returns owner byte `+0x1452`
// - original launcher `+0xb4` returns owner pointer `+0x144c` and writes out-length `+0x1450`
// - recovered state8 slot-6 producer writes that same owner family from section selector `9`
static uint32_t __thiscall Mediator_HasLiveCorpus88(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveClCfg88();
}

static_assert(
    sizeof(mxo::ltlogin::State3SelectionContextInputSketch) == kDiagnosticSelectionContextSize,
    "State3SelectionContextInputSketch must stay layout-compatible with the recovered arg6 +0xec 0xb4 snapshot");

// anchor: launcher.exe:0x41f150
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x8c
// Live original `client.dll:0x62198fa0` mcd.cfg family uses this as the mediator-backed/live-data gate.
// Exact corrected original getter proof from launcher disassembly:
// - `0x41f150` returns owner byte `+0x13f6`
// - `+0x1452` is instead the neighboring `cl.cfg` gate used by arg6 `+0x88`
static uint32_t __thiscall Mediator_HasState8PersistenceData8c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasState8PersistenceData8c();
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
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasLiveCuiCfg90();
}

static void* __thiscall Mediator_GetLiveCorpus94(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveHlCfg94(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus98(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveAnCfg98(outLength);
}

static void* __thiscall Mediator_GetLiveCorpus9c(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLivePiCfg9c(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveAiCfgA0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveCsCfgA4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusA8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveBlCfgA8(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusAc(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveIlCfgAc(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB0(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveRlCfgB0(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB4(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveClCfgB4(outLength);
}

static void* __thiscall Mediator_GetLiveCorpusB8(MinimalLoginMediatorStub* self, uint32_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLiveCuiCfgB8(outLength);
}

// anchor: launcher.exe:0x41f170
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xbc
// Live original `client.dll:0x62198fa0` copies 0x20 bytes from this pointer into DAT_629ea67c.
static void* __thiscall Mediator_GetState8PersistenceHeaderBc(MinimalLoginMediatorStub* self) {
    (void)self;
    return const_cast<void*>(mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState8PersistenceHeaderBc());
}

// anchor: launcher.exe:0x41f180
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xc0
// Live original `client.dll:0x62198fa0` copies 0x465 bytes from this pointer into DAT_629ea648-backed state.
// Post-event-0x18 continuation note:
// - event `0x0b` later reads byte `+0x464` from this returned block into client global
//   `DAT_629e689d`
// - `0x621704a0` then uses that same global as an early state-0 branch gate before any possible
//   `ClientViewFactory_GetOrCreateViewById(0x67)` / `0x6298a5e8` observer-registration path
static void* __thiscall Mediator_GetState8PersistenceBodyC0(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    void* const body = const_cast<void*>(mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState8PersistenceBodyC0());
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xc4
// Live original `client.dll:0x62198fa0` asks for the optional overflow tail pointer plus out-length.
static void* __thiscall Mediator_GetState8PersistenceOverflowC4(MinimalLoginMediatorStub* self, uint16_t* outLength) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState8PersistenceOverflowC4(outLength);
}

// anchor: launcher.exe:0x41f190
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xc8
static uint32_t __thiscall Mediator_HasState8Section11DataC8(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HasState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1a0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xcc
static uint32_t __thiscall Mediator_GetState8Section11DwordCc(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState8Section11Dword145c();
}

// anchor: launcher.exe:0x41f1b0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xd0
static Msvc2003StdStringView* __thiscall Mediator_GetState8Section11StringD0(MinimalLoginMediatorStub* self) {
    (void)self;
    static thread_local Msvc2003StdStringView view;
    view = BuildMsvc2003StdStringView(mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState8Section11String1460());
    return &view;
}

// anchor: launcher.exe:0x41b4f0 / arg6 vtable +0xd4
// Current active client-side state9 use from `0x620065e0`:
// - returns the 16-byte source pointer then consumed with size `0x10` by `0x62530630`
// - practical current read is the same launcher-owned Twofish key/seed family reused by `+0x18c`
static const void* __thiscall Mediator_GetState9CallbackSeedPointer85D4(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetState9CallbackSeedPointer85D4();
}

// anchor: launcher.exe:0x41f310 / wrapper-facing arg6 slot +0x130
static mxo::ltlogin::LaunchPadClient_0x4b0e48* __thiscall Mediator_GetLaunchPadClient65c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLaunchPadClient65c();
}

// anchor: launcher.exe:0x420d00 / wrapper-facing arg6 slot +0x134
static mxo::ltlogin::LaunchPadClient_0x4b0e48* __thiscall Mediator_EnsureLaunchPadClient65c(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->EnsureLaunchPadClient65c();
}

// anchor: client.dll:0x62170b00 gates arg7 high-byte selection flow through arg6 +0xd8
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xd8
// Tighter launcher page-`7` read now keeps this high byte aligned with the active selection-entry
// count, and on the auth-valid path that is currently better modeled through owner slot records.
static uint32_t __thiscall Mediator_GetArg7SelectionUpperBoundExclusive(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetArg7SelectionUpperBoundExclusive();
}

// anchor: deeper client init maps arg7-derived selection names through arg6 +0xdc
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xdc
// Tighter launcher page-`7` read now keeps this closer to the active selection-entry display text
// (auth-valid path: slot-record / character-entry name by selected-row high word).
static const char* __thiscall Mediator_MapSelectionName(MinimalLoginMediatorStub* self, uint32_t selectionHighByte) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->MapSelectionName(selectionHighByte);
}

// launcher.exe arg7-selection resolution still reuses arg6 `+0x54` as a generic bool gate when
// deciding whether to accept world-type values `2/5`, but the slot body itself is now anchored as
// the tiny `+0x50` truthiness wrapper at launcher.exe:0x41f0b0.

// anchor: arg7-selection resolution consults the sibling ILTLoginMediator_0x4af2b8 surface through +0xe0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xe0
// Tighter launcher page-`7` read now keeps this closer to the active selection-entry world-match
// string (auth-valid path: slot-record worldId -> world-descriptor inline name).
static const char* __thiscall Mediator_GetVariantWorldName(MinimalLoginMediatorStub* self, uint32_t variantIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetVariantWorldName(variantIndex);
}

// anchor: launcher.exe:0x41b2a0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xe4
// Direct slot-record status reader used by the page-7 row builder / resolver; the packed row high
// word may be sign-extended here, but the original body naturally returns 7 for out-of-range input.
static uint32_t __thiscall Mediator_GetSlotRecordStatusBySelectionIndex(MinimalLoginMediatorStub* self, int32_t selectionIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetSlotRecordStatusBySelectionIndex(selectionIndex);
}


// anchor: launcher.exe:0x40e5b0 = ILTLoginMediator_0x4af2b8_GetWorldListCount
// vtable: launcher.exe:0x4d3584 slot +0xf8
static uint32_t __thiscall Mediator_GetWorldCount(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldCount();
}

// anchor: launcher.exe:0x40cd10 = ILTLoginMediator_0x4af2b8_GetWorldNameByIndex
// vtable: launcher.exe:0x4d3584 slot +0xfc
static const char* __thiscall Mediator_GetWorldNameByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldNameByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b320 / launcher.exe arg7-selection writer at 0x40d763..0x40d810
// vtable: launcher.exe:0x4d3584 slot +0x100
static uint32_t __thiscall Mediator_GetWorldSelectionGateByteByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldSelectionGateByteByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b360
// vtable: launcher.exe:0x4d3584 slot +0x104
static uint32_t __thiscall Mediator_GetWorldTypeByteByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldTypeByteByIndex(worldIndex);
}

// anchor: launcher.exe:0x41b3a0
// vtable: launcher.exe:0x4d3584 slot +0x108
static uint32_t __thiscall Mediator_GetWorldPopulationNibbleByIndex(MinimalLoginMediatorStub* self, uint32_t worldIndex) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetWorldPopulationNibbleByIndex(worldIndex);
}

// ILTLoginMediator_0x4af2b8.Default wrapper minimization:
// - keep `g_LoginMediatorVtable` in the ABI shell
// - move wrapper-owned late-runtime state/scratch/logging into `CLTLoginMediator` when the owner
//   can keep it
// - keep wrapper-facing ABI object shapes explicit when they are not the same thing as the
//   higher-level owner-side helper semantics

// anchor: launcher.exe:0x41f2c0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x10c
// Current best late-runtime read from the event-0x18 observer callback:
// - returns owner `+0x30`
// - client immediately consumes the first two dwords there as a small-string begin/current pair
// - wrapper also logs the exact client return address so the successful post-0x18 route can prove
//   whether the current run actually executed the event-0x18 body or skipped it on observer byte
//   `this+0xcc`
static Msvc2003StdStringView* __thiscall Mediator_GetRouteDescriptor10c(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    static thread_local Msvc2003StdStringView descriptorView;
    descriptorView = BuildMsvc2003StdStringView(mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetRouteDescriptor30());
    const std::string descriptorText = DescribeRouteDescriptorText(&descriptorView);
    spdlog::info(
        "MediatorStub::GetRouteDescriptor10c caller={} [{}] result={} begin={} current={} text='{}'",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(&descriptorView),
        fmt::ptr(descriptorView.begin),
        fmt::ptr(descriptorView.current),
        descriptorText);
    return &descriptorView;
}

// anchor: launcher.exe:0x41af50
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x118
// Current best late-runtime read:
// - returns owner `+0x1470`
// - client reads it as a vector-like begin/current/capacity triple of 12-byte string-triple entries
// - immediate event-0x18 helper `0x621c6d90` and later consumer `0x62017150` both use this slot
// - wrapper logs the exact client return address so successful runs can show whether only the
//   immediate event-0x18 helper fired or the later metric-matcher path also ran
struct Msvc2003StdStringVectorView {
    Msvc2003StdStringView* begin = nullptr;
    Msvc2003StdStringView* current = nullptr;
    Msvc2003StdStringView* capacity = nullptr;
};

static Msvc2003StdStringVectorView* __thiscall Mediator_GetLateEntryList118(MinimalLoginMediatorStub* self) {
    (void)self;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    static thread_local std::vector<Msvc2003StdStringView> entryViews;
    static thread_local Msvc2003StdStringVectorView vectorView;

    const std::vector<std::string>& list = mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLateEntryList1470();
    entryViews.clear();
    entryViews.reserve(list.size());
    for (const std::string& entry : list) {
        entryViews.push_back(BuildMsvc2003StdStringView(entry));
    }

    vectorView.begin = entryViews.empty() ? nullptr : entryViews.data();
    vectorView.current = entryViews.empty() ? nullptr : entryViews.data() + entryViews.size();
    vectorView.capacity = entryViews.empty() ? nullptr : entryViews.data() + entryViews.capacity();

    const char* firstEntry = (!list.empty() && !list.front().empty()) ? list.front().c_str() : "<empty>";
    spdlog::info(
        "MediatorStub::GetLateEntryList118 caller={} [{}] result={} begin={} current={} capacity={} entryCount={} firstEntry='{}'",
        fmt::ptr(returnAddress),
        DescribeLateMediatorAbiCaller(returnAddress),
        fmt::ptr(&vectorView),
        fmt::ptr(vectorView.begin),
        fmt::ptr(vectorView.current),
        fmt::ptr(vectorView.capacity),
        list.size(),
        firstEntry);
    return &vectorView;
}

// UNANCHORED: C helper behind the recovered +0xec ABI wrapper.
// thin wrapper forwarding to CLTLoginMediator::PersistSelectionContextForState8
extern "C" void Mediator_PersistSelectionContextForState8_Impl(
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
    // sequence
    // - `InitClientDLL_BeginLoadingCharacterFlow` then sets visible text `"Loading Character"`
    // at `0x62170f2a`
    // - it then immediately calls arg6 `+0xec` at `0x62170f48`
    // Diagnostic stance:
    // - the earlier launcher-owned progress-text mirrors were removed because they were not a
    // trustworthy source of exact client-visible text
    // - exact loading/status logging now comes only from the opt-in `client.dll:0x6215b930`
    // hook when that diagnostic env flag is enabled
  } else {
    mediator->ResetSelectionContext0ecMirror();
  }

  // Keep the wrapper-facing arg6 `+0x1c` semantic split explicit.
  // Store the input in the stub for client-side code that may read it without going through
  // the owner vtable family. This is an ABI-wrapper concern, not a CLTLoginMediator field.
  // The actual PersistSelectionContextForState8 call is made above (lines 830-846).
  if (self) {
    // Copy the input to stub-owned storage for field1C exposure
    static thread_local mxo::ltlogin::State3SelectionContextInputSketch tl_input{};
    if (selectionContext) {
      std::memcpy(&tl_input, selectionContext, sizeof(tl_input));
    } else {
      tl_input = {};
    }
    self->field1C = &tl_input;
  }

  (void)returnAddress;
}

// anchor: client.dll:0x62170f48 consumes the assembled 0xb4 selection/config handoff through arg6 +0xec
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xec
__attribute__((naked)) static void Mediator_PersistSelectionContextForState8() {
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
      : "i"(Mediator_PersistSelectionContextForState8_Impl)
      : "eax", "edx");
}

// UNANCHORED: C helper behind the recovered +0x120 ABI wrapper.
// Fidelity correction:
// - launcher.exe already has the real body at owner `+0x120 / 0x41c3c0`
// - do not invent a second capture/mirror helper in source
// - wrapper dispatch should just burrow to the live owner/controller (`g_CurrentLoginMediator`)
extern "C" uint32_t Mediator_ProcessCreateCharacterInput120_Impl(
    MinimalLoginMediatorStub* self,
    void* input120,
    void* returnAddress) {
    (void)self;

    if (input120 == nullptr) {
        spdlog::info(
            "MediatorStub::ProcessCreateCharacterInput120 caller={} [{}] input=<null>",
            fmt::ptr(returnAddress),
            DescribeMediatorCaller(returnAddress));
        return 1u;
    }

    mxo::ltlogin::CLTLoginMediator* const mediator = mxo::ltlogin::g_CurrentLoginMediator;
    if (mediator == nullptr) {
        spdlog::info(
            "MediatorStub::ProcessCreateCharacterInput120 caller={} [{}] activeMediator=<null>",
            fmt::ptr(returnAddress),
            DescribeMediatorCaller(returnAddress));
        return 1u;
    }

    const auto& input = *static_cast<const mxo::ltlogin::ProcessCreateCharacterInput120Sketch*>(input120);
    const uint32_t result = mediator->ProcessCreateCharacterInput120(input);
    spdlog::info(
        "MediatorStub::ProcessCreateCharacterInput120 caller={} [{}] activeMediator={} field12c=0x{:08x} result=0x{:08x}",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(mediator),
        static_cast<unsigned>(input.field24),
        static_cast<unsigned>(result));
    return result;
}

// anchor: later loading-character path around client.dll:0x620547c0..0x62054eac passes the
// post-auth create-character source block to arg6 +0x120
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x120
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x124
// Important later state9-submit tightening from newer client.dll + launcher evidence:
// - the captured `netShell` object is not a self-contained callback84-side answer source
// - `ClientNetShell` vtable `+0x38` / `0x62006580` later re-enters the client-side resolved
//   `ILTLoginMediator_0x4af2b8.Default` global at `0x629df7f0`, calls its `+0x18c` writer, and only then
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x13c
// WaitForEvent uses this repeatedly while blocked on registered observer notifications.
static void __thiscall Mediator_InvokeSessionCallbackHelper13c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HelperSlot13c_InvokeSessionHelperVtable4();
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

    const uint32_t result = mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->RegisterLoginObserver(observer);
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x170
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xf4
static void* __thiscall Mediator_GetState8PersistenceF1c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? const_cast<void*>(mediator->GetState8PersistenceF1c()) : nullptr;
}

// anchor: launcher.exe:0x41f320 / owner vtable +0x148
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x148
// Historical wrapper note:
// - this wrapper slot used to carry the stale local name `Mediator_AttachRuntimeObject148`
// - current source now aligns it with the concrete launcher-owned `GameSessionID` getter
static const char* __thiscall Mediator_GetGameSessionId(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetGameSessionId();
}

// anchor: launcher.exe:0x41f210 / owner vtable +0x12c
// Current best type recovery from the client startup triple and later reads:
// this slot returns owner +0x8c, which currently lines up with the client-side
// ILTDistrObjExecutive-like third startup object.
static void* __thiscall Mediator_GetStartupDistrObjExecutive8c(MinimalLoginMediatorStub* self) {
    (void)self;
    mxo::ltlogin::CLTLoginMediator* mediator = DiagnosticEnsureMediatorModel();
    return mediator ? mediator->GetStartupDistrObjExecutive8c() : nullptr;
}

// anchor: launcher.exe:0x41f330 / wrapper-facing arg6 slot +0x14c
static void __thiscall Mediator_SetSharedMarginPacketField660(MinimalLoginMediatorStub* self, uint32_t value) {
    (void)self;
    mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->SetSharedMarginPacketField660(value);
}

// anchor: launcher.exe:0x41c510 / wrapper-facing arg6 slot +0x158
static uint32_t __thiscall Mediator_SetState9OptionalField90AndSwitchToState13(MinimalLoginMediatorStub* self, uint32_t value) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->SetState9OptionalField90AndSwitchToState13(value);
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

    const uint32_t result = mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->UnregisterLoginObserver(observer);
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x174
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x178
static uint32_t __thiscall Mediator_GetLastLoginStatus(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->GetLastLoginStatus();
}

// anchor: launcher.exe:0x41af80 / wrapper-facing arg6 slot +0x17c
static uint32_t __thiscall Mediator_HandleAuthConnectionCompletionFallback(
    MinimalLoginMediatorStub* self,
    void* connection,
    void* workItem) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HandleAuthConnectionCompletionFallback(connection, workItem);
}

// anchor: launcher.exe:0x41f250 / wrapper-facing arg6 slot +0x180
static uint32_t __thiscall Mediator_DispatchCurrentHelperAuthMessage(MinimalLoginMediatorStub* self, void* workItem) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->DispatchCurrentHelperAuthMessage(workItem);
}

// anchor: launcher.exe:0x41f260 / wrapper-facing arg6 slot +0x184
static uint32_t __thiscall Mediator_DispatchCurrentHelperSlot6(
    MinimalLoginMediatorStub* self,
    mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->DispatchCurrentHelperSlot6(workItem);
}

// anchor: launcher.exe:0x41afc0 / wrapper-facing arg6 slot +0x188
static uint32_t __thiscall Mediator_HandleMarginConnectionCompletionFallback(
    MinimalLoginMediatorStub* self,
    void* connection,
    void* workItem) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->HandleMarginConnectionCompletionFallback(connection, workItem);
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x164
// Wrapper-minimization note:
// - keep the wrapper-facing teardown meaning explicit here (`WaitForEvent(1)` predicate)
// - owner-side state/logging lives on `CLTLoginMediator`
static uint32_t __thiscall Mediator_RequestAuthConnectionCloseWaitEvent1(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->RequestAuthConnectionCloseWaitEvent1() ? 1u : 0u;
}

// anchor: launcher.exe teardown path 0x40b360..0x40b409 conditionally checks arg6 +0x16c
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x16c
// Keep the wrapper-facing split explicit:
// - teardown uses this as the `WaitForEvent(0x0f)` predicate
// - owner-side state9 success still has its own method name on `CLTLoginMediator`
static uint32_t __thiscall Mediator_RequestMarginConnectionCloseWaitEvent0f(MinimalLoginMediatorStub* self) {
    (void)self;
    return mxo::ltlogin::ILTLoginMediator_0x4af2b8::Default->RequestMarginConnectionCloseWaitEvent0f() ? 1u : 0u;
}

// UNANCHORED: client.dll teardown/late-runtime may probe wrapper slots we have not RE'd yet.
// Keep them non-null and noisy so `TermClientDLL` can progress far enough to show which slots
// still matter, without fabricating owner-side behavior.
static uint32_t Mediator_LogUnimplementedSlotReturnZero(const char* slotName, void* caller) {
    spdlog::info(
        "MediatorStub::{} unimplemented caller={} [{}] -> returnValue=0",
        slotName,
        caller,
        DescribeLateMediatorAbiCaller(caller));
    return 0;
}

#define DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(slotSuffix)                                 \
    static uint32_t __thiscall Mediator_UnimplementedSlot##slotSuffix(                 \
        MinimalLoginMediatorStub* self) {                                              \
        (void)self;                                                                    \
        return Mediator_LogUnimplementedSlotReturnZero(                                \
            "slot +0x" #slotSuffix, __builtin_return_address(0));                    \
    }

DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(04)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(14)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(18)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(20)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(28)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(34)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(E8)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(F0)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(110)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(114)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(11c)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(128)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(130)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(134)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(138)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(140)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(144)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(14c)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(150)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(154)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(158)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(15c)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(160)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(168)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(17c)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(180)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(184)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(188)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(190)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(194)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(198)
DEFINE_MEDIATOR_UNIMPLEMENTED_STUB(19c)

#undef DEFINE_MEDIATOR_UNIMPLEMENTED_STUB

// UNANCHORED: seeds the replacement ILTLoginMediator_0x4af2b8.Default ABI vtable from recovered slot usage.
void DiagnosticInitializeMediatorStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    DiagnosticLogPacketAbiValidationOnce();

    std::memset(g_LoginMediatorVtable, 0, sizeof(g_LoginMediatorVtable));
    // Forward to ILTLoginMediator_0x4af2b8::Default vtable slot 0 (original Mediator_GetName)
    g_LoginMediatorVtable[0] = (void*)Mediator_GetName;          // +0x00
    g_LoginMediatorVtable[1] = (void*)Mediator_UnimplementedSlot04; // +0x04
    g_LoginMediatorVtable[2] = (void*)Mediator_RegisterLauncherNetworkEngineObject08; // +0x08
    g_LoginMediatorVtable[3] = (void*)Mediator_ClearEngine;      // +0x0c
    g_LoginMediatorVtable[4] = (void*)Mediator_IsReady;          // +0x10
    g_LoginMediatorVtable[5] = (void*)Mediator_UnimplementedSlot14; // +0x14
    g_LoginMediatorVtable[6] = (void*)Mediator_GetUnknownByte05; // +0x18
    g_LoginMediatorVtable[7] = (void*)Mediator_SetValue1;        // +0x1c
    g_LoginMediatorVtable[8] = (void*)Mediator_UnimplementedSlot20; // +0x20
    g_LoginMediatorVtable[9] = (void*)Mediator_SetValue2;        // +0x24
    g_LoginMediatorVtable[10] = (void*)Mediator_UnimplementedSlot28; // +0x28
    g_LoginMediatorVtable[11] = (void*)Mediator_IsConnected;     // +0x2c
    g_LoginMediatorVtable[12] = (void*)Mediator_ProcessLoginRequest30; // +0x30
    g_LoginMediatorVtable[13] = (void*)Mediator_RequestAuthCloseAndSwitchToState0; // +0x34
    g_LoginMediatorVtable[14] = (void*)Mediator_GetUsername38;  // +0x38
    g_LoginMediatorVtable[15] = (void*)Mediator_GetDefaultSelectionIndex; // +0x3c
    g_LoginMediatorVtable[16] = (void*)Mediator_GetSelectionDescriptor40; // +0x40
    g_LoginMediatorVtable[17] = (void*)Mediator_GetCurrentAuthReplyPacket44; // +0x44
    g_LoginMediatorVtable[18] = (void*)Mediator_GetWorldOrSelectionName; // +0x48
    g_LoginMediatorVtable[19] = (void*)Mediator_GetProfileOrSessionName; // +0x4c
    g_LoginMediatorVtable[20] = (void*)Mediator_GetBootstrapRaw08AuxHandle50; // +0x50
    g_LoginMediatorVtable[21] = (void*)Mediator_HasBootstrapRaw08AuxHandle54; // +0x54
    g_LoginMediatorVtable[22] = (void*)Mediator_GetCrashReporterPromptForSecurId58; // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetCrashReporterUsername5c; // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetCrashReporterPassword60; // +0x60
    g_LoginMediatorVtable[25] = (void*)Mediator_GetBootstrapSuccessHeaderDword64; // +0x64
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
    g_LoginMediatorVtable[57] = (void*)Mediator_GetSlotRecordStatusBySelectionIndex; // +0xe4
    g_LoginMediatorVtable[58] = (void*)Mediator_UnimplementedSlotE8; // +0xe8
    g_LoginMediatorVtable[59] = (void*)Mediator_PersistSelectionContextForState8; // +0xec
    g_LoginMediatorVtable[60] = (void*)Mediator_UnimplementedSlotF0; // +0xf0
    g_LoginMediatorVtable[61] = (void*)Mediator_GetState8PersistenceF1c; // +0xf4
    g_LoginMediatorVtable[62] = (void*)Mediator_GetWorldCount; // +0xf8
    g_LoginMediatorVtable[63] = (void*)Mediator_GetWorldNameByIndex; // +0xfc
    g_LoginMediatorVtable[64] = (void*)Mediator_GetWorldSelectionGateByteByIndex; // +0x100
    g_LoginMediatorVtable[65] = (void*)Mediator_GetWorldTypeByteByIndex; // +0x104
    g_LoginMediatorVtable[66] = (void*)Mediator_GetWorldPopulationNibbleByIndex; // +0x108
    g_LoginMediatorVtable[67] = (void*)Mediator_GetRouteDescriptor10c; // +0x10c
    g_LoginMediatorVtable[68] = (void*)Mediator_UnimplementedSlot110; // +0x110
    g_LoginMediatorVtable[69] = (void*)Mediator_UnimplementedSlot114; // +0x114
    g_LoginMediatorVtable[70] = (void*)Mediator_GetLateEntryList118; // +0x118
    g_LoginMediatorVtable[71] = (void*)Mediator_UnimplementedSlot11c; // +0x11c
    g_LoginMediatorVtable[72] = (void*)Mediator_ProcessCreateCharacterInput120; // +0x120
    g_LoginMediatorVtable[73] = (void*)Mediator_ProvideStartupTriple; // +0x124
    g_LoginMediatorVtable[74] = (void*)Mediator_UnimplementedSlot128; // +0x128
    g_LoginMediatorVtable[75] = (void*)Mediator_GetStartupDistrObjExecutive8c; // +0x12c
    g_LoginMediatorVtable[76] = (void*)Mediator_GetLaunchPadClient65c; // +0x130
    g_LoginMediatorVtable[77] = (void*)Mediator_EnsureLaunchPadClient65c; // +0x134
    g_LoginMediatorVtable[78] = (void*)Mediator_UnimplementedSlot138; // +0x138
    g_LoginMediatorVtable[79] = (void*)Mediator_InvokeSessionCallbackHelper13c; // +0x13c
    g_LoginMediatorVtable[80] = (void*)Mediator_UnimplementedSlot140; // +0x140
    g_LoginMediatorVtable[81] = (void*)Mediator_UnimplementedSlot144; // +0x144
    g_LoginMediatorVtable[82] = (void*)Mediator_GetGameSessionId; // +0x148
    g_LoginMediatorVtable[83] = (void*)Mediator_SetSharedMarginPacketField660; // +0x14c
    g_LoginMediatorVtable[84] = (void*)Mediator_UnimplementedSlot150; // +0x150
    g_LoginMediatorVtable[85] = (void*)Mediator_UnimplementedSlot154; // +0x154
    g_LoginMediatorVtable[86] = (void*)Mediator_SetState9OptionalField90AndSwitchToState13; // +0x158
    g_LoginMediatorVtable[87] = (void*)Mediator_UnimplementedSlot15c; // +0x15c
    g_LoginMediatorVtable[88] = (void*)Mediator_UnimplementedSlot160; // +0x160
    g_LoginMediatorVtable[89] = (void*)Mediator_RequestAuthConnectionCloseWaitEvent1;   // +0x164
    g_LoginMediatorVtable[90] = (void*)Mediator_UnimplementedSlot168; // +0x168
    g_LoginMediatorVtable[91] = (void*)Mediator_RequestMarginConnectionCloseWaitEvent0f;   // +0x16c
    g_LoginMediatorVtable[92] = (void*)Mediator_RegisterLoginObserver170; // +0x170
    g_LoginMediatorVtable[93] = (void*)Mediator_UnregisterLoginObserver174; // +0x174
    g_LoginMediatorVtable[94] = (void*)Mediator_GetLastLoginStatus; // +0x178
    g_LoginMediatorVtable[95] = (void*)Mediator_HandleAuthConnectionCompletionFallback; // +0x17c
    g_LoginMediatorVtable[96] = (void*)Mediator_DispatchCurrentHelperAuthMessage; // +0x180
    g_LoginMediatorVtable[97] = (void*)Mediator_DispatchCurrentHelperSlot6; // +0x184
    g_LoginMediatorVtable[98] = (void*)Mediator_HandleMarginConnectionCompletionFallback; // +0x188
    g_LoginMediatorVtable[99] = (void*)Mediator_FillState9CallbackBlob18c; // +0x18c
    g_LoginMediatorVtable[100] = (void*)Mediator_UnimplementedSlot190; // +0x190
    g_LoginMediatorVtable[101] = (void*)Mediator_UnimplementedSlot194; // +0x194
    g_LoginMediatorVtable[102] = (void*)Mediator_UnimplementedSlot198; // +0x198
    g_LoginMediatorVtable[103] = (void*)Mediator_UnimplementedSlot19c; // +0x19c

    std::memset(&g_LoginMediatorStub, 0, sizeof(g_LoginMediatorStub));
    g_LoginMediatorStub.vtable = g_LoginMediatorVtable;

    // Startup fidelity correction:
    // - early launcher startup immediately uses the resolved arg6 surface (`+0x1c/+0x24` here)
    // - our current source spells that surface through `g_CurrentLoginMediator`
    // - so do not clear the active mediator during stub init; seed one concrete owner if absent
    if (mxo::ltlogin::g_CurrentLoginMediator == nullptr) {
        mxo::ltlogin::g_CurrentLoginMediator = new mxo::ltlogin::CLTLoginMediator();
    }
}

