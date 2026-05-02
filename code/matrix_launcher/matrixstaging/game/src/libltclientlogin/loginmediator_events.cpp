#include "loginmediator.h"
#include "loginmediator_events.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include <spdlog/spdlog.h>

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

namespace mxo::ltlogin {
namespace {

using LoginObserverSet674 = std::set<void*, std::less<void*>>;
using LoginObserverSetIterator674 = LoginObserverSet674::iterator;
using LoginObserverSetConstIterator674 = LoginObserverSet674::const_iterator;

struct LoginObserverSetBacking674 {
    LoginObserverSet674 observers;
};

static std::unordered_map<CLTLoginMediator*, LoginObserverSetBacking674>& LoginObserverSetBackings674() {
    static std::unordered_map<CLTLoginMediator*, LoginObserverSetBacking674> backings;
    return backings;
}

static LoginObserverSetBacking674& EnsureLoginObserverSetBacking674(CLTLoginMediator* owner) {
    return LoginObserverSetBackings674()[owner];
}

static LoginObserverSetBacking674* FindLoginObserverSetBacking674(const CLTLoginMediator* owner) {
    auto it = LoginObserverSetBackings674().find(const_cast<CLTLoginMediator*>(owner));
    return (it != LoginObserverSetBackings674().end()) ? &it->second : nullptr;
}

static void SynchronizeObserverTreeMirror674(CLTLoginMediator* owner) {
    if (!owner) {
        return;
    }

    LoginObserverSetBacking674& backing = EnsureLoginObserverSetBacking674(owner);
    owner->observerTree674_.header00 = &owner->observerTreeHeader674_;
    owner->observerTree674_.nodeCount04 = static_cast<uint32_t>(backing.observers.size());

    owner->observerTreeHeader674_.colorOrFlags00 = 0u;
    owner->observerTreeHeader674_.observerKey10 = nullptr;
    owner->observerTreeHeader674_.parent04 = nullptr;
    owner->observerTreeHeader674_.left08 = &owner->observerTreeHeader674_;
    owner->observerTreeHeader674_.right0c = &owner->observerTreeHeader674_;
}

static LoginObserverSetIterator674 ObserverSetBegin674(CLTLoginMediator* owner) {
    return EnsureLoginObserverSetBacking674(owner).observers.begin();
}

static LoginObserverSetIterator674 ObserverSetEnd674(CLTLoginMediator* owner) {
    return EnsureLoginObserverSetBacking674(owner).observers.end();
}

static LoginObserverSetConstIterator674 ObserverSetBegin674(const CLTLoginMediator* owner) {
    LoginObserverSetBacking674* backing = FindLoginObserverSetBacking674(owner);
    return backing ? backing->observers.begin() : LoginObserverSetConstIterator674{};
}

static LoginObserverSetConstIterator674 ObserverSetEnd674(const CLTLoginMediator* owner) {
    LoginObserverSetBacking674* backing = FindLoginObserverSetBacking674(owner);
    return backing ? backing->observers.end() : LoginObserverSetConstIterator674{};
}

// Legacy type aliases (used by CLTLoginMediator methods)
using LoginObserverOnEventFn = void(__thiscall*)(void*, uint32_t);
using LoginObserverOnErrorFn = void(__thiscall*)(void*, uint32_t);

} // namespace

// anchor-family: launcher.exe ctor/dtor field initialization of owner `+0x674`
void CLTLoginMediator::InitializeObserverTree674() {
    // Recovered caller semantics now read better as a plain unique observer set.
    // Keep the recovered owner `+0x674` header/count fields as a small mirror for logging and
    // layout-oriented diagnostics, while the source-owned behavior uses a `std::set<void*>`.
    LoginObserverSetBacking674& backing = EnsureLoginObserverSetBacking674(this);
    backing.observers.clear();
    SynchronizeObserverTreeMirror674(this);
}

// anchor: launcher.exe:0x419570 / 0x41d370
void CLTLoginMediator::ClearObserverTree674() {
    LoginObserverSetBacking674* backing = FindLoginObserverSetBacking674(this);
    if (backing) {
        backing->observers.clear();
    }
    SynchronizeObserverTreeMirror674(this);
}

// anchor-family: inlined `begin()` expression used by launcher.exe:0x41cfb0 / 0x41d090 / 0x41d430
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeBegin674() const {
    return observerTree674_.header00;
}

// anchor-family: inlined header/end expression used by launcher.exe:0x41cfb0 / 0x41d090 / 0x41d430
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeEnd674() const {
    return observerTree674_.header00;
}

LoginObserverTreeNode674* CLTLoginMediator::FindObserverNode674(void* observer) const {
    LoginObserverSetBacking674* backing = FindLoginObserverSetBacking674(this);
    if (!backing) {
        return nullptr;
    }
    return (backing->observers.find(observer) != backing->observers.end()) ? observerTree674_.header00 : nullptr;
}

uint32_t CLTLoginMediator::RemoveObserverNode674(void* observer) {
    LoginObserverSetBacking674* backing = FindLoginObserverSetBacking674(this);
    if (!backing) {
        return 0u;
    }
    const uint32_t erased = static_cast<uint32_t>(backing->observers.erase(observer));
    SynchronizeObserverTreeMirror674(this);
    return erased;
}

// anchor: launcher.exe:0x419510 / BuildEqualRangeForKey
void CLTLoginMediator::EqualRangeObserver674(
    void* observer,
    LoginObserverTreeNode674** outLowerBound,
    LoginObserverTreeNode674** outUpperBound) const {
    (void)observer;
    if (outLowerBound) {
        *outLowerBound = observerTree674_.header00;
    }
    if (outUpperBound) {
        *outUpperBound = observerTree674_.header00;
    }
}

// anchor: launcher.exe:0x415f20
bool CLTLoginMediator::InsertObserverNode674(void* observer) {
    if (!observer) {
        return false;
    }

    auto [_, inserted] = EnsureLoginObserverSetBacking674(this).observers.insert(observer);
    SynchronizeObserverTreeMirror674(this);
    return inserted;
}

// anchor: launcher.exe:0x41d430
void CLTLoginMediator::EraseObserverRange674(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last) {
    (void)first;
    (void)last;
    // `UnregisterLoginObserver()` now performs the set erase directly after computing the count.
    SynchronizeObserverTreeMirror674(this);
}

// anchor: launcher.exe:0x41b450
uint32_t CLTLoginMediator::SetCurrentState(uint32_t helperStateId) {
    CLTLoginState* const oldState = currentState_;
    CLTLoginState* const newState =
        (helperStateId < 20u)
            ? static_cast<CLTLoginState*>(reinterpret_cast<void* const*>(&g_LoginHelperDispatchTableScaffold.helper7868)[helperStateId])
            : nullptr;
    if (oldState != nullptr) {
        oldState->Slot4_NoOp();
    }

    if (newState == nullptr) {
        spdlog::info(
            "CLTLoginMediator::SetCurrentState {} -> {} newState=<null>",
            oldState->GetStateId(), helperStateId);
        return 0u;
    }

    spdlog::info(
        "CLTLoginMediator::SetCurrentState {} -> {}",
        oldState->GetStateId(), helperStateId);

    currentState_ = newState;
    newState->Slot3_BeginOrContinue(oldState);
    return 0u;
}

void CLTLoginMediator::PostEvent(uint32_t eventId) {
    spdlog::info(
        "{} Event# {} currentState={} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned observer set over recovered owner+0x674 container surface)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));

    for (LoginObserverSetConstIterator674 it = ObserverSetBegin674(this), end = ObserverSetEnd674(this); it != end; ++it) {
        void* const observer = *it;
        void** const observerVtable = *reinterpret_cast<void***>(observer);
        const auto onLoginEvent = reinterpret_cast<LoginObserverOnEventFn>(observerVtable[0]);
        spdlog::info(
            "CLTLoginMediator::PostEvent dispatching observer={} event=0x{:02x}",
            fmt::ptr(observer),
            static_cast<unsigned>(eventId & 0xffu));
        onLoginEvent(observer, eventId);
    }
}

void CLTLoginMediator::PostError(uint32_t errorId) {
    spdlog::info(
        "{} Error# {} status80=0x{:08x} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned observer set over recovered owner+0x674 container surface)",
        kLogPrefixPostError,
        static_cast<unsigned>(errorId),
        static_cast<unsigned>(g_CurrentLoginMediator ? g_CurrentLoginMediator->worldListCountOrStatus80 : 0u),
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));

    for (LoginObserverSetConstIterator674 it = ObserverSetBegin674(this), end = ObserverSetEnd674(this); it != end; ++it) {
        void* const observer = *it;
        void** const observerVtable = *reinterpret_cast<void***>(observer);
        const auto onLoginError = reinterpret_cast<LoginObserverOnErrorFn>(observerVtable[1]);
        spdlog::info(
            "CLTLoginMediator::PostError dispatching observer={} error=0x{:02x} status80=0x{:08x}",
            fmt::ptr(observer),
            static_cast<unsigned>(errorId & 0xffu),
            static_cast<unsigned>(worldListCountOrStatus80));
        onLoginError(observer, errorId);
    }
}

}  // namespace mxo::ltlogin
