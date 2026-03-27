# CLTLoginState dispatch family spanning `0x004b0b88 .. 0x004b5230`

## Current best read

These small 4-byte / 8-byte objects are best documented as the **`CLTLoginState_*` state/dispatch family** that originally lived in:
- `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`

Why this reading is stronger than older `LaunchPadClient_*` guesses:
- `CLTLoginMediator_InitializeHelperDispatchTable` allocates/installs these objects.
- `0x0043f300` contains the string:
  - `CLTLoginState_AuthenticatePending::AuthMessageDispatch()`
- `0x0043d4d0` contains the string:
  - `CLTLoginState_WorldListPending::AuthMessageDispatch()`
- both handlers reference `loginstate.cpp` directly.
- many slot-7 entries are tiny state-id getters.

So the safest project wording is:
- **class surface / source lineage:** `CLTLoginState_*`
- **runtime owner:** the launcher-owned object currently labeled `CLTLoginMediator`

`LaunchPadClient_*` names remain useful as older Ghidra breadcrumbs, but they are **not** the preferred class names for this family.

## Canonical shared slot names

Source now uses the same slot names across all recovered login-state classes in `loginstate.h/.cpp`.
These names are intentionally structural and avoid overclaiming semantics where evidence is still partial.

| Vtable slot | Canonical source name | Best current meaning |
|---|---|---|
| slot 1 | `Slot1_HandlePrimaryGate` | shared gate / event-or-error entry (`0x00438d80` on most reviewed vtables) |
| slot 2 | `Slot2_HandleSecondaryGate` | shared fallback gate / transition entry (`0x00438df0` on most reviewed vtables; locally overridden by the B1 branch) |
| slot 3 | `Slot3_BeginOrContinue` | main per-state outbound step / begin / continue body |
| slot 4 | `Slot4_NoOp` | trivial `ret` stub on most reviewed vtables |
| slot 5 | `AuthMessageDispatch` | first inbound-message / failure surface; string-backed auth/world-list handlers live here on some states |
| slot 6 | `Slot6_HandleSecondaryMessage` | second inbound-message / completion surface |
| slot 7 | `Slot7_GetStateId` | tiny state-id getter, or `purecall` on the abstract final-leaf base |
| slot 8 | `Slot8_HandleAuxiliaryEvent` | optional auxiliary completion / UI / status handler |
| slot 9 | `Slot9_IsNetworkDriven` | tiny boolean-style getter (`1` on most active states, `0` on final-leaf family) |
| slot 10 | initializer / constructor entry | tracked in docs/comments, not exposed as a C++ virtual in the scaffold |

Two important source notes:
- `DebugName()` is a scaffold helper, not a recovered original vtable slot.
- `DispatchPhaseCode()` is a source wrapper that currently forwards to `Slot7_GetStateId()`.

## Shared slot groupings

### Strongly shared slots
- slot 1 = usually `0x00438d80`
- slot 2 = usually `0x00438df0`
- slot 4 = usually `0x00441790`
- slot 5 = often `0x004397c0`
  - writes `DAT_004f78b8+0x80 = 0x12000004`, returns false
- slot 9 = usually `0x00437860`
  - tiny true-return stub

### Family-specific shared slots
- Family A slot 6 = `0x004208e0`
  - wrapper around `0x0041c5c0`
- final-leaf abstract/base family slot 6 = `0x004397e0`
- Family A slot 7 = `0x00420310 .. 0x00420350`
  - returns state ids `15 .. 19`
- Family B slot 7 = `0x0044e360`, `0x00418150`, `0x004686b0`, `0x00438c60 .. 0x00438d00`
  - returns state ids `1 .. 14`

## Reviewed state-id map

For the reviewed/documented set, slot 7 maps cleanly like this:

| VTable       | Current class name     | Slot 7 entry | Returned id / meaning |
|--------------|------------------------|--------------|-----------|
| `0x004b51e0` | `CLTLoginState_State0` | `0x00437b50` | state `0` |
| `0x004b4fc4` | `CLTLoginState_State1` | `0x0044e360` | state `1` |
| `0x004b5014` | `CLTLoginState_AuthenticatePending` | `0x00418150` | state `2` |
| `0x004b5208` | `CLTLoginState_State3` | `0x00438cf0` | state `3` |
| `0x004b503c` | `CLTLoginState_State4` | `0x004686b0` | state `4` |
| `0x004b5064` | `CLTLoginState_State5` | `0x00438c60` | state `5` |
| `0x004b508c` | `CLTLoginState_State6` | `0x00438c70` | state `6` |
| `0x004b50b4` | `CLTLoginState_State7` | `0x00438c80` | state `7` |
| `0x004b5104` | `CLTLoginState_State8` | `0x00438c90` | state `8` |
| `0x004b517c` | `CLTLoginState_State9` | `0x00438cc0` | state `9` |
| `0x004b512c` | `CLTLoginState_State10` | `0x00438ca0` | state `10` |
| `0x004b5154` | `CLTLoginState_State11` | `0x00438cb0` | state `11` |
| `0x004b5230` | `CLTLoginState_State12` | `0x00438d00` | state `12` |
| `0x004b50dc` | `CLTLoginState_State13` | `0x00438cd0` | state `13` |
| `0x004b4fec` | `CLTLoginState_WorldListPending` | `0x00438ce0` | state `14` |
| `0x004b0b88` | `CLTLoginState_State15` | `0x00420310` | state `15` |
| `0x004b0bb0` | `CLTLoginState_State16` | `0x00420320` | state `16` |
| `0x004b0bd8` | `CLTLoginState_State17` | `0x00420330` | state `17` |
| `0x004b0c00` | `CLTLoginState_State18` | `0x00420340` | state `18` |
| `0x004b0c28` | `CLTLoginState_State19` | `0x00420350` | state `19` |
| `0x004b51b8` | `CLTLoginState_AbstractFinalLeafBase` | `0x0048bc34` | `purecall` / abstract slot |

