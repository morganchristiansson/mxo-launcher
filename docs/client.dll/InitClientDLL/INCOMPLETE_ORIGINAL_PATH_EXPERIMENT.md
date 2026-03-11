# Incomplete original-path experiment

## Date
- 2026-03-11

## Purpose

Test a launcher path that is **closer to the original launcher.exe** than the earlier NULL-heavy harnesses, but still intentionally incomplete.

This experiment:
- preloads the normal support DLLs
- loads `cres.dll` before `client.dll`
- resolves the real client exports
- calls `InitClientDLL` with the correct **8-argument shape**
- does **not** inject into `client.dll`
- leaves these launcher-owned prerequisites unresolved:
  - `0x4d6304`
  - `0x4d2c58`
  - original `0x402ec0` environment setup
  - packed version / flag state

## Observed result

`InitClientDLL` returned:

```text
-7
```

and the launcher-side export `SetMasterDatabase` was **not** observed.

## Why this matters

Earlier ad-hoc harnesses could get `InitClientDLL` to return `0` while still being architecturally wrong.
That made the later `RunClientDLL` crash look more mysterious than it really was.

This newer experiment is more faithful to the original launcher path, and it shows a clearer signal:

- once `cres.dll` is part of the call path,
- and we stop patching client memory,
- missing launcher-owned objects cause `InitClientDLL` itself to reject the startup frame.

## Current interpretation

This is good evidence that the real blocker is still launcher-owned setup, especially:

1. the runtime interface pointer at `0x4d2c58`
2. the heap object at `0x4d6304`
3. the pre-client environment setup around `0x402ec0`

## Related docs

- `README.md`
- `../../launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../../launcher.exe/startup_objects/0x4d6304_network_engine.md`
