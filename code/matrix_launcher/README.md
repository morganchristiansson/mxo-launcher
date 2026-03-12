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

Current runtime note:
- for now, active progress runs use the hex-edited `~/MxO_7.6005/client.dll` variant that imports `dbghelp.dll`
- backup copies are kept as `client.dll.original` and `client.dll.patched`
- the original import-layout client still pulls us back into the old `mxowrap.dll` problem
- this is a pragmatic progress choice, not the final faithful endpoint

## Current State

The current scaffold already:
- preloads support DLLs
- loads `cres.dll` before `client.dll`
- resolves the client exports used by original `launcher.exe`
- uses the correct original 8-argument `InitClientDLL` frame shape
- refuses incomplete startup by default

Current blocker:
- launcher-owned startup state is still incomplete
- faithful current result is still blocked before a real original-equivalent startup

Current practical progress path with patched client:
- real-user `/home/morgan` runs now create a visible `MATRIX_ONLINE` window and proceed well past the old immediate mediator gate
- current deep path reaches mediator calls at `+0x48`, `+0x4c`, `+0x170`, and `+0x124`
- latest practical progress dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_2.dmp`
- current crash no longer looks like a simple null-vtable call; latest dump lands at `EIP=0x003e3b90`

Diagnostic-only forced runtime can still reproduce the older known crash:
- `client.dll+0x3b3573`
- reference dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`

That forced crash is useful for analysis, but it is **not** original-equivalent behavior.

## Build

```bash
cd /home/morgan/mxo/code/matrix_launcher
make
```

This builds the active launcher directly to:

- `~/MxO_7.6005/resurrections.exe`

## Run

Safe default run:

```bash
cd /home/morgan/mxo/code/matrix_launcher
make run
```

Optional local credentials:

```make
# secrets/launcher_login.local.mk
MXO_USER := your-username
MXO_PASS := your-password
# optional:
# MXO_SESSION := your-session-token
# MXO_CHAR := your-character-name
```

If that file exists, the Makefile now passes launcher-style switches to `resurrections.exe`, e.g.:
- `-user <name>`
- `-pwd <password>`
- optional `-session <token>`
- optional `-char <name>`

The launcher consumes those switches into launcher-owned preprocessing state and strips them back out before `InitClientDLL`.
It also now consumes known launcher-only switches like:
- `-clone`
- `-silent`
- `-nopatch`
- `-recover`
- `-deletechar`
- `-justpatch`
- `-noeula`
- `-skiplaunch`
- `-lptest`
- `-qluser`
- `-qlpwd`
- `-qlchar`
- `-qlsession`
- `-qlver`

instead of forwarding them blindly into `InitClientDLL`.

Optional diagnostic arg7/arg8 overrides for post-`IsReady()` experiments:

```bash
MXO_ARG7_SELECTION=0x01000000 MXO_ARG8_FLAG=0 make run_stub_both
```

You can also mirror the original launcher split fields directly:

```bash
MXO_CLAUNCHER_A8=1 MXO_CLAUNCHER_AC=0 make run_stub_both
```

`resurrections.exe` now rebuilds arg7 the same way the original launcher does:
- high 8 bits from `CLauncher+0xa8`
- low 24 bits from `CLauncher+0xac`

Optional mediator selection name override:

```bash
MXO_MEDIATOR_SELECTION_NAME=Vector make run_stub_both
```

By default the launcher now also probes the original registry location for:
- `HKLM\\Software\\Monolith Productions\\The Matrix Online\\Last_WorldName`

and reuses that string as the diagnostic mediator selection name when no explicit override is provided.

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

Optional iteration helper to avoid the interactive crash reporter GUI during diagnostic crash loops:

```bash
make install_crashreporter_stub
```

That target:
- builds a tiny no-op `crashreporter.exe` replacement
- preserves the original as `~/MxO_7.6005/crashreporter.exe.original`
- replaces the active runtime copy with the stub
- logs invocations to `~/MxO_7.6005/crashreporter_stub.log`

Restore the original reporter with:

```bash
make restore_crashreporter
```

This is only a workflow aid for crash iteration. It is not part of the launcher reimplementation itself.

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
