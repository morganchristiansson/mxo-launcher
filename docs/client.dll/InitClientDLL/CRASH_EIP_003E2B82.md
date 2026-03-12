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
2. carry the original arg7 low-24-bit selection value through the mediator selection-descriptor path instead of defaulting to `0`
3. keep documentation keyed to this late `arg2+2` crash family so future nearby heap addresses do not create duplicate analysis notes
