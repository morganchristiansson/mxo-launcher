#include "loginmediator.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

using LoginObserverOnEventFn = void(__thiscall*)(void*, uint32_t);
using LoginObserverOnErrorFn = void(__thiscall*)(void*, uint32_t);

static uintptr_t ObserverTreeKey(void* observer) {
    return reinterpret_cast<uintptr_t>(observer);
}

static LoginObserverTreeNode674* ObserverTreeMinimum(LoginObserverTreeNode674* node) {
    if (!node) {
        return nullptr;
    }
    while (node->left08 != nullptr) {
        node = node->left08;
    }
    return node;
}

static LoginObserverTreeNode674* ObserverTreeMaximum(LoginObserverTreeNode674* node) {
    if (!node) {
        return nullptr;
    }
    while (node->right0c != nullptr) {
        node = node->right0c;
    }
    return node;
}

static void** GetLoginObserverVtable(void* observer) {
    if (!observer) {
        return nullptr;
    }
    return *reinterpret_cast<void***>(observer);
}

static bool LooksLikeLoginObserverEventVtable(void* observer) {
    void** vtable = GetLoginObserverVtable(observer);
    return vtable != nullptr && vtable[0] != nullptr;
}

static bool LooksLikeLoginObserverErrorVtable(void* observer) {
    void** vtable = GetLoginObserverVtable(observer);
    return vtable != nullptr && vtable[1] != nullptr;
}

static void DispatchLoginObserverEvent(void* observer, uint32_t eventId) {
    if (!LooksLikeLoginObserverEventVtable(observer)) {
        return;
    }

    void** vtable = GetLoginObserverVtable(observer);
    const auto fn = reinterpret_cast<LoginObserverOnEventFn>(vtable[0]);
    fn(observer, eventId);
}

static void DispatchLoginObserverError(void* observer, uint32_t errorId) {
    if (!LooksLikeLoginObserverErrorVtable(observer)) {
        return;
    }

    void** vtable = GetLoginObserverVtable(observer);
    const auto fn = reinterpret_cast<LoginObserverOnErrorFn>(vtable[1]);
    fn(observer, errorId);
}

}  // namespace

// UNANCHORED: source-owned std::_Tree-like initialization for the recovered owner `+0x674`
// observer container scaffold.
void CLTLoginMediator::InitializeObserverTree674() {
    observerTree674_.header00 = &observerTreeHeader674_;
    observerTree674_.count04 = 0u;
    observerTreeHeader674_.reserved00 = nullptr;
    observerTreeHeader674_.parent04 = nullptr;
    observerTreeHeader674_.left08 = &observerTreeHeader674_;
    observerTreeHeader674_.right0c = &observerTreeHeader674_;
    observerTreeHeader674_.observer10 = nullptr;
    latestObserver170_ = nullptr;
    latestObserver174_ = nullptr;
    observerRegister170Count_ = 0u;
    observerUnregister174Count_ = 0u;
}

// UNANCHORED: source-owned tree cleanup for the recovered owner `+0x674` observer container.
void CLTLoginMediator::ClearObserverTree674() {
    LoginObserverTreeNode674* node = ObserverTreeBegin674();
    const LoginObserverTreeNode674* const end = ObserverTreeEnd674();
    while (node != end) {
        LoginObserverTreeNode674* const next = ObserverTreeSuccessor674(node);
        std::free(node);
        node = next;
    }
    InitializeObserverTree674();
}

// UNANCHORED: source-owned `begin()` helper over the recovered owner `+0x674` observer tree.
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeBegin674() const {
    if (observerTree674_.header00 == nullptr || observerTree674_.count04 == 0u) {
        return observerTree674_.header00;
    }
    return observerTreeHeader674_.left08;
}

// UNANCHORED: source-owned `end()`/header helper over the recovered owner `+0x674` observer tree.
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeEnd674() const {
    return observerTree674_.header00;
}

// UNANCHORED: source-owned in-order successor helper mirroring the `0x41cfb0/0x41d090`
// owner `+0x674` traversal shape.
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeSuccessor674(LoginObserverTreeNode674* node) const {
    LoginObserverTreeNode674* const header = ObserverTreeEnd674();
    if (node == nullptr || header == nullptr) {
        return header;
    }

    if (node->right0c != nullptr) {
        return ObserverTreeMinimum(node->right0c);
    }

    LoginObserverTreeNode674* parent = node->parent04;
    while (parent != nullptr && node == parent->right0c) {
        node = parent;
        parent = parent->parent04;
    }
    if (node->right0c != parent) {
        node = parent;
    }
    return node ? node : header;
}

