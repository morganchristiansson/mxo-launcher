# launcher global `0x4d3d54`

## High-confidence identity

`0x4d3d54` is a lazily created **interface registry / binder registry** object used to resolve named launcher interfaces into pointer slots such as `0x4d2c58`.

Associated global:

- `0x4d3d58` = registry refcount

## Source of truth

### Creation helper
- `0x413e60`

### Refcounted acquire/release paths
- `0x413ed0`
- `0x414450`
- `0x4145c0`

### Registration / fan-out helpers
- `0x414030`
- `0x414140`
- `0x4132a0`
- `0x4132f0`

## Object layout evidence

`0x413e60` allocates `0x24` bytes and zeroes fields at:

- `+0x00`
- `+0x0c`
- `+0x18`

That strongly suggests the object contains **three intrusive-list heads**, each 12 bytes wide.

A practical model is:

```c
struct RegistryListHead {
    void* owner;   // +0x00
    void* prev;    // +0x04
    void* next;    // +0x08
};

struct InterfaceRegistry {
    RegistryListHead list0;   // +0x00
    RegistryListHead list1;   // +0x0c
    RegistryListHead list2;   // +0x18
};
```

Exact semantic names for the three lists are still provisional.

## What the registry does

### 1. Exists before wrapper registration can work
Functions like `0x414030` and `0x414000` immediately check `0x4d3d54` and create it on demand if needed.

### 2. Holds active resolver/service nodes
The list at `+0x18` is especially important.

Multiple helpers do:

```asm
mov eax, [0x4d3d54]
lea esi, [eax+0x18]
```

then ensure a node exists there and call through the node’s vtable.

### 3. Fans wrapper objects into services
`0x414030(wrapper)` inserts a small node into the `+0x18` list and calls service vtable `+0x0c` with the wrapper.

The current best model for the concrete service node class is documented in:
- `0x4d3d54_SERVICE_NODE_4ADD34.md`

`0x414140(...)` iterates wrapper-owned lists/arrays and forwards objects through service methods at:

- `+0x08`
- `+0x0c`
- `+0x10`

So the registry is not passive storage; it actively mediates wrapper-to-service resolution.

## Active notification helpers

### `0x4132a0`
On add/update-style paths, if a service exists at `0x4d3d54+0x18`, the launcher:

- calls service vtable `+0x1c`
- checks readiness via `+0x24`
- if ready, tail-calls `+0x04`

### `0x4132f0`
On remove/teardown-style paths, the launcher:

- calls service vtable `+0x18`
- checks readiness via `+0x24`
- if ready, tail-calls `+0x04`

These functions are central to the lifecycle of named interface binders.

## Reused mechanism

The binder/registry machinery is not unique to `0x4d2c58`.
The same wrapper class (`0x4aae04`) is used elsewhere in the binary with different output slots, for example:

- object at `0x4d4d9c` with out-slot `0x4d4d60`
- object at `0x4d6414` with out-slot `0x4d6330`

Both are constructed with the same interface string:

- `"ILTLoginMediator.Default"`

This is strong evidence that the launcher has a generic named-interface binding system, not a one-off hardcoded path.

## Why this matters

For the reimplementation, `0x4d2c58` is not something we should ideally hardcode as a raw object pointer.
It comes from this registry-mediated binding system.

So the most original-faithful direction is:

1. reconstruct the registry object model,
2. reconstruct the wrapper/binder model,
3. then let those produce `0x4d2c58` naturally.
