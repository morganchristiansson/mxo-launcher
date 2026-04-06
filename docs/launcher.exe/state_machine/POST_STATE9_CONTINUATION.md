# post-state9 continuation

Focused canonical note for the existing-character path after state9 success.

Use this file first for work after:
- `0x439780`
- `0x41de40`
- `0x43c180`

Do **not** start from broad auth history or helper11/create-character notes unless new evidence
forces a branch change.

## Proven path

### Natural original
Natural original is now live-proven through:
- `0x43f930`
- `0x439780`
- `0x41de40`
- `0x43c180` success side
- `0x41b450(0x0c)`
- `0x41cfb0(0x18)`
- later `0x41cfb0(0x0f)`
- then entry into game

Representative visible UI at the `0x43c180` boundary:
- **Waiting for Regionserver**

### Representative minimal EULA attach pass (stable original launcher-spawned `matrix.exe`, `-nopatch`)
Pass conditions:
- original launcher-spawned temp `matrix.exe`
- attached at a stable EULA UI point
- narrow breakpoint set centered on:
  - `0x41c1f0`
  - `0x43bd20`
  - `0x439780`
  - `0x41de40`
  - `0x43c180`
- then `finish` from `0x43c180` to confirm the success-side helper install

Concrete observed chain from the already-proven state3 wait into later states:
- `0x41c1f0`
  - `ECX = 0x004d4e38`
  - owner `+0x10 = 0x00f9b9d0`
  - current helper vtable `= 0x004b5208` (state `3`)
  - input pointer at `ESP+4 = 0x0031fac0`
  - input first dword `= 0`
- next direct stop: `0x43bd20`
  - `ECX = 0x00f9ba70`
  - current helper vtable `= 0x004b5104` (state `8`)
  - owner `+0x10` pointed at that same state8 object
- later direct stop: `0x439780`
  - `ECX = 0x00f9bad0`
  - current helper vtable `= 0x004b517c` (state `9`)
  - helper byte `+4 = 0`
  - helper word `+6 = 0x2710`
  - owner `+0x10` pointed at that same state9 object
- immediate owner submit stop: `0x41de40`
  - `ECX = 0x004d4e38`
  - `EAX = 0x2710`
  - `EDX = 0x004b517c`
  - owner `+0x10` still pointed at the same state9 object / vtable
- later reply stop: `0x43c180`
  - `ECX = 0x00f9bad0`
  - current helper vtable `= 0x004b517c` (state `9`)
- `finish` from `0x43c180` then landed at `0x44af4b` with:
  - owner `+0x10 = 0x00f9baf0`
  - new helper vtable `= 0x004b5230` (state `12`)
  - new helper byte `+4 = 1`
  - owner `+0x80 = 0`

Practical read from that single narrow pass:
- the happy path really does progress:
  - state `3` wait -> owner `0x41c1f0`
  - state `8` at `0x43bd20`
  - state `9` at `0x439780 / 0x41de40 / 0x43c180`
  - state `12` immediately after successful return from `0x43c180`
- keep `0x43f930` as the already-proven state8 reply-stage step on this same happy path; this
  particular narrow pass was used to tighten the direct state8->state9->state12 handoff shape
  without broadening the breakpoint set back out again

### Replacement launcher
Current active existing-character path is source-owned/live enough to reach:
- state8 raw `0x0f`
- state8 raw `0x10` receive and helper9/state9 handoff
- immediate helper9/state9 slot-3 continuation during the `0x41b450` install itself
  - practical correction: helper9 submit must happen before the later event `0x0b` observer/UI work
  - the older source-owned event-`0x0b` continuation bridge was temporally too late on the active path
- `0x41de40` submit followup
- state9 raw `0x11` success
- `0x41b420`
- helper switch into state `0x0c`
- event `0x18`
- late arg6 observer bridge through:
  - `+0x170` registration
  - `+0x10c` route-descriptor getter
  - `+0x118` late-entry-list getter
