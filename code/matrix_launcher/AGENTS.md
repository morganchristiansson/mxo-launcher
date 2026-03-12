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
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_35.dmp`
    - with `MXO_MEDIATOR_SELECTION_NAME=Vector`
    - `EIP=0x003e2b82`
    - current `arg2 filteredArgv = 0x003e2b80`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_54.dmp`
    - after widening arg5 to the full recovered 13-slot primary vtable surface
    - `EIP=0x003e5e4a`
    - current `arg2 filteredArgv = 0x003e5e48`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_57.dmp`
    - with in-launcher `MXO_ARG2_RET_BYPASS=1`
    - second fault after simulated `ret`
    - `EIP=0x62000003` (`client.dll+0x3`)
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_60.dmp`
    - non-zero arg7 probe with scratch-shaped `+0x40` acceptance
    - `EIP=0x003e5e8a`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_61.dmp`
    - follow-up rerun after explicit `selectionContext[0]` logging
    - `EIP=0x003e5e8a`
- the stable higher-level signature across the unfixed runs is still:
  - control later redirects into **current `arg2 filteredArgv + 2`**
- the arg2 investigation is complete enough to treat `arg2` as a symptom:
  - the late landing in argv storage is downstream corruption, not the root bug
  - current canonical references:
    - `../../docs/client.dll/InitClientDLL/FINAL_INVESTIGATION_SUMMARY.md`
    - `../../docs/client.dll/InitClientDLL/RET_BYPASS_HACK.md`
- widening arg5 reconstruction now includes:
  - full recovered primary vtable surface `0x4b2768` slots `0..12`
  - evidence-backed empty-path modeling for slot `5` (`0x431840`)
  - corrected `+0x80` / `+0x8c` sentinel-headed container interpretation
- but even after that wider arg5 work:
  - no observed arg5 traffic has appeared before the late crash from:
    - primary slots `5..10`
    - primary slots `11..12`
    - embedded helper surfaces at `+0x5c`, `+0x60`, `+0x98`
- the in-launcher `ret` bypass was useful for validation, but only negatively:
  - it can step over the immediate `arg2` landing
  - the popped target is just stale startup-frame `arg3 hClientDll = 0x62000000`
  - execution then immediately faults at `client.dll+0x3`
  - it still does **not** reveal later arg5 method traffic
- newer stack logging now preserves the intended 8-argument `InitClientDLL` frame before the call and compares it against the crash-time stack:
  - on the late `arg2+2` crash, current stack top matches stale startup-frame values in order:
    - `esp[0] = arg3 hClientDll`
    - `esp[1] = arg4 hCresDll`
    - `esp[2] = arg5 launcherNetworkObject`
    - `esp[3] = arg6 ILTLoginMediatorDefault`
  - with non-zero arg7 experiments, `esp[4]` also matches `arg7 packedArg7Selection`
  - this further supports the current corrupted-return-chain interpretation
- arg7/method-surface scaffolding is now slightly less shallow:
  - the diagnostic mediator now exact-matches the configured selected world / variant for `+0xfc / +0x100 / +0xe4 / +0x40` instead of generically accepting every in-range index
  - `+0x3c` now returns the configured selected world id instead of hardcoded `0`
  - optional env knobs now exist for diagnostic selection-shape experiments:
    - `MXO_MEDIATOR_WORLD_TYPE`
    - `MXO_MEDIATOR_VARIANT_STATE`
- first non-zero arg7 probe result from that stricter model:
  - with `MXO_ARG7_SELECTION=0x0500002a` and `MXO_MEDIATOR_SELECTION_NAME=Vector`
  - launcher-side sibling-slot rebuild still resolves `+0xfc/+0x100/+0xe4` successfully for world `0x2a`, variant `0x05`
  - a fresh static pass now explains the later client `+0x40` request shape:
    - inside `client.dll:0x62170dc1..0x62170e59`, the client reuses the original arg7 stack slot as scratch and overwrites only its low byte with the high-8-bit variant value
    - so `arg7=0x0500002a` becomes client-side `selectionIndex=0x05000005` before later path-building helpers call `arg6->+0x40`
    - importantly, the same block also stores the original low-24-bit selection id separately via `push esi ; mov ecx, 0x629e1c7c ; call 0x620011e0` **before** that scratch mutation
  - the diagnostic mediator now accepts that scratch-shaped `+0x40` request and returns the configured descriptor (`matchMode=arg7-scratch-shape`)
  - current best reading is therefore that the client expects **both**:
    - a stable persisted low-24-bit selection id somewhere else
    - specifically through the nearby store path rooted at `0x629e1c7c` via `call 0x620011e0`
    - and the later scratch-shaped `+0x40` lookup key
  - newer static follow-up now identifies that `0x629e1c7c` state more concretely:
    - static initializer `client.dll:0x627c5320` constructs a client-side console-int object named `CreateCharacterWorldIndex` at `0x629e1c7c`
    - constructor `0x620022f0` seeds it with default/current `0`
    - base console-var ctor `0x622a2270` zeroes callback slots `+0x20 / +0x24 / +0x28`
    - current direct xref search found only one concrete direct read of its current value field `0x629e1cb0` at `client.dll:0x62054cbd`
    - surrounding strings there (`CharCreate_2_Finish`, `CharCreate_2_Back`, `Loading Character`) still tie it to character/loading flow rather than a simple mediator method surface
    - newer runtime observation now matters here: the current patched-client crash is visibly happening during the in-game **Loading Character** phase (loading bar + status text already on screen)
    - so this `CreateCharacterWorldIndex` path should stay live as a promising later consumer of the persisted arg7 low-24-bit state, not be written off as unrelated late UI only
  - so the low-24-bit arg7 path now looks more like client-owned persisted console/config state than another unresolved arg6 method contract — but it may still be materially relevant on the current failing loading-character path
  - but this still did **not** move the late crash family (`~/MxO_7.6005/MatrixOnline_0.0_crash_60.dmp`, `EIP=0x003e5e8a`)
- newer evidence-backed mediator corrections now tried without moving this crash family:
  - the scaffold now copies the full `0xb4` `+0xec` selection/config handoff object into stable mediator-owned storage
  - `+0x38` is now treated as the client's `Profiles\%s\...` root string input and returns the profile/session-style name (`morgan`)
  - that `+0x38` correction materially changed on-disk side effects by creating `~/MxO_7.6005/Profiles/morgan/aui.cfg`
  - newer static work now also explains two deeper details of this path:
    - `client.dll:0x62195ff0` uses the `+0x40` descriptor payload fields at `+3` (name pointer) and `+7` (selection id) for the `Profiles\%s\%s_%X\` suffix formatting
    - `client.dll:0x62170de2..0x62170e3b` stores the zero-extended arg7 high-8-bit variant value into the first dword of the later `+0xec` handoff object
  - in the non-zero arg7 probe, that matches current copied `selectionContext[0] = 0x00000005`
  - current runtime log now explicitly confirms that field shape:
    - `DIAGNOSTIC: selectionContext[0]=0x00000005 (configuredVariant=0x05 configuredWorld=0x00002a)`
  - but the late crash family still remained
- current best remaining launcher-owned suspects are:
  - still-incomplete arg7 low-24-bit / selection-id state
  - the launcher-owned arg7 selection-resolution chain around `0x40d763..0x40d810`
  - broader launcher-owned preprocessing / globals from `0x409950`
  - another later launcher/client ownership mismatch that redirects control into current arg2 storage
- important newer arg7 clarification from static review remains:
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

Diagnostic-only late-crash bypass experiment:
```bash
MXO_ARG2_RET_BYPASS=1 MXO_ARG2_RET_BYPASS_MAX=4 make run_binder_both
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

