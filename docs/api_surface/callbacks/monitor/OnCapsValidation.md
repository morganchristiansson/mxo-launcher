# OnCapsValidation

## Overview

**VTable Index**: 25 (0x64)  
**Category**: Monitor  
**Direction**: Client → Launcher  
**Purpose**: Validate capability bits passed to callback

---

## Function Signature

```c
int OnCapsValidation(APIObject* this, uint32_t expectedBits, uint32_t actualBits, void* userData);
```

### Parameters

| Type | Name | Offset | Purpose |
|------|------|--------|---------|
| `APIObject*` | this | ECX | Object pointer (thiscall) |
| `uint32_t` | expectedBits | [ESP+4] | Expected capability bits |
| `uint32_t` | actualBits | [ESP+8] | Actual capability bits received |
| `void*` | userData | [ESP+C] | User data passed to callback |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Validation successful |
| `int` | -1 | Invalid parameter |
| `int` | -2 | Capability mismatch |
| `int` | -3 | Validation failed |

---

## Calling Convention

**Type**: `__thiscall` (C++ member function)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  expectedBits (uint32_t)
[ESP+8]  actualBits (uint32_t)
[ESP+C]  userData (void*)

Registers:
ECX = this pointer (APIObject*)
EAX = return value
```

---

## Usage

### Assembly Pattern

```assembly
; Client.dll calls OnCapsValidation
mov ecx, [object_ptr]         ; Load 'this' pointer
push actual_bits              ; Push actual capability bits
push expected_bits            ; Push expected capability bits
push user_data                ; Push user data
call dword [edx + 0x64]       ; Call vtable[25]
; EAX = result code
```

### C++ Pattern

```c
// Validate callback capabilities
APIObject* obj = GetPrimaryObject();
int result = obj->OnCapsValidation(actualBits, expectedBits, myUserData);
```

---

## Callback Storage

When validated, bits are stored in object structure:

```c
struct APIObject {
    void* pVTable;              // 0x00
    // ...
    uint32_t pExpectedBits;     // 0x4C - Expected bits stored here
    uint32_t pActualBits;       // 0x58 - Actual bits stored here
};
```

---

## Example Implementation

### Launcher Side

```c
int Primary_OnCapsValidation(APIObject* this, uint32_t expectedBits, uint32_t actualBits, void* userData) {
    if (!this || !expectedBits || !actualBits) {
        return -1;  // Invalid parameter
    }
    
    // Compare capability bits
    if ((expectedBits & actualBits) != expectedBits) {
        return -2;  // Expected bits not present in actual
    }
    
    // Validation successful
    return 0;  // Success
}
```

### Client Side

```c
// Define callback with capability validation
int MyCallback(void* data, uint32_t* result, uint32_t flags) {
    uint32_t expectedBits = 0x[ExpectedCapabilityValue];
    
    if (expectedBits != actualBits) {
        return -2;  // Capability mismatch
    }
    
    // Process callback
    return 0;
}

// Register callback with validation
void RegisterMyCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    
    int (*validateFunc)(APIObject*, uint32_t, uint32_t, void*);
    validateFunc = (int (*)(APIObject*, uint32_t, uint32_t, void*))obj->pVTable->functions[25];
    
    int result = validateFunc(obj, expectedBits, actualBits, myUserData);
}
```

---

## Notes

- **Capability validation** ensures callback has correct permission bits
- Used to detect and reject unauthorized callbacks
- Related to diagnostic string: "One or more of the caps bits passed to the callback are incorrect." (0x6293f630)
- Must be called before callback execution

---

## Related Functions

- [RegisterCallback](RegisterCallback.md) - vtable[4] (0x10)
- [SetEventHandler](SetEventHandler.md) - vtable[23] (0x5C)
- [RegisterCallback2](RegisterCallback2.md) - vtable[24] (0x60)

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 4.1
- **Address**: VTable index 25, byte offset 0x64
- **Assembly**: `call dword [edx + 0x64]`

---

**Status**: ✅ Documented  
**Confidence**: Medium (diagnostic string available)  
**Last Updated**: 2025-06-17