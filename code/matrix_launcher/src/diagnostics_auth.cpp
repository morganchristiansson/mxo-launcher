#include "diagnostics.h"
#include "diagnostics_auth.h"

#include "../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"
#include "loginmediator.h"
#include "loginstate.h"
#include "launchpad.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

// UNANCHORED diagnostic-only queued work item.
// Its leading fields intentionally match the recovered queue work-item header shape:
// - vtable
// - workType at +0x04
struct DiagnosticQueuedWorkItemStub {
    mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader header;
    uint32_t workPayload;
    const char* debugLabel;
};

struct DiagnosticRawMessageConnectionContext {
    void** vtable;
    unsigned char autoReleaseFlag;
    unsigned char padding05[3];
    mxo::liblttcp::CMessageConnection* sidecarConnection;
    const char* debugLabel;
    void* contextKey;
    bool peerCloseQueued;
};

// Diagnostic-only owner/session wrapper for the launcher-owned login sidecar.
// Keep controller creation, reset, auth/margin context lifetime, and the current generic
// CLTLoginMediator active-state-source registration in one place so diagnostics code can query the
// active controller without directly owning raw new/delete.
struct DiagnosticLoginControllerSession {
    ~DiagnosticLoginControllerSession();

    void Reset();
    void BeginForOwner(void* owner);
    mxo::ltlogin::CLTLoginMediator* RecreateForEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
    mxo::ltlogin::CLTLoginMediator* Controller() const;
    void* CurrentOwner() const;
    DiagnosticRawMessageConnectionContext* AuthContext() const;
    DiagnosticRawMessageConnectionContext* MarginContext() const;
    bool MarkPostAuthMarginBeginAttempted();

    DiagnosticRawMessageConnectionContext* GetOrCreateAuthContext(const char* label);
    DiagnosticRawMessageConnectionContext* GetOrCreateMarginContext(const char* label);

private:
    DiagnosticRawMessageConnectionContext* GetOrCreateConnectionContext(
        DiagnosticRawMessageConnectionContext** slot,
        const char* label);

    std::unique_ptr<mxo::ltlogin::CLTLoginMediator> controller_;
    DiagnosticRawMessageConnectionContext* authContext_ = NULL;
    DiagnosticRawMessageConnectionContext* marginContext_ = NULL;
    void* currentOwner_ = NULL;
    bool postAuthMarginBeginAttempted_ = false;
};

struct DiagnosticLoginScaffoldStates {
    mxo::ltlogin::CLTLoginState_State0 state0 = {};
    mxo::ltlogin::CLTLoginState_State1 state1 = {};
    mxo::ltlogin::CLTLoginState_AuthenticatePending authenticatePending = {};
    mxo::ltlogin::CLTLoginState_State3 state3 = {};
    mxo::ltlogin::CLTLoginState_State4 state4 = {};
    mxo::ltlogin::CLTLoginState_State6 state6 = {};
    mxo::ltlogin::CLTLoginState_State8 state8 = {};
    mxo::ltlogin::CLTLoginState_State9 state9 = {};
    mxo::ltlogin::CLTLoginState_State10 state10 = {};
    mxo::ltlogin::CLTLoginState_State11 state11 = {};
    mxo::ltlogin::CLTLoginState_State12 state12 = {};
    mxo::ltlogin::CLTLoginState_State13 state13 = {};
    mxo::ltlogin::CLTLoginState_WorldListPending worldListPending = {};
    mxo::ltlogin::CLTLoginState_State15 state15 = {};
    mxo::ltlogin::CLTLoginState_State16 state16 = {};
    mxo::ltlogin::CLTLoginState_State17 state17 = {};
    mxo::ltlogin::CLTLoginState_State18 state18 = {};
    mxo::ltlogin::CLTLoginState_State19 state19 = {};
};

static DiagnosticLoginControllerSession g_DiagnosticLoginControllerSession = {};
static DiagnosticLoginScaffoldStates g_DiagnosticLoginScaffoldStates = {};
static unsigned char g_LoginControllerState9CallbackSeed85D4[16] = {0};
static void* g_DiagnosticWorkItemVtable[2] = {0};
static void* g_DiagnosticMessageConnectionContextVtable[5] = {0};

static char g_LoginControllerAuthDnsName[256] = "auth.lith.thematrixonline.net";
static uint16_t g_LoginControllerAuthPortHostOrder = 11000;
static bool g_LoginControllerIgnoreHostsFileForAuth = false;
static char g_LoginControllerMarginDnsSuffix[256] = ".lith.thematrixonline.net";
static uint16_t g_LoginControllerMarginPortHostOrder = 10000;
static bool g_LoginControllerIgnoreHostsFileForMargin = false;
static char g_LoginControllerMarginRouteHostPrefix[256] = {};
static char g_LoginControllerExactMarginHostName[256] = {};
static uint32_t g_LoginControllerSelectedWorldIndexLow24 = 0;
static char g_LoginControllerCharacterNameSeed[256] = {};
static char g_LoginControllerGameSessionIdSeed[256] = {};
static const char* g_LoginControllerAuthName = "resurrections";
static const char* g_LoginControllerAuthPassword = "";

