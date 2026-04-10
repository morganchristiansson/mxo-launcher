## Goals

- Rename variables to have descriptive names
- Expand knowledge in VTable markdown documentation

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

// Locals / functions (Sequential Rename Example)
mcp({ tool: "ghidra_functions", args: '{"file_name":"launcher.exe","action":"rename_variable","name":"Launcher_ParseCommandLine","current_name":"pcVar6","new_name":"stringCursor"}' })

// Batch Operations Example (Success Case)
// Use this for grouped, non-state-dependent changes (e.g., multiple symbol updates).
mcp({ tool: "ghidra_batch_operations", args: '{"file_name":"launcher.exe","operations":[{"tool":"symbols","arguments":{"action":"update","current_name":"DAT_004d2c69","new_name":"g_LauncherNoPatchFlowFlagByte"}},{"tool":"functions","arguments":{"action":"rename_variable","name":"Launcher_ParseCommandLine","current_name":"pcVar6","new_name":"stringCursor"}}]}' })

// Sequential Rename Example (Recommended for complex refactoring like InitClientDLL)
// If batching fails due to state/transaction issues, always use sequential calls:
// mcp({ tool: "ghidra_functions", args: '{"file_name":"client.dll","action":"rename_variable","name":"InitClientDLL","current_name":"parsed_command","new_name":"commandLineChar"}' })
// mcp({ tool: "ghidra_functions", args: '{"file_name":"client.dll","action":"rename_variable","name":"InitClientDLL","current_name":"exception_info","new_name":"exceptionInfo"}' })

// Oversized output
mcp({ tool: "ghidra_read_tool_output", args: '{"action":"read","session_id":"ses_...","output_id":"out_...","offset":0,"max_chars":12000}' })
```
