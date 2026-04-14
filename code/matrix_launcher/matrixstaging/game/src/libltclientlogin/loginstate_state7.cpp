#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

struct ParsedState7Opcode0eReplyScaffold {
    bool valid = false;
    uint32_t result09 = 0u;
};

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// anchor: launcher.exe:0x43aae0
static ParsedState7Opcode0eReplyScaffold ParseState7Opcode0eReplyScaffold(
    const std::vector<uint8_t>& stagedMarginBytes) {
    ParsedState7Opcode0eReplyScaffold parsed = {};
    if (stagedMarginBytes.size() < 13u) {
        return parsed;
    }
    if (stagedMarginBytes[0] != 0x0eu) {
        return parsed;
    }
    parsed.valid = true;
    // Current bounded field recovery from `0x43bae0` / `0x43aae0`:
    // - opcode must be `0x0e`
    // - slot 6 consumes the parsed dword at `+0x09`
    // - result `< 1` posts event `8`
    // - result `>= 1` posts error `9`
    parsed.result09 = ReadU32LE(stagedMarginBytes.data() + 0x09u);
    return parsed;
}

}  // namespace

// anchor: launcher.exe vtable 0x004b50b4
const char* CLTLoginState_State7::DebugName() const {
    return "CLTLoginState_State7";
}

// anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
uint32_t CLTLoginState_State7::Slot3_BeginOrContinue(void* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Fresh `0x43ba20` decompile/disassembly tightening:
    // - gate on owner `+0x1c` ready-state through `0x41b4b0`; on failure switch helper `4`
    // - gate on owner byte `+0xf14`; on zero switch helper `6`
    // - initialize local raw-`0x0d` margin packet (`0x43a9a0`)
    // - fetch current slot record through owner `+0x44`
    // - fetch owner source block `+0x94` through owner `+0x38`, then read the embedded
    //   small-string begin pointer at `+0x60` and thread that into `0x43aa80`
    // - copy current slot-record id pair from the record payload at `+0x10 + 0x03/+0x07`
    // - send through `0x41af70`
    // - post event `7`
    if (!mediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = mediator->SwitchHelperStateByIdScaffold(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State7::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }
    if (mediator->postAuthMarginLoadingState_.state10SendGateFlagF14 == 0u) {
        const uint32_t fallbackResult = mediator->SwitchHelperStateByIdScaffold(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State7::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }

    const SlotRecordState_0x4b5328* currentSlotRecord = mediator->GetCurrentSlotRecord();
    const char* sourceBlock94String60Begin = mediator->GetSourceBlock94SmallString60BeginScaffold();

    State7Packet0x0dBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(sourceBlock94String60Begin);
    packetBuilder.SetCharacterIdPair(
        currentSlotRecord ? currentSlotRecord->globalCharacterIdLow03 : 0u,
        currentSlotRecord ? currentSlotRecord->globalCharacterIdHigh07 : 0u);

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.Envelope());
    mediator->PostEvent(0x07u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State7::Slot3_BeginOrContinue built raw-0x0d packet fixedBytes=0x{:02x} totalBytes=0x{:02x} sourceBlock94String60='{}' gcidLow=0x{:08x} gcidHigh=0x{:08x} currentSlotName='{}' -> sendResult=0x{:08x} then posts event=0x07",
        State7Packet0x0dFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        sourceBlock94String60Begin ? sourceBlock94String60Begin : "<null>",
        currentSlotRecord ? currentSlotRecord->globalCharacterIdLow03 : 0u,
        currentSlotRecord ? currentSlotRecord->globalCharacterIdHigh07 : 0u,
        currentSlotRecord && currentSlotRecord->heapString14 ? currentSlotRecord->heapString14 : "<empty>",
        sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
uint32_t CLTLoginState_State7::Slot6_HandleSecondaryMessage(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedMarginBytes = mediator->StagedIncomingMarginPacketBytes();
    const ParsedState7Opcode0eReplyScaffold parsed = ParseState7Opcode0eReplyScaffold(stagedMarginBytes);
    if (!parsed.valid) {
        mediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State7::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; mirrored original owner+0x80=0x12000005",
            static_cast<unsigned>(stagedMarginBytes.size()),
            stagedMarginBytes.empty() ? 0u : static_cast<unsigned>(stagedMarginBytes[0]));
        return 0u;
    }

    mediator->worldListCountOrStatus80 = parsed.result09;
    (void)mediator->SwitchHelperStateByIdScaffold(3u);

    // Tightened event-8 meaning from the real state7 reply body:
    // - success/result `< 1` posts event `8`
    // - failure/result `>= 1` posts error `9`
    // - this same event `8` is what the launcher delete-character command `0x40ec70` waits on
    // - negative result: that makes the concrete `0x40ec70 -> +0xf0 -> state7 -> event 8`
    //   corridor removal-oriented, not the hidden success-side `+0xec / 0x41c1f0` producer
    if (parsed.result09 < 1u) {
        mediator->PostEvent(0x08u);
        spdlog::info(
            "CLTLoginState_State7::Slot6_HandleSecondaryMessage opcode-0x0e success result09=0x{:08x} -> switch helper state3 then PostEvent(0x08) currentState={}",
            static_cast<unsigned>(parsed.result09),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
    } else {
        mediator->PostError(0x09u);
        spdlog::info(
            "CLTLoginState_State7::Slot6_HandleSecondaryMessage opcode-0x0e failure result09=0x{:08x} -> switch helper state3 then PostError(0x09) currentState={}",
            static_cast<unsigned>(parsed.result09),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
    }
    return 1u;
}

// anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
uint32_t CLTLoginState_State7::GetStateId() const {
    return 7;
}

}  // namespace mxo::ltlogin
