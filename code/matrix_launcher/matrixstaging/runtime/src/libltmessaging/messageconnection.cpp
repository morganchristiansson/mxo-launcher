#include "messageconnection.h"

#include "../../../game/src/libltclientlogin/loginmediator.h"
#include "../libltbase/ltresult.h"
#include "../libltcrypto/auth_crypto.h"
#include "../libltcrypto/auth_internal.h"
#include "variablelengthprefixedtcpstreamparser.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
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

namespace {

static void** CMessageConnection_CompletionHelperVtableScaffold() {
    // anchor: launcher.exe vtable `0x004b3e20`
    static void* vtable[1] = {nullptr};
    return vtable;
}

}  // namespace

CMessageConnectionCompletionHelperScaffold::CMessageConnectionCompletionHelperScaffold() {
    vtable00 = CMessageConnection_CompletionHelperVtableScaffold();
    std::memset(&embeddedLockHelper04.crit, 0, sizeof(embeddedLockHelper04.crit));
    InitializeCriticalSection(&embeddedLockHelper04.crit);
    eventHandle20 = CreateEventA(nullptr, FALSE, FALSE, nullptr);
}

CMessageConnectionCompletionHelperScaffold::~CMessageConnectionCompletionHelperScaffold() {
    if (eventHandle20 != nullptr) {
        CloseHandle(eventHandle20);
        eventHandle20 = nullptr;
    }
    DeleteCriticalSection(&embeddedLockHelper04.crit);
    vtable00 = nullptr;
}

void CMessageConnectionCompletionHelperScaffold::Signal() {
    if (eventHandle20 != nullptr) {
        SetEvent(eventHandle20);
    }
}

DWORD CMessageConnectionCompletionHelperScaffold::Wait(uint32_t timeoutMs) const {
    return eventHandle20 != nullptr ? WaitForSingleObject(eventHandle20, timeoutMs) : WAIT_FAILED;
}

// UNANCHORED: source-owned narrow subset of `0x448b40` with a null engine and without the
// optional completion-helper allocation path.
CMessageConnection::CMessageConnection()
    : CLTTCPConnection(),
      packetNameCallback_(0),
      packetizedMessagesEnabled_(false),
      connectCompletionHelper7c_(),
      closeCompletionHelper80_(),
      packetAgenda_() {}

// UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
CMessageConnection::CMessageConnection(
    CLTThreadPerClientTCPEngine* engine,
    bool allocateCompletionHelpers)
    : CLTTCPConnection(),
      packetNameCallback_(0),
      packetizedMessagesEnabled_(false),
      connectCompletionHelper7c_(),
      closeCompletionHelper80_(),
      packetAgenda_() {
    CLTTCPConnection::SetEngine(engine);
    if (allocateCompletionHelpers) {
        connectCompletionHelper7c_ =
            std::make_unique<CMessageConnectionCompletionHelperScaffold>();
        closeCompletionHelper80_ =
            std::make_unique<CMessageConnectionCompletionHelperScaffold>();
    }
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
    hasConfiguredFeedbackTransform =
        feedbackTransform.FeedbackSizeTransformAdapter_ConstructLarge(
            associatedSeedBytes.data(),
            static_cast<uint32_t>(associatedSeedBytes.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data(),
            0u);
}

// anchor: launcher.exe:0x44d500
bool CStreamPacketEncryptionModuleReadTransformWorker::TryTransform(
    const CMessageConnectionMessageRef& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) {
    // Source still keeps the confirmed packet semantic at the worker boundary here, but the
    // recovered large/decrypting `FeedbackSize` adapter constructed by `0x44d910` is now held as a
    // real object alongside that packet-level behavior instead of being collapsed away entirely.
    if (!outputBuffer || !hasConfiguredFeedbackTransform) {
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
    hasConfiguredFeedbackTransform =
        feedbackTransform.FeedbackSizeTransformAdapter_ConstructSmall(
            associatedSeedBytes.data(),
            static_cast<uint32_t>(associatedSeedBytes.size()),
            mxo::auth::internal::FeedbackSizeTransformAdapterZeroIv().data(),
            0u);
}

// anchor: launcher.exe:0x44d390
bool CStreamPacketEncryptionModuleWriteTransformWorker::TryTransform(
    const CMessageConnectionMessageRef& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) {
    // Source still keeps the confirmed packet semantic at the worker boundary here, but the
    // recovered small/encrypting `FeedbackSize` adapter constructed by `0x44d820` is now held as a
    // real object alongside that packet-level behavior instead of being collapsed away entirely.
    if (!outputBuffer || !hasConfiguredFeedbackTransform) {
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
// Current tighter source mirror of the generic unhandled-operation log branch reached from later
// leaf wrappers (for example `0x449a70` / `0x44af60`) after base `0x4490c0` returns false-ish.
static void CMessageConnection_LogUnhandledOperationScaffold(void* workItem) {
    const auto* header =
        static_cast<const CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    const uint32_t workType = header ? header->workType : 0u;
    const uint32_t resultCode = header ? header->statusOrPayloadDword08 : 0u;
    const char* resultName = mxo::libltbase::CResultNameArrayItem_GetResultName(resultCode);
    spdlog::info(
        "Got unhandled op of type {} with status {}",
        static_cast<unsigned>(workType),
        resultName ? resultName : "UNKNOWN_LTRESULT");
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

struct CBaseMarginConnectionParsedPayloadSpanScaffold {
    const uint8_t* logicalPayloadBytes00 = nullptr;
    size_t logicalPayloadByteCount04 = 0u;
    bool headerless08 = false;
    bool usedHeaderlessLocatorDecode09 = false;
};

struct CBaseMarginConnectionCode2MessageScaffold {
    CBaseMarginConnectionParsedPayloadSpanScaffold parsedPayload00{};
};

struct CBaseMarginConnectionCode4MessageScaffold {
    CBaseMarginConnectionParsedPayloadSpanScaffold parsedPayload00{};
    uint32_t statusOrPayload0c = 0u;
};

struct CBaseMarginConnectionCode5MessageScaffold {
    CBaseMarginConnectionParsedPayloadSpanScaffold parsedPayload00{};
    std::array<uint8_t, 16> seedBytes0c{};
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

static bool CBaseMarginConnection_ResolveLogicalPayloadSpanScaffold(
    const CMessageConnectionMessageRef& messageRef,
    CBaseMarginConnectionParsedPayloadSpanScaffold* outParsedPayload) {
    if (outParsedPayload) {
        *outParsedPayload = {};
    }
    if (!outParsedPayload) {
        return false;
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
            &outParsedPayload->usedHeaderlessLocatorDecode09) ||
        !messageCodePointer) {
        return false;
    }

    const size_t logicalPayloadOffset =
        static_cast<size_t>(messageCodePointer - payloadBytes);
    if (logicalPayloadOffset >= payloadByteCount) {
        return false;
    }

    outParsedPayload->logicalPayloadBytes00 = messageCodePointer;
    outParsedPayload->logicalPayloadByteCount04 =
        static_cast<size_t>(payloadByteCount) - logicalPayloadOffset;
    outParsedPayload->headerless08 = (messageRef.headerless10 != 0u);
    return true;
}

static bool CBaseMarginConnection_OnMessageCode2Scaffold(
    const CMessageConnectionMessageRef& messageRef,
    CBaseMarginConnectionCode2MessageScaffold* outCode2Message,
    bool parseIncomingMessage) {
    if (outCode2Message) {
        *outCode2Message = {};
    }
    if (!parseIncomingMessage || !outCode2Message ||
        !CBaseMarginConnection_ResolveLogicalPayloadSpanScaffold(
            messageRef,
            &outCode2Message->parsedPayload00)) {
        return false;
    }

    const auto& parsedPayload = outCode2Message->parsedPayload00;
    return parsedPayload.logicalPayloadByteCount04 >= 1u &&
           parsedPayload.logicalPayloadBytes00[0] == 2u;
}

static bool CBaseMarginConnection_OnMessageCode4Scaffold(
    const CMessageConnectionMessageRef& messageRef,
    CBaseMarginConnectionCode4MessageScaffold* outCode4Message,
    bool parseIncomingMessage) {
    if (outCode4Message) {
        *outCode4Message = {};
    }
    if (!parseIncomingMessage || !outCode4Message ||
        !CBaseMarginConnection_ResolveLogicalPayloadSpanScaffold(
            messageRef,
            &outCode4Message->parsedPayload00)) {
        return false;
    }

    const auto& parsedPayload = outCode4Message->parsedPayload00;
    if (parsedPayload.logicalPayloadByteCount04 < 5u ||
        parsedPayload.logicalPayloadBytes00[0] != 4u) {
        return false;
    }

    std::memcpy(
        &outCode4Message->statusOrPayload0c,
        parsedPayload.logicalPayloadBytes00 + 1u,
        sizeof(outCode4Message->statusOrPayload0c));
    return true;
}

static bool CBaseMarginConnection_OnMessageCode5Scaffold(
    const CMessageConnectionMessageRef& messageRef,
    CBaseMarginConnectionCode5MessageScaffold* outCode5Message,
    bool parseIncomingMessage) {
    if (outCode5Message) {
        *outCode5Message = {};
    }
    if (!parseIncomingMessage || !outCode5Message ||
        !CBaseMarginConnection_ResolveLogicalPayloadSpanScaffold(
            messageRef,
            &outCode5Message->parsedPayload00)) {
        return false;
    }

    const auto& parsedPayload = outCode5Message->parsedPayload00;
    if (parsedPayload.logicalPayloadByteCount04 < 0x11u ||
        parsedPayload.logicalPayloadBytes00[0] != 5u) {
        return false;
    }

    std::copy_n(
        parsedPayload.logicalPayloadBytes00 + 1u,
        outCode5Message->seedBytes0c.size(),
        outCode5Message->seedBytes0c.begin());
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
        if (closeCompletionHelper80_) {
            closeCompletionHelper80_->Signal();
            return 1u;
        }
        return 0u;
    }
    if (workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        if (connectCompletionHelper7c_) {
            connectCompletionHelper7c_->Signal();
            return 1u;
        }
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
// VTable 0x004b64a8 - shared base margin router
// ============================================================
CBaseMarginConnection::CBaseMarginConnection()
    : CMessageConnection() {}

CBaseMarginConnection::CBaseMarginConnection(CLTThreadPerClientTCPEngine* connectionEngine)
    : CMessageConnection(connectionEngine) {}

CBaseMarginConnection::~CBaseMarginConnection() = default;

uint32_t CBaseMarginConnection::DispatchCopiedParsedPacketTailScaffold(
    CMessageConnectionMessageRef& messageRef) {
    return DispatchMessage(&messageRef);
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
    : CBaseMarginConnection() {}

CAuthStartupConnection::CAuthStartupConnection(CLTThreadPerClientTCPEngine* authEngine)
    : CBaseMarginConnection(authEngine) {}

CAuthStartupConnection::~CAuthStartupConnection() = default;

// anchor: launcher.exe:0x449a30 -> owner vtable `+0x180` / `0x41f250`
uint32_t CAuthStartupConnection::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    if (CBaseMarginConnection::DispatchMessage(messageRef) != 0u) {
        return 1u;
    }

    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    if (!mediator || !CMessageConnection_IsMediatorAuthConnectionScaffold(this, mediator)) {
        return 0u;
    }

    const uint32_t handled = mediator->DispatchCurrentHelperAuthMessage(messageRef);
    spdlog::info(
        "CAuthStartupConnection::DispatchMessage forwarded unconsumed messageRef={} to owner+0x180 currentState={} handled={} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(messageRef),
        fmt::ptr(mediator->CurrentState()),
        handled,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return handled;
}

// anchor: launcher.exe:0x449a70
// Current tighter read from the direct `0x449a70` decompile/listing:
// - call base `0x4490c0`
// - if base returns 0, call owner `+0x17c / 0x41af80`
// - if that also returns 0, fall through to `0x448a60`
// - only after that handled/unhandled decision, read `workItem+0x04`
// - if work type == 1, tear down through the connection object
// - there is no leaf-local type-2 split here; auth connect-status also flows through owner `+0x17c`
uint32_t CAuthStartupConnection::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    uint32_t handled = 0u;
    if (CMessageConnection::OnOperationCompleted(workItem) != 0u) {
        handled = 1u;
    } else {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
        if (mediator && CMessageConnection_IsMediatorAuthConnectionScaffold(this, mediator) &&
            mediator->HandleAuthConnectionCompletionFallback(this, workItem) != 0u) {
            handled = 1u;
        } else {
            CMessageConnection_LogUnhandledOperationScaffold(workItem);
        }
    }

    if (CMessageConnection_WorkItemTypeScaffold(workItem) ==
        CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        // anchor: launcher.exe:0x449a70 tail
        // The original tail calls vtable[0](1), i.e. the deleting-dtor-style teardown path,
        // not the ordinary `Close(bool)` wrapper.
        delete this;
    }

    return handled;
}

// ============================================================
// VTable 0x004aff38 - CMarginConnection
// ============================================================
// Later leaf on top of:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection-family base surface
//     └── CBaseMarginConnection (0x004b64a8)
//         └── CMarginConnection (0x004aff38)

// UNANCHORED: source-owned narrow leaf ctor.
CMarginConnection::CMarginConnection()
    : CBaseMarginConnection() {}

// UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
CMarginConnection::CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine)
    : CBaseMarginConnection(marginEngine) {}

// UNANCHORED: source-owned default destructor.
// Current tighter static-RE split:
// - live leaf teardown is through the scalar-deleting-dtor wrappers at `0x41cf50/0x41cf80`
// - `0x41ce80` is the separate connection `+0x98` reply-copy helper, not this C++ destructor body
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
void CBaseMarginConnection::SetMessageCode4SuccessFlag84(bool value) {
    messageCode4SuccessFlag84_ = value;
}

// anchor family: launcher.exe:0x441850 / 0x44af20 -> connection `+0x84`
bool CBaseMarginConnection::MessageCode4SuccessFlag84() const {
    return messageCode4SuccessFlag84_;
}

// anchor: launcher.exe:0x441850
uint32_t CBaseMarginConnection::DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus) {
    CMarginConnectionLocalCompletionWorkItemScaffold workItem = {};
    workItem.header.vtable = CMarginConnection_LocalCompletionWorkItemVtableScaffold();
    workItem.header.workType = 0x0bu;
    workItem.header.statusOrPayloadDword08 = workPayloadStatus;

    CMessageConnection* selfAsMessageConnection = this;
    const uint32_t handled = selfAsMessageConnection->OnOperationCompleted(&workItem);
    spdlog::info(
        "CBaseMarginConnection::DispatchMessageCode4LocalCompletionWorkItem synthesized local type0x0b workItem status=0x{:08x} handled={} this={} ownerContext={} currentState={} remoteHost='{}'",
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
bool CBaseMarginConnection::StoreBootstrapReplyCopy98(const void* bytes, size_t byteCount) {
    if (!bytes || byteCount != bootstrapReplyCopy98_.size()) {
        return false;
    }

    std::copy_n(
        static_cast<const uint8_t*>(bytes),
        bootstrapReplyCopy98_.size(),
        bootstrapReplyCopy98_.begin());
    hasBootstrapReplyCopy98_ = true;
    spdlog::info(
        "CBaseMarginConnection::StoreBootstrapReplyCopy98 stored reply-copy block bytes=0x{:03x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(bootstrapReplyCopy98_.size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return true;
}

namespace {

struct CMarginConnectionBootstrapPrepStateA0OwnedState {
    std::vector<uint32_t> field_0x8OwnedDigits;
    std::vector<uint32_t> field_0x1cOwnedDigits;
    std::vector<uint32_t> field_0x3cOwnedDigits;
    std::vector<uint32_t> field_0x50OwnedDigits;
    std::vector<uint32_t> field_0x64OwnedDigits;
    std::vector<uint32_t> mbr_0x78OwnedDigits;
    std::vector<uint32_t> mbr_0x8cOwnedDigits;
    std::vector<uint32_t> mbr_0xa0OwnedDigits;
};

static std::unordered_map<const CMarginConnectionBootstrapPrepStateA0Scaffold*, CMarginConnectionBootstrapPrepStateA0OwnedState>
    g_marginConnectionBootstrapPrepStateA0OwnedStateByObject;

static CMarginConnectionBootstrapPrepStateA0OwnedState& MutableCMarginConnectionBootstrapPrepStateA0OwnedState(
    const CMarginConnectionBootstrapPrepStateA0Scaffold* object) {
    return g_marginConnectionBootstrapPrepStateA0OwnedStateByObject[object];
}

static void ReleaseCMarginConnectionBootstrapPrepStateA0OwnedState(
    const CMarginConnectionBootstrapPrepStateA0Scaffold* object) {
    g_marginConnectionBootstrapPrepStateA0OwnedStateByObject.erase(object);
}

static uint32_t RoundCMarginConnectionBootstrapPrepBigIntCapacityWords(size_t requiredWordCount) {
    if (requiredWordCount == 0u) {
        return 2u;
    }
    if (requiredWordCount < 3u) {
        return 2u;
    }
    if (requiredWordCount < 5u) {
        return 4u;
    }
    if (requiredWordCount < 9u) {
        return 8u;
    }
    if (requiredWordCount < 0x11u) {
        return 0x10u;
    }
    if (requiredWordCount < 0x21u) {
        return 0x20u;
    }
    if (requiredWordCount < 0x41u) {
        return 0x40u;
    }

    uint32_t rounded = 1u;
    while (rounded < requiredWordCount && rounded < 0x80000000u) {
        rounded <<= 1u;
    }
    return rounded;
}

static void ResetCMarginConnectionBootstrapPrepBigIntObject(
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    uint32_t mbr_0x4 = 0u,
    uint32_t mbr_0x10 = 0u) {
    if (!outObject || !ownedDigits) {
        return;
    }

    ownedDigits->assign(2u, 0u);
    outObject->vftptr_0x0 = 0x004ba50cu;
    outObject->mbr_0x4 = mbr_0x4;
    outObject->mbr_0x8 = 2u;
    outObject->mbr_0xc = ownedDigits->data();
    outObject->mbr_0x10 = mbr_0x10;
}

static bool CopyCMarginConnectionBootstrapPrepBigIntObject(
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* sourceObject) {
    if (!outObject || !ownedDigits || !sourceObject) {
        return false;
    }

    const uint32_t wordCapacity = sourceObject->mbr_0x8;
    const auto* sourceDigits = static_cast<const uint32_t*>(sourceObject->mbr_0xc);
    ownedDigits->assign(static_cast<size_t>(std::max<uint32_t>(wordCapacity, 2u)), 0u);
    if (wordCapacity != 0u && sourceDigits != nullptr) {
        std::copy_n(sourceDigits, wordCapacity, ownedDigits->data());
    }

    outObject->vftptr_0x0 = sourceObject->vftptr_0x0 != 0u ? sourceObject->vftptr_0x0 : 0x004ba50cu;
    outObject->mbr_0x4 = sourceObject->mbr_0x4;
    outObject->mbr_0x8 = static_cast<uint32_t>(ownedDigits->size());
    outObject->mbr_0xc = ownedDigits->data();
    outObject->mbr_0x10 = sourceObject->mbr_0x10;
    return true;
}

static CryptoPP::Integer CMarginConnectionBootstrapPrepBigIntObjectToInteger(
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c& object) {
    const auto* digits = static_cast<const uint32_t*>(object.mbr_0xc);
    if (!digits || object.mbr_0x8 == 0u) {
        return CryptoPP::Integer::Zero();
    }

    size_t usedWordCount = object.mbr_0x8;
    while (usedWordCount != 0u && digits[usedWordCount - 1u] == 0u) {
        --usedWordCount;
    }

    CryptoPP::Integer value = CryptoPP::Integer::Zero();
    for (size_t i = usedWordCount; i != 0u; --i) {
        value <<= 32u;
        value += digits[i - 1u];
    }
    return value;
}

static bool BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
    CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* outObject,
    std::vector<uint32_t>* ownedDigits,
    const CryptoPP::Integer& value) {
    if (!outObject || !ownedDigits) {
        return false;
    }

    const size_t encodedByteCount = static_cast<size_t>(value.MinEncodedSize());
    std::vector<uint8_t> encodedBytes(std::max<size_t>(encodedByteCount, 1u), 0u);
    value.Encode(encodedBytes.data(), encodedBytes.size());

    const size_t requiredWordCount = (encodedBytes.size() + 3u) / 4u;
    const uint32_t roundedWordCapacity =
        RoundCMarginConnectionBootstrapPrepBigIntCapacityWords(requiredWordCount);
    ownedDigits->assign(static_cast<size_t>(roundedWordCapacity), 0u);
    for (size_t i = 0; i < encodedBytes.size(); ++i) {
        const size_t reversedIndex = encodedBytes.size() - 1u - i;
        const size_t wordIndex = reversedIndex / 4u;
        const size_t byteShift = (reversedIndex & 3u) * 8u;
        (*ownedDigits)[wordIndex] |= static_cast<uint32_t>(encodedBytes[i]) << byteShift;
    }

    outObject->vftptr_0x0 = 0x004ba50cu;
    outObject->mbr_0x4 = 0u;
    outObject->mbr_0x8 = roundedWordCapacity;
    outObject->mbr_0xc = ownedDigits->empty() ? nullptr : ownedDigits->data();
    outObject->mbr_0x10 = 0u;
    return true;
}

}  // namespace

// anchor: launcher.exe:0x465d70
void CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70::InitializeFromBootstrapBlocks(
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_1,
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_2,
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_3) {
    auto* owner = reinterpret_cast<CMarginConnectionBootstrapPrepStateA0Scaffold*>(
        reinterpret_cast<uint8_t*>(this) - offsetof(CMarginConnectionBootstrapPrepStateA0Scaffold, field_0xc));
    auto& ownedState = MutableCMarginConnectionBootstrapPrepStateA0OwnedState(owner);

    field_0xb4 = 0x004b630cu;
    field_0xb8 = 0x004af294u;

    CopyCMarginConnectionBootstrapPrepBigIntObject(&field_0x8, &ownedState.field_0x8OwnedDigits, param_1);
    CopyCMarginConnectionBootstrapPrepBigIntObject(&field_0x1c, &ownedState.field_0x1cOwnedDigits, param_2);
    CopyCMarginConnectionBootstrapPrepBigIntObject(&field_0x3c, &ownedState.field_0x3cOwnedDigits, param_3);
    ResetCMarginConnectionBootstrapPrepBigIntObject(&field_0x50, &ownedState.field_0x50OwnedDigits);
    ResetCMarginConnectionBootstrapPrepBigIntObject(&field_0x64, &ownedState.field_0x64OwnedDigits);
    ResetCMarginConnectionBootstrapPrepBigIntObject(&mbr_0x78, &ownedState.mbr_0x78OwnedDigits);
    ResetCMarginConnectionBootstrapPrepBigIntObject(&mbr_0x8c, &ownedState.mbr_0x8cOwnedDigits);
    ResetCMarginConnectionBootstrapPrepBigIntObject(&mbr_0xa0, &ownedState.mbr_0xa0OwnedDigits);

    if (!param_1 || !param_2 || !param_3) {
        return;
    }

    try {
        const CryptoPP::Integer modulus = CMarginConnectionBootstrapPrepBigIntObjectToInteger(field_0x8);
        const CryptoPP::Integer publicExponent =
            CMarginConnectionBootstrapPrepBigIntObjectToInteger(field_0x1c);
        const CryptoPP::Integer privateExponent =
            CMarginConnectionBootstrapPrepBigIntObjectToInteger(field_0x3c);
        if (modulus.IsZero() || publicExponent.IsZero() || privateExponent.IsZero()) {
            return;
        }

        CryptoPP::RSA::PrivateKey privateKey;
        privateKey.Initialize(modulus, publicExponent, privateExponent);
        BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
            &field_0x50,
            &ownedState.field_0x50OwnedDigits,
            privateKey.GetPrime1());
        BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
            &field_0x64,
            &ownedState.field_0x64OwnedDigits,
            privateKey.GetPrime2());
        BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
            &mbr_0x78,
            &ownedState.mbr_0x78OwnedDigits,
            privateKey.GetModPrime1PrivateExponent());
        BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
            &mbr_0x8c,
            &ownedState.mbr_0x8cOwnedDigits,
            privateKey.GetModPrime2PrivateExponent());
        BuildCMarginConnectionBootstrapPrepBigIntObjectFromInteger(
            &mbr_0xa0,
            &ownedState.mbr_0xa0OwnedDigits,
            privateKey.GetMultiplicativeInverseOfPrime2ModPrime1());
    } catch (const CryptoPP::Exception& exception) {
        spdlog::warn(
            "CMarginConnectionBootstrapPrepStateSubobject0c_0x465d70::InitializeFromBootstrapBlocks CryptoPP key reconstruction failed: {}",
            exception.what());
    }
}

// anchor: launcher.exe:0x443220 / constructor reached from `0x443340`
CMarginConnectionBootstrapPrepStateA0Scaffold::CMarginConnectionBootstrapPrepStateA0Scaffold(
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_1,
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_2,
    const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c* param_3,
    int param_4) {
    if (param_4 != 0) {
        mbr_0x4 = 0x004b6b10u;
        field_0xd4 = 0x004b6300u;
        cls_0x4b3e18 = 0x004b3e18u;
        field_0xdc = 0x004b67a0u;
    }

    vftptr_0x0 = 0x004b6ae0u;
    mbr_0x8 = 0x004b6ad4u;
    field_0xd0 = 0x004b6accu;
    field_0xc.field_0xb4 = 0x004b630cu;
    field_0xc.field_0xb8 = 0x004af294u;
    field_0xc.InitializeFromBootstrapBlocks(param_1, param_2, param_3);
}

// anchor: launcher.exe:0x443390
CMarginConnectionBootstrapPrepStateA0Scaffold::~CMarginConnectionBootstrapPrepStateA0Scaffold() {
    ReleaseCMarginConnectionBootstrapPrepStateA0OwnedState(this);
    vftptr_0x0 = 0x004af29cu;
    field_0xc.field_0xb4 = 0x004b630cu;
    field_0xc.field_0xb8 = 0x004af294u;
    field_0xd4 = 0x004b6300u;
}

// anchor: launcher.exe:0x443340 -> connection `+0xa0`
// State5 only constructs/stores this object. The first later original consumer is
// `0x4429b0`, which loads connection `+0xa0` and calls prep-object vtable
// `+0x1c / 0x437810`.
void CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0(
    const void* blockB0,
    const void* blockC4,
    const void* blockD8) {
    const auto* param_1 =
        static_cast<const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c*>(blockB0);
    const auto* param_2 =
        static_cast<const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c*>(blockC4);
    const auto* param_3 =
        static_cast<const CMarginConnectionBootstrapPrepBigIntObject20_0x4ba50c*>(blockD8);

    connection_.bootstrapPrepStateA0_.reset(new (std::nothrow)
        CMarginConnectionBootstrapPrepStateA0Scaffold(param_1, param_2, param_3, 1));
    if (!connection_.bootstrapPrepStateA0_) {
        spdlog::warn(
            "CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0 allocation failed this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(&connection_),
            fmt::ptr(connection_.OwnerContext()),
            connection_.RemoteHostName().empty() ? std::string("<empty>") : connection_.RemoteHostName());
        return;
    }

    const auto* prepState = connection_.bootstrapPrepStateA0_.get();
    spdlog::info(
        "CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0 stored connection+0xa0 prep object size=0x{:02x} modulusCap=0x{:02x} exponentCap=0x{:02x} privateExponentCap=0x{:02x} prime1Cap=0x{:02x} prime2Cap=0x{:02x} crtExp1Cap=0x{:02x} crtExp2Cap=0x{:02x} crtInverseCap=0x{:02x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(sizeof(CMarginConnectionBootstrapPrepStateA0Scaffold)),
        static_cast<unsigned>(prepState->field_0xc.field_0x8.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.field_0x1c.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.field_0x3c.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.field_0x50.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.field_0x64.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.mbr_0x78.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.mbr_0x8c.mbr_0x8),
        static_cast<unsigned>(prepState->field_0xc.mbr_0xa0.mbr_0x8),
        fmt::ptr(&connection_),
        fmt::ptr(connection_.OwnerContext()),
        connection_.RemoteHostName().empty() ? std::string("<empty>") : connection_.RemoteHostName());
}

// anchor: launcher.exe:0x41f30
uint32_t CBaseMarginConnection::SendStoredBootstrapReplyCopy98() {
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
        "CBaseMarginConnection::SendStoredBootstrapReplyCopy98 sent packetBuilderVtable=0x{:08x} payloadBase10={} reservedReplyCopyBytes=0x{:03x} totalPayloadBytes=0x{:03x} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
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
uint32_t CBaseMarginConnection::SendCertChallengeResponseFromChallengeBytes(
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
            "CBaseMarginConnection::SendCertChallengeResponseFromChallengeBytes missing agenda write helper after seed ensure this={} ownerContext={} remoteHost='{}'",
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
        "CBaseMarginConnection::SendCertChallengeResponseFromChallengeBytes sent packetBuilderVtable=0x{:08x} packetPayload10={} challengeBytes=0x{:02x} agendaModuleCount={} agendaHasWriteHead={} sendResult=0x{:08x} this={} ownerContext={} remoteHost='{}'",
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
void CBaseMarginConnection::EnsureStreamPacketEncryptionModuleFromSeed85() {
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
        "CBaseMarginConnection::EnsureStreamPacketEncryptionModuleFromSeed85 {} connection+0x9c module from seed85_94 module={} agenda={} firstDword=0x{:08x} this={} ownerContext={} remoteHost='{}'",
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
void CBaseMarginConnection::SetMessageCode5SeedBytes85(const std::array<uint8_t, 16>& value) {
    messageCode5SeedBytes85_ = value;
    hasMessageCode5SeedBytes85_ = true;
    EnsureStreamPacketEncryptionModuleFromSeed85();
}

// anchor family: launcher.exe:0x4429b0 / 0x441470 / 0x442d00 -> connection `+0x85 .. +0x94`
const uint8_t* CBaseMarginConnection::MessageCode5SeedBytes85Pointer() const {
    return hasMessageCode5SeedBytes85_ ? messageCode5SeedBytes85_.data() : nullptr;
}

// anchor: launcher.exe:0x442d00
uint32_t CBaseMarginConnection::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    auto& copiedMessageRef = *static_cast<CMessageConnectionMessageRef*>(messageRef);
    const CMessageConnectionMessageStorage* const messageStorage = copiedMessageRef.messageStorage0c;
    if (!messageStorage) {
        return 0u;
    }

    const uint8_t* const payloadBytes = messageStorage->PayloadBaseScaffold();
    const size_t payloadByteCount = messageStorage->PayloadByteCountScaffold();
    if (!payloadBytes || payloadByteCount == 0u) {
        return 0u;
    }

    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
    const bool marginOwnerPath =
        mediator && CMessageConnection_IsMediatorMarginConnectionScaffold(this, mediator);

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    bool hadValidMessageCode = false;
    (void)CBaseMarginConnection_DispatchMessageFilterScaffold(
        copiedMessageRef,
        &decodedMessageCode,
        &usedHeaderlessLocatorDecode,
        &hadValidMessageCode);
    if (!hadValidMessageCode) {
        return 0u;
    }

    const std::string remoteHostForLog =
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName();
    if (decodedMessageCode == 2u) {
        CBaseMarginConnectionCode2MessageScaffold code2Message = {};
        const bool parsedCode2 =
            CBaseMarginConnection_OnMessageCode2Scaffold(
                copiedMessageRef,
                &code2Message,
                /*parseIncomingMessage=*/true);
        const uint8_t* const logicalPayloadBytes =
            parsedCode2 ? code2Message.parsedPayload00.logicalPayloadBytes00 : payloadBytes;
        const size_t logicalPayloadByteCount =
            parsedCode2 ? code2Message.parsedPayload00.logicalPayloadByteCount04 : payloadByteCount;
        const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
        uint32_t handledCode2 = 0u;
        if (marginOwnerPath) {
            handledCode2 = mediator->HandleMarginConsumedCode2AtConnectionSeamScaffold(
                logicalPayloadBytes,
                logicalPayloadByteCount,
                /*transportEncrypted=*/false);
        }
        spdlog::info(
            "CBaseMarginConnection::DispatchMessage consumed code2 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode2={} logicalPayloadBytes={} marginOwnerPath={} handledCode2={} this={} ownerContext={} currentState={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            parsedCode2 && code2Message.parsedPayload00.headerless08 ? 1u : 0u,
            parsedCode2 && code2Message.parsedPayload00.usedHeaderlessLocatorDecode09 ? 1u : usedHeaderlessLocatorDecode ? 1u : 0u,
            parsedCode2 ? 1u : 0u,
            static_cast<unsigned>(logicalPayloadByteCount),
            marginOwnerPath ? 1u : 0u,
            static_cast<unsigned>(handledCode2),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator ? mediator->CurrentState() : nullptr),
            remoteHostForLog);
        return 1u;
    }

    if (decodedMessageCode == 4u) {
        CBaseMarginConnectionCode4MessageScaffold code4Message = {};
        const bool parsedCode4 =
            CBaseMarginConnection_OnMessageCode4Scaffold(
                copiedMessageRef,
                &code4Message,
                /*parseIncomingMessage=*/true);
        const uint8_t* const logicalPayloadBytes =
            parsedCode4 ? code4Message.parsedPayload00.logicalPayloadBytes00 : payloadBytes;
        const size_t logicalPayloadByteCount =
            parsedCode4 ? code4Message.parsedPayload00.logicalPayloadByteCount04 : payloadByteCount;
        const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
        uint32_t handledCode4 = 0u;
        if (marginOwnerPath) {
            handledCode4 = mediator->HandleMarginConsumedCode4AtConnectionSeamScaffold(
                logicalPayloadBytes,
                logicalPayloadByteCount,
                /*transportEncrypted=*/false);
        }
        spdlog::info(
            "CBaseMarginConnection::DispatchMessage consumed code4 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode4={} logicalPayloadBytes={} status=0x{:08x} marginOwnerPath={} handledCode4={} connectionByte84={} this={} ownerContext={} currentState={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            parsedCode4 && code4Message.parsedPayload00.headerless08 ? 1u : 0u,
            parsedCode4 && code4Message.parsedPayload00.usedHeaderlessLocatorDecode09 ? 1u : usedHeaderlessLocatorDecode ? 1u : 0u,
            parsedCode4 ? 1u : 0u,
            static_cast<unsigned>(logicalPayloadByteCount),
            static_cast<unsigned>(parsedCode4 ? code4Message.statusOrPayload0c : 0u),
            marginOwnerPath ? 1u : 0u,
            static_cast<unsigned>(handledCode4),
            MessageCode4SuccessFlag84() ? 1u : 0u,
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator ? mediator->CurrentState() : nullptr),
            remoteHostForLog);
        return 1u;
    }

    if (decodedMessageCode == 5u) {
        CBaseMarginConnectionCode5MessageScaffold code5Message = {};
        const bool parsedCode5 =
            CBaseMarginConnection_OnMessageCode5Scaffold(
                copiedMessageRef,
                &code5Message,
                /*parseIncomingMessage=*/true);
        const uint8_t* const logicalPayloadBytes =
            parsedCode5 ? code5Message.parsedPayload00.logicalPayloadBytes00 : payloadBytes;
        const size_t logicalPayloadByteCount =
            parsedCode5 ? code5Message.parsedPayload00.logicalPayloadByteCount04 : payloadByteCount;
        const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
        if (parsedCode5) {
            SetMessageCode5SeedBytes85(code5Message.seedBytes0c);
            spdlog::info(
                "CBaseMarginConnection::DispatchMessage consumed code5 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode5=1 logicalPayloadBytes={} storedConnectionSeed85_94=1 firstDword=0x{:08x} this={} ownerContext={} currentState={} remoteHost='{}'",
                static_cast<unsigned>(rawCode),
                code5Message.parsedPayload00.headerless08 ? 1u : 0u,
                code5Message.parsedPayload00.usedHeaderlessLocatorDecode09 ? 1u : 0u,
                static_cast<unsigned>(logicalPayloadByteCount),
                static_cast<unsigned>(
                    static_cast<uint32_t>(code5Message.seedBytes0c[0]) |
                    (static_cast<uint32_t>(code5Message.seedBytes0c[1]) << 8u) |
                    (static_cast<uint32_t>(code5Message.seedBytes0c[2]) << 16u) |
                    (static_cast<uint32_t>(code5Message.seedBytes0c[3]) << 24u)),
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                fmt::ptr(mediator ? mediator->CurrentState() : nullptr),
                remoteHostForLog);
        } else {
            spdlog::info(
                "CBaseMarginConnection::DispatchMessage consumed short/malformed code5 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode5=0 logicalPayloadBytes={} this={} ownerContext={} currentState={} remoteHost='{}'",
                static_cast<unsigned>(rawCode),
                usedHeaderlessLocatorDecode ? 1u : 0u,
                usedHeaderlessLocatorDecode ? 1u : 0u,
                static_cast<unsigned>(logicalPayloadByteCount),
                fmt::ptr(this),
                fmt::ptr(OwnerContext()),
                fmt::ptr(mediator ? mediator->CurrentState() : nullptr),
                remoteHostForLog);
        }
        return 1u;
    }

    return 0u;
}

// anchor: launcher.exe:0x44af60
// Later leaf override on top of the base `CMessageConnection::OnOperationCompleted` family.
// Current tighter read from the direct `0x44af60` decompile/listing:
// - call base `0x4490c0`
// - if base returns 0, call owner `+0x188 / 0x41afc0`
// - if that also returns 0, fall through to `0x448a60`
// - only after that handled/unhandled decision, read `workItem+0x04`
// - if work type == 1, clear owner byte `+0xf14` and tear down through the connection object
// - there is no separate type-2 connect-status split in this leaf; that work still flows through
//   the same owner `+0x188` fallback path
uint32_t CMarginConnection::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    uint32_t handled = 0u;
    if (CMessageConnection::OnOperationCompleted(workItem) != 0u) {
        handled = 1u;
    } else {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
        if (mediator && CMessageConnection_IsMediatorMarginConnectionScaffold(this, mediator) &&
            mediator->HandleMarginConnectionCompletionFallback(this, workItem) != 0u) {
            handled = 1u;
        } else {
            CMessageConnection_LogUnhandledOperationScaffold(workItem);
        }
    }

    if (CMessageConnection_WorkItemTypeScaffold(workItem) ==
        CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_LoginMediatorOwnerScaffold(this);
        if (mediator && CMessageConnection_IsMediatorMarginConnectionScaffold(this, mediator)) {
            mediator->postAuthMarginLoadingState_.state10SendGateFlagF14 = 0u;
        }

        // anchor: launcher.exe:0x44af60 tail
        // The original tail calls vtable[0](1), i.e. the deleting-dtor-style teardown path,
        // not the ordinary `Close(bool)` wrapper.
        delete this;
    }

    return handled;
}

// anchor: launcher.exe:0x44af20
// Later leaf dispatch override on top of `CBaseMarginConnection::DispatchMessage`.
// Current tighter source split:
// - base `0x442d00` now owns the consumed decoded-code `2/4/5` router again
// - only the surviving path stages bytes for the later launcher-owned bootstrap / slot-6 path
uint32_t CMarginConnection::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    if (CBaseMarginConnection::DispatchMessage(messageRef) != 0u) {
        return 1u;
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

    CBaseMarginConnectionParsedPayloadSpanScaffold parsedPayload = {};
    const bool resolvedLogicalPayload =
        CBaseMarginConnection_ResolveLogicalPayloadSpanScaffold(copiedMessageRef, &parsedPayload);

    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    bool hadValidMessageCode = false;
    (void)CBaseMarginConnection_DispatchMessageFilterScaffold(
        copiedMessageRef,
        &decodedMessageCode,
        &usedHeaderlessLocatorDecode,
        &hadValidMessageCode);

    const uint8_t* const logicalPayloadBytes =
        resolvedLogicalPayload ? parsedPayload.logicalPayloadBytes00 : payloadBytes;
    const size_t logicalPayloadByteCount =
        resolvedLogicalPayload ? parsedPayload.logicalPayloadByteCount04 : payloadByteCount;
    const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
    const bool headerless =
        resolvedLogicalPayload ? parsedPayload.headerless08 : (copiedMessageRef.headerless10 != 0u);
    const bool locatorDecoded =
        resolvedLogicalPayload ? parsedPayload.usedHeaderlessLocatorDecode09 : usedHeaderlessLocatorDecode;
    const std::string remoteHostForLog =
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName();

    mediator->stagedIncomingMarginPacketBytes_.assign(
        logicalPayloadBytes,
        logicalPayloadBytes + logicalPayloadByteCount);
    ++mediator->marginPacketReceiveCountScaffold_;
    mediator->lastMarginPacketOpcodeScaffold_ = rawCode;
    mediator->lastMarginPacketSizeScaffold_ = static_cast<uint32_t>(logicalPayloadByteCount);

    const uint32_t bootstrapHandled =
        mediator->ContinueMarginBootstrapHandshake(
            logicalPayloadBytes,
            logicalPayloadByteCount,
            /*transportEncrypted=*/false);
    if (bootstrapHandled != 0u) {
        spdlog::info(
            "CMarginConnection::DispatchMessage rawCode=0x{:02x} decodedMessageCode={} decodedCodeValid={} headerless={} locatorDecoded={} payloadBytes={} messageRef={} this={} ownerContext={} currentState={} handled={} remoteHost='{}'",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(decodedMessageCode),
            hadValidMessageCode ? 1u : 0u,
            headerless ? 1u : 0u,
            locatorDecoded ? 1u : 0u,
            static_cast<unsigned>(logicalPayloadByteCount),
            fmt::ptr(messageRef),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mediator->CurrentState()),
            bootstrapHandled,
            remoteHostForLog);
        return bootstrapHandled;
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
        locatorDecoded ? 1u : 0u,
        static_cast<unsigned>(logicalPayloadByteCount),
        fmt::ptr(messageRef),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(mediator->CurrentState()),
        handled,
        remoteHostForLog);
    return handled;
}

}  // namespace mxo::liblttcp
