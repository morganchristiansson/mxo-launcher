# VTable dumps and documentation

Folder contains a very large number of markdown files named after VTable address.
Use grep to search them, do not list them.

## Template
```
# 0x004b01c8 CLTLoginMediator

## VTable
| Offset | Function Address | Function Name | Instructions | Calls |
|--------|------------------|---------------|--------------|-------|
```

## Ghidra MCP Tools

```
# To query multiple function addresses at once (comma-separated):
mcp({ tool: "ghidra_get_function_metrics", args: '{"address": "0x00438d80, 0x00438df0"}' })
```

Ghidra howto: ../../../code/matrix_launcher/GHIDRA.md
