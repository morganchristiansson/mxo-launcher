#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "loginstate_loadcharacterreply_scaffold.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b5154
const char* CLTLoginState_State11::DebugName() const {
    return "CLTLoginState_State11";
}

// anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
uint32_t CLTLoginState_State11::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Faithfulness correction:
    // - `0x43c020` belongs to `CLTLoginState_State11` slot 3, so the packet build/send shape
    //   should live here, not on the mediator
    // - original body:
    //   - sends raw margin opcode `0x0c`
    //     - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //       `MS_CreateCharacterRequest`
    //   - treats `ESI = owner + 0x108`
    //   - creates a packet-builder object through `0x439840`
    //   - resets/initializes the raw `0x4d` payload through `0x43a470`
    //   - writes 17 dwords from owner `+0x134..+0x174`
    //   - appends `RealFirstName`, `RealLastName`, optional `Background`, and `GameSessionID`
    //     through `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940`
    //   - newer `0x43e540` debug-printer review makes those 17 dwords concrete:
    //     SkinToneID, BodyID, HeadID, HairID, HairColorID, TattooID, FacialHairID,
    //     FacialHairColorID, StartingHat, StartingGlasses, StartingShirt, StartingGloves,
    //     StartingCoat, StartingPants, StartingTights, StartingShoes, TraitID
    //   - calls `0x41af70` to forward the completed packet-envelope object through the current
    //     margin connection send path (`0x448cf0`), not to serialize raw bytes itself
    //   - then posts event `0x15`
    // Active-path caution:
    // - this is a very real character-data sender, but the natural-original password-submit path is
    //   still not live-proven here; no natural hit yet on `0x41c3c0` or `0x43c020`
    // - current replacement-launcher runtime proof now lands here explicitly:
    //   `... -> helperState 0x0b -> event 0x15 -> State11::Slot3 send -> Loading Character`
    //   and then stalls before any incoming `MS_LoadCharacterReply`
    const auto& sourceDwords134 = mediator->SourceDwords134();
    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    State11Packet0x4dBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();

    // Keep write order aligned with the original disassembly of `0x43c020`.
    packetBuilder.SetFixedDword(0x01, sourceDwords134[0]);
    packetBuilder.SetFixedDword(0x05, sourceDwords134[1]);
    packetBuilder.SetFixedDword(0x09, sourceDwords134[2]);
    packetBuilder.SetFixedDword(0x0d, sourceDwords134[3]);
    packetBuilder.SetFixedDword(0x11, sourceDwords134[4]);
    packetBuilder.SetFixedDword(0x15, sourceDwords134[5]);
    packetBuilder.SetFixedDword(0x19, sourceDwords134[6]);
    packetBuilder.SetFixedDword(0x1d, sourceDwords134[7]);
    packetBuilder.SetFixedDword(0x35, sourceDwords134[13]);
    packetBuilder.SetFixedDword(0x25, sourceDwords134[9]);
    packetBuilder.SetFixedDword(0x3d, sourceDwords134[15]);
    packetBuilder.SetFixedDword(0x2d, sourceDwords134[11]);
    packetBuilder.SetFixedDword(0x21, sourceDwords134[8]);
    packetBuilder.SetFixedDword(0x39, sourceDwords134[14]);
    packetBuilder.SetFixedDword(0x31, sourceDwords134[12]);
    packetBuilder.SetFixedDword(0x29, sourceDwords134[10]);
    packetBuilder.SetFixedDword(0x41, sourceDwords134[16]);

    packetBuilder.SetRealFirstName(reinterpret_cast<const char*>(mediator->SourceBlock178().data()));
    packetBuilder.SetRealLastName(reinterpret_cast<const char*>(mediator->SourceBlock198().data()));
    packetBuilder.SetBackground(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()));
    packetBuilder.SetGameSessionId(mediator->GetGameSessionId());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x15u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State11::Slot3_BeginOrContinue built fixed-0x4d margin payload payloadTag=0x{:02x} fixedBytes=0x{:02x} totalBytes=0x{:02x} SkinToneID=0x{:08x} RealFirstName='{}' RealLastName='{}' Background='{}' GameSessionID='{}' -> sendResult=0x{:08x} then posts event=0x15",
        State11Packet0x4dFixedPayload::kPayloadTag0c,
        State11Packet0x4dFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        sourceDwords134[0],
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock178().data())),
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock198().data())),
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data())),
        mediator->GetGameSessionId() ? mediator->GetGameSessionId() : "<empty>",
        sendResult);
    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue awaiting first helper11 reply; slot6 requires a later raw-0x10 that survives the base margin code-2/4/5 filter currentState={} marginReceiveCount={} filteredBeforeSlot6={} slot6DispatchCount={}",
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->MarginPacketReceiveCountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketFilteredBeforeSlot6CountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketSlot6DispatchCountScaffold()));
    return sendResult;
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->StagedIncomingMarginPacketBytes();
    const uint16_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(stagedBytes);
    if (!parsed.valid) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; helper11 slot6 only handles raw-0x10 after the base margin code-2/4/5 filter",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    // Ownership correction mirrors slot 3 as well:
    // - `0x440320` belongs to `CLTLoginState_State11` slot 6
    // - the state object owns reply-progress counters and the helper11 -> helper9 handoff
    // - mediator keeps the narrower owner-buffer mutation helper because those writes target the
    //   mediator-owned `0x4f78b8` state area
    const uint32_t handled = mediator->HandleStagedMarginLoadCharacterReplyPacketScaffold();
    if (handled == 0u) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage entered raw-0x10 receive path but owner-side parse/gate failed status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} currentState={}",
            static_cast<unsigned>(parsed.status),
            static_cast<unsigned>(parsed.field05),
            static_cast<unsigned>(parsed.handoffWord09),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    if (parsed.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = parsed.expectedSectionCount0b;
    }
    if (replySectionsExpected_ == 0u) {
        replySectionsExpected_ = 1u;
    }
    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                // `0x440320` writes parsed word `+9` into helper9 `this+6` before switching state.
                // Current source-owned mirror keeps that on the concrete state9 object.
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x440320 completion tail posts event 0x16 after switching to helper9.
        mediator->PostEventScaffold(0x16u);

        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage completed helper11 reply progression status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} -> currentState=helper9 event=0x16",
            parsed.status,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            parsed.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
    } else {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage routed helper11 reply status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} seedCount={}",
            parsed.status,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            parsed.handoffWord09,
            replySectionsSeen_,
            (unsigned)replySectionsExpected_,
            parsed.shouldSeedExpectedSectionCount ? 1u : 0u);
    }
    return handled;
}

// anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
uint32_t CLTLoginState_State11::Slot7_GetStateId() const {
    return 11;
}

}  // namespace mxo::ltlogin
