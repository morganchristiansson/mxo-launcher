# launcher global `0x4d2c58`

## High-confidence identity

`0x4d2c58` is **not** the storage for a wrapper object.
It is a **runtime interface pointer slot** populated through launcher-side registration.

The strongest static identifier currently attached to this slot is:

- string argument: `"ILTLoginMediator.Default"` at `0x4ab34c`

So the current canonical name is:

- **`ILTLoginMediator.Default` runtime interface pointer**

## Source of truth

### Dynamic initializer
- `launcher.exe:0x494ab0`

```asm
push 0x1
push 0x4d2c58
push 0x4ab34c      ; "ILTLoginMediator.Default"
mov  ecx, 0x4d3218
call 0x4030d0
```

### Constructor path
- `0x4030d0`
- base ctor `0x4143c0`
- registration helper `0x414030`

## What the constructor proves

Inside `0x4030d0`:

- `[this+4]  = 0x4ab34c` → interface name string
- `[this+8]  = 1`
- `[this+0xc] = 0x4d2c58` → output slot address
- `*(uint32_t*)0x4d2c58 = 0` → zero the runtime pointer slot before registration
- then `0x414030(this)` registers the wrapper with launcher-global registry state rooted at `0x4d3d54`

So:

- object at `0x4d3218` = wrapper / binder / registrar
- global at `0x4d2c58` = resolved runtime interface pointer used later by startup

## Why this matters

When startup later does:

```asm
mov ecx, [0x4d2c58]
mov edx, [ecx]
call [edx+...]
```

it is using the **resolved interface pointer value**, not a wrapper object.

That means our reimplementation should not model `0x4d2c58` as a plain homemade struct.
It needs the original launcher-side acquisition / registration behavior.

## Verified uses in the original startup path

### Nopatch branch configures it
At `0x409a73` and `0x409a98` the launcher:

- parses `"0.1"` via `0x417440`
- loads `ecx = [0x4d2c58]`
- calls vtable methods at offsets:
  - `+0x1c`
  - `+0x24`

So the nopatch path actively configures the interface before client startup.

### Startup hands `0x4d6304` into it
At `0x40a3e9..0x40a3fe`:

```asm
mov ecx, [0x4d2c58]
mov [0x4d6304], eax
mov edx, [ecx]
push eax            ; eax = object just built for 0x4d6304
call [edx+0x08]
```

So one of the interface methods consumes the newly built launcher object from `0x4d6304`.

### Client startup receives it as InitClientDLL arg6
At `0x40a587`:

```asm
mov eax, [0x4d2c58]
push eax            ; arg6 to InitClientDLL
```

### Teardown also uses it
At `0x40b360..0x40b409` the launcher uses the same interface during cleanup, including vtable offsets:

- `+0x164`
- `+0x16c`
- `+0x0c`

## High-confidence conclusions

1. `0x4d2c58` is a **runtime interface pointer slot**.
2. Its registration string is **`ILTLoginMediator.Default`**.
3. The original nopatch path configures this interface before `InitClientDLL`.
4. The launcher passes the resolved pointer value to `InitClientDLL`.
5. A replacement launcher that skips this acquisition/registration path is not equivalent to the original.

## Early method surface observed so far

### Launcher-observed offsets on `0x4d2c58`

From original `launcher.exe` startup/teardown:

| Offset | Current use | Confidence |
|---:|---|---|
| `+0x08` | launcher hands newly built `0x4d6304` object into mediator | high |
| `+0x0c` | teardown / cleanup path | medium |
| `+0x1c` | nopatch path passes parsed `"0.1"`-derived value | medium |
| `+0x24` | nopatch path passes client-version-derived value | medium |
| `+0x164` | teardown conditional check | medium |
| `+0x16c` | teardown conditional check | medium |

### Client-observed offsets on arg6-resolved `ILTLoginMediator.Default`

From `client.dll` static init and early `InitClientDLL` analysis:

| Offset | Earliest observed role | Confidence |
|---:|---|---|
| `+0x10` | readiness / availability gate; this is part of the old `-7` barrier | high |
| `+0x2c` | additional runtime readiness gate before arg5-related runtime work | medium |
| `+0x38` | returns C-string used by client formatting path | high |
| `+0x58` | string-producing helper in early init logging/config path | medium |
| `+0x5c` | chained string-producing helper | medium |
| `+0x60` | chained string-producing helper | medium |
| `+0xd8` | arg7 high-byte / world-selection gate in `0x62170b00` | high |
| `+0xdc` | maps arg7-derived selection to string/resource in deeper init | medium |
| `+0xec` | consumes assembled selection/config structure in deeper init | medium |
| `+0x124` | accepts `INetShell/INetMgr/ILTDistrObjExecutive` triple in deeper init | medium |
| `+0x170` | consumes client startup context object in deeper init | medium |

Many later runtime paths use even more offsets (`+0xf4`, `+0x10c`, `+0x118`, `+0x120`, `+0x148`, `+0x154`, `+0x158`, `+0x160`, `+0x174`, `+0x178`, etc.), which is strong evidence that the real interface is broad and not a tiny ad-hoc object.

## What the stub experiments proved

The opt-in mediator stub in the custom launcher showed this progression:

1. with arg6 = `NULL`, `InitClientDLL` hit the old `-7` path,
2. after supplying minimal arg6 methods (`+0x00`, `+0x10`, then `+0xd8`, `+0x38`),
3. startup moved past the old immediate `-7` barrier and into deeper post-network-shell / rendering startup.

This is the strongest current evidence that:

- arg6 is the highest-priority missing launcher-owned state,
- and the old `-7` barrier is more directly about `ILTLoginMediator.Default` than about arg5 or `0x402ec0`.

## Decision for reimplementation direction

The stub should be kept only as a **differential probe**.
It is useful because it reveals which vtable slots are touched and how far startup advances.

However, the growing method surface strongly suggests that the real fix should prioritize reconstructing the original launcher-side acquisition path for `0x4d2c58`, namely:

- wrapper/binder object,
- registry object,
- resolver/service node path,
- and materialization of the real interface pointer,

rather than continuing to grow a fake mediator into a large hand-emulated interface.

## Current implication for reimplementation

The current custom launcher should stop treating arg6 as a vague `master database` or arbitrary placeholder.
It is much more likely a launcher-resolved `ILTLoginMediator.Default`-style interface pointer that must already be live by the time `InitClientDLL` is called.

See also:
- `0x4d2c58_RESOLUTION_MECHANISM.md`
