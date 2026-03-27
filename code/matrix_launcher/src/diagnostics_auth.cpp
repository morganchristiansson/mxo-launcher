#include "diagnostics.h"
#include "diagnostics_auth.h"

#include "../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"
#include "loginmediator.h"
#include "loginstate.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
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

// Diagnostic owner/session wrapper for the installed launcher-owned `CLTLoginMediator`.
// Keep reset, auth/margin context lifetime, and active-state-source registration in one place so
// diagnostics code can drive the real mediator model instead of growing a second controller
// sidecar beside arg6.
struct DiagnosticLoginControllerSession {
    ~DiagnosticLoginControllerSession();

    void Reset();
    void BeginForOwner(void* owner);
    mxo::ltlogin::CLTLoginMediator* BindInstalledMediatorForEngine(
        mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
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

    DiagnosticRawMessageConnectionContext* authContext_ = NULL;
    DiagnosticRawMessageConnectionContext* marginContext_ = NULL;
    void* currentOwner_ = NULL;
    bool postAuthMarginBeginAttempted_ = false;
};

static DiagnosticLoginControllerSession g_DiagnosticLoginControllerSession = {};
static void* g_DiagnosticWorkItemVtable[2] = {0};
static void* g_DiagnosticMessageConnectionContextVtable[5] = {0};

static uint32_t __thiscall DiagnosticQueuedWorkItem_Release(DiagnosticQueuedWorkItemStub* self);
static uint32_t __thiscall DiagnosticRawMessageConnectionContext_Release(DiagnosticRawMessageConnectionContext* self);
static uint32_t __thiscall DiagnosticRawMessageConnectionContext_OnOperationCompleted(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem);

static mxo::ltlogin::CLTLoginMediator* ResolveInstalledLoginController() {
    return dynamic_cast<mxo::ltlogin::CLTLoginMediator*>(mxo::ltlogin::ILTLoginMediator::Default);
}

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

    if (mxo::ltlogin::CLTLoginMediator* controller = ResolveInstalledLoginController()) {
        controller->SetCurrentState(nullptr);
        controller->SetAuthConnectionContextKey(nullptr);
        controller->SetMarginConnectionContextKey(nullptr);
        controller->SetNetworkEngine(nullptr);
        mxo::ltlogin::CLTLoginMediator::UnregisterActiveStateSourceScaffold(controller);
    }

    currentOwner_ = NULL;
    postAuthMarginBeginAttempted_ = false;
}

void DiagnosticLoginControllerSession::BeginForOwner(void* owner) {
    currentOwner_ = owner;
    postAuthMarginBeginAttempted_ = false;
}

mxo::ltlogin::CLTLoginMediator* DiagnosticLoginControllerSession::BindInstalledMediatorForEngine(
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    if (!engine) {
        return NULL;
    }

    mxo::ltlogin::CLTLoginMediator* controller = ResolveInstalledLoginController();
    if (!controller) {
        return NULL;
    }

    controller->SetNetworkEngine(engine);
    controller->EnsureBuiltinScaffoldStatesRegistered();
    mxo::ltlogin::CLTLoginMediator::RegisterActiveStateSourceScaffold(controller);
    return controller;
}

mxo::ltlogin::CLTLoginMediator* DiagnosticLoginControllerSession::Controller() const {
    return ResolveInstalledLoginController();
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

static mxo::ltlogin::CLTLoginMediator* GetDiagnosticLoginController() {
    return GetDiagnosticLoginControllerSession().Controller();
}

static void ResetDiagnosticLoginControllerSession() {
    GetDiagnosticLoginControllerSession().Reset();
}

static void BeginDiagnosticLoginControllerSession(void* owner) {
    GetDiagnosticLoginControllerSession().BeginForOwner(owner);
}

static mxo::ltlogin::CLTLoginMediator* BindInstalledDiagnosticLoginControllerForEngine(
    mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    return GetDiagnosticLoginControllerSession().BindInstalledMediatorForEngine(engine);
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

}  // namespace

void DiagnosticAuthResetState() {
    ResetDiagnosticLoginControllerSession();
}

void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    BeginDiagnosticLoginControllerSession(owner);
    if (!engine) {
        return;
    }

    if (BindInstalledDiagnosticLoginControllerForEngine(engine)) {
        spdlog::info("DIAGNOSTIC: bound installed CLTLoginMediator to launcher object {}", fmt::ptr(owner));
    }
}

void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword) {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: auth credential configure skipped (no installed CLTLoginMediator)");
        return;
    }

    loginController->SetAuthCredentials(authName, authPassword);
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
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: login controller network configure skipped (no installed CLTLoginMediator)");
        return;
    }

    loginController->SetAuthServerConfig(
        authDnsName,
        authPortHostOrder,
        ignoreHostsFileForAuth);
    loginController->SetMarginServerConfig(
        marginDnsSuffix,
        marginPortHostOrder,
        ignoreHostsFileForMargin);
    loginController->SetMarginRouteHostPrefix(marginRouteHostPrefix);
    loginController->SetExactMarginHostName(exactMarginHostName);
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
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: login-controller character seed configure skipped (no installed CLTLoginMediator)");
        return;
    }

    const uint32_t normalizedWorldIndex = selectedWorldIndexLow24 & 0x00ffffffu;
    const uint32_t seedResult =
        loginController->MirrorCharacterSeedIntoSourceBlock120Scaffold(characterName, normalizedWorldIndex);
    if (gameSessionId && gameSessionId[0]) {
        loginController->SetGameSessionId664(gameSessionId);
    }

    spdlog::info(
        "DIAGNOSTIC: login-controller character seed configured character='{}' session='{}' selectedWorldIndexLow24=0x{:06x} mirrorResult=0x{:08x} (mirror-only source-block seed; original upstream producer still unresolved)",
        (characterName && characterName[0]) ? characterName : "<empty>",
        (gameSessionId && gameSessionId[0]) ? gameSessionId : "<empty>",
        static_cast<unsigned>(normalizedWorldIndex),
        static_cast<unsigned>(seedResult));
}

bool DiagnosticCanBeginAuthConnection() {
    return GetDiagnosticLoginController() != NULL && GetDiagnosticLoginControllerOwner() != NULL;
}

uint32_t DiagnosticBeginAuthConnection() {
    mxo::ltlogin::CLTLoginMediator* loginController = GetDiagnosticLoginController();
    if (!loginController) {
        spdlog::info("DIAGNOSTIC: installed CLTLoginMediator unavailable for auth connection");
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
        spdlog::info("DIAGNOSTIC: installed CLTLoginMediator unavailable for margin connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        GetOrCreateDiagnosticMarginContext("MarginConnection");
    if (context) {
        loginController->SetMarginConnectionContextKey(context);
    }

    const uint32_t result = loginController->BeginMarginConnectionViaState4Scaffold();
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
