# MxO Codebase - Agent Guide

## Purpose

This file is the **generic workflow guide** for code under `code/`.
Keep it reusable.
Project-specific state belongs in each project's own `AGENTS.md`.

For the active launcher project, see:
- `matrix_launcher/AGENTS.md`

## Core Workflow

### 1. Implementation and documentation should move together
Do not treat documentation as a separate afterthought.
When experiments, disassembly, or runtime traces change our understanding, update the canonical docs as part of the same work.

### 2. Documentation must follow evidence
When documenting reverse-engineered behavior, prefer this order of confidence:

1. static disassembly of original binaries
2. runtime evidence from controlled experiments / dumps / logs
3. provisional hypotheses clearly labeled as such

Do not present temporary assumptions as settled fact.

### 3. Prefer canonical docs over duplication
Keep knowledge consolidated under:
- `../docs/<binary>/<component>/`

Prefer updating an existing canonical doc rather than creating a new overlapping note.
If a doc becomes stale or redundant, prune or merge it.

### 4. Record experiment conditions, not just outcomes
When a test or crash matters, capture:
- which binary was run
- which env flags were used
- which code path was forced vs faithful
- which dump/log/doc is the canonical reference

This is especially important when comparing diagnostic runs against original behavior.

### 5. Reverse engineering should serve the reimplementation
Static analysis is not separate from implementation work.
The best docs usually come from disassembly done while building and correcting the reimplementation.
Prefer that loop:

- inspect original behavior
- implement the closest faithful step
- run experiment
- document what changed
- prune outdated claims

### 6. Original binaries remain the source of truth
If a reimplementation, old note, or experiment conflicts with original binary behavior, the original binary wins.
Document the mismatch and adjust the implementation/docs.

## Practical Rules

- keep project source/build flow inside the project directory
- deploy built binaries into the game/runtime directory only for execution
- treat diagnostic-only hacks as diagnostics, not architecture
- label forced or incomplete paths clearly in code and docs
- avoid stale success claims after the project direction changes
