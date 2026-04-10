# launcher owner `0x4f78b8` auth-bootstrap child at `+0x680`

## Purpose

This is the canonical object-level home for the separate phase-2 auth/bootstrap child rooted at
launcher owner `0x4f78b8 + 0x680`.

Keep the split explicit:
- `../auth/README.md` = auth protocol / wire-loop overview
- `0x4d2c58_ILTLoginMediator_Default.md` = arg6 / interface-facing consequences
- this doc = child ownership, layout, lifecycle, and field-family tightening

## Current best identity

High-confidence current read:
- this is a **separate heap child object**, not the mediator object itself
- owner init allocates it and stores it at owner `+0x680`
- primary vtable = `0x004b7134`
- the active ready-side dispatcher is `0x448050`

High-value construction/use anchors:
- owner init / store at owner `+0x680`:
  - `0x41b160`
- child ctor chain:
  - `0x441290`
  - `0x445500`
- primary vtable:
  - `VTABLES/0x004b7134.md`
- child `+0x54` helper subobject vtable:
  - `VTABLES/0x004b695c.md`

## Why this split exists

This topic had started to sprawl across broader docs that were supposed to stay focused on other
questions:
- arg6 surface consequences
- auth protocol loop
- arg5 network-engine ownership

The child itself now has enough narrowed structure and lifecycle evidence to deserve its own
canonical doc.

## `0x448050` prepare/dispatch call shape

`0x439210` reaches `0x448050` as the main ready-side staging/dispatch entry for this child.

Current best recovered destination mapping from the `0x448050` disassembly:
- child `+0x04`  <- arg1 C-string
- child `+0x10`  <- arg2 C-string
- child `+0x1c`  <- arg8 C-string, or fallback static string at `0x4aafbb` when null
- child `+0x28`  <- arg3 dword
- child `+0x2c`  <- arg4 dword
- child `+0x30 .. +0x3f` <- 16-byte block from arg5 when non-null
- child `+0x40 .. +0x4f` <- 16-byte block from arg6 when non-null
- child `+0x50`  <- arg7 send target / callback target

Direct branch condition recovered there:
- `mov al, [child+0xa0]`
- `test al, al`
- zero  -> `0x447eb0` / raw `0x06`
- nonzero -> `0x4474f0` / raw `0x08`

So `+0xa0` is a **byte readiness flag**, not a helper pointer.

Bridge consequence for source:
- `0x439210` calls `0x448050` directly
- current Ghidra callers/callees do **not** show a separate launcher mediator wrapper between
  those two methods
- replacement source should therefore call the separate owner `+0x680` child directly from
  state/owner flow instead of keeping a fake mediator-owned auth-bootstrap bridge

## Tightened field-family summary

### Stable enough current names/roles

| Offset | Current best read | Strongest anchors |
|---:|---|---|
| `+0x28` | raw `0x08` loginType low byte | `0x4474f0` |
| `+0x2c` | launcher-version dword used by raw `0x06`; state2 `0x439210` feeds it from owner getter vtable `+0x20` / `0x41f070` returning owner `+0x08`, with paired nopatch setter vtable `+0x1c` / `0x41f060` | `0x439210`, `0x447eb0` |
| `+0x54` | child-owned helper/transform subobject | `0x445500`, `VTABLES/0x004b695c.md` |
| `+0x80` | time-delta/cache dword later consumed during auth-reply validation | `0x448140`, `0x44aec0` |
| `+0x85 .. +0x94` | 16-byte challenge/material family | `0x448140`, neighboring corroboration `0x4429b0 / 0x441470` |
| `+0x94` | large transform helper allocated on raw `0x08` send path | `0x4474f0`, `VTABLES/0x004b7620.md` |
| `+0x98` | small transform helper allocated on raw `0x08` send path | `0x4474f0`, `VTABLES/0x004b74d4.md` |
| `+0x9c` | current reply/request public-key id dword for this child | `0x447eb0`, `0x4474f0`, `0x447780` |
| `+0xa0` | auth-request-ready byte | `0x447f50`, `0x448050` |
| `+0xa4` | lazy `pubkey.dat`-backed state | `0x447260`, `0x447c10`, `0x447eb0`, `0x447f50` |
| `+0xa8` | raw `0x08` reply-public-key worker | `0x447780`, `0x4474f0`, later `0x41f370` through copied `+0xf4` block |
| `+0xac` | sibling reply-validation / transform family; still not tightly typed | `0x447780`, `0x448140`, `0x44aec0` |
| `+0xf4` | reply-derived copied `0x136` block | `0x448140` |
| `+0xf8` | three-dword small-string object | `0x441290`, `0x41f3c0` |
| `+0x104` | crashreporter prompt flag byte | `0x41f390` |

