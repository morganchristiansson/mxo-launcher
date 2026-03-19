# Ghidra MCP

Use Ghidra as the primary static-analysis tool for launcher/client control flow, object layout, and call-shape recovery.

- **Don't trust decompiler alone**: Verify with disassembly tools when in doubt about calling conventions, stack cleanup, or field semantics
- **Cross-reference**: Combine decompilation output with direct disassembly analysis for critical functions
- **Rename functions**: Sync method names with source code. When log message strings contain method names, use it. Otherwise use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- **Rename variables**: Sync variable names with source code. Use descriptive names to improve long term clarity, improve existing names when our understanding improves.
- retype parameters / locals / globals in Ghidra when evidence supports it
- mirror confirmed names/types/anchors back into source comments and canonical docs in the same task
- use **decompile + disassembly together** for important functions; do not trust decompiler output alone for:
  - calling convention
  - stack cleanup
  - byte-vs-dword field meaning
  - inline string / small-buffer layout
- when recovering an object field, prefer this chain of evidence:
  - allocator / ctor
  - fill/helper writer
  - later reader / consumer
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

### Decompile one or multiple function
```
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_batch_decompile", args: '{"functions": "0x400000 0x410000 0x420000", "program": "launcher.exe"}' })
```

### Disassemble a function
```
mcp({ tool: "ghidra_disassemble_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

### Get function callers/callees
```
mcp({ tool: "ghidra_get_function_callers", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_function_callees", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

### Get xrefs from/to an address
```
mcp({ tool: "ghidra_get_xrefs_from", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
mcp({ tool: "ghidra_get_xrefs_to", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

### Rename a function
```
mcp({ tool: "ghidra_rename_function", args: '{"name": "CLTLoginMediator_InitializeHelperDispatchTable", "address": "0x43b300", "program": "launcher.exe"}' })
```

### Rename variables in a function
```
mcp({ tool: "ghidra_rename_variables", args: '{"function_address": "0x43b300", "variables": {"puVar1": "ptr", "DAT_004d3d4c": "mutexCounter", "DAT_004d3d50": "initCounter"}}}' })
```

### Read memory at an address
```
mcp({ tool: "ghidra_read_memory", args: '{"address": "0x4b51e0", "program": "launcher.exe"}' })
```
