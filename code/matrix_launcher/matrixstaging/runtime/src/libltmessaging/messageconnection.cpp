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

namespace {

// anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
static uint32_t __thiscall CMessageConnectionRefCounted_AddRef(void* self) {
    if (!self) {
        return 0u;
    }
    return static_cast<uint32_t>(InterlockedIncrement(reinterpret_cast<LONG*>(static_cast<uint8_t*>(self) + 4)));
}

// anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
static uint32_t __thiscall CMessageConnectionRefCounted_Release(void* self) {
    if (!self) {
        return 0u;
    }

    LONG* const refCount = reinterpret_cast<LONG*>(static_cast<uint8_t*>(self) + 4);
    LONG current = *refCount;
    if (current <= 0) {
        return 0u;
    }
    current = InterlockedDecrement(refCount);
    return static_cast<uint32_t>(current > 0 ? current : 0);
}

// anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
static void __thiscall CMessageConnectionRefCounted_SetRefCountFromPtr(void* self, LONG* refCountSource) {
    if (!self || !refCountSource) {
        return;
    }
    InterlockedExchange(
        reinterpret_cast<LONG*>(static_cast<uint8_t*>(self) + 4),
        *refCountSource);
}

// UNANCHORED: source-owned non-deleting dtor stand-in for the inner message-storage object.
static void* __thiscall CMessageConnectionMessageStorage_Dtor(
    CMessageConnectionMessageScaffold* self,
    uint8_t /*deleteFlag*/) {
    return self;
}

// UNANCHORED: source-owned non-deleting dtor stand-in for the outer receive/message-ref object.
static void* __thiscall CMessageConnectionReceivedMessageRef_Dtor(
    CMessageConnectionReceivedMessageRefScaffold* self,
    uint8_t /*deleteFlag*/) {
    return self;
}

// anchor: launcher.exe:0x4557b0 / vtable `0x004ba23c +0x18`
static void __thiscall CMessageConnectionReceivedMessageRef_GrowPayload(
    CMessageConnectionReceivedMessageRefScaffold* self,
    uint32_t additionalByteCount,
    uint32_t /*unused*/) {
    if (!self || !self->messageStorage0c) {
        return;
    }

    (void)self->messageStorage0c->GrowPayloadByteCountScaffold(
        static_cast<uint16_t>(std::min<uint32_t>(
            additionalByteCount,
            CMessageConnectionMessageScaffold::kMaxPayloadByteCount)));
}

static void* g_CMessageConnectionMessageStorageVtable[] = {
    reinterpret_cast<void*>(CMessageConnectionMessageStorage_Dtor),
    reinterpret_cast<void*>(CMessageConnectionRefCounted_AddRef),
    reinterpret_cast<void*>(CMessageConnectionRefCounted_Release),
    nullptr,
    nullptr,
    reinterpret_cast<void*>(CMessageConnectionRefCounted_SetRefCountFromPtr),
};

static void* g_CMessageConnectionReceivedMessageRefVtable[] = {
    reinterpret_cast<void*>(CMessageConnectionReceivedMessageRef_Dtor),
    reinterpret_cast<void*>(CMessageConnectionRefCounted_AddRef),
    reinterpret_cast<void*>(CMessageConnectionRefCounted_Release),
    nullptr,
    nullptr,
    reinterpret_cast<void*>(CMessageConnectionRefCounted_SetRefCountFromPtr),
    reinterpret_cast<void*>(CMessageConnectionReceivedMessageRef_GrowPayload),
};

}  // namespace

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
    // Current tighter source mirror now keeps the raw first `0x100c` bytes aligned with the
    // recovered inner object instead of inventing a vector-first storage surrogate.
    vtable00 = g_CMessageConnectionMessageStorageVtable;
    referenceCount04 = 1;
    reservedBytes08 = kBuilderReservedBytes08;
    payloadLengthHigh0a = 0u;
    payloadLengthLow0b = 0u;
    std::fill(payloadBytes0c.begin(), payloadBytes0c.end(), 0u);
}

void CMessageConnectionMessageScaffold::ResetPayloadByteCountScaffold(uint16_t payloadByteCount) {
    const uint16_t oldByteCount = PayloadByteCountScaffold();
    const uint16_t clampedByteCount = std::min<uint16_t>(payloadByteCount, kMaxPayloadByteCount);
    if (clampedByteCount < oldByteCount) {
        std::fill(
            payloadBytes0c.begin() + clampedByteCount,
            payloadBytes0c.begin() + oldByteCount,
            0u);
    } else if (clampedByteCount > oldByteCount) {
        std::fill(
            payloadBytes0c.begin() + oldByteCount,
            payloadBytes0c.begin() + clampedByteCount,
            0u);
    }
    payloadLengthLow0b = static_cast<uint8_t>(clampedByteCount & 0xffu);
    payloadLengthHigh0a =
        (clampedByteCount > 0x7fu)
            ? static_cast<uint8_t>(0x80u | ((clampedByteCount >> 8u) & 0x7fu))
            : 0u;
}

