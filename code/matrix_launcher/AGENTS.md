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
  - latest clean `run_binder_both_runtime` validation now re-proves the launcher-owned auth path,
    then continues through the launcher-owned margin CERT/MS bootstrap instead of stalling before it
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
  - newer client-side selection-context tightening now also corrected one launcher/client ownership
    read around that send:
    - the copied `+0xec` selection/config object still carries repeated cfg-parser/default-looking
      values in the `+0x24..+0xb3` tail when the client-owned per-selection cfg corpus is missing
    - but current launcher-side scaffold now preserves those client-supplied snapshot blocks by
      default for faithful launcher-only flow
    - the old zeroing hack only remains as a temporary escape hatch for narrow reruns behind
      `MXO_DIAGNOSTIC_SANITIZE_SELECTION_CFG_DERIVED_BLOCKS=1`; it is not part of the faithful path
  - this moves the replacement boundary later and closer to the natural-original shape than the old
    helper11-only stall
  - newer validation now closes the old state8 blocker later too:
    - launcher-owned margin bootstrap now runs in order on the deliberate runtime path:
      `0x01 -> 0x02 -> 0x03 -> 0x04 -> 0x06 -> 0x07 -> 0x08 -> 0x09`
    - bootstrap completion now returns control to state8 slot 3
    - post-bootstrap state8 raw `0x0f` is now sent on encrypted margin transport
    - the first decrypted incoming raw `0x10` now arrives and routes through state8 slot 6
    - state8 reply progression now completes far enough to switch into helper9/state9 with event `0x0b`
    - practical consequence: the active replacement boundary has moved past the old
      "missing first raw `0x10`" blocker and into the later state9 / post-state9 continuation
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
- the active state-8 send body is now **closed enough** for the existing-character path:
  - natural original uses raw `0x0f` / `MS_LoadCharacterRequest`
  - replacement now reaches the same raw-`0x0f` send family with the later send bridge
    `0x41af70 -> 0x41cf30`
- the old "margin connect-status -> direct state8 raw `0x0f`" framing is no longer acceptable:
  - open server/proxy evidence currently points at:
    - plaintext `CERT_ConnectRequest (0x01)`
    - plaintext `CERT_Challenge (0x02)`
    - encrypted `CERT_ChallengeResponse (0x03)`
    - encrypted `CERT_ConnectReply (0x04)`
    - encrypted `MS_ConnectRequest (0x06)`
    - encrypted `MS_ConnectChallenge (0x07)`
    - encrypted `MS_ConnectChallengeResponse (0x08)`
    - encrypted `MS_ConnectReply (0x09)`
    - only then later encrypted load-family traffic like raw `0x0f`
- latest clean runtime validation after the auth receive fix and relaxed `MS_ConnectReply` prefix
  parse now pushes the active replacement boundary later again:
  - auth now again progresses through `AS_GetPublicKeyReply`, `AS_AuthChallenge`, and `AS_AuthReply`
  - margin bootstrap now runs and completes on the deliberate runtime path
  - encrypted post-bootstrap state8 raw `0x0f` send is now live-proven
  - decrypted incoming raw `0x10` traffic is now live-proven and routed through state8 slot 6
  - state8 reply progression now completes and switches into helper9/state9 with event `0x0b`
  - a narrow source-owned continuation bridge now also re-enters helper9/state9 slot 3 on that
    proven handoff
  - the immediate blocker has therefore tightened again:
    - current deliberate state9 still reaches `0x41de40`-owned submit scaffolding on the proven run
    - the origin of the owner collaborator triple is now narrowed/source-owned:
      owner/arg6 vtable `+0x124(netShell, netMgr, distrObjExecutive)` -> `0x41f1d0`
      -> owner `+0x84/+0x88/+0x8c`
    - but keep one corrective runtime note explicit:
      attempting to mirror that arg6 `+0x124` triple directly into the live replacement runtime path
      regressed the deliberate binder run, so that bridge was backed back out
    - newer crashdump proof now narrows that failed bridge further:
      `MatrixOnline_0.0_crash_69.dmp` stops on the replacement `0x41de40` mirror at the first
      callback84-side `+0x38` query, before any later object88 branch work runs
    - newer client-side static tightening now also explains why that direct reuse is too weak:
      callback84 currently resolves to `ClientNetShell +0x38 / 0x62006580`, and that wrapper
      re-enters the client-side resolved `ILTLoginMediator.Default` global `0x629df7f0`, then
      calls its `+0x18c(&0x629e0284, 900, 0)` writer before returning pair `(&0x629e0284, 0x20)`
    - so the next active blocker remains the deeper `0x41de40` collaborator execution, with the
      callback84 side now the first concrete subtarget before `+0x88 -> (+0x44)->(+0x30)` and the
      later `0x44afd0/0x44b0d0` / submit-call work
- keep two distinct truths explicit:
  - **original launcher live boundary now crossed**: natural original reaches the state8 send tail,
    `0x43f930`, `0x439780`, `0x41de40`, `0x43c180`, then `0x41b450(0x0c)`, `0x41cfb0(0x18)`, and
    later `0x41cfb0(0x0f)` before entering game
  - **replacement launcher active boundary moved later but is still incomplete**:
    current existing-character scaffold now reaches and completes the old state8 bootstrap/reply
    barrier (`0x09` bootstrap completion, encrypted raw `0x0f`, decrypted raw `0x10`, helper9/state9
    switch), but has not yet been tightened through the later natural-original state9/post-state9 tail