static uint32_t __thiscall DiagnosticQueuedWorkItem_Release(DiagnosticQueuedWorkItemStub* self);
static uint32_t __thiscall DiagnosticRawMessageConnectionContext_Release(DiagnosticRawMessageConnectionContext* self);
static uint32_t __thiscall DiagnosticRawMessageConnectionContext_OnOperationCompleted(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem);

DiagnosticLoginControllerSession::~DiagnosticLoginControllerSession() {
    Reset();
}

void DiagnosticLoginControllerSession::Reset() {
    if (authContext_) {
        std::free(authContext_);
        authContext_ = NULL;
    }
    if (marginContext_) {
        std::free(marginContext_);
        marginContext_ = NULL;
    }
    mxo::ltlogin::CLTLoginMediator::UnregisterActiveStateSourceScaffold(controller_.get());
    controller_.reset();
    currentOwner_ = NULL;
    postAuthMarginBeginAttempted_ = false;
}

void DiagnosticLoginControllerSession::BeginForOwner(void* owner) {
    currentOwner_ = owner;
    postAuthMarginBeginAttempted_ = false;
}

mxo::ltlogin::CLTLoginMediator* DiagnosticLoginControllerSession::RecreateForEngine(
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    mxo::ltlogin::CLTLoginMediator::UnregisterActiveStateSourceScaffold(controller_.get());
    controller_.reset();
    if (!engine) {
        return NULL;
    }

    controller_ = std::unique_ptr<mxo::ltlogin::CLTLoginMediator>(new mxo::ltlogin::CLTLoginMediator());
    if (controller_) {
        controller_->SetNetworkEngine(engine);
        controller_->InitializeConnectionHelpers();
        mxo::ltlogin::CLTLoginMediator::RegisterActiveStateSourceScaffold(controller_.get());
    }
    return controller_.get();
}

mxo::ltlogin::CLTLoginMediator* DiagnosticLoginControllerSession::Controller() const {
    return controller_.get();
}

void* DiagnosticLoginControllerSession::CurrentOwner() const {
    return currentOwner_;
}

DiagnosticRawMessageConnectionContext* DiagnosticLoginControllerSession::AuthContext() const {
    return authContext_;
}

DiagnosticRawMessageConnectionContext* DiagnosticLoginControllerSession::MarginContext() const {
    return marginContext_;
}

bool DiagnosticLoginControllerSession::MarkPostAuthMarginBeginAttempted() {
    if (postAuthMarginBeginAttempted_) {
        return false;
    }

    postAuthMarginBeginAttempted_ = true;
    return true;
}

DiagnosticRawMessageConnectionContext* DiagnosticLoginControllerSession::GetOrCreateAuthContext(const char* label) {
    return GetOrCreateConnectionContext(&authContext_, label);
}

DiagnosticRawMessageConnectionContext* DiagnosticLoginControllerSession::GetOrCreateMarginContext(const char* label) {
    return GetOrCreateConnectionContext(&marginContext_, label);
}

DiagnosticRawMessageConnectionContext* DiagnosticLoginControllerSession::GetOrCreateConnectionContext(
    DiagnosticRawMessageConnectionContext** slot,
    const char* label) {
    if (!slot) return NULL;

    if (!g_DiagnosticWorkItemVtable[1]) {
        g_DiagnosticWorkItemVtable[1] = (void*)DiagnosticQueuedWorkItem_Release;
    }
    if (!g_DiagnosticMessageConnectionContextVtable[1]) {
        g_DiagnosticMessageConnectionContextVtable[1] = (void*)DiagnosticRawMessageConnectionContext_Release;
        g_DiagnosticMessageConnectionContextVtable[4] = (void*)DiagnosticRawMessageConnectionContext_OnOperationCompleted;
    }

    if (!*slot) {
        *slot = static_cast<DiagnosticRawMessageConnectionContext*>(std::calloc(1, sizeof(DiagnosticRawMessageConnectionContext)));
        if (!*slot) {
            spdlog::info("DIAGNOSTIC: failed to allocate raw message-connection context for '{}'", label ? label : "<null>");
            return NULL;
        }
        (*slot)->vtable = g_DiagnosticMessageConnectionContextVtable;
        (*slot)->autoReleaseFlag = 0;
        (*slot)->debugLabel = label;
        (*slot)->contextKey = *slot;
    }

    return *slot;
}

// Narrow helper accessors so the rest of diagnostics_auth.cpp does not reach directly into
// controller/session/state globals from every code path.
static DiagnosticLoginControllerSession& GetDiagnosticLoginControllerSession() {
    return g_DiagnosticLoginControllerSession;
}

static DiagnosticLoginScaffoldStates& GetDiagnosticLoginScaffoldStates() {
    return g_DiagnosticLoginScaffoldStates;
}

static mxo::ltlogin::CLTLoginMediator* GetDiagnosticLoginController() {
    return GetDiagnosticLoginControllerSession().Controller();
}

