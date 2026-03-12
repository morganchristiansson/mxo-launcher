# launcher.exe

Canonical launcher-side documentation lives in component folders.

## Primary components

- `client_dll_loading/LOADING_SEQUENCE.md`
  - Startup path that loads `cres.dll`, `client.dll`, resolves exports, and calls into `client.dll`.

- `nopatch/README.md`
  - Static analysis of the original `-nopatch` branch.

- `startup_objects/README.md`
  - Canonical docs for launcher-owned globals and heap objects used before `InitClientDLL`.

- `SetMasterDatabase/`
  - Launcher-side `SetMasterDatabase` export and related notes.

## Current high-confidence facts

1. The original launcher loads **`cres.dll` before `client.dll` initialization**.
2. The original launcher passes **8 nontrivial arguments** to `client.dll!InitClientDLL`.
3. The launcher's **nopatch path still performs launcher-side setup** before calling `InitClientDLL`.
4. **launcher.exe does not call `client.dll!SetMasterDatabase()` in the traced startup path**.
5. The current custom launcher crashes are more consistent with **missing launcher-owned state** than with a need to patch `client.dll` memory.
6. For current same-machine runtime validation notes, keep the canonical record in:
   - `client_dll_loading/LOADING_SEQUENCE.md`

## Active focus

Reproduce the launcher-owned environment closely enough that `client.dll!RunClientDLL` can run without any diagnostic memory seeding.
