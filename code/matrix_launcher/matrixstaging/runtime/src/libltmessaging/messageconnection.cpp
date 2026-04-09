#include "messageconnection.h"

#include "../../../game/src/libltclientlogin/loginmediator.h"
#include "../libltcrypto/auth_crypto.h"
#include "variablelengthprefixedtcpstreamparser.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <memory>
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

// Internal-only `CMessageConnectionMessage` note:
// - launcher.exe uses pooled refcounted vtables `0x004ba208/0x004ba220/0x004ba23c`
// - source now models that same behavior with ordinary C++ virtual classes because these message
//   objects are only used inside our own build and do not need to preserve the original raw ABI

// UNANCHORED: source-owned narrow subset of `0x448b40` with a null engine and without the
// optional completion-helper allocation path.
CMessageConnection::CMessageConnection()
    : CLTTCPConnection(),
      packetNameCallback_(0),
      packetizedMessagesEnabled_(false),
      packetAgenda_() {}

// UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
CMessageConnection::CMessageConnection(CLTThreadPerClientTCPEngine* engine)
    : CLTTCPConnection(),
      packetNameCallback_(0),
      packetizedMessagesEnabled_(false),
      packetAgenda_() {
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

// anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
uint32_t CMessageConnectionMessageStorage::AddRef() {
    return static_cast<uint32_t>(InterlockedIncrement(&referenceCount04));
}

// anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
uint32_t CMessageConnectionMessageStorage::Release() {
    const LONG current = InterlockedDecrement(&referenceCount04);
    if (current == 0) {
        FinalRelease();
    }
    return static_cast<uint32_t>(current);
}

void CMessageConnectionMessageStorage::FinalRelease() {
    // anchor: launcher.exe:0x455ad0 / vtable `0x004ba208 +0x0c`
    // The original heap object returns to a pool here. This internal mirror is stack/inline owned,
    // so the final-release path is intentionally non-deleting.
}

// anchor: launcher.exe:0x42f880 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x10`
void CMessageConnectionMessageStorage::ResetRefCount() {
    InterlockedExchange(&referenceCount04, 0);
}

// anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
void CMessageConnectionMessageStorage::SetRefCountFromPtr(const volatile long* refCountSource) {
    if (!refCountSource) {
        return;
    }
    InterlockedExchange(&referenceCount04, *refCountSource);
}

void CMessageConnectionMessageStorage::ResetForPacketBuilderScaffold() {
    // anchor: launcher.exe:0x455bd0 inner-storage setup before the outer object stores/AddRefs it
    referenceCount04 = 0;
    reservedBytes08 = kBuilderReservedBytes08;
    payloadLengthHigh0a = 0u;
    payloadLengthLow0b = 0u;
    std::fill(payloadBytes0c.begin(), payloadBytes0c.end(), 0u);
}

void CMessageConnectionMessageStorage::ResetPayloadByteCountScaffold(uint16_t payloadByteCount) {
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
    SetPayloadByteCountRawScaffold(clampedByteCount);
}

void CMessageConnectionMessageStorage::SetPayloadByteCountRawScaffold(uint16_t payloadByteCount) {
    const uint16_t clampedByteCount = std::min<uint16_t>(payloadByteCount, kMaxPayloadByteCount);
    payloadLengthLow0b = static_cast<uint8_t>(clampedByteCount & 0xffu);
    payloadLengthHigh0a =
        (clampedByteCount > 0x7fu)
            ? static_cast<uint8_t>(0x80u | ((clampedByteCount >> 8u) & 0x7fu))
            : 0u;
}

uint16_t CMessageConnectionMessageStorage::GrowPayloadByteCountScaffold(uint16_t additionalByteCount) {
    // anchor: launcher.exe:0x4557b0
    const uint32_t oldByteCount = PayloadByteCountScaffold();
    const uint32_t requestedByteCount = oldByteCount + static_cast<uint32_t>(additionalByteCount);
    if (requestedByteCount > kMaxPayloadByteCount) {
        return static_cast<uint16_t>(oldByteCount);
    }

    const uint16_t newByteCount = static_cast<uint16_t>(requestedByteCount);
    SetPayloadByteCountRawScaffold(newByteCount);
    return newByteCount;
}

uint16_t CMessageConnectionMessageStorage::PayloadByteCountScaffold() const {
    const uint16_t encodedPayloadByteCount =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(payloadLengthHigh0a & 0x7fu) << 8u) |
            static_cast<uint16_t>(payloadLengthLow0b));
    return std::min<uint16_t>(encodedPayloadByteCount, kMaxPayloadByteCount);
}

uint16_t CMessageConnectionMessageStorage::RemainingAppendableByteCountScaffold() const {
    const uint32_t payloadByteCount = PayloadByteCountScaffold();
    if (payloadByteCount >= kMaxPayloadByteCount || reservedBytes08 >= kMaxPayloadByteCount) {
        return 0u;
    }

    const uint32_t remaining = kMaxPayloadByteCount - payloadByteCount - reservedBytes08;
    return static_cast<uint16_t>(std::min<uint32_t>(remaining, kMaxPayloadByteCount));
}

uint8_t* CMessageConnectionMessageStorage::PayloadBaseScaffold() {
    return payloadBytes0c.data();
}

const uint8_t* CMessageConnectionMessageStorage::PayloadBaseScaffold() const {
    return payloadBytes0c.data();
}

// anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
uint32_t CMessageConnectionMessageRefBase::AddRef() {
    return static_cast<uint32_t>(InterlockedIncrement(&referenceCount04));
}

// anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
uint32_t CMessageConnectionMessageRefBase::Release() {
    const LONG current = InterlockedDecrement(&referenceCount04);
    if (current == 0) {
        FinalRelease();
    }
    return static_cast<uint32_t>(current);
}

// anchor: launcher.exe:0x42f880 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x10`
void CMessageConnectionMessageRefBase::ResetRefCount() {
    InterlockedExchange(&referenceCount04, 0);
}

// anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
void CMessageConnectionMessageRefBase::SetRefCountFromPtr(const volatile long* refCountSource) {
    if (!refCountSource) {
        return;
    }
    InterlockedExchange(&referenceCount04, *refCountSource);
}

void CMessageConnectionMessageRefBase::ResetBaseForPacketBuilderScaffold(uint32_t field08Value) {
    // anchor: launcher.exe:0x455bd0
    referenceCount04 = 0;
    field08 = field08Value;
    messageStorage0c = nullptr;
    ownedMessageStorage_.ResetForPacketBuilderScaffold();
    messageStorage0c = &ownedMessageStorage_;
    messageStorage0c->AddRef();
}

uint16_t CMessageConnectionMessageRefBase::GrowPayloadByteCountScaffold(
    uint16_t additionalByteCount) {
    if (!messageStorage0c) {
        return 0u;
    }
    return messageStorage0c->GrowPayloadByteCountScaffold(additionalByteCount);
}

uint8_t* CMessageConnectionMessageRefBase::PayloadAppendPointerScaffold() {
    if (!messageStorage0c) {
        return nullptr;
    }
    uint8_t* const payloadBase = messageStorage0c->PayloadBaseScaffold();
    return payloadBase
        ? (payloadBase + static_cast<size_t>(messageStorage0c->PayloadByteCountScaffold()))
        : nullptr;
}

// anchor: launcher.exe:0x41bb60
bool CMessageConnectionMessageRefBase::SetPayloadByteCountScaffold(
    uint32_t payloadByteCount) {
    if (!messageStorage0c || payloadByteCount > CMessageConnectionMessageStorage::kMaxPayloadByteCount) {
        return false;
    }
    messageStorage0c->SetPayloadByteCountRawScaffold(static_cast<uint16_t>(payloadByteCount));
    return true;
}

uint16_t CMessageConnectionMessageRefBase::PayloadByteCountScaffold() const {
    return messageStorage0c ? messageStorage0c->PayloadByteCountScaffold() : 0u;
}

void CMessageConnectionMessageRef::FinalRelease() {
    // anchor: launcher.exe:0x455b80 / vtable `0x004ba23c +0x0c`
    // Original live outer objects release the inner payload-storage object at `+0x0c`, collapse,
    // and return to the outer-object pool. Source still lacks the real pool, but a heap-backed
    // delete after releasing the inner storage is closer to the original lifetime than keeping the
    // receive-side outer message-ref on the stack.
    if (messageStorage0c) {
        CMessageConnectionMessageStorage* const storage = messageStorage0c;
        messageStorage0c = nullptr;
        storage->Release();
    }
    delete this;
}

void CMessageConnectionMessageRef::ResetForPacketBuilderScaffold(
    bool headerless,
    uint32_t messageContext14) {
    // anchor: launcher.exe:0x455cd0 / 0x455c60
    ResetBaseForPacketBuilderScaffold(/*field08Value=*/0u);
    headerless10 = headerless ? 1u : 0u;
    padding11_13[0] = 0u;
    padding11_13[1] = 0u;
    padding11_13[2] = 0u;
    this->messageContext14 = messageContext14;
    field18 = 0u;
    field1c = 0u;
    field20 = 0u;
    AddRef();
}

