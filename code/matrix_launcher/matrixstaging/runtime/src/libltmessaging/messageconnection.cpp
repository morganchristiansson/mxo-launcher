#include "messageconnection.h"

#include "spdlog/spdlog.h"

#include <algorithm>

#ifdef DispatchMessage
#undef DispatchMessage
#endif

namespace mxo::liblttcp {

// ============================================================
// VTable 0x004afef0 - CMessageConnection (Intermediate Base)
// ============================================================
// 0x004afef0 - CMessageConnection::CMessageConnection at 0x0041cf50
// 0x004afefc - CLTTCPConnection::CLTTCPConnection at 0x00449ca0
// 0x004aff00 - CMessageConnection::ProcessPacketResult at 0x00449a70
// 0x004aff04 - CLTTCPConnection::OnReceive at 0x00449d40
// 0x004aff08 - CLTTCPConnection::OnClose at 0x00449fd0
// 0x004aff0c - CLTTCPConnection::Close at 0x00449cd0
// 0x004aff10 - CLTTCPConnection::~CLTTCPConnection at 0x00449d20
// 0x004aff14 - FinalizeCallback at 0x0041cf30
// 0x004aff18 - CMessageConnection::SendPacket at 0x00448cf0
// 0x004aff1c - CMessageConnection::ProcessDispatchResult at 0x00449a30
// 0x004aff34 - CMarginConnection::~CMarginConnection at 0x0041ce80

// ============================================================
// FAITHFUL: VTable 0x004afef0 - Constructor at 0x0041cf50
// Type A initialization - sets vtable, calls cleanup via FUN_00441400
// ============================================================
CMessageConnection::CMessageConnection()
    : CLTTCPConnection(),
      packetNameFamilyScaffold_(CMessageConnectionPacketNameFamilyScaffold::kUnknown),
      packetizedMessagesEnabledScaffold_(false),
      packetAgendaScaffold_() {}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Constructor with engine parameter
// ============================================================
CMessageConnection::CMessageConnection(CLTThreadPerClientTCPEngine* engine)
    : CLTTCPConnection(),
      packetNameFamilyScaffold_(CMessageConnectionPacketNameFamilyScaffold::kUnknown),
      packetizedMessagesEnabledScaffold_(false),
      packetAgendaScaffold_() {
    CLTTCPConnection::SetEngine(engine);
}

// ============================================================
// FAITHFUL: VTable 0x004aff10 - Destructor at 0x00449d20
// Inherits from CLTTCPConnection
// ============================================================
CMessageConnection::~CMessageConnection() = default;

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for engine management
// ============================================================
void CMessageConnection::SetEngine(CLTThreadPerClientTCPEngine* engine) {
    CLTTCPConnection::SetEngine(engine);
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for engine pointer
// ============================================================
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

void CMessageConnection::ConfigurePacketNameFamilyScaffold(
    CMessageConnectionPacketNameFamilyScaffold family,
    bool packetizedMessagesEnabled) {
    packetNameFamilyScaffold_ = family;
    packetizedMessagesEnabledScaffold_ = packetizedMessagesEnabled;
}

CMessageConnectionPacketNameFamilyScaffold CMessageConnection::PacketNameFamilyScaffold() const {
    return packetNameFamilyScaffold_;
}

bool CMessageConnection::PacketizedMessagesEnabledScaffold() const {
    return packetizedMessagesEnabledScaffold_;
}

void CMessageConnection::EnsurePacketAgendaScaffold() {
    if (!packetAgendaScaffold_) {
        packetAgendaScaffold_ = std::make_unique<CMessageConnectionPacketAgendaScaffold>();
    }
    if (packetAgendaScaffold_) {
        packetAgendaScaffold_->created = true;
    }
}

const CMessageConnectionPacketAgendaScaffold* CMessageConnection::PacketAgendaScaffold() const {
    return packetAgendaScaffold_.get();
}

// ============================================================
// Source-owned launcher-only bridge for the narrowed margin send-authenticity gap.
// Mirrors the now-recovered original split between:
// - local packet-envelope object (`0x41af70` / `0x41cf30`)
// - shared message object consumed by `0x448cf0 -> 0x448a00`
// ============================================================
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

bool CMessageConnection::PacketAgendaAllowsEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope) const {
    // `0x448cf0` consults connection `+0x74` and may discard the packet before submit.
    // Newer tightening now makes that connection-side metadata explicit too:
    // - `0x448980` lazy-creates the `+0x74` agenda object
    // - `0x469950` runs the send-side write-helper chain and may return a replaced or null packet
    // Current source model still keeps the active path pass-through, but it now preserves whether
    // we have even modeled the presence of that agenda object on the connection.
    (void)envelope;
    return true;
}

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

uint32_t CMessageConnection::SendPacketEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope) {
    if (!Engine() || !envelope.sharedMessage) {
        return 0u;
    }

    if (!PacketAgendaAllowsEnvelopeScaffold(envelope)) {
        spdlog::info(
            "CMessageConnection::SendPacketEnvelopeScaffold discarded packet because packet-agenda reconstruction is still missing this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 0u;
    }

    const std::vector<uint8_t> framedBytesFrom0a = envelope.sharedMessage->BuildFramedBytesFrom0aScaffold();
    const size_t submitOffset = (framedBytesFrom0a.empty() || ((framedBytesFrom0a[0] >> 7) != 0u)) ? 0u : 1u;
    const size_t opcodeIndex = submitOffset + ((submitOffset == 0u) ? 2u : 1u);
    const uint8_t rawOpcode = (opcodeIndex < framedBytesFrom0a.size()) ? framedBytesFrom0a[opcodeIndex] : 0u;
    const CMessageConnectionPacketAgendaScaffold* agenda = PacketAgendaScaffold();
    spdlog::info(
        "CMessageConnection::SendPacketEnvelopeScaffold headerless={} rawOpcode=0x{:02x} reservedBytes08=0x{:04x} payloadBytes={} framedBytesFrom0a={} packetNameFamily={} packetizedEnabled={} agendaCreated={} agendaReadHelpers={} agendaWriteHelpers={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(envelope.headerless10),
        static_cast<unsigned>(rawOpcode),
        static_cast<unsigned>(envelope.sharedMessage->reservedBytes08),
        static_cast<unsigned>(envelope.sharedMessage->PayloadByteCountScaffold()),
        static_cast<unsigned>(framedBytesFrom0a.size()),
        PacketNameFamilyToString(packetNameFamilyScaffold_),
        packetizedMessagesEnabledScaffold_ ? 1u : 0u,
        (agenda && agenda->created) ? 1u : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredReadHelperCount) : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredWriteHelperCount) : 0u,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return SubmitEnvelopeBytesScaffold(envelope);
}

