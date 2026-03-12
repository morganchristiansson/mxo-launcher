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

## Current Status (2026-03-12)

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
- current logged sequence still stably reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
  - `AttachStartupContext(second)`

Current practical crash state:
- current patched-client scaffold no longer dies on a simple missing-vtable-slot `EIP=0` case
- the deepest stable logged mediator sequence now reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - repeated `GetSelectionDescriptor(...)`
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
  - `ConsumeSelectionContext(...)` at `+0xec`
- current latest crash family is now tracked canonically in:
  - `../../docs/client.dll/InitClientDLL/CRASH_EIP_003E2B82.md`
- representative current dumps:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_33.dmp`
    - default scaffold selection name
    - `EIP=0x003e2b62`
    - current `arg2 filteredArgv = 0x003e2b60`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_34.dmp`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_35.dmp`
    - with `MXO_MEDIATOR_SELECTION_NAME=Vector`
    - `EIP=0x003e2b82`
    - current `arg2 filteredArgv = 0x003e2b80`
- the stable higher-level signature across those runs is still:
  - control later redirects into **current `arg2 filteredArgv + 2`**
- widening arg5 helper/vtable probes through:
  - primary slots `1..4`
  - primary slots `11..12`
  - embedded helper surfaces at `+0x5c`, `+0x60`, `+0x98`
  has still produced no observed arg5 traffic before the late crash
- newer evidence-backed mediator corrections now tried without moving this crash family:
  - the scaffold now copies the full `0xb4` `+0xec` selection/config handoff object into stable mediator-owned storage
  - `+0x38` is now treated as the client's `Profiles\%s\...` root string input and returns the profile/session-style name (`morgan`)
  - that `+0x38` correction materially changed on-disk side effects by creating `~/MxO_7.6005/Profiles/morgan/aui.cfg`
  - but the late `arg2+2` crash still remained
- current best remaining launcher-owned suspects are:
  - still-incomplete arg7 low-24-bit / selection-id state
  - the launcher-owned arg7 selection-resolution chain around `0x40d763..0x40d810`
  - another later launcher/client ownership mismatch that redirects control into current arg2 storage
- important newer arg7 clarification from fresh static review:
  - the launcher-global root at `0x4d3584` is not just an unknown generic service object
  - initializer `0x496480..0x496491` registers `0x4d3584` through the same `0x4030d0` wrapper using the same string `"ILTLoginMediator.Default"`
  - current next arg7 focus should therefore be the sibling `ILTLoginMediator.Default`-style slot at `0x4d3584` and its `+0xfc / +0x100 / +0xe4` selection queries, not only registry fallback

Current original-launcher runtime validation note:
- the original `~/MxO_7.6005/launcher.exe` now successfully logs into the live game on this machine after manual EULA acceptance
- user also switched it to enter the loading area instead of the live game world
- that is useful differential evidence that the current host Wine/DXVK/runtime environment can support a successful launcher-driven login path
- practical caveat: original-launcher runs are manual and may block on UI/EULA interaction, so they are not suitable as unattended automation steps

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
- Be diligent about experiment documentation: every meaningful rerun, crash change, stable non-change, or new disassembly-backed interpretation must update the relevant canonical docs in `../../docs/` as part of the same work, not later
- Record negative results too when they narrow the search, but keep them in canonical component docs rather than scattered duplicate notes
- When a crash becomes a recurring reference, prefer canonical doc names keyed by a stable signature such as faulting `EIP` / `module+offset` rather than transient dump numbers alone; record the specific dump filenames inside the doc body

## Immediate Next Work

1. Reconstruct deeper `0x4d6304` state on the original path, but stop assuming the currently probed arg5 slots alone explain the late crash
2. Reconstruct more of `0x409950` launcher-side preprocessing, especially `options.cfg` side effects and launcher-global state derived before `InitClientDLL`
3. Recover the arg7 selection-resolution chain around `0x40d763..0x40d810`, `0x48baea`, and the sibling `ILTLoginMediator.Default`-style slot at `0x4d3584` (`+0xfc / +0x100 / +0xe4`) instead of treating `Last_WorldName` alone as the derivation
4. Revisit arg8 / nopatch-derived flag-byte handling once the arg7 / preprocessing path is less incomplete
5. Keep tracing what `0x402ec0` minimally sets up
6. Revisit legitimate `RunClientDLL` only after the faithful path gets past the current late `EIP=arg2+2` crash