namespace {

struct CMessageConnectionMessageRefReleaseDeleter {
    void operator()(CMessageConnectionMessageRef* messageRef) const {
        if (messageRef) {
            messageRef->Release();
        }
    }
};

static bool CMessageConnection_ResolveTransformInputSpan(
    const CMessageConnectionMessageRef& inputMessageRef,
    const uint8_t** outPayloadBytes,
    size_t* outPayloadByteCount) {
    if (outPayloadBytes) {
        *outPayloadBytes = nullptr;
    }
    if (outPayloadByteCount) {
        *outPayloadByteCount = 0u;
    }

    const CMessageConnectionMessageStorage* const messageStorage =
        inputMessageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    const uint32_t skippedPrefixByteCount = inputMessageRef.field08;
    if (!payloadBytes || skippedPrefixByteCount > payloadByteCount) {
        return false;
    }

    if (outPayloadBytes) {
        *outPayloadBytes = payloadBytes + skippedPrefixByteCount;
    }
    if (outPayloadByteCount) {
        *outPayloadByteCount =
            static_cast<size_t>(payloadByteCount - skippedPrefixByteCount);
    }
    return true;
}

static void CStreamPacketEncryptionModuleReadHelper_RecordSuccessfulTransformIndex(
    CStreamPacketEncryptionModuleReadHelper* helper,
    size_t successfulIndex) {
    if (!helper || helper->transformWorkers.size() <= 1u) {
        return;
    }

    // anchor: launcher.exe:0x44d2e0 / 0x44d130
    // The original helper keeps a small hit counter in helper `+0x0c` while more than one worker
    // is live. Repeated success on worker 0 eventually collapses the collection back down to just
    // that first worker; success on any later worker resets the counter.
    if (successfulIndex == 0u) {
        ++helper->collectionControl0c;
        if (helper->collectionControl0c > 4u && helper->transformWorkers.size() > 1u) {
            helper->transformWorkers.erase(
                helper->transformWorkers.begin() + 1,
                helper->transformWorkers.end());
        }
    } else {
        helper->collectionControl0c = 0u;
    }
}

static std::vector<uint8_t> CStreamPacketEncryptionWorker_KeyBytes(
    const std::array<uint8_t, 16>& seedBytes) {
    return std::vector<uint8_t>(seedBytes.begin(), seedBytes.end());
}

static CMessageConnectionMessageRef* CMessageConnectionMessageRefHandle_AssignRetained(
    CMessageConnectionMessageRef** slot,
    CMessageConnectionMessageRef* newMessageRef) {
    if (!slot) {
        return nullptr;
    }
    CMessageConnectionMessageRef* const oldMessageRef = *slot;
    if (oldMessageRef != newMessageRef) {
        if (oldMessageRef) {
            oldMessageRef->Release();
        }
        *slot = newMessageRef;
        if (newMessageRef) {
            newMessageRef->AddRef();
        }
    }
    return *slot;
}

}  // namespace

CMessageConnectionMessageRefOutputBuffer::~CMessageConnectionMessageRefOutputBuffer() {
    Reset();
}

// anchor: launcher.exe:0x44d390 / 0x44d500
void CMessageConnectionMessageRefOutputBuffer::Reset() {
    hasValue = false;
    if (messageRef) {
        messageRef->Release();
        messageRef = nullptr;
    }
}

// anchor: launcher.exe:0x44c8b0
bool CMessageConnectionMessageRefOutputBuffer::SetPayloadBytes(
    const uint8_t* payloadBytes,
    size_t payloadByteCount) {
    Reset();
    if (!payloadBytes || payloadByteCount == 0u ||
        payloadByteCount > CMessageConnectionMessageStorage::kMaxPayloadByteCount) {
        return false;
    }

    messageRef = new CMessageConnectionMessageRef();
    messageRef->ResetForPacketBuilderScaffold(false, 0u);
    uint8_t* const appendPointer = messageRef->PayloadAppendPointerScaffold();
    if (!appendPointer) {
        Reset();
        return false;
    }

    std::copy_n(payloadBytes, payloadByteCount, appendPointer);
    if (messageRef->GrowPayloadByteCountScaffold(static_cast<uint16_t>(payloadByteCount)) !=
        payloadByteCount) {
        Reset();
        return false;
    }

    hasValue = true;
    return true;
}

// anchor: launcher.exe:0x44d390 / 0x44d500
CMessageConnectionMessageRef* CMessageConnectionMessageRefOutputBuffer::MessageRef() {
    return hasValue ? messageRef : nullptr;
}

// anchor: launcher.exe:0x44d910 / 0x44daf0
void CStreamPacketEncryptionModuleReadTransformWorker::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    associatedSeedBytes = seedBytes;
}

// anchor: launcher.exe:0x44d500
bool CStreamPacketEncryptionModuleReadTransformWorker::TryTransform(
    const CMessageConnectionMessageRef& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) const {
    // Source collapses the original large FeedbackSize worker + StreamTransformationFilter +
    // CPacketDecryptor chain down to the confirmed packet semantic: decrypt the current payload
    // span with the associated 16-byte seed and materialize a replacement message-ref on success.
    if (!outputBuffer) {
        return false;
    }

    const uint8_t* encryptedPayloadBytes = nullptr;
    size_t encryptedPayloadByteCount = 0u;
    if (!CMessageConnection_ResolveTransformInputSpan(
            inputMessageRef,
            &encryptedPayloadBytes,
            &encryptedPayloadByteCount)) {
        return false;
    }

    std::vector<uint8_t> decryptedPayloadBytes;
    if (!mxo::auth::DecryptMarginPayloadPacket(
            encryptedPayloadBytes,
            encryptedPayloadByteCount,
            CStreamPacketEncryptionWorker_KeyBytes(associatedSeedBytes),
            &decryptedPayloadBytes)) {
        return false;
    }
    return outputBuffer->SetPayloadBytes(
        decryptedPayloadBytes.data(),
        decryptedPayloadBytes.size());
}

// anchor: launcher.exe:0x44d820 / 0x44daf0
void CStreamPacketEncryptionModuleWriteTransformWorker::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    associatedSeedBytes = seedBytes;
}

// anchor: launcher.exe:0x44d390
bool CStreamPacketEncryptionModuleWriteTransformWorker::TryTransform(
    const CMessageConnectionMessageRef& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) const {
    // Source collapses the original small FeedbackSize worker + CPacketEncryptor chain down to the
    // confirmed packet semantic: encrypt the current payload span with the associated 16-byte seed
    // and materialize a replacement message-ref on success.
    if (!outputBuffer) {
        return false;
    }

    const uint8_t* payloadBytes = nullptr;
    size_t payloadByteCount = 0u;
    if (!CMessageConnection_ResolveTransformInputSpan(
            inputMessageRef,
            &payloadBytes,
            &payloadByteCount)) {
        return false;
    }

    mxo::auth::FramedPacket encryptedPacket;
    if (!mxo::auth::EncryptMarginPayloadPacket(
            payloadBytes,
            payloadByteCount,
            CStreamPacketEncryptionWorker_KeyBytes(associatedSeedBytes),
            mxo::auth::kFrameModeAuto,
            &encryptedPacket)) {
        return false;
    }
    return outputBuffer->SetPayloadBytes(
        encryptedPacket.payloadBytes.data(),
        encryptedPacket.payloadBytes.size());
}

// UNANCHORED: source-owned helper forwarding through the recovered helper-family `nextHelper04`
// link used by the agenda read/write chains.
void CStreamPacketEncryptionHelperBase::ForwardToNextHelper(
    void* opaqueMessageRef) {
    if (nextHelper04) {
        nextHelper04->HandleOpaqueMessageRef(opaqueMessageRef);
    }
}

// anchor: launcher.exe:0x44d500
void CStreamPacketEncryptionModuleReadHelper::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    CMessageConnectionMessageRef* const inputMessageRef =
        static_cast<CMessageConnectionMessageRef*>(opaqueMessageRef);
    if (!inputMessageRef || transformWorkers.empty()) {
        collectionControl0c = 0u;
        ForwardToNextHelper(nullptr);
        return;
    }

    for (size_t workerIndex = 0; workerIndex < transformWorkers.size(); ++workerIndex) {
        if (!transformWorkers[workerIndex].TryTransform(*inputMessageRef, &transformedOutput)) {
            continue;
        }
        CStreamPacketEncryptionModuleReadHelper_RecordSuccessfulTransformIndex(
            this,
            workerIndex);
        ForwardToNextHelper(transformedOutput.MessageRef());
        return;
    }

    collectionControl0c = 0u;
    transformedOutput.Reset();
    ForwardToNextHelper(nullptr);
}

