/**
 * CLTLoginMediator - launcher-owned login/controller implementation.
 *
 * Maintenance note:
 * - keep this file focused on shared mediator logic, auth/bootstrap packet handling, and margin transport
 * - keep the owner `+0x680` phase-2 auth/bootstrap child in:
 *   - `authbootstrap680.cpp`
 * - keep early auth/state-entry scaffolding in:
 *   - `loginmediator_auth_entry.cpp`
 * - keep active-state-source / character-view helpers in:
 *   - `loginmediator_active_state.cpp`
 * - keep arg6/startup-selection scaffolding in:
 *   - `loginmediator_arg6.cpp`
 * - keep active late-login/state9 submit work in:
 *   - `loginmediator_state9.cpp`
 *   - `loginmediator_events.cpp`
 *   - `loginstate_state9.cpp`
 * - prefer anchored comments at individual methods over repeating large project summaries here
 *
 * Canonical references:
 * - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
 * - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
 * - `../../../../docs/launcher.exe/VTABLES/0x004b01c8.md`
 * - `../../../../docs/launcher.exe/VTABLES/0x004b517c.md`
 * - `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
 * - `../../../../docs/launcher.exe/auth/STATUS.md`
 */

#include "loginmediator.h"
#include "loginmediator_events.h"
#include "launchpad.h"

#include "loginstate.h"
#include "../../../../src/launcher_mediator_abi.h"
#include "authbootstrap680.h"
#include "../../../runtime/src/liblttcp/ltipaddresslist.h"
#include "../../../runtime/src/libltnet/sys/pc/pcsocket.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
// ILTLoginMediator::~ILTLoginMediator() = default;

namespace {

enum class MarginBootstrapPhase : uint32_t {
    kIdle = 0,
    kSentCertConnectRequest = 1,
    kSentCertChallengeResponse = 2,
    kSentMsConnectRequest = 3,
    kSentMsConnectChallengeResponse = 4,
    kReady = 5,
};

struct MarginBootstrapSessionState {
    MarginBootstrapPhase phase = MarginBootstrapPhase::kIdle;
    std::vector<uint8_t> authReplyPrivateExponentBytes;
    std::vector<uint8_t> marginTwofishKeyBytes;
    std::vector<uint8_t> certChallengeBytes;
    uint32_t marginSessionId = 0;
    uint32_t state6UdpSessionSecretF18 = 0;
};

// ABI-safety storage note:
// - margin CERT/MS bootstrap/session state is launcher-owned runtime state
// - but `CLTLoginMediator` layout is sensitive enough that adding members in the middle is risky
// - keep that state in a sidecar keyed by mediator pointer until/if a proven append-only layout
//   change is justified
static std::unordered_map<const CLTLoginMediator*, MarginBootstrapSessionState>
    g_marginBootstrapStateByMediator;

// UNANCHORED: no original launcher.exe anchor assigned yet.
static MarginBootstrapSessionState& MutableMarginBootstrapState(const CLTLoginMediator* mediator) {
    return g_marginBootstrapStateByMediator[mediator];
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void EraseMarginBootstrapState(const CLTLoginMediator* mediator) {
    g_marginBootstrapStateByMediator.erase(mediator);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static mxo::liblttcp::LTTCPEndpointKey BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey key = {};
    key.family = 2;  // AF_INET
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = 0;
    return key;
}


// UNANCHORED: no original launcher.exe anchor assigned yet.
static const char* NonEmptyTextOrPlaceholder(const char* value) {
    return (value && value[0]) ? value : "<empty>";
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static std::string BuildHexPreview(const void* bytes, size_t byteCount, size_t maxPreviewBytes) {
    if (!bytes || byteCount == 0u || maxPreviewBytes == 0u) {
        return "<empty>";
    }

    const uint8_t* p = static_cast<const uint8_t*>(bytes);
    const size_t previewCount = std::min(byteCount, maxPreviewBytes);
    std::string out;
    out.reserve(previewCount * 3u);
    static const char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < previewCount; ++i) {
        const uint8_t value = p[i];
        out.push_back(kHexDigits[(value >> 4) & 0x0fu]);
        out.push_back(kHexDigits[value & 0x0fu]);
        if (i + 1u != previewCount) {
            out.push_back(' ');
        }
    }
    return out;
}

// Wrapper-facing arg6 `+0x40` outer object currently only needs the common 5-slot virtual
// surface shape shared by the launcher descriptor/slot-record families.
// Current fidelity tightening from Ghidra:
// - slot `+0x04` is the shared tiny getter at `0x437b50` and returns `0`
// - slot `+0x10` is the shared tiny getter at `0x481760` and returns the `+0x10` payload pointer
// - keep `+0x08/+0x0c` conservative until the concrete wrapper object class is recovered
static uint32_t __thiscall Arg6SelectionDescriptor40_Destroy(Arg6CurrentSlotRecord44ObjectSketch* self) {
    return self ? 1u : 0u;
}

static uint32_t __thiscall Arg6SelectionDescriptor40_GetStateId(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

static uint32_t __thiscall Arg6SelectionDescriptor40_AppendDebugString(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall Arg6SelectionDescriptor40_ResetPayloadForSourceDescriptor(Arg6CurrentSlotRecord44ObjectSketch* self) {
    if (!self) {
        return 0u;
    }
    self->backingObject08 = nullptr;
    self->flag0c = (self->payload10 != nullptr) ? 1u : 0u;
    return 1u;
}

static uint32_t __thiscall Arg6SelectionDescriptor40_GetPayload10(Arg6CurrentSlotRecord44ObjectSketch* self) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(self ? self->payload10 : nullptr));
}

static void** Arg6CurrentSlotRecord44Vtable() {
    static void* vtable[5] = {
        reinterpret_cast<void*>(Arg6SelectionDescriptor40_Destroy),
        reinterpret_cast<void*>(Arg6SelectionDescriptor40_GetStateId),
        reinterpret_cast<void*>(Arg6SelectionDescriptor40_AppendDebugString),
        reinterpret_cast<void*>(Arg6SelectionDescriptor40_ResetPayloadForSourceDescriptor),
        reinterpret_cast<void*>(Arg6SelectionDescriptor40_GetPayload10),
    };
    return vtable;
}

struct LiveSelectionCfgCorpusView {
    uint32_t ready = 0u;
    void* buffer = nullptr;
    uint32_t length = 0u;
};

static uint32_t g_LastLoggedDefaultSelectionIndex3c = 0xffffffffu;



}  // namespace

// anchor: launcher.exe:0x41dba0 / embedded CLTLoginMediatorSelectionRouteState_0x41dba0 ctor
CLTLoginMediator::CLTLoginMediatorSelectionRouteState::CLTLoginMediatorSelectionRouteState() {
    slotRecordCount00_ = 0;
    for (size_t i = 0; i < routeHostStringTriples194_.size(); ++i) {
        routeHostStringTriples194_[i].Clear();
    }
    for (size_t i = 0; i < slotRecordTable04_.size(); ++i) {
        slotRecordTable04_[i] = {};
        slotRecordValid04_[i] = false;
    }
    currentSlotOrSelectionIndex644_ = 0xffu;
    persistedSelectionContext64c_ = {};
}

// anchor: launcher.exe:0x41d270 / embedded CLTLoginMediatorSelectionRouteState_0x41dba0::ResetSelectionRouteState
void CLTLoginMediator::CLTLoginMediatorSelectionRouteState::ResetSelectionRouteState() {
    const size_t activeCount = std::min(static_cast<size_t>(slotRecordCount00_), slotRecordTable04_.size());
    for (size_t i = 0; i < activeCount; ++i) {
        slotRecordTable04_[i] = {};
        slotRecordValid04_[i] = false;
        routeHostStringTriples194_[i].Clear();
    }
    slotRecordCount00_ = 0;
    currentSlotOrSelectionIndex644_ = 0xffu;
}

// anchor: launcher.exe:0x41dd00 / embedded CLTLoginMediatorSelectionRouteState_0x41dba0::DestroySelectionRouteState
void CLTLoginMediator::CLTLoginMediatorSelectionRouteState::DestroySelectionRouteState() {
    ResetSelectionRouteState();
    for (size_t i = 0; i < routeHostStringTriples194_.size(); ++i) {
        routeHostStringTriples194_[i].ReleaseStorage();
        slotRecordTable04_[i] = {};
        slotRecordValid04_[i] = false;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      authConnectionOwnedByMediator_(false),
      marginConnectionOwnedByMediator_(false),
      authPeerCloseQueuedScaffold_(false),
      marginPeerCloseQueuedScaffold_(false),
      authConnection_(nullptr),
      marginConnection_(nullptr),
      currentState_(nullptr),
      marginRouteState_{},
      marginAddressList3c_{},
      authBootstrapChild680_(nullptr),
      launchPadClient65c_(nullptr),
      selectionRouteState684_{},
      selectionContext0ecCopy_{},
      selectionContext0ecCopyValid_(false),
      selection0ecCount_(0),
      profile0f4Count_(0),
      postAuthMarginLoadingState_0xf14{},
      authServerPortHostOrder_(11000),
      ignoreHostsFileForAuth_(false),
      marginServerPortHostOrder_(10000),
      ignoreHostsFileForMargin_(false),
      authEndpoint_(BuildLoopbackEndpoint(authServerPortHostOrder_)),
      marginEndpoint_(BuildLoopbackEndpoint(marginServerPortHostOrder_)),
      authUsername_(),
      authPassword_(),
      authLauncherVersion_(76005),
      authCurrentPublicKeyId_(0),
      authLoginType_(1),
      authKeyConfigMd5_(),
      authUiConfigMd5_(),
      authGetPublicKeyRequestSent_(false),
      authRequestSent_(false),
      authChallengeResponseSent_(false),
      lastAuthPublicKeyReply_(),
      lastAuthRequestBuildResult_(),
      lastAuthChallenge_(),
      lastAuthReply_(),
      lastAuthConnectStatus_(0),
      lastMarginConnectStatus_(0),
      authConnectStatusCount_(0),
      marginConnectStatusCount_(0),
      expectedAuthRequestName_(nullptr),
      expectedMarginRequestName_(nullptr),
      worldSlots_{},
      worldPayloadSlots_{} {
    InitializeObserverTree674();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::~CLTLoginMediator() {
    selectionRouteState684_.DestroySelectionRouteState();
    FreeLateEntryList1470StorageScaffold();
    ResetLauncherConnectionsScaffold();
    ClearObserverTree674();
    EraseMarginBootstrapState(this);
}

void CLTLoginMediator::ResetLauncherConnectionsScaffold() {
    if (authConnection_) {
        authConnection_->SetOwnerContext(nullptr);
    }
    if (marginConnection_) {
        marginConnection_->SetOwnerContext(nullptr);
    }

    SetCurrentState(0u);
    SetNetworkEngine(nullptr);
    // inline UnregisterActiveStateSourceScaffold
    g_CurrentLoginMediator = nullptr;

    if (authConnectionOwnedByMediator_) {
        delete authConnection_;
    }
    if (marginConnectionOwnedByMediator_) {
        delete marginConnection_;
    }

    authConnection_ = nullptr;
    marginConnection_ = nullptr;
    authConnectionOwnedByMediator_ = false;
    marginConnectionOwnedByMediator_ = false;
    authPeerCloseQueuedScaffold_ = false;
    marginPeerCloseQueuedScaffold_ = false;

    spdlog::info("CLTLoginMediator::ResetLauncherConnectionsScaffold completed");
}

// anchor: launcher.exe:0x41b160 / owner vtable +0x08
// Faithful implementation from Ghidra decompile:
// - if arg2 is null, fall back to global `g_pThreadPerClientTCPEngine`
// - store the engine pointer at owner `+0x14` (direct store, not via SetNetworkEngine)
// - allocate/construct owner `+0x680` auth bootstrap child with malloc + in-place ctor
// - call virtual constructor method on auth bootstrap child
// - set `g_CurrentLoginMediator = this`
// - call `CLTLoginMediator_InitializeHelperDispatchTable()`
// - install state0 from `g_LoginHelperState0[0]` into owner `+0x10`
// - mark owner byte `+0x04 = 1`
// - rebuild owner `+0x4c` auth `CLTIPAddressList` from `g_qsAuthServerDNSName` with flags:
//   - `0x01` = shuffle
//   - `0x03` = shuffle | ignore-hosts-file when `g_IgnoreHostsFileForAuth != 0`
// - return status dword based on address list count
uint32_t CLTLoginMediator::Initialize(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* networkEngineOverride) {
    // anchor: launcher.exe:0x41b16f..0x41b174
    if (networkEngineOverride == nullptr) {
        networkEngineOverride = reinterpret_cast<mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768*>(g_pThreadPerClientTCPEngine);
    }

    // anchor: launcher.exe:0x41b17a - direct store to owner+0x14
    engine_ = networkEngineOverride;

    // anchor: launcher.exe:0x41b180..0x41b1a9 - allocate/construct auth bootstrap child
    // Original uses malloc + in-place ctor with allocation tracking; replacement uses make_unique
    if (!authBootstrapChild680_) {
        authBootstrapChild680_ = std::make_unique<AuthBootstrap680Child_0x441290>();
    }

    // anchor: launcher.exe:0x41b1b4 - set global current mediator
    g_CurrentLoginMediator = this;

    // anchor: launcher.exe:0x41b1ba - initialize helper dispatch table
    InitializeHelperDispatchTable();

    // anchor: launcher.exe:0x41b1c0..0x41b1c7 - install state0 into owner+0x10
    // Helper dispatch table was just initialized, so slot 0 now points to g_State0
    currentState_ = LoginHelperStateByIdScaffold(0u);

    // anchor: launcher.exe:0x41b1cd - set owner ready flag byte at +0x04
    ownerReadyFlag04_ = 1;

    // anchor: launcher.exe:0x41b1d3..0x41b1e2 - build flags and reinit address list
    uint32_t authAddressListReinitFlags = 1;  // kFlagShuffle
    if (g_IgnoreHostsFileForAuth != 0) {
        authAddressListReinitFlags = 3;  // kFlagShuffle | kFlagIgnoreHostsFile
    }

    // anchor: launcher.exe:0x41b1e8..0x41b1ef - direct CLTIPAddressList_Reinit call
    // Pass the raw pointer range for authAddressList4c_
    authAddressList4c_.Reinit(g_qsAuthServerDNSName, authAddressListReinitFlags);

    // anchor: launcher.exe:0x41b1f5..0x41b207 - return status based on address list count
    // Return value: 0x12000001 if list is empty, 0 if list has entries
    const size_t addressCount = authAddressList4c_.Count();
    const uint32_t returnStatus = addressCount != 0 ? 0u : 0x12000001u;

    spdlog::info(
        "CLTLoginMediator::Initialize engine={} currentState={} authAddressListReinitFlags=0x{:02x} authCandidates={} returnStatus=0x{:08x}",
        fmt::ptr(networkEngineOverride),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(authAddressListReinitFlags),
        static_cast<unsigned>(addressCount),
        static_cast<unsigned>(returnStatus));

    return returnStatus;
}

// +0x00
// anchor: launcher.exe:0x0041f020
const char* CLTLoginMediator::GetName() {
    return "CLTLoginMediator";
}

// +0x08
// Wrapper-facing arg6 startup handoff helper used by the current `0x4d2c58` scaffold.
// Do not sync this directly to owner vtable `0x004b01c8 +0x0c`; current Ghidra now assigns that
// owner slot to `launcher.exe:0x41f510`, which looks like reset/clear logic instead.
// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* engine) {
    engine_ = engine;
    if (authConnection_) {
        authConnection_->SetEngine(engine_);
    }
    if (marginConnection_) {
        marginConnection_->SetEngine(engine_);
        if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_)) {
            marginConnection->SetEngine(engine_);
        }
    }
}

// anchor: launcher.exe:0x41f510 vtable offset +0x0c
// Wrapper-facing arg6 clear helper reached from launcher teardown after arg5 release.
// Current fidelity direction:
// - owner vtable `0x004b01c8 +0x0c / 0x41f510` now reads as reset/clear-owned-runtime-state logic
// - keep this wrapper slot distinct from the owner vtable numbering, but mirror the highest-
//   confidence reset effects here instead of only nulling the engine pointer
// UNANCHORED: earlier `0x41f060` anchor was stale; current static RE now assigns that VA to the
// nopatch launcher-version setter instead.
void CLTLoginMediator::ClearEngine() {
    selectionRouteState684_.ResetSelectionRouteState();
    FreeLateEntryList1470StorageScaffold();
    ResetLauncherConnectionsScaffold();
    launchPadClient65c_ = nullptr;
    authBootstrapChild680_.reset();
    spdlog::info("CLTLoginMediator::ClearEngine mirrored reset-owned-runtime-state scaffold");
}

// anchor: launcher.exe:0x41f030 vtable offset +0x10
uint32_t CLTLoginMediator::IsReady() {
    spdlog::info("CLTLoginMediator::IsReady() -> 1");
    return 1;
}

// +0x1c
// anchor: launcher.exe:0x41f060
// anchor: launcher.exe:0x409a73..0x409b5f explicit nopatch path
// vtable: ILTLoginMediator.Default slot +0x1c
void CLTLoginMediator::SetValue1(void* value) {
    nopatchLauncherVersionValue08_ = value ? *static_cast<const uint32_t*>(value) : 0u;
    spdlog::debug(
        "CLTLoginMediator::SetValue1({}) -> owner+0x08=0x{:08x}",
        value,
        static_cast<unsigned>(nopatchLauncherVersionValue08_));
}

// +0x24
// anchor: launcher.exe:0x41f080
// anchor: launcher.exe:0x409a98..0x409c2d explicit nopatch path
// vtable: ILTLoginMediator.Default slot +0x24
void CLTLoginMediator::SetValue2(void* value) {
    nopatchClientVersionValue0c_ = value ? *static_cast<const uint32_t*>(value) : 0u;
    spdlog::debug(
        "CLTLoginMediator::SetValue2({}) -> owner+0x0c=0x{:08x}",
        value,
        static_cast<unsigned>(nopatchClientVersionValue0c_));
}

// UNANCHORED: shared diagnostic log-throttling helper.
static bool DiagnosticShouldLogRepeatedRuntimeCount(uint32_t count) {
    return count <= 8u || (count && ((count & (count - 1u)) == 0u)) || ((count % 1024u) == 0u);
}

// Source-owned helper for tightening owner-backed selection/world readers toward launcher.exe
// state-gated table access patterns (`stateCode >= 3`).
static uint32_t CurrentHelperStateCodeOrZero(const mxo::ltlogin::CLTLoginMediator* mediator) {
    const mxo::ltlogin::CLTLoginState* state = mediator ? mediator->currentState_ : nullptr;
    return state ? state->DispatchPhaseCode() : 0u;
}

// anchor: client.dll:0x62006cb1..0x62006cca polls arg6 before feeding arg5 into the runtime loop
// vtable: ILTLoginMediator.Default slot +0x2c
uint32_t CLTLoginMediator::IsConnected() {
    static uint32_t s_IsConnectedCount = 0;
    ++s_IsConnectedCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(s_IsConnectedCount)) {
        spdlog::debug("MediatorStub::IsConnected() -> 1 [count={:08x}]", s_IsConnectedCount);
    }
    return 1;
}

