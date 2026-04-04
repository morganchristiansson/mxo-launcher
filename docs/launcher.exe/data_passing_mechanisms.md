# launcher.exe data passing mechanisms

This file is now a short pointer to the canonical function-level docs.

## Canonical locations

- `../client.dll/InitClientDLL/README.md`
- `client_dll_loading/LOADING_SEQUENCE.md`
- `nopatch/README.md`

## Short summary

The original launcher passes an 8-argument frame into `client.dll!InitClientDLL` built from launcher-owned globals and loaded module handles, including:

- parsed argument count / argv-like storage (`g_LauncherFilteredArgCount`, `g_LauncherFilteredArgv`)
- `client.dll` handle (`0x4d2c50`)
- `cres.dll` handle (`0x4d2c4c`)
- launcher-owned objects (`0x4d6304`, `0x4d2c58`)
- packed launcher selection / saved-world state from `CLauncher+0xa8/+0xac`
- nopatch-flow flag byte (`g_LauncherNoPatchFlowFlagByte` / `0x4d2c69`)

The older sprawling version of this document was archived to `../slopdocs/launcher.exe/data_passing_mechanisms.md`.