// anchor: launcher.exe:0x44da00 / 0x44daf0
void CStreamPacketEncryptionModuleReadHelper::ResetForOwner(
    CStreamPacketEncryptionModule* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    collectionControl0c = 0u;
    transformWorkers.clear();
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44da00 / 0x44daf0
void CStreamPacketEncryptionModuleReadHelper::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    collectionControl0c = 0u;
    transformWorkers.clear();
    transformWorkers.emplace_back();
    transformWorkers.back().ResetForSeed(seedBytes);
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44d390
void CStreamPacketEncryptionModuleWriteHelper::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    CMessageConnectionMessageRef* const inputMessageRef =
        static_cast<CMessageConnectionMessageRef*>(opaqueMessageRef);
    if (!inputMessageRef || !hasTransformWorker ||
        !transformWorker.TryTransform(*inputMessageRef, &transformedOutput)) {
        transformedOutput.Reset();
        ForwardToNextHelper(nullptr);
        return;
    }

    ForwardToNextHelper(transformedOutput.MessageRef());
}

// anchor: launcher.exe:0x44d820 / 0x44daf0
void CStreamPacketEncryptionModuleWriteHelper::ResetForOwner(
    CStreamPacketEncryptionModule* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    hasTransformWorker = false;
    transformWorker = {};
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44d820 / 0x44daf0
void CStreamPacketEncryptionModuleWriteHelper::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    hasTransformWorker = true;
    transformWorker.ResetForSeed(seedBytes);
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x469980
void CStreamPacketEncryptionAgendaHelper::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    // Original helper stores/replaces a retained outer message-ref through helper field `+0x10`.
    // Now that both receive-side and transform-output message-ref objects are heap-backed, source
    // can keep the nearer AddRef/Release handle semantics instead of a raw pointer overwrite.
    if (!outputSlotAddress10) {
        return;
    }
    CMessageConnectionMessageRefHandle_AssignRetained(
        reinterpret_cast<CMessageConnectionMessageRef**>(outputSlotAddress10),
        static_cast<CMessageConnectionMessageRef*>(opaqueMessageRef));
}

// anchor: launcher.exe:0x469850
void CStreamPacketEncryptionAgendaHelper::ResetForAgenda(
    const char* helperLabel,
    void** outputSlotAddress,
    CStreamPacketEncryptionHelperBase** downstreamHelperSlot) {
    nextHelper04 = nullptr;
    field04 = 0u;
    field08 = 0u;
    helperLabel0c = helperLabel;
    outputSlotAddress10 = outputSlotAddress;
    downstreamHelperSlot14 = downstreamHelperSlot;
}

// anchor: launcher.exe:0x44da00
void CStreamPacketEncryptionModule::InitializeForMarginConnectionSeed(
    const std::array<uint8_t, 16>& seedBytes85) {
    readHelper04 = nullptr;
    writeHelper08 = nullptr;
    nextConfiguredModule0c = nullptr;
    configuredConnection10 = nullptr;
    ownedReadHelper14.ResetForOwner(this);
    ownedWriteHelper2c.ResetForOwner(this);
    ownedReadHelper14.ResetForSeed(seedBytes85);
    ownedWriteHelper2c.ResetForSeed(seedBytes85);
    readHelper04 = &ownedReadHelper14;
    writeHelper08 = &ownedWriteHelper2c;
    associatedSeedBytes40 = seedBytes85;
}

// anchor: launcher.exe:0x44daf0
void CStreamPacketEncryptionModule::RefreshFromMarginConnectionSeed(
    const std::array<uint8_t, 16>& seedBytes85) {
    if (readHelper04 == nullptr) {
        readHelper04 = &ownedReadHelper14;
    }
    if (writeHelper08 == nullptr) {
        writeHelper08 = &ownedWriteHelper2c;
    }
    if (readHelper04) {
        readHelper04->owner08 = this;
        readHelper04->ResetForSeed(seedBytes85);
    }
    if (writeHelper08) {
        writeHelper08->owner08 = this;
        writeHelper08->ResetForSeed(seedBytes85);
    }
    associatedSeedBytes40 = seedBytes85;
}

// UNANCHORED: source-owned diagnostic stringifier for the recovered packet-name family enum.
const char* CMessageConnection::PacketNameFamilyToString(CMessageConnectionPacketNameFamily family) {
    switch (family) {
        case CMessageConnectionPacketNameFamily::kAuth:
            return "auth";
        case CMessageConnectionPacketNameFamily::kMargin:
            return "margin";
        default:
            return "unknown";
    }
}

// anchor family: launcher.exe:0x448960 -> connection `+0x70`
uintptr_t CMessageConnection::PacketNameCallbackAddressScaffold(
    CMessageConnectionPacketNameFamily family) {
    switch (family) {
        case CMessageConnectionPacketNameFamily::kAuth:
            return 0x0041ce00u;
        case CMessageConnectionPacketNameFamily::kMargin:
            return 0x0041ce40u;
        default:
            return 0u;
    }
}

// anchor: launcher.exe:0x448960
void CMessageConnection::ConfigurePacketNameFamily(
    CMessageConnectionPacketNameFamily family,
    bool packetizedMessagesEnabled) {
    packetNameCallback_ = PacketNameCallbackAddressScaffold(family);
    packetizedMessagesEnabled_ = packetizedMessagesEnabled;
}

// anchor family: launcher.exe:0x448960 -> connection `+0x70`
CMessageConnectionPacketNameFamily CMessageConnection::PacketNameFamily() const {
    switch (packetNameCallback_) {
        case 0x0041ce00u:
            return CMessageConnectionPacketNameFamily::kAuth;
        case 0x0041ce40u:
            return CMessageConnectionPacketNameFamily::kMargin;
        default:
            return CMessageConnectionPacketNameFamily::kUnknown;
    }
}

// anchor family: launcher.exe:0x448960 -> connection `+0x78`
bool CMessageConnection::PacketizedMessagesEnabled() const {
    return packetizedMessagesEnabled_;
}

// anchor: launcher.exe:0x448980
void CMessageConnection::ConfigurePacketAgenda(
    CStreamPacketEncryptionModule* streamPacketEncryptionModule) {
    if (!packetAgenda_) {
        packetAgenda_ = std::make_unique<CMessageConnectionPacketAgenda>();
    }
    if (!packetAgenda_) {
        return;
    }

    CMessageConnectionPacketAgenda& agenda = *packetAgenda_;
    if (!agenda.created) {
        // anchor: launcher.exe:0x448980 -> 0x469850
        // Faithful agenda initialization recovered from `0x469850`:
        // - agenda `+0x00` stores the owning connection pointer
        // - agenda `+0x04` starts with no configured modules
        // - embedded read helper at `+0x0c` stores into agenda `+0x08` and delegates via
        //   helper field `+0x14 -> &agenda+0x44`
        // - embedded write helper at `+0x28` stores into agenda `+0x24`
        // - agenda `+0x40` starts at the embedded read helper
        // - agenda `+0x44/+0x48` start empty
        agenda.connectionOwner00 = this;
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
        agenda.configuredStreamPacketEncryptionModule = nullptr;
    }

    // anchor: launcher.exe:0x448980 -> 0x469740
    // Current tighter install read from `0x469740`:
    // - module `+0x0c` becomes the next link in agenda `+0x04`
    // - module `+0x04` is pushed onto agenda read head `+0x40`
    // - module `+0x08` is appended onto the write chain rooted at agenda `+0x44/+0x48`
    // - module `+0x10` receives agenda `+0x00` (the owning connection pointer)
    if (!streamPacketEncryptionModule) {
        return;
    }

    streamPacketEncryptionModule->nextConfiguredModule0c = agenda.configuredModuleList04;
    agenda.configuredModuleList04 = streamPacketEncryptionModule;

    if (streamPacketEncryptionModule->readHelper04) {
        CStreamPacketEncryptionHelperBase* const previousReadHead = agenda.readHelperChainHead40;
        agenda.readHelperChainHead40 = streamPacketEncryptionModule->readHelper04;
        streamPacketEncryptionModule->readHelper04->nextHelper04 = previousReadHead;
    }

    if (streamPacketEncryptionModule->writeHelper08) {
        CStreamPacketEncryptionHelperBase* const previousWriteTail = agenda.writeHelperChainTail48;
        streamPacketEncryptionModule->writeHelper08->nextHelper04 =
            &agenda.embeddedWriteHelper28;
        agenda.writeHelperChainTail48 = streamPacketEncryptionModule->writeHelper08;
        if (previousWriteTail) {
            previousWriteTail->nextHelper04 = streamPacketEncryptionModule->writeHelper08;
        } else {
            agenda.writeHelperChainHead44 = streamPacketEncryptionModule->writeHelper08;
        }
    }

    streamPacketEncryptionModule->configuredConnection10 = agenda.connectionOwner00;
    agenda.configuredStreamPacketEncryptionModule = streamPacketEncryptionModule;
    ++agenda.configuredModuleCount4c;
}

// anchor family: launcher.exe:0x448980 -> connection `+0x74`
const CMessageConnectionPacketAgenda* CMessageConnection::PacketAgenda() const {
    return packetAgenda_.get();
}

