#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace mxo::ltlogin {
namespace {

}  // namespace

// anchor: launcher.exe vtable 0x4b512c
const char* CLTLoginState_State10_0x4b512c::DebugName() const {
    return "CLTLoginState_State10_0x4b512c";
}

// anchor: launcher.exe:0x43bf90 (vtable 0x4b512c slot 3)
void CLTLoginState_State10_0x4b512c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    if (!g_CurrentLoginMediator) {
        return;
    }

    // Fresh `0x43bf90` read from decompilation + disassembly:
    // - sends raw margin opcode `0x0a`
    //   - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //     `MS_ClaimCharacterNameRequest`
    // - precheck owner `+0x1c` connection state through `0x41b4b0`
    //   - on failure, original switches helper state to `4`
    // - then check owner byte `+0xf14`
    //   - on zero, original switches helper state to `6`
    // - initialize local packet-builder family `0x43a1f0`
    // - copy owner `+0x108` (`CharacterName`) through `0x43aa80`
    // - send through `0x41af70`
    // - post event `0x13`
    if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10_0x4b512c::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }
    if (g_CurrentLoginMediator->state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10_0x4b512c::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }

    // anchor: launcher.exe:0x43a1f0 = Packet_AsAuthChallengeResponse_0x4b53b4::ResetAndInitialize
    Packet_AsAuthChallengeResponse_0x4b53b4 packetBuilder;
    packetBuilder.ResetAndInitialize();

    // anchor: launcher.exe:0x43aa80 = SetCharacterName (mediator helper)
    // Implement reservation inline for source fidelity
    const char* characterName = g_CurrentLoginMediator->createCharacterData108.characterName00.data();
    uint8_t* payload = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
    if (payload && characterName && packetBuilder.reservation14_.reservedContentByteCount04 == 0u) {
        size_t textLen = 0;
        const char* p = characterName;
        while (*p++) ++textLen;
        ++textLen;

        if (packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c) {
            auto* storage = packetBuilder.messageRef08->messageStorage0c;
            const uint16_t currentSize = storage->PayloadByteCount();
            const uint16_t remaining = storage->RemainingAppendableByteCount();

            if (remaining >= 2u + textLen) {
                const uint16_t growth = static_cast<uint16_t>(2u + textLen);
                const uint16_t newSize = storage->GrowPayloadByteCount(growth);

                if (newSize == currentSize + growth) {
                    uint8_t* lengthPrefix = payload + currentSize;
                    lengthPrefix[0] = static_cast<uint8_t>(textLen & 0xffu);
                    lengthPrefix[1] = static_cast<uint8_t>((textLen >> 8) & 0xffu);

                    const uint16_t offset = currentSize;
                    *reinterpret_cast<uint16_t*>(payload + State10Packet0x0aFixedPayload::kCharacterNameOffset) = offset;

                    if (textLen > 1u) {
                        std::copy_n(characterName, textLen - 1u, lengthPrefix + 2u);
                    }
                    if (textLen > 0u) {
                        lengthPrefix[2u + textLen - 1u] = '\0';
                    }

                    packetBuilder.reservation14_.writePointer00 = lengthPrefix + 2u;
                    packetBuilder.reservation14_.reservedContentByteCount04 = static_cast<uint16_t>(textLen);
                }
            }
        }
    }

    // Build envelope for send
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
    envelope.payloadBase04 = payload;
    envelope.messageRef08 = packetBuilder.messageRef08;
    (void)g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
    g_CurrentLoginMediator->PostEvent(0x13u);

    const uint16_t totalBytes = packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c
        ? packetBuilder.messageRef08->messageStorage0c->PayloadByteCount() : 0u;

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10_0x4b512c::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x{:02x} totalBytes=0x{:02x} CharacterName='{}' -> postEvent=0x13",
        State10Packet0x0aFixedPayload::kFixedByteCount,
        totalBytes,
        std::string(g_CurrentLoginMediator->createCharacterData108.characterName00.data()));
    return;
}

