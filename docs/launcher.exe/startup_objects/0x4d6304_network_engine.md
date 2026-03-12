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
- `launcher.exe:0x40a380`

### Constructor
- `launcher.exe:0x431c30`

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

## Constructor evidence

Ctor `0x431c30`:

- first calls base ctor `0x4366f0`
- then overwrites the primary vtable with `0x4b2768`
- allocates and initializes internal list/tree objects at offsets around:
  - `+0x80`
  - `+0x8c`
  - `+0x98`
  - `+0x9c`

Base ctor `0x4366f0` itself:

- sets base vtable `0x4b3e74`
- initializes a subobject at `+0x5c`
- initializes another helper subobject rooted at `+0x60`
- allocates an array at `+0x08`
- constructs per-entry helper objects of size `0x3c`

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

## New clarification from current scaffold work

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

The custom launcher now also wires the first few original primary vtable slots from `0x4b2768` as **logging placeholders with matching stack cleanup**:

- slot 0 -> `0x4319a0`-style release / destructor probe (`ret 4`)
- slot 1 -> `0x431ce0`-style 3-arg probe (`ret 0xc`)
- slot 2 -> `0x4325d0`-style 3-arg probe (`ret 0xc`)
- slot 3 -> `0x436000`-style 3-arg probe (`ret 0xc`)
- slot 4 -> `0x42f7c0`-style 1-arg probe (`ret 4`)

This is still **not** faithful behavior for those methods.
The current probes only log arguments / object state and return neutral placeholder values.

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

What is now modeled in the scaffold:

- slots `0..12` are all now present with matching stack cleanup shapes
- slot `10` now matches the original tiny stub exactly in effect (`xor al,al ; ret 4` -> returns `0`)
- slot `5` now models the one high-confidence semantic path already recovered from static analysis:
  - when the `+0x80` list is still empty, it zeroes the caller out-pointer and returns `0x7000004`
- slot `12` now at least distinguishes the proven empty-`+0x8c` fast path from the still-unreconstructed non-empty teardown path
- slots `6..9` are still logging placeholders only; their deeper real behavior has not been reconstructed yet

Why slot `5` matters:

Static disassembly of `0x431840` shows a concrete empty-container behavior rather than a generic opaque callback:

- it searches the `+0x80` intrusive/list container
- if the search misses the container head (`eax == [esi]` after `self += 0x80`), it writes `0` to the caller output pointer
- and returns `0x7000004`

That means the current scaffold can now reproduce one real launcher-observed miss result instead of only returning a generic neutral placeholder there.

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

Practical status of this update:

- the widened arg5 vtable scaffold built successfully
- a follow-up rerun with the usual deep path
  - `cd /home/morgan/mxo/code/matrix_launcher && make run_binder_both`
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

## New validation result from the in-launcher `ret` bypass

To test whether the current late `arg2` crash was merely blocking later arg5 traffic from becoming visible, the launcher now has an opt-in diagnostic path:

- `MXO_ARG2_RET_BYPASS=1`
- optional limit override: `MXO_ARG2_RET_BYPASS_MAX=<n>`

That path simulates a single x86 `ret` when a fault lands inside current `arg2 filteredArgv` storage.

Representative validation run:
- `cd /home/morgan/mxo/code/matrix_launcher && MXO_ARG2_RET_BYPASS=1 MXO_ARG2_RET_BYPASS_MAX=4 make run_binder_both`

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
