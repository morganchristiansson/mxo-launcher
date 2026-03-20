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
- newer original-launcher WineDbg evidence now moves the faithful branch boundary later again:
  - the natural password-submit path hits
    `0x41ecd0 -> 0x41c1f0 -> 0x43bd20 -> 0x41af70 -> 0x41cf30 -> 0x43bf64 -> 0x43bf6c -> 0x43f930 -> 0x439780`
  - natural original therefore reaches both the state8 reply body and the helper9/state9 slot-3 follow-on
  - representative live stop at `0x439780` showed helper9 local word `this+6 = 0x2710`
  - owner `+0x664` (`GameSessionID`) was still zero at the natural state8 send site
  - helper11-first theories remain secondary on the natural path:
    - no natural hit yet on `0x41c3c0`
    - no natural hit yet on `0x421220`
    - no natural hit yet on `0x420ef0`
    - no natural hit yet on `0x43c020`

### Current main blockers
The launcher still does **not** reconstruct enough launcher-owned startup/runtime state.
Current unresolved inputs remain, but the active login-side blocker has narrowed again:
- the real original password-submit branch is now confirmed as:
  - `0x41ecd0 = ProcessLoginRequest`
  - then `0x41c1f0` / state `3 -> 8`
- the active state-8 branch remains **closed enough in source for the intended path shape**, and
  newer original-launcher WineDbg runs now prove the natural path is later than the old
  post-send-boundary read:
  - natural original reaches `0x43bd20`, the `0x41af70/0x41cf30` send bridge, and
    `0x43f930`
- practical consequence:
  - for faithful original-launcher progression, the next highest-value target is now the
    **deeper state9 continuation after the now-proven `0x43f930 -> 0x439780` handoff**, especially
    owner/collaborator behavior under `0x41de40` and the later `0x43c180` reply body, before
    broadening back into helper11-first theories
- keep only two narrow state-8 leftovers explicit in source for now:
  - non-`0x10` slot-6 fallback through `0x41c5c0`
  - section-`0x0b` side effect through `0x43f8c0`
- the immediate continuation after successful state-8 completion is now also live in original
  through helper9/state9 slot 3 (`0x439780`), and the next natural-original area to tighten is
  deeper owner/collaborator behavior under `0x41de40` plus the later state9 slot-6 reply body
  `0x43c180`
- forced-`RunClientDLL` scaffold evidence still separately proves that the deliberate runtime path
  can later reach helper11/state11 first after auth success and stall there before any incoming
  `MS_LoadCharacterReply`
- so keep two distinct truths explicit:
  - **original launcher live boundary now crossed**: natural original reaches the state8 send tail
    and `0x43f930`; the next original-live question is deeper reply-side state8 behavior /
    post-state8 continuation
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

1. Treat the now-confirmed password-submit branch through state `8` and into helper9/state9 slot 3
   as **closed enough in source**, and update the next faithful original-live question accordingly:
   - `0x41ecd0`
   - `0x41c1f0`
   - `0x43bd20`
   - `0x41af70`
   - `0x41cf30`
   - `0x43f930`
   - `0x439780`
   - immediate next task is now the deeper state9 owner/helper continuation under `0x41de40` and
     the later `0x43c180` reply handling
2. Keep the remaining narrow state-8 leftovers explicit, but do not reopen the whole state unless
   the runtime path proves they matter:
   - non-`0x10` fallback through `0x41c5c0`
   - section-`0x0b` side effect through `0x43f8c0`
3. Treat helper11/state11 as a **real later scaffold/runtime stall**, but keep it secondary while
   the natural original path is now confirmed later on the state8 reply branch
4. Keep that work in lockstep across:
   - Ghidra names/types
   - source implementation
   - canonical docs
5. Use original-launcher WineDbg evidence to steer priority when it contradicts scaffold intuition;
   right now that means deeper state8 reply / post-state8 continuation outranks speculative
   helper11 producer work
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
   - natural original runs now hit `0x41c1f0`, `0x43bd20`, the `0x41af70/0x41cf30` send bridge,
     `0x43f930`, and the helper9/state9 slot-3 follow-on `0x439780`
   - inspect the deeper owner/helper behavior under `0x41de40` and the later state9 slot-6 reply
     path `0x43c180`
3. Keep the scaffold-only helper11 story explicit but secondary while doing that:
   - forced runtime still reaches helper11/state11 and stalls with no margin reply
   - even deliberately non-empty helper11 payload seeding is not enough by itself
4. Use Ghidra + source to narrow what the original state8 reply still depends on around:
   - raw `0x10` section handling
   - completion criteria / helper9 handoff
   - any launcher-owned startup state still needed before progressing past Loading Character
5. Once the original-live state8 reply boundary is better explained, return to:
   - helper11/source-block authenticity
   - deeper owner/collaborator behavior behind `0x41de40`
   - the next concrete post-state9 continuation that consumes state `0x0c`
6. Keep replacing raw queue/work/context byte-offset usage with documented source-level views when evidence is strong enough
7. For each high-value active-path function, sync Ghidra names and types with source using function rename, variable rename, and variable/parameter retyping as understanding improves
8. Prune stale or resolved notes from this file instead of letting it grow again
