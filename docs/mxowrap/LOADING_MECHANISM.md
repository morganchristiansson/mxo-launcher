# mxowrap.dll Loading Mechanism Discovery

## Critical Finding

**mxowrap.dll successfully loads when imported by launcher.exe, but fails when loaded via LoadLibrary!**

## Evidence

### Original Launcher (SUCCESS)

```
0024:trace:loaddll:build_module Loaded L"Z:\\home\\pi\\MxO_7.6005\\mxowrap.dll" at 77320000: native
0024:trace:module:process_attach (L"mxowrap.dll",0031FD24) - START
0024:trace:module:MODULE_InitDLL (77320000 L"mxowrap.dll",PROCESS_ATTACH,0031FD24) - CALL
0024:trace:module:process_attach (L"mxowrap.dll",0031FD24) - END
```

**Result**: DllMain returns TRUE, initialization succeeds.

### Test Program with LoadLibrary (FAILURE)

```cpp
HMODULE hMxo = LoadLibraryA("mxowrap.dll");
// Returns NULL with error 1114 (DllMain returned FALSE)
```

**Result**: DllMain returns FALSE, initialization fails.

## Why This Matters

The original `launcher.exe` imports `mxowrap.dll` directly in its import table:

```
DLL Name: mxowrap.dll
  MiniDumpWriteDump
  SymGetModuleBase64
  SymFunctionTableAccess64
  StackWalk64
  SymFromAddr
  UnDecorateSymbolName
  SymGetLineFromAddr64
  SymGetModuleInfo64
  SymSetContext
  SymEnumSymbols
  SymCleanup
  SymGetTypeInfo
```

This means the Windows loader loads `mxowrap.dll` **before** launcher.exe's main() function runs, as part of the process initialization sequence.

## Hypothesis

The difference in behavior could be due to:

### 1. Load Order Dependencies

When loaded via import table:
- mxowrap.dll loads early in process startup
- Certain system state may not be fully initialized yet
- Some checks in DllMain might pass because state is in a specific condition

When loaded via LoadLibrary:
- mxowrap.dll loads after process is fully running
- System state has changed
- Checks that passed during early init now fail

### 2. Reserved Parameter Difference

DllMain receives a `lpvReserved` parameter:
```cpp
BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,
  DWORD     fdwReason,
  LPVOID    lpvReserved  // <-- This might differ!
);
```

For implicit loads (import table):
- `lpvReserved` is non-NULL (indicates static link)

For explicit loads (LoadLibrary):
- `lpvReserved` is NULL (indicates dynamic load)

### 3. Loader Lock

When loaded via import table:
- The loader lock is held during DllMain
- Some operations that require the loader lock succeed

When loaded via LoadLibrary:
- Same loader lock is held, so this shouldn't differ

### 4. TLS Initialization Timing

TLS callbacks run in a specific order during process initialization:
1. Executable's TLS callbacks
2. DLLs' TLS callbacks (in load order)
3. DllMain calls (in load order)

The timing of when TLS is set up relative to other initialization could matter.

## Testing the -nopatch Argument

The original launcher has a `-nopatch` argument that might affect mxowrap.dll's behavior:

```
-justpatch
-nopatch
```

The `-nopatch` argument likely bypasses the patching system, which mxowrap.dll is part of.

## Next Steps

1. **Test launcher.exe with GDB**: Set breakpoint on mxowrap.dll's DllMain to observe its behavior
2. **Compare lpvReserved values**: Check if this parameter differs between load methods
3. **Analyze DllMain logic**: Look for code that checks load method or reserved parameter
4. **Test with delay-load**: Try using delay-load import to see if timing matters

## Implications for resurrections.exe

Our `resurrections.exe` does NOT import mxowrap.dll. It tries to load client.dll via LoadLibrary, which then tries to load mxowrap.dll via LoadLibrary, which fails.

### Potential Fix

**Option 1**: Make resurrections.exe import mxowrap.dll (add dummy import)

```cpp
// Force mxowrap.dll to load via import table
#pragma comment(linker, "/include:_MiniDumpWriteDump")
__declspec(dllimport) BOOL WINAPI MiniDumpWriteDump(...);
```

**Option 2**: Patch client.dll to load dbghelp.dll directly (already tested)

**Option 3**: Patch mxowrap.dll's DllMain to always return TRUE (risky)

## Code to Analyze

The critical code in mxowrap.dll's DllMain that decides success/failure:

```asm
10020828:  call   0x1001f338        ; Critical init function
1002082d:  test   %al,%al
1002082f:  jne    0x10020838        ; If TRUE, continue
10020831:  xor    %eax,%eax         ; Return FALSE
10020833:  jmp    0x100208f3
```

Need to analyze why function `0x1001f338` returns TRUE for implicit loads but FALSE for explicit loads.

## Related Files

- `launcher.exe` - Original launcher that successfully loads mxowrap.dll
- `client.dll` - Game client that imports mxowrap.dll
- `mxowrap.dll` - Wrapper DLL with problematic initialization
- `dbghelp.dll` - Real DbgHelp that mxowrap.dll wraps

---
*Discovery Date: March 10, 2025*
*Testing: Wine with WINEDEBUG=+loaddll,+module*
