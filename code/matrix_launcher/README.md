# Matrix Online Launcher

This project is a **reimplementation scaffold** for the original Matrix Online `launcher.exe`.

It is not a generic sample launcher.
Its purpose is to reproduce the original startup behavior closely enough to launch `client.dll` on the same path as the original binary.

## Source of Truth

- original binary: `~/MxO_7.6005/launcher.exe`
- active implementation: `src/resurrections.cpp` + `src/diagnostics.cpp`
- active executable name: `resurrections.exe`
- project notes: `AGENTS.md`
- canonical docs: `../../docs/`

## Current State

The current scaffold already:
- preloads support DLLs
- loads `cres.dll` before `client.dll`
- resolves the client exports used by original `launcher.exe`
- uses the correct original 8-argument `InitClientDLL` frame shape
- refuses incomplete startup by default

Current blocker:
- launcher-owned startup state is still incomplete
- faithful current result is `InitClientDLL = -7`

Diagnostic-only forced runtime can still reproduce the known crash:
- `client.dll+0x3b3573`
- fresh dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`

That forced crash is useful for analysis, but it is **not** original-equivalent behavior.

## Build

```bash
cd /home/pi/mxo/code/matrix_launcher
make
```

This builds the active launcher directly to:

- `~/MxO_7.6005/resurrections.exe`

## Run

Safe default run:

```bash
cd /home/pi/mxo/code/matrix_launcher
make run
```

Optional local credentials:

```make
# secrets/launcher_login.local.mk
MXO_USER := your-username
MXO_PASS := your-password
```

If that file exists, the Makefile appends those two values as argv to `resurrections.exe`.
The launcher currently treats them as launcher-only placeholder auth argv, strips them back out before `InitClientDLL`, and logs only whether they were provided, not the raw values.

Optional diagnostic arg7/arg8 overrides for post-`IsReady()` experiments:

```bash
MXO_ARG7_SELECTION=0x01000000 MXO_ARG8_FLAG=0 make run_stub_both
```

Optional mediator selection name override:

```bash
MXO_MEDIATOR_SELECTION_NAME=Vector make run_stub_both
```

Forced incomplete-init experiment:

```bash
make run_incomplete_init
```

Forced runtime after failed init (diagnostic only):

```bash
make run_forced_runtime
```

Arg6 stub experiment:

```bash
make run_stub_mediator
```

Combined arg5+arg6 diagnostic experiments:

```bash
make run_stub_both
make run_binder_both
```

Latest crash dump summary:

```bash
make crashdump
```

## Documentation

Start here:
- `../../docs/launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../docs/client.dll/InitClientDLL/README.md`
- `../../docs/client.dll/RunClientDLL/README.md`
- `../../docs/launcher.exe/startup_objects/README.md`

## Next Work

- make `0x4d2c58` resolve non-NULL on the original path
- reconstruct enough of the real launcher-side binder/registry path behind `ILTLoginMediator.Default`
- then revisit `0x4d6304`
- understand what `0x402ec0` minimally provides
- get the faithful path past `InitClientDLL` before treating `RunClientDLL` as canonical
