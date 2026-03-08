# OnWorldUpdate Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Function Does Not Exist

The documentation describes an `OnWorldUpdate` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnWorldUpdate"
(no results)

$ strings launcher.exe | grep -i "WorldUpdate"
(no results)
```

### 2. **What Actually Exists: World Events System**

The binary does have a **world events system**, but it's not a callback pattern:

| Function | Type | Purpose |
|----------|------|---------|
| `TriggerWorldEvent` | Command | Trigger a world event |
| `UntriggerWorldEvent` | Command | Untrigger a world event |
| `ListWorldEvents` | Command | List all world events |

These are **commands**, not event callbacks.

### 3. **World Status Management Exists**

The binary contains world **status management** functionality:

```
WorldStatusSeq:
AS_WorldIdAndStatus
AS_GetWorldListReply
AS_GetWorldListRequest
AS_SetWorldStatusReply
AS_SetWorldStatusRequest
AS_WorldShuttingDown
AS_PSGetWorldPopulationsReply
AS_PSGetWorldPopulationsRequest
```

This is for **admin/world management**, not player callbacks.

### 4. **World Status Constants**

The binary defines world status constants:
- `LTAS_INVALIDWORLDSTATUS`
- `LTAS_INVALIDWORLDID`
- `LTAS_WORLDSTATUSIGNORED`
- `LTAS_YOUOWNCHARONOTHERWORLD`

These are used for status checking, not callbacks.

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnWorldUpdate` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | C callback function | N/A | ❌ **N/A** |
| Parameters | `WorldUpdateEvent*` structure | N/A | ❌ **N/A** |
| Return Value | `int` (0, -1, 1) | N/A | ❌ **N/A** |
| World Event System | Callback pattern | Command pattern | ❌ **WRONG** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## What Actually Exists

### World Event Commands

```c
// Commands to manage world events (not callbacks)
void TriggerWorldEvent(int eventId);
void UntriggerWorldEvent(int eventId);
void ListWorldEvents();
```

### World Status Management

```c
// World status management (admin/server-side)
AS_SetWorldStatus(int worldId, int status);
AS_GetWorldList();
AS_WorldShuttingDown(int worldId);
```

### No World Update Callbacks

- ❌ No `OnWorldUpdate` callback
- ❌ No `WorldUpdateEvent` structure
- ❌ No world update event registration
- ❌ No `EVENT_WORLD_UPDATE` constant

---

## Binary Evidence

```bash
# Search for world-related functions
$ strings launcher.exe | grep -i "world" | grep -E "(trigger|event|status)"
TriggerWorldEvent
UntriggerWorldEvent
ListWorldEvents
AS_SetWorldStatus
AS_GetWorldList
AS_WorldShuttingDown

# Search for OnWorldUpdate
$ strings launcher.exe | grep -i "OnWorldUpdate"
(no results)
```

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**  
**Action Required**: **DEPRECATE IMMEDIATELY**
