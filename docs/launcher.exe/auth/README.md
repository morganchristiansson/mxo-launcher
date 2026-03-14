# launcher.exe auth

This folder is the canonical home for launcher-owned auth flow and auth-protocol documentation that does **not** belong under `startup_objects/`.

In particular, the standalone auth probe lives here because it is:
- a fast host-native auth-only harness
- useful for fixing launcher-owned auth
- but **not** itself a startup-owned object description

## Standalone auth-only probe result (2026-03-13)

A fast host-native auth probe now exists under the launcher project:
- build: `cd /home/morgan/mxo/code/matrix_launcher && make auth_probe`
- run with explicit credentials:
  - `./build/host/auth_probe --username <name> --password <password> --timeout-ms 5000`
- run with local project secrets:
  - `make run_auth_probe`

This probe is intentionally **outside** the full launcher/client startup path.
It exists to iterate quickly on the launcher-owned auth bootstrap sequence while keeping original `launcher.exe` static analysis as the canonical ownership anchor.

## Source split

- `tools/auth_probe.cpp`
  - small TCP/logging driver
  - no `resurrections.exe`, no Wine, no client startup
- canonical public auth API now lives under recovered runtime-style path:
  - `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`
- compatibility wrapper retained at:
  - `src/auth/auth_crypto.h`
- current conservative runtime-style implementation split:
  - `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
    - variable-length packet framing
    - current address anchors:
      - exact original helper VA: not yet isolated
      - current callers include launcher `0x447eb0 / 0x4474f0 / 0x43b830`
  - `matrixstaging/runtime/src/libltcrypto/filters.cpp`
    - opcode/text helpers and packet parse helpers
    - current address anchors:
      - `0x4401a0` later auth-reply owner handler
      - `0x43a330` auth-reply parse helper object
      - `0x41bc20` opcode read helper
  - `matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp`
    - auth/session-key crypto and outbound packet builders
    - current address anchors:
      - `0x448050` phase-2 auth/bootstrap dispatcher
      - `0x447eb0` strongest raw `0x06` send anchor
      - `0x4474f0` strongest raw `0x08` send anchor

The probe should remain the fast executable reference.
The canonical public declarations now live under `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`.
A compatibility wrapper remains at `src/auth/auth_crypto.h`, but current project direction is for that `src/auth/` surface to keep shrinking rather than remain the primary home.

Documentation policy note:
- prefer this `docs/launcher.exe/auth/` folder for packet-level auth protocol behavior and launcher-auth milestones
- avoid duplicating detailed wire-loop notes back into `startup_objects/`; those docs should link here instead

## Recovered source-file anchors and current ownership split

New string-backed source-path anchors now help separate the layers more cleanly:

Launcher/game-side login layer:
- `\matrixstaging\game\src\libltclientlogin\loginmediator.cpp`
- `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`
- `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`
- `\matrixstaging\game\src\launcher\launcher.cpp`

Runtime/network/crypto layer:
- `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`
- `\matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp`
- `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
- `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
- `\matrixstaging\runtime\src\liblttcp\lttcpconnection.cpp`
- `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

Current best interpretation of that split:
- direct MxO auth packet framing / crypto / socket transport belongs to the lower runtime/liblt* layer
- launcher-owned auth progression, helper switching, state writeback, world/character storage, and post-auth event flow belong to the `libltclientlogin` layer around `CLTLoginMediator`
- `LaunchPadClient` should currently be treated as a **neighboring pre-game account/subscription/play-request component** under `launchpad.cpp`, not as settled proof that the direct `AS_*` auth wire loop itself lives in that class

Current evidence for that last point:
- `LaunchPadClient` string-backed handlers are heavily tied to login-request / subscription / play-request result text in `launchpad.cpp`
- the direct MxO auth wire loop now recovered in the launcher path still anchors more naturally to the `CLTLoginMediator` helper chain (`0x41d170`, `0x439210`, `0x448050`, `0x4401a0`, `0x43b830`) than to the currently reviewed `LaunchPadClient` callbacks
- so the safest current architecture is:
  - keep low-level auth packet logic reusable and outside the mediator object itself
  - but keep auth **launcher-owned** and coordinated by the mediator/helper-state layer rather than moving ownership wholesale into `LaunchPadClient` or `client.dll`

