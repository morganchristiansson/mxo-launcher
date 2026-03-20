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
  - the replacement launcher no longer has to fall through the helper10/helper11 claim/create path
    on the current existing-character branch
- newest deliberate runtime evidence from the forced-`RunClientDLL` binder path:
  - the active replacement path now preserves state `8` through auth success on the
    existing-character branch instead of forcing `0x0a -> 0x0b`
  - `AS_AuthReply` now returns to the existing-character state8 path
  - successful margin connect-status now promotes owner `+0x1c` into the ready send state needed by
    `0x41b4b0`
  - state8 slot 3 now sends the real structured raw-`0x0f` / `MS_LoadCharacterRequest` packet:
    - fixed bytes `0xbb`
    - current slot GCID low `0x00006dce`
    - current slot GCID high `0x00000000`
    - host `reality.lith.thematrixonline.net`
  - newer client-side selection-context tightening now also removed one clearly bad replacement-side
    input family from that send:
    - the copied `+0xec` selection/config object was carrying repeated cfg-parser/default values in
      the `+0x24..+0xb3` tail while no per-selection profile cfg set existed under
      `Profiles/<profile>/<selection>_<id>/`
    - current scaffold now zeros those cfg-derived state8 snapshot blocks instead of forwarding the
      repeated garbage as send material
  - this moves the replacement boundary later and closer to the natural-original shape than the old
    helper11-only stall
  - current new replacement-side blocker is now the next receive step after that state8 send:
    - still no incoming margin reply yet in the short run window
    - state8 slot 6 / raw `0x10` has not fired yet on our own run
    - remaining authenticity gap now narrows further onto the state8 send's still-missing
      per-selection profile/config corpus or other envelope/transport differences
  - helper10/state10 raw `0x0a` / helper11/state11 raw `0x0c` now stay explicit as the later
    claim/create-character branch, not the default existing-character path
- newer original-launcher WineDbg evidence now moves the faithful branch boundary later again:
  - the natural password-submit path hits
    `0x41ecd0 -> 0x41c1f0 -> 0x43bd20 -> 0x41af70 -> 0x41cf30 -> 0x43bf64 -> 0x43bf6c -> 0x43f930 -> 0x439780 -> 0x41de40 -> 0x43c180`
  - natural original therefore reaches both the state8 reply body and the helper9/state9 slot-3 follow-on,
    then naturally enters the owner/helper submit path behind it, and later reaches the state9 slot-6 reply body too
  - representative live stop sequence showed:
    - at `0x439780`: helper9 local byte `this+4 = 0`, word `this+6 = 0x2710`
    - at `0x41de40`: `ECX = 0x004d4e38`, `EAX = 0x2710`, `EDX = 0x004b517c`
    - at `0x43c1c2` (state9 slot-6 success side): parsed status `0`, owner `+0x80 = 0`
  - representative natural run at the `0x43c180/0x43c1c2` boundary was visibly at:
    - **Waiting for Regionserver**
  - newer breakpoint-only proof now also closes the immediate post-success tail:
    - `0x41b450(0x0c)`
    - `0x41cfb0(0x18)`
    - later `0x41cfb0(0x0f)`
    - then the run enters game
  - immediate follow-up late probes on likely state-`0x0c` leaf continuations have still stayed
    negative so far:
    - no natural hit yet on `0x004397e0`
    - no natural hit yet on `0x0041c5c0`
  - representative natural stop at `0x41de40` also had a non-null owner callback/object triple at
    `+0x84/+0x88/+0x8c`
  - owner `+0x664` (`GameSessionID`) was still zero at the natural state8 send site
  - practical state9 consequence now tightens again:
    - the old “does natural original ever reach `0x43c180`?” question is closed
    - `0x43c180` success-side branch is now live-proven on the natural path
    - the immediate post-success boundary is now also live-proven as helper-switch + event flow,
      not immediate shared-final-leaf slot-6 flow
    - but the easy immediate-leaf theory is weaker now:
      - the first follow-up late probes on `0x004397e0` / `0x0041c5c0` did not fire naturally
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
    **post-state9 continuation after the now-proven `0x43f930 -> 0x439780 -> 0x41de40 -> 0x43c180` path**,
    especially the concrete state-`0x0c` continuation after the now-proven state9 success-side
    event `0x18`
  - current strongest visible-status clue at that later boundary is now:
    - natural original is already showing **Waiting for Regionserver** around the
      `0x43c180/0x43c1c2` success-side window
  - current strongest negative-result clue there is also now:
    - follow-up late probes on `0x004397e0` / `0x0041c5c0` did not fire naturally
  - keep the deeper owner/collaborator behavior under `0x41de40` and `0x41b420` source-owned
    before broadening back into helper11-first theories
