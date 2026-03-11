# mxowrap.dll Debugging Summary

## Problem

`mxowrap.dll` fails to load under Wine with error 1114 (ERROR_DLL_INIT_FAILED).

## What We Discovered

### 1. DllMain Returns FALSE During Initialization

- **Entry point**: `0x10020650`
- **Failure location**: Function at `0x1001f338` returns FALSE
- **Root cause**: TLS initialization function at `0x1001ef88` fails

### 2. dbghelp.dll Dependency

mxowrap.dll dynamically loads `dbghelp.dll` during initialization:

```
mxowrap.dll → LoadLibrary("dbghelp.dll") → Use for symbol operations → FreeLibrary
```

#### Wine Behavior

- **Default**: Loads Wine's builtin dbghelp.dll from `C:\windows\system32\dbghelp.dll`
- **Issue**: Wine's builtin dbghelp.dll crashes with null pointer dereference at `0x75a529ff`

#### Native dbghelp.dll

- **Version**: 6.12.2.633 (from game directory)
- **Size**: 1,231,000 bytes
- **Type**: PE32 executable (DLL) (console) Intel 80386

### 3. DLL Override Attempt

Copied native dbghelp.dll to system32 and set override:

```bash
cp ~/MxO_7.6005/dbghelp.dll ~/.wine/drive_c/windows/system32/
WINEDLLOVERRIDES="dbghelp=n" wine test_mxowrap.exe
```

**Result**: Native dbghelp.dll loads successfully, but DllMain still returns FALSE.

### 4. Load Order Analysis

```
0024:trace:loaddll:build_module Loaded L"mxowrap.dll" at 78DF0000: native
0024:trace:loaddll:build_module Loaded L"dbghelp.dll" at 78A30000: native
0024:trace:loaddll:free_modref Unloaded module L"dbghelp.dll" : native
0024:trace:loaddll:free_modref Unloaded module L"mxowrap.dll" : native
```

Both DLLs load and unload, but DllMain still fails.

## Why mxowrap.dll Exists

From analysis, mxowrap.dll serves multiple purposes:

1. **DbgHelp Proxy**: Exports all standard DbgHelp API functions
2. **API Hooking**: Uses Microsoft Detours to patch kernel32.dll and advapi32.dll
3. **Runtime Patching**: Applies patches to client.dll in memory
4. **Patch Integration**: Interfaces with patchctl.dll/xpatcher.dll

## Current Workaround

### Option 1: Patch client.dll (Recommended)

Replace the string "mxowrap.dll" with "dbghelp.dll" in client.dll:

```python
data = open('client.dll', 'rb').read()
data = data.replace(b'mxowrap.dll', b'dbghelp.dll')
open('client.dll', 'wb').write(data)
```

**Result**: client.dll loads successfully, but crashes later due to missing runtime patches.

### Option 2: Create Minimal mxowrap.dll

Create a stub DLL that:
- Exports all DbgHelp functions
- Forwards to real dbghelp.dll
- Returns TRUE from DllMain
- Skips problematic Detours initialization

## Remaining Questions

1. **What patches does mxowrap.dll apply to client.dll?**
   - Need to analyze the Detours hooks
   - May need to apply patches differently

2. **Why does TLS initialization fail?**
   - Function `0x1001ef88` processes TLS callbacks
   - Wine may not fully support TLS callbacks for native DLLs

3. **Can we run the original launcher?**
   - Original launcher.exe imports mxowrap.dll
   - May have different initialization order
   - Worth testing

## Files Analyzed

| File | Size | MD5 |
|------|------|-----|
| mxowrap.dll | 452,096 | - |
| mxowrap.dll.backup | 452,096 | - |
| dbghelp.dll | 1,231,000 | - |
| client.dll | 11,488,647 | - |

## Tools Used

- `objdump` - Disassembly and PE analysis
- `strings` - String extraction
- `nm` - Symbol table inspection
- Wine GDB server - Remote debugging
- `WINEDEBUG` - Wine debug channels

## Next Steps

1. **Analyze Detours hooks**: Determine what APIs are being hooked
2. **Test original launcher**: Check if it has special initialization
3. **Create stub mxowrap.dll**: Minimal implementation that loads successfully
4. **Static patching**: Apply required patches to client.dll at build time

---
*Session Date: March 10, 2025*