- newer successful-run caller logging now tightens that post-`0x18` surface further:
  - `+0x10c` is hit from `client.dll:ClientShell_LoginMediatorObserver_OnEvent` return site
    `0x6217082b`
  - `+0x118` is hit immediately from `0x621c6d90`, the event-`0x18` late-entry/loading-area
    setup helper
  - the replacement now mirrors state6 opcode-`9` success closely enough to populate that list with
    17 filename entries mapped from the decoded metric ids through the loaded client METR table
  - source now also mirrors the original `0x41f840 -> 0x41f640` append contract more closely by
    keeping copied owned filename strings behind owner `+0x1470`, so later `+0x118` consumers do
    not borrow transient caller buffers
  - even with that populated list, the same successful run still did **not** show a later
    `+0x118` caller from `0x62017150`
  - and did **not** show a later `+0x170` caller from `0x62031136`

Newest replacement milestone that closed the old state9 submit blocker:
- the active timeout at `0x41de40` was caused by duplicate replacement bootstrap handling of
  `MS_ConnectChallenge` / later extra `MS_ConnectReply`
- on that broken run family:
  - bootstrap sent a second challenge-response after the first had already advanced the state
  - server then produced an extra later `MS_ConnectReply` with a different session id outside the
    proven state6 slot-6 route
  - state9 submit returned `0x00000003`
- duplicate-bootstrap handling on the replacement path is still a narrower open fidelity seam,
  not a closed single-shot conclusion:
  - one bad run family showed duplicate replacement handling of `MS_ConnectChallenge` / later extra
    `MS_ConnectReply`, and state9 submit then returned `0x00000003`
  - but a stricter follow-up that consumed duplicate opcode-`7` packets outright stalled earlier at
    visible `Loading Character`
  - current static RE now outweighs that temporary single-shot guess:
    `launcher.exe:0x440780 = CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply` owns opcode
    `7` directly and does not show a bootstrap-phase guard before sending opcode `8`
  - practical current source stance: keep duplicate opcode-`7` resend-capable while state6 is still
    the active receiver, and keep only later/off-route duplicate suppression
- practical result on the active path now matches the original submit boundary again:
  - state9 submit returns `0`
  - event `0x17` is posted
  - later raw `0x11` success reaches state12 and event `0x18`
- newest replacement validation after tightening the decoded-code-`0x04` seam now moves the
  blocker later again:
  - the earlier **Loading Character** stall was caused by a duplicate mediator-owned
    `CERT_ConnectReply -> MS_ConnectRequest` fallback send after the local type-`0x0b`
    state5/state6 continuation had already re-entered state6 slot 3
  - once that duplicate send was removed, replacement again reached:
    - state8 raw `0x10`
    - state9 raw `0x11`
    - state12 / event `0x18`
    - visible **Waiting for Regionserver**
  - current blocker is now a later post-`0x18` crash, not the old state8/state9 transport stall

## Current anchored pieces

### `0x41b420 = CLTLoginMediator_HandleState9Opcode11SuccessSideEffect`
Current best read:
- reached by state9 slot-6 / `0x43c180` success
- if owner margin connection is absent, returns false-ish
- clears owner byte `+0xf14`
- sets owner byte `+0x2d`
- if margin connection state is `1` or `2`, calls connection vtable `+0x0c(1)`
- source now mirrors that more faithfully too by actually invoking graceful close on the live
  replacement path when the same state check passes; this is the best current launcher-side route
  toward the later natural `0x41afc0 -> 0x438df0 -> 0x41cfb0(0x0f)` tail
- important slot-split note:
  - owner-side strongest meaning is still this state9 opcode-`0x11` success side effect
  - launcher teardown also reuses the same vtable slot as a wrapper-facing
    close-and-wait-event-`0x0f` predicate before `WaitForEvent(0x0f)`

Source home:
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`

### `0x41b450 = CLTLoginMediator_SwitchHelperState`
Current best read:
- not just a raw current-state assignment
- calls old helper vtable `+0x0c` with the new helper object
- installs the new helper into owner `+0x10`
- then calls new helper vtable `+0x08` with the old helper object
- direct vtable reads now tighten those offsets to:
  - old helper `+0x0c` -> slot 4
  - new helper `+0x08` -> slot 3 / BeginOrContinue
- important path split:
  - state8/state11 -> helper9/state9 is **not** a tiny-stub case
  - that install immediately re-enters helper9 slot 3 / `0x439780`, so state9 submit belongs before the later event `0x0b/0x16` observer work
  - state9 -> state12 is the tiny-stub pair
- active post-state9 consequence:
  - on the state9 -> state12 switch, old helper slot 4 is the shared tiny stub
  - new helper state12 slot 3 is also the shared tiny stub
  - so `0x41b450` itself is **not** the missing immediate state-`0x0c` body

Current source stance:
- visible state switch is mirrored
- the exact old/new helper call-shape is now source-commented with slot mapping
- no extra active-path behavior is claimed there beyond that evidence

Source home:
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`

