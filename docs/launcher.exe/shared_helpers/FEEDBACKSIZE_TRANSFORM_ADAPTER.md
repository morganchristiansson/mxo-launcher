# FeedbackSize / AssemblyTwofish transform helper family

## Scope

Canonical notes for the shared launcher helper rooted at:

- `0x41df60 = FeedbackSizeTransformAdapter_ConstructSmall`
- `0x446d90 = FeedbackSizeTransformAdapter_ConstructLarge`
- `0x44b190 = FeedbackSizeTransformAdapter_InvokeConfigure40`
- `0x44b570 = FeedbackSizeTransformAdapter_TransformBuffer`
- `0x41c750 = zero + tracked-free helper used by the adapter destructors`

This is the same family reused by:

- state9 callback blob fill (`0x41e690`)
- auth-bootstrap send prep (`0x4474f0`)
- stream-packet encryption worker setup (`0x44d820` / `0x44d910`)

## Current static-RE-closed read

### Small vs large constructor split

`ConstructSmall` and `ConstructLarge` are not generic placeholder builders anymore.

Current strongest read is:

- **small** constructor branch = encrypting `AssemblyTwofish` worker family
  - seen on state9 callback tail transform
  - seen on auth-bootstrap child `+0x98`
  - seen on stream-packet write-helper setup
- **large** constructor branch = decrypting sibling
  - seen on auth-bootstrap child `+0x94`
  - seen on stream-packet read-helper setup

Both constructors:

- capture a 16-byte source block
- capture a caller-supplied `IV` pointer
- configure the worker through the shared named-parameter family
  - confirmed strings:
    - `"AssemblyTwofish"`
    - `"FeedbackSize"`
    - `"ValueNames"`
    - `"Rounds"`
- allocate two 16-byte tracked buffers at adapter `+0x14` and `+0x20`
- large branch also allocates an extra 16-byte tracked buffer at `+0x2c`

All currently recovered callers pass a **zero IV** block:

- `DAT_004d4d50`
- `DAT_004f7c2c`
- `DAT_004f7f88`

## TransformBuffer (`0x44b570`)

`0x44b570` is a chunk walker over the configured worker, not a one-off bespoke state9 helper.

Recovered behavior:

- uses a 16-byte transform width
- uses a 4-byte alignment quantum
- walks the caller span in 16-byte chunks
- when source/destination alignment does not suit the worker, it stages through helper-owned
  scratch buffers instead of assuming direct in-place access is always safe
- dispatches the per-block transform through the worker vtable after construction-time retabling

Practical source consequence:

- state9 replacement should not call a naked one-shot crypto helper and skip the helper object
  lifetime entirely
- auth/message workers should keep the constructed helper objects explicit, even when later packet
  semantics are already source-owned at a higher level

## `0x41c750`

`0x41c750` is the adapter-family free helper used by the small/large destructors.

Recovered behavior:

1. zero the whole buffer
2. free through tracked size bins:
   - `< 0x11`
   - `< 0x21`
   - `< 0x281`
   - `< 0x501`
   - `< 0x1001`
3. otherwise fall back to the general tracked free path

The paired allocator is `0x41d2e0` with the same thresholds.

## Source ownership status

Source now keeps this family explicit as concrete helper objects instead of collapsing it to raw
`std::vector`/direct-Twofish calls:

- state9 callback blob uses the small/encrypting helper object
- auth-bootstrap child `+0x94/+0x98` now keeps the recovered large/small helper allocations live
- stream-packet read/write workers now retain the recovered large/small helper objects alongside the
  already confirmed packet-level encrypt/decrypt semantics
