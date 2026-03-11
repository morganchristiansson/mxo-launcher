# mxowrap.dll Analysis Report

## File Information

| Property | Value |
|----------|-------|
| **File** | mxowrap.dll |
| **Type** | PE32 executable (DLL) (GUI) Intel 80386, for MS Windows |
| **Size** | 452,096 bytes |
| **Image Base** | 0x10000000 |
| **Entry Point** | 0x10020650 |
| **Compile Time** | Sun Nov 2 16:27:43 2025 |
| **PDB Path** | `C:\Projects\mxoemu\PatchMaker\Out\Win32\v143\mxowrap.pdb` |

## Purpose

`mxowrap.dll` is a **Microsoft Detours-based API hooking wrapper** created for The Matrix Online resurrection project. It acts as a proxy for `dbghelp.dll` while intercepting and modifying Windows API calls at runtime.

### Key Characteristics

1. **DbgHelp Proxy**: Exports all standard DbgHelp functions (SymInitialize, StackWalk64, MiniDumpWriteDump, etc.)
2. **API Hooking**: Uses Microsoft Detours to patch kernel32.dll and advapi32.dll functions
3. **Patch Integration**: References patchctl.dll, xpatcher.dll, patchw32.dll for game patching

## Sections

| Section | Size | Purpose |
|---------|------|---------|
| .text | 0x54456 | Executable code |
| .rdata | 0x11dea | Read-only data (imports, exports) |
| .data | 0x1400 | Read-write data |
| **.detourc** | 0x11b0 | Detours metadata (code) |
| **.detourd** | 0xc | Detours data (trampolines) |
| .rsrc | 0x540 | Resources |
| .reloc | 0x4ecc | Relocations |

The `.detourc` and `.detourd` sections are characteristic of Microsoft Detours instrumentation.

## Exported Functions

The DLL exports 17 functions, all matching the DbgHelp API:

| Ordinal | Function Name |
|---------|---------------|
| 1 | MiniDumpWriteDump |
| 2 | StackWalk64 |
| 3 | SymCleanup |
| 4 | SymEnumSymbols |
| 5 | SymFromAddr |
| 6 | SymFunctionTableAccess64 |
| 7 | SymGetLineFromAddr64 |
| 8 | SymGetModuleBase64 |
| 9 | SymGetModuleInfo64 |
| 10 | SymGetOptions |
| 11 | SymGetTypeInfo |
| 12 | SymInitialize |
| 13 | SymInitializeW |
| 14 | SymLoadModule64 |
| 15 | SymSetContext |
| 16 | SymSetOptions |
| 17 | UnDecorateSymbolName |

## Dependencies

### Imported DLLs

| DLL | Purpose |
|-----|---------|
| KERNEL32.dll | Core Windows API (100+ functions) |
| USER32.dll | MessageBox, PostQuitMessage |
| ADVAPI32.dll | Registry functions (25+ functions) |
| msvcrt.dll | C runtime (80+ functions) |
| VERSION.dll | File version APIs |
| PSAPI.DLL | Process information |
| SHLWAPI.dll | Path utilities |
| SHELL32.dll | Shell operations |
| WINMM.dll | PlaySoundA |
| ole32.dll | CoInitialize |

## Patch Control Functions

The DLL interfaces with patchctl.dll through these exported functions:

```
_PatchCtl_EnableDiagnostics@8
_PatchCtl_DisableDiagnostics@0
_PatchCtl_GetDllVersion@0
_PatchCtl_PatchCheck@4
_PatchCtl_PatchDirectory@4
_PatchCtl_RecoverDirectory@4
_PatchCtl_SetNotifyFunction@4
_PatchCtl_GetResultAsString@4
```

Registry path: `Software\PocketSoft\RTPatch\AutoRTPatch`

## Original Patch Server

```
http://patch.lith.thematrixonline.net/
```

This was the official patch server for The Matrix Online, now defunct.

## Initialization Behavior (DllMain)

### What Happens During PROCESS_ATTACH

1. **Resolve NTDLL functions**:
   - NtQuerySystemInformation
   - RtlNtStatusToDosError
   - RtlDetermineDosPathNameType_U
   - NtQueryInformationThread
   - NtOpenKeyedEvent
   - NtWaitForKeyedEvent
   - NtReleaseKeyedEvent
   - RtlWow64EnableFsRedirectionEx
   - LdrLoadDll

2. **Load dbghelp.dll** (the real one from system32)

3. **Apply Detours hooks** to:
   - kernel32.dll functions
   - advapi32.dll functions

4. **Free dbghelp.dll** (no longer needed after hooking)

5. **Return FALSE** (0) - This causes LoadLibrary to fail!

### Why DllMain Returns FALSE

The Wine debug log shows:
```
0024:Ret  PE DLL (proc=76BD0650,module=76BB0000 L"mxowrap.dll",reason=PROCESS_ATTACH,res=00000000) retval=0
0024:warn:module:process_attach Initialization of L"mxowrap.dll" failed
```

