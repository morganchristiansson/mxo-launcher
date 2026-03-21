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
#include <spdlog/spdlog.h>
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

static mxo::ltlogin::CLTLoginMediator* g_DiagnosticLoginController = NULL;
static unsigned char g_LoginControllerState9CallbackSeed85D4[16] = {0};
static mxo::ltlogin::CLTLoginState_AuthenticatePending g_DiagnosticLoginStateAuthenticatePending = {};
static mxo::ltlogin::CLTLoginState_State3 g_DiagnosticLoginStateState3 = {};
static mxo::ltlogin::CLTLoginState_State4 g_DiagnosticLoginStateState4 = {};
static mxo::ltlogin::CLTLoginState_State6 g_DiagnosticLoginStateState6 = {};
static mxo::ltlogin::CLTLoginState_State8 g_DiagnosticLoginStateState8 = {};
static mxo::ltlogin::CLTLoginState_State9 g_DiagnosticLoginStateState9 = {};
static mxo::ltlogin::CLTLoginState_State10 g_DiagnosticLoginStateState10 = {};
static mxo::ltlogin::CLTLoginState_State11 g_DiagnosticLoginStateState11 = {};
static mxo::ltlogin::CLTLoginState_State12 g_DiagnosticLoginStateState12 = {};
static mxo::ltlogin::CLTLoginState_State13 g_DiagnosticLoginStateState13 = {};
static mxo::ltlogin::CLTLoginState_WorldListPending g_DiagnosticLoginStateWorldListPending = {};
static DiagnosticRawMessageConnectionContext* g_DiagnosticAuthContext = NULL;
static DiagnosticRawMessageConnectionContext* g_DiagnosticMarginContext = NULL;
static bool g_DiagnosticPostAuthMarginBeginAttempted = false;
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
static uint32_t g_LoginControllerSelectedWorldIndexLow24 = 0;
static char g_LoginControllerCharacterNameSeed[256] = {};
static char g_LoginControllerGameSessionIdSeed[256] = {};
static const char* g_LoginControllerAuthName = "resurrections";
static const char* g_LoginControllerAuthPassword = "";

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
    if (!self || !workItem || !g_DiagnosticLoginController || workItem->header.workType != 2u) {
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

    const char* incomingReplyAnchor = "";
    if (self == g_DiagnosticAuthContext) {
        incomingReplyAnchor = mxo::ltlogin::CLTLoginMediator::kMessageAsAuthReply;
    } else if (self == g_DiagnosticMarginContext) {
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

                if (self == g_DiagnosticAuthContext && bytes.size() >= 2u) {
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

                    if (g_DiagnosticLoginController) {
                        const uint32_t handled =
                            g_DiagnosticLoginController->HandleAuthPacketBytes(payloadBytes, payloadSize);
                        spdlog::info(
                            "DIAGNOSTIC: launcher-owned auth packet handler label={} handled={} rawCode=0x{:02x}",
                            self->debugLabel ? self->debugLabel : "<null>",
                            handled,
                            rawCode);

                        if (handled != 0u && rawCode == 0x0b && !g_DiagnosticPostAuthMarginBeginAttempted) {
                            g_DiagnosticPostAuthMarginBeginAttempted = true;
                            const uint32_t marginConnectResult = DiagnosticBeginMarginConnection();
                            spdlog::info(
                                "DIAGNOSTIC: post-AS_AuthReply margin auto-begin result = 0x{:08x}",
                                marginConnectResult);
                        }
                    }

                    self->sidecarConnection->ConsumeReceivedBytesPrefix(consumedBytes);
                    continue;
                }

                if (self == g_DiagnosticMarginContext && bytes.size() >= 2u) {
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

                    if (g_DiagnosticLoginController && payloadBytes && payloadSize != 0u) {
                        const uint32_t handled =
                            g_DiagnosticLoginController->HandleMarginPacketBytes(payloadBytes, payloadSize);
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
    g_DiagnosticLoginController->RegisterScaffoldState3(&g_DiagnosticLoginStateState3);
    g_DiagnosticLoginController->RegisterScaffoldState4(&g_DiagnosticLoginStateState4);
    g_DiagnosticLoginController->RegisterScaffoldState6(&g_DiagnosticLoginStateState6);
    g_DiagnosticLoginController->RegisterScaffoldState8(&g_DiagnosticLoginStateState8);
    g_DiagnosticLoginController->RegisterScaffoldState9(&g_DiagnosticLoginStateState9);
    g_DiagnosticLoginController->RegisterScaffoldState10(&g_DiagnosticLoginStateState10);
    g_DiagnosticLoginController->RegisterScaffoldState11(&g_DiagnosticLoginStateState11);
    g_DiagnosticLoginController->RegisterScaffoldState12(&g_DiagnosticLoginStateState12);
    g_DiagnosticLoginController->RegisterScaffoldState13(&g_DiagnosticLoginStateState13);
    g_DiagnosticLoginController->SetCurrentState(&g_DiagnosticLoginStateAuthenticatePending);

    const char* characterNameSeed =
        g_LoginControllerCharacterNameSeed[0] ? g_LoginControllerCharacterNameSeed : NULL;
    if (characterNameSeed && characterNameSeed[0]) {
        // Diagnostic bridge only:
        // - `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the strongest concrete
        //   writer for the owner source block `+0x108/+0x12c/+0x134..+0x1b8`
        // - the exact original upstream producer feeding that blob is still unresolved
        // - the replacement launcher therefore lets explicit launcher seed inputs exercise the
        //   confirmed writer without pretending we already recovered the original producer
        mxo::ltlogin::CLTLoginMediator::ProcessLoginCredentialsInputSketch input = {};
        DiagnosticCopyCStringIntoFixed(
            input.string00.data(),
            input.string00.size(),
            characterNameSeed);
        input.field24 = g_LoginControllerSelectedWorldIndexLow24;

        g_DiagnosticLoginController->ProcessLoginCredentials(input);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig applied recovered 0x41c3c0 character seed characterName='{}' selectedWorldIndexLow24=0x{:06x}",
            characterNameSeed,
            static_cast<unsigned>(g_LoginControllerSelectedWorldIndexLow24));
    }

    mxo::ltlogin::LaunchPadClient launchPad;
    const char* playRequestSessionId =
        g_LoginControllerGameSessionIdSeed[0] ? g_LoginControllerGameSessionIdSeed : NULL;
    if (playRequestSessionId) {
        launchPad.OnPlayRequestStatus(g_DiagnosticLoginController, /*resultCode=*/0u, playRequestSessionId);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig routed launcher LaunchPadClient::OnPlayRequestStatus GameSessionID='{}'",
            playRequestSessionId);
    }
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
    g_DiagnosticPostAuthMarginBeginAttempted = false;
}

void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    g_DiagnosticCurrentOwner = owner;
    g_DiagnosticPostAuthMarginBeginAttempted = false;
    if (!engine) {
        return;
    }
    delete g_DiagnosticLoginController;
    g_DiagnosticLoginController = new mxo::ltlogin::CLTLoginMediator();
    if (g_DiagnosticLoginController) {
        g_DiagnosticLoginController->SetNetworkEngine(engine);
        g_DiagnosticLoginController->InitializeConnectionHelpers();
        DiagnosticApplyLoginControllerConfig();
        spdlog::info("DIAGNOSTIC: created CLTLoginMediator sidecar for launcher object {}", fmt::ptr(owner));
    }
}

