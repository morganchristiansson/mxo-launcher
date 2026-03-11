# launcher.exe!SetMasterDatabase

## Source of truth

- export address: `0x4143f0`
- constructor/helper nearby: `0x4143c0`

## What is currently known

- `launcher.exe` exports `SetMasterDatabase`.
- The function operates on launcher-owned structure/linkage state.
- In the launcher startup path currently traced, `launcher.exe` does **not** directly call `client.dll!SetMasterDatabase()`.

## Why this folder exists

This folder is for the launcher-side export specifically.
For the cross-binary naming confusion and current overall conclusion, also see:

- `../../client.dll/SetMasterDatabase/README.md`
- `../client_dll_loading/LOADING_SEQUENCE.md`

## Current rule

Do not collapse the launcher export and the client export into one behavior description just because they share the same symbol name.
