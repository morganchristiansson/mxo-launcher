# MxO Codebase - Agent Guide

## Core Workflow

### 1. reverse engineering of launcher.exe is the source of truth

Our structure, object shapes and implementations must be faithful to static-RE of launcher.exe

Static analysis is not separate from implementation work.
- inspect original behavior
- implement the closest faithful step
- run experiment
- document what changed
- prune outdated claims

### 2. Implementation and documentation should move together
Do not treat documentation as a separate afterthought.
When experiments, disassembly, or runtime traces change our understanding, update the canonical docs as part of the same work.

### 2. Prefer canonical docs over duplication
- Keep knowledge consolidated under:
  `docs/<binary>/<component>/`
- Prefer updating an existing canonical doc rather than creating a new overlapping note.
- If a doc becomes stale or redundant, prune or merge it.
- If doc grows large (2000 lines), split it.
- when an entry becomes resolved or stops steering current work, prune it instead of appending more history

## Practical Rules

- keep project source/build flow inside the project directory
- deploy built binaries into the game/runtime directory only for execution
- treat diagnostic-only hacks as diagnostics, not architecture
- avoid stale success claims after the project direction changes
- keep `AGENTS.md` concise and current; do not let it become a long-running progress journal
- prefer owning recovered code structure, local TODOs, and active implementation notes in inline/header source comments
- prefer canonical `docs/<binary>/<component>/` docs for experiment evidence, crash references, and cross-component conclusions
- when finishing one area of work and the next task likely moves into a different area
  - Suggest cleanup and pruing of code that is no longer needed and can be cleaned up
  - finish with a self-contained prompt to start next session; include current state, source-of-truth docs/files, exact next target, and any important context, commands or env flags
- avoid duplicate documentation; merge/prune overlapping docs and keep each topic in the smallest obvious canonical home instead of letting one large doc sprawl across many components
