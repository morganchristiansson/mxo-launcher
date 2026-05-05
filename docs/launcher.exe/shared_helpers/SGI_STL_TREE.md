# Shared launcher red-black tree family (`SGI STL / libstdc++ stl_tree` match)

## Conclusion

The launcher contains a **shared red-black tree helper family** whose strongest external match is the
classic **SGI STL / old libstdc++ `bits/stl_tree.h` `_Rb_tree` implementation**.

This is **not** currently best understood as an engine-specific custom container.
It is reused by multiple launcher subsystems, including but not limited to
`CLTThreadPerClientTCPEngine`.

Current source usage after the current MSVC-target-Clang portability pass:
- all build families now route through one project-owned `compat/sgi_tree_compat.h` shim
- that shim keeps only the recovered node layout and helper subset we actually depend on
  (`increment`, `insert_and_rebalance`, `rebalance_for_erase`)
- the MinGW `bits/stl_tree.h` lineage remains the donor/reference implementation for fidelity, but
  project translation units no longer include libstdc++ internal tree headers directly
- current recovered users routed through that shim:
  - `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp`
  - `matrixstaging/runtime/src/libltbase/consolevar.cpp`
  - current source no longer re-owns the SGI tree mechanics there either
  - the recovered outer layer is now modeled directly as a case-insensitive unique name registry
    using plain STL map semantics, with only the tiny lazily-created `0x0c` registry-global mirror
    retained for parity/logging
  - `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`

Current local header provenance:
- intended donor/reference: `/usr/lib/gcc/i686-w64-mingw32/13-win32/include/c++/bits/stl_tree.h`
- local cross-build note: `/usr/lib/gcc/i686-w64-mingw32/13-posix/include/c++/bits/stl_tree.h`
  is identical on this machine
- `/usr/include/c++/13/bits/stl_tree.h` differs and is not the header lineage currently being used
  as the launcher match

Launcher.exe remains the source of truth; the MinGW STL headers are the strongest external lineage
match for the current shim, but are no longer the direct API surface included by project code.

---

## Why this matches SGI STL / old libstdc++

### 1. Node/header layout match

Recovered shared node-link prefix:

```cpp
struct NodeBase {
    uint8_t colorOrFlag; // current best read: 0 = red, 1 = black
    uint8_t pad[3];
    NodeBase* parent; // +0x04
    NodeBase* left;   // +0x08
    NodeBase* right;  // +0x0c
};
```

This matches the classic `_Rb_tree_node_base` layout from SGI/libstdc++ on 32-bit x86:

```cpp
struct _Rb_tree_node_base {
    _Rb_tree_color _M_color;
    _Base_ptr _M_parent;
    _Base_ptr _M_left;
    _Base_ptr _M_right;
};
```

Recovered tree-head/header behavior also matches:
- head `+0x04` = root
- head `+0x08` = leftmost / first
- head `+0x0c` = rightmost / last
- empty state:
  - `root = nullptr`
  - `first = head`
  - `last = head`

That is the same header-cell design described in `stl_tree.h`.

### 2. Distinctive decrement helper match

Recovered helper `0x4151b0` contains the classic SGI/libstdc++ red-header sentinel special case:

```cpp
if (x->color == red && x->parent->parent == x)
    x = x->right;
```

That is the strongest single fingerprint for classic `_Rb_tree_decrement`.

### 3. Helper taxonomy match

The recovered helper split matches the classic `_Rb_tree` helper family:

| launcher.exe | current Ghidra rename | classic STL analogue |
|---|---|---|
| `0x415170` | `SGIStlTree_S_minimum` | `_Rb_tree_node_base::_S_minimum` |
| `0x415190` | `SGIStlTree_S_maximum` | `_Rb_tree_node_base::_S_maximum` |
| `0x4151b0` | `SGIStlTree_decrement` | `_Rb_tree_decrement` |
| `0x415200` | `SGIStlTree_rotate_left` | `_Rb_tree_rotate_left` |
| `0x415250` | `SGIStlTree_rotate_right` | `_Rb_tree_rotate_right` |
| `0x4152a0` | `SGIStlTree_insert_and_rebalance` | `_Rb_tree_insert_and_rebalance` |
| `0x4154d0` | `SGIStlTree_rebalance_for_erase` | `_Rb_tree_rebalance_for_erase` |