uint16_t CMessageConnectionMessageScaffold::GrowPayloadByteCountScaffold(uint16_t additionalByteCount) {
    // anchor: launcher.exe:0x4557b0
    const uint32_t oldByteCount = PayloadByteCountScaffold();
    const uint32_t requestedByteCount = oldByteCount + static_cast<uint32_t>(additionalByteCount);
    if (requestedByteCount > kMaxPayloadByteCount) {
        return static_cast<uint16_t>(oldByteCount);
    }

    std::fill(
        payloadBytes0c.begin() + oldByteCount,
        payloadBytes0c.begin() + requestedByteCount,
        0u);
    const uint16_t newByteCount = static_cast<uint16_t>(requestedByteCount);
    payloadLengthLow0b = static_cast<uint8_t>(newByteCount & 0xffu);
    payloadLengthHigh0a =
        (newByteCount > 0x7fu)
            ? static_cast<uint8_t>(0x80u | ((newByteCount >> 8u) & 0x7fu))
            : 0u;
    return newByteCount;
}

uint16_t CMessageConnectionMessageScaffold::PayloadByteCountScaffold() const {
    const uint16_t encodedPayloadByteCount =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(payloadLengthHigh0a & 0x7fu) << 8u) |
            static_cast<uint16_t>(payloadLengthLow0b));
    return std::min<uint16_t>(encodedPayloadByteCount, kMaxPayloadByteCount);
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
    return payloadBytes0c.data();
}

const uint8_t* CMessageConnectionMessageScaffold::PayloadBaseScaffold() const {
    return payloadBytes0c.data();
}

void CMessageConnectionReceivedMessageRefScaffold::ResetForPacketBuilderScaffold(
    bool headerless,
    uint32_t messageContext14) {
    // anchor: launcher.exe:0x455cd0 / 0x455c60
    vtable00 = g_CMessageConnectionReceivedMessageRefVtable;
    referenceCount04 = 1;
    field08 = 0u;
    messageStorage0c = nullptr;
    headerless10 = headerless ? 1u : 0u;
    padding11_13[0] = 0u;
    padding11_13[1] = 0u;
    padding11_13[2] = 0u;
    this->messageContext14 = messageContext14;
    field18 = 0u;
    field1c = 0u;
    field20 = 0u;
    ownedMessageStorageScaffold24 = {};
    ownedMessageStorageScaffold24.ResetForPacketBuilderScaffold();
    messageStorage0c = &ownedMessageStorageScaffold24;
}

void CStreamPacketEncryptionHelperBaseScaffold::ForwardToNextHelper(
    void* opaqueMessageRef) {
    if (nextHelper04) {
        nextHelper04->HandleOpaqueMessageRef(opaqueMessageRef);
    }
}

void CStreamPacketEncryptionModuleReadHelperScaffold::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    ForwardToNextHelper(opaqueMessageRef);
}

void CStreamPacketEncryptionModuleReadHelperScaffold::ResetForOwner(
    CStreamPacketEncryptionModuleScaffold* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    collectionControl0c = 0u;
    collectionBegin10 = nullptr;
    collectionEnd14 = nullptr;
}

void CStreamPacketEncryptionModuleWriteHelperScaffold::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    ForwardToNextHelper(opaqueMessageRef);
}

void CStreamPacketEncryptionModuleWriteHelperScaffold::ResetForOwner(
    CStreamPacketEncryptionModuleScaffold* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    embeddedTransformAdapter0c = nullptr;
    embeddedTransformAdapterMeta10 = nullptr;
}

void CStreamPacketEncryptionAgendaHelperScaffold::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    if (outputSlotAddress10) {
        *outputSlotAddress10 = opaqueMessageRef;
    }
}

void CStreamPacketEncryptionAgendaHelperScaffold::ResetForAgenda(
    const char* helperLabel,
    void** outputSlotAddress,
    CStreamPacketEncryptionHelperBaseScaffold** downstreamHelperSlot) {
    nextHelper04 = nullptr;
    field04 = 0u;
    field08 = 0u;
    helperLabel0c = helperLabel;
    outputSlotAddress10 = outputSlotAddress;
    downstreamHelperSlot14 = downstreamHelperSlot;
}

