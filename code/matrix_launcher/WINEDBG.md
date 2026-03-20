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
The launcher copies itself into a temp folder and runs as a temp `matrix.exe` process.

Current practical launcher-side workflow now used in-session:

```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_USER=morgan MXO_PASS='<pwd>' MXO_CHAR=Morg4n make run_original_launcher
```

That target currently expands to a real-launcher run with:
- `-noeula`
- `-nopatch`
- optional `-char $(MXO_CHAR)`
- `-user $(MXO_USER)`
- `-pwd $(MXO_PASS)`

Even with those flags, you still attach to the **spawned temp `matrix.exe`**, not the original
`launcher.exe` process.

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
C:\users\morgan\Temp\MatrixOnline.1\matrix.exe -noeula -nopatch -char Morg4n -user morgan -pwd <pwd> -clone
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
- output from one MCP request can visibly bleed into the next request's response
- that can create a **fake stop** in:
  - `ntdll!DbgUiRemoteBreakin`
- after attach + breakpoint setup, one initial `cont` is usually needed just to unfreeze the target

So for live collaborative runs, prefer this discipline:

1. attach
2. set the smallest possible breakpoint set
3. `cont` once
4. if the UI freezes, check whether it is just debugger stop state before assuming a real hit
5. on a useful stop, prefer the smallest possible capture first
   - e.g. `print $eip`, `print $ecx`, maybe one or two more values, then `cont`
6. only use heavier `bt` / `print` / `info ...` once the stop is clearly worth the extra debugger churn

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
- `0x00439780`
  - `CLTLoginState_State9::Slot3_BeginOrContinue`
  - first natural helper9/state9 follow-on after successful state8 completion
- `0x0041de40`
  - owner/helper submit path behind state9 slot 3
- `0x0043c180`
  - `CLTLoginState_State9::Slot6_HandleSecondaryMessage`
  - later raw-`0x11` reply body
  - now also live-proven naturally
  - representative natural-success stop landed at `0x0043c1c2` with parsed status `0`

Important current interpretation:
- the original live password path is now better evidenced as:
  - `0x41ecd0 -> 0x41c1f0 -> state8-side continuation`
- not as an immediate helper11 / `0x41c3c0` path
- newer live runs now move that natural branch later again:
  - natural original reaches `0x43bd20`
  - crosses the `0x41af70/0x41cf30` send bridge
  - reaches `0x43f930`
  - naturally continues into `0x00439780`
  - and is now also live-proven one step deeper into `0x0041de40`
  - representative live stop sequence showed:
    - at `0x00439780`: helper9 local byte `this+4 = 0`, word `this+6 = 0x2710`
    - at `0x0041de40`: `ECX = 0x004d4e38`, `EAX = 0x2710`, `EDX = 0x004b517c`
- current remaining natural-original question is therefore later again:
  - what happens after the now-proven `0x0043c180` success-side state-`0x0c` switch / event-`0x18`
- representative live UI consequence at that boundary:
  - a natural run was visibly at **Waiting for Regionserver** while stopped around the
    `0x0043c180 / 0x0043c1c2` success-side window
- so when debugging the original launcher, **start with the state-8 branch first** and now treat
  deeper behavior under the state9 continuation (`0x41de40`, then `0x43c180`, then post-state9
  state-`0x0c` continuation) as the current highest-value live boundary before trying to force
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

For the current original launcher-spawned temp `matrix.exe` login-state work, this is the most useful **minimal** breakpoint set:

```text
break *0x0041ecd0
break *0x0041c1f0
break *0x0043bd20
break *0x0041af70
break *0x0041cf30
break *0x0043f930
cont
```

Practical use:
- `0x0041ecd0` is chatty but confirms password-submit entry into the active branch
- after confirming that once, it is often worth deleting that breakpoint so the UI can progress
- `0x0041c1f0` confirms the active branch is persisting the `0xb4` selection/config object and
  switching to state `8`
- `0x0043bd20` confirms state8 sender entry
- `0x0041af70` and `0x0041cf30` confirm the natural path crosses the send bridge, not just the
  packet-builder body
- `0x0043f930` is the now-proven later state8 reply stop and the best next deep inspection target

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

## Current practical original-launcher workflow

Use the Makefile helper so the launcher run is reproducible and fast to relaunch:

```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_USER=morgan MXO_PASS='<pwd>' MXO_CHAR=Morg4n make run_original_launcher
```

Then in a second shell, identify the spawned temp `matrix.exe` and attach `winedbg` to that Wine PID:

```bash
cd /home/morgan/MxO_7.6005
wine tasklist /v
```

Current late-breakpoint set for the active question is intentionally small:

```text
break *0x0041de40
break *0x0043c180
cont
```

Optional one-time sanity anchor on a fresh run:

```text
break *0x00439780
break *0x0041de40
break *0x0043c180
cont
```

Optional branch split once `0x0043c180` is proven:

```text
break *0x0043c1c2
break *0x0043c1e6
cont
```

Preferred immediate post-success narrowing once `0x43c180` is proven:

