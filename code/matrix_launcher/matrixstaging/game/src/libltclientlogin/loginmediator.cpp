/**
 * CLTLoginMediator - launcher-owned login/controller implementation.
 *
 * Maintenance note:
 * - keep this file focused on shared mediator logic, auth/bootstrap, and margin transport
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

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../../../../src/diagnostics.h"
#include "../../../../src/diagnostics_auth.h"
#include "loginstate.h"
#include "launcher_mediator_abi_shared.h"

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

struct RecoveredAuthBootstrapSidecarState {
    CLTLoginMediator::AuthBootstrapReplyShadowF4Sketch fieldF4Shadow{};
    uint32_t raw08AuxHandleAvailabilityMarker = 0u;
};

static std::unordered_map<const CLTLoginMediator*, std::unique_ptr<RecoveredAuthBootstrapSidecarState>>
    g_recoveredAuthBootstrapSidecarByMediator;

static MarginBootstrapSessionState& MutableMarginBootstrapState(const CLTLoginMediator* mediator) {
    return g_marginBootstrapStateByMediator[mediator];
}

static RecoveredAuthBootstrapSidecarState* FindRecoveredAuthBootstrapSidecar(const CLTLoginMediator* mediator) {
    const auto it = g_recoveredAuthBootstrapSidecarByMediator.find(mediator);
    return (it != g_recoveredAuthBootstrapSidecarByMediator.end() && it->second)
        ? it->second.get()
        : nullptr;
}

static RecoveredAuthBootstrapSidecarState& MutableRecoveredAuthBootstrapSidecar(const CLTLoginMediator* mediator) {
    std::unique_ptr<RecoveredAuthBootstrapSidecarState>& slot =
        g_recoveredAuthBootstrapSidecarByMediator[mediator];
    if (!slot) {
        slot = std::make_unique<RecoveredAuthBootstrapSidecarState>();
    }
    return *slot;
}

static void EraseMarginBootstrapState(const CLTLoginMediator* mediator) {
    g_marginBootstrapStateByMediator.erase(mediator);
}

static void EraseRecoveredAuthBootstrapSidecar(const CLTLoginMediator* mediator) {
    g_recoveredAuthBootstrapSidecarByMediator.erase(mediator);
}

static mxo::liblttcp::LTTCPEndpointKey BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey key = {};
    key.family = 2;  // AF_INET
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = 0;
    return key;
}

static bool EnsureWinsockReady() {
    static bool initialized = false;
    static bool attempted = false;
    if (attempted) {
        return initialized;
    }
    attempted = true;

    WSADATA wsaData = {};
    initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    return initialized;
}

static bool ResolveAllIpv4Addresses(const char* hostName, std::vector<uint32_t>* outIpv4NetworkOrderList) {
    if (!hostName || !hostName[0] || !outIpv4NetworkOrderList || !EnsureWinsockReady()) {
        return false;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &results) != 0 || !results) {
        return false;
    }

    outIpv4NetworkOrderList->clear();
    for (addrinfo* it = results; it; it = it->ai_next) {
        if (it->ai_family != AF_INET || !it->ai_addr ||
            it->ai_addrlen < static_cast<int>(sizeof(sockaddr_in))) {
            continue;
        }

        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(it->ai_addr);
        const uint32_t ipv4NetworkOrder = addr->sin_addr.s_addr;
        if (std::find(outIpv4NetworkOrderList->begin(), outIpv4NetworkOrderList->end(), ipv4NetworkOrder) ==
            outIpv4NetworkOrderList->end()) {
            outIpv4NetworkOrderList->push_back(ipv4NetworkOrder);
        }
    }

    freeaddrinfo(results);
    return !outIpv4NetworkOrderList->empty();
}

static const char* MaskedAuthValue(const std::string& value) {
    return value.empty() ? "<empty>" : "<provided>";
}

static const char* NonEmptyTextOrPlaceholder(const char* value) {
    return (value && value[0]) ? value : "<empty>";
}

static std::string LowercaseAsciiString(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

static std::array<uint8_t, 16> CopyPrefix16(const std::vector<uint8_t>& bytes) {
    std::array<uint8_t, 16> out = {};
    const size_t count = std::min(out.size(), bytes.size());
    std::copy_n(bytes.begin(), count, out.begin());
    return out;
}

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

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static void CopyCStringIntoFixed(char* dest, size_t destSize, const uint8_t* src, size_t srcAvailable) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::fill(dest, dest + destSize, '\0');
    if (!src || srcAvailable == 0u) {
        return;
    }

    size_t copyCount = 0u;
    while (copyCount + 1u < destSize && copyCount < srcAvailable && src[copyCount] != '\0') {
        dest[copyCount] = static_cast<char>(src[copyCount]);
        ++copyCount;
    }
    dest[copyCount] = '\0';
}

static void AppendOwnedSectionBytes(void*& buffer, uint16_t& length, const uint8_t* src, uint16_t appendLen) {
    if (!src || appendLen == 0u) {
        return;
    }

    const size_t oldLength = length;
    const size_t newLength = oldLength + appendLen;
    void* newBuffer = buffer ? std::realloc(buffer, newLength) : std::malloc(newLength);
    if (!newBuffer) {
        return;
    }

    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, src, appendLen);
    buffer = newBuffer;
    length = static_cast<uint16_t>(newLength & 0xffffu);
}

static void AssignOwnedSmallString(
    AuthBootstrapSelectedSource38Sketch& dest,
    const char* begin,
    const char* current) {
    dest.string60Owned.clear();
    dest.string60 = {};

    if (!begin || !current || current <= begin) {
        return;
    }

    dest.string60Owned.assign(begin, current);
    dest.string60.begin = dest.string60Owned.c_str();
    dest.string60.current = dest.string60Owned.c_str() + dest.string60Owned.size();
    dest.string60.capacity = dest.string60.current;
}

static uint32_t __thiscall Arg6CurrentSlotRecord44_Destroy(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall Arg6CurrentSlotRecord44_TinyGetter(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

static uint32_t __thiscall Arg6CurrentSlotRecord44_AppendDebugString(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall Arg6CurrentSlotRecord44_ResetPayloadForSourceDescriptor(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 1u;
}

static uint32_t __thiscall Arg6CurrentSlotRecord44_TinyHelper(Arg6CurrentSlotRecord44ObjectSketch* self) {
    (void)self;
    return 0u;
}

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

static const CLTLoginMediator* ResolveActiveSelectionCfgCorpusOwner(const CLTLoginMediator* mediator) {
    if (const auto* loginController = DiagnosticAuthGetLoginController()) {
        return loginController;
    }
    return mediator;
}

static const CLTLoginMediator* ResolveActiveState8PersistenceOwner(const CLTLoginMediator* mediator) {
    // Keep the wrapper-facing split explicit:
    // - arg6 `ILTLoginMediator.Default` entrypoints may be invoked on the binder-owned stub object
    // - the live `mcd.cfg` family still belongs to the active login-controller owner when one exists
    if (const auto* loginController = DiagnosticAuthGetLoginController()) {
        return loginController;
    }
    return mediator;
}

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

CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      currentState_(nullptr),
      scaffoldState3_(nullptr),
      scaffoldState4_(nullptr),
      scaffoldState6_(nullptr),
      scaffoldState8_(nullptr),
      scaffoldState9_(nullptr),
      scaffoldState10_(nullptr),
      scaffoldState11_(nullptr),
      scaffoldState12_(nullptr),
      scaffoldState13_(nullptr),
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
      authBootstrap680_{},
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
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();
    InitializeArg6DefaultObject();
}

CLTLoginMediator::~CLTLoginMediator() {
    if (authConnectionOwnedByMediator_) {
        delete authConnection_;
    }
    if (marginConnectionOwnedByMediator_) {
        delete marginConnection_;
    }
    EraseMarginBootstrapState(this);
    EraseRecoveredAuthBootstrapSidecar(this);
}

// +0x00
const char* CLTLoginMediator::GetName() {
    return g_MediatorName;
}

// +0x08
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
// anchor launcher.exe:0x0041f060
void CLTLoginMediator::ClearEngine() {
    spdlog::info("MediatorStub::ClearEngine()");
}
// +0x10
uint32_t CLTLoginMediator::IsReady() {
    spdlog::info("CLTLoginMediator::IsReady() -> 1");
    return 1;
}

// +0x1c
// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
void CLTLoginMediator::SetValue1(void* value) {
    if (!this->lastNopatchValue1Ptr_) {
        this->lastNopatchValue1Ptr_ = value;
    } else {
        this->lastNopatchValue2Ptr_ = value;
    }
    spdlog::debug("CLTLoginMediator::SetValue1({})", value);
}

// +0x20
// anchor: launcher.exe:0x409a73..0x409a98 nopatch path configures arg6 through +0x1c and +0x24
// vtable: ILTLoginMediator.Default slots +0x1c and +0x24
void CLTLoginMediator::SetValue2(void* value) {
    if (!this->lastNopatchValue1Ptr_) {
        this->lastNopatchValue1Ptr_ = value;
    } else {
        this->lastNopatchValue2Ptr_ = value;
    }
    spdlog::debug("CLTLoginMediator::SetValue2({})", value);
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

const char* CLTLoginMediator::GetProfileRootName() const {
    const char* profileRootName = Arg6ProfileName();
    spdlog::debug(
        "CLTLoginMediator::GetProfileRootName(+0x38) -> '{}'",
        NonEmptyTextOrPlaceholder(profileRootName));
    return profileRootName;
}

const SlotRecordState004b5328* CLTLoginMediator::ResolveArg6CurrentSlotRecord44Source() const {
    const CLTLoginMediator* currentCharacterStateMediator = DiagnosticAuthGetLoginController();
    if (!currentCharacterStateMediator) {
        currentCharacterStateMediator = this;
    }

    const SlotRecordState004b5328* currentSlotRecord =
        currentCharacterStateMediator->GetCurrentSlotRecord();
    if (!currentSlotRecord) {
        currentSlotRecord = currentCharacterStateMediator->GetSlotRecordByIndex(0u);
    }
    return currentSlotRecord;
}

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
        arg6CurrentSlotRecord44Payload_.characterIdLow03 = DiagnosticAuthCurrentCharacterIdLow();
        arg6CurrentSlotRecord44Payload_.characterIdHigh07 = DiagnosticAuthCurrentCharacterIdHigh();
        const char* authCharacterName = DiagnosticAuthCurrentCharacterName();
        if (authCharacterName && authCharacterName[0]) {
            arg6CurrentSlotRecord44NameOwned_ = authCharacterName;
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

Arg6SelectionDescriptor40ObjectSketch* CLTLoginMediator::GetArg6SelectionDescriptorObject40(
    uint32_t selectionIndex,
    void* returnAddress) {
    const uint32_t low24 = selectionIndex & 0x00ffffffu;
    const uint32_t high8 = (selectionIndex >> 24) & 0xffu;
    const uint32_t expectedScratchRequest = Arg6ExpectedSelectionDescriptorScratchRequest();
    const bool matchedConfiguredRequest = Arg6SelectionDescriptorMatchesRequest(selectionIndex);
    const char* worldName = matchedConfiguredRequest ? Arg6MappedSelectionName() : nullptr;

    if (!worldName) {
        spdlog::debug(
            "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x} caller={} [{}]) -> NULL (configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x} worldUpperBoundExclusive={})",
            static_cast<unsigned>(selectionIndex),
            static_cast<unsigned>(low24),
            static_cast<unsigned>(high8),
            fmt::ptr(returnAddress),
            DescribeMediatorCaller(returnAddress),
            static_cast<unsigned>(Arg6SelectedWorldIndexLow24()),
            static_cast<unsigned>(Arg6SelectedVariantIndexHigh8()),
            static_cast<unsigned>(expectedScratchRequest),
            static_cast<unsigned>(Arg6WorldUpperBoundExclusive()));
        LogMediatorCharacterStateContext("GetArg6SelectionDescriptorObject40(+0x40)", returnAddress);
        return nullptr;
    }

    RefreshArg6CurrentSlotRecordObject44();

    const bool profilePathCaller = IsProfilePathBuilderCaller(returnAddress);
    const char* descriptorShape = "world-shaped";
    uint32_t field03 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(worldName));
    uint32_t field07 = Arg6MappedSelectionId();
    if (profilePathCaller) {
        descriptorShape = "current-slot-id-shaped";
        field03 = arg6CurrentSlotRecord44Payload_.characterIdLow03;
        field07 = arg6CurrentSlotRecord44Payload_.characterIdHigh07;
        if (field03 == 0u && field07 == 0u) {
            field03 = DiagnosticAuthCurrentCharacterIdLow();
            field07 = DiagnosticAuthCurrentCharacterIdHigh();
        }
    }

    arg6SelectionDescriptor40Packed_ = {};
    arg6SelectionDescriptor40_ = {};
    arg6SelectionDescriptor40Packed_.field03 = field03;
    arg6SelectionDescriptor40Packed_.field07 = field07;
    arg6SelectionDescriptor40_.packed = &arg6SelectionDescriptor40Packed_;

    const char* matchMode =
        (selectionIndex == expectedScratchRequest) ? "arg7-scratch-shape" :
        ((low24 == Arg6SelectedWorldIndexLow24()) ? "low24-world-match" : "other-match");
    spdlog::debug(
        "CLTLoginMediator::GetArg6SelectionDescriptorObject40(+0x40 selectionIndex=0x{:08x} low24=0x{:06x} high8=0x{:02x} caller={} [{}]) -> {} (matchMode={} descriptorShape={} mappedName='{}' field03=0x{:08x} field07=0x{:08x} field03AsPtr={} configuredWorld=0x{:06x} configuredVariant=0x{:02x} expectedScratchRequest=0x{:08x})",
        static_cast<unsigned>(selectionIndex),
        static_cast<unsigned>(low24),
        static_cast<unsigned>(high8),
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(&arg6SelectionDescriptor40_),
        matchMode,
        descriptorShape,
        worldName,
        static_cast<unsigned>(arg6SelectionDescriptor40Packed_.field03),
        static_cast<unsigned>(arg6SelectionDescriptor40Packed_.field07),
        fmt::ptr(reinterpret_cast<const void*>(static_cast<uintptr_t>(arg6SelectionDescriptor40Packed_.field03))),
        static_cast<unsigned>(Arg6SelectedWorldIndexLow24()),
        static_cast<unsigned>(Arg6SelectedVariantIndexHigh8()),
        static_cast<unsigned>(expectedScratchRequest));
    LogMediatorCharacterStateContext("GetArg6SelectionDescriptorObject40(+0x40)", returnAddress);
    return &arg6SelectionDescriptor40_;
}

Arg6CurrentSlotRecord44ObjectSketch* CLTLoginMediator::GetArg6CurrentSlotRecordObject44(
    void* returnAddress) {
    const bool hasCurrentSlot = RefreshArg6CurrentSlotRecordObject44();
    const void* currentSlotRecordPtr = hasCurrentSlot
        ? static_cast<const void*>(&arg6CurrentSlotRecord44_)
        : nullptr;

    spdlog::info(
        "CLTLoginMediator::GetArg6CurrentSlotRecordObject44(+0x44 caller={} [{}]) -> {} [name='{}' idLow=0x{:08x} idHigh=0x{:08x} status=0x{:02x} worldId=0x{:04x}]",
        fmt::ptr(returnAddress),
        DescribeMediatorCaller(returnAddress),
        fmt::ptr(currentSlotRecordPtr),
        arg6CurrentSlotRecord44_.heapString14 ? arg6CurrentSlotRecord44_.heapString14 : "<empty>",
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.characterIdLow03),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.characterIdHigh07),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.status0b),
        static_cast<unsigned>(arg6CurrentSlotRecord44Payload_.worldId0c));
    LogMediatorCharacterStateContext("GetArg6CurrentSlotRecordObject44(+0x44)", returnAddress);
    return hasCurrentSlot ? &arg6CurrentSlotRecord44_ : nullptr;
}

const char* CLTLoginMediator::GetWorldOrSelectionName() const {
    const SlotRecordState004b5328* slotRecord = GetCurrentSlotRecord();
    if (!slotRecord) {
        slotRecord = GetSlotRecordByIndex(0u);
    }

    const auto& ownerState = PostAuthMarginLoadingStateView();
    const char* authCharacterName = DiagnosticAuthCurrentCharacterName();
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
    } else if (authCharacterName && authCharacterName[0]) {
        worldOrSelectionName = authCharacterName;
        source = "auth-current-character";
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

const char* CLTLoginMediator::GetProfileOrSessionName() const {
    const char* profileOrSessionName = Arg6ProfileName();
    spdlog::debug(
        "CLTLoginMediator::GetProfileOrSessionName(+0x4c) -> '{}'",
        NonEmptyTextOrPlaceholder(profileOrSessionName));
    return profileOrSessionName;
}

// anchor: launcher.exe:0x41f370 / owner vtable +0x50
// Later runtime uses the auth-reply-derived bootstrap `+0xf4` copy, not the earlier direct
// bootstrap `+0xa8` field. Keep that extra level explicit in source too.
void* CLTLoginMediator::BootstrapRaw08AuxHandle50() const {
    const auto* fieldF4 = static_cast<const AuthBootstrapReplyShadowF4Sketch*>(authBootstrap680_.fieldF4);
    void* value = fieldF4 ? fieldF4->raw08AuxHandleA8 : nullptr;

    if (!bootstrapRaw08AuxHandle50Logged_ || lastBootstrapRaw08AuxHandle50_ != value) {
        spdlog::info(
            "CLTLoginMediator::BootstrapRaw08AuxHandle50(+0x50) -> {}{}",
            fmt::ptr(value),
            bootstrapRaw08AuxHandle50Logged_ ? " [changed]" : " [first]");
        bootstrapRaw08AuxHandle50Logged_ = true;
        lastBootstrapRaw08AuxHandle50_ = value;
    }

    return value;
}

// anchor: launcher.exe:0x41f0b0 / owner vtable +0x54
// Tiny bool wrapper over `+0x50`.
bool CLTLoginMediator::HasBootstrapRaw08AuxHandle54() const {
    const auto* fieldF4 = static_cast<const AuthBootstrapReplyShadowF4Sketch*>(authBootstrap680_.fieldF4);
    const bool present = fieldF4 && fieldF4->raw08AuxHandleA8 != nullptr;
    spdlog::debug(
        "CLTLoginMediator::HasBootstrapRaw08AuxHandle54(+0x54) -> {}",
        present ? 1u : 0u);
    return present;
}

// anchor: launcher.exe:0x41f390 / owner vtable +0x58
// Keep the split explicit:
// - owner getter returns bootstrap child byte `+0x680 + 0x104`
// - launcher/client wrapper-facing consumers use that low byte as crashreporter
//   `PromptForSecurId`
uint8_t CLTLoginMediator::GetCrashReporterPromptForSecurId58() const {
    const uint8_t prompt = authBootstrap680_.crashReporterPromptForSecurId104;
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
const char* CLTLoginMediator::GetCrashReporterUsername5c(const void* chainedValueToken) {
    const char* authName = Arg6AuthName();
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterUsername5c(+0x5c chainedValueToken={}) -> '{}'",
        fmt::ptr(chainedValueToken),
        NonEmptyTextOrPlaceholder(authName));
    return authName;
}

const char* CLTLoginMediator::GetCrashReporterPassword60(const void* chainedValueToken) {
    const char* authPassword = Arg6AuthPassword();
    spdlog::info(
        "CLTLoginMediator::GetCrashReporterPassword60(+0x60 chainedValueToken={}) -> {}",
        fmt::ptr(chainedValueToken),
        MaskedSensitiveValue(authPassword));
    return authPassword;
}

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

// anchor: launcher.exe:0x41f240
// vtable: ILTLoginMediator.Default slot +0x178
uint32_t CLTLoginMediator::GetLastLoginStatus() {
    const uint32_t status = this->WorldListCountOrStatus80();
    this->lastStatus178_ = status;
    ++this->statusQuery178Count_;
    spdlog::debug("CLTLoginMediator::GetLastLoginStatus(+0x178) -> 0x{:08x} [count={}]",
        status,
        this->statusQuery178Count_);
    return status;
}


void CLTLoginMediator::SetCurrentState(CLTLoginState* state) {
    currentState_ = state;
}

CLTLoginState* CLTLoginMediator::CurrentState() const {
    return currentState_;
}

uint32_t CLTLoginMediator::State6UdpSessionSecretF18() const {
    const auto it = g_marginBootstrapStateByMediator.find(this);
    return (it != g_marginBootstrapStateByMediator.end()) ? it->second.state6UdpSessionSecretF18 : 0u;
}

void CLTLoginMediator::SetState6UdpSessionSecretF18(uint32_t value) {
    MutableMarginBootstrapState(this).state6UdpSessionSecretF18 = value;
}

bool CLTLoginMediator::CopyMarginBootstrapTwofishKeyScaffold(std::array<uint8_t, 16>* outKey) const {
    if (!outKey) {
        return false;
    }
    outKey->fill(0u);

    const auto it = g_marginBootstrapStateByMediator.find(this);
    if (it == g_marginBootstrapStateByMediator.end() || it->second.marginTwofishKeyBytes.size() != 16u) {
        return false;
    }

    std::copy_n(it->second.marginTwofishKeyBytes.begin(), 16u, outKey->begin());
    return true;
}

const void* CLTLoginMediator::GetState9CallbackSeedPointer85D4() const {
    const auto it = g_marginBootstrapStateByMediator.find(this);
    if (it == g_marginBootstrapStateByMediator.end() || it->second.marginTwofishKeyBytes.size() != 16u) {
        spdlog::info("CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> <null> (missing 16-byte margin Twofish key)");
        return nullptr;
    }

    const void* seedPointer = it->second.marginTwofishKeyBytes.data();
    spdlog::info(
        "CLTLoginMediator::GetState9CallbackSeedPointer85D4(+0xd4) -> {}",
        fmt::ptr(seedPointer));
    return seedPointer;
}

void CLTLoginMediator::RegisterScaffoldState3(CLTLoginState* state) {
    scaffoldState3_ = state;
}

void CLTLoginMediator::RegisterScaffoldState4(CLTLoginState* state) {
    scaffoldState4_ = state;
}

void CLTLoginMediator::RegisterScaffoldState6(CLTLoginState* state) {
    scaffoldState6_ = state;
}

void CLTLoginMediator::RegisterScaffoldState8(CLTLoginState* state) {
    scaffoldState8_ = state;
}

void CLTLoginMediator::RegisterScaffoldState9(CLTLoginState* state) {
    scaffoldState9_ = state;
}

void CLTLoginMediator::RegisterScaffoldState10(CLTLoginState* state) {
    scaffoldState10_ = state;
}

void CLTLoginMediator::RegisterScaffoldState11(CLTLoginState* state) {
    scaffoldState11_ = state;
}

void CLTLoginMediator::RegisterScaffoldState12(CLTLoginState* state) {
    scaffoldState12_ = state;
}

void CLTLoginMediator::RegisterScaffoldState13(CLTLoginState* state) {
    scaffoldState13_ = state;
}

CLTLoginState* CLTLoginMediator::ScaffoldState3() const {
    return scaffoldState3_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState4() const {
    return scaffoldState4_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState6() const {
    return scaffoldState6_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState8() const {
    return scaffoldState8_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState9() const {
    return scaffoldState9_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState10() const {
    return scaffoldState10_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState11() const {
    return scaffoldState11_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState12() const {
    return scaffoldState12_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState13() const {
    return scaffoldState13_;
}

void CLTLoginMediator::SetAuthConnectionContextKey(void* contextKey) {
    authConnectionContextKey_ = contextKey;
}

void CLTLoginMediator::SetMarginConnectionContextKey(void* contextKey) {
    marginConnectionContextKey_ = contextKey;
}

void CLTLoginMediator::SetAuthCredentials(const char* username, const char* password) {
    authUsername_ = username ? username : "";
    authPassword_ = password ? password : "";
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    ResetMarginBootstrapState();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth credentials configured username='%s' password=%s",
        authUsername_.empty() ? "<empty>" : authUsername_.c_str(),
        MaskedAuthValue(authPassword_));
}

void CLTLoginMediator::SetAuthBootstrapConfig(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    uint8_t loginType,
    const std::vector<uint8_t>& keyConfigMd5,
    const std::vector<uint8_t>& uiConfigMd5) {
    authLauncherVersion_ = launcherVersion;
    authCurrentPublicKeyId_ = currentPublicKeyId;
    authLoginType_ = loginType;
    authKeyConfigMd5_ = keyConfigMd5;
    authUiConfigMd5_ = uiConfigMd5;
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator auth bootstrap configured launcherVersion=%u currentPublicKeyId=%u loginType=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
        (unsigned)authLauncherVersion_,
        (unsigned)authCurrentPublicKeyId_,
        (unsigned)authLoginType_,
        (unsigned)authKeyConfigMd5_.size(),
        (unsigned)authUiConfigMd5_.size());
}

void CLTLoginMediator::SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile) {
    authServerDnsName_ = dnsName ? dnsName : "";
    authServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForAuth_ = ignoreHostsFile;
    BuildAuthEndpoint();
}

void CLTLoginMediator::SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile) {
    marginServerDnsSuffix_ = dnsSuffix ? dnsSuffix : "";
    marginServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForMargin_ = ignoreHostsFile;
    BuildMarginEndpoint();
}

std::string CLTLoginMediator::ResolvedMarginHostName() const {
    if (!marginRouteState_.exactMarginHostName.empty()) {
        return marginRouteState_.exactMarginHostName;
    }
    if (!marginRouteState_.routeHostPrefix.empty() && !marginServerDnsSuffix_.empty()) {
        return marginRouteState_.routeHostPrefix + marginServerDnsSuffix_;
    }
    return std::string();
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::AuthConnection() const {
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::MarginConnection() const {
    return marginConnection_;
}

void CLTLoginMediator::SetMarginRouteHostPrefix(const char* routeHostPrefix) {
    marginRouteState_.routeHostPrefix = routeHostPrefix ? routeHostPrefix : "";
}

void CLTLoginMediator::SetExactMarginHostName(const char* exactMarginHostName) {
    marginRouteState_.exactMarginHostName = exactMarginHostName ? exactMarginHostName : "";
}

const CLTLoginMediator::MarginRouteState& CLTLoginMediator::CurrentMarginRouteState() const {
    return marginRouteState_;
}

void CLTLoginMediator::InitializeConnectionHelpers() {
    // anchor: launcher.exe:0x43b300
    // Initializes the helper/state dispatch table rooted at `0x4f7868..0x4f78b4`.
    // The active source only keeps the late slot initializers recovered concretely so far
    // (`0x420640/0x4206e0/0x420850/0x420920/0x4209a0`).
    InitializeHelperDispatchSlot15();
    InitializeHelperDispatchSlot16();
    InitializeHelperDispatchSlot17();
    InitializeHelperDispatchSlot18();
    InitializeHelperDispatchSlot19();
}

void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    // Address anchor: launcher.exe:0x420640 = InitializeHelperDispatchSlot15
    // Original: allocates 8 bytes, stores vtable 0x4b51e0 (`CLTLoginState_State0`)
    // Current best concrete object identity: heap object with vtable `0x4b51e0` (`CLTLoginState_State0`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b51e0);
        helpers_.helper78A4 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    // Address anchor: launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16
    // Original: allocates 4 bytes, stores vtable 0x4b4fec (`CLTLoginState_WorldListPending`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fec);
        helpers_.helper78A8 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    // Address anchor: launcher.exe:0x420850 = InitializeHelperDispatchSlot17
    // Original: allocates 4 bytes, stores vtable 0x4b4fc4 (`CLTLoginState_State1`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fc4);
        helpers_.helper78AC = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    // Address anchor: launcher.exe:0x420920 = InitializeHelperDispatchSlot18
    // Original: allocates 8 bytes, stores vtable 0x4b5014 (`CLTLoginState_AuthenticatePending`)
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b5014);
        helpers_.helper78B0 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    // Address anchor: launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19
    // Original: allocates 4 bytes, stores vtable 0x4b508c (`CLTLoginState_State6`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b508c);
        helpers_.helper78B4 = ptr;
    }
}

uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Address anchors:
    // - launcher.exe:0x41d170 = strongest current BeginAuthConnection implementation
    // - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection (upstream)
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best launcher path:
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort` from owner `+0x4c8`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)` (ensure-connected wrapper)
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    ResetMarginBootstrapState();
    ResetRecoveredAuthBootstrapDynamicStateScaffold();
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ = nullptr;
    BuildAuthEndpoint();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) return 0;
    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);
    return connection->EnsureConnected();
}

uint32_t CLTLoginMediator::HandleAuthConnectStatus(uint32_t workResultCode) {
    lastAuthConnectStatus_ = workResultCode;
    ++authConnectStatusCount_;
    return (workResultCode == kConnectStatusSuccess) ? BeginAuthHandshake() : 0u;
}

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

uint32_t CLTLoginMediator::BeginAuthHandshake() {
    // Address anchors:
    // - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    // - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (upstream dispatcher)
    // - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
    // - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    //
    // Transitional note:
    // - current scaffold still begins with an explicit 0x06 request step
    // - original helper-state progression is not yet fully rebuilt around 0x448050
    // The standalone auth probe is the current wire/reference implementation for the
    // launcher-owned auth loop. Reuse the shared low-level runtime-style auth helpers here
    // instead of keeping a second launcher-only packet path.
    expectedAuthRequestName_ = kMessageAsGetPublicKeyRequest;
    return SendAuthGetPublicKeyRequest();
}

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
uint32_t CLTLoginMediator::ProcessLoginRequest(const ProcessLoginRequestInputSketch& input) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
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
            return 0x12000007u;
        default:
            break;
    }

    if ((input.inlineString00[0] == '\0' || input.inlineString20[0] == '\0') &&
        input.string60.current == input.string60.begin) {
        return 4u;
    }

    authBootstrapSource38_.inlineString00 = input.inlineString00;
    authBootstrapSource38_.inlineString20 = input.inlineString20;
    authBootstrapSource38_.block40 = input.block40;
    authBootstrapSource38_.block50 = input.block50;
    authBootstrapSource38_.flag6C = input.flag6C;
    AssignOwnedSmallString(authBootstrapSource38_, input.string60.begin, input.string60.current);

    if (currentState_ && currentState_->DispatchPhaseCode() == 2u) {
        spdlog::info(
            "DIAGNOSTIC: ProcessLoginRequest copied owner+0x94 username='{}' password='{}' string60Len={} and remains on helper state 2",
            authBootstrapSource38_.inlineString00[0] ? authBootstrapSource38_.inlineString00.data() : "<empty>",
            authBootstrapSource38_.inlineString20[0] ? authBootstrapSource38_.inlineString20.data() : "<empty>",
            static_cast<unsigned>(authBootstrapSource38_.string60Owned.size()));
    } else {
        spdlog::info(
            "DIAGNOSTIC: ProcessLoginRequest copied owner+0x94 username='{}' password='{}' string60Len={} (state-switch scaffolding beyond active state-2 branch still partial)",
            authBootstrapSource38_.inlineString00[0] ? authBootstrapSource38_.inlineString00.data() : "<empty>",
            authBootstrapSource38_.inlineString20[0] ? authBootstrapSource38_.inlineString20.data() : "<empty>",
            static_cast<unsigned>(authBootstrapSource38_.string60Owned.size()));
    }

    // Active original branch observed under WineDbg had `DAT_004d66ec == 0`, which means:
    // - clear owner `+0xf4` (`+0x94 + 0x60`) through the small-string helper
    // - switch helper state to `2`
    // Current source scaffold keeps the already-live state-2 side conservative and only mirrors
    // the owner-state mutation here.
    AssignOwnedSmallString(authBootstrapSource38_, nullptr, nullptr);
    spdlog::info(
        "DIAGNOSTIC: ProcessLoginRequest mirrored default DAT_004d66ec==0 branch by clearing owner+0xf4 small-string state");
    return 0u;
}
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
    // Writes the state3 selection/config snapshot into owner `+0xcc8/+0xcd0..+0xd7f`, then
    // switches to helper/state `8`.
    selectionContext0ecCopy_ = input;
    selectionContext0ecCopyValid_ = true;
    ++selection0ecCount_;
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
        "CLTLoginMediator::PersistSelectionContextForState8 mirrored state3->8 selection snapshot slot=0x{:02x} blockCd0_0=0x{:08x} blockD70_3=0x{:08x} currentState={}",
        state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8,
        state8SelectionContextSnapshotState_.blockCd0[0],
        state8SelectionContextSnapshotState_.blockD70[3],
        currentState_ ? currentState_->DebugName() : "<unchanged>");
    return 0u;
}

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

    const char* characterName = nullptr;
    const char* realFirstName = nullptr;
    const char* realLastName = nullptr;
    const char* background = nullptr;

    if (const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord()) {
        if (!currentSlotRecord->heapString14.empty()) {
            characterName = currentSlotRecord->heapString14.c_str();
        }
    }
    characterName = preferNonEmpty(characterName, LookupSlotRecordHeapStringByIndex(0));
    const auto& ownerState = PostAuthMarginLoadingStateView();
    characterName = preferNonEmpty(characterName, ownerState.characterNameBufferF1c);
    characterName = preferNonEmpty(characterName, SourceLeadString108().data());

    const char* sourceBlock178 = reinterpret_cast<const char*>(SourceBlock178().data());
    const char* sourceBlock198 = reinterpret_cast<const char*>(SourceBlock198().data());
    const char* sourceBlock1b8 = reinterpret_cast<const char*>(SourceBlock1b8().data());
    const char* section0F8c = ownerState.section0StringF8c[0] ? ownerState.section0StringF8c.data() : nullptr;
    const char* section0Fac = ownerState.section0StringFac[0] ? ownerState.section0StringFac.data() : nullptr;
    const char* section0Fcc = ownerState.section0StringFcc[0] ? ownerState.section0StringFcc.data() : nullptr;
    const bool section0LooksLikeMiddleFirstLast =
        section0F8c != nullptr &&
        std::char_traits<char>::length(section0F8c) == 1u &&
        section0Fac != nullptr && section0Fac[0] != '\0' &&
        section0Fcc != nullptr && section0Fcc[0] != '\0';

    if (section0LooksLikeMiddleFirstLast) {
        realFirstName = preferNonEmpty(sourceBlock178, section0Fac);
        realLastName = preferNonEmpty(sourceBlock198, section0Fcc);
        background = preferNonEmpty(sourceBlock1b8, nullptr);
    } else {
        realFirstName = preferNonEmpty(sourceBlock178, section0F8c);
        realLastName = preferNonEmpty(sourceBlock198, section0Fac);
        background = preferNonEmpty(sourceBlock1b8, section0Fcc);
    }

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
    state8PersistenceF1c_.field28 = 0u;
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

// anchor: launcher.exe:0x440320
uint32_t CLTLoginMediator::State11HandleLoadCharacterReplyScaffold(const uint8_t* packetBytes, size_t packetSize) {
    // Evidence-backed current read:
    // - validates raw margin code `0x10`
    // - on first state11/load-character fragment it clears/seeds the owner `+0xf1c` family
    // - section selector `(byte+0x0d) - 2` drives later per-section writes
    // - cases `3/4/5/6` append bytes into owned buffers at `+0x1418/+0x1420/+0x1428/+0x1408`
    // - case `0` fills the `+0xf88/+0x13cc/+0x13d0` family plus three inline strings
    if (!packetBytes || packetSize < 0x10) {
        return 0u;
    }
    if (packetBytes[0] != 0x10) {
        return 0u;
    }

    const uint32_t parsedStatus = ReadU32LE(packetBytes + 1);
    const uint32_t parsedField05 = ReadU32LE(packetBytes + 5);
    const uint16_t parsedHandoffWord09 = ReadU16LE(packetBytes + 9);
    const uint8_t parsedExpectedCount0b = packetBytes[0x0b];
    const uint8_t parsedSeedCount0c = packetBytes[0x0c];
    const uint8_t parsedSelectorMinus2 = static_cast<uint8_t>(packetBytes[0x0d] - 2u);
    const uint16_t sectionOffset0e = ReadU16LE(packetBytes + 0x0e);

    postAuthMarginLoadingState_.worldListCountOrStatus80 = parsedStatus;
    if (parsedStatus >= 1u) {
        postAuthMarginLoadingState_.sourceLeadString108[0] = '\0';
        postAuthMarginLoadingState_.characterRouteIndexCc8 = 0xffu;
        spdlog::info(
            "DIAGNOSTIC: state11 load-character scaffold observed non-success status=0x{:08x} handoffWord=0x{:04x} and mirrored failure-side owner clears",
            static_cast<unsigned>(parsedStatus),
            static_cast<unsigned>(parsedHandoffWord09));
        return 0u;
    }

    uint16_t sectionByteCount = 0u;
    const uint8_t* sectionData = nullptr;
    if (sectionOffset0e != 0u && static_cast<size_t>(sectionOffset0e) + 2u <= packetSize) {
        sectionByteCount = ReadU16LE(packetBytes + sectionOffset0e);
        const size_t sectionDataOffset = static_cast<size_t>(sectionOffset0e) + 2u;
        if (sectionDataOffset <= packetSize) {
            sectionData = packetBytes + sectionDataOffset;
            const size_t remaining = packetSize - sectionDataOffset;
            if (sectionByteCount > remaining) {
                sectionByteCount = static_cast<uint16_t>(remaining);
            }
        }
    }

    const bool firstFragment = (postAuthMarginLoadingState_.characterNameBufferF1c[0] == '\0');
    bool usedCurrentSlotRecord = false;
    if (firstFragment) {
        std::fill(
            std::begin(postAuthMarginLoadingState_.characterNameBufferF1c),
            std::end(postAuthMarginLoadingState_.characterNameBufferF1c),
            '\0');
        postAuthMarginLoadingState_.characterReplyFieldF3c = parsedField05;
        postAuthMarginLoadingState_.characterReplyFieldF40 = postAuthMarginLoadingState_.sourceField12c;
        std::fill(postAuthMarginLoadingState_.characterFlagsF48.begin(), postAuthMarginLoadingState_.characterFlagsF48.end(), 0u);
        std::fill(postAuthMarginLoadingState_.secondaryCharacterDataF68.begin(), postAuthMarginLoadingState_.secondaryCharacterDataF68.end(), 0u);
        std::fill(postAuthMarginLoadingState_.characterRecordPointersF88.begin(), postAuthMarginLoadingState_.characterRecordPointersF88.end(), 0u);
        std::fill(postAuthMarginLoadingState_.section0StringF8c.begin(), postAuthMarginLoadingState_.section0StringF8c.end(), '\0');
        std::fill(postAuthMarginLoadingState_.section0StringFac.begin(), postAuthMarginLoadingState_.section0StringFac.end(), '\0');
        std::fill(postAuthMarginLoadingState_.section0StringFcc.begin(), postAuthMarginLoadingState_.section0StringFcc.end(), '\0');
        postAuthMarginLoadingState_.replySectionData13cc = 0u;
        postAuthMarginLoadingState_.replySectionData13d0 = 0u;
        postAuthMarginLoadingState_.section0Flag13f6 = 0u;
        postAuthMarginLoadingState_.flag13fe = 1u;
        postAuthMarginLoadingState_.flag1406 = 1u;
        postAuthMarginLoadingState_.flag1416 = 1u;
        postAuthMarginLoadingState_.flag1448 = 1u;
        postAuthMarginLoadingState_.flag1452 = 1u;

        if (postAuthMarginLoadingState_.allocatedBuffer1418) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1418);
            postAuthMarginLoadingState_.allocatedBuffer1418 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1420) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1420);
            postAuthMarginLoadingState_.allocatedBuffer1420 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1428) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1428);
            postAuthMarginLoadingState_.allocatedBuffer1428 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1408) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1408);
            postAuthMarginLoadingState_.allocatedBuffer1408 = nullptr;
        }
        postAuthMarginLoadingState_.allocatedBufferLength141c = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength1424 = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength142c = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength140c = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag141e = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag1426 = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag142e = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag140e = 0u;

        if (const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord()) {
            const size_t copyCount = std::min(
                currentSlotRecord->heapString14.size(),
                sizeof(postAuthMarginLoadingState_.characterNameBufferF1c) - 1);
            std::copy_n(
                currentSlotRecord->heapString14.data(),
                copyCount,
                postAuthMarginLoadingState_.characterNameBufferF1c);
            postAuthMarginLoadingState_.characterNameBufferF1c[copyCount] = '\0';
            postAuthMarginLoadingState_.secondaryCharacterDataF68[0] = currentSlotRecord->worldId0c;
            postAuthMarginLoadingState_.secondaryCharacterDataF68[1] = currentSlotRecord->status0b;
            usedCurrentSlotRecord = true;
        } else {
            std::copy(
                postAuthMarginLoadingState_.sourceLeadString108.begin(),
                postAuthMarginLoadingState_.sourceLeadString108.end(),
                postAuthMarginLoadingState_.characterNameBufferF1c);
            postAuthMarginLoadingState_.secondaryCharacterDataF68[0] = postAuthMarginLoadingState_.sourceField12c;
            postAuthMarginLoadingState_.secondaryCharacterDataF68[1] = 0u;
        }

        std::copy_n(
            postAuthMarginLoadingState_.sourceDwords134.begin(),
            postAuthMarginLoadingState_.characterFlagsF48.size(),
            postAuthMarginLoadingState_.characterFlagsF48.begin());
    }

    switch (parsedSelectorMinus2) {
        case 0u:
            if (sectionData && sectionByteCount >= 0x44cu) {
                postAuthMarginLoadingState_.characterRecordPointersF88[0] = ReadU32LE(sectionData + 0x00);
                postAuthMarginLoadingState_.replySectionData13cc = ReadU32LE(sectionData + 0x444);
                postAuthMarginLoadingState_.replySectionData13d0 = ReadU32LE(sectionData + 0x448);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringF8c.data(),
                    postAuthMarginLoadingState_.section0StringF8c.size(),
                    sectionData + 0x04,
                    sectionByteCount - 0x04);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringFac.data(),
                    postAuthMarginLoadingState_.section0StringFac.size(),
                    sectionData + 0x24,
                    sectionByteCount > 0x24 ? sectionByteCount - 0x24 : 0u);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringFcc.data(),
                    postAuthMarginLoadingState_.section0StringFcc.size(),
                    sectionData + 0x44,
                    sectionByteCount > 0x44 ? sectionByteCount - 0x44 : 0u);
                postAuthMarginLoadingState_.section0Flag13f6 = 1u;
            }
            break;
        case 3u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1418,
                postAuthMarginLoadingState_.allocatedBufferLength141c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag141e = 1u;
            break;
        case 4u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1420,
                postAuthMarginLoadingState_.allocatedBufferLength1424,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag1426 = 1u;
            break;
        case 5u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1428,
                postAuthMarginLoadingState_.allocatedBufferLength142c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag142e = 1u;
            break;
        case 6u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1408,
                postAuthMarginLoadingState_.allocatedBufferLength140c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag140e = 1u;
            break;
        case 0x0bu:
            spdlog::info(
                "DIAGNOSTIC: state11 load-character scaffold observed section 0x0b with byteCount={}; downstream `0x43f8c0` side effect still unresolved",
                static_cast<unsigned>(sectionByteCount));
            break;
        default:
            break;
    }

    spdlog::info(
        "DIAGNOSTIC: state11 load-character scaffold status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} expectedCount={} seedCount={} section={} sectionBytes={} firstFragment={} usedCurrentSlotRecord={} name='{}'",
        static_cast<unsigned>(parsedStatus),
        static_cast<unsigned>(parsedField05),
        static_cast<unsigned>(parsedHandoffWord09),
        static_cast<unsigned>(parsedExpectedCount0b),
        static_cast<unsigned>(parsedSeedCount0c),
        static_cast<unsigned>(parsedSelectorMinus2),
        static_cast<unsigned>(sectionByteCount),
        firstFragment ? 1u : 0u,
        usedCurrentSlotRecord ? 1u : 0u,
        postAuthMarginLoadingState_.characterNameBufferF1c);
    return 1u;
}

// anchor: launcher.exe:0x41f2e0
const SlotRecordState004b5328* CLTLoginMediator::GetSlotRecordByIndex(uint8_t slotIndex) const {
    if (slotIndex >= slotRecordValid688_.size() || !slotRecordValid688_[slotIndex]) {
        return nullptr;
    }
    return &slotRecords688_[slotIndex];
}

// anchor: launcher.exe:0x41f300
const SlotRecordState004b5328* CLTLoginMediator::GetCurrentSlotRecord() const {
    return GetSlotRecordByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8);
}

// Accessors for migrated state fields (diagnostics only)
const void* CLTLoginMediator::LastNopatchValue1Ptr() const {
    return lastNopatchValue1Ptr_;
}

const void* CLTLoginMediator::LastNopatchValue2Ptr() const {
    return lastNopatchValue2Ptr_;
}

uint32_t CLTLoginMediator::LastStatus178() const {
    return lastStatus178_;
}

uint32_t CLTLoginMediator::StatusQuery178Count() const {
    return statusQuery178Count_;
}

// anchor: launcher.exe:0x41b220
const char* CLTLoginMediator::LookupSlotRecordHeapStringByIndex(uint8_t slotIndex) const {
    const SlotRecordState004b5328* record = GetSlotRecordByIndex(slotIndex);
    if (!record || record->heapString14.empty()) {
        return nullptr;
    }
    return record->heapString14.c_str();
}

// anchor: launcher.exe:0x41f310
SessionCallbackHelper65cSketch* CLTLoginMediator::GetSessionCallbackHelper65c() const {
    // Tiny owner-vtable getter used by the later session-callback helper family.
    return sessionCallbackHelper65c_;
}

// anchor: launcher.exe:0x41f2c0
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
    const mxo::liblttcp::CMessageConnectionEnvelopeScaffold& envelope) {
    // anchor: launcher.exe:0x41af70
    // Keep this narrow: `0x41af70` forwards the shared message/envelope object through the current
    // margin connection send bridge (`0x41cf30 -> 0x448cf0`) instead of flattening caller bytes
    // directly.
    mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    if (!connection) {
        connection = EnsureMarginConnectionObject();
    }
    if (!connection || !envelope.sharedMessage) {
        return 0u;
    }

    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    if (marginBootstrapState.phase == MarginBootstrapPhase::kReady &&
        !marginBootstrapState.marginTwofishKeyBytes.empty()) {
        const uint8_t* payloadBytes = envelope.sharedMessage->PayloadBaseScaffold();
        const uint32_t payloadByteCount = envelope.sharedMessage->PayloadByteCountScaffold();
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

    const std::vector<uint8_t> framedBytesFrom0a = envelope.sharedMessage->BuildFramedBytesFrom0aScaffold();
    const size_t previewOffset = framedBytesFrom0a.empty() ? 0u : (((framedBytesFrom0a[0] >> 7) == 0u) ? 1u : 0u);
    const std::string framedPreview =
        (previewOffset < framedBytesFrom0a.size())
            ? BuildHexPreview(framedBytesFrom0a.data() + previewOffset, framedBytesFrom0a.size() - previewOffset, 32u)
            : std::string();
    spdlog::info(
        "CLTLoginMediator::SendCurrentMarginPacketScaffold ForwardEnvelopeToSendPacket host='{}' state={} reservedBytes08=0x{:04x} payloadBytes={} framedBytesFrom0a={} submitOffset={} preview={} agendaGap=packet-processing-metadata-still-missing",
        connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
        static_cast<unsigned>(connection->State()),
        static_cast<unsigned>(envelope.sharedMessage->reservedBytes08),
        static_cast<unsigned>(envelope.sharedMessage->PayloadByteCountScaffold()),
        static_cast<unsigned>(framedBytesFrom0a.size()),
        static_cast<unsigned>(previewOffset),
        framedPreview.empty() ? std::string("<empty>") : framedPreview);
    return connection->SendPacketEnvelopeScaffold(envelope);
}

uint32_t CLTLoginMediator::SendCurrentMarginPacketScaffold(const void* packetBytes, uint32_t packetByteCount) {
    if (!packetBytes || packetByteCount == 0u) {
        return 0u;
    }

    const mxo::liblttcp::CMessageConnectionEnvelopeScaffold envelope =
        mxo::liblttcp::CMessageConnection::BuildPayloadEnvelopeScaffold(packetBytes, packetByteCount, /*headerless=*/false);
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
uint32_t CLTLoginMediator::RefreshSessionHelperGameSessionId664FromSourceBlock94() {
    // Current best source-owned mirror of the state18/session helper path:
    // - ensure owner helper `+0x65c`
    // - refresh helper string `+0x18` from owner `+0x94 + 0x60`
    // - then commit that helper string into owner `+0x664`
    SessionCallbackHelper65cSketch* helper = EnsureSessionCallbackHelper65c();
    if (helper == nullptr) {
        return 0u;
    }

    if (authBootstrapSource38_.string60.begin != nullptr && authBootstrapSource38_.string60.begin[0] != '\0') {
        helper->string18 = authBootstrapSource38_.string60.begin;
    }

    return CommitSessionCallbackHelperGameSessionId664();
}

