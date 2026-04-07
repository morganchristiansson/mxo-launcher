# Current crash analysis - 2026-04-06

This file replaces the older stale `0x875895` note.

Current active crash work should be anchored to the later post-login client-shell/runtime path that
now starts successfully, reaches visible game entry, and then dies in one of two late families.

## Current state

Replacement now reliably reaches:
- state8 raw `0x10`
- state9 raw `0x11`
- state12 / event `0x18`
- visible **Waiting for Regionserver**
- in-game `MATRIX_ONLINE` window
- `RunClientDLL` active frame loop

So the current crash is no longer an early auth/bootstrap or immediate event-`0x18` blocker.

Newest tightening from live original + replacement comparison:
- the late `CLTRemoteCommCtx`-like object at client-shell `+0xd0` is present on the healthy
  original route too
- the state9 callback blob transform also now cross-checks against a live original sample
- newer replacement cross-character logging also tightens the `pi.cfg` seam itself:
  - `Morg4n` live arg6 `+0x70/+0x9c` length = `0x010e = 30 * 9`
  - `Noobish` live arg6 `+0x70/+0x9c` length = `0x002d = 5 * 9`
  - client-side `0x621c9d70 = AdoptLiveSelectionPiCfgCompactRecords` then materializes exactly
    those same `30` vs `5` non-zero table entries into `DAT_629ea4e8`
- so neither RCC object existence nor the current `+0x18c` transform algorithm is the best primary
  suspect anymore, and the launcher->client compact `pi.cfg` handoff no longer looks like a simple
  replacement-side collapse of `Morg4n` into `Noobish`

## Crash family A: late render / widget recursion / client FX render

Latest representative dump:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_39.dmp`

Backtrace:
- `d3d9+0x32e2c`
- `client.dll:0x6233821a`
- `client.dll:0x62452827`
- `client.dll:0x6244ef4d`
- `client.dll:0x624330b7`
- `client.dll:0x6244ad4b`
- repeated `client.dll:0x624330b7`
- `client.dll:0x62429a29`
- `client.dll:0x6217477f`
- `client.dll:0x62006c8e`
- `client.dll:0x6200118a`

Current named function map:
- `0x62006c30 = ClientShell_MainLoopPumpAndRunFrame`
- `0x621736f0 = ClientShell_RunFrame`
- `0x62159ef0 = ClientShell_RenderFrameAndPresent`
- `0x624299d0 = WidgetManager_DrawCurrentRootWidget`
- `0x62432fa0 = UIWidget_DrawChildWidgetsRecursive`
- newer deeper no-popup family also now names:
  - `0x6219af00 = ClientFxManager_RenderAll`
  - `0x622e5f10 = RenderFxAttachmentGroup_Draw`
  - `0x622f9530 = RenderFxBillboardGroup_Draw`
  - `0x62337440 = RenderDevice_DrawPrimitiveBatchPositionColored`

Current best read:
- the active replacement route survives the immediate late-login handoff
- then later dies during recursive widget drawing / render submission
- the deepest client frames now look like ordinary UI/render traversal rather than direct login
  mediator logic
- newer replacement-only reruns can also surface a `D3D Error` popup first, e.g.:
  - `MXODirect3DDevice9::CreateD3D9Shader() failed to compile shader ...`
  - user cross-check now says original launcher/client does **not** reproduce that popup on the same
    comparison pass, so treat it as replacement-specific evidence, not a generic Wine/original
    graphics fault
- on the popup run, client-shell field `+0xd0` is already a non-null object with vftable
  `0x628b1638`, now identified as `CLTRemoteCommCtx`-like rather than an arbitrary unknown object
- live original comparison now proves that a healthy original route also installs the same
  `0x628b1638` RCC-like object into client-shell `+0xd0`, and later cleanly calls its `+0x68`
  predicate from `ClientShell_RunFrame`
- practical consequence: the active replacement bug is no longer "why does RCC exist?" but
  "what late replacement-specific state/timing difference makes the otherwise-normal RCC/runtime
  path poison rendering or sometimes crash the later `+0x68` call?"

## Crash family B: alternate null-vcall in `ClientShell_RunFrame`

Representative dump:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_38.dmp`

Backtrace:
- `EIP = 0x00000000`
- return address on stack = `client.dll:0x62173bdc`
- then back out through:
  - `client.dll:0x62006c8e`
  - `client.dll:0x6200118a`

Exact static site:
```asm
62173bcd  mov ecx,[esi+0xd0]
62173bd7  mov edx,[ecx]
62173bd9  call [edx+0x68]
```

Important dump fact from this family:
- crash-time `EDX = 0x0a2eb1c8`
- that is heap-shaped, not a static client `.rdata` vftable address

