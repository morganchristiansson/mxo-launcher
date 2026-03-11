# client.dll!InitClientDLL

## Source of truth

- original call site in `launcher.exe` at `0x40a55c..0x40a5a4`
- surrounding startup path in `launcher.exe` at `0x40b740..0x40b7d5`

## What matters

The original launcher does **not** call `InitClientDLL` with mostly `NULL` placeholders.
It constructs launcher-owned state first, then passes an 8-argument frame.

## Correct argument mapping

At `launcher.exe:0x40a55c..0x40a5a4` the call sequence is:

```asm
mov eax, [ebx+0xac]
mov ecx, [ebx+0xa8]
xor edx, edx
mov dl, [0x4d2c69]
and eax, 0x00ffffff
shl ecx, 0x18
or  eax, ecx
mov ecx, [0x4d6304]
push edx        ; arg8
mov edx, [0x4d2c4c]
push eax        ; arg7
mov eax, [0x4d2c58]
push eax        ; arg6
push ecx        ; arg5
mov ecx, [0x4d2c60]
push edx        ; arg4
mov edx, [0x4d2c5c]
mov eax, [0x4d2c50]
push eax        ; arg3
push ecx        ; arg2
push edx        ; arg1
call edi        ; InitClientDLL
add esp, 0x20
```

## Reconstructed signature

```c
int InitClientDLL(
    uint32_t filteredArgCount,         // from 0x4d2c5c
    char**   filteredArgv,             // from 0x4d2c60
    HMODULE  hClientDll,               // from 0x4d2c50
    HMODULE  hCresDll,                 // from 0x4d2c4c
    void*    launcherNetworkObject,    // from 0x4d6304
    void*    pILTLoginMediatorDefault, // from 0x4d2c58
    uint32_t packedArg7Selection,      // [this+0xa8]/[this+0xac]
    uint32_t flagByte                  // from 0x4d2c69
);
```

Names after arg4 are still provisional, but the sources and order are not.

## Known launcher-side prerequisites

Before the call, the original launcher has already:

1. parsed command-line state into `0x4d2c5c` / `0x4d2c60`
2. built a launcher-owned object and stored it at `0x4d6304`
3. loaded `cres.dll` into `0x4d2c4c`
4. loaded `client.dll` into `0x4d2c50`
5. resolved the runtime interface pointer at `0x4d2c58` (`ILTLoginMediator.Default`)
6. selected nopatch-related state reflected in `0x4d2c69`

Relevant launcher-side object docs:
- `../../launcher.exe/startup_objects/0x4d6304_network_engine.md`
- `../../launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../../launcher.exe/startup_objects/0x4d2c58_RESOLUTION_MECHANISM.md`
- `../../launcher.exe/startup_objects/0x4d3368_CLauncher.md`
- `ARG7_0x4D3410_0x4D3414.md`

## Validation result from a closer original-path experiment

A 2026-03-11 experiment that:
- loaded `cres.dll` before `client.dll`,
- used the real 8-argument frame shape,
- avoided any `client.dll` memory injection,
- but still left launcher-owned objects unresolved,

produced:

- `InitClientDLL` return value: **`-7`**
- no observed launcher-side `SetMasterDatabase` callback

A follow-up diagnostic run then forced `RunClientDLL` anyway and produced fresh dump:

- `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`

This strengthens the conclusion that the remaining blocker is missing launcher-owned setup, not just export resolution or DLL order.

See:
- `INCOMPLETE_ORIGINAL_PATH_EXPERIMENT.md`
- `../RunClientDLL/README.md`

## New high-value finding: arg6 gates the old `-7` failure path

Static and diagnostic evidence now strongly indicate that **arg6 (`ILTLoginMediator.Default`) is the key blocker behind the earlier `InitClientDLL = -7` result**.

### Static proof inside `client.dll!InitClientDLL`
At function entry:

```asm
620012af: mov ecx, [ebp+0x1c]   ; arg6
620012b2: mov eax, [ebp+0x18]   ; arg5
...
620012bd: mov [0x62b073e4], eax ; store arg5 only
620012c2: call 0x6229d2a0       ; register arg6 under "ILTLoginMediator.Default"
```

This shows:

- arg5 is merely stored at first,
- arg6 is consumed immediately by the client-side binder/registry path.

### Client-side binder proof
`client.dll` has its own slot binder for:

- `ILTLoginMediator.Default` -> `0x629df7f0`

from static init around `0x627c3bd0`.

Later, the old `-7` path checks that resolved object:

```asm
620033f3: mov ecx, [0x629df7f0]
620033f9: mov eax, [ecx]
620033ff: call [eax+0x10]
```

### What `-7` actually is
The old `-7` result comes from `0x62003d47`, reached after `0x62003380` fails.
That stage corresponds to:

- **"Failed to properly set up the network shell."**

So the previous `-7` was not a generic mystery code.
It was a concrete network-shell setup failure reached before arg7/arg8 matter.

### What arg7/arg8 do *not* explain
The arg7/arg8-dependent path begins later at `0x620015e2 -> 0x62170b00`.
That path is not reached until after the earlier network-shell setup succeeds.

So:

- arg7/arg8 are important,
- but they are **not** the reason for the original `-7` barrier.

## Diagnostic stub experiment result

A new opt-in experiment using:

- `MXO_STUB_LOGIN_MEDIATOR=1`

supplied a minimal arg6 stub and changed behavior substantially:

1. the old immediate `InitClientDLL = -7` path was bypassed,
2. the client reached deeper mediator-dependent methods (`+0x10`, `+0xd8`, `+0x38`),
3. and after extending the stub far enough, the process progressed into much deeper startup / rendering activity instead of dying at the old `-7` point.

This is strong evidence that:

- reconstructing `0x4d2c58` is the highest-priority path to move the launcher forward,
- while arg5 (`0x4d6304`) appears to become important later than the original `-7` barrier.

## Implication for the reimplementation

The following ad-hoc call is not launcher-equivalent:

```c
InitClientDLL(argc, argv, hClient, NULL, NULL, 0, 0, NULL);
```

The most important missing state to fix first is now:

- a valid `ILTLoginMediator.Default` object for arg6,

followed by:

- enough of arg7/arg8,
- and then fuller reconstruction of arg5 / pre-client environment state.

## Open questions

- Which exact concrete interface methods of `ILTLoginMediator.Default` are required by `client.dll` during early runtime?
- What exact concrete launcher state is sufficient for the `ILTLoginMediator.Default` slot to resolve non-NULL in our reimplementation?
- Which fields/methods of those objects are actually consumed by `client.dll` during early runtime?