void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword) {
    g_LoginControllerAuthName = (authName && authName[0]) ? authName : "";
    g_LoginControllerAuthPassword = (authPassword && authPassword[0]) ? authPassword : "";
    DiagnosticApplyLoginControllerConfig();
}

mxo::ltlogin::CLTLoginMediator* DiagnosticAuthGetLoginController() {
    return g_DiagnosticLoginController;
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
                context == g_DiagnosticAuthContext ? "AuthPeerClosed" : "MarginPeerClosed");
        }
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
    if (!g_DiagnosticLoginController || !selectionContext) {
        return;
    }
    if (byteCount < sizeof(mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch)) {
        spdlog::info(
            "DIAGNOSTIC: selection context mirror into login controller skipped byteCount=0x{:x} required=0x{:x}",
            byteCount,
            sizeof(mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch));
        return;
    }

    mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch input = {};
    std::memcpy(&input, selectionContext, sizeof(input));
    g_DiagnosticLoginController->PersistSelectionContextForState8(input);
    spdlog::info(
        "DIAGNOSTIC: mirrored selection context into CLTLoginMediator sidecar slot=0x{:02x} firstBlock04=0x{:08x} lastBlockA4=0x{:08x}",
        input.slotOrSelectionIndex00 & 0xffu,
        input.block04[0],
        input.blockA4[3]);
}