// anchor: launcher.exe:0x41b260
const char* CLTLoginMediator::LookupRouteHostPrefixBySlot(uint8_t slotIndex) const {
    if (slotIndex >= routeHostStrings818_.size()) {
        return nullptr;
    }
    const RouteHostStringTripleState& slot = routeHostStrings818_[slotIndex];
    return slot.text.empty() ? nullptr : slot.text.c_str();
}

// anchor: launcher.exe:0x41b2a0
uint8_t CLTLoginMediator::GetSlotRecordStatusByIndex(uint8_t slotIndex) const {
    const SlotRecordState004b5328* record = GetSlotRecordByIndex(slotIndex);
    return record ? record->status0b : 7u;
}

// anchor: launcher.exe:0x41b2e0
const char* CLTLoginMediator::GetDescriptorInlineNameByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return nullptr;
    }
    const WorldDescriptorState004b533c& slot = worldDescriptorsD84_[slotIndex];
    return slot.inlineNamePlus03.empty() ? nullptr : slot.inlineNamePlus03.c_str();
}

// anchor: launcher.exe:0x41b320
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x17` (Status)
// - earlier docs/source had an off-by-one stale guess that put Type here
uint8_t CLTLoginMediator::GetDescriptorStatusByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return worldDescriptorsD84_[slotIndex].status17;
}

// anchor: launcher.exe:0x41b360
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x18` (Type)
// - earlier docs/source had an off-by-one stale guess that put server-version low byte here
uint8_t CLTLoginMediator::GetDescriptorTypeByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return worldDescriptorsD84_[slotIndex].type18;
}