## Current working wire sequence

The standalone probe now completes the full live auth loop:
- `0x06` / `AS_GetPublicKeyRequest`
- `0x07` / `AS_GetPublicKeyReply`
- `0x08` / `AS_AuthRequest`
- `0x09` / `AS_AuthChallenge`
- `0x0A` / `AS_AuthChallengeResponse`
- `0x0B` / `AS_AuthReply`

## Critical blocker that was resolved

The old standalone `0x08` failure was **not** just generic header noise.
The key fix was:
- stop encrypting `AS_AuthRequest` with the stale hard-coded mxo-hd public key
- parse the **live** modulus/exponent carried by `0x07 / AS_GetPublicKeyReply`
- use that reply-derived RSA key for `0x08`

So the high-value practical lesson for launcher integration is:
- treat the live `0x07` reply key material as authoritative for the current server
- do **not** assume the older open-source hard-coded key is sufficient

## Representative direct run result

Representative current run inputs:
- host: `auth.lith.thematrixonline.net:11000`
- username: `morgan`
- launcherVersion: `76005`
- currentPublicKeyId: `0`
- loginType: `1`
- zeroed key/ui MD5s
- default blob layout:
  - leading byte `0x00`
  - `rsaMethod = 4`
  - `someShort = 0x001b`
  - embedded current unix time
  - username length includes null terminator

Observed wire result:
- outbound raw `0x06` / `AS_GetPublicKeyRequest`
  - framed bytes: `09 06 e5 28 01 00 00 00 00 00`
  - payloadLen `9`, headerLen `1`, byteCount `10`
- inbound raw `0x07` / `AS_GetPublicKeyReply`
  - payloadLen `406`, headerLen `2`, byteCount `408`
  - parsed fields:
    - `status = 0`
    - `currentTime = 1773438144`
    - `publicKeyId = 4`
    - `keySize = 18`
    - `reservedByte = 0`
    - `publicExponentByte = 0x11`
    - `unknownWord = 0x0094`
    - `modulusLength = 128`
    - `signatureLength = 256`
- outbound raw `0x08` / `AS_AuthRequest`
  - authPublicKeyId `4`
  - payloadLen `170`, headerLen `2`, byteCount `172`
  - blobLen `128`
  - usernameLengthField `7`
  - null terminator included
  - fixed-header override not used
  - **usedReplyPublicKey = 1**
- inbound raw `0x09` / `AS_AuthChallenge`
  - payloadLen `17`, headerLen `1`, byteCount `18`
- outbound raw `0x0A` / `AS_AuthChallengeResponse`
  - payloadLen `69`, headerLen `1`, byteCount `70`
  - plaintextLen `64`
  - ciphertextLen `64`
- inbound raw `0x0B` / `AS_AuthReply`
  - payloadLen `535`, headerLen `2`, byteCount `537`

## Current semantic `0x0B` parse

The probe now parses the auth reply semantically enough to be useful for later launcher-owned margin/bootstrap work.

Current decoded success fields include:
- `characterCount = 2`
- `worldCount = 1`
- `username = 'morgan'`
- `offsetAuthData = 0x74`
- `offsetEncryptedData = 0x1ac`
- `offsetCharData = 0x21`
- `offsetServerData = 0x52`
- `offsetUsername = 0x20e`

Decoded entries:
- characters:
  - `Morg4n`
  - `Noobish`
- world:
  - `Reality`

Decoded auth-data block:
- marker `0x0136` (`36 01` on wire)
- signature length `128`
- signed-data block size `182`
- encrypted private exponent length `96`

Decoded signed-data block fields:
- `userId1 = 30631`
- `userName = 'morgan'`
- `publicExponent = 17`
- modulus length `96`
- `timeCreated = 1768585325`

And using:
- the auth request Twofish key
- plus the `0x09` challenge bytes as IV

