#pragma once

#include <cstddef>
#include <utility>

namespace mxo::sgi_tree {

// Narrow project-owned compatibility shim for the launcher.exe SGI/libstdc++ tree lineage.
//
// Provenance/reference implementation:
// - MinGW libstdc++ `bits/stl_tree.h` as documented in
//   `../../docs/launcher.exe/shared_helpers/SGI_STL_TREE.md`
//
// Project policy for this shim:
// - preserve the recovered launcher-facing node/header layout and helper semantics we actually use
// - avoid directly including libstdc++ internal headers in project translation units
// - keep the compatibility surface deliberately tiny instead of re-owning a general-purpose STL tree
//
// Current exported subset:
// - `_Rb_tree_node_base`
// - `_Rb_tree_node<Value>`
// - `_Rb_tree_increment`
// - `_Rb_tree_insert_and_rebalance`
// - `_Rb_tree_rebalance_for_erase`

enum _Rb_tree_color {
    _S_red = false,
    _S_black = true,
};

struct _Rb_tree_node_base {
    using _Base_ptr = _Rb_tree_node_base*;
    using _Const_Base_ptr = const _Rb_tree_node_base*;

    _Rb_tree_color _M_color = _S_red;
    _Base_ptr _M_parent = nullptr;
    _Base_ptr _M_left = nullptr;
    _Base_ptr _M_right = nullptr;

    static _Base_ptr _S_minimum(_Base_ptr node) {
        while (node->_M_left != nullptr) {
            node = node->_M_left;
        }
        return node;
    }

    static _Const_Base_ptr _S_minimum(_Const_Base_ptr node) {
        while (node->_M_left != nullptr) {
            node = node->_M_left;
        }
        return node;
    }

    static _Base_ptr _S_maximum(_Base_ptr node) {
        while (node->_M_right != nullptr) {
            node = node->_M_right;
        }
        return node;
    }

    static _Const_Base_ptr _S_maximum(_Const_Base_ptr node) {
        while (node->_M_right != nullptr) {
            node = node->_M_right;
        }
        return node;
    }
};

template <typename Value>
struct _Rb_tree_node : public _Rb_tree_node_base {
    Value _M_value_field{};

    Value* _M_valptr() {
        return &_M_value_field;
    }

