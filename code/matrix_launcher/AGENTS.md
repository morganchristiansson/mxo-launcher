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
- launcher-owned margin bootstrap completes on the deliberate runtime path
- state8 sends encrypted raw `0x0f` / `MS_LoadCharacterRequest`
- state8 receives decrypted raw `0x10` and hands off into helper9/state9
- late-login/state9 now reaches and completes the old submit blocker:
  - `0x41de40`
  - state9 raw `0x11` success
  - `0x41b420`
  - switch to state `0x0c`
  - event `0x18`

### Natural-original boundary already proven later than that
- natural original reaches:
  - `0x43f930`
  - `0x439780`
  - `0x41de40`
  - `0x43c180`
  - `0x41b450(0x0c)`
  - `0x41cfb0(0x18)`
  - later `0x41cfb0(0x0f)`
  - then entry into game

### Current blocker
- the replacement launcher is now blocked on the later post-state9 / state-`0x0c` continuation
- do **not** reopen old auth/bootstrap or helper11/create-character history unless new evidence
  forces a branch change

## First-read context for the next session

### Canonical docs
- `../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`
- `../../docs/launcher.exe/startup_objects/0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- `../../docs/launcher.exe/auth/STATUS.md`
- `../../docs/launcher.exe/VTABLES/0x004b517c.md`
- `../../docs/launcher.exe/VTABLES/0x004b5230.md`

### Active source files
- `matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_state9_submit_scaffold.h`
- `src/launcher_mediator_state9_abi.cpp`
- `src/launcher_mediator_abi.cpp`

Open the broader `matrixstaging/game/src/libltclientlogin/loginmediator.cpp` only when the late
post-state9/event work forces a step back into shared mediator transport or auth/bootstrap code.

## Keep separate from the next post-state9 session unless needed

These still matter globally, but they are not first-read context for the next late-login pass:
- helper10/helper11 create-character branch
- broad arg5/queue work
- broad startup/world-selection archaeology
- generic binder/registration bringup history

If one of those becomes active again, read the focused docs/source for that area instead of
re-expanding this file.

## Current implementation priorities

1. Tighten the post-state9 / state-`0x0c` continuation after the now-live state9 success tail
2. Keep `0x41b420`, `0x41b450`, and `0x41cfb0` treated as the immediate bridge, not the old
   immediate-final-leaf theory
3. Treat state12 as the strongest current state-identity lead, but not yet a proven immediate next
   execution leaf
4. Keep auth launcher-owned, not client-owned
5. Keep work in lockstep across source, Ghidra names/types, and canonical docs

## Documentation / ownership rules

### Prefer source ownership for recovered code structure
Own recovered active-path structure in:
- `matrixstaging/game/src/libltclientlogin/`
- `matrixstaging/runtime/src/liblttcp/`
- `matrixstaging/runtime/src/libltmessaging/`
- `src/launcher_*_abi.cpp`

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

### Logging
- use `spdlog`

## Build / run

Build:
```bash
make
```

Safe run:
```bash
make run
```

Current deliberate binder runtime path:
```bash
cd /home/morgan/mxo/code/matrix_launcher
MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Reality make run_binder_both_runtime
```

## Immediate next task

Use natural-original proof to push past the current replacement boundary after:
- state9 raw `0x11` success
- `0x41b420`
- `0x41b450(0x0c)`
- `0x41cfb0(0x18)`

Focus on the concrete post-state9 / state-`0x0c` continuation and the listener/event consumers
behind it, not on old auth/bootstrap history.
