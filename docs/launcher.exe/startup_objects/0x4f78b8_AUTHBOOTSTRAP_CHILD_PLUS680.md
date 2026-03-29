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

## Tightened field-family summary

### Stable enough current names/roles

| Offset | Current best read | Strongest anchors |
|---:|---|---|
| `+0x28` | raw `0x08` loginType low byte | `0x4474f0` |
| `+0x2c` | launcher-version dword used by raw `0x06`; state2 `0x439210` feeds it from owner getter vtable `+0x20` / `0x41f070` returning owner `+0x08`, with paired nopatch setter vtable `+0x1c` / `0x41f060` | `0x439210`, `0x447eb0` |
| `+0x54` | child-owned helper/transform subobject | `0x445500`, `VTABLES/0x004b695c.md` |
| `+0x80` | time-delta/cache dword later consumed during auth-reply validation | `0x448140`, `0x44aec0` |
| `+0x85 .. +0x94` | 16-byte challenge/material family | `0x448140`, neighboring corroboration `0x4429b0 / 0x41470` |
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

Do **not** conflate that with the neighboring `0x4429b0 / 0x41470` sibling-object path where a
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

Neighboring corroboration:
- `0x4429b0`
- `0x41470`

Current best combined read:
- the `+0x85` family is real and reused
- but the `0x4429b0 / 0x41470` path is best treated as a **neighboring/sibling object** path,
  not as proof that every `+0x85/+0x9c` write there belongs to this exact child type

### 6. Raw `0x0b` / auth-reply adoption (`0x448140`)

Current best read:
- `0x448140 = AuthBootstrap680_HandleInboundAuthMessage`
- on the raw `0x0b` success path it validates marker `0x0136`
- validation runs through child `+0xac` and the time-delta/cached state behind child `+0x80`
- then it heap-copies a `0x136` block into child `+0xf4`

That copied block is the important later exposure root:
- `+0xf4 + 0x85`
- `+0xf4 + 0xa8`

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

Current replacement source keeps this split explicitly:
- original child `+0xf4` = full reply-derived copied `0x136` block
- source-owned mirror now preserves that **full structural width** too, but still only names a
  narrower subset confidently:
  - `+0x85 .. +0x94`
  - `+0xa8`
- the remaining bytes stay structural/opaque rather than semantically over-named

That is deliberate.
It keeps the runtime path working without over-claiming that the entire copied block is already
fully typed in source.

Current bounded follow-up consequence:
- this wider structural mirror is now enough to source-own the neighboring state5 helper chain
  shape behind:
  - `0x41ce80` copy into margin connection `+0x98`
  - `0x41f30` raw-type-`1` prefix plus stored `0x136` block send
- but that chain remains dormant on the working path until the full copied block is validated more
  strongly; the current launcher-owned bootstrap continuation still preserves the known-good route

## Reimplementation guidance

1. keep `+0xa0` named/treated as a **byte readiness flag**, not a pointer/helper
2. keep `+0xa4` tied to the lazy `pubkey.dat` state family
3. keep `+0xa8` conservative as a reply-public-key worker/object family
4. keep `+0xf4` documented as a **reply-derived copied block**, even if source currently narrows it
5. do not use sibling `0x4429b0 / 0x41470` `+0x9c` behavior to overwrite the child `+0x9c`
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
