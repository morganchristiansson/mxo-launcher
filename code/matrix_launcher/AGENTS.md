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
- newer debugger + static narrowing identified a concrete stack-cleanup mismatch in the early mediator auth-name chain:
  - `client.dll:0x62001325..0x62001362` calls mediator `+0x58`, then passes that result to `+0x60`, then passes that result to `+0x5c`, then calls a formatter and finally does `add esp, 0x14`
  - that caller cleanup means mediator `+0x60` and `+0x5c` are expected to be **caller-clean single-arg methods**, not callee-clean `ret 4` methods
  - the scaffold had exposed both as normal `__thiscall` methods, so our replacement launcher was incorrectly popping 8 bytes too many before `client.dll:0x62001365`
- newer debugger-assisted proof of the overwrite mechanism:
  - at `client.dll:0x62001319`, the top-level `InitClientDLL` saved return address at `[ebp+4]` is still launcher `0x411187`
  - by `client.dll:0x62001365`, `esp` has drifted to `ebp+8`
  - so the next `push esi` writes current stale `esi = arg2 filteredArgv + 0x0c` directly over the saved return address slot
  - later returns then land in current `arg2` storage and produce the familiar late crash family
- the diagnostic mediator now exposes `+0x60` / `+0x5c` through naked caller-clean wrappers (`ret`, not `ret 4`)
- current highest-value rerun after that fix:
  - `MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
  - the client still reaches the deeper mediator path through `ConsumeSelectionContext(...)` at `+0xec`
  - but the old late `EIP=arg2+2` crash family no longer reproduces on that run
  - `InitClientDLL` now returns cleanly to the launcher with value **`1`** instead of crashing
  - newer launcher-side handling now also treats this positive return as success rather than generically treating every non-zero return as failure
  - current `resurrections.log` now ends with:
    - `InitClientDLL returned: 1`
    - `InitClientDLL succeeded, but RunClientDLL is gated.`
- new deliberate `RunClientDLL` experiment on that same clean binder path:
  - `MXO_TRACE_WINDOWS=1 MXO_FORCE_RUNCLIENT=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
  - original launcher static review now confirms `0x40a5a9`, `0x40a624`, and `0x40a6be` all use `test eax,eax ; jg ...`, so positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns are the original success contract
  - the deliberate run now does **not** reproduce the old forced-runtime crash at `client.dll+0x3b3573`
  - no fresh crash dump was produced during timed runs
  - window tracing shows a real `MATRIX_ONLINE` window first appears centered, then flips to fullscreen `800x600`
  - the current stable `RunClientDLL` loop repeatedly hits:
    - mediator `+0x2c` (`IsConnected()` in the scaffold)
    - arg5 helper `+0x60` slot `0` / slot `1` (`EnterCriticalSection` / `LeaveCriticalSection`)
  - newer throttled queue-state logging now also shows the exact arg5 cursor state observed by that loop:
    - queue0C `current0 == current1 == block0 == block1`
    - queue34 `current0 == current1 == block0 == block1`
    - that remained unchanged through sampled counts `1 .. 1024`
  - newer static comparison now shows this is not just a loose resemblance:
    - client `0x62532130 -> 0x62531c10(1)` is the **non-blocking poll variant** of the same shared arg5 queue-consumer family seen in original launcher `0x436b10`
    - original launcher producer helper `0x436820 -> 0x436670` acquires `+0x60`, enqueues an 8-byte pair into queue0C/queue34, releases `+0x60`, and signals `+0x5c` when transitioning from empty to non-empty
    - in the currently identified concrete producer xrefs (`0x4302d5`, `0x4325aa`, `0x4329cc`, `0x449d8a`), the third arg to `0x436820` is always `0`, so present startup/runtime producer evidence is currently specific to **queue0C**
    - those xrefs also narrow the queued pair shape to:
      - first dword = small freshly allocated work-item-like object (`0x2c`/`0x20`/`0x0c` families via `0x435090` / `0x435010` / `0x435050` / `0x435070`)
      - second dword = stable owner/context pointer or associated object (for example `[esi+0x38]`, `edi`, or similar caller-held context)
    - the remaining concrete producer xrefs now read so far fall into several queue0C families:
      - existing-object + context submissions
      - fresh `0x0c` / `0x20` / `0x2c` work-item submissions
      - back-to-back multi-submit paths
      - null/fallback submissions
      - one looped submit-and-wait style path at `0x449d8a`
    - all currently identified concrete `0x436820` producer xrefs have now been read/documented:
      - `0x4302d5`
      - `0x43051f`
      - `0x43067f`
      - `0x4306a7`
      - `0x4309da`
      - `0x4309ef`
      - `0x430c25`
      - `0x430d71`
      - `0x430d94`
      - `0x430da8`
      - `0x4315b0`
      - `0x4325aa`
      - `0x4329cc`
      - `0x432d86`
      - `0x432dc1`
      - `0x432dd7`
      - internal self-calls: `0x436a0e`, `0x436fa8`
      - looped submit-and-wait path: `0x449d8a`
    - newer import-backed narrowing now makes the producer family more concrete than “generic queued work”:
      - `0x4302d5` sits on a later `recvfrom` path, so it now looks like receive/packet-side event submission
      - helper `0x449b40` is a socket factory around `socket(AF_INET, type, protocol)` plus option setup
      - `0x4328a0` uses that helper as `SOCK_STREAM` / `IPPROTO_TCP` and later calls `connect`, so its queue submissions now look like TCP connect / connect-status work
      - `0x4325d0` uses that helper as `SOCK_DGRAM` / `IPPROTO_UDP`, then `setsockopt(...SO_REUSEADDR...)`, then `bind`, so that method now reads as UDP bind/setup in the same engine family
      - coded `0x435050(payload)` items now look like network status/result objects, not generic integer blobs
    - current best reading is therefore that queue0C is a launcher-owned **network-engine event/status queue**, not a generic task list
    - newer container/dispatch narrowing now also shows:
      - arg5 `+0x80` is an endpoint-keyed container (sockaddr/port-style key)
      - arg5 `+0x8c` is a pointer-keyed container (context/owner dword key)
      - arg5 slot `5` (`0x431840`) is now best read as an endpoint-removal / teardown / handle-extraction path with miss result `0x7000004`
      - arg5 slot `12` (`0x4316a0`) acquires helper `+0x98`, looks up the dequeued context in `+0x8c`, consumes/removes a payload object there, performs teardown/state-transition work, and then calls `0x44ab60(context)`
      - launcher consumer `0x436d31..0x436ee7` dequeues `(workItem, context)`, reads `[workItem+0x04]` through `0x4816f0`, calls arg5 slot `12(context)`, then calls `context->+0x10(workItem)`
    - when queue work is present, client `0x62531e31..0x62531fe7` mirrors launcher `0x436d31..0x436ee7` and then calls arg5 primary vtable offset `+0x30` (slot `12`)
    - so the current absence of arg5 slot-12 runtime traffic is now best explained by **empty queue state**, not by slot 12 being irrelevant
  - current best reading is that the binder/scaffold path now reaches a real runtime idle/poll loop rather than an immediate post-init crash, and the next blocker is missing launcher-owned work/state that should drive arg5 queue activity beyond the current empty-loop path
