# OnGameState Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Function Does Not Exist

The documentation describes an `OnGameState` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnGameState"
(no results)

$ strings launcher.exe | grep -i "GameState"
(no results)
```

### 2. **No Game State Callbacks Exist**

The binary contains **no game state event callbacks** of any kind:
- ❌ No `OnGameState`
- ❌ No `GameStateEvent`
- ❌ No game state change notifications
- ❌ No `EVENT_GAME_STATE` constant

### 3. **What Actually Exists: AI State**

The only "state" related functionality found is:

```
AIState
```

This appears to be for AI behavior, not game state callbacks.

### 4. **Session Management Exists**

The binary does have session-related functionality, but it's not callback-based:

```
LTMS_UNKNOWNSESSION
LTMS_SESSIONESTABLISHINPROGRESS
SESSION_TYPE_EQ2_KOREA
SESSION_TYPE_EQ2_CHINA
SESSION_TYPE_MATRIX_ONLINE
```

These are session types and status codes, not callbacks.

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnGameState` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | C callback function | N/A | ❌ **N/A** |
| Parameters | `GameStateEvent*` structure | N/A | ❌ **N/A** |
| Return Value | `int` (0, -1, 1) | N/A | ❌ **N/A** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Fabrication Pattern

This callback follows the **same fabricated pattern** as all other game callbacks:
- Same C callback signature pattern
- Similar structure definitions
- Same "ProcessEvent vtable index 6, offset 0x18" reference
- "Medium confidence - inferred from game callback structure"
- No binary validation performed

All game callbacks appear to have been created by filling in a template without any binary analysis.

---

## What Actually Exists

### Session Types (Not Callbacks)

```c
// Session type constants (server-side configuration)
#define SESSION_TYPE_MATRIX_ONLINE
#define SESSION_TYPE_EVERQUEST
#define SESSION_TYPE_SWG_JP_BETA
// etc.
```

### World Events (Commands, Not Callbacks)

```c
// World event commands
void TriggerWorldEvent(int eventId);
void UntriggerWorldEvent(int eventId);
void ListWorldEvents();
```

### Login Events (Observer Pattern, Not C Callbacks)

```cpp
// C++ observer pattern for login events
class CLTLoginObserver_PassThrough {
    void OnLoginEvent(int eventNumber, const char* serverResult);
    void OnLoginError(int errorNumber);
};
```

---

## Binary Evidence

```bash
# Search for game state
$ strings launcher.exe | grep -i "game" | grep -i "state"
(no results)

# Search for OnGameState
$ strings launcher.exe | grep -i "OnGameState"
(no results)

# Search for state-related functionality
$ strings launcher.exe | grep -i "state" | grep -v "^0x"
AIState
```

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**  
**Action Required**: **DEPRECATE IMMEDIATELY**
