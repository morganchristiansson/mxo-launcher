# OnDeleteCallback Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` and `../../client.dll`
**Analysis Date**: 2025-06-18

---

## Critical Findings

### 1. ❌ **FABRICATED** - Function Does Not Exist

The documentation describes an `OnDeleteCallback` notification callback that **does not exist** in either binary.

**Evidence**:
```bash
$ strings ../../launcher.exe | grep -i "OnDeleteCallback"
(no results)

$ strings ../../client.dll | grep -i "OnDeleteCallback"
(no results)

$ grep -i "OnDeleteCallback" /tmp/launcher_disasm.txt
(no matches)

$ grep -i "OnDeleteCallback" /tmp/client_disasm.txt
(no matches)
```

### 2. ❌ No Notification Callback for Deletion

The binaries contain **no callback notification mechanism** for callback deletion:
- ❌ No `OnDeleteCallback` registration
- ❌ No callback notification on deletion
- ❌ No user-provided deletion handlers
- ❌ No deletion event system

### 3. ✅ What DOES Exist

**Internal Callback Deletion Function** (not a callback itself):

Found in client.dll at address **0x622d3390**:

```assembly
622d3390: push   %ebp                    ; Function prologue
622d3391: mov    %esp,%ebp
622d3393: push   %ecx
622d3394: mov    0x629f33b4,%eax        ; Check debug flag
622d3399: test   %eax,%eax
622d339b: push   %esi
622d339c: push   %edi
622d339d: mov    0x8(%ebp),%edi         ; Get callback ID parameter
622d33a0: mov    %ecx,%esi              ; Save 'this' pointer
622d33a2: je     0x622d33c6             ; Skip logging if disabled
622d33a4: movzwl (%edi),%eax            ; Load callback ID (word)
622d33a7: push   %eax                   ; Push callback ID
622d33a8: push   $0x6289f21c            ; Push "Delete callback - ID %d"
622d33ad: lea    0xb(%ebp),%ecx
622d33b0: push   %ecx
622d33b1: lea    -0x1(%ebp),%edx
622d33b4: push   %edx
622d33b5: push   %esi
622d33b6: movb   $0xff,0xb(%ebp)
622d33ba: movb   $0x1,-0x1(%ebp)
622d33be: call   0x622cfa50             ; Log the deletion
622d33c3: add    $0x14,%esp
622d33c6: push   %edi                   ; Push callback ID
622d33c7: lea    0x4c(%esi),%ecx        ; Get callback table
622d33ca: call   0x6201ec00             ; Delete from table
622d33cf: pop    %edi
622d33d0: pop    %esi
622d33d1: mov    %ebp,%esp
622d33d3: pop    %ebp
622d33d4: ret    $0x4                   ; Return, clean 4 bytes
```

**Key Points**:
- Function signature: `void DeleteCallbackFunc(void* this, uint16_t callbackId)`
- Logs "Delete callback - ID %d" for debugging
- Removes callback from internal table
- NO notification to user code
- Purely internal implementation

---

## Documentation Claims vs Reality

### Claimed Function Signature

```c
void OnDeleteCallback(uint32_t callbackId, uint32_t reason);
```

**Status**: ❌ **FABRICATED** - No such callback exists

### Claimed Usage

```c
// ❌ FABRICATED
void OnMyCallbackDeleted(uint32_t callbackId, uint32_t reason) {
    printf("Callback %d deleted (reason: %d)\n", callbackId, reason);
}
```

**Status**: ❌ **FABRICATED** - No registration mechanism exists

### Claimed Deletion Reasons

```c
#define DELETE_REASON_NORMAL     0
#define DELETE_REASON_TIMEOUT    1
#define DELETE_REASON_ERROR      2
...
```

**Status**: ❌ **FABRICATED** - No such constants exist

---

## What Actually Exists

### Internal Callback Management

The game has an **internal callback deletion function**, not a notification callback:

```cpp
// Actual implementation (simplified)
class CallbackManager {
    void DeleteCallback(uint16_t callbackId) {
        // Debug logging
        if (debug_enabled) {
            Log("Delete callback - ID %d", callbackId);
        }

        // Remove from table
        callbackTable->Remove(callbackId);

        // NO notification to user
        // NO callback invocation
        // Just internal cleanup
    }
};
```

### No User Notification

When callbacks are deleted:
- ❌ No event fired
- ❌ No notification sent
- ❌ No user handler called
- ✅ Silent internal cleanup only

