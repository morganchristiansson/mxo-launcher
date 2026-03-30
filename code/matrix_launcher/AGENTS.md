# Matrix Online launcher reverse engineering and implementation

## Rule 1: Fidelity to static-RE of launcher.exe

- Implementations must be faithful to static-RE of launcher.exe
- Use Ghidra for static-RE
- Annotate with `// anchor: launcher.exe:0x41f390 / owner vtable +0x58`

## Rule 2: Keep launcher working

At end of task:
- `make -j6 run` should launch into game

### Rule 3: Implementation, documentation and Ghidra should move together
- Do not treat documentation as a separate afterthought.
- When experiments, disassembly, or runtime traces change our understanding, update the canonical docs as part of the same work.
- Keep knowledge consolidated under: `docs/<binary>/<component>/`
- Prefer updating an existing canonical doc rather than creating a new overlapping note.
- If a doc becomes stale or redundant, prune or merge it.
- If doc grows large (2000 lines), split it.
- when an entry becomes resolved or stops steering current work, prune it instead of appending more history
- prefer owning recovered code structure, local TODOs, and active implementation notes in inline/header source comments
- avoid duplicate documentation; merge/prune overlapping docs and keep each topic in the smallest obvious canonical home instead of letting one large doc sprawl across many components

# Misc

- deploy built binaries into the game/runtime directory only for execution
- treat diagnostic-only hacks as diagnostics, not architecture
- prefer canonical `docs/<binary>/<component>/` docs for experiment evidence, crash references, and cross-component conclusions
- when finishing one area of work and the next task likely moves into a different area
  - Suggest cleanup and pruing of code that is no longer needed and can be cleaned up
  - suggest areas where fidelity can increase further

# Ghidra

Decompile function with Ghidra:
```
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

Read `./GHIDRA.md` for more
