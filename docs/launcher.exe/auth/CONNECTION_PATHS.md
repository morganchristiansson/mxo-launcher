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
- active source now follows that direct-owner model more closely too:
  - auth/margin queue/worker paths stay on the direct connection object as `context`
  - source no longer depends on a mediator-owned bridge-context stand-in for the live auth/margin path

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

### `launcher.exe:0x449a70 -> 0x41af80` auth completion fallback clarification
- after base `0x4490c0` returns `0`, the auth leaf always falls through owner `+0x17c`
- `0x41af80` is **not** the same re-entry shape as margin `0x41afc0`
  - auth `0x41af80`
    - compares incoming connection against owner `+0x18`
    - clears owner `+0x18` on work type `1`
    - then calls current helper raw vtable entry `+0x00` / slot 1
  - margin `0x41afc0`
    - compares against owner `+0x1c`
    - clears owner `+0x1c/+0x20` on work type `1`
    - then calls current helper raw vtable entry `+0x04` / slot 2
- practical state1 consequence:
  - auth type-`2` connect status reaches state1 slot 1 (`0x4390b0`)
  - non-type-`2` auth completion work falls through the same slot-1 body into shared auth close gate `0x438d80`

### `launcher.exe:0x4390b0` exact current read
- if `CLTThreadPerClientTCPEngine_WorkItemHeader_GetWorkType(workItem) != 2`
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

## Earlier auth-side `0x4401a0` interpretation retired

Newer create-character/static review moves `launcher.exe:0x4401a0` out of the auth continuation
chain.

Current corrected read:
- `0x43bf90` sends raw margin opcode `0x0a = MS_ClaimCharacterNameRequest`
- `0x4401a0` is the matching helper10/state10 slot-6 consumer for raw margin
  `0x0b = MS_ClaimCharacterNameReply`
- its success-side tail still matters, but only on the later create-character branch rooted at
  owner `+0x120 / 0x41c3c0`
  - append one new slot record under owner `+0x688/+0x818`
  - write owner `+0xcc8 = currentCount`
  - switch to helper11/state11 (`0x43c020 / 0x440320`)
  - then continue into helper9/state9 (`0x439780 / 0x41de40`)

Negative result now worth keeping explicit in the auth doc:
- do **not** treat `0x4401a0 -> 0x43c020 -> 0x440320` as the immediate post-`AS_AuthReply`
  existing-character continuation
- that subchain is create-character specific
- the auth-valid existing-character continuation remains the earlier state8/state9 margin-load
  corridor documented elsewhere in this file and in the state-vtable docs

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

## Existing-character margin-connect continuation (`2026-03-30` WineDbg, original launcher)

Repeated narrow original-launcher attach passes on the patch-notes/Continue path now confirm the real
post-connect continuation on the active existing-character branch more concretely.

Observed stop order:
- initial state8 send attempt:
  - `0x43bd20`
  - then `0x41b4b0` with owner `+0x1c == 0`
  - so this first state8 slot-3 entry is the pre-connect gate that falls back to helper/state `4`
- margin connect completion fallback:
  - `0x41afc0`
  - current helper object vtable = `0x4b503c` / state4
  - owner `+0x1c` was non-null and connection state `+0x34 == 2`
- state4 connect-status handler:
  - `0x4393f0`
  - cached upstream object at `state4+4` was the state8 object (`0x4b5104`)
  - work-item type was `2`, payload/status dword was `0`
- immediate continuation after that zero-status state4 slot-2 success:
  - `0x43bd20` again
  - caller chain now concretely included `0x4394c0 -> 0x41aff6`
  - so this branch really does re-enter state8 slot 3 after the successful margin connect
- but the next step is **not** a direct stable state8 send:
  - from that post-connect state8 entry the run reached `0x43b8f0`
  - later original call chain proved: `0x43bd59 -> 0x43b8f0`
- first state6 pass then falls into the state5 family:
  - the run reached `0x439520`
  - caller chain proved: `0x43b959 -> 0x439520`
  - the same pass then hit `0x41b500` and `0x41ce80`
- later code-4 local completion re-entered state5 slot 2:
  - `0x441850 -> 0x41afc0 -> 0x439590`
  - current helper at that fallback stop was state5 (`0x4b5064`)
- state5 slot 2 then re-entered state6:
  - later call chain proved: `0x4395de -> 0x43b8f0`
- only after that later state6 work did the run reach state6 slot 6 and restore state8:
  - `0x440780`
  - later `0x440ae5 -> 0x43bd20`
  - at that restored state8 stop owner byte `+0xf14 == 1`
  - owner dword `+0xf18` was already non-zero

Current runtime consequence:
- the original continuation on this branch is **not** just
  `state4 slot2 -> state8 -> state6`
- but it is also **not** a simple “state5 instead of state6” replacement
- current best runtime-backed order is:
  - `state8(slot3 pre-connect gate)`
  - `-> state4 slot2 success`
  - `-> state8 slot3`
  - `-> state6 slot3`
  - `-> state5 slot3`
  - `-> state5 slot2`
  - `-> state6 slot3`
  - `-> state6 slot6`
  - `-> state8 slot3`

Static recheck of the same existing-character continuation now also matches that runtime order:
- `0x4393f0` / state4 slot2 zero-status success:
  - reads cached upstream from `this+4`
  - calls cached upstream vtable `+0x18`
  - clears `this+4`
  - switches helper through `0x41b450`
  - posts event `0x0e`
- `0x43bd48..0x43bd54` / state8 slot3 gate:
  - if owner byte `+0xf14 == 0`, it switches helper state to `6` and returns
