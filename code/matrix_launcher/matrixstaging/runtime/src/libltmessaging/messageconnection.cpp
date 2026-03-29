#include "messageconnection.h"

#include "../../../game/src/libltclientlogin/loginmediator.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <utility>

#ifdef DispatchMessage
#undef DispatchMessage
#endif

namespace mxo::liblttcp {

// ============================================================
// Message-connection family notes
// ============================================================
// Current corrected split:
// - base `CMessageConnection` surface here is centered on the shared ctor/send/completion family:
//   - `0x448b40`
//   - `0x448cf0`
//   - `0x4490c0`
//   - inherited base connection wrappers from `CLTTCPConnection`
// - later leaf-family vtables such as `0x004afef0` / `0x004aff38` layer their own completion /
//   dispatch overrides on top of that base surface
// So this source file keeps the shared base scaffolds explicit and only models the concrete
// `CMarginConnection` leaf override family directly.

// UNANCHORED: source-owned narrow subset of `0x448b40` with a null engine and without the
// optional completion-helper allocation path.
CMessageConnection::CMessageConnection()
    : CLTTCPConnection(),
      packetNameCallbackScaffold_(0),
      packetizedMessagesEnabledScaffold_(false),
      packetAgendaScaffold_() {}

// UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
CMessageConnection::CMessageConnection(CLTThreadPerClientTCPEngine* engine)
    : CLTTCPConnection(),
      packetNameCallbackScaffold_(0),
      packetizedMessagesEnabledScaffold_(false),
      packetAgendaScaffold_() {
    CLTTCPConnection::SetEngine(engine);
}

// UNANCHORED: source-owned default destructor; the original family uses several concrete deleting-dtor paths.
CMessageConnection::~CMessageConnection() = default;

// UNANCHORED: source-owned compatibility pass-through over the recovered base `+0x10` engine field.
void CMessageConnection::SetEngine(CLTThreadPerClientTCPEngine* engine) {
    CLTTCPConnection::SetEngine(engine);
}

// UNANCHORED: source-owned compatibility accessor over the recovered base `+0x10` engine field.
CLTThreadPerClientTCPEngine* CMessageConnection::Engine() const {
    return CLTTCPConnection::Engine();
}

void CMessageConnectionMessageScaffold::ResetForPacketBuilderScaffold() {
    // anchor: launcher.exe:0x455bd0 / 0x455c60 / 0x455cd0
    // Best current source-owned mirror of the shared message-object startup shape:
    // - reset the builder-reserved byte count at `+0x08` to `0x2b`
    // - clear the payload-length header bytes `+0x0a/+0x0b`
    // - leave the payload body empty until builder helpers grow it through `0x4557b0`
    reservedBytes08 = kBuilderReservedBytes08;
    payloadBytesFrom0c.clear();
}

void CMessageConnectionMessageScaffold::ResetPayloadByteCountScaffold(uint16_t payloadByteCount) {
    const uint16_t clampedByteCount = std::min<uint16_t>(payloadByteCount, kMaxPayloadByteCount);
    payloadBytesFrom0c.assign(clampedByteCount, 0u);
}

uint16_t CMessageConnectionMessageScaffold::GrowPayloadByteCountScaffold(uint16_t additionalByteCount) {
    // anchor: launcher.exe:0x4557b0
    const uint32_t oldByteCount = static_cast<uint32_t>(payloadBytesFrom0c.size());
    const uint32_t requestedByteCount = oldByteCount + static_cast<uint32_t>(additionalByteCount);
    if (requestedByteCount > kMaxPayloadByteCount) {
        return static_cast<uint16_t>(oldByteCount);
    }

    payloadBytesFrom0c.resize(static_cast<size_t>(requestedByteCount), 0u);
    return static_cast<uint16_t>(requestedByteCount);
}

uint16_t CMessageConnectionMessageScaffold::PayloadByteCountScaffold() const {
    return static_cast<uint16_t>(std::min<size_t>(payloadBytesFrom0c.size(), kMaxPayloadByteCount));
}

uint16_t CMessageConnectionMessageScaffold::RemainingAppendableByteCountScaffold() const {
    const uint32_t payloadByteCount = PayloadByteCountScaffold();
    if (payloadByteCount >= kMaxPayloadByteCount || reservedBytes08 >= kMaxPayloadByteCount) {
        return 0u;
    }

    const uint32_t remaining = kMaxPayloadByteCount - payloadByteCount - reservedBytes08;
    return static_cast<uint16_t>(std::min<uint32_t>(remaining, kMaxPayloadByteCount));
}

uint8_t* CMessageConnectionMessageScaffold::PayloadBaseScaffold() {
    return payloadBytesFrom0c.empty() ? nullptr : payloadBytesFrom0c.data();
}

const uint8_t* CMessageConnectionMessageScaffold::PayloadBaseScaffold() const {
    return payloadBytesFrom0c.empty() ? nullptr : payloadBytesFrom0c.data();
}

std::vector<uint8_t> CMessageConnectionMessageScaffold::BuildFramedBytesFrom0aScaffold() const {
    const uint16_t payloadByteCount = PayloadByteCountScaffold();
    std::vector<uint8_t> framedBytesFrom0a;
    framedBytesFrom0a.reserve(static_cast<size_t>(payloadByteCount) + ((payloadByteCount > 0x7fu) ? 2u : 1u));

    if (payloadByteCount > 0x7fu) {
        framedBytesFrom0a.push_back(static_cast<uint8_t>(0x80u | ((payloadByteCount >> 8) & 0x7fu)));
    } else {
        framedBytesFrom0a.push_back(0u);
    }
    framedBytesFrom0a.push_back(static_cast<uint8_t>(payloadByteCount & 0xffu));
    framedBytesFrom0a.insert(
        framedBytesFrom0a.end(),
        payloadBytesFrom0c.begin(),
        payloadBytesFrom0c.end());
    return framedBytesFrom0a;
}

const char* CMessageConnection::PacketNameFamilyToString(CMessageConnectionPacketNameFamilyScaffold family) {
    switch (family) {
        case CMessageConnectionPacketNameFamilyScaffold::kAuth:
            return "auth";
        case CMessageConnectionPacketNameFamilyScaffold::kMargin:
            return "margin";
        default:
            return "unknown";
    }
}

uintptr_t CMessageConnection::PacketNameCallbackAddressScaffold(
    CMessageConnectionPacketNameFamilyScaffold family) {
    switch (family) {
        case CMessageConnectionPacketNameFamilyScaffold::kAuth:
            return 0x0041ce00u;
        case CMessageConnectionPacketNameFamilyScaffold::kMargin:
            return 0x0041ce40u;
        default:
            return 0u;
    }
}

void CMessageConnection::ConfigurePacketNameFamilyScaffold(
    CMessageConnectionPacketNameFamilyScaffold family,
    bool packetizedMessagesEnabled) {
    packetNameCallbackScaffold_ = PacketNameCallbackAddressScaffold(family);
    packetizedMessagesEnabledScaffold_ = packetizedMessagesEnabled;
}

CMessageConnectionPacketNameFamilyScaffold CMessageConnection::PacketNameFamilyScaffold() const {
    switch (packetNameCallbackScaffold_) {
        case 0x0041ce00u:
            return CMessageConnectionPacketNameFamilyScaffold::kAuth;
        case 0x0041ce40u:
            return CMessageConnectionPacketNameFamilyScaffold::kMargin;
        default:
            return CMessageConnectionPacketNameFamilyScaffold::kUnknown;
    }
}

bool CMessageConnection::PacketizedMessagesEnabledScaffold() const {
    return packetizedMessagesEnabledScaffold_;
}

void CMessageConnection::ConfigurePacketAgendaScaffold(const void* agendaConfiguration) {
    if (!packetAgendaScaffold_) {
        packetAgendaScaffold_ = std::make_unique<CMessageConnectionPacketAgendaScaffold>();
    }
    if (packetAgendaScaffold_) {
        packetAgendaScaffold_->created = true;
        packetAgendaScaffold_->hasEmbeddedDefaultReadPassThroughHelper0c = true;
        packetAgendaScaffold_->hasReadOutputSlot08 = false;
        packetAgendaScaffold_->hasWriteOutputSlot24 = false;
        // anchor: launcher.exe:0x448980 -> 0x469740
        // Current best known original builder path (`0x41470 -> 0x44da00`) installs both a read
        // helper and a write helper on the agenda configuration object before `0x448980` forwards
        // it into the agenda at connection `+0x74`.
        // Source still accepts a typeless pointer here, so keep the modeled caller-installed helper
        // presence conservative: null => no extra helpers known, non-null => one read + one write
        // helper known. The embedded default read pass-through helper is treated as part of agenda
        // creation itself and is not counted here.
        packetAgendaScaffold_->configuredReadHelperCount = (agendaConfiguration != nullptr) ? 1u : 0u;
        packetAgendaScaffold_->configuredWriteHelperCount = (agendaConfiguration != nullptr) ? 1u : 0u;
    }
}

const CMessageConnectionPacketAgendaScaffold* CMessageConnection::PacketAgendaScaffold() const {
    return packetAgendaScaffold_.get();
}

// Source-owned launcher-only bridge for the narrowed margin send-authenticity gap.
// Mirrors the now-recovered original split between:
// - local packet-envelope object (`0x41af70` / `0x41cf30`)
// - shared message object consumed by `0x448cf0 -> 0x448a00`
CMessageConnectionEnvelopeScaffold CMessageConnection::BuildPacketBuilderEnvelopeScaffold(bool headerless) {
    CMessageConnectionEnvelopeScaffold envelope = {};
    envelope.sharedMessage = std::make_shared<CMessageConnectionMessageScaffold>();
    if (!envelope.sharedMessage) {
        return envelope;
    }

    envelope.sharedMessage->ResetForPacketBuilderScaffold();
    envelope.headerless10 = headerless ? 1u : 0u;
    return envelope;
}

CMessageConnectionEnvelopeScaffold CMessageConnection::BuildPayloadEnvelopeScaffold(
    const void* packetData,
    uint32_t packetByteCount,
    bool headerless) {
    CMessageConnectionEnvelopeScaffold envelope = BuildPacketBuilderEnvelopeScaffold(headerless);
    if (!packetData || packetByteCount == 0u ||
        packetByteCount > CMessageConnectionMessageScaffold::kMaxPayloadByteCount ||
        !envelope.sharedMessage) {
        envelope.sharedMessage.reset();
        return envelope;
    }

    envelope.sharedMessage->ResetPayloadByteCountScaffold(static_cast<uint16_t>(packetByteCount));
    uint8_t* payloadBytes = envelope.sharedMessage->PayloadBaseScaffold();
    if (!payloadBytes) {
        envelope.sharedMessage.reset();
        return envelope;
    }

    std::copy_n(static_cast<const uint8_t*>(packetData), packetByteCount, payloadBytes);
    return envelope;
}

bool CMessageConnection::ApplySendPacketAgendaScaffold(
    const CMessageConnectionEnvelopeScaffold& inputEnvelope,
    CMessageConnectionEnvelopeScaffold* outputEnvelope,
    bool* outAgendaTouched) {
    if (outAgendaTouched) {
        *outAgendaTouched = false;
    }
    if (outputEnvelope) {
        *outputEnvelope = inputEnvelope;
    }

    // `0x448cf0` consults connection `+0x74` and may discard the packet before submit.
    // Current bounded source model preserves the nearer `0x469950` handoff shape:
    // - no agenda / no active write helper (`+0x44 == 0`) => keep the original envelope
    // - active write helper => return the agenda write-output slot at `+0x24`
    // Current source still lacks helper-side transformation/discard, so on the active helper path
    // it seeds that output slot with the input envelope as a bounded pass-through fallback.
    CMessageConnectionPacketAgendaScaffold* agenda = packetAgendaScaffold_.get();
    if (!agenda || !agenda->created) {
        return true;
    }
    if (agenda->configuredWriteHelperCount == 0u) {
        return true;
    }

    agenda->writeOutputSlot24 = inputEnvelope;
    agenda->hasWriteOutputSlot24 = (agenda->writeOutputSlot24.sharedMessage != nullptr);
    if (outputEnvelope && agenda->hasWriteOutputSlot24) {
        *outputEnvelope = agenda->writeOutputSlot24;
    }
    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    return agenda->hasWriteOutputSlot24;
}

// anchor: launcher.exe:0x448a00
// Narrow source-owned mirror of the lower submit helper beneath the original
// envelope-based `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SubmitEnvelopeBytesScaffold(const CMessageConnectionEnvelopeScaffold& envelope) {
    if (!Engine() || !envelope.sharedMessage) {
        return 0u;
    }

