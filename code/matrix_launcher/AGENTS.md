## Build / run

Build:
```
make -j6
```

Build and run:
```
make -j6 run
```

## Ghidra

Decompile function with Ghidra:
```
mcp({ tool: "ghidra_decompile_function", args: '{"address": "0x43b300", "program": "launcher.exe"}' })
```

Read `./GHIDRA.md` for more