// ============================================================
// FAITHFUL: VTable 0x004aff1c - CMessageConnection::OnOperationCompleted at 0x00448a60
// Generic fallback handler for unhandled operation codes
// ============================================================
uint32_t CMessageConnection::OnOperationCompleted(void* workCode) {
    if (!Engine()) {
        return 0;
    }

    // FAITHFUL: Original launcher.exe FUN_00448a60 is string-backed only as a generic
    // "Got unhandled op of type %d with status %s" logger when the auth helper returns 0.
    (void)workCode;
    return 1;
}

// ============================================================
// FAITHFUL: ProcessPacketResult at 0x00449a70
// 42 instructions, 8 complexity, 5 calls
// Packet result processing handler
// ============================================================
uint32_t CMessageConnection::ProcessPacketResult(const void* packetData, uint32_t byteCount) {
    if (!Engine() || !packetData) {
        return 0;
    }

    // Placeholder for packet result handling - delegates to protocol-specific handlers
    (void)byteCount;
    return 1;
}

// ============================================================
// FAITHFUL: VTable 0x004aff18 - CMessageConnection::SendPacket at 0x00448cf0
// 316 instructions, 45 complexity, 27 calls
// Current starter path deliberately routes through the recovered connection-object-based
// engine surface instead of pretending this is already a faithful packet-envelope sender.
// Fresh `0x448cf0` review now makes the original gap narrower and explicit:
// - original input is a message/envelope object
// - packet-agenda filtering happens there before the lower submit helper `0x448a00`
// - only that lower helper forwards final byte pointer/size into the engine
// ============================================================
uint32_t CMessageConnection::SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext) {
    if (!Engine() || !packetData || packetByteCount == 0) {
        return 0;
    }

    // Current starter path deliberately routes through the recovered connection-object-based
    // engine surface instead of pretending this is already a faithful packet serializer.
    return Engine()->SendBufferConnectionScaffold(static_cast<CLTTCPConnection*>(this), packetData, packetByteCount, completionContext);
}