    const std::vector<uint8_t> framedBytesFrom0a = envelope.sharedMessage->BuildFramedBytesFrom0aScaffold();
    if (framedBytesFrom0a.size() < 2u) {
        return 0u;
    }

    const uint8_t frameByte0a = framedBytesFrom0a[0];
    const uint8_t frameByte0b = framedBytesFrom0a[1];
    const uint32_t payloadByteCount =
        (static_cast<uint32_t>(frameByte0a & 0x7fu) << 8) |
        static_cast<uint32_t>(frameByte0b);
    const size_t pointerOffsetFrom0a = ((frameByte0a >> 7) == 0u) ? 1u : 0u;
    const uint32_t submittedByteCount = payloadByteCount + ((payloadByteCount > 0x7fu) ? 2u : 1u);

    if (framedBytesFrom0a.size() < pointerOffsetFrom0a + submittedByteCount) {
        return 0u;
    }

    const uint8_t* submittedBytes = framedBytesFrom0a.data() + pointerOffsetFrom0a;
    spdlog::info(
        "CMessageConnection::SubmitEnvelopeBytesScaffold reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} submitOffset={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(envelope.sharedMessage->reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<unsigned>(pointerOffsetFrom0a),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return Engine()->SendBufferConnectionScaffold(static_cast<CLTTCPConnection*>(this), submittedBytes, submittedByteCount, nullptr);
}

// anchor: launcher.exe:0x448cf0
// Narrow source-owned mirror of the envelope-based `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SendPacketEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope) {
    if (!Engine() || !envelope.sharedMessage) {
        return 0u;
    }

    CMessageConnectionEnvelopeScaffold agendaEnvelope = {};
    bool agendaTouched = false;
    if (!ApplySendPacketAgendaScaffold(envelope, &agendaEnvelope, &agendaTouched) ||
        !agendaEnvelope.sharedMessage) {
        spdlog::info(
            "CMessageConnection::SendPacketEnvelopeScaffold discarded packet at packet-agenda write handoff this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 0u;
    }

    const std::vector<uint8_t> framedBytesFrom0a = agendaEnvelope.sharedMessage->BuildFramedBytesFrom0aScaffold();
    const size_t submitOffset = (framedBytesFrom0a.empty() || ((framedBytesFrom0a[0] >> 7) != 0u)) ? 0u : 1u;
    const size_t opcodeIndex = submitOffset + ((submitOffset == 0u) ? 2u : 1u);
    const uint8_t rawOpcode = (opcodeIndex < framedBytesFrom0a.size()) ? framedBytesFrom0a[opcodeIndex] : 0u;
    const CMessageConnectionPacketAgendaScaffold* agenda = PacketAgendaScaffold();
    spdlog::info(
        "CMessageConnection::SendPacketEnvelopeScaffold headerless={} rawOpcode=0x{:02x} reservedBytes08=0x{:04x} payloadBytes={} framedBytesFrom0a={} packetNameCallback=0x{:08x} packetNameFamily={} packetizedEnabled={} agendaCreated={} agendaReadHelpers={} agendaWriteHelpers={} agendaWriteTouched={} agendaWriteOutputSlot24={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(agendaEnvelope.headerless10),
        static_cast<unsigned>(rawOpcode),
        static_cast<unsigned>(agendaEnvelope.sharedMessage->reservedBytes08),
        static_cast<unsigned>(agendaEnvelope.sharedMessage->PayloadByteCountScaffold()),
        static_cast<unsigned>(framedBytesFrom0a.size()),
        static_cast<uint32_t>(packetNameCallbackScaffold_),
        PacketNameFamilyToString(PacketNameFamilyScaffold()),
        packetizedMessagesEnabledScaffold_ ? 1u : 0u,
        (agenda && agenda->created) ? 1u : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredReadHelperCount) : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredWriteHelperCount) : 0u,
        agendaTouched ? 1u : 0u,
        (agenda && agenda->hasWriteOutputSlot24) ? 1u : 0u,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return SubmitEnvelopeBytesScaffold(agendaEnvelope);
}