- keep only two narrow state-8 leftovers explicit in source for now:
  - non-`0x10` slot-6 fallback through `0x41c5c0`
  - section-`0x0b` side effect through `0x43f8c0`
- the immediate continuation after successful state-8 completion is now also live in original
  through helper9/state9 slot 3 (`0x439780`), the deeper owner/helper submit path `0x41de40`,
  and the later state9 slot-6 reply body `0x43c180`; the next natural-original area to tighten is
  now the post-state9 / state-`0x0c` continuation, with `0x41de40` and `0x41b420` kept
  source-owned as already-proven bridges
- forced-`RunClientDLL` scaffold evidence now proves a closer existing-character branch too:
  - preserve helper state `0x08` through `AS_AuthChallengeResponse`
  - route successful `AS_AuthReply` back onto the existing-character state8 path
  - consume margin connect-status and promote the connection into the ready state required by
    `0x41b4b0`
  - send the structured state8 raw-`0x0f` / `MS_LoadCharacterRequest` packet
- so keep two distinct truths explicit:
  - **original launcher live boundary now crossed**: natural original reaches the state8 send tail,
    `0x43f930`, `0x439780`, `0x41de40`, `0x43c180`, then `0x41b450(0x0c)`, `0x41cfb0(0x18)`, and
    later `0x41cfb0(0x0f)` before entering game
  - **replacement launcher active boundary moved later**: the current existing-character scaffold
    now reaches the same raw-`0x0f` state8 send family, but has not yet produced the later incoming
    state8 raw-`0x10` / `MS_LoadCharacterReply`
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

1. Treat the now-confirmed password-submit branch through state `8` and deeper into state9 as
   **closed enough in source**, and update the next faithful original-live question accordingly:
   - `0x41ecd0`
   - `0x41c1f0`
   - `0x43bd20`
   - `0x41af70`
   - `0x41cf30`
   - `0x43f930`
   - `0x439780`
   - `0x41de40`
   - `0x43c180`
   - immediate next task is now the later post-state9 / state-`0x0c` continuation, while keeping
     the already-proven owner/helper submit bridge under `0x41de40` and success-side effect path
     under `0x41b420` tight in source
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
   right now that means deeper state9 / post-state9 continuation outranks speculative helper11
   producer work
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
     `0x43f930`, the helper9/state9 slot-3 follow-on `0x439780`, then `0x41de40`, the state9
     slot-6 reply path `0x43c180`, then the post-success tail `0x41b450(0x0c) -> 0x41cfb0(0x18)`
     and later `0x41cfb0(0x0f)` before entering game
   - keep `0x41de40`, `0x43c180`, `0x41b420`, `0x41b450`, and `0x41cfb0` source-owned / documented
     as the now-proven natural bridge family
   - do not assume that state-`0x0c` immediately means natural hits on `0x004397e0` / `0x0041c5c0`:
     the later breakpoint-only run still entered game without either hit
   - next highest-value static question is now the event-consumer/listener path behind owner `+0x674`
     and the concrete meaning of the later `0x0f` post on that continuation
3. Keep the helper10/helper11 story explicit but secondary while doing that:
   - helper10/state10 raw `0x0a` and helper11/state11 raw `0x0c` now read as the
     claim/create-character branch
   - that branch is no longer the best default replacement-launcher path for the current account
   - even deliberately non-empty helper11 payload seeding was already proven insufficient by itself
4. In parallel, tighten the new replacement-launcher blocker directly:
   - incoming state8 slot 6 / raw `0x10` prerequisites after the now-live state8 raw-`0x0f` send
   - any remaining transport/envelope differences between our state8 send and the natural path
   - any launcher-owned startup state still needed before the first real state8 reply arrives
5. Once the original-live event-consumer side is better explained, connect it back to the
   replacement-launcher gap:
   - why original reaches the later `0x18 -> 0x0f` event sequence
   - why replacement now reaches state8 raw `0x0f` send but still does not receive the later raw
     `0x10`
6. Keep replacing raw queue/work/context byte-offset usage with documented source-level views when evidence is strong enough
7. For each high-value active-path function, sync Ghidra names and types with source using function rename, variable rename, and variable/parameter retyping as understanding improves
8. Prune stale or resolved notes from this file instead of letting it grow again