void CStreamPacketEncryptionModuleScaffold::ResetForMarginConnectionSeed(
    const std::array<uint8_t, 16>& seedBytes85) {
    readHelper04 = nullptr;
    writeHelper08 = nullptr;
    nextConfiguredModule0c = nullptr;
    configuredAgendaIdentity10 = nullptr;
    ownedReadHelperScaffold14.ResetForOwner(this);
    ownedWriteHelperScaffold2c.ResetForOwner(this);
    readHelper04 = &ownedReadHelperScaffold14;
    writeHelper08 = &ownedWriteHelperScaffold2c;
    associatedSeedBytes40 = seedBytes85;
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

void CMessageConnection::ConfigurePacketAgendaScaffold(
    CStreamPacketEncryptionModuleScaffold* streamPacketEncryptionModule) {
    if (!packetAgendaScaffold_) {
        packetAgendaScaffold_ = std::make_unique<CMessageConnectionPacketAgendaScaffold>();
    }
    if (!packetAgendaScaffold_) {
        return;
    }

    CMessageConnectionPacketAgendaScaffold& agenda = *packetAgendaScaffold_;
    agenda.configuredModuleList04 = nullptr;
    agenda.readOutputSlot08 = nullptr;
    agenda.embeddedReadHelper0c.ResetForAgenda(
        "Agenda read helper",
        reinterpret_cast<void**>(&agenda.readOutputSlot08),
        &agenda.writeHelperChainHead44);
    agenda.writeOutputSlot24 = nullptr;
    agenda.embeddedWriteHelper28.ResetForAgenda(
        "Agenda write helper",
        reinterpret_cast<void**>(&agenda.writeOutputSlot24),
        nullptr);
    agenda.readHelperChainHead40 = &agenda.embeddedReadHelper0c;
    agenda.writeHelperChainHead44 = nullptr;
    agenda.writeHelperChainTail48 = nullptr;
    agenda.configuredModuleCount4c = 0u;
    agenda.reserved4e = 0u;
    agenda.created = true;
    agenda.configuredStreamPacketEncryptionModule = streamPacketEncryptionModule;

    // anchor: launcher.exe:0x448980 -> 0x469740
    // Current tighter install read from `0x469740`:
    // - agenda starts with embedded/default read helper at `+0x0c`
    // - module `+0x04` becomes the read-chain head and links onward to that embedded helper
    // - module `+0x08` becomes the write-chain head/tail and links onward to the embedded write
    //   helper at `+0x28`
    // - module `+0x0c/+0x10` are also updated as agenda-owned bookkeeping
    if (!streamPacketEncryptionModule) {
        return;
    }

    streamPacketEncryptionModule->nextConfiguredModule0c = agenda.configuredModuleList04;
    streamPacketEncryptionModule->configuredAgendaIdentity10 = &agenda;
    agenda.configuredModuleList04 = streamPacketEncryptionModule;

    if (streamPacketEncryptionModule->readHelper04) {
        streamPacketEncryptionModule->readHelper04->nextHelper04 =
            agenda.readHelperChainHead40;
        agenda.readHelperChainHead40 = streamPacketEncryptionModule->readHelper04;
    }

    if (streamPacketEncryptionModule->writeHelper08) {
        streamPacketEncryptionModule->writeHelper08->nextHelper04 =
            &agenda.embeddedWriteHelper28;
        agenda.writeHelperChainHead44 = streamPacketEncryptionModule->writeHelper08;
        agenda.writeHelperChainTail48 = streamPacketEncryptionModule->writeHelper08;
    }

    agenda.configuredModuleCount4c = 1u;
}

const CMessageConnectionPacketAgendaScaffold* CMessageConnection::PacketAgendaScaffold() const {
    return packetAgendaScaffold_.get();
}

static void CMessageConnection_ApplySendMessageRefMutationsScaffold(
    CMessageConnectionMessageRefScaffold* messageRef) {
    if (!messageRef || messageRef->headerless10 == 0u || !messageRef->messageStorage0c) {
        return;
    }

    // anchor: launcher.exe:0x448cf0
    // The send-mode branch tests inner `+0x12` (`payload + 0x06`) and, only when that dword is
    // still zero, writes:
    // - inner `+0x12 .. +0x15` = 00 00 00 00
    // - inner `+0x16` = 0xff
    // - inner `+0x17` = 0xff
    CMessageConnectionMessageScaffold& messageStorage = *messageRef->messageStorage0c;
    uint8_t* const payloadBase = messageStorage.PayloadBaseScaffold();
    if (!payloadBase) {
        return;
    }

    const uint32_t locatorDword06 =
        static_cast<uint32_t>(payloadBase[6]) |
        (static_cast<uint32_t>(payloadBase[7]) << 8u) |
        (static_cast<uint32_t>(payloadBase[8]) << 16u) |
        (static_cast<uint32_t>(payloadBase[9]) << 24u);
    if (locatorDword06 != 0u) {
        return;
    }

    std::fill_n(payloadBase + 6u, 4u, 0u);
    payloadBase[10] = 0xffu;
    payloadBase[11] = 0xffu;
}

static void CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBitScaffold(
    CMessageConnectionMessageRefScaffold* messageRef) {
    if (!messageRef || messageRef->headerless10 == 0u || !messageRef->messageStorage0c) {
        return;
    }

    // anchor: launcher.exe:0x448cf0
    // After either submit or agenda discard, original code clears the first payload byte high bit
    // on the original input message-ref's inner storage at `inner + 0x0c`.
    uint8_t* const payloadBase = messageRef->messageStorage0c->PayloadBaseScaffold();
    if (!payloadBase) {
        return;
    }
    payloadBase[0] &= 0x7fu;
}

// anchor: launcher.exe:0x41cf30
uint32_t CMessageConnection::ForwardPacketBuilderEnvelopeToSendPacketScaffold(
    CMessageConnectionPacketBuilderEnvelopeScaffold& envelope) {
    if (!envelope.messageRef08) {
        return 0u;
    }
    return SendPacketMessageRefScaffold(*envelope.messageRef08);
}

CMessageConnectionMessageRefScaffold* CMessageConnection::ApplySendPacketAgendaScaffold(
    CMessageConnectionMessageRefScaffold& inputMessageRef,
    bool* outAgendaTouched) {
    if (outAgendaTouched) {
        *outAgendaTouched = false;
    }

    // `0x448cf0` consults connection `+0x74` and may discard the packet before submit.
    // Current bounded source model preserves the nearer `0x469950` handoff shape:
    // - no agenda / no active write helper (`+0x44 == 0`) => keep the original message-ref pointer
    // - active write helper => return the agenda write-output slot at `+0x24`
    // Current source still lacks helper-side transformation/discard, so the concrete helper
    // implementations currently forward the same pointer through the virtual chain and let the
    // embedded agenda helper populate output slot `+0x24`.
    CMessageConnectionPacketAgendaScaffold* agenda = packetAgendaScaffold_.get();
    if (!agenda || !agenda->created) {
        return &inputMessageRef;
    }

    agenda->writeOutputSlot24 = nullptr;
    if (agenda->writeHelperChainHead44 == nullptr) {
        return &inputMessageRef;
    }

    agenda->writeHelperChainHead44->HandleOpaqueMessageRef(&inputMessageRef);
    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    return agenda->writeOutputSlot24
        ? agenda->writeOutputSlot24
        : &inputMessageRef;
}

// anchor: launcher.exe:0x448a00
// Narrow source-owned mirror of the lower submit helper beneath the original message-ref-based
// `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SubmitMessageRefBytesScaffold(
    const CMessageConnectionMessageRefScaffold& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return 0u;
    }

    const CMessageConnectionMessageScaffold& messageStorage = *messageRef.messageStorage0c;
    const uint16_t payloadByteCount = messageStorage.PayloadByteCountScaffold();
    const uint8_t* const payloadBase = messageStorage.PayloadBaseScaffold();
    if (!payloadBase || payloadByteCount == 0u) {
        return 0u;
    }

    const uint8_t frameByte0a = messageStorage.payloadLengthHigh0a;
    const size_t pointerOffsetFrom0a = ((frameByte0a >> 7) == 0u) ? 1u : 0u;
    const uint32_t submittedByteCount =
        static_cast<uint32_t>(payloadByteCount) + ((payloadByteCount > 0x7fu) ? 2u : 1u);
    const uint8_t* const submittedBytes = payloadBase - 2u + pointerOffsetFrom0a;

    spdlog::info(
        "CMessageConnection::SubmitMessageRefBytesScaffold reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} submitOffset={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(messageStorage.reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<unsigned>(pointerOffsetFrom0a),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return Engine()->SendBufferConnectionScaffold(
        static_cast<CLTTCPConnection*>(this),
        submittedBytes,
        submittedByteCount,
        nullptr);
}