static void ResetDiagnosticLoginControllerSession() {
    GetDiagnosticLoginControllerSession().Reset();
}

static void BeginDiagnosticLoginControllerSession(void* owner) {
    GetDiagnosticLoginControllerSession().BeginForOwner(owner);
}

static mxo::ltlogin::CLTLoginMediator* RecreateDiagnosticLoginControllerForEngine(
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    return GetDiagnosticLoginControllerSession().RecreateForEngine(engine);
}

static DiagnosticRawMessageConnectionContext* GetDiagnosticAuthContext() {
    return GetDiagnosticLoginControllerSession().AuthContext();
}

static DiagnosticRawMessageConnectionContext* GetDiagnosticMarginContext() {
    return GetDiagnosticLoginControllerSession().MarginContext();
}

static DiagnosticRawMessageConnectionContext* GetOrCreateDiagnosticAuthContext(const char* label) {
    return GetDiagnosticLoginControllerSession().GetOrCreateAuthContext(label);
}

static DiagnosticRawMessageConnectionContext* GetOrCreateDiagnosticMarginContext(const char* label) {
    return GetDiagnosticLoginControllerSession().GetOrCreateMarginContext(label);
}

static void* GetDiagnosticLoginControllerOwner() {
    return GetDiagnosticLoginControllerSession().CurrentOwner();
}

static bool MarkDiagnosticPostAuthMarginBeginAttempted() {
    return GetDiagnosticLoginControllerSession().MarkPostAuthMarginBeginAttempted();
}

static uint32_t __thiscall DiagnosticQueuedWorkItem_Release(DiagnosticQueuedWorkItemStub* self) {
    if (self) {
        spdlog::info("DIAGNOSTIC: releasing queued work item {} type={} payload=0x{:08x} label='{}'",
            fmt::ptr(self),
            self->header.workType,
            self->workPayload,
            self->debugLabel ? self->debugLabel : "<null>");
        std::free(self);
    }
    return 1;
}

static uint32_t __thiscall DiagnosticRawMessageConnectionContext_Release(DiagnosticRawMessageConnectionContext* self) {
    spdlog::info(
        "DIAGNOSTIC: raw message-connection context release self={} label={}' autoRelease={}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        (self && self->autoReleaseFlag) ? 1u : 0u);
    return 1;
}

static const char* DiagnosticAuthRawCodeName(uint8_t rawCode) {
    switch (rawCode) {
        case mxo::ltlogin::CLTLoginMediator::kAuthRawCodeGetPublicKeyRequest:
            return mxo::ltlogin::CLTLoginMediator::kMessageAsGetPublicKeyRequest;
        case mxo::ltlogin::CLTLoginMediator::kAuthRawCodeGetPublicKeyReply:
            return mxo::ltlogin::CLTLoginMediator::kMessageAsGetPublicKeyReply;
        case mxo::ltlogin::CLTLoginMediator::kAuthRawCodeAuthRequest:
            return mxo::ltlogin::CLTLoginMediator::kMessageAsAuthRequest;
        case 0x09:
            return "AS_AuthChallenge";
        case 0x0a:
            return "AS_AuthChallengeResponse";
        case 0x0b:
            return mxo::ltlogin::CLTLoginMediator::kMessageAsAuthReply;
        case mxo::ltlogin::CLTLoginMediator::kAuthRawCodeGetWorldListRequest:
            return mxo::ltlogin::CLTLoginMediator::kMessageAsGetWorldListRequest;
        case 0x36:
            return "AS_GetWorldListReply";
        default:
            return "<unknown-auth-code>";
    }
}

static void DiagnosticCopyCStringIntoFixed(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::memset(dest, 0, destSize);
    if (!src || !src[0]) {
        return;
    }

    const size_t copyCount = std::min(destSize - 1u, std::strlen(src));
    std::memcpy(dest, src, copyCount);
    dest[copyCount] = '\0';
}

static bool DiagnosticParseVariableLengthPayload(
    const std::vector<uint8_t>& bytes,
    const uint8_t** outPayload,
    size_t* outPayloadSize,
    size_t* outHeaderBytes,
    size_t* outConsumedBytes) {
    if (outPayload) *outPayload = NULL;
    if (outPayloadSize) *outPayloadSize = 0u;
    if (outHeaderBytes) *outHeaderBytes = 0u;
    if (outConsumedBytes) *outConsumedBytes = 0u;
    if (bytes.size() < 2u) {
        return false;
    }

    uint32_t payloadLength = 0u;
    size_t headerBytes = 0u;
    if (bytes[0] & 0x80u) {
        if (bytes.size() < 3u) {
            return false;
        }
        payloadLength = (static_cast<uint32_t>(bytes[0] & 0x7fu) << 8) |
                        static_cast<uint32_t>(bytes[1]);
        headerBytes = 2u;
    } else {
        payloadLength = static_cast<uint32_t>(bytes[0]);
        headerBytes = 1u;
    }

    const size_t consumedBytes = headerBytes + payloadLength;
    if (bytes.size() < consumedBytes) {
        return false;
    }

    if (outPayload) *outPayload = bytes.data() + headerBytes;
    if (outPayloadSize) *outPayloadSize = payloadLength;
    if (outHeaderBytes) *outHeaderBytes = headerBytes;
    if (outConsumedBytes) *outConsumedBytes = consumedBytes;
    return true;
}

static void DiagnosticRouteConnectStatusToLoginController(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem) {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!self || !workItem || !loginController || workItem->header.workType != 2u) {
        return;
    }

    uint32_t handled = 0;
    const char* routeLabel = "<unknown>";
    if (self == GetDiagnosticAuthContext()) {
        handled = loginController->HandleAuthConnectStatus(workItem->workPayload);
        routeLabel = "auth";
    } else if (self == GetDiagnosticMarginContext()) {
        handled = loginController->HandleMarginConnectStatus(workItem->workPayload);
        routeLabel = "margin";
    } else {
        return;
    }

    const char* incomingReplyAnchor = "";
    if (self == GetDiagnosticAuthContext()) {
        incomingReplyAnchor = mxo::ltlogin::CLTLoginMediator::kMessageAsAuthReply;
    } else if (self == GetDiagnosticMarginContext()) {
        incomingReplyAnchor = mxo::ltlogin::CLTLoginMediator::kMessageMsLoadCharacterReply;
    }

    spdlog::info(
        "DIAGNOSTIC: routed {} type-2 connect-status payload=0x{:08x} into CLTLoginMediator scaffold -> handled={} laterIncomingReplyAnchor='{}'",
        routeLabel,
        static_cast<unsigned>(workItem->workPayload),
        static_cast<unsigned>(handled),
        (incomingReplyAnchor && incomingReplyAnchor[0]) ? incomingReplyAnchor : "<none>");
}

