# VTable dumps and documentation

Folder contains a very large number of markdown files named after VTable address.
Use grep to search them, do not list them.

## Ghidra MCP Tools

```
mcp({ tool: "ghidra_get_function_metrics", args: '{"address": "0x0041b520"}' })
mcp({ tool: "ghidra_get_function_metrics", args: '{"address": "0x0041b520 ..."}' })
```
If you discover correct syntax for querying multiple addresses, correct AGENTS.md in current folder and remove this line.

Ghidra howto: ../../../code/matrix_launcher/GHIDRA.md
