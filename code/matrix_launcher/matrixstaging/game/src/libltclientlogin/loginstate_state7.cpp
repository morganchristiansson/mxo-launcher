#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

}  // namespace

// anchor: launcher.exe vtable 0x4b50b4
const char* CLTLoginState_State7_0x4b50b4::DebugName() const {
    return "CLTLoginState_State7_0x4b50b4";
}

// anchor: launcher.exe:0x43ba20 (vtable 0x4b50b4 slot 3)
void CLTLoginState_State7_0x4b50b4::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    if (!g_CurrentLoginMediator) {
        return;
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
    if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(4u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State7_0x4b50b4::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }
    if (g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 == 0u) {
        const uint32_t fallbackResult = g_CurrentLoginMediator->SetCurrentState(6u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State7_0x4b50b4::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return;
    }

    const Packet_MsClaimCharacterNameReply_0x4b5328* currentSlotRecord = g_CurrentLoginMediator->GetCurrentSlotRecord();
    const char* sourceBlock94String60Begin = g_CurrentLoginMediator->ownerAuthBootstrapSource94_.sessionToken60.begin;

    // anchor: launcher.exe:0x43a9a0 = Packet_MsDeleteCharacterRequest_0x4b53f0::ResetAndInitialize
    Packet_MsDeleteCharacterRequest_0x4b53f0 packetBuilder;
    packetBuilder.ResetAndInitialize();

    // anchor: launcher.exe:0x43aa80 = SetCharacterName (mediator helper)
    // Implement reservation inline for source fidelity
    uint8_t* payload = static_cast<uint8_t*>(packetBuilder.payloadAlias10);
    if (payload && sourceBlock94String60Begin && packetBuilder.reservation14_.reservedContentByteCount04 == 0u) {
        // Compute string length
        size_t textLen = 0;
        const char* p = sourceBlock94String60Begin;
        while (*p++) ++textLen;
        ++textLen;  // Include NUL

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
                    *reinterpret_cast<uint16_t*>(payload + State7Packet0x0dFixedPayload::kCharacterNameOffset) = offset;

                    if (textLen > 1u) {
                        std::copy_n(sourceBlock94String60Begin, textLen - 1u, lengthPrefix + 2u);
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

    // Write character ID pair directly to payload
    if (payload) {
        *reinterpret_cast<uint32_t*>(payload + State7Packet0x0dFixedPayload::kCharacterIdLowOffset) =
            currentSlotRecord ? currentSlotRecord->characterIdLow1c : 0u;
        *reinterpret_cast<uint32_t*>(payload + State7Packet0x0dFixedPayload::kCharacterIdHighOffset) =
            currentSlotRecord ? currentSlotRecord->characterIdHigh20 : 0u;
    }

    // Build envelope for send
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
    envelope.payloadBase04 = payload;
    envelope.messageRef08 = packetBuilder.messageRef08;
    const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
    g_CurrentLoginMediator->PostEvent(0x07u);

    const uint16_t totalBytes = packetBuilder.messageRef08 && packetBuilder.messageRef08->messageStorage0c
        ? packetBuilder.messageRef08->messageStorage0c->PayloadByteCount() : 0u;

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State7_0x4b50b4::Slot3_BeginOrContinue built raw-0x0d packet fixedBytes=0x{:02x} totalBytes=0x{:02x} sourceBlock94String60='{}' gcidLow=0x{:08x} gcidHigh=0x{:08x} currentSlotName='{}' -> sendResult=0x{:08x} then posts event=0x07",
        State7Packet0x0dFixedPayload::kFixedByteCount,
        totalBytes,
        sourceBlock94String60Begin ? sourceBlock94String60Begin : "<null>",
        currentSlotRecord ? currentSlotRecord->characterIdLow1c : 0u,
        currentSlotRecord ? currentSlotRecord->characterIdHigh20 : 0u,
        currentSlotRecord && currentSlotRecord->debugString14 ? currentSlotRecord->debugString14 : "<empty>",
        sendResult);
    return;
}

// anchor: launcher.exe:0x43bae0 (vtable 0x4b50b4 slot 6)
uint32_t CLTLoginState_State7_0x4b50b4::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    CLTLoginMediator* originalMediator = g_CurrentLoginMediator;
    uint16_t messageCode = CMessageConnectionMessageRef_DecodeMessageCode(workItem);
    Packet_MsDeleteCharacterReply_0x4b5404 deleteReplyPacket(workItem, '\x01');

    if (messageCode == 0xe) {
        const uint32_t deleteReplyResult09 = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(deleteReplyPacket.payloadAlias10) + 0x09u);
        g_CurrentLoginMediator->worldListCountOrStatus80 = deleteReplyResult09;
        if (deleteReplyResult09 < 1u) {
            g_CurrentLoginMediator->SetCurrentState(3u);
            g_CurrentLoginMediator->PostEvent(0x08u);
        }
        else {
            g_CurrentLoginMediator->SetCurrentState(3u);
            g_CurrentLoginMediator->PostError(0x09u);
        }
        const uint32_t sideEffectResult = g_CurrentLoginMediator->HandleState9Opcode11SuccessSideEffect();
        (void)sideEffectResult;
        return 1u;
    }
    g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
    return reinterpret_cast<uint32_t>(originalMediator) & 0xffffff00u;
}

// anchor: launcher.exe:0x438c80 (vtable 0x4b50b4 slot 7)
uint32_t CLTLoginState_State7_0x4b50b4::GetStateId() const {
    return 7;
}

}  // namespace mxo::ltlogin