### 4. Reuse across unrelated containers

The helper family is reused across multiple launcher subsystems, which fits a shared STL/container
implementation much better than a one-off custom tree.

---

## Recovered launcher users

## A. `CLTThreadPerClientTCPEngine`

The network-engine object at `0x4d6304` uses two tree families:

### Endpoint-keyed tree (`+0x80`, `0x24` nodes)
- `0x4318f0` = `CLTThreadPerClientTCPEngine_EndpointTree_InsertUniqueHint`
- `0x42fdb0` = `CLTThreadPerClientTCPEngine_EndpointTree_Find`
- `0x431240` = `CLTThreadPerClientTCPEngine_EndpointTree_InsertNode`
- `0x4154d0` = shared erase/rebalance helper
- `0x431110` = recursive free for the `0x24` node family

Recovered node shape:
- link prefix `+0x00..+0x0f`
- key at `+0x10` = endpoint / `sockaddr_in`-like key
- payload at `+0x20` = `AcceptThread`-style payload pointer

Current compare-helper note from `0x44b040` as used by the endpoint tree wrappers:
- ordering is currently evidenced on `portNetworkOrder`, then `ipv4NetworkOrder`
- recovered `family/reserved0/reserved1` fields are still copied in the node key payload, but are
  not currently evidenced as tree-ordering fields in launcher.exe

Current source status:
- source now keeps the recovered `+0x80/+0x84` head/count surface live and manipulates explicit
  `0x24` launcher-shaped nodes through the narrow SGI-tree compatibility shim
- insertion still follows the original staged flow (`InsertUniqueHint -> socket setup -> payload
  store`), but no longer routes through a source-owned `std::map` sidecar

### Context-keyed tree (`+0x8c`, `0x18` nodes)
- `0x4196b0` = `CLTThreadPerClientTCPEngine_ContextTree_InsertUniqueHint`
- `0x42fe10` = `CLTThreadPerClientTCPEngine_ContextTree_Find`
- `0x420ba0` = `CLTThreadPerClientTCPEngine_ContextTree_InsertNode`
- `0x4154d0` = shared erase/rebalance helper
- `0x431160` = recursive free for the `0x18` node family

Recovered node shape:
- link prefix `+0x00..+0x0f`
- key at `+0x10` = 32-bit context key
- payload at `+0x14` = `WorkerThread`-style payload pointer

Current source status:
- source now keeps the recovered `+0x8c/+0x90` head/count surface live and manipulates explicit
  `0x18` launcher-shaped nodes through the same narrow SGI-tree compatibility shim
- worker insertion/removal now mirrors the recovered unique-insert/find/erase wrapper family more
  directly instead of routing through a source-owned map abstraction

## B. Console-variable registry tree

String-keyed tree use also exists outside the network engine:
- `0x4157b0` = `CConsoleVarRegistryTree_Find`
- `0x415c40` = `CConsoleVarRegistryTree_InsertNode`
- `0x415fc0` = `CConsoleVarRegistryTree_InsertUniqueHint`
- `0x4161f0` = `CConsoleVarRegistryTree_ctor`
- `0x456a90` = `CConsoleVarRegistryTree_EraseNode`
- `0x4162c0` = console variable ctor/user that reaches the registry wrapper family

Current compare-helper note:
- the registry wrappers compare string keys with `_stricmp`, i.e. case-insensitive ordering/search

Current duplicate-registration note from `0x4162c0`:
- duplicate names are not replaced in the tree
- the duplicate path builds the string `A console variable named "..." already exists!`
- but current static RE does not yet show a confirmed surviving user-visible sink before the wrapper
  still reaches the unique-insert path, so current source keeps duplicate registration as a silent
  no-op pending stronger evidence

This is strong evidence that the shared tree helper family is generic launcher infrastructure,
not something unique to the TCP engine.

## Current MSVC-target Clang build status

The original MSVC-target Clang blocker from this area was the direct inclusion of
`<bits/stl_tree.h>` into translation units that otherwise use the MSVC STL.
That blocker is now removed.

Current bounded status after the shim pass:
- stable GCC / MinGW path still builds
- GNU-target Clang still builds
- MSVC-target Clang now compiles past the tree-header incompatibility and reaches link-stage CRT
  selection issues instead