// anchor: launcher.exe:0x41b3a0
uint8_t CLTLoginMediator::GetDescriptorPopulationNibbleByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    const uint8_t value = worldDescriptorsD84_[slotIndex].populationLevel1f & 0x0f;
    return (value >= 1u && value <= 3u) ? value : 0u;
}

const char* CLTLoginMediator::ResolveMarginRouteFromCurrentCharacterSlot() const {
    // Address anchors:
    // - launcher.exe:0x439300 case `7/8/0xd`
    // - owner byte `+0xcc8`
    // - owner vtable `+0xe0`
    //
    // Current best source-owned mirror returns the reconstructed route-host string triple's first
    // string for the current slot/index.
    return LookupRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8);
}

const char* CLTLoginMediator::ResolveMarginRouteFromDescriptorIndex(uint32_t descriptorIndex) const {
    // Address anchors:
    // - launcher.exe:0x439300 case `10`
    // - owner dword `+0x12c`
    // - owner vtable `+0xfc`
    //
    // Fresh tightening from `0x41c3c0` + `0x4401a0`:
    // - `0x41c3c0` bounds-checks input `+0x24` against owner vtable `+0xf8`
    // - later `0x4401a0` indexes owner `+0xd84` using owner `+0x12c`
    // - current best read is therefore that the state4 case-10 branch forwards a
    //   world-descriptor index/selector here, not a direct world-id payload
    if (descriptorIndex >= worldDescriptorCountD80_) {
        return nullptr;
    }
    return GetDescriptorInlineNameByIndex(static_cast<uint8_t>(descriptorIndex));
}

