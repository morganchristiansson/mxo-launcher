#include "loginmediator.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace mxo::ltlogin {
namespace {

// Focused post-state9 event/listener split:
// - keep the immediate `0x41b450 -> 0x41cfb0` continuation in its own TU
// - this avoids reopening the broader mediator auth/bootstrap transport file just to work on the
//   late state-`0x0c` bridge
// - canonical reference:
//   `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
static std::string BuildRecentEventHistoryPreview(const std::array<uint32_t, 8>& events, uint32_t count) {
    if (count == 0u) {
        return "[]";
    }

    const uint32_t boundedCount = std::min<uint32_t>(count, static_cast<uint32_t>(events.size()));
    std::string out = "[";
    char buffer[16] = {};
    for (uint32_t i = 0; i < boundedCount; ++i) {
        if (i != 0u) {
            out += ", ";
        }
        std::snprintf(buffer, sizeof(buffer), "0x%02x", static_cast<unsigned>(events[i] & 0xffu));
        out += buffer;
    }
    out += "]";
    return out;
}

}  // namespace

void CLTLoginMediator::SwitchHelperStateScaffold(uint32_t helperStateId, CLTLoginState* state) {
    // anchor: launcher.exe:0x41b450
    // Exact recovered shape from the current Ghidra pass:
    // - if an old helper exists, call its vtable `+0x0c` with the new helper object
    // - install the dispatch-table target into owner `+0x10`
    // - then call the new helper's vtable `+0x08` with the old helper object
    // Current source scaffold keeps that boundary explicit and records the target helper id, but
    // does not yet claim the exact old/new helper notification slot semantics beyond the proven
    // call shape.
    lastSwitchedHelperStateScaffold_ = helperStateId;
    CLTLoginState* oldState = currentState_;
    if (!state) {
        spdlog::info(
            "CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState=<null> (source scaffold leaves currentState unchanged)",
            static_cast<unsigned>(helperStateId),
            oldState ? oldState->DebugName() : "<null>");
        Log(
            "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x%02x oldState=%s newState=<null>",
            (unsigned)(helperStateId & 0xffu),
            oldState ? oldState->DebugName() : "<null>");
        return;
    }

    currentState_ = state;
    spdlog::info(
        "CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState={} (anchor: launcher.exe:0x41b450; original also performs old/new helper notification calls around the install)",
        static_cast<unsigned>(helperStateId),
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName());
    Log(
        "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x%02x oldState=%s newState=%s",
        (unsigned)(helperStateId & 0xffu),
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName());
}

void CLTLoginMediator::PostEventScaffold(uint32_t eventId) {
    // anchor: launcher.exe:0x41cfb0
    // Current post-state9 continuation read:
    // - this is not a trivial logger
    // - original walks the owner `+0x674` listener tree and calls each observer callback
    // - `0x43c180` success returns only after that synchronous listener walk:
    //   `0x41b420 -> 0x41b450(0x0c) -> 0x41cfb0(0x18)`
    // - no natural hit is proven yet on `0x004397e0` / `0x0041c5c0` on that same continuation,
    //   so the immediate next path is best treated as observer/listener work first
    // - strongest current later `0x0f` bridge is narrower now too:
    //   - `0x41b420` sets owner byte `+0x2d = 1`
    //   - shared gate `0x438df0` posts event `0x0f` when owner `+0x2d != 0`
    //   - static `CLTEvilBlockingLoginObserver::WaitForEvent` callers currently prove waits for
    //     events `1`, `8`, and `0x0f`, but not for `0x18`
    // - practical scaffold consequence:
    //   keep explicit event-history logging here so replacement-launcher runs can be compared
    //   against the natural original event sequence without pretending the listener tree is already
    //   reconstructed.
    lastPostedEventScaffold_ = eventId;
    if (recentPostedEventCountScaffold_ < recentPostedEventsScaffold_.size()) {
        recentPostedEventsScaffold_[recentPostedEventCountScaffold_++] = eventId;
    } else {
        std::move(
            recentPostedEventsScaffold_.begin() + 1,
            recentPostedEventsScaffold_.end(),
            recentPostedEventsScaffold_.begin());
        recentPostedEventsScaffold_.back() = eventId;
    }
    const std::string recentEventsPreview =
        BuildRecentEventHistoryPreview(recentPostedEventsScaffold_, recentPostedEventCountScaffold_);
    spdlog::info(
        "{} Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={} (listener tree at owner+0x674 not yet scaffolded)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview);
    Log(
        "DIAGNOSTIC: CLTLoginMediator::PostEvent() Event# %u currentState=%s lastSwitch=0x%02x recentEvents=%s",
        (unsigned)eventId,
        currentState_ ? currentState_->DebugName() : "<null>",
        (unsigned)(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview.c_str());

    // Narrow source-owned continuation bridge for the now-live state8 -> helper9/state9 path:
    // - natural original switches to helper9, then posts event `0x0b`, and helper9 slot 3
    //   (`0x439780`) is immediately part of the same active progression family
    // - the full owner `+0x674` listener tree behind `0x41cfb0` is still unresolved
    // - keep this bridge narrow to the already-proven helper9 handoff instead of claiming a
    //   general event-listener reconstruction
    if (eventId == 0x0bu && currentState_ != nullptr && currentState_->DispatchPhaseCode() == 9u) {
        const uint32_t continueResult = currentState_->Slot3_BeginOrContinue(currentState_, this);
        spdlog::info(
            "CLTLoginMediator::PostEventScaffold narrow helper9 continuation bridge event=0x0b currentState={} -> slot3Result=0x{:08x}",
            currentState_->DebugName(),
            static_cast<unsigned>(continueResult));
        Log(
            "DIAGNOSTIC: CLTLoginMediator::PostEvent() narrow helper9 continuation bridge event=0x0b currentState=%s -> slot3Result=0x%08x",
            currentState_->DebugName(),
            (unsigned)continueResult);
    }
}

void CLTLoginMediator::PostErrorScaffold(uint32_t errorId) {
    lastPostedErrorScaffold_ = errorId;
    spdlog::info("{} Error# {}", kLogPrefixPostError, static_cast<unsigned>(errorId));
}

}  // namespace mxo::ltlogin
