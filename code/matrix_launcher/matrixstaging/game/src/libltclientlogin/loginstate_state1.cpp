#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

static uint32_t LoginState1WorkItemPayloadScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* payload = static_cast<const uint32_t*>(workItem);
    return payload[2];
}

}  // namespace

// UNANCHORED: source debug-name helper for launcher.exe vtable 0x004b4fc4.
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!workItem || !mediator) {
        return 0u;
    }

    const auto* workHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    const uint32_t workType = workHeader ? workHeader->workType : 0u;

    // Tightened current read from decompilation + direct disassembly:
    // - `0x41af80` re-enters raw state1 slot 1 on every unconsumed auth completion work item
    // - if that work item is not type `2`, state1 tail-calls shared slot-1 gate `0x438d80`
    // - only the type-2 branch consumes the cached auth connect-status payload, mirrors it into
    //   owner `+0x80`, and then splits on payload zero-vs-non-zero
    //   - zero      -> switch back to the cached upstream helper, let `0x41b450` re-enter that
    //                  helper's slot 3 with old-state `this`, then post event `0`
    //   - non-zero  -> set owner byte `+0x2c`, then either retry state1 slot 3 using the
    //                  owner `+0x28/+0x4c/+0x50` iterator/count surface or reset to state0 and
    //                  post error `0`
    // - current replacement happy path still produces `0x07000001` for connect success, so keep a
    //   narrow live success alias for that code while preserving the exact recovered helper
    //   switch/event/error shape
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        return CLTLoginState::Slot1_HandlePrimaryGate(workItem);
    }

    const uint32_t workResultCode = LoginState1WorkItemPayloadScaffold(workItem);
    mediator->worldListCountOrStatus80 = workResultCode;

    CLTLoginState* const cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    const bool originalZeroStatusSuccess = (workResultCode == 0u);
    const bool liveSuccessAlias = (workResultCode == CLTLoginMediator::kConnectStatusSuccess);

    if (originalZeroStatusSuccess) {
        // Original 0x41b450: switch to cached upstream state object, then re-enter its slot3
        // with old state passed as argument. Source now mirrors that exactly.
        if (cachedUpstreamState != nullptr && cachedUpstreamPhaseCode != 0u) {
            if (cachedUpstreamPhaseCode == 2u) {
                spdlog::info(
                    "ROUTE CHECKPOINT: early-auth original state1 success-side restore -> state2 cachedUpstream={} currentStateBeforeRestore={} (0x41b450 then re-enters state2 slot3 with oldState=state1)",
                    fmt::ptr(cachedUpstreamOrArg_),
                    mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
            }
            // Original: SwitchHelperState(value) sets currentState10 and calls new state's slot3
            mediator->currentState_ = cachedUpstreamState;
            cachedUpstreamState->Slot3_BeginOrContinue(this);
        } else {
            spdlog::info(
                "CLTLoginState_State1::Slot1_HandlePrimaryGate original zero-status success missing cached upstream cachedUpstream={} upstreamPhaseCode={} currentState={}",
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(cachedUpstreamPhaseCode),
                mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        }

        mediator->PostEvent(0u);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate status=0x{:08x} successMode=original-zero-status cachedUpstream={} upstreamPhaseCode={} -> currentState={} then PostEvent(0x00)",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (liveSuccessAlias) {
        mediator->authConnectionFlag2c_ = 1u;

        if (cachedUpstreamState != nullptr && cachedUpstreamPhaseCode != 0u) {
            // Keep the active replacement happy path source-owned without pretending the original
            // zero-status success path behaves this way: the live bridge still restores the cached
            // helper object directly, then enters the owner+0x680 auth bootstrap child.
            if (cachedUpstreamPhaseCode == 2u) {
                spdlog::info(
                    "ROUTE CHECKPOINT: early-auth live state1 success alias -> state2 phase2-bootstrap-child resume cachedUpstream={} currentStateBeforeRestore={}",
                    fmt::ptr(cachedUpstreamOrArg_),
                    mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
            }
            // Original: SwitchHelperState(value) - copies state pointer from table, then new state's slot3
            CLTLoginState* oldState = this;
            mediator->currentState_ = cachedUpstreamState;
            const uint32_t resumeResult = cachedUpstreamState->Slot3_BeginOrContinue(oldState);
            spdlog::info(
                "CLTLoginState_State1::Slot1_HandlePrimaryGate status=0x{:08x} successMode=live-0x07000001-success-alias cachedUpstream={} upstreamPhaseCode={} -> currentState={} authFlag2c={} resumeResult=0x{:08x}",
                static_cast<unsigned>(workResultCode),
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(cachedUpstreamPhaseCode),
                mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
                static_cast<unsigned>(mediator->authConnectionFlag2c_),
                static_cast<unsigned>(resumeResult));
            return resumeResult;
        }

        // Ready-side: exact call shape matched to assembly
        auto* child = &mediator->AuthBootstrapChild680();
        void* sendTarget = mediator->AuthConnection();
        const char* sessionToken = mediator->ownerAuthBootstrapSource94_.sessionToken60.begin;
        const uint32_t handshakeResult = child->PrepareAndDispatch(*mediator, sendTarget, sessionToken);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate status=0x{:08x} successMode=live-0x07000001-success-alias cachedUpstream={} upstreamPhaseCode={} -> currentState={} authFlag2c={} owner+0x680::PrepareAndDispatch=0x{:08x}",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
            static_cast<unsigned>(mediator->authConnectionFlag2c_),
            static_cast<unsigned>(handshakeResult));
        return handshakeResult;
    }

    mediator->authConnectionFlag2c_ = 1u;

    // Original: retry if attemptCount < ((addressListEnd - addressListBegin) >> 2)
    const uint32_t candidateCount = mediator->AuthConnectCandidateCountScaffold();
    const uint32_t attemptCount = mediator->authConnectAttemptCount28_;
    if (attemptCount < candidateCount) {
        // Original: retry this slot3 with cached upstream (this+4)
        ++mediator->authConnectAttemptCount28_;
        const uint32_t retryResult = Slot3_BeginOrContinue(cachedUpstreamOrArg_);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} cachedUpstream={} upstreamPhaseCode={} attemptCount28={} candidateCount={} authFlag2c={} -> retry state1 slot3 result=0x{:08x}",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            static_cast<unsigned>(attemptCount),
            static_cast<unsigned>(candidateCount),
            static_cast<unsigned>(mediator->authConnectionFlag2c_),
            static_cast<unsigned>(retryResult));
        return 1u;
    }

    // Original: reset attempt count and switch to state 0
    // Original: SwitchHelperState(0) - switch to state0 from g_LoginHelperState0[0]
    mediator->authConnectAttemptCount28_ = 0;
    CLTLoginState* newState = mediator->LoginHelperStateByIdScaffold(0u);
    mediator->currentState_ = newState;
    if (newState) {
        newState->Slot3_BeginOrContinue(this);
    }
    mediator->PostError(0u);
    spdlog::info(
        "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} retry exhausted cachedUpstream={} upstreamPhaseCode={} attemptCount28={} candidateCount={} -> currentState={} then PostError(0x00)",
        static_cast<unsigned>(workResultCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        static_cast<unsigned>(attemptCount),
        static_cast<unsigned>(candidateCount),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
uint32_t CLTLoginState_State1::Slot3_BeginOrContinue(void* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    cachedUpstreamOrArg_ = upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    const uint32_t connectResult = mediator->BeginAuthConnection();
    spdlog::info(
        "CLTLoginState_State1::Slot3_BeginOrContinue cachedUpstream={} upstreamPhaseCode={} currentState={} -> BeginAuthConnection=0x{:08x}",
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_)),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(connectResult));
    return connectResult;
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
uint32_t CLTLoginState_State1::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (mediator != nullptr) {
        mediator->worldListCountOrStatus80 = 0x12000005u;
    }
    spdlog::info(
        "CLTLoginState_State1::Slot6_HandleSecondaryMessage workItem={} set owner+0x80=0x12000005",
        fmt::ptr(workItem));
    return 0u;
}

// anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
uint32_t CLTLoginState_State1::GetStateId() const {
    return 1;
}

}  // namespace mxo::ltlogin