- visible current runtime state still includes an in-game **Loading Character** phase with loading bar / status text on the deeper path
- static ordering still explains why that UI is compatible with the currently logged mediator depth:
  - `client.dll:0x62170f2a` pushes the string `"Loading Character"`
  - and the same block then calls `arg6->+0xec` at `0x62170f48`
  - so seeing that UI proves the client reached the pre-`+0xec` loading/status phase, but does **not** yet prove later loading-character consumers such as the `CreateCharacterWorldIndex` read at `0x62054cbd`
- the deepest stable logged mediator sequence now reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - repeated `GetSelectionDescriptor(...)`
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
  - `ConsumeSelectionContext(...)` at `+0xec`
- the old late `arg2+2` family remains canonically documented in:
  - `../../docs/client.dll/InitClientDLL/CRASH_EIP_003E2B82.md`
- but it is now best treated as a **resolved scaffold bug** in the current path, caused by the mediator `+0x5c` / `+0x60` cleanup mismatch rather than by an unresolved mysterious late unwind alone
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
    - important ordering correction from newer static review: the user-visible `"Loading Character"` status text comes from earlier `client.dll:0x62170f2a`, immediately before the already-observed `arg6->+0xec` call at `0x62170f48`
    - so the current visible loading-bar state keeps this area interesting, but does **not** yet prove we reached the later `CreateCharacterWorldIndex` consumer at `0x62054cbd`
    - to avoid missing the next loading-phase transition, the diagnostic mediator now also exposes/logs slot `+0x120`; follow-up rerun `crash_62` still showed no `+0x120` traffic before the same late crash
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
4. Follow the **new post-fix blocker** now that the old late `arg2+2` crash family no longer reproduces on the current binder path:
   - preserve the now-confirmed original success contract from `launcher.exe:0x40a5a9 / 0x40a624 / 0x40a6be` (`test eax,eax ; jg ...`) and `0x40a6fd` (`al = 1` on overall success)
   - treat positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns as the original success shape, not an anomaly
