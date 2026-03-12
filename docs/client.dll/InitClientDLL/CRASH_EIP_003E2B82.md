# Crash family: `EIP=0x003e2b82`

## Why this document exists

This is the current canonical note for the newest late-startup crash family where the faulting `EIP` lands inside launcher-owned `arg2 filteredArgv` storage.

Even though nearby runs may land at slightly different heap addresses (`0x003e2b62`, `0x003e2b82`, etc.), the important stable pattern is the same:

- the client reaches the deeper mediator `+0xec` handoff,
- and control later redirects to **current `arg2 filteredArgv + 2`**.

## Representative dumps

Primary latest representative:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_35.dmp`
  - `EIP=0x003e2b82`
  - current `arg2 filteredArgv = 0x003e2b80`

Closely related sibling dumps in the same narrowed family:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_34.dmp`
  - same `Vector`-selection run family
  - `EIP=0x003e2b82`
  - current `arg2 filteredArgv = 0x003e2b80`
- `~/MxO_7.6005/MatrixOnline_0.0_crash_33.dmp`
  - same scaffold path without `MXO_MEDIATOR_SELECTION_NAME=Vector`
  - `EIP=0x003e2b62`
  - current `arg2 filteredArgv = 0x003e2b60`

Older related family members with the same higher-level behavior:
- `crash_17` / `EIP=arg2+2`
- `crash_27` / `crash_28` / `EIP=0x003e2b62`
- `crash_32` / `EIP=0x003e2b5a` with current `arg2=0x003e2b58`
- `crash_54` / `EIP=0x003e5e4a` with current `arg2=0x003e5e48`
  - after widening arg5 to the full recovered 13-slot primary vtable surface
  - still no observed new arg5 slot traffic before failure in `resurrections.log`

## Run conditions

Representative latest run:
- build/run target: `cd /home/morgan/mxo/code/matrix_launcher && MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
- active runtime binary: patched `~/MxO_7.6005/client.dll` importing `dbghelp.dll`
- launcher path: binder mediator scaffold + launcher object scaffold + forced incomplete-init experiment
- launcher auth args forwarded only through launcher-owned filtered argv duplication

Important current environment note:
- the original `~/MxO_7.6005/launcher.exe` can log into the live game on this machine, so this crash family is still more consistent with launcher-state mismatch than with a generic host GPU/audio/network failure.

## What is reached before the crash

Stable logged path in this family:
- `MediatorStub::GetWorldOrSelectionName()`
- `MediatorStub::GetProfileOrSessionName()`
- `MediatorStub::AttachStartupContext(first)`
- `MediatorStub::ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
- repeated `MediatorStub::GetSelectionDescriptor(...)`
- `MediatorStub::ConsumeSelectionContext(...)` at `+0xec`

On current real-user runs this corresponds to the game progressing far enough to show the deep startup / loading-character stage before crashing.

## Static anchor

The deepest confirmed mediator handoff on this path is now well anchored in client code:

- `client.dll:0x62170e2a..0x62170f48`
  - builds a stack object at `[ebp-0xbc]`
  - then passes it to `arg6->+0xec`
- `client.dll:0x6211d3e0`
  - zero-initializes that object through offset `+0xb0`
  - fixes the handoff size at **`0xb4` bytes**

