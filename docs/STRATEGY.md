# Strategy: Focus on Hex-Edited client.dll Path

## Decision

**Bypass mxowrap.dll entirely** by using hex-edited client.dll that loads dbghelp.dll directly.

## Rationale

1. **mxowrap.dll verification is complex** - checks many things beyond just process name
2. **We don't need mxowrap.dll** - its only purpose is runtime patching for updates
3. **Game is discontinued** - no more patches, so mxowrap.dll is unnecessary
4. **Hex-edit approach is 90% working** - just need to fix the dbghelp.dll crash

## Architecture Reality

```
launcher.exe/matrix.exe = Bootstrap loader (NOT the game)
client.dll = The actual game client (10.9 MB)
r3d9.dll = 3D rendering engine
Other DLLs = Game systems

mxowrap.dll = Runtime patcher (unnecessary for discontinued game)
```

## Implementation Plan

### Step 1: Accept Hex-Edit Approach
- Patch client.dll to load dbghelp.dll instead of mxowrap.dll
- This is a simple string replacement in the binary
- No need for mxowrap.dll's verification

### Step 2: Fix dbghelp.dll Crash
The crash at `mov (%ecx),%eax` (ECX=0) suggests:
- Missing initialization
- dbghelp.dll needs setup before use
- May need to call SymInitialize or similar

**Investigation needed**:
1. What is ECX supposed to point to?
2. Is this a missing function pointer?
3. Does dbghelp.dll need initialization?

### Step 3: Create Minimal Launcher
```cpp
// Simple launcher (no temp directory, no patching)
int main() {
    // Load dependencies
    LoadLibrary("MFC71.dll");
    LoadLibrary("MSVCR71.dll");

    // Load client.dll (hex-edited to use dbghelp.dll)
    HMODULE hClient = LoadLibrary("client.dll");

    // Get functions
    auto init = GetProcAddress(hClient, "InitClientDLL");
    auto run = GetProcAddress(hClient, "RunClientDLL");

    // Run game
    init(...);
    run();

    return 0;
}
```

## Why Not Reverse Engineer mxowrap.dll?

1. **Complex verification logic** - checking PE headers, exports, possibly signatures
2. **Time consuming** - could take days to fully understand
3. **Unnecessary** - we don't need mxowrap.dll for the game to run
4. **Hex-edit works** - already loads, just needs crash fixed

## Success Criteria

✅ client.dll loads (already works with hex-edit)
⬜ Fix dbghelp.dll crash (need to investigate)
⬜ Call InitClientDLL successfully
⬜ Call RunClientDLL to start game
⬜ Game window appears

## Next Immediate Steps

1. **Debug the dbghelp.dll crash** with GDB
   - Find out what ECX should point to
   - Check if dbghelp.dll needs SymInitialize call
   - Test with different dbghelp.dll versions

2. **Test simplified launcher**
   - Create minimal bootstrap that just loads client.dll
   - No mxowrap.dll import needed
   - Let hex-edit handle the dependency

---
*Date: March 10, 2025*
*Decision: Bypass mxowrap.dll, focus on fixing hex-edit approach*
