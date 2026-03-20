# Matrix Online Launcher - Agent Development Notes

For generic workflow and documentation policy, see:
- `../../AGENTS.md`

## Purpose of this file

Keep this file **short, current, and operational**.

Do **not** use `AGENTS.md` as a long-running research journal.
Prefer:
- **inline/header comments in source** for code-owned structure, recovered method surfaces, and local TODOs
- **canonical docs under `../../docs/`** for experiment evidence, disassembly-backed conclusions, and cross-component behavior

When something becomes resolved or stops steering current implementation work, prune it from this file.

## Current Goal

Reimplement the original Matrix Online `launcher.exe` startup path as faithfully as possible.

Source of truth:
- `~/MxO_7.6005/launcher.exe`

## Current Active Status

### Faithful pieces already in place
- preload support DLLs
- load `cres.dll` before `client.dll`
- resolve and call:
  - `InitClientDLL`
  - `RunClientDLL`
  - `TermClientDLL`
  - `ErrorClientDLL`
- preserve the original **8-argument** `InitClientDLL` frame shape
- strip launcher-only auth argv before `InitClientDLL`
- treat positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns as success, matching original launcher checks

### Current practical runtime position
- binder/scaffold path can now reach a stable deliberate `RunClientDLL` poll loop
- active patched-client runs can create a real `MATRIX_ONLINE` window and reach the in-game **Loading Character** phase
- the deliberate runtime path now no longer looks like a dead empty arg5 queue:
  - mediator `+0x2c` is live
  - arg5 helper `+0x60` slot `0` / slot `1` are live
  - auth-side queue0C type-2 / type-3 items are consumed on the live `RunClientDLL` path
  - launcher-owned auth now progresses through `AS_GetPublicKeyReply` / `AS_AuthChallenge` / `AS_AuthReply`
- newer post-auth live milestone on the real deliberate runtime path:
  - State4/`0x41e500` margin begin now returns non-zero
  - margin-side type-2 connect-status work is now consumed
  - helper11/state11 slot 3 (`0x43c020`) is now live and sends the fixed-size `0x4d` margin payload
- newest deliberate runtime evidence from the forced-`RunClientDLL` binder path:
  - the default active path really does reach helper11/state11 first after auth success
  - the run then stalls at **Loading Character** with **no incoming margin reply yet**
  - current sent helper11 payload is still source-starved by default:
    - zero appearance/customization ids
    - empty `RealFirstName`
    - empty `RealLastName`
    - empty `Background`
    - empty `GameSessionID`
  - even a deliberately non-empty diagnostic helper11 payload is still **not** enough by itself to
    elicit `MS_LoadCharacterReply`
- newer original-launcher WineDbg evidence now also narrows the faithful branch boundary:
  - the natural password-submit path hits `0x41ecd0 -> 0x41c1f0 -> 0x43bd20`
  - original runs reached state8 send `0x43bd20` repeatedly
  - owner `+0x664` (`GameSessionID`) was still zero there on the original live path
  - those runs terminated before any natural hit on:
    - `0x43f930`
    - `0x41c3c0`
    - `0x421220`
    - `0x420ef0`
    - `0x43c020`

### Current main blockers
The launcher still does **not** reconstruct enough launcher-owned startup/runtime state.
Current unresolved inputs remain, but the active login-side blocker has narrowed again:
- the real original password-submit branch is now confirmed as:
  - `0x41ecd0 = ProcessLoginRequest`
  - then `0x41c1f0` / state `3 -> 8`
- the active state-8 branch remains **closed enough in source for the intended path shape**, but
  newer original-launcher WineDbg runs now say the first faithful live boundary is still earlier
  than helper11:
  - natural original runs reached `0x43bd20`
  - then died before any natural hit on `0x43f930`
- practical consequence:
  - for faithful original-launcher progression, the next highest-value target is now the
    **state8 post-send survivability / immediate continuation after `0x43bd20`**, before treating
    helper11 as the first faithful live branch target
- keep only two narrow state-8 leftovers explicit in source for now:
  - non-`0x10` slot-6 fallback through `0x41c5c0`
  - section-`0x0b` side effect through `0x43f8c0`
- the immediate continuation after successful state-8 completion is still good enough in source
  through helper9/state9 follow-on, but that continuation is not yet the first original live stop
