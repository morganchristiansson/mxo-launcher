# OnException

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-06-18
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in either launcher.exe or client.dll.
>
> **Verification**:
> ```bash
> strings ../../launcher.exe | grep -i "OnException"     # No results
> strings ../../client.dll | grep -i "OnException"      # No results
> grep -i "OnException" /tmp/launcher_disasm.txt        # No matches
> ```
>
> See [OnException_validation.md](OnException_validation.md) for detailed validation report.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of both `launcher.exe` and `client.dll` confirms:

```bash
# String search - no callback name found
$ strings ../../launcher.exe | grep -i "OnException"
(no results)

$ strings ../../client.dll | grep -i "OnException"
(no results)

# Only found error message and field name
$ strings ../../client.dll | grep -i "exception callback"
Could not load exception callback "

$ strings ../../client.dll | grep "exceptionCB"
.exceptionCB
```

### ❌ No Exception Notification System

The binary contains **no callback notification mechanism** for exceptions:
- ❌ No `OnException` registration API
- ❌ No exception event callbacks
- ❌ No user-provided exception handlers
- ❌ No exception notification to client.dll

### ✅ What Actually Exists

**Internal Exception Handling Setup**:

Found in client.dll at address **0x62699b60**:

```assembly
; Exception handling initialization function
62699b60: push   %ebp
62699b61: mov    %esp,%ebp
62699b63: sub    $0x130,%esp         ; Large stack allocation
62699b6a: mov    %ecx,%ebx           ; Save 'this' pointer
62699b6c: cmpb   $0x0,(%ebx)         ; Check initialization
...
62699d14: push   $0x628f17d4         ; "Could not load exception callback"
62699d1c: call   0x62002060          ; Display error
```

**Purpose**: This function **initializes internal exception handling**, not a callback notification system.

**Key Points**:
- Sets up Windows SEH (Structured Exception Handling)
- Loads exception handling resources
- Initializes internal `.exceptionCB` field
- **NO callback invocation**
- **NO user notification**

---

## Original Documentation (FABRICATED)

<details>
<summary>Click to view original fabricated documentation</summary>

## Overview

**Category**: Lifecycle
**Direction**: Launcher → Client
**Purpose**: Exception/error callback notification

---

## Function Signature

```c
void OnException(void* exceptionData, uint32_t exceptionCode, uint32_t flags);
```

**Note**: This was fabricated. No such callback exists.

### Parameters (FABRICATED)

| Type | Name | Purpose |
|------|------|---------|
| `void*` | exceptionData | Exception information structure |
| `uint32_t` | exceptionCode | Exception code |
| `uint32_t` | flags | Exception flags and severity |

### Exception Codes (MISUSED)

The documentation incorrectly used Windows SEH constants as callback parameters:
- `EXCEPTION_ACCESS_VIOLATION`
- `EXCEPTION_STACK_OVERFLOW`
- etc.

**These are Windows OS exception codes, not game callback parameters.**

</details>

---

## Correct Information

### Actual Exception Handling

The game uses **Windows Structured Exception Handling (SEH)** internally:

**Standard Windows SEH**:
```cpp
// Internal crash handler
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    // Log crash information
    LogCrash(ExceptionInfo);

    // Display error
    ShowErrorDialog();

    // NO callback to user code
    return EXCEPTION_CONTINUE_SEARCH;
}

// Initialization
SetUnhandledExceptionFilter(CrashHandler);
```

**Key Points**:
- Uses standard Windows SEH APIs
- Internal crash logging and reporting
- No user notification system
- Not exposed via game API

### When Errors Occur

- **Crash/Exception**: Windows SEH handles it
- **Internal Error**: Logged by game systems
- **NO Notification**: User code is not notified
- **Silent Handling**: All internal, no callbacks

---

## Fabrication Pattern

This callback was fabricated based on:

1. **Found string**: "Could not load exception callback"
2. **Found field name**: ".exceptionCB"
3. **Incorrect inference**: "Must be a notification callback"
4. **Fabricated details**:
   - Function signature
   - Parameter types
   - Exception codes (borrowed from Windows SEH)
   - Exception data structure

**Pattern**: Error string + field name → Must be callback → **WRONG**

**Reality**: Internal initialization error and state field.

---

## Related Files

**Also Fabricated**:
- ❌ [OnPlayerJoin.md](../game/OnPlayerJoin.md) - Does not exist
- ❌ [OnPlayerLeave.md](../game/OnPlayerLeave.md) - Does not exist
- ❌ [OnPlayerUpdate.md](../game/OnPlayerUpdate.md) - Does not exist
- ❌ [OnLogout.md](../game/OnLogout.md) - Does not exist
- ❌ [OnDeleteCallback.md](../monitor/OnDeleteCallback.md) - Does not exist
- ❌ [OnException.md](OnException.md) - This file

**Valid Documentation**:
- ✅ [OnPacket.md](../network/OnPacket.md) - Real callback
- ✅ [OnLoginEvent.md](../game/OnLoginEvent.md) - Real C++ observer
- ✅ [OnLoginError.md](../game/OnLoginError.md) - Real C++ observer

**Validation Reports**:
- ✅ [OnException_validation.md](OnException_validation.md) - Detailed analysis
- ✅ [PLAYER_CALLBACKS_VALIDATION.md](../game/PLAYER_CALLBACKS_VALIDATION.md) - Related validation

---

## References

- **Validation Report**: [OnException_validation.md](OnException_validation.md)
- **Binary**: `../../launcher.exe` and `../../client.dll`
- **Analysis Method**: String search, disassembly analysis
- **Status**: Function does not exist

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Last Updated**: 2025-06-18
**Validator**: Binary Analysis
**Action**: Do not use

---

**See Also**: [VALIDATION_SUMMARY.md](VALIDATION_SUMMARY.md) for complete validation results.
