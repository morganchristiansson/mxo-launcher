# client.dll!RunClientDLL

## Current status

There are now **two distinct states** for the project-local launcher:

1. **More faithful default scaffold**
   - loads `cres.dll` before `client.dll`
   - uses the correct 8-argument `InitClientDLL` frame shape
   - currently gets `InitClientDLL = -7`
   - stops safely by default

2. **Diagnostic forced-runtime path**
   - same launcher as above
   - but with `MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE=1`
   - continues into `RunClientDLL` even after `InitClientDLL` fails
   - crashes at `client.dll+0x3b3573`

So the current crash is best understood as a **deliberate diagnostic continuation after failed initialization**, not as an original-equivalent startup path.

## Fresh canonical crash reference

Fresh dump from the current scaffold:

- dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`
- binary: `launcher_proper.exe`
- `InitClientDLL` result before the crash path: **`-7`**
- `RunClientDLL` was entered only because of the explicit diagnostic override

Crash site:

- `EIP = 0x623b3573`
- `client.dll+0x3b3573`

Observed registers:

- `EBX = 0x62999968`
- `ECX = 0x00000000`

Faulting instruction:

```asm
623b3570: mov ecx, [ebx+0x04]
623b3573: mov eax, [ecx]
```

## What the fresh dump proves

### 1. `cres.dll` is now definitely present on the crashing current path
The fresh dump shows:

- `cres` loaded at `0x10000000`
- `client` loaded at `0x62000000`

So this crash is **not** explained by "we forgot to load `cres.dll`" on the current project-local launcher path.

### 2. The remaining mismatch is launcher-owned startup state
The current launcher log immediately before the crash shows:

- `arg5 launcherNetworkObject   = 0`
- `arg6 ILTLoginMediatorDefault = 0`
- `arg7 packedArg7Selection     = 0`
- `arg8 flagByte                = 0`
- `SetMasterDatabase` callback not observed
- `InitClientDLL returned: -7`

That is the strongest current explanation for the later null dereference.

### 3. This differs from original `launcher.exe`
The original launcher path in `../../launcher.exe` is the source of truth.
From static analysis, original `launcher.exe`:

- builds launcher object `0x4d6304`
- performs pre-client environment setup at `0x402ec0`
- loads `cres.dll`
- loads `client.dll`
- resolves exports
- calls `InitClientDLL` with non-placeholder launcher-owned state
- only then calls `RunClientDLL`

Our diagnostic crash path differs in a critical way:

- it calls `RunClientDLL` **after `InitClientDLL` already reported failure**

That is not original behavior.

## Relationship to older dumps

Older dumps (`68`-`71`) from less-faithful launcher variants also crashed at:

- `client.dll+0x3b3573`

and the injection experiment (`72`) moved the crash to:

- `EIP = 0x00000000`

The fresh dump (`73`) is more useful than those older dumps because it comes from the current project-local launcher and confirms:

- `cres.dll` is present,
- the 8-argument `InitClientDLL` frame shape is used,
- and the remaining problem is still missing launcher-owned setup.

## High-confidence interpretation

The crash is evidence that `client.dll` runtime reaches a state-management path that expects additional objects to have been established earlier.
The best current explanation remains:

- missing launcher-side prerequisites,
- especially `0x4d6304`, `0x4d2c58`, arg7 selection state, and pre-client setup around `0x402ec0`.

It is **not** evidence that the final solution requires client-memory injection.

## Related docs

- `CRASH_623B3573.md`
- `../InitClientDLL/README.md`
- `../InitClientDLL/INCOMPLETE_ORIGINAL_PATH_EXPERIMENT.md`
- `../../launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../launcher.exe/startup_objects/0x4d6304_network_engine.md`
- `../../launcher.exe/startup_objects/0x4d2c58_RESOLUTION_MECHANISM.md`
