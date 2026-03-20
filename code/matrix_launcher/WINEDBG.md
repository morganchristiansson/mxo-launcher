# WineDbg MCP

Use this for live original-launcher / `matrix.exe -clone` tracing under Wine.

Canonical detailed doc:
- `/home/morgan/mxo/docs/launcher.exe/WINEDBG.md`

## Rules that matter

- attach to the **Wine internal PID**, not the Linux `ps` PID
- get that PID from:
  - `wine tasklist /v`
  - `wine cmd /c tasklist /FI "IMAGENAME eq matrix.exe"`
- the original launcher usually runs as:
  - `C:\users\morgan\Temp\MatrixOnline.0\matrix.exe -clone`
- after attach + breakpoint setup, do one `cont`
- if the UI freezes immediately after attach, it is often just debugger stop state
- do **not** trust a stop in `ntdll!DbgUiRemoteBreakin`; that is usually tooling-side interrupt noise
- the current MCP wrapper may make `cont` look stuck even when the target has resumed
- only ask for `bt` / `print` after a confirmed useful stop

## Current best original-launcher breakpoint set

Use the active state-8 branch first:

```text
break *0x0041ecd0
break *0x0041c1f0
break *0x00439300
break *0x0043bd20
break *0x0043f930
cont
```

Meaning:
- `0x0041ecd0` = `CLTLoginMediator::ProcessLoginRequest`
- `0x0041c1f0` = persist selection context / switch to state 8
- `0x00439300` = state4 margin dispatch
- `0x0043bd20` = state8 send
- `0x0043f930` = state8 reply

If `0x0041ecd0` is too chatty, delete it after the first confirmation.

## Current conclusion from live runs

The active original password-submit path is better evidenced as:

- `0x41ecd0 -> 0x41c1f0 -> state8-side continuation`

not as an immediate helper11 / `0x41c3c0` path.

So when RE and implementation disagree, reimplement the active state-8 path first.