// anchor: launcher.exe:0x4401a0 (vtable 0x4b512c slot 6)
uint32_t CLTLoginState_State10_0x4b512c::Slot6_HandleSecondaryMessage(
        mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* messageRef) {
    if (!g_CurrentLoginMediator || !messageRef) {
        return 0u;
    }
    // anchor: launcher.exe:0x4401a0
    // Original at 0x4401aa: CALL 0x41bc20 (CMessageConnectionMessageRef_DecodeMessageCode)
    // returns u16 opcode directly. At 0x4401b3: CMP AX,0xb; JNZ reject_path.
    // No separate decode-failure branch — a failed decode returns 0, which != 0xb,
    // so it takes the same reject path. The scaffold wraps this into a bool+out-param
    // pattern; keep the scaffold for consistency but merge the failure branch into
    // the single reject path below.
    uint16_t messageCode = 0;
    if (!CMessageConnection_0x4b7928_DecodeMessageCode(*messageRef, &messageCode, nullptr)) {
        // Decode failed — original would just see opcode 0 (≠ 0xb) and go to reject.
        // Fall through to the single reject path below.
    }

    // Fidelity correction from direct `0x43bf90 / 0x4401a0 / 0x41bf70` review:
    // - state10 slot 3 sends raw margin opcode `0x0a = MS_ClaimCharacterNameRequest`
    // - state10 slot 6 therefore consumes the matching raw margin opcode
    //   `0x0b = MS_ClaimCharacterNameReply`, not an auth-channel `AS_AuthReply`
    // - success appends exactly one new slot record under owner `+0x688/+0x818`, sets owner
    //   `+0xcc8 = currentCount`, then switches to helper11 and posts event `0x14`
    // - immediate continuation after that success is:
    //   `state11 slot3 / 0x43c020 -> state11 slot6 / 0x440320 -> helper9 slot3 / 0x439780
    //    -> owner 0x41de40`
    //
    // Fidelity note on return value: original reject path at 0x4402fc-0x44030b
    // returns (mediator_ptr & 0xFFFFFF00) — non-zero. Source returns 0u for now.
    // If the caller distinguishes non-zero returns, this will need revisiting.
    if (messageCode != 0x0bu) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State10_0x4b512c::Slot6_HandleSecondaryMessage rejected messageCode=0x{:04x} (expected 0x0b); mirrored original owner+0x80=0x12000005",
            static_cast<unsigned>(messageCode));
        return 0u;
    }

    // anchor: launcher.exe:0x4401a0
    // Original at 0x4401c0: CALL 0x43a330 (State10ClaimCharacterNameReplyParseObject_InitFromIncomingPacket)
    // with LEA ECX,[EBP-0x28]; PUSH 1; PUSH ESI (messageRef). This constructs a parse
    // object on the stack directly from the raw CMessageConnectionMessageRef_0x4ba23c*, not from
    // extracted payload bytes. The parse object has a virtual destructor cleaned up at
    // 0x4402e7-0x4402f0: TEST ECX,ECX; JZ skip; MOV EDX,[ECX]; CALL [EDX+8].
    //
    // Faithful source-owned replacement: model the same stack parse object family instead of
    // parsing raw payload bytes through an ad-hoc scaffold. Ghidra currently types the object as
    // `Packet_MsCreateCharacter_0x4b53c8 *` at `0x43a330`, but source keeps a distinct class name
    // because the same vtable family is already used by the state11 create-character request builder.
    Packet_MsCreateCharacterRequest_0x4b53c8 parsed(messageRef, 1);
    if (!parsed.valid) {
        const uint16_t payloadByteCount = messageRef->PayloadByteCount();
        spdlog::warn(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply parse rejected payload bytes={} (expected >= 0x0f-byte MS_ClaimCharacterNameReply layout)",
            static_cast<unsigned>(payloadByteCount));
        return 0u;
    }

    // anchor: launcher.exe:0x4401cb-0x4401d4
    // Original: MOV ECX,[EAX+3]; MOV [EDX+0x80],ECX; CMP [EAX+3],1; JGE error_path
    // Status check is BEFORE allocation. If status >= 1, jump to error path at 0x4402cd.
    g_CurrentLoginMediator->worldListCountOrStatus80 = parsed.status;

    if (parsed.status >= 1u) {
        // Error path: SetCurrentState(3), PostError(0xb) — matches 0x4402cd-0x4402e2
        (void)g_CurrentLoginMediator->SetCurrentState(3u);
        g_CurrentLoginMediator->PostError(0x0bu);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10_0x4b512c::Slot6_HandleSecondaryMessage observed error MS_ClaimCharacterNameReply; mirrored original state3 switch and error=0x0b owner+0x80=0x{:08x}",
            static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80));
        return 1u;
    }

    // Success path (status < 1): allocate and populate new slot record
    //
    // Fidelity note on allocation model:
    // Original at 0x4401ec-0x440228: PUSH 0x1c; CALL 0x403260 (TrackedMalloc);
    // CALL 0x4398b0 (SlotRecord_Initialize); then stores the pointer at
    // [ESI + count*4 + 4] (pointer table at +0x688). Current source uses an
    // inline std::array<Packet_AsAuthReply_0x4b5328, N> instead of a pointer table,
    // so the TrackedMalloc+Init is approximated by value-initialization.
    const uint8_t appendedSlotIndex = g_CurrentLoginMediator->selectionRouteState684_.slotRecordCount00_;
    const uint32_t selectedWorldDescriptorIndex =
        g_CurrentLoginMediator->createCharacterData108.selectedWorldField24;
    if (appendedSlotIndex >= g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_.size() ||
        appendedSlotIndex >= g_CurrentLoginMediator->selectionRouteState684_.routeHostStrings194_.size() ||
        selectedWorldDescriptorIndex >= g_CurrentLoginMediator->worldListPacketCountD80_ ||
        selectedWorldDescriptorIndex >= g_CurrentLoginMediator->worldListPacketsD84_.size() ||
        g_CurrentLoginMediator->worldListPacketsD84_[selectedWorldDescriptorIndex] == nullptr) {
        spdlog::warn(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply success could not append slot record appendedSlotIndex={} selectedWorldDescriptorIndex={} worldDescriptorCount=0x{:02x}",
            static_cast<unsigned>(appendedSlotIndex),
            static_cast<unsigned>(selectedWorldDescriptorIndex),
            static_cast<unsigned>(g_CurrentLoginMediator->worldListPacketCountD80_));
        return 0u;
    }

    const Packet_WorldList_0x4b533c& selectedWorldDescriptor =
        *g_CurrentLoginMediator->worldListPacketsD84_[selectedWorldDescriptorIndex];

    // anchor: launcher.exe:0x4401a0
    // Exact success-side write order recovered from listing:
    // 1. store initialized slot record at +0x688[currentCount]  (0x440228)
    // 2. copy selected descriptor inline name into +0x818[currentCount]  (0x44022c-0x44024d)
    //    via CopyInlineNameToString(0x43d430) + the `0x407dd0` basic-string assign-from-range helper
    // 3. free temp inline-name copy string  (0x440252-0x440265)
    // 4. set +0x644 = currentCount  (0x440268-0x44026a)
    // 5. increment +0x00 (count)  (0x440270-0x440272)
    // 6. SetCharacterName from createCharacterData  (0x440274-0x440283)
    // 7. write character IDs / status / worldId  (0x440288-0x4402ab)
    // 8. SetCurrentState(0xb)  (0x4402af-0x4402b7)
    // 9. PostEvent(0x14)  (0x4402bc-0x4402c4)
    //
    // Note: original has no slotRecordValid04_ write — just the pointer store at 0x440228.
    g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex] = {};
    g_CurrentLoginMediator->selectionRouteState684_.routeHostStrings194_[appendedSlotIndex] =
        selectedWorldDescriptor.inlineNamePlus03;
    // anchor: launcher.exe:0x440268-0x440272
    // Static RE shows a direct write to owner +0xcc8 / selectionRouteState684_.currentSlotOrSelectionIndex644_
    // followed by incrementing the slot count. Do not use the broader scaffold helper here because
    // `0x4401a0` does not show the extra mirrored writes to owner +0xcc8 sidecars.
    g_CurrentLoginMediator->selectionRouteState684_.SetCurrentSlotOrSelectionIndex644(appendedSlotIndex);
    g_CurrentLoginMediator->selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(appendedSlotIndex + 1u);

    // anchor: launcher.exe:0x440274-0x440283
    // Original: PUSH &mediator->createCharacterData108; MOV ECX,pNewSlotRecord;
    // CALL 0x43aa80 (SetCharacterName). This copies the character name into the slot
    // record's payload buffer via ReserveLengthPrefixedString + WriteReservedCString.
    // Current code approximates this by setting the debugString14 pointer directly.
    // TODO: Once slot records are heap-allocated with payload buffers, replace with
    // a proper SetCharacterName call.
    Packet_AsAuthReply_0x4b5328& appendedSlotRecord =
        g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex];
    appendedSlotRecord.debugString14 =
        g_CurrentLoginMediator->createCharacterData108.characterName00.data();

    // anchor: launcher.exe:0x440288-0x4402ab
    // Character ID writes: [EDI+0x10]+0x3 = parsed.+7, [EDI+0x10]+0x7 = parsed.+0xb,
    // [EDI+0x10]+0xb = 0 (status), [EDI+0x10]+0xc = worldId (word from descriptor)
    appendedSlotRecord.characterIdLow1c = parsed.characterIdLow;
    appendedSlotRecord.characterIdHigh20 = parsed.characterIdHigh;
    appendedSlotRecord.packetType1a = 0u;
    appendedSlotRecord.worldId24 = selectedWorldDescriptor.worldId01;

    // anchor: launcher.exe:0x4402af-0x4402c4
    // Original: SetCurrentState(g_Mediator, 0xb) then PostEvent(g_Mediator, 0x14).
    // No helper-state null-check, no expectedMarginRequestName_ write, no marginRouteState_
    // writes. The parse-object virtual destructor runs at 0x4402e7-0x4402f0 before return.
    (void)g_CurrentLoginMediator->SetCurrentState(0x0bu);
    g_CurrentLoginMediator->PostEvent(0x14u);
    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10_0x4b512c::Slot6_HandleSecondaryMessage successful MS_ClaimCharacterNameReply -> SetCurrentState(0xb) then PostEvent(0x14)");
    return 1u;
}

// anchor: launcher.exe:0x438ca0 (vtable 0x4b512c slot 7)
uint32_t CLTLoginState_State10_0x4b512c::GetStateId() const {
    return 10;
}

}  // namespace mxo::ltlogin
