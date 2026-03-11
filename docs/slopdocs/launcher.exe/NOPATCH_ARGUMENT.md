# -nopatch Command Line Argument

## Date: 2025-03-11

---

## Location

**Address**: `0x409a52` in launcher.exe  
**String**: `-nopatch` at 0x4ac190

---

## Disassembly

```asm
0x00409a52: push str._nopatch    ; "-nopatch" at 0x4ac190
0x00409a57: call edi             ; String comparison/check
0x00409a59: add esp, 0x8
0x00409a5c: test eax, eax
0x00409a5e: jne 0x409c47         ; Jump if -nopatch found
```

---

## Behavior

When `-nopatch` is passed on command line:

### Effects:
1. **Skips patch initialization** - Doesn't load patchctl.dll system
2. **Sets internal flag** at 0x4c8b1d (moved value from "0.1" string)
3. **Calls 0x417440** - Likely sets up minimal mode
4. **Bypasses patch system** - No runtime patching

### Code Flow:
```
Found -nopatch:
  -> mov [0x4c8b1d], al    ; Set global flag
  -> call 0x417440          ; Initialize minimal mode
  -> Continue with InitClientDLL
```

---

## Why This Matters

### Without -nopatch:
- launcher.exe tries to initialize patching system
- Loads patchctl.dll, xpatcher.dll
- May fail if patching dependencies missing
- Complex initialization path

### With -nopatch:
- Bypasses all patching infrastructure
- Simpler initialization path  
- Just loads client.dll and runs it
- **Recommended for Wine/reimplementation**

---

## Recommendation

**Always use `-nopatch` for reimplementation:**

```bash
wine launcher.exe -nopatch
# or
./launcher_proper.exe -nopatch
```

This matches the documented solution from mxowrap_dll analysis.

---

## Related Strings in launcher.exe

| Address | String | Purpose |
|---------|--------|---------|
| 0x4ac190 | `-nopatch` | Disables patching |
| 0x4ac19c | `-silent` | Possibly suppress output |
| 0x4ac1a4 | `-clone` | Unknown |
| 0x4ac17c | `launcher.exe` | Process name reference |

---

**Status**: -nopatch flag documented
