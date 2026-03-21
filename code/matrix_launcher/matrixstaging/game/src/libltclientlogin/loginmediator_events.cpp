#include "loginmediator.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

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

using LoginObserverOnEventFn = void(__thiscall*)(void*, uint32_t);

static std::vector<void*> g_registeredLoginObservers;

static std::vector<void*>& MutableRegisteredLoginObservers() {
    return g_registeredLoginObservers;
}

static const std::vector<void*>& RegisteredLoginObservers() {
    return g_registeredLoginObservers;
}

static bool LooksLikeLoginObserverEventVtable(void* observer) {
    if (!observer) {
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(observer);
    return vtable != nullptr && vtable[0] != nullptr;
}

static void DispatchLoginObserverEvent(void* observer, uint32_t eventId) {
    if (!LooksLikeLoginObserverEventVtable(observer)) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(observer);
    const auto fn = reinterpret_cast<LoginObserverOnEventFn>(vtable[0]);
    fn(observer, eventId);
}

}  // namespace

void CLTLoginMediator::SwitchHelperStateScaffold(uint32_t helperStateId, CLTLoginState* state) {
    // anchor: launcher.exe:0x41b450
    // Tightened recovered shape from the current Ghidra pass plus direct vtable reads:
    // - if an old helper exists, call its vtable `+0x0c` with the new helper object
    // - install the dispatch-table target into owner `+0x10`
    // - then call the new helper's vtable `+0x08` with the old helper object
    // Because the concrete login-state vtables begin directly at slot 1, those offsets now map to:
    // - old helper `+0x0c` -> slot 4
    // - new helper `+0x08` -> slot 3 / BeginOrContinue
    // Active post-state9 consequence:
    // - on the state9 -> state12 switch, old helper slot 4 is the shared tiny stub
    // - new helper state12 slot 3 is also the shared tiny stub
    // - so the immediate post-state9 work is not another hidden state body inside `0x41b450`; it
    //   returns to the caller and the next concrete work stays the explicit `0x41cfb0(0x18)`
    //   observer/listener walk.
    lastSwitchedHelperStateScaffold_ = helperStateId;
    CLTLoginState* oldState = currentState_;
    if (!state) {
        spdlog::info(
            "CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState=<null> (source scaffold leaves currentState unchanged)",
            static_cast<unsigned>(helperStateId),
            oldState ? oldState->DebugName() : "<null>");
        spdlog::info(
            "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState=<null>",
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
    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState={}",
        (unsigned)(helperStateId & 0xffu),
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName());
}

bool CLTLoginMediator::RegisterLoginObserverScaffold(void* observer) {
    // anchor: launcher.exe:0x41ddb0
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x170` as insertion into the owner `+0x674` listener tree, not as a startup
    // context handoff.
    if (!observer) {
        return false;
    }

    std::vector<void*>& observers = MutableRegisteredLoginObservers();
    const auto it = std::find(
        observers.begin(),
        observers.end(),
        observer);
    if (it != observers.end()) {
        spdlog::info(
            "CLTLoginMediator::RegisterLoginObserverScaffold observer={} already registered count={}",
            fmt::ptr(observer),
            static_cast<unsigned>(observers.size()));
        return false;
    }

    observers.push_back(observer);
    spdlog::info(
        "CLTLoginMediator::RegisterLoginObserverScaffold observer={} count={} (minimal source-owned bridge for owner+0x674 listener registration)",
        fmt::ptr(observer),
        static_cast<unsigned>(observers.size()));
    return true;
}

bool CLTLoginMediator::UnregisterLoginObserverScaffold(void* observer) {
    // anchor: launcher.exe:0x41dde0
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x174` as removal from the owner `+0x674` listener tree.
    if (!observer) {
        return false;
    }

    std::vector<void*>& observers = MutableRegisteredLoginObservers();
    const auto it = std::find(
        observers.begin(),
        observers.end(),
        observer);
    if (it == observers.end()) {
        spdlog::info(
            "CLTLoginMediator::UnregisterLoginObserverScaffold observer={} not found count={}",
            fmt::ptr(observer),
            static_cast<unsigned>(observers.size()));
        return false;
    }

    observers.erase(it);
    spdlog::info(
        "CLTLoginMediator::UnregisterLoginObserverScaffold observer={} count={}",
        fmt::ptr(observer),
        static_cast<unsigned>(observers.size()));
    return true;
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
    // - stronger later `0x0f` bridge is now live-backed too:
    //   - `0x41b420` sets owner byte `+0x2d = 1`
    //   - a late natural-original pass then hit shared gate `0x438df0`
    //   - live backtrace there showed caller `0x41afc0`, i.e. the margin-completion fallback that
    //     re-enters current helper vtable `+0x04` / current best read: slot 2
    //   - at that stop:
    //     - helper/object `this+4 = 1`
    //     - owner `DAT_004f78b8 + 0x2d = 1`
    //   - continuing from there immediately hit `0x41cfb0` with event `0x0f`
    //   - static `CLTEvilBlockingLoginObserver::WaitForEvent` callers currently prove waits for
    //     events `1`, `8`, and `0x0f`, but not for `0x18`
    // - practical scaffold consequence:
    //   keep explicit event-history logging here and bridge arg6 `+0x170/+0x174` observer
    //   registration into a minimal source-owned listener list, without pretending the original
    //   red-black-tree container at owner `+0x674` is fully reconstructed.
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
    const std::vector<void*>& registeredObservers = RegisteredLoginObservers();
    spdlog::info(
        "{} Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={} registeredObservers={} (minimal owner+0x674 observer bridge active; full tree container still not scaffolded)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview,
        static_cast<unsigned>(registeredObservers.size()));
    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::PostEvent() Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={}",
        (unsigned)eventId,
        currentState_ ? currentState_->DebugName() : "<null>",
        (unsigned)(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview.c_str());

    // Current implementation keeps observer dispatch deliberately narrow and evidence-backed:
    // - client-facing `+0x170/+0x174` registration is now source-owned
    // - newer `mcd.cfg` persistence tightening makes one more event concretely valuable here:
    //   original client-side save family `0x62199ed0 -> 0x62198fa0 -> 0x62197830` is reached
    //   from client event handler `0x621707e0` on event `0x0b` (and sibling `0x16`)
    // - active existing-character path posts `0x0b` immediately after the state8 -> helper9 switch
    // - so keep the bridge narrow, but now include `0x0b` alongside the already-proven later
    //   observer-driven `0x18/0x0f` work
    if (eventId == 0x0bu || eventId == 0x18u || eventId == 0x0fu) {
        const std::vector<void*> observers = registeredObservers;
        for (void* observer : observers) {
            spdlog::info(
                "CLTLoginMediator::PostEventScaffold dispatching observer={} event=0x{:02x}",
                fmt::ptr(observer),
                static_cast<unsigned>(eventId & 0xffu));
            DispatchLoginObserverEvent(observer, eventId);
        }
    }

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
        spdlog::info(
            "DIAGNOSTIC: CLTLoginMediator::PostEvent() narrow helper9 continuation bridge event=0x0b currentState={} -> slot3Result=0x{:08x}",
            currentState_->DebugName(),
            continueResult);
    }
}

void CLTLoginMediator::PostErrorScaffold(uint32_t errorId) {
    // anchor: launcher.exe:0x41d090
    // The original walks the owner `+0x674` listener tree here and calls each observer's second
    // vtable slot (`+0x04` / current best read: OnLoginError).
    // Current replacement keeps this late observer bridge narrower than events for now: error-side
    // fanout stays logged but not yet dispatched until the active event-`0x18` continuation proves
    // which registered observers are safe to call on the replacement path.
    lastPostedErrorScaffold_ = errorId;
    spdlog::info(
        "{} Error# {} registeredObservers={} (late observer bridge not yet enabled for errors)",
        kLogPrefixPostError,
        static_cast<unsigned>(errorId),
        static_cast<unsigned>(RegisteredLoginObservers().size()));
}

}  // namespace mxo::ltlogin