// anchor: launcher.exe:0x41ecd0 slot +0x2c
uint32_t CLTLoginMediator::ProcessLoginRequest(const ProcessLoginRequestInputSketch& input) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 1u:
        case 2u:
        case 3u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000006",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000006u;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000000",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000000u;
        case 12u:
            spdlog::info(
                "ROUTE CHECKPOINT: ProcessLoginRequest blocked currentState={} stateCode={} -> 0x12000007",
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(stateCode));
            return 0x12000007u;
        default:
            break;
    }

    const bool string60Empty = (input.string60.current == input.string60.begin);
    if ((input.inlineString00[0] == '\0' || input.inlineString20[0] == '\0') && string60Empty) {
        spdlog::info(
            "ROUTE CHECKPOINT: ProcessLoginRequest rejected empty credentials currentState={} -> 0x00000004",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 4u;
    }

    ownerAuthBootstrapSource94_.CopyFromSubmitLoginRequestInput(input);

    // Fidelity: session token at +0xf4 within owner+0x94 block
    ownerAuthBootstrapSource94_.sessionToken60.begin = input.string60.begin;
    ownerAuthBootstrapSource94_.sessionToken60.current = input.string60.current;
    ownerAuthBootstrapSource94_.sessionToken60.capacity = input.string60.capacity;

    spdlog::info(
        "CLTLoginMediator::ProcessLoginRequest copied owner+0x94 username='{}' password='{}' string60Len={} currentState={} stateCode={} launchPadGateState16State18AltPath={} helper65cPresent={} submitOwnership=owner",
        ownerAuthBootstrapSource94_.username00[0] ? ownerAuthBootstrapSource94_.username00.data() : "<empty>",
        ownerAuthBootstrapSource94_.password20[0] ? ownerAuthBootstrapSource94_.password20.data() : "<empty>",
        static_cast<unsigned>(ownerAuthBootstrapSource94_.sessionToken60.current - ownerAuthBootstrapSource94_.sessionToken60.begin),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(stateCode),
        0u,
        launchPadClient65c_ ? 1u : 0u);

    CLTLoginState* const upstreamState = currentState_;
    if (stateCode == 0u) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth ProcessLoginRequest from state0 currentState={} string60Empty={} launchPadGateState16State18Scaffold={} helper65cPresent={}",
            upstreamState ? upstreamState->DebugName() : "<null>",
            string60Empty ? 1u : 0u,
            0u,
            launchPadClient65c_ ? 1u : 0u);
    }
    if (true) {
        // Static + runtime now line up on the default happy path at `0x41ecd0`:
        // - after copying the input block into owner `+0x94`, the code tests
        //   `g_LaunchPadGateState16State18`
        // - on `g_LaunchPadGateState16State18 == 0`, it clears owner `+0xf4`
        //   (`+0x94 + 0x60`) through `0x407dd0`
        // - then it calls `0x41b450(2)` while the current helper is still state0
        // - the next state-owned body is therefore `0x439210` on helper/state 2 with upstream
        //   state0, and state2 owns the ready/not-ready handoff into the owner `+0x680`
        //   bootstrap child
        // This is the exact favored happy path and keeps submit ownership on the mediator/owner,
        // not on state0.
        // Fidelity: clear session token in owner+0x94 block (as per static-RE 0x41ecd0)
        ownerAuthBootstrapSource94_.sessionToken60.begin = nullptr;
        ownerAuthBootstrapSource94_.sessionToken60.current = nullptr;
        ownerAuthBootstrapSource94_.sessionToken60.capacity = nullptr;
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state0 -> state2 via owner ProcessLoginRequest (favored g_LaunchPadGateState16State18==0 happy path) upstreamState={} clearedOwnerF4=1",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (LoginHelperStateByIdScaffold(2u) != nullptr) {
            const uint32_t state2EntryResult = SetCurrentState(2u);
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest default state2 entry upstreamState={} -> slot3Result=0x{:08x}",
                upstreamState ? upstreamState->DebugName() : "<null>",
                static_cast<unsigned>(state2EntryResult));
        } else {
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest has no registered state2 scaffold; leaving currentState={} after owner+0xf4 clear",
                currentState_ ? currentState_->DebugName() : "<null>");
        }
        return 0u;
    }

    // Default-off source-owned scaffolds for the alternate
    // `g_LaunchPadGateState16State18 != 0` family.
    // Static `0x41ecd0` now narrows that split more concretely than before:
    // - if string60 is non-empty and helper65c is absent, switch to state16
    // - if string60 is non-empty and helper65c is present, switch back to state2
    // - if string60 is empty, optionally refresh owner `+0xf4` from helper65c `+0x18`, then
    //   switch to state16
    // Keep that state16/state18 family explicit but default-off so the proven
    // `g_LaunchPadGateState16State18 == 0` happy path remains the exact favored route.
    // That alternate family is separate from the active state2 -> owner+0x680 bootstrap-child
    // handoff.
    if (!string60Empty) {
        if (launchPadClient65c_ == nullptr) {
            spdlog::info(
                "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state16 (string60 non-empty, helper65c absent) upstreamState={}",
                upstreamState ? upstreamState->DebugName() : "<null>");
            if (LoginHelperStateByIdScaffold(16u) != nullptr) {
                (void)SetCurrentState(16u);
            } else {
                spdlog::info(
                    "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate no-helper65c branch currentState={}",
                    currentState_ ? currentState_->DebugName() : "<null>");
            }
            return 0u;
        }

        spdlog::info(
            "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state2 (string60 non-empty, helper65c present) upstreamState={}",
            upstreamState ? upstreamState->DebugName() : "<null>");
        if (LoginHelperStateByIdScaffold(2u) != nullptr) {
            (void)SetCurrentState(2u);
        } else {
            spdlog::info(
                "CLTLoginMediator::ProcessLoginRequest missing registered state2 scaffold for alternate helper65c-present branch currentState={}",
                currentState_ ? currentState_->DebugName() : "<null>");
        }
        return 0u;
    }

    if (launchPadClient65c_ != nullptr) {
        const char* helperString = launchPadClient65c_->authConnection18.c_str();
        // Fidelity: refresh session token in owner+0x94 block
        ownerAuthBootstrapSource94_.sessionToken60.begin = helperString;
        ownerAuthBootstrapSource94_.sessionToken60.current = helperString + launchPadClient65c_->authConnection18.size();
        ownerAuthBootstrapSource94_.sessionToken60.capacity = ownerAuthBootstrapSource94_.sessionToken60.current;
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest alternate g_LaunchPadGateState16State18!=0 branch refreshed owner+0xf4 from helper65c string18='{}'",
            launchPadClient65c_->authConnection18.empty() ? "<empty>" : launchPadClient65c_->authConnection18.c_str());
    }

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth nonhappy ProcessLoginRequest g_LaunchPadGateState16State18 branch -> state16 (string60 empty) upstreamState={} helper65cPresent={}",
        upstreamState ? upstreamState->DebugName() : "<null>",
        launchPadClient65c_ ? 1u : 0u);
    if (LoginHelperStateByIdScaffold(16u) != nullptr) {
        (void)SetCurrentState(16u);
    } else {
        spdlog::info(
            "CLTLoginMediator::ProcessLoginRequest missing registered state16 scaffold for alternate string60-empty branch currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return 0u;
}

// +0x34
void CLTLoginMediator::RequestAuthCloseAndSwitchToState0() {
    // anchor: launcher.exe:0x41c0d0
    // Current best original shape:
    // - call owner vtable `+0x164`
    // - if old helper exists, notify it with the global state0 object
    // - install global state0 object at owner `+0x10`
    // - notify state0 with the old helper object
    const CLTLoginState* const oldState = currentState_;
    const bool authCloseArmed = RequestAuthConnectionCloseWaitEvent1();
    uint32_t state0EntryResult = 0u;
    if (LoginHelperStateByIdScaffold(0u) != nullptr) {
        state0EntryResult = SetCurrentState(0u);
    }
    spdlog::info(
        "CLTLoginMediator::RequestAuthCloseAndSwitchToState0 authCloseArmed={} oldState={} currentState={} state0EntryResult=0x{:08x}",
        authCloseArmed ? 1u : 0u,
        oldState ? oldState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(state0EntryResult));
}

// anchor: launcher.exe:0x41f0a0 / owner vtable +0x38
const char* CLTLoginMediator::GetUsername() const {
    return ownerAuthBootstrapSource94_.username00.data();
}

// Wrapper-facing arg6 `+0x40` selection-descriptor object builder.
// Keep this naming split explicit from owner `+0x40 = 0x41f2e0 = GetSlotRecordByIndex`.
// Fidelity tightening:
// - keep the wrapper-facing object because client arg6 `+0x40` really returns a distinct ABI shape
// - but source its payload directly from the anchored owner-side current-slot family instead of
//   carrying a second synthetic arg6-side source picker/fallback tree
Arg6CurrentSlotRecord44ObjectSketch* CLTLoginMediator::GetArg6SelectionDescriptorObject40(
    uint32_t selectionIndex) {
    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = Arg6ExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = Arg6SelectionDescriptorMatchesRequest(selectionIndex);
    const uint8_t currentSlotIndex = this->CurrentCharacterRouteIndexCc8Scaffold();
    const bool matchedCurrentSlotIndexRequest =
        CurrentHelperStateCodeOrZero(this) >= 3u &&
        high8 == 0u &&
        low24 == static_cast<uint32_t>(currentSlotIndex);

    const SlotRecordState_0x4b5328* const currentSlotRecord =
        (matchedConfiguredRequest || matchedCurrentSlotIndexRequest)
            ? this->GetCurrentSlotRecord()
            : nullptr;

    if (!currentSlotRecord) {
        spdlog::debug(
            "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x}) -> NULL (currentSlotIndex=0x{:02x} expectedScratchRequest=0x{:08x} matchedConfiguredRequest={} matchedCurrentSlotIndexRequest={})",
            static_cast<unsigned>(selectionIndex),
            static_cast<unsigned>(low24),
            static_cast<unsigned>(high8),
            static_cast<unsigned>(currentSlotIndex),
            static_cast<unsigned>(expectedScratchRequest),
            matchedConfiguredRequest ? 1u : 0u,
            matchedCurrentSlotIndexRequest ? 1u : 0u);
        return nullptr;
    }

    arg6SelectionDescriptor40_ = {};
    // Set payload10 to point to currentSlotRecord's fields directly
    // The client accesses fields at offset +0x03/+0x07 relative to this pointer
    arg6SelectionDescriptor40_.vtable = Arg6CurrentSlotRecord44Vtable();
    arg6SelectionDescriptor40_.payload10 = const_cast<SlotRecordState_0x4b5328*>(currentSlotRecord);
    arg6SelectionDescriptor40_.flag0c = 1u;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        (matchedCurrentSlotIndexRequest ? "current-slot-index" : "configured-request");
    spdlog::debug(
        "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x}) -> {} (matchMode={} currentSlotIndex=0x{:02x} slotName='{}' vtable={} field03=0x{:08x} field07=0x{:08x})",
        static_cast<unsigned>(selectionIndex),
        static_cast<unsigned>(low24),
        static_cast<unsigned>(high8),
        fmt::ptr(&arg6SelectionDescriptor40_),
        matchMode,
        static_cast<unsigned>(currentSlotIndex),
        currentSlotRecord->heapString14 ? currentSlotRecord->heapString14 : "<empty>",
        fmt::ptr(arg6SelectionDescriptor40_.vtable),
        static_cast<unsigned>(currentSlotRecord->characterIdLow32),
        static_cast<unsigned>(currentSlotRecord->characterIdHigh36));
    return &arg6SelectionDescriptor40_;
}

// Wrapper-facing arg6 `+0x44` current-slot-record wrapper builder.
// Keep this naming split explicit from owner `+0x44 = GetCurrentSlotRecord`.
// Fidelity tightening:
// - keep the wrapper-facing object because client arg6 `+0x44` expects the `0x004b5328`-like ABI
//   shape
// - but source it directly from the anchored owner-side current-slot accessor instead of a second
//   synthetic arg6-side source/fallback path
Arg6CurrentSlotRecord44ObjectSketch* CLTLoginMediator::GetArg6CurrentSlotRecordObject44() {
    const SlotRecordState_0x4b5328* const currentSlotRecord = this->GetCurrentSlotRecord();

    arg6CurrentSlotRecord44_ = {};
    arg6CurrentSlotRecord44NameOwned_.clear();

    if (!currentSlotRecord) {
        spdlog::info(
            "CLTLoginMediator::GetArg6CurrentSlotRecordObject44(+0x44) -> NULL [currentSlotIndex=0x{:02x}]",
            static_cast<unsigned>(this->CurrentCharacterRouteIndexCc8Scaffold()));
        return nullptr;
    }

    // Set payload10 to point to currentSlotRecord's fields directly
    // The client accesses fields at offset +0x03/+0x07/+0x0b/+0x0c relative to this pointer
    arg6CurrentSlotRecord44_.vtable = Arg6CurrentSlotRecord44Vtable();
    arg6CurrentSlotRecord44_.payload10 = const_cast<SlotRecordState_0x4b5328*>(currentSlotRecord);
    arg6CurrentSlotRecord44_.flag0c = 1u;
    arg6CurrentSlotRecord44NameOwned_ = currentSlotRecord->heapString14;

    if (!arg6CurrentSlotRecord44NameOwned_.empty()) {
        arg6CurrentSlotRecord44_.heapString14 = arg6CurrentSlotRecord44NameOwned_.c_str();
        const size_t nameLength = arg6CurrentSlotRecord44NameOwned_.size();
        arg6CurrentSlotRecord44_.heapStringLen18 =
            static_cast<uint16_t>((nameLength < 0xffffu) ? nameLength : 0xffffu);
    }

    spdlog::info(
        "CLTLoginMediator::GetArg6CurrentSlotRecordObject44(+0x44) -> {} [name='{}' idLow=0x{:08x} idHigh=0x{:08x} status=0x{:02x} worldId=0x{:04x}]",
        fmt::ptr(&arg6CurrentSlotRecord44_),
        arg6CurrentSlotRecord44_.heapString14 ? arg6CurrentSlotRecord44_.heapString14 : "<empty>",
        static_cast<unsigned>(currentSlotRecord->characterIdLow32),
        static_cast<unsigned>(currentSlotRecord->characterIdHigh36),
        static_cast<unsigned>(currentSlotRecord->status3a),
        static_cast<unsigned>(currentSlotRecord->worldId3c));
    return &arg6CurrentSlotRecord44_;
}

// +0x48
// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetWorldOrSelectionName() const {
    const SlotRecordState_0x4b5328* slotRecord = GetCurrentSlotRecord();
    if (!slotRecord) {
        slotRecord = GetSlotRecordByIndex(0u);
    }

    const auto& ownerState = postAuthMarginLoadingState_0xf14;
    const ActiveCharacterStateViewScaffold characterState = DescribeActiveCharacterStateScaffold();
    const char* worldOrSelectionName = Arg6MappedSelectionName();
    const char* source = "arg6-selection";

    if (slotRecord && slotRecord->heapString14) {
        worldOrSelectionName = slotRecord->heapString14;
        source = "slotRecord+0x14";
    } else if (ownerState.characterNameBufferF1c[0]) {
        worldOrSelectionName = ownerState.characterNameBufferF1c;
        source = "owner+0xf1c";
    } else if (ownerState.createCharacterData108.characterName00[0]) {
        worldOrSelectionName = ownerState.createCharacterData108.characterName00.data();
        source = "owner+0x108";
    } else if (characterState.characterName && characterState.characterName[0]) {
        worldOrSelectionName = characterState.characterName;
        source = "active-character-state";
    }

    spdlog::debug(
        "CLTLoginMediator::GetWorldOrSelectionName(+0x48) -> '{}' [source={} currentSlot='{}' profile='{}' mappedSelection='{}']",
        NonEmptyTextOrPlaceholder(worldOrSelectionName),
        source,
        (slotRecord && slotRecord->heapString14)
            ? slotRecord->heapString14
            : "<empty>",
        NonEmptyTextOrPlaceholder(Arg6ProfileName()),
        NonEmptyTextOrPlaceholder(Arg6MappedSelectionName()));
    return worldOrSelectionName;
}