- practical ownership split to preserve:
  - state4 / `0x439300` + mediator `0x41e500` starts margin work
  - low-level margin CERT/MS bootstrap appears to belong primarily to the margin connection family
  - later state8/state11/state9 slot-6 bodies consume only reply traffic that survives base margin dispatch
- broader post-auth writeback is still incomplete:
  - launchpad-owned success mirrors for owner `+0x660`, owner `+0x664`, and owner `+0x94`
    first-string consequences are source-owned
  - but broader launcher-owned startup/runtime state is still missing
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

1. Finish the missing launcher-owned margin bootstrap between state4/`0x41e500` and the existing-character state8 raw `0x0f` send:
   - keep the ownership split explicit:
     - state4 / `0x439300` starts margin work
     - margin connection family owns the CERT/MS bootstrap surface
     - only after bootstrap completion should control naturally return to the active state slot-3 sender
2. Keep the state8 send body itself treated as closed enough unless new evidence shows a concrete mismatch:
   - raw `0x0f`
   - natural/replacement send bridge `0x41af70 -> 0x41cf30`
   - keep only the narrow explicit leftovers:
     - non-`0x10` slot-6 fallback through `0x41c5c0`
     - section-`0x0b` side effect through `0x43f8c0`
3. Keep work in lockstep across:
   - Ghidra names/types
   - source implementation
   - canonical docs
4. Use original-launcher evidence to steer ownership/sequence, and use open server/proxy code only to recover packet/crypto requirements
5. Once the replacement reaches the first real state8 raw `0x10`, retighten focus on the later natural-original post-state9 / state-`0x0c` continuation:
   - `0x43f930`
   - `0x439780`
   - `0x41de40`
   - `0x43c180`
   - `0x41b420`
   - `0x41b450(0x0c)`
   - `0x41cfb0(0x18)`
   - later `0x41cfb0(0x0f)`
6. Continue active-path arg5 queue work where it directly helps the game-running path, but do not let it displace the current margin-bootstrap blocker
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
make
```

Safe run:
```bash
make run
```

Current deliberate binder path:
```bash
MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both
```

Current deliberate binder runtime path:
```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both_runtime
```

## Immediate next tasks

1. Treat the old state8/bootstrap blocker as closed enough in source for the deliberate path:
   - auth receive rerun regression is fixed
   - launcher-owned margin bootstrap now completes on the active path
   - encrypted state8 raw `0x0f` send is live
   - decrypted raw `0x10` receive is live and routed through state8 slot 6
2. Retighten the immediate replacement-launcher question on helper9/state9 specifically:
   - keep the narrow helper9 continuation bridge explicit
   - source-own more of `0x41de40 = CLTLoginMediator_State9SubmitFollowup`
   - keep the now-identified owner collaborator origin explicit:
     owner/arg6 `+0x124(netShell, netMgr, distrObjExecutive)` -> `0x41f1d0` -> `+0x84/+0x88/+0x8c`
   - do **not** force that arg6 `+0x124` triple directly into the live replacement runtime path
     again until the deliberate state8/state9 path is locked back down
   - use the new crashdump-backed narrowing to order the work inside `0x41de40`:
     callback84 `+0x38` first, then only later object88 `(+0x44)->(+0x30)` / submit-path work
   - callback84-first now specifically means:
     `ClientNetShell +0x38 / 0x62006580 -> client resolved ILTLoginMediator.Default 0x629df7f0 -> +0x18c(&0x629e0284, 900, 0) -> pair (&0x629e0284, 0x20)`
   - launcher-side `+0x18c` is now also narrowed:
     `0x41e690 = CLTLoginMediator_FillState9CallbackBlob18c`
     - state9-gated
     - fills fixed `0x20` bytes
     - first half = current slot id low/high + caller args
     - blob `+0x10` also copies owner `+0xf18`
     - tail is materialized through the shared `ValueNames` / `FeedbackSize` helper family from
       mediator `+0xd4 = 0x41b4f0 -> owner +0x1c + 0x85`
   - tighten the remaining `0x41de40` gap on the deeper collaborator execution / submit path,
     not on generic placeholder stuffing
   - only then expect later raw `0x11` / state9 slot-6 progression
3. Keep state `8` closed enough in source while doing that:
   - keep only the narrow explicit leftovers:
     - non-`0x10` fallback through `0x41c5c0`
     - section-`0x0b` side effect through `0x43f8c0`
4. Once the replacement reaches the first real raw `0x10`, retighten focus on the later natural-original bridge family already proven live:
   - `0x43f930`
   - `0x439780`
   - `0x41de40`
   - `0x43c180`
   - `0x41b420`
   - `0x41b450`
   - `0x41cfb0`
5. Keep replacing raw queue/work/context byte-offset usage with documented source-level views when evidence is strong enough
6. For each high-value active-path function, sync Ghidra names and types with source using function rename, variable rename, and variable/parameter retyping as understanding improves
7. Prune stale or resolved notes from this file instead of letting it grow again