static void CMessageConnection_ApplySendMessageRefMutations(
    CMessageConnectionMessageRef* messageRef) {
    if (!messageRef || messageRef->headerless10 == 0u || !messageRef->messageStorage0c) {
        return;
    }

    // anchor: launcher.exe:0x448cf0
    // The send-mode branch tests inner `+0x12` (`payload + 0x06`) and, only when that dword is
    // still zero, writes:
    // - inner `+0x12 .. +0x15` = 00 00 00 00
    // - inner `+0x16` = 0xff
    // - inner `+0x17` = 0xff
    CMessageConnectionMessageStorage& messageStorage = *messageRef->messageStorage0c;
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

// anchor: launcher.exe:0x448cf0 post-submit/or-discard cleanup on the original input message-ref
static void CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBit(
    CMessageConnectionMessageRef* messageRef) {
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

static void** CMessageConnection_PacketBuilderVtablePointerScaffold(uintptr_t address) {
    return reinterpret_cast<void**>(address);
}

// anchor: launcher.exe:0x41cf30
uint32_t CMessageConnection::ForwardPacketBuilderEnvelopeToSendPacket(
    CMessageConnectionPacketBuilderEnvelope& envelope) {
    if (!envelope.messageRef08) {
        return 0u;
    }
    return SendPacketMessageRef(*envelope.messageRef08);
}

// anchor: launcher.exe:0x469950
CMessageConnectionMessageRef* CMessageConnection::ApplySendPacketAgenda(
    CMessageConnectionMessageRef& inputMessageRef,
    bool* outAgendaTouched) {
    if (outAgendaTouched) {
        *outAgendaTouched = false;
    }

    // `0x448cf0` consults connection `+0x74` and may discard the packet before submit.
    // Current bounded source model preserves the nearer `0x469950`
    // (`CMessageConnectionPacketAgenda_DispatchWriteHelperChain`) handoff shape:
    // - no agenda / no active write helper (`+0x44 == 0`) => keep the original message-ref pointer
    // - active write helper => return agenda `+0x24` exactly after the helper chain runs
    // - original does not pre-clear agenda `+0x24`; it simply returns that slot after dispatch
    // Source now models that chain with real internal worker classes, keeping helper-side
    // replacement/discard visible at the same seam as the original.
    CMessageConnectionPacketAgenda* agenda = packetAgenda_.get();
    if (!agenda || !agenda->created) {
        return &inputMessageRef;
    }

    if (agenda->writeHelperChainHead44 == nullptr) {
        return &inputMessageRef;
    }

    agenda->writeHelperChainHead44->HandleOpaqueMessageRef(&inputMessageRef);
    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    return agenda->writeOutputSlot24;
}

// anchor: launcher.exe:0x448a00
// Narrow source-owned mirror of the lower submit helper beneath the original message-ref-based
// `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SubmitMessageRefBytes(
    const CMessageConnectionMessageRef& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return 0u;
    }

    const CMessageConnectionMessageStorage& messageStorage = *messageRef.messageStorage0c;
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
        "CMessageConnection::SubmitMessageRefBytes reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} submitOffset={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(messageStorage.reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<unsigned>(pointerOffsetFrom0a),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return CLTTCPConnection::SendBuffer(
        submittedBytes,
        submittedByteCount,
        reinterpret_cast<void*>(1u));
}

// anchor: launcher.exe:0x448cf0
// Narrow source-owned mirror of the message-ref-based `CMessageConnection::SendPacket` family.
uint32_t CMessageConnection::SendPacketMessageRef(
    CMessageConnectionMessageRef& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return 0u;
    }

    CMessageConnection_ApplySendMessageRefMutations(&messageRef);

    bool agendaTouched = false;
    CMessageConnectionMessageRef* const messageRefForSubmit =
        ApplySendPacketAgenda(messageRef, &agendaTouched);
    if (!messageRefForSubmit || !messageRefForSubmit->messageStorage0c) {
        spdlog::info(
            "CMessageConnection::SendPacketMessageRef discarded packet at packet-agenda write handoff this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBit(&messageRef);
        return 0u;
    }

    const CMessageConnectionMessageStorage& messageStorage = *messageRefForSubmit->messageStorage0c;
    const uint16_t payloadByteCount = messageStorage.PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage.PayloadBaseScaffold();
    const uint8_t rawOpcode = (payloadBytes && payloadByteCount != 0u) ? payloadBytes[0] : 0u;
    const uint32_t submittedByteCount =
        static_cast<uint32_t>(payloadByteCount) + ((payloadByteCount > 0x7fu) ? 2u : 1u);
    const CMessageConnectionPacketAgenda* agenda = PacketAgenda();
    spdlog::info(
        "CMessageConnection::SendPacketMessageRef sendMode10={} field08SkipPrefix={} rawOpcode=0x{:02x} reservedBytes08=0x{:04x} payloadBytes={} submittedBytes={} packetNameCallback=0x{:08x} packetNameFamily={} packetizedEnabled={} agendaCreated={} agendaModuleCount={} agendaHasReadHead={} agendaHasWriteHead={} agendaWriteTouched={} agendaWriteOutputSlot24={} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(messageRefForSubmit->headerless10),
        static_cast<unsigned>(messageRefForSubmit->field08),
        static_cast<unsigned>(rawOpcode),
        static_cast<unsigned>(messageStorage.reservedBytes08),
        static_cast<unsigned>(payloadByteCount),
        static_cast<unsigned>(submittedByteCount),
        static_cast<uint32_t>(packetNameCallback_),
        PacketNameFamilyToString(PacketNameFamily()),
        packetizedMessagesEnabled_ ? 1u : 0u,
        (agenda && agenda->created) ? 1u : 0u,
        agenda ? static_cast<unsigned>(agenda->configuredModuleCount4c) : 0u,
        (agenda && agenda->readHelperChainHead40 != nullptr) ? 1u : 0u,
        (agenda && agenda->writeHelperChainHead44 != nullptr) ? 1u : 0u,
        agendaTouched ? 1u : 0u,
        (agendaTouched && agenda && agenda->writeOutputSlot24 != nullptr) ? 1u : 0u,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());

    const uint32_t sendResult = SubmitMessageRefBytes(*messageRefForSubmit);
    CMessageConnection_ClearSendMessageRefFirstPayloadByteHighBit(&messageRef);
    return sendResult;
}

// anchor: launcher.exe:0x448a60
// UNANCHORED: source-owned narrow fallback helper for the generic unhandled-operation log branch
// reached from the later leaf wrappers (for example `0x449a70` / `0x44af60`) after base
// `0x4490c0` returns false-ish.
static void CMessageConnection_LogUnhandledOperationScaffold(void* workItem) {
    spdlog::debug(
        "CMessageConnection_LogUnhandledOperationScaffold workItem={}",
        fmt::ptr(workItem));
}

// UNANCHORED: source-owned typed owner-context view used by the current `0x4490c0/0x449a70/0x44af60`
// reconstruction when the launcher-owned connection owner at `+0xa4` is the direct login
// mediator. Current static-RE anchor for that ownership write is `0x41d170 / 0x41e500`.
static mxo::ltlogin::CLTLoginMediator* CMessageConnection_LoginMediatorOwnerScaffold(
    CMessageConnection* self) {
    if (!self) {
        return nullptr;
    }

    mxo::ltlogin::CLTLoginMediator* mediator =
        static_cast<mxo::ltlogin::CLTLoginMediator*>(self->OwnerContext());
    if (mediator != nullptr &&
        (self == mediator->AuthConnection() || self == mediator->MarginConnection())) {
        return mediator;
    }

    mediator = mxo::ltlogin::CLTLoginMediator::ActiveStateSourceScaffold();
    return (mediator != nullptr && self->OwnerContext() == mediator) ? mediator : nullptr;
}

static bool CMessageConnection_IsMediatorAuthConnectionScaffold(
    CMessageConnection* self,
    const mxo::ltlogin::CLTLoginMediator* mediator) {
    return self != nullptr && mediator != nullptr && self == mediator->AuthConnection();
}

static bool CMessageConnection_IsMediatorMarginConnectionScaffold(
    CMessageConnection* self,
    const mxo::ltlogin::CLTLoginMediator* mediator) {
    return self != nullptr && mediator != nullptr && self == mediator->MarginConnection();
}

