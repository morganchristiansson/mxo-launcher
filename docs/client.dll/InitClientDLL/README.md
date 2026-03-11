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
    uint32_t packedVersionInfo,        // [this+0xa8]/[this+0xac]
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

## Validation result from a closer original-path experiment

A 2026-03-11 experiment that:
- loaded `cres.dll` before `client.dll`,
- used the real 8-argument frame shape,
- avoided any `client.dll` memory injection,
- but still left launcher-owned objects unresolved,

produced:

- `InitClientDLL` return value: **`-7`**
- no observed launcher-side `SetMasterDatabase` callback

See:
- `INCOMPLETE_ORIGINAL_PATH_EXPERIMENT.md`

## Implication for the reimplementation

The following ad-hoc call is not launcher-equivalent:

```c
InitClientDLL(argc, argv, hClient, NULL, NULL, 0, 0, NULL);
```

That mismatch is currently the most plausible explanation for why `InitClientDLL` returns success but `RunClientDLL` later crashes.

## Open questions

- Which exact concrete interface methods of `ILTLoginMediator.Default` are required by `client.dll` during early runtime?
- What exact concrete class name sits behind the `0x4d6304` vtable (`0x4b2768`)?
- Which fields/methods of those objects are actually consumed by `client.dll` during early runtime?
