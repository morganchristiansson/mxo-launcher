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
- `src/auth/auth_crypto.h` / `.cpp`
  - shared auth packet/framing helpers
  - `AS_GetPublicKeyReply` parse helper
  - reply-embedded RSA modulus/signature extraction
  - `AS_AuthRequest` blob/header builder
  - `AS_AuthChallenge` parse + `AS_AuthChallengeResponse` builder
  - `AS_AuthReply` parse including:
    - character/world list decode
    - auth-data block decode
    - signed-data block decode
    - auth-reply private-exponent decrypt helper

The probe should remain the fast executable reference.
Reusable packet logic should continue to live in `src/auth/auth_crypto.*` so it can be migrated back into launcher-owned code instead of forked into two implementations.

Documentation policy note:
- prefer this `docs/launcher.exe/auth/` folder for packet-level auth protocol behavior and launcher-auth milestones
- avoid duplicating detailed wire-loop notes back into `startup_objects/`; those docs should link here instead

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

## Related docs

- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../startup_objects/0x4d2c58_RESOLUTION_MECHANISM.md`
- `../startup_objects/README.md`