// UNANCHORED: source-owned keyed lookup over the recovered owner `+0x674` observer tree.
LoginObserverTreeNode674* CLTLoginMediator::FindObserverNode674(void* observer) const {
    LoginObserverTreeNode674* current = observerTreeHeader674_.parent04;
    const uintptr_t targetKey = ObserverTreeKey(observer);
    while (current != nullptr) {
        const uintptr_t currentKey = ObserverTreeKey(current->observer10);
        if (targetKey < currentKey) {
            current = current->left08;
        } else if (currentKey < targetKey) {
            current = current->right0c;
        } else {
            return current;
        }
    }
    return nullptr;
}

// UNANCHORED: source-owned equal-range helper mirroring the `0x419510` owner `+0x674`
// search-pair builder.
void CLTLoginMediator::EqualRangeObserver674(
    void* observer,
    LoginObserverTreeNode674** outLowerBound,
    LoginObserverTreeNode674** outUpperBound) const {
    LoginObserverTreeNode674* lowerBound = const_cast<LoginObserverTreeNode674*>(&observerTreeHeader674_);
    LoginObserverTreeNode674* upperBound = const_cast<LoginObserverTreeNode674*>(&observerTreeHeader674_);
    LoginObserverTreeNode674* current = observerTreeHeader674_.parent04;
    const uintptr_t targetKey = ObserverTreeKey(observer);

    while (current != nullptr) {
        if (targetKey < ObserverTreeKey(current->observer10)) {
            upperBound = current;
            current = current->left08;
        } else {
            current = current->right0c;
        }
    }

    current = observerTreeHeader674_.parent04;
    while (current != nullptr) {
        if (ObserverTreeKey(current->observer10) < targetKey) {
            current = current->right0c;
        } else {
            lowerBound = current;
            current = current->left08;
        }
    }

    if (outLowerBound) {
        *outLowerBound = lowerBound;
    }
    if (outUpperBound) {
        *outUpperBound = upperBound;
    }
}

// UNANCHORED: source-owned iterator-distance helper mirroring the `0x41baa0` owner `+0x674`
// range-count walk.
uint32_t CLTLoginMediator::CountObserverRange674(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last) const {
    uint32_t count = 0u;
    while (first != last) {
        first = ObserverTreeSuccessor674(first);
        ++count;
    }
    return count;
}

// UNANCHORED: source-owned insert helper for the recovered owner `+0x674` observer tree.
bool CLTLoginMediator::InsertObserverNode674(void* observer) {
    LoginObserverTreeNode674* parent = &observerTreeHeader674_;
    LoginObserverTreeNode674* current = observerTreeHeader674_.parent04;
    const uintptr_t targetKey = ObserverTreeKey(observer);
    bool insertLeft = true;

    while (current != nullptr) {
        parent = current;
        const uintptr_t currentKey = ObserverTreeKey(current->observer10);
        if (targetKey < currentKey) {
            insertLeft = true;
            current = current->left08;
        } else if (currentKey < targetKey) {
            insertLeft = false;
            current = current->right0c;
        } else {
            return false;
        }
    }

    auto* node = static_cast<LoginObserverTreeNode674*>(std::malloc(sizeof(LoginObserverTreeNode674)));
    if (!node) {
        return false;
    }
    node->reserved00 = nullptr;
    node->parent04 = parent;
    node->left08 = nullptr;
    node->right0c = nullptr;
    node->observer10 = observer;

    if (observerTreeHeader674_.parent04 == nullptr) {
        observerTreeHeader674_.parent04 = node;
        observerTreeHeader674_.left08 = node;
        observerTreeHeader674_.right0c = node;
    } else if (insertLeft) {
        parent->left08 = node;
        if (observerTreeHeader674_.left08 == parent) {
            observerTreeHeader674_.left08 = node;
        }
    } else {
        parent->right0c = node;
        if (observerTreeHeader674_.right0c == parent) {
            observerTreeHeader674_.right0c = node;
        }
    }

    ++observerTree674_.count04;
    return true;
}

