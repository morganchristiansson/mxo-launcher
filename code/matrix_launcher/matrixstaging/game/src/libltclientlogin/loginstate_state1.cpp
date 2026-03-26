#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b4fc4
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Current focused source-owned tightening:
    // - original `0x4390b0` consumes a type-2 auth connect-status work item and uses its payload
    //   to update owner `+0x80`
    // - current mediator contract keeps that payload recording at the owner boundary first, then
    //   re-enters state1 slot 1 so this body can consume `LastAuthConnectStatus()` locally
    // - it also has a broader owner `+0x28/+0x4c/+0x50` retry/error path that we still do not
    //   model faithfully on the active replacement path
    // - keep the current refactor focused by moving the already-live auth-connect continuation
    //   ownership here while leaving the not-yet-faithful retry iterator explicitly documented
    const uint32_t workResultCode = mediator->LastAuthConnectStatus();
    mediator->WorldListCountOrStatus80() = workResultCode;

    if (workResultCode != CLTLoginMediator::kConnectStatusSuccess) {
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate observed auth connect-status=0x{:08x} cachedUpstream={} currentState={} (original 0x4390b0 also has a broader owner+0x28/+0x4c/+0x50 retry/error branch that is not source-owned yet)",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    mediator->AuthConnectionFlag2c() = 1u;

    CLTLoginState* cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    if (cachedUpstreamState != nullptr && cachedUpstreamPhaseCode != 0u) {
        // Current focused non-broad tightening for the active existing-character path:
        // restore the cached upstream helper before the later launcher-owned auth handshake so
        // raw `0x09/0x0b` handling keeps seeing the same state-local continuation (`state8` on the
        // live path). Original `0x4390b0` also posts event `0` on this success side, but that
        // broader event/retry/error surface is still intentionally deferred.
        if (cachedUpstreamPhaseCode == 2u) {
            spdlog::info(
                "ROUTE CHECKPOINT: early-auth state1 -> state2 phase2-bootstrap-child resume cachedUpstream={} currentStateBeforeRestore={}",
                fmt::ptr(cachedUpstreamOrArg_),
                mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        }
        mediator->SwitchHelperStateScaffold(cachedUpstreamPhaseCode, cachedUpstreamState);
    }

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth state1 connect-success -> phase2 bootstrap child cachedUpstream={} upstreamPhaseCode={} currentState={} authFlag2c={}",
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->AuthConnectionFlag2c()));
    const uint32_t handshakeResult = mediator->BeginAuthHandshake();
    spdlog::info(
        "CLTLoginState_State1::Slot1_HandlePrimaryGate bridged auth connect-status=0x{:08x} cachedUpstream={} upstreamPhaseCode={} -> currentState={} authFlag2c={} BeginAuthHandshake(owner+0x680 child)=0x{:08x}",
        static_cast<unsigned>(workResultCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->AuthConnectionFlag2c()),
        static_cast<unsigned>(handshakeResult));
    return handshakeResult;
}

// anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
uint32_t CLTLoginState_State1::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    cachedUpstreamOrArg_ = upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    const uint32_t connectResult = mediator->BeginAuthConnection();
    spdlog::info(
        "CLTLoginState_State1::Slot3_BeginOrContinue cachedUpstream={} upstreamPhaseCode={} currentState={} -> BeginAuthConnection=0x{:08x}",
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_)),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(connectResult));
    return connectResult;
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
uint32_t CLTLoginState_State1::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    if (mediator != nullptr) {
        mediator->WorldListCountOrStatus80() = 0x12000005u;
    }
    spdlog::info(
        "CLTLoginState_State1::Slot6_HandleSecondaryMessage workItem={} set owner+0x80=0x12000005",
        fmt::ptr(workItem));
    return 0u;
}

// anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
uint32_t CLTLoginState_State1::Slot7_GetStateId() const {
    return 1;
}

}  // namespace mxo::ltlogin
