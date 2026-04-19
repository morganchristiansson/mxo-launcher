#include "loginmediator.h"
#include "loginmediator_events.h"

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include <spdlog/spdlog.h>

#include <bits/stl_tree.h>

#include <cstdio>
#include <cstdlib>
#include <string>

static_assert(sizeof(std::_Rb_tree_node_base) == 0x10, "observer tree node-base size mismatch");
static_assert(sizeof(mxo::ltlogin::LoginObserverTreeNode674) == 0x14, "observer tree node size mismatch");

namespace mxo::ltlogin {

// Implementations of non-template methods from loginmediator_events.h

// anchor: launcher.exe:0x419570 / DestroySubtreeNodes
void LoginObserverTreeHelper674::DestroySubtreeNodes(LoginObserverTreeNode674* subtreeRoot) {
    while (subtreeRoot != nullptr) {
        DestroySubtreeNodes(subtreeRoot->right0c);
        LoginObserverTreeNode674* currentNode = subtreeRoot;
        subtreeRoot = currentNode->left08;
        std::free(currentNode);
    }
}

// anchor: launcher.exe:0x419510 / BuildEqualRangeForKey
void LoginObserverTreeHelper674::BuildEqualRange(
    LoginObserverTreeNode674* header,
    void* observer,
    LoginObserverTreeNode674** outLowerBound,
    LoginObserverTreeNode674** outUpperBound) {
    LoginObserverTreeNode674* lowerBound = const_cast<LoginObserverTreeNode674*>(header);
    LoginObserverTreeNode674* upperBound = const_cast<LoginObserverTreeNode674*>(header);
    LoginObserverTreeNode674* current = header->parent04;
    const uintptr_t targetKey = TreeKey(observer);

    while (current != nullptr) {
        if (targetKey < TreeKey(current->observerKey10)) {
            upperBound = current;
            current = current->left08;
        } else {
            current = current->right0c;
        }
    }

    current = header->parent04;
    while (current != nullptr) {
        if (TreeKey(current->observerKey10) < targetKey) {
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

// anchor: launcher.exe:0x41baa0 / CountRange
uint32_t LoginObserverTreeHelper674::CountRange(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last) {
    uint32_t count = 0u;
    while (first != last) {
        first = NodeFromBase(std::_Rb_tree_increment(TreeNodeBase(first)));
        ++count;
    }
    return count;
}

// anchor: launcher.exe:0x4195c0 / EraseRange
void LoginObserverTreeHelper674::EraseRange(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last,
    std::_Rb_tree_node_base* headerBase,
    uint32_t* nodeCount) {
    while (first != last) {
        (void)std::_Rb_tree_rebalance_for_erase(TreeNodeBase(first), *headerBase);
        LoginObserverTreeNode674* next = NodeFromBase(std::_Rb_tree_increment(TreeNodeBase(first)));
        std::free(first);
        first = next;
        --(*nodeCount);
    }
}

// anchor: launcher.exe:0x41d430 / EraseRangeFull
void LoginObserverTreeHelper674::EraseRangeFull(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last,
    LoginObserverTreeNode674* header,
    uint32_t* nodeCount) {
    std::_Rb_tree_node_base* headerBase = TreeNodeBase(header);
    LoginObserverTreeNode674* begin = NodeFromBase(headerBase->_M_left);
    LoginObserverTreeNode674* end = header;

    if (first == begin && last == end) {
        if (*nodeCount != 0u) {
            DestroySubtreeNodes(reinterpret_cast<LoginObserverTreeNode674*>(headerBase->_M_parent));
        }
        headerBase->_M_parent = nullptr;
        headerBase->_M_left = headerBase;
        headerBase->_M_right = headerBase;
        *nodeCount = 0u;
        return;
    }
    EraseRange(first, last, headerBase, nodeCount);
}

// anchor: launcher.exe:0x415f20 / InsertNode
bool LoginObserverTreeHelper674::InsertNode(
    LoginObserverTreeNode674* header,
    void* observer,
    uint32_t* nodeCount) {
    std::_Rb_tree_node_base* headerBase = TreeNodeBase(header);
    std::_Rb_tree_node_base* parentBase = headerBase;
    LoginObserverTreeNode674* current = header->parent04;
    const uintptr_t targetKey = TreeKey(observer);
    bool insertLeft = true;

    while (current != nullptr) {
        parentBase = TreeNodeBase(current);
        const uintptr_t currentKey = TreeKey(current->observerKey10);
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
    node->colorOrFlags00 = static_cast<uint32_t>(std::_S_red);
    node->parent04 = nullptr;
    node->left08 = nullptr;
    node->right0c = nullptr;
    node->observerKey10 = observer;

    std::_Rb_tree_insert_and_rebalance(insertLeft, TreeNodeBase(node), parentBase, *headerBase);
    ++(*nodeCount);
    return true;
}

// Legacy type aliases (used by CLTLoginMediator methods)
// Focused post-state9 event/listener split:
// - keep the immediate `0x41b450 -> 0x41cfb0` continuation in its own TU
// - this avoids reopening the broader mediator auth/bootstrap transport file just to work on the
//   late state-`0x0c` bridge
// - canonical reference:
//   `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
using LoginObserverOnEventFn = void(__thiscall*)(void*, uint32_t);
using LoginObserverOnErrorFn = void(__thiscall*)(void*, uint32_t);

template <typename Node>
static std::_Rb_tree_node_base* ObserverTreeNodeBase(Node* node) {
    return reinterpret_cast<std::_Rb_tree_node_base*>(node);
}

template <typename Node>
static const std::_Rb_tree_node_base* ObserverTreeNodeBase(const Node* node) {
    return reinterpret_cast<const std::_Rb_tree_node_base*>(node);
}

static LoginObserverTreeNode674* ObserverTreeNodeFromBase(std::_Rb_tree_node_base* node) {
    return reinterpret_cast<LoginObserverTreeNode674*>(node);
}

static const LoginObserverTreeNode674* ObserverTreeNodeFromBase(const std::_Rb_tree_node_base* node) {
    return reinterpret_cast<const LoginObserverTreeNode674*>(node);
}

// UNANCHORED: source-owned free-backed mirror of the duplicate subtree destroy helpers emitted at
// `0x419570` / `0x41d370` for the small non-vtable observer tree class at owner `+0x674`.
static void DestroyObserverSubtreeNodes674(LoginObserverTreeNode674* subtreeRoot) {
    while (subtreeRoot != nullptr) {
        DestroyObserverSubtreeNodes674(subtreeRoot->right0c);
        LoginObserverTreeNode674* const currentNode = subtreeRoot;
        subtreeRoot = currentNode->left08;
        std::free(currentNode);
    }
}

// anchor-family: launcher.exe ctor/dtor field initialization of owner `+0x674`
void CLTLoginMediator::InitializeObserverTree674() {
    observerTree674_.header00 = &observerTreeHeader674_;
    observerTree674_.nodeCount04 = 0u;

    observerTreeHeader674_ = {};
    std::_Rb_tree_node_base* headerBase = LoginObserverTreeHelper674::TreeNodeBase(&observerTreeHeader674_);
    headerBase->_M_color = std::_S_red;
    headerBase->_M_parent = nullptr;
    headerBase->_M_left = headerBase;
    headerBase->_M_right = headerBase;

    latestObserver170_ = nullptr;
    latestObserver174_ = nullptr;
}

// anchor: launcher.exe:0x419570 / 0x41d370
void CLTLoginMediator::ClearObserverTree674() {
    if (observerTree674_.header00 == nullptr) {
        return;
    }
    if (observerTree674_.nodeCount04 != 0u) {
        LoginObserverTreeHelper674::DestroySubtreeNodes(&observerTreeHeader674_);
    }
    InitializeObserverTree674();
}

// anchor-family: inlined `begin()` expression used by launcher.exe:0x41cfb0 / 0x41d090 / 0x41d430
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeBegin674() const {
    if (observerTree674_.header00 == nullptr || observerTree674_.nodeCount04 == 0u) {
        return observerTree674_.header00;
    }
    return LoginObserverTreeHelper674::NodeFromBase(
        LoginObserverTreeHelper674::TreeNodeBase(observerTree674_.header00)->_M_left);
}

// anchor-family: inlined header/end expression used by launcher.exe:0x41cfb0 / 0x41d090 / 0x41d430
LoginObserverTreeNode674* CLTLoginMediator::ObserverTreeEnd674() const {
    return observerTree674_.header00;
}

// anchor: launcher.exe:0x419510 / BuildEqualRangeForKey
void CLTLoginMediator::EqualRangeObserver674(
    void* observer,
    LoginObserverTreeNode674** outLowerBound,
    LoginObserverTreeNode674** outUpperBound) const {
    LoginObserverTreeHelper674::BuildEqualRange(
        const_cast<LoginObserverTreeNode674*>(&observerTreeHeader674_), observer, outLowerBound, outUpperBound);
}

// anchor: launcher.exe:0x415f20
bool CLTLoginMediator::InsertObserverNode674(void* observer) {
    return LoginObserverTreeHelper674::InsertNode(
        &observerTreeHeader674_, observer, &observerTree674_.nodeCount04);
}

// anchor: launcher.exe:0x41d430
void CLTLoginMediator::EraseObserverRange674(
    LoginObserverTreeNode674* first,
    LoginObserverTreeNode674* last) {
    LoginObserverTreeHelper674::EraseRangeFull(
        first, last, &observerTreeHeader674_, &observerTree674_.nodeCount04);
}

// anchor: launcher.exe:0x41b450
// Static RE:
//   - old helper vtable+0x0c(new) NoOp
//   - install owner+0x10 from g_LoginHelperState0[value]
//   - new helper vtable+0x08(old) slot3
// Source returns slot3 result (original returns void).
uint32_t CLTLoginMediator::SetCurrentState(uint32_t helperStateId) {
    CLTLoginState* const oldState = currentState_;
    CLTLoginState* const newState =
        (helperStateId < 20u)
            ? static_cast<CLTLoginState*>(reinterpret_cast<void* const*>(&g_LoginHelperDispatchTableScaffold.helper7868)[helperStateId])
            : nullptr;
    if (oldState != nullptr) {
        // Original: old helper vtable+0x0c(newHelper) NoOp
        oldState->Slot4_NoOp();
    }

    if (newState == nullptr) {
        spdlog::info(
            "CLTLoginMediator::SetCurrentState {} -> {} newState=<null>",
            oldState->GetStateId(), helperStateId);
        return 0u;
    }

    // Original: this->currentState10 = newState
    currentState_ = newState;
    // Original: new helper vtable+0x08(oldState) slot3
    newState->Slot3_BeginOrContinue(oldState);
    spdlog::info(
        "CLTLoginMediator::SetCurrentState {} -> {}",
        oldState->GetStateId(), helperStateId);
    return 0u;
}

void CLTLoginMediator::PostEvent(uint32_t eventId) {
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
    // - practical source consequence:
    //   source-own only the recovered std::_Tree-like owner `+0x674` observer walk here, while
    //   leaving balancing/color bits and original node-pool recycling only partially reconstructed.
    spdlog::info(
        "{} Event# {} currentState={} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned std::_Tree-like owner+0x674 observer walk active)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));

    // Observer dispatch note:
    // - `0x41cfb0` itself walks the owner `+0x674` tree for whatever event number it is given;
    //   it is not specialized only to `0x0b/0x18/0x0f`
    // - now that the std::_Tree-like owner `+0x674` scaffold is source-owned enough, event
    //   dispatch here follows that broader original behavior again
    // - launcher-owned progress-text mirrors were removed; exact client-visible loading text now
    //   comes only from the opt-in `client.dll:0x6215b930` hook

    LoginObserverTreeNode674* node = ObserverTreeBegin674();
    const LoginObserverTreeNode674* const end = ObserverTreeEnd674();
    while (node != end) {
        void* const observer = node->observerKey10;
        void** const observerVtable = *reinterpret_cast<void***>(observer);
        const auto onLoginEvent = reinterpret_cast<LoginObserverOnEventFn>(observerVtable[0]);
        spdlog::info(
            "CLTLoginMediator::PostEvent dispatching observerNode={} observer={} event=0x{:02x}",
            fmt::ptr(node),
            fmt::ptr(observer),
            static_cast<unsigned>(eventId & 0xffu));
        onLoginEvent(observer, eventId);
        node = ObserverTreeNodeFromBase(std::_Rb_tree_increment(ObserverTreeNodeBase(node)));
    }

    // Fidelity note:
    // - no extra state9/helper9 continuation belongs in `0x41cfb0` itself
    // - the earlier source-owned event-`0x0b` fallback here has been removed because the active
    //   state8/state11 tails now mirror the immediate helper slot-3 notify before their later
    //   `PostEvent(0x0b/0x16)` observer walk
}

void CLTLoginMediator::PostError(uint32_t errorId) {
    // anchor: launcher.exe:0x41d090
    // Static Ghidra/decompilation for the original body is now concrete enough to mirror here:
    // - log `CLTLoginMediator::PostError(): Error# %d`
    // - walk the owner `+0x674` listener tree
    // - call each observer's second vtable slot (`+0x04` / OnLoginError)
    // This matters for the state8 failure path because `0x43f930` first writes the raw server
    // result dword to owner `+0x80`, then posts error `10`; client observer-side error handling
    // queries mediator slot `+0x178` to read back that status and choose the visible popup.
    spdlog::info(
        "{} Error# {} status80=0x{:08x} treeCount={} header={} root={} leftmost={} rightmost={} (source-owned std::_Tree-like owner+0x674 error walk active)",
        kLogPrefixPostError,
        static_cast<unsigned>(errorId),
        static_cast<unsigned>(g_CurrentLoginMediator ? g_CurrentLoginMediator->worldListCountOrStatus80 : 0u),
        static_cast<unsigned>(observerTree674_.nodeCount04),
        fmt::ptr(observerTree674_.header00),
        fmt::ptr(observerTreeHeader674_.parent04),
        fmt::ptr(observerTreeHeader674_.left08),
        fmt::ptr(observerTreeHeader674_.right0c));

    LoginObserverTreeNode674* node = ObserverTreeBegin674();
    const LoginObserverTreeNode674* const end = ObserverTreeEnd674();
    while (node != end) {
        void* const observer = node->observerKey10;
        void** const observerVtable = *reinterpret_cast<void***>(observer);
        const auto onLoginError = reinterpret_cast<LoginObserverOnErrorFn>(observerVtable[1]);
        spdlog::info(
            "CLTLoginMediator::PostError dispatching observerNode={} observer={} error=0x{:02x} status80=0x{:08x}",
            fmt::ptr(node),
            fmt::ptr(observer),
            static_cast<unsigned>(errorId & 0xffu),
            static_cast<unsigned>(worldListCountOrStatus80));
        onLoginError(observer, errorId);
        node = ObserverTreeNodeFromBase(std::_Rb_tree_increment(ObserverTreeNodeBase(node)));
    }
}

}  // namespace mxo::ltlogin