- forced-`RunClientDLL` scaffold evidence still separately proves that the deliberate runtime path
  can later reach helper11/state11 first after auth success and stall there before any incoming
  `MS_LoadCharacterReply`
- so keep two distinct truths explicit:
  - **original launcher live boundary**: state8 send / immediate post-send survivability
  - **forced scaffold runtime stall**: helper11 source block / packet payload is still too weak to
    elicit the first real margin reply
- launchpad-owned success mirrors for owner `+0x660`, owner `+0x664`, and owner `+0x94`
  first-string consequences are now source-owned, but broader post-auth writeback is still incomplete
- arg6: `ILTLoginMediator.Default` from `0x4d2c58`
- arg7: packed selection state from `CLauncher+0xa8/+0xac`
- arg8: flag byte from `0x4d2c69`
- pre-client environment setup at `0x402ec0`
- deeper arg5 work for later non-auth item families, especially missing live type-1 / slot-12 traffic

### Current arg5/queue focus
Current highest-value active path is:
- original launcher consumer family:
  - `0x436b10`
  - `0x436d31..0x436ee7`
- client non-blocking analog:
  - `0x62532130 -> 0x62531c10(1)`
- original producer helper family:
  - `0x436820 -> 0x436670`

Current best evidence-backed read:
- queue0C is a launcher-owned **network-engine event/status queue**
- queued pair shape is currently best read as:
  - first dword = work item
  - second dword = context / owner, often likely a `CMessageConnection`-family object
- when queue work exists, consumer flow reaches arg5 slot `12` / `CleanupConnection`, then `context->+0x10(workItem)`
- the old "purely empty queue state" reading is now too weak for the active deliberate runtime path:
  - auth-side type-2 / type-3 queue0C work now gets consumed on the live `RunClientDLL` path
- current remaining arg5-side negative result is narrower:
  - no live type-1 work has appeared yet
  - so slot `12` / `CleanupConnection` still has not become live on the deliberate runtime path
- do **not** treat simply removing the `RunClientDLL` gate as progress; the active goal is to make the post-init arg5 queue path faithful enough that ungating becomes justified

## Documentation / ownership rules for this project

### Keep AGENTS concise
Do not append resolved crash histories, repeated rerun summaries, or long xref inventories here.
Those belong in:
- source comments
- canonical docs
- commit history

When a run proves an active blocker description is outdated, rewrite the blocker summary in this file immediately.
Do not leave invalidated blocker text in place just because it was true in an earlier session.

### Prefer source ownership for recovered code structure
When RE has already produced strong class/method/field understanding, prefer owning that in:
- `matrixstaging/runtime/src/liblttcp/`
- `matrixstaging/runtime/src/libltmessaging/`
- `matrixstaging/game/src/libltclientlogin/`
- `src/launcher_*_abi.cpp`

Use inline comments like:
- `anchor: launcher.exe:0x...`
- `UNANCHORED: ...`

Every new or changed class/method should clearly say whether it is:
- anchored to a recovered original function / slot / string
- or scaffold-only / unanchored

### Prefer canonical docs for evidence and cross-component conclusions
Use `../../docs/` for:
- experiment conditions and outcomes
- crash signatures
- xref-backed conclusions
- component interaction summaries
- recovered vtable surfaces and object-interface reference docs under `../../docs/launcher.exe/VTABLES/`

Prefer updating existing docs over expanding this file.

### Logging
- new log statements should use `spdlog`

### Use Ghidra in lockstep with source
Ghidra is the primary static-analysis tool. Ghidra HOWTO: `GHIDRA.md`

Source code, VTABLE documentation and Ghidra should progress in lockstep and be synced.

When reverse engineering or decompiling new methods / vtable slots on active classes:
- add anchored source implementations or scaffolds for them in the same task
- sync function names with `../../docs/launcher.exe/VTABLES/` and Ghidra in the same task
- do not let newly recovered active-slot understanding live only in docs or only in the current Ghidra session

## Current implementation priorities

1. Treat the now-confirmed password-submit branch through state `8` as **closed enough in source**,
   but make the next faithful original-live question explicit:
   - `0x41ecd0`
   - `0x41c1f0`
   - `0x439300`
   - `0x43bd20`
   - `0x43f930`
   - immediate next task is now the boundary between natural original hits on `0x43bd20` and the
     still-missing natural hit on `0x43f930`
