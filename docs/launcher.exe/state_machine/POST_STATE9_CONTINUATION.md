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
Current deliberate existing-character path is source-owned/live enough to reach:
- state8 raw `0x0f`
- state8 raw `0x10` receive and helper9/state9 handoff
- `0x41de40` submit followup
- state9 raw `0x11` success
- `0x41b420`
- helper switch into state `0x0c`
- event `0x18`

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
- notifies old helper with new helper object
- installs the new helper into owner `+0x10`
- then notifies the new helper with the old helper object

Current source stance:
- visible state switch is mirrored
- deeper old/new helper notification slots stay explicit/unresolved

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
- the listener tree itself is still unresolved

Source home:
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`

### strongest current later `0x0f` bridge candidate
Current best static chain:
- `0x41b420` sets owner byte `+0x2d = 1`
- shared gate `0x438df0` posts event `0x0f` when owner `+0x2d != 0`
- static `CLTEvilBlockingLoginObserver::WaitForEvent` callers currently prove waits for events:
  - `1`
  - `8`
  - `0x0f`
- no static waiter is isolated yet for event `0x18`

Practical consequence:
- treat event `0x18` as the listener/observer handoff boundary
- treat shared gate `0x438df0` as the strongest current later-source candidate for the natural
  `0x41cfb0(0x0f)` tail
- do **not** claim that as a fully proven immediate next call without a live late pass

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

The active replacement boundary is now:
- post-state9 / state-`0x0c` continuation after the live state9 success tail

Most likely missing concrete work now lives in one of these areas:
1. owner `+0x674` listener-tree consumers behind `0x41cfb0`
2. state-`0x0c` body/observer continuation after event `0x18`
3. later event-driven bridge that leads to the natural-original `0x41cfb0(0x0f)` hit before game entry

## First files to read next session

- `matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9_submit_scaffold.h`
- `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`
- `../../docs/launcher.exe/VTABLES/0x004b517c.md`
- `../../docs/launcher.exe/VTABLES/0x004b5230.md`

## Related docs

- `../startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- `../auth/STATUS.md`
