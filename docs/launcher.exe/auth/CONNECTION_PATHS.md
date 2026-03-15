# launcher.exe auth connection paths

This document holds the auth/margin connection-init and launcher-owned auth progression material that was previously mixed into `startup_objects/0x4d6304_network_engine.md`.

Keep `0x4d6304_network_engine.md` focused on the network-engine object itself.
Keep auth ownership/flow details here.

## Canonical related docs
- `README.md`
- `STATUS.md`
- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../startup_objects/0x4d6304_network_engine.md`

## Why this split exists

The launcher-owned auth path does use the arg5 network engine and `CMessageConnection` family, but the auth-specific questions are now better treated as their own topic:
- where auth/margin connection is initiated from
- how auth config is consumed
- how the first launcher-owned auth send is narrowed
- how later auth-side world-list progression relates to the same owner/helper chain

## Connection-init ownership summary

Current best answer:
- connection init is **not** best modeled as a trivial raw `0x4d6304->Connect(...)` launcher-mainline call
- the original launcher appears to initiate auth/margin connection work from a higher-level owner rooted at `0x4f78b8`
- that owner creates `CMessageConnection`-family children, builds endpoint/config state, then drives their connect wrapper

High-value anchors:
- `launcher.exe:0x41d170`
- `launcher.exe:0x41e500`
- `launcher.exe:0x439090`
- `launcher.exe:0x439300`
- `launcher.exe:0x43b300`

## Auth-side launcher path

### Owner construction / helper-state seed
- owner root global: `0x4f78b8`
- owner construction path stores `0x4f78b8 = esi`
- then immediately calls `0x43b300`
- `0x43b300` initializes the helper/state family at:
  - `0x4f7868`
  - `0x4f786c`
  - `0x4f7870`
  - `0x4f78a0`

### Auth config consumption
- auth DNS current string consumed from `0x4f7b14`
  - string-backed config name: `qsAuthServerDNSName`
- auth port consumed from `0x4f7a50`
  - string-backed config name: `AuthServerPort`

### Auth connection-init body
- `launcher.exe:0x43909f -> 0x41d170`
- `0x41d170`:
  - constructs a `CMessageConnection`-family object through `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x18`
  - builds endpoint data into owner `+0x5c`
  - immediately calls `connection->+0x1c(owner+0x5c)`

Current best method read:
- that virtual `+0x1c` remains best interpreted as the connection-oriented ensure-connected / engine-`Connect` wrapper

So current auth-side connection-init model is:
1. copy auth DNS into owner `+0x4c`
2. read auth port from `0x4f7a50`
3. build endpoint at owner `+0x5c`
4. call `CMessageConnection->+0x1c(owner+0x5c)`

## Margin-side launcher path

### Margin config consumption
- margin suffix consumed from `0x4d6814`
  - string-backed config name: `MarginServerDNSSuffix`
- margin port consumed from `0x4d669c`
  - string-backed config name: `MarginServerPort`

### Margin owner-state dispatch
- `launcher.exe:0x439300`
- consults `[owner+4]->vtable+0x18`
- then dispatches into `0x41e500` using owner vtable surfaces:
  - `+0xe0`
  - `+0xfc`
  - `+0x10c`
- plus owner fields:
  - `+0xcc8`
  - `+0x12c`
  - `+0x104`

### Margin connection-init body
- `launcher.exe:0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500`
- `0x41e500`:
  - constructs another `CMessageConnection`-family object through `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x1c`
  - builds endpoint state into owner `+0x6c`
  - immediately calls `connection->+0x1c(owner+0x6c)`

So current margin-side connection-init model is parallel to auth:
1. copy/resolve margin host/suffix into owner-side string state
2. read margin port from `0x4d669c`
3. build endpoint at owner `+0x6c`
4. call `CMessageConnection->+0x1c(owner+0x6c)`

## Current server-config string surfaces

### launcher.exe
String-backed names:
- `qsAuthServerDNSName`
- `AuthServerPort`
- `MarginServerDNSSuffix`
- `MarginServerPort`

Current recovered default numeric seeds:
- `AuthServerPort = 0x2af8 = 11000`
- `MarginServerPort = 0x2710 = 10000`

### client.dll
String-backed names:
- `AuthServerDNSName`
- `AuthServerPort`
- `MarginServerDNSSuffix`
- `MarginServerPort`

Important nuance:
- launcher uses recovered auth-side name `qsAuthServerDNSName`
- client uses direct `AuthServerDNSName`
- both currently expose `MarginServerDNSSuffix`, not a direct recovered `MarginServerDNSName`

## Later auth-side sender already identified

Current concrete later auth-channel send path:
- helper object `0x4f78a0`
- sender body `launcher.exe:0x43b830`
- renamed in Ghidra as `CLTLoginMediator_Helper14_SendGetWorldListRequest`
- packet raw code `0x35` = `AS_GetWorldListRequest`

This is strong evidence for a real launcher-owned auth-channel send path.
Current best reading is that it is a **later** auth request, not automatically the first send after connect.

## Earlier bootstrap/auth send narrowing

Current strongest earlier launcher-owned bootstrap/auth chain:
- `0x41b450(2)` selects helper `0x4f7870`
- helper `+0x08 / 0x439210`
- `0x439210` gates on `0x41b490()` and then calls `0x448050`
- `0x448050` branches into:
  - `0x447eb0` -> strongest current raw `0x06` / `AS_GetPublicKeyRequest` candidate
  - `0x4474f0` -> strongest current raw `0x08` / `AS_AuthRequest` candidate

Current best bootstrap anchors:
- `launcher.exe:0x439210`
- `launcher.exe:0x448050`
- `launcher.exe:0x447eb0`
- `launcher.exe:0x4474f0`
- later material continuation: `launcher.exe:0x429b0`

The remaining unknown is now narrower than before:
- what exact helper object lives at bootstrap `+0xa0`
- what exact send-target object lives at bootstrap `+0x50`
- and what the original concrete class/name semantics of owner `+0x94` are beyond its recovered auth/bootstrap role

## Current implementation-side milestone summary

Current scaffold/runtime milestones already achieved:
- real launcher-side auth TCP connection is possible
- current defaults mirror recovered strings:
  - auth host `auth.lith.thematrixonline.net`
  - auth port `11000`
  - margin suffix `.lith.thematrixonline.net`
  - margin port `10000`
- auth now auto-begins by default on the binder/scaffold path
- optional quick-test auth opt-out:
  - `MXO_DISABLE_AUTH_CONNECTION=1`
- optional margin trigger remains:
  - `MXO_BEGIN_MARGIN_CONNECTION=1`

Current limitation summary:
- exact margin-host derivation is still unresolved
- the current scaffold still does not claim faithful full helper-state equivalence around `0x448050`
- later post-auth owner-state reconstruction around `0x4401a0` is still incomplete

## Practical boundary

Use this doc for:
- auth/margin connection-init ownership
- config names/defaults
- first-send narrowing
- later auth-side sender narrowing

Use `STATUS.md` for:
- what is complete vs remaining

Use `0x4d6304_network_engine.md` for:
- arg5 engine object structure/queues/slots/worker semantics
- not for the detailed launcher-owned auth history anymore
