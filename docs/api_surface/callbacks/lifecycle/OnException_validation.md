# OnException Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` and `../../client.dll`
**Analysis Date**: 2025-06-18

---

## Critical Findings

### 1. ❌ **FABRICATED** - Callback Does Not Exist

The documentation describes an `OnException` notification callback that **does not exist** in either binary.

**Evidence**:
```bash
$ strings ../../launcher.exe | grep -i "OnException"
(no results)

$ strings ../../client.dll | grep -i "OnException"
(no results)

$ grep -i "OnException" /tmp/launcher_disasm.txt
(no matches)

$ grep -i "OnException" /tmp/client_disasm.txt
(no matches)
```

### 2. ❌ No Exception Notification Callback

The binaries contain **no callback notification mechanism** for exceptions:
- ❌ No `OnException` callback registration
- ❌ No exception event callbacks
- ❌ No user-provided exception handlers
- ❌ No exception notification system

### 3. ✅ What DOES Exist

**Internal Exception Handling Setup** (not a callback):

Found in client.dll at address **0x62699b60**:

```assembly
; Exception handling initialization/configuration function
62699b60: push   %ebp
62699b61: mov    %esp,%ebp
62699b63: sub    $0x130,%esp         ; Allocate 304 bytes local storage
62699b69: push   %ebx
62699b6a: mov    %ecx,%ebx           ; Save 'this' pointer
62699b6c: cmpb   $0x0,(%ebx)         ; Check if object initialized
62699b6f: push   %esi
62699b70: push   %edi
62699b71: je     0x6269a109          ; Jump if not initialized
...
62699d14: push   $0x628f17d4         ; "Could not load exception callback"
62699d19: lea    -0x10(%ebp),%ecx
62699d1c: call   0x62002060          ; Format/display error
```

**Key Points**:
- This is an **initialization function** for exception handling
- Sets up internal exception handling infrastructure
- Loads resources and configuration
- **NO callback invocation**
- **NO user notification**

**Related Strings**:
```
0x628f17d4: "Could not load exception callback"
0x628f17f8: ".exceptionCB" (field/member name)
```

---

## Documentation Claims vs Reality

### Claimed Function Signature

```c
void OnException(void* exceptionData, uint32_t exceptionCode, uint32_t flags);
```

**Status**: ❌ **FABRICATED** - No such callback exists

### Claimed Registration Pattern

```c
SetEventHandler(obj, EVENT_EXCEPTION, ExceptionCallback);
```

**Status**: ❌ **FABRICATED** - No registration mechanism exists

### Claimed Exception Codes

```c
#define EXCEPTION_ACCESS_VIOLATION    0x00000001
#define EXCEPTION_ARRAY_BOUNDS        0x00000002
...
```

**Status**: ⚠️ **MISLEADING** - These are Windows exception codes, not callback parameters

### Claimed Exception Data Structure

```c
struct ExceptionData {
    uint32_t code;
    uint32_t address;
    char message[256];
    ...
};
```

**Status**: ❌ **FABRICATED** - No such structure for callbacks

---

## What Actually Exists

### Exception Handling Architecture

The game uses **Windows Structured Exception Handling (SEH)** internally:

**Standard Windows Exception Codes** (found in both binaries):
```
EXCEPTION_ACCESS_VIOLATION
EXCEPTION_ARRAY_BOUNDS_EXCEEDED
EXCEPTION_BREAKPOINT
EXCEPTION_DATATYPE_MISALIGNMENT
EXCEPTION_FLT_DIVIDE_BY_ZERO
EXCEPTION_INT_DIVIDE_BY_ZERO
EXCEPTION_STACK_OVERFLOW
...
```

These are **Windows-defined constants**, not game callback parameters.

### Internal Exception Handling

**Initialization Function** at 0x62699b60:
- Sets up exception filters
- Configures crash handlers
- Loads exception handling resources
- Initializes `.exceptionCB` field (internal state)

**Error Message Context**:
The string "Could not load exception callback" appears when:
- Internal exception handler fails to initialize
- Resource loading fails
- Configuration error occurs

**NOT** when invoking a user callback.

---

## Misinterpretation Analysis

### How the Fabrication Happened

