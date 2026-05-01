# launcher global `0x4d6304`

## High-confidence identity

`0x4d6304` stores a **heap-allocated launcher-owned object** created during startup and later passed both:

- into the interface from `0x4d2c58`
- into `client.dll!InitClientDLL` as arg5

The current best concrete identity is:

- **`CLTThreadPerClientTCPEngine`-family object**

This is supported by the vtable at `0x4b2768`, which is immediately followed in `.rdata` by the class-name string:

- `CLTThreadPerClientTCPEng...`

## Source of truth

### Builder function
- `launcher.exe:0x40a380` = `Launcher_InitializeThreadPerClientTCPEngine`

### Constructor
- `launcher.exe:0x431c30` = `CLTThreadPerClientTCPEngine_ctor`
- `launcher.exe:0x4366f0` = `CLTBaseThreadPerClientTCPEngine_ctor` (current provisional base-class name)
- `launcher.exe:0x436610` = `CLTBaseThreadPerClientTCPEngine_InitializeQueuePair`
- `launcher.exe:0x436340` = `CLTBaseThreadPerClientTCPEngine_Queue_Init`
- `launcher.exe:0x4365a0` = `CLTThreadPerClientTCPEngine_QueueThread_ctor`
- `launcher.exe:0x436fd0` = `CLTBaseThreadPerClientTCPEngine_dtor`
- `launcher.exe:0x437050` = `CLTBaseThreadPerClientTCPEngine_deleting_dtor`
- `launcher.exe:0x431310` = `CLTThreadPerClientTCPEngine_dtor`
- `launcher.exe:0x4319a0` = `CLTThreadPerClientTCPEngine_deleting_dtor`

### Primary users
- `0x40a3e9..0x40a3fe`
- `0x40a57a..0x40a59a`
- `0x40ed7c..0x40eda9`
- `0x41b16d..`
- cleanup at `0x40b389..0x40b404`

## Construction path

At `0x40a380` the launcher:

1. allocates `0xb4` bytes
2. calls ctor `0x431c30`
3. stores the pointer in `0x4d6304`
4. immediately hands the new object to the interface at `0x4d2c58`

Relevant sequence:

```asm
push 0xb4
call alloc
...
mov ecx, eax
push 0
call 0x431c30
...
mov ecx, [0x4d2c58]
mov [0x4d6304], eax
mov edx, [ecx]
push eax
call [edx+0x08]
```

Current startup-ownership clarification:
- this is the faithful launcher-owned arg5 -> arg6 handoff corridor
- current replacement work should therefore keep mediator/network-engine install here rather than
  inside lazy arg5-side sidecar accessors
- later arbitrary arg5 virtual dispatch may resolve the liblttcp sidecar, but it should not be the
  first place that performs mediator bind/reset side effects
- current source now routes the resolved arg6 wrapper handoff into
  `CLTLoginMediator::Initialize(networkEngineOverride)` so the startup-owned effects stay
  concentrated on this path rather than leaking back into lazy arg5 accessors
- that startup-owned helper now also aligns the source-owned owner `+0x680` child lifecycle a bit
  more closely with `0x41b160/0x41f510`: allocate on initialize, drop on clear/reset

## Constructor evidence

Ctor `0x431c30` / `CLTThreadPerClientTCPEngine_ctor`:

- first calls base ctor `0x4366f0` / `CLTBaseThreadPerClientTCPEngine_ctor`
- then overwrites the primary vtable with `0x4b2768`
- allocates and initializes internal list/tree objects at offsets around:
  - `+0x80` = `0x24`-byte sentinel-headed endpoint map object
  - `+0x8c` = `0x18`-byte sentinel-headed context map object
  - `+0x98` = derived helper root
  - `+0x9c` = `CRITICAL_SECTION` storage paired with that helper root
- current focused static pass still does **not** show ctor writes to `+0x88` or `+0x94`
  - so those two dwords remain lower-confidence / reserved in source naming for now
  - important faithfulness caveat: original `0x40a380` uses `malloc(0xb4)` rather than a zeroing allocator, so absent later evidence those bytes are best treated as **indeterminate on the original path**, not as proven zeros
- newer targeted consumption search also failed to show convincing arg5-object reads of those offsets on the currently known launcher/client engine paths
  - launcher engine-range scan around the `CLTThreadPerClientTCPEngine` family only surfaced unrelated virtual-call offsets in the `0x437470..0x437e80` helper cluster, not concrete `this+0x88/0x94` data reads on the arg5 engine object itself
  - client-side direct arg5 holders / users currently read as:
    - `InitClientDLL` stores arg5 in `0x62b073e4`
    - `0x62006c30` checks that global and drives `0x62532130`
    - `0x62531c10` then consumes arg5 fields at `+0x0c/+0x34/+0x5c/+0x60`
  - current focused pass therefore still has **no** positive evidence that client runtime consumption depends on arg5 `+0x88` or `+0x94`

Base ctor `0x4366f0` / `CLTBaseThreadPerClientTCPEngine_ctor` itself:

- sets base vtable `0x4b3e74`
- preserves ctor flag/count input at `+0x04`
- zeroes `+0x08`
- initializes the paired queue region at `+0x0c` / `+0x34` through:
  - `0x436610` / `CLTThreadPerClientTCPEngine_InitializeQueuePair`
  - `0x436340` / `CLTThreadPerClientTCPEngine_Queue_Init`
- initializes a subobject at `+0x5c`
- initializes another helper subobject rooted at `+0x60`
- creates the event stored at `+0x7c`
- calls `0x452e00` / `CLTSocketLayer::Init` before queue-thread allocation, so process-wide Winsock bootstrap is part of the engine ctor path rather than an unrelated later side effect
- allocates an array at `+0x08` when the effective queue-thread count is non-zero
- constructs per-entry helper objects of size `0x3c` through `0x4365a0` / `CLTThreadPerClientTCPEngine_QueueThread_ctor`

New non-original-state audit from the current constructor / destructor / worker-insert pass
(`0x431c30`, `0x4366f0`, `0x431310`, `0x431ff0`):
- the original `0xb4` object body is already fully spoken for by the recovered fields above plus:
  - endpoint tree / count at `+0x80/+0x84`
  - context tree / count at `+0x8c/+0x90`
  - cleanup lock helper at `+0x98`
- so any source-owned bookkeeping around launcher-ABI attachment or auth/margin bridge contexts is
  **not** evidence for hidden launcher fields
- current best mapping is now tighter in source too:
  - real object `+0x04/+0x08` now directly own the queue-thread count / pointer-array backing in the
    class instead of hiding that family in a synthetic side record
  - recovered endpoint-keyed and context-keyed payload families are now kept in dedicated
    source-owned tree backings keyed by engine identity, not as pretend hidden launcher fields
    - endpoint backing now uses direct MinGW `_Rb_tree_node<std::pair<key,payload*>>` node types
      with the same recovered `0x24` node size / layout expected by the
      `0x4318f0 / 0x42fdb0 / 0x4154d0` family and keeps launcher-visible head `+0x80`
      `root/first/last` pointers live instead of only toggling a fake occupancy marker
      - current tighter source correction there also stops pretending `[node+0x20]` is a
        higher-level wrapper record: it now mirrors launcher.exe more closely as the direct
        `AcceptThread` object pointer, with source-owned backing only retaining ownership of that
        object outside the raw tree node
    - context backing now uses direct MinGW `_Rb_tree_node<std::pair<key,payload*>>` node types
      with the same recovered `0x18` node size / layout expected by the
      `0x4196b0 / 0x42fe10 / 0x4154d0` family and keeps launcher-visible head `+0x8c`
      `root/first/last` pointers live instead of only toggling a fake occupancy marker
      - current tighter source correction there also stops pretending `[node+0x14]` is a
        higher-level wrapper record: it now mirrors launcher.exe more closely as the direct
        `WorkerThread` object pointer, with source-owned backing only retaining ownership of that
        object outside the raw tree node
  - source-only launcher-ABI attachment now lives in a discrete engine-keyed map, while launcher
    bridge contexts remain mediator-owned records hung directly off each connection's owner/context
    pointer instead of a synthetic `CLTThreadPerClientTCPEngine_SideState` aggregate
    - current Ghidra pass also sharpened why auth/margin-specific engine maps were infidelity:
      - `0x4325d0` / `0x4328a0` pass the direct connection object into worker helper `0x431ff0`
      - `0x431ff0` stores `[connection+0x08] = workerThread` and inserts key=`connection` into the
        `+0x8c` context tree
      - `0x4316a0` / `CleanupConnection` later searches that same `+0x8c` tree by the raw
        connection pointer
      - `0x449d40` / `CLTTCPConnection::OnReceive` enqueues completed operations with
        `context = this /* connection */`
      - `0x436b10` then consumes that queued `context` directly and invokes slot 12 on type-1 work
      - so the original engine tracks direct connection objects through this family, not separate
        auth/margin bridge-context records
    - current source consequence:
      - the old auth/margin-specific engine maps were removed
      - source connection resolution is now limited to RE-backed identities only:
        - direct connection object pointer
        - direct mediator owner stored at connection `+0xa4`, with any extra bridge record kept as a
          mediator-owned sidecar resolved from that owner/child pair
        - active worker/context-tree payloads keyed by the direct connection pointer
      - there is no longer any generic engine-keyed known-connection registry on this path
    - negative result from the current re-check of
      `0x4316a0/0x431ff0/0x4325d0/0x4328a0/0x436820/0x436b10/0x449d40`:
      - no positive Ghidra evidence shows original engine queue/worker paths owning
        `CLTLoginMediator`-specific objects or class-specific context records
      - current `CLTLoginMediatorConnectionContextScaffold` /
        `CLTLoginMediatorQueuedWorkItemScaffold` remain source-owned bridge baggage only
      - so any direct `loginmediator.h` dependency left inside `liblttcp` should still be treated
        as non-original debt to prune, not as recovered engine structure
  - earlier generic fallback engine-owned `CMessageConnection` allocation has now been retired from
    the active slot-resolution path because current RE does not support it as original engine state
  - dead source-only accessors kept only for compile compatibility (`MonitoredPorts()`,
    `WorkerThreads()`, `HasMonitoredPorts()`, `HasWorkerThreads()`, and the older fallback-message
    connection stubs) have now been pruned instead of being left behind as fake compatibility API

New shared-tree clarification from the current helper-xref pass:
- the tree helper family used by arg5 `+0x80/+0x8c` is **not** unique to
  `CLTThreadPerClientTCPEngine`
- current shared helper anchors include:
  - predecessor / search-side helper: `0x4151b0`
  - rotations: `0x415200`, `0x415250`
  - insert rebalance: `0x4152a0`
  - erase / rebalance: `0x4154d0`
- current helper shape is also strongly reminiscent of the old SGI / STL red-black-tree helper
  family rather than an engine-unique custom container:
  - `0x4151b0` has the same header-special-case pattern as `_Rb_tree_decrement`
  - `0x415200/0x415250` are left/right rotations
  - `0x4152a0` is insert rebalance
  - `0x4154d0` is erase rebalance / unlink
- non-engine xrefs now show the same generic family is also reused by other launcher containers,
  including:
  - console-variable registry string trees through `0x415fc0` / `0x4162c0`
  - other integer-keyed trees through `0x415f20`, `0x4568a0`, `0x47e8e0`, and related helpers
- so the current best naming direction is a **shared launcher tree helper family** rather than an
  engine-specific tree implementation
- current source now routes the arg5 tree family directly through MinGW libstdc++
  `<bits/stl_tree.h>`, using the recovered launcher node/head layouts as the concrete `_Rb_tree`
  objects while still treating launcher.exe itself as source of truth
- the intended donor/reference is `/usr/lib/gcc/i686-w64-mingw32/13-win32/include/c++/bits/stl_tree.h`
  (the local `13-posix` copy is identical on this machine)
- current arg5 tree users now call the low-level `_Rb_tree` helpers directly
  (`_Rb_tree_insert_and_rebalance`, `_Rb_tree_rebalance_for_erase`) and keep
  engine-specific search/insert decisions local
- the currently recovered endpoint-key compare helper (`0x44b040`) orders by
  `portNetworkOrder`, then `ipv4NetworkOrder`; the wider copied endpoint-key payload still carries
  `family/reserved` fields, but those are not currently evidenced as tree-ordering fields
- once that direct insert/erase path was in place, the older local header relink/sync helpers were
  pruned too; current source now relies on upstream `_Rb_tree` header maintenance for non-empty
  trees instead of resynchronizing `root/first/last` by hand after each change

New queue-thread clarification from the current focused pass:
- these `0x3c` children are not anonymous queue blobs
- `0x4365a0` first builds a shared generic thread base through `0x4319e0` / `CLTThread_ctor`
  - that base is the same thread-family surface also reused by the engine's `AcceptThread`, `WorkerThread`, and other launcher thread objects
- the queue-thread ctor then stores the owning engine pointer at child `+0x38`
- and overwrites the shared base vtable with queue-thread vtable `0x004b3e28`
- current high-confidence queue-thread-specific override there is slot `+0x20`:
  - `0x436fc0` / `CLTThreadPerClientTCPEngine_QueueThread_Run`
  - which simply calls the owner engine's blocking completed-operation consumer loop:
    - `0x436b10` / `CLTBaseThreadPerClientTCPEngine_RunCompletedOperationQueue(owner, 0)`
- so queue-thread start / stop / wait / dtor semantics currently come from the shared generic `CLTThread_*` base methods rather than from large queue-thread-specific lifecycle code
- source scaffolds now mirror that relationship under:
  - `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h`
  - `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp`
  - with a generic `CLTThread` base, a `CLTThreadPerClientTCPEngine_QueueThread` child, and a source-level `RunCompletedOperationQueue(bool)` entrypoint kept in lockstep with the current RE naming even though the queue-consumer body is still only scaffolded there
- newer lockstep source cleanup also moved the recovered queue-storage helpers into canonical liblttcp-side source names instead of keeping queue init/push/free logic only inside `src/launcher_network_object_abi.cpp`
  - current source anchors there are explicitly commented as:
    - `Queue_Init` <- `0x436340`
    - `Queue_PushPair` <- `0x436670 / 0x436820`
    - `Queue_IsEmpty` <- `0x436b10` / client `0x62531c10` empty-check shape
    - `Queue_TryPopPair` <- `0x436d31..0x436ee7` consumer pop shape
  - launcher ABI glue now delegates queue init/push/free to those liblttcp helpers rather than carrying a fully separate duplicate queue implementation