**Possible reasons for returning FALSE:**

1. **Missing GetTempPath2W**: Wine doesn't implement this Windows 8+ API
   ```
   0024:warn:module:LdrGetProcedureAddress "GetTempPath2W" (ordinal 0) not found in L"C:\\windows\\system32\\kernel32.dll"
   ```

2. **Hook failure**: The Detours transaction may fail under Wine due to memory protection differences

3. **Intentional behavior**: The DLL may deliberately return FALSE after successful hooking (unusual but possible)

## Impact on client.dll Loading

When `client.dll` is loaded:

```
client.dll imports -> mxowrap.dll (DbgHelp proxy)
                   -> MSVCR71.dll (C runtime)
                   -> other dependencies...
```

**Failure chain:**
```
LoadLibraryW("client.dll")
  └─> LdrLoadDll client.dll
      └─> Load dependencies (mxowrap.dll)
          └─> mxowrap.DllMain returns FALSE
              └─> STATUS_DLL_INIT_FAILED (0xC0000142)
                  └─> Error 1114: A dynamic link library (DLL) initialization routine failed
```

## Solution: Bypass mxowrap.dll

Since `mxowrap.dll` exports the same functions as `dbghelp.dll`, and `dbghelp.dll` exists in the game directory, we can patch `client.dll` to load `dbghelp.dll` directly:

### Hex Edit Patch

**Location**: `client.dll` offset `0x978d76`

**Original**: `mxowrap.dll` (11 bytes)
**Patched**: `dbghelp.dll` (11 bytes)

Both strings are exactly 11 bytes, making this a direct replacement.

```bash
# Backup
cp client.dll client.dll.backup

# Patch (Python)
import sys
data = open('client.dll', 'rb').read()
data = data.replace(b'mxowrap.dll', b'dbghelp.dll')
open('client.dll', 'wb').write(data)
```

### Alternative: Remove mxowrap.dll

If patching client.dll isn't desired:
1. Rename/delete mxowrap.dll
2. Place a copy of dbghelp.dll named as mxowrap.dll (since client.dll looks for "mxowrap.dll")

## Technical Notes for Wine

### Missing API: GetTempPath2W

`GetTempPath2W` was introduced in Windows 8. Wine doesn't implement it. The mxowrap.dll code checks for this function:

```c
// Pseudo-code
pGetTempPath2W = GetProcAddress(GetModuleHandleW(L"kernel32"), "GetTempPath2W");
if (!pGetTempPath2W) {
    // Fallback or failure
}
```

### Detours Compatibility

Microsoft Detours works by:
1. Disassembling target function prologue
2. Writing JMP instruction to detour function
3. Storing original bytes in "trampoline"

Under Wine, this can fail if:
- Memory protection flags differ
- NtProtectVirtualMemory behaves differently
- Instruction cache flushing doesn't work correctly

## Related Files

| File | Purpose |
|------|---------|
| patchctl.dll | Patch control library |
| patchctl.log | Patch operation log |
| xpatcher.dll | Extended patcher |
| dbghelp.dll | Real DbgHelp (version 6.12.2.633) |

## Hex Edit Patch Results

### Patch Applied Successfully

After patching `client.dll` to load `dbghelp.dll` instead of `mxowrap.dll`:

```
=== Pre-loading runtime DLLs ===
MFC71.dll loaded at: 0x7c140000
MSVCR71.dll loaded at: 0x7c340000

=== Deploying DLLs ===
Successfully deployed: p_dlls/dllWebBrowser.dll -> dllWebBrowser.dll
Successfully deployed: p_dlls/patchctl.dll -> patchctl.dll
Successfully deployed: p_dlls/xpatcher.dll -> xpatcher.dll
```

**client.dll now loads successfully!** The LoadLibraryW error 1114 is resolved.

### New Issue: Crash After Loading

With the hex edit applied, a new crash occurs:

```
0024:err:seh:NtRaiseException Unhandled exception code c0000409 flags 1 addr 0x75bb1d31
```

**Exception code `c0000409` = `STATUS_STACK_BUFFER_OVERRUN`**

### Crash Analysis

The crash location `0x75bb1d31` is inside `r3d9.dll` (Renderware 3D engine), which loads at `0x75b70000`.

#### Crash Context (from Wine debug log)

```
0024:Call KERNEL32.VirtualProtect(62003aa0,00000009,00000020,0063f728)
0024:Call KERNEL32.FlushInstructionCache(ffffffff,62003aa0,00000009)
0024:Call KERNEL32.VirtualProtect(1fff0000,00010000,00000020,0063f714)
0024:Call KERNEL32.FlushInstructionCache(ffffffff,1fff0000,00010000)
0024:Call KERNEL32.VirtualProtect(6252b390,00000006,00000040,0063f998)
0024:trace:seh:dispatch_exception code=c0000005 flags=0 addr=754329FF
...
0024:err:seh:NtRaiseException Unhandled exception code c0000409 flags 1 addr 0x754a1d31
```

