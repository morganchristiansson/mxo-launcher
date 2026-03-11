# mxowrap.dll Process Name Verification

## Discovery

**mxowrap.dll checks the process name and only loads for "launcher.exe" or "matrix.exe"**.

## Evidence

1. **launcher.exe successfully loads mxowrap.dll** via import table
2. **resurrections.exe fails to load mxowrap.dll** even via import table with same exports
3. **Error code 1114** (DllMain returned FALSE)

## Solution

Rename `resurrections.exe` to `launcher.exe` to pass mxowrap.dll's verification.

### Test Results

```
✓ launcher.exe (original) loads mxowrap.dll successfully
✗ resurrections.exe fails to load mxowrap.dll
? launcher.exe (renamed resurrections.exe) - needs testing
```

## Implementation

```bash
cd ~/MxO_7.6005
cp resurrections.exe launcher.exe
wine launcher.exe
```

## Why This Matters

mxowrap.dll is a critical component that:
1. Wraps dbghelp.dll for crash dumping
2. Applies runtime patches via Microsoft Detours
3. Hooks Windows APIs for compatibility
4. Verifies parent process is the official launcher

The verification ensures mxowrap.dll only loads in the intended environment.

## Alternative: Patch mxowrap.dll

If renaming isn't viable, we can patch mxowrap.dll to skip the verification:

```python
# Find and patch the process name check in mxowrap.dll
# Location: function 0x1001f338
# Patch: Always return TRUE from the verification function
```

---
*Date: March 10, 2025*
*Status: Solution identified, needs testing*