// +0x4c
// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetProfileOrSessionName() const {
    const char* profileOrSessionName = Arg6ProfileName();
    spdlog::debug(
        "CLTLoginMediator::GetProfileOrSessionName(+0x4c) -> '{}'",
        NonEmptyTextOrPlaceholder(profileOrSessionName));
    return profileOrSessionName;
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
void* CLTLoginMediator::BootstrapRaw08AuxHandle50() const {
    // Fidelity to static-RE: reads dword at offset +0xa8 from copyShadow
    const auto* copyShadow =
        authBootstrapChild680_ ? authBootstrapChild680_->authReplyCopyShadowF4 : nullptr;
    if (copyShadow != nullptr) {
        return reinterpret_cast<void*>(
            *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(copyShadow) + 0xa8));
    }
    return nullptr;
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
bool CLTLoginMediator::HasBootstrapRaw08AuxHandle54() const {
    const bool present = authBootstrapChild680_ ? authBootstrapChild680_->HasBootstrapRaw08AuxHandle54() : false;
    spdlog::debug(
        "CLTLoginMediator::HasBootstrapRaw08AuxHandle54(+0x54) -> {}",
        present ? 1u : 0u);
    return present;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t CLTLoginMediator::GetCrashReporterPromptForSecurId58() const {
    const uint8_t prompt = authBootstrapChild680_ ? authBootstrapChild680_->GetCrashReporterPromptForSecurId58() : 0u;
    spdlog::debug(
        "CLTLoginMediator::GetCrashReporterPromptForSecurId58(+0x58) -> {}",
        static_cast<unsigned>(prompt));
    return prompt;
}

// Wrapper-facing launcher/client chain note for `+0x5c/+0x60`:
// - launcher crashreporter seeding calls both slots with no stack argument
// - client `InitClientDLL` uses caller-clean wrappers and threads the previous return value
//   through the next call
// Keep the incoming value opaque here instead of forcing a false `const char*` semantic.
// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x5c
const char* CLTLoginMediator::GetCrashReporterUsername5c(const void* chainedValueToken) {
    const char* authName = Arg6AuthName();
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterUsername5c(+0x5c chainedValueToken={}) -> '{}'",
        fmt::ptr(chainedValueToken),
        NonEmptyTextOrPlaceholder(authName));
    return authName;
}

// +0x60
// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetCrashReporterPassword60(const void* chainedValueToken) {
    const char* authPassword = Arg6AuthPassword();
    const char* bootstrapPassword = authBootstrapChild680_ ? authBootstrapChild680_->stringF8.begin : nullptr;
    const char* effectivePassword =
        (bootstrapPassword && bootstrapPassword[0] != '\0') ? bootstrapPassword : authPassword;
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterPassword60(+0x60 chainedValueToken={}) -> {} [source={}]",
        fmt::ptr(chainedValueToken),
        MaskedSensitiveValue(effectivePassword),
        (bootstrapPassword && bootstrapPassword[0] != '\0') ? "owner+0x680+0xf8" : "arg6-auth-password");
    return effectivePassword;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x68
uint32_t CLTLoginMediator::HasLiveHlCfg68() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag140e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x6c
uint32_t CLTLoginMediator::HasLiveAnCfg6c() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1416 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x70
uint32_t CLTLoginMediator::HasLivePiCfg70() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag141e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x74
uint32_t CLTLoginMediator::HasLiveAiCfg74() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag1426 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x78
uint32_t CLTLoginMediator::HasLiveCsCfg78() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x7c
uint32_t CLTLoginMediator::HasLiveBlCfg7c() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag13fe != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x80
uint32_t CLTLoginMediator::HasLiveIlCfg80() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1406 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x84
uint32_t CLTLoginMediator::HasLiveRlCfg84() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1448 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x88
uint32_t CLTLoginMediator::HasLiveClCfg88() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1452 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x8c
uint32_t CLTLoginMediator::HasState8PersistenceData8c() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    const uint32_t ready =
        (ownerState && ownerState->section0Flag13f6 != 0u) ? 1u : 0u;
    spdlog::info(
        "CLTLoginMediator::HasState8PersistenceData8c(+0x8c) -> {} [flag13f6={}]",
        ready,
        ownerState ? static_cast<unsigned>(ownerState->section0Flag13f6) : 0u);
    return ready;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x90
uint32_t CLTLoginMediator::HasLiveCuiCfg90() const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag145a != 0u);
        view.buffer = ownerState->allocatedBuffer1454;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1458);
    }
    const uint32_t result = view.ready;
    if (result == 0u && !liveCuiCfgAbsentNoteLogged90_) {
        liveCuiCfgAbsentNoteLogged90_ = true;
        spdlog::info(
            "CLTLoginMediator::HasLiveCuiCfg90(+0x90) note: live cui.cfg is absent on the current path; bounded original reruns also omit final cui.cfg, while replacement may still emit an on-disk cui.cfg later through the client-owned direct-save path 0x62198490 -> 0x62197050");
    }
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x94
void* CLTLoginMediator::GetLiveHlCfg94(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag140e != 0u);
        view.buffer = ownerState->allocatedBuffer1408;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength140c);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x98
void* CLTLoginMediator::GetLiveAnCfg98(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1416 != 0u);
        view.buffer = ownerState->allocatedBuffer1410;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1414);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x9c
void* CLTLoginMediator::GetLivePiCfg9c(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag141e != 0u);
        view.buffer = ownerState->allocatedBuffer1418;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength141c);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xa0
void* CLTLoginMediator::GetLiveAiCfgA0(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag1426 != 0u);
        view.buffer = ownerState->allocatedBuffer1420;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1424);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xa4
void* CLTLoginMediator::GetLiveCsCfgA4(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u);
        view.buffer = ownerState->allocatedBuffer1428;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength142c);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xa8
void* CLTLoginMediator::GetLiveBlCfgA8(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag13fe != 0u);
        view.buffer = ownerState->allocatedBuffer13f8;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength13fc);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xac
void* CLTLoginMediator::GetLiveIlCfgAc(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1406 != 0u);
        view.buffer = ownerState->allocatedBuffer1400;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1404);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xb0
void* CLTLoginMediator::GetLiveRlCfgB0(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1448 != 0u);
        view.buffer = ownerState->allocatedBuffer1440;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1444);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xb4
void* CLTLoginMediator::GetLiveClCfgB4(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1452 != 0u);
        view.buffer = ownerState->allocatedBuffer144c;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1450);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xb8
void* CLTLoginMediator::GetLiveCuiCfgB8(uint32_t* outLength) const {
    const auto* ownerState = &this->postAuthMarginLoadingState_0xf14;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag145a != 0u);
        view.buffer = ownerState->allocatedBuffer1454;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1458);
    }
    if (outLength) {
        *outLength = view.length;
    }
    return view.buffer;
}

// anchor: launcher.exe:0x41f170 vtable +0xbc
const void* CLTLoginMediator::GetState8PersistenceHeaderBc() const {
    // Keep this wrapper-facing body close to the original tiny getter:
    // - original `0x41f170` returns owner `+0xf48`
    const void* header = static_cast<const void*>(&postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.header2c[0]);
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceHeaderBc(+0xbc) -> {} [owner={} first=0x{:08x} bytes=0x{:02x}]",
        fmt::ptr(header),
        fmt::ptr(this),
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.header2c[0]),
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.header2c.size() * sizeof(uint32_t)));
    return header;
}

// anchor: launcher.exe:0x41f180 vtable +0xc0
const void* CLTLoginMediator::GetState8PersistenceBodyC0() const {
    // Keep this wrapper-facing body close to the original tiny getter:
    // - original `0x41f180` returns owner `+0xf88`
    const void* body = static_cast<const void*>(&postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.bodyWord6c);
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceBodyC0(+0xc0) -> {} [owner={} body00=0x{:08x} bytes=0x{:04x}]",
        fmt::ptr(body),
        fmt::ptr(this),
        postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.bodyWord6c,
        static_cast<unsigned>(CLTLoginMediatorCharacterPersistenceData::kBodySize));
    return body;
}

// anchor: launcher.exe:0x41aec0 vtable +0xc4
void* CLTLoginMediator::GetState8PersistenceOverflowC4(uint16_t* outLength) const {
    // Keep this wrapper-facing body close to the original tiny getter:
    // - original `0x41aec0` returns owner `+0x13f0`
    // - when the caller supplies an out pointer, it also writes owner `+0x13f4`
    if (outLength) {
        *outLength = postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.section0OverflowLength4d8;
    }
    void* const buffer = postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.section0OverflowBuffer4d4;
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceOverflowC4(+0xc4) -> {} [owner={} length=0x{:04x}]",
        fmt::ptr(buffer),
        fmt::ptr(this),
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.section0OverflowLength4d8));
    return buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xc8
uint32_t CLTLoginMediator::HasState8Section11Dword145c() const {
    const uint32_t value = postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.section11Dword540;
    const uint32_t ready = value != 0u ? 1u : 0u;
    spdlog::info(
        "CLTLoginMediator::HasState8Section11Dword145c(+0xc8) -> {} [owner={} value=0x{:08x}]",
        ready,
        fmt::ptr(this),
        value);
    return ready;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xcc
uint32_t CLTLoginMediator::GetState8Section11Dword145c() const {
    const uint32_t value = postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c.section11Dword540;
    spdlog::info(
        "CLTLoginMediator::GetState8Section11Dword145c(+0xcc) -> 0x{:08x} [owner={}]",
        value,
        fmt::ptr(this));
    return value;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xd0
RouteDescriptor30SmallStringLikeSketch* CLTLoginMediator::GetState8Section11String1460() {
    const CLTLoginMediatorCharacterPersistenceData& snapshot =
        postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c;
    state8Section11String1460_.begin = snapshot.section11StringBegin544;
    state8Section11String1460_.current = snapshot.section11StringCurrent548;
    state8Section11String1460_.capacity = snapshot.section11StringCapacity54c;

    const char* text =
        (snapshot.section11StringBegin544 &&
         snapshot.section11StringBegin544 != snapshot.section11StringCurrent548)
            ? snapshot.section11StringBegin544
            : "<empty>";
    spdlog::info(
        "CLTLoginMediator::GetState8Section11String1460(+0xd0) -> begin={} current={} owner={} text='{}'",
        fmt::ptr(state8Section11String1460_.begin),
        fmt::ptr(state8Section11String1460_.current),
        fmt::ptr(this),
        text);
    return &state8Section11String1460_;
}

// anchor: launcher.exe:0x41b4f0 +0xd4
const void* CLTLoginMediator::GetState9CallbackSeedPointer85D4() const {
    if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_)) {
        if (const uint8_t* seedPointer = marginConnection->MessageCode5SeedBytes85Pointer()) {
            const uint32_t* seedWords = reinterpret_cast<const uint32_t*>(seedPointer);
            spdlog::info(
                "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> {} [source=connection+0x85 mirror connection={} seed[0..3]=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}]]",
                fmt::ptr(seedPointer),
                fmt::ptr(marginConnection),
                static_cast<unsigned>(seedWords[0]),
                static_cast<unsigned>(seedWords[1]),
                static_cast<unsigned>(seedWords[2]),
                static_cast<unsigned>(seedWords[3]));
            return seedPointer;
        }
    }

    const auto it = g_marginBootstrapStateByMediator.find(this);
    if (it != g_marginBootstrapStateByMediator.end() && it->second.marginTwofishKeyBytes.size() == 16u) {
        const void* seedPointer = it->second.marginTwofishKeyBytes.data();
        const uint32_t* seedWords = reinterpret_cast<const uint32_t*>(seedPointer);
        spdlog::info(
            "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> {} [source=bootstrap-sidecar-fallback marginConnection={} original+0xd4=owner+0x1c+0x85 seed[0..3]=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}]]",
            fmt::ptr(seedPointer),
            fmt::ptr(marginConnection_),
            static_cast<unsigned>(seedWords[0]),
            static_cast<unsigned>(seedWords[1]),
            static_cast<unsigned>(seedWords[2]),
            static_cast<unsigned>(seedWords[3]));
        return seedPointer;
    }

    spdlog::info(
        "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> <null> [marginConnection={} expectedLiveSource=owner+0x1c+0x85]",
        fmt::ptr(marginConnection_));
    return nullptr;
}

// anchor: launcher.exe:0x41af00 / owner vtable +0xd8
// Tiny original state gate:
// - if the current helper/state exists and its slot-7-style phase code is `>= 3`, return owner
//   byte `+0x684`
// - otherwise return `0`
uint32_t CLTLoginMediator::GetArg7SelectionUpperBoundExclusive() const {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    const bool stateAllowsRead = stateCode >= 3u;
    const uint32_t upperBoundExclusive =
        stateAllowsRead ? static_cast<uint32_t>(selectionRouteState684_.slotRecordCount00_) : 0u;

    spdlog::info(
        "CLTLoginMediator::GetArg7SelectionUpperBoundExclusive(+0xd8) -> {} [stateCode={} source={}]",
        static_cast<unsigned>(upperBoundExclusive),
        static_cast<unsigned>(stateCode),
        stateAllowsRead ? "owner+0x684" : "state-gated-zero");
    return upperBoundExclusive;
}

// anchor: launcher.exe:0x41b220 / owner vtable +0xdc
// Same recovered body as the slot-record heap-string reader:
// - gate on current helper/state code `>= 3`
// - require index `< 100`
// - return owner `+0x688[index] +0x14` when the slot exists, else `0`
const char* CLTLoginMediator::MapSelectionName(uint32_t selectionHighByte) const {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    const bool stateAllowsRead = stateCode >= 3u;
    const bool indexInRange = selectionHighByte < 100u;
    const char* selectionName = (stateAllowsRead && indexInRange)
        ? LookupSlotRecordHeapStringByIndex(static_cast<uint8_t>(selectionHighByte))
        : nullptr;

    const char* source =
        !stateAllowsRead ? "state-gated-zero" :
        !indexInRange ? "index>=100" :
        (selectionName ? "owner+0x688[index].heapString14" : "owner+0x688[index]=<null>");
    spdlog::info(
        "CLTLoginMediator::MapSelectionName(+0xdc selectionHighByte={}) -> '{}' [stateCode={} source={}]",
        static_cast<unsigned>(selectionHighByte),
        selectionName ? selectionName : "<null>",
        static_cast<unsigned>(stateCode),
        source);
    return selectionName;
}