Current best read:
- client-shell field `+0xd0` sometimes holds a wrong/incomplete runtime object on the replacement
  route
- or at least one whose first dword is not the expected polymorphic vftable for this call site

## Practical consequence

A new narrow runtime ablation now exists for the producer-side queue-context correction that landed
in commit `ab28b26`:
- default behavior remains the current higher-fidelity/original-backed direct-connection queue
  context on parsed-packet / status / close producers
- setting `MXO_USE_QUEUE_CONTEXT_BRIDGE=1` reverts only those producer contexts back to the older
  source-owned queue-context bridge
- this is intended only for A/B late-render/crash testing; it is **not** itself a conclusion that
  the older bridge mode is more faithful
- latest user A/B result: this producer-side ablation made **no meaningful difference** to the main
  graphics corruption / first-few-frames / late-render crash behavior, so the primary remaining bug
  is no longer best explained by the `ab28b26` context shape itself

The current crash investigation should stay focused on:
1. how client-shell field `+0xd0` is populated on the late runtime path
2. which later render/fx path reaches the eventual d3d9 crash
3. what replacement-specific late state/data mismatch poisons the later shader/material path even
   though:
   - the `+0xd0` RCC-like object family now matches the healthy original route closely enough
   - the current state9 callback blob transform now cross-checks against a live original sample
   - and the queued margin close path now also reaches the natural later callback/event tail
4. popup-specific shader path is now concretely narrowed too:
   - replacement now dumps the in-memory compiled source to
     `C:\users\morgan\AppData\Local\The Matrix Online\Shaders\a7f16968.fx`
   - current failing source proves the generated `VS_INPUT` struct is empty while `vs_main` still
     uses `input.pos`, which explains the popup compiler error directly
   - a bounded diagnostic retry that synthesizes `float3 pos : POSITION;` can suppress the popup,
     but the run can still continue into later graphics corruption + deeper render crash, so the
     empty-`VS_INPUT` compile failure is a real bug but not obviously the final root cause
5. current named no-popup crash chain is now narrower too:
   - `ClientFxManager_RenderAll` (`0x6219af00`)
   - `RenderFxAttachmentGroup_Draw` (`0x622e5f10`)
   - `RenderFxBillboardGroup_Draw` (`0x622f9530`)
   - `RenderDevice_DrawPrimitiveBatchPositionColored` (`0x62337440`)
   - then top d3d9 crash
6. practical current read:
   - the remaining blocker now looks increasingly like replacement-specific character/fx render
     state, not the old margin close/event tail itself
   - newest cross-character `pi.cfg` evidence sharpens that further:
     - `Morg4n` still reaches the usual rich 30-entry live table before the familiar render-family
       crash
     - an intermittent `Noobish` crash can still happen, but the sampled run used the sparse 5-entry
       live table and landed in an older/deeper d3d9 family instead of proving the same
       `Morg4n`-style rich-table corruption path
   - newest non-crash rerun adds a third late symptom family worth keeping separate from both crash
     stacks:
     - replacement can now launch into game, keep music alive, show corrupted graphics, and appear
       to stop after an initial frame without immediately crashing
     - on that rerun the late diagnostic thread only observed client-shell `state20 = 2` and did
       **not** later see the earlier crash-family `state20 = 3` transition before the user stopped
       the session
     - the same rerun also showed that every `DAT_629ea4e8` 12-byte slot retained the same third
       dword `0x627d7b00` while the compact live `pi.cfg` adopter only populated the first two
       dwords for the active 30 ids
     - practical consequence:
       - the next narrow question is whether that common retained slot-tail value is original/benign
         or whether later client code expects a richer per-entry third dword before the render/fx
         path becomes stable

## Intermittent secondary crash family

A separate intermittent replacement-only failure still appears from time to time:
- process termination through `std::bad_alloc` / C++ termination rather than the ordinary late d3d9
  access-violation family
- user reports this secondary failure was easier to trigger around commit `ab28b26`, but the newer
  queue-context producer ablation showed no meaningful change in the main late render corruption /
  crash path
- current practical treatment:
  - keep it tracked as a **secondary** issue until it becomes the dominant repro again
  - source now installs diagnostic `std::new_handler` / `std::terminate` hooks so the next hit will
    log whether termination really comes from `std::bad_alloc` or some other uncaught C++ exception

## Non-root-cause notes

These are no longer the best primary crash explanations:
- early auth/bootstrap fidelity
- immediate event-`0x18` handoff failure
- `mcd.cfg` body mismatch
  - current active original/reference and replacement `mcd.cfg` are bit-identical again
- original temp-copy / alternate AppData path side effects
  - user explicitly does **not** want those reproduced for now

## Current supporting diagnostics in source