---

## Misinterpretation Analysis

### How the Fabrication Happened

1. **Found string**: "Delete callback - ID %d" at 0x6289f21c
2. **Incorrect assumption**: "This must be a notification callback"
3. **Fabricated details**:
   - Created `OnDeleteCallback` function
   - Invented parameter signature
   - Made up deletion reason codes
   - Documented as if it exists

### Reality

The string is used for **internal logging only**:
- Debug/diagnostic message
- Printed when callback is deleted
- NOT part of a callback notification system

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnDeleteCallback` notification | Does not exist | ❌ **FABRICATED** |
| Function Type | User callback | Internal function | ❌ **WRONG TYPE** |
| Parameters | `(uint32_t id, uint32_t reason)` | Internal only | ❌ **FABRICATED** |
| Return Value | `void` | N/A | ❌ **N/A** |
| Registration | Via callback system | No registration | ❌ **FABRICATED** |
| Deletion Reasons | 5 defined constants | None exist | ❌ **FABRICATED** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Binary Evidence

### String Search Results

```bash
# Search for OnDeleteCallback
$ strings ../../launcher.exe | grep -i "OnDeleteCallback"
(no results)

$ strings ../../client.dll | grep -i "OnDeleteCallback"
(no results)

# What was actually found
$ strings ../../client.dll | grep -i "Delete callback"
Delete callback - ID %d
Delete callback for distribute monitor id %d

# These are diagnostic strings, not callback names
```

### Disassembly Analysis

```bash
# Function that uses "Delete callback - ID %d"
$ grep -B30 -A30 "push.*0x6289f21c" /tmp/client_disasm.txt

# Shows internal deletion function at 0x622d3390
# NO callback invocation found
# NO user notification mechanism
```

### VTable Analysis

```bash
# Check vtable[31] at offset 0x7c (called after deletion)
$ grep -o "call.*\*0x7c(" /tmp/client_disasm.txt | wc -l
170

# Called 170 times - common internal method
# Not a special deletion notification
# Likely just internal cleanup/state update
```

---

## Comparison: Expected vs. Actual

### Expected (Fabricated)

```c
// ❌ FABRICATED - Does NOT exist
void OnDeleteCallback(uint32_t callbackId, uint32_t reason) {
    // User handles deletion notification
    CleanupResources(callbackId);
}

// Registration
RegisterDeleteHandler(OnDeleteCallback);
```

### Actual (What Really Happens)

```cpp
// ✅ ACTUAL - Internal function
class CallbackManager {
    void DeleteCallback(uint16_t callbackId) {
        // Debug log
        if (g_debug) {
            printf("Delete callback - ID %d\n", callbackId);
        }

        // Internal cleanup
        m_callbacks.Remove(callbackId);

        // NO notification
        // NO callback
        // Silent cleanup
    }
};
```

---

## Related Functions

### Real Functions Found

| Function | Address | Purpose |
|----------|---------|---------|
| `DeleteCallbackFunc` | 0x622d3390 | Internal deletion |
| `DeleteDistributeMonitorCallback` | Related | Monitor deletion |

**Note**: These are **internal implementations**, not user callbacks.

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnDeleteCallback.md` - Mark as fabricated
2. ✅ **Document** internal callback management (if needed)
3. ✅ **Explain** that deletion is silent (no notification)

### Documentation Corrections

1. Remove fabricated callback documentation
2. Document actual callback system (registration/unregistration)
3. Explain that callbacks are silently removed
4. No notification mechanism exists

---

## Validation Commands

For future validation:

```bash
# Check for callback name
strings ../../launcher.exe | grep -i "CallbackName"
strings ../../client.dll | grep -i "CallbackName"

# Check for registration mechanism
grep -i "Register.*Callback" /tmp/launcher_disasm.txt

# Verify function exists
grep -i "FunctionName" /tmp/launcher_disasm.txt

# Check if it's internal or callback
# Look for vtable calls, registration APIs, etc.
```

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnDeleteCallback` notification callback **does not exist**. The documentation was fabricated based on:
1. Finding the diagnostic string "Delete callback - ID %d"
2. Misinterpreting it as a callback notification
3. Creating fabricated details around the assumption
4. No validation against actual binary behavior

### Corrective Action

This file should be **deprecated** and replaced with:
1. Warning that the callback does not exist
2. Explanation of internal callback management
3. Note that deletion is silent (no notification)

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-06-18
