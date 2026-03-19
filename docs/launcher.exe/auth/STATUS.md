# launcher.exe auth status

This file is the concise status view for launcher-owned auth so auth progress does not have to stay mixed into broader startup/runtime notes.

Canonical packet/protocol details remain in:
- `README.md`

## Current state summary

Auth is now best split into two layers:

1. **wire/protocol auth**
   - effectively working enough to stop treating it as the main blocker
2. **launcher-integrated original progression**
   - still incomplete, but now much closer

## What is done enough to treat as working

### Low-level wire loop
Working in both the host probe and launcher-side scaffold path:
- `0x06` / `AS_GetPublicKeyRequest`
- `0x07` / `AS_GetPublicKeyReply`
- `0x08` / `AS_AuthRequest`
- `0x09` / `AS_AuthChallenge`
- `0x0A` / `AS_AuthChallengeResponse`
- `0x0B` / `AS_AuthReply`

### Critical resolved packet/crypto issue
- `AS_AuthRequest` now uses the **live reply-derived RSA key** from `0x07`
- no longer uses the stale hard-coded key path

### Current runtime/layout cleanup already landed
- canonical public auth declarations now live at:
  - `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`
- active implementation now lives under recovered runtime-style paths:
  - `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
  - `matrixstaging/runtime/src/libltcrypto/filters.cpp`
  - `matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp`
- `src/auth/` is now compatibility-only
- launcher-owned auth diagnostics sidecar code now also has its own split source home:
  - `src/diagnostics_auth.cpp`
  - `src/diagnostics_auth.h`
  instead of living entirely inside `src/diagnostics.cpp`

### Launcher-side auth start behavior
- auth now auto-begins by default on the binder/scaffold path when the diagnostic login-controller sidecar exists
- quick-test opt-out only:
  - `MXO_DISABLE_AUTH_CONNECTION=1`
- the old explicit `MXO_BEGIN_AUTH_CONNECTION=1` gate is no longer required

### Current state writeback improvement already landed
- parsed `AS_AuthReply` now begins to be adopted into recovered mediator-owned state instead of living only in transient parse storage
- current transitional writeback lives in:
  - `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
  - `CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState()`

## What still remains on auth

These are the auth-adjacent items still worth doing before calling launcher-owned auth truly finished.

### 1. Faithful post-`0x0B` owner-state reconstruction
Current source-of-truth anchor:
- `launcher.exe:0x4401a0`
  - renamed in Ghidra as `CLTLoginMediator_Helper10_HandleAuthReply`

What remains:
- reconstruct more of the original writeback under owner regions like:
  - `+0x80`
  - `+0x684 / +0x688 / +0x818 / +0xd84`
  - `+0xcc8`
