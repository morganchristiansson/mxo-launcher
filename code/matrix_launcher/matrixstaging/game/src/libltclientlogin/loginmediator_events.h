#pragma once

#include <cstdint>

#include "../../../../compat/sgi_tree_compat.h"

// Note: LoginObserverTreeNode674 is defined in loginmediator.h

namespace mxo {
namespace ltlogin {

// Static helper class for observer tree operations.
// All methods are static - no instantiation needed.
// This mirrors the launcher-side helper class at owner `+0x674`.
class LoginObserverTreeHelper674 {
public:
    // anchor: launcher.exe:0x419510 / TreeKey
    static uintptr_t TreeKey(void* observer) {
        return reinterpret_cast<uintptr_t>(observer);
    }

    // anchor: launcher.exe:0x419510 / TreeNodeBase
    template <typename Node>
    static mxo::sgi_tree::_Rb_tree_node_base* TreeNodeBase(Node* node) {
        return reinterpret_cast<mxo::sgi_tree::_Rb_tree_node_base*>(node);
    }

    template <typename Node>
    static const mxo::sgi_tree::_Rb_tree_node_base* TreeNodeBase(const Node* node) {
        return reinterpret_cast<const mxo::sgi_tree::_Rb_tree_node_base*>(node);
    }

    // anchor: launcher.exe:0x419510 / NodeFromBase
    static LoginObserverTreeNode674* NodeFromBase(mxo::sgi_tree::_Rb_tree_node_base* node) {
        return reinterpret_cast<LoginObserverTreeNode674*>(node);
    }

    static const LoginObserverTreeNode674* NodeFromBase(const mxo::sgi_tree::_Rb_tree_node_base* node) {
        return reinterpret_cast<const LoginObserverTreeNode674*>(node);
    }

    // anchor: launcher.exe:0x419570 / DestroySubtreeNodes
    static void DestroySubtreeNodes(LoginObserverTreeNode674* subtreeRoot);

    // anchor: launcher.exe:0x41d370 / DestroySubtreeNodes2
    static void DestroySubtreeNodes2(LoginObserverTreeNode674* subtreeRoot);

    // anchor: launcher.exe:0x419510 / BuildEqualRange
    static void BuildEqualRange(
        LoginObserverTreeNode674* header,
        void* observer,
        LoginObserverTreeNode674** outLowerBound,
        LoginObserverTreeNode674** outUpperBound);

    // anchor: launcher.exe:0x41baa0 / CountRange
    static uint32_t CountRange(
        LoginObserverTreeNode674* first,
        LoginObserverTreeNode674* last);

    // anchor: launcher.exe:0x4195c0 / EraseRange
    static void EraseRange(
        LoginObserverTreeNode674* first,
        LoginObserverTreeNode674* last,
        mxo::sgi_tree::_Rb_tree_node_base* headerBase,
        uint32_t* nodeCount);

    // anchor: launcher.exe:0x41d430 / EraseRangeFull
    static void EraseRangeFull(
        LoginObserverTreeNode674* first,
        LoginObserverTreeNode674* last,
        LoginObserverTreeNode674* header,
        uint32_t* nodeCount);

    // anchor: launcher.exe:0x415f20 / InsertNode
    static bool InsertNode(
        LoginObserverTreeNode674* header,
        void* observer,
        uint32_t* nodeCount);
};

} // namespace ltlogin
} // namespace mxo