the probe also decrypts the auth-reply private exponent blob back to a `96`-byte plaintext suitable for later margin-key/bootstrap work.

## Credential policy

There is only **one user-facing password** now.

The old `soePass` field is treated as a **legacy internal payload field only**:
- it is not exposed as a second public credential anymore
- the probe mirrors the real password into that internal field by default for compatibility

So for actual launcher integration:
- preserve one real password input
- keep the legacy field internal only
- do not re-introduce a second user-facing password concept

## What this means for fixing the real launcher auth path

The probe is not the final architecture.
It is the fast reference implementation for launcher-owned auth.

The key practical integration rules are:
1. keep auth launcher-owned
2. reuse `src/auth/auth_crypto.*` instead of re-implementing packet logic elsewhere
3. preserve reply-derived RSA key usage for `0x08`
4. preserve the current `0x09 -> 0x0A -> 0x0B` handling shape
5. keep the probe runnable as the fastest auth regression harness while launcher integration progresses

## Launcher integration milestone in `resurrections.exe` (2026-03-13)

The working probe is no longer just an external reference.
The deliberate binder/runtime launcher path now reuses the same shared `src/auth/auth_crypto.*` helpers inside `resurrections.exe` through the launcher-side `CLTLoginMediator` scaffold.

Representative launcher command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_BEGIN_AUTH_CONNECTION=1 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

Current verified launcher-owned wire result on that path now also covers the full live auth loop:
- `0x06` / `AS_GetPublicKeyRequest`
- `0x07` / `AS_GetPublicKeyReply`
- `0x08` / `AS_AuthRequest`
- `0x09` / `AS_AuthChallenge`
- `0x0A` / `AS_AuthChallengeResponse`
- `0x0B` / `AS_AuthReply`

Representative launcher-side evidence from `~/MxO_7.6005/resurrections.log`:
- `launcher-owned auth send step='AS_GetPublicKeyRequest' ... -> sendResult=0x00000001`
- `launcher-owned auth parsed AS_GetPublicKeyReply ... publicKeyId=4 keySize=18 ... hasEmbeddedPublicKey=1`
- `launcher-owned auth send step='AS_AuthRequest' ... -> sendResult=0x00000001`
- `launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen=16`
- `launcher-owned auth send step='AS_AuthChallengeResponse' ... -> sendResult=0x00000001`
- `launcher-owned auth parsed AS_AuthReply success characterCount=2 worldCount=1 username='morgan'`
- `launcher-owned auth decrypted AS_AuthReply private exponent length=96`

Important interpretation:
- this keeps auth **launcher-owned**
- the probe remains the fastest auth regression harness
- but the packet logic is now materially migrated into the launcher path rather than living only in `tools/auth_probe.cpp`

Important current restraint:
- this is still a deliberate binder/scaffold run gated by `MXO_BEGIN_AUTH_CONNECTION=1`
- so it proves the launcher-owned auth wire logic now works in `resurrections.exe`
- but it does **not** yet prove that the original launcher's full helper/state machine reaches the same transition automatically without that trigger

## 0x448050 / phase-2 bootstrap object note

Further work on `0x448050` is now primarily about **faithful launcher-owned bootstrap-state reconstruction**, not about basic auth wire viability by itself.

Current canonical object-level notes for that path live under:
- `../startup_objects/0x4d6304_network_engine.md`

Newest Ghidra-backed tightening there now includes:
- the direct `0x439210 -> 0x448050` call shape
- the selected source-object layout passed from owner `+0x38`
- the three small-string destinations at bootstrap `+0x04 / +0x10 / +0x1c`
- the fixed writes to bootstrap `+0x28 / +0x2c / +0x30..+0x4f / +0x50`
- the ctor-backed tail shape from `0x45500` / `0x41290`

Keep detailed packet/auth-loop behavior here in `docs/launcher.exe/auth/` and keep the concrete `0x448050` owner-object field recovery in the startup-object doc above.

## Related docs

- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../startup_objects/0x4d2c58_RESOLUTION_MECHANISM.md`
- `../startup_objects/README.md`
