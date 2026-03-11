# Solution: Using Original Launcher with Hex-Edited client.dll

## Working Solution (with caveats)

Based on rajkosto's insight, the correct approach is:

1. **Use the original `launcher.exe`**
2. **Hex-edit `client.dll`**: Replace "mxowrap.dll" with "dbghelp.dll"
3. **Run with `-nopatch` argument**: `wine launcher.exe -nopatch`

## Why This Works

### mxowrap.dll Verification

mxowrap.dll contains code that **verifies it's loaded by launcher.exe** and checks for specific functions to patch. This is why:

- `LoadLibrary("mxowrap.dll")` fails when called from any other process
- DllMain returns FALSE (error 1114) when loaded by resurrections.exe
- It works when loaded via launcher.exe's import table

### The Hex Edit

Since mxowrap.dll won't load outside of launcher.exe, we patch client.dll to import dbghelp.dll directly:

```python
# Patch client.dll
with open('client.dll', 'rb') as f:
    data = f.read()

# Replace the import string
data = data.replace(b'mxowrap.dll', b'dbghelp.dll')

with open('client.dll', 'wb') as f:
    f.write(data)
```

**Result**: client.dll successfully loads with dbghelp.dll instead of mxowrap.dll.

### The -nopatch Argument

The `-nopatch` argument bypasses the patching system that mxowrap.dll is part of, allowing the game to run without the runtime patches that mxowrap.dll would normally apply.

## Test Results

### Successfully Loaded

```
✓ launcher.exe started
✓ mxowrap.dll loaded (via launcher.exe import table)
✓ client.dll loaded successfully
✓ dbghelp.dll loaded (hex-edited import)
✓ All dependencies resolved
```

### Current Issue: Crash in dbghelp.dll

After successful loading, there's still a crash:

```
Program received signal SIGSEGV, Segmentation fault.
0x759429ff in ?? ()
=> 0x759429ff:  mov    (%ecx),%eax    ; ECX = 0 (null pointer)
```

This crash occurs in dbghelp.dll code, likely due to:
- Missing initialization that mxowrap.dll would normally perform
- Incompatibility between the native dbghelp.dll and Wine
- Different behavior expected by client.dll

## Next Steps

### Option 1: Fix dbghelp.dll Crash
- Debug why dbghelp.dll is crashing
- May need Wine-specific fixes or different dbghelp.dll version

### Option 2: Investigate mxowrap.dll Patches
- Determine what runtime patches mxowrap.dll applies to client.dll
- Apply those patches statically or through a different mechanism

### Option 3: Create Minimal mxowrap.dll Stub
- Implement a minimal mxowrap.dll that:
  - Exports all required DbgHelp functions
  - Returns TRUE from DllMain regardless of parent process
  - Forwards to real dbghelp.dll
  - Skips the launcher.exe verification

## Key Findings Summary

1. **mxowrap.dll is a launcher.exe-specific wrapper**
   - Verifies parent process is launcher.exe
   - Applies runtime patches via Microsoft Detours
   - Refuses to load outside of launcher.exe

2. **Hex edit allows client.dll to load**
   - Replacing "mxowrap.dll" → "dbghelp.dll" in client.dll works
   - client.dll loads successfully
   - All dependencies resolve

3. **Crash occurs in dbghelp.dll**
   - Null pointer dereference
   - May be due to missing mxowrap.dll initialization
   - Needs further investigation

## Files Modified

| File | Change | Status |
|------|--------|--------|
| client.dll | Hex edit: "mxowrap.dll" → "dbghelp.dll" | ✓ Applied |
| launcher.exe | No changes | Using original |
| mxowrap.dll | Not used | Bypassed |

## Command to Run

```bash
cd ~/MxO_7.6005
wine launcher.exe -nopatch
```

---
*Test Date: March 10, 2025*
*Based on rajkosto's insight about mxowrap.dll verification*