// anchor: launcher.exe:0x41b260 / owner vtable +0xe0
// Exact owner-side string reader shared with the launcher page-`7` world-list path:
// - gate on current helper/state code `>= 3`
// - require index `< 100`
// - return owner `+0x818[index*3].begin` when begin != end, else `0`
const char* CLTLoginMediator::GetVariantWorldName(uint32_t variantIndex) {
    ++arg6VariantWorldNameQueryCountE0_;
    if ((arg6VariantWorldNameQueryCountE0_ % 5u) == 0u) {
        spdlog::info(
            "CLTLoginMediator::GetVariantWorldName(+0xe0) queryCount={}",
            arg6VariantWorldNameQueryCountE0_);
    }

    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    const bool stateAllowsRead = stateCode >= 3u;
    const bool indexInRange = variantIndex < 100u;
    const char* worldName = (stateAllowsRead && indexInRange)
        ? LookupRouteHostPrefixBySlot(static_cast<uint8_t>(variantIndex))
        : nullptr;

    const char* source =
        !stateAllowsRead ? "state-gated-zero" :
        !indexInRange ? "index>=100" :
        (worldName ? "owner+0x818[index*3].begin" : "owner+0x818[index*3]=<empty>");
    if (!worldName) {
        spdlog::info(
            "CLTLoginMediator::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> NULL [stateCode={} source={}]",
            static_cast<unsigned>(variantIndex & 0xffu),
            static_cast<unsigned>(stateCode),
            source);
        return nullptr;
    }

    spdlog::info(
        "CLTLoginMediator::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> '{}' [stateCode={} source={}]",
        static_cast<unsigned>(variantIndex & 0xffu),
        worldName,
        static_cast<unsigned>(stateCode),
        source);
    return worldName;
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0xe4
// vtable: ILTLoginMediator.Default slot +0xe4
// Tighter launcher page-`7` read now makes this high-word consumer better fit the active
// selection-entry / slot-record index on the auth-valid path.
uint8_t CLTLoginMediator::GetVariantState(int32_t variantIndex) const {
    const bool useRecoveredActiveEntryTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;

    uint32_t state = 3u;
    const char* source = "arg6-selection-fallback";
    if (useRecoveredActiveEntryTable) {
        if (variantIndex >= 0 && variantIndex <= 0xff) {
            state = GetSlotRecordStatusByIndex(static_cast<uint8_t>(variantIndex));
            source = "owner+0x688.status+0x0b";
        } else {
            source = "owner+0x688.<out-of-range>";
        }
    } else if (variantIndex >= 0) {
        const uint32_t unsignedVariantIndex = static_cast<uint32_t>(variantIndex);
        if (unsignedVariantIndex < this->Arg6VariantUpperBoundExclusive() &&
            this->Arg6VariantIndexMatchesSelection(unsignedVariantIndex)) {
            state = this->Arg6SelectedVariantState();
            source = "arg6-selected-active-entry-state";
        } else {
            source = "arg6-selection-fallback.<not-selected>";
        }
    } else {
        source = "negative-index";
    }
    spdlog::info(
        "MediatorStub::GetVariantState(+0xe4 variantIndex={}) -> {} [source={} configuredVariant=0x{:02x} configuredState={}]",
        variantIndex,
        state,
        source,
        this->Arg6SelectedVariantIndexHigh8(),
        this->Arg6SelectedVariantState());
    return state;
}

// anchor: launcher.exe:0x41ec00 +0xe8
// Original body is narrower than the current replacement-side bookkeeping:
// - gate on current helper/state code `> 2`
// - require selection-route count `!= 0` and input `< 100`
// - decrement selection-route slot count
// - release/remove owner `+0x688[index]`
// - shift later `+0x688` pointers and `+0x818` string-triples down
// - clear the final pointer/string tail
// Current source still keeps extra replacement-side mirrors like `lastAuthReply_` and
// `PersistCharactersIniFromRecoveredAuthStateScaffold()` coherent around that narrower original
// mutation.
uint32_t CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex(uint32_t selectedSlotRecordIndex) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    if (stateCode <= 2u || selectedSlotRecordIndex >= 100u ||
        selectionRouteState684_.slotRecordCount00_ == 0u) {
        spdlog::info(
            "CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex rejected slotIndex={} stateCode={} count={} currentState={}",
            static_cast<unsigned>(selectedSlotRecordIndex),
            static_cast<unsigned>(stateCode),
            static_cast<unsigned>(selectionRouteState684_.slotRecordCount00_),
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    const size_t slotIndex = static_cast<size_t>(selectedSlotRecordIndex);
    const uint8_t oldCount = selectionRouteState684_.slotRecordCount00_;
    const std::string removedName = selectionRouteState684_.slotRecordTable04_[slotIndex].heapString14 ? selectionRouteState684_.slotRecordTable04_[slotIndex].heapString14 : "";

    // anchor: launcher.exe:0x41ec00
    // Original order:
    // - decrement `+0x684`
    // - release/remove `+0x688[index]`
    // - shift later `+0x688` pointers and `+0x818` string-triples down
    // - clear the final pointer/string tail
    selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(oldCount - 1u);
    selectionRouteState684_.slotRecordTable04_[slotIndex] = {};
    selectionRouteState684_.slotRecordValid04_[slotIndex] = false;

    for (size_t i = slotIndex; i + 1u < selectionRouteState684_.slotRecordTable04_.size(); ++i) {
        selectionRouteState684_.slotRecordTable04_[i] = selectionRouteState684_.slotRecordTable04_[i + 1u];
        selectionRouteState684_.slotRecordValid04_[i] = selectionRouteState684_.slotRecordValid04_[i + 1u];
        selectionRouteState684_.routeHostStringTriples194_[i] =
            selectionRouteState684_.routeHostStringTriples194_[i + 1u];
    }
    selectionRouteState684_.slotRecordTable04_.back() = {};
    selectionRouteState684_.slotRecordValid04_.back() = false;
    selectionRouteState684_.routeHostStringTriples194_.back().Clear();

    // Replacement-only mirror maintenance kept explicit after the narrower original mutation.
    postAuthMarginLoadingState_0xf14.characterRouteIndexCc8 = selectionRouteState684_.CurrentSlotOrSelectionIndex644();
    marginRouteState_.currentCharacterOrRouteIndex = selectionRouteState684_.CurrentSlotOrSelectionIndex644();
    if (slotIndex < lastAuthReply_.characters.size()) {
        lastAuthReply_.characters.erase(lastAuthReply_.characters.begin() + static_cast<std::ptrdiff_t>(slotIndex));
    }
    PersistCharactersIniFromRecoveredAuthStateScaffold();

    spdlog::info(
        "CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex removed slotIndex={} oldCount={} newCount={} removedName='{}' currentState={}",
        static_cast<unsigned>(selectedSlotRecordIndex),
        static_cast<unsigned>(oldCount),
        static_cast<unsigned>(selectionRouteState684_.slotRecordCount00_),
        removedName.empty() ? "<empty>" : removedName.c_str(),
        currentState_ ? currentState_->DebugName() : "<null>");
    return 0u;
}

// anchor: launcher.exe:0x41c1f0 +0xec
uint32_t CLTLoginMediator::PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) {
    // anchor: launcher.exe:0x41c1f0
    // Owner-side state3-wait advance:
    // - the early route reaches this while current helper `+0x10` is still state3
    // - this method, not a state3-local slot-3 body, copies the `0xb4` selection/config snapshot
    //   into owner `+0xcc8/+0xcd0..+0xd7f`
    // - then it switches to helper/state `8`
    // Fresh happy-path proof tightens the transition boundary too:
    // - the already-proven upstream chain is
    //   `state0 -> ProcessLoginRequest -> state2 -> state1 -> state2 -> state3(wait) -> 0x41c1f0`
    // - no additional early helper-switch hits were observed in the narrow state3 wait window
    //   before the live `0x41c1f0` stop
    // - newer launcher/client bridge tightening also narrows the immediate producer split:
    //   - launcher-side selection resolution currently closes through `0x40d6f0` writing
    //     `CLauncher+0xa8/+0xac` plus `Last_WorldName`
    //   - the direct success-side `+0xec` call is then best read from
    //     `client.dll:0x62170f48 = InitClientDLL_BeginLoadingCharacterFlow`, where the client
    //     zero-initializes and fills the stack-local `0xb4` handoff immediately before calling
    //     this owner method
    selectionContext0ecCopy_ = input;
    selectionContext0ecCopyValid_ = true;
    ++selection0ecCount_;
    if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 3u) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state2 -> state3(wait) -> owner+0xec/0x41c1f0 currentState={} slot=0x{:02x}",
            currentState_->DebugName(),
            static_cast<unsigned>(selectionContext0ecCopy_.slotOrSelectionIndex00 & 0xffu));
    }
    spdlog::info(
        "CLTLoginMediator::PersistSelectionContextForState8 captured selection mirror [count={}] slot=0x{:02x} block04_0=0x{:08x} blockA4_3=0x{:08x}",
        selection0ecCount_,
        static_cast<unsigned>(selectionContext0ecCopy_.slotOrSelectionIndex00 & 0xffu),
        selectionContext0ecCopy_.block04[0],
        selectionContext0ecCopy_.blockA4[3]);
    if (input.slotOrSelectionIndex00 >= 100u) {
        return 0u;
    }

    SetCurrentCharacterRouteIndexCc8Scaffold(static_cast<uint8_t>(input.slotOrSelectionIndex00));
    static_assert(
        sizeof(selectionRouteState684_.persistedSelectionContext64c_) == sizeof(input) - sizeof(input.slotOrSelectionIndex00),
        "0x41c1f0 should copy the contiguous state3 snapshot body after the leading slot dword");
    std::copy_n(
        reinterpret_cast<const uint32_t*>(&input.block04),
        sizeof(selectionRouteState684_.persistedSelectionContext64c_) / sizeof(uint32_t),
        reinterpret_cast<uint32_t*>(&selectionRouteState684_.persistedSelectionContext64c_));

    const CLTLoginState* const oldState = currentState_;
    uint32_t state8EntryResult = 0u;
    if (LoginHelperStateByIdScaffold(8u) != nullptr) {
        // anchor: launcher.exe:0x41c1f0 -> 0x41b450(8)
        // Important existing-character continuation detail:
        // - the original owner `+0xec` tail does not stop at `currentState = state8`
        // - `0x41b450` immediately re-enters the new helper's slot 3 with the old helper object
        // - on the active path that means `state8 slot3` runs right here, sees margin state != 2,
        //   and hands off into helper/state4 before the later margin connect-status arrives
        // - without that immediate slot-3 continuation, the later type-2 margin completion lands on
        //   shared state8 slot2 and returns 0 instead of restoring the original state4/state5/state6
        //   chain back toward the first natural state8 raw-0x0f send
        state8EntryResult = SetCurrentState(8u);
    }

    spdlog::info(
        "CLTLoginMediator::PersistSelectionContextForState8 mirrored owner-advanced state3(wait)->state8 selection snapshot slot=0x{:02x} blockCd0_0=0x{:08x} blockD70_3=0x{:08x} oldState={} currentState={} state8EntryResult=0x{:08x}",
        selectionRouteState684_.CurrentSlotOrSelectionIndex644(),
        selectionRouteState684_.persistedSelectionContext64c_.blockCd0[0],
        selectionRouteState684_.persistedSelectionContext64c_.blockD70[3],
        oldState ? oldState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<unchanged>",
        static_cast<unsigned>(state8EntryResult));
    return 0u;
}

// anchor: launcher.exe:0x41c390 +0xf0
uint32_t CLTLoginMediator::SetSelectionIndexAndSwitchToState7(uint32_t selectedSlotRecordIndex) {
    if (currentState_ && currentState_->DispatchPhaseCode() > 2u && selectedSlotRecordIndex < 100u) {
        SetCurrentCharacterRouteIndexCc8Scaffold(static_cast<uint8_t>(selectedSlotRecordIndex & 0xffu));
        if (LoginHelperStateByIdScaffold(7u) != nullptr) {
            (void)SetCurrentState(7u);
        }
        // Bounded source note:
        // - original `0x41c390` switches helper state to `7`
        // - one concrete launcher-side upstream caller now visible in static RE is
        //   `launcher.exe:0x40ec70`, reached from `0x405a20` case `9`, which:
        //   - loads UI strings/resource `0x0008` (`Deleted characters cannot be recovered...`) and
        //     `0x00aa`, confirms against the selected character name, then
        //   - reads the selected row item-data high word as a signed active-selection-entry index
        //     (current tighter auth-valid read: slot-record / character-entry index)
        //   - calls sibling mediator slot `+0xf0` with that selected row high word
        //   - waits for event `8` through `0x41b6c0`
        //   - on success (`WaitForEvent` returns `0`) deletes `Profiles\%s\%s`, calls sibling
        //     `+0xe8 = 0x41ec00`, and rebuilds the list
    }
    return 0u;
}

// anchor: launcher.exe:0x41f1c0 / owner vtable +0xf4
const void* CLTLoginMediator::GetState8PersistenceF1c() const {
    // Keep the wrapper-facing body close to the original tiny getter:
    // - original `0x41f1c0` returns owner `+0xf1c`
    const CLTLoginMediatorCharacterPersistenceData* const snapshot =
        &postAuthMarginLoadingState_0xf14.state8PersistenceDataF1c;
    ++profile0f4Count_;
    spdlog::debug(
        "CLTLoginMediator::GetState8PersistenceF1c(+0xf4) -> {} [count={} copiedFrom0ec={} valid0ec={} char='{}' first='{}' last='{}' background='{}' field24=0x{:08x} overflow13f4=0x{:04x}]",
        fmt::ptr(snapshot),
        profile0f4Count_,
        selection0ecCount_,
        selectionContext0ecCopyValid_ ? 1u : 0u,
        snapshot->characterName00[0] ? snapshot->characterName00.data() : "<empty>",
        snapshot->realFirstName70[0] ? snapshot->realFirstName70.data() : "<empty>",
        snapshot->realLastName90[0] ? snapshot->realLastName90.data() : "<empty>",
        snapshot->backgroundB0[0] ? snapshot->backgroundB0.data() : "<empty>",
        static_cast<unsigned>(snapshot->selectedWorldField24),
        static_cast<unsigned>(snapshot->section0OverflowLength4d8));
    return snapshot;
}

// anchor: launcher.exe:0x41af30 / launcher.exe:0x40e5b0
// vtable: ILTLoginMediator.Default slot +0xf8
uint32_t CLTLoginMediator::GetWorldCount() const {
    const uint32_t worldCount = lastAuthReply_.valid && !lastAuthReply_.isErrorReply
        ? static_cast<uint32_t>(worldDescriptorCountD80_)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldCount(+0xf8) -> {} [source={}]",
        worldCount,
        (worldCount != 0u) ? "owner+0xd84" : "no-active-descriptor-table");
    return worldCount;
}

// anchor: launcher.exe:0x41b2e0 / launcher.exe:0x40cd10
// vtable: ILTLoginMediator.Default slot +0xfc
// Active replacement path note:
// - the text-mode launcher selection stage now runs only after auth success
// - so this wrapper-facing getter can stay owner-table-backed without the older synthetic
//   startup world-list sidecar
const char* CLTLoginMediator::GetWorldNameByIndex(uint32_t index) {
    const char* worldName =
        (lastAuthReply_.valid && !lastAuthReply_.isErrorReply && index <= 0xffu)
            ? GetDescriptorInlineNameByIndex(static_cast<uint8_t>(index))
            : nullptr;

    spdlog::info(
        "CLTLoginMediator::GetWorldNameByIndex(+0xfc index=0x{:06x}) -> '{}' [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        worldName ? worldName : "<null>",
        worldName ? "owner+0xd84.inlineName+0x03" : "no-active-descriptor-table");
    return worldName;
}

// anchor: launcher.exe:0x41b320 / launcher.exe:0x4d3584 +0x100
// vtable: ILTLoginMediator.Default slot +0x100
// Active replacement path note:
// - the current text-mode selection menu only reaches this after auth success
// - so the wrapper-facing gate byte now comes only from the recovered owner descriptor Status byte
uint8_t CLTLoginMediator::GetWorldSelectionGateByteByIndex(uint32_t index) const {
    const uint8_t selectionGateByte100 =
        (lastAuthReply_.valid && !lastAuthReply_.isErrorReply && index <= 0xffu)
            ? GetDescriptorStatusByIndex(static_cast<uint8_t>(index))
            : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldSelectionGateByteByIndex(+0x100 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(selectionGateByte100),
        (selectionGateByte100 != 0u) ? "owner+0xd84.status+0x17" : "no-active-descriptor-table");
    return selectionGateByte100;
}

// anchor: launcher.exe:0x41b360
// vtable: ILTLoginMediator.Default slot +0x104
// Corrected off-by-one read from Ghidra/disassembly: this wrapper slot now surfaces owner
// descriptor Type byte `+0x18`, not server-version low byte.
uint8_t CLTLoginMediator::GetWorldTypeByteByIndex(uint32_t index) const {
    const bool useRecoveredDescriptorTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;
    const uint8_t worldTypeByte = useRecoveredDescriptorTable
        ? ((index <= 0xffu) ? GetDescriptorTypeByIndex(static_cast<uint8_t>(index)) : 0u)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldTypeByteByIndex(+0x104 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(worldTypeByte),
        useRecoveredDescriptorTable ? "owner+0xd84.type+0x18" : "no-startup-fallback");
    return worldTypeByte;
}

// anchor: launcher.exe:0x41b3a0
// vtable: ILTLoginMediator.Default slot +0x108
uint8_t CLTLoginMediator::GetWorldPopulationNibbleByIndex(uint32_t index) const {
    const bool useRecoveredDescriptorTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;
    const uint8_t populationNibble = useRecoveredDescriptorTable
        ? ((index <= 0xffu) ? GetDescriptorPopulationNibbleByIndex(static_cast<uint8_t>(index)) : 0u)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldPopulationNibbleByIndex(+0x108 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(populationNibble),
        useRecoveredDescriptorTable ? "owner+0xd84.population+0x1f.low4" : "no-startup-fallback");
    return populationNibble;
}

// anchor: launcher.exe:0x41f2c0 slot +0x10c
RouteDescriptor30SmallStringLikeSketch* CLTLoginMediator::GetRouteDescriptor30() {
    // Keep the wrapper-facing arg6 `+0x10c` small-string object explicit.
    // The owner-side route-text resolution still lives in `ResolveMarginRouteDescriptor()`.
    const char* routeDescriptor = ResolveMarginRouteDescriptor();
    routeDescriptor30Owned_ = routeDescriptor ? routeDescriptor : "";
    routeDescriptor30_.begin = routeDescriptor30Owned_.c_str();
    routeDescriptor30_.current = routeDescriptor30_.begin + routeDescriptor30Owned_.size();
    routeDescriptor30_.capacity = routeDescriptor30_.current;

    spdlog::info(
        "CLTLoginMediator::GetRouteDescriptor30(+0x10c) -> begin={} current={} text='{}'",
        fmt::ptr(routeDescriptor30_.begin),
        fmt::ptr(routeDescriptor30_.current),
        routeDescriptor30Owned_.empty() ? "<empty>" : routeDescriptor30Owned_.c_str());
    return &routeDescriptor30_;
}

// anchor: launcher.exe:0x41af50 +0x118
LateEntryList1470VectorLikeSketch* CLTLoginMediator::GetLateEntryList1470() {
    return &lateEntryList1470_;
}

// anchor: launcher.exe:0x41c3c0 +0x120
uint32_t CLTLoginMediator::ProcessCreateCharacterInput120(const ProcessCreateCharacterInput120Sketch& input) {
    // anchor: launcher.exe:0x41c3c0
    // Later post-auth writer for owner `+0x108/+0x12c/+0x134..+0x1b8`.
    // Current wrapper-slot decision from `client.dll:0x62054d1d` + owner `0x41c3c0` disassembly:
    // - same semantic slot on the wrapper and owner sides
    // - wrapper caller builds a larger stack object, but the offsets initialized there line up
    //   with the owner-side reader instead of describing a separate slot family
    // - like the neighboring `0x41c390/0x41c1f0` pair, this remains an owner-side method gated by
    //   current state code `3`; do not treat that gate as proof of a state3-local slot body
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 0u:
        case 1u:
        case 2u:
            spdlog::info(
                "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected currentState={} stateCode={} -> 0x12000006",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 0x12000006u;
        case 3u:
            break;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            spdlog::info(
                "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected currentState={} stateCode={} -> 0x12000000",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 0x12000000u;
        case 12u:
            spdlog::info(
                "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected currentState={} stateCode={} -> 0x12000007",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 0x12000007u;
        default:
            spdlog::info(
                "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected currentState={} stateCode={} -> 0x00000001",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 1u;
    }

    if (static_cast<uint32_t>(worldDescriptorCountD80_) < input.field24) {
        spdlog::info(
            "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected selector field12c=0x{:08x} upperBoundF8=0x{:02x}",
            static_cast<unsigned>(input.field24),
            static_cast<unsigned>(worldDescriptorCountD80_));
        return 4u;
    }

    MirrorCreateCharacterInput120SourceBlock(input);

    const CLTLoginState* const oldState = currentState_;
    uint32_t state10EntryResult = 0u;
    if (LoginHelperStateByIdScaffold(10u) != nullptr) {
        // anchor: launcher.exe:0x41c3c0 -> 0x41b450(10)
        // Like the neighboring owner-side state writers, the original helper switch does not stop
        // at plain `currentState = state10`; the active continuation needs the immediate state10
        // slot-3 claim-name send (`0x43bf90`) to start `MS_ClaimCharacterNameRequest`.
        state10EntryResult = SetCurrentState(10u);
    }

    spdlog::info(
        "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120 owner) name='{}' field12c=0x{:08x} firstDword134=0x{:08x} backgroundPreview='{}' oldState={} currentState={} state10EntryResult=0x{:08x}",
        postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00[0]
            ? postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.data()
            : "<empty>",
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24),
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.createCharacterData108.header2c[0]),
        postAuthMarginLoadingState_0xf14.createCharacterData108.backgroundB0[0]
            ? postAuthMarginLoadingState_0xf14.createCharacterData108.backgroundB0.data()
            : "<empty>",
        oldState ? oldState->DebugName() : "<null>",
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(state10EntryResult));
    return 0u;
}

// Focused late-login/state9 split:
// - `loginmediator_state9.cpp` now keeps only the mediator-owned state9 methods
// - callback84/object88/submit-helper detail lives in
//   `loginmediator_state9_submit_scaffold.h`
// - canonical docs:
//   - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
//   - `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
// - this keeps future INetMgr.Default / CUDPDriver::JoinSession work scoped to the active
//   late-login surface instead of forcing rereads of broader mediator/auth files

// wrapper-facing slot +0x124 startup triple capture; owner-side mirror remains explicit below.
void CLTLoginMediator::ProvideStartupTriple(void* netShell, void* netMgr, void* distrObjExecutive) {
    provideStartupTripleNetShell_ = netShell;
    provideStartupTripleNetMgr_ = netMgr;
    provideStartupTripleDistrObjExecutive_ = distrObjExecutive;
    ++provideStartupTripleCount_;

    // Keep the wrapper-facing capture and the owner-side submit mirror unified on the mediator.
    SetState9CallbackObjectTriple84_88_8c(netShell, netMgr, distrObjExecutive);

    spdlog::info(
        "CLTLoginMediator::ProvideStartupTriple(+0x124 wrapper-facing netShell={} netMgr={} distrObjExecutive={} [count={}] ownerMirror=+0x84/+0x88/+0x8c)",
        fmt::ptr(provideStartupTripleNetShell_),
        fmt::ptr(provideStartupTripleNetMgr_),
        fmt::ptr(provideStartupTripleDistrObjExecutive_),
        provideStartupTripleCount_);
}

// anchor: launcher.exe:0x41f310 slot +0x130
[[maybe_unused]] LaunchPadClient_0x4b0e48* CLTLoginMediator::GetLaunchPadClient65c() const {
    // Tiny owner-vtable getter used by the later session-callback helper family.
    return launchPadClient65c_;
}

// anchor: launcher.exe:0x420d00 +0x134
// Lazy allocator for mediator owner +0x65c session callback helper.
// Allocates and initializes a LaunchPadClient_0x4b0e48 when null.
LaunchPadClient_0x4b0e48* CLTLoginMediator::EnsureLaunchPadClient65c() {
    if (launchPadClient65c_ == nullptr) {
        // Fidelity: original allocates 0x30 bytes with malloc
        launchPadClient65c_ = new LaunchPadClient_0x4b0e48();
        launchPadClient65c_->currentState10 = this;
    }
    return launchPadClient65c_;
}

// anchor: launcher.exe:0x4202c0 +0x13c
void CLTLoginMediator::HelperSlot13c_InvokeSessionHelperVtable4() {
    if (launchPadClient65c_ == nullptr) {
        spdlog::debug(
            "CLTLoginMediator::HelperSlot13c_InvokeSessionHelperVtable4(+0x13c) skipped (no helper)");
        return;
    }
    // Fidelity: original loads helper from this+0x65c, tests null, calls through helper vtable[+0x04]
    // Our virtual method provides the same dispatch
    launchPadClient65c_->InvokeVtableSlot4();
}

// anchor: launcher.exe:0x41f320 +0x148
const char* CLTLoginMediator::GetGameSessionId() const {
    // Important fidelity correction from fresh original-launcher WineDbg on the natural first
    // state8 send:
    // - original `0x41f320` returns owner `this + 0x664` directly
    // - the caller then forwards that pointer into `0x43ada0` even when the string is empty
    // So this getter must preserve the original non-null empty-string behavior instead of
    // collapsing empty state to nullptr.
    return gameSessionId664_.c_str();
}

// anchor: launcher.exe:0x41f330 +0x14c
void CLTLoginMediator::SetSharedMarginPacketField660(uint32_t value) {
    sharedMarginPacketField660_ = value;
}

// anchor: launcher.exe:0x41c510 +0x158
[[maybe_unused]] uint32_t CLTLoginMediator::SetState9OptionalField90AndSwitchToState13(uint32_t field90Value) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 0u:
        case 1u:
        case 2u:
        case 3u:
            return 0x12000006u;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            return 0x12000000u;
        case 12u:
            ownerOptionalField90_ = field90Value;
            SetCurrentState(0x0du);
            spdlog::info(
                "CLTLoginMediator::SetState9OptionalField90AndSwitchToState13 stored owner+0x90=0x{:08x} currentState={}",
                static_cast<unsigned>(ownerOptionalField90_),
                currentState_ ? currentState_->DebugName() : "<unchanged>");
            return 0u;
        default:
            return 1u;
    }
}