// UNANCHORED: source-owned erase-one helper for the recovered owner `+0x674` observer tree.
bool CLTLoginMediator::EraseObserverNode674(void* observer) {
    LoginObserverTreeNode674* node = FindObserverNode674(observer);
    if (node == nullptr) {
        return false;
    }

    auto transplant = [this](LoginObserverTreeNode674* oldNode, LoginObserverTreeNode674* replacement) {
        LoginObserverTreeNode674* const header = &observerTreeHeader674_;
        if (oldNode->parent04 == header) {
            header->parent04 = replacement;
        } else if (oldNode == oldNode->parent04->left08) {
            oldNode->parent04->left08 = replacement;
        } else {
            oldNode->parent04->right0c = replacement;
        }
        if (replacement != nullptr) {
            replacement->parent04 = oldNode->parent04;
        }
    };

    if (node->left08 == nullptr) {
        transplant(node, node->right0c);
    } else if (node->right0c == nullptr) {
        transplant(node, node->left08);
    } else {
        LoginObserverTreeNode674* successor = ObserverTreeMinimum(node->right0c);
        if (successor->parent04 != node) {
            transplant(successor, successor->right0c);
            successor->right0c = node->right0c;
            if (successor->right0c != nullptr) {
                successor->right0c->parent04 = successor;
            }
        }
        transplant(node, successor);
        successor->left08 = node->left08;
        if (successor->left08 != nullptr) {
            successor->left08->parent04 = successor;
        }
    }

    std::free(node);
    --observerTree674_.count04;

    if (observerTree674_.count04 == 0u) {
        observerTreeHeader674_.parent04 = nullptr;
        observerTreeHeader674_.left08 = &observerTreeHeader674_;
        observerTreeHeader674_.right0c = &observerTreeHeader674_;
    } else {
        observerTreeHeader674_.left08 = ObserverTreeMinimum(observerTreeHeader674_.parent04);
        observerTreeHeader674_.right0c = ObserverTreeMaximum(observerTreeHeader674_.parent04);
    }

    return true;
}

// UNANCHORED: source-owned erase-range helper mirroring the `0x41d430` owner `+0x674`
// erase walk, including its full-range clear special case.
void CLTLoginMediator::EraseObserverRange674(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last) {
    if (first == ObserverTreeBegin674() && last == ObserverTreeEnd674()) {
        if (observerTree674_.count04 != 0u) {
            ClearObserverTree674();
        }
        return;
    }

    while (first != last) {
        LoginObserverTreeNode674* const next = ObserverTreeSuccessor674(first);
        EraseObserverNode674(first->observer10);
        first = next;
    }
}

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