## State `1` is now anchored: `0x004b4fc4`

`0x004b4fc4` is now the best anchored concrete **`CLTLoginState_State1`** vtable.

Strongest evidence:
- slot 3 = `0x00439090 = CLTLoginMediator_Helper1_StartAuthConnection`
- slot 7 = `0x0044e360 = mov eax,1 ; ret`
- slot 10 = `0x00439060`, which installs vtable `0x004b4fc4`

That updates the family summary to:
- concrete state ids represented here: **`0 .. 19`**
- plus abstract `0x004b51b8`

## What decompilation actually proved about intermediates

The strongest derivation/intermediate clues remain:
- `0x004393f0` (`0x004b503c` slot 2)
  - handles a special case, else falls back to `0x00438df0`
- `0x00439590` (`0x004b5064` slot 2)
  - same pattern
- `0x00439680` (`0x004b50dc` slot 2)
  - same pattern again
- `0x004b51b8`
  - slot 7 is `purecall`

What this does **not** prove:
- not eight separate named intermediate classes
- not a standalone materialized root vtable

Best current compact structure:
- `0x004b0c28` = strongest Family A intermediate-style node
- `0x004b503c / 0x004b5064 / 0x004b50dc` = clearest slot-2 override sub-branch
- `0x004b51b8` = strongest abstract final-leaf base

## Clear subfamilies

### Family A PIN/new-PIN states
- `0x004b0b88 .. 0x004b0c28`

### Family B auth / margin / world-selection states
- `0x004b4fec .. 0x004b5230`

### Post-auth chain
- `0x004b512c -> 0x004b5154 -> 0x004b517c -> final leaves`

This remains the clearest sequential state pipeline recovered so far:
- state 10 handles later auth reply
- state 11 drives the immediate post-auth margin/load-character packet stage
- state 9 handles the next reply stage and switches into state 12
- final leaves then take over

## Provisional better-name suggestions

These are **documentation aliases only for now**.
Do not mass-rename the source classes yet; several are still behavior-backed rather than string-backed.

| Current class | Suggested alias | Confidence | Why |
|---|---|---:|---|
| `CLTLoginState_State1` | `CLTLoginState_AuthConnectPending` | high | slot 3 is `CLTLoginMediator_Helper1_StartAuthConnection` |
| `CLTLoginState_State3` | `CLTLoginState_SelectionContextPending` | medium | live original path reaches state `3`, then waits there while owner writers `0x41c390/0x41c1f0` consume selection-context input and switch to states `7/8`; current evidence still does not support a state3-local slot-3 body |
| `CLTLoginState_State7` | `CLTLoginState_MarginRouteProbePending` | low-medium | sends a smaller current-selection/current-character margin packet and expects reply opcode `0x0e` |
| `CLTLoginState_State8` | `CLTLoginState_MarginLoadCharacterPending` | medium | sends a large structured margin packet, handles chunked opcode `0x10`, then advances to state `9` |
| `CLTLoginState_State9` | `CLTLoginState_LoadCharacterFollowupPending` | low-medium | consumes the post-`0x10` follow-up path, handles opcode `0x11`, and advances to state `0x0c` |
| `CLTLoginState_State10` | `CLTLoginState_AuthReplyPending` | high | slot 6 is the best-anchored `AS_AuthReply` handler at `0x4401a0` |
| `CLTLoginState_State11` | `CLTLoginState_PostAuthMarginLoadPending` | high | immediate post-auth state; sends raw `0x4d`, then handles load-character reply fragments |
| `CLTLoginState_State12` | `CLTLoginState_FinalMarginLeaf12` | low | only final-leaf evidence so far; stronger semantic name still missing |
| `CLTLoginState_State13` | `CLTLoginState_LateMarginRouteProbePending` | low | later branch-specific request/reply pair that feeds state `9` via opcode `0x16` |

States still too weakly understood to rename beyond number-led placeholders:
- `State0`
- `State4`
- `State5`
- `State6`
- `State15..19`

## Naming guidance

Preferred wording:
- **`CLTLoginState` dispatch/state family** for the cluster
- address-led concrete names such as `CLTLoginState_State10` where no stronger string-backed class name exists
- string-backed names where they do exist:
  - `CLTLoginState_AuthenticatePending`
  - `CLTLoginState_WorldListPending`
- when a number-led class gains enough behavioral evidence, document the **suggested alias** first before renaming source/Ghidra wholesale

Avoid treating `LaunchPadClient_*` labels as authoritative class names unless separate evidence anchors them.
