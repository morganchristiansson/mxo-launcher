# Crash Analysis - client.dll+0x875895

## Date: 2025-03-11

---

## Crash Details

| Field | Value |
|-------|-------|
| **EIP** | `0x62875895` |
| **Relative Offset** | `0x875895` from client.dll base |
| **Memory Contents** | All `0xFF` (uninitialized) |

---

## Root Cause: Invalid Function Pointer

The crash at `0x875895` indicates execution jumped to **uninitialized memory**. This happens because:

1. A function pointer in a vtable or indirect call was invalid
2. The code called/jumped to that address
3. That address contains `0xFF` (uninitialized memory pattern)
4. Result: Execute uninitialized memory = crash

---

## Call Chain Analysis

Based on vtable research, the likely chain:

```
RunClientDLL
  -> Calls vtable[15] at 0x623b3170
     -> mov eax, [ecx+0x28]    ; Get field at offset 0x28
     -> ret                     ; Returns value in EAX
  -> Uses return value as function pointer
  -> Calls that address: 0x875895
  -> CRASH: Executing uninitialized memory
```

---

## The Bug

Our `ManagedObject` structure:

```c
struct ManagedObject {
    void* vtable;           // 0x00: Valid vtable pointer
    // ... padding ...
    void* field_28;         // 0x28: Used by vtable[15]
};
```

When vtable[15] is called:
- It reads `[this + 0x28]` 
- Returns whatever is there
- Caller uses that as a function pointer

**Problem**: We didn't initialize `field_28`, so it returns garbage, then tries to call it.

---

## Fix

Initialize `field_28` to point to a valid function or NULL-check the return:

```c
struct ManagedObject {
    void* vtable;           // 0x00
    uint32_t reserved[10];  // 0x04-0x27
    void* field_28;         // 0x28: MUST be valid pointer!
};

ManagedObject managedObj;
managedObj.vtable = g_ManagedObjectVTable;
managedObj.field_28 = (void*)SomeValidFunction;  // <-- FIX THIS
```

---

## Also Check

The crash might also be caused by:
1. Wrong vtable being used
2. Object lifetime issues
3. Wrong object passed to vtable[15]
4. Previous call corrupted the stack

---

## Next Steps

1. Find what vtable[15]'s return value should be used for
2. Identify the correct target function
3. Implement proper ManagedObject with valid field_28
4. Re-test

---

**Related**: `GLOBAL_OBJECT_VTABLES.md` for vtable details
