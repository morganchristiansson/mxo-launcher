# launcher.exe auth connection paths

This document holds the auth/margin connection-init and launcher-owned auth progression material that was previously mixed into `startup_objects/0x4d6304_network_engine.md`.

Keep `0x4d6304_network_engine.md` focused on the network-engine object itself.
Keep auth ownership/flow details here.

## Canonical related docs
- `README.md`
- `STATUS.md`
- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../startup_objects/0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`
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
  - clears owner byte `+0x2c`
  - constructs a `CMessageConnection`-family object through `0x4417e0 -> 0x448b40`
  - stores it at owner `+0x18`
  - consumes the auth-address iterator rooted at owner `+0x4c`
    - current tighter helper read:
      - owner `+0x4c` = begin
      - owner `+0x50` = end
      - owner `+0x58` = current cursor
      - helper `0x440bb0` returns the next dword IPv4 and advances the cursor
  - builds endpoint data into owner `+0x5c` through `0x44b090(selectedIpv4, authPort)`
  - increments owner dword `+0x28`
  - immediately calls `connection->+0x1c(owner+0x5c)`

Current best method read:
- that virtual `+0x1c` remains best interpreted as the connection-oriented ensure-connected / engine-`Connect` wrapper

So current auth-side connection-init model is now tighter than a single fixed-host connect:
1. clear owner byte `+0x2c`
2. select the next auth IPv4 candidate from the dword list rooted at owner `+0x4c`
3. build endpoint at owner `+0x5c`
4. increment owner `+0x28`
5. call `CMessageConnection->+0x1c(owner+0x5c)`

## Margin-side launcher path

### Margin config consumption
- margin suffix consumed from `0x4d6814`
  - string-backed config name: `MarginServerDNSSuffix`
- margin port consumed from `0x4d669c`
  - string-backed config name: `MarginServerPort`

### Margin owner-state dispatch
- `launcher.exe:0x439300`
- belongs to `CLTLoginState_State4` vtable `0x004b503c` slot 3
- caches the first incoming upstream/helper pointer at `this+4` if that slot is null
- then consults `[this+4]->vtable+0x18`
- exact case split currently backed by both decompilation and disassembly:
  - case `6`
    - owner vtable `+0x10c`
    - use the first dword of the returned object
    - call `0x41e500`
  - cases `7`, `8`, `0x0d`
    - owner byte `+0xcc8`
    - owner vtable `+0xe0(slot, 0)`
    - call `0x41e500`
  - case `10`
    - owner dword `+0x12c`
    - owner vtable `+0xfc(value, 0)`
    - call `0x41e500`
  - default
    - owner dword `+0x104`
    - if not `-1`, owner vtable `+0xfc(value)`
    - only if non-null does it call `0x41e500`
- current source ownership consequence:
  - keep the case split in `loginstate_state4.cpp` / `CLTLoginState_State4::Slot3_BeginOrContinue`
  - keep only the narrower route-getter / host-resolution / `0x41e500` transport-init helpers in
    `loginmediator_margin_route.cpp`

### Margin connection-init body
- `launcher.exe:0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500`
- `0x41e500`:
  - allocates `0xa8`
  - initializes a base margin-connection object through `0x4417e0 -> 0x448b40`
  - overwrites the vtable to `0x004aff38` (`CMarginConnection`)
  - stores the owner pointer at connection `+0xa4`
  - calls `0x448960(1, 0x41ce40)` on that margin connection family
  - stores the connection at owner `+0x1c`
  - clears owner byte `+0x2d`
  - on the `arg2 == 0` path:
    - compares/refills owner route text at `+0x30`
    - rebuilds the owner address-list object at `+0x3c` through `0x440d80`
    - if owner `+0x7c == 0`, selects the next IPv4 from that list through `0x440bb0`
    - materializes endpoint state at owner `+0x6c` through `0x44b090`
  - increments owner dword `+0x24`
  - clears owner `+0x7c`
  - immediately calls `connection->+0x1c(owner+0x6c)`

So current best margin-side connection-init model is now narrower than “parallel to auth”:
1. build a dedicated `CMarginConnection`
2. refresh owner route text / address-list state (`+0x30 / +0x3c / +0x7c`) when needed
3. build endpoint at owner `+0x6c`
4. call the connection-oriented ensure-connected wrapper with that endpoint
5. retain the selected-IP / connect-attempt bookkeeping on the owner side (`+0x24 / +0x7c / +0x2d`)

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

## State1 connect-status gate now tightened

### `launcher.exe:0x4390b0` exact current read
- if `LaunchPadClient_GetVtableOffset(workItem) != 2`
  - tail-calls shared slot-1 gate `0x438d80`
- otherwise
  - `owner +0x80 = workItem +0x08`
  - branch on that payload dword:
    - payload `== 0`
      - read cached upstream helper state id from `[state1+4]->vtable+0x18`
      - `0x41b450(stateId)`
      - new helper slot 3 is immediately re-entered with old-state `state1`
      - `0x41cfb0(0)`
    - payload `!= 0`
      - owner byte `+0x2c = 1`
      - compare owner `+0x28` against auth candidate count `((+0x50 - +0x4c) >> 2)`
      - if attempts remain
        - call state1 slot 3 again with cached upstream `state1+4`
      - else
        - zero owner `+0x28`
        - `0x41b450(0)`
        - `0x41d090(0)`

### Current replacement/source consequence
- original control flow is now source-owned narrowly enough in `loginstate_state1.cpp` and the
  auth-entry scaffolds:
  - auth address-list mirror
  - auth attempt counter mirror
  - retry-vs-reset/error branch
- live replacement nuance stays explicit:
  - active replacement auth connect success still arrives as payload `0x07000001`
  - current source therefore keeps a narrow live-success alias to preserve the proven happy path
    while reserving the zero-payload switch/event path as the original behavior

## Later auth-side sender already identified

Current concrete later auth-channel send path:
- helper object `0x4f78a0`
- sender body `launcher.exe:0x43b830`
- renamed in Ghidra as `CLTLoginMediator_Helper14_SendGetWorldListRequest`
- packet raw code `0x35` = `AS_GetWorldListRequest`

This is strong evidence for a real launcher-owned auth-channel send path.
Current best reading is that it is a **later** auth request, not automatically the first send after connect.

## Immediate post-`AS_AuthReply` continuation now identified more concretely

Current highest-value original-launcher follow-up after successful auth reply is now:
- `launcher.exe:0x4401a0`
  - `CLTLoginMediator_Helper10_HandleAuthReply`
- `launcher.exe:0x41b450(0x0b)`
  - switches current helper to `0x4f7894` / vtable `0x4b5154`
- helper11 / state11 enter path:
  - `launcher.exe:0x43c020`
  - renamed in Ghidra as `CLTLoginState_State11_SendPostAuthMarginPacket0x4d`
- helper11 / state11 incoming path:
  - `launcher.exe:0x440320`
  - renamed in Ghidra as `CLTLoginState_State11_HandleLoadCharacterReply`

Current best static read of that chain:
- `0x4401a0` success still performs the important owner writeback under:
  - `+0x80`
  - `+0x684 / +0x688 / +0x818 / +0xd84`
  - `+0xcc8`
- but its immediate next helper transition is **not** the later auth-side
  `0x43b830 / AS_GetWorldListRequest` sender
- instead helper11 `+0x8` (`0x43c020`) first reserves a fixed `0x4d`-byte payload span,
  then `0x43a470` initializes payload byte `0x00 = 0x0c`, and that completed packet-envelope is
  forwarded through `CLTLoginMediator_SendCurrentMarginPacket` (`0x41af70`) before event `0x15`
  - newer tightening there now says:
    - `0x41af70` is only a tiny forwarder
    - it jumps into current margin connection vtable `+0x24`
    - current best target is inherited `0x448cf0 = CMessageConnection::SendPacket`
    - that send path performs packet-agenda filtering before the lower submit helper
      `0x448a00` reaches the engine-facing byte send
- later helper11 `+0x14` (`0x440320`) handles raw margin code `0x10`
  / `MS_LoadCharacterReply`, accumulates reply fragments into owner `+0xf1c`, and on
  completion switches helper state to `9` then posts event `0x16`

So the current post-auth blocker is now narrower than a generic “later world-list request” gap:
- the immediate original continuation after `AS_AuthReply` is helper11-driven
  margin/loading progression
- that makes post-auth owner-state writeback and later margin/loading activation the
  highest-value active targets before treating `0x43b830` as the next practical milestone on
  this startup path

## Earlier bootstrap/auth send narrowing

The detailed earlier bootstrap/auth child chain has now been split out to:
- `../startup_objects/0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`

Keep only the auth/connection-path consequence here:
- the proved early helper sequence still passes through state `2` / `0x439210`
- that path reaches the separate owner `+0x680` bootstrap child at `0x448050`
- which then chooses the raw `0x06` vs raw `0x08` send path
- keep that active state2 -> bootstrap-child handoff separate from the default-off
  `g_LaunchPadGateState16State18` state16/state18 family and from the earlier startup
  `0x406470 -> owner vtable +0x140` (`station_login`) side effect

This doc no longer repeats the full child call shape / field layout / `+0xa0/+0xa4/+0xa8/+0xf4`
family, because that is now canonicalized in the focused startup-object doc above.

## Current implementation-side milestone summary

Current implementation/runtime milestones already achieved:
- real launcher-side auth TCP connection is possible
- current defaults mirror recovered strings:
  - auth host `auth.lith.thematrixonline.net`
  - auth port `11000`
  - margin suffix `.lith.thematrixonline.net`
  - margin port `10000`
- auth now auto-begins by default on the active launcher path
- newer live runtime milestone after the State4/`0x41e500` correction:
  - validated on the active runtime path with:
    - `make run`
    - canonical log: `~/MxO_7.6005/resurrections.log`
  - post-`AS_AuthReply` margin begin now returns non-zero on the real active runtime path
  - the real path now emits a margin-side type-2 connect-status item
  - helper11/state11 slot 3 (`0x43c020`) is now live and builds/sends the raw `0x4d` packet on that path

Current limitation summary:
- exact margin-host derivation is still unresolved
- the current implementation still does not claim faithful full helper-state equivalence around `0x448050`
- post-auth owner-state reconstruction around `0x4401a0` is still incomplete
- the remaining post-auth blocker has moved forward:
  - helper11/state11 send is now live
  - later incoming margin `0x10` / `MS_LoadCharacterReply` handling (`0x440320`) is still not yet live

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
