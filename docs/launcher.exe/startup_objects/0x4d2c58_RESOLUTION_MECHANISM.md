# `0x4d2c58` resolution mechanism

## Summary

`0x4d2c58` is not constructed directly as a concrete object.
It is a **pointer slot** that gets populated through a launcher-side interface registration and resolution system.

For the `ILTLoginMediator.Default` case, the path is:

1. static initializer registers a wrapper object bound to the string `"ILTLoginMediator.Default"`
2. that wrapper is inserted into launcher-global registry state rooted at `0x4d3d54`
3. the registry fans registered wrappers out through resolver/service objects hanging off `0x4d3d54+0x18`
4. resolver/service nodes process the wrapper through the registry
5. the wrapper vtable writes the resolved interface pointer into the slot at `0x4d2c58`

## Canonical objects involved

- pointer slot: `0x4d2c58`
- wrapper object: `0x4d3218`
- wrapper vtable: `0x4aae04`
- global registry object pointer: `0x4d3d54`
- registry refcount: `0x4d3d58`

See also:
- `0x4d3d54_INTERFACE_REGISTRY.md`

## 1. Static registration of the wrapper

Initializer at `0x494ab0`:

```asm
push 0x1
push 0x4d2c58
push 0x4ab34c      ; "ILTLoginMediator.Default"
mov  ecx, 0x4d3218
call 0x4030d0
```

This builds a wrapper object whose job is to acquire an interface by name and store the resolved pointer into `0x4d2c58`.

## 2. Wrapper object layout

Ctor `0x4030d0` calls base ctor `0x4143c0`, then sets wrapper-specific state:

- `this+0x00` = vtable `0x4aae04`
- `this+0x04` = interface name pointer (`"ILTLoginMediator.Default"`)
- `this+0x08` = small mode / tag value (`1` in this registration)
- `this+0x0c` = output slot pointer (`0x4d2c58`)

So the wrapper is effectively:

```c
struct InterfaceSlotBinder {
    void*    vtbl;          // 0x00 = 0x4aae04
    char*    interfaceName; // 0x04
    uint32_t mode;          // 0x08
    void**   outSlot;       // 0x0c -> 0x4d2c58
};
```

## 3. Wrapper vtable behavior

The first entries of vtable `0x4aae04` resolve to:

- `0x4031a0` : clear `*outSlot = 0`
- `0x4031b0` : write `*outSlot = argument`
- `0x4031c0` : return `*outSlot`

That is the key proof that the wrapper directly manages the pointer slot at `0x4d2c58`.

## 4. Registry creation

Global helper `0x413e60` lazily creates the registry object at `0x4d3d54`.

The object is `0x24` bytes and is initialized with zeros at offsets:

- `+0x00`
- `+0x0c`
- `+0x18`

That strongly indicates **three intrusive-list heads**, each 12 bytes wide.

`0x4d3d58` is a separate reference count for the registry object.

## 5. Wrapper registration into the registry

`0x414030(wrapper)`:

- ensures the registry exists via `0x4d3d54`
- allocates a small `0x14` node object if needed
- inserts that node into the registry list at `0x4d3d54+0x18`
- then calls a service method at vtable offset `+0x0c` with the wrapper pointer

This is the core "register this interface binder" path.

## 6. Active resolver/service notifications

Two helpers dispatch wrapper registration state into the active service at `0x4d3d54+0x18`:

### `0x4132a0`
- calls active service vtable `+0x1c`
- then checks vtable `+0x24`
- if ready, tail-calls vtable `+0x04`

### `0x4132f0`
- calls active service vtable `+0x18`
- then checks vtable `+0x24`
- if ready, tail-calls vtable `+0x04`

This appears to be the add/remove notification path for registered interface binders.

## 7. Fan-out / materialization path

Function `0x414140` iterates over wrapper-owned arrays/lists and pushes objects through resolver services drawn from the registry.

Within that process it allocates per-service nodes as needed and invokes service methods at:

- `+0x08`
- `+0x0c`
- `+0x10`

This is the strongest current evidence that the registry is responsible for **materializing concrete interface pointers** into the wrapper-managed output slot.

## Why this matters

When later startup code does:

```asm
mov ecx, [0x4d2c58]
mov edx, [ecx]
call [edx+...]
```

it is using a pointer produced by this registration/resolution system.
It is not using a handcrafted object embedded directly in launcher startup code.

There is also a matching **client-side** consequence:

- `client.dll!InitClientDLL` immediately registers arg6 under `"ILTLoginMediator.Default"`
- the client has its own binder-managed slot at `0x629df7f0`
- later init stages call methods on that resolved object

So if launcher arg6 is NULL or structurally wrong, the client-side resolved login mediator also remains wrong.

## Minimal state currently believed necessary before `0x4d2c58` can be non-NULL

Static analysis now supports this minimum set of launcher-owned preconditions:

1. wrapper/binder object `0x4d3218` must exist with:
   - interface name = `"ILTLoginMediator.Default"`
   - out-slot = `0x4d2c58`
2. registry object `0x4d3d54` must exist
3. resolver/service list at `0x4d3d54+0x18` must contain at least one service node
4. the add/remove notification path (`0x4132a0` / `0x4132f0`) must observe that the service is ready via vtable `+0x24`
5. the ready service must then reach the callback path that ultimately causes the wrapper vtable to write the resolved pointer into `0x4d2c58`

So a launcher that only constructs the wrapper but never materializes a ready service node cannot produce the final interface pointer.

## Reimplementation implication

To follow the original launcher path, we should aim to reproduce:

- the binder/wrapper registration model,
- the registry object,
- and enough resolver/service-node behavior for the slot write to actually occur,

not just the final pointer value.

The safest next step is to reconstruct enough of:

- wrapper object `0x4d3218`
- registry object `0x4d3d54`
- resolver/service node `0x4add34`
- registration helper `0x414030`

so that `0x4d2c58` is obtained the same way the original launcher obtains it.
