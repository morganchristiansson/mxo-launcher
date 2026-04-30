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
 * - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_0x4af2b8_Default.md`
 * - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
 * - `../../../../docs/launcher.exe/VTABLES/0x004b01c8.md`
 * - `../../../../docs/launcher.exe/VTABLES/0x004b517c.md`
 * - `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
 * - `../../../../docs/launcher.exe/auth/STATUS.md`
 */

#include "loginmediator.h"
#include "loginmediator_events.h"
#include "launchpad.h"
#include "client_chunk_hashes.h"

#include "loginstate.h"
#include "../../../../src/launcher_mediator_abi.h"
#include "authbootstrap680.h"
#include "../../../runtime/src/libltcrypto/auth_internal.h"
#include "../../../runtime/src/liblttcp/ltipaddresslist.h"
#include "../../../runtime/src/libltnet/sys/pc/pcsocket.h"

#include <modes.h>
#include <twofish.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
// ILTLoginMediator_0x4af2b8::~ILTLoginMediator_0x4af2b8() = default;

namespace {
char g_CLTLoginMediatorCharacterPersistenceDataEmptySection11Buffer[8] = {'\0'};
}

CLTLoginMediatorCharacterPersistenceData_0x41d900::CLTLoginMediatorCharacterPersistenceData_0x41d900() {
    // anchor: launcher.exe:0x41d900
    // Static-RE ctor summary:
    // - seed field28/bodyWord6c to 0x1000
    // - seed tail byte +0x4b8 to 1
    // - point the section-11 small-string triple at an empty 8-byte buffer
    field28_1000 = 0x1000;
    bodyWord6c = 0x1000;
    tail4b8 = {1u};
    section11StringBegin544 = g_CLTLoginMediatorCharacterPersistenceDataEmptySection11Buffer;
    section11StringCurrent548 = g_CLTLoginMediatorCharacterPersistenceDataEmptySection11Buffer;
    section11StringCapacity54c = g_CLTLoginMediatorCharacterPersistenceDataEmptySection11Buffer + 8;
}

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
static void EraseMarginBootstrapState(const CLTLoginMediator* mediator) {
    g_marginBootstrapStateByMediator.erase(mediator);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static mxo::liblttcp::LTTCPEndpointKey_0x44b070 BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey_0x44b070 key = {};
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
    for (size_t i = 0; i < routeHostStrings194_.size(); ++i) {
        routeHostStrings194_[i].clear();
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
        routeHostStrings194_[i].clear();
    }
    slotRecordCount00_ = 0;
    currentSlotOrSelectionIndex644_ = 0xffu;
}

