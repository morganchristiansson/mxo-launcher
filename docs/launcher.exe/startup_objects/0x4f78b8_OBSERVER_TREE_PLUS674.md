# `0x4f78b8 + 0x674` - login observer tree

## Overview

This is the launcher-owned observer/listener container walked by:

- `0x41cfb0 = CLTLoginMediator_PostEvent`
- `0x41d090 = CLTLoginMediator_PostError`

It is also the target of the arg6 / `ILTLoginMediator.Default` registration bridge:

- `+0x170 -> 0x41ddb0 = CLTLoginMediator_RegisterLoginObserver`
- `+0x174 -> 0x41dde0 = CLTLoginMediator_UnregisterLoginObserver`

Current best read: this is a std::_Tree-like ordered container keyed by the observer pointer itself.
The replacement now source-owns that shape closely enough to do in-order event/error walks without
flattening it into a vector, but balancing/color bits and original node-pool recycling are still not
fully reconstructed.

Current concrete observer identities on the active route:
- `0x629ddfc8`
  - `ClientShell_LoginMediatorObserver`
  - ctor now named in Ghidra as `ClientShell_LoginMediatorObserver_ctor` (`0x6217f370`)
  - the successful replacement game-entry run still shows this as the only observed live
    registration on the post-state9 path
- `0x6298a760`
  - `LoginMediatorObserverForwarder` registered from `RsiLayoutsView_ctor` (`0x62056585`)
- `0x6298a5e8`
  - `LoginMediatorObserverForwarder` registered from `LoadingAreaCommonLayoutView_ctor`
    (`0x62031136`) and unregistered by the paired dtors at `0x620301e0 / 0x620304a0`
  - latest successful replacement caller-logging run did **not** observe this registration, so its
    absence is currently a real not-taken late-runtime branch rather than a logging blind spot
  - strongest current reason is now concrete:
    - event `0x0b` loads mediator `+0xc0` byte `+0x464` into client global `DAT_629e689d`
    - latest successful replacement run logged that byte as `0x01`
    - `ClientShell_LoginMediatorObserver_AdvanceState` checks that flag first and, when non-zero,
      skips the later `GetOrCreateViewById(0x67)` path that would have reached this registration

## Container shape

Recovered from `0x41cfb0 / 0x41d090 / 0x41ddb0 / 0x41dde0 / 0x419510 / 0x41d430`.

```cpp
struct LoginObserverTreeNode674 {
    void* reserved00;                 // current meaning unresolved in this bounded pass
    LoginObserverTreeNode674* parent04;
    LoginObserverTreeNode674* left08;
    LoginObserverTreeNode674* right0c;
    void* observer10;                 // comparison key / stored observer pointer
};

struct LoginObserverTree674 {
    LoginObserverTreeNode674* header00;
    uint32_t count04;
};
```

### Header-node roles

The tree header is self-referential when empty:

- `header + 0x04` = root
- `header + 0x08` = leftmost / begin
- `header + 0x0c` = rightmost

`0x41cfb0` and `0x41d090` both start iteration from:

- `esi = [ [this+0x674] + 0x08 ]`

and stop when the iterator reaches the header pointer stored at:

- `[this+0x674]`

## Traversal shape

`0x41cfb0` (`PostEvent`) and `0x41d090` (`PostError`) use the same in-order successor walk:

- if current node has a right child:
  - move to right child
  - then chase left children to the minimum
- otherwise:
  - climb parents until leaving a right-child chain
  - then use that parent as successor when appropriate

That exact successor shape is also isolated by `0x41baa0`, which counts the distance between two
iterators by repeatedly advancing with the same logic.

## Callback dispatch

### `0x41cfb0 = CLTLoginMediator_PostEvent`

For each node:

- load observer pointer from `node + 0x10`
- load observer vtable
- call vtable slot `+0x00`

That is the observer `OnLoginEvent(eventNumber)` path.

### `0x41d090 = CLTLoginMediator_PostError`

For each node:

- load observer pointer from `node + 0x10`
- load observer vtable
- call vtable slot `+0x04`

That is the observer `OnLoginError(errorNumber)` path.

## Registration / removal helpers

### `0x41ddb0 = CLTLoginMediator_RegisterLoginObserver`

`0x41ddb0` is very small and delegates to `0x415f20` over `this + 0x674`.

`0x415f20`:

- searches the tree using the observer pointer at key offset `node + 0x10`
- finds the insertion position / existing node
- inserts through `0x452c10` when needed
- fills a small result pair: `{ nodePtr, insertedFlag }`

Important bounded detail:

- `0x41ddb0` returns `!insertedFlag`
- i.e. the return value is inverted relative to the intuitive “insert succeeded” meaning

Current replacement note:

- source now mirrors that odd return-value shape too, because static disassembly is explicit and no
  known caller depends on the more intuitive alternative

### `0x41dde0 = CLTLoginMediator_UnregisterLoginObserver`

`0x41dde0` is also small and delegates to three helpers over `this + 0x674`:

1. `0x419510`
   - builds an equal-range pair for the observer pointer key
   - current best read:
     - out[0] = lower_bound (first node with key `>= observer`)
     - out[1] = upper_bound (first node with key `> observer`)
2. `0x41baa0`
   - counts the iterator distance between those two nodes
3. `0x41d430`
   - erases the range `[first, last)`
   - has a full-range fast path when erasing the whole tree

Important bounded detail:

- `0x41dde0` returns `rangeCount == 0`
- again, that is inverted relative to the intuitive “erase succeeded” meaning

Current replacement note:

- source now mirrors that helper sequence and return-value shape too

## Relation to error plumbing

This container matters directly for the stale-session / unclean-shutdown popup path:

- `0x43f930` (`MS_LoadCharacterReply` / raw `0x10`) failure writes raw server status to owner `+0x80`
- then switches state to `3`
- then calls `0x41d090 = PostError(10)`
- `0x41d090` walks this tree and calls each observer `OnLoginError(10)`
- observer-side handlers then query mediator `+0x178 = 0x41f240`, the tiny getter for owner `+0x80`
- that is how the detailed status like `0x0b000025` reaches the popup logic

## Source home

- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator.h`

## Remaining gaps

Still unresolved / only partially mirrored:

- balancing/color bits of the original std::_Tree-like nodes
- original node allocation / free-list recycling path around `0x41d430`
- exact semantics of node field `+0x00`
- why the current successful replacement route still keeps the tree at only the
  `0x629ddfc8` client-shell observer after event `0x18`, instead of later adding
  `0x6298a5e8`
- whether any caller materially depends on the inverted register/unregister return values
  (no current active path has shown that yet)
