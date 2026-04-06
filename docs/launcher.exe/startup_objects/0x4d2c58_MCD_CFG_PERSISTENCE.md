# `ILTLoginMediator.Default` mcd.cfg persistence subset

Focused canonical note for the `mcd.cfg`-relevant arg6 / owner persistence family.

Use this instead of re-spreading the same facts across broader arg6 notes.

## Purpose

Track the launcher-owned state that the real `client.dll` uses when it persists selection/profile data to
`mcd.cfg`.

This is the narrow subset currently most relevant to the success criterion:
- replacement launcher drives the real `client.dll`
- `client.dll` writes `mcd.cfg`
- output matches original launcher + original `client.dll`

## Original client-side save chain

Current best original chain:

1. early client init / engine-init helper sets the save-enable byte
2. later event `0x0b` reaches the client-side cfg-save handler
3. cfg-save handler reaches the `mcd.cfg` adopt/save family
4. that family reads launcher arg6 persistence getters
5. then the direct `mcd.cfg` writer emits the file

## Proven client anchors

### Save-enable byte writer
- `client.dll:0x62199fd0`
  - sets `0x629e95a8[0] = 1`
  - also clears `0x629e95a8[0x18] = 0`
  - gated by mediator `+0x10`
  - calls the early cfg helpers behind `+0x48` / `+0x4c`
- caller now proven as:
  - `client.dll:0x6216f060`

### Event-side cfg-save handler
- `client.dll:ClientShell_LoginMediatorObserver_OnEvent` (`0x621707e0`)
- original live pass now proved event sequence reaching it includes:
  - `0x10`
  - `0x0e`
  - `0x11`
  - `0x09`
  - `0x12`
  - `0x17`
  - `0x0b`
- the important active save-side hit is:
  - event `0x0b`

### Dirty-corpus save family
- `client.dll:0x62199ed0 = PersistSelectionCfgCorpusIfDirty`
- original live pass proved:
  - reached from `0x621707e0` on event `0x0b`
  - at entry:
    - `ECX = 0x629e95a8`
    - `*(byte*)0x629e95a8 = 1`
    - `*(byte*)(0x629e95a8 + 0x18) = 0`

### mcd adopt/save family
- `client.dll:0x62198fa0 = LoadOrAdoptSelectionMcdStateAndMaybePersist`
- original live pass proved it is reached from the same active save family

### Direct writer
- `client.dll:0x62197830 = PersistSelectionMcdCfgFromMediatorF4`
- original live pass proved pre-write locals at `0x62197ad1`:
  - header dwords:
    - `0x6`
    - `0x69`
    - `0x82`
    - `0x71`
    - `0x7`
    - `0x5`
    - `0x7a`
    - `0x0`
  - body first dword:
    - `0x130`
  - body dwords later sampled from the same pass:
    - `+0x444 = 0x00030d40`
    - `+0x448 = 0x11a5dc44`

## Proven launcher arg6 getters used by the original client

Original live + static tightening now proves this getter family on the active `mcd.cfg` path:

- `+0x8c  -> 0x41f150 = CLTLoginMediator_HasState8PersistenceData8c`
  - returns owner byte `+0x13f6`
  - corrected sibling distinction: owner byte `+0x1452` belongs to the neighboring live `cl.cfg` gate at arg6 `+0x88`, not the `mcd.cfg` gate
- `+0xbc -> 0x41f170 = CLTLoginMediator_GetState8PersistenceHeaderF48`
  - returns owner `+0xf48`
- `+0xc0 -> 0x41f180 = CLTLoginMediator_GetState8PersistenceBodyF88`
  - returns owner `+0xf88`
- `+0xc4 -> 0x41aec0 = CLTLoginMediator_GetState8PersistenceOverflow13f0`
  - returns owner `+0x13f0`
  - optional out-length from owner `+0x13f4`
- `+0xf4 -> 0x41f1c0 = CLTLoginMediator_GetState8PersistenceF1c`
  - tiny getter returning owner `+0xf1c`

Sibling late getters in the same cluster:
- `+0xc8 -> 0x41f190 = CLTLoginMediator_HasState8Section11Dword145c`
- `+0xcc -> 0x41f1a0 = CLTLoginMediator_GetState8Section11Dword145c`
- `+0xd0 -> 0x41f1b0 = CLTLoginMediator_GetState8Section11String1460`

## Proven original owner producer

The real producer behind this family is the earlier load-character reply materialization path:
- `launcher.exe:0x43f930 = CLTLoginState_State8::Slot6_HandleSecondaryMessage`

Current strongest owner fields there:
- `+0xf1c`
- `+0xf48`
- `+0xf88`
- `+0x13f0/+0x13f4`
- `+0x13f6`
- `+0x145c/+0x1460`

## Replacement current status

### 2026-03-22 breakthrough
The replacement launcher now reaches the real client save path and the real `client.dll` now writes a
fresh `mcd.cfg` on the replacement run.

Current reference pair:
- replacement:
  - `~/MxO_7.6005/Profiles/morgan/Reality_3ED980/mcd.cfg`
- original launcher + original client:
  - `~/.wine/drive_c/users/morgan/AppData/Local/The Matrix Online/Profiles/morgan/Morg4n_6DCE/mcd.cfg`

Current evidence from the completed comparison:
- both files are `1218` bytes
- they are **bit-identical**
- shared SHA-256:
  - `9bdd22d0c69affd70d5b4436534d58b58025856d1105988e14f636b286ada0bb`

