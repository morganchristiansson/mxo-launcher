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

Some Ghidra installs report extra grouped tools internally, but this harness consistently exposes the common gateway-visible calls below; use `ghidra_list_tool_groups` / `ghidra_load_tool_group` to probe that gap when needed.

```
# List optional Ghidra tool groups and their load state
mcp({ tool: "ghidra_list_tool_groups", args: '{}' })

# Ask Ghidra to load extra tool groups when they are available
mcp({ tool: "ghidra_load_tool_group", args: '{"group": "all"}' })

# Confirm the active program and image base
mcp({ tool: "ghidra_get_current_program_info", args: '{"program": "launcher.exe"}' })

# Resolve the function currently owning an address
mcp({ tool: "ghidra_get_function_by_address", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# Decompile one function by address
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# Decompile several named functions together
mcp({ tool: "ghidra_batch_decompile", args: '{"functions": "CLTSocketLayer_Init,CLTBaseThreadPerClientTCPEngine_ctor", "program": "launcher.exe"}' })

# Inspect assembly for one function
mcp({ tool: "ghidra_disassemble_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })

# List renameable parameters and locals in one function
mcp({ tool: "ghidra_get_function_variables", args: '{"function_name": "CLauncher_InitInstance", "program": "launcher.exe"}' })

# Rename a function when the address is known
mcp({ tool: "ghidra_rename_function_by_address", args: '{"function_address": "0x43b300", "new_name": "CLTLoginMediator_InitializeHelperDispatchTable", "program": "launcher.exe"}' })

# Rename the function, params, and locals in one shot
mcp({ tool: "ghidra_batch_rename_function_components", args: '{"function_address": "0x43b300", "function_name": "CLTLoginMediator_InitializeHelperDispatchTable", "parameter_renames": {"param_1": "mediator"}, "local_renames": {"local_18": "dispatchTable"}, "program": "launcher.exe"}' })

# Batch rename variables referenced from one function
mcp({ tool: "ghidra_batch_rename_variables", args: '{"function_address": "0x43b300", "variable_renames": {"DAT_004d3d4c": "mutexCounter", "DAT_004d3d50": "initCounter"}, "program": "launcher.exe"}' })

# Improve a function signature for cleaner decompilation
mcp({ tool: "ghidra_set_function_prototype", args: '{"function_address": "0x403390", "prototype": "LauncherLoginDialog * LauncherLoginDialog_ctor(void *this)", "calling_convention": "__thiscall", "program": "launcher.exe"}' })

# Retype a local variable when layout is known
mcp({ tool: "ghidra_set_local_variable_type", args: '{"function_address": "0x43b300", "variable_name": "local_18", "new_type": "char[260]", "program": "launcher.exe"}' })

# Retype one parameter
mcp({ tool: "ghidra_set_parameter_type", args: '{"function_address": "0x403390", "parameter_name": "this", "new_type": "LauncherLoginDialog *", "program": "launcher.exe"}' })

# Read raw memory, strings, or tables at an address
mcp({ tool: "ghidra_read_memory", args: '{"address": "0x4b51e0", "length": 64, "program": "launcher.exe"}' })

# Search functions by name substring
mcp({ tool: "ghidra_search_functions", args: '{"name_pattern": "LoginMediator", "program": "launcher.exe"}' })

# Search defined strings by regex
mcp({ tool: "ghidra_search_strings", args: '{"pattern": "launcher\\.cpp", "program": "launcher.exe"}' })

# Create a missing function at a high-confidence entry
mcp({ tool: "ghidra_create_function", args: '{"address": "0x4472f0", "name": "AuthBootstrap680ReplyAuthDataValidator_CreateTemporaryWorker", "program": "launcher.exe"}' })

# Leave a small recoverability note in the listing
mcp({ tool: "ghidra_set_bookmark", args: '{"address": "0x40ec70", "category": "RENOTE", "comment": "selection command helper", "program": "launcher.exe"}' })

# Save rename/type/comment changes back to the project
mcp({ tool: "ghidra_save_program", args: '{"program": "launcher.exe"}' })
```