- newer lockstep source cleanup also added an explicit sidecar bridge for the recovered ownership mismatch in the current implementation:
  - the runtime-visible queue fields still live on the launcher ABI object (`+0x0c` / `+0x34`)
  - the current class-side attachment model now reaches those live shell queues through one explicit ABI-surface attachment map rather than a queue-only sidecar hook
  - source-level `RunCompletedOperationQueue(bool)` can therefore model the current best consumer ordering against either:
    - the live launcher-visible queue storage when attached, or
    - the class-owned fallback queue surrogates when not attached
  - current source-level consumer skeleton now preserves these highest-confidence rules only:
    - prefer queue34 when non-empty, else queue0C
    - null work item means shutdown-style sentinel
    - cleanup/slot-12-style connection teardown logically precedes later context callback
    - newer bounded correction: active source now keeps the recovered direct connection identity on
      that path, but wraps it in a tiny queue-dispatch ABI adapter when raw client.dll consumers are
      still involved; slot-12-style cleanup therefore unwraps back to the owning connection object
      before worker/message-table teardown
    - newer bounded correction: the blocking `RunCompletedOperationQueue(false)` path no longer
      returns immediately on empty queues; when an attached arg5 queue-signal event exists it now
      waits on that event and re-enters the dequeue loop, matching the recovered `+0x5c`
      wait-helper role more closely
    - newer bounded correction: queue selection/pop now happens while the attached arg5 queue lock
      is held, and the blocking empty-queue path releases that lock before the wait and reacquires
      it on the next loop iteration
    - later runtime confirmation overturned that earlier A/B note:
      - commit `ab28b26` itself is now the confirmed regression point for the late launcher-into-
        game corruption/crash family
      - current source therefore no longer keeps a runtime toggle here; the active producer path
        keeps the recovered direct-connection identity but now projects it through a tiny
        queue-dispatch ABI adapter while the remaining late instability investigation stays focused
        on replacement-only callback timing / release gaps
    - newer bounded correction: the shutdown-sentinel cascade now re-enters the normal enqueue
      helper path instead of open-coding a raw `Queue_PushPair(0,0)` write
    - newer bounded correction: source now also keeps the recovered `0x4816f0(workItem)`-style
      type read explicit at dequeue time before the later slot-12/context callback branch
    - newer bounded correction: after the context callback returns, source now mirrors the
      recovered release order more closely by doing any conditional type-1 context auto-release
      before the final work-item release
    - remaining gap there is now sharper too: the order matches the current static read, but the
      concrete release bodies are still source-owned vtable-dispatch scaffolds rather than exact
      recovered object-class implementations
    - newer bounded correction: `CleanupConnection` now always runs the owner-state sync tail at
      function exit, even on miss/no-worker paths, as the bounded source stand-in for the later
      always-run `0x44ab60(arg)` side effect recovered from original slot `12`
    - newer bounded correction: slot-12 cleanup now also drops any generic engine-owned fallback
      `CMessageConnection` entry for that context key after worker teardown or on later miss/tail
      cleanup, which keeps source closer to the original pointer-keyed unique-owner model
    - newer bounded correction: worker lookup itself now again keys off the recovered direct
      connection identity and direct owner-context forms instead of only one raw stored key
    - newer bounded correction: worker-teardown now makes the intermediate closing state explicit
      before the later wakeup/stop/remove steps instead of jumping straight from active to closed
      only at the tail
    - newer bounded correction: cross-block dequeue no longer immediately frees the exhausted head
      `0x80` queue block; source now caches it for later `Queue_PushPair` growth reuse, which is a
      closer source-owned mirror of the original block recycling / free-list behavior
  - that source consumer is still explicitly scaffold-first and should not yet be treated as a faithful final replacement for original `0x436b10`

## New clarification: arg5 contains helper subobjects the client can call directly

Recent static analysis tightened one important detail that the earlier summary understated.

The arg5 object is not only a primary-vtable object with some passive fields.
At least three internal regions are themselves object-like helper surfaces:

- `+0x5c` holds a vtable pointer used with `ecx = arg5 + 0x5c`
- `+0x60` holds a vtable pointer used with `ecx = arg5 + 0x60`
- `+0x98` holds another helper-object root paired with ctor work on `+0x9c`

High-confidence proof from original ctor/static client usage:

- `0x436715` / `0x436732` updates the helper-object root at `+0x5c`
- `0x43671d` / `0x436739` seeds the helper-object root at `+0x60`
- `0x431cbe` seeds the helper-object root at `+0x98` and constructs data at `+0x9c`
- later, `client.dll:0x62531c20` does:

```asm
mov eax, [esi+0x60]
lea edi, [esi+0x60]
mov ecx, edi
call [eax]
```

- and `client.dll:0x62531ca0` does:

```asm
mov eax, [esi+0x5c]
lea ecx, [esi+0x5c]
push -1
call [eax+0x4]
```

So the client can later drive arg5 through these embedded helper objects without first touching the primary arg5 vtable.
That matters because a scaffold that only fakes the top-level `0x4b2768` vtable still leaves real internal call surfaces missing.

The derived vtable at `0x4b2768` is immediately followed by the class-name string fragment:

- `CLTThreadPerClientTCPEng...`

and nearby strings include networking teardown text such as:

- `CLTThreadPerClientTCPEngine dtor: Unmonitoring ports...`

That makes `CLTThreadPerClientTCPEngine` the current best concrete class identity for this startup object.

## Verified role in client startup

At `0x40a57a` the launcher loads it and passes it into `InitClientDLL`:

```asm
mov ecx, [0x4d6304]
push ecx            ; arg5
```

So arg5 is not optional launcher fluff.
It is a real startup object created on the launcher side before `client.dll` begins.

## Other uses

### Error / reporting helper path
At `0x40ed7c` the launcher embeds `0x4d6304` into a local descriptor together with string table `0x4ac4c0` and passes that structure into `0x41b6c0`.

### Default dependency path
At `0x41b16d` a function uses `0x4d6304` as a default object when its explicit argument is NULL:

```asm
mov eax, [ebp+0x8]
test eax, eax
jne  short ...
mov eax, [0x4d6304]
```

This is more evidence that `0x4d6304` is a shared launcher service object, not a transient local.

### Cleanup path
At `0x40b389..0x40b404` the launcher:

- checks `0x4d6304`
- if non-NULL, calls its vtable slot 0 with `push 1`
- clears `0x4d6304`

So startup and teardown mirror each other around this object.

## High-confidence conclusions

1. `0x4d6304` is a heap object created by the launcher before `InitClientDLL`.
2. The original launcher immediately registers / hands it to the interface from `0x4d2c58`.
3. The same object is then passed into `InitClientDLL` as arg5.
4. The launcher later releases it during cleanup.
5. A reimplementation that passes `NULL` for arg5 is not following the original path.

## Current implication for reimplementation

The next correct experiments should try to reproduce the original launcher-owned creation path for this object and its registration through `0x4d2c58`, rather than seeding unrelated objects inside `client.dll`.

## New clarification from current implementation work

The custom launcher's arg5 probe has now been tightened to match more of the original ctor layout instead of using a fully zeroed `0xb4` block.

Current diagnostic scaffold now mirrors these constructor facts from `0x431c30` / `0x4366f0`:

- object size still `0xb4`
- ctor arg / field `+0x04 = 0`
- base pointer-array field `+0x08 = NULL`
- derived list heads at:
  - `+0x80` -> allocated `0x24` block with self-linked `next/prev`
  - `+0x8c` -> allocated `0x18` block with self-linked `next/prev`
- helper-object roots now seeded at:
  - `+0x5c` -> placeholder helper vtable with logged slot `+0x04`
  - `+0x60` -> placeholder helper vtable with logged slots `+0x00` / `+0x04`, matching the two earliest client-observed calls on the original `0x4add70` helper root (`0x4147b0` / `0x4147c0`)
  - `+0x98` -> placeholder helper vtable with logged slots `+0x00` / `+0x04`, mirroring the derived ctor's reuse of the same `0x4add70`-style helper root before initializing `+0x9c`

This is still **not** a faithful reimplementation of the real `CLTThreadPerClientTCPEngine` object:

- the real base/derived vtables are not reconstructed,
- helper subobjects around `+0x0c`, `+0x64`, and `+0x9c` are still only partial placeholders,
- imported helper results such as the original `+0x7c` value are not reproduced,
- and the current helper-object vtables at `+0x5c/+0x60/+0x98` only provide safe logging stubs for the small method surface observed so far.

But this change matters because it removes one especially crude mismatch: arg5 is no longer just a zero-filled block with a single fake release slot.

## Current experiment result after tightening arg5

Even with the more ctor-shaped arg5 scaffold in place, the latest practical run still:

- reaches the deep mediator sequence
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(...)`
  - `AttachStartupContext(second)`
- and then still crashes with control flow landing in the current `arg2 filteredArgv` pointer array.

So this improved arg5 shape did **not** remove the current late crash by itself.
That does **not** mean arg5 is unimportant.
It means only that the present crash is not explained by the earlier ultra-minimal arg5 stub alone.

## New clarification from early arg5 vtable-slot probes

The custom launcher now also wires the first few original primary vtable slots from `0x4b2768` with matching stack cleanup, and the scaffold has started moving some of them beyond pure probes:

- slot 0 -> `0x4319a0`-style release / destructor probe (`ret 4`)
- slot 1 -> `0x431ce0`-style 3-arg starter wiring (`MonitorPort`)
- slot 2 -> `0x4325d0`-style 3-arg starter wiring (`UDPMonitorPort`)
- slot 3 -> `0x436000`-style 3-arg starter wiring (`MonitorEphemeralUDPPort` wrapper)
- slot 4 -> `0x42f7c0`-style 1-arg probe (`ret 4`)

This is still **not** fully faithful behavior for those methods.
But slot `3` is no longer just a neutral placeholder return: the current implementation now routes it into the starter `CLTThreadPerClientTCPEngine::MonitorEphemeralUDPPort(...)` implementation instead of leaving it as a dormant future probe.

Practical result from the latest deep patched-client runs:

- the launcher still reaches the same post-mediator path,
- the late crash still lands in the launcher-owned `arg2 filteredArgv` region,
- no new arg5 primary-vtable slot-1..4 probe logs appeared before that crash,
- and after restoring arg1/arg2 to heap-backed launcher-owned duplicated storage for better `0x409950` faithfulness, the latest dump still landed at `EIP=arg2` (`~/MxO_7.6005/MatrixOnline_0.0_crash_17.dmp`: `EIP=0x003e3bb2`, current `arg2=0x003e3bb0`).

So, in the current practical path, merely exposing the first original arg5 vtable entries did **not** shift the crash and did **not** yet show evidence that those early arg5 methods are being exercised before the current failure point.

A follow-up faithfulness pass then widened the embedded-helper probe surface slightly further:

- `+0x60` now exposes placeholder slots `+0x00` and `+0x04`
- `+0x98` now exposes placeholder slots `+0x00` and `+0x04`
- this matches the earliest static launcher-side evidence around the shared `0x4add70` helper root used by both base and derived ctors

Current practical result after that follow-up:

- deep startup still reaches the stable mediator sequence
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(...)`
  - `AttachStartupContext(second)`
- the latest dump still lands in launcher-owned arg2 storage
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_19.dmp`
  - `EIP=0x003e3bb2`
- and no new logs from:
  - arg5 primary vtable slots `1..4`
  - arg5 embedded helper slots at `+0x60/+0x98`
  appeared before that crash

That is useful narrowing evidence:

- making arg5's helper/vtable surface less fake is still the correct faithfulness direction,
- but the present late failure is still not explained by the absence of only those earliest arg5 slots,
- so the current remaining launcher-owned risk is more likely in deeper arg5 state, the still-incomplete `0x409950` preprocessing path, or another later launcher/client ownership mismatch.

## Updated priority note

New `InitClientDLL` analysis indicates arg5 (`0x4d6304`) is stored immediately by the client at startup, but the older `-7` failure path is more directly gated by arg6 (`ILTLoginMediator.Default`).

So for the current reimplementation order of work:

1. fix / reconstruct arg6 first,
2. then revisit arg5 for later runtime correctness.

## New clarification from explicit base-queue reconstruction

The custom launcher now initializes the two base queue subobjects inside arg5 much closer to the original `0x436610 -> 0x436340(size=0)` path instead of leaving them zeroed:

- `+0x0c` and `+0x34` are each built as a `0x28` queue object
- each queue now gets:
  - a slot-pointer array with minimum capacity `8`
  - one initial `0x80` block centered in that slot array
  - `current0/block0/end0/slotsCurrent`
  - `current1/block1/end1/slotsLast`
  - `slotsBase/slotCapacity`
- the diagnostic scaffold also now has matching cleanup for:
  - queue slot arrays
  - queue-owned `0x80` blocks
- helper implementations for the original queue-style grow/pop behavior were also added so later arg5 work can use the same recovered model instead of ad-hoc blobs

Current practical result after landing that queue reconstruction:

- deep patched-client startup still reaches the same stable mediator sequence
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(...)`
  - `AttachStartupContext(second)`
