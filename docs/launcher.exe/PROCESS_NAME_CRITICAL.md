# Process Name is CRITICAL

## Discovery: 2025-03-11

---

## Problem

When running `launcher_proper.exe`:
- ✅ InitClientDLL returned 0 (success)
- ❌ SetMasterDatabase was NOT called back
- ❌ Client DB is NULL
- ❌ RunClientDLL crashed

---

## Hypothesis

**client.dll may verify the hosting process name**

Evidence:
```
Architecture: launcher_proper.exe
InitClientDLL returned: 0 (success!)
Client DB received: 00000000 (NULL - no callback)
```

client.dll likely:
1. Gets parent process name via `GetModuleHandle(NULL)` or `GetProcessImageFileName`
2. Checks if it's "launcher.exe"
3. Only calls SetMasterDatabase if hosted by expected launcher

---

## Test Required

Rename or rebuild as "launcher.exe":

```bash
# Rename test
mv launcher_proper.exe launcher.exe
wine launcher.exe
```

Also check strings in client.dll:
```bash
strings client.dll | grep -i "launcher\\.exe"
```

---

## Related Strings Found

In launcher.exe:
- `"ErrorClientDLL"` - Error when client fails
- `"TermClientDLL"` - Graceful termination
- `"The Matrix Online client crashed"` - Crash message

These suggest process-level checks exist.

---

## Next Steps

1. **Search strings**: `strings client.dll | grep -i launcher`
2. **Check imports**: Look for process name functions
3. **Rename test**: Try running as "launcher.exe"
4. **Analyze InitClientDLL**: Disassemble InitClientDLL entry point

---

**Status**: SetMasterDatabase export confirmed working, but client.dll doesn't call it when hosted by different process name
