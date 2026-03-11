# Architecture Decision: Always Use -nopatch

## Date: 2025-03-11

---

## Background

The Matrix Online was **discontinued in 2009**. The official servers shut down, and no further patches exist.

---

## Why -nopatch Is Correct

### Official Launcher Behavior

The original launcher.exe had a patching system that:
1. Downloaded updates from `http://patch.lith.thematrixonline.net/`
2. Applied runtime patches via Microsoft Detours
3. Required `patchctl.dll`, `xpatcher.dll`, `mxowrap.dll`

### For Discontinued Games

**-nopatch mode** bypasses:
- Patch server communication (server is gone)
- Runtime patching (no patches to apply)
- Complex patch DLL dependencies (mxowrap.dll fails under Wine)

### Result

Simpler, more reliable path:
```
launcher.exe → client.dll → game runs
```

Instead of:
```
launcher.exe → patchctl.dll → mxowrap.dll → patches → client.dll → game
```

---

## Implementation

Our launcher **always uses -nopatch mode**: 

```c
// Default to nopatch - game discontinued, no patches available
g_NoPatch = true;
```

This is not a workaround - it's the **correct architecture** for discontinued MMOs.

---

## Benefits

1. **Simpler code path** - No patch system to emulate
2. **Fewer dependencies** - Don't need mxowrap.dll
3. **Wine compatible** - Avoids Wine-specific patch DLL issues
4. **Faster startup** - No patch checking/downloading
5. **Future proof** - Don't rely on dead servers

---

## Documentation

- `launcher.exe/NOPATCH_ARGUMENT.md` - Technical details
- `mxowrap.dll/` - Why we bypass the patch system
- `client.dll/` - Hex edit to remove mxowrap dependency

---

**Status**: -nopatch is the correct mode for this reimplementation.