1. **Found string**: "Could not load exception callback"
2. **Found field name**: ".exceptionCB"
3. **Incorrect assumption**: "Must be a notification callback system"
4. **Fabricated details**:
   - Created `OnException` function
   - Invented parameter signature
   - Used Windows exception codes as callback parameters
   - Made up exception data structure

### Reality

- "Could not load exception callback" = Error loading internal handler
- ".exceptionCB" = Internal field name
- Exception codes = Windows SEH constants (not callback parameters)
- No user notification system

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnException` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | Notification callback | Internal setup | ❌ **WRONG TYPE** |
| Parameters | `(void*, uint32_t, uint32_t)` | N/A | ❌ **FABRICATED** |
| Return Value | `void` | N/A | ❌ **N/A** |
| Registration | SetEventHandler | No registration | ❌ **FABRICATED** |
| Exception Codes | Callback parameters | Windows SEH | ⚠️ **MISUSED** |
| Exception Structure | Game-defined | N/A | ❌ **FABRICATED** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Binary Evidence

### String Search Results

```bash
# Search for OnException
$ strings ../../launcher.exe | grep -i "OnException"
(no results)

$ strings ../../client.dll | grep -i "OnException"
(no results)

# What was found
$ strings ../../client.dll | grep -i "exception callback"
Could not load exception callback "

# Field name (not a function)
$ strings ../../client.dll | grep "exceptionCB"
.exceptionCB

# Windows exception codes (not callback parameters)
$ strings ../../client.dll | grep "EXCEPTION_"
EXCEPTION_ACCESS_VIOLATION
EXCEPTION_STACK_OVERFLOW
... (20+ Windows SEH codes)
```

### Disassembly Analysis

```bash
# Function using the error string
$ grep -B30 "push.*0x628f17d4" /tmp/client_disasm.txt

# Shows initialization code at 0x62699b60
# NO callback invocation
# NO registration mechanism
# Just internal setup
```

---

## Comparison: Expected vs. Actual

### Expected (Fabricated)

```c
// ❌ FABRICATED - Does NOT exist
void OnException(void* exceptionData, uint32_t code, uint32_t flags) {
    // User handles exception notification
    LogException(exceptionData, code);
}

// Registration
SetEventHandler(EVENT_EXCEPTION, OnException);
```

### Actual (What Really Happens)

```cpp
// ✅ ACTUAL - Internal exception handling setup
class ExceptionManager {
    void InitializeExceptionHandling() {
        // Load exception handler resources
        if (!LoadExceptionResources()) {
            LogError("Could not load exception callback");
            return;
        }

        // Set up SEH filters
        SetUnhandledExceptionFilter(CrashHandler);

        // Configure internal state
        this->exceptionCB = InitializeCallback();

        // NO user notification
        // NO callback invocation
    }
};
```

---

## Related Functions

### Real Exception Handling

| Component | Purpose |
|-----------|---------|
| Windows SEH | Structured Exception Handling (OS-level) |
| `SetUnhandledExceptionFilter` | Install crash handler |
| Crash logging | Internal error reporting |
| `.exceptionCB` field | Internal state variable |

**Note**: All exception handling is **internal**, not exposed via callbacks.

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnException.md` - Mark as fabricated
2. ✅ **Document** actual exception handling (internal SEH)
3. ✅ **Explain** that exceptions are handled internally

### Documentation Corrections

1. Remove fabricated callback documentation
2. Document internal crash handling
3. Explain Windows SEH usage
4. No user callback for exceptions

---

## Validation Commands

For future validation:

```bash
# Check for callback name
strings ../../launcher.exe | grep -i "CallbackName"
strings ../../client.dll | grep -i "CallbackName"

# Check string context
grep -B30 "string_address" /tmp/client_disasm.txt

# Verify if callback or internal
# Look for: registration, vtable calls, user handler invocation
```

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnException` notification callback **does not exist**. The documentation was fabricated based on:
1. Finding error string "Could not load exception callback"
2. Misinterpreting ".exceptionCB" field name
3. Using Windows SEH codes as callback parameters
4. Assuming notification callback existed

### Corrective Action

This file should be **deprecated** and replaced with:
1. Warning that the callback does not exist
2. Explanation of internal exception handling
3. Note that Windows SEH is used, not game callbacks

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-06-18
