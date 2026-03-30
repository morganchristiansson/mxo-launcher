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

#include "loginstate.h"
#include "launcher_mediator_abi_shared.h"
#include "authbootstrap680_internal.h"
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

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall Arg6CurrentSlotRecord44_Destroy(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall Arg6CurrentSlotRecord44_TinyGetter(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall Arg6CurrentSlotRecord44_AppendDebugString(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall Arg6CurrentSlotRecord44_ResetPayloadForSourceDescriptor(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall Arg6CurrentSlotRecord44_TinyHelper(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void** Arg6CurrentSlotRecord44Vtable() {
    static void* vtable[5] = {
        reinterpret_cast<void*>(Arg6CurrentSlotRecord44_Destroy),
        reinterpret_cast<void*>(Arg6CurrentSlotRecord44_TinyGetter),
        reinterpret_cast<void*>(Arg6CurrentSlotRecord44_AppendDebugString),
        reinterpret_cast<void*>(Arg6CurrentSlotRecord44_ResetPayloadForSourceDescriptor),
        reinterpret_cast<void*>(Arg6CurrentSlotRecord44_TinyHelper),
    };
    return vtable;
}

struct LiveSelectionCfgCorpusView {
    uint32_t ready = 0u;
    void* buffer = nullptr;
    uint32_t length = 0u;
};

// UNANCHORED: no original launcher.exe anchor assigned yet.
static const CLTLoginMediator* ResolveActiveSelectionCfgCorpusOwner(const CLTLoginMediator* mediator) {
    return mediator ? mediator->ResolveActiveStateSourceScaffold() : nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static const CLTLoginMediator* ResolveActiveState8PersistenceOwner(const CLTLoginMediator* mediator) {
    // Keep the wrapper-facing split explicit:
    // - arg6 `ILTLoginMediator.Default` entrypoints may be invoked on the binder-owned stub object
    // - the live `mcd.cfg` family still belongs to whichever mediator instance currently owns the
    //   active character/load state when one is registered
    return mediator ? mediator->ResolveActiveStateSourceScaffold() : nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t LogLiveSelectionCfgCorpusFlag(
    const char* slotLabel,
    const char* corpusLabel,
    const char* storageLabel,
    const LiveSelectionCfgCorpusView& view) {
    spdlog::info(
        "{} -> {} [live {} via {} ptr={} length=0x{:04x}]",
        slotLabel ? slotLabel : "<slot>",
        static_cast<unsigned>(view.ready),
        corpusLabel ? corpusLabel : "<corpus>",
        storageLabel ? storageLabel : "<owner fields>",
        fmt::ptr(view.buffer),
        static_cast<unsigned>(view.length));
    if (view.ready == 0u) {
        LogMediatorCharacterStateContext(slotLabel, nullptr);
    }
    return view.ready;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void* LogLiveSelectionCfgCorpusGetter(
    const char* slotLabel,
    const char* corpusLabel,
    const char* storageLabel,
    const LiveSelectionCfgCorpusView& view,
    uint32_t* outLength) {
    if (outLength) {
        *outLength = view.length;
    }
    spdlog::info(
        "{} -> {} [live {} via {} flag={} length=0x{:04x}]",
        slotLabel ? slotLabel : "<slot>",
        fmt::ptr(view.buffer),
        corpusLabel ? corpusLabel : "<corpus>",
        storageLabel ? storageLabel : "<owner fields>",
        static_cast<unsigned>(view.ready),
        static_cast<unsigned>(view.length));
    if (view.buffer == nullptr || view.length == 0u) {
        LogMediatorCharacterStateContext(slotLabel, nullptr);
    }
    return view.buffer;
}

}  // namespace

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      currentState_(nullptr),
      scaffoldState0_(nullptr),
      scaffoldState1_(nullptr),
      scaffoldState2_(nullptr),
      scaffoldState3_(nullptr),
      scaffoldState4_(nullptr),
      scaffoldState5_(nullptr),
      scaffoldState6_(nullptr),
      scaffoldState8_(nullptr),
      scaffoldState9_(nullptr),
      scaffoldState10_(nullptr),
      scaffoldState11_(nullptr),
      scaffoldState12_(nullptr),
      scaffoldState13_(nullptr),
      scaffoldState14_(nullptr),
      scaffoldState15_(nullptr),
      scaffoldState16_(nullptr),
      scaffoldState17_(nullptr),
      scaffoldState18_(nullptr),
      scaffoldState19_(nullptr),
      authConnection_(nullptr),
      marginConnection_(nullptr),
      authConnectionOwnedByMediator_(false),
      marginConnectionOwnedByMediator_(false),
      authConnectionContextKey_(nullptr),
      marginConnectionContextKey_(nullptr),
      helpers_{},
      marginRouteState_{},
      marginAddressList3c_{},
      authBootstrapSource38_{},
      authBootstrapChild680_{},
      sessionCallbackHelper65c_(nullptr),
      state8SelectionContextSnapshotState_{},
      selectionContext0ecCopy_{},
      selectionContext0ecCopyValid_(false),
      selection0ecCount_(0),
      state8PersistenceF1c_{},
      profile0f4Count_(0),
      postAuthMarginLoadingState_{},
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
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();
    InitializeArg6DefaultObject();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginMediator::~CLTLoginMediator() {
    ResetLauncherConnectionBridgeScaffold();
    ClearObserverTree674();
    EraseMarginBootstrapState(this);
    AuthBootstrap680Ops::EraseSidecar(this);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::ResetLauncherConnectionBridgeScaffold() {
    if (authConnection_) {
        authConnection_->SetOwnerContext(nullptr);
    }
    if (marginConnection_) {
        marginConnection_->SetOwnerContext(nullptr);
    }

    if (authConnectionContextScaffold_) {
        std::free(authConnectionContextScaffold_);
        authConnectionContextScaffold_ = nullptr;
    }
    if (marginConnectionContextScaffold_) {
        std::free(marginConnectionContextScaffold_);
        marginConnectionContextScaffold_ = nullptr;
    }

    SetCurrentState(nullptr);
    SetAuthConnectionContextKey(nullptr);
    SetMarginConnectionContextKey(nullptr);
    SetNetworkEngine(nullptr);
    UnregisterActiveStateSourceScaffold(this);

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

    spdlog::info("CLTLoginMediator::ResetLauncherConnectionBridgeScaffold completed");
}

static const char* LauncherConnectionBridgeContext_WorkTypeName(uint32_t workType) {
    switch (workType) {
        case mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeClose:
            return "Close";
        case mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus:
            return "ConnectionStatus";
        case mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeParsedPacket:
            return "ParsedPacket";
        case mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain:
            return "SyntheticReceiveDrain";
        default:
            return "Unknown";
    }
}

// UNANCHORED: source-owned outer-seam bridge callback, intentionally kept out of liblttcp.
uint32_t __thiscall LauncherConnectionBridgeContext_ReleaseScaffold(
    CLTLoginMediatorConnectionContextScaffold* self) {
    spdlog::info(
        "CLTLoginMediator launcher bridge context release self={} label='{}' autoRelease={}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        (self && self->autoReleaseFlag) ? 1u : 0u);
    return 1u;
}

// UNANCHORED: source-owned outer-seam bridge callback, intentionally kept out of liblttcp.
uint32_t __thiscall LauncherConnectionBridgeContext_OnOperationCompletedScaffold(
    CLTLoginMediatorConnectionContextScaffold* self,
    CLTLoginMediatorQueuedWorkItemScaffold* workItem) {
    spdlog::info(
        "CLTLoginMediator launcher bridge OnOperationCompleted context={} label='{}' workItem={} type=0x{:08x} ({}) payload=0x{:08x}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        fmt::ptr(workItem),
        workItem ? workItem->header.workType : 0u,
        LauncherConnectionBridgeContext_WorkTypeName(workItem ? workItem->header.workType : 0u),
        workItem ? workItem->workPayload : 0u);

    CLTLoginMediator* mediator = self ? self->mediator : nullptr;
    if (!self || !workItem || !mediator) {
        return 1u;
    }

    if (workItem->header.workType ==
        mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        const uint32_t handled = self->isMarginConnection
            ? mediator->HandleMarginConnectStatus(workItem->workPayload)
            : mediator->HandleAuthConnectStatus(workItem->workPayload);
        const char* routeLabel = self->isMarginConnection ? "margin" : "auth";
        const char* incomingReplyAnchor = self->isMarginConnection
            ? CLTLoginMediator::kMessageMsLoadCharacterReply
            : CLTLoginMediator::kMessageAsAuthReply;
        spdlog::info(
            "CLTLoginMediator launcher bridge routed {} type-2 connect-status payload=0x{:08x} -> handled={} laterIncomingReplyAnchor='{}'",
            routeLabel,
            static_cast<unsigned>(workItem->workPayload),
            static_cast<unsigned>(handled),
            (incomingReplyAnchor && incomingReplyAnchor[0]) ? incomingReplyAnchor : "<none>");
    }

    if (workItem->header.workType ==
        mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain) {
        if (!self->isMarginConnection) {
            const uint32_t receiveActions = mediator->HandleAuthConnectionReceiveScaffold();
            if (receiveActions & CLTLoginMediator::kReceiveActionBeginMarginAfterAuthReply) {
                const uint32_t marginConnectResult =
                    mediator->BeginLauncherMarginConnectionScaffold();
                spdlog::info(
                    "CLTLoginMediator launcher bridge synthetic receive-drain post-AS_AuthReply margin auto-begin result=0x{:08x}",
                    static_cast<unsigned>(marginConnectResult));
            }
        } else {
            (void)mediator->HandleMarginConnectionReceiveScaffold();
        }
    }

    return 1u;
}

// +0x00
// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetName() {
    return g_MediatorName;
}

// +0x08
// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
    if (authConnection_) {
        authConnection_->SetEngine(engine_);
    }
    if (marginConnection_) {
        marginConnection_->SetEngine(engine_);
        if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(marginConnection_)) {
            marginConnection->SetMarginEngine(engine_);
        }
    }
}

// +0x0c
// UNANCHORED: earlier `0x41f060` anchor was stale; current static RE now assigns that VA to the
// nopatch launcher-version setter instead.
void CLTLoginMediator::ClearEngine() {
    SetNetworkEngine(nullptr);
    spdlog::info("MediatorStub::ClearEngine()");
}
// +0x10
// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::IsReady() {
    spdlog::info("CLTLoginMediator::IsReady() -> 1");
    return 1;
}

// +0x1c
// anchor: launcher.exe:0x41f060
// anchor: launcher.exe:0x409a73..0x409b5f explicit nopatch path
// vtable: ILTLoginMediator.Default slot +0x1c
void CLTLoginMediator::SetValue1(void* value) {
    lastNopatchValue1Ptr_ = value;
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
    lastNopatchValue2Ptr_ = value;
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

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetProfileRootName() const {
    const char* profileRootName = Arg6ProfileName();
    spdlog::debug(
        "CLTLoginMediator::GetProfileRootName(+0x38) -> '{}'",
        NonEmptyTextOrPlaceholder(profileRootName));
    return profileRootName;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const SlotRecordState004b5328* CLTLoginMediator::ResolveArg6CurrentSlotRecord44Source() const {
    const CLTLoginMediator* currentCharacterStateMediator = ResolveActiveStateSourceScaffold();

    const SlotRecordState004b5328* currentSlotRecord =
        currentCharacterStateMediator->GetCurrentSlotRecord();
    if (!currentSlotRecord) {
        currentSlotRecord = currentCharacterStateMediator->GetSlotRecordByIndex(0u);
    }
    return currentSlotRecord;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::RefreshArg6CurrentSlotRecordObject44() {
    arg6CurrentSlotRecord44Payload_ = {};
    arg6CurrentSlotRecord44_ = {};
    arg6CurrentSlotRecord44_.vtable = Arg6CurrentSlotRecord44Vtable();
    arg6CurrentSlotRecord44_.payload10 = &arg6CurrentSlotRecord44Payload_;
    arg6CurrentSlotRecord44NameOwned_.clear();

    if (const SlotRecordState004b5328* currentSlotRecord = ResolveArg6CurrentSlotRecord44Source()) {
        arg6CurrentSlotRecord44Payload_.characterIdLow03 = currentSlotRecord->globalCharacterIdLow03;
        arg6CurrentSlotRecord44Payload_.characterIdHigh07 = currentSlotRecord->globalCharacterIdHigh07;
        arg6CurrentSlotRecord44Payload_.status0b = currentSlotRecord->status0b;
        arg6CurrentSlotRecord44Payload_.worldId0c = currentSlotRecord->worldId0c;
        arg6CurrentSlotRecord44NameOwned_ = currentSlotRecord->heapString14;
    } else {
        const ActiveCharacterStateViewScaffold characterState = DescribeActiveCharacterStateScaffold();
        arg6CurrentSlotRecord44Payload_.characterIdLow03 = characterState.characterIdLow;
        arg6CurrentSlotRecord44Payload_.characterIdHigh07 = characterState.characterIdHigh;
        if (characterState.characterName && characterState.characterName[0]) {
            arg6CurrentSlotRecord44NameOwned_ = characterState.characterName;
        }
    }

    if (!arg6CurrentSlotRecord44NameOwned_.empty()) {
        arg6CurrentSlotRecord44_.heapString14 = arg6CurrentSlotRecord44NameOwned_.c_str();
        const size_t nameLength = arg6CurrentSlotRecord44NameOwned_.size();
        arg6CurrentSlotRecord44_.heapStringLen18 =
            static_cast<uint16_t>((nameLength < 0xffffu) ? nameLength : 0xffffu);
    }

    arg6CurrentSlotRecord44Present_ =
        arg6CurrentSlotRecord44_.heapString14 != nullptr ||
        arg6CurrentSlotRecord44Payload_.characterIdLow03 != 0u ||
        arg6CurrentSlotRecord44Payload_.characterIdHigh07 != 0u;
    return arg6CurrentSlotRecord44Present_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
Arg6SelectionDescriptor40ObjectSketch* CLTLoginMediator::GetArg6SelectionDescriptorObject40(
    uint32_t selectionIndex) {
    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = Arg6ExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = Arg6SelectionDescriptorMatchesRequest(selectionIndex);
    const char* worldName = matchedConfiguredRequest ? Arg6MappedSelectionName() : nullptr;

    if (!worldName) {
        spdlog::debug(
            "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x}) -> NULL (configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x} worldUpperBoundExclusive={})",
            static_cast<unsigned>(selectionIndex),
            static_cast<unsigned>(low24),
            static_cast<unsigned>(high8),
            static_cast<unsigned>(Arg6SelectedWorldIndexLow24()),
            static_cast<unsigned>(Arg6SelectedVariantIndexHigh8()),
            static_cast<unsigned>(expectedScratchRequest),
            static_cast<unsigned>(Arg6WorldUpperBoundExclusive()));
        return nullptr;
    }

    arg6SelectionDescriptor40Packed_ = {};
    arg6SelectionDescriptor40_ = {};
    arg6SelectionDescriptor40Packed_.field03 =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(worldName));
    arg6SelectionDescriptor40Packed_.field07 = Arg6MappedSelectionId();
    arg6SelectionDescriptor40_.packed = &arg6SelectionDescriptor40Packed_;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        ((low24 == Arg6SelectedWorldIndexLow24()) ? "low24-world-match" : "other-match");
    spdlog::debug(
        "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x}) -> {} (matchMode={} mappedName='{}' field03=0x{:08x} field07=0x{:08x} field03AsPtr={} configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x})",
        static_cast<unsigned>(selectionIndex),
        static_cast<unsigned>(low24),
        static_cast<unsigned>(high8),
        fmt::ptr(&arg6SelectionDescriptor40_),
        matchMode,
        worldName,
        static_cast<unsigned>(arg6SelectionDescriptor40Packed_.field03),
        static_cast<unsigned>(arg6SelectionDescriptor40Packed_.field07),
        fmt::ptr(reinterpret_cast<const void*>(static_cast<uintptr_t>(arg6SelectionDescriptor40Packed_.field03))),
        static_cast<unsigned>(Arg6SelectedWorldIndexLow24()),
        static_cast<unsigned>(Arg6SelectedVariantIndexHigh8()),
        static_cast<unsigned>(expectedScratchRequest));
    return &arg6SelectionDescriptor40_;
}

// +0x44
// UNANCHORED: no original launcher.exe anchor assigned yet.
Arg6CurrentSlotRecord44ObjectSketch* CLTLoginMediator::GetArg6CurrentSlotRecordObject44() {
    const bool hasCurrentSlot = RefreshArg6CurrentSlotRecordObject44();
    const void* currentSlotRecordPtr = hasCurrentSlot
        ? static_cast<const void*>(&arg6CurrentSlotRecord44_)
        : nullptr;

    spdlog::info(
        "CLTLoginMediator::GetArg6CurrentSlotRecordObject44(+0x44) -> {} [name='{}' idLow=0x{:08x} idHigh=0x{:08x} status=0x{:02x} worldId=0x{:04x}]",
        fmt::ptr(currentSlotRecordPtr),
        arg6CurrentSlotRecord44_.heapString14 ? arg6CurrentSlotRecord44_.heapString14 : "<empty>",
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.characterIdLow03),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.characterIdHigh07),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.status0b),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.worldId0c));
    return hasCurrentSlot ? &arg6CurrentSlotRecord44_ : nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetWorldOrSelectionName() const {
    const SlotRecordState004b5328* slotRecord = GetCurrentSlotRecord();
    if (!slotRecord) {
        slotRecord = GetSlotRecordByIndex(0u);
    }

    const auto& ownerState = PostAuthMarginLoadingStateView();
    const ActiveCharacterStateViewScaffold characterState = DescribeActiveCharacterStateScaffold();
    const char* worldOrSelectionName = Arg6MappedSelectionName();
    const char* source = "arg6-selection";

    if (slotRecord && !slotRecord->heapString14.empty()) {
        worldOrSelectionName = slotRecord->heapString14.c_str();
        source = "slotRecord+0x14";
    } else if (ownerState.characterNameBufferF1c[0]) {
        worldOrSelectionName = ownerState.characterNameBufferF1c;
        source = "owner+0xf1c";
    } else if (ownerState.sourceLeadString108[0]) {
        worldOrSelectionName = ownerState.sourceLeadString108.data();
        source = "owner+0x108";
    } else if (characterState.characterName && characterState.characterName[0]) {
        worldOrSelectionName = characterState.characterName;
        source = "active-character-state";
    }

    spdlog::debug(
        "CLTLoginMediator::GetWorldOrSelectionName(+0x48) -> '{}' [source={} currentSlot='{}' profile='{}' mappedSelection='{}']",
        NonEmptyTextOrPlaceholder(worldOrSelectionName),
        source,
        (slotRecord && !slotRecord->heapString14.empty())
            ? slotRecord->heapString14.c_str()
            : "<empty>",
        NonEmptyTextOrPlaceholder(Arg6ProfileName()),
        NonEmptyTextOrPlaceholder(Arg6MappedSelectionName()));
    return worldOrSelectionName;
}

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
    return AuthBootstrap680Ops::BootstrapRaw08AuxHandle50(*this);
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
bool CLTLoginMediator::HasBootstrapRaw08AuxHandle54() const {
    return AuthBootstrap680Ops::HasBootstrapRaw08AuxHandle54(*this);
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
uint8_t CLTLoginMediator::GetCrashReporterPromptForSecurId58() const {
    return AuthBootstrap680Ops::GetCrashReporterPromptForSecurId58(*this);
}

// Wrapper-facing launcher/client chain note for `+0x5c/+0x60`:
// - launcher crashreporter seeding calls both slots with no stack argument
// - client `InitClientDLL` uses caller-clean wrappers and threads the previous return value
//   through the next call
// Keep the incoming value opaque here instead of forcing a false `const char*` semantic.
// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetCrashReporterUsername5c(const void* chainedValueToken) {
    const char* authName = Arg6AuthName();
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterUsername5c(+0x5c chainedValueToken={}) -> '{}'",
        fmt::ptr(chainedValueToken),
        NonEmptyTextOrPlaceholder(authName));
    return authName;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetCrashReporterPassword60(const void* chainedValueToken) {
    const char* authPassword = Arg6AuthPassword();
    const char* bootstrapPassword = authBootstrapChild680_.stringF8.begin;
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
uint32_t CLTLoginMediator::HasLiveHlCfg68() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag140e != 0u);
        view.buffer = ownerState->allocatedBuffer1408;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength140c);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveHlCfg68(+0x68)",
        "hl.cfg / state8 section6",
        "owner+0x140e/0x1408/0x140c",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveAnCfg6c() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1416 != 0u);
        view.buffer = ownerState->allocatedBuffer1410;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1414);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveAnCfg6c(+0x6c)",
        "an.cfg / state8 section7",
        "owner+0x1416/0x1410/0x1414",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLivePiCfg70() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag141e != 0u);
        view.buffer = ownerState->allocatedBuffer1418;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength141c);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLivePiCfg70(+0x70)",
        "pi.cfg / state8 section3",
        "owner+0x141e/0x1418/0x141c",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveAiCfg74() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag1426 != 0u);
        view.buffer = ownerState->allocatedBuffer1420;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1424);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveAiCfg74(+0x74)",
        "ai.cfg / state8 section4",
        "owner+0x1426/0x1420/0x1424",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveCsCfg78() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u);
        view.buffer = ownerState->allocatedBuffer1428;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength142c);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveCsCfg78(+0x78)",
        "cs.cfg / state8 section5",
        "owner+0x142e/0x1428/0x142c",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveBlCfg7c() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag13fe != 0u);
        view.buffer = ownerState->allocatedBuffer13f8;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength13fc);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveBlCfg7c(+0x7c)",
        "bl.cfg / state8 section1",
        "owner+0x13fe/0x13f8/0x13fc",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveIlCfg80() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1406 != 0u);
        view.buffer = ownerState->allocatedBuffer1400;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1404);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveIlCfg80(+0x80)",
        "il.cfg / state8 section2",
        "owner+0x1406/0x1400/0x1404",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveRlCfg84() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1448 != 0u);
        view.buffer = ownerState->allocatedBuffer1440;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1444);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveRlCfg84(+0x84)",
        "rl.cfg / state8 section8",
        "owner+0x1448/0x1440/0x1444",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveClCfg88() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1452 != 0u);
        view.buffer = ownerState->allocatedBuffer144c;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1450);
    }
    return LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveClCfg88(+0x88)",
        "cl.cfg / state8 section9",
        "owner+0x1452/0x144c/0x1450",
        view);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasState8PersistenceData8c() const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready =
        (ownerState && ownerState->section0Flag13f6 != 0u) ? 1u : 0u;
    spdlog::info(
        "CLTLoginMediator::HasState8PersistenceData8c(+0x8c) -> {} [owner={} flag13f6={}]",
        ready,
        fmt::ptr(mediator),
        ownerState ? static_cast<unsigned>(ownerState->section0Flag13f6) : 0u);
    return ready;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasLiveCuiCfg90() const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag145a != 0u);
        view.buffer = ownerState->allocatedBuffer1454;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1458);
    }
    const uint32_t result = LogLiveSelectionCfgCorpusFlag(
        "CLTLoginMediator::HasLiveCuiCfg90(+0x90)",
        "cui.cfg / state8 section10",
        "owner+0x145a/0x1454/0x1458",
        view);
    if (result == 0u && !liveCuiCfgAbsentNoteLogged90_) {
        liveCuiCfgAbsentNoteLogged90_ = true;
        spdlog::info(
            "CLTLoginMediator::HasLiveCuiCfg90(+0x90) note: live cui.cfg is absent on the current path; bounded original reruns also omit final cui.cfg, while replacement may still emit an on-disk cui.cfg later through the client-owned direct-save path 0x62198490 -> 0x62197050");
    }
    return result;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveHlCfg94(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag140e != 0u);
        view.buffer = ownerState->allocatedBuffer1408;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength140c);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveHlCfg94(+0x94)",
        "hl.cfg / state8 section6",
        "owner+0x1408/0x140c",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveAnCfg98(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1416 != 0u);
        view.buffer = ownerState->allocatedBuffer1410;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1414);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveAnCfg98(+0x98)",
        "an.cfg / state8 section7",
        "owner+0x1410/0x1414",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLivePiCfg9c(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag141e != 0u);
        view.buffer = ownerState->allocatedBuffer1418;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength141c);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLivePiCfg9c(+0x9c)",
        "pi.cfg / state8 section3",
        "owner+0x1418/0x141c",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveAiCfgA0(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag1426 != 0u);
        view.buffer = ownerState->allocatedBuffer1420;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1424);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveAiCfgA0(+0xa0)",
        "ai.cfg / state8 section4",
        "owner+0x1420/0x1424",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveCsCfgA4(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->allocatedBufferFlag142e != 0u);
        view.buffer = ownerState->allocatedBuffer1428;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength142c);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveCsCfgA4(+0xa4)",
        "cs.cfg / state8 section5",
        "owner+0x1428/0x142c",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveBlCfgA8(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag13fe != 0u);
        view.buffer = ownerState->allocatedBuffer13f8;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength13fc);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveBlCfgA8(+0xa8)",
        "bl.cfg / state8 section1",
        "owner+0x13f8/0x13fc",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveIlCfgAc(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1406 != 0u);
        view.buffer = ownerState->allocatedBuffer1400;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1404);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveIlCfgAc(+0xac)",
        "il.cfg / state8 section2",
        "owner+0x1400/0x1404",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveRlCfgB0(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1448 != 0u);
        view.buffer = ownerState->allocatedBuffer1440;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1444);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveRlCfgB0(+0xb0)",
        "rl.cfg / state8 section8",
        "owner+0x1440/0x1444",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveClCfgB4(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag1452 != 0u);
        view.buffer = ownerState->allocatedBuffer144c;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1450);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveClCfgB4(+0xb4)",
        "cl.cfg / state8 section9",
        "owner+0x144c/0x1450",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetLiveCuiCfgB8(uint32_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveSelectionCfgCorpusOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    LiveSelectionCfgCorpusView view = {};
    if (ownerState) {
        view.ready = static_cast<uint32_t>(ownerState->flag145a != 0u);
        view.buffer = ownerState->allocatedBuffer1454;
        view.length = static_cast<uint32_t>(ownerState->allocatedBufferLength1458);
    }
    return LogLiveSelectionCfgCorpusGetter(
        "CLTLoginMediator::GetLiveCuiCfgB8(+0xb8)",
        "cui.cfg / state8 section10",
        "owner+0x1454/0x1458",
        view,
        outLength);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const void* CLTLoginMediator::GetState8PersistenceHeaderBc() const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const State8PersistenceF1cSnapshot* snapshot =
        mediator ? &mediator->State8PersistenceF1cView() : nullptr;
    const void* header = snapshot ? static_cast<const void*>(snapshot->header2c.data()) : nullptr;
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceHeaderBc(+0xbc) -> {} [owner={} first=0x{:08x} bytes=0x{:02x}]",
        fmt::ptr(header),
        fmt::ptr(mediator),
        snapshot ? static_cast<unsigned>(snapshot->header2c[0]) : 0u,
        snapshot ? static_cast<unsigned>(snapshot->header2c.size() * sizeof(uint32_t)) : 0u);
    return header;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const void* CLTLoginMediator::GetState8PersistenceBodyC0() const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const State8PersistenceF1cSnapshot* snapshot =
        mediator ? &mediator->State8PersistenceF1cView() : nullptr;
    const void* body = snapshot ? static_cast<const void*>(snapshot->body6c.data()) : nullptr;
    const uint32_t bodyWord00 =
        snapshot ? ReadU32LE(snapshot->body6c.data()) : 0u;
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceBodyC0(+0xc0) -> {} [owner={} body00=0x{:08x} bytes=0x{:04x}]",
        fmt::ptr(body),
        fmt::ptr(mediator),
        bodyWord00,
        snapshot ? static_cast<unsigned>(snapshot->body6c.size()) : 0u);
    return body;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::GetState8PersistenceOverflowC4(uint16_t* outLength) const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint16_t length = ownerState ? ownerState->state8Section0OverflowLength13f4 : 0u;
    void* buffer =
        (ownerState && ownerState->state8Section0OverflowLength13f4 != 0u)
            ? ownerState->state8Section0OverflowBuffer13f0
            : nullptr;
    if (outLength) {
        *outLength = length;
    }
    spdlog::info(
        "CLTLoginMediator::GetState8PersistenceOverflowC4(+0xc4) -> {} [owner={} length=0x{:04x}]",
        fmt::ptr(buffer),
        fmt::ptr(mediator),
        static_cast<unsigned>(length));
    return buffer;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HasState8Section11Dword145c() const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t ready =
        (ownerState && ownerState->state8Section11Dword145c != 0u) ? 1u : 0u;
    spdlog::info(
        "CLTLoginMediator::HasState8Section11Dword145c(+0xc8) -> {} [owner={} value=0x{:08x}]",
        ready,
        fmt::ptr(mediator),
        ownerState ? ownerState->state8Section11Dword145c : 0u);
    return ready;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::GetState8Section11Dword145c() const {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;
    const uint32_t value = ownerState ? ownerState->state8Section11Dword145c : 0u;
    spdlog::info(
        "CLTLoginMediator::GetState8Section11Dword145c(+0xcc) -> 0x{:08x} [owner={}]",
        value,
        fmt::ptr(mediator));
    return value;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
RouteDescriptor30SmallStringLikeSketch* CLTLoginMediator::GetState8Section11String1460() {
    const CLTLoginMediator* mediator = ResolveActiveState8PersistenceOwner(this);
    const auto* ownerState = mediator ? &mediator->PostAuthMarginLoadingStateView() : nullptr;

    state8Section11String1460Owned_ = ownerState ? ownerState->state8Section11String1460 : std::string();
    if (!state8Section11String1460Owned_.empty()) {
        state8Section11String1460_.begin = state8Section11String1460Owned_.c_str();
        state8Section11String1460_.current =
            state8Section11String1460_.begin + state8Section11String1460Owned_.size();
        state8Section11String1460_.capacity = state8Section11String1460_.current;
    } else {
        state8Section11String1460_ = {};
    }

    spdlog::info(
        "CLTLoginMediator::GetState8Section11String1460(+0xd0) -> begin={} current={} owner={} text='{}'",
        fmt::ptr(state8Section11String1460_.begin),
        fmt::ptr(state8Section11String1460_.current),
        fmt::ptr(mediator),
        state8Section11String1460Owned_.empty() ? "<empty>" : state8Section11String1460Owned_.c_str());
    return &state8Section11String1460_;
}

// anchor: launcher.exe arg7-selection writer at 0x40d763..0x40d810 consults ILTLoginMediator sibling slot +0xe4
// vtable: ILTLoginMediator.Default slot +0xe4
uint8_t CLTLoginMediator::GetVariantState(int32_t variantIndex) const {
    uint32_t state = 3u;
    if (variantIndex >= 0) {
        const uint32_t unsignedVariantIndex = static_cast<uint32_t>(variantIndex);
        if (unsignedVariantIndex < this->Arg6VariantUpperBoundExclusive() &&
            this->Arg6VariantIndexMatchesSelection(unsignedVariantIndex)) {
            state = this->Arg6SelectedVariantState();
        }
    }
    spdlog::info(
        "MediatorStub::GetVariantState(+0xe4 variantIndex={}) -> {} (configuredVariant=0x{:02x} configuredState={})",
        variantIndex,
        state,
        this->Arg6SelectedVariantIndexHigh8(),
        this->Arg6SelectedVariantState());
    return state;
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

    spdlog::info(
        "CLTLoginMediator::RequestAuthConnectionCloseWaitEvent1(+0x164 wrapper-facing) -> 1 [owner+0x2c={} authConnectionState={} wouldCallConnectionClose0cArg1={} currentReplacementDoesNotInvokeCloseYet=1]",
        static_cast<unsigned>(authConnectionFlag2c_),
        rawState,
        wouldCallConnectionClose0c ? 1u : 0u);
    return true;
}

// anchor: launcher.exe:0x41f240
// vtable: ILTLoginMediator.Default slot +0x178
uint32_t CLTLoginMediator::GetLastLoginStatus() {
    // Keep this wrapper-facing slot as close as practical to the original tiny getter
    // `0x41f240: mov eax, [ecx+0x80] ; ret`.
    return this->WorldListCountOrStatus80();
}


// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetCurrentState(CLTLoginState* state) {
    currentState_ = state;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
CLTLoginState* CLTLoginMediator::CurrentState() const {
    return currentState_;
}

// anchor: launcher.exe:0x41f250 / owner vtable `+0x180`
uint32_t CLTLoginMediator::DispatchCurrentHelperAuthMessageScaffold(void* workItem) {
    if (!currentState_) {
        spdlog::info(
            "CLTLoginMediator::DispatchCurrentHelperAuthMessageScaffold skipped because currentState is null workItem={}",
            fmt::ptr(workItem));
        return 0u;
    }

    return currentState_->AuthMessageDispatch(workItem, this);
}

// anchor: launcher.exe:0x41afc0 -> current helper vtable `+0x04`
uint32_t CLTLoginMediator::DispatchCurrentHelperSecondaryGateScaffold(void* workItem) {
    if (!currentState_) {
        spdlog::info(
            "CLTLoginMediator::DispatchCurrentHelperSecondaryGateScaffold skipped because currentState is null workItem={}",
            fmt::ptr(workItem));
        return 0u;
    }

    return currentState_->Slot2_HandleSecondaryGate(workItem, this);
}

// UNANCHORED: source-owned staging wrapper for the narrowed auth-side
// `0x4490c0 -> local message-ref/base-filter -> 0x449a30 -> owner+0x180` receive seam.
// Current role is narrower than the earlier raw-byte bridge:
// - the auth leaf now source-owns the nearer receive/message-ref pre-owner step
// - this mediator helper is therefore only the surviving owner-slot-5 consumer on that branch
uint32_t CLTLoginMediator::StageAuthPacketBytesAndDispatchCurrentHelperScaffold(
    const uint8_t* packetBytes,
    size_t packetSize,
    void* workItem) {
    if (!packetBytes || packetSize == 0u) {
        return 0u;
    }

    stagedIncomingAuthPacketBytes_.assign(packetBytes, packetBytes + packetSize);
    const uint8_t rawCode = stagedIncomingAuthPacketBytes_[0];
    const uint32_t handled = DispatchCurrentHelperAuthMessageScaffold(workItem);
    spdlog::info(
        "CLTLoginMediator::StageAuthPacketBytesAndDispatchCurrentHelperScaffold rawCode=0x{:02x} packetSize={} currentState={} handled={}",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(packetSize),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(handled));
    return handled;
}

// UNANCHORED: source-owned staging wrapper for the remaining margin-side receive branches.
// Current narrowed role:
// - consumed decoded-code-2/4 handling can now be taken earlier at the connection/leaf seam
// - this helper stays the broader fallback/remaining consumer for other margin receive paths
uint32_t CLTLoginMediator::StageMarginPacketBytesAndDispatchCurrentHelperScaffold(
    const uint8_t* packetBytes,
    size_t packetSize,
    void* workItem) {
    if (!packetBytes || packetSize == 0u) {
        return 0u;
    }

    const uint8_t rawCode = packetBytes[0];
    const uint32_t handled = HandleMarginPacketBytes(packetBytes, packetSize, workItem);
    spdlog::info(
        "CLTLoginMediator::StageMarginPacketBytesAndDispatchCurrentHelperScaffold rawCode=0x{:02x} packetSize={} currentState={} handled={}",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(packetSize),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(handled));
    return handled;
}

// anchor: launcher.exe:0x41afc0 / owner vtable `+0x188`
uint32_t CLTLoginMediator::HandleMarginConnectionCompletionFallbackScaffold(
    mxo::liblttcp::CMessageConnection* connection,
    void* workItem) {
    if (!connection || !workItem || connection != marginConnection_) {
        return 0u;
    }

    const auto* workHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    const uint32_t workType = workHeader ? workHeader->workType : 0u;
    if (workType == mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        marginConnection_ = nullptr;
        marginConnectionContextKey_ = nullptr;
    }

    const uint32_t handled = DispatchCurrentHelperSecondaryGateScaffold(workItem);
    spdlog::info(
        "CLTLoginMediator::HandleMarginConnectionCompletionFallbackScaffold workType=0x{:08x} thisConnection={} currentState={} handled={} ownerMarginConnection={} ownerContextKey={}",
        static_cast<unsigned>(workType),
        fmt::ptr(connection),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(handled),
        fmt::ptr(marginConnection_),
        fmt::ptr(marginConnectionContextKey_));
    return handled;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::State6UdpSessionSecretF18() const {
    const auto it = g_marginBootstrapStateByMediator.find(this);
    return (it != g_marginBootstrapStateByMediator.end()) ? it->second.state6UdpSessionSecretF18 : 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetState6UdpSessionSecretF18(uint32_t value) {
    MutableMarginBootstrapState(this).state6UdpSessionSecretF18 = value;
}

// anchor: launcher.exe:0x41b4f0
const void* CLTLoginMediator::GetState9CallbackSeedPointer85D4() const {
    if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(marginConnection_)) {
        if (const uint8_t* seedPointer = marginConnection->MessageCode5SeedBytes85Pointer()) {
            spdlog::info(
                "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> {} [source=connection+0x85 mirror connection={}]",
                fmt::ptr(seedPointer),
                fmt::ptr(marginConnection));
            return seedPointer;
        }
    }

    const auto it = g_marginBootstrapStateByMediator.find(this);
    if (it != g_marginBootstrapStateByMediator.end() && it->second.marginTwofishKeyBytes.size() == 16u) {
        const void* seedPointer = it->second.marginTwofishKeyBytes.data();
        spdlog::info(
            "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> {} [source=bootstrap-sidecar-fallback marginConnection={} original+0xd4=owner+0x1c+0x85]",
            fmt::ptr(seedPointer),
            fmt::ptr(marginConnection_));
        return seedPointer;
    }

    spdlog::info(
        "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> <null> [marginConnection={} expectedLiveSource=owner+0x1c+0x85]",
        fmt::ptr(marginConnection_));
    return nullptr;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HandleMarginConnectStatus(uint32_t workResultCode) {
    lastMarginConnectStatus_ = workResultCode;
    ++marginConnectStatusCount_;
    if (workResultCode == kConnectStatusSuccess && marginConnection_ != nullptr) {
        // Active state8/state10 sender gates at `0x41b4b0` require owner `+0x1c` connection state `== 2`.
        // On the current scaffold path the successful margin connect-status callback is the
        // narrowest evidence-backed place to promote the live margin connection into that ready
        // send state before slot-3 send bodies run.
        marginConnection_->SetState(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);
        spdlog::info(
            "CLTLoginMediator::HandleMarginConnectStatus promoted margin connection to ready state=2 after connect-status success currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return (workResultCode == kConnectStatusSuccess) ? BeginMarginHandshake() : 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::BeginMarginHandshake() {
    // Important ownership split to keep explicit in source:
    // - mediator owns the shared margin-connection transport helper (`0x41e500` family)
    // - the concrete post-bootstrap payload send body remains on the active `CLTLoginState`
    //   vtable object
    // - mediator only owns the launcher-side CERT/MS bootstrap progression that must complete on
    //   the connected margin transport before state8/state11 payload sends like raw `0x0f`
    if (!currentState_) {
        spdlog::warn("DIAGNOSTIC: BeginMarginHandshake has no active CLTLoginState to dispatch");
        return 0u;
    }

    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    switch (marginBootstrapState.phase) {
        case MarginBootstrapPhase::kReady:
            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap already complete; returning control to current state slot3 currentState={} sessionId=0x{:08x}",
                currentState_->DebugName(),
                marginBootstrapState.marginSessionId);
            return currentState_->Slot3_BeginOrContinue(/*upstreamOrArg=*/currentState_, this);

        case MarginBootstrapPhase::kIdle:
            break;

        case MarginBootstrapPhase::kSentCertConnectRequest:
        case MarginBootstrapPhase::kSentCertChallengeResponse:
        case MarginBootstrapPhase::kSentMsConnectRequest:
        case MarginBootstrapPhase::kSentMsConnectChallengeResponse:
            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap already in progress phase={} waitingOn='{}' currentState={}",
                static_cast<uint32_t>(marginBootstrapState.phase),
                expectedMarginRequestName_ ? expectedMarginRequestName_ : "<unset>",
                currentState_->DebugName());
            return 1u;
    }

    if (!lastAuthReply_.signedData.valid || lastAuthReply_.signedData.rawBytes.empty() ||
        lastAuthReply_.authSignatureBytes.empty()) {
        spdlog::info(
            "DIAGNOSTIC: BeginMarginHandshake missing auth-reply signed-data material for CERT_ConnectRequest currentState={}",
            currentState_->DebugName());
        return 0u;
    }

    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildMarginCertConnectRequestPacket(
            lastAuthReply_,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        spdlog::info("DIAGNOSTIC: BeginMarginHandshake failed to build CERT_ConnectRequest");
        return 0u;
    }

    const uint32_t sendResult = SendMarginFramedPacket(
        packet,
        0x01u,
        "CERT_ConnectRequest",
        /*encryptedTransport=*/false);
    if (sendResult != 0u) {
        marginBootstrapState.phase = MarginBootstrapPhase::kSentCertConnectRequest;
        expectedMarginRequestName_ = "CERT_Challenge";
    }
    return sendResult;
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

// +0xec
// anchor: launcher.exe:0x41c1f0
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

    state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8 =
        static_cast<uint8_t>(input.slotOrSelectionIndex00);
    std::copy(input.block04.begin(), input.block04.end(), state8SelectionContextSnapshotState_.blockCd0.begin());
    std::copy(input.block14.begin(), input.block14.end(), state8SelectionContextSnapshotState_.blockCe0.begin());
    std::copy(input.block24.begin(), input.block24.end(), state8SelectionContextSnapshotState_.blockCf0.begin());
    std::copy(input.block34.begin(), input.block34.end(), state8SelectionContextSnapshotState_.blockD00.begin());
    std::copy(input.block44.begin(), input.block44.end(), state8SelectionContextSnapshotState_.blockD10.begin());
    std::copy(input.block54.begin(), input.block54.end(), state8SelectionContextSnapshotState_.blockD20.begin());
    std::copy(input.block64.begin(), input.block64.end(), state8SelectionContextSnapshotState_.blockD30.begin());
    std::copy(input.block74.begin(), input.block74.end(), state8SelectionContextSnapshotState_.blockD40.begin());
    std::copy(input.block84.begin(), input.block84.end(), state8SelectionContextSnapshotState_.blockD50.begin());
    std::copy(input.block94.begin(), input.block94.end(), state8SelectionContextSnapshotState_.blockD60.begin());
    std::copy(input.blockA4.begin(), input.blockA4.end(), state8SelectionContextSnapshotState_.blockD70.begin());

    if (scaffoldState8_ != nullptr) {
        SwitchHelperStateScaffold(8u, scaffoldState8_);
    }

    spdlog::info(
        "CLTLoginMediator::PersistSelectionContextForState8 mirrored owner-advanced state3(wait)->state8 selection snapshot slot=0x{:02x} blockCd0_0=0x{:08x} blockD70_3=0x{:08x} currentState={}",
        state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8,
        state8SelectionContextSnapshotState_.blockCd0[0],
        state8SelectionContextSnapshotState_.blockD70[3],
        currentState_ ? currentState_->DebugName() : "<unchanged>");
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const CLTLoginMediator::State8PersistenceF1cSnapshot& CLTLoginMediator::State8PersistenceF1cView() const {
    auto copyCStringIntoFixed = [](std::array<char, 0x20>& dest, const char* src) {
        std::fill(dest.begin(), dest.end(), '\0');
        if (!src || !src[0]) {
            return;
        }
        const size_t copyCount = std::min(std::char_traits<char>::length(src), dest.size() - 1u);
        std::memcpy(dest.data(), src, copyCount);
        dest[copyCount] = '\0';
    };
    auto copyCStringIntoByteSpan = [](uint8_t* dest, size_t destSize, const char* src) {
        if (!dest || destSize == 0u) {
            return;
        }
        std::memset(dest, 0, destSize);
        if (!src || !src[0]) {
            return;
        }
        const size_t copyCount = std::min(std::char_traits<char>::length(src), destSize - 1u);
        std::memcpy(dest, src, copyCount);
        dest[copyCount] = '\0';
    };
    auto preferNonEmpty = [](const char* primary, const char* fallback) {
        return (primary && primary[0]) ? primary : fallback;
    };

    state8PersistenceF1c_ = {};

    const ActiveCharacterStateViewScaffold characterState = DescribeOwnCharacterStateScaffold();
    const auto& ownerState = PostAuthMarginLoadingStateView();
    const char* characterName = characterState.characterName;
    const char* realFirstName = characterState.realFirstName;
    const char* realLastName = characterState.realLastName;
    const char* background = characterState.background;

    state8PersistenceF1c_.field24 = SourceField12c();
    if (state8PersistenceF1c_.field24 == 0u) {
        state8PersistenceF1c_.field24 = Arg6SelectedWorldIndexLow24();
    }

    characterName = preferNonEmpty(characterName, ownerState.characterNameBufferF1c);
    realFirstName = preferNonEmpty(realFirstName, ownerState.characterNameBufferF1c);
    realLastName = preferNonEmpty(realLastName, ownerState.characterNameBufferF1c);
    background = preferNonEmpty(background, ownerState.characterNameBufferF1c);

    copyCStringIntoFixed(state8PersistenceF1c_.string00, characterName);
    state8PersistenceF1c_.field20 = ownerState.characterReplyFieldF3c;
    state8PersistenceF1c_.field24 =
        ownerState.characterReplyFieldF40 ? ownerState.characterReplyFieldF40 : state8PersistenceF1c_.field24;
    state8PersistenceF1c_.field28 = ownerState.characterReplyFieldF44;
    std::copy(ownerState.characterFlagsF48.begin(), ownerState.characterFlagsF48.end(), state8PersistenceF1c_.header2c.begin());
    std::copy(ownerState.secondaryCharacterDataF68.begin(), ownerState.secondaryCharacterDataF68.end(), state8PersistenceF1c_.secondary4c.begin());
    std::memcpy(state8PersistenceF1c_.body6c.data(), ownerState.state8Section0RawF88.data(), state8PersistenceF1c_.body6c.size());

    copyCStringIntoByteSpan(state8PersistenceF1c_.body6c.data() + 0x04, 0x20, realFirstName);
    copyCStringIntoByteSpan(state8PersistenceF1c_.body6c.data() + 0x24, 0x20, realLastName);
    copyCStringIntoByteSpan(state8PersistenceF1c_.body6c.data() + 0x44, 0x400, background);

    if (ownerState.replySectionData13cc != 0u) {
        std::memcpy(
            state8PersistenceF1c_.body6c.data() + 0x444,
            &ownerState.replySectionData13cc,
            sizeof(uint32_t));
    }
    if (ownerState.replySectionData13d0 != 0u) {
        std::memcpy(
            state8PersistenceF1c_.body6c.data() + 0x448,
            &ownerState.replySectionData13d0,
            sizeof(uint32_t));
    }

    return state8PersistenceF1c_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const void* CLTLoginMediator::GetState8PersistenceF1c() const {
    const State8PersistenceF1cSnapshot& snapshot = State8PersistenceF1cView();
    ++profile0f4Count_;
    const auto& ownerState = PostAuthMarginLoadingStateView();
    const char* firstName = reinterpret_cast<const char*>(snapshot.body6c.data() + 0x04);
    const char* lastName = reinterpret_cast<const char*>(snapshot.body6c.data() + 0x24);
    const char* background = reinterpret_cast<const char*>(snapshot.body6c.data() + 0x44);
    spdlog::debug(
        "CLTLoginMediator::GetState8PersistenceF1c(+0xf4) -> {} [count={} copiedFrom0ec={} valid0ec={} char='{}' first='{}' last='{}' background='{}' field24=0x{:08x} overflow13f4=0x{:04x}]",
        fmt::ptr(&snapshot),
        profile0f4Count_,
        selection0ecCount_,
        selectionContext0ecCopyValid_ ? 1u : 0u,
        snapshot.string00[0] ? snapshot.string00.data() : "<empty>",
        firstName && firstName[0] ? firstName : "<empty>",
        lastName && lastName[0] ? lastName : "<empty>",
        background && background[0] ? background : "<empty>",
        static_cast<unsigned>(snapshot.field24),
        static_cast<unsigned>(ownerState.state8Section0OverflowLength13f4));
    return &snapshot;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::MirrorProcessLoginCredentialsSourceBlock120(const ProcessLoginCredentialsInputSketch& input) {
    std::copy(input.string00.begin(), input.string00.end(), postAuthMarginLoadingState_.sourceLeadString108.begin());
    postAuthMarginLoadingState_.sourceField12c = input.field24;

    std::copy(input.dwords2c.begin(), input.dwords2c.end(), postAuthMarginLoadingState_.sourceDwords134.begin());
    std::copy(input.dwords4c.begin(), input.dwords4c.end(), postAuthMarginLoadingState_.sourceDwords134.begin() + 8);
    postAuthMarginLoadingState_.sourceDwords134[16] =
        static_cast<uint32_t>(input.bytes6c[0]) |
        (static_cast<uint32_t>(input.bytes6c[1]) << 8) |
        (static_cast<uint32_t>(input.bytes6c[2]) << 16) |
        (static_cast<uint32_t>(input.bytes6c[3]) << 24);

    std::copy(input.string70.begin(), input.string70.end(), postAuthMarginLoadingState_.sourceBlock178.begin());
    std::copy(input.string90.begin(), input.string90.end(), postAuthMarginLoadingState_.sourceBlock198.begin());
    std::copy(input.stringB0.begin(), input.stringB0.end(), postAuthMarginLoadingState_.sourceBlock1b8.begin());
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::MirrorCharacterSeedIntoSourceBlock120Scaffold(
    const char* characterName,
    uint32_t selectedWorldIndexLow24) {
    auto copyCStringIntoFixed = [](auto& dest, const char* src) {
        std::fill(dest.begin(), dest.end(), 0);
        if (!src || !src[0]) {
            return;
        }
        const size_t copyCount = std::min(std::char_traits<char>::length(src), dest.size() - 1u);
        std::memcpy(dest.data(), src, copyCount);
        dest[copyCount] = '\0';
    };

    if (!characterName || !characterName[0]) {
        spdlog::info(
            "CLTLoginMediator::MirrorCharacterSeedIntoSourceBlock120Scaffold skipped (empty characterName)");
        return 1u;
    }

    ProcessLoginCredentialsInputSketch input = {};
    const ActiveCharacterStateViewScaffold characterState = DescribeOwnCharacterStateScaffold();
    copyCStringIntoFixed(input.string00, characterName);
    input.field24 = selectedWorldIndexLow24 & 0x00ffffffu;
    copyCStringIntoFixed(input.string70, characterState.realFirstName);
    copyCStringIntoFixed(input.string90, characterState.realLastName);
    copyCStringIntoFixed(input.stringB0, characterState.background);

    MirrorProcessLoginCredentialsSourceBlock120(input);
    spdlog::info(
        "CLTLoginMediator::MirrorCharacterSeedIntoSourceBlock120Scaffold name='{}' field12c=0x{:08x} realFirst='{}' realLast='{}' background='{}' currentState={} (mirror-only; no 0x41c3c0 state-3 gate claim)",
        postAuthMarginLoadingState_.sourceLeadString108[0]
            ? postAuthMarginLoadingState_.sourceLeadString108.data()
            : "<empty>",
        static_cast<unsigned>(postAuthMarginLoadingState_.sourceField12c),
        postAuthMarginLoadingState_.sourceBlock178[0]
            ? reinterpret_cast<const char*>(postAuthMarginLoadingState_.sourceBlock178.data())
            : "<empty>",
        postAuthMarginLoadingState_.sourceBlock198[0]
            ? reinterpret_cast<const char*>(postAuthMarginLoadingState_.sourceBlock198.data())
            : "<empty>",
        postAuthMarginLoadingState_.sourceBlock1b8[0]
            ? reinterpret_cast<const char*>(postAuthMarginLoadingState_.sourceBlock1b8.data())
            : "<empty>",
        currentState_ ? currentState_->DebugName() : "<null>");
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::CaptureProcessLoginCredentialsArg6Slot120(
    const void* input120,
    void* returnAddress,
    bool applyOwnerSemantics) {
    arg6ProcessLoginCredentialsInput120_ = input120;
    ++arg6ProcessLoginCredentialsCount120_;

    if (!input120) {
        spdlog::info(
            "CLTLoginMediator::CaptureProcessLoginCredentialsArg6Slot120(+0x120) input=<null> caller={} [count={}] applyOwnerSemantics={}",
            fmt::ptr(returnAddress),
            arg6ProcessLoginCredentialsCount120_,
            applyOwnerSemantics ? 1u : 0u);
        return 1u;
    }

    const auto& input = *static_cast<const ProcessLoginCredentialsInputSketch*>(input120);
    if (!applyOwnerSemantics) {
        MirrorProcessLoginCredentialsSourceBlock120(input);
        spdlog::info(
            "CLTLoginMediator::CaptureProcessLoginCredentialsArg6Slot120(+0x120 mirror-only input={} caller={} [count={}] field12c=0x{:08x} name='{}')",
            fmt::ptr(input120),
            fmt::ptr(returnAddress),
            arg6ProcessLoginCredentialsCount120_,
            static_cast<unsigned>(postAuthMarginLoadingState_.sourceField12c),
            postAuthMarginLoadingState_.sourceLeadString108[0]
                ? postAuthMarginLoadingState_.sourceLeadString108.data()
                : "<empty>");
        return 0u;
    }

    spdlog::debug(
        "CLTLoginMediator::CaptureProcessLoginCredentialsArg6Slot120(+0x120 owner-dispatch input={} caller={} [count={}])",
        fmt::ptr(input120),
        fmt::ptr(returnAddress),
        arg6ProcessLoginCredentialsCount120_);
    return ProcessLoginCredentials(input);
}

// anchor: launcher.exe:0x41c3c0
uint32_t CLTLoginMediator::ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input) {
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
                "CLTLoginMediator::ProcessLoginCredentials(+0x120) rejected currentState={} stateCode={} -> 0x12000006",
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
                "CLTLoginMediator::ProcessLoginCredentials(+0x120) rejected currentState={} stateCode={} -> 0x12000000",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 0x12000000u;
        case 12u:
            spdlog::info(
                "CLTLoginMediator::ProcessLoginCredentials(+0x120) rejected currentState={} stateCode={} -> 0x12000007",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 0x12000007u;
        default:
            spdlog::info(
                "CLTLoginMediator::ProcessLoginCredentials(+0x120) rejected currentState={} stateCode={} -> 0x00000001",
                currentState_ ? currentState_->DebugName() : "<null>",
                stateCode);
            return 1u;
    }

    if (static_cast<uint32_t>(worldDescriptorCountD80_) < input.field24) {
        spdlog::info(
            "CLTLoginMediator::ProcessLoginCredentials(+0x120) rejected selector field12c=0x{:08x} upperBoundF8=0x{:02x}",
            static_cast<unsigned>(input.field24),
            static_cast<unsigned>(worldDescriptorCountD80_));
        return 4u;
    }

    MirrorProcessLoginCredentialsSourceBlock120(input);

    if (scaffoldState10_ != nullptr) {
        SwitchHelperStateScaffold(10u, scaffoldState10_);
    }

    spdlog::info(
        "CLTLoginMediator::ProcessLoginCredentials(+0x120 owner) name='{}' field12c=0x{:08x} firstDword134=0x{:08x} backgroundPreview='{}' -> currentState={}",
        postAuthMarginLoadingState_.sourceLeadString108[0]
            ? postAuthMarginLoadingState_.sourceLeadString108.data()
            : "<empty>",
        static_cast<unsigned>(postAuthMarginLoadingState_.sourceField12c),
        static_cast<unsigned>(postAuthMarginLoadingState_.sourceDwords134[0]),
        postAuthMarginLoadingState_.sourceBlock1b8[0]
            ? reinterpret_cast<const char*>(postAuthMarginLoadingState_.sourceBlock1b8.data())
            : "<empty>",
        currentState_ ? currentState_->DebugName() : "<null>");
    return 0u;
}

// Post-auth margin/loading state ownership (`launcher.exe:0x4f78b8`) shared by the later
// state11 send/reply path and the active existing-character path.

// anchor: launcher.exe:0x41b4b0
bool CLTLoginMediator::State10HasReadyConnectionState2() const {
    // Exact recovered gate from `0x41b4b0`:
    // - owner `+0x1c` must be non-null
    // - connection state field `+0x34` must equal `2`
    const mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    return connection != nullptr &&
           connection->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// Accessors for migrated state fields (diagnostics only)
// UNANCHORED: no original launcher.exe anchor assigned yet.
const void* CLTLoginMediator::LastNopatchValue1Ptr() const {
    return lastNopatchValue1Ptr_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const void* CLTLoginMediator::LastNopatchValue2Ptr() const {
    return lastNopatchValue2Ptr_;
}

// anchor: launcher.exe:0x41f070
const uint32_t* CLTLoginMediator::GetNoPatchLauncherVersionValuePtr08() const {
    return &nopatchLauncherVersionValue08_;
}

// anchor: launcher.exe:0x41f090
const uint32_t* CLTLoginMediator::GetNoPatchClientVersionValuePtr0c() const {
    return &nopatchClientVersionValue0c_;
}

// anchor: launcher.exe:0x41f310
SessionCallbackHelper65cSketch* CLTLoginMediator::GetSessionCallbackHelper65c() const {
    // Tiny owner-vtable getter used by the later session-callback helper family.
    return sessionCallbackHelper65c_;
}

// anchor: launcher.exe:0x41af50
LateEntryList1470VectorLikeSketch* CLTLoginMediator::GetLateEntryList1470() {
    // Keep the wrapper-facing arg6 `+0x118` vector-like object explicit instead of leaving this
    // scratch state in the ABI shell.
    const LateEntryList1470EntrySketch* begin =
        lateEntryList1470Entries_.capacity() ? lateEntryList1470Entries_.data() : nullptr;
    lateEntryList1470_.begin = begin;
    lateEntryList1470_.current = begin ? (begin + lateEntryList1470Entries_.size()) : nullptr;
    lateEntryList1470_.capacity = begin ? (begin + lateEntryList1470Entries_.capacity()) : nullptr;

    spdlog::info(
        "CLTLoginMediator::GetLateEntryList1470(+0x118) -> begin={} current={} capacity={} entryCount={} entryCapacity={}{}",
        fmt::ptr(lateEntryList1470_.begin),
        fmt::ptr(lateEntryList1470_.current),
        fmt::ptr(lateEntryList1470_.capacity),
        static_cast<unsigned>(lateEntryList1470Entries_.size()),
        static_cast<unsigned>(lateEntryList1470Entries_.capacity()),
        lateEntryList1470Entries_.empty() ? " (empty scaffold)" : "");
    return &lateEntryList1470_;
}

// anchor: launcher.exe:0x41f320
const char* CLTLoginMediator::GetGameSessionId() const {
    // Important fidelity correction from fresh original-launcher WineDbg on the natural first
    // state8 send:
    // - original `0x41f320` returns owner `this + 0x664` directly
    // - the caller then forwards that pointer into `0x43ada0` even when the string is empty
    // So this getter must preserve the original non-null empty-string behavior instead of
    // collapsing empty state to nullptr.
    return gameSessionId664_.c_str();
}

// anchor: launcher.exe:0x41af70
uint32_t CLTLoginMediator::SendCurrentMarginPacketScaffold(
    mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& envelope) {
    // anchor: launcher.exe:0x41af70
    // Keep this narrow: `0x41af70` forwards the local packet-builder envelope through the current
    // margin connection send bridge (`0x41cf30 -> 0x448cf0`) instead of flattening caller bytes
    // directly.
    mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    if (!connection) {
        connection = EnsureMarginConnectionObject();
    }

    mxo::liblttcp::CMessageConnectionMessageRefScaffold* const messageRef = envelope.messageRef08;
    mxo::liblttcp::CMessageConnectionMessageScaffold* const messageStorage =
        messageRef ? messageRef->messageStorage0c : nullptr;
    if (!connection || !messageRef || !messageStorage) {
        return 0u;
    }

    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    if (marginBootstrapState.phase == MarginBootstrapPhase::kReady &&
        !marginBootstrapState.marginTwofishKeyBytes.empty()) {
        const uint8_t* payloadBytes = messageStorage->PayloadBaseScaffold();
        const uint32_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
        mxo::auth::FramedPacket encryptedPacket;
        if (!payloadBytes || payloadByteCount == 0u ||
            !mxo::auth::EncryptMarginPayloadPacket(
                payloadBytes,
                payloadByteCount,
                marginBootstrapState.marginTwofishKeyBytes,
                mxo::auth::kFrameModeAuto,
                &encryptedPacket)) {
            spdlog::warn(
                "CLTLoginMediator::SendCurrentMarginPacketScaffold failed to encrypt post-bootstrap margin payload rawOpcode=0x{:02x} payloadBytes={} state={} sessionId=0x{:08x}",
                payloadBytes ? static_cast<unsigned>(payloadBytes[0]) : 0u,
                static_cast<unsigned>(payloadByteCount),
                static_cast<unsigned>(connection->State()),
                static_cast<unsigned>(marginBootstrapState.marginSessionId));
            return 0u;
        }

        spdlog::info(
            "CLTLoginMediator::SendCurrentMarginPacketScaffold encrypted post-bootstrap margin payload rawOpcode=0x{:02x} payloadBytes={} outerHeaderBytes={} outerPayloadBytes={} sessionId=0x{:08x} host='{}' state={}",
            static_cast<unsigned>(payloadBytes[0]),
            static_cast<unsigned>(payloadByteCount),
            static_cast<unsigned>(encryptedPacket.headerBytes.size()),
            static_cast<unsigned>(encryptedPacket.payloadBytes.size()),
            static_cast<unsigned>(marginBootstrapState.marginSessionId),
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
            static_cast<unsigned>(connection->State()));
        return connection->SendBuffer(
            encryptedPacket.bytes.data(),
            static_cast<uint32_t>(encryptedPacket.bytes.size()),
            nullptr);
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    const uint8_t* const payloadBase = messageStorage->PayloadBaseScaffold();
    const size_t submitOffset = ((messageStorage->payloadLengthHigh0a >> 7) == 0u) ? 1u : 0u;
    const uint32_t submittedByteCount =
        static_cast<uint32_t>(payloadByteCount) + ((payloadByteCount > 0x7fu) ? 2u : 1u);
    const uint8_t* const submittedBytes =
        (payloadBase && payloadByteCount != 0u) ? (payloadBase - 2u + submitOffset) : nullptr;
    const std::string submittedPreview =
        (submittedBytes && submittedByteCount != 0u)
            ? BuildHexPreview(submittedBytes, submittedByteCount, 32u)
            : std::string();
    spdlog::info(
        "CLTLoginMediator::SendCurrentMarginPacketScaffold ForwardPacketBuilderEnvelopeToSendPacket host='{}' state={} reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} submitOffset={} preview={} agendaGap=packet-processing-metadata-still-missing",
        connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
        static_cast<unsigned>(connection->State()),
        static_cast<unsigned>(messageStorage->reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<unsigned>(submitOffset),
        submittedPreview.empty() ? std::string("<empty>") : submittedPreview);
    return connection->ForwardPacketBuilderEnvelopeToSendPacket(envelope);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::SendCurrentMarginPacketScaffold(const void* packetBytes, uint32_t packetByteCount) {
    if (!packetBytes || packetByteCount == 0u ||
        packetByteCount > mxo::liblttcp::CMessageConnectionMessageScaffold::kMaxPayloadByteCount) {
        return 0u;
    }

    mxo::liblttcp::CMessageConnectionMessageRefScaffold messageRef = {};
    messageRef.ResetForPacketBuilderScaffold(/*headerless=*/false);
    if (!messageRef.messageStorage0c) {
        return 0u;
    }

    messageRef.messageStorage0c->ResetPayloadByteCountScaffold(static_cast<uint16_t>(packetByteCount));
    uint8_t* const payloadBytes = messageRef.messageStorage0c->PayloadBaseScaffold();
    if (!payloadBytes) {
        return 0u;
    }
    std::copy_n(static_cast<const uint8_t*>(packetBytes), packetByteCount, payloadBytes);

    mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope = {};
    envelope.payloadBase04 = payloadBytes;
    envelope.messageRef08 = &messageRef;
    return SendCurrentMarginPacketScaffold(envelope);
}

// anchor: launcher.exe:0x41f270
void CLTLoginMediator::SetLaunchPadSourceBlock94FirstString(const char* value) {
    std::fill(authBootstrapSource38_.inlineString00.begin(), authBootstrapSource38_.inlineString00.end(), '\0');
    if (!value) {
        return;
    }

    const size_t copyCount = std::min(
        std::char_traits<char>::length(value),
        authBootstrapSource38_.inlineString00.size() - 1);
    std::copy_n(value, copyCount, authBootstrapSource38_.inlineString00.begin());
    authBootstrapSource38_.inlineString00[copyCount] = '\0';
}

// UNANCHORED: source-owned shared GameSessionID writer mirror for later session/play callback paths
void CLTLoginMediator::SetGameSessionId664(const char* value) {
    gameSessionId664_ = value ? value : "";
}

// anchor: launcher.exe:0x41f330
void CLTLoginMediator::SetSharedMarginPacketField660(uint32_t value) {
    sharedMarginPacketField660_ = value;
}

// anchor: launcher.exe:0x420d00
SessionCallbackHelper65cSketch* CLTLoginMediator::EnsureSessionCallbackHelper65c() {
    if (sessionCallbackHelper65c_ == nullptr) {
        sessionCallbackHelper65cState_ = SessionCallbackHelper65cSketch();
        sessionCallbackHelper65cState_.owner10 = this;
        sessionCallbackHelper65c_ = &sessionCallbackHelper65cState_;
    }
    return &sessionCallbackHelper65cState_;
}

// anchor: launcher.exe:0x420e70
uint32_t CLTLoginMediator::CommitSessionCallbackHelperGameSessionId664() {
    SessionCallbackHelper65cSketch* helper = EnsureSessionCallbackHelper65c();
    if (helper == nullptr) {
        return 0u;
    }
    return InvokeSessionCallbackHelper65cVtable4IfPresent();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::InvokeSessionCallbackHelper65cVtable4IfPresent() {
    SessionCallbackHelper65cSketch* helper = sessionCallbackHelper65c_;
    if (helper == nullptr) {
        return 0u;
    }

    if (helper->flag2D != 0) {
        spdlog::info(
            "CLTLoginMediator::InvokeSessionCallbackHelper65cVtable4IfPresent deferred helper={} helperString18='{}' flag2D=0x{:02x}",
            fmt::ptr(helper),
            helper->string18.empty() ? "<empty>" : helper->string18.c_str(),
            static_cast<unsigned>(helper->flag2D));
        return 0u;
    }

    SetGameSessionId664(helper->string18.c_str());
    helper->field24 = 0;

    spdlog::info(
        "CLTLoginMediator::InvokeSessionCallbackHelper65cVtable4IfPresent helper={} owner660=0x{:08x} GameSessionID='{}'",
        fmt::ptr(helper),
        sharedMarginPacketField660_,
        gameSessionId664_.empty() ? "<empty>" : gameSessionId664_);
    return 1u;
}

// anchor: launcher.exe:0x4202c0
void CLTLoginMediator::HelperSlot13c_InvokeSessionHelperVtable4() {
    if (sessionCallbackHelper65c_ == nullptr) {
        spdlog::debug(
            "CLTLoginMediator::HelperSlot13c_InvokeSessionHelperVtable4(+0x13c) skipped (no helper)");
        return;
    }

    (void)InvokeSessionCallbackHelper65cVtable4IfPresent();
}

// source-owned shared helper for `CLTLoginState_State18` slot 3 / `0x421a50`
// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::RefreshSessionHelperGameSessionId664FromSourceBlock94() {
    // Current best source-owned mirror of the alternate state18 session-helper path:
    // - ensure owner helper `+0x65c`
    // - refresh helper string `+0x18` from owner `+0x94 + 0x60`
    // - then commit that helper string into owner `+0x664`
    // - keep this separate from the active state2 -> owner+0x680 bootstrap-child handoff
    SessionCallbackHelper65cSketch* helper = EnsureSessionCallbackHelper65c();
    if (helper == nullptr) {
        return 0u;
    }

    if (authBootstrapSource38_.string60.begin != nullptr && authBootstrapSource38_.string60.begin[0] != '\0') {
        helper->string18 = authBootstrapSource38_.string60.begin;
    }

    return CommitSessionCallbackHelperGameSessionId664();
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
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
//
// VTABLE METHODS (at offset +0xc from object pointer):
// +0xfc = GetWorldNameByIndex(index) -> char* world name string
// +0x100 = startup-only synthetic selection-gate byte used by the launcher arg7 path before the
//          recovered owner `+0xd84` descriptor table exists
// +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
// +0xf8 = GetWorldListCount() -> uint total count
// +0xd8 = GetActiveWorldCount() -> uint active count
// +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
// +0xdc = GetAvailableWorldName(index) -> char* (fallback path)
//
// ARG7 PACKING FORMAT:
// g_PackedArg7Selection = (high8bits << 24) | low24bits
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

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds(index)
    // - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)

    // Transitional stub preserves the slot structure for later completion.
    return worldSlots_[index];
}

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
// UNANCHORED: no original launcher.exe anchor assigned yet.
void* CLTLoginMediator::WorldPayloadSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName(index)
    // - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl

    // Transitional stub preserves the payload structure for later completion.
    return worldPayloadSlots_[index];
}

// anchor: launcher.exe:0x40e480
// sibling slot/vtable family: launcher.exe:0x4d3584
void CLTLoginMediator::InitializeArg6DefaultObject() {
    arg6WorldList_.worldNames_ = {
        "Default", "Starter", "Classic", "Advanced", "Extreme"
    };
    arg6WorldList_.worldSelectionGateBytes100_ = {1, 2, 3, 5, 1};
    arg6WorldList_.worldValid_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.available_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.totalCount_ = 5;
    arg6WorldList_.activeCount_ = 5;

    arg6Selection_ = Arg6SelectionConfig();

    spdlog::info(
        "DIAGNOSTIC: InitializeArg6DefaultObject populated arg6 defaults worlds={} active={} selectedWorld=0x{:06x} selectedVariant=0x{:02x}",
        arg6WorldList_.totalCount_,
        arg6WorldList_.activeCount_,
        arg6Selection_.selectedWorldIndexLow24_,
        arg6Selection_.selectedVariantIndexHigh8_);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::ConfigureArg6Selection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedSelectionGateByte100,
    uint32_t selectedVariantState) {
    arg6Selection_.worldUpperBoundExclusive_ = worldUpperBoundExclusive ? worldUpperBoundExclusive : 1u;
    arg6Selection_.variantUpperBoundExclusive_ = variantUpperBoundExclusive ? variantUpperBoundExclusive : 1u;
    arg6Selection_.selectedWorldIndexLow24_ = selectedWorldIndexLow24 & 0x00ffffffu;
    arg6Selection_.selectedVariantIndexHigh8_ = selectedVariantIndexHigh8 & 0xffu;
    arg6Selection_.selectedSelectionGateByte100_ = selectedSelectionGateByte100;
    arg6Selection_.selectedVariantState_ = selectedVariantState;
    arg6Selection_.mappedSelectionId_ = arg6Selection_.selectedWorldIndexLow24_;
    arg6Selection_.mappedSelectionName_ =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : "standalone";
    arg6Selection_.mappedVariantName_ =
        (mappedVariantName && mappedVariantName[0]) ? mappedVariantName : arg6Selection_.mappedSelectionName_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetArg6ProfileName(const char* profileName) {
    arg6Selection_.profileName_ = (profileName && profileName[0]) ? profileName : "resurrections";
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetArg6AuthName(const char* authName) {
    arg6Selection_.authName_ = (authName && authName[0]) ? authName : arg6Selection_.profileName_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::SetArg6AuthPassword(const char* authPassword) {
    arg6Selection_.authPassword_ = authPassword ? authPassword : "";
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6WorldUpperBoundExclusive() const {
    return arg6Selection_.worldUpperBoundExclusive_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6VariantUpperBoundExclusive() const {
    return arg6Selection_.variantUpperBoundExclusive_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6SelectedWorldIndexLow24() const {
    return arg6Selection_.selectedWorldIndexLow24_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6SelectedVariantIndexHigh8() const {
    return arg6Selection_.selectedVariantIndexHigh8_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6SelectedSelectionGateByte100() const {
    return arg6Selection_.selectedSelectionGateByte100_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6SelectedVariantState() const {
    return arg6Selection_.selectedVariantState_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6MappedSelectionId() const {
    return arg6Selection_.mappedSelectionId_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::Arg6MappedSelectionName() const {
    return arg6Selection_.mappedSelectionName_.c_str();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::Arg6MappedVariantName() const {
    return arg6Selection_.mappedVariantName_.c_str();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::Arg6ProfileName() const {
    return arg6Selection_.profileName_.c_str();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::Arg6AuthName() const {
    return arg6Selection_.authName_.c_str();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::Arg6AuthPassword() const {
    return arg6Selection_.authPassword_.c_str();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::Arg6WorldIndexMatchesSelection(uint32_t worldIndex) const {
    return worldIndex == arg6Selection_.selectedWorldIndexLow24_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const {
    return variantIndex == arg6Selection_.selectedVariantIndexHigh8_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::Arg6ExpectedSelectionDescriptorScratchRequest() const {
    const uint32_t variantHigh8 = (arg6Selection_.selectedVariantIndexHigh8_ & 0xffu) << 24;
    const uint32_t preservedMiddle16 = arg6Selection_.selectedWorldIndexLow24_ & 0x00ffff00u;
    const uint32_t lowByteOverwrittenWithVariant = arg6Selection_.selectedVariantIndexHigh8_ & 0xffu;
    return variantHigh8 | preservedMiddle16 | lowByteOverwrittenWithVariant;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTLoginMediator::Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const {
    const uint32_t normalizedSelectionIndex = selectionIndex & 0xffffffffu;
    if ((normalizedSelectionIndex & 0x00ffffffu) == arg6Selection_.selectedWorldIndexLow24_) {
        return true;
    }
    return normalizedSelectionIndex == Arg6ExpectedSelectionDescriptorScratchRequest();
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::GetDefaultSelectionIndex() const {
    const uint32_t selectionIndex = Arg6SelectedWorldIndexLow24();
    spdlog::debug(
        "CLTLoginMediator::GetDefaultSelectionIndex(+0x3c) -> 0x{:06x}",
        static_cast<unsigned>(selectionIndex));
    return selectionIndex;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::GetArg7SelectionUpperBoundExclusive() const {
    const uint32_t upperBoundExclusive = Arg6VariantUpperBoundExclusive();
    spdlog::info(
        "CLTLoginMediator::GetArg7SelectionUpperBoundExclusive(+0xd8) -> {}",
        static_cast<unsigned>(upperBoundExclusive));
    return upperBoundExclusive;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::MapSelectionName(uint32_t selectionHighByte) const {
    const char* selectionName = nullptr;
    if (selectionHighByte < Arg6VariantUpperBoundExclusive() &&
        Arg6VariantIndexMatchesSelection(selectionHighByte)) {
        selectionName = Arg6MappedVariantName();
    }

    spdlog::info(
        "CLTLoginMediator::MapSelectionName(+0xdc selectionHighByte={}) -> '{}'",
        static_cast<unsigned>(selectionHighByte),
        selectionName ? selectionName : "<null>");
    return selectionName;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
const char* CLTLoginMediator::GetVariantWorldName(uint32_t variantIndex) {
    ++arg6VariantWorldNameQueryCountE0_;
    if ((arg6VariantWorldNameQueryCountE0_ % 5u) == 0u) {
        spdlog::info(
            "CLTLoginMediator::GetVariantWorldName(+0xe0) queryCount={}",
            arg6VariantWorldNameQueryCountE0_);
    }

    const uint32_t worldIndex = Arg6SelectedWorldIndexLow24();
    const char* worldName = nullptr;
    if (worldIndex < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(worldIndex)) {
        worldName = Arg6MappedSelectionName();
    }

    if (!worldName ||
        variantIndex >= Arg6VariantUpperBoundExclusive() ||
        !Arg6VariantIndexMatchesSelection(variantIndex)) {
        spdlog::info(
            "CLTLoginMediator::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> NULL (world='{}' configuredVariant=0x{:02x} variantUpperBoundExclusive={})",
            static_cast<unsigned>(variantIndex & 0xffu),
            worldName ? worldName : "<null>",
            Arg6SelectedVariantIndexHigh8(),
            static_cast<unsigned>(Arg6VariantUpperBoundExclusive()));
        return nullptr;
    }

    spdlog::info(
        "CLTLoginMediator::GetVariantWorldName(+0xe0 variantIndex=0x{:02x}) -> '{}'",
        static_cast<unsigned>(variantIndex & 0xffu),
        worldName);
    return worldName;
}

// anchor: launcher.exe:0x41af30 / launcher.exe:0x40e5b0
// vtable: ILTLoginMediator.Default slot +0xf8
uint32_t CLTLoginMediator::GetWorldCount() const {
    const bool useRecoveredDescriptorTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;
    const uint32_t worldCount = useRecoveredDescriptorTable
        ? static_cast<uint32_t>(worldDescriptorCountD80_)
        : Arg6GetWorldListCount();

    spdlog::info(
        "CLTLoginMediator::GetWorldCount(+0xf8) -> {} [source={}]",
        worldCount,
        useRecoveredDescriptorTable ? "owner+0xd84" : "arg6-selection-fallback");
    return worldCount;
}

// anchor: launcher.exe:0x41b2e0 / launcher.exe:0x40cd10
// vtable: ILTLoginMediator.Default slot +0xfc
const char* CLTLoginMediator::GetWorldNameByIndex(uint32_t index) {
    const bool useRecoveredDescriptorTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;

    const char* worldName = nullptr;
    const char* source = "arg6-world-list";
    if (useRecoveredDescriptorTable) {
        worldName = (index <= 0xffu)
            ? GetDescriptorInlineNameByIndex(static_cast<uint8_t>(index))
            : nullptr;
        source = "owner+0xd84.inlineName+0x03";
    } else if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        worldName = Arg6MappedSelectionName();
        source = "arg6-selection-mapped-name";
    } else if (index < arg6WorldList_.totalCount_) {
        worldName = arg6WorldList_.worldNames_[index].c_str();
    }

    spdlog::info(
        "CLTLoginMediator::GetWorldNameByIndex(+0xfc index=0x{:06x}) -> '{}' [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        worldName ? worldName : "<null>",
        source);
    return worldName;
}

// anchor: launcher.exe:0x41b320 / launcher.exe:0x4d3584 +0x100
// vtable: ILTLoginMediator.Default slot +0x100
// Keep the wrapper/owner split explicit:
// - once auth-reply world descriptors exist, this slot reads owner descriptor Status byte `+0x17`
// - before that, startup selection still needs the older synthetic gate byte path
// UNANCHORED: no original launcher.exe anchor assigned yet.
uint8_t CLTLoginMediator::GetWorldSelectionGateByteByIndex(uint32_t index) const {
    const bool useRecoveredDescriptorTable = lastAuthReply_.valid && !lastAuthReply_.isErrorReply;

    uint8_t selectionGateByte100 = 0u;
    const char* source = "no-startup-fallback";
    if (useRecoveredDescriptorTable) {
        selectionGateByte100 = (index <= 0xffu)
            ? GetDescriptorStatusByIndex(static_cast<uint8_t>(index))
            : 0u;
        source = "owner+0xd84.status+0x17";
    } else if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        selectionGateByte100 = static_cast<uint8_t>(Arg6SelectedSelectionGateByte100());
        source = "arg6-selection-gate-byte100";
    } else if (index < arg6WorldList_.totalCount_) {
        selectionGateByte100 = arg6WorldList_.worldSelectionGateBytes100_[index];
        source = "arg6-world-list-gate-byte100";
    }

    spdlog::info(
        "CLTLoginMediator::GetWorldSelectionGateByteByIndex(+0x100 index=0x{:06x}) -> {} [source={}]",
        static_cast<unsigned>(index & 0x00ffffffu),
        static_cast<unsigned>(selectionGateByte100),
        source);
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

// anchor: launcher.exe:0x4d3584 +0xe4
// vtable: launcher.exe:0x4d3584 +0xe4
uint8_t CLTLoginMediator::Arg6ValidateWorldSelection(uint8_t variant) {
    if (variant < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(variant)) {
        return static_cast<uint8_t>(Arg6SelectedVariantState());
    }
    return 3u;
}

// anchor: launcher.exe:0x40e5b0
// vtable: launcher.exe:0x4d3584 +0xf8
uint32_t CLTLoginMediator::Arg6GetWorldListCount() const {
    return Arg6WorldUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e560
// vtable: launcher.exe:0x4d3584 +0xd8
uint32_t CLTLoginMediator::Arg6GetActiveWorldListCount() const {
    return Arg6VariantUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e670
// vtable: launcher.exe:0x4d3584 +0xe0
bool CLTLoginMediator::Arg6GetAvailableWorlds(uint32_t index) const {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return true;
    }
    return (index < arg6WorldList_.available_.size()) ? arg6WorldList_.available_[index] : false;
}

// anchor: launcher.exe:0x40cd60
// vtable: launcher.exe:0x4d3584 +0xdc
const char* CLTLoginMediator::Arg6GetAvailableWorldName(uint32_t index) {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return Arg6MappedVariantName();
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

// =============================================================================
// Private helper: Populate client.dll's world list view for InitClientDLL
// Address anchor: launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject
// =============================================================================
// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTLoginMediator::PopulateClientWorldView() {
    // Populate the client's world list view with launcher-provided data
    // This ensures client.dll receives populated world data when InitClientDLL passes arg6
    spdlog::info("launcher-owned PopulateClientWorldView called");

    // Copy launcher-owned world list into the mediator's client-facing view
    for (uint32_t i = 0; i < kRecoveredWorldSlotCapacity && i < arg6WorldList_.totalCount_; ++i) {
        worldSlots_[i] = const_cast<void*>(reinterpret_cast<const void*>(arg6WorldList_.worldNames_[i].c_str()));
        worldPayloadSlots_[i] = const_cast<void*>(
            reinterpret_cast<const void*>(&arg6WorldList_.worldSelectionGateBytes100_[i]));
        arg6WorldList_.worldValid_[i] = true;
        arg6WorldList_.available_[i] = true;
    }

    spdlog::info("launcher-owned PopulateClientWorldView populated {} worlds", kRecoveredWorldSlotCapacity);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::SendMarginFramedPacket(
    const mxo::auth::FramedPacket& packet,
    uint8_t plainRawCode,
    const char* stepLabel,
    bool encryptedTransport) {
    mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    if (!connection) {
        connection = EnsureMarginConnectionObject();
    }
    if (!connection || packet.bytes.empty()) {
        return 0u;
    }

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
void CLTLoginMediator::ResetMarginBootstrapState() {
    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    marginBootstrapState.phase = MarginBootstrapPhase::kIdle;
    marginBootstrapState.authReplyPrivateExponentBytes.clear();
    marginBootstrapState.marginTwofishKeyBytes.clear();
    marginBootstrapState.certChallengeBytes.clear();
    marginBootstrapState.marginSessionId = 0u;
    marginBootstrapState.state6UdpSessionSecretF18 = 0u;
    postAuthMarginLoadingState_.state10SendGateFlagF14 = 0u;
    stagedIncomingMarginPacketBytes_.clear();
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
            if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(MarginConnection());
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
            if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(MarginConnection());
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
                        "DIAGNOSTIC: launcher-owned margin mirrored original 0x4429b0 send seam by routing CERT_ChallengeResponse through CMarginConnection packet-builder/message-ref send connection={}",
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
                "launcher-owned margin bootstrap received MS_ConnectChallenge transportEncrypted={} payloadLen={}",
                transportEncrypted ? 1u : 0u,
                payloadSize);

            std::array<uint8_t, 16> md5Bytes = {};
            if (authKeyConfigMd5_.size() >= md5Bytes.size()) {
                std::copy_n(authKeyConfigMd5_.begin(), md5Bytes.size(), md5Bytes.begin());
            }

            mxo::auth::FramedPacket response;
            if (!mxo::auth::BuildMarginConnectChallengeResponsePacket(
                    md5Bytes,
                    marginBootstrapState.marginTwofishKeyBytes,
                    mxo::auth::kFrameModeAuto,
                    &response)) {
                spdlog::info("DIAGNOSTIC: launcher-owned margin failed to build MS_ConnectChallengeResponse");
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
            spdlog::info(
                "launcher-owned margin bootstrap received MS_ConnectReply transportEncrypted={} payloadLen={}",
                transportEncrypted ? 1u : 0u,
                payloadSize);

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
            const uint32_t currentHelperPhaseCodeBeforeReply =
                currentState_ ? currentState_->DispatchPhaseCode() : 0u;
            uint32_t state6Handled = 0u;
            if (currentHelperPhaseCodeBeforeReply == 6u) {
                state6Handled = currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
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
                postAuthMarginLoadingState_.state10SendGateFlagF14,
                State6UdpSessionSecretF18(),
                currentHelperPhaseCodeBeforeReply,
                currentState_ ? currentState_->DebugName() : "<null>");
            if (state6Handled != 0u) {
                return state6Handled;
            }

            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap completed MS_ConnectReply outside the state6 slot6 route; not synthesizing owner+0xf14/+0xf18 or restoring slot3 from bootstrap completion alone currentHelperPhaseBeforeReply=0x{:02x} currentState={} ownerF14={} ownerF18=0x{:08x}",
                currentHelperPhaseCodeBeforeReply,
                currentState_ ? currentState_->DebugName() : "<null>",
                postAuthMarginLoadingState_.state10SendGateFlagF14,
                State6UdpSessionSecretF18());
            return 1u;
        }

        default:
            break;
    }

    return 0u;
}

// anchor: launcher.exe:0x447eb0
uint32_t CLTLoginMediator::SendAuthGetPublicKeyRequest() {
    return AuthBootstrap680Ops::SendAuthGetPublicKeyRequest(*this);
}

// anchor: launcher.exe:0x4474f0
uint32_t CLTLoginMediator::SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply) {
    return AuthBootstrap680Ops::SendAuthRequestFromReply(*this, reply);
}

// UNANCHORED: thin mediator wrapper over the source-owned owner+0x680 bootstrap child raw `0x0a`
// send bridge.
uint32_t CLTLoginMediator::SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge) {
    return AuthBootstrap680Ops::SendAuthChallengeResponse(*this, challenge);
}

// UNANCHORED: thin mediator wrapper over the source-owned owner+0x680 parsed-auth logging helper.
void CLTLoginMediator::LogParsedAuthReply(const mxo::auth::AuthReply& reply) const {
    AuthBootstrap680Ops::LogParsedAuthReply(*this, reply);
}

// UNANCHORED: thin mediator wrapper over fixed-field sync for the owner `+0x680` bootstrap child.
void CLTLoginMediator::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig() {
    AuthBootstrap680Ops::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig(*this);
}

// UNANCHORED: thin mediator wrapper over dynamic-state reset for the owner `+0x680` bootstrap
// child.
void CLTLoginMediator::ResetRecoveredAuthBootstrapDynamicStateScaffold() {
    AuthBootstrap680Ops::ResetRecoveredAuthBootstrapDynamicStateScaffold(*this);
}

// UNANCHORED: thin mediator wrapper over parsed `AS_GetPublicKeyReply` sync for the owner `+0x680`
// bootstrap child.
void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    const mxo::auth::GetPublicKeyReply& reply) {
    AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(*this, reply);
}

// UNANCHORED: thin mediator wrapper over post-`0x0a` challenge-material sync for the owner `+0x680`
// bootstrap child.
void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
    const mxo::auth::AuthChallengeResponseBuildResult& buildResult) {
    AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(*this, buildResult);
}

// UNANCHORED: thin mediator wrapper over auth-reply shadow sync for the owner `+0x680` bootstrap
// child.
void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(const mxo::auth::AuthReply& reply) {
    AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(*this, reply);
}

bool CLTLoginMediator::PrepareState5MarginConnectionCopySendScaffold(
    mxo::liblttcp::CMarginConnection* marginConnection) {
    return marginConnection != nullptr
        ? AuthBootstrap680Ops::PrepareState5MarginConnectionCopySendScaffold(*this, *marginConnection)
        : false;
}

const void* CLTLoginMediator::AuthBootstrapReplyCopyShadowF4Scaffold() const {
    return authBootstrapChild680_.authReplyCopyShadowF4;
}

bool CLTLoginMediator::HasValidState5ReplyCopyShadowF4Scaffold() const {
    // Exact recovered gate split from `0x433c0 -> 0x44add0`:
    // - if child `+0xf4` is null, state5 slot3 takes the helper-state-2 branch
    // - otherwise the copied `0x136` block stays usable only while
    //   `(time(NULL) - child+0x80) < *(uint32_t*)(child+0xf4 + 0xac)`
    const auto* copyShadow =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(authBootstrapChild680_.authReplyCopyShadowF4);
    if (copyShadow == nullptr) {
        return false;
    }

    const uint32_t expiryTimeAc = ReadU32LE(copyShadow->signedData80.data() + 0x2cu);
    const std::time_t now = std::time(nullptr);
    const uint32_t ageSinceGetPublicKey =
        (now > static_cast<std::time_t>(authBootstrapChild680_.timestamp80))
            ? static_cast<uint32_t>(now - static_cast<std::time_t>(authBootstrapChild680_.timestamp80))
            : 0u;
    return ageSinceGetPublicKey < expiryTimeAc;
}

// UNANCHORED: source-owned table mirror fed from parsed auth reply worlds
void CLTLoginMediator::SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world) {
    if (worldIndex >= worldDescriptorsD84_.size()) {
        return;
    }

    WorldDescriptorState004b533c& descriptor = worldDescriptorsD84_[worldIndex];
    descriptor.worldId01 = world.worldId;
    descriptor.inlineNamePlus03 = world.worldName;
    descriptor.status17 = static_cast<uint8_t>(world.status & 0xffu);
    descriptor.type18 = static_cast<uint8_t>(world.type & 0xffu);
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
    if (characterIndex >= slotRecords688_.size()) {
        return;
    }

    SlotRecordState004b5328& slotRecord = slotRecords688_[characterIndex];
    slotRecord.heapString14 = character.handle.text;
    slotRecord.globalCharacterIdLow03 = static_cast<uint32_t>(character.characterId & 0xffffffffull);
    slotRecord.globalCharacterIdHigh07 = static_cast<uint32_t>((character.characterId >> 32) & 0xffffffffull);
    slotRecord.status0b = static_cast<uint8_t>(character.status & 0xffu);
    slotRecord.worldId0c = character.worldId;
    slotRecordValid688_[characterIndex] = true;
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
    // - owner `+0x12c` should not be backfilled from slot-record `worldId0c`
    // - the active branch uses `+0x12c` as a world-descriptor index/selector
    const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord();
    if (currentSlotRecord != nullptr) {
        if (postAuthMarginLoadingState_.sourceLeadString108[0] == '\0' && !currentSlotRecord->heapString14.empty()) {
            const size_t copyCount = std::min(
                currentSlotRecord->heapString14.size(),
                postAuthMarginLoadingState_.sourceLeadString108.size() - 1);
            std::copy_n(
                currentSlotRecord->heapString14.data(),
                copyCount,
                postAuthMarginLoadingState_.sourceLeadString108.begin());
            postAuthMarginLoadingState_.sourceLeadString108[copyCount] = '\0';
        }

        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(currentSlotRecord->worldId0c);
        if (matchedWorldIndex >= 0 &&
            (postAuthMarginLoadingState_.sourceField12c >= static_cast<uint32_t>(worldDescriptorCountD80_) ||
             (postAuthMarginLoadingState_.sourceField12c == 0u && matchedWorldIndex != 0))) {
            postAuthMarginLoadingState_.sourceField12c = static_cast<uint32_t>(matchedWorldIndex);
        }
    }
}

// anchor: launcher.exe:0x41e760
void CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold() const {
    const char* profileName = authUsername_.empty() ? Arg6AuthName() : authUsername_.c_str();
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
    for (size_t i = 0; i < slotRecordCount684_ && i < slotRecords688_.size(); ++i) {
        if (!slotRecordValid688_[i]) {
            continue;
        }

        const SlotRecordState004b5328& slotRecord = slotRecords688_[i];
        const RouteHostStringTripleState& route = routeHostStrings818_[i];
        const char* characterName = slotRecord.heapString14.empty() ? "" : slotRecord.heapString14.c_str();
        const char* routeText = route.text.empty() ? "" : route.text.c_str();
        std::fprintf(file, "Character%u:=%s,%s\n", static_cast<unsigned>(i), characterName, routeText);
        ++persistedCount;
    }
    std::fclose(file);

    spdlog::info(
        "CLTLoginMediator::PersistCharactersIniFromRecoveredAuthStateScaffold wrote '{}' characterCount={} currentIndex=0x{:02x}",
        outputPath,
        persistedCount,
        static_cast<unsigned>(postAuthMarginLoadingState_.characterRouteIndexCc8));
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HandleAuthConnectionReceiveScaffold() {
    mxo::liblttcp::CMessageConnection* connection = AuthConnection();
    if (connection == nullptr) {
        return kReceiveActionNone;
    }

    uint32_t actions = kReceiveActionNone;
    std::vector<uint8_t> queuedPacketBodyBytes;
    bool queuedPacketHeaderless = false;
    while (connection->TakeNextReceivedPacketScaffold(
        &queuedPacketBodyBytes,
        &queuedPacketHeaderless)) {
        const uint8_t rawCode = queuedPacketBodyBytes.empty() ? 0u : queuedPacketBodyBytes[0];
        spdlog::info(
            "CLTLoginMediator::HandleAuthConnectionReceiveScaffold queuedParsedPacket payloadBytes={} headerless={} rawCode=0x{:02x} likelyMessage='{}'",
            static_cast<unsigned>(queuedPacketBodyBytes.size()),
            queuedPacketHeaderless ? 1u : 0u,
            static_cast<unsigned>(rawCode),
            mxo::auth::AuthOpcodeName(rawCode));

        const uint32_t handled = HandleAuthPacketBytes(
            queuedPacketBodyBytes.empty() ? nullptr : queuedPacketBodyBytes.data(),
            queuedPacketBodyBytes.size());
        spdlog::info(
            "CLTLoginMediator::HandleAuthConnectionReceiveScaffold handledQueuedPacket={} rawCode=0x{:02x}",
            static_cast<unsigned>(handled),
            static_cast<unsigned>(rawCode));

        if (handled != 0u && rawCode == 0x0bu && !postAuthMarginAutoBeginAttemptedScaffold_) {
            postAuthMarginAutoBeginAttemptedScaffold_ = true;
            actions |= kReceiveActionBeginMarginAfterAuthReply;
            spdlog::info(
                "CLTLoginMediator::HandleAuthConnectionReceiveScaffold requested one-shot post-AS_AuthReply margin auto-begin currentState={}",
                currentState_ ? currentState_->DebugName() : "<null>");
        }
    }

    return actions;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    // Receive-side note:
    // - the current replacement receive scaffolds strip the variable-length frame header before
    //   calling this wrapper entry point
    // - so this function operates on logical auth payload bytes beginning at raw opcode, not on a
    //   second frame layer
    // Ownership note:
    // - this mediator entry is only the current staging/demux wrapper
    // - original `0x43f300/0x448140` consume a higher-level incoming auth-message object, not raw
    //   payload bytes directly
    // - current source therefore keeps only a staged-payload demux boundary here, routing the
    //   early semantic handling into state2 / the owner+0x680 child instead of claiming mediator
    //   ownership
    // - later/narrower selected-slot raw `0x0b` handling still belongs to state10 slot 6
    //   (`0x4401a0`)
    // - one deliberate current-source exception remains: the replacement's existing-character
    //   state8 branch still bypasses the natural state10/state11 claim/create transition
    if (!packetBytes || packetSize == 0u) {
        return 0u;
    }

    const uint8_t rawCode = packetBytes[0];
    const auto dispatchStagedEarlyAuthViaState2 = [this, packetBytes, packetSize, rawCode]() -> uint32_t {
        stagedIncomingAuthPacketBytes_.assign(packetBytes, packetBytes + packetSize);
        if (scaffoldState2_ == nullptr) {
            spdlog::info(
                "CLTLoginMediator::HandleAuthPacketBytes received early auth rawCode=0x{:02x} with no registered state2/AuthMessageDispatch receiver",
                static_cast<unsigned>(rawCode));
            return 0u;
        }
        return scaffoldState2_->AuthMessageDispatch(nullptr, this);
    };

    switch (rawCode) {
        case kAuthRawCodeGetPublicKeyReply:
            return dispatchStagedEarlyAuthViaState2();

        case 0x09u: {
            const uint32_t handled = dispatchStagedEarlyAuthViaState2();
            const bool preserveExistingCharacterState8Path =
                currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u;
            if (handled != 0u && scaffoldState10_ != nullptr && !preserveExistingCharacterState8Path) {
                SwitchHelperStateScaffold(0x0au, scaffoldState10_);
            } else if (handled != 0u && preserveExistingCharacterState8Path) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes preserving current state8 through AS_AuthChallengeResponse for the existing-character path instead of forcing helperState=0x0a claim/create flow");
            }
            return handled;
        }

        case 0x0bu: {
            // Address anchors:
            // - launcher.exe:0x41bc20 = later auth opcode read helper
            // - launcher.exe:0x43f300 = state2 broader inbound auth dispatcher
            // - launcher.exe:0x4401a0 = state10 slot 6 / later narrower selected-slot auth-reply handler
            stagedIncomingAuthPacketBytes_.assign(packetBytes, packetBytes + packetSize);
            if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes routing AS_AuthReply onto the existing-character state8 path; keeping currentState={} and skipping the later natural state10/state11 claim/create transition",
                    currentState_->DebugName());
                const uint32_t handled = CLTLoginState_State10::HandleStagedAuthReplyScaffold(this);
                if (handled != 0u && lastAuthReply_.valid && !lastAuthReply_.isErrorReply) {
                    expectedMarginRequestName_ = "existing-character state8 raw-0x0f margin packet";
                }
                return handled;
            }
            if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 10u) {
                return currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
            }
            if (scaffoldState2_ != nullptr) {
                return scaffoldState2_->AuthMessageDispatch(nullptr, this);
            }
            spdlog::info(
                "CLTLoginMediator::HandleAuthPacketBytes received AS_AuthReply with no faithful state2/state10 receiver available currentState={}",
                currentState_ ? currentState_->DebugName() : "<null>");
            return 0u;
        }

        default:
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth received unhandled packet rawCode=0x{:02x} message='{}' payloadLen={}",
                rawCode,
                mxo::auth::AuthOpcodeName(rawCode),
                packetSize);
            break;
    }

    return 0u;
}

// anchor: launcher.exe:0x442d00 -> 0x41bc20 / 0x441a30 / 0x4429b0
uint32_t CLTLoginMediator::HandleMarginConsumedCode2AtConnectionSeamScaffold(
    const uint8_t* packetBytes,
    size_t packetSize,
    bool transportEncrypted) {
    if (!packetBytes || packetSize == 0u) {
        return 0u;
    }

    stagedIncomingMarginPacketBytes_.assign(packetBytes, packetBytes + packetSize);
    const uint32_t handled = ContinueMarginBootstrapHandshake(packetBytes, packetSize, transportEncrypted);
    spdlog::info(
        "CLTLoginMediator::HandleMarginConsumedCode2AtConnectionSeamScaffold rawCode=0x{:02x} transportEncrypted={} packetSize={} currentState={} handled={}",
        static_cast<unsigned>(packetBytes[0]),
        transportEncrypted ? 1u : 0u,
        static_cast<unsigned>(packetSize),
        currentState_ ? currentState_->DebugName() : "<null>",
        handled);
    return handled;
}

// anchor: launcher.exe:0x442d00 -> 0x41bc20 / 0x441bc0 / 0x441850
uint32_t CLTLoginMediator::HandleMarginConsumedCode4AtConnectionSeamScaffold(
    const uint8_t* packetBytes,
    size_t packetSize,
    bool transportEncrypted) {
    if (!packetBytes || packetSize < 5u) {
        return 0u;
    }

    stagedIncomingMarginPacketBytes_.assign(packetBytes, packetBytes + packetSize);
    const uint32_t status = ReadU32LE(packetBytes + 1u);
    auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(MarginConnection());
    if (marginConnection != nullptr) {
        marginConnection->SetMessageCode4SuccessFlag84(status == 0u);
    }

    uint32_t localWorkItemHandled = 0u;
    if (marginConnection != nullptr) {
        localWorkItemHandled =
            marginConnection->DispatchMessageCode4LocalCompletionWorkItem(status);
    }

    spdlog::info(
        "CLTLoginMediator::HandleMarginConsumedCode4AtConnectionSeamScaffold rawCode=0x{:02x} status=0x{:08x} transportEncrypted={} connectionByte84={} localType0x0bHandled={} currentState={}",
        static_cast<unsigned>(packetBytes[0]),
        status,
        transportEncrypted ? 1u : 0u,
        (marginConnection != nullptr && marginConnection->MessageCode4SuccessFlag84()) ? 1u : 0u,
        static_cast<unsigned>(localWorkItemHandled),
        currentState_ ? currentState_->DebugName() : "<null>");

    const uint32_t currentHelperPhaseCode =
        currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    if (currentHelperPhaseCode == 5u && localWorkItemHandled != 0u) {
        spdlog::info(
            "CLTLoginMediator::HandleMarginConsumedCode4AtConnectionSeamScaffold preserved explicit state5 local type0x0b seam currentHelperPhase=0x{:02x} localType0x0bHandled={} -> skipping broader bootstrap fallback",
            static_cast<unsigned>(currentHelperPhaseCode),
            static_cast<unsigned>(localWorkItemHandled));
        return localWorkItemHandled;
    }

    // Current source now keeps the runtime-backed local code-4 seam explicit:
    // - consume code 4 at the margin leaf
    // - synthesize the local type-0x0b work item (`0x441850` shape)
    // - let owner fallback `0x41afc0` re-enter current helper slot 2
    // Any remaining bootstrap fallback here is only for the broader launcher-owned receive path,
    // not because the local state5 slot2 continuation is still treated as speculative.
    const uint32_t bootstrapHandled =
        ContinueMarginBootstrapHandshake(packetBytes, packetSize, transportEncrypted);
    return (bootstrapHandled != 0u) ? bootstrapHandled : localWorkItemHandled;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HandleMarginConnectionReceiveScaffold() {
    mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    if (connection == nullptr) {
        return kReceiveActionNone;
    }

    std::vector<uint8_t> queuedPacketBodyBytes;
    bool queuedPacketHeaderless = false;
    while (connection->TakeNextReceivedPacketScaffold(
        &queuedPacketBodyBytes,
        &queuedPacketHeaderless)) {
        const uint8_t rawCode = queuedPacketBodyBytes.empty() ? 0u : queuedPacketBodyBytes[0];
        const bool looksLikePlainBootstrapReply =
            rawCode == 0x02u || rawCode == 0x04u || rawCode == 0x07u || rawCode == 0x09u;
        spdlog::info(
            "CLTLoginMediator::HandleMarginConnectionReceiveScaffold queuedParsedPacket payloadBytes={} headerless={} outerByte0=0x{:02x} framingHint={}",
            static_cast<unsigned>(queuedPacketBodyBytes.size()),
            queuedPacketHeaderless ? 1u : 0u,
            static_cast<unsigned>(rawCode),
            looksLikePlainBootstrapReply ? "plaintext-bootstrap-reply" : "possibly-encrypted-post-bootstrap-payload");

        const uint32_t handled = HandleMarginPacketBytes(
            queuedPacketBodyBytes.empty() ? nullptr : queuedPacketBodyBytes.data(),
            queuedPacketBodyBytes.size());
        spdlog::info(
            "CLTLoginMediator::HandleMarginConnectionReceiveScaffold handledQueuedPacket={} outerByte0=0x{:02x}",
            static_cast<unsigned>(handled),
            static_cast<unsigned>(rawCode));
    }

    return kReceiveActionNone;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
uint32_t CLTLoginMediator::HandleMarginPacketBytes(
    const uint8_t* packetBytes,
    size_t packetSize,
    void* workItem) {
    if (!packetBytes || packetSize < 1u) {
        return 0u;
    }

    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    std::vector<uint8_t> decryptedPayloadBytes;
    const uint8_t* effectivePacketBytes = packetBytes;
    size_t effectivePacketSize = packetSize;
    bool transportEncrypted = false;
    if (!marginBootstrapState.marginTwofishKeyBytes.empty() &&
        mxo::auth::DecryptMarginPayloadPacket(
            packetBytes,
            packetSize,
            marginBootstrapState.marginTwofishKeyBytes,
            &decryptedPayloadBytes) &&
        !decryptedPayloadBytes.empty()) {
        effectivePacketBytes = decryptedPayloadBytes.data();
        effectivePacketSize = decryptedPayloadBytes.size();
        transportEncrypted = true;
    }

    stagedIncomingMarginPacketBytes_.assign(
        effectivePacketBytes,
        effectivePacketBytes + effectivePacketSize);
    const uint16_t rawCode = effectivePacketBytes[0];
    ++marginPacketReceiveCountScaffold_;
    lastMarginPacketOpcodeScaffold_ = rawCode;
    lastMarginPacketSizeScaffold_ = static_cast<uint32_t>(effectivePacketSize);

    spdlog::debug(
        "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} transportEncrypted={} currentState={} receiveCount={} filteredBeforeSlot6={} slot6DispatchCount={} bootstrapPhase={}",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(effectivePacketSize),
        transportEncrypted ? 1u : 0u,
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(marginPacketReceiveCountScaffold_),
        static_cast<unsigned>(marginPacketFilteredBeforeSlot6CountScaffold_),
        static_cast<unsigned>(marginPacketSlot6DispatchCountScaffold_),
        static_cast<unsigned>(marginBootstrapState.phase));

    const uint32_t currentHelperPhaseCode =
        currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    if (rawCode == 0x04u && currentHelperPhaseCode == 5u) {
        const uint32_t handledLocalCode4 =
            HandleMarginConsumedCode4AtConnectionSeamScaffold(
                effectivePacketBytes,
                effectivePacketSize,
                transportEncrypted);
        if (handledLocalCode4 != 0u) {
            spdlog::info(
                "CLTLoginMediator::HandleMarginPacketBytes routed rawCode=0x04 directly into the runtime-backed state5 local completion seam currentHelperPhase=0x{:02x} transportEncrypted={} handled={}",
                static_cast<unsigned>(currentHelperPhaseCode),
                transportEncrypted ? 1u : 0u,
                static_cast<unsigned>(handledLocalCode4));
            return handledLocalCode4;
        }
    }

    // Route launcher-owned CERT/MS bootstrap packets first.
    // This keeps the bootstrap progression explicit instead of faking a ready state after
    // transport connect.
    if (marginBootstrapState.phase != MarginBootstrapPhase::kReady ||
        rawCode == 0x02u || rawCode == 0x04u || rawCode == 0x07u || rawCode == 0x09u) {
        const uint32_t bootstrapHandled =
            ContinueMarginBootstrapHandshake(effectivePacketBytes, effectivePacketSize, transportEncrypted);
        if (bootstrapHandled != 0u) {
            return bootstrapHandled;
        }
    }

    // anchor: launcher.exe:0x44af20 -> 0x442d00 -> 0x41f260
    // Exact receive-side boundary now mirrored in source:
    // - decoded codes 2 / 4 / 5 are consumed by base margin dispatch
    // - only other decoded codes survive into owner +0x184 / current helper slot 6
    if ((rawCode == 0x10u || rawCode == 0x11u) && effectivePacketSize >= 5u) {
        const uint32_t leadingStatusDword =
            static_cast<uint32_t>(effectivePacketBytes[1]) |
            (static_cast<uint32_t>(effectivePacketBytes[2]) << 8) |
            (static_cast<uint32_t>(effectivePacketBytes[3]) << 16) |
            (static_cast<uint32_t>(effectivePacketBytes[4]) << 24);
        spdlog::info(
            "CLTLoginMediator::HandleMarginPacketBytes observed post-bootstrap rawCode=0x{:02x} statusDword=0x{:08x} packetSize={} encrypted={} receiveCount={} filteredBeforeSlot6={} currentState={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(leadingStatusDword),
            static_cast<unsigned>(effectivePacketSize),
            transportEncrypted ? 1u : 0u,
            static_cast<unsigned>(marginPacketReceiveCountScaffold_),
            static_cast<unsigned>(marginPacketFilteredBeforeSlot6CountScaffold_),
            currentState_ ? currentState_->DebugName() : "<null>");
    }

    if (rawCode == 2u || rawCode == 4u || rawCode == 5u) {
        ++marginPacketFilteredBeforeSlot6CountScaffold_;
        spdlog::debug(
            "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} would be consumed by base margin dispatch before helper slot6 receiveCount={} filteredBeforeSlot6={} currentState={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(effectivePacketSize),
            static_cast<unsigned>(marginPacketReceiveCountScaffold_),
            static_cast<unsigned>(marginPacketFilteredBeforeSlot6CountScaffold_),
            currentState_ ? currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (currentState_ != nullptr) {
        ++marginPacketSlot6DispatchCountScaffold_;
        spdlog::debug(
            "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} routing post-bootstrap packet to current helper slot6 dispatchCount={} currentState={} workItem={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(effectivePacketSize),
            static_cast<unsigned>(marginPacketSlot6DispatchCountScaffold_),
            currentState_->DebugName(),
            fmt::ptr(workItem));
        return currentState_->Slot6_HandleSecondaryMessage(workItem, this);
    }

    spdlog::debug(
        "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} has no active helper state; no faithful slot6 receiver is available for this post-bootstrap packet",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(effectivePacketSize));
    return 0u;
}

// UNANCHORED: narrower mediator-owned bridge into the auth-reply-derived margin-bootstrap
// sidecar. State10 owns the broader raw-0x0b parse/adopt body and calls this only for the
// private-exponent recovery step.
void CLTLoginMediator::RecoverAuthReplyPrivateExponentIntoMarginBootstrapState(
    const mxo::auth::AuthReply& reply) {
    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    if (!mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            lastAuthChallenge_.encryptedChallengeBytes,
            &marginBootstrapState.authReplyPrivateExponentBytes)) {
        marginBootstrapState.authReplyPrivateExponentBytes.clear();
        spdlog::info("DIAGNOSTIC: launcher-owned auth could not recover private exponent bytes needed for later margin CERT bootstrap");
    }
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
        spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold unresolved margin host");
        return false;
    }

    uint32_t flags = mxo::liblttcp::CLTIPAddressList::kFlagShuffle;
    if (ignoreHostsFileForMargin_) {
        flags |= mxo::liblttcp::CLTIPAddressList::kFlagIgnoreHostsFile;
    }

    if (!marginAddressList3c_.Reinit(resolvedHostName.c_str(), flags)) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnectionScaffold failed to resolve margin host '{}' flags=0x{:02x}",
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
mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureMarginConnectionObject() {
    if (marginConnectionContextKey_ == nullptr && engine_ != nullptr) {
        // Bounded source-owned bridge correction:
        // - original `0x41e500` is reached directly from state4 and does not pass through the
        //   outer `BeginLauncherMarginConnectionScaffold()` helper
        // - current replacement therefore has to materialize the queue/context bridge here too,
        //   not only at the outer wrapper
        CLTLoginMediatorConnectionContextScaffold* context =
            engine_->EnsureLauncherConnectionContextScaffold(
                &marginConnectionContextScaffold_,
                this,
                "MarginConnection",
                /*isMarginConnection=*/true);
        if (context) {
            context->peerCloseQueued = false;
            SetMarginConnectionContextKey(context);
        }
    }

    mxo::liblttcp::CMarginConnection* marginConnection =
        dynamic_cast<mxo::liblttcp::CMarginConnection*>(marginConnection_);
    if (!marginConnection) {
        if (marginConnectionOwnedByMediator_) {
            delete marginConnection_;
        }
        marginConnection = new mxo::liblttcp::CMarginConnection(engine_);
        if (!marginConnection) {
            marginConnection_ = nullptr;
            marginConnectionOwnedByMediator_ = false;
            return nullptr;
        }

        marginConnection->SetEngine(engine_);
        marginConnection->SetMarginEngine(engine_);
        marginConnection_ = marginConnection;
        marginConnectionOwnedByMediator_ = true;
    }

    marginConnection->SetEngine(engine_);
    marginConnection->SetMarginEngine(engine_);
    marginConnection->ConfigurePacketNameFamily(
        mxo::liblttcp::CMessageConnectionPacketNameFamily::kMargin,
        /*packetizedMessagesEnabled=*/true);
    if (marginConnectionContextKey_) {
        marginConnection->SetOwnerContext(marginConnectionContextKey_);
    }
    if (marginConnectionContextScaffold_ != nullptr) {
        marginConnectionContextScaffold_->sidecarConnection = marginConnection;
    }
    return marginConnection_;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
// ILTLoginMediator::Default - static member initialization (original: launcher.exe:0x4d2c58)
ILTLoginMediator* ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();

}  // namespace mxo::ltlogin
