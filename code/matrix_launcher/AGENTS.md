# Matrix Online launcher reverse engineering and implementation

## Rule 1: Fidelity to static-RE of launcher.exe

- Implementations must be faithful to static-RE of launcher.exe
- Use Ghidra for static-RE
- Anchor code to static-RE with comments like `// anchor: launcher.exe:0x41f390 / vtable +0x58`
- VTables are documented in ../../docs/launcher.exe/VTABLES/0x*.md
- Decompile and xrefs nearby anchored methods to discover static-RE faithful implementations
- Classes and methods should map directly to orignal launcher.exe vtable and implementation
- Fields and methods should be ordered by VTable slot from launcher.exe
- Follow method boundaries - don't create helpers when there weren't any in static-RE.
- All fidelity improvements that don't require further exploration or static-RE investigation are in scope
- When discovering new VTables on the active path, exploring, documenting and implementing them as C++ classes is a big improvement to fidelity.

## Rule 2: Keep launcher working

At end of task:
- Run `make -j4` to build OR `make -j4 run` to build and run the game.

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
  - `ghidra_data_types update` supports `member_update_mode:"patch"` for single-member struct edits by offset.
  - use full `members` replacement only when you need to grow/reflow overlapping regions.
  - always `list`/`get` the exact current struct name first (OOAnalyzer names may include the vtable/address suffix, e.g. `CLTLoginMediator_0x4b01c8`).
  - OOAnalyzer class cleanup often needs both sides updated:
    - rename/move the class namespace with `ghidra_symbols`
    - update the matching class data type under `/ClassDataTypes/...` with `ghidra_data_types`
- retype parameters / locals / globals in Ghidra when evidence supports it
  - use `ghidra_functions update_prototype` for parameter / return-type edits
  - use `ghidra_functions rename_variable` with `new_data_type` for local-variable retyping
- when renaming many decompiler locals in one function, prefer small batches and re-run `list_variables` / `decompile` between batches; Ghidra can renumber synthetic locals after a rename
- when renaming numbered stack locals like `local_1cc`, work from the highest stack offset / highest local number downward; this minimizes fill-gap renumbering churn
- `ghidra_batch_operations` works well for descending stack-local rename batches, but mixed batches of decompiler temps (`uVar*`, `bVar*`, `extraout_*`) can still fail even when the same renames succeed one-by-one
- renaming decompiler temps through MCP can leave the old synthetic name visible in `list_variables` while the decompiler switches to the new user-defined alias; treat `decompile` as the source of truth for whether the new name actually stuck
- mirror confirmed names/types/anchors back into source comments and canonical docs in the same task
- use callers/callees/xrefs so isolated helper bodies are not over-interpreted
- when a callsite is high-value, write down the concrete argument mapping from the assembly, not just the decompiler's guessed prototype
- push confirmed Ghidra findings into source and canonical docs in the same task so knowledge does not live only in Ghidra
- record negative results too, especially when Ghidra proves a suspected path is **not** the caller / producer / first-send origin
- pay extra attention to LauncherLogin world-selection / character-selection / create-character / delete-character flows
  - when discovering them in Ghidra, prioritize the exact LoginMediator interaction: concrete vtable slot, caller/callee relation, argument mapping, and resulting mediator state/owner-field effects
- Create function in ghidra when you have high confidence that it should be a function. You have done this before.

## Ghidra usage

Use the current `ghidra_functions` / `ghidra_inspect` / `ghidra_symbols` tool family below, not the older legacy command names. Default to `file_name: "launcher.exe"` and swap to `client.dll` when needed. Prefer `ghidra_batch_operations` for grouped edits, then `ghidra_project save`.
- if `ghidra_batch_operations` fails with an active transaction / lock issue, retry the same edits as sequential tool calls.
- `ghidra_functions update_prototype` parameter entries use `data_type`, **not** `type`.
- for `ghidra_functions update_prototype`, pointer types are easiest to spell with the Ghidra display name (e.g. `cls_0x4ba23c *`), not the full `/OOAnalyzer/...` path.
- if `ghidra_symbols update` hits duplicate OOAnalyzer names, first `get` by address and then rename using the fully-qualified `Namespace::symbol` form.

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

// VTable inspection - list vtable entries at address
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"listing","address":"0x4af2a4","end_address":"0x4af300"}' })
mcp({ tool: "ghidra_inspect", args: '{"file_name":"launcher.exe","action":"listing","address":"0x4b6524","end_address":"0x4b6538"}' })

