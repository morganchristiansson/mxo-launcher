#include "diagnostics.h"
#include "diagnostics_auth.h"

#include "liblttcp/cmessageconnection.h"
#include "liblttcp/ltthreadperclienttcpengine.h"
#include "ltlogin/loginmediator.h"
#include "ltlogin/loginstates.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct DiagnosticQueuedWorkItemStub {
    void** vtable;
    uint32_t workType;
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
};

static mxo::ltlogin::CLTLoginMediator* g_DiagnosticLoginController = NULL;
static mxo::ltlogin::CLTLoginState_AuthenticatePending g_DiagnosticLoginStateAuthenticatePending = {};
static mxo::ltlogin::CLTLoginState_WorldListPending g_DiagnosticLoginStateWorldListPending = {};
static DiagnosticRawMessageConnectionContext* g_DiagnosticAuthContext = NULL;
static DiagnosticRawMessageConnectionContext* g_DiagnosticMarginContext = NULL;
static void* g_DiagnosticWorkItemVtable[2] = {0};
static void* g_DiagnosticMessageConnectionContextVtable[5] = {0};
static void* g_DiagnosticCurrentOwner = NULL;

static char g_LoginControllerAuthDnsName[256] = "auth.lith.thematrixonline.net";
static uint16_t g_LoginControllerAuthPortHostOrder = 11000;
static bool g_LoginControllerIgnoreHostsFileForAuth = false;
static char g_LoginControllerMarginDnsSuffix[256] = ".lith.thematrixonline.net";
static uint16_t g_LoginControllerMarginPortHostOrder = 10000;
static bool g_LoginControllerIgnoreHostsFileForMargin = false;
static char g_LoginControllerMarginRouteHostPrefix[256] = {};
static char g_LoginControllerExactMarginHostName[256] = {};
static const char* g_LoginControllerAuthName = "resurrections";
static const char* g_LoginControllerAuthPassword = "";

static uint32_t __thiscall DiagnosticQueuedWorkItem_Release(DiagnosticQueuedWorkItemStub* self) {
    if (self) {
        Log(
            "DIAGNOSTIC: releasing queued work item %p type=%u payload=0x%08x label='%s'",
            self,
            (unsigned)self->workType,
            (unsigned)self->workPayload,
            self->debugLabel ? self->debugLabel : "<null>");
        std::free(self);
    }
    return 1;
}