static uint32_t __thiscall DiagnosticRawMessageConnectionContext_OnOperationCompleted(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem) {
    spdlog::info(
        "DIAGNOSTIC: raw message-connection context OnOperationCompleted self={} label={}' workItem={} type={} payload=0x{:08x}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        fmt::ptr(workItem),
        workItem ? workItem->header.workType : 0u,
        workItem ? workItem->workPayload : 0u);

    if (self && self->sidecarConnection && workItem) {
        if (workItem->header.workType == 2u) {
            DiagnosticRouteConnectStatusToLoginController(self, workItem);
        }
        if (workItem->header.workType == 3u) {
            while (true) {
                const std::vector<uint8_t>& bytes = self->sidecarConnection->ReceivedBytes();
                if (bytes.empty()) {
                    break;
                }

                const size_t preview = (bytes.size() < 16u) ? bytes.size() : 16u;
                char hexPreview[16 * 3 + 1] = {0};
                char* out = hexPreview;
                for (size_t i = 0; i < preview; ++i) {
                    std::snprintf(out, 4, "%02x ", bytes[i]);
                    out += 3;
                }
                spdlog::info(
                    "DIAGNOSTIC: received-bytes label={}' total={} preview={}",
                    self->debugLabel ? self->debugLabel : "<null>",
                    (unsigned)bytes.size(),
                    hexPreview);

                if (self == GetDiagnosticAuthContext() && bytes.size() >= 2u) {
                    const uint8_t* payloadBytes = NULL;
                    size_t payloadSize = 0u;
                    size_t headerBytes = 0u;
                    size_t consumedBytes = 0u;
                    const bool parsedFrame = DiagnosticParseVariableLengthPayload(
                        bytes,
                        &payloadBytes,
                        &payloadSize,
                        &headerBytes,
                        &consumedBytes);
                    const uint8_t rawCode = (parsedFrame && payloadBytes && payloadSize != 0u) ? payloadBytes[0] : 0u;
                    if (!parsedFrame) {
                        spdlog::info(
                            "DIAGNOSTIC: auth receive buffering incomplete frame buffered={} preview={}",
                            bytes.size(),
                            hexPreview);
                        break;
                    }

                    spdlog::info(
                        "DIAGNOSTIC: auth receive framing payloadLength={} headerBytes={} rawCode=0x{:02x} likelyMessage='{}",
                        payloadSize,
                        headerBytes,
                        rawCode,
                        DiagnosticAuthRawCodeName(rawCode));

                    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
                    if (loginController) {
                        const uint32_t handled =
                            loginController->HandleAuthPacketBytes(payloadBytes, payloadSize);
                        spdlog::info(
                            "DIAGNOSTIC: launcher-owned auth packet handler label={} handled={} rawCode=0x{:02x}",
                            self->debugLabel ? self->debugLabel : "<null>",
                            handled,
                            rawCode);

                        if (handled != 0u && rawCode == 0x0b &&
                            MarkDiagnosticPostAuthMarginBeginAttempted()) {
                            const uint32_t marginConnectResult = DiagnosticBeginMarginConnection();
                            spdlog::info(
                                "DIAGNOSTIC: post-AS_AuthReply margin auto-begin result = 0x{:08x}",
                                marginConnectResult);
                        }
                    }

                    self->sidecarConnection->ConsumeReceivedBytesPrefix(consumedBytes);
                    continue;
                }

                if (self == GetDiagnosticMarginContext() && bytes.size() >= 2u) {
                    const uint8_t* payloadBytes = NULL;
                    size_t payloadSize = 0u;
                    size_t headerBytes = 0u;
                    size_t consumedBytes = 0u;
                    const bool parsedFrame = DiagnosticParseVariableLengthPayload(
                        bytes,
                        &payloadBytes,
                        &payloadSize,
                        &headerBytes,
                        &consumedBytes);
                    const uint8_t rawCode = (parsedFrame && payloadBytes && payloadSize != 0u) ? payloadBytes[0] : 0u;
                    if (!parsedFrame) {
                        spdlog::info(
                            "DIAGNOSTIC: margin receive buffering incomplete frame buffered={} preview={}",
                            bytes.size(),
                            hexPreview);
                        break;
                    }

                    const bool looksLikePlainBootstrapReply =
                        rawCode == 0x02u || rawCode == 0x04u || rawCode == 0x07u || rawCode == 0x09u;
                    spdlog::info(
                        "DIAGNOSTIC: margin receive framing payloadLength={} headerBytes={} outerByte0=0x{:02x} framingHint={} logicalOpcode=resolved-later-by-mediator-after-optional-decrypt",
                        payloadSize,
                        headerBytes,
                        rawCode,
                        looksLikePlainBootstrapReply ? "plaintext-bootstrap-reply" : "possibly-encrypted-post-bootstrap-payload");

                    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
                    if (loginController && payloadBytes && payloadSize != 0u) {
                        const uint32_t handled =
                            loginController->HandleMarginPacketBytes(payloadBytes, payloadSize);
                        spdlog::info(
                            "DIAGNOSTIC: launcher-owned margin packet handler label={} handled={} outerByte0=0x{:02x} decryptedOpcode=see-CLTLoginMediator-log-when-transportEncrypted=1",
                            self->debugLabel ? self->debugLabel : "<null>",
                            handled,
                            rawCode);
                    }

                    self->sidecarConnection->ConsumeReceivedBytesPrefix(consumedBytes);
                    continue;
                }

                break;
            }
        }
        return self->sidecarConnection->OnOperationCompleted(reinterpret_cast<void*>(workItem->header.workType));
    }
    return 1;
}