// anchor: launcher.exe:0x448cf0
// Narrow source-owned mirror of the message-ref-based `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SendPacketMessageRefScaffold(
    CMessageConnectionMessageRefScaffold& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return 0u;
    }

    CMessageConnection_ApplySendMessageRefMutationsScaffold(&messageRef);

    bool agendaTouched = false;
    CMessageConnectionMessageRefScaffold* const messageRefForSubmit =
        ApplySendPacketAgendaScaffold(messageRef, &agendaTouched);
    if (!messageRefForSubmit || !messageRefForSubmit->messageStorage0c) {
        spdlog::info(
            "CMessageConnection::SendPacketMessageRefScaffold discarded packet at packet-agenda write handoff this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBitScaffold(&messageRef);
        return 0u;
    }

    const CMessageConnectionMessageScaffold& messageStorage = *messageRefForSubmit->messageStorage0c;
    const uint16_t payloadByteCount = messageStorage.PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage.PayloadBaseScaffold();
    const uint8_t rawOpcode = (payloadBytes && payloadByteCount != 0u) ? payloadBytes[0] : 0u;
    const uint32_t submittedByteCount =
        static_cast<uint32_t>(payloadByteCount) + ((payloadByteCount > 0x7fu) ? 2u : 1u);
    const CMessageConnectionPacketAgendaScaffold* agenda = PacketAgendaScaffold();
    spdlog::info(
        "CMessageConnection::SendPacketMessageRefScaffold sendMode10={} rawOpcode=0x{:02x} reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} packetNameCallback=0x{:08x} packetNameFamily={} packetizedEnabled={} agendaCreated={} agendaModuleCount={} agendaHasReadHead={} agendaHasWriteHead={} agendaWriteTouched={} agendaWriteOutputSlot24={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(messageRefForSubmit->headerless10),
        static_cast<unsigned>(rawOpcode),
        static_cast<unsigned>(messageStorage.reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<uint32_t>(packetNameCallbackScaffold_),
        PacketNameFamilyToString(PacketNameFamilyScaffold()),
        packetizedMessagesEnabledScaffold_ ? 1u : 0u,
        (agenda && agenda->created) ? 1u : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredModuleCount4c) : 0u,
        (agenda && agenda->readHelperChainHead40 != nullptr) ? 1u : 0u,
        (agenda && agenda->writeHelperChainHead44 != nullptr) ? 1u : 0u,
        agendaTouched ? 1u : 0u,
        (agenda && agenda->writeOutputSlot24 != nullptr) ? 1u : 0u,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());

    const uint32_t sendResult = SubmitMessageRefBytesScaffold(*messageRefForSubmit);
    CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBitScaffold(&messageRef);
    return sendResult;
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
// Source-owned creation/reset of the local outer message-ref scaffold that `0x4490c0`
// materializes before later `0x41bc20/0x41bbb0`-style message-code reads.

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

    const CMessageConnectionMessageScaffold* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
        return false;
    }

    if (messageRef.headerless10 == 0u) {
        if (outMessageCodePointer) {
            *outMessageCodePointer = payloadBytes;
        }
        return true;
    }

    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = true;
    }
    if (payloadByteCount < 2u) {
        return false;
    }

    const uint8_t locatorByte0d = payloadBytes[1];
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
    if (payloadOffset >= payloadByteCount) {
        return false;
    }

    if (outMessageCodePointer) {
        *outMessageCodePointer = payloadBytes + payloadOffset;
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

    const CMessageConnectionMessageScaffold* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
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
        static_cast<size_t>(messageCodePointer - payloadBytes);
    const uint8_t firstByte = *messageCodePointer;
    uint16_t messageCode = firstByte;
    if ((firstByte & 0x80u) != 0u) {
        if (messageCodeOffset + 1u >= payloadByteCount) {
            return false;
        }
        messageCode = static_cast<uint16_t>(
            ((static_cast<uint16_t>(firstByte) << 8) |
             static_cast<uint16_t>(payloadBytes[messageCodeOffset + 1u])) &
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

// anchor: launcher.exe:0x469980 / embedded helper vtable `0x004baf48 +0x0c`
// Source-owned mirror of the currently modeled read-side agenda effect:
// store the incoming message-ref into the agenda read-output slot at `+0x08`.
// With the internal-only family now modeled as full virtual C++ classes, source routes this
// through the helper-chain head directly. The concrete helper implementations remain conservative:
// module read helper forwards to the embedded agenda helper, which then stores the output slot.
static bool CMessageConnection_DefaultAgendaReadPassThroughScaffold(
    CMessageConnectionPacketAgendaScaffold* agenda,
    CMessageConnectionReceivedMessageRefScaffold* inputMessageRef) {
    if (!agenda || agenda->readHelperChainHead40 == nullptr) {
        return false;
    }
    agenda->readHelperChainHead40->HandleOpaqueMessageRef(inputMessageRef);
    return agenda->readOutputSlot08 != nullptr;
}

// anchor: launcher.exe:0x469930
// Source-owned mirror of the read-side packet-agenda handoff just before leaf dispatch.
// Current bounded source model:
// - a created agenda always has the embedded default read pass-through helper at `+0x0c`
// - source now owns that helper's concrete agenda `+0x08` output-slot effect
// - source now keeps the nearer `0x4489d0` seam as a raw pointer handoff instead of deep-copying
//   a source-owned message-ref owner tail first
// - caller-installed read helpers are still modeled as pass-through-only until helper-side
//   transformation/discard behavior is recovered
static CMessageConnectionReceivedMessageRefScaffold* CMessageConnection_ApplyReceivePacketAgendaScaffold(
    CMessageConnectionPacketAgendaScaffold* agenda,
    CMessageConnectionReceivedMessageRefScaffold* inputMessageRef,
    bool* outAgendaTouched) {
    if (outAgendaTouched) {
        *outAgendaTouched = false;
    }
    if (!inputMessageRef) {
        return nullptr;
    }
    if (!agenda || !agenda->created) {
        return inputMessageRef;
    }

    agenda->readOutputSlot08 = nullptr;
    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    if (!CMessageConnection_DefaultAgendaReadPassThroughScaffold(agenda, inputMessageRef)) {
        return nullptr;
    }

    return agenda->readOutputSlot08;
}

}  // namespace

uint32_t CMessageConnection::DispatchCopiedParsedPacketTailScaffold(
    void* workItem,
    CMessageConnectionReceivedMessageRefScaffold& messageRef) {
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
    if (workType == 0x0bu) {
        // Preserve the explicit local code-4 continuation seam:
        // `0x441850` synthesizes a type-0x0b work-item and routes it back through connection
        // `OnOperationCompleted`; the base family must leave that work item for the later
        // margin-leaf/owner fallback chain (`0x44af60 -> 0x41afc0 -> helper slot2`), not consume it
        // as a generic unhandled op.
        spdlog::info(
            "CMessageConnection::OnOperationCompleted preserved local type0x0b completion workItem for leaf/owner fallback this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
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
    CMessageConnectionReceivedMessageRefScaffold copiedMessageRef = {};
    copiedMessageRef.ResetForPacketBuilderScaffold(!packetizedMessagesEnabledScaffold_);
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

    lastReceivedPacketBodyBytesScaffold_.clear();
    if (const CMessageConnectionMessageScaffold* const messageStorage = copiedMessageRef.messageStorage0c) {
        const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
        if (const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
            payloadBytes && payloadByteCount != 0u) {
            lastReceivedPacketBodyBytesScaffold_.assign(
                payloadBytes,
                payloadBytes + payloadByteCount);
        }
    }
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

    CMessageConnectionReceivedMessageRefScaffold* messageRefForDispatch = &copiedMessageRef;
    bool agendaTouched = false;
    if (CMessageConnectionPacketAgendaScaffold* agenda = packetAgendaScaffold_.get();
        agenda && agenda->created) {
        messageRefForDispatch = CMessageConnection_ApplyReceivePacketAgendaScaffold(
            agenda,
            &copiedMessageRef,
            &agendaTouched);
        if (!messageRefForDispatch) {
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
            lastReceivedPacketBodyBytesScaffold_.clear();
            if (const CMessageConnectionMessageScaffold* const messageStorage = messageRefForDispatch->messageStorage0c) {
                const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
                if (const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
                    payloadBytes && payloadByteCount != 0u) {
                    lastReceivedPacketBodyBytesScaffold_.assign(
                        payloadBytes,
                        payloadBytes + payloadByteCount);
                }
            }
            lastReceivedPacketHeaderlessScaffold_ = (messageRefForDispatch->headerless10 != 0u);
            spdlog::debug(
                "CMessageConnection::OnOperationCompleted preserved raw packet-agenda read handoff via agenda+0x08 message-ref pointer payloadBytes={} agendaModuleCount={} readHeadIsEmbedded={} this={} ownerContext={} remoteHost='{}'",
                static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
                static_cast<unsigned>(agenda->configuredModuleCount4c),
                (agenda->readHelperChainHead40 ==
                 static_cast<const CStreamPacketEncryptionHelperBaseScaffold*>(&agenda->embeddedReadHelper0c))
                    ? 1u
                    : 0u,
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

// UNANCHORED: source-owned raw-byte send wrapper used by current auth/bootstrap direct-send
// callsites; keep it distinct from the local packet-builder/message-ref path at
// `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00`.
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
    CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    (void)workItem;

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(this);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    const CMessageConnectionMessageScaffold* const messageStorage = messageRef.messageStorage0c;
    if (!context || !mediator || context->isMarginConnection || !messageStorage) {
        return 0u;
    }

    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    const size_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
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
        (resolvedMessageCodePointer && messageCodePointer) ? *messageCodePointer : payloadBytes[0];

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
        payloadBytes,
        payloadByteCount,
        &messageRef);
    spdlog::info(
        "CAuthStartupConnection::DispatchCopiedParsedPacketTailScaffold rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(decodedMessageCode),
        hadValidMessageCode ? 1u : 0u,
        headerless ? 1u : 0u,
        usedHeaderlessLocatorDecode ? 1u : 0u,
        static_cast<unsigned>(payloadByteCount),
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
void CMarginConnection::SetMessageCode4SuccessFlag84(bool value) {
    messageCode4SuccessFlag84_ = value;
}

bool CMarginConnection::MessageCode4SuccessFlag84() const {
    return messageCode4SuccessFlag84_;
}

// anchor: launcher.exe:0x441850
uint32_t CMarginConnection::DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus) {
    CMarginConnectionLocalCompletionWorkItemScaffold workItem = {};
    workItem.header.vtable = CMarginConnection_LocalCompletionWorkItemVtableScaffold();
    workItem.header.workType = 0x0bu;
    workItem.workPayload = workPayloadStatus;

    CMessageConnection* selfAsMessageConnection = this;
    const uint32_t handled = selfAsMessageConnection->OnOperationCompleted(&workItem);
    spdlog::info(
        "CMarginConnection::DispatchMessageCode4LocalCompletionWorkItem synthesized local type0x0b workItem status=0x{:08x} handled={} this={} ownerContext={} currentState={} remoteHost='{}'",
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
bool CMarginConnection::StoreBootstrapReplyCopy98(const void* bytes, size_t byteCount) {
    if (!bytes || byteCount != bootstrapReplyCopy98_.size()) {
        return false;
    }

    std::copy_n(
        static_cast<const uint8_t*>(bytes),
        bootstrapReplyCopy98_.size(),
        bootstrapReplyCopy98_.begin());
    hasBootstrapReplyCopy98_ = true;
    spdlog::info(
        "CMarginConnection::StoreBootstrapReplyCopy98 stored reply-copy block bytes=0x{:03x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(bootstrapReplyCopy98_.size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return true;
}

// anchor: launcher.exe:0x443340 -> connection `+0xa0`
bool CMarginConnection::StoreBootstrapPrepStateA0(
    const void* blockB0,
    const void* blockC4,
    const void* blockD8,
    size_t blockByteCount) {
    constexpr size_t kExpectedBlockByteCount = 0x14u;
    if (!blockB0 || !blockC4 || !blockD8 || blockByteCount != kExpectedBlockByteCount) {
        return false;
    }

    if (!bootstrapPrepStateA0_) {
        bootstrapPrepStateA0_ = std::make_unique<CMarginConnectionBootstrapPrepStateA0Scaffold>();
    }
    if (!bootstrapPrepStateA0_) {
        return false;
    }

    std::copy_n(
        static_cast<const uint8_t*>(blockB0),
        kExpectedBlockByteCount,
        bootstrapPrepStateA0_->blockB0.begin());
    std::copy_n(
        static_cast<const uint8_t*>(blockC4),
        kExpectedBlockByteCount,
        bootstrapPrepStateA0_->blockC4.begin());
    std::copy_n(
        static_cast<const uint8_t*>(blockD8),
        kExpectedBlockByteCount,
        bootstrapPrepStateA0_->blockD8.begin());

    const auto readFirstDword = [](const std::array<uint8_t, 0x14>& bytes) -> uint32_t {
        return static_cast<uint32_t>(bytes[0]) |
               (static_cast<uint32_t>(bytes[1]) << 8u) |
               (static_cast<uint32_t>(bytes[2]) << 16u) |
               (static_cast<uint32_t>(bytes[3]) << 24u);
    };

    spdlog::info(
        "CMarginConnection::StoreBootstrapPrepStateA0 staged owner+0x680 child blocks +0xb0/+0xc4/+0xd8 into connection-side +0xa0 mirror blockBytes=0x{:02x} firstDwordB0=0x{:08x} firstDwordC4=0x{:08x} firstDwordD8=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(kExpectedBlockByteCount),
        static_cast<unsigned>(readFirstDword(bootstrapPrepStateA0_->blockB0)),
        static_cast<unsigned>(readFirstDword(bootstrapPrepStateA0_->blockC4)),
        static_cast<unsigned>(readFirstDword(bootstrapPrepStateA0_->blockD8)),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return true;
}

// anchor: launcher.exe:0x41f30
uint32_t CMarginConnection::SendStoredBootstrapReplyCopy98() {
    if (!hasBootstrapReplyCopy98_) {
        return 0u;
    }

    constexpr uint16_t kReplyCopyByteCount = 0x136u;
    constexpr uint16_t kLeadingType1PrefixByteCount = 3u;
    constexpr uint16_t kNestedReplyCopyLengthFieldByteCount = sizeof(uint16_t);
    constexpr uint16_t kTotalPayloadByteCount =
        kLeadingType1PrefixByteCount +
        kNestedReplyCopyLengthFieldByteCount +
        kReplyCopyByteCount;

    CMessageConnectionMessageRefScaffold messageRef = {};
    messageRef.ResetForPacketBuilderScaffold(/*headerless=*/false);
    if (!messageRef.messageStorage0c) {
        return 0u;
    }

    messageRef.messageStorage0c->ResetPayloadByteCountScaffold(kTotalPayloadByteCount);
    uint8_t* payloadBytes = messageRef.messageStorage0c->PayloadBaseScaffold();
    if (!payloadBytes) {
        return 0u;
    }

    // Tightest current static read from `0x441f30 -> 0x43a230(0x136)`:
    // - leading raw type-1 prefix bytes `01 00 00`
    // - `0x43a230` then grows the retained message-ref's inner storage by `(0x136 + 2)` and
    //   points the later copy at
    //   the span immediately after that inner little-endian `0x0136` word
    payloadBytes[0] = 0x01u;
    payloadBytes[1] = 0u;
    payloadBytes[2] = 0u;
    payloadBytes[3] = static_cast<uint8_t>(kReplyCopyByteCount & 0xffu);
    payloadBytes[4] = static_cast<uint8_t>((kReplyCopyByteCount >> 8) & 0xffu);
    std::copy(
        bootstrapReplyCopy98_.begin(),
        bootstrapReplyCopy98_.end(),
        payloadBytes + kLeadingType1PrefixByteCount + kNestedReplyCopyLengthFieldByteCount);

    const uint32_t sendResult = SendPacketMessageRefScaffold(messageRef);
    spdlog::info(
        "CMarginConnection::SendStoredBootstrapReplyCopy98 sent rawType1PrefixPlusLengthPrefixedReplyCopy payloadBytes=0x{:03x} nestedReplyCopyBytes=0x{:03x} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(kTotalPayloadByteCount),
        static_cast<unsigned>(kReplyCopyByteCount),
        static_cast<unsigned>(sendResult),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return sendResult;
}

// anchor: launcher.exe:0x441470 / 0x44da00 / 0x44daf0
void CMarginConnection::EnsureStreamPacketEncryptionModuleFromSeed85() {
    if (!hasMessageCode5SeedBytes85_) {
        return;
    }

    if (!streamPacketEncryptionModule9c_) {
        streamPacketEncryptionModule9c_ =
            std::make_unique<CStreamPacketEncryptionModuleScaffold>();
    }
    if (!streamPacketEncryptionModule9c_) {
        ConfigurePacketAgendaScaffold(nullptr);
        return;
    }

    streamPacketEncryptionModule9c_->ResetForMarginConnectionSeed(
        messageCode5SeedBytes85_);
    ConfigurePacketAgendaScaffold(streamPacketEncryptionModule9c_.get());

    spdlog::info(
        "CMarginConnection::EnsureStreamPacketEncryptionModuleFromSeed85 synced connection+0x9c module from seed85_94 module={} agenda={} firstDword=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(streamPacketEncryptionModule9c_.get()),
        fmt::ptr(PacketAgendaScaffold()),
        static_cast<unsigned>(
            static_cast<uint32_t>(messageCode5SeedBytes85_[0]) |
            (static_cast<uint32_t>(messageCode5SeedBytes85_[1]) << 8u) |
            (static_cast<uint32_t>(messageCode5SeedBytes85_[2]) << 16u) |
            (static_cast<uint32_t>(messageCode5SeedBytes85_[3]) << 24u)),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
}

// anchor: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
void CMarginConnection::SetMessageCode5SeedBytes85(const std::array<uint8_t, 16>& value) {
    messageCode5SeedBytes85_ = value;
    hasMessageCode5SeedBytes85_ = true;
    EnsureStreamPacketEncryptionModuleFromSeed85();
}

bool CMarginConnection::CopyMessageCode5SeedBytes85(std::array<uint8_t, 16>* outValue) const {
    if (!outValue || !hasMessageCode5SeedBytes85_) {
        return false;
    }

    *outValue = messageCode5SeedBytes85_;
    return true;
}

const uint8_t* CMarginConnection::MessageCode5SeedBytes85Pointer() const {
    return hasMessageCode5SeedBytes85_ ? messageCode5SeedBytes85_.data() : nullptr;
}

// anchor: launcher.exe:0x44af20 -> 0x442d00 -> owner vtable `+0x184` / `0x41f260`
uint32_t CMarginConnection::DispatchCopiedParsedPacketTailScaffold(
    void* workItem,
    CMessageConnectionReceivedMessageRefScaffold& messageRef) {
    (void)workItem;

    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context =
        CMessageConnection_LoginMediatorContextScaffold(this);
    mxo::ltlogin::CLTLoginMediator* mediator = context ? context->mediator : nullptr;
    const CMessageConnectionMessageScaffold* const messageStorage = messageRef.messageStorage0c;
    if (!context || !mediator || !context->isMarginConnection || !messageStorage) {
        return 0u;
    }

    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    const size_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
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
        (resolvedMessageCodePointer && messageCodePointer) ? *messageCodePointer : payloadBytes[0];
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
                payloadBytes,
                payloadByteCount,
                /*transportEncrypted=*/false);
        logConsumedMarginBranch(decodedMessageCode, handledCode2, nullptr, 0u);
        if (handledCode2 != 0u) {
            return handledCode2;
        }
    }

    if (hadValidMessageCode && decodedMessageCode == 4u) {
        const uint32_t handledCode4 =
            mediator->HandleMarginConsumedCode4AtConnectionSeamScaffold(
                payloadBytes,
                payloadByteCount,
                /*transportEncrypted=*/false);
        logConsumedMarginBranch(
            decodedMessageCode,
            handledCode4,
            "connectionByte84",
            MessageCode4SuccessFlag84() ? 1u : 0u);
        if (handledCode4 != 0u) {
            return handledCode4;
        }
    }

    if (hadValidMessageCode && decodedMessageCode == 5u && payloadByteCount >= 17u) {
        std::array<uint8_t, 16> seedBytes85 = {};
        std::copy_n(payloadBytes + 1u, seedBytes85.size(), seedBytes85.begin());
        SetMessageCode5SeedBytes85(seedBytes85);
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
        payloadBytes,
        payloadByteCount,
        &messageRef);
    spdlog::info(
        "CMarginConnection::DispatchCopiedParsedPacketTailScaffold rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} messageRef={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(decodedMessageCode),
        hadValidMessageCode ? 1u : 0u,
        headerless ? 1u : 0u,
        usedHeaderlessLocatorDecode ? 1u : 0u,
        static_cast<unsigned>(payloadByteCount),
        fmt::ptr(&messageRef),
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
