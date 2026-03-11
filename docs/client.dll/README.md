# client.dll

Canonical client-side documentation lives in function/component folders.

## Primary functions/components

- `InitClientDLL/README.md`
  - Argument mapping and launcher-side prerequisites.

- `SetMasterDatabase/README.md`
  - Current understanding of the two `SetMasterDatabase` implementations.

- `RunClientDLL/README.md`
  - Current crash status and what it implies.

## Current high-confidence facts

1. `InitClientDLL` is not being called with a launcher-equivalent environment in the current reimplementation.
2. `RunClientDLL` currently crashes after successful `InitClientDLL` because launcher-owned setup is still missing.
3. Direct client-memory injection changed crash behavior, but it did not reproduce the original launcher architecture.
4. Static analysis currently indicates that `launcher.exe` does **not** directly call `client.dll!SetMasterDatabase()` in the traced startup path.

## Rule

If a note is primarily about a specific client export or internal component, add it under `docs/client.dll/<function-or-component>/`.
