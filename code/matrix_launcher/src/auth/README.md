# src/auth transitional compatibility layer

This directory is no longer the canonical implementation home for the working auth path.

## Canonical homes

Public declarations:
- `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`

Active implementation:
- `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
- `matrixstaging/runtime/src/libltcrypto/filters.cpp`
- `matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp`

## What remains here

- `auth_crypto.h`
  - thin compatibility wrapper only
- `auth_crypto.cpp`
  - intentionally empty/deprecated placeholder

## Why keep this directory at all?

Only to avoid breaking stale include paths while the codebase finishes moving to the recovered
runtime-style file layout.

## Address anchors for the active low-level auth path

- `launcher.exe:0x448050`
  - phase-2 auth/bootstrap dispatcher
- `launcher.exe:0x447eb0`
  - strongest current raw `0x06` / `AS_GetPublicKeyRequest` send anchor
- `launcher.exe:0x4474f0`
  - strongest current raw `0x08` / `AS_AuthRequest` send anchor
- `launcher.exe:0x4401a0`
  - later owner-side `AS_AuthReply` handler
