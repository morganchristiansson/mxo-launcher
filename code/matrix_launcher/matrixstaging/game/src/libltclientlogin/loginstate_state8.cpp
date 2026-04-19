#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mxo::ltlogin {
namespace {

using PostAuthMarginLoadingState = CLTLoginMediator::PostAuthMarginLoadingState;

// Shared chunk-seen helper used by state8 slot6 section-0x0a handling.
// anchor: launcher.exe:0x43c240 = StateReplyChunkBitset_SetSeen
// anchor: launcher.exe:0x43c280 = StateReplyChunkBitset_HasSeen
class StateReplyChunkBitset {
public:
    explicit StateReplyChunkBitset(uint32_t& bits)
        : bits_(bits) {}

    void SetSeen(uint32_t chunkIndex) {
        if (chunkIndex >= 32u) {
            std::abort();
        }
        bits_ |= (1u << chunkIndex);
    }

    bool HasSeen(uint32_t chunkIndex) const {
        if (chunkIndex >= 32u) {
            std::abort();
        }
        return (bits_ & (1u << chunkIndex)) != 0u;
    }

private:
    uint32_t& bits_;
};

static std::string FormatU32x4Block(const std::array<uint32_t, 4>& block) {
    return fmt::format(
        "[{:#010x} {:#010x} {:#010x} {:#010x}]",
        block[0],
        block[1],
        block[2],
        block[3]);
}

static bool U32x4BlockHasAnyNonZero(const std::array<uint32_t, 4>& block) {
    return block[0] != 0u || block[1] != 0u || block[2] != 0u || block[3] != 0u;
}

static void ResetOwnedSectionBytes(void*& buffer, uint16_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void ResetOwnedSectionBytes(void*& buffer, uint32_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void CopyBoundedRawBytes(uint8_t* dest, size_t destSize, const uint8_t* src, size_t srcSize) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::memset(dest, 0, destSize);
    if (!src || srcSize == 0u) {
        return;
    }

    std::memcpy(dest, src, std::min(destSize, srcSize));
}

static void CopyCStringIntoFixed(char* dest, size_t destSize, const uint8_t* src, size_t srcAvailable) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::fill(dest, dest + destSize, '\0');
    if (!src || srcAvailable == 0u) {
        return;
    }

    size_t copyLen = 0u;
    while (copyLen + 1u < destSize && copyLen < srcAvailable && src[copyLen] != '\0') {
        dest[copyLen] = static_cast<char>(src[copyLen]);
        ++copyLen;
    }
    dest[copyLen] = '\0';
}

static void LogState8PersistenceFamilySnapshot(
    const PostAuthMarginLoadingState& ownerState,
    const char* reason,
    uint32_t sectionSelectorMinus2,
    uint16_t sectionByteCount,
    bool completed);

static void LogState8PersistenceFamilySnapshot(
    const PostAuthMarginLoadingState& ownerState,
    const char* reason,
    uint32_t sectionSelectorMinus2,
    uint16_t sectionByteCount,
    bool completed) {
    const uint32_t bodyWord00 = ReadU32LE(ownerState.state8Section0RawF88.data());
    const uint32_t headerWord00 = ownerState.characterFlagsF48[0];
    const uint32_t headerWord04 = ownerState.characterFlagsF48[1];
    const uint32_t headerWord08 = ownerState.characterFlagsF48[2];
    const uint32_t headerWord0c = ownerState.characterFlagsF48[3];
    const uint32_t secondaryWord00 = ownerState.secondaryCharacterDataF68[0];
    const uint32_t secondaryWord04 = ownerState.secondaryCharacterDataF68[1];
    const uint32_t section11Length = static_cast<uint32_t>(ownerState.state8Section11String1460.size());

    spdlog::info(
        "CLTLoginState_State8_0x4b5104 persistence family [{}] completed={} section={} bytes={} f1c='{}' f3c=0x{:08x} f40=0x{:08x} f48[0..3]=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] f68[0..1]=[0x{:08x} 0x{:08x}] f88_00=0x{:08x} f88_444=0x{:08x} f88_448=0x{:08x} overflow13f4=0x{:04x} gate1452={} sec11_145c=0x{:08x} sec11_len={}",
        reason ? reason : "<unknown>",
        completed ? 1u : 0u,
        static_cast<unsigned>(sectionSelectorMinus2),
        static_cast<unsigned>(sectionByteCount),
        ownerState.characterNameBufferF1c[0] ? ownerState.characterNameBufferF1c : "<empty>",
        static_cast<unsigned>(ownerState.characterReplyFieldF3c),
        static_cast<unsigned>(ownerState.characterReplyFieldF40),
        static_cast<unsigned>(headerWord00),
        static_cast<unsigned>(headerWord04),
        static_cast<unsigned>(headerWord08),
        static_cast<unsigned>(headerWord0c),
        static_cast<unsigned>(secondaryWord00),
        static_cast<unsigned>(secondaryWord04),
        static_cast<unsigned>(bodyWord00),
        static_cast<unsigned>(ownerState.replySectionData13cc),
        static_cast<unsigned>(ownerState.replySectionData13d0),
        static_cast<unsigned>(ownerState.state8Section0OverflowLength13f4),
        static_cast<unsigned>(ownerState.flag1452),
        static_cast<unsigned>(ownerState.state8Section11Dword145c),
        static_cast<unsigned>(section11Length));
}

}  // namespace