// anchor family: launcher.exe:0x449a70 / 0x44af60 type-2 owner-fallback tail
static uint32_t CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold(
    CMessageConnection* self,
    uint32_t workPayload,
    bool isMarginConnection,
    const char* ownerSlotLabel) {
    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(self);
    const bool liveIsAuthConnection =
        CMessageConnection_IsMediatorAuthConnectionScaffold(self, mediator);
    const bool liveIsMarginConnection =
        CMessageConnection_IsMediatorMarginConnectionScaffold(self, mediator);
    if (!self || !mediator || (!liveIsAuthConnection && !liveIsMarginConnection) ||
        liveIsMarginConnection != isMarginConnection) {
        spdlog::debug(
            "CMessageConnection_HandleConnectionStatusOwnerCallbackScaffold missing mediator owner this={} ownerContext={} payload=0x{:08x} ownerSlot={} expectedMargin={}",
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

// anchor: launcher.exe:0x4490c0 -> 0x434d00
// Source-owned read of the shared `workItem+0x08` status/payload dword used by the type-3 early
// return and by several later source-owned owner-fallback helpers.
static uint32_t CMessageConnection_WorkItemStatusOrPayloadDwordScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const CLTThreadPerClientTCPEngine_WorkItemHeader* statusWorkItem =
        static_cast<const CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return statusWorkItem->statusOrPayloadDword08;
}

// anchor: launcher.exe:0x455cd0 / 0x455c60
// Source-owned creation/reset of the local outer message-ref scaffold that `0x4490c0`
// materializes before later `0x41bc20/0x41bbb0`-style message-code reads.

// anchor: launcher.exe:0x4557b0
// Source-owned append helper mirroring the `0x4490c0` copy order more closely:
// - resolve the current append pointer from the outer message-ref
// - copy bytes into that tail span
// - then commit the new payload length through outer grow helper `+0x18`
static bool CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
    CMessageConnectionMessageRef* messageRef,
    const uint8_t* payloadBytes,
    uint32_t payloadByteCount) {
    if (!messageRef || !messageRef->messageStorage0c) {
        return false;
    }
    if (payloadByteCount == 0u) {
        return true;
    }
    if (!payloadBytes || payloadByteCount > CMessageConnectionMessageStorage::kMaxPayloadByteCount) {
        return false;
    }

    const uint16_t oldPayloadByteCount = messageRef->PayloadByteCountScaffold();
    const uint32_t requestedPayloadByteCount =
        static_cast<uint32_t>(oldPayloadByteCount) + payloadByteCount;
    if (requestedPayloadByteCount > CMessageConnectionMessageStorage::kMaxPayloadByteCount) {
        return false;
    }

    uint8_t* const appendPointer = messageRef->PayloadAppendPointerScaffold();
    if (!appendPointer) {
        return false;
    }

    std::copy_n(payloadBytes, payloadByteCount, appendPointer);
    const uint16_t newPayloadByteCount =
        messageRef->GrowPayloadByteCountScaffold(static_cast<uint16_t>(payloadByteCount));
    return newPayloadByteCount == requestedPayloadByteCount;
}

// anchor: launcher.exe:0x4490c0 type-3 parsed-packet body copy path
// Source-owned decomposition of the packet-body extraction that copies from
// `CParsedPacketWorkItem.currentCursor24` / `assembledByteCount28` into the later outer
// receive/message-ref scaffold.
// Current source now follows the original retained-fragment walk more closely:
// - `0x4350c0 = CParsedPacketWorkItem_BeginFragmentTraversal`
// - `0x435510 = CParsedPacketWorkItem_GetNextFragment`
// - `0x434fa0 = CLTTCPReadOperationRefHandle_AssignRetained`
static bool CMessageConnection_CopyParsedPacketIntoReceivedMessageRefScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CMessageConnectionMessageRef* outMessageRef,
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

    CLTTCPReadOperationRefHandle currentFragmentRef{};
    CLTTCPReadOperation* currentFragment =
        CParsedPacketWorkItem_BeginFragmentTraversal(
            workItem,
            &currentFragmentRef)->retainedFragment00;
    if (!currentFragment || !workItem->currentCursor24) {
        CLTTCPReadOperation* clearedFragment = nullptr;
        (void)CLTTCPReadOperationRefHandle_AssignRetained(
            &currentFragmentRef,
            &clearedFragment);
        return false;
    }

    const uint8_t* fragmentBegin =
        reinterpret_cast<const uint8_t*>(currentFragment + 1);
    const uint8_t* fragmentEnd = fragmentBegin + currentFragment->byteCount08;
    const uint8_t* currentCursor = workItem->currentCursor24;
    if (currentCursor < fragmentBegin || currentCursor > fragmentEnd) {
        CLTTCPReadOperation* clearedFragment = nullptr;
        (void)CLTTCPReadOperationRefHandle_AssignRetained(
            &currentFragmentRef,
            &clearedFragment);
        return false;
    }

    const uint32_t firstCopyByteCount = std::min<uint32_t>(
        packetBodyByteCount,
        static_cast<uint32_t>(fragmentEnd - currentCursor));
    if (!CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
            outMessageRef,
            currentCursor,
            firstCopyByteCount)) {
        CLTTCPReadOperation* clearedFragment = nullptr;
        (void)CLTTCPReadOperationRefHandle_AssignRetained(
            &currentFragmentRef,
            &clearedFragment);
        return false;
    }

    uint32_t remainingPacketBodyByteCount = packetBodyByteCount - firstCopyByteCount;
    CLTTCPReadOperationRefHandle nextFragmentRef{};
    CLTTCPReadOperation* nextFragment =
        CParsedPacketWorkItem_GetNextFragment(
            workItem,
            &nextFragmentRef)->retainedFragment00;
    (void)CLTTCPReadOperationRefHandle_AssignRetained(&currentFragmentRef, &nextFragment);
    if (nextFragment) {
        nextFragment->Release();
    }
    currentFragment = currentFragmentRef.retainedFragment00;
    while (currentFragment) {
        if (remainingPacketBodyByteCount == 0u) {
            if (outHadUnusedBuffers) {
                *outHadUnusedBuffers = true;
            }
            break;
        }

        const uint32_t copyByteCount =
            std::min<uint32_t>(remainingPacketBodyByteCount, currentFragment->byteCount08);
        if (!CMessageConnection_AppendReceiveMessagePayloadSpanScaffold(
                outMessageRef,
                reinterpret_cast<const uint8_t*>(currentFragment + 1),
                copyByteCount)) {
            CLTTCPReadOperation* clearedFragment = nullptr;
            (void)CLTTCPReadOperationRefHandle_AssignRetained(
                &currentFragmentRef,
                &clearedFragment);
            return false;
        }
        remainingPacketBodyByteCount -= copyByteCount;

        CLTTCPReadOperationRefHandle laterFragmentRef{};
        CLTTCPReadOperation* laterFragment =
            CParsedPacketWorkItem_GetNextFragment(
                workItem,
                &laterFragmentRef)->retainedFragment00;
        (void)CLTTCPReadOperationRefHandle_AssignRetained(&currentFragmentRef, &laterFragment);
        if (laterFragment) {
            laterFragment->Release();
        }
        currentFragment = currentFragmentRef.retainedFragment00;
    }

    CLTTCPReadOperation* clearedFragment = nullptr;
    (void)CLTTCPReadOperationRefHandle_AssignRetained(
        &currentFragmentRef,
        &clearedFragment);
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