static uint32_t __thiscall DiagnosticRawMessageConnectionContext_Release(DiagnosticRawMessageConnectionContext* self) {
    Log(
        "DIAGNOSTIC: raw message-connection context release self=%p label='%s' autoRelease=%u",
        self,
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

static void DiagnosticRouteConnectStatusToLoginController(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem) {
    if (!self || !workItem || !g_DiagnosticLoginController || workItem->workType != 2u) {
        return;
    }

    uint32_t handled = 0;
    const char* routeLabel = "<unknown>";
    if (self == g_DiagnosticAuthContext) {
        handled = g_DiagnosticLoginController->HandleAuthConnectStatus(workItem->workPayload);
        routeLabel = "auth";
    } else if (self == g_DiagnosticMarginContext) {
        handled = g_DiagnosticLoginController->HandleMarginConnectStatus(workItem->workPayload);
        routeLabel = "margin";
    } else {
        return;
    }

    const char* expectedNextRequest = "";
    const char* incomingReplyAnchor = "";
    if (self == g_DiagnosticAuthContext) {
        expectedNextRequest = g_DiagnosticLoginController->ExpectedAuthRequestName();
        incomingReplyAnchor = mxo::ltlogin::CLTLoginMediator::kMessageAsAuthReply;
    } else if (self == g_DiagnosticMarginContext) {
        expectedNextRequest = g_DiagnosticLoginController->ExpectedMarginRequestName();
        incomingReplyAnchor = mxo::ltlogin::CLTLoginMediator::kMessageMsLoadCharacterReply;
    }

    Log(
        "DIAGNOSTIC: routed %s type-2 connect-status payload=0x%08x into CLTLoginMediator scaffold -> handled=%u nextOutboundRequest='%s' laterIncomingReplyAnchor='%s'",
        routeLabel,
        (unsigned)workItem->workPayload,
        (unsigned)handled,
        (expectedNextRequest && expectedNextRequest[0]) ? expectedNextRequest : "<unresolved>",
        (incomingReplyAnchor && incomingReplyAnchor[0]) ? incomingReplyAnchor : "<none>");
}

static uint32_t __thiscall DiagnosticRawMessageConnectionContext_OnOperationCompleted(
    DiagnosticRawMessageConnectionContext* self,
    DiagnosticQueuedWorkItemStub* workItem) {
    Log(
        "DIAGNOSTIC: raw message-connection context OnOperationCompleted self=%p label='%s' workItem=%p type=%u payload=0x%08x",
        self,
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        workItem,
        workItem ? (unsigned)workItem->workType : 0u,
        workItem ? (unsigned)workItem->workPayload : 0u);

    if (self && self->sidecarConnection && workItem) {
        if (workItem->workType == 2u) {
            DiagnosticRouteConnectStatusToLoginController(self, workItem);
        }
        if (workItem->workType == 3u) {
            const std::vector<uint8_t>& bytes = self->sidecarConnection->ReceivedBytes();
            if (!bytes.empty()) {
                const size_t preview = (bytes.size() < 16u) ? bytes.size() : 16u;
                char hexPreview[16 * 3 + 1] = {0};
                char* out = hexPreview;
                for (size_t i = 0; i < preview; ++i) {
                    std::snprintf(out, 4, "%02x ", bytes[i]);
                    out += 3;
                }
                Log(
                    "DIAGNOSTIC: received-bytes label='%s' total=%u preview=%s",
                    self->debugLabel ? self->debugLabel : "<null>",
                    (unsigned)bytes.size(),
                    hexPreview);

                if (self == g_DiagnosticAuthContext && bytes.size() >= 2u) {
                    uint32_t payloadLength = 0;
                    size_t headerBytes = 0;
                    uint8_t rawCode = 0;
                    if (bytes[0] & 0x80u) {
                        if (bytes.size() >= 3u) {
                            payloadLength = (static_cast<uint32_t>(bytes[0] & 0x7fu) << 8) |
                                            static_cast<uint32_t>(bytes[1]);
                            headerBytes = 2u;
                            rawCode = bytes[2];
                        }
                    } else {
                        payloadLength = static_cast<uint32_t>(bytes[0]);
                        headerBytes = 1u;
                        rawCode = bytes[1];
                    }

                    if (headerBytes != 0u) {
                        Log(
                            "DIAGNOSTIC: auth receive framing payloadLength=%u headerBytes=%u rawCode=0x%02x likelyMessage='%s'",
                            (unsigned)payloadLength,
                            (unsigned)headerBytes,
                            (unsigned)rawCode,
                            DiagnosticAuthRawCodeName(rawCode));
                    }

                    if (g_DiagnosticLoginController) {
                        const uint32_t handled =
                            g_DiagnosticLoginController->HandleAuthPacketBytes(bytes.data(), bytes.size());
                        Log(
                            "DIAGNOSTIC: launcher-owned auth packet handler label='%s' handled=%u rawCode=0x%02x",
                            self->debugLabel ? self->debugLabel : "<null>",
                            (unsigned)handled,
                            (unsigned)rawCode);
                    }
                }

                self->sidecarConnection->ClearReceivedBytes();
            }
        }
        return self->sidecarConnection->OnOperationCompleted(workItem->workType);
    }
    return 1;
}

static DiagnosticRawMessageConnectionContext* DiagnosticGetOrCreateRawConnectionContext(
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
            Log("DIAGNOSTIC: failed to allocate raw message-connection context for '%s'", label ? label : "<null>");
            return NULL;
        }
        (*slot)->vtable = g_DiagnosticMessageConnectionContextVtable;
        (*slot)->autoReleaseFlag = 0;
        (*slot)->debugLabel = label;
        (*slot)->contextKey = *slot;
    }

    return *slot;
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
        Log("DIAGNOSTIC: failed to allocate queued work item for '%s'", label ? label : "<null>");
        return false;
    }

    workItem->vtable = g_DiagnosticWorkItemVtable;
    workItem->workType = workType;
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

    Log(
        "DIAGNOSTIC: queued connection-status work item label='%s' workItem=%p context=%p type=%u payload=0x%08x",
        label ? label : "<null>",
        workItem,
        context,
        (unsigned)workType,
        (unsigned)workPayload);
    return true;
}

static void DiagnosticApplyLoginControllerConfig() {
    if (!g_DiagnosticLoginController) return;

    const uint32_t launcherVersion = 76005u;
    const uint32_t currentPublicKeyId = 0u;
    const uint8_t loginType = 1u;
    const std::vector<uint8_t> keyConfigMd5;
    const std::vector<uint8_t> uiConfigMd5;

    g_DiagnosticLoginController->SetAuthServerConfig(
        g_LoginControllerAuthDnsName,
        g_LoginControllerAuthPortHostOrder,
        g_LoginControllerIgnoreHostsFileForAuth);
    g_DiagnosticLoginController->SetMarginServerConfig(
        g_LoginControllerMarginDnsSuffix,
        g_LoginControllerMarginPortHostOrder,
        g_LoginControllerIgnoreHostsFileForMargin);
    g_DiagnosticLoginController->SetMarginRouteHostPrefix(g_LoginControllerMarginRouteHostPrefix);
    g_DiagnosticLoginController->SetExactMarginHostName(g_LoginControllerExactMarginHostName);
    g_DiagnosticLoginController->SetAuthCredentials(g_LoginControllerAuthName, g_LoginControllerAuthPassword);
    g_DiagnosticLoginController->SetAuthBootstrapConfig(
        launcherVersion,
        currentPublicKeyId,
        static_cast<uint8_t>(loginType),
        keyConfigMd5,
        uiConfigMd5);
    g_DiagnosticLoginController->SetCurrentState(&g_DiagnosticLoginStateAuthenticatePending);
}

}  // namespace

