# Matrix Online launcher reverse engineering and implementation

## Rule 1: Fidelity to static-RE of launcher.exe

- Implementations must be faithful to static-RE of launcher.exe
- Use Ghidra for static-RE
- Anchor code to static-RE with comments like `// anchor: launcher.exe:0x41f390 / vtable +0x58`
- VTables are documented in ../../docs/launcher.exe/VTABLES/0x*.md
- Decompile and xrefs nearby anchored methods to discover static-RE faithful implementations
- Classes and methods should map directly to orignal launcher.exe vtable and implementation
- Fields and methods should be ordered by VTable slot from launcher.exe
- All fidelity improvements that don't require further exploration or static-RE investigation are in scope
- When discovering new VTables on the active path, exploring, documenting and implementing them as C++ classes is a big improvement to fidelity.

## Rule 2: Keep launcher working

At end of task:
- `make -j6 run` should launch into game

### Rule 3: Implementation, documentation and Ghidra should move together
- Do not treat documentation as a separate afterthought.
- When experiments, disassembly, or runtime traces change our understanding, update the canonical docs as part of the same work.
- Keep knowledge consolidated under: `../../docs/<binary>/<component>/`
- Prefer updating an existing canonical doc rather than creating a new overlapping note.
- If a doc becomes stale or redundant, prune or merge it.
- If doc grows large (2000 lines), split it.
- when an entry becomes resolved or stops steering current work, prune it instead of appending more history
- prefer owning recovered code structure, local TODOs, and active implementation notes in inline/header source comments
- avoid duplicate documentation; merge/prune overlapping docs and keep each topic in the smallest obvious canonical home instead of letting one large doc sprawl across many components
- prefer canonical `../../docs/<binary>/<component>/` docs for experiment evidence, crash references, and cross-component conclusions

# Misc

- deploy built binaries into the game/runtime directory only for execution
- when finishing one area of work and the next task likely moves into a different area
  - pruning of code that is no longer needed and can be cleaned up
  - suggest areas where further fidelity gains can be made
- Prefer batching tool calls
- Prefer batched small edit tool calls over single large edit
- When done, git add <paths ..> && git commit -m "<commit message>"

# Ghidra MCP

Use Ghidra as the primary static-analysis tool for launcher/client control flow, object layout, and call-shape recovery.

- Prefer decompile and verify with disassembly when extra confidence is needed.
- **Rename functions**: Sync method names with source code. When log message strings contain method names, use it. Otherwise use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- **Rename variables**: Sync variable names with source code. Use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- retype parameters / locals / globals in Ghidra when evidence supports it
- mirror confirmed names/types/anchors back into source comments and canonical docs in the same task
- use callers/callees/xrefs so isolated helper bodies are not over-interpreted
- when a callsite is high-value, write down the concrete argument mapping from the assembly, not just the decompiler's guessed prototype
- push confirmed Ghidra findings into source and canonical docs in the same task so knowledge does not live only in Ghidra
- record negative results too, especially when Ghidra proves a suspected path is **not** the caller / producer / first-send origin
- Create function in ghidra when you have high confidence that it should be a function. You have done this before.

## Ghidra usage

```
# Decompile one function by address
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# Decompile multiple functions by name
mcp({ tool: "ghidra_batch_decompile", args: '{"functions": "CLTSocketLayer_Init,CLTBaseThreadPerClientTCPEngine_ctor", "program": "launcher.exe"}' })

# Disassemble a function
mcp({ tool: "ghidra_disassemble_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# Current program info (tool requires an explicit program)
mcp({ tool: "ghidra_get_current_program_info", args: '{"program": "launcher.exe"}' })

# Get function callers/callees
# NOTE: ghidra_get_function_callers / ghidra_get_function_callees take a FUNCTION NAME, not an address.
# If you only have an address, first resolve/create the function, then query by name.
mcp({ tool: "ghidra_get_function_by_address", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_function_callers", args: '{"name": "CLTSocketLayer_Init", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_function_callees", args: '{"name": "CLTSocketLayer_Init", "program": "launcher.exe"}' })

# Create a function when bytes clearly form one but Ghidra has not created it yet
mcp({ tool: "ghidra_create_function", args: '{"address": "0x4472f0", "name": "AuthBootstrap680ReplyAuthDataValidator_CreateTemporaryWorker", "program": "launcher.exe"}' })

# Get xrefs from/to an address
mcp({ tool: "ghidra_get_xrefs_from", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_xrefs_to", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# Rename function by address:
mcp({ tool: "ghidra_rename_function_by_address", args: '{"function_address": "0x43b300", "new_name": "CLTLoginMediator_InitializeHelperDispatchTable", "program": "launcher.exe"}' })

# Rename function by name:
mcp({ tool: "ghidra_rename_function", args: '{"oldName": "FUN_0043b300", "newName": "CLTLoginMediator_InitializeHelperDispatchTable", "program": "launcher.exe"}' })

# Rename variables in a function
mcp({ tool: "ghidra_rename_variables", args: '{"function_address": "0x43b300", "variable_renames": {"puVar1": "ptr", "DAT_004d3d4c": "mutexCounter", "DAT_004d3d50": "initCounter"}, "program": "launcher.exe"}' })

# Read memory at an address
mcp({ tool: "ghidra_read_memory", args: '{"address": "0x4b51e0", "program": "launcher.exe"}' })

# Search direct virtual-call byte patterns (example: call [edx+0xf0])
mcp({ tool: "ghidra_search_byte_patterns", args: '{"pattern": "ff 92 f0 00 00 00", "mask": "xx xx xx xx xx xx", "program": "launcher.exe"}' })

# Switch/discriminator tables near current function
mcp({ tool: "ghidra_read_memory", args: '{"address": "0x41c4e4", "program": "launcher.exe", "length": 40}' })
```