- current next blocker is no longer the tree helper lineage itself; it is the Debug-profile
  runtime-library mismatch in the experimental xwin/MSVC-ABI path (for example `cryptopp.lib`
  objects still advertising `MDd_DynamicDebug` while only retail CRT import libraries are present
  in this environment)

## C. Other integer-keyed tree users

Further non-engine reuse currently evidenced by xrefs and wrapper helpers:
- `0x415f20`
- `0x4568a0`
- `0x47e8e0`
- `ResolveResourcePath` also reaches the shared insert/rebalance family

These users reinforce the conclusion that we are seeing a shared STL-style RB-tree helper layer
with multiple key/payload wrappers on top.

---

## Important distinction: multiple users, not multiple underlying tree implementations

Current Ghidra evidence supports this split:

- one **shared** low-level RB-tree implementation:
  - min / max / decrement
  - left/right rotation
  - insert rebalance
  - erase rebalance
- multiple **wrapper families** on top of it:
  - endpoint-keyed engine tree
  - integer-keyed engine tree
  - case-insensitive string-keyed console tree
  - other integer-keyed launcher trees

So the launcher does **not** currently appear to contain several unrelated tree algorithms here.
Instead, it appears to contain one SGI/libstdc++-style RB-tree implementation reused by many
container instantiations/adapters.

---

## Current source decision: one narrow project-owned SGI-tree compatibility shim

For the current faithfulness pass, the project no longer keeps a separate local mirror in
`src/launcher_tree.*`, but it also no longer includes `<bits/stl_tree.h>` directly from project
translation units.

Instead, recovered users now route through one narrow project-owned compatibility surface in
`compat/sgi_tree_compat.h`, using the recovered launcher node/head layouts as the concrete objects
passed into:
- `_Rb_tree_insert_and_rebalance`
- `_Rb_tree_rebalance_for_erase`

The current source still reads the recovered `_Rb_tree_node_base` / `_Rb_tree_node<_Val>`-style
layout surface (`_M_parent/_M_left/_M_right`, `_M_valptr()`), but that surface is now defined by
our bounded shim rather than by direct dependency on libstdc++ internals.

Current rationale:
- the recovered launcher helper family matches the SGI/libstdc++ lineage closely enough that a
  small donor-shaped shim is the most faithful simplification for this pass
- keeping one project-owned shim removes the MSVC-STL incompatibility caused by directly including
  libstdc++ internal tree headers into mixed-STL translation units
- launcher.exe still remains the authority for wrapper behavior, key comparison, and object layout
- node/head structs in project source stay source-owned so recovered `0x24` / `0x18` payload shapes
  and `+0x80` / `+0x8c` sentinel heads remain explicit

This means the current direction is now:
- **keep one project-local compatibility façade** (`compat/sgi_tree_compat.h`)
- **treat MinGW SGI/libstdc++ `stl_tree.h` as donor/reference provenance, not as a directly included build dependency**
- **do not reintroduce a second project-specific container wrapper layer on top of that façade unless
  a future fidelity need forces it**

---

## Current project naming / documentation state

### Ghidra names applied

Low-level upstream-lineage helpers now use a distinct SGI/libstdc++-style prefix in Ghidra so the
boundary between upstream `_Rb_tree` mechanics and project-owned wrapper families stays explicit:
- `0x415170` → `SGIStlTree_S_minimum`
- `0x415190` → `SGIStlTree_S_maximum`
- `0x4151b0` → `SGIStlTree_decrement`
- `0x415200` → `SGIStlTree_rotate_left`
- `0x415250` → `SGIStlTree_rotate_right`
- `0x4152a0` → `SGIStlTree_insert_and_rebalance`
- `0x4154d0` → `SGIStlTree_rebalance_for_erase`