static bool DiagnosticEnqueueConnectionStatusWorkItem(
    void* owner,
    DiagnosticRawMessageConnectionContext* context,
    uint32_t workType,
    uint32_t workPayload,
    const char* label) {
    if (!owner || !context) return false;

    DiagnosticQueuedWorkItemStub* workItem = static_cast<DiagnosticQueuedWorkItemStub*>(std::calloc(1, sizeof(DiagnosticQueuedWorkItemStub)));
    if (!workItem) {
        spdlog::info("DIAGNOSTIC: failed to allocate queued work item for '{}'", label ? label : "<null>");
        return false;
    }

    workItem->header.vtable = g_DiagnosticWorkItemVtable;
    workItem->header.workType = workType;
    workItem->workPayload = workPayload;
    workItem->debugLabel = label;

    const bool pushed = DiagnosticAuthBridgePushQueue0C(
        owner,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(workItem)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));
    if (!pushed) {
        std::free(workItem);
        return false;
    }

    spdlog::info(
        "DIAGNOSTIC: queued connection-status work item label={}' workItem={} context={} type={} payload=0x{:08x}",
        label ? label : "<null>",
        fmt::ptr(workItem),
        fmt::ptr(context),
        workType,
        workPayload);
    return true;
}

static void DiagnosticApplyLoginControllerConfig() {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) return;

    DiagnosticLoginScaffoldStates& states = GetDiagnosticLoginScaffoldStates();

    const uint32_t launcherVersion = 76005u;
    const uint32_t currentPublicKeyId = 0u;
    const uint8_t loginType = 1u;
    const std::vector<uint8_t> keyConfigMd5;
    const std::vector<uint8_t> uiConfigMd5;

    loginController->SetAuthServerConfig(
        g_LoginControllerAuthDnsName,
        g_LoginControllerAuthPortHostOrder,
        g_LoginControllerIgnoreHostsFileForAuth);
    loginController->SetMarginServerConfig(
        g_LoginControllerMarginDnsSuffix,
        g_LoginControllerMarginPortHostOrder,
        g_LoginControllerIgnoreHostsFileForMargin);
    loginController->SetMarginRouteHostPrefix(g_LoginControllerMarginRouteHostPrefix);
    loginController->SetExactMarginHostName(g_LoginControllerExactMarginHostName);
    loginController->SetAuthCredentials(g_LoginControllerAuthName, g_LoginControllerAuthPassword);
    loginController->SetAuthBootstrapConfig(
        launcherVersion,
        currentPublicKeyId,
        static_cast<uint8_t>(loginType),
        keyConfigMd5,
        uiConfigMd5);
    loginController->RegisterScaffoldState0(&states.state0);
    loginController->RegisterScaffoldState1(&states.state1);
    loginController->RegisterScaffoldState2(&states.authenticatePending);
    loginController->RegisterScaffoldState3(&states.state3);
    loginController->RegisterScaffoldState4(&states.state4);
    loginController->RegisterScaffoldState6(&states.state6);
    loginController->RegisterScaffoldState8(&states.state8);
    loginController->RegisterScaffoldState9(&states.state9);
    loginController->RegisterScaffoldState10(&states.state10);
    loginController->RegisterScaffoldState11(&states.state11);
    loginController->RegisterScaffoldState12(&states.state12);
    loginController->RegisterScaffoldState13(&states.state13);
    loginController->RegisterScaffoldState14(&states.worldListPending);
    loginController->RegisterScaffoldState15(&states.state15);
    loginController->RegisterScaffoldState16(&states.state16);
    loginController->RegisterScaffoldState17(&states.state17);
    loginController->RegisterScaffoldState18(&states.state18);
    loginController->RegisterScaffoldState19(&states.state19);
    // Preserve the proven startup convention explicitly before any diagnostic auto-begin logic:
    // state0 is the initial idle/start helper, and owner-owned `ProcessLoginRequest` performs the
    // first happy-path handoff into state2 later.
    loginController->InstallInitialState0Scaffold();

    const char* characterNameSeed =
        g_LoginControllerCharacterNameSeed[0] ? g_LoginControllerCharacterNameSeed : NULL;
    if (characterNameSeed && characterNameSeed[0]) {
        // Diagnostic bridge only:
        // - `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the strongest concrete
        //   writer for the owner source block `+0x108/+0x12c/+0x134..+0x1b8`
        // - the sidecar continues to exercise that recovered writer without mutating the live
        //   launcher mediator outside the targeted wrapper-minimization scope
        mxo::ltlogin::ProcessLoginCredentialsInputSketch input = {};
        const auto characterState = loginController->DescribeOwnCharacterStateScaffold();
        DiagnosticCopyCStringIntoFixed(
            input.string00.data(),
            input.string00.size(),
            characterNameSeed);
        input.field24 = g_LoginControllerSelectedWorldIndexLow24;

        if (const char* realFirstName = characterState.realFirstName) {
            DiagnosticCopyCStringIntoFixed(input.string70.data(), input.string70.size(), realFirstName);
        }
        if (const char* realLastName = characterState.realLastName) {
            DiagnosticCopyCStringIntoFixed(input.string90.data(), input.string90.size(), realLastName);
        }
        if (const char* background = characterState.background) {
            DiagnosticCopyCStringIntoFixed(input.stringB0.data(), input.stringB0.size(), background);
        }

        loginController->ProcessLoginCredentials(input);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig applied recovered 0x41c3c0 character seed to diagnostic mediator characterName='{}' selectedWorldIndexLow24=0x{:06x} realFirst='{}' realLast='{}' background='{}'",
            characterNameSeed,
            static_cast<unsigned>(g_LoginControllerSelectedWorldIndexLow24),
            input.string70.data(),
            input.string90.data(),
            input.stringB0.data());
    }

    mxo::ltlogin::LaunchPadClient launchPad;
    const char* playRequestSessionId =
        g_LoginControllerGameSessionIdSeed[0] ? g_LoginControllerGameSessionIdSeed : NULL;
    if (playRequestSessionId) {
        launchPad.OnPlayRequestStatus(loginController, /*resultCode=*/0u, playRequestSessionId);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig routed diagnostic LaunchPadClient::OnPlayRequestStatus GameSessionID='{}'",
            playRequestSessionId);
    }
}

}  // namespace