// anchor: launcher.exe:0x448a60
// UNANCHORED: source-owned narrow fallback helper for the generic unhandled-operation log branch
// reached from the much larger `CMessageConnection::OnOperationCompleted` family.
static void CMessageConnection_LogUnhandledOperationScaffold(void* workItem) {
    spdlog::debug(
        "CMessageConnection_LogUnhandledOperationScaffold workItem={}",
        fmt::ptr(workItem));
}

static mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*
CMessageConnection_LoginMediatorContextScaffold(CMessageConnection* self) {
    return self
        ? static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(self->OwnerContext())
        : nullptr;
}

static uint32_t CMessageConnection_HandleSyntheticReceiveDrainProxyScaffold(
    CMessageConnection* self,
    uint32_t workPayload) {
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(self);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    if (!self || !context || !mediator) {
        spdlog::debug(
            "CMessageConnection_HandleSyntheticReceiveDrainProxyScaffold missing mediator context this={} ownerContext={} payload=0x{:08x}",
            fmt::ptr(self),
            fmt::ptr(self ? self->OwnerContext() : nullptr),
            workPayload);
        return 0u;
    }

    if (!context->isMarginConnection) {
        const uint32_t receiveActions = mediator->HandleAuthConnectionReceiveScaffold();
        if (receiveActions & mxo::ltlogin::CLTLoginMediator::kReceiveActionBeginMarginAfterAuthReply) {
            const uint32_t marginConnectResult = mediator->BeginLauncherMarginConnectionScaffold();
            spdlog::info(
                "CMessageConnection::OnOperationCompleted synthetic receive-drain post-AS_AuthReply margin auto-begin result=0x{:08x}",
                static_cast<unsigned>(marginConnectResult));
        }
    } else {
        (void)mediator->HandleMarginConnectionReceiveScaffold();
    }

    spdlog::info(
        "CMessageConnection::OnOperationCompleted handled synthetic receive-drain proxy payload=0x{:08x} this={} ownerContext={} isMargin={} remoteHost='{}'",
        workPayload,
        fmt::ptr(self),
        fmt::ptr(self->OwnerContext()),
        context->isMarginConnection ? 1u : 0u,
        self->RemoteHostName().empty() ? std::string("<empty>") : self->RemoteHostName());
    return 1u;
}

static uint32_t CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold(
    CMessageConnection* self,
    uint32_t workPayload,
    bool isMarginConnection,
    const char* ownerSlotLabel) {
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(self);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    if (!self || !context || !mediator) {
        spdlog::debug(
            "CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold missing mediator context this={} ownerContext={} payload=0x{:08x} ownerSlot={} expectedMargin={}",
            fmt::ptr(self),
            fmt::ptr(self ? self->OwnerContext() : nullptr),
            workPayload,
            ownerSlotLabel ? ownerSlotLabel : "<null>",
            isMarginConnection ? 1u : 0u);
        return 0u;
    }

    const uint32_t handled = isMarginConnection
        ? mediator->HandleMarginConnectStatus(workPayload)
        : mediator->HandleAuthConnectStatus(workPayload);
    spdlog::info(
        "CMessageConnection::OnOperationCompleted routed type-2 connect-status through owner fallback payload=0x{:08x} this={} ownerContext={} ownerSlot={} isMargin={} handled={} remoteHost='{}'",
        workPayload,
        fmt::ptr(self),
        fmt::ptr(self->OwnerContext()),
        ownerSlotLabel ? ownerSlotLabel : "<null>",
        isMarginConnection ? 1u : 0u,
        handled,
        self->RemoteHostName().empty() ? std::string("<empty>") : self->RemoteHostName());
    return handled;
}

// anchor: launcher.exe:0x4490c0 first dispatch on `workItem+0x04`
// Source-owned decomposition of the initial work-type test inside
// `CMessageConnection::OnOperationCompleted`.
static uint32_t CMessageConnection_WorkItemTypeScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const CLTThreadPerClientTCPEngine_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

// anchor: launcher.exe:0x4490c0 type-3 first-fragment copy setup
// Source-owned decomposition of the narrowed type-3 packet-copy path, which begins from the
// first retained fragment returned by the `CParsedPacketWorkItem` traversal state.
static const CLTTCPReadOperationFragmentScaffold* CMessageConnection_FirstRetainedFragmentScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem) {
    return (workItem && workItem->retainedFragmentCount0C != 0u)
        ? workItem->firstRetainedFragment10
        : nullptr;
}

// anchor: launcher.exe:0x4490c0 type-3 later-fragment copy loop
// Source-owned decomposition of the later-fragment walk performed after the first copy span.
static const CLTTCPReadOperationFragmentScaffold* CMessageConnection_NextRetainedFragmentScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    const CLTTCPReadOperationFragmentScaffold* fragment) {
    if (!workItem || !fragment || workItem->retainedFragmentCount0C == 0u) {
        return nullptr;
    }

    if (fragment == workItem->firstRetainedFragment10) {
        if (!workItem->retainedFragmentListOwner14 ||
            !workItem->retainedFragmentListOwner14->sentinel) {
            return nullptr;
        }

        const CParsedPacketWorkItem_RetainedFragmentNodeScaffold* firstNode =
            workItem->retainedFragmentListOwner14->sentinel->next;
        return (firstNode && firstNode != workItem->retainedFragmentListOwner14->sentinel)
            ? firstNode->retainedFragment08
            : nullptr;
    }

    if (!workItem->retainedFragmentListOwner14 ||
        !workItem->retainedFragmentListOwner14->sentinel) {
        return nullptr;
    }

    const CParsedPacketWorkItem_RetainedFragmentNodeScaffold* sentinel =
        workItem->retainedFragmentListOwner14->sentinel;
    for (const CParsedPacketWorkItem_RetainedFragmentNodeScaffold* node = sentinel->next;
         node && node != sentinel;
         node = node->next) {
        if (node->retainedFragment08 != fragment) {
            continue;
        }

        const CParsedPacketWorkItem_RetainedFragmentNodeScaffold* nextNode = node->next;
        return (nextNode && nextNode != sentinel) ? nextNode->retainedFragment08 : nullptr;
    }
    return nullptr;
}

// anchor: launcher.exe:0x455cd0 / 0x455c60
// Source-owned creation of the outer receive/message-ref scaffold that `0x4490c0` materializes
// before later `0x41bc20/0x41bbb0`-style message-code reads.
static CMessageConnectionReceivedMessageRefScaffold CMessageConnection_CreateReceivedMessageRefScaffold(
    bool headerless,
    uint32_t messageContext14 = 0u) {
    CMessageConnectionReceivedMessageRefScaffold messageRef = {};
    messageRef.messageStorage0c = std::make_shared<CMessageConnectionMessageScaffold>();
    if (!messageRef.messageStorage0c) {
        return messageRef;
    }

    messageRef.messageStorage0c->ResetForPacketBuilderScaffold();
    messageRef.headerless10 = headerless ? 1u : 0u;
    messageRef.messageContext14 = messageContext14;
    return messageRef;
}

// anchor: launcher.exe:0x4557b0
// Source-owned append helper mirroring the outer message-ref vtable `+0x18` growth path used by
// `0x4490c0` while copying packet-body spans into the inner message storage at outer `+0x0c`.
static bool CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
    CMessageConnectionReceivedMessageRefScaffold* messageRef,
    const uint8_t* payloadBytes,
    uint32_t payloadByteCount) {
    if (!messageRef || !messageRef->messageStorage0c) {
        return false;
    }
    if (payloadByteCount == 0u) {
        return true;
    }
    if (!payloadBytes || payloadByteCount > CMessageConnectionMessageScaffold::kMaxPayloadByteCount) {
        return false;
    }

    CMessageConnectionMessageScaffold& messageStorage = *messageRef->messageStorage0c;
    const uint16_t oldPayloadByteCount = messageStorage.PayloadByteCountScaffold();
    const uint32_t requestedPayloadByteCount =
        static_cast<uint32_t>(oldPayloadByteCount) + payloadByteCount;
    if (requestedPayloadByteCount > CMessageConnectionMessageScaffold::kMaxPayloadByteCount) {
        return false;
    }

    const uint16_t newPayloadByteCount =
        messageStorage.GrowPayloadByteCountScaffold(static_cast<uint16_t>(payloadByteCount));
    if (newPayloadByteCount != requestedPayloadByteCount) {
        return false;
    }

    uint8_t* payloadBase = messageStorage.PayloadBaseScaffold();
    if (!payloadBase) {
        return false;
    }

    std::copy_n(
        payloadBytes,
        payloadByteCount,
        payloadBase + static_cast<size_t>(oldPayloadByteCount));
    return true;
}

