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
    const std::vector<uint8_t>& bytes) {
    ParsedState10ClaimCharacterNameReplyScaffold out = {};
    if (bytes.size() < 0x0fu || bytes[0] != 0x0bu) {
        return out;
    }

    out.valid = true;
    out.optionalTextOffset01 = ReadU16LE(bytes.data() + 1u);
    out.status = ReadU32LE(bytes.data() + 3u);
    out.globalCharacterIdLow03 = ReadU32LE(bytes.data() + 7u);
    out.globalCharacterIdHigh07 = ReadU32LE(bytes.data() + 0x0bu);

    if (out.optionalTextOffset01 != 0u) {
        const size_t stringLengthFieldOffset = static_cast<size_t>(out.optionalTextOffset01);
        if (stringLengthFieldOffset + 2u <= bytes.size()) {
            out.optionalTextLength = ReadU16LE(bytes.data() + stringLengthFieldOffset);
            const size_t stringBytesOffset = stringLengthFieldOffset + 2u;
            const size_t availableTextBytes = bytes.size() - stringBytesOffset;
            if (out.optionalTextLength > availableTextBytes) {
                out.optionalTextLength = static_cast<uint16_t>(availableTextBytes);
            }
            if (out.optionalTextLength != 0u) {
                out.optionalText = reinterpret_cast<const char*>(bytes.data() + stringBytesOffset);
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
            mediator->LookupRouteHostPrefixBySlot(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)) {
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
        mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)
            ? mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}


// UNANCHORED: source-owned raw-0x0b margin claim-name reply helper for state10 slot 6.
uint32_t CLTLoginState_State10::HandleStagedClaimCharacterNameReplyScaffold(CLTLoginMediator* mediator) {
    if (!mediator || mediator->stagedIncomingMarginPacketBytes_.empty()) {
        return 0u;
    }

    const ParsedState10ClaimCharacterNameReplyScaffold parsed =
        ParseState10ClaimCharacterNameReplyScaffold(mediator->stagedIncomingMarginPacketBytes_);
    if (!parsed.valid) {
        spdlog::warn(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply parse rejected staged margin bytes={} (expected >= 0x0f-byte MS_ClaimCharacterNameReply layout)",
            static_cast<unsigned>(mediator->stagedIncomingMarginPacketBytes_.size()));
        return 0u;
    }

    mediator->worldListCountOrStatus80 = parsed.status;
    if (parsed.status >= 1u) {
        spdlog::info(
            "DIAGNOSTIC: state10 raw-0x0b claim-name reply failure status=0x{:08x} optionalTextOffset=0x{:04x} optionalTextLen=0x{:04x} optionalText='{}'",
            static_cast<unsigned>(parsed.status),
            static_cast<unsigned>(parsed.optionalTextOffset01),
            static_cast<unsigned>(parsed.optionalTextLength),
            DescribeOptionalState10ClaimReplyText(parsed));
        return 1u;
    }

    const uint8_t appendedSlotIndex = mediator->selectionRouteState684_.slotRecordCount00_;
    const uint32_t selectedWorldDescriptorIndex =
        mediator->postAuthMarginLoadingState_.createCharacterData108.selectedWorldField24;
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

    const CLTLoginMediator::WorldDescriptorState004b533c& selectedWorldDescriptor =
        mediator->worldDescriptorsD84_[selectedWorldDescriptorIndex];

    // anchor: launcher.exe:0x4401a0
    // Exact success-side write order recovered from the listing:
    // - allocate/init new slot-record object
    // - store it at +0x688[currentCount]
    // - copy selected descriptor inline name into +0x818[currentCount]
    // - set +0xcc8 = currentCount
    // - increment +0x684
    // - then fill the slot-record payload/name fields
    mediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex] = {};
    mediator->selectionRouteState684_.slotRecordValid04_[appendedSlotIndex] = true;
    mediator->selectionRouteState684_.routeHostStringTriples194_[appendedSlotIndex].Assign(
        selectedWorldDescriptor.inlineNamePlus03);
    mediator->SetCurrentCharacterRouteIndexCc8Scaffold(appendedSlotIndex);
    mediator->selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(appendedSlotIndex + 1u);

    SlotRecordState_0x4b5328& appendedSlotRecord =
        mediator->selectionRouteState684_.slotRecordTable04_[appendedSlotIndex];
    appendedSlotRecord.heapString14 =
        mediator->postAuthMarginLoadingState_.createCharacterData108.characterName00.data();
    appendedSlotRecord.globalCharacterIdLow03 = parsed.globalCharacterIdLow03;
    appendedSlotRecord.globalCharacterIdHigh07 = parsed.globalCharacterIdHigh07;
    appendedSlotRecord.status0b = 0u;
    appendedSlotRecord.worldId0c = selectedWorldDescriptor.worldId01;

    mediator->marginRouteState_.pendingWorldId = selectedWorldDescriptor.worldId01;
    mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(selectedWorldDescriptor.worldId01);
    if (const char* routeHostPrefix = mediator->LookupRouteHostPrefixBySlot(appendedSlotIndex)) {
        mediator->marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        mediator->marginRouteState_.routeHostPrefix.clear();
    }

    spdlog::info(
        "DIAGNOSTIC: state10 raw-0x0b claim-name reply appended slot={} status=0x{:08x} globalCharacterIdLow=0x{:08x} globalCharacterIdHigh=0x{:08x} selectedWorldDescriptorIndex=0x{:08x} routeText='{}' characterName='{}' optionalText='{}'",
        static_cast<unsigned>(appendedSlotIndex),
        static_cast<unsigned>(parsed.status),
        static_cast<unsigned>(parsed.globalCharacterIdLow03),
        static_cast<unsigned>(parsed.globalCharacterIdHigh07),
        static_cast<unsigned>(selectedWorldDescriptorIndex),
        mediator->selectionRouteState684_.routeHostStringTriples194_[appendedSlotIndex].BeginOrNull()
            ? mediator->selectionRouteState684_.routeHostStringTriples194_[appendedSlotIndex].BeginOrNull()
            : "<empty>",
        appendedSlotRecord.heapString14.empty() ? "<empty>" : appendedSlotRecord.heapString14.c_str(),
        DescribeOptionalState10ClaimReplyText(parsed));
    return 1u;
}

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(void* upstreamOrArg) {
    (void)upstreamOrArg;
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
        return 0u;
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
        const uint32_t fallbackResult = mediator->SwitchHelperStateByIdScaffold(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }
    if (mediator->postAuthMarginLoadingState_.state10SendGateFlagF14 == 0) {
        const uint32_t fallbackResult = mediator->SwitchHelperStateByIdScaffold(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(
        mediator->postAuthMarginLoadingState_.createCharacterData108.characterName00.data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.Envelope());
    mediator->PostEvent(0x13u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x{:02x} totalBytes=0x{:02x} CharacterName='{}' -> sendResult=0x{:08x} then posts event=0x13",
        State10Packet0x0aFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        std::string(mediator->postAuthMarginLoadingState_.createCharacterData108.characterName00.data()),
        sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(void* workItem) {
    (void)workItem;
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->StagedIncomingMarginPacketBytes();
    const uint8_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];

    // Fidelity correction from direct `0x43bf90 / 0x4401a0 / 0x41bf70` review:
    // - state10 slot 3 sends raw margin opcode `0x0a = MS_ClaimCharacterNameRequest`
    // - state10 slot 6 therefore consumes the matching raw margin opcode
    //   `0x0b = MS_ClaimCharacterNameReply`, not an auth-channel `AS_AuthReply`
    // - success appends exactly one new slot record under owner `+0x688/+0x818`, sets owner
    //   `+0xcc8 = currentCount`, then switches to helper11 and posts event `0x14`
    // - immediate continuation after that success is:
    //   `state11 slot3 / 0x43c020 -> state11 slot6 / 0x440320 -> helper9 slot3 / 0x439780
    //    -> owner 0x41de40`
    if (rawCode != 0x0bu) {
        mediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State10::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; mirrored original owner+0x80=0x12000005 and returned false-like",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    const uint32_t handled = HandleStagedClaimCharacterNameReplyScaffold(mediator);
    if (handled == 0u) {
        return 0u;
    }

    if (mediator->worldListCountOrStatus80 >= 1u) {
        (void)mediator->SwitchHelperStateByIdScaffold(3u);
        mediator->PostError(0x0bu);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage observed error MS_ClaimCharacterNameReply; mirrored original state3 switch and error=0x0b owner+0x80=0x{:08x}",
            static_cast<unsigned>(mediator->worldListCountOrStatus80));
        return 1u;
    }

    uint32_t helper11EntryResult = 0u;
    if (mediator->LoginHelperStateByIdScaffold(0x0bu) != nullptr) {
        // anchor: launcher.exe:0x4401a0 success tail
        // Original ends with `0x41b450(0x0b)`, so keep the immediate helper11 slot-3 continuation
        // inside the central switch helper instead of restaging it through a source-only wrapper.
        helper11EntryResult = mediator->SwitchHelperStateByIdScaffold(0x0bu);
        mediator->expectedMarginRequestName_ = CLTLoginMediator::kMessageMsLoadCharacterReply;
    } else {
        spdlog::warn(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage parsed successful MS_ClaimCharacterNameReply but has no registered helper11 state");
    }
    mediator->PostEvent(0x14u);
    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage successful MS_ClaimCharacterNameReply -> helper11 entry result=0x{:08x} then PostEvent(0x14)",
        static_cast<unsigned>(helper11EntryResult));
    return 1u;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

}  // namespace mxo::ltlogin