1. Recover the arg7 selection-resolution chain around `0x40d763..0x40d810`, `0x48baea`, and the sibling `ILTLoginMediator.Default`-style slot at `0x4d3584` (`+0xfc / +0x100 / +0xe4`) instead of treating `Last_WorldName` alone as the derivation
2. Follow the new `+0x40` scratch-shape explanation deeper:
   - determine how the client expects the mutated arg7 scratch request to map back to persisted low-24-bit selection id / descriptor data after `0x62170dc1..0x62170e59`
   - stop assuming that accepting the scratch-shaped request alone is enough
3. Reconstruct more of `0x409950` launcher-side preprocessing, especially `options.cfg` side effects and launcher-global state derived before `InitClientDLL`
4. Trace the persisted low-24-bit selection-id path rooted at `0x629e1c7c` / `0x620011e0` now that it is identified as client-side `CreateCharacterWorldIndex`, and determine how its later consumers (especially on the visible `Loading Character` path) depend on launcher-owned state before or alongside the later scratch-shaped `+0x40` lookup
5. Improve semantic validation of the post-`+0xec` `0xb4` selection/config handoff object instead of treating it as only an opaque copied buffer
6. Reconstruct deeper `0x4d6304` state on the original path, but stop assuming the currently recovered arg5 slots alone explain the late crash
7. Revisit arg8 / nopatch-derived flag-byte handling once the arg7 / preprocessing path is less incomplete
8. Keep tracing what `0x402ec0` minimally sets up
9. Revisit legitimate `RunClientDLL` only after the faithful path gets past the current late `EIP=arg2+2` crash