### Important restraint on `+0x9c`

For **this child**, `+0x9c` still reads best as the current auth public-key id dword.

Do **not** conflate that with the neighboring `0x4429b0 / 0x441470` sibling-object path where a
separate object also has a `+0x9c` field that behaves like a wrapper/object pointer.
Those two `+0x9c` reads do not need to belong to the same type.

## Lifecycle

### 1. Before raw `0x06`

On the prepare/dispatch path:
- child strings / fixed blocks are staged by `0x448050`
- `+0xa0` is still zero
- branch therefore selects `0x447eb0`

### 2. Raw `0x06` send path (`0x447eb0`)

Current best read:
- lazily ensures `+0xa4` through:
  - `0x447260 = AuthBootstrap680_CreateLazyPubkeyDatState`
  - `0x447c10 = AuthBootstrap680_InitializeLazyPubkeyDatState`
- builds and sends the raw `0x06` packet
- packet uses:
  - child `+0x2c` = launcher-version dword
  - child `+0x9c` = current public-key id dword
  - child `+0x50` = send target
- send step itself is a direct virtual call through child `+0x50` slot `+0x24`

So `+0xa4` is no longer best described as generic lazy state; it is specifically the lazy
`pubkey.dat`-backed state family used on the get-public-key path.

### 3. Raw `0x07` handling (`0x447f50 -> 0x447780`)

Current best read:
- `0x447f50 = AuthBootstrap680_HandleGetPublicKeyReply`
- on the success path it ensures/reuses `+0xa4`
- then `0x447780 = AuthBootstrap680_RebuildReplyPublicKeyWorkers` rebuilds the worker family
- that path:
  - stores the current key id at child `+0x9c`
  - rebuilds child `+0xa8`
  - rebuilds child `+0xac`
- and finally `0x447f50` sets byte `+0xa0 = 1`

That is the key current reason the later `0x448050` branch chooses raw `0x08` instead of raw
`0x06`.

### 4. Raw `0x08` send path (`0x4474f0`)

Current best read:
- packet uses:
  - child `+0x28` low byte as loginType
  - child `+0x9c` as public-key id
  - child `+0x30 .. +0x4f` as two fixed 16-byte blocks
  - child `+0x50` as the send target
- send step itself is again the direct virtual call through child `+0x50` slot `+0x24`
- the same path also allocates/rebuilds helper objects at:
  - child `+0x94`
  - child `+0x98`
- and consumes the reply-public-key worker at child `+0xa8`

Current conservative naming guidance remains:
- `+0xa8` is clearly a worker/object family in the live reply-public-key path
- but it is still not safe to collapse it into a concrete crypto class name

### 5. Raw `0x09` / challenge-material continuation

The same broader bootstrap area also uses the shared `+0x85 .. +0x94` material family later.

Current strongest direct child-side anchor:
- `0x448140` case `0x09`
- narrowed send/build subrange now visible inside that case:
  - `0x44831c .. 0x448467`
  - current field mapping there is:
    - child `+0x10` -> first password string fed into the raw `0x0a` builder
    - child `+0x1c` -> second password/station string fed into the same builder
    - child `+0x54` -> 16-byte challenge-material family read for the build
    - child `+0x98` -> helper used during the encrypted reply-body build step
    - child `+0x50` -> direct send target through vtable slot `+0x24`

Neighboring corroboration:
- `0x4429b0`
- `0x441470`

Current best combined read:
- the `+0x85` family is real and reused
- Ghidra callers currently show `0x43f300` reaching this work only through
  `0x448140 = AuthBootstrap680_HandleInboundAuthMessage`
- the narrowed raw-`0x0a` builder/send at `0x44831c..0x448467` is inline inside that child helper,
  not a separate launcher mediator method
