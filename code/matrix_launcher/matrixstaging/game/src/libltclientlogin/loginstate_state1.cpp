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
    // Original entry: no null checks - directly accesses g_CurrentLoginMediator and workItem
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    (void)mediator;
    if (!workItem || !mediator) {
        return 0u;
    }
    // Note: source-added safety check above not in original (assembly directly uses inputs)

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

    // Assembly flow: read work-item payload (offset +8 from workItem header)
    // 0x004390d3: CALL GetStatusOrPayloadDword -> store to [mediator+0x80]
    // 0x004390e0: CALL GetStatusOrPayloadDword again -> TEST result
    // The second result is NOT stored, just used for zero-check
    const uint32_t workResultCode = LoginState1WorkItemPayloadScaffold(workItem);
    g_CurrentLoginMediator->worldListCountOrStatus80 = workResultCode;
    (void)LoginState1WorkItemPayloadScaffold(workItem);  // 2nd call (matches assembly: unused result)

    // Assembly branches on: payload == 0 vs payload != 0
    // There's only ONE success path in original: workResultCode == 0
    // The 0x07000001 branch is source-added replacement logic (liveSuccessAlias)
    const bool originalZeroStatusSuccess = (workResultCode == 0u);
    // Source-added: live bridge success alias for 0x07000001 in replacement flow
    const bool liveSuccessAlias = (workResultCode == CLTLoginMediator::kConnectStatusSuccess);

    if (originalZeroStatusSuccess) {
        // Original flow at 0x00439147-0x0043916c:
        // - Get cached upstream state from this+4
        // - Call vtable+0x18 on it to get state ID
        // - Call SetCurrentState(mediator, stateId) at 0x0041b450
        // - Call PostEvent(mediator, 0) at 0x0041cfb0
        CLTLoginState* const cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
        if (cachedUpstreamState != nullptr) {
            mediator->currentState_ = cachedUpstreamState;
            cachedUpstreamState->Slot3_BeginOrContinue(this);
        }
        mediator->PostEvent(0u);
        return 1u;
    }

    // All non-zero status values fall into this path in original binary
    // Source splits into: 0x07000001 -> liveSuccessAlias path, else -> retry/error
    if (liveSuccessAlias) {
        // Source-added: live replacement flow with auth flag + bootstrap child
        mediator->authConnectionFlag2c_ = 1u;
        CLTLoginState* const cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
        if (cachedUpstreamState != nullptr) {
            CLTLoginState* oldState = this;
            mediator->currentState_ = cachedUpstreamState;
            return cachedUpstreamState->Slot3_BeginOrContinue(oldState);
        }
        // No cached upstream: continue to bootstrap child dispatch
        auto* child = &mediator->AuthBootstrapChild680();
        void* sendTarget = mediator->AuthConnection();
        const char* sessionToken = mediator->ownerAuthBootstrapSource94_.sessionToken60.begin;
        return child->PrepareAndDispatch(*mediator, sendTarget, sessionToken);
    }

    // Original: non-zero status retry/error path at 0x004390e7-0x00439144
    g_CurrentLoginMediator->authConnectionFlag2c_ = 1u;
    const uint32_t candidateCount = mediator->AuthConnectCandidateCountScaffold();
    const uint32_t attemptCount = mediator->authConnectAttemptCount28_;
    if (attemptCount < candidateCount) {
        // Original: retry this slot3 with cached upstream (this+4)
        ++mediator->authConnectAttemptCount28_;
        const uint32_t retryResult = Slot3_BeginOrContinue(cachedUpstreamOrArg_);
        spdlog::info(
            "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} cachedUpstream={} attemptCount28={} candidateCount={} -> retry state1 slot3 result=0x{:08x}",
            static_cast<unsigned>(workResultCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(attemptCount),
            static_cast<unsigned>(candidateCount),
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
        "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} retry exhausted cachedUpstream={} attemptCount28={} candidateCount={} -> currentState={} then PostError(0x00)",
        static_cast<unsigned>(workResultCode),
        fmt::ptr(cachedUpstreamOrArg_),
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