- the latest practical crash still lands in launcher-owned arg2 storage rather than moving to a fresh arg5-observed call site
  - earlier reference dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_22.dmp`
  - `EIP=0x003e2b80`
  - current `arg2 filteredArgv = 0x003e2b80`

That means the earlier zeroed queue mismatch was real and worth correcting for faithfulness, but this queue fix by itself still does **not** explain the current late crash.

So the remaining likely suspects stay roughly the same:

- deeper arg5 behavior beyond queue initialization alone,
- still-incomplete launcher-owned preprocessing / side effects around `0x409950`,
- or another later launcher/client ownership mismatch that still turns current arg2 memory into a bad control-flow target.

## New clarification from faithful helper semantics and wider arg5 probes

The arg5 scaffold has now been tightened one step further to match more of the original helper behavior recovered from `launcher.exe`:

- `+0x60` is now modeled as a helper object with:
  - vtable root at `+0x60`
  - real `CRITICAL_SECTION` storage at `+0x64`
- `+0x7c` is now backed by a real event created via:
  - `CreateEventA(NULL, FALSE, FALSE, NULL)`
- `+0x98` is now modeled as another helper object with:
  - vtable root at `+0x98`
  - real `CRITICAL_SECTION` storage at `+0x9c`
- the `+0x5c` helper now follows the recovered Win32 intent more closely:
  - slot `+0x00` -> `SetEvent(field7C)`
  - slot `+0x04` -> leave lock, `WaitForSingleObject(field7C, timeout)`, then reacquire on success/timeout
- the shared helper vtable family behind `+0x60` / `+0x98` is now modeled as:
  - slot `+0x00` -> `EnterCriticalSection`
  - slot `+0x04` -> `LeaveCriticalSection`

This was driven by static recovery of the original imported helper calls:

- `0x4a9094 = InitializeCriticalSection`
- `0x4a9090 = EnterCriticalSection`
- `0x4a908c = LeaveCriticalSection`
- `0x4a9110 = CreateEventA`
- `0x4a910c = SetEvent`
- `0x4a9180 = WaitForSingleObject`

The primary arg5 vtable probe was also widened slightly again:

- slot 11 -> `0x431670`-shaped logging placeholder
- slot 12 -> `0x4316a0`-shaped logging placeholder

## Current experiment result after those deeper arg5 updates

After rebuilding with the new helper semantics and the extra slot-11/12 probes, the launcher still behaved the same in the important ways:

- deep patched-client startup again reached the stable mediator sequence
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(...)`
  - `AttachStartupContext(second)`
- no new arg5 logs appeared before the crash from:
  - primary vtable slots `1..4`
  - primary vtable slots `11..12`
  - helper slots at `+0x5c`
  - helper slots at `+0x60`
  - helper slots at `+0x98`
- the newest dump still landed inside launcher-owned arg2 storage:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_24.dmp`
  - `EIP=0x003e2b62`
  - current `arg2 filteredArgv = 0x003e2b60`

That is useful narrowing evidence.
The more faithful arg5 event/lock helper reconstruction did **not** move the crash and still did **not** surface any observed arg5 method traffic before failure.

So the current best reading is still:

- arg5 remains important and still should be reconstructed faithfully,
- but the present late crash is not yet explained by the absence of only the currently instrumented arg5 helper/vtable behavior,
- and the most likely remaining launcher-owned gaps are still:
  - deeper arg5 state not yet reconstructed,
  - broader `0x409950` launcher preprocessing / `options.cfg` side effects,
  - or another later ownership mismatch that converts the launcher-owned arg2 area into control flow.

## New clarification from widening arg5 to the full recovered 13-slot primary vtable surface

A new static/code reconstruction pass widened the diagnostic arg5 scaffold from the earlier partial primary-vtable coverage to the full currently recovered 13-slot table at `0x4b2768`.

Recovered primary table entries from `launcher.exe` / canonical vtable table:

- slot 0 -> `0x4319a0`
- slot 1 -> `0x431ce0`
- slot 2 -> `0x4325d0`
- slot 3 -> `0x436000`
- slot 4 -> `0x42f7c0`
- slot 5 -> `0x431840`
- slot 6 -> `0x4328a0`
- slot 7 -> `0x42f970`
- slot 8 -> `0x42fbd0`
- slot 9 -> `0x42fd10`
- slot 10 -> `0x443810`
- slot 11 -> `0x431670`
- slot 12 -> `0x4316a0`

Newer string-backed naming now tightens several of those slots substantially:
- slot 1 / `0x431ce0` = **`MonitorPort`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::MonitorPort: Successfully monitored ...`
    - `... port is already monitored`
- slot 2 / `0x4325d0` = **`UDPMonitorPort`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::UDPMonitorPort: Successfully monitored ...`
    - `... Failed to monitor port ...`
  - current best argument read:
    - arg1 = port
    - arg2 = connection object pointer
    - arg3 = IPv4 bind address / network-order `sin_addr.s_addr`
    - i.e. this arg3 is not currently evidenced as a generic owner-context pointer
- slot 3 / `0x436000` = **provisional UDP-monitor helper / local-port query wrapper**
  - no direct surviving string name recovered yet
  - current static behavior:
    - calls slot `2` / `UDPMonitorPort` with `port = 0`, forwarding the same connection pointer and IPv4 bind-address arg
    - and on one branch then queries `getsockname` / `ntohs` to hand a bound local port back to the caller
  - this is still lower confidence than the string-backed names above, but it is no longer just an anonymous opaque callback
- slot 6 / `0x4328a0` = **`Connect`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::Connect: ...`
  - current source lockstep:
    - `CLTTCPConnection::Connect` now forwards the direct connection object here
    - `CLTThreadPerClientTCPEngine::Connect` now mirrors the same direct-object entry shape instead
      of detouring through a synthetic endpoint-repacking helper
- slot 7 / `0x42f970` = **`Close`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::Close: shutdown() failed ...`
    - `CLTThreadPerClientTCPEngine::Close: closesocket() failed ...`
  - caller shape is now tighter too:
    - `0x449ca0` pushes the direct connection object into this slot
    - the destructor loop at `0x43157e` does the same
    - current source therefore now mirrors `0x42f970` as a direct connection-object consumer rather
      than resolving a synthetic owner/context record through engine scaffolds
- slot 8 / `0x42fbd0` = **`SendBuffer`**
  - proven by in-function log string:
    - `CLTThreadPerClientTCPEngine::SendBuffer: Send failed ...`
  - caller shape is now tighter too:
    - `0x449d20` pushes `(buffer, byteCount, connection, completionContext)` into this slot
    - current source therefore now mirrors `0x42fbd0` as a direct connection-object consumer rather
      than resolving a synthetic owner/context record through engine scaffolds
- slot 12 / `0x4316a0` = **`CleanupConnection`**
  - proven by in-function log string:
    - `CLTThreadPerClientTCPEngine::CleanupConnection: Couldn't find socket ...`

Slot `5` still does not have an equally direct surviving string label in the recovered pass, but its concrete endpoint-keyed behavior is now much narrower than an anonymous unknown callback.

The next cluster of names also now has a useful state-backed interpretation:
- slot `7` / `Close` checks `[conn+0x34]` and only proceeds for state `1` or `2`
  - then it drives `shutdown` / `closesocket` cleanup on the connection object
- slot `8` / `SendBuffer` also only accepts connection state `1` or `2`
  - otherwise it logs the original `SendBuffer: ... not connected/connecting` failure string
- slot `2` / `UDPMonitorPort` success marks worker payload state with `[worker+0x34] = 2`
- slot `6` / `Connect` success marks worker payload state with `[worker+0x34] = 1`

Current best reading:
- worker/connection state `1` and `2` are the launcher's active network states for the paths now recovered here
- `Close` and `SendBuffer` are explicitly gated on those active states
- and the slot-2 / slot-6 success paths are the current best-evidenced writers of those state values on the worker side

What is now modeled in the scaffold:

- slots `0..12` are all now present with matching stack cleanup shapes
- slot `10` now matches the original tiny stub exactly in effect (`xor al,al ; ret 4` -> returns `0`)
- slot `5` now models the one high-confidence semantic path already recovered from static analysis:
  - when the `+0x80` list is still empty, it zeroes the caller out-pointer and returns `0x7000004`
  - and when the `+0x80` sidecar is non-empty, the current starter path now routes into `CLTThreadPerClientTCPEngine::UnmonitorPort(...)` instead of returning a generic neutral value
- slot `12` now at least distinguishes the proven empty-`+0x8c` fast path from the still-unreconstructed non-empty teardown path
- slots `6..9` are still only partially reconstructed; slot `6/7/8` now route into starter liblttcp methods, while slot `9` remains a logging placeholder

Why slot `5` matters:

Static disassembly of `0x431840` shows a concrete empty-container behavior rather than a generic opaque callback:

- it searches the `+0x80` intrusive/list container
- if the search misses the container head (`eax == [esi]` after `self += 0x80`), it writes `0` to the caller output pointer
- and returns `0x7000004`

That means the current implementation can now reproduce one real launcher-observed miss result instead of only returning a generic neutral placeholder there.

### New clarification: `+0x80` / `+0x8c` are sentinel-headed tree/list containers, not simple counted lists

A follow-up static pass tightened the container interpretation further.
The earlier scaffold treated the allocated `0x24` / `0x18` heads somewhat like generic list heads with a count field.
The recovered code instead points to **sentinel node objects** with these high-confidence fields:

For the `+0x80` allocated `0x24` head:
- `+0x00` = flag/color byte
- `+0x04` = root node pointer (`NULL` in ctor)
- `+0x08` = first / sentinel-linked forward pointer (`self` in ctor)
- `+0x0c` = last / sentinel-linked backward pointer (`self` in ctor)
- `+0x10` = key area used by the comparator path
- `+0x20` = payload/object pointer used by `0x431840`

For the `+0x8c` allocated `0x18` head:
- `+0x00` = flag/color byte
- `+0x04` = root node pointer (`NULL` in ctor)
- `+0x08` = first / sentinel-linked forward pointer (`self` in ctor)
- `+0x0c` = last / sentinel-linked backward pointer (`self` in ctor)
- `+0x10` = dword key used by `0x42fe10`
- `+0x14` = payload/object pointer used by `0x4316a0`

Evidence:
- ctor `0x431c30` sets the allocated heads as:
  - `[head+0x04] = 0`
  - `[head+0x08] = head`
  - `[head+0x0c] = head`
- search helper `0x42fdb0` starts from `[head+0x04]` and compares keys at `node+0x10`
- search helper `0x42fe10` also starts from `[head+0x04]` and compares the requested dword key against `node+0x10`
- slot `5` then consumes payload from `[node+0x20]`
- slot `12` then consumes payload from `[node+0x14]`

So the scaffold has now been corrected to treat emptiness as:
- `root == NULL`
- and sentinel forward/back links still pointing to the head itself

rather than as a guessed count-based condition.

### New clarification: `+0x80` and `+0x8c` are not the same kind of key space

A newer static pass tightens the semantic split between the two containers.

Recovered helper meaning:
- `0x44b070 = LTTCPEndpointKey_ctor`
  - zeroes the full 16-byte key
  - then writes `family = AF_INET`
- `0x44b020 = LTTCPEndpointKey_DiffersFrom`
  - compares the full 16-byte key as four dwords (`repe cmpsd`)
  - returns true when any dword differs
- `0x44aff0 = LTTCPEndpointKey_Copy`
  - copies the full 16-byte key as four dwords
- `0x44b090` builds a `sockaddr_in`-shaped 16-byte key from IPv4 + port
- `0x44b040` compares two such keys by:
  - port at `+0x02`
  - then address at `+0x04`
- `0x42fdb0` uses that comparator over the `+0x80` tree
- `0x42fe10` is a simpler dword-key tree walk over the `+0x8c` tree

That means the two arg5 containers are now best read as:
- `+0x80` = endpoint-keyed container (network address / port)
- `+0x8c` = pointer-keyed container (dword context/owner key)

A newer naming pass also makes the payload families on those containers more concrete.
Current best read:
- `+0x80` stores endpoint nodes whose payload at `[node+0x20]` is an **AcceptThread-style worker object**
  - slot `1` / `MonitorPort` allocates a `0x44` object via `0x431ab0`
  - that ctor is string-backed by `CLTThreadPerClientTCPEngine::AcceptThread`
  - the derived thread-main slot for that object is now named `0x432070 = CLTThreadPerClientTCPEngine_AcceptThread_Run`
  - `MonitorPort` creates `socket(AF_INET, SOCK_STREAM, 0)`, then `bind`, then `listen`
  - more exact insert sequencing from `0x4318f0 / 0x431240 / 0x431200`:
    - it first inserts the endpoint-keyed node with `[node+0x20] = 0`
    - on success it then stores the new AcceptThread-style payload into `[node+0x20]`
    - bind/listen failure erases that just-inserted node again
- `+0x8c` stores pointer-keyed nodes whose payload at `[node+0x14]` is a **WorkerThread-style worker object**
  - helper `0x431ff0` allocates a `0x48` object via `0x431b60`
  - that ctor is string-backed by `CLTThreadPerClientTCPEngine::WorkerThread`
  - the derived thread-main slot for that object is now named `0x42fe50 = CLTThreadPerClientTCPEngine_WorkerThread_Run`
  - helper `0x431ff0` stores that new worker pointer back through the connection object at `[connection+0x08]`
  - helper `0x431ff0` then inserts `(connection, workerPayload)` into `+0x8c` under helper `+0x98` lock
  - slot `2` / `UDPMonitorPort` uses that helper after successful UDP socket/bind setup, then marks the connection object state with `[connection+0x34] = 2`, and starts the returned worker with priority `2`
  - slot `6` / `Connect` uses that helper after successful TCP setup/connect sequencing, then marks the connection object state with `[connection+0x34] = 1`, and starts the returned worker with priority `3`

### New clarification: client.dll touches the inline QueuePair subobject directly

Latest source/runtime pruning now treats the queue family differently from the surrounding raw arg5
shell fields.

Current best read:
- client.dll can read/write the inline completed-operation `QueuePair` subobject at arg5
  `+0x0c..+0x5b` directly
- current MinGW/MSVC2003 bridge therefore keeps that queue storage native on the live
  `CLTThreadPerClientTCPEngine_0x4b2768` object rather than copying it into/out of a separate
  shell mirror
- the arg5 ABI wrapper still republishes only the non-queue direct-read field surface that later
  raw shell readers may observe between virtual calls:
  - `+0x04` / `+0x08`
  - `+0x7c`
  - `+0x80` / `+0x84`
  - `+0x8c` / `+0x90`
- practical source consequence:
  - old queue attach/detach copy helpers were pruned
  - on the current native-object build, detached-shell field publication is skipped, but the
    helper still refreshes engine-owned endpoint/context count mirrors and empty-head
    normalization so runtime state stays closer to the original insert/remove helpers
  - shell `queuePair0C` bytes should now be treated as ABI/layout coverage only for fallback
    detached-shell builds unless a future compiler-port proves direct cross-module subobject access
    can no longer remain native