const char* CLTLoginMediator::ResolveMarginRouteFromWorldId(uint32_t worldId) const {
    // Address anchors:
    // - launcher.exe:0x439300 default branch
    // - owner vtable `+0xfc`
    //
    // Keep this narrower fallback helper for places where source still only has a recovered
    // world-id value and must rejoin it against the descriptor table.
    if (worldId > 0xffffu) {
        return nullptr;
    }

    const int descriptorIndex = FindRecoveredWorldDescriptorIndexByWorldId(static_cast<uint16_t>(worldId));
    if (descriptorIndex < 0) {
        return nullptr;
    }
    return GetDescriptorInlineNameByIndex(static_cast<uint8_t>(descriptorIndex));
}

const char* CLTLoginMediator::ResolveMarginRouteDescriptor() const {
    // Address anchors:
    // - launcher.exe:0x439300 case `6`
    // - owner vtable `+0x10c`
    //
    // Current best source-owned mirror of that branch:
    // - the original fetches an object through `+0x10c`
    // - then uses the first dword of that object as the string argument into `0x41e500`
    // - current scaffold keeps this narrow by preferring the already mirrored current-slot
    //   route-host string and only falling back to older diagnostic route text when needed
    if (const char* currentSlotRouteHost = LookupRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8)) {
        return currentSlotRouteHost;
    }
    return marginRouteState_.routeHostPrefix.empty() ? nullptr : marginRouteState_.routeHostPrefix.c_str();
}

