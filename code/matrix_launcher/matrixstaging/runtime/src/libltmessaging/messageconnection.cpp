#include "messageconnection.h"
#include "crypto_init_helper.h"

#include "../../../game/src/libltclientlogin/loginmediator.h"
#include "../libltbase/ltresult.h"
#include "../libltcrypto/auth_crypto.h"
#include "../libltcrypto/auth_internal.h"
#include "variablelengthprefixedtcpstreamparser.h"
#include "spdlog/spdlog.h"

#include <integer.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
#include <utility>

#ifdef DispatchMessage
#undef DispatchMessage
#endif

// Global function pointers from original launcher.exe
// anchor: launcher.exe:0x004f8364 - logging enable flag
static bool g_LogRouter = false;

// anchor: launcher.exe:0x004f7cec - alternate packet name decoder function pointer
typedef void (*PacketNameDecoderFunc)(void*, int);
static PacketNameDecoderFunc g_PacketNameDecoderAlternate = nullptr;

namespace mxo::liblttcp {

// ============================================================
// Message-connection family notes
// ============================================================
// Current corrected split:
// - base `CMessageConnection_0x4b7928` surface here is centered on the shared ctor/send/completion family:
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

static void** CMessageConnection_0x4b7928_CompletionHelperVtable() {
    // anchor: launcher.exe vtable `0x004b3e20`
    static void* vtable[1] = {nullptr};
    return vtable;
}

}  // namespace

CMessageConnectionCompletionHelperScaffold::CMessageConnectionCompletionHelperScaffold() {
    vtable00 = CMessageConnection_0x4b7928_CompletionHelperVtable();
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
CMessageConnection_0x4b7928::CMessageConnection_0x4b7928()
    : CLTTCPConnection(),
      packetNameCallback_(0),
      packetizedMessagesEnabled_(false),
      connectCompletionHelper7c_(),
      closeCompletionHelper80_(),
      packetAgenda_() {}

// UNANCHORED: source-owned narrow subset of `0x448b40(engine, createCompletionHelpers)`.
CMessageConnection_0x4b7928::CMessageConnection_0x4b7928(
    CLTThreadPerClientTCPEngine_0x4b2768* engine,
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
CMessageConnection_0x4b7928::~CMessageConnection_0x4b7928() = default;

// UNANCHORED: source-owned compatibility pass-through over the recovered base `+0x10` engine field.
void CMessageConnection_0x4b7928::SetEngine(CLTThreadPerClientTCPEngine_0x4b2768* engine) {
    CLTTCPConnection::SetEngine(engine);
}

// UNANCHORED: source-owned compatibility accessor over the recovered base `+0x10` engine field.
CLTThreadPerClientTCPEngine_0x4b2768* CMessageConnection_0x4b7928::Engine() const {
    return CLTTCPConnection::Engine();
}

// anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
uint32_t CMessageConnectionMessageStorage_0x4ba208::AddRef() {
    return static_cast<uint32_t>(InterlockedIncrement(&referenceCount04));
}

// anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
uint32_t CMessageConnectionMessageStorage_0x4ba208::Release() {
    const LONG current = InterlockedDecrement(&referenceCount04);
    if (current == 0) {
        FinalRelease();
    }
    return static_cast<uint32_t>(current);
}

void CMessageConnectionMessageStorage_0x4ba208::FinalRelease() {
    // anchor: launcher.exe:0x455ad0 / vtable `0x004ba208 +0x0c`
    // The original heap object returns to a pool here. This internal mirror is stack/inline owned,
    // so the final-release path is intentionally non-deleting.
}

// anchor: launcher.exe:0x42f880 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x10`
void CMessageConnectionMessageStorage_0x4ba208::ResetRefCount() {
    InterlockedExchange(&referenceCount04, 0);
}

// anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
void CMessageConnectionMessageStorage_0x4ba208::SetRefCountFromPtr(const volatile long* refCountSource) {
    if (!refCountSource) {
        return;
    }
    InterlockedExchange(&referenceCount04, *refCountSource);
}

void CMessageConnectionMessageStorage_0x4ba208::ResetForPacketBuilder() {
    // anchor: launcher.exe:0x455bd0 inner-storage setup before the outer object stores/AddRefs it
    referenceCount04 = 0;
    reservedBytes08 = kBuilderReservedBytes08;
    payloadLengthHigh0a = 0u;
    payloadLengthLow0b = 0u;
    // FIDELITY: Zero the inline payload array at offset 0xc
    std::fill(payloadBytes0c.begin(), payloadBytes0c.end(), 0u);
}

void CMessageConnectionMessageStorage_0x4ba208::ResetPayloadByteCount(uint16_t payloadByteCount) {
    const uint16_t oldByteCount = PayloadByteCount();
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
    SetPayloadByteCountRaw(clampedByteCount);
}

void CMessageConnectionMessageStorage_0x4ba208::SetPayloadByteCountRaw(uint16_t payloadByteCount) {
    const uint16_t clampedByteCount = std::min<uint16_t>(payloadByteCount, kMaxPayloadByteCount);
    payloadLengthLow0b = static_cast<uint8_t>(clampedByteCount & 0xffu);
    payloadLengthHigh0a =
        (clampedByteCount > 0x7fu)
            ? static_cast<uint8_t>(0x80u | ((clampedByteCount >> 8u) & 0x7fu))
            : 0u;
}

uint16_t CMessageConnectionMessageStorage_0x4ba208::GrowPayloadByteCount(uint16_t additionalByteCount) {
    // anchor: launcher.exe:0x4557b0
    const uint32_t oldByteCount = PayloadByteCount();
    const uint32_t requestedByteCount = oldByteCount + static_cast<uint32_t>(additionalByteCount);
    if (requestedByteCount > kMaxPayloadByteCount) {
        return static_cast<uint16_t>(oldByteCount);
    }

    const uint16_t newByteCount = static_cast<uint16_t>(requestedByteCount);
    SetPayloadByteCountRaw(newByteCount);
    return newByteCount;
}

uint16_t CMessageConnectionMessageStorage_0x4ba208::PayloadByteCount() const {
    const uint16_t encodedPayloadByteCount =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(payloadLengthHigh0a & 0x7fu) << 8u) |
            static_cast<uint16_t>(payloadLengthLow0b));
    return std::min<uint16_t>(encodedPayloadByteCount, kMaxPayloadByteCount);
}

uint16_t CMessageConnectionMessageStorage_0x4ba208::RemainingAppendableByteCount() const {
    const uint32_t payloadByteCount = PayloadByteCount();
    if (payloadByteCount >= kMaxPayloadByteCount || reservedBytes08 >= kMaxPayloadByteCount) {
        return 0u;
    }

    const uint32_t remaining = kMaxPayloadByteCount - payloadByteCount - reservedBytes08;
    return static_cast<uint16_t>(std::min<uint32_t>(remaining, kMaxPayloadByteCount));
}

uint8_t* CMessageConnectionMessageStorage_0x4ba208::PayloadBase() {
    // FIDELITY: Original DefaultCtor at 0x439840 computes:
    //   nopatchLauncherVersionValue04 = *(messageRef08 + 0xc) + 0xc
    // = messageStorage0c + 0xc = &payloadBytes0c[0]
    // Inline array starts at offset 0xc in this struct.
    return payloadBytes0c.data();
}

const uint8_t* CMessageConnectionMessageStorage_0x4ba208::PayloadBase() const {
    return payloadBytes0c.data();
}

// anchor: launcher.exe:0x42f850 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x04`
uint32_t CMessageConnectionMessageRefBase_0x4ba220::AddRef() {
    return static_cast<uint32_t>(InterlockedIncrement(&referenceCount04));
}

// anchor: launcher.exe:0x42f860 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x08`
uint32_t CMessageConnectionMessageRefBase_0x4ba220::Release() {
    const LONG current = InterlockedDecrement(&referenceCount04);
    if (current == 0) {
        FinalRelease();
    }
    return static_cast<uint32_t>(current);
}

// anchor: launcher.exe:0x42f880 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x10`
void CMessageConnectionMessageRefBase_0x4ba220::ResetRefCount() {
    InterlockedExchange(&referenceCount04, 0);
}

// anchor: launcher.exe:0x42f890 / vtable `0x004ba208/0x004ba220/0x004ba23c +0x14`
void CMessageConnectionMessageRefBase_0x4ba220::SetRefCountFromPtr(const volatile long* refCountSource) {
    if (!refCountSource) {
        return;
    }
    InterlockedExchange(&referenceCount04, *refCountSource);
}

void CMessageConnectionMessageRefBase_0x4ba220::ResetBaseForPacketBuilder(uint32_t field08Value) {
    // anchor: launcher.exe:0x455bd0
    referenceCount04 = 0;
    field08 = field08Value;
    messageStorage0c = nullptr;
    ownedMessageStorage_.ResetForPacketBuilder();
    messageStorage0c = &ownedMessageStorage_;
    messageStorage0c->AddRef();
}

uint16_t CMessageConnectionMessageRefBase_0x4ba220::GrowPayloadByteCount(
    uint16_t additionalByteCount) {
    if (!messageStorage0c) {
        return 0u;
    }
    return messageStorage0c->GrowPayloadByteCount(additionalByteCount);
}

uint8_t* CMessageConnectionMessageRefBase_0x4ba220::PayloadAppendPointer() {
    if (!messageStorage0c) {
        return nullptr;
    }
    uint8_t* const payloadBase = messageStorage0c->PayloadBase();
    return payloadBase
        ? (payloadBase + static_cast<size_t>(messageStorage0c->PayloadByteCount()))
        : nullptr;
}

// anchor: launcher.exe:0x41bb60
bool CMessageConnectionMessageRefBase_0x4ba220::SetPayloadByteCount(
    uint32_t payloadByteCount) {
    if (!messageStorage0c || payloadByteCount > CMessageConnectionMessageStorage_0x4ba208::kMaxPayloadByteCount) {
        return false;
    }
    messageStorage0c->SetPayloadByteCountRaw(static_cast<uint16_t>(payloadByteCount));
    return true;
}

uint16_t CMessageConnectionMessageRefBase_0x4ba220::PayloadByteCount() const {
    return messageStorage0c ? messageStorage0c->PayloadByteCount() : 0u;
}

void CMessageConnectionMessageRef_0x4ba23c::FinalRelease() {
    // anchor: launcher.exe:0x455b80 / vtable `0x004ba23c +0x0c`
    // Original live outer objects release the inner payload-storage object at `+0x0c`, collapse,
    // and return to the outer-object pool. Source still lacks the real pool, but a heap-backed
    // delete after releasing the inner storage is closer to the original lifetime than keeping the
    // receive-side outer message-ref on the stack.
    if (messageStorage0c) {
        CMessageConnectionMessageStorage_0x4ba208* const storage = messageStorage0c;
        messageStorage0c = nullptr;
        storage->Release();
    }
    delete this;
}

void CMessageConnectionMessageRef_0x4ba23c::ResetForPacketBuilder(
    bool headerless,
    uint32_t messageContext14) {
    // anchor: launcher.exe:0x455cd0 / 0x455c60
    ResetBaseForPacketBuilder(/*field08Value=*/0u);
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
    void operator()(CMessageConnectionMessageRef_0x4ba23c* messageRef) const {
        if (messageRef) {
            messageRef->Release();
        }
    }
};

static bool CMessageConnection_0x4b7928_ResolveTransformInputSpan(
    const CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
    const uint8_t** outPayloadBytes,
    size_t* outPayloadByteCount) {
    if (outPayloadBytes) {
        *outPayloadBytes = nullptr;
    }
    if (outPayloadByteCount) {
        *outPayloadByteCount = 0u;
    }

    const CMessageConnectionMessageStorage_0x4ba208* const messageStorage =
        inputMessageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
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
    CStreamPacketEncryptionModuleReadHelper_0x4b86f0* helper,
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

static CMessageConnectionMessageRef_0x4ba23c* CMessageConnectionMessageRefHandle_AssignRetained(
    CMessageConnectionMessageRef_0x4ba23c** slot,
    CMessageConnectionMessageRef_0x4ba23c* newMessageRef) {
    if (!slot) {
        return nullptr;
    }
    CMessageConnectionMessageRef_0x4ba23c* const oldMessageRef = *slot;
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
        payloadByteCount > CMessageConnectionMessageStorage_0x4ba208::kMaxPayloadByteCount) {
        return false;
    }

    messageRef = new CMessageConnectionMessageRef_0x4ba23c();
    messageRef->ResetForPacketBuilder(false, 0u);
    uint8_t* const appendPointer = messageRef->PayloadAppendPointer();
    if (!appendPointer) {
        Reset();
        return false;
    }

    std::copy_n(payloadBytes, payloadByteCount, appendPointer);
    if (messageRef->GrowPayloadByteCount(static_cast<uint16_t>(payloadByteCount)) !=
        payloadByteCount) {
        Reset();
        return false;
    }

    hasValue = true;
    return true;
}

// anchor: launcher.exe:0x44d390 / 0x44d500
CMessageConnectionMessageRef_0x4ba23c* CMessageConnectionMessageRefOutputBuffer::MessageRef() {
    return hasValue ? messageRef : nullptr;
}



// Basic implementation of bootstrap prep for fidelity
// anchor: launcher.exe:0x4b6778 vtable family
// This is a minimal implementation to support the faithful flow