- newer recovered detail worth preserving while doing that work:
  - owner vtable `+0x40` / `0x41f2e0` returns `owner + 0x688[index]`
  - owner vtable `+0x44` / `0x41f300` returns the current entry from that same table using
    owner byte `+0xcc8`
  - this makes `+0x688` look less like a generic world-slot array and more like a
    current-slot cached record table used by later load-character paths
  - owner vtable `+0xe0` / `0x41b260` is now the strongest current state-8 margin-route getter:
    - it reads owner `+0x818[index*0x0c]`
    - and returns the first dword only when begin != current
    - current best read is therefore a per-slot **route-host string triple**
  - owner vtable `+0xfc/+0x100/+0x104/+0x108` now also read a linked owner `+0xd84[index]`
    **world-descriptor** family through payload offsets `+0x03`, `+0x18`, `+0x19`, and `+0x1f & 0xf`
  - stronger current class/layout read for that `+0xd84` family:
    - concrete `0x14`-byte object rooted at vtable `0x004b533c`
    - ctor/init `0x43c310`, dtor `0x443aa0`, debug printer `0x43ded0`, payload reset `0x439a70`
    - debug printer proves payload fields:
      - `+0x01` = world id
      - `+0x03` = world name
      - `+0x17` = status
      - `+0x18` = type
      - `+0x19` = server version
      - `+0x1d` = server language
      - `+0x1e` = private flag
      - `+0x1f` = population levels
  - helper10 (`0x4401a0`) still looks like the selected-slot bridge between those families, but the
    broader writer path is now materially clearer:
  - `0x43f300 = CLTLoginState_AuthenticatePending_AuthMessageDispatch`
    - builds `+0xd84` first from auth **world** data
    - then builds `+0x688` as the auth **character-slot** table
    - then seeds `+0x818` by matching each character record's world id against the
      world-descriptor table and copying the descriptor name
  - practical consequence for the reimplementation:
    - state-8 margin routing should prefer reconstructed `+0x688/+0x818/+0xd84` data over the old
      single fallback world-name path
  - owner vtable `+0x120` / `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the
    strongest recovered writer for the immediate helper11 source block:
    - writes owner `+0x12c`
    - writes owner `+0x134..+0x177`
    - writes owner `+0x108`, `+0x178`, `+0x198`, `+0x1b8`
    - then switches helper state to `10`
- important runtime narrowing from live original `matrix.exe` under WineDbg:
  - confirming the launcher password does hit owner vtable `+0xec` / `0x41ecd0 = ProcessLoginRequest`
  - that live path then visibly transitions through `0x41b450` as:
    - state `0 -> 2`
    - state `2 -> 3`
    - state `3 -> 8`
  - the state-`3 -> 8` step returns to `0x41c382`, i.e. the `0x41c1f0` family, while current helper
    vtable is `0x004b5208` (state `3`)
  - `0x41c3c0` did **not** fire on that observed authenticated launch path before later game loading
  - current practical consequence: treat `0x41c3c0` as a real writer for one branch, but not yet as
    the active default post-password progression we most urgently need for launching the game
  - stronger active-path replacement target now looks like owner `+0xec -> 0x41c1f0 -> state 8`,
    with `0x41c1f0` persisting a `0xb4` selection/config snapshot under owner `+0xcc8/+0xcd0..+0xd7f`
  - replacement launcher now also mirrors the copied arg6 `+0xec` snapshot into the source-owned
    `CLTLoginMediator::PersistSelectionContextForState8(...)` path so this active branch no longer
    lives only in ABI-side logs
  - that same copied snapshot is now also mirrored into the auth/login-controller sidecar model,
    so launcher-owned runtime experiments can consume the observed state-3->8 selection snapshot too
  - practical host-name correction from live auth data:
    - decoded auth-reply world name `Reality` is the current ground truth for
      `reality.lith.thematrixonline.net`
- stop relying on only the current partial/diagnostic table adoption

### 2. Faithful post-auth progression into the actual immediate original continuation
Current next-step anchors after successful `AS_AuthReply` are now:
- `launcher.exe:0x4401a0`
  - `CLTLoginMediator_Helper10_HandleAuthReply`
- `launcher.exe:0x43c020`
  - `CLTLoginMediator_Helper11_SendPostAuthMarginPacket0x4d`
- `launcher.exe:0x440320`
  - `CLTLoginMediator_Helper11_HandleLoadCharacterReply`

Current best read:
- `0x4401a0` success does **not** immediately fall into the later auth-side
  `0x43b830 / CLTLoginMediator_Helper14_SendGetWorldListRequest` path
- instead it switches helper state to `0x4f7894` and immediately runs helper11 `+0x8`
- that helper builds a larger margin-side packet whose first payload byte is raw `0x4d`,
  sends it through `CLTLoginMediator_SendCurrentMarginPacket`, and posts event `0x15`
- later helper11 `+0x14` handles raw `0x10` / `MS_LoadCharacterReply`, accumulates reply
  fragments into owner `+0xf1c`, and on completion switches helper state to `9` then posts
  event `0x16`

What remains:
- reconstruct enough post-`0x0b` owner state that the helper11 margin/loading phase becomes
  live in the scaffold
- stop advertising the later `AS_GetWorldListRequest` helper as though it were the immediate
  next original step on this startup path
- keep `0x43b830` as a real later auth-side sender, but no longer treat it as the best first
  post-auth anchor

### 3. Better reconstruction of the `0x448050` helper/bootstrap chain
Current anchors:
- `launcher.exe:0x439210`
- `launcher.exe:0x448050`
- `launcher.exe:0x447eb0`
- `launcher.exe:0x4474f0`

What remains:
- rebuild more of the original helper/object setup around this branch
- reduce the remaining difference between “working auth” and “original helper-driven auth progression”

### 4. Isolate a few still-unknown exact helper VAs
Still explicitly unresolved in source comments:
- exact original framing helper VA
- exact original standalone `0x07` parser helper VA
- exact original standalone `0x09` parser helper VA
- exact original raw `0x0A` builder/send VA
- exact original auth-reply private-exponent decrypt helper VA

These are now fidelity/documentation gaps, not core auth blockers.

## What is probably **not** worth calling “auth unfinished” anymore

These belong more to post-auth launcher/runtime progression than to auth itself:
- exact margin host derivation
- margin connection follow-on behavior
- loading-character handoff after launcher-side login
- late client/runtime world-loading issues

## Practical working definition of “auth finished” for this project

Auth can reasonably be treated as finished enough when all of the following are true:
1. the current working wire loop remains stable
2. auth no longer depends on manual env triggering
3. post-`0x0B` owner-state writeback is close enough to original to drive later launcher logic
4. the launcher naturally reaches the immediate post-`AS_AuthReply` helper11 margin/loading progression, with any later auth-side world-list helper activity treated as a separate later milestone

## Current recommendation

Treat auth as **no longer the main blocker**.
The highest-value remaining auth-adjacent work is now:
- faithful post-`0x0B` mediator state writeback
- and faithful progression into the immediate helper11-driven post-auth margin/loading phase
  (`0x43c020` / `0x440320`), rather than prematurely aiming at the later
  `AS_GetWorldListRequest` helper path
