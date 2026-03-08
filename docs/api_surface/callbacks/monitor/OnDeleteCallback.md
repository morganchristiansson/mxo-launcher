# OnDeleteCallback

## Overview

**Category**: Monitor/System  
**Direction**: Launcher → Client  
**Purpose**: Callback deletion notification

---

## Function Signature

```c
void OnDeleteCallback(uint32_t callbackId, uint32_t reason);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `uint32_t` | callbackId | ID of deleted callback |
| `uint32_t` | reason | Deletion reason code |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Diagnostic Strings

Found in client.dll:

| String | Address | Context |
|--------|---------|---------|
| "Delete callback - ID %d\n" | 0x6289f21c | General callback cleanup |

---

## Usage

### When Called

This callback is invoked when:
- Callback is explicitly unregistered
- Callback lifetime expires
- System cleanup occurs
- Error condition requires removal

---

## Implementation

### Launcher Side

```c
void DeleteCallback(uint32_t callbackId, uint32_t reason) {
    printf("Delete callback - ID %d\n", callbackId);
    
    CallbackEntry* entry = FindCallback(callbackId);
    if (entry) {
        // Notify client if delete callback registered
        if (entry->onDeleteCallback) {
            entry->onDeleteCallback(callbackId, reason);
        }
        
        // Remove from table
        RemoveCallback(callbackId);
    }
}
```

### Client Side

```c
// Callback deletion handler
void OnMyCallbackDeleted(uint32_t callbackId, uint32_t reason) {
    printf("Callback %d deleted (reason: %d)\n", callbackId, reason);
    
    // Cleanup any resources
    CleanupCallbackResources(callbackId);
}
```

---

## Deletion Reasons

| Code | Name | Description |
|------|------|-------------|
| 0 | DELETE_REASON_NORMAL | Normal unregistration |
| 1 | DELETE_REASON_TIMEOUT | Callback timed out |
| 2 | DELETE_REASON_ERROR | Error condition |
| 3 | DELETE_REASON_SHUTDOWN | System shutdown |
| 4 | DELETE_REASON_REPLACED | Replaced by new callback |

---

## Notes

- **Cleanup notification callback**
- Allows client to release resources
- Called before callback is removed
- Useful for resource management

---

## Related Callbacks

- [OnDistributeMonitor](../network/OnDistributeMonitor.md) - Monitor cleanup
- [RegisterCallback2](../registration/RegisterCallback2.md) - Returns callback ID

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 4.1
- **String Address**: 0x6289f21c
- **String**: "Delete callback - ID %d\n"

---

**Status**: ✅ Documented  
**Confidence**: Medium (inferred from strings)  
**Last Updated**: 2025-06-17