// anchor: launcher.exe:0x41b3f0 / owner vtable +0x164
// Wrapper-facing teardown meaning:
// - launcher `0x40b360` waits for event `1` only when this returns true
// - tiny slot body uses owner auth-connection state (`+0x18`, `+0x2c`)
// - keep that explicit instead of conflating it with the margin/state9 `+0x16c` family
// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::RequestAuthConnectionCloseWaitEvent1() {
    if (!authConnection_) {
        spdlog::info(
            "CLTLoginMediator::RequestAuthConnectionCloseWaitEvent1(+0x164 wrapper-facing) -> 0 [authConnection=<null> owner+0x2c={}]",
            static_cast<unsigned>(authConnectionFlag2c_));
        return false;
    }

    authConnectionFlag2c_ = 1u;
    const uint32_t rawState = static_cast<uint32_t>(authConnection_->State());
    const bool wouldCallConnectionClose0c =
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kConnectActive) ||
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);
    const uint32_t closeResult = wouldCallConnectionClose0c
        ? authConnection_->Close(/*graceful=*/true)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::RequestAuthConnectionCloseWaitEvent1(+0x164 wrapper-facing) -> 1 [owner+0x2c={} authConnectionState={} wouldCallConnectionClose0cArg1={} closeResult=0x{:08x}]",
        static_cast<unsigned>(authConnectionFlag2c_),
        rawState,
        wouldCallConnectionClose0c ? 1u : 0u,
        static_cast<unsigned>(closeResult));
    return true;
}

// anchor: launcher.exe teardown path 0x40b360..0x40b3df / wrapper-facing arg6 slot +0x16c
// Keep the split explicit:
// - launcher teardown treats this as a close-and-wait-event-`0x0f` predicate
// - the same underlying owner body is also the state9 success-side helper anchored below at
//   `0x41b420`
bool CLTLoginMediator::RequestMarginConnectionCloseWaitEvent0f() {
    if (!marginConnection_) {
        return false;
    }
    // anchor: launcher.exe:0x41b42c / clear owner+0xf14
    postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 0u;
    // anchor: launcher.exe:0x41b433 / set owner+0x2d
    marginConnectionFlag2d_ = 1u;
    // anchor: launcher.exe:0x41b437 / query margin connection state at +0x34
    const uint32_t rawState = static_cast<uint32_t>(marginConnection_->State());
    // anchor: launcher.exe:0x41b43a-0x41b448 / state check (1 or 2) and vtable+0x0c(1) call
    const uint32_t closeResult =
        (rawState == 1 || rawState == 2) && marginConnection_
            ? marginConnection_->Close(/*graceful=*/true)
            : 0u;

    spdlog::info(
        "CLTLoginMediator::RequestMarginConnectionCloseWaitEvent0f(+0x16c wrapper-facing) -> 1 [owner+0xf14={} owner+0x2d={} marginConnectionState={} wouldCallConnectionClose0cArg1={} closeResult=0x{:08x} currentState={} split=teardown-wait-event-0x0f vs owner-state9-success-helper laterExpectedTail=0x41afc0->0x438df0->0x41cfb0(0x0f)]",
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.state10SendGateFlagF14),
        static_cast<unsigned>(marginConnectionFlag2d_),
        rawState,
        (rawState == 1 || rawState == 2) ? 1u : 0u,
        static_cast<unsigned>(closeResult),
        currentState_ ? currentState_->DebugName() : "<null>");
    return true;
}

// anchor: launcher.exe:0x41ddb0 slot +0x170
bool CLTLoginMediator::RegisterLoginObserver(void* observer) {
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x170` as insertion into the owner `+0x674` listener tree, not as a startup
    // context handoff.
    if (!observer) {
        return false;
    }

    latestObserver170_ = observer;

    const bool inserted = InsertObserverNode674(observer);
    const bool returnValue = !inserted;
    if (!inserted) {
        spdlog::info(
            "CLTLoginMediator::RegisterLoginObserver observer={} already registered treeCount={} header={} root={} leftmost={} rightmost={} returnValue={} (0x41ddb0 returns !insertedFlag from the helper result pair)",
            fmt::ptr(observer),
            static_cast<unsigned>(observerTree674_.nodeCount04),
            fmt::ptr(observerTree674_.header00),
            fmt::ptr(observerTreeHeader674_.parent04),
            fmt::ptr(observerTreeHeader674_.left08),
            fmt::ptr(observerTreeHeader674_.right0c),
            returnValue ? 1u : 0u);
        return returnValue;
    }

    spdlog::info(
        "CLTLoginMediator::RegisterLoginObserver observer={} treeCount={} header={} root={} leftmost={} rightmost={} inserted={} returnValue={} (source-owned std::_Tree-like owner+0x674 bridge active)",
        fmt::ptr(observer),
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c),
        inserted ? 1u : 0u,
        returnValue ? 1u : 0u);
    return returnValue;
}

// anchor: launcher.exe:0x41dde0 slot +0x174
bool CLTLoginMediator::UnregisterLoginObserver(void* observer) {
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x174` as removal from the owner `+0x674` listener tree.
    if (!observer) {
        return false;
    }

    latestObserver174_ = observer;

    LoginObserverTreeNode674* lowerBound = nullptr;
    LoginObserverTreeNode674* upperBound = nullptr;
    EqualRangeObserver674(observer, &lowerBound, &upperBound);
    const uint32_t rangeCount = LoginObserverTreeHelper674::CountRange(lowerBound, upperBound);
    EraseObserverRange674(lowerBound, upperBound);
    const bool returnValue = (rangeCount == 0u);

    spdlog::info(
        "CLTLoginMediator::UnregisterLoginObserverScaffold observer={} rangeCount={} treeCount={} header={} root={} leftmost={} rightmost={} returnValue={} (0x41dde0 mirrors equal_range + distance + erase_range and returns rangeCount==0)",
        fmt::ptr(observer),
        static_cast<unsigned>(rangeCount),
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c),
        returnValue ? 1u : 0u);
    return returnValue;
}

// anchor: launcher.exe:0x41f240 slot +0x178
uint32_t CLTLoginMediator::GetLastLoginStatus() {
    // Keep this wrapper-facing slot as close as practical to the original tiny getter
    // `0x41f240: mov eax, [ecx+0x80] ; ret`.
    return worldListCountOrStatus80;
}







// anchor: launcher.exe:0x41af80 / owner vtable `+0x17c`
uint32_t CLTLoginMediator::HandleAuthConnectionCompletionFallback(void* connection, void* workItem) {
    // anchor: launcher.exe:0x41af89
    // Reject anything except the live owner `+0x18` auth connection.
    if (connection != authConnection_) {
        return 0u;
    }

    const auto* workHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    if (workHeader->workType == mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        // anchor: launcher.exe:0x41afa5
        authConnection_ = nullptr;
    }

    // anchor: launcher.exe:0x41afac / current helper vtable `+0x00`
    return currentState_->Slot1_HandlePrimaryGate(workItem);
}

// anchor: launcher.exe:0x41f250 / owner vtable `+0x180`
uint32_t CLTLoginMediator::DispatchCurrentHelperAuthMessage(void* workItem) {
    // Exact launcher wrapper body:
    // - mov ecx, [ecx+0x10]
    // - mov eax, [ecx]
    // - jmp [eax+0x10]
    return currentState_->AuthMessageDispatch(workItem);
}

// anchor: launcher.exe:0x41f260 / owner vtable `+0x184`
uint32_t CLTLoginMediator::DispatchCurrentHelperSlot6(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    // Exact launcher wrapper body:
    // - mov ecx, [ecx+0x10]
    // - mov eax, [ecx]
    // - jmp [eax+0x14]
    return currentState_->Slot6_HandleSecondaryMessage(workItem);
}

// anchor: launcher.exe:0x41afc0 / owner vtable `+0x188`
uint32_t CLTLoginMediator::HandleMarginConnectionCompletionFallback(void* connection, void* workItem) {
    // anchor: launcher.exe:0x41afc9
    if (connection != marginConnection_) {
        return 0u;
    }

    const auto* workHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    if (workHeader->workType == mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        // anchor: launcher.exe:0x41afe7
        // launcher.exe also zeroes owner `+0x20` here; keep that discrepancy documented until the
        // source layout grows a proven home for it.
        marginConnection_ = nullptr;
    }

    // anchor: launcher.exe:0x41afed / current helper vtable `+0x04`
    return currentState_->Slot2_HandleSecondaryGate(workItem);
}