### `0x41cfb0 = CLTLoginMediator_PostEvent`
Current best read:
- not just logging
- walks the owner `+0x674` listener tree
- calls observer/UI callbacks
- natural path is now live-proven at least for:
  - event `0x18`
  - later event `0x0f`
- practical immediate-continuation consequence is now tighter:
  - `0x43c180` returns only after the synchronous listener walk inside `0x41cfb0(0x18)`
  - so the immediate next path is best treated as observer/listener work first, not as an already
    proven direct fall into `0x004397e0` / `0x0041c5c0`

Current source stance:
- event history and the narrow helper9 bridge are source-owned
- arg6/observer registration is now source-owned through a std::_Tree-like owner `+0x674`
  scaffold instead of the older flat vector bridge
- source now uses that same tree shape for general in-order event/error walks again, not just for
  a hand-picked subset of events
- the container/traversal shape is documented closely enough for those walks, but
  balancing/color bits and original node-pool recycling are still only partial

Canonical tree/container home:
- `../startup_objects/0x4f78b8_OBSERVER_TREE_PLUS674.md`

Source home:
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `src/launcher_mediator_abi.cpp`

### arg6 / `ILTLoginMediator.Default` observer bridge into owner `+0x674`
New live/runtime proof now tightens the client-facing registration side too:
- resolved arg6 object is the same launcher owner object:
  - `DAT_004d4d60 = 0x004d4e38`
  - vtable `= 0x004b01c8`
- client-facing slots now resolve concretely to launcher functions:
  - `+0x10c -> 0x41f2c0`
  - `+0x118 -> 0x41af50`
  - `+0x13c -> 0x4202c0`
  - `+0x170 -> 0x41ddb0`
  - `+0x174 -> 0x41dde0`
  - `+0x178 -> 0x41f240`
- function bodies now read as:
  - `0x41f2c0`: return owner `+0x30` small-string-like route descriptor
  - `0x41af50`: return owner `+0x1470` vector-like late-entry list
    - newer `0x41f840 -> 0x41f640` tightening now shows those are 12-byte string-triple entries
    - exact active-path producer is state6 slot-6 opcode-`0x09` success (`0x440780`):
      clear `+0x1470`, map each metric id through the client METR table, then append one filename
      entry per id through owner `+0x190`
    - later client consumer `0x62017150` reads the first dword of each entry as a filename-like
      string and maps it through `FUN_622a9cf0` / `METR` metadata
    - source now mirrors that vector more directly as the raw owner `+0x1470/+0x1474/+0x1478`
      header of deep-copied 12-byte string-triple entries, which better matches the original
      `0x41f640 / 0x41f3e0 / 0x41e410 / 0x41eb20` contract than the earlier borrowed-string
      scaffold
  - `0x41ddb0`: insert observer into owner `+0x674`
  - `0x41dde0`: remove observer from owner `+0x674`
  - `0x41f240`: return owner `+0x80`
  - `0x4202c0`: pump owner helper `+0x65c` vtable `+0x04` when present
- important correction:
  - old replacement-side names like `AttachStartupContext(+0x170)` and
    `ConsumeRuntimeDescriptor(+0x178)` were misleading for this late-runtime surface
- concrete client-side proof now also shows one startup-era `+0x170` caller is already observer-like:
  - `client.dll:RsiLayoutsView_ctor` (`0x620557c0`) at `0x62056585` pushes observer object
    `0x6298a760` into `+0x170`
