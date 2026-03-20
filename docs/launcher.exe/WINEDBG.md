# winedbg guide for launcher/client startup work

This note collects the current practical `winedbg` workflow for tracing the Matrix Online launcher startup path under Wine.

Use it for two related jobs:
- quick crash-dump triage
- live stepping around `launcher.exe` / `resurrections.exe` and `client.dll!InitClientDLL`

## Scope

This guide is intentionally biased toward the current active investigation:
- launcher-owned startup reconstruction
- `client.dll!InitClientDLL`
- the original `launcher.exe` / temp-copied `matrix.exe` login-state path under Wine
- active auth/login-state progression into state-8 / margin work
- the late crash family that lands in current `arg2 filteredArgv + 2`

It is **not** a generic Windows debugging tutorial.

## New practical note: original launcher copies itself to `matrix.exe`

For the original launcher path, the process you usually want is **not** `launcher.exe` itself.
The launcher copies itself into a temp folder and runs as:

- `C:\users\morgan\Temp\MatrixOnline.0\matrix.exe -clone`

That means two practical rules matter a lot:

1. use the **Wine internal PID**, not the Linux host PID, when attaching with `winedbg`
2. verify the target with `wine tasklist`, not only `ps`

Useful commands:

```bash
wine tasklist /v
wine cmd /c tasklist /FI "IMAGENAME eq matrix.exe"
```

If `ps` shows something like:

```text
C:\users\morgan\Temp\MatrixOnline.0\matrix.exe -clone
```

and `wine tasklist` shows:

```text
matrix.exe   300
```

then attach with:

```text
winedbg 300
```

not with the Linux PID from `ps`.

## MCP / wrapper cautions from live runs

The current `mcp-winedbg` wrapper is good enough for breakpoints, but has some important quirks:

- `cont` may succeed even when the MCP call looks like it hangs
- inspection commands like `bt`, `print`, `info ...` can auto-interrupt the target
- that can create a **fake stop** in:
  - `ntdll!DbgUiRemoteBreakin`
- after attach + breakpoint setup, one initial `cont` is usually needed just to unfreeze the target

So for live collaborative runs, prefer this discipline:

1. attach
2. set the smallest possible breakpoint set
3. `cont` once
4. if the UI freezes, check whether it is just debugger stop state before assuming a real hit
5. only use `bt` / `print` after a confirmed useful stop

A backtrace rooted in:

```text
ntdll!DbgUiRemoteBreakin
```

usually means the debugger tooling interrupted the process; it is **not** evidence of a real game-side branch hit.

## Current high-value addresses

These are the most useful current anchors.

### `resurrections.exe`
- `main` = `0x0040f340`

### `client.dll`
- `InitClientDLL` export = `0x620012a0`
- current arg7/mediator helper call = `0x620015f8 -> 0x62170b00`
- helper return check = `0x620015fd`
- helper failure return = `0x62001629..0x62001633`
- helper success return = `0x62001634..0x6200163c`
- confirmed `+0xec` call site = `0x62170f48`
- post-`+0xec` continuation = `0x62170f4e -> 0x62170f62`

### Current crash family
- representative late crash = `EIP=0x003e5e8a`
- stable higher-level signature = control later redirects into current `arg2 filteredArgv + 2`

### Original launcher / `matrix.exe` active login-state branch

These are the current highest-value original-launcher anchors for active-path work:

- `0x0041ecd0`
  - `CLTLoginMediator::ProcessLoginRequest`
  - confirmed live on password submit
  - current observed input shape:
    - `param + 0x00` = username
    - `param + 0x20` = password
- `0x0041c1f0`
  - `CLTLoginMediator_PersistSelectionContextAndSwitchToState8`
  - confirmed live after the password-submit branch
- `0x00439300`
  - `CLTLoginState_State4::Slot3_BeginOrContinue`
  - state-4 margin-route dispatch body
- `0x0043bd20`
  - `CLTLoginState_State8::Slot3_BeginOrContinue`
  - active state-8 structured margin send
- `0x0043f930`
  - `CLTLoginState_State8::Slot6_HandleSecondaryMessage`
  - active state-8 reply body

Important current interpretation:
- the original live password path is now better evidenced as:
  - `0x41ecd0 -> 0x41c1f0 -> state8-side continuation`
- not as an immediate helper11 / `0x41c3c0` path
- newer live runs narrow that boundary further:
  - natural original stops reached `0x43bd20`
  - then the process died before any natural stop at `0x43f930`
- so when debugging the original launcher, **start with the state-8 branch first** and treat the
  `0x43bd20 -> 0x43f930` gap as the current highest-value live boundary before trying to force
  helper11-specific hypotheses

## Fastest path: inspect the latest dump

From the project directory:

```bash
cd /home/morgan/mxo/code/matrix_launcher
make crashdump
```

That currently expands to a simple `winedbg` dump workflow against the newest `~/MxO_7.6005/MatrixOnline_0.0_crash_*.dmp`.

Direct equivalent:

```bash
cd /home/morgan/MxO_7.6005
printf 'info reg\nbt\ninfo share\nquit\n' | winedbg MatrixOnline_0.0_crash_62.dmp
```

Good first checks in dump triage:
- `info reg`
- `bt`
- `info share`
- top of stack versus the preserved `InitClientDLL` frame in `resurrections.log`

## Live-run baseline for the current deep startup path

For the current reproduced loading-character / `+0xec` path, this is the most useful live configuration:

```bash
cd /home/morgan/MxO_7.6005
MXO_BINDER_LOGIN_MEDIATOR=1 \
MXO_STUB_LAUNCHER_OBJECT=1 \
MXO_FORCE_INCOMPLETE_INIT=1 \
MXO_ARG7_SELECTION=0x0500002a \
MXO_MEDIATOR_SELECTION_NAME=Reality \
winedbg ./resurrections.exe
```

