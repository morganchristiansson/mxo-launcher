# Family - `0x004c0540` non-interlocked refcounted buffer/helper cluster

## Current best family name

The exact original semantic class names are still unresolved.

Current safest family-level read is:
- an **abstract non-interlocked refcounted helper base** at `0x004c0540`
- with two presently visible subgroups:
  1. byte-storage / buffer leaves
  2. owner-linked helper leaves

This family is **separate** from the `CLTTCPReadOperation` family rooted at `0x004b211c`.
The overlap that first made this confusing is only from tiny shared helper-body reuse.

## Proven root

```text
0x004c0540  abstract non-interlocked refcounted helper base
```

So yes: for this cluster, `0x004c0540` is the true currently proven root class.

What we have **not** proven is a larger common root above both:
- `0x004c0540`
- `0x004b211c`

Those are currently best treated as **distinct roots** whose families happen to reuse a few tiny
field-access helpers.

## Current mapped hierarchy

```text
0x004c0540  abstract non-interlocked refcounted helper base
├── 0x004c0564  heap-backed variable-capacity byte-storage leaf
├── 0x004c05d0  inline 0x80-byte storage leaf
├── 0x004c05f4  inline 0x100-byte storage leaf
├── 0x004c0618  inline 0x200-byte storage leaf
├── 0x004c063c  inline 0x400-byte storage leaf
├── 0x004c0588  owner-linked heap-buffer leaf
└── 0x004c05ac  owner-linked delegating wrapper leaf
```

## Buffer-storage subgroup

These leaves share the same basic contract:
- `+0x00` = plain/non-interlocked AddRef (`0x42f7e0`)
- `+0x04` = plain/non-interlocked Release
- `+0x08` = getter over local `refcount04`
- `+0x0c/+0x10` = buffer-base getter(s)
- `+0x14/+0x18` = size getter/setter pair
- `+0x1c` = shared tiny `return 0` stub
- `+0x20` = deleting dtor

Currently mapped leaves:

| VTable | Storage model | Proven capacity / size field |
|---:|---|---|
| `0x004c0564` | heap-backed dynamic buffer | heap pointer at `+0x0c`, size field at `+0x10` |
| `0x004c05d0` | inline fixed buffer | `0x80` bytes at `+0x0c .. +0x8b`, size at `+0x8c` |
| `0x004c05f4` | inline fixed buffer | `0x100` bytes at `+0x0c .. +0x10b`, size at `+0x10c` |
| `0x004c0618` | inline fixed buffer | `0x200` bytes at `+0x0c .. +0x20b`, size at `+0x20c` |
| `0x004c063c` | inline fixed buffer | `0x400` bytes at `+0x0c .. +0x40b`, size at `+0x40c` |

### Size-based allocation helper tied to this subgroup

`0x4830f0` is the current key factory/producer for the storage leaves.

It:
- takes two optional source spans plus their lengths
- computes `totalBytes = leftLen + rightLen`
- selects one concrete storage leaf based on that total size
- initializes refcount to `1`
- writes the chosen leaf's stored byte count field
- copies the left span, then the right span, into the returned storage buffer

Current best selection ladder:
- `totalBytes <= 0x80`  -> `0x004c05d0`
- `totalBytes <= 0x100` -> `0x004c05f4`
- `totalBytes <= 0x200` -> `0x004c0618`
- `totalBytes <= 0x400` -> `0x004c063c`
- larger sizes -> `0x004c0564` heap-backed fallback

So the storage subgroup now reads as a small family of size-specialized refcounted byte buffers.

## Owner-linked subgroup

Two other currently mapped leaves branch off the same root but are **not** part of the simple
size-specialized storage ladder:

### `0x004c0588`

Owner-linked heap-buffer leaf.

Key differences from the storage leaves:
- custom release hook at `+0x04` (`0x481870`), not the plain `0x4816e0`
- owner pointer at `+0x18`
- participates in owner-managed pending/list behavior before final decrement/delete

### `0x004c05ac`

Owner-linked delegating wrapper leaf.

Key differences:
- owner pointer at `+0x10`
- child/helper pointer at `+0x0c`
- slots `+0x0c` and `+0x10` delegate into the child helper's vtable `+0x0c`
- custom release hook at `+0x04` (`0x4830c0`) that first returns the wrapper to an owner-managed
  list/helper path when the wrapper is at refcount `1`

## Important family-boundary cautions

### This is not the `CLTTCPReadOperation` family

Do **not** fold this cluster into:
- `0x004b211c`
- `0x004b2300`
- `FAMILY_CLTTCPReadOperation.md`

Those are a different receive-fragment family.

### Helper-body reuse that caused the earlier confusion

These tiny bodies are reused across unrelated families:
- `0x42f7e0` = plain `++[this+4]`
- `0x4816f0` = `mov eax,[ecx+4]`
- `0x41f090` = tiny `this+0x0c` getter
- `0x437b40` = tiny `return 0` stub

So semantic names attached to those bodies in some other family must not be projected onto this
cluster.

## Related docs

- `0x004c0540.md`
- `0x004c0564.md`
- `0x004c0588.md`
- `0x004c05ac.md`
- `0x004c05d0.md`
- `0x004c05f4.md`
- `0x004c0618.md`
- `0x004c063c.md`
- `FAMILY_CLTTCPReadOperation.md`
- `0x004b211c.md`