### New clarification: slot `5` is an endpoint-removal / handle-extraction path

Static disassembly of `0x431840` now supports a stronger reading than only “empty-path returns `0x7000004`.”

Recovered behavior:
- builds a `sockaddr_in`-style lookup key through `0x44b090`
- searches arg5 `+0x80` through `0x42fdb0`
- on miss:
  - writes `0` to caller out-pointer
  - returns `0x7000004`
- on hit:
  - removes the matched node from the `+0x80` container
  - decrements the container count
  - loads payload object from `[node+0x20]`
  - writes `[payload+0x38]` to the caller out-pointer
    - current best read of that out-value is the `AcceptThread` owner/context field, **not** the
      listening socket handle
  - calls cleanup helpers on the payload state including:
    - `0x452320` on `payload+0x40`
    - payload virtual `+0x14`
    - `closesocket([payload+0x3c])`
    - payload virtual `+0x2c(1)`
- then returns `0`

Because slot `1` / `MonitorPort` is now known to populate `+0x80` with `AcceptThread`-style worker payloads, slot `5` can now be read more specifically as the **endpoint-keyed unmonitor / stop-monitoring counterpart** to `MonitorPort`, even though the exact exported/public method name is still not directly string-labeled in the recovered pass.

Current best interpretation:
- slot `5` is an **endpoint-keyed unmonitor / teardown / handle-extraction** method in the network-engine family, not a generic opaque callback

### New clarification: slot `12` is part of the non-empty queue-dispatch continuation

Static disassembly of `0x4316a0` now also explains the later `slot 12` milestone more concretely.

Recovered behavior:
- acquires arg5 helper `+0x98`
- searches arg5 `+0x8c` using the raw dword argument as key via `0x42fe10`
- on hit:
  - loads payload object from `[node+0x14]`
  - sets `[payload+0x44] = 1`
  - calls `0x452320(payload+0x40)`
  - calls payload virtual `+0x14`
  - calls payload virtual `+0x2c(1)` if non-NULL
  - removes the matched node from `+0x8c`
  - decrements the container count
  - calls `0x44ab60(arg)` before releasing the helper lock
- on miss:
  - logs the miss path
  - still calls `0x44ab60(arg)` before releasing the helper lock

That matters because launcher consumer `0x436d31..0x436ee7` now reads more concretely too.
On the non-empty dequeue branch it:
- pops one queued pair:
  - first dword = `workItem`
  - second dword = `context`
- logs/debug-prints using `0x4816f0(workItem)`, which simply returns `[workItem+0x04]`
- when `context` is non-NULL, reads the same work type again and only on the **type-1** path calls
  arg5 primary slot `12` with the dequeued `context`
- then calls `context->+0x10(workItem)`
- on that same type-1 path, conditionally calls `context->+0x04()` when the low byte of
  `context[1]` is non-zero
  - current best connection-layout read ties that low byte to base connection field `+0x04`, which
    `0x44a9f0` explicitly zeroes during construction
- and only after that runs the final `workItem->+0x04()` release

That ties several earlier partial observations together:
- the queued first-dword object really is a **work/status item** rather than only arbitrary pointer noise
- the queued second-dword object is a real paired **context/owner** pointer
- and arg5 primary slot `12` is part of the **non-empty queue dispatch / teardown / state-transition path**, not just an arbitrary later method

Practical status of this update:

- the widened arg5 vtable scaffold built successfully
- a follow-up rerun with the usual active path
  - `cd /home/morgan/mxo/code/matrix_launcher && make run`
  still showed **no** new arg5 logs before failure from:
  - primary vtable slots `5..10`
  - primary vtable slots `11..12`
  - helper slots at `+0x5c / +0x60 / +0x98`
- representative latest dump after that rerun:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_54.dmp`
  - `EIP=0x003e5e4a`
  - current `arg2 filteredArgv = 0x003e5e48`
- so this wider primary-vtable coverage still did **not** move the launcher past the current late `arg2+2` crash family

That makes this update useful faithfulness groundwork for later differential runs, but still **not** a demonstrated fix.

## Historical validation result from the in-launcher `ret` bypass

To test whether the current late `arg2` crash was merely blocking later arg5 traffic from becoming visible, an earlier in-launcher diagnostic `ret` bypass path was tried.

That path simulated a single x86 `ret` when a fault landed inside current `arg2 filteredArgv` storage.

Representative earlier validation run family:
- active launcher path with the temporary `ret` bypass enabled

Observed result:
- the first fault still lands at current `arg2+2`
  - `EIP=0x003e5e82`
  - current `arg2 filteredArgv = 0x003e5e80`
- the bypass fires once and pops the current stack top as though executing `ret`
- the popped target is:
  - `0x62000000`
  - which is just current `arg3 hClientDll`, not a meaningful later client continuation
- execution then immediately faults again at:
  - `client.dll+0x3`
  - representative dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_57.dmp`
  - `EIP=0x62000003`
- the stack at that second fault still starts with stale startup-frame values:
  - `0x003e71c8` (arg5)
  - `0x0041bb60` (arg6)
- and `resurrections.log` still shows no new observed arg5 traffic before that second crash from:
  - primary slots `5..10`
  - primary slots `11..12`
  - helper slots `+0x5c / +0x60 / +0x98`

Interpretation:
- this finally gives a useful **runtime validation result**, but it is a negative one
- the temporary `ret` bypass does **not** reveal later arg5 method traffic on the present path
- instead, it shows that the corrupted return chain is currently collapsing into stale `InitClientDLL` startup-frame values
- so the current arg5 reconstruction remains only partially runtime-validated:
  - object creation / registration / passing are live
  - deeper arg5 method behavior is still not observed before the current corruption wins

## New runtime validation after clean `InitClientDLL = 1` and deliberate `RunClientDLL`

A newer active-path rerun changed that runtime picture materially.