- but the `0x4429b0 / 0x441470` path is now tightened further as a **neighboring margin-connection
  object** path:
  - `0x4429b0 = CBaseMarginConnection_HandleCode2CertChallengeAndSendResponse` is reached from
    `CBaseMarginConnection::DispatchMessage` consumed code-2 handling
  - `0x441470` lazily builds/refeshes connection `+0x9c = CStreamPacketEncryptionModule`
  - that same `0x4429b0` body then sends raw code `0x03` through the margin connection's own
    vtable `+0x24` packet-builder/message-ref path, not through this auth child's `+0x50`
- so that path is still not proof that every `+0x85/+0x9c` write there belongs to this exact child
  type

### 6. Raw `0x0b` / auth-reply adoption (`0x448140`)

Current best read:
- `0x448140 = AuthBootstrap680_HandleInboundAuthMessage`
- on the raw `0x0b` success path it validates an auth-data field length of `0x136`
- validation runs through child `+0xac` and the time-delta/cached state behind child `+0x80`
- then it heap-copies that `0x136` auth-data field into child `+0xf4`
- important later-path restraint from Ghidra callee review:
  - `0x4401a0 = CLTLoginState_State10_Slot6_HandleSecondaryMessage` does **not** call `0x448140`
  - its callees stay inside the local state10 parse/object/slot-record/event helpers
  - so any replacement helper shared between the broader state2 auth-reply path and the later
    state10 auth-reply path is a deliberate source bridge, not a newly discovered standalone
    launcher method

Current tighter copied-block layout from static `0x448140 / 0x44add0 / 0x44aec0` plus live source logs:
- `+0xf4 + 0x00 .. +0x7f` = 128-byte auth-signature span
- `+0xf4 + 0x80 .. +0x135` = signed-data span (`0xb6` bytes)
- high-value later exposures inside that signed-data suffix:
  - `+0xf4 + 0x85`
  - `+0xf4 + 0xa8`
  - `+0xf4 + 0xac` = expiry-time dword used by `0x44add0 / 0x44aec0`

Important neighboring-success restraint:
- child `+0xf8` and byte `+0x104` are **not** written by `0x448140` itself
- those are instead updated later by the surrounding `0x43f300` success branch through helper
  `0x441330 = AuthBootstrap680_SetPromptPasswordF8AndSecurIdFlag`
  - copy owner `+0x94 + 0x20` into child `+0xf8`
  - detect the concrete slash+6-digit SecurID tail shape
  - set child `+0x104` from that test
  - strip that trailing `/dddddd` suffix from child `+0xf8` when present
- the same `0x43f300` neighboring success cluster also refreshes owner `+0x94 + 0x00`
  through owner vtable `+0x150` from `0x43d480 = AuthBootstrap680_CopyReplyString54`
- current source now mirrors that narrower subset with the original split kept explicit:
  - pre-gate `0x441330 = AuthBootstrap680_SetPromptPasswordF8AndSecurIdFlag`
    - child `+0xf8`
    - child `+0x104`
    - child `+0x110` from the parsed success-header dword at payload offset `0x07`
  - gated once on the surrounding state2 success path
    - broader owner `+0xd84/+0x688/+0x818` materialization
    - child `+0x114/+0x118` through `0x441260 = AuthBootstrap680_StoreField114AndTimestamp118`
    - owner `+0x94 + 0x00`
    - one-time `PostEvent(6)`
    - `Profiles/<username>/characters.ini` write from the rebuilt slot/route tables
    - child `+0x108/+0x10c` through `0x441170 = AuthBootstrap680_CopyOpaqueReplyBlobs108_10c`
- current strongest replacement-side candidate maps those two opaque copied blob families to:
  - parsed auth-signature bytes
  - parsed encrypted-private-exponent bytes
  but keep that specific blob-to-field pairing provisional until stronger same-object proof lands
- current remaining uncertainty is mainly the exact meaning of those copied reply fields/blobs, not whether the side effects exist

## Arg6-facing consequences

The broad arg6 doc should only keep the interface-facing consequence, but the child lifecycle is:

- owner vtable `+0x50 / 0x41f370`
  - returns `owner +0x680 -> +0xf4 -> +0xa8`
- owner vtable `+0x54 / 0x41f0b0`
  - truthiness test for that same value
- owner vtable `+0x58 / 0x41f390`
  - returns child `+0x104`
- owner vtable `+0x5c / 0x41f3a0`
  - returns `owner +0x680 -> +0xf4 + 0x85`, else static fallback
- owner vtable `+0x60 / 0x41f3c0`
  - returns the first dword / begin pointer of child `+0xf8`