// anchor: launcher.exe:0x41e690
uint32_t CLTLoginMediator::FillState9CallbackBlob18c(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) {
    if (!outDwords) {
        return 1u;
    }

    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    if (stateCode != 9u) {
        return 0x12000009u;
    }

    const SlotRecordState_0x4b5328* currentSlotRecord = GetCurrentSlotRecord();
    if (!currentSlotRecord) {
        std::memset(outDwords, 0, 0x20u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18c missing current slot record while state9-gated; zeroed 0x20-byte blob and returned generic failure");
        return 1u;
    }

    outDwords[0] = currentSlotRecord->characterIdLow32;
    outDwords[1] = currentSlotRecord->characterIdHigh36;
    outDwords[2] = arg2;
    outDwords[3] = arg3;

    // anchor: launcher.exe:0x41e690 / direct field access at vtable +0x18c
    std::array<uint8_t, 16> transformInput{};
    std::memcpy(transformInput.data(), &state6UdpSessionSecretF18_, sizeof(state6UdpSessionSecretF18_));

    std::array<uint8_t, 16> marginTwofishKey{};
    const uint8_t* state9SeedPointer85D4 =
        static_cast<const uint8_t*>(GetState9CallbackSeedPointer85D4());
    if (state9SeedPointer85D4 == nullptr) {
        std::memset(outDwords + 4, 0, 16u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18c missing 16-byte state9 seed from mediator+0xd4; zeroed blob tail ownerF18=0x{:08x}",
            static_cast<unsigned>(state6UdpSessionSecretF18_));
        return 1u;
    }
    std::memcpy(marginTwofishKey.data(), state9SeedPointer85D4, marginTwofishKey.size());
    const uint32_t* seedWords = reinterpret_cast<const uint32_t*>(marginTwofishKey.data());

    mxo::auth::internal::FeedbackSizeTransformAdapterSmall feedbackTransformAdapter;
    if (!feedbackTransformAdapter.FeedbackSizeTransformAdapter_ConstructSmall(
            marginTwofishKey.data(),
            static_cast<uint32_t>(marginTwofishKey.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data(),
            0u) ||
        !feedbackTransformAdapter.FeedbackSizeTransformAdapter_TransformBuffer(
            outDwords + 4,
            transformInput.data(),
            16u)) {
        std::memset(outDwords + 4, 0, 16u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18c Twofish block transform failed ownerF18=0x{:08x}",
            static_cast<unsigned>(state6UdpSessionSecretF18_));
        return 1u;
    }
    spdlog::info(
        "CLTLoginMediator::FillState9CallbackBlob18c built blob currentSlotLow=0x{:08x} currentSlotHigh=0x{:08x} arg2=0x{:08x} arg3=0x{:08x} ownerF18=0x{:08x} seedSource=mediator+0xd4 seed[0..3]=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] tail10=0x{:08x} tail14=0x{:08x} tail18=0x{:08x} tail1c=0x{:08x} (AssemblyTwofish + zero-IV one-block transform over [ownerF18,0,0,0])",
        static_cast<unsigned>(outDwords[0]),
        static_cast<unsigned>(outDwords[1]),
        static_cast<unsigned>(outDwords[2]),
        static_cast<unsigned>(outDwords[3]),
        static_cast<unsigned>(state6UdpSessionSecretF18_),
        static_cast<unsigned>(seedWords[0]),
        static_cast<unsigned>(seedWords[1]),
        static_cast<unsigned>(seedWords[2]),
        static_cast<unsigned>(seedWords[3]),
        static_cast<unsigned>(outDwords[4]),
        static_cast<unsigned>(outDwords[5]),
        static_cast<unsigned>(outDwords[6]),
        static_cast<unsigned>(outDwords[7]));
    return 0u;
}

// anchor: launcher.exe:0x41ecd0
void CLTLoginMediator::ResetSelectionContext0ecMirror() {
    selectionContext0ecCopy_ = {};
    selectionContext0ecCopyValid_ = false;
    ++selection0ecCount_;
    spdlog::info(
        "CLTLoginMediator::ResetSelectionContext0ecMirror cleared selection mirror [count={}]",
        selection0ecCount_);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::MirrorCreateCharacterInput120SourceBlock(const ProcessCreateCharacterInput120Sketch& input) {
    auto& createCharacterData108 = postAuthMarginLoadingState_0xf14.createCharacterData108;
    std::copy(input.string00.begin(), input.string00.end(), createCharacterData108.characterName00.begin());
    createCharacterData108.selectedWorldField24 = input.field24;

    std::copy(input.dwords2c.begin(), input.dwords2c.end(), createCharacterData108.header2c.begin());
    std::copy(input.dwords4c.begin(), input.dwords4c.end(), createCharacterData108.secondary4c.begin());
    createCharacterData108.bodyWord6c =
        static_cast<uint32_t>(input.bytes6c[0]) |
        (static_cast<uint32_t>(input.bytes6c[1]) << 8) |
        (static_cast<uint32_t>(input.bytes6c[2]) << 16) |
        (static_cast<uint32_t>(input.bytes6c[3]) << 24);

    std::copy(input.string70.begin(), input.string70.end(), createCharacterData108.realFirstName70.begin());
    std::copy(input.string90.begin(), input.string90.end(), createCharacterData108.realLastName90.begin());
    std::copy(input.stringB0.begin(), input.stringB0.end(), createCharacterData108.backgroundB0.begin());
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::CaptureCreateCharacterInputArg6Slot120(
    const void* input120,
    void* returnAddress,
    bool applyOwnerSemantics) {
    arg6CreateCharacterInput120_ = input120;
    ++arg6CreateCharacterInputCount120_;

    if (!input120) {
        spdlog::info(
            "CLTLoginMediator::CaptureCreateCharacterInputArg6Slot120(+0x120) input=<null> caller={} [count={}] applyOwnerSemantics={}",
            fmt::ptr(returnAddress),
            arg6CreateCharacterInputCount120_,
            applyOwnerSemantics ? 1u : 0u);
        return 1u;
    }

    const auto& input = *static_cast<const ProcessCreateCharacterInput120Sketch*>(input120);
    if (!applyOwnerSemantics) {
        MirrorCreateCharacterInput120SourceBlock(input);
        spdlog::info(
            "CLTLoginMediator::CaptureCreateCharacterInputArg6Slot120(+0x120 mirror-only input={} caller={} [count={}] field12c=0x{:08x} name='{}')",
            fmt::ptr(input120),
            fmt::ptr(returnAddress),
            arg6CreateCharacterInputCount120_,
            static_cast<unsigned>(postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24),
            postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00[0]
                ? postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.data()
                : "<empty>");
        return 0u;
    }

    spdlog::debug(
        "CLTLoginMediator::CaptureCreateCharacterInputArg6Slot120(+0x120 owner-dispatch input={} caller={} [count={}])",
        fmt::ptr(input120),
        fmt::ptr(returnAddress),
        arg6CreateCharacterInputCount120_);
    return ProcessCreateCharacterInput120(input);
}

// Post-auth margin/loading state ownership (`launcher.exe:0x4f78b8`) shared by the later
// state11 send/reply path and the active existing-character path.

// anchor: launcher.exe:0x41b4b0
bool CLTLoginMediator::State10HasReadyConnectionState2() const {
    // Exact recovered gate from `0x41b4b0`:
    // - owner `+0x1c` must be non-null
    // - connection state field `+0x34` must equal `2`
    const mxo::liblttcp::CMessageConnection_0x4b7928* connection = MarginConnection();
    return connection != nullptr &&
           connection->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// anchor: launcher.exe:0x41f070
const uint32_t* CLTLoginMediator::GetNoPatchLauncherVersionValuePtr08() const {
    return &nopatchLauncherVersionValue08_;
}

// anchor: launcher.exe:0x41f090
const uint32_t* CLTLoginMediator::GetNoPatchClientVersionValuePtr0c() const {
    return &nopatchClientVersionValue0c_;
}

namespace {

static size_t LateEntryList1470EntryCountScaffold(const LateEntryList1470VectorLikeSketch& list) {
    return (list.begin != nullptr && list.current != nullptr && list.current >= list.begin)
        ? static_cast<size_t>(list.current - list.begin)
        : 0u;
}

static void LateEntryList1470ResetEntryScaffold(LateEntryList1470EntrySketch* entry) {
    if (!entry) {
        return;
    }
    entry->begin = nullptr;
    entry->current = nullptr;
    entry->capacity = nullptr;
}

// anchor: launcher.exe:0x41e410 = StringTripleArray_DestroyRange
static void LateEntryList1470DestroyRangeScaffold(
    LateEntryList1470EntrySketch* begin,
    LateEntryList1470EntrySketch* end) {
    if (begin == nullptr || end == nullptr || end < begin) {
        return;
    }

    for (LateEntryList1470EntrySketch* entry = begin; entry != end; ++entry) {
        if (entry->begin != nullptr) {
            std::free(entry->begin);
        }
        LateEntryList1470ResetEntryScaffold(entry);
    }
}

static bool LateEntryList1470CopyConstructSingleScaffold(
    LateEntryList1470EntrySketch* destination,
    const LateEntryList1470EntrySketch* source) {
    if (destination == nullptr || source == nullptr || source->begin == nullptr ||
        source->current == nullptr || source->current < source->begin) {
        return false;
    }

    const size_t stringLength = static_cast<size_t>(source->current - source->begin);
    char* const ownedCopy = static_cast<char*>(std::malloc(stringLength + 1u));
    if (ownedCopy == nullptr) {
        return false;
    }

    if (stringLength != 0u) {
        std::memcpy(ownedCopy, source->begin, stringLength);
    }
    ownedCopy[stringLength] = '\0';
    destination->begin = ownedCopy;
    destination->current = ownedCopy + stringLength;
    destination->capacity = ownedCopy + stringLength + 1u;
    return true;
}

// anchor: launcher.exe:0x41d750 = StringTripleArray_CopyConstructRange
static LateEntryList1470EntrySketch* LateEntryList1470CopyConstructRangeScaffold(
    const LateEntryList1470EntrySketch* sourceBegin,
    const LateEntryList1470EntrySketch* sourceEnd,
    LateEntryList1470EntrySketch* destinationBegin) {
    LateEntryList1470EntrySketch* destination = destinationBegin;
    for (const LateEntryList1470EntrySketch* source = sourceBegin; source != sourceEnd;
         ++source, ++destination) {
        LateEntryList1470ResetEntryScaffold(destination);
        if (!LateEntryList1470CopyConstructSingleScaffold(destination, source)) {
            LateEntryList1470DestroyRangeScaffold(destinationBegin, destination);
            return nullptr;
        }
    }
    return destination;
}

}  // namespace

void CLTLoginMediator::FreeLateEntryList1470StorageScaffold() {
    LateEntryList1470DestroyRangeScaffold(lateEntryList1470_.begin, lateEntryList1470_.current);
    if (lateEntryList1470_.begin != nullptr) {
        std::free(lateEntryList1470_.begin);
    }
    lateEntryList1470_.begin = nullptr;
    lateEntryList1470_.current = nullptr;
    lateEntryList1470_.capacity = nullptr;
}

// anchor: launcher.exe:0x407dd0 = StringTriple_AssignFromRange
// Narrow source-owned bridge over the original per-entry assignment helper.
static bool LateEntryList1470AssignFromRangeScaffold(
    LateEntryList1470EntrySketch* destination,
    const char* sourceBegin,
    const char* sourceEnd) {
    if (destination == nullptr || sourceBegin == nullptr || sourceEnd == nullptr ||
        sourceEnd < sourceBegin) {
        return false;
    }

    if (destination->begin != nullptr) {
        std::free(destination->begin);
    }
    LateEntryList1470ResetEntryScaffold(destination);

    LateEntryList1470EntrySketch sourceEntry{};
    sourceEntry.begin = const_cast<char*>(sourceBegin);
    sourceEntry.current = const_cast<char*>(sourceEnd);
    sourceEntry.capacity = const_cast<char*>(sourceEnd);
    return LateEntryList1470CopyConstructSingleScaffold(destination, &sourceEntry);
}

// anchor: launcher.exe:0x41eb20 = StringTripleArray_CopyAssignRange
static LateEntryList1470EntrySketch* StringTripleArray_CopyAssignRangeScaffold(
    LateEntryList1470EntrySketch* sourceBegin,
    LateEntryList1470EntrySketch* sourceEnd,
    LateEntryList1470EntrySketch* destinationBegin) {
    LateEntryList1470EntrySketch* destination = destinationBegin;
    for (LateEntryList1470EntrySketch* source = sourceBegin; source != sourceEnd;
         ++source, ++destination) {
        if (source != destination) {
            LateEntryList1470AssignFromRangeScaffold(destination, source->begin, source->current);
        }
    }
    return destination;
}

// anchor: launcher.exe:0x41d7a0 = StringTripleArray_CopyConstructRepeatedEntry
static LateEntryList1470EntrySketch* StringTripleArray_CopyConstructRepeatedEntryScaffold(
    LateEntryList1470EntrySketch* destinationBegin,
    size_t repeatCount,
    const LateEntryList1470EntrySketch* sourceEntry) {
    LateEntryList1470EntrySketch* destination = destinationBegin;
    for (; repeatCount != 0u; --repeatCount, ++destination) {
        LateEntryList1470ResetEntryScaffold(destination);
        if (!LateEntryList1470CopyConstructSingleScaffold(destination, sourceEntry)) {
            LateEntryList1470DestroyRangeScaffold(destinationBegin, destination);
            return nullptr;
        }
    }
    return destination;
}

// anchor: launcher.exe:0x41f3e0 = StringTripleArray_GrowAndAppend
static bool StringTripleArray_GrowAndAppendScaffold(
    LateEntryList1470VectorLikeSketch* vectorHeader,
    LateEntryList1470EntrySketch* insertPosition,
    const LateEntryList1470EntrySketch* sourceEntry,
    size_t requestedInsertCount,
    bool preservePrefix) {
    const size_t existingEntryCount = LateEntryList1470EntryCountScaffold(*vectorHeader);
    const size_t growthCount = std::max(existingEntryCount, requestedInsertCount);
    const size_t newCapacity = existingEntryCount + growthCount;
    LateEntryList1470EntrySketch* const grownBegin =
        static_cast<LateEntryList1470EntrySketch*>(
            std::calloc(newCapacity, sizeof(LateEntryList1470EntrySketch)));
    if (grownBegin == nullptr) {
        return false;
    }

    LateEntryList1470EntrySketch* grownCurrent =
        LateEntryList1470CopyConstructRangeScaffold(vectorHeader->begin, insertPosition, grownBegin);
    if (grownCurrent == nullptr) {
        std::free(grownBegin);
        return false;
    }

    if (requestedInsertCount == 1u) {
        LateEntryList1470ResetEntryScaffold(grownCurrent);
        if (!LateEntryList1470CopyConstructSingleScaffold(grownCurrent, sourceEntry)) {
            LateEntryList1470DestroyRangeScaffold(grownBegin, grownCurrent);
            std::free(grownBegin);
            return false;
        }
        ++grownCurrent;
    } else {
        LateEntryList1470EntrySketch* const postInsert =
            StringTripleArray_CopyConstructRepeatedEntryScaffold(
                grownCurrent,
                requestedInsertCount,
                sourceEntry);
        if (postInsert == nullptr) {
            LateEntryList1470DestroyRangeScaffold(grownBegin, grownCurrent);
            std::free(grownBegin);
            return false;
        }
        grownCurrent = postInsert;
    }

    if (!preservePrefix) {
        LateEntryList1470EntrySketch* const postTail = LateEntryList1470CopyConstructRangeScaffold(
            insertPosition,
            vectorHeader->current,
            grownCurrent);
        if (postTail == nullptr) {
            LateEntryList1470DestroyRangeScaffold(grownBegin, grownCurrent);
            std::free(grownBegin);
            return false;
        }
        grownCurrent = postTail;
    }

    LateEntryList1470DestroyRangeScaffold(vectorHeader->begin, vectorHeader->current);
    if (vectorHeader->begin != nullptr) {
        std::free(vectorHeader->begin);
    }

    vectorHeader->begin = grownBegin;
    vectorHeader->current = grownCurrent;
    vectorHeader->capacity = grownBegin + newCapacity;
    return true;
}

// anchor: launcher.exe:0x41f640 = StringTripleArray_Append
static bool StringTripleArray_AppendScaffold(
    LateEntryList1470VectorLikeSketch* vectorHeader,
    const LateEntryList1470EntrySketch* sourceEntry) {
    if (vectorHeader == nullptr || sourceEntry == nullptr || sourceEntry->begin == nullptr ||
        sourceEntry->current == nullptr || sourceEntry->current < sourceEntry->begin) {
        return false;
    }

    LateEntryList1470EntrySketch* const insertPosition = vectorHeader->current;
    if (insertPosition != vectorHeader->capacity) {
        if (insertPosition == nullptr) {
            return false;
        }
        LateEntryList1470ResetEntryScaffold(insertPosition);
        if (!LateEntryList1470CopyConstructSingleScaffold(insertPosition, sourceEntry)) {
            return false;
        }
        vectorHeader->current = insertPosition + 1;
        return true;
    }

    return StringTripleArray_GrowAndAppendScaffold(
        vectorHeader,
        insertPosition,
        sourceEntry,
        1u,
        true);
}

// anchor: launcher.exe:0x41f5f0
void CLTLoginMediator::ClearLateEntryList1470Scaffold() {
    LateEntryList1470EntrySketch* const newCurrent = StringTripleArray_CopyAssignRangeScaffold(
        lateEntryList1470_.current,
        lateEntryList1470_.current,
        lateEntryList1470_.begin);
    LateEntryList1470DestroyRangeScaffold(newCurrent, lateEntryList1470_.current);
    lateEntryList1470_.current = newCurrent;
}

// anchor: launcher.exe:0x41f840 / owner vtable +0x190
void CLTLoginMediator::AppendLateEntryStringTriple1470Scaffold(
    const LateEntryList1470EntrySketch* sourceEntry) {
    (void)StringTripleArray_AppendScaffold(&lateEntryList1470_, sourceEntry);
}

// anchor: launcher.exe:0x41af70
// Original is a thin thunk:
//   - loads margin connection from this+0x1c
//   - jumps to connection->vtable[+0x24] (CMessageConnection_SendPacket)
// which then chains to vtable[+0x28] (ForwardEnvelopeToSendPacket -> SendPacketMessageRef)
// Current source preserves envelope-based send bridge for packet-builder integration.
uint32_t CLTLoginMediator::SendCurrentMarginPacket(
    mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& envelope) {
    mxo::liblttcp::CMessageConnection_0x4b7928* const connection = MarginConnection();
    if (!connection) {
        return 0u;
    }

    return connection->ForwardPacketBuilderEnvelopeToSendPacket(envelope);
}

// anchor: launcher.exe:0x41f270
void CLTLoginMediator::SetLaunchPadSourceBlock94FirstString(const char* value) {
    // Fidelity: write to owner+0x94 block as per static-RE
    std::fill(ownerAuthBootstrapSource94_.username00.begin(), ownerAuthBootstrapSource94_.username00.end(), '\0');
    if (!value) {
        return;
    }

    const size_t copyCount = std::min(
        std::char_traits<char>::length(value),
        ownerAuthBootstrapSource94_.username00.size() - 1);
    std::copy_n(value, copyCount, ownerAuthBootstrapSource94_.username00.begin());
    ownerAuthBootstrapSource94_.username00[copyCount] = '\0';
}

// anchor: launcher.exe:0x420e70
// Fidelity note: original function directly reads/writes string at +0x18 (owned by auth connection),
// copies to +0x664 (gameSessionId), clears +0x18, resets marginBeginCount24_=0, then calls
// AuthenticatePending state[2] vtable[+0x1c](2). Does NOT use session callback helper.
void CLTLoginMediator::CommitSessionCallbackHelperGameSessionId664() {
    // +0x2d: marginConnectionFlag2d_ / marginConnectionCloseWaitEvent0fGateArmed_2d
    if (marginConnectionFlag2d_ != 0) {
        // Flag set - defer path: call through some helper at +0x4
        // meth_0x488330(param_1=authConnection_, param_2=0x10, param_3=0)
        // This path just returns early without committing
        return;
    }

    // Original reads string directly from +0x18 - in original this is the owner session token string
    // that gets cleared after copy. In our layout, there's a StringTriple at +0x18.
    // For fidelity, we read from the original location and copy to gameSessionId664_ (+0x664).
    // Note: The original launcher uses StringTriple_AssignFromRange for the copy.

    // Access +0x18 string: in original this is a StringTriple (session token from auth)
    // We'll read what's at +0x18 and treat it as a string.
    // Current best mirror: the owner session token from auth bootstrap at owner+0x94+0x60
    // But for exact static-RE fidelity, read directly from +0x18 as the original does.

    // For now, use ownerAuthBootstrapSource94_.sessionToken60 which maps to the original source
    // Note: This differs slightly from static-RE which reads from +0x18 directly, but +0x18 in
    // our layout is occupied by authConnection_ pointer (CMessageConnection_0x4b7928*). Need Ghidra
    // to confirm if original +0x18 is actually the string or something else.
    // For now, use the best source-side mirror of the data that should be at +0x18.
    const char* sessionString = ownerAuthBootstrapSource94_.sessionToken60.begin;
    if (sessionString == nullptr || sessionString[0] == '\0') {
        // Try fallback: empty string if no session token
        gameSessionId664_.clear();
    } else {
        gameSessionId664_ = sessionString;
    }

    marginBeginCount24_ = 0;

    spdlog::info(
        "CLTLoginMediator::CommitSessionCallbackHelperGameSessionId664 GameSessionID='{}' marginBeginCount24_=0",
        gameSessionId664_.empty() ? "<empty>" : gameSessionId664_.c_str());

    // Call AuthenticatePending state[2] vtable slot1 (+0x1c) with arg 2
    CLTLoginState* state2 = LoginHelperStateByIdScaffold(2u);
    if (state2 != nullptr) {
        state2->Slot1_HandlePrimaryGate(reinterpret_cast<void*>(2));
    }
}

// source-owned shared helper for `CLTLoginState_State18` slot 3 / `0x421a50`
// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::RefreshSessionHelperGameSessionId664FromSourceBlock94() {
    // Current best source-owned mirror of the alternate state18 session-helper path:
    // - ensure owner helper `+0x65c`
    // - refresh helper string `+0x18` from owner `+0x94 + 0x60`
    // - then commit that helper string into owner `+0x664`
    // - keep this separate from the active state2 -> owner+0x680 bootstrap-child handoff

    // Fidelity: read session token from owner+0x94 block
    if (ownerAuthBootstrapSource94_.sessionToken60.begin != nullptr && ownerAuthBootstrapSource94_.sessionToken60.begin[0] != '\0') {
        gameSessionId664_ = ownerAuthBootstrapSource94_.sessionToken60.begin;
    } else {
        gameSessionId664_.clear();
    }

    marginBeginCount24_ = 0;

    // Call AuthenticatePending state[2] vtable slot1 (+0x1c) with arg 2
    CLTLoginState* state2 = LoginHelperStateByIdScaffold(2u);
    if (state2 != nullptr) {
        state2->Slot1_HandlePrimaryGate(reinterpret_cast<void*>(2));
    }
}

// =============================================================================
// ARG7 SELECTION RESOLUTION (ILTLoginMediator sibling object at 0x4d3584)
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl (vtable access)
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList (world list construction)
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback path)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList / available-world list population
//
// VTABLE METHODS (at offset +0xc from object pointer):
// +0xfc = GetWorldNameByIndex(index) -> char* world name string
// +0x100 = wrapper-facing selection gate byte; on the current active replacement path this now
//          comes directly from owner descriptor Status byte `+0x17`
// +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
// +0xf8 = GetWorldListCount() -> uint total count
// +0xd8 = GetActiveWorldCount() -> uint active count
// +0xe0 = active-world/world-match string getter used by `0x40e480` while pairing the total-world
//         list against the active-world list before writing packed row item-data
// +0xdc = active-world display-name getter used for list column 1 on the matched-row path
//
// ARG7 PACKING FORMAT:
// packedArg7Selection = (high8bits << 24) | low24bits
//   high8bits = variant state from launcher selection data
//   low24bits = world index from GetItemData low bits
// =============================================================================

// Focused arg6/selection split:
// - keep `ILTLoginMediator.Default` world-list/selection scaffolding out of the main mediator TU
// - this lets active auth/state8/state9 work avoid rereading arg6 startup-selection code
// - canonical RE references:
//   - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
//   - late-login arg6 slots `+0xd4/+0x124/+0x18c` live separately under:
//     `../../../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::ConfigureArg6Selection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedVariantState) {
    arg6Selection_.worldUpperBoundExclusive_ = worldUpperBoundExclusive ? worldUpperBoundExclusive : 1u;
    arg6Selection_.variantUpperBoundExclusive_ = variantUpperBoundExclusive ? variantUpperBoundExclusive : 1u;
    arg6Selection_.selectedWorldIndexLow24_ = selectedWorldIndexLow24 & 0x00ffffffu;
    arg6Selection_.selectedVariantIndexHigh8_ = selectedVariantIndexHigh8 & 0xffu;
    arg6Selection_.selectedVariantState_ = selectedVariantState;
    arg6Selection_.mappedSelectionId_ = arg6Selection_.selectedWorldIndexLow24_;
    arg6Selection_.mappedSelectionName_ =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : "standalone";
    arg6Selection_.mappedVariantName_ =
        (mappedVariantName && mappedVariantName[0]) ? mappedVariantName : arg6Selection_.mappedSelectionName_;
}

// Source-owned arg6 bootstrap-selection seed helpers.
// These are replacement-side scaffolds, not recovered launcher.exe vtable methods. They seed the
// startup-side arg6/arg7 bridge while the anchored wrapper-facing readers below stay tied to the
// recovered owner fields.
uint32_t CLTLoginMediator::Arg6WorldUpperBoundExclusive() const {
    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    if (stateCode >= 3u && worldDescriptorCountD80_ != 0u) {
        return static_cast<uint32_t>(worldDescriptorCountD80_);
    }
    return arg6Selection_.worldUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6VariantUpperBoundExclusive() const {
    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    if (stateCode >= 3u) {
        return static_cast<uint32_t>(selectionRouteState684_.slotRecordCount00_);
    }
    return arg6Selection_.variantUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldIndexLow24() const {
    return arg6Selection_.selectedWorldIndexLow24_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantIndexHigh8() const {
    return arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantState() const {
    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    const uint32_t variantIndex = arg6Selection_.selectedVariantIndexHigh8_;
    if (stateCode >= 3u && variantIndex < 100u) {
        return static_cast<uint32_t>(GetSlotRecordStatusByIndex(static_cast<uint8_t>(variantIndex)));
    }
    return arg6Selection_.selectedVariantState_;
}

const char* CLTLoginMediator::Arg6MappedSelectionName() const {
    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    const uint32_t variantIndex = arg6Selection_.selectedVariantIndexHigh8_;
    if (stateCode >= 3u && variantIndex < 100u) {
        if (const char* worldName = LookupRouteHostPrefixBySlot(static_cast<uint8_t>(variantIndex))) {
            return worldName;
        }
    }
    return arg6Selection_.mappedSelectionName_.c_str();
}

const char* CLTLoginMediator::Arg6MappedVariantName() const {
    const uint32_t stateCode = CurrentHelperStateCodeOrZero(this);
    const uint32_t variantIndex = arg6Selection_.selectedVariantIndexHigh8_;
    if (stateCode >= 3u && variantIndex < 100u) {
        if (const char* selectionName = LookupSlotRecordHeapStringByIndex(static_cast<uint8_t>(variantIndex))) {
            return selectionName;
        }
    }
    return arg6Selection_.mappedVariantName_.c_str();
}

const char* CLTLoginMediator::Arg6ProfileName() const {
    // Fidelity: read from owner+0x94 block
    const char* profileName = ownerAuthBootstrapSource94_.username00.data();
    return (profileName && profileName[0] != '\0') ? profileName : arg6Selection_.profileName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthName() const {
    // Fidelity: read from owner+0x94 block
    const char* authName = ownerAuthBootstrapSource94_.username00.data();
    return (authName && authName[0] != '\0') ? authName : arg6Selection_.authName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthPassword() const {
    // Fidelity: read from owner+0x94 block
    const char* authPassword = ownerAuthBootstrapSource94_.password20.data();
    return (authPassword && authPassword[0] != '\0') ? authPassword : arg6Selection_.authPassword_.c_str();
}

bool CLTLoginMediator::Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const {
    return variantIndex == arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6ExpectedSelectionDescriptorScratchRequest() const {
    const uint32_t variantHigh8 = (arg6Selection_.selectedVariantIndexHigh8_ & 0xffu) << 24;
    const uint32_t preservedMiddle16 = arg6Selection_.selectedWorldIndexLow24_ & 0x00ffff00u;
    const uint32_t lowByteOverwrittenWithVariant = arg6Selection_.selectedVariantIndexHigh8_ & 0xffu;
    return variantHigh8 | preservedMiddle16 | lowByteOverwrittenWithVariant;
}

bool CLTLoginMediator::Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const {
    const uint32_t normalizedSelectionIndex = selectionIndex & 0xffffffffu;
    if ((normalizedSelectionIndex & 0x00ffffffu) == arg6Selection_.selectedWorldIndexLow24_) {
        return true;
    }
    return normalizedSelectionIndex == Arg6ExpectedSelectionDescriptorScratchRequest();
}

// anchor: launcher.exe:0x41f2d0 / owner vtable +0x3c
uint32_t CLTLoginMediator::GetDefaultSelectionIndex() const {
    const uint32_t selectionIndex = static_cast<uint32_t>(CurrentCharacterRouteIndexCc8Scaffold());
    if (g_LastLoggedDefaultSelectionIndex3c != selectionIndex) {
        g_LastLoggedDefaultSelectionIndex3c = selectionIndex;
        spdlog::debug(
            "CLTLoginMediator::GetDefaultSelectionIndex(+0x3c) -> 0x{:02x} [source=owner+0xcc8]",
            static_cast<unsigned>(selectionIndex & 0xffu));
    }
    return selectionIndex;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::SendMarginFramedPacket(
    const mxo::auth::FramedPacket& packet,
    uint8_t plainRawCode,
    const char* stepLabel,
    bool encryptedTransport) {
    mxo::liblttcp::CMessageConnection_0x4b7928* connection = MarginConnection();
    if (!connection) {
        connection = EnsureMarginConnectionObject();
    }
    if (!connection || packet.bytes.empty()) {
        return 0u;
    }

    // Keep the current narrower builder contract explicit:
    // - these bootstrap helpers currently materialize already-encrypted/framed bytes
    // - so they still submit through the raw byte-send seam here
    // - moving them onto `0x41af70` cleanly requires splitting out the original raw logical
    //   payload builders first, otherwise the agenda/write path would encrypt them a second time
    const uint32_t sendResult = connection->SendBuffer(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned margin bootstrap send step='{}' rawCode=0x{:02x} transportEncrypted={} outerHeaderLen={} outerPayloadLen={} outerByteCount={} -> sendResult=0x{:08x}",
        (stepLabel && stepLabel[0]) ? stepLabel : "<unnamed>",
        plainRawCode,
        encryptedTransport ? 1u : 0u,
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        sendResult);
    return sendResult;
}



// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::ContinueMarginBootstrapHandshake(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    bool transportEncrypted) {
    if (!payloadBytes || payloadSize == 0u) {
        return 0u;
    }

    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    const uint8_t rawCode = payloadBytes[0];
    switch (rawCode) {
        case 0x02u: {
            spdlog::info(
                "launcher-owned margin bootstrap received CERT_Challenge transportEncrypted={} payloadLen={}",
                transportEncrypted ? 1u : 0u,
                payloadSize);

            // REMOVED from auth-reply path (unfaithful static RE from launcher.exe:0x43f300)
            // Original does inline recovery during margin bootstrap, not pre-recovery at auth-reply success.
            // Do lazy inline recovery if buffer is empty (original recovers inline via margin +0xa0 bootstrap object).
            if (marginBootstrapState.authReplyPrivateExponentBytes.empty()) {
                spdlog::info(
                    "DIAGNOSTIC: lazy inline private exponent recovery for margin CERT_Challenge (original recovers inline from margin +0xa0 bootstrap object)");
                if (!mxo::auth::DecryptAuthReplyPrivateExponent(
                        lastAuthReply_,
                        lastAuthRequestBuildResult_.twofishKeyBytes,
                        lastAuthChallenge_.encryptedChallengeBytes,
                        &marginBootstrapState.authReplyPrivateExponentBytes)) {
                    marginBootstrapState.authReplyPrivateExponentBytes.clear();
                    spdlog::info("DIAGNOSTIC: inline private exponent recovery failed");
                }
            }

            mxo::auth::MarginCertChallenge challenge;
            if (!mxo::auth::ParseMarginCertChallengePayload(
                    payloadBytes,
                    payloadSize,
                    lastAuthReply_.signedData,
                    marginBootstrapState.authReplyPrivateExponentBytes,
                    &challenge)) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin failed to parse CERT_Challenge transportEncrypted={} payloadLen={}",
                    transportEncrypted ? 1u : 0u,
                    payloadSize);
                return 0u;
            }

            marginBootstrapState.marginTwofishKeyBytes = challenge.twofishKeyBytes;
            marginBootstrapState.certChallengeBytes = challenge.challengeBytes;

            // Current active-path timing gap:
            // - original later state9 seed reads want live `+0xd4 -> owner+0x1c+0x85`
            // - the replacement's launcher-owned bootstrap parse currently learns the same 16-byte
            //   key here before the narrowed post-copy margin leaf ever sees a decoded code-5 writeback
            // Keep the nearer live connection mirror populated from that earlier recovery point so
            // direct client `+0xd4` callers stop depending on the launcher-owned sidecar on the
            // active path.
            // Current tighter source consequence from the new `0x4429b0 -> 0x441470` pass:
            // - writing that live `+0x85..+0x94` mirror on the margin connection now also triggers
            //   the lazy local `+0x9c = CStreamPacketEncryptionModule` scaffold install/refresh,
            //   matching the newly recovered agenda-module ownership point more closely.
            if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(MarginConnection());
                marginConnection != nullptr && marginBootstrapState.marginTwofishKeyBytes.size() == 16u) {
                std::array<uint8_t, 16> liveSeedBytes85 = {};
                std::copy_n(
                    marginBootstrapState.marginTwofishKeyBytes.begin(),
                    liveSeedBytes85.size(),
                    liveSeedBytes85.begin());
                marginConnection->SetMessageCode5SeedBytes85(liveSeedBytes85);
                spdlog::info(
                    "DIAGNOSTIC: mirrored launcher-owned CERT_Challenge Twofish key into live margin connection +0x85..+0x94 early because the active replacement path has not yet naturally hit the decoded code-5 writeback seam connection={}",
                    fmt::ptr(marginConnection));
            }

            uint32_t sendResult = 0u;
            if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(MarginConnection());
                marginConnection != nullptr &&
                marginBootstrapState.marginTwofishKeyBytes.size() == 16u &&
                marginBootstrapState.certChallengeBytes.size() == 16u) {
                std::array<uint8_t, 16> challengeBytes = {};
                std::copy_n(
                    marginBootstrapState.certChallengeBytes.begin(),
                    challengeBytes.size(),
                    challengeBytes.begin());
                sendResult = marginConnection->SendCertChallengeResponseFromChallengeBytes(
                    challengeBytes);
                if (sendResult != 0u) {
                    spdlog::info(
                        "DIAGNOSTIC: launcher-owned margin mirrored original 0x4429b0 send seam by routing CERT_ChallengeResponse through CMarginConnection_0x4aff38 packet-builder/message-ref send connection={}",
                        fmt::ptr(marginConnection));
                }
            }

            if (sendResult == 0u) {
                mxo::auth::FramedPacket response;
                if (!mxo::auth::BuildMarginCertChallengeResponsePacket(
                        marginBootstrapState.certChallengeBytes,
                        marginBootstrapState.marginTwofishKeyBytes,
                        mxo::auth::kFrameModeAuto,
                        &response)) {
                    spdlog::info("DIAGNOSTIC: launcher-owned margin failed to build CERT_ChallengeResponse");
                    return 0u;
                }

                sendResult = SendMarginFramedPacket(
                    response,
                    0x03u,
                    "CERT_ChallengeResponse",
                    /*encryptedTransport=*/true);
            }

            if (sendResult != 0u) {
                marginBootstrapState.phase = MarginBootstrapPhase::kSentCertChallengeResponse;
                expectedMarginRequestName_ = "CERT_ConnectReply";

                // DEBUG: Log that we're now waiting for CERT_ConnectReply
                const std::string remoteHost = MarginConnection()
                    ? static_cast<mxo::liblttcp::CMessageConnection_0x4b7928*>(MarginConnection())->RemoteHostName()
                    : std::string("<no connection>");
                spdlog::info(
                    "CLTLoginMediator::ContinueMarginBootstrapHandshake ADVANCED to phase kSentCertChallengeResponse, now waiting for CERT_ConnectReply (opcode 0x04) this={} marginConnection={} remoteHost='{}'",
                    fmt::ptr(this),
                    fmt::ptr(MarginConnection()),
                    remoteHost.empty() ? std::string("<empty>") : remoteHost);
            }
            return sendResult;
        }

        case 0x04u: {
            spdlog::info(
                "launcher-owned margin bootstrap received CERT_ConnectReply transportEncrypted={} payloadLen={}",
                transportEncrypted ? 1u : 0u,
                payloadSize);

            if (payloadSize < 5u) {
                return 0u;
            }
            const uint32_t status = ReadU32LE(payloadBytes + 1u);
            if (status != 0u) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin observed CERT_ConnectReply failure status=0x{:08x}",
                    status);
                return 0u;
            }

            auto pickWeirdSequence = [this]() {
                const std::array<const std::array<uint32_t, 4>*, 9> candidates = {
                    &SelectionContextBlockCf0(),
                    &SelectionContextBlockD00(),
                    &SelectionContextBlockD10(),
                    &SelectionContextBlockD20(),
                    &SelectionContextBlockD30(),
                    &SelectionContextBlockD40(),
                    &SelectionContextBlockD50(),
                    &SelectionContextBlockD60(),
                    &SelectionContextBlockD70()};
                std::array<uint8_t, 16> bytes = {};
                for (const auto* candidate : candidates) {
                    if (!candidate) {
                        continue;
                    }
                    if ((*candidate)[0] == 0u && (*candidate)[1] == 0u && (*candidate)[2] == 0u && (*candidate)[3] == 0u) {
                        continue;
                    }
                    for (size_t i = 0; i < 4u; ++i) {
                        const uint32_t value = (*candidate)[i];
                        bytes[i * 4u + 0u] = static_cast<uint8_t>(value & 0xffu);
                        bytes[i * 4u + 1u] = static_cast<uint8_t>((value >> 8u) & 0xffu);
                        bytes[i * 4u + 2u] = static_cast<uint8_t>((value >> 16u) & 0xffu);
                        bytes[i * 4u + 3u] = static_cast<uint8_t>((value >> 24u) & 0xffu);
                    }
                    break;
                }
                return bytes;
            };

            const uint32_t* launcherVersionPtr = GetNoPatchLauncherVersionValuePtr08();
            const uint32_t* clientVersionPtr = GetNoPatchClientVersionValuePtr0c();
            uint32_t launcherVersion = (launcherVersionPtr && *launcherVersionPtr != 0u)
                ? *launcherVersionPtr
                : authLauncherVersion_;
            uint32_t clientDllVersion = (clientVersionPtr && *clientVersionPtr != 0u)
                ? *clientVersionPtr
                : launcherVersion;
            if (!lastAuthReply_.worlds.empty() && lastAuthReply_.worlds[0].clientVersion != 0u) {
                clientDllVersion = lastAuthReply_.worlds[0].clientVersion;
            }

            mxo::auth::FramedPacket response;
            if (!mxo::auth::BuildMarginConnectRequestPacket(
                    launcherVersion,
                    clientDllVersion,
                    pickWeirdSequence(),
                    marginBootstrapState.marginTwofishKeyBytes,
                    mxo::auth::kFrameModeAuto,
                    &response)) {
                spdlog::info("DIAGNOSTIC: launcher-owned margin failed to build MS_ConnectRequest");
                return 0u;
            }

            const uint32_t sendResult = SendMarginFramedPacket(
                response,
                0x06u,
                kMessageMsConnectRequest,
                /*encryptedTransport=*/true);
            if (sendResult != 0u) {
                marginBootstrapState.phase = MarginBootstrapPhase::kSentMsConnectRequest;
                expectedMarginRequestName_ = "MS_ConnectChallenge";
            }
            return sendResult;
        }

        case 0x07u: {
            spdlog::info(
                "launcher-owned margin bootstrap received MS_ConnectChallenge transportEncrypted={} payloadLen={} phase={}",
                transportEncrypted ? 1u : 0u,
                payloadSize,
                static_cast<unsigned>(marginBootstrapState.phase));

            // Current tighter fidelity read from launcher.exe:0x440780:
            // - state6 slot 6 handles opcode `7` directly
            // - the recovered body does not show a bootstrap-phase single-shot guard, seed/body
            //   compare, or other duplicate-challenge cache before it goes straight into the
            //   parse/hash/send path (`0x4407a5 -> 0x43d800 -> 0x4566a0 -> 0x41af70`)
            // - practical replay consequence on the active path: while state6 is still waiting for
            //   the first real opcode-`9` continuation, a retransmitted `MS_ConnectChallenge`
            //   still needs another response
            // - once the state6 success-side writeback has happened, or once a later helper owns the
            //   route, consume duplicates without another response.
            const uint32_t currentHelperPhaseCode =
                currentState_ ? currentState_->DispatchPhaseCode() : 0u;
            const bool awaitingFirstState6ConnectReply =
                marginBootstrapState.phase == MarginBootstrapPhase::kSentMsConnectChallengeResponse &&
                currentHelperPhaseCode == 6u &&
                postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0u &&
                state6UdpSessionSecretF18_ == 0u;
            if ((marginBootstrapState.phase == MarginBootstrapPhase::kSentMsConnectChallengeResponse ||
                 marginBootstrapState.phase == MarginBootstrapPhase::kReady) &&
                !awaitingFirstState6ConnectReply) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin ignoring duplicate MS_ConnectChallenge after first response phase={} currentState={} ownerF14={} ownerF18=0x{:08x}",
                    static_cast<unsigned>(marginBootstrapState.phase),
                    currentState_ ? currentState_->DebugName() : "<null>",
                    postAuthMarginLoadingState_0xf14.state10SendGateFlagF14,
                    state6UdpSessionSecretF18_);
                return 1u;
            }

            mxo::auth::MarginConnectChallenge challenge;
            if (!mxo::auth::ParseMarginConnectChallengePayload(payloadBytes, payloadSize, &challenge)) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin failed to parse MS_ConnectChallenge transportEncrypted={} payloadLen={}",
                    transportEncrypted ? 1u : 0u,
                    payloadSize);
                return 0u;
            }

            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin parsed MS_ConnectChallenge chunkBytes={} seed16={} currentState={} helperPhase=0x{:02x}",
                static_cast<unsigned>(challenge.chunkByteCount),
                BuildHexPreview(
                    challenge.seedBytes.data(),
                    challenge.seedBytes.size(),
                    challenge.seedBytes.size()),
                currentState_ ? currentState_->DebugName() : "<null>",
                static_cast<unsigned>(currentHelperPhaseCode));

            mxo::auth::FramedPacket response;
            if (!mxo::auth::BuildMarginConnectChallengeResponsePacket(
                    challenge,
                    marginBootstrapState.marginTwofishKeyBytes,
                    mxo::auth::kFrameModeAuto,
                    &response)) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin failed to build MS_ConnectChallengeResponse chunkBytes={} seed00=0x{:02x}",
                    static_cast<unsigned>(challenge.chunkByteCount),
                    static_cast<unsigned>(challenge.seedBytes[0]));
                return 0u;
            }

            const uint32_t sendResult = SendMarginFramedPacket(
                response,
                0x08u,
                "MS_ConnectChallengeResponse",
                /*encryptedTransport=*/true);
            if (sendResult != 0u) {
                marginBootstrapState.phase = MarginBootstrapPhase::kSentMsConnectChallengeResponse;
                expectedMarginRequestName_ = kMessageMsConnectReply;
            }
            return sendResult;
        }

        case 0x09u: {
            const uint32_t currentHelperPhaseCodeBeforeReply =
                currentState_ ? currentState_->DispatchPhaseCode() : 0u;
            spdlog::info(
                "launcher-owned margin bootstrap received MS_ConnectReply transportEncrypted={} payloadLen={} phase={} currentHelperPhaseBeforeReply=0x{:02x}",
                transportEncrypted ? 1u : 0u,
                payloadSize,
                static_cast<unsigned>(marginBootstrapState.phase),
                currentHelperPhaseCodeBeforeReply);

            mxo::auth::MarginConnectReply duplicateReadyReplyPreview;
            const bool duplicateReadyReplyParsed =
                mxo::auth::ParseMarginConnectReplyPayload(
                    payloadBytes,
                    payloadSize,
                    &duplicateReadyReplyPreview);
            if (marginBootstrapState.phase == MarginBootstrapPhase::kReady &&
                currentHelperPhaseCodeBeforeReply != 6u &&
                (postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 != 0u ||
                 state6UdpSessionSecretF18_ != 0u)) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin ignoring duplicate MS_ConnectReply outside the proven state6 slot6 route phase={} ownerF14={} ownerF18=0x{:08x} parsed={} duplicateSessionId=0x{:08x} duplicateStatus0=0x{:08x} duplicateStatus1=0x{:08x} currentState={}",
                    static_cast<unsigned>(marginBootstrapState.phase),
                    postAuthMarginLoadingState_0xf14.state10SendGateFlagF14,
                    state6UdpSessionSecretF18_,
                    duplicateReadyReplyParsed ? 1u : 0u,
                    duplicateReadyReplyParsed ? duplicateReadyReplyPreview.sessionId : 0u,
                    duplicateReadyReplyParsed ? duplicateReadyReplyPreview.status0 : 0u,
                    duplicateReadyReplyParsed ? duplicateReadyReplyPreview.status1 : 0u,
                    currentState_ ? currentState_->DebugName() : "<null>");
                return 1u;
            }

            mxo::auth::MarginConnectReply reply;
            if (!mxo::auth::ParseMarginConnectReplyPayload(payloadBytes, payloadSize, &reply)) {
                spdlog::info(
                    "DIAGNOSTIC: launcher-owned margin failed to parse MS_ConnectReply transportEncrypted={} payloadLen={}",
                    transportEncrypted ? 1u : 0u,
                    payloadSize);
                return 0u;
            }

            marginBootstrapState.marginSessionId = reply.sessionId;
            marginBootstrapState.phase = MarginBootstrapPhase::kReady;

            // Static + runtime-backed existing-character continuation boundary:
            // - `0x440ab9..0x440ac9` writes owner `+0xf14/+0xf18` inside state6 slot 6
            // - `0x440acc..0x440ae5` then restores the next helper through cached upstream `this+4`
            // - `0x43bd48..0x43bd54` keeps state8 slot 3 gated on owner `+0xf14`
            // Keep that as the only source-owned owner `+0xf14/+0xf18` writer and helper-restore
            // route here instead of synthesizing readiness directly from bootstrap completion
            uint32_t state6Handled = 0u;
            if (currentHelperPhaseCodeBeforeReply == 6u) {
                state6Handled = currentState_->Slot6_HandleSecondaryMessage(nullptr);
            }

            if (state6Handled != 0u &&
                currentState_ != nullptr &&
                currentState_->DispatchPhaseCode() == 8u) {
                expectedMarginRequestName_ = "existing-character state8 raw-0x0f margin packet";
            } else if (state6Handled != 0u) {
                expectedMarginRequestName_ = "post-auth helper state margin packet";
            } else {
                expectedMarginRequestName_ =
                    "post-MS_ConnectReply continuation pending helper restore";
            }
            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap completed sessionId=0x{:08x} field0d=0x{:04x} field0f=0x{:04x} field11=0x{:04x} field13=0x{:04x} field15=0x{:04x} state6Handled=0x{:08x} ownerF14={} ownerF18=0x{:08x} currentHelperPhaseBeforeReply=0x{:02x} currentState={}",
                reply.sessionId,
                reply.field0d,
                reply.field0f,
                reply.field11,
                reply.field13,
                reply.field15,
                state6Handled,
                postAuthMarginLoadingState_0xf14.state10SendGateFlagF14,
                state6UdpSessionSecretF18_,
                currentHelperPhaseCodeBeforeReply,
                currentState_ ? currentState_->DebugName() : "<null>");
            if (state6Handled != 0u) {
                return state6Handled;
            }

            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap completed MS_ConnectReply outside the state6 slot6 route; not synthesizing owner+0xf14/+0xf18 or restoring slot3 from bootstrap completion alone currentHelperPhaseBeforeReply=0x{:02x} currentState={} ownerF14={} ownerF18=0x{:08x}",
                currentHelperPhaseCodeBeforeReply,
                currentState_ ? currentState_->DebugName() : "<null>",
                postAuthMarginLoadingState_0xf14.state10SendGateFlagF14,
                state6UdpSessionSecretF18_);
            return 1u;
        }

        default:
            break;
    }

    return 0u;
}

