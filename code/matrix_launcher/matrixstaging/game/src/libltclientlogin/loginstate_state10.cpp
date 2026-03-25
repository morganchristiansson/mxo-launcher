#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(
    void* upstreamOrArg,
    CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
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
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6");
        return 0u;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(mediator->SourceLeadString108().data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x13u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x{:02x} totalBytes=0x{:02x} CharacterName='{}' -> sendResult=0x{:08x} then posts event=0x13",
        State10Packet0x0aFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        std::string(reinterpret_cast<const char*>(mediator->SourceLeadString108().data())),
        sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(
    void* workItem,
    CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    // Ownership correction from the vtable docs + Ghidra decompilation:
    // - `0x4401a0` belongs to `CLTLoginState_State10` slot 6, not to the mediator vtable
    // - the state entry itself handles raw auth code `0x0b`, performs the owner writeback, then
    //   switches helper state to `11`
    // - the mediator keeps only the narrower staged-packet + owner-state helpers
    const uint32_t handled = mediator->HandleStagedAuthReplyPacketScaffold();
    if (handled == 0u) {
        return 0u;
    }

    if (CLTLoginState* nextState = mediator->ScaffoldState11()) {
        mediator->SwitchHelperStateScaffold(0x0bu, nextState);
    } else {
        spdlog::warn(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage parsed AS_AuthReply but has no registered helper11 state");
    }
    return handled;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

}  // namespace mxo::ltlogin
