# Ghidra MCP

Use Ghidra as the primary static-analysis tool for launcher/client control flow, object layout, and call-shape recovery.

- Verify with disassembly tools when in doubt about calling conventions, stack cleanup, or field semantics
- **Cross-reference**: Combine decompilation output with direct disassembly analysis for critical functions
- **Rename functions**: Sync method names with source code. When log message strings contain method names, use it. Otherwise use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- **Rename variables**: Sync variable names with source code. Use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- retype parameters / locals / globals in Ghidra when evidence supports it
- mirror confirmed names/types/anchors back into source comments and canonical docs in the same task
- use callers/callees/xrefs aggressively so isolated helper bodies are not over-interpreted
- for branch conditions, check the actual instruction width (`test al,al` vs `test eax,eax`, etc.) before documenting field semantics
- when a callsite is high-value, write down the concrete argument mapping from the assembly, not just the decompiler's guessed prototype
- push confirmed Ghidra findings into source comments/scaffolds and canonical docs in the same task so knowledge does not live only in the tool session
- record negative results too, especially when Ghidra proves a suspected path is **not** the caller / producer / first-send origin

## Anchors
Document function addresses in comment above methods like this:
```
// anchor: launcher.exe:0x420640
void CLTLoginMediator::InitializeHelperDispatchSlot15() {
```

## Example usage

### Decompile one function by address, or multiple functions by name
```
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_batch_decompile", args: '{"functions": "CLTSocketLayer_Init,CLTBaseThreadPerClientTCPEngine_ctor", "program": "launcher.exe"}' })
```

### Disassemble a function
```
mcp({ tool: "ghidra_disassemble_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

### Get function callers/callees
```
# Note: these tools currently expect a function name, not an address.
mcp({ tool: "ghidra_get_function_callers", args: '{"name": "CLTSocketLayer_Init", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_function_callees", args: '{"name": "CLTSocketLayer_Init", "program": "launcher.exe"}' })
```

### Get xrefs from/to an address
```
mcp({ tool: "ghidra_get_xrefs_from", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_xrefs_to", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

### Rename a function
```
# Rename by address:
mcp({ tool: "ghidra_rename_function_by_address", args: '{"function_address": "0x43b300", "new_name": "CLTLoginMediator_InitializeHelperDispatchTable", "program": "launcher.exe"}' })

# Or rename by old/new function name:
mcp({ tool: "ghidra_rename_function", args: '{"oldName": "FUN_0043b300", "newName": "CLTLoginMediator_InitializeHelperDispatchTable", "program": "launcher.exe"}' })
```

### Rename variables in a function
```
mcp({ tool: "ghidra_rename_variables", args: '{"function_address": "0x43b300", "variable_renames": {"puVar1": "ptr", "DAT_004d3d4c": "mutexCounter", "DAT_004d3d50": "initCounter"}, "program": "launcher.exe"}' })
```

### Read memory at an address
```
mcp({ tool: "ghidra_read_memory", args: '{"address": "0x4b51e0", "program": "launcher.exe"}' })
```
