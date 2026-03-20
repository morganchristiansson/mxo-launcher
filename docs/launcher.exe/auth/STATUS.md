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
    - the load-character scaffold should likewise prefer the reconstructed current-slot record
      (`+0x688[owner+0xcc8]`) when seeding the `+0xf1c` name/world family, with owner `+0x108`
      left only as fallback scaffolding
  - important distinction for current RE/source work:
    - auth reply handling already reconstructs and source-owns the auth-side character/world data
      families under owner `+0x688/+0x818/+0xd84`
      - character slot records there currently give us character handle, gcid pair, status, and world id
      - world descriptors give us world id/name/type/status/version/load
    - that is **not** yet the same thing as the helper11 human-name / appearance source block used by
      state11 sender `0x43c020`
  - owner vtable `+0x120` / `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the
    strongest recovered writer for the immediate helper11 source block:
    - writes owner `+0x12c`
      - newer tightening from `0x41c3c0` + `0x4401a0` now makes that field less opaque:
        it is bounds-checked against owner vtable `+0xf8` and then used as an index into
        owner `+0xd84`, so current best read is a selected world-descriptor index / selector,
        not a direct world-id payload
    - writes owner `+0x134..+0x177`
    - writes owner `+0x108`, `+0x178`, `+0x198`, `+0x1b8`
    - then switches helper state to `10`
- important runtime narrowing from live original `matrix.exe` under WineDbg:
  - confirming the launcher password does hit owner vtable `+0xec` / `0x41ecd0 = ProcessLoginRequest`
    - newer live stop there also tightens the concrete input shape:
      - input `+0x00` = username
      - input `+0x20` = password
      - observed active branch had `DAT_004d66ec == 0`, owner `+0x65c == 0`, owner `+0x12c == 0`
      - that matches the earlier/default auth-bootstrap branch rather than the later helper11 writer branch
  - that live path then visibly transitions through `0x41b450` as:
    - state `0 -> 2`
    - state `2 -> 3`
    - state `3 -> 8`
  - the state-`3 -> 8` step returns to `0x41c382`, i.e. the `0x41c1f0` family, while current helper
    vtable is `0x004b5208` (state `3`)
  - newer live stop now confirms `0x41c1f0` itself is reached on that same branch, not merely inferred
    from the return site
  - `0x41c3c0` did **not** fire on that observed authenticated launch path before later game loading
  - current practical consequence: treat `0x41c3c0` as a real writer for one branch, but not yet as
    the active default post-password progression we most urgently need for launching the game
  - stronger active-path replacement target now looks like owner `+0xec -> 0x41c1f0 -> state 8`,
    with `0x41c1f0` persisting a `0xb4` selection/config snapshot under owner `+0xcc8/+0xcd0..+0xd7f`
  - practical implementation consequence: prioritize faithful state4/state8 continuation
    (`0x439300`, `0x43bd20`, `0x43f930`) before trying to force helper11-only data paths
  - replacement launcher now also mirrors the copied arg6 `+0xec` snapshot into the source-owned
    `CLTLoginMediator::PersistSelectionContextForState8(...)` path so this active branch no longer
    lives only in ABI-side logs
  - when a scaffold state8 object is registered, that same source-owned mirror now also advances
    the mediator's active state to `CLTLoginState_State8`, making the recovered state-3 -> state-8
    handoff live in source instead of only implicit in docs/logs
  - representative binder-side validation after that change:
    - command:
      `MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both`
    - current `resurrections.log` now shows the launcher-owned login-controller sidecar step as:
      `PersistSelectionContextForState8 ... currentState=CLTLoginState_State8`
    - that run still stops at the expected current deliberate boundary
      (`InitClientDLL succeeded, but RunClientDLL is gated.`), so treat it as a state-handoff
      validation run, not a full state8 runtime proof by itself
  - source now also has a first anchored mirror for `0x41ecd0 = ProcessLoginRequest`, keeping the
    owner `+0x94` copy and default `DAT_004d66ec == 0` small-string clear on the mediator side
    instead of leaving that active password-submit branch entirely outside source ownership
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
  - `CLTLoginState_State11` slot-3 send body
- `launcher.exe:0x440320`
  - `CLTLoginState_State11` slot-6 reply body

Current best read:
- `0x4401a0` success does **not** immediately fall into the later auth-side
  `0x43b830 / CLTLoginMediator_Helper14_SendGetWorldListRequest` path
- instead it switches helper state to `0x4f7894` / vtable `0x004b5154` and immediately runs
  `CLTLoginState_State11` slot 3 (`0x43c020`)
- newer `0x43c020` decompilation tightening corrects one packet-detail claim:
  - it reserves a fixed `0x4d`-byte payload span first
  - then `0x43a470` initializes payload byte `0x00 = 0x0c`
  - current best read is therefore a framed margin send of fixed-length `0x4d` payload bytes
    whose payload starts with `0x0c`, not a flat payload whose first byte is `0x4d`
- that state body then sends the packet through `CLTLoginMediator_SendCurrentMarginPacket` and posts event `0x15`
- newer opcode-name tightening now makes the helper10/11 branch identity less generic:
  - `0x41bf70 = CLTLoginMediator_MarginOpcodeName`
  - state10 slot 3 raw `0x0a` = `MS_ClaimCharacterNameRequest`
  - state11 slot 3 raw `0x0c` = `MS_CreateCharacterRequest`
  - state8 slot 3 raw `0x0f` = `MS_LoadCharacterRequest`
- practical consequence for the current replacement-launcher stall:
  - the helper10/11 branch we are currently proving is specifically a
    **claim/create-character** branch, not the already-proven natural-original state8
    `MS_LoadCharacterRequest` branch
- source ownership now mirrors that more closely:
  - packet build/send shape lives in `CLTLoginState_State11::Slot3_BeginOrContinue`
  - `0x4401a0` / state10 slot 6 is now likewise routed through
    `CLTLoginState_State10::Slot6_HandleSecondaryMessage` instead of living only as a mediator-side
    catch-all
  - `0x440320` / state11 slot 6 is now likewise routed through
    `CLTLoginState_State11::Slot6_HandleSecondaryMessage`, with the mediator reduced to the narrower
    staged-packet and owner-buffer helper role
  - that same source-owned slot-6 path now also performs the scaffold handoff into registered
    helper9/state9 when helper11 reply progression completes, instead of leaving the state-9 bridge
    entirely implicit
  - newer slot-6 parsing work now also preserves more of the recovered helper-local metadata there:
    - expected fragment count from parsed reply `+0x0b/+0x0c`
    - helper9 handoff word from parsed reply `+0x09`
    - section-specific owner writes for section `0` and append sections `3/4/5/6`
  - mediator only keeps the narrower current-margin-connection transport helper for `0x41af70`
  - the local `0x43c020` packet-builder family is now also source-owned as internal helper
    scaffolding instead of one flat raw-byte helper:
    - `0x439840 = CLTLoginMediatorPacketBuilderEnvelope_Initialize`
    - `0x43a470 = CLTLoginMediatorPacket0x4d_ResetAndInitialize`
    - `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940` = string append helpers
- newer concrete packet-builder detail worth preserving:
  - state11 sender `0x43c020` is no longer just a generic margin packet builder
  - packet debug printer `0x43e540` now shows its owner fields as:
    - `+0x134..+0x174` = appearance/customization ids (`SkinToneID .. TraitID`)
    - `+0x178` = `RealFirstName`
    - `+0x198` = `RealLastName`
    - `+0x1b8` = `Background`
  - the send body also appends a final `GameSessionID` string from owner vtable `+0x148`
  - that getter is now recovered as `0x41f320 = CLTLoginMediator_GetGameSessionId664`
    returning owner `+0x664`
  - comparison against the state-8 sender `0x43bd20` now narrows the remaining source problem:
    - state-8 does share that same trailing `GameSessionID` append through `+0x148`
    - but `0x43bd20` does **not** consume owner `+0x178/+0x198/+0x1b8`
    - instead it pulls:
      - the current-slot record id pair through owner vtable `+0x44`
      - the persisted state-3->8 snapshot blocks from owner `+0xcd0..+0xd7f`
    - source now mirrors that more faithfully too:
      - packet build/send shape lives in `CLTLoginState_State8::Slot3_BeginOrContinue`
      - the local state8 packet-builder family is now source-owned around:
        - `0x43ac10 = CLTLoginMediatorPacket0x0f_ResetAndInitialize`
        - `0x43acf0 = CLTLoginMediatorPacket0x0f_ReserveGameSessionId`
        - `0x43ada0 = CLTLoginMediatorPacket0x0f_SetGameSessionId`
      - newer `0x41af70` tightening now also sharpens the immediate post-send boundary:
        - `0x41af70` is only a tiny mediator forwarder
        - it jumps through current margin connection vtable `+0x24`
        - current best read there is now two-step, not one-step:
          - `+0x24` = `0x41cf30 = CMessageConnection_ForwardEnvelopeToSendPacket`
          - that wrapper forwards into `+0x28` = inherited
            `0x448cf0 = CMessageConnection::SendPacket`
        - that send path performs packet-agenda filtering before lower submit helper `0x448a00`
          reaches the engine-facing byte send
        - practical consequence: original state8/state11 send still depends on a real
          **packet-envelope / agenda** path, not just raw payload bytes reaching the socket
      - the later natural state8 reply target also now has a tighter receive-side prerequisite,
        and newer live original runs now confirm that this prerequisite is really crossed:
        - incoming margin work survives through
          `0x44af60 -> 0x4490c0 -> 0x442d00`
          and the natural original path now does reach `0x43f930`
        - that receive chain is now slightly tighter at the mediator edge too:
          - `0x44af20` fallback reaches owner vtable `+0x184`
          - current best target there is `0x41f260 = CLTLoginMediator_DispatchCurrentHelperSlot6`
          - newer `0x442d00` review now also makes the discriminator explicit:
            base margin dispatch fully consumes decoded message codes `2`, `4`, and `5`
          - practical consequence: the active state's slot-6 body is the intended receive-side
            landing point only for other decoded message codes, including the raw state8/state11
            reply opcode `0x10`
        - but newer `0x442d00/0x441bc0/0x441850` review now also narrows a nearby non-reply branch:
          - the base type-4/MS wrapper path can synthesize a local type-`0x0b` completion object
          - that then falls through `0x44af60 -> 0x41afc0`
          - and `0x41afc0` re-enters the active helper at vtable `+0x04`
            (current best read: slot 2), **not** slot 6
          - practical consequence: not every surviving incoming MS-side path is automatically the
            later natural state8 reply path
        - newer `0x44af60` review also exposes an immediate kill-side effect worth tracking on that
          same boundary:
          - fallback target there is now narrowed to
            `0x41afc0 = CLTLoginMediator_HandleMarginConnectionCompletionFallback`
          - completion work type `1` clears owner margin-connection fields and would strand state8
            before any later reply handler runs
    - practical consequence: `RealFirstName/RealLastName/Background` are currently a
      state11-only source problem, while `GameSessionID` is the real shared state8/state11 field
  - stronger `GameSessionID` writer chain now in scope:
    - owner init `0x41ee60` zeros `+0x660` and empty-initializes `+0x664`
    - login-request success `0x421220` writes `+0x660` through owner vtable `+0x14c / 0x41f330`
    - that same login-success path writes owner `+0x94` through `+0x150 / 0x41f270`, not `+0x664`
    - later play-request success `0x420ef0` copies its callback string into owner `+0x664`
    - alternate helper path now has a stronger full chain:
      - owner vtable `+0x130 / 0x41f310` returns lazy helper `+0x65c`
      - owner vtable `+0x134 / 0x420d00` allocates that `0x30`-byte helper through `0x420ca0`
      - `0x421a50` refreshes helper string `+0x18` from owner `+0x94 + 0x60`
        (the recovered bootstrap/source embedded small string)
      - `0x420e70` then copies helper `+0x18` into owner `+0x664`
  - source ownership now matches that narrowing better:
    - state8 slot 3 has an anchored send scaffold
    - state8 slot 6 now also has an anchored reply scaffold in `loginstate.cpp`
      - keeps the state-local fragment counters on `CLTLoginState_State8`
      - mirrors first-fragment seeding from the current slot record via owner vtable `+0x44`
      - now keeps the newer `0x43f930` section routing explicit instead of flattening it:
        - case `0x00` one-shot overflow tail at owner `+0x13f0/+0x13f4`
        - cases `0x06/0x07` at owner `+0x1408/+0x1410`
        - cases `0x0c/0x0d` at owner `+0x1430/+0x1438`
      - keeps section-`0x0b` / `0x43f8c0` source-owned as the narrow owner `+0x145c/+0x1460` side effect
      - mirrors the failure-side switch back to helper state `3` plus error `10`
      - now only completes when the expected-count byte is non-zero, matching the original state-local completion gate before the helper9 handoff / event `0x0b`
      - completes with the helper9 handoff using parsed word `+0x09` plus event `0x0b`
    - practical current read: state8 is now closed enough for the active password-submit branch,
      with only the non-`0x10` fallback through `0x41c5c0` left explicitly unresolved
    - state10 slot 3 now also has an anchored gated send scaffold in `loginstate.cpp`
      - stronger current read from `0x43bf90`:
        - gate on `0x41b4b0` (`owner +0x1c`, connection state `+0x34 == 2`)
        - gate on owner byte `+0xf14`
        - initialize `0x43a1f0`
        - copy `CharacterName` from owner `+0x108` through `0x43aa80`
        - send through `0x41af70`
        - post event `0x13`
      - source now also reifies that local packet-builder family as:
        - `0x43a1f0 = CLTLoginMediatorPacket0x0a_ResetAndInitialize`
        - `0x43aa80 = CLTLoginMediatorPacket0x0a_SetCharacterName`
    - state18 slot 3 is now explicitly owned in `loginstate.cpp`
    - source-owned mirrors now exist for owner `+0x65c`, `+0x660`, and `+0x664`
    - launchpad-owned success mirrors now live in `launchpad.cpp` for:
      - `0x421220 -> +0x660` and owner `+0x94` first-string consequences
      - `0x420ef0 -> +0x664` (`GameSessionID`)
  - practical consequence: do **not** keep synthesizing `+0x178/+0x198/+0x1b8` from route/world
    fallback data in the scaffold just because those fields were once structurally opaque
- later `CLTLoginState_State11` slot 6 (`0x440320`) handles raw `0x10` / `MS_LoadCharacterReply`,
  accumulates reply fragments into owner `+0xf1c`, and on completion switches helper state to `9`
  then posts event `0x16`

What remains:
- preserve the now-proven split in the canonical status view:
  - post-`AS_AuthReply` State4/`0x41e500` margin begin is now live on the real deliberate runtime path
  - helper11/state11 slot 3 (`0x43c020`) is now likewise live there
  - newer source/runtime tightening now also shows the **current scaffold bridge** sending framed
    bytes beginning `4d 0c ...` on that path
    - keep this labeled as scaffold transport evidence, not proof that original `0x41af70`
      serialized raw bytes the same way
    - the tighter original-static read is still: state11 builds a packet-envelope, then
      `0x41af70 -> connection vtable +0x24 -> 0x448cf0`
  - the next remaining post-auth blocker is still later incoming margin `0x10` / `MS_LoadCharacterReply`
    handling in state11 slot 6 (`0x440320`), with current active-path source starvation in the
    helper11 payload (`+0x134..+0x1b8`, `+0x664`) now looking more relevant to that lack of reply
  - newer helper11 receive-boundary tightening now keeps the first real slot-6 prerequisite explicit:
    - decoded margin codes `2`, `4`, and `5` are consumed by
      `0x44af20 -> 0x442d00 = CBaseMarginConnection_DispatchMessage`
    - only other decoded codes survive into owner `+0x184 / 0x41f260`
      (`CLTLoginMediator_DispatchCurrentHelperSlot6`)
    - practical consequence: the first real helper11 reply candidate must be a later raw `0x10`
      that survives that base-dispatch filter before `0x440320` can even run
    - source/runtime logs now distinguish three short-run outcomes:
      - no margin packet arrived yet
      - margin packet arrived but would be base-dispatch-consumed before slot 6
      - helper11 slot 6 was entered but parse/status gating failed
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
- keep the active password-submit state-8 branch marked as closed enough for now
- keep the immediate post-state8 helper9/state9 continuation marked as closed enough for the active
  path
  - now narrowed as:
    - `0x00439780 = CLTLoginState_State9::Slot3_BeginOrContinue`
    - `0x41de40 = CLTLoginMediator_State9SubmitFollowup`
    - `0x43c180 = CLTLoginState_State9::Slot6_HandleSecondaryMessage`
    - `0x41b420 = CLTLoginMediator_HandleState9Opcode11SuccessSideEffect`
  - practical current read:
    - slot 3 keeps the helper-local payload lifecycle on the state object
    - natural original is now live-proven through the submit bridge too:
      - representative natural stop at `0x439780` showed `this+4 = 0`, `this+6 = 0x2710`
      - immediate follow-on stop at `0x41de40` showed `ECX = 0x004d4e38`, `EAX = 0x2710`,
        `EDX = 0x004b517c`
      - that same `0x41de40` stop also had a non-null owner callback/object triple at
        `+0x84/+0x88/+0x8c`
    - slot 6 keeps the raw `0x11` success/failure transition on the state object
    - natural original is now live-proven through the slot-6 success side too:
      - natural hit now reached `0x43c180`
      - representative natural-success stop landed at `0x43c1c2`
      - parsed status there was `0`
      - owner `+0x80` was likewise `0`
      - representative visible UI at that same boundary was **Waiting for Regionserver**
      - newer Ghidra-first tightening now also makes the immediate success tail concrete:
        - `0x41b420`
        - `0x41b450(0x0c)`
        - `0x41cfb0(0x18)`
      - practical consequence of that tail:
        - post-state9 continuation is now better read as helper-switch + listener-tree event flow,
          not as an already-proven immediate fall into `0x004397e0` / `0x0041c5c0`
        - current known blocking observer path (`CLTEvilBlockingLoginObserver::WaitForEvent`) is
          only statically confirmed at events `1`, `8`, and `0x0f`, so event `0x18` consumer work
          remains a later question rather than a closed mapping
      - newer live breakpoint-only proof now also closes the next concrete event step there:
        - after the live `0x41cfb0(0x18)` post, the same natural run later hit `0x41cfb0(0x0f)`
        - the run then continued into game without any observed hit on `0x004397e0` or
          `0x0041c5c0`
      - first follow-up late probes on `0x004397e0` / `0x0041c5c0` still stayed negative
    - remaining unresolved work now moves later into the post-state9 / state-`0x0c` continuation,
      while tightening the deeper owner/collaborator behavior already proven under `0x41de40`
- forced-`RunClientDLL` scaffold evidence now proves a closer existing-character branch too:
  - command:
    `env -u MXO_FORCE_INCOMPLETE_INIT -u MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE MXO_BINDER_LOGIN_MEDIATOR=1 MXO_STUB_LAUNCHER_OBJECT=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality MXO_FORCE_RUNCLIENT=1 wine ./resurrections.exe -user morgan -pwd '<pwd>'`
  - newer short-run source-log proof now makes the current replacement-launcher progression more
    explicit too:
    - `SwitchHelperStateScaffold(0x08)`
      - `CLTLoginState_AuthenticatePending -> CLTLoginState_State8`
    - preserve state `8` through `AS_AuthChallengeResponse`
      - do **not** force helper state `0x0a` on the existing-character path
    - successful `AS_AuthReply`
      - route it back onto the existing-character state8 path
    - post-auth margin connect-status is consumed
    - promote owner `+0x1c` into the ready send state required by `0x41b4b0`
    - `CLTLoginState_State8::Slot3_BeginOrContinue` now sends the structured raw-`0x0f`
      / `MS_LoadCharacterRequest` packet
    - representative short-run send now shows:
      - host `reality.lith.thematrixonline.net`
      - connection state `2`
      - payload bytes `0xbb`
      - GCID low `0x00006dce`
      - GCID high `0x00000000`
    - newer client-side selection-context tightening also removed one clearly bad state8 input
      family there:
      - `client.dll:0x62170b00` fills the `+0xec` tail from per-selection profile/config helpers
        (`FUN_621996a0`) rooted under `Profiles\%s\%s_%X\` and files like
        `hl.cfg / pi.cfg / ai.cfg / ...`
      - current replacement runs do not have that per-selection cfg corpus on disk
      - the copied `+0xec` state8 snapshot had therefore degenerated into repeated cfg-parser/
        default values across `+0x24..+0xb3`
      - the scaffold now keeps the arg7-derived first dword and zeros the cfg-derived tail instead
        of forwarding those repeated values into the replacement launcher's state8 send
    - but no later incoming raw-`0x10` / `MS_LoadCharacterReply` arrives during the deliberate run window
  - practical comparison against the natural original path:
    - this moves the replacement launcher later and closer to the natural-original shape than the
      old helper11-only stall
    - current replacement proof now ends at the existing-character state8 send family, not at the
      helper11 send / event `0x15` claim/create branch
- newer original-launcher WineDbg runs now move the natural password-submit branch later again:
  - natural original hits confirmed:
    - `0x41ecd0`
    - `0x41c1f0`
    - `0x43bd20`
    - `0x41af70`
    - `0x41cf30`
    - `0x43bf64`
    - `0x43bf6c`
    - `0x43f930`
    - `0x439780`
    - `0x41de40`
    - `0x43c180`
  - representative deeper state9 stop sequence now also shows:
    - at `0x439780`: helper9 local byte `this+4 = 0`, word `this+6 = 0x2710`
    - at `0x41de40`: `ECX = 0x004d4e38`, `EAX = 0x2710`, `EDX = 0x004b517c`
    - at `0x43c1c2` (state9 slot-6 success side): parsed status `0`, owner `+0x80 = 0`
    - representative natural `0x41de40` stop also had a non-null owner callback/object triple at
      `+0x84/+0x88/+0x8c`
  - helper11-first alternatives remain unsupported on that same natural path:
    - no natural hit yet on `0x41c3c0`
    - no natural hit yet on `0x421220`
    - no natural hit yet on `0x420ef0`
    - no natural hit yet on `0x43c020`
  - additional concrete state at the natural original state8 send site:
    - owner `+0x664` (`GameSessionID`) was still zero
- practical consequence:
  - for **faithful original-launcher progression**, the old post-send/live-boundary question is now
    crossed; natural original not only reaches `0x43f930`, `0x439780`, and the deeper
    submit helper `0x41de40 = CLTLoginMediator_State9SubmitFollowup`, but now also the later
    raw-`0x11` reply body `0x43c180`
  - representative live stops there also showed the state8->state9 handoff word (`this+6`) as
    `0x2710`, i.e. the state8 completion handoff is live and non-zero on the natural path
  - representative natural-success stop at `0x43c1c2` also now shows the state9 slot-6 success
    branch is live with parsed status `0`
  - representative visible status at that same later boundary is now:
    - **Waiting for Regionserver**
  - but the easy immediate state-`0x0c` leaf follow-on theory is weaker now too:
    - first follow-up late probes on `0x004397e0` / `0x0041c5c0` did not fire naturally
  - separate but important later loading-character note:
    - `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials` is still the strongest recovered writer
      for the helper11 appearance/name/background source block
    - `0x43c020 = CLTLoginState_State11_SendPostAuthMarginPacket0x4d` is the later sender that
      consumes those fields
    - `0x43e540` debug-printer keeps the concrete field names anchored there:
      - `SkinToneID`, `BodyID`, `HeadID`, `HairID`, `HairColorID`, `TattooID`
      - `FacialHairID`, `FacialHairColorID`
      - `StartingHat`, `StartingGlasses`, `StartingShirt`, `StartingGloves`
      - `StartingCoat`, `StartingPants`, `StartingTights`, `StartingShoes`
      - `TraitID`, `RealFirstName`, `RealLastName`, `Background`, `GameSessionID`
  - current active-path caution on that same note:
    - those fields still look likely relevant to the later Loading Character phase
    - but they are still **not** the first natural-original boundary to force back onto the active
      path while `0x41b450/0x41cfb0` after state9 success remain the tighter next question
  - keep helper11/state11 as a **real later scaffold/runtime stall**, but stop treating it as the
    first faithful original-live breakpoint target while the natural original path is now confirmed
    later on state8/state9
  - post-state9 / state-`0x0c` continuation now moves up again in priority; the next natural-live
    target is whatever concrete helper/state body follows the now-proven state9 success-side switch
    to `0x0c` plus event `0x18`
  - current replacement-launcher experiment bridge now mirrors a little more of the confirmed
    helper11 writer chain without pretending the original upstream producer is solved:
    - explicit helper11 character/customization seed inputs are routed through
      `0x41c3c0 = CLTLoginMediator_ProcessLoginCredentials`
    - explicit session/game-session seed inputs are routed through
      `0x420ef0 = LaunchPadClient_OnPlayRequestStatus`
    - replacement launcher `-char` / `-session` values can now feed that bridge on the scaffold path
      - validation run:
        `env -u MXO_FORCE_INCOMPLETE_INIT -u MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE MXO_BINDER_LOGIN_MEDIATOR=1 MXO_STUB_LAUNCHER_OBJECT=1 MXO_FORCE_RUNCLIENT=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality wine ./resurrections.exe -user morgan -pwd '<pwd>' -char TestChar -session TESTSESSION`
      - current observed effect in `resurrections.log`:
        - `ProcessLoginCredentials mirrored recovered helper11 source write name='TestChar' ...`
        - `mirrored launchpad play-request success GameSessionID='TESTSESSION'`
        - helper11 send changed from fixed-only `0x4d` bytes to total `0x5a` bytes because the
          trailing `GameSessionID` append became live
      - stronger negative-result follow-up with a deliberately non-empty helper11 payload:
        `env -u MXO_FORCE_INCOMPLETE_INIT -u MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE MXO_BINDER_LOGIN_MEDIATOR=1 MXO_STUB_LAUNCHER_OBJECT=1 MXO_FORCE_RUNCLIENT=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality MXO_DIAGNOSTIC_REAL_FIRST_NAME=Morg4n MXO_DIAGNOSTIC_REAL_LAST_NAME=Anderson MXO_DIAGNOSTIC_BACKGROUND=Operator MXO_DIAGNOSTIC_APPEARANCE_IDS='1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17' wine ./resurrections.exe -user morgan -pwd '<pwd>' -char Morg4n -session TESTSESSION`
        - observed helper11 send became:
          - `totalBytes=0x76`
          - `SkinToneID=0x00000001`
          - `RealFirstName='Morg4n'`
          - `RealLastName='Anderson'`
          - `Background='Operator'`
          - `GameSessionID='TESTSESSION'`
        - but the runtime still produced **no** later `MarginReceivePacket` / `MS_LoadCharacterReply`
      - practical consequence of that negative result:
        - merely making the helper11-visible fields non-empty is **not** enough
        - the remaining problem is now more likely the authenticity/source of those values or some
          still-missing neighboring launcher-owned state, not just empty-string/zero presence alone
    - treat this as a diagnostic bridge into confirmed writers, **not** as proof that the original
      launcher used those exact startup arguments as the upstream producer for helper11 data
