# launcher.exe -> client.dll loading sequence

## Source of truth

- startup driver around `launcher.exe:0x40b739..0x40b7d5`
- client export resolution/calls at `launcher.exe:0x40a4d0..0x40a6fb`
- helper loaders at `0x40a380`, `0x40a420`, `0x40a780`

## Canonical sequence

### 1. Build launcher object at `0x4d6304`
Call site:
- `0x40b740 -> 0x40a380`

What `0x40a380` does:
- allocates `0xb4` bytes
- constructs a launcher-owned object
- stores it at `0x4d6304`
- registers / hands it through the runtime interface pointer stored at `0x4d2c58`

Canonical object docs:
- `../startup_objects/0x4d6304_network_engine.md`
- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`

### 2. Perform additional launcher environment setup
Call site:
- `0x40b74d -> 0x402ec0`

This happens before any `client.dll` entry point is called.

Canonical note:
- `PRECLIENT_ENVIRONMENT_402EC0.md`

Current best interpretation:
- launcher thread / message / event environment setup that blocks until ready

### 3. Load `cres.dll`
Call site:
- `0x40b7b1 -> 0x40a780`

What `0x40a780` does:
- `LoadLibraryA("cres.dll")`
- stores the handle at `0x4d2c4c`

### 4. Load `client.dll`
Call site:
- `0x40b7bc -> 0x40a420`

What `0x40a420` does:
- `LoadLibraryA("client.dll")`
- stores the handle at `0x4d2c50`

### 5. Resolve client exports
Call site:
- `0x40b7c7 -> 0x40a4d0`

Resolved in order:
- `InitClientDLL`
- `RunClientDLL`
- `TermClientDLL`
- `ErrorClientDLL`

### 6. Call `InitClientDLL`
Still inside `0x40a4d0`:
- arguments prepared at `0x40a55c..0x40a5a4`
- exact mapping documented in `../../client.dll/InitClientDLL/README.md`

### 7. Call `RunClientDLL`
On the successful path inside `0x40a4d0`:
- `call [ebp-0x14]` invokes `RunClientDLL`

### 8. Cleanup after the client phase
After the main client path returns:
- `0x40b88a -> 0x40b360` tears down `0x4d6304` / `0x4d2c58` related state
- `0x40b88f -> 0x40a000` frees parsed-argument storage from `0x4d2c60`

## Hard boundary from static analysis

In this traced startup path, `launcher.exe`:
- loads modules,
- resolves exports,
- calls `InitClientDLL`,
- calls `RunClientDLL`,
- but does **not** directly call `client.dll!SetMasterDatabase()`.

## High-confidence conclusions

1. `cres.dll` is part of the original startup path and is passed to `InitClientDLL`.
2. The launcher does significant work before calling `InitClientDLL`.
3. A minimal host that only loads `client.dll` and passes mostly `NULL` arguments is not equivalent to the original launcher.
4. No client-memory injection is required by the original launcher path we are trying to reproduce.

## Current implication

The current custom launcher is still missing launcher-owned setup that happens before `RunClientDLL`. That is the primary line of investigation.