// anchor: launcher.exe:0x41e500
uint32_t CLTLoginMediator::BeginMarginConnectionScaffold(const char* routeHostText, uint8_t cachedRouteSelector) {
    // Narrow transport/init helper intentionally kept separate from the state4 `0x439300`
    // case split.
    //
    // Current best recovered `0x41e500` shape:
    // - allocate/configure a margin-specific connection object at owner `+0x1c`
    // - clear owner byte `+0x2d`
    // - if arg2 == 0, refresh owner `+0x30`, rebuild owner `+0x3c`, select owner `+0x7c`,
    //   and materialize owner `+0x6c`
    // - increment owner dword `+0x24`
    // - clear owner `+0x7c`
    // - call `connection->+0x1c(owner+0x6c)`
    mxo::liblttcp::CMessageConnection* connection = EnsureMarginConnectionObject();
    if (!connection) {
        spdlog::warn("CLTLoginMediator::BeginMarginConnectionScaffold failed to allocate margin connection");
        return 0u;
    }

    marginConnectionFlag2d_ = 0;

    const bool shouldRefreshRouteState = (cachedRouteSelector == 0u) ||
                                         (marginAddressList3c_.ipv4NetworkOrderList.empty()) ||
                                         (marginEndpoint_.ipv4NetworkOrder == 0u);
    if (shouldRefreshRouteState) {
        if (routeHostText && routeHostText[0] != '\0') {
            marginRouteState_.routeHostPrefix = routeHostText;
        }

        const std::string marginHost = ResolvedMarginHostName();
        if (marginHost.empty()) {
            spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold has no resolved margin host");
            return 0u;
        }

        const bool routeChanged = (marginAddressList3c_.resolvedHostName != marginHost);
        if (routeChanged || marginAddressList3c_.ipv4NetworkOrderList.empty()) {
            if (!RebuildMarginAddressList()) {
                return 0u;
            }
        }

        if (marginSelectedIpv4_7c_ == 0u && !SelectMarginEndpointIpv4()) {
            spdlog::warn(
                "CLTLoginMediator::BeginMarginConnectionScaffold found no IPv4 candidates for '{}'",
                marginHost);
            return 0u;
        }

        BuildMarginEndpoint();
    }

    ++marginBeginCount24_;
    marginSelectedIpv4_7c_ = 0u;

    const std::string marginHost = ResolvedMarginHostName();
    if (!marginHost.empty()) {
        connection->SetRemoteHostName(marginHost.c_str());
    }
    connection->SetRemoteEndpoint(marginEndpoint_);

    const uint32_t result = connection->EnsureConnected();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnectionScaffold resolvedHost='{}' routeHostText='{}' selector=0x{:02x} beginCount={} selectedIpv4=0x{:08x} port={} ensureConnectedResult=0x{:08x}",
        marginHost.empty() ? std::string("<empty>") : marginHost,
        (routeHostText && routeHostText[0]) ? std::string(routeHostText) : std::string("<empty>"),
        cachedRouteSelector,
        marginBeginCount24_,
        marginEndpoint_.ipv4NetworkOrder,
        marginServerPortHostOrder_,
        result);
    if (result == 0u) {
        spdlog::debug(
            "CLTLoginMediator::BeginMarginConnectionScaffold connect failed host='{}' port={} ip=0x{:08x} selector={} beginCount={}",
            marginHost.empty() ? std::string("<empty>") : marginHost,
            marginServerPortHostOrder_,
            marginEndpoint_.ipv4NetworkOrder,
            cachedRouteSelector,
            marginBeginCount24_);
    }
    return result;
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
void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds(index)
    // - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)

    // Transitional stub preserves the slot structure for later completion.
    return worldSlots_[index];
}

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
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