// anchor: launcher.exe:0x4490c0 type-3 parsed-packet body copy path
// Source-owned decomposition of the packet-body extraction that copies from
// `CParsedPacketWorkItem.currentCursor24` / `assembledByteCount28` into the later outer
// receive/message-ref scaffold.
static bool CMessageConnection_CopyParsedPacketIntoReceivedMessageRefScaffold(
    const CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CMessageConnectionReceivedMessageRefScaffold* outMessageRef,
    bool* outHadUnusedBuffers) {
    if (outHadUnusedBuffers) {
        *outHadUnusedBuffers = false;
    }
    if (!workItem || !outMessageRef || !outMessageRef->messageStorage0c) {
        return false;
    }

    const uint32_t packetBodyByteCount = workItem->assembledByteCount28;
    if (packetBodyByteCount == 0u) {
        return true;
    }

    const CLTTCPReadOperationFragmentScaffold* currentFragment =
        CMessageConnection_FirstRetainedFragmentScaffold(workItem);
    if (!currentFragment || !workItem->currentCursor24) {
        return false;
    }

    const uint8_t* fragmentBegin = currentFragment->bytes0C;
    const uint8_t* fragmentEnd = fragmentBegin + currentFragment->byteCount;
    const uint8_t* currentCursor = workItem->currentCursor24;
    if (currentCursor < fragmentBegin || currentCursor > fragmentEnd) {
        return false;
    }

    const uint32_t firstCopyByteCount = std::min<uint32_t>(
        packetBodyByteCount,
        static_cast<uint32_t>(fragmentEnd - currentCursor));
    if (!CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
            outMessageRef,
            currentCursor,
            firstCopyByteCount)) {
        return false;
    }

    uint32_t remainingPacketBodyByteCount = packetBodyByteCount - firstCopyByteCount;
    currentFragment = CMessageConnection_NextRetainedFragmentScaffold(workItem, currentFragment);
    while (currentFragment) {
        if (remainingPacketBodyByteCount == 0u) {
            if (outHadUnusedBuffers) {
                *outHadUnusedBuffers = true;
            }
            break;
        }

        const uint32_t copyByteCount =
            std::min<uint32_t>(remainingPacketBodyByteCount, currentFragment->byteCount);
        if (!CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
                outMessageRef,
                currentFragment->bytes0C,
                copyByteCount)) {
            return false;
        }
        remainingPacketBodyByteCount -= copyByteCount;
        currentFragment = CMessageConnection_NextRetainedFragmentScaffold(workItem, currentFragment);
    }

    return remainingPacketBodyByteCount == 0u;
}

namespace {

constexpr uint32_t kMessageLocatorPayloadOffsetTable[7] = {
    0x11u,
    0x04u,
    0x10u,
    0x0bu,
    0x10u,
    0x11u,
    0x10u,
};

static const std::vector<uint8_t>* CMessageConnection_MessagePayloadBytesScaffold(
    const CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    return messageRef.messageStorage0c ? &messageRef.messageStorage0c->payloadBytesFrom0c : nullptr;
}

static bool CMessageConnection_ResolveMessageCodePointerScaffold(
    const CMessageConnectionReceivedMessageRefScaffold& messageRef,
    const uint8_t** outMessageCodePointer,
    uint8_t* outTargetLocatorType,
    uint8_t* outSenderLocatorType,
    bool* outUsedHeaderlessLocatorDecode) {
    if (outMessageCodePointer) {
        *outMessageCodePointer = nullptr;
    }
    if (outTargetLocatorType) {
        *outTargetLocatorType = 0u;
    }
    if (outSenderLocatorType) {
        *outSenderLocatorType = 0u;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = false;
    }

    const std::vector<uint8_t>* payloadBytes =
        CMessageConnection_MessagePayloadBytesScaffold(messageRef);
    if (!payloadBytes || payloadBytes->empty()) {
        return false;
    }

    if (messageRef.headerless10 == 0u) {
        if (outMessageCodePointer) {
            *outMessageCodePointer = payloadBytes->data();
        }
        return true;
    }

    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = true;
    }
    if (payloadBytes->size() < 2u) {
        return false;
    }

    const uint8_t locatorByte0d = (*payloadBytes)[1];
    const uint8_t targetLocatorType = static_cast<uint8_t>(locatorByte0d & 0x07u);
    const uint8_t senderLocatorType = static_cast<uint8_t>((locatorByte0d >> 4) & 0x07u);
    if (outTargetLocatorType) {
        *outTargetLocatorType = targetLocatorType;
    }
    if (outSenderLocatorType) {
        *outSenderLocatorType = senderLocatorType;
    }

    if (targetLocatorType == 0u || targetLocatorType > 6u ||
        senderLocatorType == 0u || senderLocatorType > 6u) {
        return false;
    }

    const size_t payloadOffset =
        0x12u +
        static_cast<size_t>(kMessageLocatorPayloadOffsetTable[targetLocatorType - 1u]) +
        static_cast<size_t>(kMessageLocatorPayloadOffsetTable[senderLocatorType - 1u]);
    if (payloadOffset >= payloadBytes->size()) {
        return false;
    }

    if (outMessageCodePointer) {
        *outMessageCodePointer = payloadBytes->data() + payloadOffset;
    }
    return true;
}

static bool CMessageConnection_DecodeMessageCodeScaffold(
    const CMessageConnectionReceivedMessageRefScaffold& messageRef,
    uint16_t* outMessageCode,
    bool* outUsedHeaderlessLocatorDecode) {
    if (outMessageCode) {
        *outMessageCode = 0u;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = false;
    }

    const std::vector<uint8_t>* payloadBytes =
        CMessageConnection_MessagePayloadBytesScaffold(messageRef);
    if (!payloadBytes || payloadBytes->empty()) {
        return false;
    }

    const uint8_t* messageCodePointer = nullptr;
    if (!CMessageConnection_ResolveMessageCodePointerScaffold(
            messageRef,
            &messageCodePointer,
            /*outTargetLocatorType=*/nullptr,
            /*outSenderLocatorType=*/nullptr,
            outUsedHeaderlessLocatorDecode)) {
        return false;
    }

    const size_t messageCodeOffset =
        static_cast<size_t>(messageCodePointer - payloadBytes->data());
    const uint8_t firstByte = *messageCodePointer;
    uint16_t messageCode = firstByte;
    if ((firstByte & 0x80u) != 0u) {
        if (messageCodeOffset + 1u >= payloadBytes->size()) {
            return false;
        }
        messageCode = static_cast<uint16_t>(
            ((static_cast<uint16_t>(firstByte) << 8) |
             static_cast<uint16_t>((*payloadBytes)[messageCodeOffset + 1u])) &
            0x7fffu);
    }

    if (outMessageCode) {
        *outMessageCode = messageCode;
    }
    return true;
}

static uint32_t CBaseMarginConnection_DispatchMessageFilterScaffold(
    const CMessageConnectionReceivedMessageRefScaffold& messageRef,
    uint16_t* outDecodedMessageCode,
    bool* outUsedHeaderlessLocatorDecode,
    bool* outHadValidMessageCode) {
    if (outHadValidMessageCode) {
        *outHadValidMessageCode = false;
    }

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    const bool hadValidMessageCode = CMessageConnection_DecodeMessageCodeScaffold(
        messageRef,
        &decodedMessageCode,
        &usedHeaderlessLocatorDecode);
    if (outDecodedMessageCode) {
        *outDecodedMessageCode = decodedMessageCode;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = usedHeaderlessLocatorDecode;
    }
    if (outHadValidMessageCode) {
        *outHadValidMessageCode = hadValidMessageCode;
    }
    if (!hadValidMessageCode) {
        return 0u;
    }

    switch (decodedMessageCode) {
        case 2u:
        case 4u:
        case 5u:
            return 1u;
        default:
            return 0u;
    }
}