Command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && make run
```

What is now statically confirmed first:
- original launcher helper `0x40a4d0` treats positive export returns as success on this path:
  - after `InitClientDLL`: `0x40a5a9..0x40a5ab` -> `test eax,eax ; jg ...`
  - after `RunClientDLL`: `0x40a622..0x40a624` -> `test eax,eax ; jg ...`
  - after `TermClientDLL`: `0x40a6bc..0x40a6be` -> `test eax,eax ; jg ...`
  - overall success return: `0x40a6fd` -> `al = 1`
- so the current clean `InitClientDLL returned: 1` binder result is now evidence-backed success, not only a local launcher heuristic

What the active runtime path then shows:
- `RunClientDLL` no longer immediately reproduces the old forced-runtime crash at `client.dll+0x3b3573`
- no fresh crash dump was produced during timed runs
- window tracing shows a real `MATRIX_ONLINE` window appears and then transitions to fullscreen `800x600`
- the stable runtime loop now repeatedly hits:
  - mediator `+0x2c`
  - arg5 helper `+0x60` slot `0`
  - arg5 helper `+0x60` slot `1`

Representative in-log evidence:
- `WindowTrace hwnd=00030058 visible=1 iconic=0 class='MATRIX_ONLINE' title='The Matrix Online' ... rect=(200,150)-(600,450)`
- later on the same run:
  - `WindowTrace hwnd=00030058 visible=1 iconic=0 class='MATRIX_ONLINE' title='The Matrix Online' ... rect=(0,0)-(800,600)`
- repeated runtime traffic:
  - `MediatorStub::IsConnected() -> 1`
  - `LauncherObjectStub::Subobject60::Slot0(...)`
  - `LauncherObjectStub::Subobject60::Slot1(...)`

Static explanation of that loop:
- `RunClientDLL` export `0x62001180` just calls `0x62006c30`
- inside `0x62006cb1..0x62006cca`, the client:
  - calls mediator `+0x2c`
  - tests `al`
  - loads stored `InitClientDLL` arg5 from `0x62b073e4`
  - and calls `0x62532130`
- inside `0x62531c20..0x62532053`, the arg5-driven helper:
  - calls arg5 subobject `+0x60` slot `0`
  - compares queue cursor state at `+0x1c` vs `+0x0c`
  - compares queue cursor state at `+0x44` vs `+0x34`
  - then releases arg5 subobject `+0x60` through slot `1`

With the current recovered queue layout, those compared fields now map more precisely as:
- `+0x0c` = queue0C `current0`
- `+0x1c` = queue0C `current1`
- `+0x34` = queue34 `current0`
- `+0x44` = queue34 `current1`

A newer static comparison now tightens the meaning of that client runtime path.
The client helper `client.dll:0x62531c10` is structurally the same consumer-family logic as original `launcher.exe:0x436b10`:
- both acquire arg5 helper `+0x60`
- both compare queue0C `current1` vs `current0`
- both compare queue34 `current1` vs `current0`
- both use arg5 helper `+0x5c` as the event/wait helper when no work is present
- both release arg5 helper `+0x60` before returning

That comparison also explains an important runtime detail on the current client path:
- `client.dll:0x62532130` calls `0x62531c10(1)`
- so `RunClientDLL` is currently driving the **non-blocking poll variant** of this shared arg5 queue-consumer logic

A newer throttled runtime log on the same active path now shows that state staying unchanged through repeated polling:
- queue0C: `current0 == current1 == block0 == block1`
- queue34: `current0 == current1 == block0 == block1`
- representative sampled counts still showing that exact state: `1`, `2`, `4`, `8`, `16`, `32`, `64`, `128`, `256`, `512`, `1024`

A newer original-launcher static pass now also identifies the corresponding producer side more concretely.
Original enqueue helper `launcher.exe:0x436820`:
- exact public argument order after engine `this` is:
  - `workItem`
  - `context`
  - `queueSelect`
- acquires arg5 helper `+0x60`
- snapshots whether both queues were empty before enqueue
- calls `0x436670(workItem, context, queueSelect)` to push an **8-byte pair** into one of the two queues
  - `0x436670` itself is also `void`
  - so this producer family does **not** surface enqueue-success feedback back to its callers
- `queueSelect = 0` uses queue0C
- `queueSelect != 0` uses queue34
- releases arg5 helper `+0x60`
- and if both queues were previously empty, signals arg5 helper `+0x5c` slot `0`
- the signal decision is therefore keyed to the pre-push queue-pair emptiness snapshot, not to any caller-visible push-success result

Representative original xrefs to that producer helper now identified statically include:
- `launcher.exe:0x4302d5`
- `launcher.exe:0x4325aa`
- `launcher.exe:0x4329cc`
- `launcher.exe:0x449d8a`

A newer pass over those producer xrefs tightens the currently evidenced traffic shape further.
In the identified callsites so far, the third argument passed to `0x436820` is always `0`, which means the current concrete startup/runtime producer evidence is specifically for **queue0C** rather than queue34.
That does **not** prove queue34 is unused in the wider program.
It only means the currently recovered producer xrefs feeding arg5 during this startup/runtime family all target queue0C.

Those same xrefs also narrow the queued pair shape:
- first dword = a freshly allocated small work-item-like object in many paths
  - representative constructors / shapes seen at current producer xrefs:
    - `0x435090` on a `0x2c` allocation
    - `0x435010` on a `0x20` allocation
    - `0x435050` on a `0x0c` allocation with immediate payload value such as `0x7000001`
- second dword = a stable owner/context pointer or associated object
  - representative sources seen so far:
    - `[esi+0x38]`
    - `edi`

## Remaining concrete `0x436820` producer xrefs read so far

The currently read xrefs now fall into several concrete families.
This is still not a full semantic decode of each work item, but it is enough to tighten the queue/work picture beyond a generic “producer exists” statement.

### Work-item constructor families now concretely seen

The queued first-dword object families currently evidenced are:
- `0x435db0 -> 0x435090`
  - allocation size `0x2c`
  - constructor sets vtable `0x4b3e08`
  - later producer example also sets `[obj+0x08] = 1` through `0x4444e0`
- `0x435d80 -> 0x435010`
  - allocation size `0x20`
  - constructor sets vtable `0x4b3df0`
- `0x435d90 -> 0x435050`
  - allocation size `0x0c`
  - constructor sets vtable `0x4b3df8`
  - seeds `[obj+0x04] = 2`
  - stores the connect-status / payload dword in `[obj+0x08]`
  - current best family name: **type-2 connection-status work item**
- `0x435da0 -> 0x435070`
  - allocation size `0x0c`
  - constructor sets vtable `0x4b3e00`
  - seeds `[obj+0x04] = 1`, `[obj+0x08] = 0`
  - current best family name: **type-1 close work item**

Those constructors all belong to the same nearby vtable family around `0x4b3df0..0x4b3e10`, which is strong evidence that the queue is carrying a small launcher-defined work-item class family rather than arbitrary raw integers.

### Xref taxonomy

#### `launcher.exe:0x4302d5`
- allocates `0x2c` via `0x435db0`
- constructs via `0x435090`
- sets `[work+0x08] = 1` through `0x4444e0`
- enqueues `(work=ebx, context=[esi+0x38], queueSelect=0)`
- newer import-backed review tightens this substantially beyond generic “I/O-adjacent” wording:
  - the same surrounding function later calls `recvfrom` (`WS2_32!recvfrom` via `0x4a9840`) on a `0x1000` buffer
  - so this producer is now best read as part of a **UDP receive / packet-available** path rather than a generic launcher task submission

#### `launcher.exe:0x43051f`
- does **not** allocate a fresh queued object on this branch
- sets `[context+0x34] = 2` first
- then enqueues `(work=edi, context=[esi+0x38], queueSelect=0)`

#### `launcher.exe:0x43067f` and `0x4306a7`
- same function, two producer calls
- first enqueues `(work=edi, context=[esi+0x38], queueSelect=0)`
- then allocates `0x0c` via `0x435da0`, constructs via `0x435070`, and enqueues `(work=eax, context=[esi+0x38], queueSelect=0)`
- so this path clearly shows **back-to-back queue0C submissions** of two different work-item shapes

#### `launcher.exe:0x4309da` and `0x4309ef`
- same function, two producer calls
- first fills `edi+0x0c .. +0x18` with four dwords and then enqueues `(work=edi, context=[esi+0x38], queueSelect=0)`
- second immediately enqueues `(work=0, context=0, queueSelect=0)`
- current best reading is that this is a deliberate paired submission pattern, but its exact sentinel/flush semantics are still unresolved

#### `launcher.exe:0x430c25`
- allocates `0x0c` via `0x435da0`
- constructs via `0x435070`
- enqueues `(work=eax-or-NULL, context=[esi+0x38], queueSelect=0)`
- this is one of the clearest “simple queue0C command object” producer cases

#### `launcher.exe:0x430d71`, `0x430d94`, and `0x430da8`
- same function, up to three producer calls
- first enqueues `(work=edi, context=[esi+0x38], queueSelect=0)`
- then either:
  - allocates `0x0c` via `0x435da0`, constructs via `0x435070`, and enqueues that new work object with the same context, or
  - falls back to `(work=0, context=[esi+0x38], queueSelect=0)`
- like `0x43067f/0x4306a7`, this is another concrete multi-submit queue0C path

#### `launcher.exe:0x4315b0`
- allocates `0x0c` via `0x435da0`
- constructs via `0x435070`
- enqueues `(work=eax-or-NULL, context=[ebp-0x08], queueSelect=0)`
- the same function later calls `0x436920` and then tears down queue-related state, so this producer path currently looks more teardown-oriented than the others

#### `launcher.exe:0x4325aa`
- allocates `0x20` via `0x435d80`
- constructs via `0x435010`
- enqueues `(work=eax, context=edi, queueSelect=0)`
- newer naming/static review now ties this more concretely to original arg5 slot `2` / `UDPMonitorPort`
- because `UDPMonitorPort` now also clearly creates/inserts a `WorkerThread` payload into arg5 `+0x8c` and marks it with `[worker+0x34] = 2`, this producer now looks like a **UDP monitor-port setup completion / worker-start submission** rather than a generic socket-facing task

#### `launcher.exe:0x4329cc`
- allocates `0x0c` via `0x435d90`
- constructs via `0x435050(0x7000001)`
- enqueues `(work=eax, context=edi, queueSelect=0)`
- this is the clearest currently read path proving that some queue items carry an immediate status/code payload rather than only pointer state
- newer import-backed review also tightens the enclosing function meaning:
  - enclosing method is original arg5 slot `6` / `Connect`
  - `0x4328a0` creates a socket through helper `0x449b40(1, 6, 0)`
  - `0x449b40` wraps `WS2_32!socket(AF_INET, type, protocol)` and option setup
    - current decompile now makes that helper more concrete:
      - disables Nagle for TCP stream sockets unless caller opts out
      - sets non-blocking mode unless caller opts out
  - later in the same `0x4328a0` method the launcher calls `WS2_32!connect`
    - current decompile also shows the connect-success branch covers both:
      - `connect(...) == 0`
      - `connect(...) == SOCKET_ERROR` with `WSAGetLastError() == 0x2733` / `WSAEWOULDBLOCK`
    - immediate non-`WSAEWOULDBLOCK` failure instead closes the socket, queues a type-1 close work item, then queues a type-2 status work item with payload `1`
  - successful connect then creates/inserts a `WorkerThread` payload into arg5 `+0x8c` and marks it with `[worker+0x34] = 1`
  - the later connect-status item should therefore be treated as an **async completion from the
    worker/connect loop**, not as a mediator-side immediate success alias
  - current source now also moved closer to that read:
    - connect no longer fabricates an immediate launcher-bridge success work item from the mediator
      begin wrapper
    - `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp` now keeps the primary
      `0x4328a0` body on the direct connection object pointer instead of re-resolving a synthetic
      context key first
    - the same source pass also replaced several generic connect-side scaffolds with nearer
      original-name bodies:
      - `CLTIPSocket_StaticAllocateSocket` for `0x449b40`
      - `CLTThreadPerClientTCPEngine_CreateAndInsertWorkerThread` for `0x431ff0`
      - explicit `0x435d90/0x435050` and `0x435da0/0x435070` work-item allocation/ctor sequences
        on the connect failure paths
    - allocation-failure queue submissions on this path now also follow the original nearer read by
      enqueuing a `NULL` work-item sentinel instead of silently dropping the queue event
    - the worker-thread slot `0x42fe50` now owns the nearer blocking select/connect-completion path
      instead of a mediator-side synthetic completion
      - read set = connection socket + wakeup socket
      - except set = connection socket
      - write set = connection socket while connect completion is pending, and later again while a
        queued send buffer is being drained
      - current source now mirrors the original post-select order as:
        1. except/connect-failure handling
        2. readable-socket recv drain
        3. writable-socket connect-success or send-progress handling
        4. wakeup-socket drain / exit-by-request
      - successful connect completion now queues type-2 status `0`
  - so `0x4329cc` is now best read as part of a **TCP connect / connect-status / worker-start** producer path

#### `launcher.exe:0x432d86`, `0x432dc1`, and `0x432dd7`
- same function, several queue0C variants
- one branch enqueues `(work=eax-or-NULL, context=edi, queueSelect=0)` where work may come from a `0x0c` / `0x4b3e00` object family
- another branch allocates `0x0c` via `0x435d90`, constructs via `0x435050(1)`, and enqueues that coded object with `context=edi`
- allocation failure path falls back to `(work=0, context=edi, queueSelect=0)`
- because these branches sit in the same `0x4328a0` socket/connect method, they are now best treated as **TCP connect follow-up / completion-status queue submissions**, not just anonymous queue variants

#### `launcher.exe:0x449d8a`
- sits inside the `CLTTCPConnection::OnReceive` loop
- exact recovered producer shape is now narrower than the older “poll helper” reading:
  - first pass:
    - `readOperationFragment->+0x04()`
    - `parser = [edi+0x6c]`
    - `parseResult = parser->Parse(readOperationFragment, &completedPacketWorkItem)`
  - while `parseResult == 0`:
    - enqueue `(work=completedPacketWorkItem, context=edi, queueSelect=0)` through `0x436820`
      - the machine-code call sequence is `push 0 ; push edi ; push edx ; call 0x436820`
      - i.e. exact enqueue argument order after engine `this` is `(workItem, connection, queueSelect)`
    - call `parser->Parse(0, &completedPacketWorkItem)` to drain more completed packets from the
      same buffered stream state
- current best reading is therefore a **parser-drain queue0C producer**, not a one-shot fire-and-forget submission and not an anonymous socket poll stub
- newer parser read from `0x469bf0` also tightens lifetime / nullability on this path:
  - on the success case `Parse(...) == 0`, the parser writes `*outCompletedPacketWorkItem = parser+0x14`
    before `ResetAfterPacket` allocates the next assembly object
  - so current RE does **not** support an intentional `parseResult == 0` / `completedPacketWorkItem == NULL`
    result on this receive path
  - the queue consumer's `workItem == NULL` shutdown sentinel still exists, but current evidence ties
    that to later lifecycle / teardown producers such as:
    - `0x436a0e`
    - `0x436fa8`
    - `0x4309ef`
    - allocation-failure fallbacks in `0x432d86..0x432dd7`
- after that drain loop, this function checks return/status values in the `0x700000x` family (`0x7000000`, `0x700000b`)
  - that makes the coded `0x435050(payload)` queue objects from `0x4329cc` / `0x432dc1` look even more like **network status/result items** rather than generic integers

#### Internal self-calls: `launcher.exe:0x436a0e` and `0x436fa8`
- both call `0x436820(this, 0, 0, 0)` from inside the queue/engine family itself
- these are **not** external launcher feature producers like the others
- they appear in internal lifecycle / drain / teardown-style paths (`0x436920`, `0x436fd0`) and should be treated separately from the externally interesting producer xrefs above

### New high-confidence interpretation of queue0C producer meaning

The current concrete xref set now supports a stronger claim than “some launcher work queue exists.”
Queue0C now looks specifically like a **network-engine async work / status queue**.

Evidence supporting that tighter read:
- `0x4302d5` sits in a function that later calls `WS2_32!recvfrom`, so that producer is best read as a receive-side / packet-side event submission
- helper `0x449b40` now resolves cleanly as a socket factory around `WS2_32!socket(AF_INET, type, protocol)` plus option setup
- `0x4328a0` (arg5 primary vtable slot `6`) calls `0x449b40(1, 6, 0)` and later `WS2_32!connect`, so its queue submissions are part of a **TCP connect / connect-result** family
- `0x4325d0` (arg5 primary vtable slot `2`) calls `0x449b40(2, 0x11, 0)`, then `WS2_32!setsockopt(..., SOL_SOCKET, SO_REUSEADDR, ...)`, then `WS2_32!bind`, so that method is best read as a **UDP bind/setup** path inside the same engine family
- coded `0x435050(payload)` objects carry literals like `0x7000001`
- later wait/submit logic at `0x449d40` checks return/status values in that same `0x700000x` family (`0x7000000`, `0x700000b`)

Current best reading from that combination:
- queue0C is not a generic launcher job queue
- it is a launcher-owned **network-engine event/work channel** carrying packet-side events, connect/setup commands, and coded network-status results between socket-facing producer code and the later shared consumer logic

A further client/launcher comparison now explains the next branch after dequeue more concretely.
When work is present, launcher consumer `0x436d31..0x436ee7` and client consumer `0x62531e31..0x62531fe7` both:
- dequeue one 8-byte pair
- treat the first dword as a queued work-item object
- treat the second dword as a paired context/owner object
- read the work-item type from `[workItem+0x04]`
- and only on the **type-1** path call arg5 primary vtable offset `+0x30` before the later
  context callback / release tail

On the client side that is:

```asm
62531fb8: mov edx, [esi]
62531fba: push edi
62531fbb: mov ecx, esi
62531fbd: call [edx+0x30]   ; arg5 primary slot 12
```

That means the current absence of arg5 slot-12 runtime traffic on deliberate `RunClientDLL` runs is now explained more narrowly than before:
- not because slot 12 is irrelevant,
- but because the current implementation never feeds the queue branch that would reach it.

Current best interpretation:
- arg5 is now runtime-validated more concretely than before
- the helper/lock surface at `+0x60` is definitely live on the `RunClientDLL` path
- the queue cursor fields around `+0x0c/+0x1c` and `+0x34/+0x44` are also definitely live on that path
- `RunClientDLL` is repeatedly exercising the **consumer** side of this engine in non-blocking poll mode
- the original launcher producer side now has concrete xrefs and concrete queued pair shapes
- and if queue0C were actually being fed with the recovered type-1 close/terminal items, the next
  observable arg5 step would likely be primary slot `12` (`+0x30`)
- but on the current implementation that producer side still does not appear to be feeding even the now-best-understood queue0C path, so both queues remain in a stable **empty cursor** state rather than advancing
- so the next arg5 problem is no longer just “which missing slot causes the old late crash?”
- it is now more specifically “which missing launcher-owned state should populate or advance this arg5-owned runtime work path beyond the current empty-loop behavior?”

### New clarification: the queued `context` is now likely a `CMessageConnection`-family object

A newer pass over the worker/connection side tightens the meaning of the dequeued second dword.

High-value evidence:
- vtable at `0x4b7928` is followed by strings for:
  - `CMessageConnection::SendPacket()`
  - `CMessageConnection::OnOperationCompleted()`
- constructor path around `0x448b40` installs that vtable and stores an engine pointer at `+0x10`
- methods on that same class then call back into the engine through that stored pointer, including:
  - engine `+0x18`
  - engine `+0x1c`
  - engine `+0x20`
  - and the queue producer helper `0x436820`
- notably, `0x449d40` uses object field `+0x6c` as the framing parser pointer and repeatedly enqueues `(work, self, 0)` through `0x436820` while parser `Parse` keeps returning `0`

Current best virtual-method mapping on that class is now:
- vtable `+0x10` / `0x4490c0` = likely **`OnOperationCompleted(workItem)`**
  - dispatches on `workItem->[+0x04]`
  - contains string-backed receive/completion/error handling paths
- send bridge on this class family now tightens as:
  - vtable `+0x24` = `0x41cf30 = CMessageConnection_ForwardEnvelopeToSendPacket`
    - wrapper used by mediator-side send helper `0x41af70`
    - extracts envelope `+0x08`, i.e. the retained outer live message-ref object from the
      stack-local packet builder
  - vtable `+0x28` = `0x448cf0 = CMessageConnection::SendPacket`
    - consumes that outer message-ref object rather than raw bytes
    - performs packet-agenda filtering first
    - headerless/send-mode branch there also conditionally mutates inner payload storage around
      `+0x12/+0x16/+0x17` and later clears the first payload byte high bit at inner `+0x0c`
    - then reaches lower submit helper `0x448a00`, which forwards final byte pointer/size into
      engine `+0x20`
    - newer source tightening now mirrors that one step closer too:
      - agenda write handoff stays pointer-first through agenda `+0x24`
      - final send bytes are taken directly from inner raw `+0x0a/+0x0b/+0x0c..` storage instead of
        rebuilding a temporary framed vector first
      - remaining source-owned tails are now sharper too:
        - helper-side agenda transformation/discard still pass through unchanged
        - exact refcount/release lifetime after `0x448a00` is still not modeled as the original
          pooled heap object family
  - which matches current arg5 slot `8` / `SendBuffer`
- vtable `+0x1c` / `0x449cd0` = likely endpoint-update / ensure-connected wrapper
  - copies a new endpoint into object `+0x24`
  - then calls engine `+0x18`
  - which matches current arg5 slot `6` / `Connect`
- vtable `+0x0c` / `0x449ca0` = likely close/abort wrapper
  - calls engine `+0x1c`
  - which matches current arg5 slot `7` / `Close`

Newer ctor/vtable-backed clarification now makes that class family more concrete than before:
- `0x448b40` first constructs a **base `CLTTCPConnection`-family object** and then overwrites its vtable to `0x4b7928`
- base vtable `0x4b8018` is now string-backed by nearby `CLTTCPConnection::OnReceive()` strings
- so current best reading is:
  - `CLTTCPConnection` = base connection/socket object family
  - `CMessageConnection` = derived message-oriented connection object layered on top of that base
- this also explains why the same object family carries:
  - engine pointer at `+0x10`
  - endpoint copy at `+0x24`
  - connection state at `+0x34`
  - parser pointer at `+0x6c`
- newer focused receive-path RE now narrows that `+0x6c` parser materially:
  - it is a `CVariableLengthPrefixedTCPStreamParser`-family object
  - ctor path: `0x469f50 -> 0x469b20`
  - primary receive call: `0x469bf0 = CVariableLengthPrefixedTCPStreamParser::Parse`
  - parser slot `+0x10` / `0x469b40` allocates the parser's current `CParsedPacketWorkItem` through
    `0x435db0 = CParsedPacketWorkItem_Allocate -> 0x435090 = CParsedPacketWorkItem_ctor`
    - i.e. the same `0x2c` / vtable-`0x4b3e08` work-item family already seen in queue producer analysis
  - that `0x4b3e08` object family is now best read as **both**:
    - parser-owned assembly state while bytes are still being accumulated, and
    - the emitted completed packet object when `Parse(...)` returns `0`
  - the concrete phase boundary is now static-RE-backed:
    - `Parse` appends retained read fragments into parser current work item `+0x14`
    - after prefix decode it stores packet-body byte count at work-item `+0x28`
    - just before emit it stores work-item `+0x24 = parser+0x08`
    - `ResetAfterPacket` then allocates a fresh replacement work item and, if unread bytes remain,
      carries the old tail fragment plus the new cursor into that replacement object
  - `CLTTCPConnection::OnReceive` (`0x449d40`) therefore feeds received stream fragments into a framing parser there, not an anonymous poll stub
  - the parser-emitted object queued by `0x449d40` is now best read as a fragment-backed parsed-packet work item with retained-fragment, cursor, and assembled-byte-count state, not only an opaque buffer pointer
  - newer focused pass also narrows the explicit `0x449d40` argument itself:
    - it is best read as a refcounted `CLTTCPReadOperation`-family buffer fragment consumed by the parser
    - worker-thread receive producer now gives that object a much tighter concrete shape:
      - allocation size `0x100c`
      - vtable `0x004b2300`
      - `+0x04 =` interlocked refcount
      - `+0x08 =` received byte count
      - `+0x0c =` first payload byte
    - worker-thread TCP receive subpath actually takes **two** refs before the callback:
      - one worker-owned outer ref immediately after allocation/setup
      - one delivery-temp ref immediately before the vtable `+0x14` / `OnReceive` call
    - source now also mirrors one more concrete worker-side detail there:
      - recv lands directly into fragment `+0x0c` instead of first copying through a
        connection-owned staging vector
    - the original same-poll recv-drain loop is still a separate follow-up target on the launcher
      bridge path
    - early `param_1->+0x04` inside `0x449d40` is now best read as another **no-arg AddRef only**
      on that fragment
      - the stack dwords prepared around that call remain in place for the immediately following
        `parser->Parse(param_1, &completedPacketWorkItem)` call
    - later `param_1->+0x08` inside `0x449d40` is now best read as the matching outer-reference
      Release hook
    - `Parse` itself also takes/releases its own transient fragment reference while moving any
      retained fragment ownership into the completed work item
    - `0x435e60 = CParsedPacketWorkItem_AppendFragment` then consumes one caller-held temp itself
      with trailing `Release(param_1)`
  - source lockstep update from the same focused pass:
    - `lttcpconnection.h` now carries explicit scaffold types for the `CLTTCPReadOperation`-family
      parser input fragment and the emitted `0x2c` packet work item
    - the parsed-packet scaffold there now also records the currently proven parser-owned fields:
      - retained fragment count / first retained fragment / additional-fragment list owner
      - traversal-only state at `+0x18/+0x1c/+0x20`
      - `+0x24` as a cursor pointer
      - `+0x28` as assembled packet-body byte count
    - source there now also carries a recovered parser-prefix scaffold for connection `+0x6c` with:
      - `+0x04` current-cursor fragment reference
      - `+0x08` next unread buffered byte pointer
      - `+0x0c` unread buffered byte count across retained fragments
      - `+0x10` provisional cursor-advance byte-count state
      - `+0x14` current parser-owned work item
    - `CLTTCPConnection::OnReceive` / `OnClose` now model the narrower AddRef / Parse / Release seam
      instead of treating the fragment virtual `+0x04` as a possible materialization helper
    - source comments now also record the wider original `OnClose` callback ABI proven by the UDP
      worker-thread caller:
      - `ret 0xc`
      - current concrete caller shape = `(readOperationFragment, peerAddressBlob16Ptr, 0x004b2118)`
      - recovered semantic effect still only releases the fragment
    - `CLTTCPConnection::EnqueueCompletedPacketWorkItemScaffold(...)` is now best read as the exact receive-side handoff
      `(completedPacketWorkItem, this, false)` into engine helper `0x436820`, not as a generic
      raw-pointer convenience wrapper
    - because original `0x436820` is `void`, current source ownership notes now also treat that seam
      as an unconditional transfer into the queue/consumer boundary rather than as a caller-visible
      success/failure API
- source lockstep update from the current focused pass:
  - `matrixstaging/runtime/src/liblttcp/lttcpconnection.*` now owns the corrected base-wrapper mapping directly
  - `matrixstaging/runtime/src/libltmessaging/messageconnection.*` no longer keeps a duplicate source-only engine pointer separate from the recovered base `+0x10` field
  - current source `CMessageConnection` wrapper calls now intentionally route through base `CLTTCPConnection` `Connect` / `Close` / `SendBuffer` semantics instead of open-coding a second parallel connection-engine bridge

That newer read also narrows the real engine-call signatures more than the earlier slot-name pass alone:
- `0x449cd0` does **not** build a raw `(ip, port, context)` call
  - it compares/copies the requested endpoint into `self+0x24`
  - then calls engine `+0x18` with **`self`**
  - so current best original read is that arg5 slot `6` / `Connect` is importantly a **connection-object-based** entrypoint
  - source lockstep update: `matrixstaging/runtime/src/liblttcp/lttcpconnection.cpp` now calls `engine_->Connect(this)` directly instead of detouring through a synthetic `ConnectConnectionScaffold(...)` adapter
- `0x449ca0` similarly forwards **`self`** into engine `+0x1c`
  - so arg5 slot `7` / `Close` is likewise connection-object-based on this path
- `0x449d20` forwards `(buffer, byteCount, self, completionContext)` into engine `+0x20`
  - so arg5 slot `8` / `SendBuffer` is also reached through the connection object, not as a free-standing raw socket helper alone
  - source lockstep update: the current C++ `ILTTCPEngine` slot-8 signature now uses that recovered
    argument order directly instead of detouring through a source-only helper with a mismatched
    `(contextKey, buffer, byteCount, completionContext)` shape

Current best reading from that combination:
- the queued second dword currently described as generic `context/owner` is now more specifically likely a **`CMessageConnection`-family object pointer** on at least important producer/consumer paths
- that object is no longer just a passive owner token:
  - it appears to be an active bridge back into engine `Connect` / `Close` / `SendBuffer` paths
- and the pointer-keyed `+0x8c` container now looks more concrete too:
  - helper `0x431ff0` inserts nodes there using the raw connection pointer as key and a `WorkerThread`-style payload as value
  - that same helper also writes the new worker pointer back through the connection-side object at `[connection+0x08]`
  - `UDPMonitorPort` and `Connect` then mark the connection object state `2` / `1` and start the worker with priorities `2` / `3`
- that also helps explain why the engine methods are hard to find through direct global `0x4d6304` xrefs alone:
  - meaningful calls are likely mediated through these connection objects after they capture the engine pointer, not only through raw global-engine direct calls

### New clarification: how the engine appears to be called from launcher startup so far

A newer direct-xref pass over global `0x4d6304` is useful mainly for what it does **not** show.
Current direct uses of the global slot still narrow to:
- construction and registration through mediator `+0x08`
- passing it into `InitClientDLL` as arg5
- embedding it into a local descriptor at `0x40ed7c`
- default-object fallback at `0x41b16d`
- teardown via slot `0` at `0x40b3ee`

What it does **not** yet show is an obvious simple launcher-mainline direct call to global `0x4d6304` slots like:
- `MonitorPort`
- `UDPMonitorPort`
- `Connect`

Current best reading from that absence:
- the interesting network-engine methods are probably not driven by a single easy-to-spot direct startup call on the raw global pointer alone
- they are more likely reached indirectly through engine-owned connection/worker/helper objects, or through later launcher subsystems after the engine object has already been registered and handed off
- that fits the current evidence better than assuming the remaining missing startup work is a single direct `g_4d6304->Connect(...)` call that we simply have not noticed yet

Newer startup-side owner-path review now makes that indirect reading substantially more concrete.
There is now a specific original launcher path where higher-level owner objects construct `CMessageConnection`-family instances and immediately drive their connect wrapper, rather than calling raw engine `Connect` on the global directly.

Concrete evidence:
- `launcher.exe:0x41d170`
  - allocates / constructs a `CMessageConnection`-family object through `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x18`
  - stores the owning `CLTLoginMediator*` directly at connection `+0xa4`
  - builds endpoint data into owner `+0x5c`
  - immediately calls `connection->+0x1c(owner+0x5c)`
- `launcher.exe:0x41e500`
  - allocates / constructs another `CMessageConnection`-family object through the same `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x1c`
  - stores the owning `CLTLoginMediator*` directly at connection `+0xa4`
  - builds endpoint data into owner `+0x6c`
  - immediately calls `connection->+0x1c(owner+0x6c)`
- those calls match the current best `CMessageConnection` mapping where virtual `+0x1c` is the connection-oriented ensure-connected / engine-`Connect` wrapper

This also gives a more concrete startup owner for the previously abstract “indirect engine entry” model.
Current xrefs show:
- `launcher.exe:0x43909f -> 0x41d170`
- `launcher.exe:0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500`
- the latter set is preceded by mediator-derived fetches from a higher-level owner rooted at `0x4f78b8`, including methods at offsets such as `+0xe0`, `+0xfc`, and `+0x10c`

So the current best concrete startup/runtime reading is now:
- the original launcher really does seem to activate network connection work **indirectly**
- through higher-level owner objects that create `CMessageConnection` children, populate endpoint/config state, and then immediately invoke `connection->+0x1c(...)`
- not through one trivial raw `g_4d6304->Connect(...)` call sitting in launcher mainline

That narrowing materially raises the priority of tracing these owner paths:
- `0x439090 -> 0x41d170`
- `0x439300 -> 0x41e500`
- and the higher-level object rooted at `0x4f78b8`

## New concrete answer: where connection is initiated from

Auth/margin connection-init and post-auth launcher-owned connection-path notes have now been split out of this engine doc so `0x4d6304_network_engine.md` can stay focused on the arg5 object itself.

Canonical auth-side connection-path docs now live under:
- `../auth/CONNECTION_PATHS.md`
- `../auth/README.md`
- `../auth/STATUS.md`

Concise retained summary here:
- current best connection-init model is still **indirect**, not a trivial raw `0x4d6304->Connect(...)` launcher-mainline call
- higher-level owner object rooted at `0x4f78b8` creates `CMessageConnection` children and then drives their connect wrapper
- auth-side high-value anchors:
  - `launcher.exe:0x439090 -> 0x41d170`
- margin-side high-value anchors:
  - `launcher.exe:0x439300 -> 0x41e500`
- later auth-side sender anchor:
  - `launcher.exe:0x43b830` (`AS_GetWorldListRequest` path)
- the separate phase-2 auth/bootstrap child entered from `0x439210` now has its own canonical doc:
  - `0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`
- server-config names/defaults and current auth/margin host/port handling are also now documented in `../auth/CONNECTION_PATHS.md`

Important limitation:
- those new source files are still **not** a faithful full arg5 runtime implementation inside the launcher scaffold
- but they are no longer completely disconnected placeholders either:
  - the current replacement launcher now keeps the arg5 ABI/object-shape trampolines in a dedicated source file (`src/launcher_network_object_abi.cpp`) instead of mixing that scaffold directly into `src/diagnostics.cpp`
  - that ABI layer now incrementally delegates slot `1` / `MonitorPort`, slot `2` / `UDPMonitorPort`, slot `3` / `MonitorEphemeralUDPPort`, slot `5` / `UnmonitorPort`, slot `6` / `Connect`, slot `7` / `Close`, slot `8` / `SendBuffer`, and slot `12` / `CleanupConnection` into the recovered `liblttcp` / `libltmessaging` classes through a sidecar engine/connection binding
  - `Connect`, `Close`, and `SendBuffer` are now routed through liblttcp-side context wrappers (`ConnectContext()` / `CloseContext()` / `SendPacketContext()`) instead of keeping those connection-oriented paths entirely inside `diagnostics.cpp`
  - newer bounded fidelity correction there now keeps slot `6` / `Connect` as the sidecar creation / ensure-connected seam, while slot `7` / `Close` and slot `8` / `SendBuffer` require an already-existing sidecar connection instead of implicitly materializing a new one on demand
  - newer bounded lookup correction also lets the engine resolve those connection-oriented slots by only the tighter RE-backed identities now left in source:
    - the direct connection object pointer, or
    - the connection's direct mediator owner at `+0xa4`, with any extra launcher-bridge record kept only as a mediator-owned sidecar keyed by that auth/margin child pointer
  - the earlier generic engine-keyed known-connection registry is gone; current source falls back only through the active pointer-keyed worker/context tree payloads
  - newer bounded slot-6 correction now makes that direct-connection preference explicit on `Connect(...)` and
    the lower resolved-endpoint helper too:
    - prefer an already-known direct connection-family object first
    - worker insertion on the connect path now also stores the direct connection object and mirrors
      the resulting socket/state back onto the matched connection-family object immediately
  - newer source pruning there also collapsed repeated direct-connection resolution into shared
    local engine helpers and moved repeated worker->connection state/socket mirroring behind one
    helper instead of open-coding that logic at each call site
  - sidecar owner/engine binding state is now also kept by `CLTThreadPerClientTCPEngineBinding` on the liblttcp side rather than by diagnostics-local owner/engine globals
  - sidecar `CMessageConnection` ownership/lookup/drop is now also managed by `CLTThreadPerClientTCPEngine` itself rather than by a diagnostics-local connection table
  - newer class-side cleanup tightening now keeps the pointer-keyed `+0x8c` model closer to the recovered direct-payload read:
    - source now keys `+0x8c` by the resolved `CMessageConnection` object pointer rather than by a synthetic normalized owner key
    - source now stores direct `WorkerThread` payloads in that tree instead of an extra wrapper record
    - `CleanupConnection` now also marks the matching `CMessageConnection` sidecar closed before removing the worker payload
    - generic fallback `CMessageConnection` entries are now dropped during slot-12-style cleanup instead of lingering after the worker-side owner goes away
    - current miss path now logs the nearer recovered `CleanupConnection: Couldn't find socket ...` outcome instead of only returning `0`
  - current diagnostic list-head emptiness for arg5 `+0x80` / `+0x8c` is also synchronized from that sidecar engine state so later stub logs track the new class-backed state more directly
  - newer arg5-helper seam cleanup now also moves the current nonblocking launcher-bridge pump closer to the recovered engine ownership:
    - `LauncherObject_Subobject60_Slot0(...)` no longer calls a mediator polling helper directly
    - arg5 helper `+0x60` slot `0` now resolves the sidecar `CLTThreadPerClientTCPEngine` and calls an engine-owned `PumpLauncherConnectionsFromArg5HelperScaffold()` helper
    - the queue push shaped after original `0x436820` and the synthetic launcher-bridge work-item allocation now also live on the liblttcp engine side rather than in `loginmediator.cpp` or the ABI shell
    - `CLTLoginMediator` still owns only the queued context callback surface and the narrow auth/margin begin wrappers that seed those contexts into the engine