// anchor: launcher.exe vtable 0x004b5104
const char* CLTLoginState_State8_0x4b5104::DebugName() const {
    return "CLTLoginState_State8_0x4b5104";
}

// anchor: launcher.exe:0x0043bd20 (vtable 0x004b5104 slot 3)
void CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)upstreamOrArg;
    if (!mediator) {
        return;
    }

    // Ownership/fidelity correction:
    // - `0x43bd20` is `CLTLoginState_State8_0x4b5104` slot 3
    // - it owns a structured packet-builder path, not a mediator slot body
    // - current best read from decompilation + disassembly:
    //   - precheck owner `+0x1c` through `0x41b4b0`
    //     - on failure, original switches helper state to `4`
    //   - then gate on owner byte `+0xf14`
    //     - on zero, original switches helper state to `6`
    //   - fetch current slot record through owner vtable `+0x44`
    //   - initialize packet-builder family `0x43ac10`
    //   - write the current character id pair from that current slot record into fixed packet
    //     dwords `0x01/0x05`
    //   - then write selection snapshot blocks from owner `+0xcd0..+0xd7f` in the original packet
    //     order (`0x09/0x19/0x29`, `0x79/0x89/0x99/0xa9`, then `0x39/0x49/0x59/0x69`)
    //   - negative result worth keeping explicit: state8 slot3 does **not** reuse persisted
    //     snapshot `blockCd0[0..1]` for those fixed GCID packet fields
    //   - append `GameSessionID` through `0x43ada0`
    //     - newer `0x43acf0 + 0x4557b0` tightening now makes the growth rule explicit:
    //       reserve `(GameSessionID byte count including NUL) + 2` bytes at the tail of the
    //       retained message-ref's inner storage, write the resulting payload-relative offset back
    //       to fixed field `+0xb9`, then copy the text into that reservation
    //     - but fresh original-launcher WineDbg validation on the natural first state8 send now
    //       shows owner `+0x664` / `GetGameSessionId()` returning `""` there
    //     - practical consequence: the natural `0x0bb -> 0x13b` growth is **not** explained by a
    //       non-empty `GameSessionID` append on that first existing-character send
    //   - send through `0x41af70`
    //     - newer `0x41af70` tightening matters for the current blocker:
    //       it does not serialize raw bytes itself
    //       it forwards the stack-local packet-envelope object into current margin connection
    //       vtable `+0x24` / `0x41cf30 = CMessageConnection_ForwardEnvelopeToSendPacket`
    //       and that wrapper then forwards envelope `+0x08` (the retained outer message-ref)
    //       into vtable `+0x28` / inherited `CMessageConnection_0x4b7928::SendPacket` (`0x448cf0`)
    //   - post event `9`
    // Practical current boundary from the newest original-launcher runs:
    // - natural original reaches this sender, crosses the `0x41af70/0x41cf30` send bridge, and
    //   later does reach state8 slot 6 / `0x43f930`
    // - so the old pre-`0x43f930` survivability question is no longer the first missing natural
    //   boundary; the next target is deeper reply-side behavior inside slot 6 and the continuation
    //   after it
    // - the receive-side route for that live slot-6 hit is now tighter too:
    //   `CMarginConnection::OnOperationCompleted` (`0x44af60`) ->
    //   `CMessageConnection_0x4b7928::OnOperationCompleted` (`0x4490c0`) ->
    //   `CBaseMarginConnection::DispatchMessage` (`0x442d00`) ->
    //   mediator re-entry `0x41f260` into the active helper/state slot-6 body
    // - newer `0x442d00/0x441bc0/0x441850` review now also rules out one tempting shortcut:
    //   the base type-4/MS wrapper path can synthesize a local type-`0x0b` completion object and
    //   fall into mediator fallback `0x41afc0`, which re-enters helper slot 2 instead of slot 6
    if (!mediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = mediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x} currentState={}",
            static_cast<unsigned>(fallbackResult),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<unchanged>");
        return;
    }
    if (mediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = mediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue hit the 0x43bd48 owner+0xf14 gate; switched/dispatched helper6 result=0x{:08x} currentState={} (state8 sender stays gated until the later state6 slot6 writer at 0x440ab9..0x440ae5)",
            static_cast<unsigned>(fallbackResult),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<unchanged>");
        return;
    }

    spdlog::info(
        "ROUTE CHECKPOINT: state8 slot3 entered past the 0x43bd48 owner+0xf14 gate ownerF14={} ownerF18=0x{:08x} currentState={}",
        static_cast<unsigned>(mediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14),
        static_cast<unsigned>(mediator->state6UdpSessionSecretF18_),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");

    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    const SlotRecordState_0x4b5328* currentSlotRecord = mediator->GetCurrentSlotRecord();
    State8StructuredMarginPacketBuilder_0x4af2a4 packetBuilder;
    packetBuilder.ResetAndInitialize();

    packetBuilder.SetFixedDword(0x01, currentSlotRecord ? currentSlotRecord->characterIdLow32 : 0u);
    packetBuilder.SetFixedDword(0x05, currentSlotRecord ? currentSlotRecord->characterIdHigh36 : 0u);

    // Keep block write order aligned with the original `0x43bd20` disassembly, not numeric order.
    // Newer client-side layout aliasing helps interpret the later blocks too:
    // - `cf0` = il.cfg
    // - `d00` = hl.cfg
    // - `d10` = an.cfg
    // - `d20` = rl.cfg
    // - `d30` = cl.cfg
    // - `d40` = pi.cfg
    // - `d50` = ai.cfg
    // - `d60` = shared cs/bl temporary slot on the proven client path
    // - `d70` = cui.cfg
    packetBuilder.SetSelectionBlock(0x09, mediator->SelectionContextBlockCd0());
    packetBuilder.SetSelectionBlock(0x19, mediator->SelectionContextBlockCe0());
    packetBuilder.SetSelectionBlock(0x29, mediator->SelectionContextBlockCf0());
    packetBuilder.SetSelectionBlock(0x79, mediator->SelectionContextBlockD40());
    packetBuilder.SetSelectionBlock(0x89, mediator->SelectionContextBlockD50());
    packetBuilder.SetSelectionBlock(0x99, mediator->SelectionContextBlockD60());
    packetBuilder.SetSelectionBlock(0xa9, mediator->SelectionContextBlockD70());
    packetBuilder.SetSelectionBlock(0x39, mediator->SelectionContextBlockD00());
    packetBuilder.SetSelectionBlock(0x49, mediator->SelectionContextBlockD10());
    packetBuilder.SetSelectionBlock(0x59, mediator->SelectionContextBlockD20());
    packetBuilder.SetSelectionBlock(0x69, mediator->SelectionContextBlockD30());

    packetBuilder.SetGameSessionId(mediator->GetGameSessionId());

    const uint32_t sendResult = mediator->SendCurrentMarginPacket(packetBuilder.Envelope());
    mediator->PostEvent(0x09u);

    spdlog::debug(
        "CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue state8 snapshot blocks cd0={} ce0={} cf0(il.cfg)={} d00(hl.cfg)={} d10(an.cfg)={} d20(rl.cfg)={} d30(cl.cfg)={} d40(pi.cfg)={} d50(ai.cfg)={} d60(bl/cs.cfg)={} d70(cui.cfg)={}",
        FormatU32x4Block(mediator->SelectionContextBlockCd0()),
        FormatU32x4Block(mediator->SelectionContextBlockCe0()),
        FormatU32x4Block(mediator->SelectionContextBlockCf0()),
        FormatU32x4Block(mediator->SelectionContextBlockD00()),
        FormatU32x4Block(mediator->SelectionContextBlockD10()),
        FormatU32x4Block(mediator->SelectionContextBlockD20()),
        FormatU32x4Block(mediator->SelectionContextBlockD30()),
        FormatU32x4Block(mediator->SelectionContextBlockD40()),
        FormatU32x4Block(mediator->SelectionContextBlockD50()),
        FormatU32x4Block(mediator->SelectionContextBlockD60()),
        FormatU32x4Block(mediator->SelectionContextBlockD70()));

    const unsigned nonZeroSnapshotBlockCount =
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCd0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCe0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCf0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD00())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD10())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD20())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD30())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD40())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD50())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD60())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD70()));
    const char* gameSessionId = mediator->GetGameSessionId();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue built structured margin packet fixedBytes=0x{:02x} totalBytes=0x{:02x} gcidLow=0x{:08x} gcidHigh=0x{:08x} nonZeroSnapshotBlocks={}/11 blockCd0_0=0x{:08x} blockD70_3=0x{:08x} GameSessionID='{}' -> sendResult=0x{:08x} then posts event=9",
        State8StructuredMarginPacketFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        currentSlotRecord ? currentSlotRecord->characterIdLow32 : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdHigh36 : 0u,
        nonZeroSnapshotBlockCount,
        mediator->SelectionContextBlockCd0()[0],
        mediator->SelectionContextBlockD70()[3],
        gameSessionId ? gameSessionId : "<empty>",
        sendResult);
    return;
}

// anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
uint32_t CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    // Newer receive-side boundary tightening from `0x44af20/0x442d00` + live original WineDbg:
    // - the active state slot-6 body is now proven live on the natural password-submit path
    // - it is not the first recipient for every incoming margin message
    // - base margin dispatch fully consumes decoded message codes `2`, `4`, and `5`
    // - only other decoded message codes fall through owner `+0x184 -> 0x41f260` and land here
    // - practical consequence: the raw state8 reply opcode `0x10` belongs on that fallback path,
    //   not on the base code-4 wrapper branch
    auto* messageRef = static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(workItem);
    // anchor: launcher.exe:0x43ae50
    LoadCharacterReplyEnvelope_0x4b542c loadCharacterReplyEnvelope(messageRef, 1);
    if (!loadCharacterReplyEnvelope.valid) {
        // Log only if we got a message ref but it couldn't be parsed
        if (messageRef != nullptr) {
            spdlog::info(
                "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage saw message ref but parse rejected currentState={} (expected MS_LoadCharacterReply layout)",
                mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        }
        const uint32_t fallbackResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (fallbackResult < 1u) {
            spdlog::info(
                "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage delegated non-0x10 fallback through owner callback84 -> dispatchResult=0x{:08x}",
                fallbackResult);
            return 1u;
        }
        mediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage non-0x10 fallback through owner callback84 returned 0x{:08x}; mirrored owner+0x80=0x12000005",
            fallbackResult);
        return 0u;
    }

    spdlog::info(
        "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage parsed MS_LoadCharacterReply status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} expectedSections={} seedExpected={} sectionSelector={} sectionOffset=0x{:04x} sectionBytes={} currentState={}",
        static_cast<unsigned>(loadCharacterReplyEnvelope.status),
        static_cast<unsigned>(loadCharacterReplyEnvelope.field05),
        static_cast<unsigned>(loadCharacterReplyEnvelope.handoffWord09),
        static_cast<unsigned>(loadCharacterReplyEnvelope.expectedSectionCount0b),
        loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount ? 1u : 0u,
        static_cast<unsigned>(loadCharacterReplyEnvelope.sectionSelectorMinus2),
        static_cast<unsigned>(loadCharacterReplyEnvelope.sectionOffset0e),
        static_cast<unsigned>(loadCharacterReplyEnvelope.sectionByteCount),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");

    auto& ownerState = mediator->postAuthMarginLoadingState_0xf14;
    mediator->worldListCountOrStatus80 = loadCharacterReplyEnvelope.status;
    if (loadCharacterReplyEnvelope.status >= 1u) {
        (void)mediator->SetCurrentState(3u);
        mediator->PostError(10u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage observed failure status=0x{:08x}; original would latch owner+0x80 to that raw server code, switch helper state to 3, and post generic OnLoginError error=10 currentState={}",
            loadCharacterReplyEnvelope.status,
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<unchanged>");
        return 1u;
    }

    const bool firstFragment = (replySectionsSeen_ == 0u);
    bool usedCurrentSlotRecord = false;
    if (firstFragment) {
        // anchor: launcher.exe:0x438a50 first-fragment reset inside `0x43f930`
        // Keep this inlined here instead of routing through replacement-only helper methods:
        // current static-RE shows this work as part of the original slot6 body.
        ownerState.state8PersistenceDataF1c = {};
        std::fill(std::begin(ownerState.characterNameBufferF1c), std::end(ownerState.characterNameBufferF1c), '\0');
        ownerState.characterReplyFieldF3c = 0u;
        ownerState.characterReplyFieldF40 = 0u;
        ownerState.characterReplyFieldF44 = 0x1000u;
        std::fill(ownerState.characterFlagsF48.begin(), ownerState.characterFlagsF48.end(), 0u);
        std::fill(ownerState.secondaryCharacterDataF68.begin(), ownerState.secondaryCharacterDataF68.end(), 0u);
        std::fill(ownerState.characterRecordPointersF88.begin(), ownerState.characterRecordPointersF88.end(), 0u);
        std::fill(ownerState.section0StringF8c.begin(), ownerState.section0StringF8c.end(), '\0');
        std::fill(ownerState.section0StringFac.begin(), ownerState.section0StringFac.end(), '\0');
        std::fill(ownerState.section0StringFcc.begin(), ownerState.section0StringFcc.end(), '\0');
        std::fill(ownerState.state8Section0RawF88.begin(), ownerState.state8Section0RawF88.end(), 0u);
        ownerState.replySectionData13cc = 0u;
        ownerState.replySectionData13d0 = 0u;
        ownerState.section0Flag13f6 = 0u;
        ResetOwnedSectionBytes(
            ownerState.state8Section0OverflowBuffer13f0,
            ownerState.state8Section0OverflowLength13f4,
            ownerState.section0Flag13f6);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer13f8, ownerState.allocatedBufferLength13fc, ownerState.flag13fe);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1400, ownerState.allocatedBufferLength1404, ownerState.flag1406);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1408, ownerState.allocatedBufferLength140c, ownerState.allocatedBufferFlag140e);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1410, ownerState.allocatedBufferLength1414, ownerState.flag1416);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1418, ownerState.allocatedBufferLength141c, ownerState.allocatedBufferFlag141e);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1420, ownerState.allocatedBufferLength1424, ownerState.allocatedBufferFlag1426);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1428, ownerState.allocatedBufferLength142c, ownerState.allocatedBufferFlag142e);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1430, ownerState.allocatedBufferLength1434, ownerState.flag1436);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1438, ownerState.allocatedBufferLength143c, ownerState.flag143e);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1440, ownerState.allocatedBufferLength1444, ownerState.flag1448);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer144c, ownerState.allocatedBufferLength1450, ownerState.flag1452);
        ResetOwnedSectionBytes(ownerState.allocatedBuffer1454, ownerState.allocatedBufferLength1458, ownerState.flag145a);
        ownerState.state8Section10ChunkBitmap = 0u;
        ownerState.state8Section11Dword145c = 0u;
        ownerState.state8Section11String1460.clear();

        ownerState.characterReplyFieldF3c = loadCharacterReplyEnvelope.field05;
        ownerState.state8PersistenceDataF1c.replyField20 = loadCharacterReplyEnvelope.field05;

        const SlotRecordState_0x4b5328* currentSlotRecord = mediator->GetCurrentSlotRecord();
        if (currentSlotRecord != nullptr) {
            if (currentSlotRecord->heapString14) {
                const size_t copyCount = std::min(
                    std::strlen(currentSlotRecord->heapString14),
                    sizeof(ownerState.characterNameBufferF1c) - 1u);
                std::copy_n(
                    currentSlotRecord->heapString14,
                    copyCount,
                    ownerState.characterNameBufferF1c);
                ownerState.characterNameBufferF1c[copyCount] = '\0';
                std::copy(
                    ownerState.characterNameBufferF1c,
                    ownerState.characterNameBufferF1c + sizeof(ownerState.characterNameBufferF1c),
                    ownerState.state8PersistenceDataF1c.characterName00.begin());
            }
            ownerState.characterReplyFieldF40 = currentSlotRecord->worldId3c;
            ownerState.secondaryCharacterDataF68[0] = currentSlotRecord->worldId3c;
            ownerState.secondaryCharacterDataF68[1] = currentSlotRecord->status3a;
            ownerState.state8PersistenceDataF1c.selectedWorldField24 = currentSlotRecord->worldId3c;
            ownerState.state8PersistenceDataF1c.secondary4c[0] = currentSlotRecord->worldId3c;
            ownerState.state8PersistenceDataF1c.secondary4c[1] = currentSlotRecord->status3a;
            usedCurrentSlotRecord = true;
        } else {
            std::copy(
                ownerState.createCharacterData108.characterName00.begin(),
                ownerState.createCharacterData108.characterName00.end(),
                ownerState.characterNameBufferF1c);
            ownerState.characterNameBufferF1c[sizeof(ownerState.characterNameBufferF1c) - 1u] = '\0';
            std::copy(
                ownerState.createCharacterData108.characterName00.begin(),
                ownerState.createCharacterData108.characterName00.end(),
                ownerState.state8PersistenceDataF1c.characterName00.begin());
            ownerState.characterReplyFieldF40 = ownerState.createCharacterData108.selectedWorldField24;
            ownerState.state8PersistenceDataF1c.selectedWorldField24 =
                ownerState.createCharacterData108.selectedWorldField24;
        }
    }

    if (loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = loadCharacterReplyEnvelope.expectedSectionCount0b;
    }

    auto& persistence = ownerState.state8PersistenceDataF1c;
    switch (loadCharacterReplyEnvelope.sectionSelectorMinus2) {
        case 0u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr) {
                const size_t fixedPrefixBytes = std::min<size_t>(
                    loadCharacterReplyEnvelope.sectionByteCount,
                    ownerState.characterFlagsF48.size() * sizeof(uint32_t));
                if (fixedPrefixBytes != 0u) {
                    std::memcpy(ownerState.characterFlagsF48.data(), loadCharacterReplyEnvelope.sectionData, fixedPrefixBytes);
                    std::memcpy(persistence.header2c.data(), loadCharacterReplyEnvelope.sectionData, fixedPrefixBytes);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x20u) {
                    CopyBoundedRawBytes(
                        ownerState.state8Section0RawF88.data(),
                        ownerState.state8Section0RawF88.size(),
                        loadCharacterReplyEnvelope.sectionData + 0x20u,
                        loadCharacterReplyEnvelope.sectionByteCount - 0x20u);
                    persistence.bodyWord6c = 0x1000u;
                    std::fill(persistence.realFirstName70.begin(), persistence.realFirstName70.end(), '\0');
                    std::fill(persistence.realLastName90.begin(), persistence.realLastName90.end(), '\0');
                    std::fill(persistence.backgroundB0.begin(), persistence.backgroundB0.end(), '\0');
                    persistence.replySectionData4b0 = 0u;
                    persistence.replySectionData4b4 = 0u;
                    persistence.tail4b8 = {1u};
                    CopyBoundedRawBytes(
                        reinterpret_cast<uint8_t*>(&persistence.bodyWord6c),
                        CLTLoginMediator::CLTLoginMediatorCharacterPersistenceData::kBodySize,
                        loadCharacterReplyEnvelope.sectionData + 0x20u,
                        loadCharacterReplyEnvelope.sectionByteCount - 0x20u);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount >= 4u) {
                    ownerState.characterRecordPointersF88[0] = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x00u);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x444u) {
                    ownerState.replySectionData13cc = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x444u);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x448u) {
                    ownerState.replySectionData13d0 = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x448u);
                }
                CopyCStringIntoFixed(
                    ownerState.section0StringF8c.data(),
                    ownerState.section0StringF8c.size(),
                    loadCharacterReplyEnvelope.sectionByteCount > 0x04u ? (loadCharacterReplyEnvelope.sectionData + 0x04u) : nullptr,
                    loadCharacterReplyEnvelope.sectionByteCount > 0x04u ? loadCharacterReplyEnvelope.sectionByteCount - 0x04u : 0u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFac.data(),
                    ownerState.section0StringFac.size(),
                    loadCharacterReplyEnvelope.sectionByteCount > 0x24u ? (loadCharacterReplyEnvelope.sectionData + 0x24u) : nullptr,
                    loadCharacterReplyEnvelope.sectionByteCount > 0x24u ? loadCharacterReplyEnvelope.sectionByteCount - 0x24u : 0u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFcc.data(),
                    ownerState.section0StringFcc.size(),
                    loadCharacterReplyEnvelope.sectionByteCount > 0x44u ? (loadCharacterReplyEnvelope.sectionData + 0x44u) : nullptr,
                    loadCharacterReplyEnvelope.sectionByteCount > 0x44u ? loadCharacterReplyEnvelope.sectionByteCount - 0x44u : 0u);
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x485u && ownerState.state8Section0OverflowBuffer13f0 == nullptr) {
                    const uint16_t overflowAppendLen =
                        static_cast<uint16_t>(loadCharacterReplyEnvelope.sectionByteCount - 0x485u);
                    const size_t newLength = overflowAppendLen;
                    void* newBuffer = std::malloc(newLength);
                    if (newBuffer != nullptr) {
                        std::memcpy(
                            newBuffer,
                            loadCharacterReplyEnvelope.sectionData + 0x485u,
                            overflowAppendLen);
                        ownerState.state8Section0OverflowBuffer13f0 = newBuffer;
                        ownerState.state8Section0OverflowLength13f4 = overflowAppendLen;
                    }
                }
                persistence.section0OverflowBuffer4d4 = ownerState.state8Section0OverflowBuffer13f0;
                persistence.section0OverflowLength4d8 = ownerState.state8Section0OverflowLength13f4;
                ownerState.section0Flag13f6 = 1u;
                persistence.section0PresentFlag4da = 1u;
                spdlog::info(
                    "CLTLoginState_State8_0x4b5104 section0 parsed name='{}' first='{}' last='{}' background='{}' ptr0=0x{:08x} extra13cc=0x{:08x} extra13d0=0x{:08x}",
                    ownerState.characterNameBufferF1c[0] ? std::string(ownerState.characterNameBufferF1c) : std::string("<empty>"),
                    ownerState.section0StringF8c[0] ? std::string(ownerState.section0StringF8c.data()) : std::string("<empty>"),
                    ownerState.section0StringFac[0] ? std::string(ownerState.section0StringFac.data()) : std::string("<empty>"),
                    ownerState.section0StringFcc[0] ? std::string(ownerState.section0StringFcc.data()) : std::string("<empty>"),
                    static_cast<unsigned>(ownerState.characterRecordPointersF88[0]),
                    static_cast<unsigned>(ownerState.replySectionData13cc),
                    static_cast<unsigned>(ownerState.replySectionData13d0));
                LogState8PersistenceFamilySnapshot(ownerState, "section0", loadCharacterReplyEnvelope.sectionSelectorMinus2, loadCharacterReplyEnvelope.sectionByteCount, false);
            }
            break;
        case 1u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength13fc;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer13f8
                    ? std::realloc(ownerState.allocatedBuffer13f8, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer13f8 = newBuffer;
                    ownerState.allocatedBufferLength13fc = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag13fe = 1u;
            persistence.section01Buffer4dc = ownerState.allocatedBuffer13f8;
            persistence.section01Length4e0 = ownerState.allocatedBufferLength13fc;
            persistence.section01PresentFlag4e2 = 1u;
            break;
        case 2u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1404;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1400
                    ? std::realloc(ownerState.allocatedBuffer1400, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1400 = newBuffer;
                    ownerState.allocatedBufferLength1404 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag1406 = 1u;
            persistence.section02Buffer4e4 = ownerState.allocatedBuffer1400;
            persistence.section02Length4e8 = ownerState.allocatedBufferLength1404;
            persistence.section02PresentFlag4ea = 1u;
            break;
        case 3u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength141c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1418
                    ? std::realloc(ownerState.allocatedBuffer1418, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1418 = newBuffer;
                    ownerState.allocatedBufferLength141c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.allocatedBufferFlag141e = 1u;
            persistence.section03Buffer4fc = ownerState.allocatedBuffer1418;
            persistence.section03Length500 = ownerState.allocatedBufferLength141c;
            persistence.section03PresentFlag502 = 1u;
            break;
        case 4u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1424;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1420
                    ? std::realloc(ownerState.allocatedBuffer1420, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1420 = newBuffer;
                    ownerState.allocatedBufferLength1424 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.allocatedBufferFlag1426 = 1u;
            persistence.section04Buffer504 = ownerState.allocatedBuffer1420;
            persistence.section04Length508 = ownerState.allocatedBufferLength1424;
            persistence.section04PresentFlag50a = 1u;
            break;
        case 5u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength142c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1428
                    ? std::realloc(ownerState.allocatedBuffer1428, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1428 = newBuffer;
                    ownerState.allocatedBufferLength142c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.allocatedBufferFlag142e = 1u;
            persistence.section05Buffer50c = ownerState.allocatedBuffer1428;
            persistence.section05Length510 = ownerState.allocatedBufferLength142c;
            persistence.section05PresentFlag512 = 1u;
            break;
        case 6u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength140c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1408
                    ? std::realloc(ownerState.allocatedBuffer1408, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1408 = newBuffer;
                    ownerState.allocatedBufferLength140c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.allocatedBufferFlag140e = 1u;
            persistence.section06Buffer4ec = ownerState.allocatedBuffer1408;
            persistence.section06Length4f0 = ownerState.allocatedBufferLength140c;
            persistence.section06PresentFlag4f2 = 1u;
            break;
        case 7u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1414;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1410
                    ? std::realloc(ownerState.allocatedBuffer1410, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1410 = newBuffer;
                    ownerState.allocatedBufferLength1414 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag1416 = 1u;
            persistence.section07Buffer4f4 = ownerState.allocatedBuffer1410;
            persistence.section07Length4f8 = ownerState.allocatedBufferLength1414;
            persistence.section07PresentFlag4fa = 1u;
            break;
        case 8u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1444;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1440
                    ? std::realloc(ownerState.allocatedBuffer1440, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1440 = newBuffer;
                    ownerState.allocatedBufferLength1444 = static_cast<uint32_t>(newLength);
                }
            }
            ownerState.flag1448 = 1u;
            persistence.section08Buffer524 = ownerState.allocatedBuffer1440;
            persistence.section08Length528 = ownerState.allocatedBufferLength1444;
            persistence.section08PresentFlag52c = 1u;
            break;
        case 9u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1450;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer144c
                    ? std::realloc(ownerState.allocatedBuffer144c, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer144c = newBuffer;
                    ownerState.allocatedBufferLength1450 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag1452 = 1u;
            persistence.section09Buffer530 = ownerState.allocatedBuffer144c;
            persistence.section09Length534 = ownerState.allocatedBufferLength1450;
            persistence.section09PresentFlag536 = 1u;
            LogState8PersistenceFamilySnapshot(ownerState, "section9_clcfg1452", loadCharacterReplyEnvelope.sectionSelectorMinus2, loadCharacterReplyEnvelope.sectionByteCount, false);
            break;
        case 10u:
            if (ownerState.allocatedBuffer1454 == nullptr) {
                ownerState.allocatedBuffer1454 = std::malloc(0x7d00u);
                ownerState.allocatedBufferLength1458 = 0u;
                ownerState.state8Section10ChunkBitmap = 0u;
            }
            if (ownerState.allocatedBuffer1454 != nullptr && loadCharacterReplyEnvelope.sectionData && loadCharacterReplyEnvelope.expectedSectionCount0b != 0u) {
                const size_t chunkIndex = static_cast<size_t>(loadCharacterReplyEnvelope.expectedSectionCount0b - 1u);
                const size_t chunkOffset = chunkIndex * 1000u;
                if (chunkOffset + loadCharacterReplyEnvelope.sectionByteCount <= 0x7d00u) {
                    std::memcpy(
                        static_cast<uint8_t*>(ownerState.allocatedBuffer1454) + chunkOffset,
                        loadCharacterReplyEnvelope.sectionData,
                        loadCharacterReplyEnvelope.sectionByteCount);
                    if (chunkIndex < 32u) {
                        StateReplyChunkBitset(ownerState.state8Section10ChunkBitmap).SetSeen(chunkIndex);
                    }
                    ownerState.allocatedBufferLength1458 = static_cast<uint16_t>(
                        ownerState.allocatedBufferLength1458 + loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.flag145a = 1u;
                    persistence.section0aChunkedBuffer538 = ownerState.allocatedBuffer1454;
                    persistence.section0aChunkedLength53c = ownerState.allocatedBufferLength1458;
                    persistence.section0aPresentFlag53e = 1u;
                }
            }
            break;
        case 11u:
            // anchor: launcher.exe:0x43f8c0
            // Current best read from disassembly:
            // - if section byteCount > 4, copy the leading dword into owner `+0x145c`
            // - then copy the remaining bytes into the small-string-like family at owner `+0x1460`
            // - otherwise clear both fields
            ownerState.state8Section11Dword145c = 0u;
            ownerState.state8Section11String1460.clear();
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount > 4u) {
                ownerState.state8Section11Dword145c = ReadU32LE(loadCharacterReplyEnvelope.sectionData);
                ownerState.state8Section11String1460.assign(
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + 4u),
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + loadCharacterReplyEnvelope.sectionByteCount));
                persistence.section11Dword540 = ownerState.state8Section11Dword145c;
                char* const section11Begin = ownerState.state8Section11String1460.data();
                persistence.section11StringBegin544 = section11Begin;
                persistence.section11StringCurrent548 =
                    section11Begin + ownerState.state8Section11String1460.size();
                persistence.section11StringCapacity54c =
                    section11Begin + ownerState.state8Section11String1460.capacity();
            }
            spdlog::info(
                "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage applied section 0x0b side effect dword145c=0x{:08x} string1460Len={}",
                static_cast<unsigned>(ownerState.state8Section11Dword145c),
                static_cast<unsigned>(ownerState.state8Section11String1460.size()));
            break;
        case 12u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength1434;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1430
                    ? std::realloc(ownerState.allocatedBuffer1430, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1430 = newBuffer;
                    ownerState.allocatedBufferLength1434 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag1436 = 1u;
            persistence.section0cBuffer514 = ownerState.allocatedBuffer1430;
            persistence.section0cLength518 = ownerState.allocatedBufferLength1434;
            persistence.section0cPresentFlag51a = 1u;
            break;
        case 13u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = ownerState.allocatedBufferLength143c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = ownerState.allocatedBuffer1438
                    ? std::realloc(ownerState.allocatedBuffer1438, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    ownerState.allocatedBuffer1438 = newBuffer;
                    ownerState.allocatedBufferLength143c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            ownerState.flag143e = 1u;
            persistence.section0dBuffer51c = ownerState.allocatedBuffer1438;
            persistence.section0dLength520 = ownerState.allocatedBufferLength143c;
            persistence.section0dPresentFlag522 = 1u;
            break;
        default:
            break;
    }

    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        if (ownerState.allocatedBuffer1454 != nullptr) {
            size_t firstChunkIndex = 0u;
            while (firstChunkIndex < 32u &&
                   !StateReplyChunkBitset(ownerState.state8Section10ChunkBitmap).HasSeen(firstChunkIndex)) {
                ++firstChunkIndex;
            }
            if (firstChunkIndex < 32u) {
                std::memmove(
                    ownerState.allocatedBuffer1454,
                    static_cast<uint8_t*>(ownerState.allocatedBuffer1454) + (firstChunkIndex * 1000u),
                    ownerState.allocatedBufferLength1458);
            }
            if (void* compacted = std::realloc(ownerState.allocatedBuffer1454, ownerState.allocatedBufferLength1458)) {
                ownerState.allocatedBuffer1454 = compacted;
            }
        }
        LogState8PersistenceFamilySnapshot(ownerState, "completed", loadCharacterReplyEnvelope.sectionSelectorMinus2, loadCharacterReplyEnvelope.sectionByteCount, true);

        if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(mediator->LoginHelperStateByIdScaffold(9u))) {
            nextState->SetPendingPayload(/*byte4=*/0, loadCharacterReplyEnvelope.handoffWord09);
        }
        const uint32_t slot3Result = mediator->SetCurrentState(9u);
        spdlog::info(
            "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage mirrored 0x41b450 helper9 handoff before event=0x0b handoffWord=0x{:04x} -> slot3Result=0x{:08x}",
            loadCharacterReplyEnvelope.handoffWord09,
            static_cast<unsigned>(slot3Result));
        spdlog::info(
            "ROUTE CHECKPOINT: late-login state8 complete -> state9 handoffWord=0x{:04x} currentState={}",
            loadCharacterReplyEnvelope.handoffWord09,
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        // anchor: launcher.exe:0x43f930 completion tail posts event 0x0b after switching to helper9.
        // Important recovered ordering detail from `0x41b450`:
        // - the helper9/state9 install itself immediately notifies the new helper through slot 3
        // - `0x439780` therefore runs before this later `PostEvent(0x0b)` tail
        mediator->PostEvent(0x0bu);

        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage completed state8 reply progression status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} firstFragment={} usedCurrentSlotRecord={} -> currentState=helper9 event=0x0b",
            loadCharacterReplyEnvelope.status,
            loadCharacterReplyEnvelope.sectionSelectorMinus2,
            loadCharacterReplyEnvelope.sectionByteCount,
            loadCharacterReplyEnvelope.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_,
            firstFragment ? 1u : 0u,
            usedCurrentSlotRecord ? 1u : 0u);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
        ownerState.state8Section10ChunkBitmap = 0u;
    } else {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage routed state8 reply status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} seedCount={} firstFragment={} usedCurrentSlotRecord={}",
            loadCharacterReplyEnvelope.status,
            loadCharacterReplyEnvelope.sectionSelectorMinus2,
            loadCharacterReplyEnvelope.sectionByteCount,
            loadCharacterReplyEnvelope.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_,
            loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount ? 1u : 0u,
            firstFragment ? 1u : 0u,
            usedCurrentSlotRecord ? 1u : 0u);
    }
    return 1u;
}

// anchor: launcher.exe:0x00438c90 (vtable 0x004b5104 slot 7)
uint32_t CLTLoginState_State8_0x4b5104::GetStateId() const {
    return 8;
}

}  // namespace mxo::ltlogin