void CLTLoginMediator::SetArg6ProfileName(const char* profileName) {
    arg6Selection_.profileName_ = (profileName && profileName[0]) ? profileName : "resurrections";
}

void CLTLoginMediator::SetArg6AuthName(const char* authName) {
    arg6Selection_.authName_ = (authName && authName[0]) ? authName : arg6Selection_.profileName_;
}

void CLTLoginMediator::SetArg6AuthPassword(const char* authPassword) {
    arg6Selection_.authPassword_ = authPassword ? authPassword : "";
}

uint32_t CLTLoginMediator::Arg6WorldUpperBoundExclusive() const {
    return arg6Selection_.worldUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6VariantUpperBoundExclusive() const {
    return arg6Selection_.variantUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldIndexLow24() const {
    return arg6Selection_.selectedWorldIndexLow24_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantIndexHigh8() const {
    return arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6SelectedSelectionGateByte100() const {
    return arg6Selection_.selectedSelectionGateByte100_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantState() const {
    return arg6Selection_.selectedVariantState_;
}

uint32_t CLTLoginMediator::Arg6MappedSelectionId() const {
    return arg6Selection_.mappedSelectionId_;
}

const char* CLTLoginMediator::Arg6MappedSelectionName() const {
    return arg6Selection_.mappedSelectionName_.c_str();
}

const char* CLTLoginMediator::Arg6MappedVariantName() const {
    return arg6Selection_.mappedVariantName_.c_str();
}

const char* CLTLoginMediator::Arg6ProfileName() const {
    return arg6Selection_.profileName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthName() const {
    return arg6Selection_.authName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthPassword() const {
    return arg6Selection_.authPassword_.c_str();
}

bool CLTLoginMediator::Arg6WorldIndexMatchesSelection(uint32_t worldIndex) const {
    return worldIndex == arg6Selection_.selectedWorldIndexLow24_;
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

uint32_t CLTLoginMediator::GetDefaultSelectionIndex() const {
    const uint32_t selectionIndex = Arg6SelectedWorldIndexLow24();
    spdlog::debug(
        "CLTLoginMediator::GetDefaultSelectionIndex(+0x3c) -> 0x{:06x}",
        static_cast<unsigned>(selectionIndex));
    return selectionIndex;
}

uint32_t CLTLoginMediator::GetArg7SelectionUpperBoundExclusive() const {
    const uint32_t upperBoundExclusive = Arg6VariantUpperBoundExclusive();
    spdlog::info(
        "CLTLoginMediator::GetArg7SelectionUpperBoundExclusive(+0xd8) -> {}",
        static_cast<unsigned>(upperBoundExclusive));
    return upperBoundExclusive;
}

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

uint32_t CLTLoginMediator::SendAuthFramedPacket(
    const mxo::auth::FramedPacket& packet,
    const char* stepLabel) {
    mxo::liblttcp::CMessageConnection* connection = AuthConnection();
    if (!connection) {
        connection = EnsureAuthConnectionObject();
    }
    if (!connection || packet.bytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    const uint32_t sendResult = connection->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth send step='{}' rawCode=0x{:02x} message='{}' headerLen={} payloadLen={} byteCount={} -> sendResult=0x{:08x}",
        (stepLabel && stepLabel[0]) ? stepLabel : "<unnamed>",
        rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        (unsigned)sendResult);
    return sendResult;
}

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

            mxo::auth::FramedPacket response;
            if (!mxo::auth::BuildMarginCertChallengeResponsePacket(
                    marginBootstrapState.certChallengeBytes,
                    marginBootstrapState.marginTwofishKeyBytes,
                    mxo::auth::kFrameModeAuto,
                    &response)) {
                spdlog::info("DIAGNOSTIC: launcher-owned margin failed to build CERT_ChallengeResponse");
                return 0u;
            }

            const uint32_t sendResult = SendMarginFramedPacket(
                response,
                0x03u,
                "CERT_ChallengeResponse",
                /*encryptedTransport=*/true);
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

            uint32_t clientDllVersion = authLauncherVersion_;
            if (!lastAuthReply_.worlds.empty() && lastAuthReply_.worlds[0].clientVersion != 0u) {
                clientDllVersion = lastAuthReply_.worlds[0].clientVersion;
            }

            mxo::auth::FramedPacket response;
            if (!mxo::auth::BuildMarginConnectRequestPacket(
                    authLauncherVersion_,
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

            // Narrow live mirror of the anchored state6 opcode-`9` success core:
            // - original `0x00440780` writes owner byte `+0xf14 = 1`
            // - then writes owner dword `+0xf18 = parsedReply(+0x09)`
            // - current best field read for that source dword is the opcode-`9`
            //   `UDPSessionSecret` / session-id value
            // Keep the live mirror limited to that proven write pair here; broader state6 wrapper
            // behavior (metric-id list processing, cached-upstream helper-switch/event flow, opcode-7
            // branch) remains source-owned but is not re-entered on the deliberate runtime path yet.
            postAuthMarginLoadingState_.state10SendGateFlagF14 = 1u;
            SetState6UdpSessionSecretF18(reply.sessionId);
            const uint32_t state6Handled = 1u;

            expectedMarginRequestName_ =
                (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u)
                    ? "existing-character state8 raw-0x0f margin packet"
                    : "post-auth helper state margin packet";
            spdlog::info(
                "DIAGNOSTIC: launcher-owned margin bootstrap completed sessionId=0x{:08x} field0d=0x{:04x} field0f=0x{:04x} field11=0x{:04x} field13=0x{:04x} field15=0x{:04x} state6Handled=0x{:08x} ownerF14={} ownerF18=0x{:08x} currentState={}",
                reply.sessionId,
                reply.field0d,
                reply.field0f,
                reply.field11,
                reply.field13,
                reply.field15,
                state6Handled,
                postAuthMarginLoadingState_.state10SendGateFlagF14,
                State6UdpSessionSecretF18(),
                currentState_ ? currentState_->DebugName() : "<null>");
            return currentState_ ? currentState_->Slot3_BeginOrContinue(currentState_, this) : 1u;
        }

        default:
            break;
    }

    return 0u;
}

uint32_t CLTLoginMediator::SendAuthGetPublicKeyRequest() {
    // Address anchors:
    // - launcher.exe:0x447eb0 = strongest current raw 0x06 send builder
    // - launcher.exe:0x448050 = upstream phase-2 bootstrap dispatcher
    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            authLauncherVersion_,
            authCurrentPublicKeyId_,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_GetPublicKeyRequest");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(packet, kMessageAsGetPublicKeyRequest);
    authGetPublicKeyRequestSent_ = (sendResult != 0u);
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply) {
    // Address anchors:
    // - launcher.exe:0x4474f0 = strongest current raw 0x08 / AS_AuthRequest send builder
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authUsername_.empty()) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without a username");
        return 0;
    }
    if (!reply.hasEmbeddedPublicKey) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth GetPublicKeyReply has no embedded public key material");
        return 0;
    }

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = authLoginType_;
    requestLayout.keyConfigMd5 = authKeyConfigMd5_;
    requestLayout.uiConfigMd5 = authUiConfigMd5_;
    requestLayout.rsaModulusBytes = reply.modulusBytes;
    requestLayout.rsaExponentBytes.assign(1u, reply.publicExponentByte);

    mxo::auth::AuthRequestBuildResult buildResult;
    if (!mxo::auth::BuildAuthRequestPacket(
            authUsername_,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::info("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest");
        return 0;
    }

    lastAuthRequestBuildResult_ = buildResult;
    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, kMessageAsAuthRequest);
    authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId=%u loginType=%u keySize=%u blobLen=%u usernameLengthField=%u usedReplyPublicKey=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
            (unsigned)reply.publicKeyId,
            (unsigned)authLoginType_,
            (unsigned)reply.keySize,
            (unsigned)buildResult.blobCiphertextBytes.size(),
            (unsigned)buildResult.usernameLengthField,
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            (unsigned)buildResult.keyConfigMd5Bytes.size(),
            (unsigned)buildResult.uiConfigMd5Bytes.size());
    }
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge) {
    // Address anchors:
    // - launcher.exe:0x429b0 = later challenge/material continuation anchor
    // - exact original raw 0x0a builder/send VA: [not yet isolated]
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authPassword_.empty()) {
        spdlog::error("launcher-owned auth received AS_AuthChallenge but has no password to send in AS_AuthChallengeResponse");
        return 0;
    }
    if (lastAuthRequestBuildResult_.twofishKeyBytes.size() != 16u) {
        spdlog::error("launcher-owned auth missing Twofish key from AS_AuthRequest build result");
        return 0;
    }

    mxo::auth::AuthChallengeResponseLayout layout;
    mxo::auth::AuthChallengeResponseBuildResult buildResult;
    if (!mxo::auth::BuildAuthChallengeResponsePacket(
            challenge.encryptedChallengeBytes,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            authPassword_,
            authPassword_,
            layout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        spdlog::error("launcher-owned auth failed to build AS_AuthChallengeResponse");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, "AS_AuthChallengeResponse");
    authChallengeResponseSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(buildResult);
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthChallengeResponse passwordLengthField={} soePasswordLengthField={} plaintextLen={} ciphertextLen={}",
            (unsigned)buildResult.passwordLengthField,
            (unsigned)buildResult.soePasswordLengthField,
            (unsigned)buildResult.plaintextBytes.size(),
            (unsigned)buildResult.ciphertextBytes.size());
    }
    return sendResult;
}

