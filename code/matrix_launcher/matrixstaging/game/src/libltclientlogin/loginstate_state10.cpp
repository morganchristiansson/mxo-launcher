#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace mxo::ltlogin {
namespace {

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

struct ParsedState10ClaimCharacterNameReplyScaffold {
    bool valid = false;
    uint32_t status = 0;
    uint32_t globalCharacterIdLow03 = 0;
    uint32_t globalCharacterIdHigh07 = 0;
    uint16_t optionalTextOffset01 = 0;
    const char* optionalText = nullptr;
    uint16_t optionalTextLength = 0;
};

static ParsedState10ClaimCharacterNameReplyScaffold ParseState10ClaimCharacterNameReplyScaffold(
    const uint8_t* bytes,
    uint16_t byteCount) {
    ParsedState10ClaimCharacterNameReplyScaffold out = {};
    if (byteCount < 0x0fu || bytes[0] != 0x0bu) {
        return out;
    }

    out.valid = true;
    out.optionalTextOffset01 = ReadU16LE(bytes + 1u);
    out.status = ReadU32LE(bytes + 3u);
    out.globalCharacterIdLow03 = ReadU32LE(bytes + 7u);
    out.globalCharacterIdHigh07 = ReadU32LE(bytes + 0x0bu);

    if (out.optionalTextOffset01 != 0u) {
        const size_t stringLengthFieldOffset = static_cast<size_t>(out.optionalTextOffset01);
        if (stringLengthFieldOffset + 2u <= byteCount) {
            out.optionalTextLength = ReadU16LE(bytes + stringLengthFieldOffset);
            const size_t stringBytesOffset = stringLengthFieldOffset + 2u;
            const size_t availableTextBytes = byteCount - stringBytesOffset;
            if (out.optionalTextLength > availableTextBytes) {
                out.optionalTextLength = static_cast<uint16_t>(availableTextBytes);
            }
            if (out.optionalTextLength != 0u) {
                out.optionalText = reinterpret_cast<const char*>(bytes + stringBytesOffset);
            }
        }
    }

    return out;
}

static std::string DescribeOptionalState10ClaimReplyText(
    const ParsedState10ClaimCharacterNameReplyScaffold& parsed) {
    if (!parsed.optionalText || parsed.optionalTextLength == 0u) {
        return "<empty>";
    }

    const size_t boundedLength = std::min<size_t>(parsed.optionalTextLength, 96u);
    std::string text(parsed.optionalText, parsed.optionalText + boundedLength);
    for (char& ch : text) {
        if (ch == '\0') {
            ch = ' ';
        }
    }
    return text;
}

}  // namespace

void CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorStateScaffold(CLTLoginMediator* mediator) {
    if (!mediator) {
        return;
    }

    // Address anchors:
    // - launcher.exe:0x43f300 broader auth-reply writer
    // - launcher.exe:0x441260 / 0x441330 narrower auth-reply adoption helpers nearby in the same
    //   broader early-auth corridor
    // Keep this helper scoped to the broader auth-reply adoption used by state2/current existing-
    // character auth bridges.
    // Important create/delete correction from the latest static pass:
    // - `0x4401a0` is not an auth-reply adopter
    // - it is the later margin-side `MS_ClaimCharacterNameReply` append helper for state10 slot 6
    // - so do not treat this broader auth-table rebuild as the owner-side body for `0x4401a0`
    mediator->worldSlots_.fill(nullptr);
    mediator->worldPayloadSlots_.fill(nullptr);
    mediator->worldDescriptorValidD84_.fill(false);
    mediator->worldDescriptorCountD80_ = 0;

    const size_t worldCount = std::min(mediator->worldSlots_.size(), mediator->lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        mediator->worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->SeedRecoveredWorldDescriptorFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.worlds[i]);
        ++mediator->worldDescriptorCountD80_;
    }

    mediator->selectionRouteState684_.ResetSelectionRouteState();

    const size_t characterCount = std::min(
        mediator->selectionRouteState684_.slotRecordTable04_.size(),
        mediator->lastAuthReply_.characters.size());
    mediator->selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(characterCount);
    for (size_t i = 0; i < characterCount; ++i) {
        mediator->SeedRecoveredCharacterSlotRecordFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.characters[i]);
        const SlotRecordState_0x4b5328& slotRecord = mediator->selectionRouteState684_.slotRecordTable04_[i];
        const int matchedWorldIndex = mediator->FindRecoveredWorldDescriptorIndexByWorldId(slotRecord.worldId0c);
        if (matchedWorldIndex >= 0) {
            // anchor: launcher.exe:0x43f74a
            // Original joins the just-built slot-record world id against the earlier +0xd84 table,
            // then copies the descriptor inline name into +0x818[currentCharacterIndex].
            mediator->selectionRouteState684_.routeHostStringTriples194_[i].Assign(
                mediator->worldDescriptorsD84_[static_cast<size_t>(matchedWorldIndex)].inlineNamePlus03);
        }
    }

    // Writeback to owner +0x80 (world list count/status family)
    mediator->worldListCountOrStatus80 =
        static_cast<uint32_t>(mediator->lastAuthReply_.worlds.size());

    if (characterCount != 0) {
        // Replacement-side mirror only:
        // - broader `0x43f300` resets `+0xcc8` through `0x41d270` and does not show a direct
        //   rewrite of the current-slot byte during the one-time auth adoption body
        // - current source still seeds slot 0 here so the non-GUI launcher path keeps a concrete
        //   current-slot mirror after the original table rebuild
        mediator->SetCurrentCharacterRouteIndexCc8Scaffold(0u);
    }

    if (characterCount != 0) {
        const SlotRecordState_0x4b5328& currentSlotRecord =
            mediator->selectionRouteState684_.slotRecordTable04_[0];
        mediator->marginRouteState_.pendingWorldId = currentSlotRecord.worldId0c;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(currentSlotRecord.worldId0c);
    } else if (worldCount != 0) {
        const mxo::auth::AuthWorldEntry& firstWorld = mediator->lastAuthReply_.worlds[0];
        mediator->marginRouteState_.pendingWorldId = firstWorld.worldId;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(firstWorld.worldId);
    }

    if (const char* routeHostPrefix =
            mediator->LookupRouteHostPrefixBySlot(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)) {
        mediator->marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        mediator->marginRouteState_.routeHostPrefix.clear();
    }

    mediator->SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();

    const char* currentDescriptorName = "<empty>";
    if (characterCount != 0) {
        const int matchedWorldIndex =
            mediator->FindRecoveredWorldDescriptorIndexByWorldId(
                mediator->selectionRouteState684_.slotRecordTable04_[0].worldId0c);
        if (matchedWorldIndex >= 0) {
            if (const char* name = mediator->GetDescriptorInlineNameByIndex(static_cast<uint8_t>(matchedWorldIndex))) {
                currentDescriptorName = name;
            }
        }
    } else if (worldCount != 0) {
        if (const char* name = mediator->GetDescriptorInlineNameByIndex(0)) {
            currentDescriptorName = name;
        }
    }

    spdlog::info(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount={} characterCount={} currentCharacterOrRouteIndex={} currentSlotWorldId={} routeHostPrefix='{}' slotRecordHeapString='{}' currentWorldDescriptorName='{}'",
        static_cast<unsigned>(worldCount),
        static_cast<unsigned>(characterCount),
        static_cast<unsigned>(mediator->marginRouteState_.currentCharacterOrRouteIndex),
        characterCount == 0
            ? 0u
            : static_cast<unsigned>(mediator->selectionRouteState684_.slotRecordTable04_[0].worldId0c),
        mediator->marginRouteState_.routeHostPrefix.empty() ? "<empty>" : mediator->marginRouteState_.routeHostPrefix.c_str(),
        mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)
            ? mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}



// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
void CLTLoginState_State10::Slot3_BeginOrContinue(void* upstreamOrArg) {
    (void)upstreamOrArg;
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
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
    if (!mediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = mediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }
    if (mediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = mediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }

    State10Packet0x0aBuilder_0x4b53b4 packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(
        mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.data());

    (void)mediator->SendCurrentMarginPacket(packetBuilder.Envelope());
    mediator->PostEvent(0x13u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x{:02x} totalBytes=0x{:02x} CharacterName='{}' -> postEvent=0x13",
        State10Packet0x0aFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        std::string(mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.data()));
    return;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(
        mxo::liblttcp::CMessageConnectionMessageRef* messageRef) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator || !messageRef) {
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
    if (!CMessageConnection_DecodeMessageCodeScaffold(*messageRef, &messageCode, nullptr)) {
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
        mediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State10::Slot6_HandleSecondaryMessage rejected messageCode=0x{:04x} (expected 0x0b); mirrored original owner+0x80=0x12000005",
            static_cast<unsigned>(messageCode));
        return 0u;
    }

    // anchor: launcher.exe:0x4401a0
    // Original at 0x4401c0: CALL 0x43a330 (State10ClaimCharacterNameReplyParseObject_InitFromIncomingPacket)
    // with LEA ECX,[EBP-0x28]; PUSH 1; PUSH ESI (messageRef). This constructs a parse
    // object on the stack directly from the raw CMessageConnectionMessageRef*, not from
    // extracted payload bytes. The parse object has a virtual destructor cleaned up at
    // 0x4402e7-0x4402f0: TEST ECX,ECX; JZ skip; MOV EDX,[ECX]; CALL [EDX+8].
    //
    // Current approach: extract payload bytes + use a POD scaffold parser. This produces
    // the same field values (offsets +3=status, +7=charIdLow, +0xb=charIdHigh match) but
    // doesn't mirror the original's parse-object lifetime or virtual dispatch.
    const uint8_t* payloadBytes = messageRef->messageStorage0c->payloadBytes0c.data();
    const uint16_t payloadByteCount = messageRef->PayloadByteCountScaffold();

    // Inline recovery of MS_ClaimCharacterNameReply parsing and slot record allocation:
    // - parse inline name/status/character IDs from message-ref payload
    // - set owner+0x80 = parsed status
    // - if status < 1: allocate/init new slot record at +0x688[currentCount],
    //   copy descriptor inline name into +0x818[currentCount], set owner+0xcc8=currentCount,
    //   increment +0x684, fill slot record with character ID/name/worldId fields
    const ParsedState10ClaimCharacterNameReplyScaffold parsed =
        ParseState10ClaimCharacterNameReplyScaffold(payloadBytes, payloadByteCount);
    if (!parsed.valid) {
        spdlog::warn(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply parse rejected payload bytes={} (expected >= 0x0f-byte MS_ClaimCharacterNameReply layout)",
            static_cast<unsigned>(payloadByteCount));
        return 0u;
    }

    // anchor: launcher.exe:0x4401cb-0x4401d4
    // Original: MOV ECX,[EAX+3]; MOV [EDX+0x80],ECX; CMP [EAX+3],1; JGE error_path
    // Status check is BEFORE allocation. If status >= 1, jump to error path at 0x4402cd.
    mediator->worldListCountOrStatus80 = parsed.status;

    if (parsed.status >= 1u) {
        // Error path: SetCurrentState(3), PostError(0xb) — matches 0x4402cd-0x4402e2
        (void)mediator->SetCurrentState(3u);
        mediator->PostError(0x0bu);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage observed error MS_ClaimCharacterNameReply; mirrored original state3 switch and error=0x0b owner+0x80=0x{:08x}",
            static_cast<unsigned>(mediator->worldListCountOrStatus80));
        return 1u;
    }

    // Success path (status < 1): allocate and populate new slot record
    //
    // Fidelity note on allocation model:
    // Original at 0x4401ec-0x440228: PUSH 0x1c; CALL 0x403260 (TrackedMalloc);
    // CALL 0x4398b0 (SlotRecord_Initialize); then stores the pointer at
    // [ESI + count*4 + 4] (pointer table at +0x688). Current source uses an
    // inline std::array<SlotRecordState_0x4b5328, N> instead of a pointer table,
    // so the TrackedMalloc+Init is approximated by value-initialization.
    const uint8_t appendedSlotIndex = mediator->selectionRouteState684_.slotRecordCount00_;
    const uint32_t selectedWorldDescriptorIndex =
        mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24;
    if (appendedSlotIndex >= mediator->selectionRouteState684_.slotRecordTable04_.size() ||
        appendedSlotIndex >= mediator->selectionRouteState684_.routeHostStringTriples194_.size() ||
        selectedWorldDescriptorIndex >= mediator->worldDescriptorCountD80_ ||
        selectedWorldDescriptorIndex >= mediator->worldDescriptorsD84_.size() ||
        !mediator->worldDescriptorValidD84_[selectedWorldDescriptorIndex]) {
        spdlog::warn(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply success could not append slot record appendedSlotIndex={} selectedWorldDescriptorIndex={} worldDescriptorCount=0x{:02x}",
            static_cast<unsigned>(appendedSlotIndex),
            static_cast<unsigned>(selectedWorldDescriptorIndex),
            static_cast<unsigned>(mediator->worldDescriptorCountD80_));
        return 0u;
    }

    const CLTLoginMediator::WorldDescriptorState_0x4b533c& selectedWorldDescriptor =
        mediator->worldDescriptorsD84_[selectedWorldDescriptorIndex];

    // anchor: launcher.exe:0x4401a0
    // Exact success-side write order recovered from listing:
    // 1. store initialized slot record at +0x688[currentCount]  (0x440228)
    // 2. copy selected descriptor inline name into +0x818[currentCount]  (0x44022c-0x44024d)
    //    via CopyInlineNameToString(0x43d430) + StringTriple_AssignFromRange(0x407dd0)
    // 3. free temp inline-name copy string  (0x440252-0x440265)
    // 4. set +0x644 = currentCount  (0x440268-0x44026a)
    // 5. increment +0x00 (count)  (0x440270-0x440272)
    // 6. SetCharacterName from createCharacterData  (0x440274-0x440283)
    // 7. write character IDs / status / worldId  (0x440288-0x4402ab)
    // 8. SetCurrentState(0xb)  (0x4402af-0x4402b7)
    // 9. PostEvent(0x14)  (0x4402bc-0x4402c4)
    //
    // Note: original has no slotRecordValid04_ write — just the pointer store at 0x440228.
    mediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex] = {};
    mediator->selectionRouteState684_.routeHostStringTriples194_[appendedSlotIndex].Assign(
        selectedWorldDescriptor.inlineNamePlus03);
    mediator->SetCurrentCharacterRouteIndexCc8Scaffold(appendedSlotIndex);
    mediator->selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(appendedSlotIndex + 1u);

    // anchor: launcher.exe:0x440274-0x440283
    // Original: PUSH &mediator->createCharacterData108; MOV ECX,pNewSlotRecord;
    // CALL 0x43aa80 (SetCharacterName). This copies the character name into the slot
    // record's payload buffer via ReserveLengthPrefixedString + WriteReservedCString.
    // Current code approximates this by setting the heapString14 pointer directly.
    // TODO: Once slot records are heap-allocated with payload buffers, replace with
    // a proper SetCharacterName call.
    SlotRecordState_0x4b5328& appendedSlotRecord =
        mediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex];
    appendedSlotRecord.heapString14 =
        mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.characterName00.data();

    // anchor: launcher.exe:0x440288-0x4402ab
    // Character ID writes: [EDI+0x10]+0x3 = parsed.+7, [EDI+0x10]+0x7 = parsed.+0xb,
    // [EDI+0x10]+0xb = 0 (status), [EDI+0x10]+0xc = worldId (word from descriptor)
    appendedSlotRecord.globalCharacterIdLow03 = parsed.globalCharacterIdLow03;
    appendedSlotRecord.globalCharacterIdHigh07 = parsed.globalCharacterIdHigh07;
    appendedSlotRecord.status0b = 0u;
    appendedSlotRecord.worldId0c = selectedWorldDescriptor.worldId01;

    // anchor: launcher.exe:0x4402af-0x4402c4
    // Original: SetCurrentState(g_Mediator, 0xb) then PostEvent(g_Mediator, 0x14).
    // No helper-state null-check, no expectedMarginRequestName_ write, no marginRouteState_
    // writes. The parse-object virtual destructor runs at 0x4402e7-0x4402f0 before return.
    (void)mediator->SetCurrentState(0x0bu);
    mediator->PostEvent(0x14u);
    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage successful MS_ClaimCharacterNameReply -> SetCurrentState(0xb) then PostEvent(0x14)");
    return 1u;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::GetStateId() const {
    return 10;
}

}  // namespace mxo::ltlogin