// anchor: launcher.exe:0x41dd00 / embedded CLTLoginMediatorSelectionRouteState_0x41dba0::DestroySelectionRouteState
void CLTLoginMediator::CLTLoginMediatorSelectionRouteState::DestroySelectionRouteState() {
    ResetSelectionRouteState();
    for (size_t i = 0; i < routeHostStrings194_.size(); ++i) {
        StringReleaseStorage(routeHostStrings194_[i]);
        slotRecordTable04_[i] = {};
        slotRecordValid04_[i] = false;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      authConnection_(nullptr),
      marginConnection_(nullptr),
      currentState_(nullptr),
      authPeerCloseQueuedScaffold_(false),
      marginPeerCloseQueuedScaffold_(false),
      marginRouteState_{},
      marginAddressList3c_{},
      authBootstrapChild680_(nullptr),
      launchPadClient65c_(nullptr),
    selectionRouteState684_{},
          authServerPortHostOrder_(11000),
      marginServerPortHostOrder_(10000),
      ignoreHostsFileForMargin_(false),
      authEndpoint_(BuildLoopbackEndpoint(authServerPortHostOrder_)),
      marginEndpoint_(BuildLoopbackEndpoint(marginServerPortHostOrder_)),
      authUsername_(),
      authPassword_(),
      authLauncherVersion_(76005),
      authKeyConfigMd5_(),
      authUiConfigMd5_(),
      expectedAuthRequestName_(nullptr),
      expectedMarginRequestName_(nullptr) {
    InitializeObserverTree674();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::~CLTLoginMediator() {
    for (Packet_WorldList_0x4b533c*& packet : worldListPacketsD84_) {
        delete packet;
        packet = nullptr;
    }
    worldListPacketCountD80_ = 0;
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

    authConnection_ = nullptr;
    marginConnection_ = nullptr;
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
    // Ensure Winsock is initialized before DNS resolution (Initialize runs early before engine connect)
    CLTSocketLayer::Init();
    authAddressList4c_.Reinit(g_qsAuthServerDNSName, authAddressListReinitFlags);

    // Seed instance port field from global for BeginAuthConnection (uses g_AuthServerPort directly per static-RE)
    authServerPortHostOrder_ = g_AuthServerPort;

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
// Wrapper-facing selection startup handoff helper used by the current `0x4d2c58` scaffold.
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
// Wrapper-facing selection clear helper reached from launcher teardown after arg5 release.
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x1c
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x24
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
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x2c
uint32_t CLTLoginMediator::IsConnected() {
    static uint32_t s_IsConnectedCount = 0;
    ++s_IsConnectedCount;
    if (DiagnosticShouldLogRepeatedRuntimeCount(s_IsConnectedCount)) {
        spdlog::debug("MediatorStub::IsConnected() -> 1 [count={:08x}]", s_IsConnectedCount);
    }
    return 1;
}

// anchor: launcher.exe:0x41ecd0 slot +0x2c
uint32_t CLTLoginMediator::ProcessLoginRequest(const SubmitLoginRequestInput_0x407d50& input) {
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

    const bool sessionTokenEmpty = !input.HasSessionToken();
    if ((!input.HasUsername() || !input.HasPassword()) && sessionTokenEmpty) {
        spdlog::info(
            "ROUTE CHECKPOINT: ProcessLoginRequest rejected empty credentials currentState={} -> 0x00000004",
            currentState_ ? currentState_->DebugName() : "<null>");
        return 4u;
    }

    ownerAuthBootstrapSource94_.CopyFromSubmitLoginRequestInput(input);

    // Fidelity: session token at +0xf4 within owner+0x94 block
    ownerAuthBootstrapSource94_.sessionToken60.begin = input.submitSessionTokenString.begin;
    ownerAuthBootstrapSource94_.sessionToken60.current = input.submitSessionTokenString.current;
    ownerAuthBootstrapSource94_.sessionToken60.capacity = input.submitSessionTokenString.capacity;

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
            sessionTokenEmpty ? 1u : 0u,
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
    if (!sessionTokenEmpty) {
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

// anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
// Original body: direct field access without validation
Packet_AsAuthReply_0x4b5328* CLTLoginMediator::GetAuthReplyPacketByIndex40(
    uint32_t selectionIndex) {
    const uint8_t slotIndex = static_cast<uint8_t>(selectionIndex & 0xffu);
    if (slotIndex != 0xffu) {
        return &selectionRouteState684_.slotRecordTable04_[slotIndex];
    }
    return nullptr;
}

// anchor: launcher.exe:0x41f300 / owner vtable +0x44
// Original body: direct field access
Packet_AsAuthReply_0x4b5328* CLTLoginMediator::GetCurrentAuthReplyPacket44() {
    const uint8_t currentSlot = selectionRouteState684_.currentSlotOrSelectionIndex644_;
    if (currentSlot != 0xffu) {
        return &selectionRouteState684_.slotRecordTable04_[currentSlot];
    }
    return nullptr;
}

// anchor: launcher.exe:0x41f350 / vtable +0x48
// Exact tiny body: read child `+0x108` with no fallback/scaffold logic.
const char* CLTLoginMediator::GetWorldOrSelectionName() const {
    return static_cast<const char*>(authBootstrapChild680_->opaqueReplyBlob108);
}

// anchor launcher.exe:0x41f360 / vtable +0x4c
// Exact tiny body: read child `+0x10c` with no fallback/scaffold logic.
const char* CLTLoginMediator::GetProfileOrSessionName() const {
    return static_cast<const char*>(authBootstrapChild680_->opaqueReplyBlob10C);
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
void* CLTLoginMediator::BootstrapRaw08AuxHandle50() const {
    // Fidelity to static-RE: reads dword at offset +0xa8 from copyShadow
    const auto* copyShadow =
        authBootstrapChild680_
            ? AuthBootstrapChildFromWriteHelper(*authBootstrapChild680_).authReplyCopyShadowF4
            : nullptr;
    if (copyShadow != nullptr) {
        return reinterpret_cast<void*>(
            *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(copyShadow) + 0xa8));
    }
    return nullptr;
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
bool CLTLoginMediator::HasBootstrapRaw08AuxHandle54() const {
    const bool present = authBootstrapChild680_
                             ? AuthBootstrapChildFromWriteHelper(*authBootstrapChild680_)
                                   .HasBootstrapRaw08AuxHandle54()
                             : false;
    spdlog::debug(
        "CLTLoginMediator::HasBootstrapRaw08AuxHandle54(+0x54) -> {}",
        present ? 1u : 0u);
    return present;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t CLTLoginMediator::GetCrashReporterPromptForSecurId58() const {
    // Static body:
    //   mov eax, [ecx+0x680]
    //   mov al,  [eax+0x104]
    //   ret
    // So the meaningful ABI result is AL/uint8_t; Ghidra's CONCAT31 is just stale upper EAX
    // bytes from the child pointer because the original does not zero-extend before returning.
    const uint8_t prompt = authBootstrapChild680_->crashReporterPromptForSecurId104;
    spdlog::debug(
        "CLTLoginMediator::GetCrashReporterPromptForSecurId58(+0x58) -> {} [source=owner+0x680+0x104/AL]",
        static_cast<unsigned>(prompt));
    return prompt;
}

// Wrapper-facing launcher/client chain note for `+0x5c/+0x60`:
// - launcher crashreporter seeding calls both slots with no stack argument
// - client `InitClientDLL` uses caller-clean wrappers and threads the previous return value
//   through the next call
// Keep the incoming value opaque here instead of forcing a false `const char*` semantic.
// The recovered launcher implementation does not actually consume that token; it reads the
// crash-reporter seed directly from the auth bootstrap child.
// anchor: launcher.exe:0x41f3a0 / vtable +0x5c
const char* CLTLoginMediator::GetCrashReporterUsername5c(const void* chainedValueToken) {
    // Original body ignores caller state and returns:
    //   owner+0x680->authReplyCopyShadowF4(+0xf4)->field_0x85 when +0xf4 is non-null,
    //   otherwise a static fallback object at launcher.exe:this_004aafbb.
    // Do not return the launcher VA in the replacement process. The original fallback address is a
    // pooled empty C-string in .rdata: code can pass it anywhere a non-null "" pointer is needed.
    // Source mirrors that role with a local static empty string instead of hardcoding an original
    // image address.
    // The public ABI wrapper may still pass a caller-clean chained token on the client path; keep
    // it only for diagnostics because launcher.exe:0x41f3a0 does not read a stack argument.
    static constexpr char kRdataEmptyStringMirror[] = "";
    const auto* copyShadow = authBootstrapChild680_->authReplyCopyShadowF4;
    const char* usernameSeed = copyShadow
                                   ? reinterpret_cast<const char*>(copyShadow->signedData80.data() + 0x5u)
                                   : kRdataEmptyStringMirror;
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterUsername5c(+0x5c chainedValueToken={}) -> {} [source={}]",
        fmt::ptr(chainedValueToken),
        fmt::ptr(usernameSeed),
        copyShadow ? "owner+0x680+0xf4+0x85" : "local-mirror-of-g_rdataEmptyString");
    return usernameSeed;
}

// anchor: launcher.exe:0x41f3c0 / vtable +0x60
const char* CLTLoginMediator::GetCrashReporterPassword60(const void* chainedValueToken) {
    // Exact tiny body: load owner `+0x680`, then return child `+0xf8` begin pointer.
    // Keep the chained token for wrapper-signature compatibility only; the original body does not
    // read it.
    const char* passwordSeed = authBootstrapChild680_->stringF8.c_str();
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterPassword60(+0x60 chainedValueToken={}) -> {} [source={}]",
        fmt::ptr(chainedValueToken),
        MaskedSensitiveValue(passwordSeed),
        "owner+0x680+0xf8.begin");
    return passwordSeed;
}

// anchor: launcher.exe:0x41f2b0 / vtable +0x64
uint32_t CLTLoginMediator::GetBootstrapSuccessHeaderDword64() const {
    // Exact static body returns owner+0x680+0x110. This field is written in the state2 auth-reply
    // success path from authReplyParseObjectF0->replyHeader10 + 0x07 before the one-time gate.
    const uint32_t value = authBootstrapChild680_->authReplySuccessHeaderDword07_110;
    spdlog::info(
        "CLTLoginMediator::GetBootstrapSuccessHeaderDword64(+0x64) -> 0x{:08x} [source=owner+0x680+0x110]",
        static_cast<unsigned>(value));
    return value;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x68
uint32_t CLTLoginMediator::HasLiveHlCfg68() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag140e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x6c
uint32_t CLTLoginMediator::HasLiveAnCfg6c() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1416 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x70
uint32_t CLTLoginMediator::HasLivePiCfg70() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag141e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x74
uint32_t CLTLoginMediator::HasLiveAiCfg74() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag1426 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x78
uint32_t CLTLoginMediator::HasLiveCsCfg78() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x7c
uint32_t CLTLoginMediator::HasLiveBlCfg7c() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag13fe != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x80
uint32_t CLTLoginMediator::HasLiveIlCfg80() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1406 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x84
uint32_t CLTLoginMediator::HasLiveRlCfg84() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1448 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x88
uint32_t CLTLoginMediator::HasLiveClCfg88() const {
    const auto* ownerState = this;
    if (ownerState) {
        return static_cast<uint32_t>(ownerState->flag1452 != 0u);
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0x8c
uint32_t CLTLoginMediator::HasState8PersistenceData8c() const {
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const auto* ownerState = this;
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
    const void* header = static_cast<const void*>(&state8PersistenceDataF1c.header2c[0]);
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceHeaderBc(+0xbc) -> {} [owner={} first=0x{:08x} bytes=0x{:02x}]",
        fmt::ptr(header),
        fmt::ptr(this),
        static_cast<unsigned>(state8PersistenceDataF1c.header2c[0]),
        static_cast<unsigned>(state8PersistenceDataF1c.header2c.size() * sizeof(uint32_t)));
    return header;
}

// anchor: launcher.exe:0x41f180 vtable +0xc0
const void* CLTLoginMediator::GetState8PersistenceBodyC0() const {
    // Keep this wrapper-facing body close to the original tiny getter:
    // - original `0x41f180` returns owner `+0xf88`
    const void* body = static_cast<const void*>(&state8PersistenceDataF1c.bodyWord6c);
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceBodyC0(+0xc0) -> {} [owner={} body00=0x{:08x} bytes=0x{:04x}]",
        fmt::ptr(body),
        fmt::ptr(this),
        state8PersistenceDataF1c.bodyWord6c,
        static_cast<unsigned>(CLTLoginMediatorCharacterPersistenceData_0x41d900::kBodySize));
    return body;
}

// anchor: launcher.exe:0x41aec0 vtable +0xc4
void* CLTLoginMediator::GetState8PersistenceOverflowC4(uint16_t* outLength) const {
    // Keep this wrapper-facing body close to the original tiny getter:
    // - original `0x41aec0` returns owner `+0x13f0`
    // - when the caller supplies an out pointer, it also writes owner `+0x13f4`
    if (outLength) {
        *outLength = state8PersistenceDataF1c.section0OverflowLength4d8;
    }
    void* const buffer = state8PersistenceDataF1c.section0OverflowBuffer4d4;
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceOverflowC4(+0xc4) -> {} [owner={} length=0x{:04x}]",
        fmt::ptr(buffer),
        fmt::ptr(this),
        static_cast<unsigned>(state8PersistenceDataF1c.section0OverflowLength4d8));
    return buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// +0xc8
uint32_t CLTLoginMediator::HasState8Section11Dword145c() const {
    const uint32_t value = state8PersistenceDataF1c.section11Dword540;
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
    const uint32_t value = state8PersistenceDataF1c.section11Dword540;
    spdlog::info(
        "CLTLoginMediator::GetState8Section11Dword145c(+0xcc) -> 0x{:08x} [owner={}]",
        value,
        fmt::ptr(this));
    return value;
}

// anchor: launcher.exe:0x41f1b0 / vtable +0xd0
std::string_view CLTLoginMediator::GetState8Section11String1460() const {
    const CLTLoginMediatorCharacterPersistenceData_0x41d900& snapshot = state8PersistenceDataF1c;
    const char* const begin = snapshot.section11StringBegin544;
    const char* const current = snapshot.section11StringCurrent548;
    const std::string_view value =
        (begin && current && current >= begin) ? std::string_view(begin, static_cast<size_t>(current - begin))
                                               : std::string_view{};

    spdlog::info(
        "CLTLoginMediator::GetState8Section11String1460(+0xd0) -> begin={} current={} owner={} text='{}'",
        fmt::ptr(begin),
        fmt::ptr(current),
        fmt::ptr(this),
        value.empty() ? "<empty>" : std::string(value));
    return value;
}

// anchor: launcher.exe:0x41b4f0 +0xd4
const void* CLTLoginMediator::GetState9CallbackSeedPointer85D4() const {
    if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_)) {
        const uint8_t* seedPointer = marginConnection->messageCode5SeedBytes85_.data();
        if (seedPointer) {
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
        (selectionName ? "owner+0x688[index].debugString14" : "owner+0x688[index]=<null>");
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

// anchor: launcher.exe:0x41b2a0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xe4
// Direct owner-side slot-record status reader reused by the page-7 row builder / resolver. The
// launcher passes the packed-row high word here; any negative / out-of-range index naturally falls
// through to the same default status 7 as the original body.
uint8_t CLTLoginMediator::GetSlotRecordStatusBySelectionIndex(int32_t selectionIndex) const {
    if (!currentState_) {
        return 7u;
    }
    const uint32_t stateId = currentState_->GetStateId();
    if (stateId <= 2u) {
        return 7u;
    }
    const uint32_t unsignedSelectionIndex = static_cast<uint32_t>(selectionIndex);
    if (unsignedSelectionIndex >= 100u) {
        return 7u;
    }
    const Packet_AsAuthReply_0x4b5328* record =
        const_cast<CLTLoginMediator*>(this)->GetAuthReplyPacketByIndex40(unsignedSelectionIndex);
    return record ? record->packetType1a : 7u;
}

// anchor: launcher.exe:0x41ec00 +0xe8
// Fidelity: 0x41ec00 gates on GetStateId()>2, selectedSlotRecordIndex<100, and
// selectionRouteState684_.slotRecordCount00_!=0; then it decrements +0x684, releases the
// selected +0x688 slot record when non-null, shifts later +0x688 entries and +0x818
// string-triples down, and clears the final pointer/string tail.
uint32_t CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex(uint32_t selectedSlotRecordIndex) {
    const uint32_t stateId = currentState_ ? currentState_->GetStateId() : 0u;
    if (stateId <= 2u || selectedSlotRecordIndex >= 100u ||
        selectionRouteState684_.slotRecordCount00_ == 0u) {
        spdlog::info(
            "CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex rejected slotIndex={} stateId={} count={} currentState={}",
            static_cast<unsigned>(selectedSlotRecordIndex),
            static_cast<unsigned>(stateId),
            static_cast<unsigned>(selectionRouteState684_.slotRecordCount00_),
            currentState_ ? currentState_->DebugName() : "<null>");
        return 0u;
    }

    const size_t slotIndex = static_cast<size_t>(selectedSlotRecordIndex);
    const uint8_t oldCount = selectionRouteState684_.slotRecordCount00_;
    const char* removedName = selectionRouteState684_.slotRecordTable04_[slotIndex].debugString14;

    selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(oldCount - 1u);
    selectionRouteState684_.slotRecordValid04_[slotIndex] = false;
    selectionRouteState684_.slotRecordTable04_[slotIndex] = {};

    for (size_t i = slotIndex; i + 1u < static_cast<size_t>(selectionRouteState684_.slotRecordCount00_); ++i) {
        selectionRouteState684_.slotRecordTable04_[i] = selectionRouteState684_.slotRecordTable04_[i + 1u];
        selectionRouteState684_.slotRecordValid04_[i] = selectionRouteState684_.slotRecordValid04_[i + 1u];
        selectionRouteState684_.routeHostStrings194_[i] =
            selectionRouteState684_.routeHostStrings194_[i + 1u];
    }

    const size_t tailIndex = static_cast<size_t>(selectionRouteState684_.slotRecordCount00_);
    selectionRouteState684_.slotRecordTable04_[tailIndex] = {};
    selectionRouteState684_.slotRecordValid04_[tailIndex] = false;
    selectionRouteState684_.routeHostStrings194_[tailIndex].clear();

    PersistCharactersIniFromRecoveredAuthStateScaffold();

    spdlog::info(
        "CLTLoginMediator::RemoveSlotRecordAndCompactRouteStateByIndex removed slotIndex={} oldCount={} newCount={} removedName='{}' currentState={}",
        static_cast<unsigned>(selectedSlotRecordIndex),
        static_cast<unsigned>(oldCount),
        static_cast<unsigned>(selectionRouteState684_.slotRecordCount00_),
        (removedName && removedName[0] != '\0') ? removedName : "<empty>",
        currentState_ ? currentState_->DebugName() : "<null>");
    return 0u;
}

// anchor: launcher.exe:0x41c1f0 +0xec
// Fidelity: Original 0x41c1f0 directly writes from param_1 into selectionRouteState684.
// No intermediate storage or diagnostic counters exist in the original binary.
uint32_t CLTLoginMediator::PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) {
 // anchor: launcher.exe:0x41c1f0 +0xec
 // Fidelity: Original checks DispatchPhaseCode() > 2 (phase 3+) before any operation
 // Disassembly: 0x41c1f6 MOV ECX,[ESI+0x10]; 0x41c1fb CALL [EAX+0x18]; 0x41c201 JL out
 if (currentState_ == nullptr || currentState_->DispatchPhaseCode() <= 2u) {
 return 0u;
 }

 // Owner-side state3-wait advance:
 // - the early route reaches this while current helper `+0x10` is still state3
 // - this method, not a state3-local slot-3 body, copies the `0xb4` selection/config snapshot
 // into owner `+0xcc8/+0xcd0..+0xd7f` (via selectionRouteState684_ at owner +0x684)
 // - then it switches to helper/state `8`
 //
 // Fidelity note: Original 0x41c1f0 directly writes from param_1 into selectionRouteState684
 // without intermediate storage. No selectionContext0ecCopy_, selectionContext0ecCopyValid_,
 // or selection0ecCount_ exist in the original binary - these were diagnostic additions.
 if (input.slotOrSelectionIndex00 >= 100u) {
 return 0u;
 }

 // anchor: launcher.exe:0x41c390 = SetCurrentCharacterRouteIndexCc8Scaffold
 // Write slot index to owner +0xcc8 / selectionRouteState684_.currentSlotOrSelectionIndex644_
 SetCurrentCharacterRouteIndexCc8Scaffold(static_cast<uint8_t>(input.slotOrSelectionIndex00));

 // anchor: launcher.exe:0x41c1f0 body - direct field copies from param_1 to selectionRouteState684_
 // Original directly assigns: (this->selectionRouteState684).currentSlotOrSelectionIndex644 = (byte)*param_1;
 // Then copies param_1[1..0x2c] into the selectionContextBlock* fields
 static_assert(
 sizeof(selectionRouteState684_.persistedSelectionContext64c_) == sizeof(input) - sizeof(input.slotOrSelectionIndex00),
 "0x41c1f0 should copy the contiguous state3 snapshot body after the leading slot dword");
 std::copy_n(
 reinterpret_cast<const uint32_t*>(&input.block04),
 sizeof(selectionRouteState684_.persistedSelectionContext64c_) / sizeof(uint32_t),
 reinterpret_cast<uint32_t*>(&selectionRouteState684_.persistedSelectionContext64c_));

 // anchor: launcher.exe:0x41c1f0 tail -> 0x41b450(8) = SetCurrentState
 const CLTLoginState* const oldState = currentState_;
 uint32_t state8EntryResult = 0u;
 if (LoginHelperStateByIdScaffold(8u) != nullptr) {
 // Important existing-character continuation detail:
 // - the original owner `+0xec` tail does not stop at `currentState = state8`
 // - `0x41b450` immediately re-enters the new helper's slot 3 with the old helper object
 // - on the active path that means `state8 slot3` runs right here, sees margin state != 2,
 // and hands off into helper/state4 before the later margin connect-status arrives
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
 // anchor: launcher.exe:0x41f1c0
 // Original tiny body: return &owner->state8PersistenceDataF1c;
 // No diagnostic counters exist in original binary - they were source additions.
 return &state8PersistenceDataF1c;
}

// anchor: launcher.exe:0x41af30 / launcher.exe:0x40e5b0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xf8
// Fidelity: 0x41af30 only calls GetStateId() and returns owner byte +0xd80 when the
// current helper/state id is >2; otherwise it returns 0.
uint32_t CLTLoginMediator::GetWorldCount() const {
    const uint32_t stateId = currentState_ ? currentState_->GetStateId() : 0u;
    const uint32_t worldCount = (stateId > 2u) ? static_cast<uint32_t>(worldListPacketCountD80_) : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldCount(+0xf8) -> {} [source={}]",
        worldCount,
        (stateId > 2u) ? "owner+0xd80" : "state<=2");
    return worldCount;
}

// anchor: launcher.exe:0x41b2e0 / launcher.exe:0x40cd10
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0xfc
// Active replacement path note:
// - the text-mode launcher selection stage now runs only after auth success
// - so this wrapper-facing getter can stay owner-table-backed without the older synthetic
//   startup world-list sidecar
const char* CLTLoginMediator::GetWorldNameByIndex(uint32_t index) {
    const bool stateAllowsDescriptorRead = currentState_ != nullptr && currentState_->GetStateId() > 2u;
    const char* worldName =
        (stateAllowsDescriptorRead && index <= 0xffu)
            ? GetWorldListNameByIndex(static_cast<uint8_t>(index))
            : nullptr;

    spdlog::info(
        "CLTLoginMediator::GetWorldNameByIndex(+0xfc index=0x{:06x}) -> '{}' [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        worldName ? worldName : "<null>",
        worldName ? "owner+0xd84.inlineName+0x03" : "no-active-descriptor-table");
    return worldName;
}

// anchor: launcher.exe:0x41b320 / launcher.exe:0x4d3584 +0x100
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x100
// Active replacement path note:
// - the current text-mode selection menu only reaches this after auth success
// - so the wrapper-facing gate byte now comes only from the recovered owner descriptor Status byte
uint8_t CLTLoginMediator::GetWorldSelectionGateByteByIndex(uint32_t index) const {
    const bool stateAllowsDescriptorRead = currentState_ != nullptr && currentState_->GetStateId() > 2u;
    const uint8_t selectionGateByte100 =
        (stateAllowsDescriptorRead && index <= 0xffu)
            ? GetWorldListStatusByIndex(static_cast<uint8_t>(index))
            : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldSelectionGateByteByIndex(+0x100 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(selectionGateByte100),
        (selectionGateByte100 != 0u) ? "owner+0xd84.status+0x17" : "no-active-descriptor-table");
    return selectionGateByte100;
}

// anchor: launcher.exe:0x41b360
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x104
// Corrected off-by-one read from Ghidra/disassembly: this wrapper slot now surfaces owner
// descriptor Type byte `+0x18`, not server-version low byte.
uint8_t CLTLoginMediator::GetWorldTypeByteByIndex(uint32_t index) const {
    const bool stateAllowsDescriptorRead = currentState_ != nullptr && currentState_->GetStateId() > 2u;
    const uint8_t worldTypeByte = stateAllowsDescriptorRead
        ? ((index <= 0xffu) ? GetWorldListTypeByIndex(static_cast<uint8_t>(index)) : 0u)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldTypeByteByIndex(+0x104 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(worldTypeByte),
        stateAllowsDescriptorRead ? "owner+0xd84.type+0x18" : "state<=2");
    return worldTypeByte;
}

// anchor: launcher.exe:0x41b3a0
// vtable: ILTLoginMediator_0x4af2b8.Default slot +0x108
uint8_t CLTLoginMediator::GetWorldPopulationNibbleByIndex(uint32_t index) const {
    const bool stateAllowsDescriptorRead = currentState_ != nullptr && currentState_->GetStateId() > 2u;
    const uint8_t populationNibble = stateAllowsDescriptorRead
        ? ((index <= 0xffu) ? GetWorldListPopulationByIndex(static_cast<uint8_t>(index)) : 0u)
        : 0u;

    spdlog::info(
        "CLTLoginMediator::GetWorldPopulationNibbleByIndex(+0x108 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(populationNibble),
        stateAllowsDescriptorRead ? "owner+0xd84.population+0x1f.low4" : "state<=2");
    return populationNibble;
}

// anchor: launcher.exe:0x41f2c0 slot +0x10c
std::string_view CLTLoginMediator::GetRouteDescriptor30() const {
    // Static-RE body is the tiny accessor `return &this->mbr_0x30`.
    return routeDescriptor30_;
}

// anchor: launcher.exe:0x41af50 +0x118
const std::vector<std::string>& CLTLoginMediator::GetLateEntryList1470() const {
    return lateEntryList1470_;
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

    if (static_cast<uint32_t>(worldListPacketCountD80_) < input.field24) {
        spdlog::info(
            "CLTLoginMediator::ProcessCreateCharacterInput120(+0x120) rejected selector field12c=0x{:08x} upperBoundF8=0x{:02x}",
            static_cast<unsigned>(input.field24),
            static_cast<unsigned>(worldListPacketCountD80_));
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
        createCharacterData108.characterName00[0]
            ? createCharacterData108.characterName00.data()
            : "<empty>",
        static_cast<unsigned>(createCharacterData108.selectedWorldField24),
        static_cast<unsigned>(createCharacterData108.header2c[0]),
        createCharacterData108.backgroundB0[0]
            ? createCharacterData108.backgroundB0.data()
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
    state10SendGateFlagF14 = 0u;
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
        static_cast<unsigned>(state10SendGateFlagF14),
        static_cast<unsigned>(marginConnectionFlag2d_),
        rawState,
        (rawState == 1 || rawState == 2) ? 1u : 0u,
        static_cast<unsigned>(closeResult),
        currentState_ ? currentState_->DebugName() : "<null>");
    return true;
}

// anchor: launcher.exe:0x41ddb0 slot +0x170
bool CLTLoginMediator::RegisterLoginObserver(void* observer) {
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator_0x4af2b8.Default` object now
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
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator_0x4af2b8.Default` object now
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

    const Packet_AsAuthReply_0x4b5328* currentSlotRecord =
        GetCurrentAuthReplyPacket44();
    if (!currentSlotRecord) {
        std::memset(outDwords, 0, 0x20u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18c missing current slot record while state9-gated; zeroed 0x20-byte blob and returned generic failure");
        return 1u;
    }

    outDwords[0] = currentSlotRecord->characterIdLow1c;
    outDwords[1] = currentSlotRecord->characterIdHigh20;
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

    // anchor: launcher.exe:0x41df60 / 0x44b570
    // Safe flattening step 1: this is a local one-shot transform site, so we can use the direct
    // Crypto++ CBC Twofish encryptor without disturbing any recovered embedded object layout.
    CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption feedbackTransform;
    bool transformOk = false;
    try {
        feedbackTransform.SetKeyWithIV(
            marginTwofishKey.data(),
            static_cast<uint32_t>(marginTwofishKey.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data());
        feedbackTransform.ProcessData(
            reinterpret_cast<CryptoPP::byte*>(outDwords + 4),
            reinterpret_cast<const CryptoPP::byte*>(transformInput.data()),
            16u);
        transformOk = true;
    } catch (const CryptoPP::Exception&) {
        transformOk = false;
    }
    if (!transformOk) {
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
// Original 0x41ecd0: Clears state. No diagnostic counters exist in original binary.
void CLTLoginMediator::ResetSelectionContext0ecMirror() {
 // Fidelity note: Original does not maintain selectionContext0ecCopy_, selectionContext0ecCopyValid_,
 // or selection0ecCount_ - these were diagnostic additions. The original simply clears the state
 // or performs minimal cleanup as part of owner state management.
 // It also duplicates CLTLoginMediator::ProcessLoginRequest anchor
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::MirrorCreateCharacterInput120SourceBlock(const ProcessCreateCharacterInput120Sketch& input) {
    auto& createCharacterData = createCharacterData108;
    std::copy(input.string00.begin(), input.string00.end(), createCharacterData.characterName00.begin());
    createCharacterData.selectedWorldField24 = input.field24;

    std::copy(input.dwords2c.begin(), input.dwords2c.end(), createCharacterData.header2c.begin());
    std::copy(input.dwords4c.begin(), input.dwords4c.end(), createCharacterData.secondary4c.begin());
    createCharacterData.bodyWord6c =
        static_cast<uint32_t>(input.bytes6c[0]) |
        (static_cast<uint32_t>(input.bytes6c[1]) << 8) |
        (static_cast<uint32_t>(input.bytes6c[2]) << 16) |
        (static_cast<uint32_t>(input.bytes6c[3]) << 24);

    std::copy(input.string70.begin(), input.string70.end(), createCharacterData.realFirstName70.begin());
    std::copy(input.string90.begin(), input.string90.end(), createCharacterData.realLastName90.begin());
    std::copy(input.stringB0.begin(), input.stringB0.end(), createCharacterData.backgroundB0.begin());
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::CaptureCreateCharacterInputSlot120(
    const void* input120,
    void* returnAddress,
    bool applyOwnerSemantics) {
    createCharacterInput120_ = input120;
    ++createCharacterInputCount120_;

    if (!input120) {
        spdlog::info(
            "CLTLoginMediator::CaptureCreateCharacterInputSlot120(+0x120) input=<null> caller={} [count={}] applyOwnerSemantics={}",
            fmt::ptr(returnAddress),
            createCharacterInputCount120_,
            applyOwnerSemantics ? 1u : 0u);
        return 1u;
    }

    const auto& input = *static_cast<const ProcessCreateCharacterInput120Sketch*>(input120);
    if (!applyOwnerSemantics) {
        MirrorCreateCharacterInput120SourceBlock(input);
        spdlog::info(
            "CLTLoginMediator::CaptureCreateCharacterInputSlot120(+0x120 mirror-only input={} caller={} [count={}] field12c=0x{:08x} name='{}')",
            fmt::ptr(input120),
            fmt::ptr(returnAddress),
            createCharacterInputCount120_,
            static_cast<unsigned>(createCharacterData108.selectedWorldField24),
            createCharacterData108.characterName00[0]
                ? createCharacterData108.characterName00.data()
                : "<empty>");
        return 0u;
    }

    spdlog::debug(
        "CLTLoginMediator::CaptureCreateCharacterInputSlot120(+0x120 owner-dispatch input={} caller={} [count={}])",
        fmt::ptr(input120),
        fmt::ptr(returnAddress),
        createCharacterInputCount120_);
    return ProcessCreateCharacterInput120(input);
}

// Post-auth margin/loading state ownership (`launcher.exe:0x4f78b8`) shared by the later
// state11 send/reply path and the active existing-character path.

// anchor: launcher.exe:0x41b4b0
bool CLTLoginMediator::State10HasReadyConnectionState2() const {
    // Exact recovered gate from `0x41b4b0`:
    // - owner `+0x1c` must be non-null
    // - connection state field `+0x34` must equal `2`
    const mxo::liblttcp::CMessageConnection_0x4b7928* connection = marginConnection_;
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

// anchor: launcher.exe:0x41f050
uint8_t CLTLoginMediator::GetUnknownByte05() const {
    return unknownByte05_;
}

namespace {

}  // namespace

void CLTLoginMediator::FreeLateEntryList1470StorageScaffold() {
    lateEntryList1470_.clear();
    lateEntryList1470_.shrink_to_fit();
}

// anchor: launcher.exe:0x41f5f0
void CLTLoginMediator::ClearLateEntryList1470Scaffold() {
    lateEntryList1470_.clear();
}

// anchor: launcher.exe:0x41f840 / owner vtable +0x190
void CLTLoginMediator::AppendLateEntryStringTriple1470Scaffold(std::string_view sourceEntry) {
    lateEntryList1470_.emplace_back(sourceEntry);
}

// anchor: launcher.exe:0x41af70
// Original is a thin thunk:
//   - loads margin connection from this+0x1c
//   - tail-jumps to connection->vtable[+0x24] with the caller's stack-local
//     Packet_0x4af2a4-family object still on the stack
// That downstream wrapper reads packetBuilder+0x08 and forwards the retained outer
// message-ref object into `0x448cf0`.
void CLTLoginMediator::SendCurrentMarginPacket(
    mxo::liblttcp::Packet_0x4af2a4& packetBuilder) {
    mxo::liblttcp::CMessageConnection_0x4b7928* const connection = marginConnection_;
    if (!connection) {
        return;
    }

    connection->ForwardPacketBuilderToSendPacket(packetBuilder);
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
    // that gets cleared after copy. In our layout, there's a basic-string helper at +0x18.
    // For fidelity, we read from the original location and copy to gameSessionId664_ (+0x664).
    // Note: The original launcher uses the `0x407dd0` assign-from-range helper for the copy.

    // Access +0x18 string: in original this is a recovered basic-string helper (session token from auth)
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

// source-owned shared helper for `CLTLoginState_State18_0x4b0c00` slot 3 / `0x421a50`
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
// ARG7 SELECTION RESOLUTION (ILTLoginMediator_0x4af2b8 sibling object at 0x4d3584)
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x40d6f0 = ILTLoginMediator_0x4af2b8_ResolveSelectionFromListCtrl (vtable access)
// - launcher.exe:0x40e480 = ILTLoginMediator_0x4af2b8_BuildWorldList (world list construction)
// - launcher.exe:0x40cd10 = ILTLoginMediator_0x4af2b8_GetWorldNameByIndex (fallback path)
// - launcher.exe:0x40cd60 = ILTLoginMediator_0x4af2b8_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_0x4af2b8_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_0x4af2b8_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_0x4af2b8_GetAvailableWorlds
// - launcher.exe:0x40e480 = ILTLoginMediator_0x4af2b8_BuildWorldList / available-world list population
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
// - keep `ILTLoginMediator_0x4af2b8.Default` world-list/selection scaffolding out of the main mediator TU
// - this lets active auth/state8/state9 work avoid rereading arg6 startup-selection code
// - canonical RE references:
//   - `../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_0x4af2b8_Default.md`
//   - late-login arg6 slots `+0xd4/+0x124/+0x18c` live separately under:
//     `../../../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`

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



// anchor: launcher.exe:0x41b500 -> 0x4435f0 / 0x441f30
void CLTLoginMediator::PrepareState5MarginConnectionCopySend() {
    auto* marginConnection = static_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(marginConnection_);
    AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0(
        AuthBootstrapChildFromWriteHelper(*authBootstrapChild680_))
        .PrepareState5MarginConnectionCopySend(*marginConnection);
    marginConnection->SendStoredBootstrapReplyCopy98();
}

// anchor: launcher.exe:0x41e760
void CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold() const {
    const char* profileName = ownerAuthBootstrapSource94_.username00.data();
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

        const Packet_AsAuthReply_0x4b5328& slotRecord = selectionRouteState684_.slotRecordTable04_[i];
        const std::string& route = selectionRouteState684_.routeHostStrings194_[i];
        const char* characterName = slotRecord.debugString14 ? slotRecord.debugString14 : "";
        const char* routeText = route.empty() ? "" : route.c_str();
        std::fprintf(file, "Character%u:=%s,%s\n", static_cast<unsigned>(i), characterName, routeText);
        ++persistedCount;
    }
    std::fclose(file);

    spdlog::info(
        "CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold wrote '{}' characterCount={} currentIndex=0x{:02x}",
        outputPath,
        persistedCount,
        static_cast<unsigned>(characterRouteIndexCc8));
}


// ILTLoginMediator_0x4af2b8::Default - static member initialization (original: launcher.exe:0x4d2c58)
ILTLoginMediator_0x4af2b8* ILTLoginMediator_0x4af2b8::Default = new mxo::ltlogin::CLTLoginMediator();

} // namespace mxo::ltlogin