// Functions / locals
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"list_variables","name":"Launcher_ParseCommandLine"}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"rename_variable","name":"Launcher_ParseCommandLine","variable_symbol_id":12345,"new_name":"stringCursor","new_data_type":"char *"}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"update_prototype","name":"CMessageConnection_OnOperationCompleted","return_type":"uint","parameters":[{"name":"operationWorkItem","data_type":"void *"}]}' })
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"create","address":"0x4472f0","function_name":"AuthBootstrap680ReplyAuthDataValidator_CreateTemporaryWorker"}' })

// Symbols / classes
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"update","current_name":"DAT_004d2c69","new_name":"g_LauncherNoPatchFlowFlagByte"}' })
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"create","symbol_type":"namespace","name":"SubmitLoginRequestInput_0x407d50","namespace":"OOAnalyzer"}' })
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"convert_to_class","name":"SubmitLoginRequestInput_0x407d50","namespace":"OOAnalyzer"}' })
mcp({ tool: "ghidra_symbols", args: '{"file_name":"launcher.exe","action":"update","current_name":"OOAnalyzer::cls_0x407d50::ReleaseSubmitSessionTokenStringStorage","new_name":"ReleaseSubmitSessionTokenStringStorage","namespace":"OOAnalyzer::SubmitLoginRequestInput_0x407d50"}' })

// Data types
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"get","data_type_kind":"struct","name":"CLTLoginMediator_0x4b01c8","category_path":"/OOAnalyzer"}' })
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"update","data_type_kind":"struct","name":"CLTLoginMediator_0x4b01c8","category_path":"/OOAnalyzer","member_update_mode":"patch","members":[{"offset":148,"name":"ownerAuthBootstrapSource94"}]}' })
mcp({ tool: "ghidra_data_types", args: '{"file_name":"launcher.exe","action":"update","data_type_kind":"struct","name":"SubmitLoginRequestInput_0x407d50","category_path":"/ClassDataTypes/OOAnalyzer","members":[{"name":"submitUsername","data_type_path":"char[32]","offset":0},{"name":"submitPassword","data_type_path":"char[32]","offset":32},{"name":"submitSessionTokenString","data_type_path":"/OOAnalyzer/cls_0x403f90","offset":96},{"name":"submitRequestFlag6c","data_type_path":"byte","offset":108}]}' })

// Comments / bookmarks
mcp({ tool: "ghidra_annotate", args: '{"file_name":"launcher.exe","action":"set_comment","address":"0x40ec70","comment_type":"EOL","text":"selection command helper"}' })
mcp({ tool: "ghidra_annotate", args: '{"file_name":"launcher.exe","action":"create_bookmark","address":"0x40ec70","bookmark_type":"Analysis","bookmark_category":"RENOTE","comment":"selection command helper"}' })

// Batch related edits in one transaction
mcp({ tool: "ghidra_batch_operations", args: '{"file_name":"launcher.exe","operations":[{"tool":"symbols","arguments":{"action":"update","current_name":"DAT_004d2c69","new_name":"g_LauncherNoPatchFlowFlagByte"}},{"tool":"functions","arguments":{"action":"rename_variable","name":"Launcher_ParseCommandLine","variable_symbol_id":12345,"new_name":"stringCursor","new_data_type":"char *"}}]}' })

// Variable renaming workflow (complete example):
// 1. First list variables to discover the symbol_id or high_symbol_id
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"list_variables","name":"FunctionName"}' })
// 2. Then rename using the symbol_id (small integers for parameters/USER_DEFINED locals)
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"rename_variable","name":"FunctionName","variable_symbol_id":9621,"new_name":"betterName","new_data_type":"uint8_t *"}' })
// 3. For decompiler synthetics (pcVar*, uVar*, extraout_*), use high_symbol_id AS A STRING
//    JSON truncates 64-bit integers, so "4614873502636310661" works but 4614873502636310661 fails
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"rename_variable","name":"CLTIPAddressList_Reinit","variable_symbol_id":"4614873502636310661","new_name":"tokenCursor","new_data_type":"char *"}' })
// 4. Batch multiple renames (use string IDs for synthetics, integers for regular symbol_ids)
mcp({ tool: "ghidra_batch_operations", args: '{"file_name":"launcher.exe","operations":[{"tool":"functions","arguments":{"action":"rename_variable","name":"FunctionName","variable_symbol_id":9621,"new_name":"var1","new_data_type":"uint8_t *"}},{"tool":"functions","arguments":{"action":"rename_variable","name":"CLTIPAddressList_Reinit","variable_symbol_id":"4614873502636310662","new_name":"resolveResult","new_data_type":"uint"}}]}' })

// Oversized output
mcp({ tool: "ghidra_read_tool_output", args: '{"action":"read","session_id":"ses_...","output_id":"out_...","offset":0,"max_chars":12000}' })
```