void CLTLoginMediator::LogParsedAuthReply(const mxo::auth::AuthReply& reply) const {
    // Address anchors:
    // - launcher.exe:0x4401a0 = strongest current HandleAuthReply implementation
    // - launcher.exe:0x43a330 = concrete auth-reply parser object helper
    // - launcher.exe:0x43b830 = later auth-side GetWorldList sender (upstream after success)
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (reply.isErrorReply) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply error errorCode=0x{:08x} zeroDword=0x{:08x} trailingWord=0x{:04x}",
            reply.errorCode,
            reply.zeroDword,
            reply.trailingWord);
        return;
    }

    spdlog::info(
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount={} worldCount={} username='{}' authDataMarker=0x{:04x} signatureLen={} encryptedPrivateExponentLen={}",
        reply.characterCount,
        reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text,
        reply.authDataMarker,
        reply.authSignatureBytes.size(),
        reply.encryptedPrivateExponentLength);

    for (size_t i = 0; i < reply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = reply.characters[i];
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth character[%u] handle='%s' characterId=%llu status=%u worldId=%u",
            (unsigned)i,
            entry.handle.text.empty() ? "<empty>" : entry.handle.text.c_str(),
            static_cast<unsigned long long>(entry.characterId),
            (unsigned)entry.status,
            (unsigned)entry.worldId);
    }

    for (size_t i = 0; i < reply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = reply.worlds[i];
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth world[%u] id=%u name='%s' status=%u type=%u clientVersion=%u load='%c'",
            (unsigned)i,
            (unsigned)world.worldId,
            world.worldName.empty() ? "<empty>" : world.worldName.c_str(),
            (unsigned)world.status,
            (unsigned)world.type,
            (unsigned)world.clientVersion,
            world.load ? static_cast<char>(world.load) : '?');
    }

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    if (mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            lastAuthChallenge_.encryptedChallengeBytes,
            &decryptedPrivateExponentBytes)) {
        spdlog::info(
            "DIAGNOSTIC: launcher-owned auth decrypted AS_AuthReply private exponent length=%u",
            (unsigned)decryptedPrivateExponentBytes.size());
    }
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig() {
    authBootstrap680_.loginType28 = authLoginType_;
    authBootstrap680_.launcherVersion2C = authLauncherVersion_;
    authBootstrap680_.block30 = CopyPrefix16(authKeyConfigMd5_);
    authBootstrap680_.block40 = CopyPrefix16(authUiConfigMd5_);
    authBootstrap680_.currentPublicKeyId9C = authCurrentPublicKeyId_;
}

void CLTLoginMediator::ResetRecoveredAuthBootstrapDynamicStateScaffold() {
    authBootstrap680_.timestamp80 = 0u;
    authBootstrap680_.sendTarget50 = nullptr;
    std::fill(authBootstrap680_.material85.begin(), authBootstrap680_.material85.end(), 0u);
    authBootstrap680_.sideObject94 = nullptr;
    authBootstrap680_.sideObject98 = nullptr;
    authBootstrap680_.helperA0 = nullptr;
    authBootstrap680_.lazyRaw06StateA4 = nullptr;
    authBootstrap680_.raw08AuxHandleA8 = nullptr;
    authBootstrap680_.fieldAC = nullptr;
    authBootstrap680_.fieldF0 = nullptr;
    authBootstrap680_.fieldF4 = nullptr;
    authBootstrap680_.fieldF8 = nullptr;
    authBootstrap680_.fieldFC = nullptr;
    authBootstrap680_.field100 = nullptr;
    authBootstrap680_.field108 = 0u;
    authBootstrap680_.field10C = 0u;
    authBootstrap680_.field110 = 0u;
    authBootstrap680_.field114 = 0u;
    authBootstrap680_.field118 = 0u;
    EraseRecoveredAuthBootstrapSidecar(this);
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
    const mxo::auth::GetPublicKeyReply& reply) {
    authBootstrap680_.currentPublicKeyId9C = reply.publicKeyId;
    RecoveredAuthBootstrapSidecarState& sidecar = MutableRecoveredAuthBootstrapSidecar(this);
    sidecar.raw08AuxHandleAvailabilityMarker = (reply.publicKeyId != 0u) ? reply.publicKeyId : 1u;
    authBootstrap680_.helperA0 = &sidecar.raw08AuxHandleAvailabilityMarker;
    authBootstrap680_.raw08AuxHandleA8 = &sidecar.raw08AuxHandleAvailabilityMarker;
    authBootstrap680_.fieldAC = &sidecar.raw08AuxHandleAvailabilityMarker;
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
    const mxo::auth::AuthChallengeResponseBuildResult& buildResult) {
    authBootstrap680_.material85 = CopyPrefix16(buildResult.decryptedChallengeBytes);
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(const mxo::auth::AuthReply& reply) {
    authBootstrap680_.fieldF4 = nullptr;

    RecoveredAuthBootstrapSidecarState* sidecar = FindRecoveredAuthBootstrapSidecar(this);
    if (sidecar) {
        sidecar->fieldF4Shadow = {};
    }

    if (reply.isErrorReply || !reply.valid || !reply.hasAuthDataMarker ||
        reply.authDataMarker != 0x0136u || authBootstrap680_.raw08AuxHandleA8 == nullptr) {
        return;
    }

    RecoveredAuthBootstrapSidecarState& materializedSidecar = MutableRecoveredAuthBootstrapSidecar(this);
    materializedSidecar.fieldF4Shadow.material85 = authBootstrap680_.material85;
    materializedSidecar.fieldF4Shadow.raw08AuxHandleA8 = authBootstrap680_.raw08AuxHandleA8;
    authBootstrap680_.fieldF4 = &materializedSidecar.fieldF4Shadow;

    spdlog::info(
        "CLTLoginMediator::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold material85='{}' raw08AuxHandle={} authDataMarker=0x{:04x}",
        BuildHexPreview(
            materializedSidecar.fieldF4Shadow.material85.data(),
            materializedSidecar.fieldF4Shadow.material85.size(),
            materializedSidecar.fieldF4Shadow.material85.size()),
        fmt::ptr(materializedSidecar.fieldF4Shadow.raw08AuxHandleA8),
        static_cast<unsigned>(reply.authDataMarker));
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

void CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState() {
    // Address anchors:
    // - launcher.exe:0x4401a0 / `0x43f300`
    // Rebuild the owner auth-reply tables now in active scope:
    // - `+0x688/+0x818/+0xd84` slot/world/route families
    // - `+0x80/+0xcc8` summary/current-index fields
    // This still does not reconstruct the separate later post-auth human-name / appearance writer
    // rooted at owner `+0x108`.
    worldSlots_.fill(nullptr);
    worldPayloadSlots_.fill(nullptr);
    slotRecordValid688_.fill(false);
    worldDescriptorValidD84_.fill(false);
    slotRecordCount684_ = 0;
    worldDescriptorCountD80_ = 0;
    for (RouteHostStringTripleState& routeString : routeHostStrings818_) {
        routeString.text.clear();
    }

    const size_t worldCount = std::min(worldSlots_.size(), lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
        worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
        SeedRecoveredWorldDescriptorFromAuthReply(static_cast<uint8_t>(i), lastAuthReply_.worlds[i]);
    }
    worldDescriptorCountD80_ = static_cast<uint8_t>(worldCount);

    const size_t characterCount = std::min(slotRecords688_.size(), lastAuthReply_.characters.size());
    for (size_t i = 0; i < characterCount; ++i) {
        SeedRecoveredCharacterSlotRecordFromAuthReply(static_cast<uint8_t>(i), lastAuthReply_.characters[i]);
        const SlotRecordState004b5328& slotRecord = slotRecords688_[i];
        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(slotRecord.worldId0c);
        if (matchedWorldIndex >= 0) {
            // Current source-owned tightening for the active state-8 margin path:
            // preserve the original descriptor-name join, but lowercase the copied text so the
            // reconstructed `+0x818` family can feed DNS host-prefix use directly (`Reality`
            // -> `reality`).
            routeHostStrings818_[i].text =
                LowercaseAsciiString(worldDescriptorsD84_[static_cast<size_t>(matchedWorldIndex)].inlineNamePlus03);
        }
    }
    slotRecordCount684_ = static_cast<uint8_t>(characterCount);

    // Writeback to owner +0x80 (world list count/status family)
    postAuthMarginLoadingState_.worldListCountOrStatus80 = static_cast<uint32_t>(lastAuthReply_.worlds.size());

    // Writeback to owner +0xcc8 (current character/route index byte)
    postAuthMarginLoadingState_.characterRouteIndexCc8 = 0;
    marginRouteState_.currentCharacterOrRouteIndex = 0;

    if (characterCount != 0) {
        const SlotRecordState004b5328& currentSlotRecord = slotRecords688_[0];
        marginRouteState_.pendingWorldId = currentSlotRecord.worldId0c;
        marginRouteState_.currentWorldId = static_cast<int32_t>(currentSlotRecord.worldId0c);
    } else if (worldCount != 0) {
        const mxo::auth::AuthWorldEntry& firstWorld = lastAuthReply_.worlds[0];
        marginRouteState_.pendingWorldId = firstWorld.worldId;
        marginRouteState_.currentWorldId = static_cast<int32_t>(firstWorld.worldId);
    }

    if (const char* routeHostPrefix = LookupRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8)) {
        marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        marginRouteState_.routeHostPrefix.clear();
    }

    SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();

    const char* currentDescriptorName = "<empty>";
    if (characterCount != 0) {
        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(slotRecords688_[0].worldId0c);
        if (matchedWorldIndex >= 0) {
            if (const char* name = GetDescriptorInlineNameByIndex(static_cast<uint8_t>(matchedWorldIndex))) {
                currentDescriptorName = name;
            }
        }
    } else if (worldCount != 0) {
        if (const char* name = GetDescriptorInlineNameByIndex(0)) {
            currentDescriptorName = name;
        }
    }

    spdlog::info(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount=%u characterCount=%u currentCharacterOrRouteIndex=%u currentSlotWorldId=%u routeHostPrefix='%s' slotRecordHeapString='%s' currentWorldDescriptorName='%s'",
        (unsigned)worldCount,
        (unsigned)characterCount,
        (unsigned)marginRouteState_.currentCharacterOrRouteIndex,
        characterCount == 0 ? 0u : (unsigned)slotRecords688_[0].worldId0c,
        marginRouteState_.routeHostPrefix.empty() ? "<empty>" : marginRouteState_.routeHostPrefix.c_str(),
        LookupSlotRecordHeapStringByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8)
            ? LookupSlotRecordHeapStringByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}

uint32_t CLTLoginMediator::HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    // Address anchors:
    // - launcher.exe:0x41bc20 = auth opcode read helper on later incoming path
    // - launcher.exe:0x4401a0 = strongest current AS_AuthReply handler
    // - launcher.exe:0x43a330 = concrete auth-reply parse/helper object builder
    // Receive-side note:
    // - the diagnostic auth bridge already strips the variable-length frame header before calling
    //   this mediator entry point
    // - so this function must operate on logical auth payload bytes beginning at raw opcode, not
    //   try to frame-parse them a second time
    if (!packetBytes || packetSize == 0u) {
        return 0;
    }

    const uint8_t rawCode = packetBytes[0];
    switch (rawCode) {
        case kAuthRawCodeGetPublicKeyReply: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPayload(packetBytes, packetSize, &reply)) {
                spdlog::info("DIAGNOSTIC: launcher-owned auth failed to parse AS_GetPublicKeyReply");
                return 0;
            }

            lastAuthPublicKeyReply_ = reply;
            authCurrentPublicKeyId_ = reply.publicKeyId;
            SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
            SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(reply);
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status={} currentTime={} publicKeyId={} keySize={} modulusLength={} signatureLength={} exponentByte=0x{:02x} hasEmbeddedPublicKey={}",
                (unsigned)reply.status,
                (unsigned)reply.currentTime,
                (unsigned)reply.publicKeyId,
                (unsigned)reply.keySize,
                (unsigned)reply.modulusLength,
                (unsigned)reply.signatureLength,
                (unsigned)reply.publicExponentByte,
                reply.hasEmbeddedPublicKey ? 1u : 0u);
            expectedAuthRequestName_ = kMessageAsAuthRequest;
            return SendAuthRequestFromReply(reply);
        }

        case 0x09: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::AuthChallenge challenge;
            if (!mxo::auth::ParseAuthChallengePayload(packetBytes, packetSize, &challenge)) {
                spdlog::info("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthChallenge");
                return 0;
            }

            lastAuthChallenge_ = challenge;
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen={}",
                challenge.encryptedChallengeBytes.size());
            expectedAuthRequestName_ = "AS_AuthChallengeResponse";
            const uint32_t sendResult = SendAuthChallengeResponse(challenge);
            const bool preserveExistingCharacterState8Path =
                currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u;
            if (sendResult != 0u && scaffoldState10_ != nullptr && !preserveExistingCharacterState8Path) {
                SwitchHelperStateScaffold(0x0au, scaffoldState10_);
            } else if (sendResult != 0u && preserveExistingCharacterState8Path) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes preserving current state8 through AS_AuthChallengeResponse for the existing-character path instead of forcing helperState=0x0a claim/create flow");
            }
            return sendResult;
        }

        case 0x0b: {
            // Address anchors:
            // - launcher.exe:0x4401a0 = state10 slot 6 / current best AS_AuthReply handler
            // - immediate post-success continuation there is not the later helper14
            //   `AS_GetWorldListRequest` sender at `0x43b830`
            // - instead it goes through the later state11 path:
            //   `0x41b450(0x0b)` -> `0x43c020` (raw post-auth margin packet `0x4d`) -> later
            //   `0x440320` (`MS_LoadCharacterReply`)
            stagedIncomingAuthPacketBytes_.assign(packetBytes, packetBytes + packetSize);
            if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes routing AS_AuthReply onto the existing-character state8 path; keeping currentState={} and skipping the later state10/state11 claim/create transition",
                    currentState_->DebugName());
                const uint32_t handled = HandleStagedAuthReplyPacketScaffold();
                if (handled != 0u) {
                    expectedMarginRequestName_ = "existing-character state8 raw-0x0f margin packet";
                }
                return handled;
            }
            if (currentState_ != nullptr) {
                return currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
            }
            return HandleStagedAuthReplyPacketScaffold();
        }

        default:
            spdlog::info(
                "DIAGNOSTIC: launcher-owned auth received unhandled packet rawCode=0x{:02x} message='{}' payloadLen={}",
                rawCode,
                mxo::auth::AuthOpcodeName(rawCode),
                packetSize);
            break;
    }

    return 0;
}