```text
break *0x0041b450
break *0x0041cfb0
cont
```

Optional later-continuation probes only after that helper-switch / event boundary is understood:

```text
break *0x004397e0
break *0x0041c5c0
cont
```

Current caution from the first follow-up probe pass:
- a natural run that already proved `0x43c180` success still produced **no** natural hit on
  `0x004397e0` or `0x0041c5c0`
- newer Ghidra review now explains why that non-hit is not surprising:
  - success first goes through `0x41b420`, then `0x41b450(0x0c)`, then `0x41cfb0(0x18)`
  - `0x41cfb0` walks the owner `+0x674` listener tree
- newer breakpoint-only live proof now tightens that one step further too:
  - after the proven `0x41cfb0(0x18)` post, the same natural run later hit `0x41cfb0(0x0f)`
  - the run then entered game without any observed hit on `0x004397e0` or `0x0041c5c0`
- so treat `0x004397e0` / `0x0041c5c0` as informed later probes, not as guaranteed immediate next
  stops after state `0x0c`

Why this set is better now:
- earlier state8 send-side hits have already been re-proven enough
- natural original is now live through `0x43f930 -> 0x439780 -> 0x41de40 -> 0x43c180`
- the next missing original-live question is no longer whether state9 slot 6 is reached, but what
  the later post-state9 / state-`0x0c` continuation does after the now-proven success-side branch
- newer Ghidra tightening now makes the first concrete post-success boundary:
  - `0x41b420 -> 0x41b450(0x0c) -> 0x41cfb0(0x18)`
  - and `0x41cfb0` delivers through the owner `+0x674` listener tree
- newer breakpoint-only live proof now also shows a later event post on that same continuation:
  - `0x41cfb0(0x0f)`
  - before the run enters game
- current practical live-status clue there is the visible **Waiting for Regionserver** phase
- current practical negative-result clue there is also that the first late probes on
  `0x004397e0` / `0x0041c5c0` stayed silent

## Current preferred live trace target

For original `matrix.exe` login-state work, the highest-value live question is no longer the old
**state8 post-send boundary**. Natural original is now confirmed later on the state8 reply path.

Current best proven target sequence:
- `0x41ecd0`
- `0x41c1f0`
- `0x43bd20`
- `0x41af70`
- `0x41cf30`
- `0x43f930`
- `0x439780`
- `0x41de40`
- `0x43c180`

Why this is the current priority:
- newer original-launcher runs confirmed the password-submit branch through
  `0x41c1f0`, `0x43bd20`, the send bridge, `0x43f930`, `0x439780`, `0x41de40`, and now `0x43c180`
- the state9 slot-6 natural hit also reached the success-side branch at `0x43c1c2`
- they still did **not** naturally hit `0x41c3c0`, `0x421220`, `0x420ef0`, or `0x43c020`

Practical consequence:
- if you are choosing between chasing helper11 writers and chasing the natural original branch,
  prefer the now-proven state9 continuation through `0x43c180` and then move later into the
  post-state9 / state-`0x0c` continuation
- the next tight original-launcher breakpoints should now center on the helper-switch / event tail
  behind that success:
  - `0x0041b450`
  - `0x0041cfb0`
- yes, there is also a later concrete character-data path:
  - `0x0041c3c0 = CLTLoginMediator_ProcessLoginCredentials`
  - `0x0043c020 = CLTLoginState_State11_SendPostAuthMarginPacket0x4d`
  - `0x0043e540` debug-printer confirms fields like SkinToneID, HairID, StartingHat,
    StartingShirt, StartingCoat, RealFirstName, RealLastName, Background, GameSessionID
- but keep that path secondary for the next natural-original pass unless the helper-switch / event
  tail itself proves it feeds there
- helper11 remains relevant for the forced scaffold runtime stall, but it is not the first
  faithful original-live breakpoint target

## Minimal live workflow for the current original-launcher question

For the active original `matrix.exe` branch, a better minimal live workflow is now:

1. attach to `matrix.exe`
2. set only the late continuation breakpoints:
   - `break *0x0041de40`
   - `break *0x0043c180`
3. optional on a fresh sanity run:
   - add `break *0x00439780`
4. `cont`
5. once `0x43c180` is confirmed on a natural run, optionally split the branch with:
   - `break *0x0043c1c2`
   - `break *0x0043c1e6`
6. on the next pass, prefer the immediate post-success tail:
   - `break *0x0041b450`
   - `break *0x0041cfb0`
7. only after that, reuse the later probes:
   - `break *0x004397e0`
   - `break *0x0041c5c0`
8. optional secondary sanity probes if the tail suggests helper11 re-entry later:
   - `break *0x0041c3c0`
   - `break *0x0043c020`
9. keep the next pass centered on the later post-state9 / state-`0x0c` continuation instead of
   repeatedly re-proving the earlier state8/state9 entries

Keep the older `client.dll` / `InitClientDLL` workflow below for client-startup questions, but it is
no longer the highest-value live path for the active original-launcher state8 investigation.

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
