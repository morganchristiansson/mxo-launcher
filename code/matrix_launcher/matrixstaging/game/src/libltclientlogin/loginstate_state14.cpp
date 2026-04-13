#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b4fec
const char* CLTLoginState_WorldListPending::DebugName() const {
    return "CLTLoginState_WorldListPending";
}

// anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
uint32_t CLTLoginState_WorldListPending::Slot3_BeginOrContinue(void* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Current evidence-backed narrow scaffold for helper/state 14:
    // - verify auth-side connectivity first
    // - build a tiny auth packet whose first payload byte is raw `0x35`
    // - send it on the existing auth connection
    // - post event `0x1b`
    // Keep this intentionally limited to the outgoing half already described in the vtable docs;
    // the broader `0x36` reply handling remains owned by AuthMessageDispatch().
    if (mediator->AuthConnectionFlag2c() == 0u) {
        spdlog::info(
            "CLTLoginState_WorldListPending::Slot3_BeginOrContinue blocked on owner+0x2c==0 currentState={}",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    mxo::liblttcp::CMessageConnection* connection = mediator->AuthConnection();
    if (connection == nullptr) {
        spdlog::info(
            "CLTLoginState_WorldListPending::Slot3_BeginOrContinue missing auth connection object currentState={}",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    const uint8_t payload[] = {CLTLoginMediator::kAuthRawCodeGetWorldListRequest};
    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildVariableLengthPacket(
            payload,
            sizeof(payload),
            mxo::auth::kFrameModeAuto,
            &packet)) {
        spdlog::info("DIAGNOSTIC: CLTLoginState_WorldListPending::Slot3_BeginOrContinue failed to build AS_GetWorldListRequest");
        return 0u;
    }

    const uint32_t sendResult = connection->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    mediator->PostEvent(0x1bu);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_WorldListPending::Slot3_BeginOrContinue built raw-0x35 packet headerLen={} payloadLen={} byteCount={} currentState={} -> sendResult=0x{:08x} then posts event=0x1b",
        packet.headerBytes.size(),
        packet.payloadBytes.size(),
        packet.bytes.size(),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(sendResult));
    return sendResult;
}

// anchor: launcher.exe:0x0043d4d0 (string/file anchors: loginstate.cpp, CLTLoginState_WorldListPending::AuthMessageDispatch())
uint32_t CLTLoginState_WorldListPending::AuthMessageDispatch(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    // Current best contextual role from the vtable and string anchors:
    // - vtable 0x004b4fec / slot 5
    // - AS_GetWorldListReply / AS_PSGetWorldListReply
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
uint32_t CLTLoginState_WorldListPending::Slot7_GetStateId() const {
    return 14;
}

}  // namespace mxo::ltlogin
