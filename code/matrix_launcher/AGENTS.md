# Matrix Online Launcher - Agent Development Notes

For generic workflow and documentation policy, see:
- `../../AGENTS.md`

## Purpose

Keep this file short, current, and operational.
Do not use it as a long-running research journal.

Prefer:
- inline/header source comments for code-owned structure and local TODOs
- canonical docs under `../../docs/` for evidence and cross-component conclusions

## Current goal

Reimplement the original Matrix Online `launcher.exe` startup/runtime path as faithfully as possible.

Source of truth:
- `~/MxO_7.6005/launcher.exe`

## Current active status

### Closed enough on the active existing-character path
- preload support DLLs
- load `cres.dll` before `client.dll`
- preserve the original 8-argument `InitClientDLL` call shape
- strip launcher-only auth argv before `InitClientDLL`
- treat positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns as success
- launcher-owned auth progresses through `AS_GetPublicKeyReply`, `AS_AuthChallenge`, and `AS_AuthReply`
- launcher-owned margin bootstrap completes on the active runtime path
- state8 sends encrypted raw `0x0f` / `MS_LoadCharacterRequest`
- state8 receives decrypted raw `0x10` and hands off into helper9/state9
- late-login/state9 now reaches and completes the old submit blocker:
  - `0x41de40`
  - state9 raw `0x11` success
  - `0x41b420`
  - switch to state `0x0c`
  - event `0x18`
- late arg6 / observer bridge is now source-owned enough for the first game-entry pass:
  - arg6 `+0x170` observer registration
  - arg6 `+0x174` observer unregistration
  - arg6 `+0x178` status `+0x80` getter
  - event-`0x18` observer callback path now gets past arg6 `+0x10c`
  - event-`0x18` observer callback currently tolerates an empty arg6 `+0x118` late-entry list scaffold
- the replacement launcher has now entered game on the active existing-character path

### Natural-original boundary already proven later than that
- natural original reaches:
  - `0x43f930`
  - `0x439780`
  - `0x41de40`
  - `0x43c180`
  - `0x41b450(0x0c)`
  - `0x41cfb0(0x18)`
  - later `0x438df0`
  - then `0x41cfb0(0x0f)`
  - then entry into game

### Current blocker
- the old post-state9 / state-`0x0c` continuation blocker is no longer the active one
- current open work is the later late-runtime / in-game transition fidelity behind the now-live
  arg6 observer bridge
- do **not** reopen old auth/bootstrap or helper11/create-character history unless new evidence
  forces a branch change

## First-read context for the next session

### Canonical docs
- `../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
- `../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- `../../docs/launcher.exe/auth/STATUS.md`
- `../../docs/launcher.exe/VTABLES/0x004b517c.md`
- `../../docs/launcher.exe/VTABLES/0x004b5230.md`

## Keep separate from the next post-state9 session unless needed

These still matter globally, but they are not first-read context for the next late-login pass:
- helper10/helper11 create-character branch
- broad arg5/queue work
- broad startup/world-selection archaeology
- generic binder/registration bringup history

If one of those becomes active again, read the focused docs/source for that area instead of
re-expanding this file.

## Current implementation priorities

1. Keep auth launcher-owned, not client-owned
2. Keep work in lockstep across source, Ghidra names/types, runtime evidence, and canonical docs

## Documentation / ownership rules

### Prefer source ownership for recovered code structure

Use inline comments like:
- `anchor: launcher.exe:0x...`
- `UNANCHORED: ...`

### Prefer canonical docs for evidence and cross-component conclusions
Use `../../docs/` for:
- experiment conditions and outcomes
- crash signatures
- xref-backed conclusions
- recovered vtable surfaces

Prefer updating an existing canonical doc over adding new overlapping notes.

## Build / run

Build:
```
make -j6
```

Build and run:
```
make -j6 run
```

## Ghidra

Decompile function with Ghidra:
```
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

Read GHIDRA.md for more
