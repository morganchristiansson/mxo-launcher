#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
uint32_t CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Current evidence-backed narrow scaffold for helper/state 2:
    // - `0x439210` is the strongest current BeginAuthBootstrap entry
    // - it caches the incoming upstream/helper unless that object's phase/state code is already 1
    // - on the connected branch it reaches the shared auth bootstrap dispatcher `0x448050`
    // Current source ownership stays intentionally narrow here:
    // - preserve the upstream-caching contract needed for future state-2 cleanup
    // - only execute the already-owned connected bootstrap send side through
    //   `CLTLoginMediator::BeginAuthHandshake()`
    // - leave the broader non-connected / retry / helper-switch details explicitly deferred
    const uint32_t incomingUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
    if (incomingUpstreamPhaseCode != 1u) {
        cachedUpstreamOrArg_ = upstreamOrArg;
    }

    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    if (mediator->AuthConnectionFlag2c() == 0u) {
        spdlog::info(
            "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue blocked on owner+0x2c==0 incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} (connected auth-bootstrap branch only is source-owned here)",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    const uint32_t sendResult = mediator->BeginAuthHandshake();
    spdlog::info(
        "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authFlag2c={} -> BeginAuthHandshake=0x{:08x}",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->AuthConnectionFlag2c()),
        static_cast<unsigned>(sendResult));
    return sendResult;
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best recovered role from `0x43f300`:
    // - parses an auth-side message through the mediator-owned `+0x680` helper object
    // - on the success branch (`case 2`) it performs the broader post-auth table writeback before
    //   the narrower helper10 selected-slot path becomes relevant
    // - concrete broader writeback now looks like:
    //   - build owner `+0xd84` as a world-descriptor table
    //   - validate world status/type there
    //   - build owner `+0x688` as a character-slot record table
    //   - validate character status there
    //   - seed owner `+0x818` by matching each character record's world id against the
    //     world-descriptor table and copying the descriptor name
    //   - post event `5`, then switch helper state based on the current helper object
    // Current source ownership note:
    // - the replacement launcher now mirrors the reconstructed `+0xd84/+0x688/+0x818` families
    //   inside `CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState()` so later state-8
    //   margin dispatch can consume reconstructed data instead of only fallback state
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::Slot7_GetStateId() const {
    return 2;
}

}  // namespace mxo::ltlogin
