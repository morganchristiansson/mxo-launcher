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
    // Source-added null-check (original directly used inputs)
    if (!workItem || !g_CurrentLoginMediator) {
        return 0u;
    }

    const auto* workHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    const uint32_t workType = workHeader ? workHeader->workType : 0u;

    // Assembly flow:
    // - if work type != 2: tail-call base CLTLoginState::Slot1_HandlePrimaryGate (0x438d80)
    // - if work type == 2: read payload, store to owner+0x80, TEST for zero
    // - payload == 0: switch to cached upstream, re-enter that state's slot3, PostEvent(0)
    // - payload != 0: set owner+0x2c=1, retry or exhaust/error
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
    if (workResultCode == 0u) {
        // Original flow at 0x00439147-0x0043916c:
        // - Get cached upstream state from this+4: [EDI+4]
        // - Call vtable+0x18 on cached upstream to get state ID
        // - Call SetCurrentState(mediator, stateId) at 0x0041b450
        // - Call PostEvent(mediator, 0) at 0x0041cfb0
        CLTLoginState* const cachedUpstreamState = static_cast<CLTLoginState*>(cachedUpstreamOrArg_);
        if (cachedUpstreamState != nullptr) {
            g_CurrentLoginMediator->currentState_ = cachedUpstreamState;
            cachedUpstreamState->Slot3_BeginOrContinue(this);
        }
        g_CurrentLoginMediator->PostEvent(0u);
        return 1u;
    }

    // Original: non-zero status retry/error path at 0x004390e7-0x00439144:
    // - Set owner+0x2c = 1 (authConnectionFlag)
    // - Compare attemptCount (owner+0x28) vs candidateCount ((owner+0x50-owner+0x4c)>>2)
    // - If attempts remain: retry this slot3 with this+4 (cached upstream), increment count
    // - If exhausted: reset count, switch to state0, PostError(0)
    g_CurrentLoginMediator->authConnectionFlag2c_ = 1u;
    const uint32_t candidateCount = g_CurrentLoginMediator->AuthConnectCandidateCountScaffold();
    const uint32_t attemptCount = g_CurrentLoginMediator->authConnectAttemptCount28_;
    if (attemptCount < candidateCount) {
        // Original: retry this slot3 with this+4 (cached upstream)
        ++g_CurrentLoginMediator->authConnectAttemptCount28_;
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

    // Original: reset attempt count (owner+0x28=0), switch to state0, PostError(0)
    g_CurrentLoginMediator->authConnectAttemptCount28_ = 0;
    CLTLoginState* newState = g_CurrentLoginMediator->LoginHelperStateByIdScaffold(0u);
    g_CurrentLoginMediator->currentState_ = newState;
    if (newState) {
        newState->Slot3_BeginOrContinue(this);
    }
    g_CurrentLoginMediator->PostError(0u);
    spdlog::info(
        "CLTLoginState_State1::Slot1_HandlePrimaryGate non-zero status=0x{:08x} retry exhausted cachedUpstream={} attemptCount28={} candidateCount={} -> currentState={} then PostError(0x00)",
        static_cast<unsigned>(workResultCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(attemptCount),
        static_cast<unsigned>(candidateCount),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
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
