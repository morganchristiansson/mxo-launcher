# `ILTLoginMediator.Default` late-login arg6 surface

This note is the focused late-login subset of:
- `0x4d2c58_ILTLoginMediator_Default.md`

Use this file first when working on the existing-character late path after state8.
Do **not** start from the broader startup-selection doc unless you actually need early arg6 setup.

## Scope

Current active late-login arg6 surface:
- `+0xd4`
- `+0x124`
- `+0x18c`

These three slots are the arg6-side inputs that matter most for the current
`0x439780 -> 0x41de40 -> 0x43c180` path.

Closely related later continuation siblings now live in the post-state9 observer bridge instead of
this narrower submit surface:
- `+0x10c`
- `+0x118`
- `+0x13c`

See:
- `../state_machine/POST_STATE9_CONTINUATION.md`
- `../VTABLES/0x004b01c8.md`

## Canonical source homes

- ABI shell:
  - `src/launcher_mediator_state9_abi.cpp`
- mediator-owned late-login logic:
  - `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`
  - `matrixstaging/game/src/libltclientlogin/loginmediator_state9_submit_scaffold.h`
- state-owned continuation:
  - `matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp`
  - `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`

## Slot summary

| Slot | Anchor | Current role |
|---:|---|---|
| `+0xd4` | `launcher.exe:0x41b4f0` | returns the 16-byte seed/key pointer reused by the state9 callback-blob path |
| `+0x124` | wrapper-facing `ProvideStartupTriple` / `launcher.exe:0x41f1d0` store behind deeper init handoff | preserves the startup triple `netShell/netMgr/distrObjExecutive` into owner `+0x84/+0x88/+0x8c` |
| `+0x18c` | `launcher.exe:0x41e690` | fills the fixed `0x20`-byte state9 callback blob used by callback84/client-side `ClientNetShell +0x38` |

## `+0x124` startup triple

Current bounded read:
- deeper client init calls wrapper-facing arg6 `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
- launcher-side owner fields `+0x84/+0x88/+0x8c` are zero-initialized earlier at `0x41ee60`
- `0x41f1d0` is the concrete non-init store for that triple
- within the active late-login scope, later `0x41de40` reads those fields; no stronger later
  launcher-side rewrite of owner `+0x88` is isolated yet

Practical consequence:
- preserve the same-run startup triple on the live replacement path
- do **not** replace it with cross-run object transplants or generic placeholders

## `+0x18c` callback blob

Current best recovered shape:
- state-gated: returns `0x12000009` unless current helper/state is `9`
- fixed exposed size: `0x20` bytes
- first half:
  - current slot id low
  - current slot id high
  - caller arg2
  - caller arg3
- second half:
  - seeded from owner `+0xf18`
  - transformed in place through the shared `ValueNames` / `FeedbackSize` helper family
  - current live-matched replacement read: one-block Twofish transform of `[ownerF18, 0, 0, 0]`
    with the current margin-bootstrap 16-byte key and zero IV

Client-side consequence:
- callback84-side `ClientNetShell +0x38` is not self-contained
- it re-enters the client-resolved `ILTLoginMediator.Default` global and calls `+0x18c`
- the returned pair is effectively `(pointer-to-0x20-byte-blob, 0x20)`

## `+0xd4` seed pointer

Current best read:
- `+0xd4` returns the 16-byte source pointer consumed on the client side with size `0x10`
- this surface corroborates that the state9 callback/blob tail is fed from the same broader
  launcher-owned margin/bootstrap crypto material, not a state9-only ad hoc buffer

## Active path status

Closed enough on the deliberate existing-character path:
- startup `+0x124` triple is preserved live
- launcher-side `+0x18c` blob fill is source-owned/live
- state9 submit reaches the real object88 direct-vs-managed branch
- deliberate runtime now reaches:
  - `0x41de40`
  - state9 slot-6 raw `0x11` success
  - `0x41b420`
  - switch to state `0x0c`
  - event `0x18`

So the active blocker is no longer “missing arg6 late-login ABI”.
The next blocker is later post-state9 / state-`0x0c` continuation.

## Related docs

- `../state_machine/POST_STATE9_CONTINUATION.md`
- `../VTABLES/0x004b517c.md`
- `../VTABLES/0x004b5230.md`
- `0x4d2c58_ILTLoginMediator_Default.md`