That is stronger than the session success criterion required for file-content parity.

### Replacement-side chain now observed live
Replacement-side state8 producer logs still match the key original header/body facts:
- `f48[0..3] = [0x00000006 0x00000069 0x00000082 0x00000071]`
- `f88_00 = 0x00000130`
- `overflow13f4 = 0x0029`
- neighboring `cl.cfg` gate `1452 = 1` is also present on the same active reply materialization path, but that field is no longer the `mcd.cfg` gate after the corrected `0x41f150 -> +0x13f6` proof

And the replacement now also shows the later client-side save chain concretely enough to close the
mcd-content parity question:
- event `0x0b` is dispatched through the observer bridge
- replacement reaches `client.dll:0x62197560` and needs arg6 `+0x44`
- replacement reaches the mediator-backed save family:
  - `+0xbc`
  - `+0xc0`
  - `+0xc4`
  - `+0xf4`
- replacement then enters the direct writer family at `client.dll:0x62197830`
- the written file matches the original exactly

### Newly isolated narrow dependency that mattered
The first exact missing transition blocking the replacement was not broad auth/bootstrap or broad
state9 work.
It narrowed to the pre-save client helper family immediately before the dirty-corpus walk:
- `client.dll:0x62197560`
- that helper gates on arg6 `+0x44` before continuing deeper
- replacement source now provides a minimal current-slot record object there
  - name
  - id low/high
  - status byte
  - world id word

### Still-open fidelity note, but no longer a blocker for file-content parity
Replacement logs now also show a broader pre-`0x62198fa0` client family at:
- bool-like gates `+0x68/+0x6c/+0x70/+0x74/+0x78/+0x7c/+0x80/+0x84/+0x88/+0x90`
- paired pointer/out-length getters `+0x94/+0x98/+0x9c/+0xa0/+0xa4/+0xa8/+0xac/+0xb0/+0xb4/+0xb8`

Current replacement behavior there is deliberately narrow:
- return `0` / `NULL`
- let the client take its fallback/default path
- then continue into the already-proven mediator-backed `mcd.cfg` adopt/save family

That exact original launcher-side semantic family is still unresolved, but it is **not** the active blocker for
`mcd.cfg` content parity anymore.

## Replacement source / log anchors

Producer-side logs:
- `matrixstaging/game/src/libltclientlogin/loginstate_state8.cpp`
  - `CLTLoginState_State8 persistence family [...]`

Getter-side logs:
- focused source-owned ABI home now lives under:
  - `src/launcher_mediator_abi_mcd.cpp`
  - wrappers there are now thin forwarders for the `mcd.cfg`/section11 family
- class-owned logging and scratch/state assembly now live under:
  - `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- key log anchors:
  - `CLTLoginMediator::GetState8PersistenceHeaderBc(+0xbc)`
  - `CLTLoginMediator::GetState8PersistenceBodyC0(+0xc0)`
  - `CLTLoginMediator::GetState8PersistenceOverflowC4(+0xc4)`
  - `CLTLoginMediator::HasState8Section11Dword145c(+0xc8)`
  - `CLTLoginMediator::GetState8Section11Dword145c(+0xcc)`
  - `CLTLoginMediator::GetState8Section11String1460(+0xd0)`
  - `CLTLoginMediator::GetState8PersistenceF1c(+0xf4)`

Observer/event bridge relevant to entering the save family:
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
  - `dispatching observer=... event=0x0b`

## Refreshed active reference status (2026-04-06)

After the narrow `+0xf4` tightening, a refreshed original reference run on the active
`Morg4n_6DCE` route now brings `mcd.cfg` back to bit-identical parity with the current replacement
output.

Current checked pair:
- original reference corpus observed under:
  - `~/.wine/drive_c/users/morgan/AppData/Local/The Matrix Online/Profiles/morgan/Morg4n_6DCE/`
- current replacement corpus observed under:
  - `~/MxO_7.6005/Profiles/morgan/Morg4n_6DCE/`

Current content result:
- `mcd.cfg`
  - original md5: `a0ae34af5211ae1c45ecf4735ece388e`
  - replacement md5: `a0ae34af5211ae1c45ecf4735ece388e`
  - bit-identical again
- remaining neighboring diffs are now outside this narrowed `mcd.cfg` body question:
  - original-only `btl.cfg`
  - saved `cs.cfg` five-byte low-bit family `{0,1,7,15,19}`

Current project scope note from the latest user clarification:
- the original temp-copy + alternate-profile-root behavior is **not** a desired reimplementation
  target for now
- so the active fidelity question here is file **content**, not reproducing the original temp/
  AppData path side effects

## Practical next step

For `mcd.cfg`, keep the issue closed unless a new concrete mismatch appears.

The remaining cfg-parity work should stay on its own exact client-owned surfaces:
1. `btl.cfg`
   - later bulk-save writer `client.dll:0x62196b80 = PersistSelectionBtlCfg`
2. saved `cs.cfg` final-form parity
   - later save path centered on
     `client.dll:0x621966d0 = PersistSelectionCsCfg`
   - current five-byte difference still matches the already-documented later-save ordering issue
3. keep the pre-`0x62198fa0` live-corpus family (`+0x68 .. +0xb8`) separate unless a new concrete
   mismatch forces it back open

Do not regress into broad client-side archaeology unless a new concrete parity failure appears.
