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
- stop relying on only the current partial/diagnostic table adoption

### 2. Faithful post-auth progression into the next launcher-owned request
Current next-step anchor:
- `launcher.exe:0x43b830`
  - renamed in Ghidra as `CLTLoginMediator_Helper14_SendGetWorldListRequest`

What remains:
- make the launcher naturally reach this from the helper/state chain
- not just as a known likely next request name in logs

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
4. the launcher naturally reaches the next auth-side/world-list progression step

## Current recommendation

Treat auth as **no longer the main blocker**.
The highest-value remaining auth work is now:
- faithful post-`0x0B` mediator state writeback
- and faithful progression into the later `AS_GetWorldListRequest` path
