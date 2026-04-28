#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

// anchor: launcher.exe:0x43bd20 = CLTLoginState_State8_Slot3_BeginOrContinue
// Static-RE faithful implementation using cls_0x4b5418 (vtable 0x4b5418).
// Note: Original does NOT use a separate State8* wrapper class - uses cls_0x4b5418 directly.

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
    (void)upstreamOrArg;
    if (!g_CurrentLoginMediator) {
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
    if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x} currentState={}",
            static_cast<unsigned>(fallbackResult),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
        return;
    }
    if (g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue hit the 0x43bd48 owner+0xf14 gate; switched/dispatched helper6 result=0x{:08x} currentState={} (state8 sender stays gated until the later state6 slot6 writer at 0x440ab9..0x440ae5)",
            static_cast<unsigned>(fallbackResult),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
        return;
    }

    spdlog::info(
        "ROUTE CHECKPOINT: state8 slot3 entered past the 0x43bd48 owner+0xf14 gate ownerF14={} ownerF18=0x{:08x} currentState={}",
        static_cast<unsigned>(g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14),
        static_cast<unsigned>(g_CurrentLoginMediator->state6UdpSessionSecretF18_),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");

    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    const Packet_MsClaimCharacterNameReply_0x4b5328* currentSlotRecord =
        g_CurrentLoginMediator->GetCurrentAuthReplyPacket44();

    // DIAGNOSTIC: Trace slot record state before using it
    spdlog::info(
        "DIAGNOSTIC: State8 Slot3 currentSlotRecord={} charIdLow=0x{:08x} charIdHigh=0x{:08x}",
        fmt::ptr(currentSlotRecord),
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->characterIdLow1c) : 0u,
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->characterIdHigh20) : 0u);

    // anchor: launcher.exe:0x43bd6a = Packet_MsLoadCharacterRequest_0x4b5418::ResetAndInitialize
    // anchor: launcher.exe:0x43ac10 = ResetAndInitialize
    Packet_MsLoadCharacterRequest_0x4b5418 packetBuilder;
    packetBuilder.ResetAndInitialize();

    // DIAGNOSTIC: Log raw payload header for trace