// anchor family: launcher.exe:0x41bc20 / 0x41bbb0 headerless-locator message-code decode
static bool CMessageConnection_ResolveMessageCodePointerScaffold(
    const CMessageConnectionMessageRef& messageRef,
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

    const CMessageConnectionMessageStorage* const messageStorage = messageRef.messageStorage0c;
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

// anchor family: launcher.exe:0x41bc20 / 0x41bbb0 decoded message-id read from a message-ref
static bool CMessageConnection_DecodeMessageCodeScaffold(
    const CMessageConnectionMessageRef& messageRef,
    uint16_t* outMessageCode,
    bool* outUsedHeaderlessLocatorDecode) {
    if (outMessageCode) {
        *outMessageCode = 0u;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = false;
    }

    const CMessageConnectionMessageStorage* const messageStorage = messageRef.messageStorage0c;
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

// anchor: launcher.exe:0x442d00
// Source-owned narrow predicate exposing the specific consumed-code gate inside
// `CBaseMarginConnection::DispatchMessage`.
static uint32_t CBaseMarginConnection_DispatchMessageFilterScaffold(
    const CMessageConnectionMessageRef& messageRef,
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
// drive the read-helper chain and let the eventual embedded agenda helper store the final
// message-ref pointer into agenda `+0x08`.
// With the internal-only family now modeled as full virtual C++ classes, source routes this
// through the helper-chain head directly, so module read helpers can now replace or discard the
// message-ref before the embedded helper stores the output slot.
static bool CMessageConnection_DriveAgendaReadHelperChainScaffold(
    CMessageConnectionPacketAgenda* agenda,
    CMessageConnectionMessageRef* inputMessageRef) {
    if (!agenda || agenda->readHelperChainHead40 == nullptr) {
        return false;
    }
    agenda->readHelperChainHead40->HandleOpaqueMessageRef(inputMessageRef);
    return agenda->readOutputSlot08 != nullptr;
}

// anchor: launcher.exe:0x469930
// Source-owned mirror of the read-side packet-agenda handoff just before leaf dispatch.
// Current bounded source model:
// - a created agenda always has the embedded default read helper at `+0x0c`
// - source now owns that helper's concrete agenda `+0x08` output-slot effect
// - source keeps the nearer `0x4489d0` seam as a raw pointer handoff instead of deep-copying a
//   source-owned message-ref owner tail first
// - caller-installed read helpers can now replace or discard the message-ref through the recovered
//   module family, though the exact original worker object/lifetime split is still narrower than
//   source
// - original `0x469930` (`CMessageConnectionPacketAgenda_DispatchReadHelperChain`) does not
//   pre-clear agenda `+0x08`; it simply drives the chain and returns the slot afterward
static CMessageConnectionMessageRef* CMessageConnection_ApplyReceivePacketAgendaScaffold(
    CMessageConnectionPacketAgenda* agenda,
    CMessageConnectionMessageRef* inputMessageRef,
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

    if (outAgendaTouched) {
        *outAgendaTouched = true;
    }
    if (!CMessageConnection_DriveAgendaReadHelperChainScaffold(agenda, inputMessageRef)) {
        return nullptr;
    }

    return agenda->readOutputSlot08;
}

// anchor: launcher.exe:0x4490c0 packetized branch reads inner `+0x0f & 0x07`
static bool CMessageConnection_ResolvePacketizedProtocolIdScaffold(
    const CMessageConnectionMessageRef& messageRef,
    uint8_t* outProtocolId) {
    if (outProtocolId) {
        *outProtocolId = 0u;
    }

    const CMessageConnectionMessageStorage* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    if (!payloadBytes || payloadByteCount < 4u) {
        return false;
    }

    if (outProtocolId) {
        *outProtocolId = static_cast<uint8_t>(payloadBytes[3] & 0x07u);
    }
    return true;
}

}  // namespace

// UNANCHORED: source-owned post-copy dispatch seam beneath `launcher.exe:0x4490c0`.
uint32_t CMessageConnection::DispatchCopiedParsedPacketTailScaffold(
    CMessageConnectionMessageRef& messageRef) {
    (void)messageRef;
    return 0u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x30`
uint32_t CMessageConnection::DispatchPacketizedProtocol5MessageRefScaffold(
    CMessageConnectionMessageRef& messageRef) {
    (void)messageRef;
    return 1u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x34`
uint32_t CMessageConnection::DispatchPacketizedProtocol7MessageRefScaffold(
    CMessageConnectionMessageRef& messageRef) {
    (void)messageRef;
    return 1u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x38`
void CMessageConnection::PreDispatchMessageRefScaffold(
    CMessageConnectionMessageRef& messageRef) {
    (void)messageRef;
}

// anchor: launcher.exe:0x4490c0
// string-backed original name: CMessageConnection::OnOperationCompleted
// Current source body now mirrors the tighter current boundary read:
// - the initial `workItem+0x04` dispatch only directly handles work types `1`, `2`, and `3`
// - work types `1/2` are the optional completion-helper signal branches (`+0x80/+0x7c`)
//   - current source still does not materialize those helper objects, so it preserves only the
//     original false-ish return boundary seen on the auth/margin startup path where they are null
// - every other non-type-3 work item returns false-ish to the later leaf wrappers unchanged
//   instead of being consumed or generically logged by base `0x4490c0`
// - on type `3`, the callback first checks shared `workItem+0x08` status/payload through the
//   `0x434d00` helper family and returns handled immediately when that dword is non-zero
// - otherwise it copies packet-body bytes from the retained-fragment-backed parsed-packet work item
//   via `currentCursor24` / `assembledByteCount28`
// - preserve the original oversized-packet close branch before later dispatch/agenda work
// - materialize the nearer local receive/message-ref scaffold mirroring
//   `0x455cd0/0x455c60` outer-ref + inner-storage construction
// - keep the original locator-id validity gate on the non-zero-flag branch before later dispatch
// - preserve the optional `0x469930 -> 0x4489d0` read-agenda handoff
// - the later in-callback virtual tail consumes only that local message-ref object:
//   - call the vtable `+0x38` pre-dispatch hook first
//   - non-zero-flag protocol `5` -> vtable `+0x30`
//   - non-zero-flag protocol `7` -> vtable `+0x34`
//   - zero-flag/simple path and all other cases -> vtable `+0x2c`
// - crucial current fidelity correction:
//   - once original `0x4490c0` reaches that virtual dispatch family, it still consumes the packet
//     locally inside the same callback even when the callee returns false-ish
//   - source-owned synthetic receive-drain and local type-`0x0b` continuations therefore belong to
//     later compatibility / leaf-owner fallback scaffolding, not to base `0x4490c0`
uint32_t CMessageConnection::OnOperationCompleted(void* workItem) {
    if (!Engine() || !workItem) {
        return 0u;
    }

    const uint32_t workType = CMessageConnection_WorkItemTypeScaffold(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        return 0u;
    }
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        return 0u;
    }
    if (workType != CLTThreadPerClientTCPEngine::kWorkTypeParsedPacket) {
        return 0u;
    }
    if (CMessageConnection_WorkItemStatusOrPayloadDwordScaffold(workItem) != 0u) {
        return 1u;
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold* parsedPacketWorkItem =
        static_cast<CLTTCPConnection_ParsedPacketWorkItemScaffold*>(workItem);
    if (parsedPacketWorkItem->assembledByteCount28 > 0x1000u) {
        spdlog::info(
            "CMessageConnection::OnOperationCompleted received illegally large packet payloadBytes={} this={} ownerContext={} remoteHost='{}' -> closing",
            static_cast<unsigned>(parsedPacketWorkItem->assembledByteCount28),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        (void)Close(false);
        lastReceivedPacketBodyBytesScaffold_.clear();
        lastReceivedPacketHeaderlessScaffold_ = !packetizedMessagesEnabled_;
        return 1u;
    }

    bool hadUnusedBuffers = false;
    std::unique_ptr<CMessageConnectionMessageRef, CMessageConnectionMessageRefReleaseDeleter>
        ownedCopiedMessageRef(new CMessageConnectionMessageRef());
    CMessageConnectionMessageRef* const copiedMessageRef = ownedCopiedMessageRef.get();
    copiedMessageRef->ResetForPacketBuilderScaffold(!packetizedMessagesEnabled_);
    const bool copied = CMessageConnection_CopyParsedPacketIntoReceivedMessageRefScaffold(
        parsedPacketWorkItem,
        copiedMessageRef,
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
    if (const CMessageConnectionMessageStorage* const messageStorage = copiedMessageRef->messageStorage0c) {
        const uint16_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
        if (const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
            payloadBytes && payloadByteCount != 0u) {
            lastReceivedPacketBodyBytesScaffold_.assign(
                payloadBytes,
                payloadBytes + payloadByteCount);
        }
    }
    lastReceivedPacketHeaderlessScaffold_ = (copiedMessageRef->headerless10 != 0u);

    if (lastReceivedPacketHeaderlessScaffold_) {
        uint8_t targetLocatorType = 0u;
        uint8_t senderLocatorType = 0u;
        if (!CMessageConnection_ResolveMessageCodePointerScaffold(
                *copiedMessageRef,
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

    CMessageConnectionMessageRef* messageRefForDispatch = copiedMessageRef;
    bool agendaTouched = false;
    if (CMessageConnectionPacketAgenda* agenda = packetAgenda_.get();
        agenda && agenda->created) {
        messageRefForDispatch = CMessageConnection_ApplyReceivePacketAgendaScaffold(
            agenda,
            copiedMessageRef,
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
            if (const CMessageConnectionMessageStorage* const messageStorage = messageRefForDispatch->messageStorage0c) {
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
                 static_cast<const CStreamPacketEncryptionHelperBase*>(&agenda->embeddedReadHelper0c))
                    ? 1u
                    : 0u,
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        }
    }

    PreDispatchMessageRefScaffold(*messageRefForDispatch);

    const bool headerlessForDispatch = (messageRefForDispatch->headerless10 != 0u);
    uint8_t headerlessProtocolId = 0u;
    const bool hasHeaderlessProtocolId =
        headerlessForDispatch &&
        CMessageConnection_ResolvePacketizedProtocolIdScaffold(
            *messageRefForDispatch,
            &headerlessProtocolId);

    const char* dispatchSlotLabel = "+0x2c";
    uint32_t postCopyDispatchResult = 0u;
    if (hasHeaderlessProtocolId && headerlessProtocolId == 5u) {
        dispatchSlotLabel = "+0x30";
        postCopyDispatchResult =
            DispatchPacketizedProtocol5MessageRefScaffold(*messageRefForDispatch);
    } else if (hasHeaderlessProtocolId && headerlessProtocolId == 7u) {
        dispatchSlotLabel = "+0x34";
        postCopyDispatchResult =
            DispatchPacketizedProtocol7MessageRefScaffold(*messageRefForDispatch);
    } else {
        postCopyDispatchResult =
            DispatchCopiedParsedPacketTailScaffold(*messageRefForDispatch);
    }

    if (postCopyDispatchResult == 0u) {
        spdlog::debug(
            "CMessageConnection::OnOperationCompleted mirrored original local type-3 consume after dispatchSlot={} returned false-ish payloadBytes={} headerless={} headerlessProtocolValid={} headerlessProtocol={} this={} ownerContext={} remoteHost='{}'",
            dispatchSlotLabel,
            static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
            headerlessForDispatch ? 1u : 0u,
            hasHeaderlessProtocolId ? 1u : 0u,
            static_cast<unsigned>(headerlessProtocolId),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    }

    spdlog::info(
        "CMessageConnection::OnOperationCompleted copied parsed packet body payloadBytes={} headerless={} retainedFragmentCount={} headerlessProtocolValid={} headerlessProtocol={} dispatchSlot={} and handled it on the in-callback post-copy tail this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(lastReceivedPacketBodyBytesScaffold_.size()),
        headerlessForDispatch ? 1u : 0u,
        static_cast<unsigned>(parsedPacketWorkItem->retainedFragmentCount0C),
        hasHeaderlessProtocolId ? 1u : 0u,
        static_cast<unsigned>(headerlessProtocolId),
        dispatchSlotLabel,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return 1u;
}

// anchor family: launcher.exe:0x449d20
// Source-owned bool-return wrapper over the inherited `CLTTCPConnection_SendBuffer` path.
// Keep it distinct from the message-ref send family rooted at
// `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00`.
uint32_t CMessageConnection::SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext) {
    if (!Engine() || !packetData || packetByteCount == 0) {
        return 0;
    }

    // Current starter path deliberately routes through the inherited recovered
    // `CLTTCPConnection::SendBuffer` wrapper instead of pretending this is already a faithful
    // packet serializer.
    return CLTTCPConnection::SendBuffer(packetData, packetByteCount, completionContext);
}

// anchor family: launcher.exe:0x449cd0
// Source-owned bool-return wrapper over the inherited `CLTTCPConnection_Connect` path.
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
    CMessageConnectionMessageRef& messageRef) {

    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    const CMessageConnectionMessageStorage* const messageStorage = messageRef.messageStorage0c;
    if (!mediator || !CMessageConnection_IsMediatorAuthConnectionScaffold(this, mediator) ||
        !messageStorage) {
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

    // Tightened `0x449a70 -> 0x41af80` read:
    // - after base `0x4490c0` returns 0, the auth leaf does not split type-2 status work away to
    //   a different owner helper
    // - it always falls through owner `+0x17c`, and that owner callback then re-enters current
    //   helper slot 1 / vtable `+0x00`
    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    if (mediator && CMessageConnection_IsMediatorAuthConnectionScaffold(this, mediator)) {
        const uint32_t handled =
            mediator->HandleAuthConnectionCompletionFallbackScaffold(this, workItem);
        if (handled != 0u) {
            return 1u;
        }
    }

    CMessageConnection_LogUnhandledOperationScaffold(workItem);
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

// anchor family: launcher.exe:0x441850 / 0x44af20 -> connection `+0x84`
bool CMarginConnection::MessageCode4SuccessFlag84() const {
    return messageCode4SuccessFlag84_;
}

// anchor: launcher.exe:0x441850
uint32_t CMarginConnection::DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus) {
    CMarginConnectionLocalCompletionWorkItemScaffold workItem = {};
    workItem.header.vtable = CMarginConnection_LocalCompletionWorkItemVtableScaffold();
    workItem.header.workType = 0x0bu;
    workItem.header.statusOrPayloadDword08 = workPayloadStatus;

    CMessageConnection* selfAsMessageConnection = this;
    const uint32_t handled = selfAsMessageConnection->OnOperationCompleted(&workItem);
    spdlog::info(
        "CMarginConnection::DispatchMessageCode4LocalCompletionWorkItem synthesized local type0x0b workItem status=0x{:08x} handled={} this={} ownerContext={} currentState={} remoteHost='{}'",
        workPayloadStatus,
        handled,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(CMessageConnection_LoginMediatorOwnerScaffold(this)
                     ? CMessageConnection_LoginMediatorOwnerScaffold(this)->CurrentState()
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
    constexpr uintptr_t kPacketBuilderVtable00 = 0x004b6524u;

    mxo::liblttcp::CMessageConnectionPacketBuilderPayloadWithReservationScaffold builder = {};
    CMessageConnectionMessageRef messageRef = {};
    messageRef.ResetForPacketBuilderScaffold(/*headerless=*/false);
    if (!messageRef.messageStorage0c) {
        return 0u;
    }

    builder.builder00.envelope00.vtable00 =
        CMessageConnection_PacketBuilderVtablePointerScaffold(kPacketBuilderVtable00);
    builder.builder00.envelope00.payloadBase04 =
        messageRef.messageStorage0c->PayloadBaseScaffold();
    builder.builder00.envelope00.messageRef08 = &messageRef;
    builder.builder00.builderFlag0c = 0u;
    builder.builder00.packetPayload10 = builder.builder00.envelope00.payloadBase04;
    if (!builder.builder00.packetPayload10) {
        return 0u;
    }

    // Tightened local builder mirror from `0x441f30` / vtable `0x004b6524`:
    // - raw builder `+0x10` is the packet payload base
    // - helper `0x43a230(0x136)` then reserves `(0x136 + 2)` bytes at the tail, caches the
    //   concrete copy target in builder `+0x14`, and stores reserved content byte count at `+0x18`
    messageRef.messageStorage0c->ResetPayloadByteCountScaffold(kLeadingType1PrefixByteCount);
    builder.builder00.packetPayload10[0] = 0x01u;
    builder.builder00.packetPayload10[1] = 0u;
    builder.builder00.packetPayload10[2] = 0u;

    const uint16_t currentPayloadByteCount =
        messageRef.messageStorage0c->PayloadByteCountScaffold();
    uint8_t* const reservationHeader =
        builder.builder00.envelope00.payloadBase04 + currentPayloadByteCount;
    const uint16_t requestedGrowth =
        static_cast<uint16_t>(kReplyCopyByteCount + sizeof(uint16_t));
    const uint16_t newPayloadByteCount =
        messageRef.messageStorage0c->GrowPayloadByteCountScaffold(requestedGrowth);
    if (!reservationHeader ||
        newPayloadByteCount != currentPayloadByteCount + requestedGrowth) {
        return 0u;
    }

    reservationHeader[0] = static_cast<uint8_t>(kReplyCopyByteCount & 0xffu);
    reservationHeader[1] = static_cast<uint8_t>((kReplyCopyByteCount >> 8) & 0xffu);
    builder.reservation14.writePointer00 = reservationHeader + 2u;
    builder.reservation14.reservedContentByteCount04 = kReplyCopyByteCount;
    std::copy(
        bootstrapReplyCopy98_.begin(),
        bootstrapReplyCopy98_.end(),
        builder.reservation14.writePointer00);

    const uint32_t sendResult =
        ForwardPacketBuilderEnvelopeToSendPacket(builder.builder00.envelope00);
    spdlog::info(
        "CMarginConnection::SendStoredBootstrapReplyCopy98 sent packetBuilderVtable=0x{:08x} payloadBase10={} reservedReplyCopyBytes=0x{:03x} totalPayloadBytes=0x{:03x} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(kPacketBuilderVtable00),
        fmt::ptr(builder.builder00.packetPayload10),
        static_cast<unsigned>(builder.reservation14.reservedContentByteCount04),
        static_cast<unsigned>(messageRef.messageStorage0c->PayloadByteCountScaffold()),
        static_cast<unsigned>(sendResult),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return sendResult;
}

// anchor: launcher.exe:0x4429b0 / 0x439840 / 0x41cf30
uint32_t CMarginConnection::SendCertChallengeResponseFromChallengeBytes(
    const std::array<uint8_t, 16>& challengeBytes) {
    if (!hasMessageCode5SeedBytes85_) {
        return 0u;
    }

    // Original `0x4429b0` writes `+0x85..+0x94`, then immediately reaches
    // `0x441470 = CBaseMarginConnection_EnsureStreamPacketEncryptionModule` before building the
    // local envelope and sending through connection vtable `+0x24`.
    EnsureStreamPacketEncryptionModuleFromSeed85();
    const CMessageConnectionPacketAgenda* const agenda = PacketAgenda();
    if (!agenda || !agenda->created || agenda->writeHelperChainHead44 == nullptr) {
        spdlog::info(
            "CMarginConnection::SendCertChallengeResponseFromChallengeBytes missing agenda write helper after seed ensure this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
        return 0u;
    }

    constexpr uint16_t kPayloadByteCount = 0x11u;
    constexpr uintptr_t kPacketBuilderVtable00 = 0x004b6560u;
    CMessageConnectionPacketBuilderPayloadScaffold builder = {};
    CMessageConnectionMessageRef messageRef = {};
    messageRef.ResetForPacketBuilderScaffold(/*headerless=*/false);
    if (!messageRef.messageStorage0c) {
        return 0u;
    }

    builder.envelope00.vtable00 =
        CMessageConnection_PacketBuilderVtablePointerScaffold(kPacketBuilderVtable00);
    builder.envelope00.payloadBase04 = messageRef.messageStorage0c->PayloadBaseScaffold();
    builder.envelope00.messageRef08 = &messageRef;
    builder.builderFlag0c = 0u;
    builder.packetPayload10 = builder.envelope00.payloadBase04;
    if (!builder.packetPayload10) {
        return 0u;
    }

    messageRef.messageStorage0c->ResetPayloadByteCountScaffold(kPayloadByteCount);
    builder.packetPayload10[0] = 0x03u;
    std::copy_n(
        challengeBytes.begin(),
        challengeBytes.size(),
        builder.packetPayload10 + 1u);

    const uint32_t sendResult =
        ForwardPacketBuilderEnvelopeToSendPacket(builder.envelope00);
    spdlog::info(
        "CMarginConnection::SendCertChallengeResponseFromChallengeBytes sent packetBuilderVtable=0x{:08x} packetPayload10={} challengeBytes=0x{:02x} agendaModuleCount={} agendaHasWriteHead={} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(kPacketBuilderVtable00),
        fmt::ptr(builder.packetPayload10),
        static_cast<unsigned>(challengeBytes.size()),
        static_cast<unsigned>(agenda->configuredModuleCount4c),
        agenda->writeHelperChainHead44 != nullptr ? 1u : 0u,
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

    const bool needsInitialInstall = (streamPacketEncryptionModule9c_ == nullptr);
    if (needsInitialInstall) {
        streamPacketEncryptionModule9c_ =
            std::make_unique<CStreamPacketEncryptionModule>();
    }
    if (!streamPacketEncryptionModule9c_) {
        ConfigurePacketAgenda(nullptr);
        return;
    }

    if (needsInitialInstall) {
        streamPacketEncryptionModule9c_->InitializeForMarginConnectionSeed(
            messageCode5SeedBytes85_);
        ConfigurePacketAgenda(streamPacketEncryptionModule9c_.get());
    } else {
        // Preserve the already-installed agenda links on the original `0x44daf0` refresh path.
        streamPacketEncryptionModule9c_->RefreshFromMarginConnectionSeed(
            messageCode5SeedBytes85_);
    }

    spdlog::info(
        "CMarginConnection::EnsureStreamPacketEncryptionModuleFromSeed85 {} connection+0x9c module from seed85_94 module={} agenda={} firstDword=0x{:08x} this={} ownerContext={} remoteHost='{}'",
        needsInitialInstall ? "installed" : "refreshed",
        fmt::ptr(streamPacketEncryptionModule9c_.get()),
        fmt::ptr(PacketAgenda()),
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

// anchor family: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
const uint8_t* CMarginConnection::MessageCode5SeedBytes85Pointer() const {
    return hasMessageCode5SeedBytes85_ ? messageCode5SeedBytes85_.data() : nullptr;
}

// UNANCHORED: source-owned post-copy seam beneath `launcher.exe:0x4490c0`.
// Keep this wrapper thin: the original leaf-specific margin routing belongs to
// `CMarginConnection::DispatchMessage` (`0x44af20`), not to the base copied-message-ref seam.
uint32_t CMarginConnection::DispatchCopiedParsedPacketTailScaffold(
    CMessageConnectionMessageRef& messageRef) {
    return DispatchMessage(&messageRef);
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

    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    const bool isMarginConnection =
        CMessageConnection_IsMediatorMarginConnectionScaffold(this, mediator);

    const uint32_t workType = CMessageConnection_WorkItemTypeScaffold(workItem);
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        spdlog::info(
            "CMarginConnection::OnOperationCompleted close work this={} ownerContext={} mediator={} isMarginConnection={} currentState={} connectionState={} remoteHost='{}'",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator),
            isMarginConnection ? 1u : 0u,
            fmt::ptr(mediator ? mediator->CurrentState() : nullptr),
            static_cast<unsigned>(State()),
            RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    }
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        const uint32_t workPayload =
            CMessageConnection_WorkItemStatusOrPayloadDwordScaffold(workItem);

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

        if (isMarginConnection && mediator) {
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

    if (isMarginConnection && mediator) {
        const uint32_t handled =
            mediator->HandleMarginConnectionCompletionFallbackScaffold(this, workItem);
        if (handled != 0u) {
            return 1u;
        }
    }

    CMessageConnection_LogUnhandledOperationScaffold(workItem);
    return 0u;
}

// anchor: launcher.exe:0x44af20
// Later leaf dispatch override on top of `CBaseMarginConnection::DispatchMessage`.
// Current source keeps the original ownership split closer than the earlier post-copy seam did:
// - base decoded-code `2/4/5` handling is modeled here as the `0x442d00`-side filter/consumer
// - only the surviving path re-enters owner vtable `+0x184 / 0x41f260`
uint32_t CMarginConnection::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    auto& copiedMessageRef = *static_cast<CMessageConnectionMessageRef*>(messageRef);
    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    const CMessageConnectionMessageStorage* const messageStorage = copiedMessageRef.messageStorage0c;
    if (!mediator || !CMessageConnection_IsMediatorMarginConnectionScaffold(this, mediator) ||
        !messageStorage) {
        return 0u;
    }

    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    const size_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
        return 0u;
    }

    const bool headerless = (copiedMessageRef.headerless10 != 0u);
    const uint8_t* messageCodePointer = nullptr;
    const bool resolvedMessageCodePointer = CMessageConnection_ResolveMessageCodePointerScaffold(
        copiedMessageRef,
        &messageCodePointer,
        /*outTargetLocatorType=*/nullptr,
        /*outSenderLocatorType=*/nullptr,
        /*outUsedHeaderlessLocatorDecode=*/nullptr);

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    bool hadValidMessageCode = false;
    (void)CBaseMarginConnection_DispatchMessageFilterScaffold(
        copiedMessageRef,
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
                    "CMarginConnection::DispatchMessage source-owned local code{} branch rawCode=0x{:02x} headerless={} locatorDecoded={} {}={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
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
                    "CMarginConnection::DispatchMessage source-owned local code{} branch rawCode=0x{:02x} headerless={} locatorDecoded={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
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
            "CMarginConnection::DispatchMessage source-owned local code5 branch rawCode=0x{:02x} headerless={} locatorDecoded={} storedConnectionSeed85_94=1 firstDword=0x{:08x} this={} ownerContext={} currentState={} remoteHost='{}'",
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

    mediator->stagedIncomingMarginPacketBytes_.assign(payloadBytes, payloadBytes + payloadByteCount);
    ++mediator->marginPacketReceiveCountScaffold_;
    mediator->lastMarginPacketOpcodeScaffold_ = rawCode;
    mediator->lastMarginPacketSizeScaffold_ = static_cast<uint32_t>(payloadByteCount);

    const uint32_t bootstrapHandled =
        mediator->ContinueMarginBootstrapHandshake(payloadBytes, payloadByteCount, /*transportEncrypted=*/false);
    if (bootstrapHandled != 0u) {
        spdlog::info(
            "CMarginConnection::DispatchMessage rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} messageRef={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(decodedMessageCode),
            hadValidMessageCode ? 1u : 0u,
            headerless ? 1u : 0u,
            usedHeaderlessLocatorDecode ? 1u : 0u,
            static_cast<unsigned>(payloadByteCount),
            fmt::ptr(messageRef),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator->CurrentState()),
            bootstrapHandled,
            remoteHostForLog);
        return bootstrapHandled;
    }

    if (rawCode == 2u || rawCode == 4u || rawCode == 5u) {
        ++mediator->marginPacketFilteredBeforeSlot6CountScaffold_;
        spdlog::info(
            "CMarginConnection::DispatchMessage rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} messageRef={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(decodedMessageCode),
            hadValidMessageCode ? 1u : 0u,
            headerless ? 1u : 0u,
            usedHeaderlessLocatorDecode ? 1u : 0u,
            static_cast<unsigned>(payloadByteCount),
            fmt::ptr(messageRef),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator->CurrentState()),
            1u,
            remoteHostForLog);
        return 1u;
    }

    uint32_t handled = 0u;
    if (mediator->CurrentState() != nullptr) {
        ++mediator->marginPacketSlot6DispatchCountScaffold_;
        handled = mediator->DispatchCurrentHelperSlot6(messageRef);
    }
    spdlog::info(
        "CMarginConnection::DispatchMessage rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} messageRef={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(decodedMessageCode),
        hadValidMessageCode ? 1u : 0u,
        headerless ? 1u : 0u,
        usedHeaderlessLocatorDecode ? 1u : 0u,
        static_cast<unsigned>(payloadByteCount),
        fmt::ptr(messageRef),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(mediator->CurrentState()),
        handled,
        remoteHostForLog);
    return handled;
}

}  // namespace mxo::liblttcp