- they are therefore now best treated as **partially wired starter structure**, still far from faithful semantics but no longer only dormant future placeholders

New practical rerun result after that partial wiring:
- a fresh `make run` rerun was made after tightening the arg5-side producer/consumer scaffolds further
- that rerun still showed the same stable consumer-side live surfaces:
  - mediator `+0x2c`
  - arg5 helper `+0x60` slot `0`
  - arg5 helper `+0x60` slot `1`
- but it is no longer accurate to summarize the live path as "still purely empty queue state":
  - queue0C now visibly transitions non-empty and back to empty while the client consumes queued auth-side work items
  - the current diagnostic producer bridge now also mirrors original `0x436820` one step more faithfully by:
    - locking through the arg5 `+0x60` critical-section helper surface
    - checking the combined queue-pair empty state before enqueue
    - signaling arg5 `+0x5c` on empty -> non-empty transition
    - and no longer treating enqueue as a caller-visible success/failure API on the connection-owned
      parsed-packet path, which matches the original `void` helper shape more closely
- concrete deliberate-run evidence now includes consumed queue0C items for:
  - `AuthConnectStatus` (type `2`)
  - repeated `AuthReceivePacket` items
    - earlier logs showed these as type `3`
    - newer static RE now rejects that as too optimistic
    - original type `3` is already the parsed-packet work item queued by `CLTTCPConnection::OnReceive`
    - the extra `AuthReceivePacket` item is better read as a later synthetic receive-drain proxy for
      the unimplemented tail of `CMessageConnection::OnOperationCompleted`
    - newer bounded source correction now also routes that proxy back through the
      `CMessageConnection` queue callback path first, instead of sending it straight to the
      mediator bridge context on the normal path