// anchor: launcher.exe:0x4489d0
// Source-owned shared_ptr-based mirror of the receive/message-ref swap helper used after
// `0x469930` returns a read-side packet-agenda result.
static void CMessageConnection_AssignReceivedMessageRefScaffold(
    CMessageConnectionReceivedMessageRefScaffold* destination,
    const CMessageConnectionReceivedMessageRefScaffold& source) {
    if (!destination) {
        return;
    }
    *destination = source;
}

// anchor: launcher.exe:0x469980 / embedded helper vtable `0x004baf48 +0x0c`
// Source-owned mirror of the default read helper's concrete effect:
// store the incoming message-ref into the agenda read-output slot at `+0x08`.
static bool CMessageConnection_DefaultAgendaReadPassThroughScaffold(
    CMessageConnectionPacketAgendaScaffold* agenda,
    const CMessageConnectionReceivedMessageRefScaffold& inputMessageRef) {
    if (!agenda || !agenda->hasEmbeddedDefaultReadPassThroughHelper0c) {
        return false;
    }
    agenda->readOutputSlot08 = inputMessageRef;
    agenda->hasReadOutputSlot08 = true;
    return true;
}

// anchor: launcher.exe:0x469930
// Source-owned mirror of the read-side packet-agenda handoff just before leaf dispatch.
// Current bounded source model:
// - a created agenda always has the embedded default read pass-through helper at `+0x0c`
// - source now owns that helper's concrete agenda `+0x08` output-slot effect
// - caller-installed read helpers are still modeled as pass-through-only until helper-side
//   transformation/discard behavior is recovered
static bool CMessageConnection_ApplyReceivePacketAgendaScaffold(
    CMessageConnectionPacketAgendaScaffold* agenda,
    const CMessageConnectionReceivedMessageRefScaffold& inputMessageRef,
    CMessageConnectionReceivedMessageRefScaffold* outputMessageRef,
    bool* outAgendaTouched) {
    if (outAgendaTouched) {
        *outAgendaTouched = false;
    }
    if (!outputMessageRef) {
        return false;
    }

    CMessageConnection_AssignReceivedMessageRefScaffold(outputMessageRef, inputMessageRef);
    if (!agenda || !agenda->created) {
        return true;
    }

    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    if (!CMessageConnection_DefaultAgendaReadPassThroughScaffold(agenda, inputMessageRef)) {
        return false;
    }

    if (agenda->hasReadOutputSlot08) {
        CMessageConnection_AssignReceivedMessageRefScaffold(outputMessageRef, agenda->readOutputSlot08);
        return true;
    }
    return false;
}

}  // namespace

uint32_t CMessageConnection::DispatchCopiedParsedPacketTailScaffold(
    void* workItem,
    const CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    (void)workItem;
    (void)messageRef;
    return 0u;
}