void DiagnosticMirrorState9StartupTripleIntoLoginController(void* callback84, void* object88, void* object8c) {
    if (!g_DiagnosticLoginController) {
        return;
    }

    g_DiagnosticLoginController->SetState9CallbackObjectTriple84_88_8c(callback84, object88, object8c);
    spdlog::info(
        "DIAGNOSTIC: mirrored state9 startup triple into CLTLoginMediator sidecar callback84={} object88={} object8c={}",
        fmt::ptr(callback84),
        fmt::ptr(object88),
        fmt::ptr(object8c));
}

uint32_t DiagnosticFillState9CallbackBlob18c(void* outBuffer, uint32_t arg2, uint32_t arg3) {
    if (!g_DiagnosticLoginController || !outBuffer) {
        return 1u;
    }
    return g_DiagnosticLoginController->FillState9CallbackBlob18cScaffold(
        static_cast<uint32_t*>(outBuffer),
        arg2,
        arg3);
}

const void* DiagnosticGetState9CallbackSeedPointer85D4() {
    if (!g_DiagnosticLoginController) {
        return NULL;
    }

    std::array<uint8_t, 16> seed = {};
    if (!g_DiagnosticLoginController->CopyMarginBootstrapTwofishKeyScaffold(&seed)) {
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

const char* DiagnosticAuthCurrentCharacterName() {
    if (g_DiagnosticLoginController) {
        if (const auto* currentSlotRecord = g_DiagnosticLoginController->GetCurrentSlotRecord()) {
            if (!currentSlotRecord->heapString14.empty()) {
                return currentSlotRecord->heapString14.c_str();
            }
        }
        if (const char* slotZeroName = g_DiagnosticLoginController->GetSlotRecordHeapStringByIndex(0)) {
            return slotZeroName;
        }
        if (const char* materializedName = g_DiagnosticLoginController->CharacterNameBufferF1c()) {
            if (materializedName[0] != '\0') {
                return materializedName;
            }
        }
        const auto& sourceLeadString108 = g_DiagnosticLoginController->SourceLeadString108();
        if (sourceLeadString108[0] != '\0') {
            return sourceLeadString108.data();
        }
    }
    return g_LoginControllerCharacterNameSeed[0] ? g_LoginControllerCharacterNameSeed : nullptr;
}

uint32_t DiagnosticAuthCurrentCharacterIdLow() {
    if (!g_DiagnosticLoginController) {
        return 0u;
    }
    if (const auto* currentSlotRecord = g_DiagnosticLoginController->GetCurrentSlotRecord()) {
        return currentSlotRecord->globalCharacterIdLow03;
    }
    if (const auto* slotZeroRecord = g_DiagnosticLoginController->GetSlotRecordByIndex(0)) {
        return slotZeroRecord->globalCharacterIdLow03;
    }
    return 0u;
}

uint32_t DiagnosticAuthCurrentCharacterIdHigh() {
    if (!g_DiagnosticLoginController) {
        return 0u;
    }
    if (const auto* currentSlotRecord = g_DiagnosticLoginController->GetCurrentSlotRecord()) {
        return currentSlotRecord->globalCharacterIdHigh07;
    }
    if (const auto* slotZeroRecord = g_DiagnosticLoginController->GetSlotRecordByIndex(0)) {
        return slotZeroRecord->globalCharacterIdHigh07;
    }
    return 0u;
}

static bool IsLikelyMiddleInitialOnly(const char* value) {
    return value != nullptr && std::char_traits<char>::length(value) == 1u;
}

const char* DiagnosticAuthCurrentRealFirstName() {
    if (!g_DiagnosticLoginController) {
        return nullptr;
    }
    const char* sourceBlock178 = reinterpret_cast<const char*>(g_DiagnosticLoginController->SourceBlock178().data());
    if (sourceBlock178[0] != '\0') {
        return sourceBlock178;
    }
    const auto& ownerState = g_DiagnosticLoginController->PostAuthMarginLoadingStateView();
    const char* section0F8c = ownerState.section0StringF8c[0] ? ownerState.section0StringF8c.data() : nullptr;
    const char* section0Fac = ownerState.section0StringFac[0] ? ownerState.section0StringFac.data() : nullptr;
    const char* section0Fcc = ownerState.section0StringFcc[0] ? ownerState.section0StringFcc.data() : nullptr;
    if (IsLikelyMiddleInitialOnly(section0F8c) && section0Fac && section0Fcc) {
        return section0Fac;
    }
    return section0F8c;
}

const char* DiagnosticAuthCurrentRealLastName() {
    if (!g_DiagnosticLoginController) {
        return nullptr;
    }
    const char* sourceBlock198 = reinterpret_cast<const char*>(g_DiagnosticLoginController->SourceBlock198().data());
    if (sourceBlock198[0] != '\0') {
        return sourceBlock198;
    }
    const auto& ownerState = g_DiagnosticLoginController->PostAuthMarginLoadingStateView();
    const char* section0F8c = ownerState.section0StringF8c[0] ? ownerState.section0StringF8c.data() : nullptr;
    const char* section0Fac = ownerState.section0StringFac[0] ? ownerState.section0StringFac.data() : nullptr;
    const char* section0Fcc = ownerState.section0StringFcc[0] ? ownerState.section0StringFcc.data() : nullptr;
    if (IsLikelyMiddleInitialOnly(section0F8c) && section0Fac && section0Fcc) {
        return section0Fcc;
    }
    return section0Fac;
}

const char* DiagnosticAuthCurrentBackground() {
    if (!g_DiagnosticLoginController) {
        return nullptr;
    }
    const char* sourceBlock1b8 = reinterpret_cast<const char*>(g_DiagnosticLoginController->SourceBlock1b8().data());
    if (sourceBlock1b8[0] != '\0') {
        return sourceBlock1b8;
    }
    const auto& ownerState = g_DiagnosticLoginController->PostAuthMarginLoadingStateView();
    const char* section0F8c = ownerState.section0StringF8c[0] ? ownerState.section0StringF8c.data() : nullptr;
    const char* section0Fac = ownerState.section0StringFac[0] ? ownerState.section0StringFac.data() : nullptr;
    const char* section0Fcc = ownerState.section0StringFcc[0] ? ownerState.section0StringFcc.data() : nullptr;
    if (IsLikelyMiddleInitialOnly(section0F8c) && section0Fac && section0Fcc) {
        return nullptr;
    }
    return section0Fcc;
}

bool DiagnosticCanBeginAuthConnection() {
    return g_DiagnosticLoginController != NULL;
}

uint32_t DiagnosticBeginAuthConnection() {
    if (!g_DiagnosticLoginController) {
        spdlog::info("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for auth connection");
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

    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::BeginAuthConnection() -> 0x{:08x}",
        static_cast<unsigned>(result));
    return result;
}

uint32_t DiagnosticBeginMarginConnection() {
    if (!g_DiagnosticLoginController) {
        spdlog::info("DIAGNOSTIC: CLTLoginMediator sidecar unavailable for margin connection");
        return 0;
    }

    DiagnosticRawMessageConnectionContext* context =
        DiagnosticGetOrCreateRawConnectionContext(&g_DiagnosticMarginContext, "MarginConnection");
    if (context) {
        g_DiagnosticLoginController->SetMarginConnectionContextKey(context);
    }

    const uint32_t result = g_DiagnosticLoginStateState4.Slot3_BeginOrContinue(
        g_DiagnosticLoginController->CurrentState(),
        g_DiagnosticLoginController);
    const std::string marginHost = g_DiagnosticLoginController->ResolvedMarginHostName();
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

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State4::Slot3_BeginOrContinue() marginHost='{}' -> 0x{:08x}",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        static_cast<unsigned>(result));
    return result;
}
