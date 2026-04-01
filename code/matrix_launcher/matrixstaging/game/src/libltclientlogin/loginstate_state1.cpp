#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// UNANCHORED: source debug-name helper for launcher.exe vtable 0x004b4fc4.
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Tightened current read from decompilation + direct disassembly:
    // - consume only type-2 auth connect-status payload already cached by the mediator
    // - always mirror that payload into owner `+0x80`
    // - exact original branch split is payload-zero vs payload-non-zero
    //   - zero      -> switch back to the cached upstream helper, let `0x41b450` re-enter that
    //                  helper's slot 3 with old-state `this`, then post event `0`
    //   - non-zero  -> set owner byte `+0x2c`, then either retry state1 slot 3 using the
    //                  owner `+0x28/+0x4c/+0x50` iterator/count surface or reset to state0 and
    //                  post error `0`
    // - current replacement happy path still produces `0x07000001` for connect success, so keep a
    //   narrow live success alias for that code while preserving the exact recovered helper
    //   switch/event/error shape
    const uint32_t workResultCode = mediator->LastAuthConnectStatus();
    mediator->WorldListCountOrStatus80() = workResultCode;

    CLTLoginState* const cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    const bool originalZeroStatusSuccess = (workResultCode == 0u);
    const bool liveSuccessAlias = (workResultCode == CLTLoginMediator::kConnectStatusSuccess);

    if (originalZeroStatusSuccess) {
        uint32_t switchDispatchResult = 0u;
        if (cachedUpstreamState != nullptr && cachedUpstreamPhaseCode != 0u) {
            if (cachedUpstreamPhaseCode == 2u) {
                spdlog::info(
                    "ROUTE CHECKPOINT: early-auth original state1 success-side restore -> state2 cachedUpstream={} currentStateBeforeRestore={} (0x41b450 then re-enters state2 slot3 with oldState=state1)",
                    fmt::ptr(cachedUpstreamOrArg_),
                    mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
            }
            switchDispatchResult = mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                cachedUpstreamPhaseCode,
                cachedUpstreamState,
                this,
                "State1 slot1 zero-status success -> restore cached upstream and re-enter new helper slot3");
        } else {
            spdlog::info(
                "CLTLoginState_State1::Slot1_HandlePrimaryGate original zero-status success missing cached upstream cachedUpstream={} upstreamPhaseCode={} currentState={}",
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(cachedUpstreamPhaseCode),
                mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        }

        mediator->PostEventScaffold(0u);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate status=0x{:08x} successMode=original-zero-status cachedUpstream={} upstreamPhaseCode={} -> currentState={} switchDispatchResult=0x{:08x} then PostEvent(0x00)",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
            static_cast<unsigned>(switchDispatchResult));
        return 1u;
    }

    if (liveSuccessAlias) {
        mediator->AuthConnectionFlag2c() = 1u;

        if (cachedUpstreamState != nullptr && cachedUpstreamPhaseCode != 0u) {
            // Keep the active replacement happy path source-owned without pretending the original
            // zero-status success path behaves this way: the live bridge still restores the cached
            // helper object directly, then enters the owner+0x680 auth bootstrap child.
            if (cachedUpstreamPhaseCode == 2u) {
                spdlog::info(
                    "ROUTE CHECKPOINT: early-auth live state1 success alias -> state2 phase2-bootstrap-child resume cachedUpstream={} currentStateBeforeRestore={}",
                    fmt::ptr(cachedUpstreamOrArg_),
                    mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
            }
            mediator->SwitchHelperStateScaffold(cachedUpstreamPhaseCode, cachedUpstreamState);
        }

        const uint32_t handshakeResult = mediator->AuthBootstrapChild680().PrepareAndDispatch(*mediator);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate status=0x{:08x} successMode=live-0x07000001-success-alias cachedUpstream={} upstreamPhaseCode={} -> currentState={} authFlag2c={} owner+0x680::PrepareAndDispatch=0x{:08x}",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
            static_cast<unsigned>(mediator->AuthConnectionFlag2c()),
            static_cast<unsigned>(handshakeResult));
        return handshakeResult;
    }

    mediator->AuthConnectionFlag2c() = 1u;

    const uint32_t attemptCount = mediator->AuthConnectAttemptCountScaffold();
    const uint32_t candidateCount = mediator->AuthConnectCandidateCountScaffold();
    if (mediator->HasAuthConnectRetryCandidateRemainingScaffold()) {
        const uint32_t retryResult = Slot3_BeginOrContinue(cachedUpstreamOrArg_, mediator);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} cachedUpstream={} upstreamPhaseCode={} attemptCount28={} candidateCount={} authFlag2c={} -> retry state1 slot3 result=0x{:08x}",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            static_cast<unsigned>(attemptCount),
            static_cast<unsigned>(candidateCount),
            static_cast<unsigned>(mediator->AuthConnectionFlag2c()),
            static_cast<unsigned>(retryResult));
        return 1u;
    }

    mediator->ResetAuthConnectRetryStateScaffold();
    const uint32_t resetResult = mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
        0u,
        mediator->ScaffoldState0(),
        this,
        "State1 slot1 retry exhausted -> state0 / error0");
    mediator->PostErrorScaffold(0u);
    spdlog::info(
        "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} retry exhausted cachedUpstream={} upstreamPhaseCode={} attemptCount28={} candidateCount={} -> currentState={} resetResult=0x{:08x} then PostError(0x00)",
        static_cast<unsigned>(workResultCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        static_cast<unsigned>(attemptCount),
        static_cast<unsigned>(candidateCount),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(resetResult));
    return 1u;
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