- replacement-side runtime progress now tightens the event-`0x18` callback path further too:
  - event-`0x18` observer callback now reaches and consumes arg6 `+0x10c`
  - newer client renames/plate comments also correct the immediate helper ownership:
    - `0x62130700 = ClientShell_LoginMediatorObserver_PrepareTransition`
    - `0x621704a0 = ClientShell_LoginMediatorObserver_AdvanceState`
    - the older loading-area naming on those two helpers was too strong
  - newer caller logging on a successful replacement game-entry run makes the immediate `+0x118`
    split concrete:
    - the observed caller was `0x621c6db3` inside `0x621c6d90`
    - that is the immediate event-`0x18` late-entry/loading-area setup helper
    - the returned list now held 17 entries on the successful run, starting with:
      `resource/worlds/final_world/slums_barrens_full.metr`
  - client-side static proof still says `+0x118` is a real later consumer surface, not a
    permanently ignorable empty scaffold:
    - `0x62017150` iterates 12-byte entries from arg6 `+0x118`
    - compares metric ids derived from each entry's first-dword string
    - later runtime callers `0x620181f0 / 0x62018250` use that helper
    - newer xref review also matters negatively: `0x62017150` is only reached from those later
      helpers, not directly from the immediate event-`0x18` callback body
  - but the newest successful replacement run did **not** show a `+0x118` call from `0x62017150`
  - static client proof also still says a second observer registration exists at:
    - `client.dll:LoadingAreaCommonLayoutView_ctor` (`0x62030d90`) at `0x62031136`
    - observer object `0x6298a5e8`
  - newest static+runtime explanation for why that second observer is skipped on the working route:
    - event `0x0b` reads mediator `+0xc0` and copies byte `returnedBlock+0x464` into client global
      `DAT_629e689d`
    - latest successful replacement run now logs that source byte concretely as `0x01`
    - `ClientShell_LoginMediatorObserver_AdvanceState` state-0 branch immediately checks
      `DAT_629e689d != 0`
    - when that flag is non-zero, it switches straight to state `2` and skips the later
      `ClientViewFactory_GetOrCreateViewById(0x67)` path entirely
    - that is the current concrete reason the successful replacement route does not reach
      `LoadingAreaCommonLayoutView_ctor` / observer `0x6298a5e8`

Practical consequence:
- the client-visible observer registration bridge is no longer speculative
- the current replacement can now source-own a std::_Tree-like observer container at owner `+0x674`
  closely enough to match the original in-order event/error walks without flattening it into a
  vector, while still not claiming balancing/color bits or late-entry-list container
  implementations are fully reconstructed
- wrapper minimization for this late arg6 family is now tighter too:
  - `src/launcher_mediator_abi.cpp` keeps only thin vtable forwarders for `+0x10c/+0x118/+0x13c`
  - `matrixstaging/game/src/libltclientlogin/loginmediator.cpp` now owns the wrapper-facing
    `+0x10c` small-string-like object, the `+0x118` vector-like scaffold, and the `+0x13c`
    session-helper pump/logging
- this is the strongest current implementation lead for stabilizing post-entry fidelity work

### later `0x0f` bridge now live-confirmed
Current best chain:
- `0x41b420` sets owner byte `+0x2d = 1`
- late natural-original pass then hit shared gate `0x438df0 = CLTLoginState_SharedSlot2Gate`
- live backtrace there showed the concrete caller as:
  - `0x41afc0 = CLTLoginMediator_HandleMarginConnectionCompletionFallback`
  - that helper re-enters current helper vtable `+0x04` / current best read: slot 2
- at the `0x438df0` stop:
  - helper/object `this+4 = 1`
  - owner `DAT_004f78b8 + 0x2d = 1`
- continuing from that stop immediately hit `0x41cfb0` with event `0x0f`
- static `CLTEvilBlockingLoginObserver::WaitForEvent` callers currently prove waits for events:
  - `1`
  - `8`
  - `0x0f`
- no static waiter is isolated yet for event `0x18`

Practical consequence:
- treat event `0x18` as the listener/observer handoff boundary
- treat shared gate `0x438df0` as the now-live-backed later-source for the natural
  `0x41cfb0(0x0f)` tail
- source now also invokes the matching graceful margin close from `0x41b420` / wrapper `+0x16c`
  when margin state is `1/2`, so the remaining question is runtime reach/order, not whether the
  close call itself is still missing from the replacement