- that same run reaches launcher-owned auth progression through:
  - `AS_GetPublicKeyReply`
  - `AS_AuthChallenge`
  - `AS_AuthReply`
- current remaining negative runtime result is now narrower:
  - still no new live primary-slot traffic from slot `6` / `Connect`, slot `7` / `Close`, slot `8` / `SendBuffer`, or slot `12` / `CleanupConnection`
  - and in particular still no slot-12 traffic has appeared, which remains consistent with the recovered consumer rule that slot `12` is only reached for type-1 work items

So the new class wiring is no longer just dormant cleanup: the auth-side queue0C consumer chain is now observably live on the active runtime path. The next missing progression is later launcher-owned state after successful auth-side queue consumption, not proof that arg5 queue consumption itself is still dead.

## Newer source-ownership update after that rerun

A later source pass tightened the seam further without yet claiming a fresh runtime validation result.

Build-validated update:
- `src/launcher_network_object_abi.cpp` arg5 helper `+0x60` slot `0` now calls into the liblttcp engine sidecar instead of directly calling a mediator poll helper
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp` now owns the current narrow nonblocking launcher-bridge pump and the current queue0C enqueue helper used by that seam
  - newer bounded pacing correction there keeps `CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold()` as the one-fragment recv seam, but lets the bridge re-enter it within one arg5 helper poll
  - source-owned `AuthReceivePacket` / `MarginReceivePacket` notifications are now best read as
    legacy synthetic receive-drain baggage only
  - newer static RE narrows why:
    - they are not another original type-3 family
    - original type `3` already comes from the parsed-packet work items queued by `CLTTCPConnection::OnReceive`
    - once `0x4490c0` reaches its post-copy virtual tail (`+0x38`, then `+0x2c/+0x30/+0x34`), the
      packet is already consumed locally inside the same callback
    - so current source keeps a distinct synthetic work type only as dormant compatibility
      scaffolding for unexpected paths, not as a faithful later fallback on the auth/margin startup
      path
  - this narrower bridge-level batching remains the current compromise because the earlier fuller
    same-poll recv-drain restoration regressed live runs into the later `Loading Character` stall
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp` now also owns the current launcher-ABI surface attachment / mirror rules used after engine-side connect work reached through connection wrappers
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp` now drives owner-visible arg5 state refresh after `MonitorPort`, `UDPMonitorPort`, `Connect`, `Close`, and `CleanupConnection` sidecar mutations instead of leaving those refresh calls open-coded in the ABI shell wrappers
- later cleanup moved one more mediator-specific coupling point back out of `liblttcp`:
  - `CLTThreadPerClientTCPEngineBinding` now owns only owner<->engine pairing
  - outer launcher/login seam code in `src/launcher_network_object_abi.cpp` again owns the current mediator bind/reset handshake around that binding
  - this is the current better source match for the negative RE result that the original engine object itself is not evidenced to own mediator lifecycle
- newer `2026-04-09` fidelity pass finished the direct queued-context identity restoration on the active worker path while keeping a narrow ABI adapter for raw client.dll consumers
  - `CLTThreadPerClientTCPEngine::WorkerThread_Run` still decides auth-vs-margin from the direct connection family and still allocates the original small `0x0c` queued families for type `2` / connect-status (`0x435050`, vtable `0x004b3df8`) and type `1` / close (`0x435070`, vtable `0x004b3e00`)
  - static RE for both `0x449d40` and `0x436d31..0x436ee7` is now mirrored more directly in source:
    - launcher.exe still establishes the direct connection object as the queued identity
    - active source wraps that identity in a tiny queue-dispatch ABI adapter only where raw client.dll queue consumers would otherwise misinterpret MinGW virtual-table slot numbering
    - `CleanupConnection` searches/tears down by that same unwrapped direct connection identity
    - the consumer still models `context->+0x10(workItem)` and the conditional type-1 `context->+0x04()` release gate, with adapter slot `+0x10` forwarding into the owning connection and adapter byte `+0x04` mirroring the recovered base-connection auto-release flag
  - queue timing remains on the normal completed-operation consumer path rather than the earlier immediate producer-thread drain:
    - source no longer immediately drains type `1/2/3` work items on the producing worker/connect thread
    - it leaves margin/auth completions for the normal completed-operation consumer path instead (`0x436fc0 -> 0x436b10` queue thread or client arg5 helper `+0x60` poll family)
    - practical current reason remains unchanged: the live late instability family is better explained by replacement-only synchronous re-entry of margin type-2 connect-status / later queue callbacks during game-entry bringup than by the recovered direct queued-context identity itself
  - `FindMessageConnection` / engine slot resolution still stop accepting mediator bridge-context identities on the active path; the remaining accepted shapes are direct connection identities plus direct owner-context lookup
  - arg5 helper polling still looks at the mediator's direct auth/margin child connections only as a no-worker fallback, and no longer routes through a separate mediator bridge-context record
- `matrixstaging/game/src/libltclientlogin/loginmediator_auth_entry.cpp` correspondingly no longer creates/maintains those bridge-context sidecars on auth/margin begin; the live owner link stays the direct mediator pointer at connection `+0xa4`
- newer successful launcher-into-game runtime logs still line up with that tighter read on the active path:
  - no `pendingCopiedPackets=` logs
  - no synthetic receive-drain handling logs
  - copied auth/margin packets log as consumed directly on the in-callback post-copy leaf path instead
- `src/launcher_network_object_abi.cpp` is correspondingly thinner on this seam:
  - the file is now best read as a **raw arg5 ABI shell** rather than as the real engine implementation
    - current source names that shell explicitly as `LauncherObjectAbiShell`
    - the shell object's first field is the replacement primary arg5 vtable pointer
    - the rest of the shell only preserves the client-visible object layout (`+0x0c/+0x34/+0x5c/+0x60/+0x7c/+0x80/+0x8c/+0x98`)
    - real queue/connection/producer behavior continues to move into `liblttcp`
  - primary-slot wrappers no longer manually call the sidecar-state sync helper after those engine mutations
  - helper-side call-count throttling / queue-state debug logging used during earlier bridge bringup has been pruned back out of the ABI shell
  - helper subobject wrappers are now smaller again:
    - `+0x98` uses shared lock-helper enter/leave bodies directly
    - `+0x60` keeps only the one extra engine-pump side effect on slot `0`, layered above the
      now-separate pure `0x4147b0` enter-helper body
    - the helper-vtable arrays are now sized only for the live 2-slot surfaces instead of oversized diagnostic tables
  - the ABI shell keeps raw arg5 object layout ownership, but less of the bridge-controller logic and less diagnostic scaffolding

Current tightened worker-loop read after the latest `0x42fe50 / 0x42f970 / 0x42fbd0 / 0x44a9f0 / 0x44aa70 / 0x44ac90` pass:
- worker-thread socket ordering is now much narrower than the older source-owned poll loop
  - `select()` is blocking
  - main-loop sets are rebuilt each iteration as:
    - read  = connection socket + wakeup socket
    - except = connection socket
    - write = connection socket only while TCP connect completion is pending or while a queued send
      buffer is being drained
  - after select returns, current best original order is:
    1. connection except handling
    2. readable-socket recv drain
    3. writable-socket connect/send handling
    4. wakeup-socket drain / exit-by-request
  - after terminal close/failure, original code falls into a wakeup-only wait loop rather than
    returning immediately; current source now mirrors that intent more closely
- connection-side send/close state is also tighter now:
  - `0x431ff0` writes the direct worker pointer back to `[connection+0x08]`
  - `0x44a9f0` seeds connection byte `+0x38 = 1`
  - worker-side pop helper `0x44aa70 = TryPopQueuedSendBufferWithEndpoint` drains one queued item
    and restores `+0x38 = 1` when the queue is empty
  - `0x42fbd0 -> 0x44ad80 = CLTTCPConnection::QueueSendBuffer` pushes copied send buffers through
    the connection-owned queue rooted at `+0x3c`, clears `+0x38`, and signals the worker wakeup
    socket
    - sibling helper `0x44ac90 = CLTTCPConnection::QueueSendBufferWithEndpoint` is the explicit
      endpoint-taking variant reached from slot `9` / `0x42fd10 = SendBufferWithEndpoint`
    - active-state guard there is exactly connection state `1` or `2`
    - outside those states the original path does not silently return; it emits the string-backed
      `Send failed! ... not connected/connecting ...` warning using endpoint bytes from
      connection `+0x24`
    - send-queue helper family now narrows further too:
      - `0x44a500 = AllocateQueuedSendBufferWithEndpointItem`
        - allocates the `0x14` queued wrapper `{sendBufferStorageDescriptor*, endpointKey16}`
      - `0x44a3e0 = AllocateQueuedSendBufferStorageDescriptor`
        - allocates the `0x0c` send-buffer descriptor `{usesPooledBuffer00, bufferStorage04, bufferByteCount08}`
      - `0x44a830 = EnqueuePendingSendQueueItemWrapper`
        - lock-free enqueue helper over connection `+0x3c`
        - allocates a separate `0x14` queue node whose payload pointer lives at node `+0x10`
        - if tail-node `next` is already non-null, it first helps advance the versioned tail state
      - `0x44a900 = TryDequeuePendingSendQueueItemWrapper`
        - not just a raw unlink helper: it performs one Michael-Scott-style dequeue step
        - when dequeue succeeds, it returns the queued wrapper pointer from `headNext + 0x10`
        - then retires the old dummy head node onto the queue-local retired-node stack at queue `+0x28`
      - `0x449ff0 = TryCompareExchangeVersionedPointerPair`
        - generic `cmpxchg8b` helper used by the queue family to publish `{pointer, version}` pairs
        - negative result: current pass does **not** support reading it as an arbitrary container-wide commit helper; it is the narrow compare/publish primitive beneath enqueue/dequeue retries
      - `0x44a7c0 = ReleaseSendBufferStorage`
        - `usesPooledBuffer00 == 0` frees `bufferStorage04` through tracked free
        - `usesPooledBuffer00 == 1` zeros a full `0x1000` byte block and returns it to the pooled-send-buffer freelist
        - that means the currently recovered ownership modes are:
          - `0` = transfer tracked heap buffer
          - `1` = copy bytes into pooled `0x1000` storage
          - `2` = transfer an already-pooled `0x1000` block
      - `0x44a620 = AllocatePooledSendBufferStorage`
        - fixed-size pooled allocator for raw `0x1000` send-buffer blocks
        - backing blocks grow toward roughly half a page (`(pageSize >> 1) - 4`) on first sizing, just like the other launcher fixed allocators
  - `0x42f970` writes state `4` first, then:
    - graceful close uses `shutdown(socket, 1)` immediately only when the send queue is already
      empty
      - there is no source-side invalid-socket precheck on the original path; the call simply logs
        the exact shutdown-failed warning on `SOCKET_ERROR`
    - otherwise the worker-thread write side issues that same half-close after queued sends drain
    - hard close uses `closesocket(socket)` directly
      - likewise without a preceding invalid-socket guard; failure flows into the exact
        closesocket-failed warning text
- active receive ownership is tighter too:
  - worker-backed connections now use the direct worker-thread recv -> `OnReceive` path on the live
    socket
  - the launcher-bridge pump remains only as a fallback for no-worker source-owned paths, not as
    the primary active-path receive producer

Important remaining gaps:
- this is still not a byte-faithful end state for the whole worker family
- current source still does **not** enqueue the original send-failure/type-6 work-item family from
  the worker write path; non-`WSAEWOULDBLOCK` send failure is only logged at the moment
- datagram-specific `recvfrom` / `sendto` peer-address side effects from the original worker body
  remain outside the active source path
- some later wakeup-only / teardown queue item combinations are still source-owned approximations
  rather than exact original class implementations

Newer source-ownership cleanup on the launcher entry side:
- launcher-facing arg5 entrypoints no longer hang off `src/diagnostics.h`
  - current source now exposes them through `src/launcher_network_object_abi.h` as launcher-owned ABI helpers instead
- `matrixstaging/game/src/launcher/launcher.cpp` now preserves a dedicated anchored
  `CLauncher::InitializeThreadPerClientTCPEngine()` call for the `0x40b740 -> 0x40a380` boundary,
  and that method now owns the explicit launcher-side sequence more directly:
  - build raw `0xb4` ABI shell
  - store it in `g_pLauncherObject6304`
  - call resolved arg6 wrapper slot `+0x08`
  - preserve the raw `result < 1` success test
- the remaining launcher ABI helper on this path is now narrowed to the allocation + ctor step only,
  instead of a broader synthetic install helper that also hid the store/register sequence
- the liblttcp sidecar may still be materialized from the startup handoff helper, but later arg5
  vtable/helper dispatch now prefers the single registered launcher binding instead of calling a
  broad `GetOrCreate...` path on every use
- arbitrary arg5 accessors no longer own mediator install/reset side effects
- a follow-up cleanup pass also tightened the teardown side toward original `0x40b389..0x40b404`:
  - launcher-owned shutdown now explicitly releases arg5 through primary slot `0` with flag `1`
  - clears the launcher-side arg5 pointer
  - and then calls the mediator clear slot `+0x0c`
- the launcher ABI shell no longer keeps a separate `g_CurrentLauncherObject` owner-global just to make repeated installs safe
  - reinstall now releases any prior caller-owned arg5 object through the same public release helper
- the sidecar binding itself is now a single launcher-owned static binding object instead of a separately heap-allocated binding pointer
- a newer ctor-shape cleanup also now splits the ABI-shell initialization by the same recovered constructor boundary:
  - base-style init after `0x4366f0`
  - then derived-style init after `0x431c30`
- a follow-up bounded cleanup also reduced source dependence on whole-object zeroing:
  - source no longer `memset`s the entire `0xb4` shell before ctor-shaped init
  - instead it seeds only the fields that partial cleanup or the current ctor-shaped steps may read before full initialization
  - the older source-only explicit ctor writes to `+0x88/+0x94` have now also been pruned back out again
  - so those dwords remain only pre-init stability values in source, not fake derived-ctor side effects
- the old mutable runtime vtable seeding step is gone too
  - arg5 primary and helper vtable tables are now compile-time static tables instead
- so the current arg5 path is still not a fully faithful ctor/runtime reproduction, but it is less diagnostics-owned and a little closer to the original launcher-owned create/register and release/clear boundaries

## Newer arg5 ownership pass: structural mismatch reduction between shell and target class

A later focused pass revisited the structural mismatch between:
- original launcher-visible arg5 object at `0x4d6304`
- `src/launcher_network_object_abi.cpp`
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.*`