2. Keep the remaining narrow state-8 leftovers explicit, but do not reopen the whole state unless
   the runtime path proves they matter:
   - non-`0x10` fallback through `0x41c5c0`
   - section-`0x0b` side effect through `0x43f8c0`
3. Treat helper11/state11 as a **real later scaffold/runtime stall**, but stop treating it as the
   first faithful original-launcher breakpoint target until original live evidence reaches it
4. Keep that work in lockstep across:
   - Ghidra names/types
   - source implementation
   - canonical docs
5. Use original-launcher WineDbg evidence to steer priority when it contradicts scaffold intuition;
   right now that means state8-post-send survivability outranks speculative helper11 producer work
6. Continue active-path arg5 queue work where it directly helps the game-running path, but do not
   let arg5 work displace the confirmed state8/original-live continuation boundary
7. Preserve faithful structure even when a path may be inactive, but do **not** overinvest in likely-dead code before it becomes relevant to the active blocker
8. Keep pruning duplicated queue logic from ABI scaffolding into canonical liblttcp/libltmessaging code when safe
9. Keep auth launcher-owned, not client-owned

## Key files

### Active implementation
- `src/resurrections.cpp`
- `src/launcher_network_object_abi.cpp`
- `src/launcher_mediator_abi.cpp`
- `src/diagnostics_auth.cpp`
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h`
- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp`
- `matrixstaging/runtime/src/liblttcp/lttcpconnection.h`
- `matrixstaging/runtime/src/liblttcp/lttcpconnection.cpp`
- `matrixstaging/runtime/src/libltmessaging/messageconnection.h`
- `matrixstaging/runtime/src/libltmessaging/messageconnection.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator.h`
- `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- `matrixstaging/game/src/libltclientlogin/loginstate.h`
- `matrixstaging/game/src/libltclientlogin/loginstate.cpp`
- `matrixstaging/game/src/libltclientlogin/launchpad.h`
- `matrixstaging/game/src/libltclientlogin/launchpad.cpp`

### Canonical docs
- `../../docs/client.dll/InitClientDLL/README.md`
- `../../docs/client.dll/RunClientDLL/README.md`
- `../../docs/launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../docs/launcher.exe/startup_objects/README.md`
- `../../docs/launcher.exe/startup_objects/0x4d6304_network_engine.md`
- `../../docs/launcher.exe/VTABLES/`
- `../../docs/launcher.exe/auth/README.md`
- `../../docs/launcher.exe/auth/STATUS.md`

## Build / run

Build:
```bash
cd /home/morgan/mxo/code/matrix_launcher
make
```

Safe run:
```bash
cd /home/morgan/mxo/code/matrix_launcher
make run
```

Current deliberate binder path:
```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both
```

Current deliberate binder runtime path:
```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both_runtime
```

## Immediate next tasks

1. Keep state `8` closed enough in source for the active path:
   - keep only the narrow explicit leftovers:
     - non-`0x10` fallback through `0x41c5c0`
     - section-`0x0b` side effect through `0x43f8c0`
2. Retarget the immediate faithful-original problem using the newest WineDbg evidence:
   - natural original runs hit `0x41c1f0` and `0x43bd20`
   - natural original runs did **not** reach `0x43f930` before process death
   - inspect `0x43bd20`, `0x41af70`, and the immediate post-send/runtime continuation that should
     survive long enough for a later state8 reply
3. Keep the scaffold-only helper11 story explicit but secondary while doing that:
   - forced runtime still reaches helper11/state11 and stalls with no margin reply
   - even deliberately non-empty helper11 payload seeding is not enough by itself
4. Use Ghidra + source to narrow what the original state8 send still depends on around:
   - margin connection/runtime state
   - packet send / post-send event path
   - any launcher-owned startup state needed before a later natural `0x43f930` can exist
5. Once the original-live state8 post-send boundary is better explained, return to:
   - helper11/source-block authenticity
   - deeper owner/collaborator behavior behind `0x41de40`
   - the next concrete post-state9 continuation that consumes state `0x0c`
6. Keep replacing raw queue/work/context byte-offset usage with documented source-level views when evidence is strong enough
7. For each high-value active-path function, sync Ghidra names and types with source using function rename, variable rename, and variable/parameter retyping as understanding improves
8. Prune stale or resolved notes from this file instead of letting it grow again