Related path-building helpers in the same family:
- `client.dll:0x62195f00`
  - formats `Profiles\%s\`
- `client.dll:0x62195ff0`
  - formats `Profiles\%s\%s_%X\`
- later helper cases generate names such as:
  - `hl.cfg`
  - `an.cfg`
  - `pi.cfg`
  - `ai.cfg`
  - `cs.cfg`
  - `bl.cfg`
  - `il.cfg`
  - `rl.cfg`
  - `cl.cfg`
  - `mcd.cfg`

## Newer narrowing results

### 1. Persisting the `+0xec` object did not move the crash

The replacement launcher now copies the full `0xb4` selection/config object into stable mediator-owned storage when `+0xec` is called.

Result:
- the client still later crashes in the same `arg2+2` family.

Interpretation:
- the current late crash is **not** explained solely by the old theory that the mediator was retaining only a raw pointer to a dead stack object.

### 2. Correcting the obvious `+0x38` path-root meaning did not move the crash either

`+0x38` is now treated as a profile-root string input for the client's `Profiles\%s\...` formatting path.
The scaffold now returns the profile/session-style name (`morgan`) there.

Observable side effect:
- newer runs now create `~/MxO_7.6005/Profiles/morgan/aui.cfg`
- older runs had only `~/MxO_7.6005/Profiles/resurrections/aui.cfg`

Result:
- the crash still remained at `arg2+2` (`crash_35`).

Interpretation:
- fixing the obvious profile-root string mismatch alone is still insufficient.

### 3. A manual `ret` can step past the immediate `arg2` landing, but only as a hack

A newer interactive debugging result is that the immediate late crash can be bypassed temporarily by forcing the bad landing site in current `arg2` storage to behave like a single x86 `ret` (`0xc3`).

Interpretation:
- this is consistent with the current model that control flow is already corrupted **before** execution lands in `arg2`
- the `ret` does not fix the earlier corruption; it only pops one more return target and lets debugging continue farther
- so this is useful only as a **diagnostic control-flow bypass**, not as a faithful launcher behavior

Important caution:
- do **not** treat this as evidence that `arg2` should be executable
- do **not** treat it as a fix for this crash family
- do **not** build it into the intended launcher path

Canonical note for this hack:
- `RET_BYPASS_HACK.md`

### 4. Widening arg5 to the full recovered 13-slot primary vtable surface still did not move the crash

A newer arg5 reconstruction pass widened the launcher-object scaffold to expose the full recovered primary vtable at `0x4b2768`, including newly modeled slots `5..10` and a more evidence-backed empty-path result for slot `5`.

Follow-up rerun result:
- `make run_binder_both` still failed in the same late family
- latest representative dump from that rerun:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_54.dmp`
  - `EIP=0x003e5e4a`
  - current `arg2 filteredArgv = 0x003e5e48`
- `resurrections.log` still showed no new observed arg5 traffic before failure from:
  - primary slots `5..10`
  - primary slots `11..12`
  - helper surfaces at `+0x5c / +0x60 / +0x98`

Interpretation:
- widening the known arg5 vtable surface was the right faithfulness direction,
- but this rerun still argues that the present late `arg2+2` crash is **not** explained solely by the absence of those currently recovered arg5 entrypoints.

### 5. The in-launcher `ret` bypass still did not reveal later arg5 traffic

The diagnostic scaffold now has an opt-in `MXO_ARG2_RET_BYPASS=1` path that simulates a single x86 `ret` when faulting `EIP` lands inside current `arg2 filteredArgv` storage.

Representative validation run:
- `cd /home/morgan/mxo/code/matrix_launcher && MXO_ARG2_RET_BYPASS=1 MXO_ARG2_RET_BYPASS_MAX=4 make run_binder_both`

Observed result:
- first late fault still lands at `arg2+2`
  - `EIP=0x003e5e82`
  - current `arg2=0x003e5e80`
- the bypass then pops the current stack top and resumes at:
  - `0x62000000` (`arg3 hClientDll`)
