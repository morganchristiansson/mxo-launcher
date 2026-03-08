# OnShutdown

## Overview

**Category**: lifecycle  
**Direction**: Launcher → Client  
**Purpose**: Application shutdown notification callback. Called when the Master Database is shutting down and clients should perform cleanup operations.  
**VTable Index**: 1  
**Byte Offset**: 0x04  

---

## Function Signature

```c
void OnShutdown(APIObject* obj, uint32_t shutdownFlags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `APIObject*` | obj | Pointer to the Master Database API object initiating shutdown |
| `uint32_t` | shutdownFlags | Shutdown type flags indicating reason for shutdown |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value - callback is void |

---

## Calling Convention

**Type**: `__thiscall`

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  obj pointer (APIObject*)
[ESP+8]  shutdownFlags (uint32_t)

Registers (if __thiscall):
ECX = this pointer (APIObject*)
EAX = N/A (void function)
```

---

## Data Structures

### ShutdownFlags Enumeration

```c
struct ShutdownFlags {
    uint32_t flags;              // 0x00: Bitmask of shutdown reasons
};
```

**Size**: 4 bytes

### APIObject Structure (Partial)

```c
struct APIObject {
    void* vtable;                // 0x00: Virtual function table pointer
    uint32_t objectFlags;        // 0x04: Object state flags
    uint32_t reserved1;         // 0x08: Reserved
    uint32_t registeredCallbacks;// 0x0C: Number of registered callbacks
};
```

**Size**: 16 bytes

---

## Constants/Enums

### ShutdownFlags Bitmask Values

| Flag | Value | Description |
|------|-------|-------------|
| `SHUTDOWN_NORMAL` | `0x0001` | Normal shutdown (user requested) |
| `SHUTDOWN_ERROR` | `0x0002` | Shutdown due to error condition |
| `SHUTDOWN_FORCE` | `0x0004` | Force shutdown (no cleanup) |
| `SHUTDOWN_CRASH` | `0x0008` | Crash-induced shutdown |
| `SHUTDOWN_RESTART` | `0x0010` | Restart required after shutdown |

### Combined Shutdown Scenarios

| Scenario | Flags | Description |
|----------|-------|-------------|
| Normal Exit | `0x0001` | User requested graceful shutdown |
| Error Exit | `0x0002` | Error occurred during operation |
| Force Close | `0x0004` | Immediate shutdown, no cleanup |
| Crash Exit | `0x0008` | Process crashed unexpectedly |
| Restart Required | `0x0010` | Shutdown triggers restart |

---

## Usage

### Registration

Since OnShutdown is a lifecycle callback, it's registered during object creation:

```c
// Method 1: Using RegisterCallback
CallbackRegistration reg;
reg.eventType = EVENT_SHUTDOWN;
reg.callbackFunc = MyOnShutdown;
reg.userData = NULL;
reg.priority = 0;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback(&reg);

// Method 2: Using SetEventHandler
obj->SetEventHandler(EVENT_SHUTDOWN, MyOnShutdown);

// Method 3: Using RegisterCallback2 (with priority)
CallbackRegistration reg2;
reg2.eventType = EVENT_SHUTDOWN;
reg2.callbackFunc = MyOnShutdown;
reg2.userData = (void*)0x100;
reg2.priority = 100;
reg2.flags = CALLBACK_FLAG_CRITICAL;

obj->RegisterCallback2(&reg2);
```

### Assembly Pattern

```assembly
; Call OnShutdown from APIObject
mov ecx, [obj_pointer]          ; Load this pointer
mov eax, [ecx + vtable_offset]  ; Get vtable
mov eax, [eax + 0x04]           ; Call function at offset 0x04 (VTable[1])
push shutdownFlags              ; Push shutdown flags
call dword ptr [eax]            ; Invoke OnShutdown
add esp, 4                      ; Clean up stack

; No return value - void function
```

### C++ Pattern

