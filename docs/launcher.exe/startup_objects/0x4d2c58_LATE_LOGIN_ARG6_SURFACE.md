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

Newer tightening also sharpens what `+0x118` really is on that later side:
- owner `+0x1470` is a vector-like container of 12-byte string-triple entries
- owner slot `+0x190 / 0x41f840` appends one entry at a time by forwarding into
  `0x41f640 = StringTripleArray_Append`
- state6 opcode-`9` success clears `+0x1470`, appends metric-name strings through `+0x190`, and
  later arg6 `+0x118 / 0x41af50` exposes that same vector to client late-runtime consumers

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
- direct assembly now tightens that slot to the live connection field, not a fallback chooser:
  - `0x41b4f0 = mov eax,[ecx+0x1c] ; add eax,0x85 ; ret`
  - so the original slot is just `owner +0x1c + 0x85`
- `0x41e690` then calls `+0xd4` and passes that returned pointer straight into
  `0x41df60 / 0x44b190` for the state9 tail transform; no alternate launcher-side seed branch is
  visible in that body
- replacement `+0x18c` now mirrors that tighter call shape too: it sources the seed through
  mediator `+0xd4` alone instead of keeping an extra local fallback chain in the blob builder
- `0x442d00` code-5 handling writes the consumed 16-byte payload tail back into connection
  `+0x85 .. +0x94`, which matches that later `+0xd4` consumption
- practical replacement consequence:
  - live connection `+0x85 .. +0x94` is now the real preferred/original source on the active path
  - the older launcher-owned bootstrap-sidecar key is not part of the original `0x41b4f0` body
  - bounded replacement smoke on `2026-03-28` initially still hit a direct client `+0xd4` caller
    before the live connection mirror was present
  - current replacement narrows that timing gap by mirroring the already-recovered
    CERT_Challenge Twofish key into the live margin connection `+0x85 .. +0x94` as soon as the
    launcher-owned bootstrap parse recovers it, because the active replacement path has not yet
    naturally hit the decoded code-5 writeback seam early enough
  - repeated successful active-path runs now usually show only live `connection+0x85` reads, but
    keep fallback behind `+0xd4` until runtime remains stable enough to prune it without risking
    game entry

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
- newest successful replacement caller-logging run then makes the immediate post-`0x18` arg6 use
  concrete too:
  - `+0x10c` is called from `0x6217082b` inside `ClientShell_LoginMediatorObserver_OnEvent`
  - `+0x118` is called from `0x621c6db3` inside `0x621c6d90`
  - that `+0x118` list is still empty on the successful run
- the same successful run did **not** show:
  - a later `+0x118` caller from `0x62017150`
  - a later `+0x170` caller from `0x62031136` / observer `0x6298a5e8`

So the active blocker is no longer “missing arg6 late-login ABI”.
The next blocker is later post-state9 / state-`0x0c` continuation, specifically why the working
replacement route currently stops after the immediate `0x621c6d90` helper instead of taking the
later original-looking late-entry / second-observer branches.

## Related docs

- `../state_machine/POST_STATE9_CONTINUATION.md`
- `../VTABLES/0x004b517c.md`
- `../VTABLES/0x004b5230.md`
- `0x4d2c58_ILTLoginMediator_Default.md`
