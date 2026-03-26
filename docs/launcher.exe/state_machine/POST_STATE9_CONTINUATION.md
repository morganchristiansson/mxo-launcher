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

### Replacement launcher
Current active existing-character path is source-owned/live enough to reach:
- state8 raw `0x0f`
- state8 raw `0x10` receive and helper9/state9 handoff
- `0x41de40` submit followup
- state9 raw `0x11` success
- `0x41b420`
- helper switch into state `0x0c`
- event `0x18`
- late arg6 observer bridge through:
  - `+0x170` registration
  - `+0x10c` route-descriptor getter
  - `+0x118` late-entry-list getter (currently empty scaffold)
- later second observer registration after event `0x18`
- first entry into game on the active existing-character path

So the old state9 submit blocker is closed enough for the active path.

## Current anchored pieces

### `0x41b420 = CLTLoginMediator_HandleState9Opcode11SuccessSideEffect`
Current best read:
- reached by state9 slot-6 / `0x43c180` success
- if owner margin connection is absent, returns false-ish
- clears owner byte `+0xf14`
- sets owner byte `+0x2d`
- if margin connection state is `1` or `2`, calls connection vtable `+0x0c(1)`

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
- a minimal arg6/observer registration bridge is now source-owned too
- the original listener tree container itself is still unresolved

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
  - `0x41ddb0`: insert observer into owner `+0x674`
  - `0x41dde0`: remove observer from owner `+0x674`
  - `0x41f240`: return owner `+0x80`
  - `0x4202c0`: pump owner helper `+0x65c` vtable `+0x04` when present
- important correction:
  - old replacement-side names like `AttachStartupContext(+0x170)` and
    `ConsumeRuntimeDescriptor(+0x178)` were misleading for this late-runtime surface
- concrete client-side proof now also shows one startup-era `+0x170` caller is already observer-like:
  - `client.dll:0x62056585` pushes observer object `0x6298a760` into `+0x170`
- replacement-side runtime progress now tightens the event-`0x18` callback path further too:
  - event-`0x18` observer callback now reaches and consumes arg6 `+0x10c`
  - it also now tolerates an empty arg6 `+0x118` late-entry list scaffold
  - after that callback, the replacement later reaches a second observer registration:
    - `client.dll:0x62031136`
    - observer object `0x6298a5e8`

Practical consequence:
- the client-visible observer registration bridge is no longer speculative
- the current replacement can source-own a minimal observer list and enough late arg6 getters to
  reach first game entry without claiming the original red-black-tree or late-entry-list container
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

## Current blocker

The old post-state9 / state-`0x0c` continuation blocker is no longer the active one.

The active replacement boundary has moved later, into the post-entry late-runtime surface behind the
now-live arg6 observer bridge.

Most likely missing concrete work now lives in one of these areas:
1. stabilizing or understanding the later second observer registration (`client.dll:0x62031136`,
   object `0x6298a5e8`)
2. replacing placeholder late arg6 data surfaces (`+0x118`, possibly later siblings) with more
   faithful content once runtime evidence demands it
3. checking replacement parity against the natural-original later `0x438df0 -> 0x41cfb0(0x0f)`
   tail and any post-entry follow-on behavior

## First files to read next session

- `matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `src/launcher_mediator_abi.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9_submit_scaffold.h`
- `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`
- `../../docs/launcher.exe/VTABLES/0x004b517c.md`
- `../../docs/launcher.exe/VTABLES/0x004b5230.md`
- `../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`

## Related docs

- `../startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- `../auth/STATUS.md`