// ============================================================
// FAITHFUL: VTable 0x004aff1c - CMessageConnection::ProcessDispatchResult at 0x00449a30
// 23 instructions, 2 complexity, 2 calls
// Message dispatch result handler
// ============================================================
uint32_t CMessageConnection::ProcessDispatchResult(const void* packetData, uint32_t byteCount) {
    if (!Engine() || !packetData) {
        return 0;
    }

    // Placeholder for dispatch result handling - delegates to protocol-specific handlers
    (void)byteCount;
    return 1;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Wrapper for engine Connect method
// ============================================================
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
// VTable 0x004aff38 - CMarginConnection (Type D)
// ============================================================
// Inheritance Chain:
// CLTTCPConnection (0x004b8034) [Base]
// └── CMessageConnection (0x004afef0) [Intermediate Base]
//     └── Type A (0x004b64a8) [Base for Type D]
//         └── CMarginConnection (Type D) [0x004aff38] - Leaf
// ============================================================
// 0x004aff38 - FUN_0041cf80 (Type D initialization)
// 0x004aff48 - FUN_0044af60 (Advanced message routing)
// 0x004aff64 - FUN_0044af20 (Message dispatcher)
// Inherits: CLTTCPConnection methods + CMessageConnection::SendPacket

// ============================================================
// FAITHFUL: VTable 0x004aff38 - Constructor at 0x0041cf80
// Type D initialization - sets vtable pointer, calls FUN_00441820 cleanup
// Sets vtable to &PTR_FUN_004aff38 at offset 0xa0
// ============================================================
CMarginConnection::CMarginConnection()
    : CMessageConnection(),
      marginEngine_(nullptr) {}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Constructor with margin engine parameter
// ============================================================
CMarginConnection::CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine)
    : CMessageConnection(),
      marginEngine_(marginEngine) {}

// ============================================================
// FAITHFUL: VTable 0x004aff34 - CMarginConnection::~CMarginConnection at 0x0041ce80
// 60 instructions, 10 complexity, 8 calls
// Destructor - inherits from CLTTCPConnection (via CMessageConnection)
// Note: Cleanup calls FUN_00441820 which sets vtable to Type A's vtable
// ============================================================
CMarginConnection::~CMarginConnection() = default;

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Placeholder entry point for margin engine management
// ============================================================
void CMarginConnection::SetMarginEngine(CLTThreadPerClientTCPEngine* marginEngine) {
    marginEngine_ = marginEngine;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// Getter for margin engine pointer
// ============================================================
CLTThreadPerClientTCPEngine* CMarginConnection::MarginEngine() const {
    return marginEngine_;
}

// ============================================================
// FAITHFUL: VTable 0x004aff48 - CMarginConnection::RouteMessage at 0x0044af60
// 42 instructions, 7 complexity, 5 calls
// Advanced message routing with fallback handlers for robustness
// Calls OnOperationCompleted first, then delegates to protocol-specific handlers
// ============================================================
uint32_t CMarginConnection::RouteMessage(const void* packetData, uint32_t byteCount) {
    if (!marginEngine_ || !packetData) {
        return 0;
    }

    // FAITHFUL: Calls Type A's OnOperationCompleted first (FUN_0044af60 pattern)
    uint32_t result = OnOperationCompleted(nullptr);
    if (result == 0) {
        return 0;
    }

    // Placeholder for advanced routing - delegates to protocol handlers at offsets
    // 0xa4+0x188 and 0xa4+0x184 for CERT/MS protocol support
    (void)byteCount;
    return 1;
}

// ============================================================
// FAITHFUL: VTable 0x004aff64 - FUN_0044af20 at 0x0044af20
// 23 instructions, 2 complexity, 2 calls
// Message dispatcher - calls dispatch router FUN_00442d00
// ============================================================
uint32_t CMarginConnection::DispatchMessage(const void* packetData, uint32_t byteCount) {
    if (!marginEngine_ || !packetData) {
        return 0;
    }

    // FAITHFUL: Calls dispatch router FUN_00442d00 (FUN_0044af20 pattern)
    // Placeholder for actual dispatch logic
    (void)byteCount;
    return 1;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// CERT protocol handler placeholder
// ============================================================
uint32_t CMarginConnection::HandleCERTMessage(const void* packetData, uint32_t byteCount) {
    if (!marginEngine_ || !packetData) {
        return 0;
    }

    // Placeholder for CERT_* message handling (FUN_0041bca0 family)
    // CERT_ConnectRequest, CERT_Challenge, CERT_ChallengeResponse, etc.
    (void)byteCount;
    return 1;
}

// ============================================================
// UNANCHORED: Not based on vtable analysis
// MS protocol handler placeholder
// ============================================================
uint32_t CMarginConnection::HandleMSMessage(const void* packetData, uint32_t byteCount) {
    if (!marginEngine_ || !packetData) {
        return 0;
    }

    // Placeholder for MS_* message handling (FUN_0041bf70 family)
    // MS_ConnectRequest, MS_ClaimCharacterNameRequest, etc.
    (void)byteCount;
    return 1;
}

}  // namespace mxo::liblttcp