Useful optional diagnostic toggles:
- `MXO_ARG2_RET_BYPASS=1`
- `MXO_ARG2_RET_BYPASS_MAX=4`

But keep that bypass strictly diagnostic-only; see:
- `../client.dll/InitClientDLL/RET_BYPASS_HACK.md`

## Breakpoint strategy that works well here

### Active original-launcher breakpoint set

For the current original `matrix.exe -clone` login-state work, this is the most useful **minimal** breakpoint set:

```text
break *0x0041ecd0
break *0x0041c1f0
break *0x00439300
break *0x0043bd20
break *0x0043f930
cont
```

Practical use:
- `0x0041ecd0` is chatty but confirms password-submit entry into the active branch
- after confirming that once, it is often worth deleting that breakpoint so the UI can progress:

```text
delete 1
cont
```

- `0x0041c1f0` is the highest-value next stop because it confirms the active branch is persisting
  the `0xb4` selection/config object and switching to state `8`
- `0x00439300`, `0x0043bd20`, and `0x0043f930` are the next state-4/state-8 continuation anchors

### 1. Break in the custom launcher first

Because `resurrections.exe` has usable DWARF symbols, the easiest first anchor is:

```text
break main
cont
```

Current known good symbol:
- `main` resolves at `0x0040f340`

### 2. Prefer absolute client addresses after `client.dll` is loaded

In current sessions, `client.dll` export-name breakpoints have been unreliable enough that absolute addresses are usually faster once the module is present.

After `client.dll` is loaded, useful breakpoints are:

```text
break *0x620015fd
break *0x62001634
break *0x6200163c
break *0x62170f48
break *0x62170f4e
```

Practical meaning:
- `0x620015fd`: did `0x62170b00` return success?
- `0x62001634`: success epilogue of the enclosing `InitClientDLL` path
- `0x6200163c`: exact `ret` that should leave that path cleanly
- `0x62170f48`: confirmed mediator `+0xec` handoff
- `0x62170f4e`: immediate post-`+0xec` continuation

### 3. If you need deferred breakpoints by address

If you try `break *0x620015fd` too early, before `client.dll` is loaded, `winedbg` will reject it.

The debugger itself reports the workaround:
- enable deferred breakpoints by address with `$CanDeferOnBPByAddr = 1`

So in a fully interactive `winedbg` session, if you want to queue client absolute breakpoints before module load, set that option first.

If you do **not** want to rely on that, use this simpler workflow instead:
1. `break main`
2. `cont`
3. once startup has advanced and `client.dll` is loaded, add the absolute client breakpoints
4. `cont` again

### 4. Verify load state before assuming an address is valid

Use:

```text
info share
```

On the current path, `client.dll` should appear at base `0x62000000`.

## Current preferred live trace target

For original `matrix.exe` login-state work, the highest-value live question is now the **state8 post-send boundary**, not helper11-first speculation.

Current best target sequence:
- `0x41ecd0`
- `0x41c1f0`
- `0x43bd20`
- hoped-for next natural stop: `0x43f930`

Why this is the current priority:
- newer original-launcher runs repeatedly confirmed the password-submit branch through
  `0x41c1f0` and into `0x43bd20`
- those same runs then terminated before any natural hit on `0x43f930`
- they also did **not** naturally hit `0x41c3c0`, `0x421220`, `0x420ef0`, or `0x43c020`

Practical consequence:
- if you are choosing between chasing helper11 writers and chasing the natural original branch,
  prefer the `0x43bd20 -> 0x43f930` gap first
- helper11 remains relevant for the forced scaffold runtime stall, but it is not yet the first
  faithful original-live breakpoint target

## Minimal live workflow for that question

1. Start under `winedbg`
2. `break main`
3. `cont`
4. after the process is far enough along that `client.dll` is loaded, set:
   - `break *0x620015fd`
   - `break *0x62001634`
   - `break *0x6200163c`
5. `cont`
6. at `0x620015fd`:
   - inspect `al`
   - inspect stack top
   - confirm whether the helper returned success
7. if execution reaches `0x62001634`, single-step the epilogue
8. watch whether `ret` returns to launcher-owned code or collapses into stale startup-frame data

## Useful interactive commands

Common commands worth keeping handy:

```text
info reg
bt
info share
stepi
nexti
finish
frame 0
up
down
```

Useful memory checks:

```text
x/16wx $esp
x/16i $eip
```

For this project, the most informative quick inspection is often:
- current `eip`
- current `esp`
- first few dwords at `esp`
- whether those dwords match preserved `InitClientDLL` arguments in `resurrections.log`

## How this relates to project logging

`winedbg` is most useful here when paired with:
- `~/MxO_7.6005/resurrections.log`
- preserved `InitClientDLL` argument-frame logging from the custom launcher
- canonical crash-family docs under `../client.dll/InitClientDLL/`

The most useful cross-checks are:
- does the client reach `MediatorStub::ConsumeSelectionContext(+0xec)`?
- does the live stack at the late crash still match stale startup-frame values?
- does execution ever cleanly return past `0x6200163c`?

## Current cautions

- Do **not** treat the `arg2` landing as the root bug.
- Do **not** treat `MXO_ARG2_RET_BYPASS` as a fix.
- Do **not** jump straight to the later `0x62054cbd` / `+0x120` path until the enclosing `InitClientDLL` success-return path is accounted for.

## See also

- `README.md`
- `startup_objects/README.md`
- `../client.dll/InitClientDLL/README.md`
- `../client.dll/InitClientDLL/CRASH_EIP_003E2B82.md`
- `../client.dll/InitClientDLL/RET_BYPASS_HACK.md`