bool CLTLoginMediator::RegisterLoginObserver(void* observer) {
    // anchor: launcher.exe:0x41ddb0
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x170` as insertion into the owner `+0x674` listener tree, not as a startup
    // context handoff.
    if (!observer) {
        return false;
    }

    latestObserver170_ = observer;
    ++observerRegister170Count_;

    const bool inserted = InsertObserverNode674(observer);
    const bool returnValue = !inserted;
    if (!inserted) {
        spdlog::info(
            "CLTLoginMediator::RegisterLoginObserver observer={} already registered treeCount={} header={} root={} leftmost={} rightmost={} returnValue={} (0x41ddb0 returns !insertedFlag from the helper result pair)",
            fmt::ptr(observer),
            static_cast<unsigned>(observerTree674_.count04),
            fmt::ptr(observerTree674_.header00),
            fmt::ptr(observerTreeHeader674_.parent04),
            fmt::ptr(observerTreeHeader674_.left08),
            fmt::ptr(observerTreeHeader674_.right0c),
            returnValue ? 1u : 0u);
        return returnValue;
    }

    spdlog::info(
        "CLTLoginMediator::RegisterLoginObserver observer={} treeCount={} header={} root={} leftmost={} rightmost={} inserted={} returnValue={} (source-owned std::_Tree-like owner+0x674 bridge active)",
        fmt::ptr(observer),
        static_cast<unsigned>(observerTree674_.count04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c),
        inserted ? 1u : 0u,
        returnValue ? 1u : 0u);
    return returnValue;
}

bool CLTLoginMediator::UnregisterLoginObserver(void* observer) {
    // anchor: launcher.exe:0x41dde0
    // Direct runtime/vtable proof on the client-resolved `ILTLoginMediator.Default` object now
    // identifies `+0x174` as removal from the owner `+0x674` listener tree.
    if (!observer) {
        return false;
    }

    latestObserver174_ = observer;
    ++observerUnregister174Count_;

    LoginObserverTreeNode674* lowerBound = nullptr;
    LoginObserverTreeNode674* upperBound = nullptr;
    EqualRangeObserver674(observer, &lowerBound, &upperBound);
    const uint32_t rangeCount = CountObserverRange674(lowerBound, upperBound);
    EraseObserverRange674(lowerBound, upperBound);
    const bool returnValue = (rangeCount == 0u);

    spdlog::info(
        "CLTLoginMediator::UnregisterLoginObserverScaffold observer={} rangeCount={} treeCount={} header={} root={} leftmost={} rightmost={} returnValue={} (0x41dde0 mirrors equal_range + distance + erase_range and returns rangeCount==0)",
        fmt::ptr(observer),
        static_cast<unsigned>(rangeCount),
        static_cast<unsigned>(observerTree674_.count04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c),
        returnValue ? 1u : 0u);
    return returnValue;
}

void CLTLoginMediator::ResetPostedLoginResultScaffold() {
    lastPostedEventScaffold_ = 0u;
    lastPostedErrorScaffold_ = 0u;
    recentPostedEventCountScaffold_ = 0u;
    recentPostedEventsScaffold_.fill(0u);
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
    //   keep explicit event-history logging here and source-own a std::_Tree-like owner `+0x674`
    //   observer container/traversal shape, while still leaving balancing/color bits and the
    //   original node-pool recycling helpers only partially reconstructed.
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
        "{} Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned std::_Tree-like owner+0x674 observer walk active)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview,
        static_cast<unsigned>(observerTree674_.count04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));
    spdlog::info(
        "DIAGNOSTIC: CLTLoginMediator::PostEvent() Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={}",
        (unsigned)eventId,
        currentState_ ? currentState_->DebugName() : "<null>",
        (unsigned)(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview.c_str());

    // Observer dispatch note:
    // - `0x41cfb0` itself walks the owner `+0x674` tree for whatever event number it is given;
    //   it is not specialized only to `0x0b/0x18/0x0f`
    // - now that the std::_Tree-like owner `+0x674` scaffold is source-owned enough, event
    //   dispatch here follows that broader original behavior again
    // - only event-specific extra side effects stay narrow, e.g. the diagnostic text mirror below
    //   for the now-proved `0x0b` / "Waiting for Regionserver" path
    if (eventId == 0x0bu) {
        DiagnosticLogClientLoadingStateText(
            "Waiting for Regionserver",
            "client.dll:ClientShell_LoginMediatorObserver_OnEvent event 0x0b");
    }

    LoginObserverTreeNode674* node = ObserverTreeBegin674();
    const LoginObserverTreeNode674* const end = ObserverTreeEnd674();
    while (node != end) {
        void* const observer = node->observer10;
        spdlog::info(
            "CLTLoginMediator::PostEventScaffold dispatching observerNode={} observer={} event=0x{:02x}",
            fmt::ptr(node),
            fmt::ptr(observer),
            static_cast<unsigned>(eventId & 0xffu));
        DispatchLoginObserverEvent(observer, eventId);
        node = ObserverTreeSuccessor674(node);
    }

    // Narrow source-owned continuation bridge for the now-live state8 -> helper9/state9 path:
    // - natural original switches to helper9, then posts event `0x0b`, and helper9 slot 3
    //   (`0x439780`) is immediately part of the same active progression family
    // - the owner `+0x674` container/traversal shape is now source-owned enough to walk here,
    //   but this helper9 continuation bridge is still intentionally narrower than claiming the
    //   whole later observer/UI runtime is fully understood
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
    // Static Ghidra/decompilation for the original body is now concrete enough to mirror here:
    // - log `CLTLoginMediator::PostError(): Error# %d`
    // - walk the owner `+0x674` listener tree
    // - call each observer's second vtable slot (`+0x04` / OnLoginError)
    // This matters for the state8 failure path because `0x43f930` first writes the raw server
    // result dword to owner `+0x80`, then posts error `10`; client observer-side error handling
    // queries mediator slot `+0x178` to read back that status and choose the visible popup.
    lastPostedErrorScaffold_ = errorId;
    spdlog::info(
        "{} Error# {} status80=0x{:08x} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned std::_Tree-like owner+0x674 error walk active)",
        kLogPrefixPostError,
        static_cast<unsigned>(errorId),
        static_cast<unsigned>(WorldListCountOrStatus80()),
        static_cast<unsigned>(observerTree674_.count04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));

    LoginObserverTreeNode674* node = ObserverTreeBegin674();
    const LoginObserverTreeNode674* const end = ObserverTreeEnd674();
    while (node != end) {
        void* const observer = node->observer10;
        spdlog::info(
            "CLTLoginMediator::PostErrorScaffold dispatching observerNode={} observer={} error=0x{:02x} status80=0x{:08x}",
            fmt::ptr(node),
            fmt::ptr(observer),
            static_cast<unsigned>(errorId & 0xffu),
            static_cast<unsigned>(WorldListCountOrStatus80()));
        DispatchLoginObserverError(observer, errorId);
        node = ObserverTreeSuccessor674(node);
    }
}

}  // namespace mxo::ltlogin
