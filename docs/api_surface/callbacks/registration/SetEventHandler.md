# SetEventHandler

## Overview

**VTable Index**: 23 (0x5C)  
**Category**: Registration  
**Direction**: Client → Launcher  
**Purpose**: Register event handler for specific event type

---

## Function Signature

```c
int SetEventHandler(APIObject* this, uint32_t eventType, void* handler);
```

### Parameters

| Type | Name | Offset | Purpose |
|------|------|--------|---------|
| `APIObject*` | this | ECX | Object pointer (thiscall) |
| `uint32_t` | eventType | [ESP+4] | Event type identifier |
| `void*` | handler | [ESP+8] | Event handler function pointer |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Success |
| `int` | -1 | Invalid parameter |
| `int` | -2 | Unknown event type |
| `int` | -3 | Registration failed |

---

## Calling Convention

**Type**: `__thiscall` (C++ member function)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  eventType (uint32_t)
[ESP+8]  handler (void*)

Registers:
ECX = this pointer (APIObject*)
EAX = return value
```

---

## Event Types

Event types determine which callback storage location is used:

| Event Type | Storage Location | Purpose |
|------------|------------------|---------|
| 1 | pCallback1 (0x20) | General events |
| 2 | pCallback2 (0x24) | Network events |
| 3 | pCallback3 (0x28) | Game events |

---

## Usage

### Assembly Pattern

```assembly
; Client.dll calls SetEventHandler
mov ecx, [object_ptr]         ; Load 'this' pointer
mov edx, [ecx]                ; Load vtable
push handler_func             ; Push handler
push event_type               ; Push event type (1, 2, or 3)
call dword [edx + 0x5C]       ; Call vtable[23]
; EAX = result code
```

### C++ Pattern

```c
// Register event handler
APIObject* obj = GetPrimaryObject();
int result = obj->SetEventHandler(EVENT_NETWORK, MyNetworkHandler);
```

---

## Implementation

### Launcher Side

```c
int Primary_SetEventHandler(APIObject* this, uint32_t eventType, void* handler) {
    if (!this || !handler) {
        return -1;  // Invalid parameter
    }
    
    // Store handler based on event type
    switch (eventType) {
        case 1:
            this->pCallback1 = handler;
            break;
        case 2:
            this->pCallback2 = handler;
            break;
        case 3:
            this->pCallback3 = handler;
            break;
        default:
            return -2;  // Unknown event type
    }
    
    return 0;  // Success
}
```

### Client Side

```c
// Define event handler
void MyNetworkHandler(void* data, uint32_t size, uint32_t flags) {
    // Process network event
}

// Register handler
void RegisterMyHandler() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*setHandler)(APIObject*, uint32_t, void*);
    setHandler = (int (*)(APIObject*, uint32_t, void*))vtable->functions[23];
    
    // Register for network events (type 2)
    int result = setHandler(obj, 2, MyNetworkHandler);
}
```

---

## Event Handler Signature

```c
typedef void (*EventHandlerFunc)(void* data, uint32_t size, uint32_t flags);
```

### Handler Parameters

| Type | Name | Purpose |
|------|------|---------|
| `void*` | data | Event data pointer |
| `uint32_t` | size | Size of event data |
| `uint32_t` | flags | Event flags |

---

## Invocation Pattern

When launcher invokes registered handler:

```assembly
; Check if handler exists
mov eax, [object + 0x20]     ; Load handler (for event type 1)
test eax, eax                ; Check if NULL
je skip_handler              ; Skip if no handler

; Invoke handler
push flags                   ; Push flags
push size                    ; Push data size
push data_ptr                ; Push data pointer
call eax                     ; Call handler
add esp, 12                  ; Cleanup stack

skip_handler:
```

---

## Notes

- **Primary event registration function**
- Supports **multiple event types** (1, 2, 3)
- Handlers stored at **different offsets** based on type
- Called during **InitClientDLL** initialization
- More flexible than `RegisterCallback`

---

## Related Functions

- [RegisterCallback](RegisterCallback.md) - vtable[4] (0x10)
- [RegisterCallback2](RegisterCallback2.md) - vtable[24] (0x60)
- [ProcessEvent](../lifecycle/ProcessEvent.md) - vtable[6] (0x18)

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 3.1
- **Address**: VTable index 23, byte offset 0x5C
- **Assembly**: `call dword [edx + 0x5c]`
- **Evidence**: Found in InitClientDLL at 0x620012a0

---

**Status**: ✅ Documented  
**Confidence**: High (verified in disassembly)  
**Last Updated**: 2025-06-17
