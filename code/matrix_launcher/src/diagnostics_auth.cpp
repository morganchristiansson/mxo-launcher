#include "diagnostics.h"
#include "diagnostics_auth.h"

#include "../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
#include "../matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h"
#include "loginmediator.h"
#include "loginstate.h"
#include "launchpad.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
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
static char g_LoginControllerSelectionSeedName[64] = {};
static uint32_t g_LoginControllerSelectionSeedWorldIndexLow24 = 0;
static bool g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed = false;
static char g_LoginControllerHelper11CharacterName[256] = {};
static char g_LoginControllerHelper11GameSessionId[256] = {};
static const char* g_LoginControllerAuthName = "resurrections";
static const char* g_LoginControllerAuthPassword = "";

static uint32_t __thiscall DiagnosticQueuedWorkItem_Release(DiagnosticQueuedWorkItemStub* self) {
    if (self) {
        Log(
            "DIAGNOSTIC: releasing queued work item %p type=%u payload=0x%08x label='%s'",
            self,
            (unsigned)self->header.workType,
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

static const char* DiagnosticGetEnvNonEmpty(const char* name) {
    if (!name || !name[0]) {
        return NULL;
    }

    const char* value = std::getenv(name);
    return (value && value[0]) ? value : NULL;
}

static bool DiagnosticTryParseEnvU32(const char* name, uint32_t* outValue) {
    if (outValue) {
        *outValue = 0u;
    }

    const char* value = DiagnosticGetEnvNonEmpty(name);
    if (!value || !outValue) {
        return false;
    }

    char* end = NULL;
    const unsigned long parsed = std::strtoul(value, &end, 0);
    if (!end || end == value || *end != '\0') {
        return false;
    }

    *outValue = static_cast<uint32_t>(parsed & 0xffffffffu);
    return true;
}

static size_t DiagnosticParseEnvU32List(
    const char* name,
    uint32_t* outValues,
    size_t maxValues) {
    if (!outValues || maxValues == 0u) {
        return 0u;
    }

    std::fill(outValues, outValues + maxValues, 0u);

    const char* text = DiagnosticGetEnvNonEmpty(name);
    if (!text) {
        return 0u;
    }

    size_t count = 0u;
    const char* cursor = text;
    while (*cursor && count < maxValues) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == ';' || *cursor == '|') {
            ++cursor;
        }
        if (!*cursor) {
            break;
        }

        char* end = NULL;
        const unsigned long parsed = std::strtoul(cursor, &end, 0);
        if (!end || end == cursor) {
            break;
        }

        outValues[count++] = static_cast<uint32_t>(parsed & 0xffffffffu);
        cursor = end;
    }

    return count;
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
        workItem ? (unsigned)workItem->header.workType : 0u,
        workItem ? (unsigned)workItem->workPayload : 0u);

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
                Log(
                    "DIAGNOSTIC: received-bytes label='%s' total=%u preview=%s",
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
                        Log(
                            "DIAGNOSTIC: auth receive buffering incomplete frame buffered=%u preview=%s",
                            (unsigned)bytes.size(),
                            hexPreview);
                        break;
                    }

                    Log(
                        "DIAGNOSTIC: auth receive framing payloadLength=%u headerBytes=%u rawCode=0x%02x likelyMessage='%s'",
                        (unsigned)payloadSize,
                        (unsigned)headerBytes,
                        (unsigned)rawCode,
                        DiagnosticAuthRawCodeName(rawCode));

                    if (g_DiagnosticLoginController) {
                        const uint32_t handled =
                            g_DiagnosticLoginController->HandleAuthPacketBytes(payloadBytes, payloadSize);
                        Log(
                            "DIAGNOSTIC: launcher-owned auth packet handler label='%s' handled=%u rawCode=0x%02x",
                            self->debugLabel ? self->debugLabel : "<null>",
                            (unsigned)handled,
                            (unsigned)rawCode);

                        if (handled != 0u && rawCode == 0x0b && !g_DiagnosticPostAuthMarginBeginAttempted) {
                            g_DiagnosticPostAuthMarginBeginAttempted = true;
                            const uint32_t marginConnectResult = DiagnosticBeginMarginConnection();
                            Log(
                                "DIAGNOSTIC: post-AS_AuthReply margin auto-begin result = 0x%08x",
                                (unsigned)marginConnectResult);
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
                        Log(
                            "DIAGNOSTIC: margin receive buffering incomplete frame buffered=%u preview=%s",
                            (unsigned)bytes.size(),
                            hexPreview);
                        break;
                    }

                    const bool looksLikePlainBootstrapReply =
                        rawCode == 0x02u || rawCode == 0x04u || rawCode == 0x07u || rawCode == 0x09u;
                    Log(
                        "DIAGNOSTIC: margin receive framing payloadLength=%u headerBytes=%u outerByte0=0x%02x framingHint='%s' logicalOpcode=resolved-later-by-mediator-after-optional-decrypt",
                        (unsigned)payloadSize,
                        (unsigned)headerBytes,
                        (unsigned)rawCode,
                        looksLikePlainBootstrapReply ? "plaintext-bootstrap-reply" : "possibly-encrypted-post-bootstrap-payload");

                    if (g_DiagnosticLoginController && payloadBytes && payloadSize != 0u) {
                        const uint32_t handled =
                            g_DiagnosticLoginController->HandleMarginPacketBytes(payloadBytes, payloadSize);
                        Log(
                            "DIAGNOSTIC: launcher-owned margin packet handler label='%s' handled=%u outerByte0=0x%02x decryptedOpcode=see-CLTLoginMediator-log-when-transportEncrypted=1",
                            self->debugLabel ? self->debugLabel : "<null>",
                            (unsigned)handled,
                            (unsigned)rawCode);
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

    const char* helper11RealFirstName = DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_REAL_FIRST_NAME");
    const char* helper11RealLastName = DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_REAL_LAST_NAME");
    const char* helper11Background = DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_BACKGROUND");
    std::array<uint32_t, 17> appearanceIds = {};
    const size_t appearanceCount = DiagnosticParseEnvU32List(
        "MXO_DIAGNOSTIC_APPEARANCE_IDS",
        appearanceIds.data(),
        appearanceIds.size());
    const char* helper11CharacterName =
        g_LoginControllerHelper11CharacterName[0]
            ? g_LoginControllerHelper11CharacterName
            : ((g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed &&
                g_LoginControllerSelectionSeedName[0])
                   ? g_LoginControllerSelectionSeedName
                   : NULL);
    const bool shouldSeedRecoveredProcessLoginCredentials =
        g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed ||
        g_LoginControllerHelper11CharacterName[0] ||
        (helper11RealFirstName && helper11RealFirstName[0]) ||
        (helper11RealLastName && helper11RealLastName[0]) ||
        (helper11Background && helper11Background[0]) ||
        (appearanceCount != 0u);
    if (shouldSeedRecoveredProcessLoginCredentials && helper11CharacterName && helper11CharacterName[0]) {
        // Diagnostic bridge only:
        // - `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the strongest concrete
        //   writer for helper11 owner fields `+0x108/+0x12c/+0x134..+0x1b8`
        // - the exact original upstream producer feeding that blob is still unresolved
        // - the replacement launcher therefore lets explicit launcher/runtime seed inputs exercise
        //   the confirmed writer without pretending we already recovered the original producer
        mxo::ltlogin::CLTLoginMediator::ProcessLoginCredentialsInputSketch input = {};
        DiagnosticCopyCStringIntoFixed(
            input.string00.data(),
            input.string00.size(),
            helper11CharacterName);
        input.field24 = g_LoginControllerSelectionSeedWorldIndexLow24;

        DiagnosticCopyCStringIntoFixed(
            input.string70.data(),
            input.string70.size(),
            helper11RealFirstName);
        DiagnosticCopyCStringIntoFixed(
            input.string90.data(),
            input.string90.size(),
            helper11RealLastName);
        DiagnosticCopyCStringIntoFixed(
            input.stringB0.data(),
            input.stringB0.size(),
            helper11Background);

        for (size_t i = 0; i < 8u; ++i) {
            input.dwords2c[i] = appearanceIds[i];
            input.dwords4c[i] = appearanceIds[8u + i];
        }
        input.bytes6c[0] = static_cast<uint8_t>(appearanceIds[16] & 0xffu);
        input.bytes6c[1] = static_cast<uint8_t>((appearanceIds[16] >> 8) & 0xffu);
        input.bytes6c[2] = static_cast<uint8_t>((appearanceIds[16] >> 16) & 0xffu);
        input.bytes6c[3] = static_cast<uint8_t>((appearanceIds[16] >> 24) & 0xffu);

        g_DiagnosticLoginController->ProcessLoginCredentials(input);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig applied recovered 0x41c3c0 seed characterName='{}' selectedWorldIndexLow24=0x{:06x} realFirst='{}' realLast='{}' background='{}' appearanceCount={} explicitEnable={} launcherCharacterSeed={}",
            helper11CharacterName,
            static_cast<unsigned>(g_LoginControllerSelectionSeedWorldIndexLow24),
            input.string70[0] ? input.string70.data() : "<empty>",
            input.string90[0] ? input.string90.data() : "<empty>",
            input.stringB0[0] ? input.stringB0.data() : "<empty>",
            static_cast<unsigned>(appearanceCount),
            g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed ? 1u : 0u,
            g_LoginControllerHelper11CharacterName[0] ? 1u : 0u);
    }

    mxo::ltlogin::LaunchPadClient launchPad;
    const char* diagnosticPlayRequestSessionId =
        g_LoginControllerHelper11GameSessionId[0]
            ? g_LoginControllerHelper11GameSessionId
            : DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_PLAY_REQUEST_SESSION_ID");
    if (diagnosticPlayRequestSessionId) {
        launchPad.OnPlayRequestStatus(g_DiagnosticLoginController, /*resultCode=*/0u, diagnosticPlayRequestSessionId);
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig routed diagnostic LaunchPadClient::OnPlayRequestStatus GameSessionID='{}' launcherSessionSeed={}",
            diagnosticPlayRequestSessionId,
            g_LoginControllerHelper11GameSessionId[0] ? 1u : 0u);
    }

    const char* diagnosticLoginSource94First = DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_LOGIN_SOURCE94_FIRST");
    const char* diagnosticLoginSessionText = DiagnosticGetEnvNonEmpty("MXO_DIAGNOSTIC_LOGIN_SESSION_TEXT");
    uint32_t diagnosticLoginShared660 = 0u;
    const bool haveDiagnosticLoginShared660 =
        DiagnosticTryParseEnvU32("MXO_DIAGNOSTIC_LOGIN_SHARED660", &diagnosticLoginShared660);
    if (diagnosticLoginSource94First || diagnosticLoginSessionText || haveDiagnosticLoginShared660) {
        launchPad.OnLoginRequestStatus(
            g_DiagnosticLoginController,
            /*resultCode=*/0u,
            diagnosticLoginSource94First ? diagnosticLoginSource94First : "",
            diagnosticLoginShared660,
            diagnosticLoginSessionText ? diagnosticLoginSessionText : "");
        spdlog::info(
            "DiagnosticApplyLoginControllerConfig routed diagnostic LaunchPadClient::OnLoginRequestStatus owner660=0x{:08x} source94='{}' session='{}'",
            static_cast<unsigned>(diagnosticLoginShared660),
            diagnosticLoginSource94First ? diagnosticLoginSource94First : "<empty>",
            diagnosticLoginSessionText ? diagnosticLoginSessionText : "<empty>");
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

void DiagnosticMirrorSelectionContextIntoLoginController(const void* selectionContext, uint32_t byteCount) {
    if (!g_DiagnosticLoginController || !selectionContext) {
        return;
    }
    if (byteCount < sizeof(mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch)) {
        Log(
            "DIAGNOSTIC: selection context mirror into login controller skipped byteCount=0x%lx required=0x%lx",
            (unsigned long)byteCount,
            (unsigned long)sizeof(mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch));
        return;
    }

    mxo::ltlogin::CLTLoginMediator::State3SelectionContextInputSketch input = {};
    std::memcpy(&input, selectionContext, sizeof(input));
    g_DiagnosticLoginController->PersistSelectionContextForState8(input);
    Log(
        "DIAGNOSTIC: mirrored selection context into CLTLoginMediator sidecar slot=0x%02x firstBlock04=0x%08x lastBlockA4=0x%08x",
        (unsigned)(input.slotOrSelectionIndex00 & 0xffu),
        (unsigned)input.block04[0],
        (unsigned)input.blockA4[3]);
}

void DiagnosticConfigureLoginControllerSelectionSeed(
    const char* selectionName,
    uint32_t selectedWorldIndexLow24,
    bool enableRecoveredProcessLoginCredentialsSeed) {
    std::strncpy(
        g_LoginControllerSelectionSeedName,
        selectionName ? selectionName : "",
        sizeof(g_LoginControllerSelectionSeedName) - 1);
    g_LoginControllerSelectionSeedName[sizeof(g_LoginControllerSelectionSeedName) - 1] = '\0';
    g_LoginControllerSelectionSeedWorldIndexLow24 = selectedWorldIndexLow24 & 0x00ffffffu;
    g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed = enableRecoveredProcessLoginCredentialsSeed;
    DiagnosticApplyLoginControllerConfig();
    Log(
        "DIAGNOSTIC: recovered 0x41c3c0 seed configured enabled=%u selectionName='%s' selectedWorldIndexLow24=0x%06x",
        g_LoginControllerEnableRecoveredProcessLoginCredentialsSeed ? 1u : 0u,
        g_LoginControllerSelectionSeedName[0] ? g_LoginControllerSelectionSeedName : "<empty>",
        (unsigned)g_LoginControllerSelectionSeedWorldIndexLow24);
}

void DiagnosticConfigureLoginControllerHelper11Seed(
    const char* characterName,
    const char* gameSessionId) {
    std::strncpy(
        g_LoginControllerHelper11CharacterName,
        characterName ? characterName : "",
        sizeof(g_LoginControllerHelper11CharacterName) - 1);
    g_LoginControllerHelper11CharacterName[sizeof(g_LoginControllerHelper11CharacterName) - 1] = '\0';
    std::strncpy(
        g_LoginControllerHelper11GameSessionId,
        gameSessionId ? gameSessionId : "",
        sizeof(g_LoginControllerHelper11GameSessionId) - 1);
    g_LoginControllerHelper11GameSessionId[sizeof(g_LoginControllerHelper11GameSessionId) - 1] = '\0';
    DiagnosticApplyLoginControllerConfig();
    Log(
        "DIAGNOSTIC: helper11 seed bridge configured character='%s' session='%s' (diagnostic bridge into confirmed 0x41c3c0 / 0x420ef0 writers; original upstream producer still unresolved)",
        g_LoginControllerHelper11CharacterName[0] ? g_LoginControllerHelper11CharacterName : "<empty>",
        g_LoginControllerHelper11GameSessionId[0] ? g_LoginControllerHelper11GameSessionId : "<empty>");
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

    Log(
        "DIAGNOSTIC: CLTLoginState_State4::Slot3_BeginOrContinue() marginHost='%s' port=%u -> 0x%08x",
        marginHost.empty() ? "<unresolved>" : marginHost.c_str(),
        (unsigned)g_DiagnosticLoginController->MarginServerPortHostOrder(),
        (unsigned)result);
    return result;
}
