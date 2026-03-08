# OnDeleteCallback

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-06-18
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in either launcher.exe or client.dll.
>
> **Verification**:
> ```bash
> strings ../../launcher.exe | grep -i "OnDeleteCallback"     # No results
> strings ../../client.dll | grep -i "OnDeleteCallback"      # No results
> grep -i "OnDeleteCallback" /tmp/launcher_disasm.txt        # No matches
> ```
>
> See [OnDeleteCallback_validation.md](OnDeleteCallback_validation.md) for detailed validation report.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of both `launcher.exe` and `client.dll` confirms:

```bash
# String search - no callback name found
$ strings ../../launcher.exe | grep -i "OnDeleteCallback"
(no results)

$ strings ../../client.dll | grep -i "OnDeleteCallback"
(no results)

# Only found diagnostic string (not a callback)
$ strings ../../client.dll | grep -i "Delete callback"
Delete callback - ID %d
Delete callback for distribute monitor id %d

# These are internal log messages, not callback names
```

### ❌ No Notification Mechanism Exists

The binary contains **no callback notification system** for deletions:
- ❌ No `OnDeleteCallback` registration API
- ❌ No deletion event callbacks
- ❌ No user-provided handlers
- ❌ No deletion reason codes

### ✅ What Actually Exists

**Internal Callback Deletion Function** (not a callback):

Found in client.dll at address **0x622d3390**:

```assembly
; Internal function that deletes callbacks
622d3390: push   %ebp
622d3391: mov    %esp,%ebp
...
622d33a4: movzwl (%edi),%eax        ; Get callback ID
622d33a7: push   %eax               ; Push for logging
622d33a8: push   $0x6289f21c        ; "Delete callback - ID %d"
622d33be: call   0x622cfa50         ; Log message
...
622d33c7: lea    0x4c(%esi),%ecx    ; Get callback table
622d33ca: call   0x6201ec00         ; Remove from table
622d33d4: ret    $0x4               ; Clean 4 bytes
```

**Key Points**:
- This is an **internal implementation** function
- Logs deletion for debugging purposes
- Removes callback from internal table
- **NO notification** to user code
- Silent cleanup only

---

## Original Documentation (FABRICATED)

<details>
<summary>Click to view original fabricated documentation</summary>

## Overview

**Category**: Monitor/System
**Direction**: Launcher → Client
**Purpose**: Callback deletion notification

---

## Function Signature

```c
void OnDeleteCallback(uint32_t callbackId, uint32_t reason);
```

**Note**: This was fabricated. No such callback exists.

### Parameters (FABRICATED)

| Type | Name | Purpose |
|------|------|---------|
| `uint32_t` | callbackId | ID of deleted callback |
| `uint32_t` | reason | Deletion reason code |

### Deletion Reasons (FABRICATED)

| Code | Name | Description |
|------|------|-------------|
| 0 | DELETE_REASON_NORMAL | Normal unregistration |
| 1 | DELETE_REASON_TIMEOUT | Callback timed out |
| 2 | DELETE_REASON_ERROR | Error condition |
| 3 | DELETE_REASON_SHUTDOWN | System shutdown |
| 4 | DELETE_REASON_REPLACED | Replaced by new callback |

**Note**: These constants do not exist in the binary.

</details>

---

## Correct Information

### Actual Callback System

The game's callback system works as follows:

1. **Registration**: Callbacks are registered via vtable methods
2. **Usage**: Callbacks are invoked during game operation
3. **Deletion**: Callbacks are removed silently (no notification)

**No deletion notification exists** because:
- Not needed for game operation
- Callbacks are internal implementation
- Silent cleanup is sufficient

### When Callbacks Are Deleted

Callbacks are removed internally without notification when:
- Explicitly unregistered
- Object destroyed
- System shutdown
- Error conditions

**User code is NOT notified** of these events.

---

## Fabrication Pattern

This callback was fabricated based on:

1. **Found string**: "Delete callback - ID %d"
2. **Incorrect inference**: "Must be a notification callback"
3. **Fabricated details**:
   - Function signature
   - Parameter types
   - Deletion reason codes
   - Usage examples

**Pattern**: String exists → Must be callback → **WRONG**

**Reality**: String is just a debug log message.

---

## Related Files

**Also Fabricated**:
- ❌ [OnPlayerJoin.md](../game/OnPlayerJoin.md) - Does not exist
- ❌ [OnPlayerLeave.md](../game/OnPlayerLeave.md) - Does not exist
- ❌ [OnPlayerUpdate.md](../game/OnPlayerUpdate.md) - Does not exist
- ❌ [OnLogout.md](../game/OnLogout.md) - Does not exist
- ❌ [OnDeleteCallback.md](OnDeleteCallback.md) - This file

**Valid Documentation**:
- ✅ [OnPacket.md](../network/OnPacket.md) - Real callback
- ✅ [OnLoginEvent.md](../game/OnLoginEvent.md) - Real C++ observer
- ✅ [OnLoginError.md](../game/OnLoginError.md) - Real C++ observer

**Validation Reports**:
- ✅ [OnDeleteCallback_validation.md](OnDeleteCallback_validation.md) - Detailed analysis
- ✅ [PLAYER_CALLBACKS_VALIDATION.md](../game/PLAYER_CALLBACKS_VALIDATION.md) - Related validation

---

## References

- **Validation Report**: [OnDeleteCallback_validation.md](OnDeleteCallback_validation.md)
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