void DiagnosticAuthResetState() {
    ResetDiagnosticLoginControllerSession();
}

void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    BeginDiagnosticLoginControllerSession(owner);
    if (!engine) {
        return;
    }

    if (RecreateDiagnosticLoginControllerForEngine(engine)) {
        DiagnosticApplyLoginControllerConfig();
        spdlog::info("DIAGNOSTIC: created CLTLoginMediator sidecar for launcher object {}", fmt::ptr(owner));
    }
}

void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword) {
    g_LoginControllerAuthName = (authName && authName[0]) ? authName : "";
    g_LoginControllerAuthPassword = (authPassword && authPassword[0]) ? authPassword : "";
    DiagnosticApplyLoginControllerConfig();
}

void DiagnosticAuthPollLiveConnectionTraffic(void* owner) {
    if (!owner) return;

    auto tryPoll = [owner](DiagnosticRawMessageConnectionContext* context, const char* label) {
        if (!context || !context->sidecarConnection) return;

        const int received = context->sidecarConnection->PollReceiveNonBlocking();
        if (received > 0) {
            DiagnosticEnqueueConnectionStatusWorkItem(
                owner,
                context,
                /*workType=*/3u,
                /*workPayload=*/static_cast<uint32_t>(received),
                label);
            return;
        }

        if (received < 0 && !context->peerCloseQueued) {
            context->peerCloseQueued = true;
            spdlog::info(
                "DiagnosticAuthPollLiveConnectionTraffic queued peer-close work label='{}' context={} connection={}",
                (context->debugLabel && context->debugLabel[0]) ? context->debugLabel : "<null>",
                fmt::ptr(context),
                fmt::ptr(context->sidecarConnection));
            DiagnosticEnqueueConnectionStatusWorkItem(
                owner,
                context,
                /*workType=*/1u,
                /*workPayload=*/0u,
                context == GetDiagnosticAuthContext() ? "AuthPeerClosed" : "MarginPeerClosed");
        }
    };

    tryPoll(GetDiagnosticAuthContext(), "AuthReceivePacket");
    tryPoll(GetDiagnosticMarginContext(), "MarginReceivePacket");
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
    std::strncpy(g_LoginControllerAuthDnsName, authDnsName ? authDnsName : "", sizeof(g_LoginControllerAuthDnsName) - 1);
    g_LoginControllerAuthDnsName[sizeof(g_LoginControllerAuthDnsName) - 1] = '\0';
    g_LoginControllerAuthPortHostOrder = authPortHostOrder;
    g_LoginControllerIgnoreHostsFileForAuth = ignoreHostsFileForAuth;

    std::strncpy(g_LoginControllerMarginDnsSuffix, marginDnsSuffix ? marginDnsSuffix : "", sizeof(g_LoginControllerMarginDnsSuffix) - 1);
    g_LoginControllerMarginDnsSuffix[sizeof(g_LoginControllerMarginDnsSuffix) - 1] = '\0';
    g_LoginControllerMarginPortHostOrder = marginPortHostOrder;
    g_LoginControllerIgnoreHostsFileForMargin = ignoreHostsFileForMargin;

    std::strncpy(g_LoginControllerMarginRouteHostPrefix, marginRouteHostPrefix ? marginRouteHostPrefix : "", sizeof(g_LoginControllerMarginRouteHostPrefix) - 1);
    g_LoginControllerMarginRouteHostPrefix[sizeof(g_LoginControllerMarginRouteHostPrefix) - 1] = '\0';
    std::strncpy(g_LoginControllerExactMarginHostName, exactMarginHostName ? exactMarginHostName : "", sizeof(g_LoginControllerExactMarginHostName) - 1);
    g_LoginControllerExactMarginHostName[sizeof(g_LoginControllerExactMarginHostName) - 1] = '\0';

    DiagnosticApplyLoginControllerConfig();
    spdlog::info(
        "DIAGNOSTIC: login controller network configured auth='{}' port={} marginSuffix='{}' marginPort={} marginRoutePrefix='{}' exactMarginHost='{}' ignoreAuthHosts={} ignoreMarginHosts={}",
        g_LoginControllerAuthDnsName,
        (unsigned)g_LoginControllerAuthPortHostOrder,
        g_LoginControllerMarginDnsSuffix,
        (unsigned)g_LoginControllerMarginPortHostOrder,
        g_LoginControllerMarginRouteHostPrefix[0] ? g_LoginControllerMarginRouteHostPrefix : "<empty>",
        g_LoginControllerExactMarginHostName[0] ? g_LoginControllerExactMarginHostName : "<empty>",
        g_LoginControllerIgnoreHostsFileForAuth ? 1u : 0u,
        g_LoginControllerIgnoreHostsFileForMargin ? 1u : 0u);
}

