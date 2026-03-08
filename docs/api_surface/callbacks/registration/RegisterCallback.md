# RegisterCallback

## Overview

**VTable Index**: 4 (0x10)  
**Category**: Registration  
**Direction**: Client → Launcher  
**Purpose**: Register callback function with launcher

---

## Function Signature

```c
int RegisterCallback(APIObject* this, void* callback, void* userData);
```

### Parameters

| Type | Name | Offset | Purpose |
|------|------|--------|---------|
| `APIObject*` | this | ECX | Object pointer (thiscall) |
| `void*` | callback | [ESP+4] | Callback function pointer |
| `void*` | userData | [ESP+8] | User data passed to callback |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Success |
| `int` | -1 | Invalid parameter |
| `int` | -2 | Registration failed |

---

## Calling Convention

**Type**: `__thiscall` (C++ member function)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  callback (void*)
[ESP+8]  userData (void*)

Registers:
ECX = this pointer (APIObject*)
EAX = return value
```

---

## Usage

### Assembly Pattern

```assembly
; Client.dll calls RegisterCallback
mov ecx, [object_ptr]         ; Load 'this' pointer
mov edx, [ecx]                ; Load vtable
push user_data                ; Push user data
push callback_func            ; Push callback function
call dword [edx + 0x10]       ; Call vtable[4]
; EAX = result code
```

### C++ Pattern

```c
// Register callback with launcher
APIObject* obj = GetPrimaryObject();
int result = obj->RegisterCallback(MyCallback, myUserData);
```

---

## Callback Storage

When registered, callback is stored in object structure:

```c
struct APIObject {
    void* pVTable;              // 0x00
    // ...
    void* pCallback1;           // 0x20 - Stored here
    // ...
    void* pCallbackData;        // 0x34 - User data stored here
};
```

---

## Example Implementation

### Launcher Side

```c
int Primary_RegisterCallback(APIObject* this, void* callback, void* userData) {
    if (!this || !callback) {
        return -1;  // Invalid parameter
    }
    
    // Store callback and user data
    this->pCallback1 = callback;
    this->pCallbackData = userData;
    
    return 0;  // Success
}
```

### Client Side

```c
// Define callback function
int MyCallback(void* data, uint32_t* result, uint32_t flags) {
    // Process callback
    return 0;
}

// Register callback
void RegisterMyCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*registerFunc)(APIObject*, void*, void*);
    registerFunc = (int (*)(APIObject*, void*, void*))vtable->functions[4];
    
    int result = registerFunc(obj, MyCallback, NULL);
}
```

---

## Notes

- **First registration function** called during initialization
- Used to register **primary callback** (stored at offset 0x20)
- Similar to `RegisterCallback2` (vtable[24])
- Must be called before event handlers can be triggered

---

## Related Functions

- [SetEventHandler](SetEventHandler.md) - vtable[23] (0x5C)
- [RegisterCallback2](RegisterCallback2.md) - vtable[24] (0x60)
- [UnregisterCallback](UnregisterCallback.md) - vtable[5] (0x14)

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 3.2
- **Address**: VTable index 4, byte offset 0x10
- **Assembly**: `call dword [edx + 0x10]`

---

**Status**: ✅ Documented  
**Confidence**: High (verified in disassembly)  
**Last Updated**: 2025-06-17