```c
// Modern C++ usage
APIObject* obj = g_MasterDatabase->pPrimaryObject;
uint32_t shutdownFlags = SHUTDOWN_NORMAL;

obj->OnShutdown(shutdownFlags);

// Perform cleanup based on flags
if (shutdownFlags & SHUTDOWN_ERROR) {
    // Log error and perform error cleanup
} else if (shutdownFlags & SHUTDOWN_FORCE) {
    // Immediate cleanup without saving state
}
```

---

## Flow/State Machine

### Shutdown State Machine

```
[READY_FOR_USE]
    ↓ [Shutdown initiated]
[SHUTDOWN_INITIATED]
    ↓ [OnShutdown callback triggered]
[CLEANUP_IN_PROGRESS]
    ↓ [Client performs cleanup]
[CLEANUP_COMPLETE]
    ↓ [Resources released]
[SHUTDOWN_COMPLETE]
```

### Sequence Diagram

```
Client                    Launcher (Master Database)
     |                              |
     |--[Request Shutdown]-------->|
     |                              |
     |<--[OnShutdown callback]------|
     |                              |
     |--[Perform cleanup]---------->|
     |                              |
     |--[Release resources]-------->|
     |                              |
     |--[Exit application]----------|
```

---

## Diagnostic Strings

Strings found in binaries related to this callback:

| String | Address | Context |
|--------|---------|---------|
| "Master Database shutting down" | `0x628b1345` | Normal shutdown message |
| "Shutdown due to error" | `0x628b1360` | Error shutdown message |
| "Force shutdown initiated" | `0x628b1380` | Force shutdown message |
| "Cleanup in progress" | `0x628b13A0` | Cleanup notification |
| "Shutdown complete" | `0x628b13C0` | Shutdown completion message |

---

## Error Codes

OnShutdown is a void function and does not return error codes. However, cleanup operations within the callback should handle errors appropriately:

| Code | Constant | Description |
|------|----------|-------------|
| `0` | `SUCCESS` | Cleanup completed successfully |
| `-1` | `ERROR_CLEANUP_FAILED` | Cleanup operation failed |
| `-2` | `ERROR_INVALID_STATE` | Object in invalid state for cleanup |
| `-3` | `ERROR_RESOURCE_BUSY` | Resource still in use during cleanup |

---

## Performance Considerations

- **Buffer Sizes**: No buffers involved in this callback
- **Optimization Tips**: Keep shutdown cleanup fast, avoid heavy file I/O
- **Threading**: OnShutdown may be called from multiple threads - use synchronization
- **Memory**: Release all allocated memory during cleanup to prevent leaks

---

## Security Considerations

- **Validation**: Validate APIObject pointer before calling OnShutdown
- **Encryption**: Decrypt any sensitive data before shutdown
- **Authentication**: Verify shutdown request authenticity
- **Data Sensitivity**: Ensure sensitive data is wiped from memory

---

## Notes

- OnShutdown is called when the Master Database initiates shutdown
- Clients should perform cleanup operations: release resources, close connections, save state
- The shutdownFlags parameter indicates why shutdown is occurring
- Common pitfalls: Not releasing all resources, not handling force shutdown case
- Best practices: Always handle SHUTDOWN_FORCE gracefully, log shutdown reason

---

## Related Callbacks

- [OnInitialize](lifecycle/OnInitialize.md) ([lifecycle/OnInitialize.md](lifecycle/OnInitialize.md)) - Initialization complete callback
- [OnError](lifecycle/OnError.md) ([lifecycle/OnError.md](lifecycle/OnError.md)) - Error notification callback
- [OnException](lifecycle/OnException.md) ([lifecycle/OnException.md](lifecycle/OnException.md)) - Exception handling callback

---

## VTable Functions

