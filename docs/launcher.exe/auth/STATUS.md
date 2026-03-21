# launcher.exe auth status

This file is the short current-status front door for launcher-owned auth.
Do not turn it back into a long historical journal.

## Read this first, then branch out

- packet/protocol details:
  - `README.md`
- auth/world/character materialization:
  - `../VTABLES/0x004b5014.md`
- existing-character post-auth path:
  - `../VTABLES/0x004b5104.md`
  - `../VTABLES/0x004b517c.md`
  - `../state_machine/POST_STATE9_CONTINUATION.md`
- late-login arg6 surface (`+0xd4`, `+0x124`, `+0x18c`):
  - `../startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`

## Current status

Auth is no longer the main blocker.

### Done enough to treat as working
- low-level wire loop is working enough on the launcher-owned path:
  - `0x06` / `AS_GetPublicKeyRequest`
  - `0x07` / `AS_GetPublicKeyReply`
  - `0x08` / `AS_AuthRequest`
  - `0x09` / `AS_AuthChallenge`
  - `0x0A` / `AS_AuthChallengeResponse`
  - `0x0B` / `AS_AuthReply`
- `AS_AuthRequest` now uses the live reply-derived RSA key from `0x07`
- launcher-owned auth now auto-begins by default on the active launcher path when the login-controller sidecar exists
- parsed `AS_AuthReply` is source-owned enough to populate mediator-side world/character state instead of living only in transient parse storage

### Existing-character path status after auth
Closed enough on the active replacement path:
- auth reaches `AS_AuthReply`
- launcher-owned margin bootstrap completes
- state8 sends raw `0x0f`
- state8 receives raw `0x10` and hands off to helper9/state9
- state9 submit reaches `0x41de40`
- state9 raw `0x11` success reaches state `0x0c` / event `0x18`

Natural original is proven later still:
- `0x41b450(0x0c)`
- `0x41cfb0(0x18)`
- later `0x41cfb0(0x0f)`
- then entry into game

So the active blocker has moved beyond auth proper and into post-state9 continuation.

## Auth-specific work that still remains

Keep these separate from the later post-state9 blocker.

### 1. Faithful auth-side owner writeback cleanup
Still worth tightening when it directly matters:
- owner families around the post-`0x0B` path
- selected-slot/current-slot reconstruction rooted in:
  - owner `+0x688`
  - owner `+0x818`
  - owner `+0xd84`

Current practical rule:
- prefer the reconstructed auth-side character/world data already source-owned in
  `CLTLoginState_AuthenticatePending::AuthMessageDispatch`
- do **not** mix this up with the separate helper11/create-character source block

### 2. Keep launcher-owned auth launcher-owned
- auth/bootstrap ownership stays in launcher-side source
- do not solve remaining late-login gaps by sliding auth responsibilities into client-owned code

## What is no longer useful to call “auth unfinished”

These are closed enough that they should not dominate the next sessions:
- public-key / RSA / challenge-loop bringup
- the old “state8/bootstrap blocker” framing
- helper11/create-character branch history
- generic auth packet archaeology that no longer changes active implementation choices

## Working definition of “auth finished” for this project

Auth can be treated as finished enough when:
1. launcher-owned auth wire traffic is faithful enough not to be the active blocker
2. auth reply data is persisted into the mediator-side world/character structures we actually use
3. the next blocker clearly lives later than `AS_AuthReply`

That threshold is already met for current work.

## Current recommendation

For the next session, do **not** start from old auth history.
Start from:
- `../state_machine/POST_STATE9_CONTINUATION.md`
- `../startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
