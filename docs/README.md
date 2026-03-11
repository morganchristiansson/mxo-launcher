# MxO Documentation Index

The source of truth for all reverse-engineering notes is:

- `../launcher.exe`
- `../client.dll`

Documentation should mirror that layout as closely as possible:

- `docs/launcher.exe/<component>/...`
- `docs/client.dll/<function-or-component>/...`

## Canonical entry points

### launcher.exe
- `launcher.exe/README.md`
- `launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `launcher.exe/nopatch/README.md`

### client.dll
- `client.dll/README.md`
- `client.dll/InitClientDLL/README.md`
- `client.dll/SetMasterDatabase/README.md`
- `client.dll/RunClientDLL/README.md`

## Current project position

- Reimplement the original launcher's **nopatch** path.
- Do **not** treat direct writes into `client.dll` globals as architecture.
- If a note does not clearly derive from `launcher.exe` or `client.dll`, it should not be promoted to canonical documentation.

## Documentation rule

When a topic belongs to a concrete binary function or component, prefer adding a folder under that binary instead of writing another top-level summary.