    const Value* _M_valptr() const {
        return &_M_value_field;
    }
};

inline _Rb_tree_node_base* _Rb_tree_increment(_Rb_tree_node_base* node) {
    if (node->_M_right != nullptr) {
        node = node->_M_right;
        while (node->_M_left != nullptr) {
            node = node->_M_left;
        }
    } else {
        _Rb_tree_node_base* parent = node->_M_parent;
        while (node == parent->_M_right) {
            node = parent;
            parent = parent->_M_parent;
        }
        if (node->_M_right != parent) {
            node = parent;
        }
    }
    return node;
}

inline const _Rb_tree_node_base* _Rb_tree_increment(const _Rb_tree_node_base* node) {
    return _Rb_tree_increment(const_cast<_Rb_tree_node_base*>(node));
}

inline void _Rb_tree_rotate_left(_Rb_tree_node_base* const x, _Rb_tree_node_base*& root) {
    _Rb_tree_node_base* const y = x->_M_right;
    x->_M_right = y->_M_left;
    if (y->_M_left != nullptr) {
        y->_M_left->_M_parent = x;
    }
    y->_M_parent = x->_M_parent;
    if (x == root) {
        root = y;
    } else if (x == x->_M_parent->_M_left) {
        x->_M_parent->_M_left = y;
    } else {
        x->_M_parent->_M_right = y;
    }
    y->_M_left = x;
    x->_M_parent = y;
}

inline void _Rb_tree_rotate_right(_Rb_tree_node_base* const x, _Rb_tree_node_base*& root) {
    _Rb_tree_node_base* const y = x->_M_left;
    x->_M_left = y->_M_right;
    if (y->_M_right != nullptr) {
        y->_M_right->_M_parent = x;
    }
    y->_M_parent = x->_M_parent;
    if (x == root) {
        root = y;
    } else if (x == x->_M_parent->_M_right) {
        x->_M_parent->_M_right = y;
    } else {
        x->_M_parent->_M_left = y;
    }
    y->_M_right = x;
    x->_M_parent = y;
}

inline void _Rb_tree_insert_and_rebalance(
    const bool insertLeft,
    _Rb_tree_node_base* x,
    _Rb_tree_node_base* p,
    _Rb_tree_node_base& header) {
    _Rb_tree_node_base*& root = header._M_parent;
    _Rb_tree_node_base*& leftmost = header._M_left;
    _Rb_tree_node_base*& rightmost = header._M_right;

    x->_M_parent = p;
    x->_M_left = nullptr;
    x->_M_right = nullptr;
    x->_M_color = _S_red;

    if (insertLeft) {
        p->_M_left = x;
        if (p == &header) {
            root = x;
            rightmost = x;
        } else if (p == leftmost) {
            leftmost = x;
        }
    } else {
        p->_M_right = x;
        if (p == rightmost) {
            rightmost = x;
        }
    }

    while (x != root && x->_M_parent->_M_color == _S_red) {
        _Rb_tree_node_base* const xParent = x->_M_parent;
        _Rb_tree_node_base* const xGrandparent = xParent->_M_parent;
        if (xParent == xGrandparent->_M_left) {
            _Rb_tree_node_base* const y = xGrandparent->_M_right;
            if (y != nullptr && y->_M_color == _S_red) {
                xParent->_M_color = _S_black;
                y->_M_color = _S_black;
                xGrandparent->_M_color = _S_red;
                x = xGrandparent;
            } else {
                if (x == xParent->_M_right) {
                    x = xParent;
                    _Rb_tree_rotate_left(x, root);
                }
                x->_M_parent->_M_color = _S_black;
                x->_M_parent->_M_parent->_M_color = _S_red;
                _Rb_tree_rotate_right(x->_M_parent->_M_parent, root);
            }
        } else {
            _Rb_tree_node_base* const y = xGrandparent->_M_left;
            if (y != nullptr && y->_M_color == _S_red) {
                xParent->_M_color = _S_black;
                y->_M_color = _S_black;
                xGrandparent->_M_color = _S_red;
                x = xGrandparent;
            } else {
                if (x == xParent->_M_left) {
                    x = xParent;
                    _Rb_tree_rotate_right(x, root);
                }
                x->_M_parent->_M_color = _S_black;
                x->_M_parent->_M_parent->_M_color = _S_red;
                _Rb_tree_rotate_left(x->_M_parent->_M_parent, root);
            }
        }
    }
    root->_M_color = _S_black;
}

inline _Rb_tree_node_base* _Rb_tree_rebalance_for_erase(
    _Rb_tree_node_base* const z,
    _Rb_tree_node_base& header) {
    _Rb_tree_node_base*& root = header._M_parent;
    _Rb_tree_node_base*& leftmost = header._M_left;
    _Rb_tree_node_base*& rightmost = header._M_right;
    _Rb_tree_node_base* y = z;
    _Rb_tree_node_base* x = nullptr;
    _Rb_tree_node_base* xParent = nullptr;

    if (y->_M_left == nullptr) {
        x = y->_M_right;
    } else if (y->_M_right == nullptr) {
        x = y->_M_left;
    } else {
        y = y->_M_right;
        while (y->_M_left != nullptr) {
            y = y->_M_left;
        }
        x = y->_M_right;
    }

    if (y != z) {
        z->_M_left->_M_parent = y;
        y->_M_left = z->_M_left;
        if (y != z->_M_right) {
            xParent = y->_M_parent;
            if (x != nullptr) {
                x->_M_parent = y->_M_parent;
            }
            y->_M_parent->_M_left = x;
            y->_M_right = z->_M_right;
            z->_M_right->_M_parent = y;
        } else {
            xParent = y;
        }

        if (root == z) {
            root = y;
        } else if (z->_M_parent->_M_left == z) {
            z->_M_parent->_M_left = y;
        } else {
            z->_M_parent->_M_right = y;
        }
        y->_M_parent = z->_M_parent;
        std::swap(y->_M_color, z->_M_color);
        y = z;
    } else {
        xParent = y->_M_parent;
        if (x != nullptr) {
            x->_M_parent = y->_M_parent;
        }
        if (root == z) {
            root = x;
        } else if (z->_M_parent->_M_left == z) {
            z->_M_parent->_M_left = x;
        } else {
            z->_M_parent->_M_right = x;
        }
        if (leftmost == z) {
            if (z->_M_right == nullptr) {
                leftmost = z->_M_parent;
            } else {
                leftmost = _Rb_tree_node_base::_S_minimum(x);
            }
        }
        if (rightmost == z) {
            if (z->_M_left == nullptr) {
                rightmost = z->_M_parent;
            } else {
                rightmost = _Rb_tree_node_base::_S_maximum(x);
            }
        }
    }

    if (y->_M_color != _S_red) {
        while (x != root && (x == nullptr || x->_M_color == _S_black)) {
            if (x == xParent->_M_left) {
                _Rb_tree_node_base* w = xParent->_M_right;
                if (w->_M_color == _S_red) {
                    w->_M_color = _S_black;
                    xParent->_M_color = _S_red;
                    _Rb_tree_rotate_left(xParent, root);
                    w = xParent->_M_right;
                }
                if ((w->_M_left == nullptr || w->_M_left->_M_color == _S_black) &&
                    (w->_M_right == nullptr || w->_M_right->_M_color == _S_black)) {
                    w->_M_color = _S_red;
                    x = xParent;
                    xParent = xParent->_M_parent;
                } else {
                    if (w->_M_right == nullptr || w->_M_right->_M_color == _S_black) {
                        if (w->_M_left != nullptr) {
                            w->_M_left->_M_color = _S_black;
                        }
                        w->_M_color = _S_red;
                        _Rb_tree_rotate_right(w, root);
                        w = xParent->_M_right;
                    }
                    w->_M_color = xParent->_M_color;
                    xParent->_M_color = _S_black;
                    if (w->_M_right != nullptr) {
                        w->_M_right->_M_color = _S_black;
                    }
                    _Rb_tree_rotate_left(xParent, root);
                    break;
                }
            } else {
                _Rb_tree_node_base* w = xParent->_M_left;
                if (w->_M_color == _S_red) {
                    w->_M_color = _S_black;
                    xParent->_M_color = _S_red;
                    _Rb_tree_rotate_right(xParent, root);
                    w = xParent->_M_left;
                }
                if ((w->_M_right == nullptr || w->_M_right->_M_color == _S_black) &&
                    (w->_M_left == nullptr || w->_M_left->_M_color == _S_black)) {
                    w->_M_color = _S_red;
                    x = xParent;
                    xParent = xParent->_M_parent;
                } else {
                    if (w->_M_left == nullptr || w->_M_left->_M_color == _S_black) {
                        if (w->_M_right != nullptr) {
                            w->_M_right->_M_color = _S_black;
                        }
                        w->_M_color = _S_red;
                        _Rb_tree_rotate_left(w, root);
                        w = xParent->_M_left;
                    }
                    w->_M_color = xParent->_M_color;
                    xParent->_M_color = _S_black;
                    if (w->_M_left != nullptr) {
                        w->_M_left->_M_color = _S_black;
                    }
                    _Rb_tree_rotate_right(xParent, root);
                    break;
                }
            }
        }
        if (x != nullptr) {
            x->_M_color = _S_black;
        }
    }
    return y;
}

} // namespace mxo::sgi_tree
