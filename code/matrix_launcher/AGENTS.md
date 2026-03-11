# Matrix Online Launcher - Agent Development Notes

For generic workflow and documentation policy, see:
- `../../AGENTS.md`

## Current Goal

Reimplement the original Matrix Online `launcher.exe` startup path as faithfully as possible.

Source of truth:
- `~/MxO_7.6005/launcher.exe`

Current active sources:
- `src/resurrections.cpp`
- `src/diagnostics.cpp`
- `src/diagnostics.h`

## Current Status (2026-03-11)

### Faithful path now implemented in the scaffold
- preloads support DLLs
- loads `cres.dll` before `client.dll`
- resolves:
  - `InitClientDLL`
  - `RunClientDLL`
  - `TermClientDLL`
  - `ErrorClientDLL`
- uses the correct original **8-argument** `InitClientDLL` frame shape
- strips launcher-only auth argv back out before `InitClientDLL`
- models arg7 the same packed way as original launcher from `CLauncher+0xa8/+0xac`
- refuses incomplete `InitClientDLL` by default

### Current blocker
The launcher still does **not** reconstruct enough launcher-owned startup state.
Current unresolved inputs:
- arg5: launcher object at `0x4d6304`
- arg6: `ILTLoginMediator.Default` from `0x4d2c58`
- arg7: packed selection state from `CLauncher+0xa8/+0xac`
- arg8: flag byte from `0x4d2c69`
- pre-client environment setup at `0x402ec0`

Current faithful experiment result:
- with original `client.dll` restored, we fall back to the old import/wrapper problem (`mxowrap.dll`) and do **not** keep the deeper progress path alive yet
- faithful launcher-owned startup state is still incomplete regardless

Current practical runtime note:
- active progress runs now intentionally use the hex-edited `client.dll` import variant (`dbghelp.dll` instead of `mxowrap.dll`)
- current file layout in `~/MxO_7.6005/`:
  - `client.dll` = active patched runtime binary
  - `client.dll.original` = original import layout backup
  - `client.dll.patched` = patched backup copy
- treat this as a pragmatic progress choice, not final faithful equivalence

Current deep diagnostic progress with patched client:
- under the `/home/morgan` user with working video/audio permissions, the game creates a real `MATRIX_ONLINE` window and reaches much deeper client startup
- current observed mediator surface on that path includes:
  - `+0x48`
  - `+0x4c`
  - `+0x170`
  - `+0x124`
- current logged sequence reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - `AttachStartupContext(629ddfc8)`
  - `ProvideStartupTriple(netShell=6298a288 netMgr=62999968 distrObjExecutive=629e9dbc)`
  - `AttachStartupContext(6298a760)`

Current practical crash state:
- latest patched-client progress dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_2.dmp`
- current crash is no longer a simple missing-vtable-slot `EIP=0` case
- current dump lands at `EIP=0x003e3b90`, which looks more like data / corrupted state than a direct null vtable call
- likely next focus: verify post-`+170` / post-`+124` expectations rather than only adding more stub slots

Diagnostic-only forced runtime result:
- older forced-runtime reference dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`
- crash: `client.dll+0x3b3573`

That forced crash is useful for diagnostics, but it is **not** original-equivalent behavior.

## Build / Install / Run

Build:
```bash
cd /home/morgan/mxo/code/matrix_launcher
make
```

The active launcher is built directly to:
- `~/MxO_7.6005/resurrections.exe`

Safe run:
```bash
make run
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

## Key Files

Code:
- `src/resurrections.cpp` - startup orchestration / DLL loading / InitClientDLL frame
- `src/diagnostics.cpp` - mediator stub, launcher-object stub, window tracing
- `src/diagnostics.h`
- `Makefile`
- runtime executable: `resurrections.exe`

Canonical docs:
- `../../docs/launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../docs/client.dll/InitClientDLL/README.md`
- `../../docs/client.dll/RunClientDLL/README.md`
- `../../docs/launcher.exe/startup_objects/README.md`

## Project-Specific Rules

- Do not treat old test harnesses as the solution architecture
- Do not add `client.dll` memory injection to the intended path
- Do not treat a forced `RunClientDLL` after failed `InitClientDLL` as original-equivalent behavior

## Immediate Next Work

1. Reconstruct `0x4d6304` on the original path
2. Reconstruct enough binder/registry state for `0x4d2c58` to become non-NULL
3. Keep tracing what `0x402ec0` minimally sets up
4. Revisit legitimate `RunClientDLL` only after the faithful path gets past `InitClientDLL`
