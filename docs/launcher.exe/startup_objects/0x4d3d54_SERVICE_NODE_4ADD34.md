# resolver/service node (`vtable 0x4add34`)

## Summary

The registry list at `0x4d3d54+0x18` contains service/resolver nodes with primary vtable:

- `0x4add34`

These nodes are created by ctor `0x413410` and are the objects that actually process binder registration and removal for interface slots like `0x4d2c58`.

## Ctor

### Constructor
- `0x413410`

### Size
- allocated as `0x1c` bytes

### Fields initialized by ctor
- `+0x00` = vtable `0x4add34`
- `+0x04` = owned copy of interface name string
- `+0x08` = secondary constructor argument / discriminator
- `+0x0c` = dynamic array object
- `+0x10` = dynamic array object
- `+0x14` = current/active pointer, initially `0`
- `+0x18` = dynamic array object

So a practical current model is:

```c
struct ResolverNode {
    void*    vtbl;          // +0x00 = 0x4add34
    char*    nameCopy;      // +0x04
    uint32_t tagOrTarget;   // +0x08
    DynArray arr0;          // +0x0c
    DynArray arr1;          // +0x10
    void*    active;        // +0x14
    DynArray arr2;          // +0x18
};
```

## Vtable block

From `0x4add34`:

| Index | Address | Current interpretation |
|---|---:|---|
| 0 | `0x413dc0` | destructor / release owned arrays |
| 1 | `0x414490` | bulk fan-out over wrapper-owned collection |
| 2 | `0x4145a0` | deleting destructor helper |
| 3 | `0x413d10` | register wrapper into node (`0x413880`) |
| 4 | `0x413c60` | register wrapper into node (`0x413840`) |
| 5 | `0x414090` | resolve/dispatch wrapper through node |
| 6 | `0x4136c0` | remove wrapper from arr2 path |
| 7 | `0x413640` | remove wrapper from arr1 path |
| 8 | `0x413be0` | route wrapper into node (`0x4138c0`) |
| 9 | `0x413740` | validate/activate wrapper (`0x413510`) |
| 10 | `0x413340` | comparator / ordering helper family |
| 11 | `0x414610` | alternate destructor path |

Indices after 9 are still provisional because the table boundary is inferred from nearby data.

## Dynamic array helpers used by the node

The three array-like fields at `+0x0c`, `+0x10`, `+0x18` use shared helpers:

- `0x412fe0` : initialize small dynamic array object
- `0x413170` : grow backing storage
- `0x4135f0` : insert at index
- `0x413030` : remove by pointer (preserve order)
- `0x413090` : remove by pointer (swap with last)
- `0x4130d0` : binary/linear search with comparator callback

That confirms these are owned collections of pointers.

## Key behavior

### Wrapper activation / current pointer tracking
`0x413570` updates `node->active` (`+0x14`) and notifies objects stored in `+0x10` by calling each object's vtable `+0x08` with the active pointer.

### Wrapper validation path
`0x413510` validates that the candidate wrapper is the most recent element in `+0x18`, then calls the wrapper's vtable `+0x08`.
This is consistent with "activate this binder" semantics.

### Registration paths
Several methods (`0x413c60`, `0x413d10`, `0x413be0`) all:
- find or allocate a resolver node by interface name,
- insert it into a registry-owned collection,
- then push the wrapper into one of the node's dynamic arrays.

### Removal paths
`0x413640` and `0x4136c0` remove wrappers from the node's arrays and delete the node when all three arrays become empty.

## Why this matters

This is the missing middle layer between:

- binder wrapper object (`ILTLoginMediator.Default` slot binder), and
- final live pointer in `0x4d2c58`.

The service node owns the collections and activation state that determine when the binder wrapper gets called to write the slot.

## Current implication

A truly original-faithful launcher recreation may need at least a minimal reconstruction of:

1. wrapper/binder object (`0x4aae04`)
2. registry object (`0x4d3d54`)
3. resolver/service node (`0x4add34` / `0x413410`)

rather than only hardcoding the eventual interface pointer value.