// anchor: launcher.exe:0x4490c0
// string-backed original name: CMessageConnection::OnOperationCompleted
// Current source body now mirrors the smallest static-RE-backed live subset of the type-3 path:
// - dispatch on `workItem+0x04`
// - on type `3`, copy packet-body bytes from the retained-fragment-backed parsed-packet work item
//   via `currentCursor24` / `assembledByteCount28`
// - preserve the original oversized-packet close branch before later dispatch/agenda work
// - newer bounded base-step correction at the post-copy seam:
//   - source now also materializes the nearer local receive/message-ref scaffold that mirrors
//     `0x455cd0/0x455c60` outer-ref + inner-storage construction
//   - copied packet spans are appended into that inner storage before later code reads
//   - headerless packets now keep the original locator-id validity gate on that object before leaf dispatch
// - current bounded leaf correction:
//   - if connection `+0x74` is present, source now also preserves the nearer
//     `0x469930 -> 0x4489d0` read-agenda handoff shape as a pass-through swap before dispatch
//   - auth leaf `0x449a30 -> owner+0x180 / 0x41f250` can now re-enter current-helper slot 5
//     through a local message-ref/base-filter step instead of jumping straight from copied bytes
//   - margin leaf `0x44af20 -> 0x442d00 -> owner+0x184 / 0x41f260` can now re-enter the nearer
//     base-dispatch/current-helper-slot6 path directly from this callback on the handled margin path
// - remaining message-object / agenda work is still incomplete, so the launcher bridge still keeps
//   the later synthetic receive-drain proxy for the paths that are not yet consumed here
uint32_t CMessageConnection::OnOperationCompleted(void* workItem) {
    if (!Engine() || !workItem) {
        return 0u;
    }

    const uint32_t workType = CMessageConnection_WorkItemTypeScaffold(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain) {
        const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* syntheticWorkItem =
            static_cast<const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold*>(workItem);
        return CMessageConnection_HandleSyntheticReceiveDrainProxyScaffold(
            this,
            syntheticWorkItem ? syntheticWorkItem->workPayload : 0u);
    }
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        // Current best startup-path read from `0x4490c0` + wrapper callers `0x449a70/0x44af60`:
        // - base type-2 handling first tries optional completion helper `+0x7c`
        // - on the auth/margin startup path those helpers are null
        // - control therefore falls through into the leaf owner-callback wrapper
        return 0u;
    }
    if (workType != CLTThreadPerClientTCPEngine::kWorkTypeParsedPacket) {
        CMessageConnection_LogUnhandledOperationScaffold(workItem);
        return 1u;
    }

    const CLTTCPConnection_ParsedPacketWorkItemScaffold* parsedPacketWorkItem =
        static_cast<const CLTTCPConnection_ParsedPacketWorkItemScaffold*>(workItem);
    if (parsedPacketWorkItem->assembledByteCount28 > 0x1000u) {
        spdlog::info(
            "CMessageConnection::OnOperationCompleted received illegally large packet payloadBytes={} this={} ownerContext={} remoteHost='{}' -> closing",
            static_cast<unsigned>(parsedPacketWorkItem->assembledByteCount28),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        (void)Close(false);
        lastReceivedPacketBodyBytesScaffold_.clear();
        lastReceivedPacketHeaderlessScaffold_ = !packetizedMessagesEnabledScaffold_;
        return 1u;
    }

    bool hadUnusedBuffers = false;
    CMessageConnectionReceivedMessageRefScaffold copiedMessageRef =
        CMessageConnection_CreateReceivedMessageRefScaffold(!packetizedMessagesEnabledScaffold_);
    const bool copied = CMessageConnection_CopyParsedPacketIntoReceivedMessageRefScaffold(
        parsedPacketWorkItem,
        &copiedMessageRef,
        &hadUnusedBuffers);
    if (!copied) {
        spdlog::info(
            "CMessageConnection::OnOperationCompleted unresolved parsed-packet receive/message-ref copy this={} ownerContext={} payloadBytes={} retainedFragmentCount={} currentCursor={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            static_cast<unsigned>(parsedPacketWorkItem->assembledByteCount28),
            static_cast<unsigned>(parsedPacketWorkItem->retainedFragmentCount0C),
            fmt::ptr(parsedPacketWorkItem->currentCursor24),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        CMessageConnection_LogUnhandledOperationScaffold(workItem);
        return 1u;
    }

    if (hadUnusedBuffers) {
        spdlog::info(
            "CMessageConnection::OnOperationCompleted unused retained buffers remained after receive/message-ref copy this={} ownerContext={} payloadBytes={} retainedFragmentCount={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            static_cast<unsigned>(parsedPacketWorkItem->assembledByteCount28),
            static_cast<unsigned>(parsedPacketWorkItem->retainedFragmentCount0C),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    }

    lastReceivedPacketBodyBytesScaffold_ =
        copiedMessageRef.messageStorage0c ? copiedMessageRef.messageStorage0c->payloadBytesFrom0c
                                          : std::vector<uint8_t>{};
    lastReceivedPacketHeaderlessScaffold_ = (copiedMessageRef.headerless10 != 0u);

    if (lastReceivedPacketHeaderlessScaffold_) {
        uint8_t targetLocatorType = 0u;
        uint8_t senderLocatorType = 0u;
        if (!CMessageConnection_ResolveMessageCodePointerScaffold(
                copiedMessageRef,
                /*outMessageCodePointer=*/nullptr,
                &targetLocatorType,
                &senderLocatorType,
                /*outUsedHeaderlessLocatorDecode=*/nullptr)) {
            spdlog::info(
                "CMessageConnection::OnOperationCompleted discarded copied headerless receive/message-ref because locator ids were invalid payloadBytes={} targetLocatorType={} senderLocatorType={} this={} ownerContext={} remoteHost='{}'",
                static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
                static_cast<unsigned>(targetLocatorType),
                static_cast<unsigned>(senderLocatorType),
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
            return 1u;
        }
    }

    CMessageConnectionReceivedMessageRefScaffold agendaMessageRef = {};
    const CMessageConnectionReceivedMessageRefScaffold* messageRefForDispatch = &copiedMessageRef;
    bool agendaTouched = false;
    if (CMessageConnectionPacketAgendaScaffold* agenda = packetAgendaScaffold_.get();
        agenda && agenda->created) {
        if (!CMessageConnection_ApplyReceivePacketAgendaScaffold(
                agenda,
                copiedMessageRef,
                &agendaMessageRef,
                &agendaTouched)) {
            spdlog::info(
                "CMessageConnection::OnOperationCompleted discarded receive/message-ref through packet-agenda read handoff payloadBytes={} headerless={} this={} ownerContext={} remoteHost='{}'",
                static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
                lastReceivedPacketHeaderlessScaffold_ ? 1u : 0u,
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
            return 1u;
        }
        if (agendaTouched) {
            messageRefForDispatch = &agendaMessageRef;
            if (const std::vector<uint8_t>* agendaPayloadBytes =
                    CMessageConnection_MessagePayloadBytesScaffold(*messageRefForDispatch)) {
                lastReceivedPacketBodyBytesScaffold_ = *agendaPayloadBytes;
            }
            lastReceivedPacketHeaderlessScaffold_ = (messageRefForDispatch->headerless10 != 0u);
            spdlog::debug(
                "CMessageConnection::OnOperationCompleted preserved source-owned packet-agenda read handoff via agenda+0x08 output slot payloadBytes={} configuredReadHelpers={} hasDefaultReadHelper={} this={} ownerContext={} remoteHost='{}'",
                static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
                static_cast<unsigned>(agenda->configuredReadHelperCount),
                agenda->hasEmbeddedDefaultReadPassThroughHelper0c ? 1u : 0u,
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        }
    }

    const uint32_t postCopyDispatchHandled = DispatchCopiedParsedPacketTailScaffold(
        workItem,
        *messageRefForDispatch);
    if (postCopyDispatchHandled != 0u) {
        spdlog::info(
            "CMessageConnection::OnOperationCompleted copied parsed packet body payloadBytes={} headerless={} retainedFragmentCount={} and dispatched it on the post-copy leaf path this={} ownerContext={} remoteHost='{}'",
            static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
            lastReceivedPacketHeaderlessScaffold_ ? 1u : 0u,
            static_cast<unsigned>(parsedPacketWorkItem->retainedFragmentCount0C),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 1u;
    }

    pendingReceivedPacketsScaffold_.push_back(
        CMessageConnectionReceivedPacketScaffold{
            lastReceivedPacketBodyBytesScaffold_,
            lastReceivedPacketHeaderlessScaffold_});
    spdlog::info(
        "CMessageConnection::OnOperationCompleted copied parsed packet body payloadBytes={} headerless={} retainedFragmentCount={} pendingCopiedPackets={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
        lastReceivedPacketHeaderlessScaffold_ ? 1u : 0u,
        static_cast<unsigned>(parsedPacketWorkItem->retainedFragmentCount0C),
        static_cast<unsigned>(pendingReceivedPacketsScaffold_.size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return 1u;
}

const std::vector<uint8_t>& CMessageConnection::LastReceivedPacketBodyBytesScaffold() const {
    return lastReceivedPacketBodyBytesScaffold_;
}

bool CMessageConnection::LastReceivedPacketHeaderlessScaffold() const {
    return lastReceivedPacketHeaderlessScaffold_;
}

bool CMessageConnection::TakeNextReceivedPacketScaffold(
    std::vector<uint8_t>* outPayloadBytes,
    bool* outHeaderless) {
    if (outPayloadBytes) {
        outPayloadBytes->clear();
    }
    if (outHeaderless) {
        *outHeaderless = false;
    }
    if (pendingReceivedPacketsScaffold_.empty()) {
        return false;
    }

    CMessageConnectionReceivedPacketScaffold pendingPacket =
        std::move(pendingReceivedPacketsScaffold_.front());
    pendingReceivedPacketsScaffold_.erase(pendingReceivedPacketsScaffold_.begin());
    if (outPayloadBytes) {
        *outPayloadBytes = std::move(pendingPacket.payloadBytes);
    }
    if (outHeaderless) {
        *outHeaderless = pendingPacket.headerless;
    }
    return true;
}

// UNANCHORED: source-owned payload-span submit wrapper beneath the original envelope-based
// `CMessageConnection::SendPacket` family at `0x448cf0 -> 0x448a00`.
uint32_t CMessageConnection::SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext) {
    if (!Engine() || !packetData || packetByteCount == 0) {
        return 0;
    }

    // Current starter path deliberately routes through the recovered connection-object-based
    // engine surface instead of pretending this is already a faithful packet serializer.
    return Engine()->SendBufferConnectionScaffold(static_cast<CLTTCPConnection*>(this), packetData, packetByteCount, completionContext);
}

// UNANCHORED: source-owned wrapper over base `CLTTCPConnection::Connect` / engine slot 6.
uint32_t CMessageConnection::EnsureConnected() {
    if (!Engine()) {
        spdlog::debug(
            "CMessageConnection::EnsureConnected failed because engine is null this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 0;
    }

    const uint32_t result = CLTTCPConnection::Connect(RemoteEndpoint());
    if (result == 0u) {
        spdlog::debug(
            "CMessageConnection::EnsureConnected connect failed this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    }
    return result;
}

// ============================================================
// VTable 0x004afef0 - auth-side startup leaf wrapper
// ============================================================
// Current best anchored role from `0x41d170`:
// - auth startup allocates `0xa8`
// - runs shared connection ctor setup
// - overwrites vtable to `0x004afef0`
// - stores owner at `+0xa4`
// - configures packet-name callback `0x41ce00`
// - then immediately calls `connection->+0x1c(owner+0x5c)`
// Current source keeps the class name conservative until wider naming cleanup is done.
CAuthStartupConnection::CAuthStartupConnection()
    : CMessageConnection() {}

CAuthStartupConnection::CAuthStartupConnection(CLTThreadPerClientTCPEngine* authEngine)
    : CMessageConnection(authEngine) {}

CAuthStartupConnection::~CAuthStartupConnection() = default;

// anchor: launcher.exe:0x449a30 -> owner vtable `+0x180` / `0x41f250`
uint32_t CAuthStartupConnection::DispatchCopiedParsedPacketTailScaffold(
    void* workItem,
    const CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    (void)workItem;

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(this);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    const std::vector<uint8_t>* payloadBytes =
        CMessageConnection_MessagePayloadBytesScaffold(messageRef);
    if (!context || !mediator || context->isMarginConnection || !payloadBytes || payloadBytes->empty()) {
        return 0u;
    }

    const bool headerless = (messageRef.headerless10 != 0u);
    const uint8_t* messageCodePointer = nullptr;
    const bool resolvedMessageCodePointer = CMessageConnection_ResolveMessageCodePointerScaffold(
        messageRef,
        &messageCodePointer,
        /*outTargetLocatorType=*/nullptr,
        /*outSenderLocatorType=*/nullptr,
        /*outUsedHeaderlessLocatorDecode=*/nullptr);
    const uint8_t rawCode =
        (resolvedMessageCodePointer && messageCodePointer) ? *messageCodePointer : (*payloadBytes)[0];

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    bool hadValidMessageCode = false;
    const uint32_t baseFilterHandled = CBaseMarginConnection_DispatchMessageFilterScaffold(
        messageRef,
        &decodedMessageCode,
        &usedHeaderlessLocatorDecode,
        &hadValidMessageCode);
    if (baseFilterHandled != 0u) {
        spdlog::info(
            "CAuthStartupConnection::DispatchCopiedParsedPacketTailScaffold source-owned local 0x449a30/0x442d00 filter consumed decodedMessageCode={} rawCode=0x{:02x} headerless={} locatorDecoded={} this={} ownerContext={} remoteHost='{}'",
            static_cast<unsigned>(decodedMessageCode),
            static_cast<unsigned>(rawCode),
            headerless ? 1u : 0u,
            usedHeaderlessLocatorDecode ? 1u : 0u,
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 1u;
    }

    const uint32_t handled = mediator->StageAuthPacketBytesAndDispatchCurrentHelperScaffold(
        payloadBytes->data(),
        payloadBytes->size(),
        /*workItem=*/nullptr);
    spdlog::info(
        "CAuthStartupConnection::DispatchCopiedParsedPacketTailScaffold rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(decodedMessageCode),
        hadValidMessageCode ? 1u : 0u,
        headerless ? 1u : 0u,
        usedHeaderlessLocatorDecode ? 1u : 0u,
        static_cast<unsigned>(payloadBytes->size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(mediator->CurrentState()),
        handled,
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return handled;
}

// anchor: launcher.exe:0x449a70
uint32_t CAuthStartupConnection::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const uint32_t baseResult = CMessageConnection::OnOperationCompleted(workItem);
    if (baseResult != 0u) {
        return 1u;
    }

    const uint32_t workType = CMessageConnection_WorkItemTypeScaffold(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* statusWorkItem =
            static_cast<const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold*>(workItem);
        return CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold(
            this,
            statusWorkItem ? statusWorkItem->workPayload : 0u,
            /*isMarginConnection=*/false,
            "+0x17c");
    }

    spdlog::debug(
        "CAuthStartupConnection::OnOperationCompleted unresolved owner+0x17c fallback workItem={} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(workItem),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return 0u;
}

// ============================================================
// VTable 0x004aff38 - CMarginConnection
// ============================================================
// Later leaf on top of:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection-family base surface
//     └── CBaseMarginConnection (0x004b64a8)
//         └── CMarginConnection (0x004aff38)

// UNANCHORED: source-owned narrow leaf ctor over the `0x41cf80 -> 0x448b40` family.
CMarginConnection::CMarginConnection()
    : CMessageConnection() {}

// UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
CMarginConnection::CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine)
    : CMessageConnection(marginEngine) {}

// UNANCHORED: source-owned default destructor; original cleanup runs through `0x41ce80` after
// restoring the shared base-margin vtable.
CMarginConnection::~CMarginConnection() = default;

// UNANCHORED: source-owned compatibility pass-through over the recovered base `+0x10` engine field.
void CMarginConnection::SetMarginEngine(CLTThreadPerClientTCPEngine* marginEngine) {
    CMessageConnection::SetEngine(marginEngine);
}

// UNANCHORED: source-owned compatibility accessor over the recovered base `+0x10` engine field.
CLTThreadPerClientTCPEngine* CMarginConnection::MarginEngine() const {
    return CMessageConnection::Engine();
}

namespace {

static void** CMarginConnection_LocalCompletionWorkItemVtableScaffold() {
    // anchor: launcher.exe:0x464870 / data `0x004baa00`
    // The current source does not need the concrete local dtor body from that vtable family, but
    // keep a stable non-null vtable pointer in the recovered 12-byte stack work-item shape.
    static void* vtable[1] = {nullptr};
    return vtable;
}

}  // namespace

// anchor: launcher.exe:0x441850
void CMarginConnection::SetMessageCode4SuccessFlag84Scaffold(bool value) {
    messageCode4SuccessFlag84Scaffold_ = value;
}

bool CMarginConnection::MessageCode4SuccessFlag84Scaffold() const {
    return messageCode4SuccessFlag84Scaffold_;
}

// anchor: launcher.exe:0x441850
uint32_t CMarginConnection::DispatchMessageCode4LocalCompletionWorkItemScaffold(uint32_t workPayloadStatus) {
    CMarginConnectionLocalCompletionWorkItemScaffold workItem = {};
    workItem.header.vtable = CMarginConnection_LocalCompletionWorkItemVtableScaffold();
    workItem.header.workType = 0x0bu;
    workItem.workPayload = workPayloadStatus;

    CMessageConnection* selfAsMessageConnection = this;
    const uint32_t handled = selfAsMessageConnection->OnOperationCompleted(&workItem);
    spdlog::info(
        "CMarginConnection::DispatchMessageCode4LocalCompletionWorkItemScaffold synthesized local type0x0b workItem status=0x{:08x} handled={} this={} ownerContext={} currentState={} remoteHost='{}'",
        workPayloadStatus,
        handled,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(CMessageConnection_LoginMediatorContextScaffold(this)
                     ? CMessageConnection_LoginMediatorContextScaffold(this)->mediator
                           ? CMessageConnection_LoginMediatorContextScaffold(this)->mediator->CurrentState()
                           : nullptr
                     : nullptr),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return handled;
}

// anchor: launcher.exe:0x41ce80 -> connection `+0x98`
bool CMarginConnection::StoreBootstrapReplyCopy98Scaffold(const void* bytes, size_t byteCount) {
    if (!bytes || byteCount != bootstrapReplyCopy98Scaffold_.size()) {
        return false;
    }

    std::copy_n(
        static_cast<const uint8_t*>(bytes),
        bootstrapReplyCopy98Scaffold_.size(),
        bootstrapReplyCopy98Scaffold_.begin());
    hasBootstrapReplyCopy98Scaffold_ = true;
    spdlog::info(
        "CMarginConnection::StoreBootstrapReplyCopy98Scaffold stored reply-copy block bytes=0x{:03x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(bootstrapReplyCopy98Scaffold_.size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return true;
}

// anchor: launcher.exe:0x41f30
uint32_t CMarginConnection::SendStoredBootstrapReplyCopy98Scaffold() {
    if (!hasBootstrapReplyCopy98Scaffold_) {
        return 0u;
    }

    CMessageConnectionEnvelopeScaffold envelope = CMessageConnection::BuildPacketBuilderEnvelopeScaffold(false);
    if (!envelope.sharedMessage) {
        return 0u;
    }

    envelope.sharedMessage->ResetPayloadByteCountScaffold(
        static_cast<uint16_t>(3u + bootstrapReplyCopy98Scaffold_.size()));
    uint8_t* payloadBytes = envelope.sharedMessage->PayloadBaseScaffold();
    if (!payloadBytes) {
        return 0u;
    }

    payloadBytes[0] = 0x01u;
    payloadBytes[1] = 0u;
    payloadBytes[2] = 0u;
    std::copy(
        bootstrapReplyCopy98Scaffold_.begin(),
        bootstrapReplyCopy98Scaffold_.end(),
        payloadBytes + 3u);

    const uint32_t sendResult = SendPacketEnvelopeScaffold(envelope);
    spdlog::info(
        "CMarginConnection::SendStoredBootstrapReplyCopy98Scaffold sent rawType1PrefixPlusReplyCopy payloadBytes=0x{:03x} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(3u + bootstrapReplyCopy98Scaffold_.size()),
        static_cast<unsigned>(sendResult),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return sendResult;
}

// anchor: launcher.exe:0x442d00 code-5 branch -> connection `+0x85 .. +0x94`
void CMarginConnection::SetMessageCode5SeedBytes85Scaffold(const std::array<uint8_t, 16>& value) {
    messageCode5SeedBytes85Scaffold_ = value;
    hasMessageCode5SeedBytes85Scaffold_ = true;
}

bool CMarginConnection::CopyMessageCode5SeedBytes85Scaffold(std::array<uint8_t, 16>* outValue) const {
    if (!outValue || !hasMessageCode5SeedBytes85Scaffold_) {
        return false;
    }

    *outValue = messageCode5SeedBytes85Scaffold_;
    return true;
}

const uint8_t* CMarginConnection::MessageCode5SeedBytes85PointerScaffold() const {
    return hasMessageCode5SeedBytes85Scaffold_ ? messageCode5SeedBytes85Scaffold_.data() : nullptr;
}

// anchor: launcher.exe:0x44af20 -> 0x442d00 -> owner vtable `+0x184` / `0x41f260`
uint32_t CMarginConnection::DispatchCopiedParsedPacketTailScaffold(
    void* workItem,
    const CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    (void)workItem;

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(this);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    const std::vector<uint8_t>* payloadBytes =
        CMessageConnection_MessagePayloadBytesScaffold(messageRef);
    if (!context || !mediator || !context->isMarginConnection || !payloadBytes || payloadBytes->empty()) {
        return 0u;
    }

    const bool headerless = (messageRef.headerless10 != 0u);
    const uint8_t* messageCodePointer = nullptr;
    const bool resolvedMessageCodePointer = CMessageConnection_ResolveMessageCodePointerScaffold(
        messageRef,
        &messageCodePointer,
        /*outTargetLocatorType=*/nullptr,
        /*outSenderLocatorType=*/nullptr,
        /*outUsedHeaderlessLocatorDecode=*/nullptr);

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    bool hadValidMessageCode = false;
    (void)CBaseMarginConnection_DispatchMessageFilterScaffold(
        messageRef,
        &decodedMessageCode,
        &usedHeaderlessLocatorDecode,
        &hadValidMessageCode);

    const uint8_t rawCode =
        (resolvedMessageCodePointer && messageCodePointer) ? *messageCodePointer : (*payloadBytes)[0];
    const std::string remoteHostForLog =
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName();

    const auto logConsumedMarginBranch =
        [this, mediator, rawCode, headerless, usedHeaderlessLocatorDecode, &remoteHostForLog](
            uint16_t branchDecodedMessageCode,
            uint32_t handled,
            const char* extraFieldLabel,
            uint32_t extraFieldValue) {
            if (extraFieldLabel && extraFieldLabel[0] != '\0') {
                spdlog::info(
                    "CMarginConnection::DispatchCopiedParsedPacketTailScaffold source-owned local code{} branch rawCode=0x{:02x} headerless={} locatorDecoded={} {}={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
                    static_cast<unsigned>(branchDecodedMessageCode),
                    static_cast<unsigned>(rawCode),
                    headerless ? 1u : 0u,
                    usedHeaderlessLocatorDecode ? 1u : 0u,
                    extraFieldLabel,
                    extraFieldValue,
                    fmt::ptr(this),
                    fmt::ptr(OwnerContext()),
                    fmt::ptr(mediator->CurrentState()),
                    handled,
                    remoteHostForLog);
            } else {
                spdlog::info(
                    "CMarginConnection::DispatchCopiedParsedPacketTailScaffold source-owned local code{} branch rawCode=0x{:02x} headerless={} locatorDecoded={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
                    static_cast<unsigned>(branchDecodedMessageCode),
                    static_cast<unsigned>(rawCode),
                    headerless ? 1u : 0u,
                    usedHeaderlessLocatorDecode ? 1u : 0u,
                    fmt::ptr(this),
                    fmt::ptr(OwnerContext()),
                    fmt::ptr(mediator->CurrentState()),
                    handled,
                    remoteHostForLog);
            }
        };

    if (hadValidMessageCode && decodedMessageCode == 2u) {
        const uint32_t handledCode2 =
            mediator->HandleMarginConsumedCode2AtConnectionSeamScaffold(
                payloadBytes->data(),
                payloadBytes->size(),
                /*transportEncrypted=*/false);
        logConsumedMarginBranch(decodedMessageCode, handledCode2, nullptr, 0u);
        if (handledCode2 != 0u) {
            return handledCode2;
        }
    }

    if (hadValidMessageCode && decodedMessageCode == 4u) {
        const uint32_t handledCode4 =
            mediator->HandleMarginConsumedCode4AtConnectionSeamScaffold(
                payloadBytes->data(),
                payloadBytes->size(),
                /*transportEncrypted=*/false);
        logConsumedMarginBranch(
            decodedMessageCode,
            handledCode4,
            "connectionByte84",
            MessageCode4SuccessFlag84Scaffold() ? 1u : 0u);
        if (handledCode4 != 0u) {
            return handledCode4;
        }
    }

    if (hadValidMessageCode && decodedMessageCode == 5u && payloadBytes->size() >= 17u) {
        std::array<uint8_t, 16> seedBytes85 = {};
        std::copy_n(payloadBytes->data() + 1u, seedBytes85.size(), seedBytes85.begin());
        SetMessageCode5SeedBytes85Scaffold(seedBytes85);
        spdlog::info(
            "CMarginConnection::DispatchCopiedParsedPacketTailScaffold source-owned local code5 branch rawCode=0x{:02x} headerless={} locatorDecoded={} storedConnectionSeed85_94=1 firstDword=0x{:08x} this={} ownerContext={} currentState={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            headerless ? 1u : 0u,
            usedHeaderlessLocatorDecode ? 1u : 0u,
            static_cast<unsigned>(
                static_cast<uint32_t>(seedBytes85[0]) |
                (static_cast<uint32_t>(seedBytes85[1]) << 8u) |
                (static_cast<uint32_t>(seedBytes85[2]) << 16u) |
                (static_cast<uint32_t>(seedBytes85[3]) << 24u)),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator->CurrentState()),
            remoteHostForLog);
        return 1u;
    }

    const uint32_t handled = mediator->StageMarginPacketBytesAndDispatchCurrentHelperScaffold(
        payloadBytes->data(),
        payloadBytes->size(),
        /*workItem=*/nullptr);
    spdlog::info(
        "CMarginConnection::DispatchCopiedParsedPacketTailScaffold rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(decodedMessageCode),
        hadValidMessageCode ? 1u : 0u,
        headerless ? 1u : 0u,
        usedHeaderlessLocatorDecode ? 1u : 0u,
        static_cast<unsigned>(payloadBytes->size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(mediator->CurrentState()),
        handled,
        remoteHostForLog);
    return handled;
}

// anchor: launcher.exe:0x44af60
// Later leaf override on top of the base `CMessageConnection::OnOperationCompleted` family.
// Current source body keeps only the proven base-first order explicit; the owner `+0x188` fallback
// chain remains unmodeled in this class scaffold.
uint32_t CMarginConnection::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const uint32_t baseResult = CMessageConnection::OnOperationCompleted(workItem);
    if (baseResult != 0u) {
        return 1u;
    }

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(this);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;

    const uint32_t workType = CMessageConnection_WorkItemTypeScaffold(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* statusWorkItem =
            static_cast<const mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold*>(workItem);
        const uint32_t workPayload = statusWorkItem ? statusWorkItem->workPayload : 0u;

        // Bounded fidelity correction for the active state4 existing-character margin path:
        // - original margin type-2 completion re-enters the current helper slot-2 chain
        // - state4 slot-2 then restores the cached upstream helper and lets that continuation
        //   progress through `0x41b450` semantics
        // - the active sender gates also require owner `+0x1c` state `== 2`, so keep the
        //   connect-status success promotion immediately before that slot-2 re-entry
        if (workPayload == 0u && State() == LTTCPEngineConnectionState::kConnectActive) {
            SetState(LTTCPEngineConnectionState::kUdpMonitorActive);
            spdlog::info(
                "CMarginConnection::OnOperationCompleted promoted margin connection to ready state=2 on type-2 zero-status completion this={} ownerContext={} remoteHost='{}'",
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        }

        if (context && mediator && context->isMarginConnection) {
            const uint32_t handled = mediator->DispatchCurrentHelperSecondaryGateScaffold(workItem);
            spdlog::info(
                "CMarginConnection::OnOperationCompleted routed type-2 connect-status through current helper slot2 payload=0x{:08x} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
                workPayload,
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                fmt::ptr(mediator->CurrentState()),
                handled,
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
            return handled;
        }

        return CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold(
            this,
            workPayload,
            /*isMarginConnection=*/true,
            "+0x188");
    }

    if (context && mediator && context->isMarginConnection) {
        const uint32_t handled =
            mediator->HandleMarginConnectionCompletionFallbackScaffold(this, workItem);
        if (handled != 0u) {
            return 1u;
        }
    }

    spdlog::debug(
        "CMarginConnection::OnOperationCompleted unresolved owner+0x188 fallback workItem={} workType=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(workItem),
        static_cast<unsigned>(workType),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return 0u;
}

// anchor: launcher.exe:0x44af20
// Later leaf dispatch override on top of the unmodeled `CBaseMarginConnection` dispatch family.
// Current source note:
// - the staged-payload equivalent of this later destination is now owned one step earlier through
//   `DispatchCopiedParsedPacketTailScaffold(...)`
// - this literal one-argument message-ref form still remains unmodeled until source reconstructs
//   the original message object created inside `0x4490c0`
uint32_t CMarginConnection::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    spdlog::debug(
        "CMarginConnection::DispatchMessage unresolved CBaseMarginConnection/owner+0x184 chain messageRef={} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(messageRef),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return 0u;
}

}  // namespace mxo::liblttcp