void DiagnosticMirrorSelectionContextIntoLoginController(const void* selectionContext, uint32_t byteCount) {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController || !selectionContext) {
        return;
    }
    if (byteCount < sizeof(mxo::ltlogin::State3SelectionContextInputSketch)) {
        spdlog::info(
            "DIAGNOSTIC: selection context mirror into login controller skipped byteCount=0x{:x} required=0x{:x}",
            byteCount,
            sizeof(mxo::ltlogin::State3SelectionContextInputSketch));
        return;
    }

    mxo::ltlogin::State3SelectionContextInputSketch input = {};
    std::memcpy(&input, selectionContext, sizeof(input));
    loginController->PersistSelectionContextForState8(input);
    spdlog::info(
        "DIAGNOSTIC: mirrored selection context into CLTLoginMediator sidecar slot=0x{:02x} firstBlock04=0x{:08x} lastBlockA4=0x{:08x}",
        input.slotOrSelectionIndex00 & 0xffu,
        input.block04[0],
        input.blockA4[3]);
}

void DiagnosticMirrorState9StartupTripleIntoLoginController(void* callback84, void* object88, void* object8c) {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        return;
    }

    loginController->SetState9CallbackObjectTriple84_88_8c(callback84, object88, object8c);
    spdlog::info(
        "DIAGNOSTIC: mirrored state9 startup triple into CLTLoginMediator sidecar callback84={} object88={} object8c={}",
        fmt::ptr(callback84),
        fmt::ptr(object88),
        fmt::ptr(object8c));
}