5. Trace the stable deliberate `RunClientDLL` loop now reachable on the clean binder path:
   - `RunClientDLL -> 0x62006c30` repeatedly polls mediator `+0x2c`
   - then drives arg5 through `0x62532130 -> 0x62531c10(1)`
   - treat that as the **non-blocking consumer** side of the same engine-family logic seen in launcher `0x436b10`, not merely as arbitrary queue comparisons
   - keep the queue-field mapping precise there:
     - `+0x0c` = queue0C `current0`
     - `+0x1c` = queue0C `current1`
     - `+0x34` = queue34 `current0`
     - `+0x44` = queue34 `current1`
   - follow the original producer side now identified at `0x436820 -> 0x436670`:
     - acquires `+0x60`
     - enqueues an 8-byte pair into queue0C / queue34
     - releases `+0x60`
     - signals `+0x5c` on empty->non-empty transition
   - the representative set is no longer enough; all currently identified concrete producer xrefs have now been read and should be treated as the active reference set:
     - `0x4302d5`
     - `0x43051f`
     - `0x43067f`
     - `0x4306a7`
     - `0x4309da`
     - `0x4309ef`
     - `0x430c25`
     - `0x430d71`
     - `0x430d94`
     - `0x430da8`
     - `0x4315b0`
     - `0x4325aa`
     - `0x4329cc`
     - `0x432d86`
     - `0x432dc1`
     - `0x432dd7`
     - internal lifecycle/self-calls: `0x436a0e`, `0x436fa8`
     - looped submit-and-wait path: `0x449d8a`
   - current concrete startup/runtime producer evidence passes third-arg `0`, so prioritize understanding the **queue0C** feed path first
   - keep the newer semantic narrowing in mind while doing that:
     - queue0C now looks like a launcher-owned **network-engine event/status queue**
     - not a generic arbitrary job list
     - concrete recovered producer families now already touch `recvfrom`, `socket`, `setsockopt`, `bind`, and `connect`
   - determine what the queued pair means semantically:
     - first dword = work-item-like object (`0x435090` / `0x435010` / `0x435050` / `0x435070` families)
     - second dword = owner/context pointer or paired object
   - keep the current concrete xref taxonomy straight while doing that:
     - some producers enqueue existing objects
     - some enqueue fresh small command/status objects
     - some do paired multi-submit sequences
     - some fall back to null submissions
     - `0x449d8a` currently looks like a submit-and-wait loop
   - current highest-value semantic anchors inside that taxonomy are now:
     - `0x4302d5` = recvfrom-adjacent / packet-side producer
     - arg5 primary slot `6` / `0x4328a0` = TCP socket/connect family producer path
     - arg5 primary slot `2` / `0x4325d0` = UDP socket/bind family path in the same engine
     - `0x435050(payload)` = coded status/result item family (`0x700000x`-style)
   - keep the now-identified next consumer milestone in mind:
     - once queue work exists, client `0x62531e31..0x62531fe7` should reach arg5 primary slot `12` at vtable offset `+0x30`
     - original launcher consumer `0x436d31..0x436ee7` then treats the dequeued pair as `(workItem, context)`, reads `[workItem+0x04]`, calls slot `12(context)`, and then calls `context->+0x10(workItem)`
     - that means the next fidelity gap is not just “feed any queue entry” but “feed the right **workItem + context** pair family so the later slot-12 / context callback chain is meaningful”
   - determine which launcher-owned startup/runtime state should cause those producer paths to become live beyond the present idle loop where both queues still show `current0 == current1`
6. Trace the persisted low-24-bit selection-id path rooted at `0x629e1c7c` / `0x620011e0` now that it is identified as client-side `CreateCharacterWorldIndex`, and determine how its later consumers (especially on the loading-character path around `0x620547c0..0x62054eac`) depend on launcher-owned state before or alongside the later scratch-shaped `+0x40` lookup
7. Improve semantic validation of the post-`+0xec` `0xb4` selection/config handoff object instead of treating it as only an opaque copied buffer
8. Reconstruct deeper `0x4d6304` state on the original path, but stop assuming the currently recovered arg5 slots alone explain the late crash
9. Revisit arg8 / nopatch-derived flag-byte handling once the arg7 / preprocessing path is less incomplete
10. Keep tracing what `0x402ec0` minimally sets up
11. Continue deliberate `RunClientDLL` experiments on the now-clean positive-return path, but keep them clearly labeled as binder/scaffold progress rather than faithful original-equivalent success until the launcher-owned arg5/arg6/arg7/pre-client state is reconstructed more faithfully
