# winedbg guide for launcher/client startup work

This note collects the current practical `winedbg` workflow for tracing the Matrix Online launcher startup path under Wine.

Use it for two related jobs:
- quick crash-dump triage
- live stepping around `launcher.exe` / `resurrections.exe` and `client.dll!InitClientDLL`

## Scope

This guide is intentionally biased toward the current active investigation:
- launcher-owned startup reconstruction
- `client.dll!InitClientDLL`
- the late crash family that lands in current `arg2 filteredArgv + 2`

It is **not** a generic Windows debugging tutorial.

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
MXO_MEDIATOR_SELECTION_NAME=Vector \
winedbg ./resurrections.exe
```

Useful optional diagnostic toggles:
- `MXO_ARG2_RET_BYPASS=1`
- `MXO_ARG2_RET_BYPASS_MAX=4`

But keep that bypass strictly diagnostic-only; see:
- `../client.dll/InitClientDLL/RET_BYPASS_HACK.md`

## Breakpoint strategy that works well here

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

Right now, the highest-value live question is **not** the later `0x62054cbd` / `+0x120` loading-character path.

The higher-value target is the enclosing `InitClientDLL` logic immediately after the already-confirmed successful helper return:
- `0x620015f8` calls `0x62170b00`
- `0x620015fd` tests `al`
- success path goes to `0x62001634`
- the path returns at `0x6200163c`

That matters because current static analysis now says:
- the already-reached `+0xec` helper itself performs no further mediator calls after `+0xec`
- it just continues through `0x62170f62`, calls `0x6216a1c0`, and returns success

So if the current crash still lands in `arg2+2`, the best immediate question is whether failure occurs:
- during the enclosing success epilogue
- at the final `ret`
- or immediately after that return target is consumed

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