`src/resurrections.cpp` now logs extra crash-time client context for this path:
- `DAT_629e68a8` client-shell slot
- client-shell state around `+0x18 .. +0x34`
- client-shell runtime-object field `+0xd0 .. +0xec`
- `client shell +0xd0` pointed object and first vftable words when readable
- render-family globals used by the late frames:
  - `DAT_629f84e8`
  - `DAT_629f1748`
  - `DAT_62a01e5c`
  - `DAT_62a333a4`
- newest direct D3D9-device hooks now also log:
  - `Direct3DCreate9` / `IDirect3D9::CreateDevice`
  - per-frame `Present` counts
  - last seen FVF / vertex declaration / stream0 / indices state
  - last draw-call shape (`DrawPrimitive*` / `DrawIndexedPrimitive*`) at crash time
  - one-time warnings when draws happen with missing vertex layout / missing stream0 / missing indices
- newest replacement rerun with those hooks materially tightened the render symptom:
  - this is **not** literally a one-present/one-frame path
  - the crashing run reached at least `50+` successful `Present` calls before the late d3d9 crash
  - early frames were dominated by `DrawPrimitiveUP` triangle-fan quads with stride `28`
  - later by crash time the device had seen both:
    - many `DrawPrimitiveUP`
    - and a smaller number of `DrawIndexedPrimitive`
  - suspicious raw draw caller addresses are now concrete too:
    - `0x6233755e` inside `RenderDevice_DrawPrimitiveBatchPositionColored`
    - `0x6233821a` / `0x623382cf` inside neighboring unnamed batch-draw helper `0x62337d70`
    - `0x62337f29` inside that same `0x62337d70` helper
    - `0x6235e309` inside `0x6235e2e0`
- important instrumentation correction:
  - the first D3D9 device-hook pass had `SetVertexDeclaration` / `SetFVF` vtable indices off by one
  - the corrected pass now shows replacement **does** bind vertex declarations before the suspicious
    draws; the raw draw sites are no longer best explained by a literal missing-declaration state
- newest corrected suspicious draw sites now isolate concrete client render helpers:
  - `0x6233755e` inside `RenderDevice_DrawPrimitiveBatchPositionColored`
  - `0x6233821a` / `0x623382cf` / `0x62337f29` inside neighboring batch helper `0x62337d70`
  - `0x6235e309` inside `0x6235e2e0`
- practical consequence:
  - the current best read is now “client keeps presenting corrupted frames while issuing malformed
    or poisoned draw batches for dozens of frames”, not “no layout state was bound at all”
  - because user explicitly believes the root still leads back into launcher reimplementation, keep
    checking launcher-side state8/live-corpus contracts — but two formerly suspicious anomalies have
    now been demoted by original-live proof:
    - original `matrix.exe` breakpointing at launcher getters `0x41f120 / 0x41ae60` shows the
      active path really does reach `+0x80 == 1` with `+0xac -> null` / length `0`
    - original `matrix.exe` breakpointing at `0x41f110 / 0x41ae40` also matches replacement `bl.cfg`
      shape: length `0x000e`, first dword `0x00006dd0`, string tail `"Nicodemus"`
    - original `matrix.exe` breakpointing at `0x41f170 / 0x41f180 / 0x41aec0` also matches the
      current replacement state8 persistence family closely enough on the active route:
      - header `+0xf48` lead values
      - body first dword `0x00000130`
      - overflow low-word length `0x0029`
    - so neither the odd empty-`il.cfg` live contract, nor the small live `bl.cfg` payload, nor the
      current raw `+0xbc/+0xc0/+0xc4` persisted bytes are good replacement-only explanations by
      themselves
    - the replacement-side attempt to suppress the `il.cfg` flag on zero-byte section-2 payloads was
      a fidelity regression and has already been reverted
    - a newer refcount-fidelity pass also tightened one concrete replacement difference on the
      active path:
      - shared base refcount contract `0x004b211c` is correctly non-interlocked
      - but message-object `Release()` on the replacement path had extra guard logic before the
        original interlocked decrement helper
      - source now removes that lower-fidelity precheck so the message-storage / outer-message-ref
        families behave more like the original `0x42f860` leaf helper
      - source also now heap-materializes the receive-side outer message-ref in
        `CMessageConnection::OnOperationCompleted` instead of keeping it on the stack for the
        current callback only
      - practical current result: this is a fidelity improvement to lifetime semantics, but the
        ordinary replacement run still reproduces the late graphics corruption / crash, so another
        ordering/lifetime mismatch likely remains further downstream
    - current launcher-side suspicion should therefore move one layer deeper than the raw section
      bytes themselves, e.g. toward ordering / timing / client-side object-materialization effects
      downstream of those launcher-provided contracts

Use those logs on the next rerun before widening scope.
