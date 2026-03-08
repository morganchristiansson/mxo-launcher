# OnWorldUpdate

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
> 
> **Validation Date**: 2025-03-08
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
> 
> This callback was fabricated and does not exist in the launcher binary.
> 
> See [OnWorldUpdate_validation.md](OnWorldUpdate_validation.md) for detailed disassembly analysis.
> 
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnWorldUpdate"
(no results)

$ strings launcher.exe | grep -i "WorldUpdate"
(no results)
```

### ✅ What Actually Exists: World Event Commands

The binary contains **world event commands** (not callbacks):

| Command | Purpose |
|---------|---------|
| `TriggerWorldEvent` | Trigger a world event |
| `UntriggerWorldEvent` | Untrigger a world event |
| `ListWorldEvents` | List all world events |

These are **admin/server commands**, not event callbacks.

### ✅ World Status Management

The binary also contains world status management:
- `AS_SetWorldStatus` - Set world status
- `AS_GetWorldList` - Get world list
- `AS_WorldShuttingDown` - World shutdown notification

These are **server-side management functions**, not callbacks.

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**  
**Last Updated**: 2025-03-08  
**Validator**: Binary Analysis  
**Action**: Do not use