The main result is **not** “remove the wrapper.”
The result is a narrower, more mechanical wrapper because more of the recovered object-facing state
now lives in `CLTThreadPerClientTCPEngine` itself.

### Constructor / helper evidence kept in scope

This pass stayed anchored to the same original facts:
- builder: `launcher.exe:0x40a380`
- derived ctor: `launcher.exe:0x431c30`
- base ctor: `launcher.exe:0x4366f0`
- helper enter/leave family: `launcher.exe:0x4147b0` / `0x4147c0`
- wait helper: `launcher.exe:0x435f90` / `0x435fa0`
- primary slot `12`: `launcher.exe:0x4316a0`

Static ctor evidence still says the shell boundary must remain because original code consumes raw
embedded addresses for:
- queue pair at `+0x0c` / `+0x34`
- wait helper at `+0x5c`
- lock helpers at `+0x60` / `+0x98`

But this pass also confirmed that those surfaces do **not** all need to remain wrapper-owned in the
source-level implementation.

### Per-offset ownership matrix after the pass

| Offset | Original meaning | Before this pass | After this pass | Current status |
|---|---|---|---|---|
| `+0x0c` | queue0C object from `0x436610 -> 0x436340` | shell-owned live queue storage; class consumed through an explicit launcher-ABI attachment seam | class now owns a first-class fallback queue surrogate and chooses active queue storage through `AttachLauncherAbiSurfaceScaffold(...)`; live launcher ABI path still uses shell queue storage | **shared**: shell keeps raw storage, class owns queue semantics / fallback model |
| `+0x34` | queue34 object from `0x436610 -> 0x436340` | same as `+0x0c` | same as `+0x0c` | **shared** |
| `+0x5c` | wait/event helper root | shell-owned helper body (`SetEvent` / wait / reacquire) | helper body now lives on `CLTThreadPerClientTCPEngine::{SignalQueueEventHelper, WaitQueueEventHelper}`; shell helper thunk only routes raw embedded calls there | **raw address shell-owned; semantics class-owned** |
| `+0x60` | helper root + `CRITICAL_SECTION` for queue lock | shell-owned enter/leave plus pump side effect | target class now owns the pure `0x4147b0/0x4147c0` queue-lock helper bodies and fallback lock surrogate; the shell `+0x60` slot-0 wrapper now calls `EnterQueueLockHelper()` and then layers the extra engine-pump side effect above that pure helper body, while slot `1` routes to `LeaveQueueLockHelper()` | **raw address shell-owned; semantics class-owned** |
| `+0x7c` | queue signal event from `CreateEventA` | shell-owned event and helper usage | target class now owns fallback event state and helper semantics; live launcher ABI path still uses the shell event handle through attached surface mapping | **shared** |
| `+0x80` | endpoint-keyed sentinel/tree head pointer | shell allocated and shell synchronized empty/non-empty shape | target class now owns the sentinel-head object and count mirror; attachment updates shell `+0x80/+0x84` to point at class-owned head/count state | **class-owned data mirrored through shell** |
| `+0x8c` | context-keyed sentinel/tree head pointer | shell allocated and shell synchronized empty/non-empty shape | target class now owns the sentinel-head object and count mirror; attachment updates shell `+0x8c/+0x90` to point at class-owned head/count state | **class-owned data mirrored through shell** |
| `+0x98` | cleanup lock helper root + `CRITICAL_SECTION` | shell-owned enter/leave; shell wrapper also locked slot `12` manually | cleanup helper semantics now live in `EnterCleanupLockHelper()` / `LeaveCleanupLockHelper()` and `CleanupConnection()` now acquires/releases that lock internally | **raw address shell-owned; semantics class-owned** |

### What moved into `CLTThreadPerClientTCPEngine`

`matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h` and `.cpp` now carry explicit
source-owned surrogates for the missing object-facing surfaces:
- fallback queue pair fields matching the recovered `+0x0c` / `+0x34` queue shape
- explicit helper-family surrogate structs for:
  - wait helper `+0x5c`
  - lock helper `+0x60`
  - lock helper `+0x98`
- fallback event state for `+0x7c`
- explicit sentinel-head structs for:
  - endpoint-tree head `+0x80`
  - context-tree head `+0x8c`
- mirrored count state for:
  - `+0x84`
  - `+0x90`
- a new launcher-ABI attachment map so the class can use live shell queue/lock/event addresses when
  present, while still owning the conceptual engine-side state model

That means the target class is now the canonical owner of:
- helper semantics
- fallback queue/event/lock state
- sentinel-head occupancy/count mirror state
- and the shell-to-class attachment rules for the current arg5 boundary

### What became thinner in `src/launcher_network_object_abi.cpp`

The wrapper is still required, but it is now more mechanical.

Main reductions in the shell:
- the old shell-side state-sync callback path is gone from this seam
- the shell no longer open-codes `+0x80` / `+0x8c` occupancy synchronization
- the shell no longer open-codes helper semantics as the primary implementation
  - `+0x5c` now routes to class-owned wait/event helpers
  - `+0x60` now routes to class-owned queue-lock helper bodies
  - `+0x98` now routes to class-owned cleanup-lock helper bodies
- primary slot `12` wrapper no longer acquires/releases arg5 `+0x98` itself before calling into the
  class
- the binder now attaches the raw shell surfaces into the class with one explicit attachment struct
  instead of separately pushing queue/event/sync callback pieces
- once attached, the shell's original constructor-allocated list heads are replaced by class-owned
  sentinel-head objects and the transient shell allocations are freed

### Current slot-ownership read after the pass

Primary slots `0..12` now classify as:
- **shell-owned**
  - slot `0` / deleting dtor
    - top-level `malloc(0xb4)` lifetime and release order still belong to the launcher shell
- **thin shell -> class thunks**
  - slots `1,2,3,4,5,6,7,8,9,10,11,12`
- **candidate for later direct-vtable experiment**
  - slots `4,9,10,11`
  - slot `12` is now a better later candidate than before because the helper lock behavior moved
    into the target class, but the top-level shell boundary is still required today

### What still remains shell-only

These surfaces still intentionally stay on the wrapper side:
- raw `0xb4` object size/layout consumed by original code
- compile-time replacement primary/helper vtable tables used by original callers
- top-level allocate/register/release/clear boundary around:
  - `0x40a380`
  - mediator `+0x08`
  - mediator `+0x0c`
  - slot `0` deleting-dtor release path
- live embedded helper/object address identity for original code using:
  - `ecx = arg5 + 0x5c`
  - `ecx = arg5 + 0x60`
  - `ecx = arg5 + 0x98`

### Runtime note

Local harness `make -j6 run` still returned a non-zero Wine/process code on one observed run after
this pass, but user validation for the same code state reported that the launcher **entered game
successfully** on the active path.

That is the important regression check for this pass:
- the wrapper became thinner
- more arg5 structure moved into the target class
- and the working path still launched

## 2026-03-29 layout-restructuring follow-up for the MinGW native-vptr experiment

A follow-up restructuring pass pulled `mxo::liblttcp::CLTThreadPerClientTCPEngine` back toward the
recovered original arg5 body instead of leaving later source-owned baggage inside the class.

What changed:
- non-original source bookkeeping moved out of the class body into external side storage instead of pretending to be hidden launcher fields
  - launcher-ABI attachment map
  - endpoint-payload backing keyed by raw tree node identity
  - context-payload backing keyed by raw tree node identity
  - queue-thread ownership that mirrors real `+0x04/+0x08` class fields without inventing extra launcher offsets
- the native class body itself now again carries the recovered top-level arg5 fields at the
  original offsets
- helper roots at `+0x60` and `+0x98` now again embed inline `CRITICAL_SECTION` storage, matching
  the original `0x1c` helper shape instead of the older heap-backed `0x8` surrogate
- `ltthreadperclienttcpengine.h` now statically enforces:
  - `sizeof(CLTThreadPerClientTCPEngine) == 0xb4`
  - recovered field positions through the `0xb4` top-level layout via a compile-time layout mirror

Observed result from the latest default MinGW run log (`~/MxO_7.6005/resurrections.log`):
- `active=mingw-native-vptr`
- native size now matches shell size:
  - `nativeSize=0xb4`
  - `shellSize=0xb4`
- top-level offsets now match the launcher shell completely:
  - `field04=0x4`
  - `field08=0x8`
  - `queue0C=0xc`
  - `queue34=0x34`
  - `wait5C=0x5c`
  - `lock60=0x60`
  - `event=0x7c`
  - `list80=0x80`
  - `count84=0x84`
  - `list8C=0x8c`
  - `count90=0x90`
  - `cleanup98=0x98`
  - `lockHelperSize=0x1c`
- the live shell now stores the native GCC address-point directly:
  - `storedVptr=0x6ae4b4`
  - `nativeAddressPointMatch=1`
- the live shell still keeps the wrapper-owned helper tables at those embedded addresses, so direct
  helper dispatch remains valid while only the primary vptr changed

So the blocker moved one step further and the experiment now crossed it:
- **no longer** the big top-level `0xf0` vs `0xb4` body mismatch from the earlier pass
- **no longer** blocked from installing the native primary address-point on the live shell
- the embedded helper dispatch surface remains wrapper-owned, which is acceptable for this narrow
  primary-dispatch change

Current conclusion after the later late-runtime crash/corruption regression work:
- the restructuring pass was still the right faithful move for the recovered `0xb4` body/layout
- but the earlier launch-only validation was not enough to prove the MinGW native-vptr path safe on
  the full late launcher-into-game route
- current source therefore puts the arg5 **wrapper-table** primary-dispatch path back as the
  default, even on MinGW builds
- the MinGW native-vptr path survives only as an explicit compile-time experiment:
  - `make MXO_ENABLE_MINGW_NATIVE_ARG5_VPTR=1`
- optional hard rollback remains available too:
  - `make MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR=1`
- practical reason for the rollback: arg5 is passed straight into `InitClientDLL`, so this is one
  of the few places where MSVC2003-vs-MinGW virtual-dispatch ABI differences can directly matter

## ABI boundary note

The broader ABI-boundary / wrapper-reduction investigation for arg5 was moved out of this per-object doc because the lessons also apply to other launcher-owned objects.

See:
- `./OBJECT_ABI_BOUNDARIES.md`

Current short takeaway for `0x4d6304` specifically:
- keep the launcher-owned arg5 shell as the top-level object boundary
- do not replace it wholesale with the native C++ `CLTThreadPerClientTCPEngine` object
- if reduction is pursued later, reduce individual slot bodies, not the shell layout contract