uint32_t CLTLoginMediator::HandleMarginPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
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
            "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} routing post-bootstrap packet to current helper slot6 dispatchCount={} currentState={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(effectivePacketSize),
            static_cast<unsigned>(marginPacketSlot6DispatchCountScaffold_),
            currentState_->DebugName());
        return currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
    }

    spdlog::debug(
        "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} has no active helper state; using direct state11/post-auth scaffold parser",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(effectivePacketSize));
    return HandleStagedMarginLoadCharacterReplyPacketScaffold();
}

uint32_t CLTLoginMediator::HandleStagedAuthReplyPacketScaffold() {
    if (stagedIncomingAuthPacketBytes_.empty()) {
        return 0u;
    }

    mxo::auth::AuthReply reply;
    if (!mxo::auth::ParseAuthReplyPayload(
            stagedIncomingAuthPacketBytes_.data(),
            stagedIncomingAuthPacketBytes_.size(),
            &reply)) {
        spdlog::warn("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthReply");
        return 0u;
    }

    lastAuthReply_ = reply;
    SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(reply);
    ResetMarginBootstrapState();
    MarginBootstrapSessionState& marginBootstrapState = MutableMarginBootstrapState(this);
    if (!mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            lastAuthChallenge_.encryptedChallengeBytes,
            &marginBootstrapState.authReplyPrivateExponentBytes)) {
        marginBootstrapState.authReplyPrivateExponentBytes.clear();
        spdlog::info("DIAGNOSTIC: launcher-owned auth could not recover private exponent bytes needed for later margin CERT bootstrap");
    }
    AdoptAuthReplyIntoRecoveredMediatorState();
    LogParsedAuthReply(reply);
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ = "CERT_ConnectRequest";
    return 1u;
}

uint32_t CLTLoginMediator::HandleStagedMarginLoadCharacterReplyPacketScaffold() {
    if (stagedIncomingMarginPacketBytes_.empty()) {
        return 0u;
    }
    return State11HandleLoadCharacterReplyScaffold(
        stagedIncomingMarginPacketBytes_.data(),
        stagedIncomingMarginPacketBytes_.size());
}

const std::vector<uint8_t>& CLTLoginMediator::StagedIncomingMarginPacketBytes() const {
    return stagedIncomingMarginPacketBytes_;
}

void CLTLoginMediator::BuildAuthEndpoint() {
    // Placeholder only.
    // Original launcher currently appears to preserve host text in owner `+0x4c` and then
    // build a sockaddr-like endpoint block at owner `+0x5c` using the current auth port.
    authEndpoint_ = BuildLoopbackEndpoint(authServerPortHostOrder_);
}

void CLTLoginMediator::BuildMarginEndpoint() {
    marginEndpoint_ = {};
    marginEndpoint_.family = 2;
    marginEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((marginServerPortHostOrder_ << 8) | (marginServerPortHostOrder_ >> 8));
    marginEndpoint_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
}

bool CLTLoginMediator::RebuildMarginAddressList() {
    const std::string resolvedHostName = ResolvedMarginHostName();
    marginAddressList3c_.resolvedHostName = resolvedHostName;
    marginAddressList3c_.ipv4NetworkOrderList.clear();
    marginAddressList3c_.nextIndex = 0;

    if (resolvedHostName.empty()) {
        spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold unresolved margin host");
        return false;
    }

    if (!ResolveAllIpv4Addresses(resolvedHostName.c_str(), &marginAddressList3c_.ipv4NetworkOrderList)) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnectionScaffold failed to resolve margin host '{}'",
            resolvedHostName);
        return false;
    }

    return true;
}

bool CLTLoginMediator::SelectMarginEndpointIpv4() {
    if (marginAddressList3c_.ipv4NetworkOrderList.empty()) {
        return false;
    }

    if (marginAddressList3c_.nextIndex >= marginAddressList3c_.ipv4NetworkOrderList.size()) {
        marginAddressList3c_.nextIndex = 0;
    }

    marginSelectedIpv4_7c_ = marginAddressList3c_.ipv4NetworkOrderList[marginAddressList3c_.nextIndex++];
    return true;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureAuthConnectionObject() {
    if (engine_ && authConnectionContextKey_) {
        authConnection_ = engine_->GetOrCreateMessageConnection(authConnectionContextKey_);
        authConnectionOwnedByMediator_ = false;
    } else if (!authConnection_) {
        authConnection_ = new mxo::liblttcp::CMessageConnection(engine_);
        authConnectionOwnedByMediator_ = true;
    }

    if (authConnection_) {
        // anchor: launcher.exe:0x448960 via `0x41d170`
        // Auth connection startup config writes `+0x78 = 1` and `+0x70 = FUN_0041ce00`.
        // Current source scaffold keeps the packet-name family / packetized-mode side of that
        // connection metadata explicit even though the callback body itself is still only used as
        // evidence for naming, not as a live function pointer.
        authConnection_->ConfigurePacketNameFamilyScaffold(
            mxo::liblttcp::CMessageConnectionPacketNameFamilyScaffold::kAuth,
            /*packetizedMessagesEnabled=*/true);
    }
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureMarginConnectionObject() {
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
    marginConnection->ConfigurePacketNameFamilyScaffold(
        mxo::liblttcp::CMessageConnectionPacketNameFamilyScaffold::kMargin,
        /*packetizedMessagesEnabled=*/true);
    if (marginConnectionContextKey_) {
        marginConnection->SetOwnerContext(marginConnectionContextKey_);
    }
    return marginConnection_;
}

// ILTLoginMediator::Default - static member initialization (original: launcher.exe:0x4d2c58)
ILTLoginMediator* ILTLoginMediator::Default = new mxo::ltlogin::CLTLoginMediator();

}  // namespace mxo::ltlogin