- `0x439590` / state5 slot2 local type-`0x0b`:
  - reads cached upstream from `this+4`
  - calls its vtable `+0x18`
  - switches helper through `0x41b450`
  - practical fidelity consequence for replacement receive seams:
    - once decoded margin code `0x04` has already synthesized/handled the local type-`0x0b`
      completion path, do **not** also run a second launcher-owned bootstrap-side
      `CERT_ConnectReply -> MS_ConnectRequest` send
    - that duplicate source-owned send creates an extra `MS_ConnectRequest`, which then cascades
      into duplicated `0x07/0x09` traffic not supported by the recovered existing-character
      continuation
- `0x440ab9..0x440ae5` / state6 slot6 opcode-`9` success:
  - writes owner `+0xf18 = parsedReply(+0x09)`
  - writes owner `+0xf14 = 1`
  - reloads cached upstream from `this+4`
  - calls its vtable `+0x18`
  - switches helper through `0x41b450`
  - posts event `0x12`

Current replacement milestone on that exact blocker (`2026-03-30`, later same-day rerun):
- source now preserves the runtime-backed existing-character happy-path order through:
  - `state8(slot3 pre-connect gate)`
  - `-> state4 slot2 success`
  - `-> state8 slot3`
  - `-> state6 slot3`
  - `-> state5 slot3`
  - `-> state5 slot2 local type-0x0b`
  - `-> state6 slot3`
  - `-> state6 slot6 opcode-9 success`
  - `-> restored state8 slot3`
- source now also keeps the raw code-`0x04` seam narrower on that branch:
  - if the local type-`0x0b` completion path already handled the packet, the broader bootstrap
    fallback is skipped
  - practical consequence: the natural state5/state6 continuation remains the sole owner of the
    first `MS_ConnectRequest` send instead of synthesizing a duplicate mediator-side resend
  - latest validation confirms that this was the active `Loading Character` blocker:
    removing the duplicate mediator-side resend restored the first state8 raw `0x10`
    `MS_LoadCharacterReply` and moved the run forward to the later
    **Waiting for Regionserver** crash
- source state5 slot3 still materializes the runtime-backed owner `+0x680 +0xf4` copy/send path:
  - non-null `authReplyCopyShadowF4`
  - `replyCopyShadowStillValid=1`
  - copy into margin connection `+0x98`
  - bounded mirror of owner child `+0xb0/+0xc4/+0xd8` into connection-side `+0xa0`
  - raw type-1 send through the preserved `0x41ce80 -> 0x441f30` route
    - newer local-builder tightening now keeps that sender on vtable `0x004b6524`
    - builder `+0x10` = payload base
    - builder `+0x14/+0x18` = reserved reply-copy write pointer / byte count after `0x43a230`
- source state6 slot6 now owns the opcode-`9` success side narrowly enough to match the
  original restore model from `0x440ab9..0x440ae5`:
  - owner byte `+0xf14 = 1`
  - owner dword `+0xf18 = parsedReply(+0x09)`
  - next helper chosen from cached upstream `this+4` phase code, not from `+0xf18`
  - restored continuation re-enters state8 slot 3
- source no longer synthesizes owner `+0xf14/+0xf18` directly from `MS_ConnectReply` bootstrap
  completion when the state6 slot6 route is the active branch
- newer state6 opcode-`7` tightening also closed the bootstrap response builder much further:
  - `launcher.exe:0x440780` parses raw `0x07` as:
    - seed bytes at `+0x01..+0x10`
    - chunk-byte count at `+0x11..+0x14`
  - `0x43d800 = GenerateClientChunkHashes` then MD5-hashes each chunk of:
    - `client.dll`
    - the current module path from `GetModuleFileNameA(NULL, ...)`
  - `0x4566a0` MD5-folds `seed16 || chunkDigest0 || ... || chunkDigestN`
  - raw `0x08 / MS_ConnectChallengeResponse` carries that final 16-byte digest, not the older
    source-side auth-key-MD5 shortcut
- newer late-login/state9 submit tightening also exposed a narrower replacement-only fidelity
  question on this same bootstrap corridor:
  - replacement runs can see a retransmitted `MS_ConnectChallenge` while state6 is still waiting for
    the first opcode-`9` continuation
  - one bad run family also showed an extra later `MS_ConnectReply` with a different session id
    outside the proven state6 slot-6 route, and state9 submit then returned `0x00000003`
  - but a stricter follow-up experiment that consumed duplicate opcode-`7` packets outright stalled
    earlier at visible `Loading Character`
  - current static-RE now matters more than that temporary runtime guess:
    `launcher.exe:0x440780 = CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply` owns opcode
    `7` directly and does not show a bootstrap-phase single-shot guard before sending opcode `8`
  - practical source consequence: late duplicate opcode-`7` handling on the active path should stay
    resend-capable while state6 is still the live receiver; only later/off-route duplicates should be
    consumed
- active rerun/log milestone on this branch now shows:
  - state6 slot6 handling opcode-`9` success
  - owner `+0xf14` set
  - owner `+0xf18` non-zero
  - restored state8 slot3 after state6 slot6
  - later continuation through state8 reply -> state9 -> game entry

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
- newer practical rerun note after the arg5 ctor/ABI-shell cleanup pass:
  - one active-path run stalled at visible `Loading Character`
  - a second retry then entered game successfully
  - so current late-runtime behavior is still intermittent around the post-state11 / load-character continuation rather than a fully stable deterministic handoff

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
