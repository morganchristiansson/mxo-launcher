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
        - current source now mirrors that one step more closely too:
          - margin send no longer pre-submits caller-framed bytes directly
          - it now wraps payload bytes in a source-owned envelope/message scaffold
            whose inner header bytes are submitted using the same `0x448a00`-style
            length/pointer derivation (`+0x0a/+0x0b`, 1-byte vs 2-byte header)
          - still-missing launcher-owned authenticity is now narrower and explicit:
            packet-agenda / callback metadata around `0x448cf0`, not the old raw-byte
            framing alone
          - current source now also owns the `0x448960` connection-side packet-name-family /
            packetized-mode configuration more explicitly:
            - auth connection -> auth family, packetized enabled
            - margin connection -> margin family, packetized enabled
          - latest short replacement run still showed the remaining concrete gap on the live state8
            send:
            - margin send logged `packetNameFamily=margin packetizedEnabled=1`
            - but still `agendaCreated=0`
          - newer original-launcher WineDbg send-bridge capture now narrows that read again:
            - natural original state8 send at `0x448cf0/0x448a00` also had margin connection
              `+0x70 = 0x41ce40`
            - and natural original still had connection `+0x74 = 0` on that first state8 send
            - so the remaining launcher-owned authenticity gap is now weighted more toward the
              **actual message-object content/shape** than toward agenda presence alone
            - strongest single concrete mismatch from that capture:
              - natural original submitted payload length `0x13b`
              - current replacement submits payload length `0x0bb`
              - delta = `0x80`
            - newer `0x43acf0 + 0x4557b0` tightening now explains the growth rule more concretely:
              - state8 fixed body is really `0x0bb`
              - trailing growth is owned by the shared message-object family, not by `0x41af70`
              - `0x43acf0` reserves `(GameSessionID byte count including NUL) + 2` bytes through
                shared-message vtable `+0x18 = 0x4557b0`
            - but fresh original-launcher WineDbg validation on the natural **first** state8 send
              now also disproves the tempting easy explanation for the whole `0x80` delta:
              - natural path hit `0x41c1f0 -> 0x43bd20 -> 0x41f320 -> 0x43ada0`
              - at `0x41f320`, owner `this = 0x004d4e38`
              - owner `+0x664` getter printed `""`
              - return site on that hit was `0x43bf4a` inside the state8 send body
              - `0x43ada0` then likewise received `param_1 = ""`
              - important fidelity correction for source:
                `0x41f320` returns a non-null pointer to owner `+0x664` even when empty, and
                `0x43ada0` still routes that through the reserve helper, so an empty string still
                contributes the trailing 2-byte zero-length field reservation
            - newer targeted reruns now correct an important mixed-send mistake from that earlier
              read:
              - the earlier `0x41cf30/0x448a00` hit with length `0x13b` and bytes
                `01 03 00 36 ...` returned to `0x441f9f`, i.e. `FUN_00441f30`, not state8
                `0x43bf64`
              - so that `0x13b` payload belongs to a different send family and should **not** be
                used as the state8 submit target
            - newer targeted state8 stops now give the tighter real state8 picture instead:
              - natural state8 reached `0x41af70` with return `0x43bf64`
              - then natural state8 reached `0x41cf30` with return `0x43bf64`
              - at that exact state8 `0x41cf30` stop, the shared outer object pointed at inner
                payload buffer `0x357f0b4`, and that inner object already carried:
                - length bytes `+0x0a/+0x0b = 0x80 / 0xbe` -> payload `0x0be`
                - first payload dword at `+0x0c = 0x006dce0f` (raw bytes `0f ce 6d 00`)
              - reading the actual payload base (`inner + 0x0c`) then closed the trailing state8
                field too:
                - payload `+0xb9 = 0x00bb`
                - payload `+0xbb = 0x0001`
                - payload `+0xbd = 0x00`
              - natural state8 snapshot blocks also matched the replacement's previously suspicious
                repeated client-supplied values exactly:
                - `cd0` / `ce0` still zero
                - `cf0 .. d70` matched the repeated `d98c1dd4 / 04b2008f / 980980e9 / 7e42f8ec`
                  family
            - practical consequence:
              - the natural first existing-character state8 send still looks like the raw `0x0f`
                builder family after all
              - the previous `0x13b` theory was a wrong cross-send association
              - the suspicious repeated snapshot blocks are no longer a state8 send-authenticity
                blocker by themselves
              - the NUL-inclusive empty-string `GameSessionID` reservation was the real remaining
                state8 builder mismatch, and source now mirrors that
          - but a separate faithful-direction correction also landed on the launcher side:
            - the copied arg6 `+0xec` state8 snapshot no longer zeroes cfg-derived blocks by default
            - that old zeroing is now explicit diagnostic-only behavior behind
              `MXO_DIAGNOSTIC_SANITIZE_SELECTION_CFG_DERIVED_BLOCKS=1`
            - current short run therefore preserves more of the real client-supplied snapshot even
              when the client-owned per-selection cfg corpus is incomplete
            - representative effect on the live replacement state8 send was immediate:
              `blockD70_3` stopped being zero (`0x7e42f8ec` in the short run)
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
      - current replacement runs do not have that active client-owned per-selection cfg corpus on
        disk
      - the copied `+0xec` state8 snapshot had therefore degenerated into repeated cfg-parser/
        default values across `+0x24..+0xb3`
      - the scaffold now keeps the arg7-derived first dword and zeros the cfg-derived tail as
        **diagnostic narrowing only**, not as a claim that launcher.exe owned or should recreate
        that client cfg writer/reader family
    - newer server/proxy-side protocol review narrowed the remaining replacement gap further:
      - the old replacement framing was too weak: post-connect margin traffic cannot be treated as
        though state8 raw `0x0f` may go straight out once the TCP connection reaches owner `+0x1c`
        state `2`
      - `mxoemu` `MarginSocket.cpp` + `Proxy/Logging.cpp` instead show a required margin-side
        bootstrap before later load-character traffic:
        - plaintext `CERT_ConnectRequest (0x01)`
        - plaintext `CERT_Challenge (0x02)`
        - encrypted `CERT_ChallengeResponse (0x03)`
        - encrypted `CERT_ConnectReply (0x04)`
        - encrypted `MS_ConnectRequest (0x06)`
        - encrypted `MS_ConnectChallenge (0x07)`
        - encrypted `MS_ConnectChallengeResponse (0x08)`
        - encrypted `MS_ConnectReply (0x09)`
        - only then later encrypted load-family traffic like raw `0x0f`
      - practical consequence for the current launcher reimplementation:
        - the active replacement was better read as missing the margin CERT/MS bootstrap and
          encrypted post-bootstrap transport layer, not as merely missing one more state8 payload tweak
        - current source ownership therefore moved that bootstrap into the launcher-owned
          mediator/runtime path instead of reviving a separate probe-style client
      - current source-side groundwork now in place under:
        - `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`
        - `matrixstaging/runtime/src/libltcrypto/auth_internal.h`
        - `matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp`
        - `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
      - runtime validation updated on `2026-03-21` using:
        - `MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality timeout 20 make run_binder_both_runtime`
      - current validation result is now positive through the old state8 blocker:
        - auth again progresses through `AS_GetPublicKeyReply`, `AS_AuthChallenge`, and `AS_AuthReply`
        - launcher-owned margin bootstrap now runs in order:
          - `0x01 -> 0x02 -> 0x03 -> 0x04 -> 0x06 -> 0x07 -> 0x08 -> 0x09`
        - bootstrap completion now returns control to state8 slot 3
        - state8 raw `0x0f` is now sent on encrypted post-bootstrap margin transport
        - the first decrypted incoming raw `0x10` now arrives and routes through state8 slot 6
        - state8 reply progression now completes far enough to switch into helper9/state9 with event `0x0b`
      - two concrete source/runtime fixes were needed to reach that point:
        - `CLTLoginMediator::HandleAuthPacketBytes(...)` had been re-parsing already-unframed auth
          payload bytes as though they still carried a variable-length header; it now consumes raw
          payload bytes directly on the receive bridge
        - `ParseMarginMsConnectReplyPayload(...)` had been requiring an exact 23-byte raw `0x09`
          body; current live traffic preserves the same leading field family but carries a longer
          decrypted payload, so the parser now accepts the stable 23-byte prefix while preserving
          the full payload bytes
      - practical consequence for the active blocker:
        - the old “missing first decrypted raw `0x10`” state8 blocker is now closed enough
        - focus has now moved later again onto helper9/state9 and the post-state9 continuation
        - latest deliberate runtime tightening there first proved one narrower immediate blocker:
          - a narrow source-owned bridge now re-dispatches the proven state8 `event 0x0b` handoff into
            `CLTLoginState_State9::Slot3_BeginOrContinue`
          - that live run now reaches `CLTLoginMediator::State9SubmitFollowupScaffold`
          - the initially visible replacement-side gap there was the null owner collaborator triple at
            `+0x84/+0x88/+0x8c`
        - newer source/runtime tightening now narrows the origin of that triple further too:
          - strongest current source/runtime origin is owner/arg6 vtable `+0x124`
          - deeper client init already captures that call as:
            `arg6->+0x124(netShell, netMgr, distrObjExecutive)`
          - `0x41f1d0 = CLTLoginMediator_SetState9CallbackObjectTriple84_88_8c` then stores those
            three parameters directly into owner `+0x84/+0x88/+0x8c`
        - practical consequence for the active blocker:
          - the state9 submit problem is now narrower than generic “missing collaborators”
          - but one attempted runtime bridge mattered in the wrong way:
            - temporarily mirroring the captured arg6 `+0x124` startup triple into the live
              replacement runtime path regressed the deliberate binder run
            - newer crashdump-backed rerun tightening now makes that failure site concrete:
              - dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_69.dmp`
              - top frame: `client:0x629ddfc8`
              - replacement caller frame: `resurrections:0x004230df`
                inside `CLTLoginMediator::State9SubmitFollowupScaffold`
              - current rebuilt `0x41de40` map plus that return site now pin the crash to the
                attempted owner callback84 query at `+0x38`, before any later object88
                `(+0x44)->(+0x30)` branch work ran
            - newer client-side static tightening now explains that crash one step further:
              - the transplanted callback84 object currently resolves to the `ClientNetShell` family
              - callback84 vtable `+0x38` is `client.dll:0x62006580`
              - that method is not self-contained on the callback object:
                it first checks client global `0x629df7f0 = resolved ILTLoginMediator.Default`
                through `+0x10`
              - it then calls that global's vtable `+0x18c(&DAT_629e0284, 900, 0)`
              - and only then returns pair `(&DAT_629e0284, 0x20)` to the launcher-side submit path
              - that later arg6 slot is now tightened on the launcher side too:
                - mediator vtable base `0x004b01c8`
                - `+0x18c` = `0x0041e690 = CLTLoginMediator_FillState9CallbackBlob18c`
                - it is state9-gated (`current state must be 9` or it returns `0x12000009`)
                - it writes a fixed `0x20`-byte blob whose first half is:
                  current slot id low/high + caller args `(900, 0)`
                - its second half now tightens a little further too:
                  - blob `+0x10..+0x1f` is best read as a 16-byte **in/out transform region**
                  - `0x41e690` seeds that region from owner dword `+0xf18`
                  - bounded `2026-03-21` launcher-side provenance pass over exactly:
                    - `0x41ee60`
                    - `0x41e690`
                    - `0x41de40`
                    - `0x439780`
                    - `0x43c180`
                    - `0x41b420`
                    - `0x4401a0`
                    - `0x41c1f0`
                    - `0x41ecd0`
                    - cross-check only: `0x4429b0`, `0x441470`
                  - outcome of that bounded static pass:
                    - `0x41ee60` is still the only checked writer and it zero-initializes owner `+0xf18`
                    - `0x41e690` reads/seeds owner `+0xf18` into blob `+0x10`
                    - no non-init launcher writer for owner `+0xf18` was found within that static scope
                  - but immediate natural-original WineDbg follow-up on the same session tightened the practical read again:
                    - natural breakpoints hit `0x439780 -> 0x41de40 -> 0x41e690`
                    - owner at both later stops was `0x004d4e38`
                    - owner `+0xf18` was already non-zero by that point
                      - observed run 1: `0xfe3e2a9f`
                      - observed run 2: `0x6e3c3dc5`
                      - later EULA-stage attach rerun: `0xc006b2db`
                    - that later rerun also armed a typed WineDbg watch on `*(int*)0x4d5d50` while the field was still zero before progression, but the watch only tripped late at `0x41de41`
                    - newer breakpoint-ladder rerun from EULA then tightened the practical write window further:
                      - `0x41ecd0` -> still `0`
                      - `0x41c1f0` -> still `0`
                      - `0x43bd20` (state8 slot3) -> still `0`
                      - `0x43f930` (state8 slot6 entry) -> already non-zero (`0xc1206989`)
                      - same value then persisted through `0x439780 -> 0x41de40 -> 0x41e690`
                    - narrower static follow-up then isolated the concrete non-init launcher writer:
                      - `0x00440780 = CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply`
                      - checked context around the already-bracketed window:
                        - `0x442d00`
                        - `0x44af20`
                        - `0x41f260`
                        - `0x43f930`
                      - on opcode-`9` success, `0x440780` writes owner byte `+0xf14 = 1` and owner dword `+0xf18 = parsedReply(+0x09)`
                      - parsed reply `+0x09` is now best read as the opcode-`9` `UDPSessionSecret` / session-id dword
                      - the next helper-state target is still chosen separately from cached upstream object `this+4` via vtable `+0x18`
                      - canonical detail now lives in `VTABLES/0x004b508c.md`
                      - this still explains why the earlier direct displacement search was misleading:
                        launcher.exe whole-binary search for literal `[base+0xf18]` still finds only
                        `0x41ee60` and `0x41e690`, because the real writer reaches `+0xf18` via
                        base `+0xf14` plus `[eax+4]`
                      - client.dll narrow mirror search still only finds the corresponding pair:
                        - `0x6252a680` = client-side zero-init mirror
                        - `0x62529f20` = client-side read/seed mirror for the `+0x18c` callback-blob path
                        - `0x62006580 = ClientNetShell_FillCallback84Pair38` calls that client-side `+0x18c` mirror on the resolved mediator global before returning `(&DAT_629e0284, 0x20)`
                    - practical consequence: the `owner +0xf18` question is now positively answered on the launcher side; the known non-init writer is `0x00440780`, and it lands before `0x43f930`
                    - go/no-go for replacement source stays conservative:
                      - do **not** mirror or synthesize `+0xf18` on unrelated paths
                      - only a faithful source-owned mirror of state6 opcode-`9` success should write it
                      - that future mirror should source the original opcode-`9` `UDPSessionSecret` / session-id field, not a placeholder value
                - that same 16-byte region is then transformed/materialized in place from mediator
                  `+0xd4 = 0x41b4f0` (`owner +0x1c + 0x85`) through helper `0x41df60 / 0x44b190`
                - that helper family is also reused by `AuthBootstrap680_SendAuthRequest`, and
                  carries string-backed `ValueNames` / `FeedbackSize` parameters plus neighboring
                  `EMSA-PKCS1-v1_5` literals, so current best read is shared Crypto++-style
                  transform/parameter machinery rather than state9-only custom glue
                - a second launcher-side getter now also corroborates the `+0x85` family outside
                  immediate state9 submit:
                  `0x41f3a0` returns `owner +0x680 -> +0xf4 + 0x85` when present, else static fallback;
                  companion `0x41f3c0` returns that same object's `+0xf8`
                - newer bootstrap-side decomp now tightens that shared family one step further too:
                  `0x4429b0 -> 0x41470` writes decrypted challenge-derived bytes into bootstrap `+0x85`,
                  then lazily wraps the same 16-byte source through sibling helper objects before later reuse
              - the client-side resolved mediator slot itself is now also re-tightened as a binder-managed
                output under the same service string `"ILTLoginMediator.Default"`, via static init
                around `client.dll:0x627c3bd0`
            - practical read from that crash:
              direct raw reuse of the client-captured arg6 `+0x124` objects is **not** yet a valid
              launcher-owned state9 collaborator reconstruction
              because callback84 is a wrapper around broader client-side mediator/global state,
              not a sealed standalone collaborator
            - backing that live runtime bridge back out restored the proven deliberate behavior
        - newer replacement status now closes the old `0x41de40` blocker enough on the deliberate path:
          - encrypted margin transport still works
          - state8 raw `0x0f` still sends
          - decrypted raw `0x10` still routes through state8 slot 6
          - state8 reply progression still completes with helper9/state9 handoff and event `0x0b`
          - launcher-side `+0x18c` callback blob fill is now source-owned/live on the active path
            - current slot id pair + caller args
            - one-block `AssemblyTwofish` tail transform over `[ownerF18, 0, 0, 0]`
          - the replacement now also preserves the same-run startup `+0x124` callback/object triple
            on the login-controller sidecar
          - deliberate runtime now executes the real managed object88 submit branch on the active
            existing-character path
          - practical consequence:
            - replacement now reaches the later state9 slot-6 raw `0x11` success side
            - then runs `0x41b420`
            - then switches to state `0x0c`
            - then posts event `0x18`
          - current next blocker therefore moves later again, into the post-state9 / state-`0x0c`
            continuation instead of `0x41de40` itself
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