// anchor: launcher.exe:0x41b500 -> 0x4435f0 / 0x441f30
void CLTLoginMediator::PrepareState5MarginConnectionCopySend() {
    auto* marginConnection = static_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_);
    AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0(*authBootstrapChild680_)
        .PrepareState5MarginConnectionCopySend(*marginConnection);
    marginConnection->SendStoredBootstrapReplyCopy98();
}

const void* CLTLoginMediator::AuthBootstrapReplyCopyShadowF4Scaffold() const {
    return authBootstrapChild680_ ? authBootstrapChild680_->authReplyCopyShadowF4 : nullptr;
}

bool CLTLoginMediator::HasValidState5ReplyCopyShadowF4Scaffold() const {
    // Exact recovered gate split from `0x433c0 -> AuthBootstrapReplyCopyShadowF4_IsFresh`:
    // - if child `+0xf4` is null, state5 slot3 takes the helper-state-2 branch
    // - otherwise the copied `0x136` block stays usable only while
    //   `(time(NULL) - child+0x80) < *(uint32_t*)(child+0xf4 + 0xac)`
    //   where child `+0x80` is the server-time bias seeded by `0x448140`, so the left side is
    //   effectively the current auth-server time
    const auto* copyShadow = static_cast<const AuthBootstrapReplyCopyShadowF4_0x44add0*>(
        authBootstrapChild680_ ? authBootstrapChild680_->authReplyCopyShadowF4 : nullptr);
    if (copyShadow == nullptr) {
        return false;
    }

    const uint32_t expiryTimeAc = ReadU32LE(copyShadow->signedData80.data() + 0x2cu);
    const std::time_t now = std::time(nullptr);
    const uint32_t authServerTimeBias80 =
        authBootstrapChild680_ ? authBootstrapChild680_->authServerTimeBias80 : 0u;
    const uint32_t currentAuthServerTime =
        (now > static_cast<std::time_t>(authServerTimeBias80))
            ? static_cast<uint32_t>(now - static_cast<std::time_t>(authServerTimeBias80))
            : 0u;
    return currentAuthServerTime < expiryTimeAc;
}