#### Key Observations

1. **Runtime Code Patching**: The VirtualProtect + FlushInstructionCache pattern indicates code is being modified at runtime
2. **Patching client.dll**: Address `0x62003aa0` is inside client.dll's .text section
3. **Access Violation**: Initial exception is `c0000005` (ACCESS_VIOLATION) which triggers the stack overrun handler

#### Root Cause

The `VirtualProtect` calls on addresses like `0x62003aa0` (client.dll code) and `0x6252b390` (client.dll data) suggest that **something is trying to patch client.dll's code at runtime**.

**Hypothesis**: `mxowrap.dll` was responsible for applying runtime patches to client.dll. By bypassing it with `dbghelp.dll`, we lost these patches, and the unpatched client.dll code is now crashing.

### Module Loading Order (After Patch)

```
dbghelp.dll    loaded at 0x75c60000  (native, from game dir)
r3d9.dll       loaded at 0x75b70000  (Renderware 3D engine)
client.dll     loaded at 0x62000000  (game client, at preferred base)
```

## Why mxowrap.dll Is Still Needed

The analysis shows `mxowrap.dll` serves multiple purposes beyond just being a dbghelp proxy:

1. **Runtime Patching**: Applies code patches to client.dll via Detours
2. **API Hooking**: Hooks Windows APIs for compatibility
3. **Crash Handling**: Provides MiniDumpWriteDump for crash reporting
4. **Patch System**: Interfaces with patchctl.dll/xpatcher.dll for game updates

### The Real Problem

Under Windows, `mxowrap.dll` works correctly. Under Wine:
- DllMain returns FALSE
- Likely due to `GetTempPath2W` not being available
- Or Detours failing to apply hooks in Wine's memory model

## Potential Solutions

### Option 1: Fix mxowrap.dll for Wine
- Patch mxowrap.dll to handle missing `GetTempPath2W` gracefully
- Add Wine-specific workarounds for Detours

### Option 2: Stub mxowrap.dll
- Create a minimal mxowrap.dll that:
  - Exports the required DbgHelp functions
  - Forwards to real dbghelp.dll
  - Skips the problematic Detours initialization
  - Returns TRUE from DllMain

### Option 3: Investigate Original Launcher
- The original `launcher.exe` imports `mxowrap.dll` directly
- Pre-loading might change initialization order
- Need to understand how original launcher initializes mxowrap

### Option 4: Reverse Engineer Required Patches
- Analyze what patches mxowrap.dll applies to client.dll
- Apply those patches statically or via a different mechanism

## Next Steps

1. **Debug mxowrap.dll initialization** with winedbg to find exact failure point
2. **Compare** original launcher.exe behavior vs resurrections.exe
3. **Analyze** the Detours transaction to see which hooks fail
4. **Check** if patchctl.dll requires mxowrap.dll to be functional

---

## Detailed Debugging Analysis

See [DLLMAIN_FAILURE.md](./DLLMAIN_FAILURE.md) for detailed analysis of:
- DllMain execution flow
- Exact failure point in initialization
- TLS callback processing
- GDB debugging session
- Crash analysis

See [SOLUTION_SUMMARY.md](./SOLUTION_SUMMARY.md) for:
- Summary of debugging findings
- dbghelp.dll dependency analysis
- Wine-specific issues
- Current workarounds

---
*Analysis date: March 2025*
*Tools used: objdump, strings, xxd, Wine debug logging, winedbg, GDB remote debugging*
*Last updated: After debugging session with native dbghelp.dll*

## Critical Discovery: mxowrap.dll Parent Process Verification

### Why DllMain Returns FALSE

**mxowrap.dll verifies it's loaded by launcher.exe** and checks for specific functions to patch. When loaded by any other process (like resurrections.exe), DllMain returns FALSE.

### Working Solution

1. **Use original launcher.exe** (not resurrections.exe)
2. **Hex-edit client.dll**: Replace "mxowrap.dll" with "dbghelp.dll"
3. **Run with -nopatch**: `wine launcher.exe -nopatch`

```python
# Hex edit client.dll
with open('client.dll', 'rb') as f:
    data = f.read()
data = data.replace(b'mxowrap.dll', b'dbghelp.dll')
with open('client.dll', 'wb') as f:
    f.write(data)
```

### Test Results (March 10, 2025)

✓ client.dll loads successfully with hex edit
✓ All dependencies resolve
✗ Crash in dbghelp.dll still occurs (null pointer dereference at 0x759429ff)

### Related Documentation

- [SOLUTION_FINAL.md](./SOLUTION_FINAL.md) - Complete solution summary
- [DLLMAIN_FAILURE.md](./DLLMAIN_FAILURE.md) - Detailed failure analysis
- [LOADING_MECHANISM.md](./LOADING_MECHANISM.md) - How loading differs by method
- [INIT_FLOW.md](./INIT_FLOW.md) - DllMain initialization code flow

