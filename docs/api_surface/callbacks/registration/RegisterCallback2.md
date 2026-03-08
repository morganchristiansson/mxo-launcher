# RegisterCallback2

## Overview

**VTable Index**: 24 (0x60)  
**Category**: Registration  
**Direction**: Client → Launcher  
**Purpose**: Alternative callback registration with extended options

---

## Function Signature

```c
int RegisterCallback2(APIObject* this, CallbackRegistration* reg);
```

### Parameters

| Type | Name | Offset | Purpose |
|------|------|--------|---------|
| `APIObject*` | this | ECX | Object pointer (thiscall) |
| `CallbackRegistration*` | reg | [ESP+4] | Registration structure pointer |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Success |
| `int` | -1 | Invalid parameter |
| `int` | -2 | Registration failed |
| `int` | >= 1 | Callback ID (success) |

---

## CallbackRegistration Structure

```c
struct CallbackRegistration {
    uint32_t eventType;        // Event type identifier
    void* callbackFunc;        // Callback function pointer
    void* userData;            // User-provided context
    uint32_t priority;         // Callback priority
    uint32_t flags;            // Registration flags
};
```

### Structure Fields

| Offset | Type | Name | Purpose |
|--------|------|------|---------|
| 0x00 | `uint32_t` | eventType | Type of event to handle |
| 0x04 | `void*` | callbackFunc | Callback function pointer |
| 0x08 | `void*` | userData | User-provided context |
| 0x0C | `uint32_t` | priority | Callback priority (0-255) |
| 0x10 | `uint32_t` | flags | Registration options |

---

## Calling Convention

**Type**: `__thiscall` (C++ member function)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  reg (CallbackRegistration*)

Registers:
ECX = this pointer (APIObject*)
EAX = return value (callback ID or error code)
```

---

## Usage

### Assembly Pattern

```assembly
; Prepare registration structure
sub esp, 20                  ; Allocate structure
mov [esp], event_type        ; eventType
mov [esp + 4], callback_func ; callbackFunc
mov [esp + 8], user_data     ; userData
mov [esp + 12], priority     ; priority
mov [esp + 16], flags        ; flags

; Call RegisterCallback2
mov ecx, [object_ptr]        ; Load 'this' pointer
mov edx, [ecx]               ; Load vtable
push esp                     ; Push registration structure
call dword [edx + 0x60]      ; Call vtable[24]
add esp, 20                  ; Cleanup structure

; EAX = callback ID or error
```

### C++ Pattern

```c
// Prepare registration
CallbackRegistration reg;
reg.eventType = EVENT_NETWORK;
reg.callbackFunc = MyCallback;
reg.userData = myContext;
reg.priority = 100;
reg.flags = 0;

// Register callback
APIObject* obj = GetPrimaryObject();
int callbackId = obj->RegisterCallback2(&reg);
```

---

## Implementation

### Launcher Side

```c
int Primary_RegisterCallback2(APIObject* this, CallbackRegistration* reg) {
    if (!this || !reg) {
        return -1;  // Invalid parameter
    }
    
    if (!reg->callbackFunc) {
        return -1;  // Invalid callback
    }
    
    // Allocate callback entry
    CallbackEntry* entry = AllocateCallbackEntry();
    if (!entry) {
        return -2;  // Registration failed
    }
    
    // Store registration data
    entry->eventType = reg->eventType;
    entry->callback = reg->callbackFunc;
    entry->userData = reg->userData;
    entry->priority = reg->priority;
    entry->flags = reg->flags;
    
    // Return callback ID
    return entry->id;
}
```

### Client Side

```c
// Define callback
int MyCallback(void* data, uint32_t* result, uint32_t flags) {
    // Process callback
    return 0;
}

// Register with extended options
int RegisterMyCallback() {
    CallbackRegistration reg;
    reg.eventType = 1;
    reg.callbackFunc = MyCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];
    
    return regFunc(obj, &reg);
}
```

---

## Callback Entry Storage

Launcher maintains callback table:

```c
struct CallbackEntry {
    uint32_t id;               // Unique callback ID
    uint32_t eventType;        // Event type
    void* callback;            // Function pointer
    void* userData;            // User context
    uint32_t priority;         // Priority (for ordering)
    uint32_t flags;            // Flags
};
```

---

## Advantages Over RegisterCallback

| Feature | RegisterCallback | RegisterCallback2 |
|---------|------------------|-------------------|
| Event types | Single | Multiple |
| Priority | No | Yes |
| Flags | No | Yes |
| Returns ID | No | Yes |
| Unregistration | Manual | By ID |

---

## Unregistration

Use returned callback ID to unregister:

```c
int UnregisterCallback(APIObject* this, uint32_t callbackId);
```

---

## Notes

- **Most advanced registration function**
- Returns **callback ID** for later unregistration
- Supports **priority-based ordering**
- Allows **multiple callbacks** per event type
- Called after `SetEventHandler` in InitClientDLL

---

## Related Functions

- [RegisterCallback](RegisterCallback.md) - vtable[4] (0x10)
- [SetEventHandler](SetEventHandler.md) - vtable[23] (0x5C)
- [UnregisterCallback](UnregisterCallback.md) - vtable[5] (0x14)

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 3.2
- **Address**: VTable index 24, byte offset 0x60
- **Assembly**: `call dword [edx + 0x60]`
- **Evidence**: Found in InitClientDLL at 0x620012a0

---

**Status**: ✅ Documented  
**Confidence**: High (verified in disassembly)  
**Last Updated**: 2025-06-17