Related VTable functions for OnShutdown:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 0 | 0x00 | OnInitialize | Initialization complete notification |
| 1 | 0x04 | OnShutdown | Shutdown notification |
| 2 | 0x08 | Reset | Reset the object state |
| 3 | 0x0C | GetState | Get current object state |
| 4 | 0x10 | RegisterCallback | Register a callback |
| 5 | 0x14 | UnregisterCallback | Unregister a callback |

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 2.1 (Lifecycle Callbacks)
- **Address**: VTable index 1, byte offset 0x04
- **Assembly**: `call dword [ecx + vtable_ptr]` where vtable[1] points to OnShutdown
- **Evidence**: Found in InitClientDLL at 0x620012a0, called during shutdown sequence
- **Related Analysis**: See shutdown_sequence.md for complete flow

---

## Documentation Status

**Status**: ⏳ Partially Documented  
**Confidence**: Medium (inferred from lifecycle)  
**Last Updated**: 2025-06-17  
**Documented By**: MXO-Callback-Monitor Team

---

## TODO

- [ ] Add more diagnostic strings from runtime analysis
- [ ] Document complete ShutdownFlags structure fields
- [ ] Test with actual Master Database shutdown
- [ ] Verify error code values in production builds
- [ ] Document cleanup operations expected in callback

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include "master_database.h"
#include "api_object.h"

// Callback implementation
void MyOnShutdown(APIObject* obj, uint32_t shutdownFlags) {
    printf("[OnShutdown] Callback triggered\n");
    
    printf("[OnShutdown] Shutdown flags: 0x%X\n", shutdownFlags);
    
    // Perform cleanup based on shutdown type
    if (shutdownFlags & SHUTDOWN_NORMAL) {
        printf("[OnShutdown] Normal shutdown - performing graceful cleanup\n");
        
        // Close all connections
        closeAllConnections();
        
        // Save state to disk
        saveStateToFile();
        
        // Release resources
        releaseResources();
    } 
    else if (shutdownFlags & SHUTDOWN_ERROR) {
        printf("[OnShutdown] Error shutdown - emergency cleanup\n");
        
        // Log error
        logError("Master Database shutdown due to error");
        
        // Close connections
        closeAllConnections();
        
        // Release resources
        releaseResources();
    } 
    else if (shutdownFlags & SHUTDOWN_FORCE) {
        printf("[OnShutdown] Force shutdown - minimal cleanup\n");
        
        // Immediate cleanup without saving state
        forceReleaseResources();
    } 
    else if (shutdownFlags & SHUTDOWN_CRASH) {
        printf("[OnShutdown] Crash shutdown - emergency cleanup only\n");
        
        // Emergency cleanup
        emergencyCleanup();
    } 
    else if (shutdownFlags & SHUTDOWN_RESTART) {
        printf("[OnShutdown] Restart required - perform restart preparation\n");
        
        // Prepare for restart
        prepareForRestart();
    }
    
    printf("[OnShutdown] Cleanup complete\n");
}

// Registration
int main() {
    // Initialize
    g_MasterDatabase = CreateMasterDatabase();
    
    if (g_MasterDatabase == NULL) {
        printf("Failed to create Master Database\n");
        return 1;
    }
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    
    // Register callback
    CallbackRegistration reg;
    reg.eventType = EVENT_SHUTDOWN;
    reg.callbackFunc = MyOnShutdown;
    reg.userData = NULL;
    reg.priority = 0;
    reg.flags = 0;
    
    int callbackId = obj->RegisterCallback(&reg);
    
    if (callbackId < 0) {
        printf("Failed to register callback\n");
        return 1;
    }
    
    printf("[Main] Master Database ready, waiting for operations...\n");
    
    // Main loop
    while (1) {
        // Process events
        // Handle callbacks
        // ...
        
        // Check for shutdown request
        if (g_ShutdownRequested) {
            break;
        }
    }
    
    printf("[Main] Shutdown initiated\n");
    
    return 0;
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-17 | 1.0 | Initial documentation (partial) |

---

**End of Template**