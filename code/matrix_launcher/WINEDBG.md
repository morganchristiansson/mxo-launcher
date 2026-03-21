# winedbg guide for Matrix launcher work

This is the **current practical workflow** for debugging the original Matrix launcher path under Wine.

Keep it operational.
Do not let this file become a long-running history log.

## Scope

Use this guide for:
- attaching to the original launcher-spawned temp `matrix.exe`
- quick crash-dump triage
- narrow live breakpoint passes on the launcher-owned state8/state9 path

It is **not** a general Windows or WineDbg tutorial.

## Core rule: debug the spawned temp `matrix.exe`

For the original launcher path, the process you usually want is **not** `launcher.exe` itself.
The original launcher copies itself into Wine temp and runs as a temp `matrix.exe`.

Normal launch:

```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_USER=morgan MXO_PASS='<pwd>' MXO_CHAR=Morg4n make run_original_launcher
```

That uses the real launcher with:
- `-noeula`
- `-nopatch`
- optional `-char`
- `-user`
- `-pwd`

Even then, the debugger target is the spawned temp `matrix.exe`.

## Always use the Wine PID

Do **not** attach using the Linux host PID from `ps`.
Use the Wine internal PID from `wine tasklist`.

Useful commands:

```bash
cd /home/morgan/MxO_7.6005
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

attach with:

```text
winedbg 300
```

not the Linux PID.

## Current attach discipline

### Best collaborative workflow

When working together interactively:

1. launch with `make run_original_launcher`
2. wait until the spawned `matrix.exe` is at a **stable UI point**
   - patch notes / Continue button
   - other visibly settled launcher UI
3. attach to the spawned `matrix.exe`
4. set the smallest breakpoint set possible
5. `cont`
6. only after breakpoints are armed and the debugger has continued, click the UI button that advances the launcher

### Important timing caution

Recent live runs showed that attach timing can be **fragile**.
If you attach too early in the temp `matrix.exe` startup window, you may see weird startup failures like:
- `DLL initialisation failed`
- `client.dll could not be found`
- patch-system-looking behavior even though `-nopatch` was used

Do **not** over-interpret those as normal launcher behavior.
If that happens:
- relaunch cleanly
- attach later, at a stable UI point
- keep the breakpoint set minimal

### Continue-button coordination rule

If the launcher is waiting on a **Continue** button:
- do **not** click it before the debugger pass is armed
- wait for explicit confirmation that:
  - attach succeeded
  - breakpoints are set
  - debugger has already `cont`'d

## MCP / wrapper cautions

The current `mcp-winedbg` wrapper is good enough for breakpoint work, but it has quirks:
- `cont` may succeed even when the MCP request looks hung
- output from one request can bleed into the next response
- inspection commands can auto-interrupt the target
- a stop rooted in `ntdll!DbgUiRemoteBreakin` is often just debugger churn, not a real game-side hit

Practical rule:
- prefer small breakpoint sets
- prefer tiny captures first:
  - `print $eip`
  - `print $ecx`
  - one or two more values
- only use heavier `bt` / `info ...` when the stop is clearly worth it

## Do not run temp `matrix.exe` directly

Do **not** treat direct temp-`matrix.exe` execution as the main workflow.
Running the copied temp `matrix.exe` directly can hit the deprecation check:

```text
Please run the game by using launcher.exe.
Running matrix.exe directly or using -clone is deprecated.
```

So the faithful and practical path remains:
- run `launcher.exe`
- attach to spawned `matrix.exe`

## Do not kill `matrix.exe` after a pass

Prefer **detach** over kill.

Killing `matrix.exe` does not gracefully log out and can leave the account/session stuck until the server expires it.
That blocks quick relogin.

Practical rule:
- when a debug pass is done, `detach`
- let the game exit normally or close it intentionally outside the debugger

## Crash-dump fast path

```bash
cd /home/morgan/mxo/code/matrix_launcher
make crashdump
```

Direct equivalent:

```bash
cd /home/morgan/MxO_7.6005
printf 'info reg\nbt\ninfo share\nquit\n' | winedbg MatrixOnline_0.0_crash_<n>.dmp
```

Good first checks:
- `info reg`
- `bt`
- `info share`

## Current high-value launcher addresses

### Active natural launcher branch
- `0x0041ecd0`
  - `CLTLoginMediator::ProcessLoginRequest`
- `0x0041c1f0`
  - `CLTLoginMediator_PersistSelectionContextAndSwitchToState8`
- `0x0043bd20`
  - `CLTLoginState_State8::Slot3_BeginOrContinue`
- `0x0043f930`
  - `CLTLoginState_State8::Slot6_HandleSecondaryMessage`
- `0x00439780`
  - `CLTLoginState_State9::Slot3_BeginOrContinue`
- `0x0041de40`
  - state9 submit followup
- `0x0043c180`
  - `CLTLoginState_State9::Slot6_HandleSecondaryMessage`

### Receive-side bridge immediately before `0x43f930`
- `0x004490c0`
  - `CMessageConnection_OnOperationCompleted`
- `0x004499a1`
  - late dispatch call site inside `CMessageConnection_OnOperationCompleted`
- `0x00442d00`
  - `CBaseMarginConnection_DispatchMessage`
- `0x0044af20`
  - `CMarginConnection_DispatchMessage`
- `0x0041f260`
  - `CLTLoginMediator_DispatchCurrentHelperSlot6`

### Newly proven owner `+0xf18` writer
- `0x00440780`
  - `CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply`
  - on opcode-`9` success writes:
    - owner byte `+0xf14 = 1`
    - owner dword `+0xf18 = parsedReply(+0x09)`

## Current useful breakpoint sets

### Minimal natural state8/state9 branch set

```text
break *0x0041ecd0
break *0x0041c1f0
break *0x0043bd20
break *0x0043f930
cont
```

Use this when you need to confirm the active password-submit branch and the state8 send/reply pair.

### Later state9 continuation set

```text
break *0x00439780
break *0x0041de40
break *0x0043c180
cont
```

Use this once the state8 path is already proven and you want the later submit/reply continuation.

### Post-state9 success tail

```text
break *0x0041b450
break *0x0041cfb0
cont
```

Use this after `0x0043c180` success is already proven.

### Receive-side bridge before state8 slot 6

```text
break *0x004490c0
break *0x004499a1
break *0x00442d00
break *0x0043f930
cont
```

Use this only for the narrow question of what survives into state8 slot 6.

### Owner `+0xf18` watch syntax

If you need to watch the field directly, use a **typed** expression:

```text
watch *(int*)0x4d5d50
```

Do **not** rely on a raw address form.

Important current limitation from live runs:
- for this specific field, WineDbg watchpoints tended to trip **late** with the already-materialized value
- they did **not** reliably expose the true earlier producer
- so use them as a sanity check, not as the primary proof technique

## Current proven runtime bracket for owner `+0xf18`

Natural launcher runs already proved:
- still `0` at:
  - `0x0041ecd0`
  - `0x0041c1f0`
  - `0x0043bd20`
  - `0x00442d00`
  - `0x004499a1`
- already non-zero by:
  - `0x0043f930`
  - then still non-zero through `0x00439780 -> 0x0041de40 -> 0x0041e690`

This is now explained by the state6 slot-6 writer at `0x00440780`.

## Current practical recommendation

For live original-launcher work:
- keep breakpoint sets small
- attach only once the UI is stable enough to tolerate a stop
- use `matrix.exe`, not `launcher.exe`
- detach, do not kill
- if attach timing starts causing weird startup failures, relaunch and attach later rather than assuming the launcher logic changed

## See also

- `README.md`
- `startup_objects/README.md`
- `../client.dll/InitClientDLL/README.md`
- `../client.dll/InitClientDLL/CRASH_EIP_003E2B82.md`
- `../client.dll/InitClientDLL/RET_BYPASS_HACK.md`