// anchor: launcher.exe:0x44d910 / 0x44daf0
void CStreamPacketEncryptionModuleReadTransformWorker_0x4b86f0::ResetForSeed(
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
bool CStreamPacketEncryptionModuleReadTransformWorker_0x4b86f0::TryTransform(
    const CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) {
    // Source still keeps the confirmed packet semantic at the worker boundary here, but the
    // recovered large/decrypting `FeedbackSize` adapter constructed by `0x44d910` is now held as a
    // real object alongside that packet-level behavior instead of being collapsed away entirely.
    if (!outputBuffer || !hasConfiguredFeedbackTransform) {
        return false;
    }

    const uint8_t* encryptedPayloadBytes = nullptr;
    size_t encryptedPayloadByteCount = 0u;
    if (!CMessageConnection_0x4b7928_ResolveTransformInputSpan(
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
void CStreamPacketEncryptionModuleWriteTransformWorker_0x4b86a8::ResetForSeed(
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
bool CStreamPacketEncryptionModuleWriteTransformWorker_0x4b86a8::TryTransform(
    const CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
    CMessageConnectionMessageRefOutputBuffer* outputBuffer) {
    // Source still keeps the confirmed packet semantic at the worker boundary here, but the
    // recovered small/encrypting `FeedbackSize` adapter constructed by `0x44d820` is now held as a
    // real object alongside that packet-level behavior instead of being collapsed away entirely.
    if (!outputBuffer || !hasConfiguredFeedbackTransform) {
        spdlog::debug("TryTransform: no outputBuffer or no hasConfiguredFeedbackTransform");
        return false;
    }

    const uint8_t* payloadBytes = nullptr;
    size_t payloadByteCount = 0u;
    if (!CMessageConnection_0x4b7928_ResolveTransformInputSpan(
            inputMessageRef,
            &payloadBytes,
            &payloadByteCount)) {
        spdlog::debug("TryTransform: failed to resolve transform input span");
        return false;
    }

    spdlog::debug("TryTransform: input payload[0]={:02x} count={}", payloadBytes[0], payloadByteCount);

    // FIDELITY: Check for CERT packet format [opcode][3][data] which should NOT be encrypted
    // Server expects CERT packets (opcodes 1-5) with second field = 3 to be sent unencrypted
    // This matches launcher.exe behavior where certain critical packets bypass stream encryption
    bool shouldEncrypt = true;
    if (payloadByteCount >= 3 &&
        payloadBytes[0] >= 0x01 && payloadBytes[0] <= 0x05) {  // CERT opcodes 1-5
        const uint16_t secondField = *reinterpret_cast<const uint16_t*>(payloadBytes + 1);
        if (secondField == 3) {
            shouldEncrypt = false;
            spdlog::debug("TryTransform: skipping encryption for CERT packet with unencrypted flag (opcode={:02x}, field=3)",
                payloadBytes[0]);
        }
    }

    if (shouldEncrypt) {
        mxo::auth::FramedPacket encryptedPacket;
        if (!mxo::auth::EncryptMarginPayloadPacket(
                payloadBytes,
                payloadByteCount,
                CStreamPacketEncryptionWorker_KeyBytes(associatedSeedBytes),
                mxo::auth::kFrameModeAuto,
                &encryptedPacket)) {
            spdlog::debug("TryTransform: EncryptMarginPayloadPacket failed");
            return false;
        }

        spdlog::debug("TryTransform: encrypted output size={}", encryptedPacket.payloadBytes.size());
        return outputBuffer->SetPayloadBytes(
            encryptedPacket.payloadBytes.data(),
            encryptedPacket.payloadBytes.size());
    } else {
        // Send unencrypted - copy payload directly
        spdlog::debug("TryTransform: sending unencrypted, output size={}", payloadByteCount);
        return outputBuffer->SetPayloadBytes(payloadBytes, payloadByteCount);
    }
}

// UNANCHORED: source-owned helper forwarding through the recovered helper-family `nextHelper04`
// link used by the agenda read/write chains.
void CStreamPacketEncryptionHelperBase_0x4b81c8::ForwardToNextHelper(
    void* opaqueMessageRef) {
    if (nextHelper04) {
        nextHelper04->HandleOpaqueMessageRef(opaqueMessageRef);
    }
}

// anchor: launcher.exe:0x44d500
void CStreamPacketEncryptionModuleReadHelper_0x4b86f0::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    CMessageConnectionMessageRef_0x4ba23c* const inputMessageRef =
        static_cast<CMessageConnectionMessageRef_0x4ba23c*>(opaqueMessageRef);
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
void CStreamPacketEncryptionModuleReadHelper_0x4b86f0::ResetForOwner(
    CStreamPacketEncryptionModule_0x4b8704* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    collectionControl0c = 0u;
    transformWorkers.clear();
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44da00 / 0x44daf0
void CStreamPacketEncryptionModuleReadHelper_0x4b86f0::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    collectionControl0c = 0u;
    transformWorkers.clear();
    transformWorkers.emplace_back();
    transformWorkers.back().ResetForSeed(seedBytes);
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44d390
void CStreamPacketEncryptionModuleWriteHelper_0x4b8690::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    CMessageConnectionMessageRef_0x4ba23c* const inputMessageRef =
        static_cast<CMessageConnectionMessageRef_0x4ba23c*>(opaqueMessageRef);
    if (!inputMessageRef || !hasTransformWorker ||
        !transformWorker.TryTransform(*inputMessageRef, &transformedOutput)) {
        transformedOutput.Reset();
        ForwardToNextHelper(nullptr);
        return;
    }

    ForwardToNextHelper(transformedOutput.MessageRef());
}

// anchor: launcher.exe:0x44d820 / 0x44daf0
void CStreamPacketEncryptionModuleWriteHelper_0x4b8690::ResetForOwner(
    CStreamPacketEncryptionModule_0x4b8704* owner) {
    nextHelper04 = nullptr;
    owner08 = owner;
    hasTransformWorker = false;
    transformWorker = {};
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x44d820 / 0x44daf0
void CStreamPacketEncryptionModuleWriteHelper_0x4b8690::ResetForSeed(
    const std::array<uint8_t, 16>& seedBytes) {
    hasTransformWorker = true;
    transformWorker.ResetForSeed(seedBytes);
    transformedOutput.Reset();
}

// anchor: launcher.exe:0x469980
void PacketProcessingAgenda_0x4baf48::HandleOpaqueMessageRef(
    void* opaqueMessageRef) {
    // Original helper stores/replaces a retained outer message-ref through helper field `+0x10`.
    // Now that both receive-side and transform-output message-ref objects are heap-backed, source
    // can keep the nearer AddRef/Release handle semantics instead of a raw pointer overwrite.
    if (!outputSlotAddress10) {
        return;
    }
    CMessageConnectionMessageRefHandle_AssignRetained(
        reinterpret_cast<CMessageConnectionMessageRef_0x4ba23c**>(outputSlotAddress10),
        static_cast<CMessageConnectionMessageRef_0x4ba23c*>(opaqueMessageRef));
}

// anchor: launcher.exe:0x469980
void PacketProcessingAgenda_0x4baf48::StoreOpaqueMessageRef(CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    // Faithful mirror of original CMessageConnectionPacketAgendaHelper_StoreOpaqueMessageRef
    // Handles proper ref counting when storing/replacing message refs
    if (!outputSlotAddress10) {
        return;
    }

    CMessageConnectionMessageRef_0x4ba23c** slot = reinterpret_cast<CMessageConnectionMessageRef_0x4ba23c**>(outputSlotAddress10);
    CMessageConnectionMessageRef_0x4ba23c* current = *slot;

    if (messageRef != current) {
        if (current != nullptr) {
            current->Release();
        }
        *slot = messageRef;
        if (messageRef != nullptr) {
            messageRef->AddRef();
        }
    }
}

// anchor: launcher.exe:0x44c680 / vtable `0x004baf48 +0x00`
PacketProcessingAgenda_0x4baf48::~PacketProcessingAgenda_0x4baf48() {
    // Default destructor implementation
}

// anchor: launcher.exe:0x44bb60 / vtable `0x004baf48 +0x04`
void PacketProcessingAgenda_0x4baf48::VirtualMethod1_0x44bb60() {
    // Virtual method 1 implementation
    // Original at 0x44bb60 - placeholder for future implementation
}

// anchor: launcher.exe:0x481750 / vtable `0x004baf48 +0x08`
uint32_t PacketProcessingAgenda_0x4baf48::VirtualMethod2_0x481750() {
    // Virtual method 2 implementation
    // Original at 0x481750 - placeholder for future implementation
    return 0;
}

// anchor: launcher.exe:0x469720 / vtable `0x004baf48 +0x10`
void PacketProcessingAgenda_0x4baf48::DelegateToChainedHelper() {
    // Delegate to chained helper implementation
    // Original at 0x469720 - delegates to nextHelper04 if available
    if (nextHelper04) {
        nextHelper04->HandleOpaqueMessageRef(nullptr);
    }
}

// anchor: launcher.exe:0x44da00
void CStreamPacketEncryptionModule_0x4b8704::InitializeForMarginConnectionSeed(
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
void CStreamPacketEncryptionModule_0x4b8704::RefreshFromMarginConnectionSeed(
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
const char* CMessageConnection_0x4b7928::PacketNameFamilyToString(CMessageConnectionPacketNameFamily family) {
    switch (family) {
        case CMessageConnectionPacketNameFamily::kAuth:
            return "auth";
        case CMessageConnectionPacketNameFamily::kMargin:
            return "margin";
        default:
            return "unknown";
    }
}

void CMessageConnection_0x4b7928::ConfigurePacketNameFamily(
    CMessageConnectionPacketNameFamily family,
    bool packetizedMessagesEnabled) {
    packetNameFamily_ = family;
    packetizedMessagesEnabled_ = packetizedMessagesEnabled;
}

// anchor: launcher.exe:0x448980
void CMessageConnection_0x4b7928::ConfigurePacketAgenda(
    CStreamPacketEncryptionModule_0x4b8704* streamPacketEncryptionModule) {
    // anchor: launcher.exe:0x448980
    // Faithful mirror of original CMessageConnection_ConfigurePacketAgenda:
    // - if packetAgenda74 is null, allocate and construct it
    // - then install the stream packet encryption module via 0x469740

    if (!packetAgenda_) {
        packetAgenda_ = std::make_unique<PacketProcessingAgenda_0x469850>(this);
        if (!packetAgenda_) {
            return;
        }
    }

    // anchor: launcher.exe:0x448980 -> 0x469740
    // Install stream packet encryption module (always called, even if agenda was already created)
    packetAgenda_->InstallStreamPacketEncryptionModule(streamPacketEncryptionModule);
}

// anchor family: launcher.exe:0x448980 -> connection `+0x74`
const PacketProcessingAgenda_0x469850* CMessageConnection_0x4b7928::PacketAgenda() const {
    return packetAgenda_.get();
}

static void CMessageConnection_0x4b7928_ApplySendMessageRefMutations(
    CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    if (!messageRef || messageRef->headerless10 == 0u || !messageRef->messageStorage0c) {
        return;
    }

    // anchor: launcher.exe:0x448cf0 / mutation block at 0x448d08
    // Mirror of original mutation logic for headerless send mode:
    // - Tests payload offset +6 (inner storage +0x12) as a dword locator
    // - If locator is zero, writes 4 bytes from g_messageConnectionZeroTransactionId
    //   (launcher.exe:0x4f7ce8 = 00 00 00 00) to payload offset +6
    // - Then writes g_messageConnectionDefaultFieldHighByte (launcher.exe:0x4cbd99 = 0xff)
    //   to payload offset +10 (inner storage +0x16)
    // - And writes g_messageConnectionDefaultFieldLowByte (launcher.exe:0x4cbd98 = 0xff)
    //   to payload offset +11 (inner storage +0x17)
    //
    // Packet layout context:
    // - Offset 0: Frame byte (header flag in high bit)
    // - Offset 1-2: Protocol/Opcode (2 bytes) - this is the "2 byte field at offset 1"
    // - Offset 3: Additional flags
    // - Offset 4-7: Transaction ID (4 bytes, written by this mutation if zero)
    // - Offset 8-9: Reserved/padding (not touched by mutation)
    // - Offset 10-11: Default field values (0xFF 0xFF written by this mutation)
    CMessageConnectionMessageStorage_0x4ba208& messageStorage = *messageRef->messageStorage0c;
    uint8_t* const payloadBase = messageStorage.PayloadBase();
    if (!payloadBase) {
        return;
    }

    // anchor: launcher.exe:0x448d0e - check locator at payload+6 (storage+0x12)
    const uint32_t locatorDword06 =
        static_cast<uint32_t>(payloadBase[6]) |
        (static_cast<uint32_t>(payloadBase[7]) << 8u) |
        (static_cast<uint32_t>(payloadBase[8]) << 16u) |
        (static_cast<uint32_t>(payloadBase[9]) << 24u);
    if (locatorDword06 != 0u) {
        return;
    }

    // anchor: launcher.exe:0x448d18 - copy 4 bytes from g_messageConnectionZeroTransactionId
    // The original uses a byte-by-byte copy loop; we use fill_n for equivalent semantics
    std::fill_n(payloadBase + 6u, 4u, 0u);

    // anchor: launcher.exe:0x448d35 / 0x448d3e - write 0xFF bytes
    // These correspond to g_messageConnectionDefaultFieldHighByte (0x4cbd99) and
    // g_messageConnectionDefaultFieldLowByte (0x4cbd98) respectively
    payloadBase[10] = 0xffu;  // inner +0x16
    payloadBase[11] = 0xffu;  // inner +0x17
}

// anchor: launcher.exe:0x448cf0 post-submit/or-discard cleanup on the original input message-ref
static void CMessageConnection_0x4b7928_ClearSendMessageRefFirstPayloadByteHighBit(
    CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    if (!messageRef || messageRef->headerless10 == 0u || !messageRef->messageStorage0c) {
        return;
    }

    // anchor: launcher.exe:0x448cf0
    // After either submit or agenda discard, original code clears the first payload byte high bit
    // on the original input message-ref's inner storage at `inner + 0x0c`.
    uint8_t* const payloadBase = messageRef->messageStorage0c->PayloadBase();
    if (!payloadBase) {
        return;
    }
    payloadBase[0] &= 0x7fu;
}

// anchor: launcher.exe:0x469850
PacketProcessingAgenda_0x469850::PacketProcessingAgenda_0x469850(CMessageConnection_0x4b7928* connectionOwner)
    : connectionOwner00(connectionOwner),
      configuredModuleList04(nullptr),
      readOutputSlot08(nullptr),
      embeddedReadHelper0c(),
      writeOutputSlot24(nullptr),
      embeddedWriteHelper28(),
      readHelperChainHead40(nullptr),
      writeHelperChainHead44(nullptr),
      writeHelperChainTail48(nullptr),
      configuredModuleCount4c(0),
      reserved4e(0),
      created(true),
      configuredStreamPacketEncryptionModule(nullptr) {
    // Set up embedded read helper at +0x0c (mirrors 0x46986c-0x469883)
    embeddedReadHelper0c.nextHelper04 = nullptr;
    embeddedReadHelper0c.field04 = 0u;
    embeddedReadHelper0c.field08 = 0u;
    embeddedReadHelper0c.helperLabel0c = "Agenda read helper";
    embeddedReadHelper0c.outputSlotAddress10 = reinterpret_cast<void**>(&readOutputSlot08);
    embeddedReadHelper0c.downstreamHelperSlot14 = &writeHelperChainHead44;

    // Set up embedded write helper at +0x28 (mirrors 0x469895-0x4698a3)
    embeddedWriteHelper28.nextHelper04 = nullptr;
    embeddedWriteHelper28.field04 = 0u;
    embeddedWriteHelper28.field08 = 0u;
    embeddedWriteHelper28.helperLabel0c = "Agenda write helper";
    embeddedWriteHelper28.outputSlotAddress10 = reinterpret_cast<void**>(&writeOutputSlot24);
    embeddedWriteHelper28.downstreamHelperSlot14 = nullptr;

    // Initialize helper chains
    readHelperChainHead40 = &embeddedReadHelper0c;
    writeHelperChainHead44 = nullptr;
    writeHelperChainTail48 = nullptr;
}

// anchor: launcher.exe:0x469740
uint16_t PacketProcessingAgenda_0x469850::InstallStreamPacketEncryptionModule(
    CStreamPacketEncryptionModule_0x4b8704* streamPacketEncryptionModule) {
    // Faithful mirror of original CMessageConnectionPacketAgenda_InstallStreamPacketEncryptionModule
    if (!streamPacketEncryptionModule || (!streamPacketEncryptionModule->readHelper04 && !streamPacketEncryptionModule->writeHelper08)) {
        // Original calls some function through vtable when both helpers are null
        // For now, we'll just return early like the original does
        return configuredModuleCount4c;
    }

    // Install the module into the agenda
    streamPacketEncryptionModule->nextConfiguredModule0c = configuredModuleList04;
    configuredModuleList04 = streamPacketEncryptionModule;

    if (streamPacketEncryptionModule->readHelper04) {
        CStreamPacketEncryptionHelperBase_0x4b81c8* const previousReadHead = readHelperChainHead40;
        readHelperChainHead40 = streamPacketEncryptionModule->readHelper04;
        streamPacketEncryptionModule->readHelper04->nextHelper04 = previousReadHead;
    }

    if (streamPacketEncryptionModule->writeHelper08) {
        CStreamPacketEncryptionHelperBase_0x4b81c8* const previousWriteTail = writeHelperChainTail48;
        streamPacketEncryptionModule->writeHelper08->nextHelper04 =
            &embeddedWriteHelper28;
        writeHelperChainTail48 = streamPacketEncryptionModule->writeHelper08;
        if (previousWriteTail) {
            previousWriteTail->nextHelper04 = streamPacketEncryptionModule->writeHelper08;
        } else {
            writeHelperChainHead44 = streamPacketEncryptionModule->writeHelper08;
        }
    }

    streamPacketEncryptionModule->configuredConnection10 = connectionOwner00;
    configuredStreamPacketEncryptionModule = streamPacketEncryptionModule;
    ++configuredModuleCount4c;

    return configuredModuleCount4c;
}

// anchor: launcher.exe:0x469950
CMessageConnectionMessageRef_0x4ba23c* PacketProcessingAgenda_0x469850::ApplySendPacketAgenda(
    CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
    bool* outAgendaTouched) {
    // Faithful mirror of original PacketProcessingAgenda_DispatchWriteHelperChain
    // - if no write helper chain head, return original message ref
    // - otherwise drive the chain and return the output slot
    if (outAgendaTouched) {
        *outAgendaTouched = (writeHelperChainHead44 != nullptr);
    }

    spdlog::debug("ApplySendPacketAgenda: writeHelperChainHead44={} inputPayloadBytes={}",
        fmt::ptr(writeHelperChainHead44),
        inputMessageRef.PayloadByteCount());

    if (writeHelperChainHead44 == nullptr) {
        return &inputMessageRef;
    }

    writeHelperChainHead44->HandleOpaqueMessageRef(&inputMessageRef);
    spdlog::debug("ApplySendPacketAgenda: outputSlot={} outputPayloadBytes={}",
        fmt::ptr(writeOutputSlot24),
        writeOutputSlot24 ? writeOutputSlot24->PayloadByteCount() : 0);
    return writeOutputSlot24;
}

// anchor: launcher.exe:0x469930
CMessageConnectionMessageRef_0x4ba23c* PacketProcessingAgenda_0x469850::ApplyReceivePacketAgenda(
    CMessageConnectionMessageRef_0x4ba23c* inputMessageRef,
    bool* outAgendaTouched) {
    // Faithful mirror of original PacketProcessingAgenda_DispatchReadHelperChain
    // - if no input message ref or no read helper chain head, return null/original
    // - otherwise drive the chain and return the output slot
    if (outAgendaTouched) {
        *outAgendaTouched = (readHelperChainHead40 != nullptr && inputMessageRef != nullptr);
    }

    if (!inputMessageRef || readHelperChainHead40 == nullptr) {
        return inputMessageRef;
    }

    readHelperChainHead40->HandleOpaqueMessageRef(inputMessageRef);
    return readOutputSlot08;
}

static void** CMessageConnection_0x4b7928_PacketBuilderVtablePointer(uintptr_t address) {
    return reinterpret_cast<void**>(address);
}

// anchor: launcher.exe:0x41cf30
uint32_t CMessageConnection_0x4b7928::ForwardPacketBuilderEnvelopeToSendPacket(
    CMessageConnectionPacketBuilderEnvelope& envelope) {
    if (!envelope.messageRef08) {
        return 0u;
    }
    spdlog::debug("ForwardPacketBuilderEnvelopeToSendPacket: messageRef08={} payloadBytes={}",
        fmt::ptr(envelope.messageRef08),
        envelope.messageRef08->PayloadByteCount());
    SendPacketMessageRef(*envelope.messageRef08);
    return 0u; // Original function returns void, but wrapper maintains uint32_t signature
}

// anchor: launcher.exe:0x469950
CMessageConnectionMessageRef_0x4ba23c* CMessageConnection_0x4b7928::ApplySendPacketAgenda(
    CMessageConnectionMessageRef_0x4ba23c& inputMessageRef,
    bool* outAgendaTouched) {
    // anchor: launcher.exe:0x469950
    // Delegate to the agenda's ApplySendPacketAgenda method
    PacketProcessingAgenda_0x469850* agenda = packetAgenda_.get();
    return agenda ? agenda->ApplySendPacketAgenda(inputMessageRef, outAgendaTouched) : &inputMessageRef;
}

// anchor: launcher.exe:0x448a00
// Narrow source-owned mirror of the lower submit helper beneath the original message-ref-based
// `CMessageConnection_0x4b7928::SendPacket` family.
uint32_t CMessageConnection_0x4b7928::SubmitMessageRefBytes(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return 0u;
    }

    const CMessageConnectionMessageStorage_0x4ba208& messageStorage = *messageRef.messageStorage0c;
    const uint16_t payloadByteCount = messageStorage.PayloadByteCount();
    const uint8_t* const payloadBase = messageStorage.PayloadBase();
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

// anchor: launcher.exe:0x448cf0 / CMessageConnection_SendPacket
// Faithful mirror of the original message-ref-based `CMessageConnection::SendPacket` family.
// Inherited margin/auth send path reached from mediator send helper via connection wrapper
// 0x41cf30. Consumes the envelope/shared message object extracted from the stack-local packet
// builder, performs packet-agenda filtering, then reaches CMessageConnection_SubmitEnvelopeBytes
// (0x448a00).
//
// Ghidra globals documentation:
// - g_bMessageConnectionGlobalSendLoggingEnabled (launcher.exe:0x4f8364) - master toggle for send logging
// - g_bMessageConnectionSendPacketLoggingEnabled (launcher.exe:0x4d3e90) - per-send logging toggle
// - g_pLogRouterInstance (launcher.exe:0x4d3d68) - LogRouter singleton for dispatch
// - g_pMessageConnectionAlternatePacketNameCallback (launcher.exe:0x4f7cec) - alternate packet name decoder
// - g_messageConnectionZeroTransactionId (launcher.exe:0x4f7ce8, 4 bytes) - zero fill for mutation
// - g_messageConnectionDefaultFieldHighByte (launcher.exe:0x4cbd99) - 0xff constant for mutation
// - g_messageConnectionDefaultFieldLowByte (launcher.exe:0x4cbd98) - 0xff constant for mutation
void CMessageConnection_0x4b7928::SendPacketMessageRef(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    if (!Engine() || !messageRef.messageStorage0c) {
        return;
    }

    // Apply send message ref mutations (headerless mode specific)
    // anchor: launcher.exe:0x448d03 - mutation logic for headerless messages
    // This mutation writes to payload offsets +6..+11 when the locator at +6 is zero.
    // The 2-byte field at payload offset 1 (Protocol/Opcode) is NOT touched here.
    // That field is set by the packet builder before reaching this function.
    CMessageConnection_0x4b7928_ApplySendMessageRefMutations(&messageRef);

    // Apply packet agenda filtering (compression/encryption modules)
    // anchor: launcher.exe:0x448f5e - agenda processing via packetAgenda74
    bool agendaTouched = false;
    CMessageConnectionMessageRef_0x4ba23c* messageRefForSubmit =
        ApplySendPacketAgenda(messageRef, &agendaTouched);

    // Check if agenda processing resulted in a valid message ref
    // anchor: launcher.exe:0x448f63 - null check and agenda processing
    if (!messageRefForSubmit || !messageRefForSubmit->messageStorage0c) {
        // Packet discarded by agenda - log appropriately based on message type
        // anchor: launcher.exe:0x448f94 - discarded packet logging path
        spdlog::warn("SendPacketMessageRef: packet DISCARDED by agenda! agendaTouched={}", agendaTouched);
        if (g_LogRouter) {
            // Logging mirrors original LogRouter_DispatchMappedSourceLocMessageWithLevelGateV calls
            // The original checks g_bMessageConnectionGlobalSendLoggingEnabled and
            // g_bMessageConnectionSendPacketLoggingEnabled before dispatching.
            if (messageRef.headerless10 == 0u) {
                // Headered (non-headerless) message logging path
                // anchor: launcher.exe:0x448da1 - headered logging with g_pMessageConnectionAlternatePacketNameCallback
                if (g_PacketNameDecoderAlternate != nullptr) {
                    // TODO: Implement proper alternate packet name decoder invocation
                    // Original: (*g_pMessageConnectionAlternatePacketNameCallback)(packetMessageRef, 1)
                }
                // TODO: Implement proper packet/msg IDs, endpoints, and TTL extraction
                spdlog::info("Sending message {} (P: {}, M: {}) from LKAIP: {}.{}.{}.{}, LKAProcId: {} to LKAIP: {}.{}.{}.{}, LKAProcId: {} (ttl = {}).\n",
                    "UNKNOWN", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            } else {
                // Headerless message logging path
                // anchor: launcher.exe:0x448d5e - headerless logging with packetNameCallback70
                if (packetNameCallback_ != 0) {
                    // TODO: Implement proper packet name callback invocation
                    // packetNameCallback_ is a uintptr_t function pointer at connection+0x70
                    // Original: (*packetNameCallback70)(packetMessageRef, 0)
                }
                // TODO: Implement proper endpoint extraction and packet name decoding
                spdlog::info("Sending headerless message {} (M: {}) to {}.{}.{}.{}:{}.\n",
                    "UNKNOWN", 0, 0, 0, 0, 0, 0);
            }
        }

        // Clear high bit on original message ref when headerless
        // anchor: launcher.exe:0x448f87 - high bit clearing logic on payloadBytes0c[0]
        CMessageConnection_0x4b7928_ClearSendMessageRefFirstPayloadByteHighBit(&messageRef);
        return;
    }

    // Submit the message through the engine
    // anchor: launcher.exe:0x448f72 - CMessageConnection_SubmitEnvelopeBytes call
    if (messageRefForSubmit->messageStorage0c) {
        SubmitMessageRefBytes(*messageRefForSubmit);
    }

    // Clear high bit on original message ref after submit (headerless only)
    // anchor: launcher.exe:0x448f87 - final high bit clearing: payloadBytes0c[0] &= 0x7f
    if (messageRef.headerless10 != 0u && messageRef.messageStorage0c) {
        uint8_t* const payloadBase = messageRef.messageStorage0c->PayloadBase();
        if (payloadBase) {
            payloadBase[0] &= 0x7fu;
        }
    }
}

// anchor: launcher.exe:0x448a60
// Current tighter source mirror of the generic unhandled-operation log branch reached from later
// leaf wrappers (for example `0x449a70` / `0x44af60`) after base `0x4490c0` returns false-ish.
static void CMessageConnection_0x4b7928_LogUnhandledOperation(void* workItem) {
    const auto* header =
        static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    const uint32_t workType = header ? header->workType : 0u;
    const uint32_t resultCode = header ? header->statusOrPayloadDword08 : 0u;
    const char* resultName = CResultNameArrayItem_GetResultName(resultCode);
    spdlog::info(
        "Got unhandled op of type {} with status {}",
        static_cast<unsigned>(workType),
        resultName ? resultName : "UNKNOWN_LTRESULT");
}

// UNANCHORED: source-owned typed owner-context view used by the current `0x4490c0/0x449a70/0x44af60`
// reconstruction when the launcher-owned connection owner at `+0xa4` is the direct login
// mediator. Current static-RE anchor for that ownership write is `0x41d170 / 0x41e500`.
static mxo::ltlogin::CLTLoginMediator* CMessageConnection_0x4b7928_LoginMediatorOwner(
    CMessageConnection_0x4b7928* self) {
    if (!self) {
        return nullptr;
    }

    mxo::ltlogin::CLTLoginMediator* mediator =
        static_cast<mxo::ltlogin::CLTLoginMediator*>(self->OwnerContext());
    if (mediator != nullptr &&
        (self == mediator->authConnection_ || self == mediator->marginConnection_)) {
        return mediator;
    }

    mediator = mxo::ltlogin::g_CurrentLoginMediator;
    return (mediator != nullptr && self->OwnerContext() == mediator) ? mediator : nullptr;
}

static bool CMessageConnection_0x4b7928_IsMediatorAuthConnection(
    CMessageConnection_0x4b7928* self,
    const mxo::ltlogin::CLTLoginMediator* mediator) {
    return self != nullptr && mediator != nullptr && self == mediator->authConnection_;
}

static bool CMessageConnection_0x4b7928_IsMediatorMarginConnection(
    CMessageConnection_0x4b7928* self,
    const mxo::ltlogin::CLTLoginMediator* mediator) {
    return self != nullptr && mediator != nullptr && self == mediator->marginConnection_;
}

// anchor: launcher.exe:0x4490c0 first dispatch on `workItem+0x04`
// Source-owned decomposition of the initial work-type test inside
// `CMessageConnection_0x4b7928::OnOperationCompleted`.
static uint32_t CMessageConnection_0x4b7928_WorkItemType(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    return header->workType;
}

// anchor: launcher.exe:0x4490c0 -> 0x434d00
// Source-owned read of the shared `workItem+0x08` status/payload dword used by the type-3 early
// return and by several later source-owned owner-fallback helpers.
static uint32_t CMessageConnection_0x4b7928_WorkItemStatusOrPayloadDword(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader* statusWorkItem =
        static_cast<const CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
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
static bool CMessageConnection_0x4b7928_AppendReceiveMessagePayloadSpan(
    CMessageConnectionMessageRef_0x4ba23c* messageRef,
    const uint8_t* payloadBytes,
    uint32_t payloadByteCount) {
    if (!messageRef || !messageRef->messageStorage0c) {
        return false;
    }
    if (payloadByteCount == 0u) {
        return true;
    }
    if (!payloadBytes || payloadByteCount > CMessageConnectionMessageStorage_0x4ba208::kMaxPayloadByteCount) {
        return false;
    }

    const uint16_t oldPayloadByteCount = messageRef->PayloadByteCount();
    const uint32_t requestedPayloadByteCount =
        static_cast<uint32_t>(oldPayloadByteCount) + payloadByteCount;
    if (requestedPayloadByteCount > CMessageConnectionMessageStorage_0x4ba208::kMaxPayloadByteCount) {
        return false;
    }

    uint8_t* const appendPointer = messageRef->PayloadAppendPointer();
    if (!appendPointer) {
        return false;
    }

    std::copy_n(payloadBytes, payloadByteCount, appendPointer);
    const uint16_t newPayloadByteCount =
        messageRef->GrowPayloadByteCount(static_cast<uint16_t>(payloadByteCount));
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
static bool CMessageConnection_0x4b7928_CopyParsedPacketIntoReceivedMessageRefScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* workItem,
    CMessageConnectionMessageRef_0x4ba23c* outMessageRef,
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
    if (!CMessageConnection_0x4b7928_AppendReceiveMessagePayloadSpan(
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
        if (!CMessageConnection_0x4b7928_AppendReceiveMessagePayloadSpan(
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
static bool CMessageConnection_0x4b7928_ResolveMessageCodePointer(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
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

    const CMessageConnectionMessageStorage_0x4ba208* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
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

static bool CBaseMarginConnection_0x4b64a8_ResolveLogicalPayloadSpanScaffold(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    CBaseMarginConnection_0x4b64a8_ParsedPayloadSpanScaffold* outParsedPayload) {
    if (outParsedPayload) {
        *outParsedPayload = {};
    }
    if (!outParsedPayload) {
        return false;
    }

    const CMessageConnectionMessageStorage_0x4ba208* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
    if (!payloadBytes || payloadByteCount == 0u) {
        return false;
    }

    const uint8_t* messageCodePointer = nullptr;
    if (!CMessageConnection_0x4b7928_ResolveMessageCodePointer(
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

static bool CBaseMarginConnection_0x4b64a8_OnMessageCode4Scaffold(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    CBaseMarginConnection_0x4b64a8_Code4MessageScaffold* outCode4Message,
    bool parseIncomingMessage) {
    if (outCode4Message) {
        *outCode4Message = {};
    }
    if (!parseIncomingMessage || !outCode4Message ||
        !CBaseMarginConnection_0x4b64a8_ResolveLogicalPayloadSpanScaffold(
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

static bool CBaseMarginConnection_0x4b64a8_OnMessageCode5Scaffold(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    CBaseMarginConnection_0x4b64a8_Code5MessageScaffold* outCode5Message,
    bool parseIncomingMessage) {
    if (outCode5Message) {
        *outCode5Message = {};
    }
    if (!parseIncomingMessage || !outCode5Message ||
        !CBaseMarginConnection_0x4b64a8_ResolveLogicalPayloadSpanScaffold(
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

// anchor: launcher.exe:0x442d00
// Source-owned narrow predicate exposing the specific consumed-code gate inside
// `CBaseMarginConnection_0x4b64a8::DispatchMessage`.
// FIDELITY: Static-RE at 0x442d00 calls 0x41bc20, which handles BOTH headerless
// and non-headerless cases internally via headerless10 check.
// See launcher.exe:0x442d0e - CALL 0x41bc20
static uint32_t CBaseMarginConnection_0x4b64a8_DispatchMessageFilterScaffold(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    uint16_t* outDecodedMessageCode,
    bool* outUsedHeaderlessLocatorDecode,
    bool* outHadValidMessageCode) {
    if (outHadValidMessageCode) {
        *outHadValidMessageCode = false;
    }
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = (messageRef.headerless10 != 0u);
    }

    // anchor: launcher.exe:0x442d0e - CALL 0x41bc20
    // Static-RE uses single decode function for all cases
    const uint16_t decodedMessageCode =
        CMessageConnectionMessageRef_DecodeMessageCode(
            const_cast<CMessageConnectionMessageRef_0x4ba23c*>(&messageRef));

    if (outDecodedMessageCode) {
        *outDecodedMessageCode = decodedMessageCode;
    }

    const bool hadValidMessageCode = (decodedMessageCode != 0u);
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
static bool CMessageConnection_0x4b7928_DriveAgendaReadHelperChainScaffold(
    PacketProcessingAgenda_0x469850* agenda,
    CMessageConnectionMessageRef_0x4ba23c* inputMessageRef) {
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
// - original `0x469930` (`PacketProcessingAgenda_0x469850_DispatchReadHelperChain`) does not
//   pre-clear agenda `+0x08`; it simply drives the chain and returns the slot afterward
static CMessageConnectionMessageRef_0x4ba23c* CMessageConnection_0x4b7928_ApplyReceivePacketAgendaScaffold(
    PacketProcessingAgenda_0x469850* agenda,
    CMessageConnectionMessageRef_0x4ba23c* inputMessageRef,
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
    if (!CMessageConnection_0x4b7928_DriveAgendaReadHelperChainScaffold(agenda, inputMessageRef)) {
        return nullptr;
    }

    return agenda->readOutputSlot08;
}

// anchor: launcher.exe:0x4490c0 packetized branch reads inner `+0x0f & 0x07`
static bool CMessageConnection_0x4b7928_ResolvePacketizedProtocolIdScaffold(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    uint8_t* outProtocolId) {
    if (outProtocolId) {
        *outProtocolId = 0u;
    }

    const CMessageConnectionMessageStorage_0x4ba208* const messageStorage = messageRef.messageStorage0c;
    if (!messageStorage) {
        return false;
    }

    const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
    const uint8_t* const payloadBytes = messageStorage->PayloadBase();
    if (!payloadBytes || payloadByteCount < 4u) {
        return false;
    }

    if (outProtocolId) {
        *outProtocolId = static_cast<uint8_t>(payloadBytes[3] & 0x07u);
    }
    return true;
}

}  // namespace

// anchor: launcher.exe:0x41bc20 / CMessageConnectionMessageRef_DecodeMessageCode
// Exported decode helper for CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84
// at launcher.exe:0x41c5c0. Decodes the message code from a message-ref payload.
// FIDELITY: Static-RE function at 0x41bc20 handles BOTH headerless and non-headerless cases.
// It checks headerless10 internally and uses locator-based offset lookup for headerless.
bool CMessageConnection_0x4b7928_DecodeMessageCode(
    const CMessageConnectionMessageRef_0x4ba23c& messageRef,
    uint16_t* outMessageCode,
    bool* outUsedHeaderlessLocatorDecode) {
    // anchor: launcher.exe:0x41bc20 - CMessageConnectionMessageRef_DecodeMessageCode
    // Handles both non-headerless (payload[0]) and headerless (locator-based offset) internally
    *outMessageCode = CMessageConnectionMessageRef_DecodeMessageCode(
        const_cast<CMessageConnectionMessageRef_0x4ba23c*>(&messageRef));
    if (outUsedHeaderlessLocatorDecode) {
        *outUsedHeaderlessLocatorDecode = (messageRef.headerless10 != 0u);
    }
    return (*outMessageCode != 0u);
}

// UNANCHORED: source-owned post-copy dispatch seam beneath `launcher.exe:0x4490c0`.
uint32_t CMessageConnection_0x4b7928::DispatchCopiedParsedPacketTailScaffold(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    (void)messageRef;
    return 0u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x30`
uint32_t CMessageConnection_0x4b7928::DispatchPacketizedProtocol5MessageRefScaffold(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    (void)messageRef;
    return 1u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x34`
uint32_t CMessageConnection_0x4b7928::DispatchPacketizedProtocol7MessageRefScaffold(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    (void)messageRef;
    return 1u;
}

// anchor family: launcher.exe:0x4490c0 -> vtable `+0x38`
void CMessageConnection_0x4b7928::PreDispatchMessageRefScaffold(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
    (void)messageRef;
}

// anchor: launcher.exe:0x4490c0
// string-backed original name: CMessageConnection_0x4b7928::OnOperationCompleted
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
uint32_t CMessageConnection_0x4b7928::OnOperationCompleted(void* workItem) {
    if (!Engine() || !workItem) {
        return 0u;
    }

    const uint32_t workType = CMessageConnection_0x4b7928_WorkItemType(workItem);

    // anchor: launcher.exe:0x4490c0 - work type 1 (close) handling
    if (workType == CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        if (closeCompletionHelper80_) {
            closeCompletionHelper80_->Signal();
            return 1u;
        }
        return 0u;
    }

    // anchor: launcher.exe:0x4490f0 - work type 2 (connection status) handling
    if (workType == CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus) {
        if (connectCompletionHelper7c_) {
            connectCompletionHelper7c_->Signal();
            return 1u;
        }
        return 0u;
    }

    // anchor: launcher.exe:0x449120 - work type 3 (parsed packet) handling
    if (workType != CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeParsedPacket) {
        return 0u;
    }

    // anchor: launcher.exe:0x449130 - check status/payload dword
    if (CMessageConnection_0x4b7928_WorkItemStatusOrPayloadDword(workItem) != 0u) {
        return 1u;
    }

    CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08* parsedPacketWorkItem =
        static_cast<CLTTCPConnection_ParsedPacketWorkItemScaffold_0x4b3e08*>(workItem);
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
    std::unique_ptr<CMessageConnectionMessageRef_0x4ba23c, CMessageConnectionMessageRefReleaseDeleter>
        ownedCopiedMessageRef(new CMessageConnectionMessageRef_0x4ba23c());
    CMessageConnectionMessageRef_0x4ba23c* const copiedMessageRef = ownedCopiedMessageRef.get();
    copiedMessageRef->ResetForPacketBuilder(!packetizedMessagesEnabled_);
    const bool copied = CMessageConnection_0x4b7928_CopyParsedPacketIntoReceivedMessageRefScaffold(
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
        CMessageConnection_0x4b7928_LogUnhandledOperation(workItem);
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
    if (const CMessageConnectionMessageStorage_0x4ba208* const messageStorage = copiedMessageRef->messageStorage0c) {
        const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
        if (const uint8_t* const payloadBytes = messageStorage->PayloadBase();
            payloadBytes && payloadByteCount != 0u) {
            lastReceivedPacketBodyBytesScaffold_.assign(
                payloadBytes,
                payloadBytes + payloadByteCount);
        }
    }
    lastReceivedPacketHeaderlessScaffold_ = (copiedMessageRef->headerless10 != 0u);

    // anchor: launcher.exe:0x449340 - headerless locator validation
    CMessageConnectionMessageRef_0x4ba23c* messageRefForDispatch = copiedMessageRef;
    bool agendaTouched = false;
    if (messageRefForDispatch->headerless10 != 0u) {
        uint8_t targetLocatorType = 0u;
        uint8_t senderLocatorType = 0u;
        if (!CMessageConnection_0x4b7928_ResolveMessageCodePointer(
                *messageRefForDispatch,
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

    if (PacketProcessingAgenda_0x469850* agenda = packetAgenda_.get();
        agenda && agenda->created) {
        messageRefForDispatch = agenda->ApplyReceivePacketAgenda(
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
            if (const CMessageConnectionMessageStorage_0x4ba208* const messageStorage = messageRefForDispatch->messageStorage0c) {
                const uint16_t payloadByteCount = messageStorage->PayloadByteCount();
                if (const uint8_t* const payloadBytes = messageStorage->PayloadBase();
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
                 static_cast<const CStreamPacketEncryptionHelperBase_0x4b81c8*>(&agenda->embeddedReadHelper0c))
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
        CMessageConnection_0x4b7928_ResolvePacketizedProtocolIdScaffold(
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
uint32_t CMessageConnection_0x4b7928::SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext) {
    if (!Engine() || !packetData || packetByteCount == 0) {
        return 0;
    }

    // Current starter path deliberately routes through the inherited recovered
    // `CLTTCPConnection::SendBuffer` wrapper instead of pretending this is already a faithful
    // packet serializer.
    return CLTTCPConnection::SendBuffer(packetData, packetByteCount, completionContext);
}

// anchor: launcher.exe:0x455cd0 - CMessageConnectionMessage_CreateRef
void CMessageConnectionMessage_CreateRef(
    MessageConnectionMessageRefHelper_0x4489d0* messageRefHelper,
    int messageContext) {
    // This corresponds to the original CMessageConnectionMessage_CreateRef function
    // that creates a new message reference and sets up the context.

    messageRefHelper->messageRef00 = new CMessageConnectionMessageRef_0x4ba23c();
    if (messageRefHelper->messageRef00) {
        messageRefHelper->messageRef00->AddRef();
        messageRefHelper->messageRef00->messageContext14 = messageContext;
        // Reset the message ref for packet building
        messageRefHelper->messageRef00->ResetForPacketBuilder(false);
    }
}

// anchor: launcher.exe:0x4489d0 - Message ref handle assignment
void MessageConnectionMessageRefHelper_0x4489d0::CMessageConnectionMessageRefHandle_AssignRetained(
    MessageConnectionMessageRefHelper_0x4489d0* targetHandle,
    CMessageConnectionMessageRef_0x4ba23c* sourceMessageRef) {
    // Release any existing reference
    if (targetHandle->messageRef00) {
        targetHandle->messageRef00->Release();
    }

    // Assign new reference and add ref if it exists
    targetHandle->messageRef00 = sourceMessageRef;
    if (sourceMessageRef) {
        sourceMessageRef->AddRef();
    }
}

// anchor: launcher.exe:0x41bc20 - CMessageConnectionMessageRef_DecodeMessageCode
// FIDELITY: Static-RE shows this function handles BOTH headerless and non-headerless cases.
// When headerless10 != 0, uses locator-based offset lookup. Also sets headerless10=1 (redundant set).
uint16_t CMessageConnectionMessageRef_DecodeMessageCode(
    CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    if (!messageRef || !messageRef->messageStorage0c) {
        return 0;
    }

    messageRef->AddRef();  // FIDELITY: AddRef from static-RE at +0x04 vtable

    const uint8_t* payload = messageRef->messageStorage0c->PayloadBase();
    if (!payload) {
        messageRef->Release();
        return 0;
    }

    uint16_t messageCode = 0;
    if (messageRef->headerless10 == 0u) {
        // Non-headerless: message code at payload[0]
        messageCode = static_cast<uint16_t>(payload[0]);
    } else {
        // Headerless: use locator-based offset lookup
        // anchor: launcher.exe:0x41bbd5-0x41bbe6 - g_MessageOffsetLookupTable lookups
        const uint8_t locatorByte = payload[1];
        const uint8_t targetLocatorType = static_cast<uint8_t>((locatorByte >> 4) & 0x07u);
        const uint8_t senderLocatorType = static_cast<uint8_t>(locatorByte & 0x07u);
        static const uint8_t kMessageOffsetLookupTable[8] = {
            0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c
        };
        const size_t offset =
            kMessageOffsetLookupTable[targetLocatorType] +
            kMessageOffsetLookupTable[senderLocatorType] + 0x12;
        messageRef->headerless10 = 1u;  // FIDELITY: redundant set from static-RE
        messageCode = static_cast<uint16_t>(payload[offset]);
    }

    // Check for high-bit set (means 2-byte little-endian code)
    if ((messageCode & 0x80u) != 0u) {
        messageRef->Release();
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(messageCode) << 8) |
            static_cast<uint16_t>(payload[1])) & 0x7fffu;
    }

    messageRef->Release();
    return messageCode;
}

// anchor: launcher.exe:0x41bbb0 - CMessageConnectionMessageRef_DecodeMessageCodeAlternate
// FIDELITY: Separate function for headerless that doesn't check headerless10 flag.
// Uses raw payload[2] directly (after locator bytes).
uint16_t CMessageConnectionMessageRef_DecodeMessageCodeAlternate(
    CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    if (!messageRef || !messageRef->messageStorage0c) {
        return 0;
    }

    messageRef->AddRef();  // FIDELITY: AddRef from static-RE

    const uint8_t* payload = messageRef->messageStorage0c->PayloadBase();
    if (!payload || messageRef->messageStorage0c->PayloadByteCount() < 2) {
        messageRef->Release();
        return 0;
    }

    // Headerless format: message code is in bytes 2-3 (after locator bytes)
    uint16_t messageCode = static_cast<uint16_t>(payload[2]);

    if ((messageCode & 0x80u) != 0u) {
        messageRef->Release();
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(messageCode) << 8) |
            static_cast<uint16_t>(payload[3])) & 0x7fffu;
    }

    messageRef->Release();
    return messageCode;
}



// ============================================================
// VTable 0x004b64a8 - shared base margin router
// ============================================================
CBaseMarginConnection_0x4b64a8::CBaseMarginConnection_0x4b64a8()
    : CMessageConnection_0x4b7928() {}

CBaseMarginConnection_0x4b64a8::CBaseMarginConnection_0x4b64a8(CLTThreadPerClientTCPEngine_0x4b2768* connectionEngine)
    : CMessageConnection_0x4b7928(connectionEngine) {}

CBaseMarginConnection_0x4b64a8::~CBaseMarginConnection_0x4b64a8() = default;

uint32_t CBaseMarginConnection_0x4b64a8::DispatchCopiedParsedPacketTailScaffold(
    CMessageConnectionMessageRef_0x4ba23c& messageRef) {
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
CAuthStartupConnection_0x4afef0::CAuthStartupConnection_0x4afef0()
    : CBaseMarginConnection_0x4b64a8() {}

CAuthStartupConnection_0x4afef0::CAuthStartupConnection_0x4afef0(CLTThreadPerClientTCPEngine_0x4b2768* authEngine)
    : CBaseMarginConnection_0x4b64a8(authEngine) {}

CAuthStartupConnection_0x4afef0::~CAuthStartupConnection_0x4afef0() = default;

// anchor: launcher.exe:0x449a30 -> owner vtable `+0x180` / `0x41f250`
uint32_t CAuthStartupConnection_0x4afef0::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    if (CBaseMarginConnection_0x4b64a8::DispatchMessage(messageRef) != 0u) {
        return 1u;
    }

    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_0x4b7928_LoginMediatorOwner(this);
    if (!mediator || !CMessageConnection_0x4b7928_IsMediatorAuthConnection(this, mediator)) {
        return 0u;
    }

    const uint32_t handled = mediator->DispatchCurrentHelperAuthMessage(messageRef);
    spdlog::info(
        "CAuthStartupConnection_0x4afef0::DispatchMessage forwarded unconsumed messageRef={} to owner+0x180 currentState={} handled={} this={} ownerContext={} remoteHost='{}'",
        fmt::ptr(messageRef),
        fmt::ptr(mediator->currentState_),
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
uint32_t CAuthStartupConnection_0x4afef0::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    uint32_t handled = 0u;
    if (CMessageConnection_0x4b7928::OnOperationCompleted(workItem) != 0u) {
        handled = 1u;
    } else {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_0x4b7928_LoginMediatorOwner(this);
        if (mediator && CMessageConnection_0x4b7928_IsMediatorAuthConnection(this, mediator) &&
            mediator->HandleAuthConnectionCompletionFallback(this, workItem) != 0u) {
            handled = 1u;
        } else {
            CMessageConnection_0x4b7928_LogUnhandledOperation(workItem);
        }
    }

    if (CMessageConnection_0x4b7928_WorkItemType(workItem) ==
        CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        // anchor: launcher.exe:0x449a70 tail
        // The original tail calls vtable[0](1), i.e. the deleting-dtor-style teardown path,
        // not the ordinary `Close(bool)` wrapper.
        delete this;
    }

    return handled;
}

// ============================================================
// VTable 0x004aff38 - CMarginConnection_0x4aff38
// ============================================================
// Later leaf on top of:
// CLTTCPConnection (0x004b8034)
// └── CMessageConnection_0x4b7928-family base surface
//     └── CBaseMarginConnection_0x4b64a8 (0x004b64a8)
//         └── CMarginConnection_0x4aff38 (0x004aff38)

// UNANCHORED: source-owned narrow leaf ctor.
CMarginConnection_0x4aff38::CMarginConnection_0x4aff38()
    : CBaseMarginConnection_0x4b64a8() {}

// UNANCHORED: source-owned narrow leaf ctor that only seeds the recovered base engine field.
CMarginConnection_0x4aff38::CMarginConnection_0x4aff38(CLTThreadPerClientTCPEngine_0x4b2768* marginEngine)
    : CBaseMarginConnection_0x4b64a8(marginEngine) {}

// UNANCHORED: source-owned default destructor.
// Current tighter static-RE split:
// - live leaf teardown is through the scalar-deleting-dtor wrappers at `0x41cf50/0x41cf80`
// - `0x41ce80` is the separate connection `+0x98` reply-copy helper, not this C++ destructor body
CMarginConnection_0x4aff38::~CMarginConnection_0x4aff38() = default;

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
void CBaseMarginConnection_0x4b64a8::SetMessageCode4SuccessFlag84(bool value) {
    messageCode4SuccessFlag84_ = value;
}

// anchor family: launcher.exe:0x441850 / 0x44af20 -> connection `+0x84`
bool CBaseMarginConnection_0x4b64a8::MessageCode4SuccessFlag84() const {
    return messageCode4SuccessFlag84_;
}

// anchor: launcher.exe:0x441850
uint32_t CBaseMarginConnection_0x4b64a8::DispatchMessageCode4LocalCompletionWorkItem(uint32_t workPayloadStatus) {
    // meth_0x441850 (0x441850) creates a local work item and passes it to connection vtable+0x10.
    // Original directly constructs the work item; use the simple scaffold approach that works.
    CMarginConnectionLocalCompletionWorkItemScaffold workItem = {};
    workItem.header.workType = 0x0bu;
    workItem.header.statusOrPayloadDword08 = workPayloadStatus;

    CMessageConnection_0x4b7928* selfAsMessageConnection = this;
    const uint32_t handled = selfAsMessageConnection->OnOperationCompleted(&workItem);
    spdlog::info(
        "CBaseMarginConnection_0x4b64a8::DispatchMessageCode4LocalCompletionWorkItem synthesized local type0x0b workItem status=0x{:08x} handled={} this={} ownerContext={} currentState={} remoteHost='{}'",
        workPayloadStatus,
        handled,
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        fmt::ptr(CMessageConnection_0x4b7928_LoginMediatorOwner(this)
                     ? CMessageConnection_0x4b7928_LoginMediatorOwner(this)->currentState_
                     : nullptr),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return handled;
}

// anchor: launcher.exe:0x41ce80 -> connection `+0x98`
bool CBaseMarginConnection_0x4b64a8::StoreBootstrapReplyCopy98(const void* bytes, size_t byteCount) {
    if (!bytes || byteCount != bootstrapReplyCopy98_.size()) {
        return false;
    }

    std::copy_n(
        static_cast<const uint8_t*>(bytes),
        bootstrapReplyCopy98_.size(),
        bootstrapReplyCopy98_.begin());
    hasBootstrapReplyCopy98_ = true;
    spdlog::info(
        "CBaseMarginConnection_0x4b64a8::StoreBootstrapReplyCopy98 stored reply-copy block bytes=0x{:03x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(bootstrapReplyCopy98_.size()),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
    return true;
}

namespace {

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

static uint32_t CryptoPP_Integer_RoundWordCapacityForValue(
    const CryptoPP::Integer& value) {
    const size_t encodedByteCount = std::max<size_t>(
        static_cast<size_t>(value.MinEncodedSize()),
        1u);
    const size_t requiredWordCount = (encodedByteCount + 3u) / 4u;
    return RoundCMarginConnectionBootstrapPrepBigIntCapacityWords(requiredWordCount);
}

static bool CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKeyFromIntegers(
    CryptoPP::RSA::PrivateKey& outPrivateKey,
    const CryptoPP::Integer& modulus,
    const CryptoPP::Integer& publicExponent,
    const CryptoPP::Integer& privateExponent) {
    spdlog::debug(
        "CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKeyFromIntegers: "
        "modulusBits={} publicExponentBits={} privateExponentBits={} modulusBytes={}",
        modulus.BitCount(),
        publicExponent.BitCount(),
        privateExponent.BitCount(),
        modulus.ByteCount());

    if (modulus.IsZero() || publicExponent.IsZero() || privateExponent.IsZero()) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKeyFromIntegers: "
            "zero CryptoPP::Integer component detected");
        return false;
    }

    outPrivateKey.Initialize(modulus, publicExponent, privateExponent);

    spdlog::debug(
        "CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKeyFromIntegers: "
        "CRT derivation succeeded ciphertextBytes={} prime1Bits={} prime2Bits={}",
        outPrivateKey.GetModulus().ByteCount(),
        outPrivateKey.GetPrime1().BitCount(),
        outPrivateKey.GetPrime2().BitCount());
    return true;
}

}  // namespace

// anchor: launcher.exe:0x465d70
static bool CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKey(
    CryptoPP::RSA::PrivateKey& outPrivateKey,
    const CryptoPP::Integer& modulus,
    const CryptoPP::Integer& publicExponent,
    const CryptoPP::Integer& privateExponent) {
    try {
        return CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKeyFromIntegers(
            outPrivateKey,
            modulus,
            publicExponent,
            privateExponent);
    } catch (const CryptoPP::Exception& exception) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKey failed: {}",
            exception.what());
        return false;
    }
}

static uint32_t CMarginConnectionAuthBootstrapState_0x443220_CiphertextByteCount(
    const CryptoPP::RSA::PrivateKey& privateKey,
    bool privateKeyInitialized) {
    return privateKeyInitialized ? static_cast<uint32_t>(privateKey.GetModulus().ByteCount()) : 0u;
}

// anchor: launcher.exe:0x443220 / complete-object ctor reached from `0x443340`
CMarginConnectionAuthBootstrapState_0x443220::CMarginConnectionAuthBootstrapState_0x443220(
    const CryptoPP::Integer& modulus,
    const CryptoPP::Integer& publicExponent,
    const CryptoPP::Integer& privateExponent,
    [[maybe_unused]] int constructVirtualBaseStateFlag) {
    // Fidelity notes for launcher.exe:0x443220:
    // - the original complete-object ctor walks MSVC MI/vbptr state before calling 0x465d70
    // - the original caller hands old-Crypto++ `Integer` objects into that boundary
    // - source keeps the ctor/helper boundaries but uses direct `CryptoPP::Integer` semantics
    //   rather than a launcher-local stand-in for that third-party type
    bootstrapPrivateKeyInitialized_ =
        CMarginConnectionAuthBootstrapState_0x443220_InitializeBootstrapPrivateKey(
            bootstrapPrivateKey_0x0c,
            modulus,
            publicExponent,
            privateExponent);

    if (!bootstrapPrivateKeyInitialized_) {
        return;
    }

    try {
        cryptoPPDecryptor_0x442b70 = CryptoPP::RSAES_OAEP_SHA_Decryptor(
            bootstrapPrivateKey_0x0c.GetModulus(),
            bootstrapPrivateKey_0x0c.GetPublicExponent(),
            bootstrapPrivateKey_0x0c.GetPrivateExponent(),
            bootstrapPrivateKey_0x0c.GetPrime1(),
            bootstrapPrivateKey_0x0c.GetPrime2(),
            bootstrapPrivateKey_0x0c.GetModPrime1PrivateExponent(),
            bootstrapPrivateKey_0x0c.GetModPrime2PrivateExponent(),
            bootstrapPrivateKey_0x0c.GetMultiplicativeInverseOfPrime2ModPrime1());
    } catch (const CryptoPP::Exception& exception) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220 ctor failed to initialize CryptoPP::RSAES_OAEP_SHA_Decryptor key: {}",
            exception.what());
    }
}

// anchor: launcher.exe:0x468130 / vtable+0x24 in cls_0x4b69b4
// Source now uses the real CryptoPP::RSAES_OAEP_SHA_Decryptor implementation for the
// RSA private operation + OAEP/SHA1 unpadding path.
void* CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption(
    void* outputBuffer,
    const void* cryptoContext,
    uint32_t encryptedBlobPtr,
    void* localBufferPtr) {
    auto* outBytes = static_cast<uint8_t*>(outputBuffer);
    outBytes[0] = 0;
    *reinterpret_cast<uint32_t*>(outBytes + 4) = 0;

    spdlog::debug(
        "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption BEGIN "
        "this={} outputBuffer={} cryptoContext={} encryptedBlobPtr={} localBufferPtr={}",
        fmt::ptr(this),
        fmt::ptr(outputBuffer),
        fmt::ptr(cryptoContext),
        encryptedBlobPtr,
        fmt::ptr(localBufferPtr));

    if (!bootstrapPrivateKeyInitialized_) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption missing private key");
        return outputBuffer;
    }

    auto* cryptoHelper = const_cast<CryptoInitHelperGlobal_0x4f7bf4*>(
        static_cast<const CryptoInitHelperGlobal_0x4f7bf4*>(cryptoContext));
    if (!cryptoHelper) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption missing RNG helper");
        return outputBuffer;
    }

    try {
        const size_t ciphertextByteCount = cryptoPPDecryptor_0x442b70.FixedCiphertextLength();
        const size_t plaintextCapacity =
            std::max<size_t>(cryptoPPDecryptor_0x442b70.FixedMaxPlaintextLength(), 1u);
        std::vector<CryptoPP::byte> plaintextBytes(plaintextCapacity, 0u);

        const CryptoPP::DecodingResult decodingResult = cryptoPPDecryptor_0x442b70.Decrypt(
            cryptoHelper->RandomPoolSubobject04(),
            reinterpret_cast<const CryptoPP::byte*>(encryptedBlobPtr),
            ciphertextByteCount,
            plaintextBytes.data(),
            CryptoPP::g_nullNameValuePairs);

        if (!decodingResult.isValidCoding) {
            spdlog::warn(
                "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption OAEP decode failed");
            return outputBuffer;
        }

        outBytes[0] = 1;
        *reinterpret_cast<uint32_t*>(outBytes + 4) =
            static_cast<uint32_t>(decodingResult.messageLength);
        const size_t bytesToCopy = std::min<size_t>(decodingResult.messageLength, 96u);
        std::copy_n(plaintextBytes.data(), bytesToCopy, outBytes + 8);

        spdlog::debug(
            "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption SUCCESS "
            "ciphertextBytes={} plaintextBytes={} copiedBytes={} algorithm='{}'",
            ciphertextByteCount,
            decodingResult.messageLength,
            bytesToCopy,
            cryptoPPDecryptor_0x442b70.AlgorithmName());
    } catch (const CryptoPP::Exception& exception) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220::PerformRSADecryption failed: {}",
            exception.what());
    }

    return outputBuffer;
}

// anchor: launcher.exe:0x437810 / vtable +0x1c
void* CMarginConnectionAuthBootstrapState_0x443220::DecryptChallenge(
    void* outputBuffer,
    const void* cryptoContext,
    uint32_t encryptedBlobPtr,
    uint16_t expectedOutputSize,
    void* localBufferPtr) {
    spdlog::debug(
        "CMarginConnectionAuthBootstrapState_0x443220::DecryptChallenge BEGIN "
        "outputBuffer={} cryptoContext={} encryptedBlobPtr={} expectedOutputSize={} localBufferPtr={}",
        fmt::ptr(outputBuffer),
        fmt::ptr(cryptoContext),
        encryptedBlobPtr,
        expectedOutputSize,
        fmt::ptr(localBufferPtr));

    const uint32_t expectedPayloadSize =
        CMarginConnectionAuthBootstrapState_0x443220_CiphertextByteCount(
            bootstrapPrivateKey_0x0c, bootstrapPrivateKeyInitialized_);
    if (expectedOutputSize != expectedPayloadSize) {
        spdlog::warn(
            "CMarginConnectionAuthBootstrapState_0x443220::DecryptChallenge size mismatch expected={} actual={}",
            expectedPayloadSize,
            expectedOutputSize);
        auto* outBytes = static_cast<uint8_t*>(outputBuffer);
        outBytes[0] = 0;
        *reinterpret_cast<uint32_t*>(outBytes + 4) = 0;
        return outputBuffer;
    }

    PerformRSADecryption(outputBuffer, cryptoContext, encryptedBlobPtr, localBufferPtr);
    return outputBuffer;
}

// anchor: launcher.exe:0x443340 -> connection `+0xa0`
// State5 only constructs/stores this object. The first later original consumer is
// `0x4429b0`, which loads connection `+0xa0` and calls prep-object `0x437810`.
void CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0(
    const CryptoPP::Integer& modulus,
    const CryptoPP::Integer& publicExponent,
    const CryptoPP::Integer& privateExponent) {
    connection_.bootstrapPrepStateA0_.reset(new (std::nothrow)
        CMarginConnectionAuthBootstrapState_0x443220(
            modulus, publicExponent, privateExponent, 1));
    if (!connection_.bootstrapPrepStateA0_) {
        spdlog::warn(
            "CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0 allocation failed this={} ownerContext={} remoteHost='{}'",
            fmt::ptr(&connection_),
            fmt::ptr(connection_.OwnerContext()),
            connection_.RemoteHostName().empty() ? std::string("<empty>") : connection_.RemoteHostName());
        return;
    }

    const auto* prepState = connection_.bootstrapPrepStateA0_.get();

    uint32_t prime1Cap = 0u;
    uint32_t prime2Cap = 0u;
    uint32_t crtExp1Cap = 0u;
    uint32_t crtExp2Cap = 0u;
    uint32_t crtInverseCap = 0u;
    if (prepState->HasBootstrapPrivateKey()) {
        const auto& privateKey = prepState->BootstrapPrivateKey();
        prime1Cap =
            CryptoPP_Integer_RoundWordCapacityForValue(privateKey.GetPrime1());
        prime2Cap =
            CryptoPP_Integer_RoundWordCapacityForValue(privateKey.GetPrime2());
        crtExp1Cap = CryptoPP_Integer_RoundWordCapacityForValue(
            privateKey.GetModPrime1PrivateExponent());
        crtExp2Cap = CryptoPP_Integer_RoundWordCapacityForValue(
            privateKey.GetModPrime2PrivateExponent());
        crtInverseCap = CryptoPP_Integer_RoundWordCapacityForValue(
            privateKey.GetMultiplicativeInverseOfPrime2ModPrime1());
    }

    spdlog::info(
        "CMarginConnectionBootstrapPrepStateOwner_0x443340::StoreBootstrapPrepStateA0 stored "
        "CryptoPP-backed auth bootstrap state sourceSize=0x{:02x} originalCompleteObjectSize=0xe0 modulusCap=0x{:02x} exponentCap=0x{:02x} privateExponentCap=0x{:02x} prime1Cap=0x{:02x} prime2Cap=0x{:02x} crtExp1Cap=0x{:02x} crtExp2Cap=0x{:02x} crtInverseCap=0x{:02x} this={} ownerContext={} remoteHost='{}'",
        static_cast<unsigned>(sizeof(CMarginConnectionAuthBootstrapState_0x443220)),
        static_cast<unsigned>(CryptoPP_Integer_RoundWordCapacityForValue(modulus)),
        static_cast<unsigned>(CryptoPP_Integer_RoundWordCapacityForValue(publicExponent)),
        static_cast<unsigned>(CryptoPP_Integer_RoundWordCapacityForValue(privateExponent)),
        static_cast<unsigned>(prime1Cap),
        static_cast<unsigned>(prime2Cap),
        static_cast<unsigned>(crtExp1Cap),
        static_cast<unsigned>(crtExp2Cap),
        static_cast<unsigned>(crtInverseCap),
        fmt::ptr(&connection_),
        fmt::ptr(connection_.OwnerContext()),
        connection_.RemoteHostName().empty() ? std::string("<empty>") : connection_.RemoteHostName());
}

// anchor: launcher.exe:0x441f30
// Faithful reimplementation of CMarginConnection_SendStoredBootstrapReplyCopy98
// Assembly flow:
//   0x441f3b-0x441f3e: Packet_0x4af2a4_DefaultCtor(&packetBuilder) - creates messageRef, sets payloadBegin10
//   0x441f4c: Set vtable to 0x4b6524 (Packet_CertConnectRequest_0x4b6524)
//   0x441f5c: messageRef->GrowPayloadByteCount(3)
//   0x441f64-0x441f6a: Write opcode 0x01 and zero word to payload
//   0x441f73: LocalPacketBuilder_ReserveLengthPrefixedTail(0x136)
//   0x441f8b-0x441f97: Copy 0x136 bytes from this+0x98 via REP MOVSD + MOVSW
//   0x441f9c: Call vtable slot 9 (send function) - return value ignored
//   0x441fb0: Release messageRef
void CBaseMarginConnection_0x4b64a8::SendStoredBootstrapReplyCopy98() {
    constexpr uint16_t kReplyCopyByteCount = 0x136u;

    // anchor: launcher.exe:0x441f3b-0x441f3e - construct packet builder on stack
    // DefaultCtor creates a fresh messageRef and sets payloadBegin10 = messageStorage->PayloadBase()
    Packet_CertConnectRequest_0x4b6524 builderEnvelope;

    // anchor: launcher.exe:0x441f5c - grow messageRef payload by 3 bytes (opcode + 2-byte field)
    // The messageRef was created by DefaultCtor with initial payload length 0
    builderEnvelope.messageRef08->GrowPayloadByteCount(3);

    // anchor: launcher.exe:0x441f64, 0x441f6a - write CERT_ConnectRequest opcode (0x01) and zero word
    // FIDELITY: Original assembly at 0x441f43 reads from offset 4 (payloadPtr04),
    // NOT offset 16 (payloadAlias10). The DefaultCtor only writes to offset 4.
    uint8_t* packetPayloadPtr = reinterpret_cast<uint8_t*>(builderEnvelope.payloadPtr04);
    packetPayloadPtr[0] = 0x01u;  // CERT_ConnectRequest opcode
    // FIDELITY: Bytes [1-2] initially zero, updated to offset (3) after reservation by
    // LocalPacketBuilder_ReserveLengthPrefixedTail @ 0x43a2aa: MOV word ptr [ECX+0x1], DI
    *reinterpret_cast<uint16_t*>(packetPayloadPtr + 1) = 0u;

    // anchor: launcher.exe:0x441f73 - reserve length-prefixed tail for bootstrap reply copy
    // Original: LocalPacketBuilder_ReserveLengthPrefixedTail(this, 0x136)
    // This non-virtual helper on Packet_0x4af2a4 is used by 3 packet builder subclasses.
    // It updates bytes [1-2] with offset, sets heapString14/payloadLength14 fields.
    builderEnvelope.ReserveLengthPrefixedTail(kReplyCopyByteCount);

    // anchor: launcher.exe:0x441f8b-0x441f97 - copy 0x136 bytes from bootstrapReplyCopy98_
    // Original uses REP MOVSD (0x4d dwords) + MOVSW (1 word) = 0x136 bytes
    // Base class ReserveLengthPrefixedTail sets debugString14 to point AFTER length prefix
    // (at reservationHeader + 2), which is exactly where we want to copy the data.
    uint8_t* copyDest = reinterpret_cast<uint8_t*>(const_cast<char*>(builderEnvelope.debugString14));
    const uint8_t* bootstrapReplySrc = bootstrapReplyCopy98_.data();
    for (int copyLoopCounter = 0x4d; copyLoopCounter != 0; --copyLoopCounter) {
        *reinterpret_cast<uint32_t*>(copyDest) = *reinterpret_cast<const uint32_t*>(bootstrapReplySrc);
        bootstrapReplySrc += 4;
        copyDest += 4;
    }
    *reinterpret_cast<uint16_t*>(copyDest) = *reinterpret_cast<const uint16_t*>(bootstrapReplySrc);

    // anchor: launcher.exe:0x441f9c - send packet via vtable slot 9
    // Original: (**(code **)(vftptr + 0x24))(&builderEnvelope, 0)
    // Passes packet builder directly, return value ignored
    SendPacketMessageRef(*builderEnvelope.messageRef08);

    spdlog::info(
        "CBaseMarginConnection_0x4b64a8::SendStoredBootstrapReplyCopy98 sent CERT_ConnectRequest "
        "opcode=0x01 payloadBase10={} reservedReplyCopyBytes=0x{:03x} "
        "this={} ownerContext={} remoteHost='{}'",
        builderEnvelope.payloadPtr04,
        static_cast<unsigned>(builderEnvelope.payloadSize18),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()),
        RemoteHostName().empty() ? std::string("<empty>") : RemoteHostName());
}

// anchor: launcher.exe:0x441470 / 0x44da00 / 0x44daf0
void CBaseMarginConnection_0x4b64a8::EnsureStreamPacketEncryptionModuleFromSeed85() {
    const bool needsInitialInstall = (streamPacketEncryptionModule9c_ == nullptr);
    if (needsInitialInstall) {
        streamPacketEncryptionModule9c_ =
            std::make_unique<CStreamPacketEncryptionModule_0x4b8704>();
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
        "CBaseMarginConnection_0x4b64a8::EnsureStreamPacketEncryptionModuleFromSeed85 {} connection+0x9c module from seed85_94 module={} agenda={} firstDword=0x{:08x} this={} ownerContext={} remoteHost='{}'",
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



// Fidelity helper: invoke message ref vtable+0x08 completion callback if present.
// anchor: launcher.exe:0x442d65 / call dword ptr [EDX + 0x8]
// Note: Original uses raw vtable[2] function pointer call. Our C++ implementation uses virtual
// methods for the message ref, so we model this as the inner storage Release which runs after
// dispatch completes. The original callback signals the completion helper for async flows.
// anchor: launcher.exe:0x442daa / 0x442dac / 0x442d65 / vtable+0x08
// anchor: launcher.exe:0x442da6 / 0x442d65
// Original invokes completion callback via vtable+0x8 on the parse-result object returned
// by OnMessageCodeX. The callback fires only when OnMessageCode returns a valid parse object,
// not based on our parse-success determination. This is the key fidelity point: original flow
// checks parse result object validity (non-null), extracts callback from that object, calls it.
// We model this callback for tracing; the original invokes completion via:
// (**(code **)(*completionCallback + 8))()
// The messageRef is kept alive by the caller's unique_ptr through the dispatch.
static void CBaseMarginConnection_0x4b64a8_InvokeMessageRefCompletionCallback(
    CMessageConnectionMessageRef_0x4ba23c* messageRef,
    void** completionCallbackSlot) {
    if (!completionCallbackSlot || *completionCallbackSlot == nullptr) {
        return;
    }
    spdlog::trace(
        "CBaseMarginConnection_0x4b64a8_InvokeMessageRefCompletionCallback callback={} messageRef={}",
        fmt::ptr(*completionCallbackSlot),
        fmt::ptr(messageRef));
    // Note: In original, this is (**(code **)(*callback + 8))(); call vtable+0x8
    // The callback signals back to the connection completion chain (connection+0x94 vtable+0x08)
}

// anchor: launcher.exe:0x4429b0 (CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse)
// Original signature: void __thiscall CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse(void *this, int param_1)
//   param_1 = parsed message result object with:
//     - message context at param_1+0x14 (dword)
//     - message context at param_1+0x18 (word)
// Original flow (decompile verified):
// 1. Static init check for crypto context at DAT_004f7bf4 (cls_0x4b42bc with vtables 0x4b695c/0x4b68a8/0x4b41e0)
// 2. Create local message ref via CMessageConnectionMessage_CreateRef(&local_8, 0) at cls_0x4489d0
// 3. Get inner storage: pCVar1 = local_8.messageRef00->messageStorage0c
// 4. Read local_c = *(dword *)(param_1 + 0x14)
// 5. Call prep object vtable+0x1c: (**(code **)(**(int **)(this+0xa0) + 0x1c))(local_10, &DAT_004f7bf4, local_c, *(word *)(param_1 + 0x18), pCVar1->payloadBytes0c + payload_offset)
//    - vtable+0x1c = 0x437810 -> calls virt_meth_0x468130 for actual RSA decrypt operation
// 6. Check result: if (*(int *)(iVar2 + 4) == 0) -> error path
//    - Error: MessageBoxA("Failed to decrypt challenge blob from server!", "Error", 0)
//    - Call connection vtable+0xc: (**(code **)(*(int *)this + 0xc))(1) -> close/disconnect
//    - Release message ref via vtable+0x08: (*(local_8.messageRef00->vftptr_0x0->Release_8))(local_8.messageRef00)
//    - Return
// 7. Success path:
//    - GrowPayloadByteCount: (*(local_8.messageRef00->vftptr_0x0->GrowPayloadByteCount_24))(local_8.messageRef00, *(int *)(iVar2 + 4))
//    - Construct cls_0x4b6538 envelope: cls_0x4b6538::cls_0x4b6538(&local_38, (int *)local_8.messageRef00, '\x01') at 0x443220
//      - vtable_0x4b6538 = {0x443aa0 (Destroy), 0x437b50, 0x4425f0, 0x4418a0, 0x481760}
//    - Extract 16 bytes from local_38.mbr_0x10 + 1, +5, +9, +0xd -> write to this+0x85, 0x89, 0x8d, 0x91
//    - Call CBaseMarginConnection_EnsureStreamPacketEncryptionModule(this) at 0x441470
//    - Initialize packet builder envelope: CLTLoginMediatorPacketBuilderEnvelope_Initialize(&local_24)
//      - local_24.vftable set to cls_0x4b654c__vftable_4b654c_004b654c initially
//    - Set opcode: *(byte *)&local_14 = 3 (later overwritten to 0x11)
//    - Copy 16 bytes from local_38.mbr_0x10 + 0x11, +0x15, +0x19, +0x1d to packet at +1, +5, +9, +0xd
//    - Send via connection vtable+0x24: (**(code **)(*(int *)this + 0x24))(&local_24)
//    - Cleanup: release local_1c, local_38, local_8 via vtable+0x08
//    - Return
//
// SOURCE DIVERGENCE NOTES:
// - Current source takes raw bytes (packetBytes, packetSize) instead of parsed message object.
//   Original extracts encrypted blob from message ref at param_1+0x14/0x18 context.
// - Source now routes through the stored prep object wrapper `CMarginConnectionAuthBootstrapState_0x443220`
//   and uses direct CryptoPP::RSAES_OAEP_SHA_Decryptor decryption under that preserved
//   launcher entrypoint sequence (`0x437810 -> 0x468130`).
// - Source does not create local message ref via CMessageConnectionMessage_CreateRef.
//   Original creates local_8 (cls_0x4489d0), decrypts into its storage, grows payload.
// - Source does not construct cls_0x4b6538 envelope object (vtable 0x4b6538 at 0x443220).
//   Original constructs envelope wrapping message ref with flag '\x01', extracts bytes from mbr_0x10.
// - Source builds response packet inline matching launcher.exe:0x442ab9-0x442b30.
// - Source lacks MessageBox error handling and vtable+0xc close call on decrypt failure.
//   Original shows MessageBox("Failed to decrypt challenge blob from server!") then closes.
// - FIDELITY: Removed infidel mediator continuation fallback on decrypt failure - matches original return 0.

// ============================================================
// Packet_MarginChallenge_0x4b654c implementation
// ============================================================
// anchor: launcher.exe:0x441a30 / CBaseMarginConnection_OnMessageCode2
// Original manually sets Packet_0x4af2a4 vtable, stores messageRef with AddRef,
// computes payloadPtr04, then switches to 0x4b654c vtable and calls meth_0x4416d0.
// We model this as a constructor on the Packet_MarginChallenge_0x4b654c class.
Packet_MarginChallenge_0x4b654c::Packet_MarginChallenge_0x4b654c(
    CMessageConnectionMessageRef_0x4ba23c* messageRef,
    bool isHeaderless) {
    // Release base-allocated message ref (from Packet_0x4af2a4 default ctor) and replace
    // with the provided external ref, matching original manual field init.
    if (messageRef08) {
        messageRef08->Release();
    }
    messageRef08 = messageRef;
    if (messageRef08) {
        messageRef08->AddRef();
    }

    // Compute payloadPtr04 (+0x04) based on messageRef headerless flag
    // anchor: launcher.exe:0x441a30
    // FIDELITY: Original checks messageRef->headerless10, NOT isHeaderless param
    if (messageRef08 && messageRef08->messageStorage0c) {
        if (messageRef08->headerless10 == 0) {
            // Non-headerless: payloadPtr04 = payloadBytes0c
            payloadPtr04 = reinterpret_cast<uint32_t>(
                messageRef08->messageStorage0c->payloadBytes0c.data());
        } else {
            // Headerless: payloadPtr04 = payloadBytes0c + lookup[high] + lookup[low] + 0x12
            uint8_t* payloadBase = messageRef08->messageStorage0c->payloadBytes0c.data();
            uint8_t descriptor = payloadBase[1];
            uint32_t offset = g_MessageOffsetLookupTable[(descriptor >> 4) & 7] +
                              g_MessageOffsetLookupTable[descriptor & 7];
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase) + offset + 0x12;
            messageRef08->headerless10 = 1;
        }
    }

    // Set createRefParam0c (+0x0c) to isHeaderless flag
    createRefParam0c = isHeaderless ? 1u : 0u;

    // Extract encrypted blob info (mirrors original meth_0x4416d0)
    ExtractEncryptedBlobFromPayload(isHeaderless);

    // anchor: launcher.exe:0x441a30 tail
    // Original tail writes differ by headerless flag:
    if (!isHeaderless) {
        // Non-headerless: write opcode 0x02 and zero word to payloadAlias10
        if (payloadAlias10) {
            uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
            payload[0] = 0x02u;
            *reinterpret_cast<uint16_t*>(payload + 1) = 0u;
        }
        // Zero debugString14 / payloadSize18 (original field_0x14 / field_0x18)
        debugString14 = nullptr;
        payloadSize18 = 0u;
    } else {
        // Headerless: set incoming messageRef headerless flag
        // anchor: launcher.exe:0x441a30: *(undefined1 *)(messageRef + 4) = 1;
        // messageRef is int* in decompile, +4 = offset 0x10 = headerless10
        if (messageRef08) {
            messageRef08->headerless10 = 1u;
        }
    }
}

// anchor: launcher.exe:0x4416d0 / MarginConnectionChallengeParsedResult_0x4b654c::meth_0x4416d0
void Packet_MarginChallenge_0x4b654c::ExtractEncryptedBlobFromPayload(bool isHeaderless) {
    uint8_t* payloadPtr = reinterpret_cast<uint8_t*>(payloadPtr04);
    payloadAlias10 = payloadPtr;
    if (!isHeaderless) {
        // Non-headerless: grow messageRef payload by 3 bytes
        if (messageRef08) {
            messageRef08->GrowPayloadByteCount(3);
        }
        return;
    }
    // Headerless: read word at payloadPtr04 + 1 as offset to length-prefixed blob
    if (payloadPtr) {
        uint16_t offsetWord = *reinterpret_cast<uint16_t*>(payloadPtr + 1);
        if (offsetWord != 0) {
            payloadSize18 = *reinterpret_cast<uint16_t*>(payloadPtr + offsetWord);
            debugString14 = reinterpret_cast<const char*>(payloadPtr + offsetWord + 2);
            return;
        }
    }
    payloadSize18 = 0;
    debugString14 = nullptr;
}

// anchor: launcher.exe:0x442790 / vtable slot 2
void Packet_MarginChallenge_0x4b654c::DebugString(int formatType) {
    const uint8_t* blobBytes = reinterpret_cast<const uint8_t*>(debugString14);
    uint16_t blobSize = payloadSize18;
    if (formatType == 2) {
        spdlog::debug("Packet_MarginChallenge_0x4b654c: EncryptedBlob:(Array of size 0x{:04x})", blobSize);
    } else if (formatType == 3 && blobBytes && blobSize > 0) {
        std::string byteStr;
        for (uint16_t i = 0; i < blobSize && i < 64; ++i) {
            if (i > 0) byteStr += ",";
            byteStr += fmt::format("0x{:02x}", blobBytes[i]);
        }
        if (blobSize > 64) byteStr += ",...";
        spdlog::debug("Packet_MarginChallenge_0x4b654c: EncryptedBlob:[{}]", byteStr);
    }
}

// anchor: launcher.exe:0x441ad0 / vtable slot 3
void Packet_MarginChallenge_0x4b654c::InitializePayloadSize() {
    if (!messageRef08 || !messageRef08->messageStorage0c) {
        return;
    }
    auto* msgStorage = messageRef08->messageStorage0c;
    uint8_t* storageBase = reinterpret_cast<uint8_t*>(msgStorage);
    uint8_t descriptor = storageBase[0xd];
    uint32_t offset1 = g_MessageOffsetLookupTable[(descriptor >> 4) & 7];
    uint32_t offset2 = g_MessageOffsetLookupTable[descriptor & 7];
    uint32_t payloadSize = offset1 + offset2 + 0x12;

    // Set payloadPtr04 to END of payload
    payloadPtr04 = reinterpret_cast<uint32_t>(storageBase + 0xc) + payloadSize;

    // Zero messageRef payload length
    messageRef08->SetPayloadByteCount(0);

    // Grow messageStorage by payloadSize
    msgStorage->GrowPayloadByteCount(static_cast<uint16_t>(payloadSize));

    // Set payloadAlias10 to END pointer
    payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

    // Grow messageRef by 3
    messageRef08->GrowPayloadByteCount(3);

    // Write opcode 0x02 and zero word
    uint8_t* packetPayload = static_cast<uint8_t*>(payloadAlias10);
    if (packetPayload) {
        packetPayload[0] = 0x02u;
        *reinterpret_cast<uint16_t*>(packetPayload + 1) = 0u;
    }

    // Clear blob fields
    debugString14 = nullptr;
    payloadSize18 = 0u;
}

// anchor: launcher.exe:0x4429b0 -> 0x442b6f
// Original signature: uint __thiscall CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse
//                     (CBaseMarginConnection_0x4b64a8 *this, Packet_MarginChallenge_0x4b654c *parsedMessageResult)
uint32_t CBaseMarginConnection_0x4b64a8::HandleCode2ForBootstrap(
    Packet_MarginChallenge_0x4b654c* parsedMessageResult) {
    if (!parsedMessageResult) {
        return 0u;
    }

    // FIDELITY: Comprehensive logging for HandleCode2ForBootstrap (launcher.exe:0x4429b0)
    spdlog::info("HandleCode2ForBootstrap: BEGIN this={} parsedMessageResult={}",
        fmt::ptr(this), fmt::ptr(parsedMessageResult));

    // Log encrypted payload info from parsed message
    // FIDELITY: direct field access to debugString14 (+0x14) and payloadSize18 (+0x18)
    const uint8_t* encPayload = reinterpret_cast<const uint8_t*>(parsedMessageResult->debugString14);
    const uint16_t encSize = parsedMessageResult->payloadSize18;
    if (encPayload && encSize >= 16) {
        spdlog::info("HandleCode2ForBootstrap: encrypted payload size={} bytes [0-15]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            encSize, encPayload[0], encPayload[1], encPayload[2], encPayload[3],
            encPayload[4], encPayload[5], encPayload[6], encPayload[7],
            encPayload[8], encPayload[9], encPayload[10], encPayload[11],
            encPayload[12], encPayload[13], encPayload[14], encPayload[15]);
    }

    // Log encrypted blob fields (stored in inherited debugString14 / payloadSize18)
    spdlog::debug("HandleCode2ForBootstrap: encryptedBlobPtr={} encryptedBlobSize={}",
        fmt::ptr(parsedMessageResult->debugString14),
        parsedMessageResult->payloadSize18);

    // anchor: launcher.exe:0x4429b9-0x442a17: Static crypto context initialization (inline)
    // Original: if ((_g_CryptoInitializedFlag_0x4f7c20 & 1) == 0) { ...init... }
    // Disassembly:
    //   0x4429b9: MOV CL,byte ptr [0x004f7c20]  ; load flag
    //   0x4429bf: MOV EAX,0x1
    //   0x4429c4: TEST AL,CL
    //   0x4429c6: JNZ 0x442a17                  ; skip if initialized
    //   0x4429c9: OR dword ptr [0x4f7c20],EAX   ; set flag bit 0
    //   0x4429cf: PUSH 0x180
    //   0x4429d4: MOV ECX,0x4f7bf4
    //   0x4429d9: CALL 0x4686e0                 ; CryptoInitHelper_0x4b42bc ctor
    //   0x4429de: PUSH 0x20
    //   0x4429e0: PUSH 0x0
    //   0x4429e2: MOV ECX,0x4f7bf4
    //   0x4429e7: MOV [0x4f7bf4],0x4b695c       ; vtable 0x4b695c at +0x00
    //   0x4429f1: MOV [0x4f7bf8],0x4b68a8       ; vtable 0x4b68a8 at +0x04
    //   0x4429fb: MOV [0x4f7bfc],0x4b41e0       ; vtable 0x4b41e0 at +0x08
    //   0x442a05: CALL 0x468dc0                 ; meth_0x468dc0(0, &DAT_00000020)
    //   0x442a0a: PUSH 0x4a6e80
    //   0x442a0f: CALL 0x48bc66                 ; _atexit(FUN_004a6e80)
    //
    // FIDELITY: Call EnsureCryptoContextInitialized() which replicates the original
    // sequence: constructor call, manual vtable pointer setup, InitializeCryptoState call
    EnsureCryptoContextInitialized();

    // anchor: launcher.exe:0x4429b0 -> this+0xa0 bootstrap prep state
    // Original: Loads [this+0xa0], calls prep-object vtable +0x1c (0x437810)
    if (!bootstrapPrepStateA0_) {
        spdlog::warn(
            "CBaseMarginConnection_0x4b64a8::HandleCode2ForBootstrap missing bootstrapPrepStateA0_ (this+0xa0) this={} ownerContext={}",
            fmt::ptr(this),
            fmt::ptr(OwnerContext()));
        // FIDELITY: Original shows MessageBox and calls vtable+0xc to close connection
        return 0u;
    }

    // anchor: launcher.exe:0x442a1e -> CMessageConnectionMessage_CreateRef(&local_8, 0)
    // Original: Creates local message ref (cls_0x4489d0) for decryption target
    CMessageConnectionMessageRef_0x4ba23c localMessageRef;
    localMessageRef.ResetForPacketBuilder(/*headerless=*/false);

    if (!localMessageRef.messageStorage0c) {
        spdlog::warn(
            "CBaseMarginConnection_0x4b64a8::HandleCode2ForBootstrap: failed to create local message ref for decryption target");
        return 0u;
    }

    // anchor: launcher.exe:0x442a33 -> prep-object vtable+0x1c call
    // Original parameters:
    //   - param_1: local_10 (output buffer pointer, usually local stack buffer)
    //   - param_2: &DAT_004f7bf4 (crypto context with vtables 0x4b695c/0x4b68a8/0x4b41e0)
    //   - param_3: dStack_c = parsedMessageResult->mbr_0x14 (message context dword)
    //   - param_4: parsedMessageResult->mbr_0x18 (message context word)
    //   - param_5: encrypted challenge blob pointer from local message storage calculation
    //
    // FIDELITY NOTE: Original uses message pool where local message ref already contains
    // the received packet data. Direct field access to debugString14 / payloadSize18.

    // anchor: launcher.exe:0x442a26-0x442a56: Build encrypted payload pointer
    // Original loads from messageStorage0c (local message ref at [EBP-0x4] + 0xc):
    //   0x442a26: MOV EAX, [ECX+0xc]  ; get messageStorage0c
    //   0x442a36: MOVZX EDI, [EAX+0xb] ; payloadLengthLow0b
    //   0x442a42: MOV BL, [EAX+0xa]   ; payloadLengthHigh0a
    //   0x442a4e: AND EBX, 0x7f
    //   0x442a51: SHL EBX, 0x8
    //   0x442a54: OR EBX, EDI         ; EBX = ((high & 0x7f) << 8) | low
    //   0x442a56: LEA EAX, [EBX+EAX+0xc]  ; &payloadBytes0c + length + 0xc
    //
    // For fresh message (payloadLength=0), this gives &payloadBytes0c[0xc].
    // The original does NOT copy encrypted data into localMessageRef; it passes
    // the blob pointer directly from parsedMessageResult->mbr_0x14 / mbr_0x18.

    // Get encrypted blob pointer and size directly from parsed result.
    // These map to original mbr_0x14 (blob pointer) and mbr_0x18 (blob size).
    // FIDELITY: no accessor methods exist in static-RE; direct field access only.
    const uint8_t* encryptedBlobPtr = reinterpret_cast<const uint8_t*>(parsedMessageResult->debugString14);
    const uint16_t encryptedBlobSize = parsedMessageResult->payloadSize18;

    if (!encryptedBlobPtr || encryptedBlobSize == 0) {
        spdlog::warn(
            "CBaseMarginConnection_0x4b64a8::HandleCode2ForBootstrap: no encrypted payload in parsed message");
        return 0u;
    }

    // anchor: launcher.exe:0x442a33 -> DecryptChallenge with &DAT_004f7bf4 crypto context
    // Original passes 5 explicit params (plus implicit this):
    //   param 1 (EBP+0x08): &decryptOutputBuffer
    //   param 2 (EBP+0x0c): &g_CryptoInitHelper_0x4f7bf4
    //   param 3 (EBP+0x10): parsedMessageResult->mbr_0x14 (encrypted blob ptr)
    //   param 4 (EBP+0x14): parsedMessageResult->mbr_0x18 (encrypted blob size word)
    //   param 5 (EBP+0x18): localBufferPtr
    std::array<uint8_t, 128> decryptOutput{};  // [success, byteCount, full decrypted buffer]

    void* cryptoContext = &g_CryptoInitHelper_0x4f7bf4;

    // Compute local buffer pointer matching original formula
    uint8_t* localBufferPtr = nullptr;
    if (localMessageRef.messageStorage0c) {
        const uint8_t payloadLengthHigh = localMessageRef.messageStorage0c->payloadLengthHigh0a;
        const uint8_t payloadLengthLow = localMessageRef.messageStorage0c->payloadLengthLow0b;
        const uint16_t payloadOffset = ((payloadLengthHigh & 0x7f) << 8) | payloadLengthLow;
        localBufferPtr = localMessageRef.messageStorage0c->PayloadBase() + payloadOffset;
    }

    // anchor: launcher.exe:0x442a5a-0x442a6d: 5 stack params pushed (plus ECX=this)
    //   param 1 (EBP+0x08): &decryptOutputBuffer
    //   param 2 (EBP+0x0c): &g_CryptoInitHelper_0x4f7bf4
    //   param 3 (EBP+0x10): parsedMessageResult->mbr_0x14 (encrypted blob ptr)
    //   param 4 (EBP+0x14): parsedMessageResult->mbr_0x18 (encrypted blob size word)
    //   param 5 (EBP+0x18): localBufferPtr
    bootstrapPrepStateA0_->DecryptChallenge(
        decryptOutput.data(),
        cryptoContext,
        reinterpret_cast<uint32_t>(encryptedBlobPtr), // param 3: encrypted blob ptr (mbr_0x14)
        encryptedBlobSize,                             // param 4: encrypted blob size (mbr_0x18)
        localBufferPtr);                               // param 5: local buffer ptr

    // anchor: launcher.exe:0x442a3d-0x442a44: Check decrypt result at *(int*)(iVar3 + 4)
    // Original: iVar3 = DecryptChallenge(...); if (*(int *)(iVar3 + 4) == 0) -> error path
    // The decrypt returns a pointer to result structure where offset +4 is the byte count
    // Our implementation writes directly to decryptOutput: [0]=success, [4]=byteCount
    const uint32_t decryptedByteCount = *reinterpret_cast<const uint32_t*>(decryptOutput.data() + 4);
    const bool decryptSuccess = decryptedByteCount != 0;

    // FIDELITY: Log the decrypt result structure
    spdlog::info("HandleCode2ForBootstrap: decrypt result success={} byteCount={}",
        decryptSuccess ? 1 : 0, decryptedByteCount);
    if (decryptSuccess && decryptedByteCount > 0) {
        spdlog::debug("HandleCode2ForBootstrap: decrypt output [0-31]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            decryptOutput[0], decryptOutput[1], decryptOutput[2], decryptOutput[3],
            decryptOutput[4], decryptOutput[5], decryptOutput[6], decryptOutput[7],
            decryptOutput[8], decryptOutput[9], decryptOutput[10], decryptOutput[11],
            decryptOutput[12], decryptOutput[13], decryptOutput[14], decryptOutput[15],
            decryptOutput[16], decryptOutput[17], decryptOutput[18], decryptOutput[19],
            decryptOutput[20], decryptOutput[21], decryptOutput[22], decryptOutput[23],
            decryptOutput[24], decryptOutput[25], decryptOutput[26], decryptOutput[27],
            decryptOutput[28], decryptOutput[29], decryptOutput[30], decryptOutput[31]);
    }

    if (!decryptSuccess) {
        // anchor: launcher.exe:0x442a46-0x442a57: Error path - MessageBox + vtable+0xc close
        // Original: MessageBoxA(0, "Failed to decrypt challenge blob from server!", "Error", 0)
        // Then: (**(code **)(*(int *)this + 0xc))(1) -> close connection
        MessageBoxA(nullptr, "Failed to decrypt challenge blob from server!", "Error", 0);
        // FIDELITY: Call vtable+0xc to close connection with param 1
        // Original: (**(code **)(*(int *)this + 0xc))(1)
        Close(true);
        return 0u;
    }

    // anchor: launcher.exe:0x442a48: Success path - GrowPayloadByteCount via vtable+0x24
    // Original: (*(local_8.messageRef00)->vftptr_0x0->GrowPayloadByteCount_24)(local_8.messageRef00, *(int *)(iVar3 + 4));
    // FIDELITY: Original calls GrowPayloadByteCount (not ResetPayloadByteCount)
    if (decryptedByteCount > 0 && decryptedByteCount <= CMessageConnectionMessageStorage_0x4ba208::kMaxPayloadByteCount) {
        // FIDELITY: GrowPayloadByteCount adds to existing size. Message ref was created with size 0,
        // so growing by decryptedByteCount sets the size to decryptedByteCount.
        // We need the full 96-byte decrypted buffer for seed extraction.
        localMessageRef.messageStorage0c->GrowPayloadByteCount(static_cast<uint16_t>(decryptedByteCount));
        uint8_t* payloadBase = localMessageRef.messageStorage0c->PayloadBase();
        if (payloadBase) {
            std::copy(
                decryptOutput.begin() + 8,
                decryptOutput.begin() + 8 + decryptedByteCount,
                payloadBase);
        }
    }

    // anchor: launcher.exe:0x442a5e -> Packet_CertChallenge_0x4b6538::cls_0x4b6538
    // Original: (&local_38, (int *)local_8.messageRef00, '\x01')
    // The message ref already has decrypted payload from above, now construct envelope with it
    Packet_CertChallenge_0x4b6538 envelope(&localMessageRef, 0x01);

    spdlog::debug("HandleCode2ForBootstrap: localMessageRef={:08x}, storage={:08x}, payloadBase={:08x}",
            reinterpret_cast<uintptr_t>(&localMessageRef),
            reinterpret_cast<uintptr_t>(localMessageRef.messageStorage0c),
            reinterpret_cast<uintptr_t>(localMessageRef.messageStorage0c->PayloadBase()));

    spdlog::debug("HandleCode2ForBootstrap: after envelope construct, payload[0-16]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
        localMessageRef.messageStorage0c->PayloadBase()[0],
        localMessageRef.messageStorage0c->PayloadBase()[1],
        localMessageRef.messageStorage0c->PayloadBase()[2],
        localMessageRef.messageStorage0c->PayloadBase()[3],
        localMessageRef.messageStorage0c->PayloadBase()[4],
        localMessageRef.messageStorage0c->PayloadBase()[5],
        localMessageRef.messageStorage0c->PayloadBase()[6],
        localMessageRef.messageStorage0c->PayloadBase()[7],
        localMessageRef.messageStorage0c->PayloadBase()[8],
        localMessageRef.messageStorage0c->PayloadBase()[9],
        localMessageRef.messageStorage0c->PayloadBase()[10],
        localMessageRef.messageStorage0c->PayloadBase()[11],
        localMessageRef.messageStorage0c->PayloadBase()[12],
        localMessageRef.messageStorage0c->PayloadBase()[13],
        localMessageRef.messageStorage0c->PayloadBase()[14],
        localMessageRef.messageStorage0c->PayloadBase()[15]);

    // DEBUG: Log what's in the envelope mbr_0x10_ptr after construction
    spdlog::debug("HandleCode2ForBootstrap: decryptedByteCount={}, payloadBase={:08x}, payload[0-4]={:02x} {:02x} {:02x} {:02x}",
        decryptedByteCount,
        localMessageRef.messageStorage0c ? reinterpret_cast<uintptr_t>(localMessageRef.messageStorage0c->PayloadBase()) : 0,
        localMessageRef.messageStorage0c ? localMessageRef.messageStorage0c->PayloadBase()[0] : 0,
        localMessageRef.messageStorage0c ? localMessageRef.messageStorage0c->PayloadBase()[1] : 0,
        localMessageRef.messageStorage0c ? localMessageRef.messageStorage0c->PayloadBase()[2] : 0,
        localMessageRef.messageStorage0c ? localMessageRef.messageStorage0c->PayloadBase()[3] : 0);

    spdlog::debug("HandleCode2ForBootstrap: envelope.packetPayloadPtr={:08x}, *mbr_0x10_ptr[0-4]={:02x} {:02x} {:02x} {:02x}",
        reinterpret_cast<uintptr_t>(envelope.packetPayloadPtr),
        envelope.packetPayloadPtr ? envelope.packetPayloadPtr[0] : 0,
        envelope.packetPayloadPtr ? envelope.packetPayloadPtr[1] : 0,
        envelope.packetPayloadPtr ? envelope.packetPayloadPtr[2] : 0,
        envelope.packetPayloadPtr ? envelope.packetPayloadPtr[3] : 0);

    // anchor: launcher.exe:0x442ac6-0x442ae0 -> Extract seed bytes inline to this+0x85/0x89/0x8d/0x91
    // Original disassembly:
    //   0x442ac6: MOV EDX,dword ptr [EDI + 0x1]  ; EDI = envelope.packetPayloadPtr
    //   0x442acf: MOV dword ptr [ECX],EDX        ; ECX = this+0x85
    //   0x442ad1: MOV EAX,dword ptr [EDI + 0x5]
    //   0x442ad4: MOV dword ptr [ECX + 0x4],EAX
    //   0x442ad7: MOV EDX,dword ptr [EDI + 0x9]
    //   0x442ada: MOV dword ptr [ECX + 0x8],EDX
    //   0x442add: MOV EAX,dword ptr [EDI + 0xd]
    //   0x442ae0: MOV dword ptr [ECX + 0xc],EAX
    // FIDELITY: No ExtractChallengeBytes() helper - all inline dword copies
    // The original extracts from envelope.packetPayloadPtr at offsets 1-16
    if (envelope.packetPayloadPtr) {
        // FIDELITY VERIFICATION: Log source bytes before extraction
        spdlog::debug("HandleCode2ForBootstrap: seed extraction source envelope.packetPayloadPtr[0-20]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            envelope.packetPayloadPtr[0], envelope.packetPayloadPtr[1], envelope.packetPayloadPtr[2], envelope.packetPayloadPtr[3],
            envelope.packetPayloadPtr[4], envelope.packetPayloadPtr[5], envelope.packetPayloadPtr[6], envelope.packetPayloadPtr[7],
            envelope.packetPayloadPtr[8], envelope.packetPayloadPtr[9], envelope.packetPayloadPtr[10], envelope.packetPayloadPtr[11],
            envelope.packetPayloadPtr[12], envelope.packetPayloadPtr[13], envelope.packetPayloadPtr[14], envelope.packetPayloadPtr[15],
            envelope.packetPayloadPtr[16], envelope.packetPayloadPtr[17], envelope.packetPayloadPtr[18], envelope.packetPayloadPtr[19]);

        messageCode5SeedBytes85_[0] = envelope.packetPayloadPtr[1];
        messageCode5SeedBytes85_[1] = envelope.packetPayloadPtr[2];
        messageCode5SeedBytes85_[2] = envelope.packetPayloadPtr[3];
        messageCode5SeedBytes85_[3] = envelope.packetPayloadPtr[4];
        messageCode5SeedBytes85_[4] = envelope.packetPayloadPtr[5];
        messageCode5SeedBytes85_[5] = envelope.packetPayloadPtr[6];
        messageCode5SeedBytes85_[6] = envelope.packetPayloadPtr[7];
        messageCode5SeedBytes85_[7] = envelope.packetPayloadPtr[8];
        messageCode5SeedBytes85_[8] = envelope.packetPayloadPtr[9];
        messageCode5SeedBytes85_[9] = envelope.packetPayloadPtr[10];
        messageCode5SeedBytes85_[10] = envelope.packetPayloadPtr[11];
        messageCode5SeedBytes85_[11] = envelope.packetPayloadPtr[12];
        messageCode5SeedBytes85_[12] = envelope.packetPayloadPtr[13];
        messageCode5SeedBytes85_[13] = envelope.packetPayloadPtr[14];
        messageCode5SeedBytes85_[14] = envelope.packetPayloadPtr[15];
        messageCode5SeedBytes85_[15] = envelope.packetPayloadPtr[16];

        // FIDELITY: Log extracted seed bytes (will be used for this+0x85, 0x89, 0x8d, 0x91)
        spdlog::info("HandleCode2ForBootstrap: extracted seed bytes for this+0x85 [0-15]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            messageCode5SeedBytes85_[0], messageCode5SeedBytes85_[1], messageCode5SeedBytes85_[2], messageCode5SeedBytes85_[3],
            messageCode5SeedBytes85_[4], messageCode5SeedBytes85_[5], messageCode5SeedBytes85_[6], messageCode5SeedBytes85_[7],
            messageCode5SeedBytes85_[8], messageCode5SeedBytes85_[9], messageCode5SeedBytes85_[10], messageCode5SeedBytes85_[11],
            messageCode5SeedBytes85_[12], messageCode5SeedBytes85_[13], messageCode5SeedBytes85_[14], messageCode5SeedBytes85_[15]);

        // Log as DWORDs to match how they get written to connection fields
        const uint32_t seedDword0 = *reinterpret_cast<const uint32_t*>(&messageCode5SeedBytes85_[0]);
        const uint32_t seedDword1 = *reinterpret_cast<const uint32_t*>(&messageCode5SeedBytes85_[4]);
        const uint32_t seedDword2 = *reinterpret_cast<const uint32_t*>(&messageCode5SeedBytes85_[8]);
        const uint32_t seedDword3 = *reinterpret_cast<const uint32_t*>(&messageCode5SeedBytes85_[12]);
        spdlog::info("HandleCode2ForBootstrap: seed DWORDs this+0x85=0x{:08x} this+0x89=0x{:08x} this+0x8d=0x{:08x} this+0x91=0x{:08x}",
            seedDword0, seedDword1, seedDword2, seedDword3);
    }
    // anchor: launcher.exe:0x442aae -> EnsureStreamPacketEncryptionModule
    // Original: CBaseMarginConnection_EnsureStreamPacketEncryptionModule(this)
    EnsureStreamPacketEncryptionModuleFromSeed85();

    // anchor: launcher.exe:0x442ab9-0x442b03 -> Initialize packet builder and copy response bytes
    // Original flow:
    // 1. CLTLoginMediatorPacketBuilderEnvelope_Initialize(&local_24)
    // 2. Manually set vtable to 0x4b6560 at 0x442afd
    // 3. Set opcode 0x11
    // 4. Copy from envelope.mbr_0x10 +0x11/+0x15/+0x19/+0x1d to packet+1/+5/+9/+0xd
    // 5. Send via connection vtable+0x24

    // FIDELITY: Build response packet inline to match launcher.exe:0x442ab9-0x442b03
    // The original at 0x442ab9 creates a response envelope on the stack and populates it directly
    // FIDELITY: Use Packet_CertChallengeResponse_0x4b6560 (vtable 0x4b6560) for response envelope
    // The original manually switches vtable from base (0x4af2a4) to 0x4b6560 at 0x442afd

    Packet_CertChallengeResponse_0x4b6560 responseEnvelope;
    // anchor: launcher.exe:0x442afd: Original manually sets field_0x4 = 0x4b6560 (vtable)
    // C++ handles this through the class vtable automatically
    // anchor: launcher.exe:0x442af4: responseEnvelope.mbr_0x10 = responseEnvelope.mbr_0x4
    // Our C++ class uses payloadBegin10 from base PacketBuilder

    // anchor: launcher.exe:0x442b00: Get message ref from response envelope (mbr_0x8)
    // and packet payload base (mbr_0x4/mbr_0x10) for building response
    CMessageConnectionMessageRef_0x4ba23c* responseMessageRef = responseEnvelope.messageRef08;
    if (!responseMessageRef) {
        // Should not happen - envelope has default-constructed message ref
        spdlog::warn("HandleCode2ForBootstrap: responseEnvelope has no messageRef");
        return 0u;
    }

    // anchor: launcher.exe:0x442b0f-0x442b2f -> Build response packet inline
    // Original disassembly:
    //   0x442b0f: MOV EAX,dword ptr [EBP + -0x10]  ; EAX = responseEnvelope.packetPayloadPtr
    //   0x442b12: MOV byte ptr [EAX],0x3           ; frame byte = 3
    //   0x442b15: MOV ECX,dword ptr [EBP + -0x10]  ; ECX = responsePayload base
    //   0x442b18: ADD EDI,0x11                      ; EDI = seedEnvelope.packetPayloadPtr + 0x11
    //   0x442b1b: MOV EDX,dword ptr [EDI]          ; first dword from seed+0x11
    //   0x442b1d: INC ECX                           ; ECX = responsePayload + 1
    //   0x442b1e: MOV dword ptr [ECX],EDX          ; copy to response+1
    //   0x442b20: MOV EAX,dword ptr [EDI + 0x4]    ; second dword from seed+0x15
    //   0x442b23: MOV dword ptr [ECX + 0x4],EAX    ; copy to response+5
    //   0x442b26: MOV EDX,dword ptr [EDI + 0x8]    ; third dword from seed+0x19
    //   0x442b29: MOV dword ptr [ECX + 0x8],EDX    ; copy to response+9
    //   0x442b2c: MOV EAX,dword ptr [EDI + 0xc]    ; fourth dword from seed+0x1d
    //   0x442b2f: MOV dword ptr [ECX + 0xc],EAX    ; copy to response+0xd
    // FIDELITY: No ExtractForResponsePacket() helper - all inline dword copies
    // Packet structure: [frame=0x03][16 challenge response bytes] = 17 bytes (0x11) total
    // Note: The 2-byte Protocol/Opcode field at offset 1 is NOT present in this packet type.
    // The challenge data starts immediately at offset 1, matching the original binary.
    uint8_t* responsePayload = responseMessageRef->PayloadAppendPointer();
    if (envelope.packetPayloadPtr && responsePayload) {
        responsePayload[0] = 0x03;  // frame byte
        // Copy 16 bytes (4 dwords) from envelope.packetPayloadPtr+0x11/0x15/0x19/0x1d
        // to responsePayload+1/5/9/0xd (immediately after frame byte, NO 2-byte opcode field)
        *reinterpret_cast<uint32_t*>(responsePayload + 1) = *reinterpret_cast<const uint32_t*>(envelope.packetPayloadPtr + 0x11);
        *reinterpret_cast<uint32_t*>(responsePayload + 5) = *reinterpret_cast<const uint32_t*>(envelope.packetPayloadPtr + 0x15);
        *reinterpret_cast<uint32_t*>(responsePayload + 9) = *reinterpret_cast<const uint32_t*>(envelope.packetPayloadPtr + 0x19);
        *reinterpret_cast<uint32_t*>(responsePayload + 13) = *reinterpret_cast<const uint32_t*>(envelope.packetPayloadPtr + 0x1d);
        responseMessageRef->GrowPayloadByteCount(17);  // 1 frame + 16 response bytes = 0x11
    } else {
        spdlog::warn("HandleCode2ForBootstrap: missing envelope.packetPayloadPtr or responsePayload for response packet");
        return 0u;
    }

    // FIDELITY: Log response packet details before sending (already have responsePayload from above)
    const size_t responsePayloadSize = responseMessageRef->messageStorage0c->PayloadByteCount();

    spdlog::info("HandleCode2ForBootstrap: RESPONSE PACKET opcode=0x{:02x} size={} bytes",
        responsePayload ? responsePayload[0] : 0, responsePayloadSize);
    if (responsePayload && responsePayloadSize >= 17) {
        spdlog::info("HandleCode2ForBootstrap: response payload [0-16]={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            responsePayload[0], responsePayload[1], responsePayload[2], responsePayload[3],
            responsePayload[4], responsePayload[5], responsePayload[6], responsePayload[7],
            responsePayload[8], responsePayload[9], responsePayload[10], responsePayload[11],
            responsePayload[12], responsePayload[13], responsePayload[14], responsePayload[15]);

        // Verify response bytes match expected challenge
        const bool responseMatchesChallenge =
            (responsePayload[1] == messageCode5SeedBytes85_[0]) &&
            (responsePayload[2] == messageCode5SeedBytes85_[1]) &&
            (responsePayload[3] == messageCode5SeedBytes85_[2]) &&
            (responsePayload[4] == messageCode5SeedBytes85_[3]);
        spdlog::info("HandleCode2ForBootstrap: response byte[1-4] match seed byte[0-3]: {}",
            responseMatchesChallenge ? "YES" : "NO");
    }

    // Send via connection vtable+0x24 - anchor: launcher.exe:0x442b30
    // Original calls: connection->vtable+0x24(envelope)
    // In our code, use SendPacketMessageRef with the response message ref
    spdlog::debug("HandleCode2ForBootstrap: about to send responseMessageRef={:08x}",
        reinterpret_cast<uintptr_t>(responseMessageRef));
    SendPacketMessageRef(*responseMessageRef);
    const uint32_t sendResult = 0;  // Success

    spdlog::info(
        "HandleCode2ForBootstrap: SUCCESS sendResult=0x{:08x} decryptedByteCount={} this={} ownerContext={}",
        sendResult,
        static_cast<unsigned>(decryptedByteCount),
        fmt::ptr(this),
        fmt::ptr(OwnerContext()));

    return sendResult != 0u ? 1u : 0u;
}

// anchor: launcher.exe:0x442d00 -> 0x442d83 -> 0x441850
// Original dispatches directly to meth_0x441850 - sets byte+0x84 if status==0, synthesizes local work.
// This scaffold mirrors that direct connection flow.
// UNANCHORED: keeps bootstrap logic local to connection, mediator only involved via later work-item.
// anchor: launcher.exe:0x441850
uint32_t CBaseMarginConnection_0x4b64a8::HandleCode4ForBootstrap(
    const uint8_t* packetBytes,
    size_t packetSize) {
    if (!packetBytes || packetSize < 5u) {
        return 0u;
    }

    // meth_0x441850: if status == 0, set byte+0x84 to 1
    const uint32_t status = static_cast<uint32_t>(packetBytes[1]) |
                            (static_cast<uint32_t>(packetBytes[2]) << 8u) |
                            (static_cast<uint32_t>(packetBytes[3]) << 16u) |
                            (static_cast<uint32_t>(packetBytes[4]) << 24u);
    SetMessageCode4SuccessFlag84(status == 0u);

    // Synthesize local type-0x0b work item (meth_0x441850)
    uint32_t handled = DispatchMessageCode4LocalCompletionWorkItem(status);

    spdlog::info(
        "CBaseMarginConnection_0x4b64a8::HandleCode4ForBootstrap rawCode=0x{:02x} status=0x{:08x} connectionByte84={} localWorkItemHandled={}",
        static_cast<unsigned>(packetBytes[0]),
        status,
        MessageCode4SuccessFlag84() ? 1u : 0u,
        static_cast<unsigned>(handled));
    return handled;
}

// anchor: launcher.exe:0x442d00
uint32_t CBaseMarginConnection_0x4b64a8::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    // anchor: launcher.exe:0x442d0e
    // Static-RE routes through the shared message-code decode helper before the
    // compact subtract / branch ladder at 0x442d19-0x442d24.
    auto& copiedMessageRef = *static_cast<CMessageConnectionMessageRef_0x4ba23c*>(messageRef);
    uint16_t decodedMessageCode = 0u;
    bool usedHeaderlessLocatorDecode = false;
    const uint32_t handledByBaseRouter =
        CBaseMarginConnection_0x4b64a8_DispatchMessageFilterScaffold(
            copiedMessageRef,
            &decodedMessageCode,
            &usedHeaderlessLocatorDecode,
            nullptr);

    if (handledByBaseRouter == 0u) {
        // anchor: launcher.exe:0x442d26..0x442d2d
        // Original fallthrough is the subtract-ladder residue with AL zeroed; callers
        // only treat this as false-ish and forward the message to the owner helper path.
        return (static_cast<int>(decodedMessageCode) - 5) & 0xFFFFFF00;
    }

    const CMessageConnectionMessageStorage_0x4ba208* messageStorage = copiedMessageRef.messageStorage0c;
    const uint8_t* payloadBytes = messageStorage ? messageStorage->PayloadBase() : nullptr;
    const size_t payloadByteCount = messageStorage ? messageStorage->PayloadByteCount() : 0u;

    if (decodedMessageCode == 2u) {
        // anchor: launcher.exe:0x442d8d -> 0x441a30
        Packet_MarginChallenge_0x4b654c code2ParseResultBuffer(
            &copiedMessageRef,
            /*isHeaderless=*/true);

        // anchor: launcher.exe:0x442d9e -> 0x4429b0
        HandleCode2ForBootstrap(&code2ParseResultBuffer);

        // anchor: launcher.exe:0x442da3..0x442dac
        // Original loads the parse-result callback slot from EBP-0x14 and, when non-null,
        // calls vtable+0x08 before forcing AL=1 for the handled return.
        void* code2CompletionCallbackSlot = &code2ParseResultBuffer;
        CBaseMarginConnection_0x4b64a8_InvokeMessageRefCompletionCallback(
            &copiedMessageRef,
            &code2CompletionCallbackSlot);
        return 1u;
    }

    if (decodedMessageCode == 4u) {
        // anchor: launcher.exe:0x442d72 -> 0x441bc0
        CBaseMarginConnection_0x4b64a8_Code4MessageScaffold code4ParseResultBuffer{};
        const bool hasCode4ParseResult =
            CBaseMarginConnection_0x4b64a8_OnMessageCode4Scaffold(
                copiedMessageRef,
                &code4ParseResultBuffer,
                /*parseIncomingMessage=*/true);
        const uint8_t* const logicalPayloadBytes =
            hasCode4ParseResult ? code4ParseResultBuffer.parsedPayload00.logicalPayloadBytes00 : payloadBytes;
        const size_t logicalPayloadByteCount =
            hasCode4ParseResult ? code4ParseResultBuffer.parsedPayload00.logicalPayloadByteCount04 : payloadByteCount;
        const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
        uint32_t handledCode4 = 0u;
        if (hasCode4ParseResult) {
            // anchor: launcher.exe:0x442d83 -> 0x441850
            handledCode4 = HandleCode4ForBootstrap(
                logicalPayloadBytes,
                logicalPayloadByteCount);
        }
        spdlog::info(
            "CBaseMarginConnection_0x4b64a8::DispatchMessage consumed code4 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode4={} logicalPayloadBytes={} status=0x{:08x} handledCode4={} connectionByte84={} this={} ownerContext={} currentState={}",
            static_cast<unsigned>(rawCode),
            hasCode4ParseResult && code4ParseResultBuffer.parsedPayload00.headerless08 ? 1u : 0u,
            hasCode4ParseResult ? (code4ParseResultBuffer.parsedPayload00.usedHeaderlessLocatorDecode09 ? 1u : 0u)
                                : (usedHeaderlessLocatorDecode ? 1u : 0u),
            hasCode4ParseResult ? 1u : 0u,
            static_cast<unsigned>(logicalPayloadByteCount),
            static_cast<unsigned>(hasCode4ParseResult ? code4ParseResultBuffer.statusOrPayload0c : 0u),
            static_cast<unsigned>(handledCode4),
            MessageCode4SuccessFlag84() ? 1u : 0u,
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mxo::ltlogin::g_CurrentLoginMediator ? mxo::ltlogin::g_CurrentLoginMediator->currentState_ : nullptr));

        if (hasCode4ParseResult) {
            // anchor: launcher.exe:0x442d88..0x442dac
            void* code4CompletionCallbackSlot = &code4ParseResultBuffer;
            CBaseMarginConnection_0x4b64a8_InvokeMessageRefCompletionCallback(
                &copiedMessageRef,
                &code4CompletionCallbackSlot);
        }
        return 1u;
    }

    // anchor: launcher.exe:0x442d30..0x442d6f / decodedMessageCode == 5
    CBaseMarginConnection_0x4b64a8_Code5MessageScaffold code5Message{};
    const bool parsedCode5 =
        CBaseMarginConnection_0x4b64a8_OnMessageCode5Scaffold(
            copiedMessageRef,
            &code5Message,
            /*parseIncomingMessage=*/true);
    const uint8_t* const logicalPayloadBytes =
        parsedCode5 ? code5Message.parsedPayload00.logicalPayloadBytes00 : payloadBytes;
    const size_t logicalPayloadByteCount =
        parsedCode5 ? code5Message.parsedPayload00.logicalPayloadByteCount04 : payloadByteCount;
    const uint8_t rawCode = logicalPayloadBytes ? logicalPayloadBytes[0] : 0u;
    if (parsedCode5) {
        // anchor: launcher.exe:0x442d3e..0x442d5e
        // Original copies four dwords from parse-result+1/+5/+9/+d straight into this+0x85.
        messageCode5SeedBytes85_ = code5Message.seedBytes0c;
        EnsureStreamPacketEncryptionModuleFromSeed85();
        spdlog::info(
            "CBaseMarginConnection_0x4b64a8::DispatchMessage consumed code5 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode5=1 logicalPayloadBytes={} storedConnectionSeed85_94=1 firstDword=0x{:08x} this={} ownerContext={} currentState={}",
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
            fmt::ptr(mxo::ltlogin::g_CurrentLoginMediator ? mxo::ltlogin::g_CurrentLoginMediator->currentState_ : nullptr));

        // anchor: launcher.exe:0x442d56..0x442d65
        void* code5CompletionCallbackSlot = &code5Message;
        CBaseMarginConnection_0x4b64a8_InvokeMessageRefCompletionCallback(
            &copiedMessageRef,
            &code5CompletionCallbackSlot);
    } else {
        spdlog::warn(
            "CBaseMarginConnection_0x4b64a8::DispatchMessage consumed short/malformed code5 rawCode=0x{:02x} headerless={} locatorDecoded={} parsedCode5=0 logicalPayloadBytes={} this={} ownerContext={} currentState={}",
            static_cast<unsigned>(rawCode),
            usedHeaderlessLocatorDecode ? 1u : 0u,
            usedHeaderlessLocatorDecode ? 1u : 0u,
            static_cast<unsigned>(logicalPayloadByteCount),
            fmt::ptr(this),
            fmt::ptr(OwnerContext()),
            fmt::ptr(mxo::ltlogin::g_CurrentLoginMediator ? mxo::ltlogin::g_CurrentLoginMediator->currentState_ : nullptr));
    }

    return 1u;
}

// anchor: launcher.exe:0x44af60
// Later leaf override on top of the base `CMessageConnection_0x4b7928::OnOperationCompleted` family.
// Current tighter read from the direct `0x44af60` decompile/listing:
// - call base `0x4490c0`
// - if base returns 0, call owner `+0x188 / 0x41afc0`
// - if that also returns 0, fall through to `0x448a60`
// - only after that handled/unhandled decision, read `workItem+0x04`
// - if work type == 1, clear owner byte `+0xf14` and tear down through the connection object
// - there is no separate type-2 connect-status split in this leaf; that work still flows through
//   the same owner `+0x188` fallback path
uint32_t CMarginConnection_0x4aff38::OnOperationCompleted(void* workItem) {
    if (!workItem) {
        return 0u;
    }

    uint32_t handled = 0u;
    if (CMessageConnection_0x4b7928::OnOperationCompleted(workItem) != 0u) {
        handled = 1u;
    } else {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_0x4b7928_LoginMediatorOwner(this);
        if (mediator && CMessageConnection_0x4b7928_IsMediatorMarginConnection(this, mediator) &&
            mediator->HandleMarginConnectionCompletionFallback(this, workItem) != 0u) {
            handled = 1u;
        } else {
            CMessageConnection_0x4b7928_LogUnhandledOperation(workItem);
        }
    }

    if (CMessageConnection_0x4b7928_WorkItemType(workItem) ==
        CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_0x4b7928_LoginMediatorOwner(this);
        if (mediator && CMessageConnection_0x4b7928_IsMediatorMarginConnection(this, mediator)) {
            mediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 0u;
        }

        // anchor: launcher.exe:0x44af60 tail
        // The original tail calls vtable[0](1), i.e. the deleting-dtor-style teardown path,
        // not the ordinary `Close(bool)` wrapper.
        delete this;
    }

    return handled;
}

// anchor: launcher.exe:0x44af20
// Later leaf dispatch override on top of `CBaseMarginConnection_0x4b64a8::DispatchMessage`.
// Current tighter source split:
// - base `0x442d00` now owns the consumed decoded-code `2/4/5` router again
// - only the surviving path stages bytes for the later launcher-owned bootstrap / slot-6 path
uint32_t CMarginConnection_0x4aff38::DispatchMessage(void* messageRef) {
    if (!messageRef) {
        return 0u;
    }

    if (CBaseMarginConnection_0x4b64a8::DispatchMessage(messageRef) != 0u) {
        return 1u;
    }

    // Call owner->DispatchCurrentHelperSlot6(messageRef)
    mxo::ltlogin::CLTLoginMediator* mediator = CMessageConnection_0x4b7928_LoginMediatorOwner(this);
    if (mediator && CMessageConnection_0x4b7928_IsMediatorMarginConnection(this, mediator)) {
        return mediator->DispatchCurrentHelperSlot6(
            static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(messageRef));
    }
    return 0u;
}

// anchor: launcher.exe:0x43a230 - LocalPacketBuilder_ReserveLengthPrefixedTail
// Non-virtual helper used by packet builder subclasses to reserve length-prefixed tails.
// Assembly flow:
//   0x43a288: CALL vtable slot 0x10 (GetPacketPayloadBase @ 0x481760) to get payload base
//   0x43a290: SUB EDI, EAX - compute offset = reservationHeader - payloadBase
//   0x43a2a4: MOV word ptr [EAX], BX - write content length at reservation header
//   0x43a2aa: MOV word ptr [ECX+0x1], DI - write offset to bytes [1-2] of packet
//   0x43a2b5: LEA EAX,[EDX+EAX*0x1+0x2] - compute end pointer for heapString14
uint16_t Packet_0x4af2a4::ReserveLengthPrefixedTail(uint16_t contentByteCount) {
    // FIDELITY: Variable names synced with Ghidra:0x43a230
    // No null checks in original - straight dereference like launcher.exe

    // Original at 0x43a23e-0x43a241: load messageStorage from messageRef08->messageStorage0c
    auto* messageStorage = messageRef08->messageStorage0c;

    // Clamp to available space in message storage
    // Original at 0x43a257-0x43a267: contentByteCount = min(contentByteCount, 0xffc - payloadSize)
    constexpr uint16_t kMaxPayloadSize = 0xffcu;
    const uint16_t availableSpace = kMaxPayloadSize - messageStorage->PayloadByteCount();
    if (contentByteCount > availableSpace) {
        contentByteCount = availableSpace;
    }

    // Get current payload position for reservation header
    // Original at 0x43a26c-0x43a285: calculates reservationHeader from messageStorage fields
    auto* payloadBase = reinterpret_cast<uint8_t*>(this->payloadAlias10);
    auto* reservationHeader = payloadBase + messageStorage->PayloadByteCount();

    // Grow payload by (content + 2 bytes for length prefix)
    // Original at 0x43a288: CALL vtable slot 0x10 (GetPacketPayloadBase @ 0x481760)
    // Original at 0x43a298-0x43a29e: push (contentByteCount + 2), push 0, call Grow
    messageStorage->GrowPayloadByteCount(contentByteCount + 2u);

    // Write content length at reservation header (little-endian)
    // Original at 0x43a2a4: MOV word ptr [EAX], BX
    reservationHeader[0] = static_cast<uint8_t>(contentByteCount & 0xffu);
    reservationHeader[1] = static_cast<uint8_t>((contentByteCount >> 8) & 0xffu);

    // FIDELITY: Update bytes [1-2] of packet with offset to reservation header
    // Original at 0x43a290: SUB EDI, EAX (reservationHeader - payloadBase)
    // Original at 0x43a2aa: MOV word ptr [ECX+0x1], DI
    auto* packetPayloadPtr = reinterpret_cast<uint8_t*>(payloadPtr04);
    *reinterpret_cast<uint16_t*>(packetPayloadPtr + 1) = static_cast<uint16_t>(reservationHeader - packetPayloadPtr);

    // Update heapString14 (debugString14) to point to START of content (after length prefix)
    // Original at 0x43a2b5: LEA EAX,[EDX+EAX*0x1+0x2] where EDX=offset, EAX=payloadBase
    // Result: heapString14 = payloadBase + offset + 2 = reservationHeader + 2
    // This is the START of the content area (after the 2-byte length prefix)
    debugString14 = reinterpret_cast<const char*>(reservationHeader + 2);

    // Update payloadLength14 (payloadSize18) to content byte count
    // Original at 0x43a2c0: MOV word ptr [ESI+0x18], BX
    payloadSize18 = contentByteCount;

    // FIDELITY: Single return at end of function
    return contentByteCount;
}

}  // namespace mxo::liblttcp