uint32_t DiagnosticFillState9CallbackBlob18c(void* outBuffer, uint32_t arg2, uint32_t arg3) {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController || !outBuffer) {
        return 1u;
    }
    return loginController->FillState9CallbackBlob18c(
        static_cast<uint32_t*>(outBuffer),
        arg2,
        arg3);
}

const void* DiagnosticGetState9CallbackSeedPointer85D4() {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        return NULL;
    }

    std::array<uint8_t, 16> seed = {};
    if (!loginController->CopyMarginBootstrapTwofishKeyScaffold(&seed)) {
        return NULL;
    }

    std::memcpy(g_LoginControllerState9CallbackSeed85D4, seed.data(), sizeof(g_LoginControllerState9CallbackSeed85D4));
    return g_LoginControllerState9CallbackSeed85D4;
}

void DiagnosticConfigureLoginControllerCharacterSeed(
    const char* characterName,
    const char* gameSessionId,
    uint32_t selectedWorldIndexLow24) {
    g_LoginControllerSelectedWorldIndexLow24 = selectedWorldIndexLow24 & 0x00ffffffu;
    std::strncpy(
        g_LoginControllerCharacterNameSeed,
        characterName ? characterName : "",
        sizeof(g_LoginControllerCharacterNameSeed) - 1);
    g_LoginControllerCharacterNameSeed[sizeof(g_LoginControllerCharacterNameSeed) - 1] = '\0';
    std::strncpy(
        g_LoginControllerGameSessionIdSeed,
        gameSessionId ? gameSessionId : "",
        sizeof(g_LoginControllerGameSessionIdSeed) - 1);
    g_LoginControllerGameSessionIdSeed[sizeof(g_LoginControllerGameSessionIdSeed) - 1] = '\0';
    DiagnosticApplyLoginControllerConfig();
    spdlog::info(
        "DIAGNOSTIC: login-controller character seed configured character='{}' session='{}' selectedWorldIndexLow24=0x{:06x} (bridge into confirmed 0x41c3c0 / 0x420ef0 writers; original upstream producer still unresolved)",
        g_LoginControllerCharacterNameSeed[0] ? g_LoginControllerCharacterNameSeed : "<empty>",
        g_LoginControllerGameSessionIdSeed[0] ? g_LoginControllerGameSessionIdSeed : "<empty>",
        static_cast<unsigned>(g_LoginControllerSelectedWorldIndexLow24));
}

bool DiagnosticCanBeginAuthConnection() {
    return GetDiagnosticLoginController() != NULL;
}

uint32_t DiagnosticBeginAuthConnection() {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for auth connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        GetOrCreateDiagnosticAuthContext("AuthConnection");
    if (context) {
        loginController->SetAuthConnectionContextKey(context);
    }

    spdlog::info(
        "ROUTE CHECKPOINT: diagnostics auto-begin auth from currentState={}",
        loginController->CurrentState() ? loginController->CurrentState()->DebugName() : "<null>");
    const uint32_t result =
        loginController->BeginAuthConnectionViaState1Scaffold();
    if (context) {
        context->sidecarConnection = loginController->AuthConnection();
    }

    void* currentOwner = GetDiagnosticLoginControllerOwner();
    DiagnosticAuthBridgeSyncOwnerState(currentOwner);

    if (result != 0u && context && currentOwner) {
        DiagnosticEnqueueConnectionStatusWorkItem(
            currentOwner,
            context,
            /*workType=*/2u,
            /*workPayload=*/0x7000001u,
            "AuthConnectStatus");
    }

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::BeginAuthConnectionViaState1Scaffold() -> 0x{:08x}",
        static_cast<unsigned>(result));
    return result;
}

uint32_t DiagnosticBeginMarginConnection() {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for margin connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        GetOrCreateDiagnosticMarginContext("MarginConnection");
    if (context) {
        loginController->SetMarginConnectionContextKey(context);
    }

    mxo::ltlogin::CLTLoginState* state4 = loginController->ScaffoldState4();
    if (!state4) {
        spdlog::info("DIAGNOSTIC: CLTLoginMediator sidecar missing registered state4 scaffold for margin connection");
        return 0;
    }

    const uint32_t result = state4->Slot3_BeginOrContinue(
        loginController->CurrentState(),
        loginController);
    const std::string marginHost = loginController->ResolvedMarginHostName();
    if (context) {
        context->sidecarConnection = loginController->MarginConnection();
    }

    void* currentOwner = GetDiagnosticLoginControllerOwner();
    DiagnosticAuthBridgeSyncOwnerState(currentOwner);

    if (result != 0u && context && currentOwner) {
        DiagnosticEnqueueConnectionStatusWorkItem(
            currentOwner,
            context,
            /*workType=*/2u,
            /*workPayload=*/0x7000001u,
            "MarginConnectStatus");
    }

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::ScaffoldState4()->Slot3_BeginOrContinue() marginHost='{}' -> 0x{:08x}",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}