Project-owned wrapper families keep project/component-specific names in Ghidra:
- `0x4318f0` → `CLTThreadPerClientTCPEngine_EndpointTree_InsertUniqueHint`
- `0x42fdb0` → `CLTThreadPerClientTCPEngine_EndpointTree_Find`
- `0x431240` → `CLTThreadPerClientTCPEngine_EndpointTree_InsertNode`
- `0x44b040` → `CLTThreadPerClientTCPEngine_EndpointTree_ComparePortIpv4`
- `0x4196b0` → `CLTThreadPerClientTCPEngine_ContextTree_InsertUniqueHint`
- `0x42fe10` → `CLTThreadPerClientTCPEngine_ContextTree_Find`
- `0x420ba0` → `CLTThreadPerClientTCPEngine_ContextTree_InsertNode`
- `0x4157b0` → `CConsoleVarRegistryTree_Find`
- `0x415c40` → `CConsoleVarRegistryTree_InsertNode`
- `0x415fc0` → `CConsoleVarRegistryTree_InsertUniqueHint`
- `0x4161f0` → `CConsoleVarRegistryTree_ctor`
- `0x456a90` → `CConsoleVarRegistryTree_EraseNode`

### Current source usage
`CLTThreadPerClientTCPEngine`, the console-variable registry, and the login-mediator observer tree
now route their tree mechanics through `compat/sgi_tree_compat.h`.

The current source split is:
- shared low-level RB-tree mechanics exposed as `mxo::sgi_tree::*`
- all build families: the same narrow project-owned implementation behind that façade
- donor/reference lineage: classic MinGW SGI/libstdc++ `stl_tree.h`
- direct `_Rb_tree_node<std::pair<key,payload*>>`-style node types for recovered engine and
  console-registry node families
- engine-specific comparison/search/container decisions in
  `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp`
- console-registry-specific case-insensitive search/insert/erase decisions in
  `matrixstaging/runtime/src/libltbase/consolevar.cpp`
- mediator-observer-specific pointer-keyed insert/erase/range helpers in
  `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
  - current source no longer re-owns the SGI tree mechanics there
  - the recovered outer layer is now modeled directly as a unique observer set keyed by the
    observer pointer, with the owner `+0x674` header/count retained only as a lightweight mirror
    for logging/layout diagnostics

Current non-duplication boundary in source:
- **kept**: launcher-specific key comparison, search shape, duplicate handling, payload/backing
  ownership, and thin head-layout adapters that cast the recovered launcher sentinel heads onto the
  `_Rb_tree_node_base` layout
- **removed**: local reimplementations of low-level rebalance/unlink mechanics
- **newer cleanup**: local header relink/sync helpers were also pruned once the direct `_Rb_tree`
  insert/erase path was in place, because the upstream helpers already maintain the header
  `root/leftmost/rightmost` links for non-empty trees
- **console registry follow-up**: the older source-owned `std::vector<CConsoleVar*>` registry was
  also retired in favor of the recovered case-insensitive string-keyed tree wrapper family above the
  same `_Rb_tree` core

---

## Source-structure hypothesis from the recovered endpoint/context tree families

The recovered `CLTThreadPerClientTCPEngine_EndpointTree*` and
`CLTThreadPerClientTCPEngine_ContextTree*` families strongly suggest that the original engine code
was **logically more granular** than the current monolithic reimplementation file.

What supports that reading:
- the endpoint-keyed and context-keyed tree wrappers are distinct enough to look like real internal
  sub-families, not one undifferentiated blob of hand-written pointer code
- the tree logic now matches a classic STL-family implementation style, which historically often
  lived across a mix of header-defined template code, inline helpers, and thin wrapper code
- repeated near-identical tree-helper bodies in a final binary are compatible with header/template
  instantiation and older STL implementation styles

What this does **not** prove yet:
- that the original project definitely used multiple `ltthreadperclienttcpengine*.cpp` files
- that every repeated tree helper body in `launcher.exe` corresponds to a separate translation unit
- that the current single recovered source-path string is misleading or incomplete

Current cautious conclusion:
- the original engine implementation was likely **more structurally split** than the current
  single-file reimplementation
- but the exact original translation-unit / file split remains **unproven**

---

## Open follow-up questions

1. Are there still more launcher containers using the same shared RB-tree helpers that have not yet
   been documented?
2. Do any wrappers deviate materially from classic SGI/libstdc++ behavior beyond key comparison and
   node payload size?
3. Is there any surviving static evidence for the original local wrapper names beyond the current
   descriptive names?

For now, the best project-wide conclusion is:

> launcher.exe uses a shared SGI STL / old libstdc++ `stl_tree`-style red-black tree helper family,
> reused by `CLTThreadPerClientTCPEngine` and other launcher systems.
