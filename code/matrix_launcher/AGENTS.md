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
- **Rename fields / struct members**: When class layouts become clearer, update OOAnalyzer / recovered structs in Ghidra so decompilation uses field names instead of raw offsets.
- retype parameters / locals / globals in Ghidra when evidence supports it
- when renaming many decompiler locals in one function, prefer small batches and re-run `list_variables` between batches; Ghidra can renumber synthetic locals after a rename
- mirror confirmed names/types/anchors back into source comments and canonical docs in the same task
- use callers/callees/xrefs so isolated helper bodies are not over-interpreted
- when a callsite is high-value, write down the concrete argument mapping from the assembly, not just the decompiler's guessed prototype
- push confirmed Ghidra findings into source and canonical docs in the same task so knowledge does not live only in Ghidra
- record negative results too, especially when Ghidra proves a suspected path is **not** the caller / producer / first-send origin
- Create function in ghidra when you have high confidence that it should be a function. You have done this before.

## Ghidra usage

Use the current `ghidra_functions` / `ghidra_inspect` / `ghidra_symbols` tool family below, not the older legacy command names. Default to `file_name: "launcher.exe"` and swap to `client.dll` when needed. Prefer `ghidra_batch_operations` for grouped edits, then `ghidra_project save`.

```js
// Programs
mcp({ tool: "ghidra_get_program_list", args: '{}' })
mcp({ tool: "ghidra_project", args: '{"file_name":"launcher.exe","action":"save"}' })

// Find / inspect
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"list","name_pattern":"CLauncher.*"}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"get","address":"0x40b430"}' })
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"decompile","name":"CLauncher_InitInstance"}' })
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"listing","address":"0x40b430","end_address":"0x40b4d0"}' })
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"references_to","address":"0x004d2c69"}' })
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"references_from","address":"0x0040a5a4"}' })

// Locals / functions
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"list_variables","name":"Launcher_ParseCommandLine"}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"rename_variable","name":"Launcher_ParseCommandLine","current_name":"pcVar6","new_name":"stringCursor"}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"create","address":"0x4472f0","function_name":"AuthBootstrap680ReplyAuthDataValidator_CreateTemporaryWorker"}' })

// Globals / symbols
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"get","address":"0x004d2c69"}' })
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"update","current_name":"DAT_004d2c69","new_name":"g_LauncherNoPatchFlowFlagByte"}' })

// Data types / field renames
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"list","name_pattern":".*LauncherLoginDialog.*"}' })
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"get","data_type_kind":"struct","name":"LauncherLoginDialog_0x4aae28","category_path":"/OOAnalyzer"}' })
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"update","data_type_kind":"struct","name":"LauncherLoginDialog_0x4aae28","category_path":"/OOAnalyzer","members":[{"name":"vftptr_0x0","data_type_path":"/OOAnalyzer/LauncherLoginDialog_0x4aae28::vftable_4aae28 *","offset":0},{"name":"currentPageState","data_type_path":"int","offset":116},{"name":"selectionList","data_type_path":"/OOAnalyzer/cls_0x4acd98","offset":2572},{"name":"hostedBrowserControl","data_type_path":"/Demangler/Browser/IControl *","offset":2792}]}' })
// note: data_types update replaces the struct member list; preserve/rename every member you care about and keep explicit offsets

// Comments / bookmarks
mcp({ tool: "ghidra_annotate", args: '{"file_name":"launcher.exe","action":"set_comment","address":"0x40ec70","comment_type":"EOL","text":"selection command helper"}' })
mcp({ tool: "ghidra_annotate", args: '{"file_name":"launcher.exe","action":"create_bookmark","address":"0x40ec70","bookmark_type":"Analysis","bookmark_category":"RENOTE","comment":"selection command helper"}' })

// Memory / tables
mcp({ tool: "ghidra_memory", args: '{"file_name":"launcher.exe","action":"read","address":"0x4b51e0","length":64}' })

// Batch related edits in one transaction
mcp({ tool: "ghidra_batch_operations", args: '{"file_name":"launcher.exe","operations":[{"tool":"symbols","arguments":{"action":"update","current_name":"DAT_004d2c69","new_name":"g_LauncherNoPatchFlowFlagByte"}},{"tool":"functions","arguments":{"action":"rename_variable","name":"Launcher_ParseCommandLine","current_name":"pcVar6","new_name":"stringCursor"}}]}' })

// Oversized output
mcp({ tool: "ghidra_read_tool_output", args: '{"action":"read","session_id":"ses_...","output_id":"out_...","offset":0,"max_chars":12000}' })
```