uint8_t* rawPayloadForDiag = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
    if (rawPayloadForDiag) {
        spdlog::info(
            "DIAGNOSTIC: State8 Slot3 raw payload after Reset: opcode=0x{:02x} charIdLow=0x{:08x} charIdHigh=0x{:08x} zeroPrefixDword0=0x{:08x} weirdBlock0=0x{:08x}",
            static_cast<unsigned>(rawPayloadForDiag[0]),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(rawPayloadForDiag + 1)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(rawPayloadForDiag + 5)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(rawPayloadForDiag + 0x09)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(rawPayloadForDiag + 0x29)));
    }

    // anchor: launcher.exe:0x43bd6f-0x43bd81 = write character ID pair directly to payload
    // The 0x43bd20 body only writes the GCID pair here. The following 0x09..0x28 zero-prefix and
    // 0x29..0xb8 repeated-selection blocks come from the persisted owner snapshot writes below;
    // there is no separate state8-side "counter=9" byte write in the original body.
    uint8_t* payload = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
    if (payload) {
        *reinterpret_cast<uint32_t*>(payload + 0x01) = currentSlotRecord ? currentSlotRecord->characterIdLow1c : 0u;
        *reinterpret_cast<uint32_t*>(payload + 0x05) = currentSlotRecord ? currentSlotRecord->characterIdHigh20 : 0u;
    }

    // anchor: launcher.exe:0x43bd84-0x43bdfa = write selection blocks directly to payload
    // Write block order matches original `0x43bd20` disassembly exactly.
    // anchor: matches launcher.exe:0x43bd84-0x43bdfa original offsets exactly
    // fidelity: direct element access like original decompile (not helper lambda)
    if (payload) {
        // Block Cd0 at 0x09-0x17 (selection context, 16 bytes total)
        *reinterpret_cast<uint32_t*>(payload + 0x09) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[0];
        *reinterpret_cast<uint32_t*>(payload + 0x0d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[1];
        *reinterpret_cast<uint32_t*>(payload + 0x11) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[2];
        *reinterpret_cast<uint32_t*>(payload + 0x15) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[3];

        // Block Ce0 at 0x19-0x27
        *reinterpret_cast<uint32_t*>(payload + 0x19) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[0];
        *reinterpret_cast<uint32_t*>(payload + 0x1d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[1];
        *reinterpret_cast<uint32_t*>(payload + 0x21) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[2];
        *reinterpret_cast<uint32_t*>(payload + 0x25) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[3];

        // Block Cf0 at 0x29-0x37
        *reinterpret_cast<uint32_t*>(payload + 0x29) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[0];
        *reinterpret_cast<uint32_t*>(payload + 0x2d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[1];
        *reinterpret_cast<uint32_t*>(payload + 0x31) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[2];
        *reinterpret_cast<uint32_t*>(payload + 0x35) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[3];

        // Block D40 at 0x79-0x87
        *reinterpret_cast<uint32_t*>(payload + 0x79) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[0];
        *reinterpret_cast<uint32_t*>(payload + 0x7d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[1];
        *reinterpret_cast<uint32_t*>(payload + 0x81) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[2];
        *reinterpret_cast<uint32_t*>(payload + 0x85) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[3];

        // Block D50 at 0x89-0x97
        *reinterpret_cast<uint32_t*>(payload + 0x89) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[0];
        *reinterpret_cast<uint32_t*>(payload + 0x8d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[1];
        *reinterpret_cast<uint32_t*>(payload + 0x91) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[2];
        *reinterpret_cast<uint32_t*>(payload + 0x95) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[3];

        // Block D60 at 0x99-0xa7
        *reinterpret_cast<uint32_t*>(payload + 0x99) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[0];
        *reinterpret_cast<uint32_t*>(payload + 0x9d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[1];
        *reinterpret_cast<uint32_t*>(payload + 0xa1) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[2];
        *reinterpret_cast<uint32_t*>(payload + 0xa5) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[3];

        // Block D70 at 0xa9-0xb7
        *reinterpret_cast<uint32_t*>(payload + 0xa9) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[0];
        *reinterpret_cast<uint32_t*>(payload + 0xad) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[1];
        *reinterpret_cast<uint32_t*>(payload + 0xb1) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[2];
        *reinterpret_cast<uint32_t*>(payload + 0xb5) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[3];

        // Block D00 at 0x39-0x47
        *reinterpret_cast<uint32_t*>(payload + 0x39) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[0];
        *reinterpret_cast<uint32_t*>(payload + 0x3d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[1];
        *reinterpret_cast<uint32_t*>(payload + 0x41) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[2];
        *reinterpret_cast<uint32_t*>(payload + 0x45) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[3];

        // Block D10 at 0x49-0x57
        *reinterpret_cast<uint32_t*>(payload + 0x49) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[0];
        *reinterpret_cast<uint32_t*>(payload + 0x4d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[1];
        *reinterpret_cast<uint32_t*>(payload + 0x51) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[2];
        *reinterpret_cast<uint32_t*>(payload + 0x55) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[3];

        // Block D20 at 0x59-0x67
        *reinterpret_cast<uint32_t*>(payload + 0x59) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[0];
        *reinterpret_cast<uint32_t*>(payload + 0x5d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[1];
        *reinterpret_cast<uint32_t*>(payload + 0x61) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[2];
        *reinterpret_cast<uint32_t*>(payload + 0x65) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[3];

        // Block D30 at 0x69-0x77
        *reinterpret_cast<uint32_t*>(payload + 0x69) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[0];
        *reinterpret_cast<uint32_t*>(payload + 0x6d) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[1];
        *reinterpret_cast<uint32_t*>(payload + 0x71) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[2];
        *reinterpret_cast<uint32_t*>(payload + 0x75) = g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[3];
    }

    // anchor: launcher.exe:0x43ada0 = SetGameSessionId (mediator helper)
    // Note: Original 0x43ada0 is a CLTLoginMediator method that operates on mediator fields.
    // For source fidelity, we implement the reservation logic inline here.
    // The mediator caches the write pointer in its own fields (not packet builder fields).

    // DIAGNOSTIC: Log payload after all writes
    if (payload) {
        spdlog::info(
            "DIAGNOSTIC: State8 Slot3 final payload: charIdLow=0x{:08x} charIdHigh=0x{:08x} zeroPrefixDword0=0x{:08x} zeroPrefixDword4=0x{:08x} weirdBlock0=0x{:08x} weirdBlock8=0x{:08x}",
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0x01)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0x05)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0x09)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0x19)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0x29)),
            static_cast<unsigned>(*reinterpret_cast<uint32_t*>(payload + 0xa9)));
    }

    const char* gameSessionId = g_CurrentLoginMediator->GetGameSessionId();
    if (gameSessionId) {
        // Compute string length including NUL
        size_t textLen = 0;
        const char* p = gameSessionId;
        while (*p++) ++textLen;
        ++textLen;

        if (packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c) {
            auto* storage = packetBuilder.messageRef08->messageStorage0c;
            const uint16_t currentSize = storage->PayloadByteCount();
            const uint16_t remaining = storage->RemainingAppendableByteCount();

            if (remaining >= 2u + textLen) {
                const uint16_t growth = static_cast<uint16_t>(2u + textLen);
                const uint16_t newSize = storage->GrowPayloadByteCount(growth);

                if (newSize == currentSize + growth && payload) {
                    uint8_t* lengthPrefix = payload + currentSize;
                    lengthPrefix[0] = static_cast<uint8_t>(textLen & 0xffu);
                    lengthPrefix[1] = static_cast<uint8_t>((textLen >> 8) & 0xffu);

                    // Write payload-relative offset to fixed field at +0xb9
                    *reinterpret_cast<uint16_t*>(payload + 0xb9) = currentSize;

                    if (textLen > 1u) {
                        std::copy_n(gameSessionId, textLen - 1u, lengthPrefix + 2u);
                    }
                    if (textLen > 0u) {
                        lengthPrefix[2u + textLen - 1u] = '\0';
                    }
                }
            }
        }
    }

    // anchor: launcher.exe:0x439840 = envelope field access pattern
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
    envelope.payloadBase04 = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
    envelope.messageRef08 = packetBuilder.messageRef08;
    const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
    g_CurrentLoginMediator->PostEvent(0x09u);

    const unsigned nonZeroSnapshotBlockCount =
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60)) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70));
    // gameSessionId already declared above in SetGameSessionId block

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue built structured margin packet fixedBytes=0x{:02x} totalBytes=0x{:02x} gcidLow=0x{:08x} gcidHigh=0x{:08x} nonZeroSnapshotBlocks={}/11 blockCd0_0=0x{:08x} blockD70_3=0x{:08x} GameSessionID='{}' -> sendResult=0x{:08x} then posts event=9",
        0xbbu,
        packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c
            ? packetBuilder.messageRef08->messageStorage0c->PayloadByteCount() : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdLow1c : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdHigh20 : 0u,
        nonZeroSnapshotBlockCount,
        g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[0],
        g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[3],
        gameSessionId ? gameSessionId : "<empty>",
        sendResult);
    return;
}

// anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
uint32_t CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    if (!g_CurrentLoginMediator || workItem == nullptr) {
        return 0u;
    }

    // Newer receive-side boundary tightening from `0x44af20/0x442d00` + live original WineDbg:
    // - the active state slot-6 body is now proven live on the natural password-submit path
    // - it is not the first recipient for every incoming margin message
    // - base margin dispatch fully consumes decoded message codes `2`, `4`, and `5`
    // - only other decoded message codes fall through owner `+0x184 -> 0x41f260` and land here
    // - practical consequence: the raw state8 reply opcode `0x10` belongs on that fallback path,
    //   not on the base code-4 wrapper branch
    // anchor: launcher.exe:0x43f941 / 0x41bc20 then 0x43f949 CMP AX,0x10
    uint16_t messageCode = 0;
    if (!mxo::liblttcp::CMessageConnection_0x4b7928_DecodeMessageCode(*workItem, &messageCode, nullptr)) {
        // Original just observes opcode 0 here and takes the same non-0x10 fallback path.
    }

    if (messageCode != 0x10u) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (fallbackResult < 1u) {
            spdlog::info(
                "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage delegated non-0x10 fallback through owner callback84 messageCode=0x{:04x} -> dispatchResult=0x{:08x}",
                static_cast<unsigned>(messageCode),
                fallbackResult);
            return 1u;
        }
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage non-0x10 fallback through owner callback84 messageCode=0x{:04x} returned 0x{:08x}; mirrored owner+0x80=0x12000005",
            static_cast<unsigned>(messageCode),
            fallbackResult);
        return 0u;
    }

    auto* messageRef = static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(workItem);
    // anchor: launcher.exe:0x43ae50
    Packet_MsLoadCharacterReply_0x4b542c loadCharacterReplyEnvelope(messageRef, 1);
    if (!loadCharacterReplyEnvelope.valid) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage rejected decoded opcode-0x10 message layout; mirrored owner+0x80=0x12000005");
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
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");

    auto& ownerState = g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14;
    g_CurrentLoginMediator->worldListCountOrStatus80 = loadCharacterReplyEnvelope.status;
    if (loadCharacterReplyEnvelope.status >= 1u) {
        (void)g_CurrentLoginMediator->SetCurrentState(3u);
        g_CurrentLoginMediator->PostError(10u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage observed failure status=0x{:08x}; original would latch owner+0x80 to that raw server code, switch helper state to 3, and post generic OnLoginError error=10 currentState={}",
            loadCharacterReplyEnvelope.status,
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
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

        const Packet_MsClaimCharacterNameReply_0x4b5328* currentSlotRecord =
            g_CurrentLoginMediator->GetCurrentAuthReplyPacket44();
        if (currentSlotRecord == nullptr) {
            spdlog::info(
                "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage first-fragment invariant failed: currentSlotRecord is null; original 0x43f930 dereferences owner vtable +0x44 result directly here");
            return 0u;
        }

        if (currentSlotRecord->debugString14) {
            const size_t copyCount = std::min(
                std::strlen(currentSlotRecord->debugString14),
                sizeof(ownerState.characterNameBufferF1c) - 1u);
            std::copy_n(
                currentSlotRecord->debugString14,
                copyCount,
                ownerState.characterNameBufferF1c);
            ownerState.characterNameBufferF1c[copyCount] = '\0';
            std::copy(
                ownerState.characterNameBufferF1c,
                ownerState.characterNameBufferF1c + sizeof(ownerState.characterNameBufferF1c),
                ownerState.state8PersistenceDataF1c.characterName00.begin());
        }
        ownerState.characterReplyFieldF40 = currentSlotRecord->worldId24;
        ownerState.secondaryCharacterDataF68[0] = currentSlotRecord->worldId24;
        ownerState.secondaryCharacterDataF68[1] = currentSlotRecord->packetType1a;
        ownerState.state8PersistenceDataF1c.selectedWorldField24 = currentSlotRecord->worldId24;
        ownerState.state8PersistenceDataF1c.secondary4c[0] = currentSlotRecord->worldId24;
        ownerState.state8PersistenceDataF1c.secondary4c[1] = currentSlotRecord->packetType1a;
        usedCurrentSlotRecord = true;
    }

    if (loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = loadCharacterReplyEnvelope.expectedSectionCount0b;
    }

    auto& persistence = ownerState.state8PersistenceDataF1c;
    switch (loadCharacterReplyEnvelope.sectionSelectorMinus2) {
        case 0u:
            // anchor: launcher.exe:0x43fa6e
            // Exact current case-0 read:
            // - copy the first 0x20 bytes from section data into owner/persistence `+0x2c`
            // - copy the next 0x465 bytes from section data + 0x20 into owner/persistence `+0x6c`
            // - if byteCount > 0x485 and overflow buffer is still null, allocate byteCount-0x485
            //   bytes and copy the tail from section data + 0x485 into owner `+0x4d4/+0x4d8`
            // - set owner/persistence section-0 present flags unconditionally on this case path
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
                        CLTLoginMediatorCharacterPersistenceData_0x41d900::kBodySize,
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
                    void* const newBuffer = std::malloc(overflowAppendLen);
                    if (newBuffer != nullptr) {
                        std::memcpy(
                            newBuffer,
                            loadCharacterReplyEnvelope.sectionData + 0x485u,
                            overflowAppendLen);
                        ownerState.state8Section0OverflowBuffer13f0 = newBuffer;
                        ownerState.state8Section0OverflowLength13f4 = overflowAppendLen;
                    }
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
            // anchor: launcher.exe:0x43ffc6
            // Exact current read from `0x43f930`:
            // - section selector `0x0a` uses packet byte `+0x0b` as a 1-based chunk ordinal
            // - allocate a fixed 0x7d00-byte backing buffer on first sight
            // - copy chunk bytes to `buffer + ((ordinal * 1000) - 1000)`
            // - mark that 1-based ordinal as seen in the helper-local bitset
            // - accumulate total materialized byte count in owner `+0x1458`
            if (ownerState.allocatedBuffer1454 == nullptr) {
                ownerState.allocatedBuffer1454 = std::malloc(0x7d00u);
                ownerState.allocatedBufferLength1458 = 0u;
                ownerState.state8Section10ChunkBitmap = 0u;
            }
            if (ownerState.allocatedBuffer1454 != nullptr && loadCharacterReplyEnvelope.sectionData != nullptr) {
                if (loadCharacterReplyEnvelope.expectedSectionCount0b == 0u) {
                    spdlog::info(
                        "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage section0x0a invariant failed: packet byte +0x0b chunk ordinal is zero");
                } else {
                    const uint32_t chunkIndex = static_cast<uint32_t>(loadCharacterReplyEnvelope.expectedSectionCount0b - 1u);
                    const size_t chunkOffset = static_cast<size_t>(chunkIndex) * 1000u;
                    if (chunkOffset + loadCharacterReplyEnvelope.sectionByteCount <= 0x7d00u) {
                        std::memcpy(
                            static_cast<uint8_t*>(ownerState.allocatedBuffer1454) + chunkOffset,
                            loadCharacterReplyEnvelope.sectionData,
                            loadCharacterReplyEnvelope.sectionByteCount);
                        StateReplyChunkBitset(ownerState.state8Section10ChunkBitmap).SetSeen(chunkIndex);
                        ownerState.allocatedBufferLength1458 = static_cast<uint16_t>(
                            ownerState.allocatedBufferLength1458 + loadCharacterReplyEnvelope.sectionByteCount);
                    }
                }
            }
            ownerState.flag145a = 1u;
            persistence.section0aChunkedBuffer538 = ownerState.allocatedBuffer1454;
            persistence.section0aChunkedLength53c = ownerState.allocatedBufferLength1458;
            persistence.section0aPresentFlag53e = 1u;
            break;
        case 11u:
            // anchor: launcher.exe:0x43f8c0 = CLTLoginMediatorCharacterPersistenceData_ApplySection11SideEffect
            // Exact helper behavior:
            // - if byteCount > 4, store leading dword and assign range [data+4, data+byteCount)
            // - else zero dword and, if the current string is non-empty, write a terminating NUL at
            //   begin and collapse current back to begin
            ownerState.state8Section11Dword145c = 0u;
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount > 4u) {
                ownerState.state8Section11Dword145c = ReadU32LE(loadCharacterReplyEnvelope.sectionData);
                ownerState.state8Section11String1460.assign(
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + 4u),
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + loadCharacterReplyEnvelope.sectionByteCount));
            } else if (!ownerState.state8Section11String1460.empty()) {
                ownerState.state8Section11String1460[0] = '\0';
                ownerState.state8Section11String1460.resize(0u);
            } else {
                ownerState.state8Section11String1460.clear();
            }
            persistence.section11Dword540 = ownerState.state8Section11Dword145c;
            {
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

 spdlog::info(
 "DIAGNOSTIC: CLTLoginState_State8 BEFORE completion check: status=0x{:08x} sectionSelector={} sectionBytes={} handoffWord=0x{:04x} expectedCount={} seedFlag={} section0Flag={} section0aFlag={} chunkBitmap=0x{:08x}",
 static_cast<unsigned>(loadCharacterReplyEnvelope.status),
 static_cast<unsigned>(loadCharacterReplyEnvelope.sectionSelectorMinus2),
 static_cast<unsigned>(loadCharacterReplyEnvelope.sectionByteCount),
 static_cast<unsigned>(loadCharacterReplyEnvelope.handoffWord09),
 static_cast<unsigned>(loadCharacterReplyEnvelope.expectedSectionCount0b),
 loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount ? 1u : 0u,
 static_cast<unsigned>(ownerState.section0Flag13f6),
 static_cast<unsigned>(ownerState.flag145a),
 static_cast<unsigned>(ownerState.state8Section10ChunkBitmap));

 // anchor: launcher.exe:0x4408ed - completion check
 // Exact `0x43f930` tail shape from Ghidra now tightened enough to keep literal here:
 // - byte `this+5` / our `replySectionsExpected_` is only latched when packet byte `+0x0c == 1`
 // - byte `this+4` / our `replySectionsSeen_` is then incremented for every accepted reply
 // - completion is only `replySectionsExpected_ != 0 && replySectionsSeen_ >= replySectionsExpected_`
 // - there is no alternate early-complete path on section-0 presence, chunk presence, or
 //   zero-byte terminal markers in the original slot-6 tail
 const uint8_t packetExpectedCount = loadCharacterReplyEnvelope.expectedSectionCount0b;
 const uint8_t currentSection = loadCharacterReplyEnvelope.sectionSelectorMinus2;
 const uint16_t sectionBytes = loadCharacterReplyEnvelope.sectionByteCount;
 const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);

 spdlog::info(
 "DIAGNOSTIC: CLTLoginState_State8 completion check: packetExpected={} seededExpected={} currentSection={} sectionBytes={} seen={} completed={} seedFlag={} section0Flag={} section0aFlag={} chunkBitmap=0x{:08x}",
 packetExpectedCount,
 replySectionsExpected_,
 currentSection,
 static_cast<unsigned>(sectionBytes),
 replySectionsSeen_,
 completed ? 1u : 0u,
 loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount ? 1u : 0u,
 static_cast<unsigned>(ownerState.section0Flag13f6),
 static_cast<unsigned>(ownerState.flag145a),
 static_cast<unsigned>(ownerState.state8Section10ChunkBitmap));

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

        if (auto* nextState = dynamic_cast<CLTLoginState_State9_0x4b517c*>(g_CurrentLoginMediator->LoginHelperStateByIdScaffold(9u))) {
            nextState->SetPendingPayload(/*byte4=*/0, loadCharacterReplyEnvelope.handoffWord09);
        }
        const uint32_t slot3Result = g_CurrentLoginMediator->SetCurrentState(9u);
        spdlog::info(
            "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage mirrored 0x41b450 helper9 handoff before event=0x0b handoffWord=0x{:04x} -> slot3Result=0x{:08x}",
            loadCharacterReplyEnvelope.handoffWord09,
            static_cast<unsigned>(slot3Result));
        spdlog::info(
            "ROUTE CHECKPOINT: late-login state8 complete -> state9 handoffWord=0x{:04x} currentState={}",
            loadCharacterReplyEnvelope.handoffWord09,
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        // anchor: launcher.exe:0x43f930 completion tail posts event 0x0b after switching to helper9.
        // Important recovered ordering detail from `0x41b450`:
        // - the helper9/state9 install itself immediately notifies the new helper through slot 3
        // - `0x439780` therefore runs before this later `PostEvent(0x0b)` tail
        g_CurrentLoginMediator->PostEvent(0x0bu);

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
