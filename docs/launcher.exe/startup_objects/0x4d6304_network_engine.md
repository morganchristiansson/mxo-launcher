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

Newer string-backed naming now tightens several of those slots substantially:
- slot 1 / `0x431ce0` = **`MonitorPort`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::MonitorPort: Successfully monitored ...`
    - `... port is already monitored`
- slot 2 / `0x4325d0` = **`UDPMonitorPort`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::UDPMonitorPort: Successfully monitored ...`
    - `... Failed to monitor port ...`
- slot 3 / `0x436000` = **provisional UDP-monitor helper / local-port query wrapper**
  - no direct surviving string name recovered yet
  - current static behavior:
    - calls slot `2` / `UDPMonitorPort` with `port = 0`
    - and on one branch then queries `getsockname` / `ntohs` to hand a bound local port back to the caller
  - this is still lower confidence than the string-backed names above, but it is no longer just an anonymous opaque callback
- slot 6 / `0x4328a0` = **`Connect`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::Connect: ...`
- slot 7 / `0x42f970` = **`Close`**
  - proven by in-function log strings:
    - `CLTThreadPerClientTCPEngine::Close: shutdown() failed ...`
    - `CLTThreadPerClientTCPEngine::Close: closesocket() failed ...`
- slot 8 / `0x42fbd0` = **`SendBuffer`**
  - proven by in-function log string:
    - `CLTThreadPerClientTCPEngine::SendBuffer: Send failed ...`
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

### New clarification: `+0x80` and `+0x8c` are not the same kind of key space

A newer static pass tightens the semantic split between the two containers.

Recovered helper meaning:
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
  - `MonitorPort` creates `socket(AF_INET, SOCK_STREAM, 0)`, then `bind`, then `listen`
  - on success it stores the new AcceptThread-style payload into the matched/new `+0x80` node at `[node+0x20]`
- `+0x8c` stores pointer-keyed nodes whose payload at `[node+0x14]` is a **WorkerThread-style worker object**
  - helper `0x431ff0` allocates a `0x48` object via `0x431b60`
  - that ctor is string-backed by `CLTThreadPerClientTCPEngine::WorkerThread`
  - helper `0x431ff0` then inserts `(contextKey, workerPayload)` into `+0x8c`
  - slot `2` / `UDPMonitorPort` uses that helper after successful UDP socket/bind setup and then marks the returned worker object with `[worker+0x34] = 2`
  - slot `6` / `Connect` uses that helper after successful TCP setup/connect sequencing and then marks the returned worker object with `[worker+0x34] = 1`

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
- if both work item and context are present, calls arg5 primary slot `12` with the dequeued `context`
- then calls `context->+0x10(workItem)`
- and later releases both objects as needed

That ties several earlier partial observations together:
- the queued first-dword object really is a **work/status item** rather than only arbitrary pointer noise
- the queued second-dword object is a real paired **context/owner** pointer
- and arg5 primary slot `12` is part of the **non-empty queue dispatch / teardown / state-transition path**, not just an arbitrary later method

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

## New runtime validation after clean `InitClientDLL = 1` and deliberate `RunClientDLL`

A newer binder/scaffold experiment changed that runtime picture materially.

Command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_TRACE_WINDOWS=1 \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

What is now statically confirmed first:
- original launcher helper `0x40a4d0` treats positive export returns as success on this path:
  - after `InitClientDLL`: `0x40a5a9..0x40a5ab` -> `test eax,eax ; jg ...`
  - after `RunClientDLL`: `0x40a622..0x40a624` -> `test eax,eax ; jg ...`
  - after `TermClientDLL`: `0x40a6bc..0x40a6be` -> `test eax,eax ; jg ...`
  - overall success return: `0x40a6fd` -> `al = 1`
- so the current clean `InitClientDLL returned: 1` binder result is now evidence-backed success, not only a local launcher heuristic

What the deliberate runtime experiment then shows:
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

A newer throttled runtime log on the same deliberate path now shows that state staying unchanged through repeated polling:
- queue0C: `current0 == current1 == block0 == block1`
- queue34: `current0 == current1 == block0 == block1`
- representative sampled counts still showing that exact state: `1`, `2`, `4`, `8`, `16`, `32`, `64`, `128`, `256`, `512`, `1024`

A newer original-launcher static pass now also identifies the corresponding producer side more concretely.
Original enqueue helper `launcher.exe:0x436820`:
- acquires arg5 helper `+0x60`
- snapshots whether both queues were empty before enqueue
- calls `0x436670(argA, argB, queueSelect)` to push an **8-byte pair** into one of the two queues
- `queueSelect = 0` uses queue0C
- `queueSelect != 0` uses queue34
- releases arg5 helper `+0x60`
- and if both queues were previously empty, signals arg5 helper `+0x5c` slot `0`

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
  - stores an immediate payload/code in `[obj+0x08]`
- `0x435da0 -> 0x435070`
  - allocation size `0x0c`
  - constructor sets vtable `0x4b3e00`
  - seeds `[obj+0x04] = 1`, `[obj+0x08] = 0`

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
  - later in the same `0x4328a0` method the launcher calls `WS2_32!connect`
  - successful connect then creates/inserts a `WorkerThread` payload into arg5 `+0x8c` and marks it with `[worker+0x34] = 1`
  - so `0x4329cc` is now best read as part of a **TCP connect / connect-status / worker-start** producer path

#### `launcher.exe:0x432d86`, `0x432dc1`, and `0x432dd7`
- same function, several queue0C variants
- one branch enqueues `(work=eax-or-NULL, context=edi, queueSelect=0)` where work may come from a `0x0c` / `0x4b3e00` object family
- another branch allocates `0x0c` via `0x435d90`, constructs via `0x435050(1)`, and enqueues that coded object with `context=edi`
- allocation failure path falls back to `(work=0, context=edi, queueSelect=0)`
- because these branches sit in the same `0x4328a0` socket/connect method, they are now best treated as **TCP connect follow-up / completion-status queue submissions**, not just anonymous queue variants

#### `launcher.exe:0x449d8a`
- sits inside a loop
- repeatedly enqueues `(work=[ebp-0x08], context=edi, queueSelect=0)`
- then polls another object at `[edi+0x6c]` until that helper returns non-zero
- current best reading is that this is a submit-and-wait style queue0C interaction rather than a one-shot fire-and-forget producer
- newer static narrowing adds one useful detail:
  - after the wait loop, this function checks return/status values in the `0x700000x` family (`0x7000000`, `0x700000b`)
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
- and then call arg5 primary vtable offset `+0x30`

On the client side that is:

```asm
62531fb8: mov edx, [esi]
62531fba: push edi
62531fbb: mov ecx, esi
62531fbd: call [edx+0x30]   ; arg5 primary slot 12
```

That means the current absence of arg5 slot-12 runtime traffic on deliberate `RunClientDLL` runs is now explained more narrowly than before:
- not because slot 12 is irrelevant,
- but because the current scaffold never feeds the queue branch that would reach it.

Current best interpretation:
- arg5 is now runtime-validated more concretely than before
- the helper/lock surface at `+0x60` is definitely live on the `RunClientDLL` path
- the queue cursor fields around `+0x0c/+0x1c` and `+0x34/+0x44` are also definitely live on that path
- `RunClientDLL` is repeatedly exercising the **consumer** side of this engine in non-blocking poll mode
- the original launcher producer side now has concrete xrefs and concrete queued pair shapes
- and if queue0C were actually being fed, the next observable arg5 step would likely be primary slot `12` (`+0x30`)
- but on the current scaffold that producer side still does not appear to be feeding even the now-best-understood queue0C path, so both queues remain in a stable **empty cursor** state rather than advancing
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
- notably, `0x449d40` uses object field `+0x6c` as the wait/poll helper and repeatedly enqueues `(work, self, 0)` through `0x436820`

Current best virtual-method mapping on that class is now:
- vtable `+0x10` / `0x4490c0` = likely **`OnOperationCompleted(workItem)`**
  - dispatches on `workItem->[+0x04]`
  - contains string-backed receive/completion/error handling paths
- vtable `+0x20` / `0x449d20` = likely **`SendPacket(...)`**
  - forwards into engine `+0x20`
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
  - wait/poll helper at `+0x6c`

That newer read also narrows the real engine-call signatures more than the earlier slot-name pass alone:
- `0x449cd0` does **not** build a raw `(ip, port, context)` call
  - it compares/copies the requested endpoint into `self+0x24`
  - then calls engine `+0x18` with **`self`**
  - so current best original read is that arg5 slot `6` / `Connect` is importantly a **connection-object-based** entrypoint
- `0x449ca0` similarly forwards **`self`** into engine `+0x1c`
  - so arg5 slot `7` / `Close` is likewise connection-object-based on this path
- `0x449d20` forwards packet/buffer args together with **`self`** into engine `+0x20`
  - so arg5 slot `8` / `SendBuffer` is also reached through the connection object, not as a free-standing raw socket helper alone

Current best reading from that combination:
- the queued second dword currently described as generic `context/owner` is now more specifically likely a **`CMessageConnection`-family object pointer** on at least important producer/consumer paths
- that object is no longer just a passive owner token:
  - it appears to be an active bridge back into engine `Connect` / `Close` / `SendBuffer` paths
- and the pointer-keyed `+0x8c` container now looks more concrete too:
  - helper `0x431ff0` inserts nodes there using the raw connection/context pointer as key and a `WorkerThread`-style payload as value
  - both `UDPMonitorPort` and `Connect` then store that returned worker back through the connection-side object before marking worker state `2` / `1`
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
  - builds endpoint data into owner `+0x5c`
  - immediately calls `connection->+0x1c(owner+0x5c)`
- `launcher.exe:0x41e500`
  - allocates / constructs another `CMessageConnection`-family object through the same `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x1c`
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

The current best answer is now substantially tighter than “some later indirect owner path”.
The original launcher appears to initiate connection work from a higher-level owner object rooted at `0x4f78b8`, using a two-stage setup that consumes the recovered auth/margin server config variables before calling into `CMessageConnection`.

### Auth-side launcher path

Owner root construction/seed path:
- function body around `launcher.exe:0x41b1c1`
- stores global owner pointer: `0x4f78b8 = esi`
- immediately calls `0x43b300`, which allocates/initializes a small family of launcher-global helper/state objects at:
  - `0x4f7868`
  - `0x4f786c`
  - `0x4f7870`
  - `0x4f78a0`
- then seeds owner `+0x4c` from current auth DNS config:
  - reads `qsAuthServerDNSName` current string via `0x4f7b14`
  - calls `0x440d80(owner+0x4c, authDnsName, mode)`
- this is now the strongest current launcher-side auth-DNS consumer tied to the recovered config object family

A further state/dispatch clue now also exists in the same owner family.
`launcher.exe:0x439300` consults the owner's current state object through `[owner+4]->vtable+0x18`, then dispatches several cases into the margin-side connection initializer `0x41e500`.
The concrete cases currently recovered are:
- use owner byte `+0xcc8` and call owner vtable `+0xe0`, then `0x41e500`
- use owner dword `+0x12c` and call owner vtable `+0xfc`, then `0x41e500`
- call owner vtable `+0x10c`, then `0x41e500`
- or, if owner dword `+0x104 != -1`, call owner vtable `+0xfc(owner+0x104)`, then `0x41e500`

So the current best reading is not just “owner object exists”.
There is now a concrete **owner-state-driven dispatcher** that decides when to start margin-side connection work.

Auth connection-init path:
- `launcher.exe:0x43909f -> 0x41d170`
- `0x41d170`:
  - constructs a `CMessageConnection`-family object through `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x18`
  - reads auth port from `AuthServerPort` current value `0x4f7a50`
  - builds endpoint data into owner `+0x5c`
  - then immediately calls `connection->+0x1c(owner+0x5c)`
- with the current method mapping, that virtual `+0x1c` remains best read as the connection-oriented ensure-connected / engine-`Connect` wrapper

So the current best auth-side connection-init model is:
1. auth DNS name is copied into owner `+0x4c`
2. auth port is read from `0x4f7a50`
3. endpoint is built at owner `+0x5c`
4. `CMessageConnection->+0x1c(owner+0x5c)` initiates the connection through the launcher-owned engine

### Margin-side launcher path

Margin connection-init path:
- `launcher.exe:0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500`
- `0x41e500`:
  - constructs another `CMessageConnection`-family object through the same `0x4417e0 -> 0x448b40` path
  - stores it at owner `+0x1c`
  - reads `MarginServerDNSSuffix` current string from `0x4d6814`
  - reads `MarginServerPort` current value from `0x4d669c`
  - calls `0x440d80(owner+0x3c, marginSuffix, mode)` and later `0x440bb0(owner+0x3c, mode)`
  - builds endpoint data into owner `+0x6c`
  - then immediately calls `connection->+0x1c(owner+0x6c)`

So the current best margin-side connection-init model is parallel to the auth side:
1. margin suffix is copied into owner-side string state
2. margin port is read from `0x4d669c`
3. endpoint is built at owner `+0x6c`
4. `CMessageConnection->+0x1c(owner+0x6c)` initiates the connection through the launcher-owned engine

### Why this matters

This now answers the “where is connection initiated from?” question more concretely:
- **not** directly from raw global `0x4d6304`
- **not** directly from client.dll alone
- but from higher-level launcher owner objects that:
  - consume auth/margin server config variables
  - create `CMessageConnection` children
  - build endpoint state
  - and immediately invoke the connection wrapper that drives the launcher-owned network engine

### New implementation milestone: real auth connect now possible from the scaffold

The current replacement launcher now also has an implementation-side milestone that goes beyond naming-only scaffolds.
The diagnostic sidecar engine/login-controller path can now make a **real TCP auth connection** on the binder/stub path.

Current defaults now wired from recovered binary strings:
- auth host = `auth.lith.thematrixonline.net`
- auth port = `11000`
- margin suffix = `.lith.thematrixonline.net`
- margin port = `10000`

Current opt-in env knobs:
- `MXO_BEGIN_AUTH_CONNECTION=1`
- `MXO_BEGIN_MARGIN_CONNECTION=1`
- optional host/port overrides:
  - `MXO_AUTH_SERVER_DNS`
  - `MXO_AUTH_SERVER_PORT`
  - `MXO_MARGIN_SERVER_DNS`
  - `MXO_MARGIN_SERVER_SUFFIX`
  - `MXO_MARGIN_SERVER_PORT`
  - `MXO_MARGIN_ROUTE_PREFIX`

Current verified auth result on the binder/stub init-success path:
- `CLTLoginMediator::BeginAuthConnection() authHost='auth.lith.thematrixonline.net' port=11000 -> 0x00000001`

Current limitation from the same implementation pass:
- margin connect is still unresolved because the current scaffold only knows the recovered suffix/port pair and not yet the faithful exact margin-host derivation
- current guessed `vector.lith.thematrixonline.net:10000` attempt still returns `0x00000000`
- but a newer exact-host override check proves the raw socket/connect path itself is not the blocker there:
  - with `MXO_MARGIN_SERVER_DNS=auth.lith.thematrixonline.net`
  - the margin-side connect path also returns `0x00000001`
- so the remaining margin gap is now best described as **host derivation fidelity**, not basic TCP capability

So the implementation frontier has now moved from “can the replacement launcher name the config and owner paths?” to:
- auth connect = **yes, concretely possible now**
- margin connect = socket-capable too with an exact host override, but still blocked on faithful host derivation
- client-visible queue/work integration = no longer completely blocked, but still only partially scaffolded

Newer implementation milestone after the real auth connect work:
- the replacement launcher can now deliberately enqueue queue0C `(workItem, context)` pairs using raw diagnostic stubs shaped to match the client consumer expectations enough for live consumption
- on deliberate binder/scaffold `RunClientDLL` runs, current runtime evidence now shows:
  - queue0C cursor divergence before first consume (`current0 != current1`)
  - later queue cursor convergence after consume
  - client-side callback into a raw `context->+0x10(workItem)` surrogate (`OnOperationCompleted`-style scaffold)
  - later queued work-item release through the work-item vtable `+0x04` surrogate
- this is still explicitly scaffold/diagnostic behavior, not a claim that the full original producer semantics are reconstructed yet
- but it is now a concrete step past the old purely empty queue0C runtime loop

## Newly confirmed server-config string surfaces

Fresh string/xref review now identifies the launcher/client-side server-config names around this network family more concretely.

### launcher.exe

String-backed names found:
- `qsAuthServerDNSName` at `0x4b617e`
- `AuthServerPort` at `0x4b61b0`
- `MarginServerDNSSuffix` at `0x4b1974`
- `MarginServerPort` at `0x4b19a6`

Current initializer-style xrefs recovered:
- `0x49f660`
  - pushes default `0x2af8`
  - pushes string `AuthServerPort`
- `0x49c670`
  - pushes default `0x2710`
  - pushes string `MarginServerPort`
- `0x49f660` / `0x49c640`
  - also register the paired DNS/suffix names above through the same config/registration helper family

So the current best launcher-side reading is:
- the launcher clearly knows about an auth-server DNS name + port pair
- and a margin-server DNS suffix + port pair
- with current recovered default numeric seeds:
  - `AuthServerPort = 0x2af8 = 11000`
  - `MarginServerPort = 0x2710 = 10000`

### client.dll

String-backed names found:
- `AuthServerDNSName` at `0x628cf160`
- `AuthServerPort` at `0x628cf190`
- `MarginServerDNSSuffix` at `0x628cf2e8`
- `MarginServerPort` at `0x628cf31a`

Recovered initializer-style xrefs:
- `0x6281d620` registers `AuthServerDNSName`
- `0x6281d650` registers `AuthServerPort` with default `0x2af8`
- `0x6281dd80` registers `MarginServerDNSSuffix`
- `0x6281ddb0` registers `MarginServerPort` with default `0x2710`

Important naming nuance:
- in `client.dll`, the auth-side string is the direct name **`AuthServerDNSName`**
- in `launcher.exe`, the closest currently recovered auth-side launcher string is **`qsAuthServerDNSName`** rather than the exact client spelling
- for the margin side, both binaries currently show **`MarginServerDNSSuffix`**, not an exact direct string `MarginServerDNSName`

That does not prove the runtime never builds a fuller margin host name elsewhere.
But it does narrow the currently recovered config surface to:
- a direct auth-server DNS name
- a margin-server DNS suffix
- and their paired ports

Important limitation:
- those new source files are still **not** a faithful full arg5 runtime implementation inside the launcher scaffold
- but they are no longer completely disconnected placeholders either:
  - the current diagnostics scaffold now incrementally delegates slot `1` / `MonitorPort`, slot `2` / `UDPMonitorPort`, slot `6` / `Connect`, slot `7` / `Close`, slot `8` / `SendBuffer`, and slot `12` / `CleanupConnection` into the new `src/liblttcp/` classes through a diagnostic sidecar engine/connection binding
  - `Connect`, `Close`, and `SendBuffer` are now routed through sidecar `CMessageConnection` wrappers (`EnsureConnected()` / `CloseConnection()` / `SendPacket(...)`) instead of keeping those connection-oriented paths entirely inside `diagnostics.cpp`
  - sidecar `CMessageConnection` ownership/lookup/drop is now also managed by `CLTThreadPerClientTCPEngine` itself rather than by a diagnostics-local connection table
  - current diagnostic list-head emptiness for arg5 `+0x80` / `+0x8c` is also synchronized from that sidecar engine state so later stub logs track the new class-backed state more directly
- they are therefore now best treated as **partially wired starter structure**, still far from faithful semantics but no longer only dormant future placeholders

New practical rerun result after that partial wiring:
- a fresh deliberate binder/scaffold `RunClientDLL` rerun was made after wiring slots `1/2/6/7/8/12` into the `src/liblttcp/` sidecar engine/connection classes
- that rerun still showed only:
  - mediator `+0x2c`
  - arg5 helper `+0x60` slot `0`
  - arg5 helper `+0x60` slot `1`
- it still did **not** show any new primary-slot traffic from:
  - slot `6` / `Connect`
  - slot `7` / `Close`
  - slot `8` / `SendBuffer`
  - slot `12` / `CleanupConnection`
- queue0C / queue34 also remained in the same empty-cursor state on that rerun

So the new class wiring is currently best treated as implementation cleanup / groundwork, not as proof that those arg5 paths are live on the present runtime branch yet.