- earlier successful replacement runs still did **not** hit that later close-completion /
  event-`0x0f` bridge before entering game, so this remains a fidelity question rather than a
  current game-entry prerequisite on the replacement route
- this strengthens the case that the post-state9 continuation remains observer/event-driven before
  any later unproven `0x004397e0` / `0x0041c5c0` involvement

### state `0x0c` / state12 final leaf
Strongest current identity lead:
- `0x004b5230 = CLTLoginState_State12`
- constructor sets `this+4 = 1`
- that enables the shared final-leaf slot-6 path `0x004397e0 -> 0x41c5c0`

Important caution:
- this is still the strongest **state identity** lead, not a proven immediate execution leaf
- natural late probes on:
  - `0x004397e0`
  - `0x0041c5c0`
  have remained negative so far

Practical consequence:
- do not collapse post-state9 reasoning into “state12 shared slot-6 is definitely next”
- prioritize the helper-switch/event-consumer side first

Source home:
- `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`

## Current remaining fidelity questions

The old state9 submit blocker is no longer active.

Current replacement work has moved later into the post-state12 / event-`0x18` continuation, and
current validation now shows that this is no longer a hard game-entry blocker.

Representative successful replacement run (`2026-04-02`, latest caller-logging + late-entry pass):
- reaches:
  - state9 submit return `0`
  - event `0x17`
  - raw `0x11` success
  - `0x41b420`
  - helper switch to state12 / `0x0c`
  - event `0x18`
- event-`0x18` observer work then visibly consumes:
  - arg6 `+0x10c` route descriptor (`"Reality"`) from caller `0x6217082b`
  - arg6 `+0x118` late-entry list from caller `0x621c6db3` (`0x621c6d90`)
  - that returned list now contains 17 entries populated from state6 opcode-`9` metric ids
- latest source pass also tightens fidelity on two later-runtime seams behind that success:
  - owner `+0x1470` is now mirrored as the raw vector header of deep-copied 12-byte entries
  - `0x41b420` / wrapper `+0x16c` now invoke graceful margin close when the original state test
    says they should
- then replacement continues far enough to:
  - show the in-game `MATRIX_ONLINE` window
  - return `RunClientDLL = 1`

Important remaining fidelity note from that same successful run:
- current replacement logs still did **not** show:
  - later event `0x0f`
  - any later `+0x118` caller from `0x62017150`
  - second observer registration (`0x6298a5e8`) through `+0x170`
- because the ABI shell now logs exact caller addresses for `+0x10c/+0x118/+0x170/+0x174`, the
  missing `0x62017150` / `0x62031136` activity is no longer best read as a logging blind spot
- practical current read:
  - successful replacement game entry already works
  - even after populating the late-entry list, the later metric-matcher work is still a true
    not-currently-taken later runtime path
  - the absent second observer registration is now concretely explained by the
    `DAT_629e689d != 0` shortcut in `ClientShell_LoginMediatorObserver_AdvanceState`
  - later event `0x0f` likewise remains a true missing/not-yet-taken fidelity path, and the
    current launcher-side reason is also concrete now: `0x41b420` sees `marginConnectionState=2`
    and would call close with arg `1`, but the replacement still logs
    `currentReplacementDoesNotInvokeCloseYet=1`

Current strongest remaining concrete questions:
1. whether the still-missing later `0x62017150` path is actually optional on the working route or
   gated by additional client state beyond the now-populated late-entry list
2. whether the now-restored faithful margin close at `0x41b420` is sufficient by itself to make
   the replacement visibly re-hit the natural later `0x41afc0 -> 0x438df0 -> 0x41cfb0(0x0f)` tail,
   or whether another runtime/order dependency is still missing

## First files to read next session

- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `matrixstaging/game/src/libltclientlogin/loginstate.cpp`
- `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- `src/launcher_mediator_abi.cpp`
- `../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
- `../../docs/launcher.exe/startup_objects/0x4f78b8_OBSERVER_TREE_PLUS674.md`
- `../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../../docs/client.dll/VTABLES/0x628836ec.md`
- `../../docs/client.dll/VTABLES/0x6286e924.md`

## Related docs

- `../startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- `../auth/STATUS.md`