- execution immediately faults again at:
  - `client.dll+0x3` / `EIP=0x62000003`
  - representative dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_57.dmp`
- the stack at that second fault still begins with stale startup-frame values:
  - `arg5 = 0x003e71c8`
  - `arg6 = 0x0041bb60`
- and `resurrections.log` still shows **no** new `LauncherObjectStub::Slot...` traffic before that second crash

Interpretation:
- on the current path, the `ret` bypass does not uncover a hidden valid continuation into deeper client work
- it only demonstrates that the corrupted return chain is falling into stale startup-frame values
- so this still does **not** provide runtime validation that the current arg5 method surface is being exercised after the visible `arg2` crash

### 6. Preserving the intended `InitClientDLL` frame now confirms the stale return chain more directly

The launcher now preserves the intended 8-argument `InitClientDLL` frame before the call and compares it against the crash-time stack in the exception logger.

Representative late-crash result:
- current top-of-stack words at the `arg2+2` crash now directly match the preserved startup-frame values in order:
  - `esp[0] = arg3 hClientDll`
  - `esp[1] = arg4 hCresDll`
  - `esp[2] = arg5 launcherNetworkObject`
  - `esp[3] = arg6 ILTLoginMediatorDefault`
- in a non-zero arg7 run, `esp[4]` also matched preserved `arg7 packedArg7Selection`

Interpretation:
- this materially strengthens the current model that the bad control flow is collapsing into stale `InitClientDLL` startup-frame data,
- not exposing some otherwise-valid later continuation.

### 7. A non-zero arg7 probe exposed, and then partially explained, the `+0x40` request shape

Representative run:
- `MXO_ARG7_SELECTION=0x0500002a`
- `MXO_MEDIATOR_SELECTION_NAME=Vector`
- `make run_binder_both`
- representative dumps from that run family:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_59.dmp`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_60.dmp`
  - both still land at `EIP=0x003e5e8a`

Observed launcher-side sibling-slot phase still succeeded:
- `+0xfc(worldIndex=0x2a)` -> `"Vector"`
- `+0x100(worldIndex=0x2a)` -> `1`
- `+0xe4(variantIndex=0x05)` -> `0`
- launcher-side arg7 rebuild remained:
  - `a8=0x00000005`
  - `ac=0x0000002a`
  - `packed=0x0500002a`

Later client-side `+0x40` requests on that same run arrived as:
- `selectionIndex=0x05000005`

A fresh static pass now explains that shape.
Inside `client.dll:0x62170dc1..0x62170e59`, the client reuses the original arg7 stack slot as scratch storage and does:
- `and esi, 0x00ffffff`
- `shr ebx, 0x18`
- `mov [ebp+0x14], bl`
- later `mov esi, [ebp+0x14]`

So with arg7 `0x0500002a`, the client itself mutates that stack slot into `0x05000005` before later path-building helpers call `arg6->+0x40`.

Important neighboring detail from the same block:
- before mutating the arg7 stack slot, the client also stores the original masked low-24-bit selection id separately via `push esi ; mov ecx, 0x629e1c7c ; call 0x620011e0`
- so the current best interpretation is that the client expects both:
  - a stable persisted low-24-bit selection id somewhere else, and
  - the later scratch-shaped `+0x40` request key

The replacement launcher now accepts that scratch-shaped `+0x40` request diagnostically and returns the configured descriptor with:
- `mappedName='Vector'`
- `packedSelectionId=0x00002a`
- `matchMode=arg7-scratch-shape`

Related new static clarification:
- inside `client.dll:0x62195ff0`, the `%s_%X` profile-path formatter calls `arg6->+0x40(selectionIndex)` and then reads descriptor payload fields at:
  - `+3` -> name pointer
  - `+7` -> selection id
- and inside `client.dll:0x62170de2..0x62170e3b`, the first dword of the later `+0xec` handoff object is populated from the zero-extended arg7 high-8-bit variant value
  - which matches the newer non-zero run where copied `selectionContext[0] = 0x00000005`

But practical result:
- this still did **not** move the late crash family
- crash remained at the same `arg2+2` signature (`crash_60`)
- a follow-up rerun after adding explicit `selectionContext[0]` logging still remained in the same family:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_61.dmp`
  - `EIP=0x003e5e8a`

Interpretation:
- the `+0x40` request format is now **partially explained**,
- and the early `+0xec` object field layout is slightly less opaque,
- but the remaining mismatch is still deeper than merely rejecting that request shape.

## Current best interpretation

This family now most likely means:
- the client gets past the deeper `+0xec` mediator handoff,
- but something still goes wrong before later `+0xf4`-style mediator accessor traffic becomes visible from this scaffold family,
- and the resulting bad control flow still tracks launcher-owned `arg2` storage.

The most plausible remaining causes are now:
1. still-incomplete arg7 low-24-bit / selection-id state,
2. another launcher-owned config/state expectation inside the same `0x62170b00` / `0x622a39d0` family,
3. or a later ownership/signature mismatch that still poisons control flow with the current `arg2` pointer.

## What this crash family currently rules out

It materially weakens these narrower explanations:
- "the next problem is only another missing simple mediator slot"
- "the crash is only because `+0xec` retained a raw dead stack pointer"
- "the crash is only because `+0x38` returned the wrong obvious root string"

## Next focus

Priority next work from this crash family:
1. keep reconstructing arg7 from the original launcher-owned selection/service chain near `0x4d3584`
2. explain how the client expects the new scratch-shaped `+0x40` request (`0x62170dc1..0x62170e59`) to map back to persisted low-24-bit selection id / descriptor state, rather than assuming that merely accepting the request shape is enough
3. keep documentation keyed to this late `arg2+2` crash family so future nearby heap addresses do not create duplicate analysis notes
