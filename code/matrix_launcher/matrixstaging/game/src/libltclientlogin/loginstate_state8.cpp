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

// anchor: launcher.exe:DAT_004f79e4
// State8 slot6 keeps the section-0x0a seen-chunk bitmap in a process-global dword.
static uint32_t g_State8Section10ChunkBitmap_0x4f79e4 = 0u;

// Faithful file-local class used only by state8 slot6 chunk reassembly.
// Ghidra shows the original class is tiny and just wraps a uint32_t bitfield reference.
class StateReplyChunkBitset_0x43c240 {
public:
    explicit StateReplyChunkBitset_0x43c240(uint32_t& bits)
        : bits_(bits) {}

    // anchor: launcher.exe:0x43c240 = StateReplyChunkBitset_0x43c240::SetSeen
    StateReplyChunkBitset_0x43c240* SetSeen(uint32_t chunkIndex) {
        if (chunkIndex >= 32u) {
            spdlog::critical("StateReplyChunkBitset_0x43c240::SetSeen invariant failed: bitset");
            std::abort();
        }
        bits_ |= (1u << (chunkIndex & 0x1fu));
        return this;
    }

    // anchor: launcher.exe:0x43c280 = StateReplyChunkBitset_0x43c240::HasSeen
    bool HasSeen(uint32_t chunkIndex) const {
        if (chunkIndex >= 32u) {
            spdlog::critical("StateReplyChunkBitset_0x43c240::HasSeen invariant failed: bitset");
            std::abort();
        }
        return (bits_ & (1u << (chunkIndex & 0x1fu))) != 0u;
    }

private:
    uint32_t& bits_;
};

}  // namespace

// anchor: launcher.exe vtable 0x4b5104
const char* CLTLoginState_State8_0x4b5104::DebugName() const {
    return "CLTLoginState_State8_0x4b5104";
}