void DiagnosticAuthResetState() {
    if (g_DiagnosticAuthContext) {
        std::free(g_DiagnosticAuthContext);
        g_DiagnosticAuthContext = NULL;
    }
    if (g_DiagnosticMarginContext) {
        std::free(g_DiagnosticMarginContext);
        g_DiagnosticMarginContext = NULL;
    }
    delete g_DiagnosticLoginController;
    g_DiagnosticLoginController = NULL;
    g_DiagnosticCurrentOwner = NULL;
}

void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    g_DiagnosticCurrentOwner = owner;
    if (!engine) {
        return;
    }
    delete g_DiagnosticLoginController;
    g_DiagnosticLoginController = new mxo::ltlogin::CLTLoginMediator();
    if (g_DiagnosticLoginController) {
        g_DiagnosticLoginController->SetNetworkEngine(engine);
        g_DiagnosticLoginController->InitializeConnectionHelpers();
        DiagnosticApplyLoginControllerConfig();
        Log("DIAGNOSTIC: created CLTLoginMediator sidecar for launcher object %p", owner);
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
        if (received <= 0) return;

        DiagnosticEnqueueConnectionStatusWorkItem(
            owner,
            context,
            /*workType=*/3u,
            /*workPayload=*/static_cast<uint32_t>(received),
            label);
    };

    tryPoll(g_DiagnosticAuthContext, "AuthReceivePacket");
    tryPoll(g_DiagnosticMarginContext, "MarginReceivePacket");
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
    Log(
        "DIAGNOSTIC: login controller network configured auth='%s':%u marginSuffix='%s' marginPort=%u marginRoutePrefix='%s' exactMarginHost='%s' ignoreAuthHosts=%u ignoreMarginHosts=%u",
        g_LoginControllerAuthDnsName,
        (unsigned)g_LoginControllerAuthPortHostOrder,
        g_LoginControllerMarginDnsSuffix,
        (unsigned)g_LoginControllerMarginPortHostOrder,
        g_LoginControllerMarginRouteHostPrefix[0] ? g_LoginControllerMarginRouteHostPrefix : "<empty>",
        g_LoginControllerExactMarginHostName[0] ? g_LoginControllerExactMarginHostName : "<empty>",
        g_LoginControllerIgnoreHostsFileForAuth ? 1u : 0u,
        g_LoginControllerIgnoreHostsFileForMargin ? 1u : 0u);
}

bool DiagnosticCanBeginAuthConnection() {
    return g_DiagnosticLoginController != NULL;
}

uint32_t DiagnosticBeginAuthConnection() {
    if (!g_DiagnosticLoginController) {
        Log("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for auth connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        DiagnosticGetOrCreateRawConnectionContext(&g_DiagnosticAuthContext, "AuthConnection");
    if (context) {
        g_DiagnosticLoginController->SetAuthConnectionContextKey(context);
    }

    const uint32_t result = g_DiagnosticLoginController->BeginAuthConnection();
    if (context) {
        context->sidecarConnection = g_DiagnosticLoginController->AuthConnection();
    }
    DiagnosticAuthBridgeSyncOwnerState(g_DiagnosticCurrentOwner);

    if (result != 0u && context && g_DiagnosticCurrentOwner) {
        DiagnosticEnqueueConnectionStatusWorkItem(
            g_DiagnosticCurrentOwner,
            context,
            /*workType=*/2u,
            /*workPayload=*/0x7000001u,
            "AuthConnectStatus");
    }

    Log(
        "DIAGNOSTIC: CLTLoginMediator::BeginAuthConnection() authHost='%s' port=%u -> 0x%08x",
        g_DiagnosticLoginController->AuthServerDnsName().c_str(),
        (unsigned)g_DiagnosticLoginController->AuthServerPortHostOrder(),
        (unsigned)result);
    return result;
}

uint32_t DiagnosticBeginMarginConnection() {
    if (!g_DiagnosticLoginController) {
        Log("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for margin connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        DiagnosticGetOrCreateRawConnectionContext(&g_DiagnosticMarginContext, "MarginConnection");
    if (context) {
        g_DiagnosticLoginController->SetMarginConnectionContextKey(context);
    }

    const std::string marginHost = g_DiagnosticLoginController->ResolvedMarginHostName();
    const uint32_t result = g_DiagnosticLoginController->DispatchMarginConnectionByState();
    if (context) {
        context->sidecarConnection = g_DiagnosticLoginController->MarginConnection();
    }
    DiagnosticAuthBridgeSyncOwnerState(g_DiagnosticCurrentOwner);

    if (result != 0u && context && g_DiagnosticCurrentOwner) {
        DiagnosticEnqueueConnectionStatusWorkItem(
            g_DiagnosticCurrentOwner,
            context,
            /*workType=*/2u,
            /*workPayload=*/0x7000001u,
            "MarginConnectStatus");
    }

    Log(
        "DIAGNOSTIC: CLTLoginMediator::DispatchMarginConnectionByState() marginHost='%s' port=%u -> 0x%08x",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        (unsigned)g_DiagnosticLoginController->MarginServerPortHostOrder(),
        (unsigned)result);
    return result;
}