// UNANCHORED: source-owned table mirror fed from parsed auth reply worlds
void CLTLoginMediator::SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world) {
    if (worldIndex >= worldDescriptorsD84_.size()) {
        return;
    }

    const uint8_t rawStatus = static_cast<uint8_t>(world.status & 0xffu);
    const uint8_t rawType = static_cast<uint8_t>(world.type & 0xffu);
    const uint8_t normalizedStatus = (rawStatus != 0u && rawStatus < 6u) ? rawStatus : 0u;
    const uint8_t normalizedType = (rawType != 0u && rawType < 4u) ? rawType : 0u;

    if (normalizedStatus != rawStatus) {
        spdlog::info(
            CLTLoginState_AuthenticatePending::kLogInvalidWorldStatus,
            world.worldName.c_str(),
            static_cast<unsigned>(world.worldId),
            static_cast<unsigned>(rawStatus));
    }
    if (normalizedType != rawType) {
        spdlog::info(
            CLTLoginState_AuthenticatePending::kLogInvalidWorldType,
            world.worldName.c_str(),
            static_cast<unsigned>(world.worldId),
            static_cast<unsigned>(rawType));
    }

    WorldDescriptorState_0x4b533c& descriptor = worldDescriptorsD84_[worldIndex];
    descriptor.worldId01 = world.worldId;
    descriptor.inlineNamePlus03 = world.worldName;
    descriptor.status17 = normalizedStatus;
    descriptor.type18 = normalizedType;
    descriptor.serverVersion19 = world.clientVersion;
    descriptor.serverLanguage1d = static_cast<uint8_t>(world.unknown4 & 0xffu);
    descriptor.privateFlag1e = static_cast<uint8_t>((world.unknown4 >> 8) & 0xffu);
    descriptor.populationLevel1f = world.load;
    worldDescriptorValidD84_[worldIndex] = true;
}

// UNANCHORED: source-owned table mirror fed from parsed auth reply characters
void CLTLoginMediator::SeedRecoveredCharacterSlotRecordFromAuthReply(
    uint8_t characterIndex,
    const mxo::auth::AuthCharacterEntry& character) {
    if (characterIndex >= selectionRouteState684_.slotRecordTable04_.size()) {
        return;
    }

    const uint8_t rawStatus = static_cast<uint8_t>(character.status & 0xffu);
    const uint8_t normalizedStatus = (rawStatus <= 6u) ? rawStatus : 7u;
    if (normalizedStatus != rawStatus) {
        spdlog::info(
            CLTLoginState_AuthenticatePending::kLogInvalidCharacterStatus,
            character.handle.text.c_str(),
            static_cast<unsigned long long>(character.characterId),
            static_cast<unsigned>(rawStatus));
    }

    SlotRecordState_0x4b5328& slotRecord = selectionRouteState684_.slotRecordTable04_[characterIndex];
    slotRecord = {};
    slotRecord.heapString14 = character.handle.text.c_str();
    slotRecord.characterIdLow32 = static_cast<uint32_t>(character.characterId & 0xffffffffull);
    slotRecord.characterIdHigh36 = static_cast<uint32_t>((character.characterId >> 32) & 0xffffffffull);
    slotRecord.status3a = normalizedStatus;
    slotRecord.worldId3c = character.worldId;
    selectionRouteState684_.slotRecordValid04_[characterIndex] = true;
}

// UNANCHORED: source-owned lookup helper over the mirrored world-descriptor table
int CLTLoginMediator::FindRecoveredWorldDescriptorIndexByWorldId(uint16_t worldId) const {
    for (size_t i = 0; i < worldDescriptorsD84_.size(); ++i) {
        if (worldDescriptorValidD84_[i] && worldDescriptorsD84_[i].worldId01 == worldId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// UNANCHORED: source-owned synthesis helper for active-branch scaffolding only
void CLTLoginMediator::SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset() {
    // Transitional/source-owned synthesis only:
    // - newer packet debug printers now make one important negative result explicit:
    //   `+0x178/+0x198/+0x1b8` are not generic route/world strings but
    //   `RealFirstName/RealLastName/Background`
    // - so do **not** synthesize those fields from reconstructed route/world tables
    // - the only safe active-path seed we currently keep here is the current-slot character name
    //   and the paired selector/index dword at `+0x12c`
    //
    // Fresh tightening from `0x41c3c0` + `0x4401a0`:
    // - owner `+0x12c` should not be backfilled from slot-record `worldId3c`
    // - the active branch uses `+0x12c` as a world-descriptor index/selector
    const SlotRecordState_0x4b5328* currentSlotRecord = GetCurrentSlotRecord();
    if (currentSlotRecord != nullptr) {
        if (postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00[0] == '\0' &&
            currentSlotRecord->heapString14) {
            const size_t copyCount = std::min(
                std::strlen(currentSlotRecord->heapString14),
                postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.size() - 1);
            std::copy_n(
                currentSlotRecord->heapString14,
                copyCount,
                postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.begin());
            postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00[copyCount] = '\0';
        }

        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(currentSlotRecord->worldId3c);
        if (matchedWorldIndex >= 0 &&
            (postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24 >=
                 static_cast<uint32_t>(worldDescriptorCountD80_) ||
             (postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24 == 0u &&
              matchedWorldIndex != 0))) {
            postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24 =
                static_cast<uint32_t>(matchedWorldIndex);
        }
    }
}

// anchor: launcher.exe:0x41e760
void CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold() const {
    const char* profileName = Arg6AuthName();
    if (!profileName || profileName[0] == '\0') {
        spdlog::info(
            "CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold skipped (no auth/profile name available for Profiles/<name>/characters.ini)");
        return;
    }

    std::string outputPath = "Profiles/";
    outputPath += profileName;
    outputPath += "/characters.ini";

    FILE* file = std::fopen(outputPath.c_str(), "wb");
    if (!file) {
        spdlog::info(
            "CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold could not open '{}' for write",
            outputPath);
        return;
    }

    std::fputs("[Characters]\n", file);
    uint32_t persistedCount = 0u;
    for (size_t i = 0; i < selectionRouteState684_.slotRecordCount00_ &&
                       i < selectionRouteState684_.slotRecordTable04_.size();
         ++i) {
        if (!selectionRouteState684_.slotRecordValid04_[i]) {
            continue;
        }

        const SlotRecordState_0x4b5328& slotRecord = selectionRouteState684_.slotRecordTable04_[i];
        const RouteHostStringTripleState& route = selectionRouteState684_.routeHostStringTriples194_[i];
        const char* characterName = slotRecord.heapString14 ? slotRecord.heapString14 : "";
        const char* routeText = route.BeginOrNull() ? route.BeginOrNull() : "";
        std::fprintf(file, "Character%u:=%s,%s\n", static_cast<unsigned>(i), characterName, routeText);
        ++persistedCount;
    }
    std::fclose(file);

    spdlog::info(
        "CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold wrote '{}' characterCount={} currentIndex=0x{:02x}",
        outputPath,
        persistedCount,
        static_cast<unsigned>(postAuthMarginLoadingState_0xf14.characterRouteIndexCc8));
}



// UNANCHORED: no original launcher.exe anchor assigned yet.
const std::vector<uint8_t>& CLTLoginMediator::StagedIncomingMarginPacketBytes() const {
    return stagedIncomingMarginPacketBytes_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::RebuildMarginAddressList() {
    const std::string resolvedHostName = ResolvedMarginHostName();
    marginAddressListResolvedHostName3c_ = resolvedHostName;

    if (resolvedHostName.empty()) {
        marginAddressList3c_.Reset();
        spdlog::debug("CLTLoginMediator::BeginMarginConnection unresolved margin host");
        return false;
    }

    uint32_t flags = mxo::liblttcp::CLTIPAddressList::kFlagShuffle;
    if (ignoreHostsFileForMargin_) {
        flags |= mxo::liblttcp::CLTIPAddressList::kFlagIgnoreHostsFile;
    }

    if (!marginAddressList3c_.Reinit(resolvedHostName.c_str(), flags)) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnection failed to resolve margin host '{}' flags=0x{:02x}",
            resolvedHostName,
            flags);
        return false;
    }

    return true;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::SelectMarginEndpointIpv4() {
    marginSelectedIpv4_7c_ = marginAddressList3c_.GetNextAddress(/*wrap=*/true);
    return marginSelectedIpv4_7c_ != 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::liblttcp::CMessageConnection_0x4b7928* CLTLoginMediator::EnsureMarginConnectionObject() {
    mxo::liblttcp::CMarginConnection_0x4aff38* marginConnection =
        dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_);
    if (!marginConnection) {
        if (marginConnectionOwnedByMediator_) {
            delete marginConnection_;
        }
        marginConnection = new mxo::liblttcp::CMarginConnection_0x4aff38(engine_);
        if (!marginConnection) {
            marginConnection_ = nullptr;
            marginConnectionOwnedByMediator_ = false;
            return nullptr;
        }

        marginConnection->SetEngine(engine_);
        marginConnection->SetEngine(engine_);
        marginConnection_ = marginConnection;
        marginConnectionOwnedByMediator_ = true;
    }

    marginConnection->SetEngine(engine_);
    marginConnection->SetEngine(engine_);
    marginConnection->ConfigurePacketNameFamily(
        mxo::liblttcp::CMessageConnectionPacketNameFamily::kMargin,
        /*packetizedMessagesEnabled=*/true);
    marginConnection->SetOwnerContext(this);
    return marginConnection_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// ILTLoginMediator::Default - static member initialization (original: launcher.exe:0x4d2c58)
ILTLoginMediator* ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();

}  // namespace mxo::ltlogin