Practical consequence for the replacement launcher:
- arg6 `+0x50` should stay null until the successful raw `0x07`/raw `0x0b` chain has materially
  reached copied-block adoption at child `+0xf4`
- a permanent fake non-null `+0x50` is less faithful than the real lifecycle

## Source-ownership consequence

Ctor/reset restraint now tightened from `0x445500 / 0x441290`:
- construction zeros the child's dynamic families across `+0x80 .. +0x118`
- replacement reset helpers should therefore aim to restore that ctor-like transient subset only
- fixed config mirrors such as child `+0x28/+0x2c/+0x9c` stay better modeled under the separate
  prepare/config bridge feeding `0x448050 / 0x447eb0 / 0x4474f0 / 0x447780`

Current replacement source keeps this split explicitly:
- source now also stores the bootstrap child as a separate child object rather than flattening it
  into the mediator body, which better matches the original owner `+0x680` pointer relationship
- original child `+0xf4` = full reply-derived copied `0x136` block
- source-owned mirror now preserves that full structural width with the tighter current layout:
  - `+0x00 .. +0x7f` signature span
  - `+0x80 .. +0x135` signed-data span
  - later wrapper/state users reached through the signed-data suffix (`+0x85`, `+0xa8`, `+0xac`)
- the remaining bytes still stay structural rather than semantically over-named

That is deliberate.
It keeps the runtime path working without over-claiming that every byte of the copied block is
already fully typed in source.

Current bounded follow-up consequence:
- this mirror is now enough to source-own the neighboring state5 helper chain directly on the live
  existing-character path, with the newer static-RE split kept explicit:
  - mediator helper `0x41b500` is a direct body, not a login-mediator vtable method
  - child helper `0x4435f0` first forwards child `+0xf4` into margin connection vtable `+0x44 /
    0x41ce80`, rebuilding connection `+0x98`
  - that same `0x4435f0` body then calls standalone helper `0x443340`
  - `0x443340` allocates a fresh `0xe0` prep object, seeds it from child
    `+0xb0/+0xc4/+0xd8`, and stores the resulting pointer at margin connection `+0xa0`
    - newer constructor-side tightening now keeps that object as its own class again instead of an
      opaque byte sidecar:
      - `0x443220 = CMarginConnectionBootstrapPrepStateA0_ctor`
      - `0x443390 = ~CMarginConnectionBootstrapPrepStateA0`
      - inner subobject method `0x465d70 = InitializeFromBootstrapBlocks`
      - current field map carried in source now mirrors the Ghidra field names at
        `+0x00/+0x04/+0x08/+0x0c/+0xd0/+0xd4/+0xd8/+0xdc`, with the `+0x0c` subobject exposing
        the copied/derived big-int family at `+0x08/+0x1c/+0x3c/+0x50/+0x64/+0x78/+0x8c/+0xa0`
  - `0x41f30` then sends the raw type-`1` prefix with bytes `01 00 00`, then the copied `0x136`
    block
- live replacement log on `2026-04-10` now shows:
  - state5 slot3 `replyCopyShadowStillValid=1`
  - non-null child `+0xf4`
  - `AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0`
  - `CMarginConnectionBootstrapPrepStateOwner_0x443340`
  - `CMarginConnection::SendStoredBootstrapReplyCopy98` with `payloadBytes=0x139`
- immediate current limitation after that milestone:
  - the margin socket then returns EOF, so the next fidelity blocker has moved from
    “materialize child `+0xf4`” to “make the raw type-1 state5 packet acceptable enough for the
    server/original continuation”

## Reimplementation guidance

1. keep `+0xa0` named/treated as a **byte readiness flag**, not a pointer/helper
2. keep `+0xa4` tied to the lazy `pubkey.dat` state family
3. keep `+0xa8` conservative as a reply-public-key worker/object family
4. keep `+0xf4` documented as a **reply-derived copied block**, even if source currently narrows it
5. do not use sibling `0x4429b0 / 0x441470` `+0x9c` behavior to overwrite the child `+0x9c`
   meaning without stronger same-type proof

## Related docs

- `README.md`
- `0x4d2c58_ILTLoginMediator_Default.md`
- `0x4d6304_network_engine.md`
- `../auth/README.md`
- `../VTABLES/0x004b7134.md`
- `../VTABLES/0x004b695c.md`
- `../VTABLES/0x004b7620.md`
- `../VTABLES/0x004b74d4.md`