// anchor: launcher.exe:0x43bd20 (vtable 0x4b5104 slot 3)
void CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    if (!g_CurrentLoginMediator) {
        return;
    }

    // anchor: launcher.exe:0x43bd20 slot 3
    // Exact behavior: gate on helper state, build/load request, append session id, send, post event 9.

    if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x} currentState={}",
            static_cast<unsigned>(fallbackResult),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
        return;
    }
    if (g_CurrentLoginMediator->state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue hit the 0x43bd48 owner+0xf14 gate; switched/dispatched helper6 result=0x{:08x} currentState={} (state8 sender stays gated until the later state6 slot6 writer at 0x440ab9..0x440ae5)",
            static_cast<unsigned>(fallbackResult),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
        return;
    }

    spdlog::info(
        "ROUTE CHECKPOINT: state8 slot3 entered past the 0x43bd48 owner+0xf14 gate ownerF14={} ownerF18=0x{:08x} currentState={}",
        static_cast<unsigned>(g_CurrentLoginMediator->state10SendGateFlagF14),
        static_cast<unsigned>(g_CurrentLoginMediator->state6UdpSessionSecretF18_),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");

    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    const Packet_AsAuthReply_0x4b5328* currentSlotRecord =
        g_CurrentLoginMediator->GetCurrentAuthReplyPacket44();

    spdlog::info(
        "DIAGNOSTIC: State8 Slot3 currentSlotRecord={} charIdLow=0x{:08x} charIdHigh=0x{:08x}",
        fmt::ptr(currentSlotRecord),
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->characterIdLow1c) : 0u,
        currentSlotRecord ? static_cast<unsigned>(currentSlotRecord->characterIdHigh20) : 0u);

    // anchor: launcher.exe:0x43bd6a = Packet_MsLoadCharacterRequest_0x4b5418::ResetAndInitialize
    // anchor: launcher.exe:0x43ac10 = ResetAndInitialize
    Packet_MsLoadCharacterRequest_0x4b5418 packetBuilder;
    packetBuilder.ResetAndInitialize();

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

    // anchor: launcher.exe state8 send thunk - pass the stack-local packet builder itself
    g_CurrentLoginMediator->SendCurrentMarginPacket(packetBuilder);
    g_CurrentLoginMediator->PostEvent(0x09u);

    const unsigned nonZeroSnapshotBlockCount =
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCe0[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCf0[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD00[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD10[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD20[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD30[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD40[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD50[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD60[3] != 0u) +
        static_cast<unsigned>(g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[0] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[1] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[2] != 0u || g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[3] != 0u);
    // gameSessionId already declared above in SetGameSessionId block

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State8_0x4b5104::Slot3_BeginOrContinue built structured margin packet fixedBytes=0x{:02x} totalBytes=0x{:02x} gcidLow=0x{:08x} gcidHigh=0x{:08x} nonZeroSnapshotBlocks={}/11 blockCd0_0=0x{:08x} blockD70_3=0x{:08x} GameSessionID='{}' then posts event=9",
        0xbbu,
        packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c
            ? packetBuilder.messageRef08->messageStorage0c->PayloadByteCount() : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdLow1c : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdHigh20 : 0u,
        nonZeroSnapshotBlockCount,
        g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockCd0[0],
        g_CurrentLoginMediator->selectionRouteState684_.persistedSelectionContext64c_.blockD70[3],
        gameSessionId ? gameSessionId : "<empty>");
    return;
}

// anchor: launcher.exe:0x43f930 (vtable 0x4b5104 slot 6)
uint32_t CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    if (!g_CurrentLoginMediator || workItem == nullptr) {
        return 0u;
    }

    // anchor: launcher.exe:0x43ae50
    Packet_MsLoadCharacterReply_0x4b542c loadCharacterReplyEnvelope(workItem, 1);
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
        // anchor: launcher.exe:0x438a50 first-fragment reset
        g_CurrentLoginMediator->state8PersistenceDataF1c = {};
        std::fill(std::begin(g_CurrentLoginMediator->characterNameBufferF1c), std::end(g_CurrentLoginMediator->characterNameBufferF1c), '\0');
        g_CurrentLoginMediator->state8PersistenceDataF1c.replyField20 = 0u;
        g_CurrentLoginMediator->state8PersistenceDataF1c.selectedWorldField24 = 0u;
        g_CurrentLoginMediator->state8PersistenceDataF1c.field28_1000 = 0x1000u;
        std::fill(g_CurrentLoginMediator->state8PersistenceDataF1c.header2c.begin(), g_CurrentLoginMediator->state8PersistenceDataF1c.header2c.end(), 0u);
        std::fill(g_CurrentLoginMediator->state8PersistenceDataF1c.secondary4c.begin(), g_CurrentLoginMediator->state8PersistenceDataF1c.secondary4c.end(), 0u);
        std::fill(g_CurrentLoginMediator->characterRecordPointersF88.begin(), g_CurrentLoginMediator->characterRecordPointersF88.end(), 0u);
        std::fill(g_CurrentLoginMediator->section0StringF8c.begin(), g_CurrentLoginMediator->section0StringF8c.end(), '\0');
        std::fill(g_CurrentLoginMediator->section0StringFac.begin(), g_CurrentLoginMediator->section0StringFac.end(), '\0');
        std::fill(g_CurrentLoginMediator->section0StringFcc.begin(), g_CurrentLoginMediator->section0StringFcc.end(), '\0');
        std::fill(g_CurrentLoginMediator->state8Section0RawF88.begin(), g_CurrentLoginMediator->state8Section0RawF88.end(), 0u);
        g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b0 = 0u;
        g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b4 = 0u;
        g_CurrentLoginMediator->state8PersistenceDataF1c.section0PresentFlag4da = 0u;
                if (g_CurrentLoginMediator->state8Section0OverflowBuffer13f0) {
            std::free(g_CurrentLoginMediator->state8Section0OverflowBuffer13f0);
            g_CurrentLoginMediator->state8Section0OverflowBuffer13f0 = nullptr;
        }
        g_CurrentLoginMediator->state8Section0OverflowLength13f4 = 0u;
        g_CurrentLoginMediator->state8PersistenceDataF1c.section0PresentFlag4da = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer13f8) {
            std::free(g_CurrentLoginMediator->allocatedBuffer13f8);
            g_CurrentLoginMediator->allocatedBuffer13f8 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength13fc = 0u;
        g_CurrentLoginMediator->flag13fe = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1400) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1400);
            g_CurrentLoginMediator->allocatedBuffer1400 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1404 = 0u;
        g_CurrentLoginMediator->flag1406 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1408) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1408);
            g_CurrentLoginMediator->allocatedBuffer1408 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength140c = 0u;
        g_CurrentLoginMediator->allocatedBufferFlag140e = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1410) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1410);
            g_CurrentLoginMediator->allocatedBuffer1410 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1414 = 0u;
        g_CurrentLoginMediator->flag1416 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1418) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1418);
            g_CurrentLoginMediator->allocatedBuffer1418 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength141c = 0u;
        g_CurrentLoginMediator->allocatedBufferFlag141e = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1420) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1420);
            g_CurrentLoginMediator->allocatedBuffer1420 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1424 = 0u;
        g_CurrentLoginMediator->allocatedBufferFlag1426 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1428) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1428);
            g_CurrentLoginMediator->allocatedBuffer1428 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength142c = 0u;
        g_CurrentLoginMediator->allocatedBufferFlag142e = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1430) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1430);
            g_CurrentLoginMediator->allocatedBuffer1430 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1434 = 0u;
        g_CurrentLoginMediator->flag1436 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1438) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1438);
            g_CurrentLoginMediator->allocatedBuffer1438 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength143c = 0u;
        g_CurrentLoginMediator->flag143e = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1440) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1440);
            g_CurrentLoginMediator->allocatedBuffer1440 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1444 = 0u;
        g_CurrentLoginMediator->flag1448 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer144c) {
            std::free(g_CurrentLoginMediator->allocatedBuffer144c);
            g_CurrentLoginMediator->allocatedBuffer144c = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1450 = 0u;
        g_CurrentLoginMediator->flag1452 = 0u;
                if (g_CurrentLoginMediator->allocatedBuffer1454) {
            std::free(g_CurrentLoginMediator->allocatedBuffer1454);
            g_CurrentLoginMediator->allocatedBuffer1454 = nullptr;
        }
        g_CurrentLoginMediator->allocatedBufferLength1458 = 0u;
        g_CurrentLoginMediator->flag145a = 0u;
        g_State8Section10ChunkBitmap_0x4f79e4 = 0u;
        g_CurrentLoginMediator->state8Section11Dword145c = 0u;
        g_CurrentLoginMediator->state8Section11String1460.clear();

        g_CurrentLoginMediator->state8PersistenceDataF1c.replyField20 = loadCharacterReplyEnvelope.field05;

        const Packet_AsAuthReply_0x4b5328* currentSlotRecord =
            g_CurrentLoginMediator->GetCurrentAuthReplyPacket44();
        if (currentSlotRecord == nullptr) {
            spdlog::info(
                "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage first-fragment invariant failed: currentSlotRecord is null; original 0x43f930 dereferences owner vtable +0x44 result directly here");
            return 0u;
        }

        if (currentSlotRecord->debugString14) {
            const size_t copyCount = std::min(
                std::strlen(currentSlotRecord->debugString14),
                sizeof(g_CurrentLoginMediator->characterNameBufferF1c) - 1u);
            std::copy_n(
                currentSlotRecord->debugString14,
                copyCount,
                g_CurrentLoginMediator->characterNameBufferF1c);
            g_CurrentLoginMediator->characterNameBufferF1c[copyCount] = '\0';
            std::copy(
                g_CurrentLoginMediator->characterNameBufferF1c,
                g_CurrentLoginMediator->characterNameBufferF1c + sizeof(g_CurrentLoginMediator->characterNameBufferF1c),
                g_CurrentLoginMediator->state8PersistenceDataF1c.characterName00.begin());
        }
        g_CurrentLoginMediator->state8PersistenceDataF1c.selectedWorldField24 = currentSlotRecord->worldId24;
        g_CurrentLoginMediator->state8PersistenceDataF1c.secondary4c[0] = currentSlotRecord->worldId24;
        g_CurrentLoginMediator->state8PersistenceDataF1c.secondary4c[1] = currentSlotRecord->packetType1a;
        usedCurrentSlotRecord = true;
    }

    if (loadCharacterReplyEnvelope.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = loadCharacterReplyEnvelope.expectedSectionCount0b;
    }

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
                    g_CurrentLoginMediator->state8PersistenceDataF1c.header2c.size() * sizeof(uint32_t));
                if (fixedPrefixBytes != 0u) {
                    std::memcpy(g_CurrentLoginMediator->state8PersistenceDataF1c.header2c.data(), loadCharacterReplyEnvelope.sectionData, fixedPrefixBytes);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x20u) {
                    if (g_CurrentLoginMediator->state8Section0RawF88.size() != 0u) {
                        std::memset(g_CurrentLoginMediator->state8Section0RawF88.data(), 0, g_CurrentLoginMediator->state8Section0RawF88.size());
                        std::memcpy(
                            g_CurrentLoginMediator->state8Section0RawF88.data(),
                            loadCharacterReplyEnvelope.sectionData + 0x20u,
                            std::min(
                                g_CurrentLoginMediator->state8Section0RawF88.size(),
                                loadCharacterReplyEnvelope.sectionByteCount - 0x20u));
                    }
                    g_CurrentLoginMediator->state8PersistenceDataF1c.bodyWord6c = 0x1000u;
                    std::fill(g_CurrentLoginMediator->state8PersistenceDataF1c.realFirstName70.begin(), g_CurrentLoginMediator->state8PersistenceDataF1c.realFirstName70.end(), '\0');
                    std::fill(g_CurrentLoginMediator->state8PersistenceDataF1c.realLastName90.begin(), g_CurrentLoginMediator->state8PersistenceDataF1c.realLastName90.end(), '\0');
                    std::fill(g_CurrentLoginMediator->state8PersistenceDataF1c.backgroundB0.begin(), g_CurrentLoginMediator->state8PersistenceDataF1c.backgroundB0.end(), '\0');
                    g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b0 = 0u;
                    g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b4 = 0u;
                    g_CurrentLoginMediator->state8PersistenceDataF1c.tail4b8 = {1u};
                    if (CLTLoginMediatorCharacterPersistenceData_0x41d900::kBodySize != 0u) {
                        std::memset(
                            reinterpret_cast<uint8_t*>(&g_CurrentLoginMediator->state8PersistenceDataF1c.bodyWord6c),
                            0,
                            CLTLoginMediatorCharacterPersistenceData_0x41d900::kBodySize);
                        std::memcpy(
                            reinterpret_cast<uint8_t*>(&g_CurrentLoginMediator->state8PersistenceDataF1c.bodyWord6c),
                            loadCharacterReplyEnvelope.sectionData + 0x20u,
                            std::min(
                                CLTLoginMediatorCharacterPersistenceData_0x41d900::kBodySize,
                                loadCharacterReplyEnvelope.sectionByteCount - 0x20u));
                    }
                }
                if (loadCharacterReplyEnvelope.sectionByteCount >= 4u) {
                    g_CurrentLoginMediator->characterRecordPointersF88[0] = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x00u);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x444u) {
                    g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b0 = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x444u);
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x448u) {
                    g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b4 = ReadU32LE(loadCharacterReplyEnvelope.sectionData + 0x448u);
                }
                if (g_CurrentLoginMediator->section0StringF8c.size() != 0u) {
                    std::fill(g_CurrentLoginMediator->section0StringF8c.data(), g_CurrentLoginMediator->section0StringF8c.data() + g_CurrentLoginMediator->section0StringF8c.size(), '\0');
                    const uint8_t* section0StringF8cSrc = loadCharacterReplyEnvelope.sectionByteCount > 0x04u ? (loadCharacterReplyEnvelope.sectionData + 0x04u) : nullptr;
                    const size_t section0StringF8cSrcAvail = loadCharacterReplyEnvelope.sectionByteCount > 0x04u ? loadCharacterReplyEnvelope.sectionByteCount - 0x04u : 0u;
                    if (section0StringF8cSrc && section0StringF8cSrcAvail != 0u) {
                        size_t copyLen = 0u;
                        while (copyLen + 1u < g_CurrentLoginMediator->section0StringF8c.size() && copyLen < section0StringF8cSrcAvail && section0StringF8cSrc[copyLen] != '\0') {
                            g_CurrentLoginMediator->section0StringF8c.data()[copyLen] = static_cast<char>(section0StringF8cSrc[copyLen]);
                            ++copyLen;
                        }
                        g_CurrentLoginMediator->section0StringF8c.data()[copyLen] = '\0';
                    }
                }
                if (g_CurrentLoginMediator->section0StringFac.size() != 0u) {
                    std::fill(g_CurrentLoginMediator->section0StringFac.data(), g_CurrentLoginMediator->section0StringFac.data() + g_CurrentLoginMediator->section0StringFac.size(), '\0');
                    const uint8_t* section0StringFacSrc = loadCharacterReplyEnvelope.sectionByteCount > 0x24u ? (loadCharacterReplyEnvelope.sectionData + 0x24u) : nullptr;
                    const size_t section0StringFacSrcAvail = loadCharacterReplyEnvelope.sectionByteCount > 0x24u ? loadCharacterReplyEnvelope.sectionByteCount - 0x24u : 0u;
                    if (section0StringFacSrc && section0StringFacSrcAvail != 0u) {
                        size_t copyLen = 0u;
                        while (copyLen + 1u < g_CurrentLoginMediator->section0StringFac.size() && copyLen < section0StringFacSrcAvail && section0StringFacSrc[copyLen] != '\0') {
                            g_CurrentLoginMediator->section0StringFac.data()[copyLen] = static_cast<char>(section0StringFacSrc[copyLen]);
                            ++copyLen;
                        }
                        g_CurrentLoginMediator->section0StringFac.data()[copyLen] = '\0';
                    }
                }
                if (g_CurrentLoginMediator->section0StringFcc.size() != 0u) {
                    std::fill(g_CurrentLoginMediator->section0StringFcc.data(), g_CurrentLoginMediator->section0StringFcc.data() + g_CurrentLoginMediator->section0StringFcc.size(), '\0');
                    const uint8_t* section0StringFccSrc = loadCharacterReplyEnvelope.sectionByteCount > 0x44u ? (loadCharacterReplyEnvelope.sectionData + 0x44u) : nullptr;
                    const size_t section0StringFccSrcAvail = loadCharacterReplyEnvelope.sectionByteCount > 0x44u ? loadCharacterReplyEnvelope.sectionByteCount - 0x44u : 0u;
                    if (section0StringFccSrc && section0StringFccSrcAvail != 0u) {
                        size_t copyLen = 0u;
                        while (copyLen + 1u < g_CurrentLoginMediator->section0StringFcc.size() && copyLen < section0StringFccSrcAvail && section0StringFccSrc[copyLen] != '\0') {
                            g_CurrentLoginMediator->section0StringFcc.data()[copyLen] = static_cast<char>(section0StringFccSrc[copyLen]);
                            ++copyLen;
                        }
                        g_CurrentLoginMediator->section0StringFcc.data()[copyLen] = '\0';
                    }
                }
                if (loadCharacterReplyEnvelope.sectionByteCount > 0x485u && g_CurrentLoginMediator->state8Section0OverflowBuffer13f0 == nullptr) {
                    const uint16_t overflowAppendLen =
                        static_cast<uint16_t>(loadCharacterReplyEnvelope.sectionByteCount - 0x485u);
                    void* const newBuffer = std::malloc(overflowAppendLen);
                    if (newBuffer != nullptr) {
                        std::memcpy(
                            newBuffer,
                            loadCharacterReplyEnvelope.sectionData + 0x485u,
                            overflowAppendLen);
                        g_CurrentLoginMediator->state8Section0OverflowBuffer13f0 = newBuffer;
                        g_CurrentLoginMediator->state8Section0OverflowLength13f4 = overflowAppendLen;
                    }
                }
            }
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0OverflowBuffer4d4 = g_CurrentLoginMediator->state8Section0OverflowBuffer13f0;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0OverflowLength4d8 = g_CurrentLoginMediator->state8Section0OverflowLength13f4;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0PresentFlag4da = 1u;
            spdlog::info(
                "CLTLoginState_State8_0x4b5104 section0 parsed name='{}' first='{}' last='{}' background='{}' ptr0=0x{:08x} extra13cc=0x{:08x} extra13d0=0x{:08x}",
                g_CurrentLoginMediator->characterNameBufferF1c[0] ? std::string(g_CurrentLoginMediator->characterNameBufferF1c) : std::string("<empty>"),
                g_CurrentLoginMediator->section0StringF8c[0] ? std::string(g_CurrentLoginMediator->section0StringF8c.data()) : std::string("<empty>"),
                g_CurrentLoginMediator->section0StringFac[0] ? std::string(g_CurrentLoginMediator->section0StringFac.data()) : std::string("<empty>"),
                g_CurrentLoginMediator->section0StringFcc[0] ? std::string(g_CurrentLoginMediator->section0StringFcc.data()) : std::string("<empty>"),
                static_cast<unsigned>(g_CurrentLoginMediator->characterRecordPointersF88[0]),
                static_cast<unsigned>(g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b0),
                static_cast<unsigned>(g_CurrentLoginMediator->state8PersistenceDataF1c.replySectionData4b4));
            break;
        case 1u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength13fc;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer13f8
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer13f8, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer13f8 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength13fc = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag13fe = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section01Buffer4dc = g_CurrentLoginMediator->allocatedBuffer13f8;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section01Length4e0 = g_CurrentLoginMediator->allocatedBufferLength13fc;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section01PresentFlag4e2 = 1u;
            break;
        case 2u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1404;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1400
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1400, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1400 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1404 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag1406 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section02Buffer4e4 = g_CurrentLoginMediator->allocatedBuffer1400;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section02Length4e8 = g_CurrentLoginMediator->allocatedBufferLength1404;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section02PresentFlag4ea = 1u;
            break;
        case 3u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength141c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1418
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1418, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1418 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength141c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->allocatedBufferFlag141e = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section03Buffer4fc = g_CurrentLoginMediator->allocatedBuffer1418;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section03Length500 = g_CurrentLoginMediator->allocatedBufferLength141c;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section03PresentFlag502 = 1u;
            break;
        case 4u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1424;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1420
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1420, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1420 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1424 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->allocatedBufferFlag1426 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section04Buffer504 = g_CurrentLoginMediator->allocatedBuffer1420;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section04Length508 = g_CurrentLoginMediator->allocatedBufferLength1424;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section04PresentFlag50a = 1u;
            break;
        case 5u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength142c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1428
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1428, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1428 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength142c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->allocatedBufferFlag142e = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section05Buffer50c = g_CurrentLoginMediator->allocatedBuffer1428;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section05Length510 = g_CurrentLoginMediator->allocatedBufferLength142c;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section05PresentFlag512 = 1u;
            break;
        case 6u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength140c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1408
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1408, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1408 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength140c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->allocatedBufferFlag140e = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section06Buffer4ec = g_CurrentLoginMediator->allocatedBuffer1408;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section06Length4f0 = g_CurrentLoginMediator->allocatedBufferLength140c;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section06PresentFlag4f2 = 1u;
            break;
        case 7u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1414;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1410
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1410, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1410 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1414 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag1416 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section07Buffer4f4 = g_CurrentLoginMediator->allocatedBuffer1410;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section07Length4f8 = g_CurrentLoginMediator->allocatedBufferLength1414;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section07PresentFlag4fa = 1u;
            break;
        case 8u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1444;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1440
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1440, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1440 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1444 = static_cast<uint32_t>(newLength);
                }
            }
            g_CurrentLoginMediator->flag1448 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section08Buffer524 = g_CurrentLoginMediator->allocatedBuffer1440;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section08Length528 = g_CurrentLoginMediator->allocatedBufferLength1444;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section08PresentFlag52c = 1u;
            break;
        case 9u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1450;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer144c
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer144c, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer144c = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1450 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag1452 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section09Buffer530 = g_CurrentLoginMediator->allocatedBuffer144c;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section09Length534 = g_CurrentLoginMediator->allocatedBufferLength1450;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section09PresentFlag536 = 1u;
            break;
        case 10u:
            // anchor: launcher.exe:0x43ffc6
            // Exact current read from `0x43f930`:
            // - section selector `0x0a` uses packet byte `+0x0b` as a 1-based chunk ordinal
            // - allocate a fixed 0x7d00-byte backing buffer on first sight
            // - copy chunk bytes to `buffer + ((ordinal * 1000) - 1000)`
            // - mark that 1-based ordinal as seen in the helper-local bitset
            // - accumulate total materialized byte count in owner `+0x1458`
            if (g_CurrentLoginMediator->allocatedBuffer1454 == nullptr) {
                g_CurrentLoginMediator->allocatedBuffer1454 = std::malloc(0x7d00u);
                g_CurrentLoginMediator->allocatedBufferLength1458 = 0u;
                g_State8Section10ChunkBitmap_0x4f79e4 = 0u;
            }
            if (g_CurrentLoginMediator->allocatedBuffer1454 != nullptr && loadCharacterReplyEnvelope.sectionData != nullptr) {
                if (loadCharacterReplyEnvelope.expectedSectionCount0b == 0u) {
                    spdlog::info(
                        "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage section0x0a invariant failed: packet byte +0x0b chunk ordinal is zero");
                } else {
                    const uint32_t chunkIndex = static_cast<uint32_t>(loadCharacterReplyEnvelope.expectedSectionCount0b - 1u);
                    const size_t chunkOffset = static_cast<size_t>(chunkIndex) * 1000u;
                    if (chunkOffset + loadCharacterReplyEnvelope.sectionByteCount <= 0x7d00u) {
                        std::memcpy(
                            static_cast<uint8_t*>(g_CurrentLoginMediator->allocatedBuffer1454) + chunkOffset,
                            loadCharacterReplyEnvelope.sectionData,
                            loadCharacterReplyEnvelope.sectionByteCount);
                        StateReplyChunkBitset_0x43c240(g_State8Section10ChunkBitmap_0x4f79e4).SetSeen(chunkIndex);
                        g_CurrentLoginMediator->allocatedBufferLength1458 = static_cast<uint16_t>(
                            g_CurrentLoginMediator->allocatedBufferLength1458 + loadCharacterReplyEnvelope.sectionByteCount);
                    }
                }
            }
            g_CurrentLoginMediator->flag145a = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0aChunkedBuffer538 = g_CurrentLoginMediator->allocatedBuffer1454;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0aChunkedLength53c = g_CurrentLoginMediator->allocatedBufferLength1458;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0aPresentFlag53e = 1u;
            break;
        case 11u:
            // anchor: launcher.exe:0x43f8c0 = CLTLoginMediatorCharacterPersistenceData_ApplySection11SideEffect
            // Exact helper behavior:
            // - if byteCount > 4, store leading dword and assign range [data+4, data+byteCount)
            // - else zero dword and, if the current string is non-empty, write a terminating NUL at
            //   begin and collapse current back to begin
            g_CurrentLoginMediator->state8Section11Dword145c = 0u;
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount > 4u) {
                g_CurrentLoginMediator->state8Section11Dword145c = ReadU32LE(loadCharacterReplyEnvelope.sectionData);
                g_CurrentLoginMediator->state8Section11String1460.assign(
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + 4u),
                    reinterpret_cast<const char*>(loadCharacterReplyEnvelope.sectionData + loadCharacterReplyEnvelope.sectionByteCount));
            } else if (!g_CurrentLoginMediator->state8Section11String1460.empty()) {
                g_CurrentLoginMediator->state8Section11String1460[0] = '\0';
                g_CurrentLoginMediator->state8Section11String1460.resize(0u);
            } else {
                g_CurrentLoginMediator->state8Section11String1460.clear();
            }
            g_CurrentLoginMediator->state8PersistenceDataF1c.section11Dword540 = g_CurrentLoginMediator->state8Section11Dword145c;
            {
                char* const section11Begin = g_CurrentLoginMediator->state8Section11String1460.data();
                g_CurrentLoginMediator->state8PersistenceDataF1c.section11StringBegin544 = section11Begin;
                g_CurrentLoginMediator->state8PersistenceDataF1c.section11StringCurrent548 =
                    section11Begin + g_CurrentLoginMediator->state8Section11String1460.size();
                g_CurrentLoginMediator->state8PersistenceDataF1c.section11StringCapacity54c =
                    section11Begin + g_CurrentLoginMediator->state8Section11String1460.capacity();
            }
            spdlog::info(
                "CLTLoginState_State8_0x4b5104::Slot6_HandleSecondaryMessage applied section 0x0b side effect dword145c=0x{:08x} string1460Len={}",
                static_cast<unsigned>(g_CurrentLoginMediator->state8Section11Dword145c),
                static_cast<unsigned>(g_CurrentLoginMediator->state8Section11String1460.size()));
            break;
        case 12u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength1434;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1430
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1430, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1430 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength1434 = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag1436 = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0cBuffer514 = g_CurrentLoginMediator->allocatedBuffer1430;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0cLength518 = g_CurrentLoginMediator->allocatedBufferLength1434;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0cPresentFlag51a = 1u;
            break;
        case 13u:
            if (loadCharacterReplyEnvelope.sectionData != nullptr && loadCharacterReplyEnvelope.sectionByteCount != 0u) {
                const size_t oldLength = g_CurrentLoginMediator->allocatedBufferLength143c;
                const size_t newLength = oldLength + loadCharacterReplyEnvelope.sectionByteCount;
                void* newBuffer = g_CurrentLoginMediator->allocatedBuffer1438
                    ? std::realloc(g_CurrentLoginMediator->allocatedBuffer1438, newLength)
                    : std::malloc(newLength);
                if (newBuffer != nullptr) {
                    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, loadCharacterReplyEnvelope.sectionData, loadCharacterReplyEnvelope.sectionByteCount);
                    g_CurrentLoginMediator->allocatedBuffer1438 = newBuffer;
                    g_CurrentLoginMediator->allocatedBufferLength143c = static_cast<uint16_t>(newLength & 0xffffu);
                }
            }
            g_CurrentLoginMediator->flag143e = 1u;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0dBuffer51c = g_CurrentLoginMediator->allocatedBuffer1438;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0dLength520 = g_CurrentLoginMediator->allocatedBufferLength143c;
            g_CurrentLoginMediator->state8PersistenceDataF1c.section0dPresentFlag522 = 1u;
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
 static_cast<unsigned>(g_CurrentLoginMediator->section0Flag13f6),
 static_cast<unsigned>(g_CurrentLoginMediator->flag145a),
 static_cast<unsigned>(g_State8Section10ChunkBitmap_0x4f79e4));

 // anchor: launcher.exe:0x4408ed completion check
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
 static_cast<unsigned>(g_CurrentLoginMediator->section0Flag13f6),
 static_cast<unsigned>(g_CurrentLoginMediator->flag145a),
 static_cast<unsigned>(g_State8Section10ChunkBitmap_0x4f79e4));

 if (completed) {
        if (g_CurrentLoginMediator->allocatedBuffer1454 != nullptr) {
            size_t firstChunkIndex = 0u;
            while (firstChunkIndex < 32u &&
                   !StateReplyChunkBitset_0x43c240(g_State8Section10ChunkBitmap_0x4f79e4).HasSeen(firstChunkIndex)) {
                ++firstChunkIndex;
            }
            if (firstChunkIndex < 32u) {
                std::memmove(
                    g_CurrentLoginMediator->allocatedBuffer1454,
                    static_cast<uint8_t*>(g_CurrentLoginMediator->allocatedBuffer1454) + (firstChunkIndex * 1000u),
                    g_CurrentLoginMediator->allocatedBufferLength1458);
            }
            if (void* compacted = std::realloc(g_CurrentLoginMediator->allocatedBuffer1454, g_CurrentLoginMediator->allocatedBufferLength1458)) {
                g_CurrentLoginMediator->allocatedBuffer1454 = compacted;
            }
        }

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
        g_State8Section10ChunkBitmap_0x4f79e4 = 0u;
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

// anchor: launcher.exe:0x438c90 (vtable 0x4b5104 slot 7)
uint32_t CLTLoginState_State8_0x4b5104::GetStateId() const {
    return 8;
}

}  // namespace mxo::ltlogin
