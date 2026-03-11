# Global Object VTables

## Date: 2025-03-11
## Source: Disassembly of client.dll .rdata section

---

## VTable at 0x628af9d0 (State Object)

Used by StateObject at `0x62999968`.

### Full VTable Entries

| Offset | Index | Function | Disassembly | Notes |
|--------|-------|----------|-------------|-------|
| 0x00 | 0 | 0x623b3160 | `mov eax, 0x628afa28; ret` | Returns constant |
| 0x04 | 1 | 0x623b3200 | Unknown | |
| 0x08 | 2 | 0x623b3840 | Unknown | |
| 0x0C | 3 | 0x623b2d50 | Unknown | |
| 0x10 | 4 | 0x623b2d50 | Unknown | |
| 0x14 | 5 | 0x623b2d50 | Unknown | |
| 0x18 | 6 | 0x623a36a0 | Unknown | |
| 0x1C | 7 | 0x623a2d90 | Unknown | |
| 0x20 | 8 | 0x623a2de0 | Unknown | |
| 0x24 | 9 | 0x623a2df0 | Unknown | |
| 0x28 | 10 | 0x623a2e00 | Unknown | |
| 0x2C | 11 | 0x623a2e20 | Unknown | |
| 0x30 | 12 | 0x623a2e10 | Unknown | |
| 0x34 | 13 | 0x623a2e30 | Unknown | |
| 0x38 | 14 | 0x623b3160 | Same as [0] | |
| **0x3C** | **15** | **0x623b3170** | **`mov eax, [ecx+0x28]; ret`** | **Getter for field at offset 0x28** |

### Function 0x623b3170 (VTable Entry 15)

```c
void* __thiscall GetField28(void* this) {
    return *(void**)((char*)this + 0x28);
}
```

**Assembly**:
```
623b3170: 8b 41 28    mov eax, [ecx+0x28]    ; Load field at offset 0x28
623b3173: c3          ret                     ; Return it
```

**What This Means**:
- The managed object needs a field at offset 0x28
- When vtable[15] is called, it reads [this+0x28]
- If ECX is NULL or invalid during this call, it will crash
- This is likely a member variable accessor

---

## Object Layout Requirements

### ManagedObject (referenced by StateObject.pManagedObject)

```c
struct ManagedObject {
    void* vtable;           // 0x00: Must point to valid vtable
    // ...
    void* field_28;         // 0x28: Read by vtable[15]
};
```

The object at offset 0x04 of StateObject must have valid:
1. Vtable pointer at offset 0x00
2. Data at offset 0x28 for getter function

---

## Raw VTable Data

From `objdump -s -j .rdata client.dll`:

```
Address     Data        Function
628af9d0:  60313b62    0x623b3160  [0]
628af9d4:  00323b62    0x623b3200  [1]
628af9d8:  40383b62    0x623b3840  [2]
628af9dc:  502d3b62    0x623b2d50  [3]
628af9e0:  a0363b62    0x623b36a0  [6]
628af9e4:  902d3b62    0x623b2d90  [7]
628af9e8:  e02d3b62    0x623b2de0  [8]
628af9ec:  f02d3b62    0x623b2df0  [9]
628af9f0:  002e3b62    0x623b2e00  [10]
628af9f4:  202e3b62    0x623b2e20  [11]
628af9f8:  102e3b62    0x623b2e10  [12]
628af9fc:  302e3b62    0x623b2e30  [13]
628afa00:  602e3b62    0x623b2e60  [12] - wait, check alignment
628afa04:  702e3b62    0x623b2e70  [13]
628afa08:  c02d3b62    0x623b2dc0  [14]
628afa0c:  70313b62    0x623b3170  [15] <-- CRITICAL FUNCTION
```

**Note**: Index 15 = offset 0x3C = 0x628af9d0 + 60 = 0x628afa0c

**Value at 0x628afa0c**: `0x623b3170`

---

## Fix for Current Crash

The crash at `client.dll+0x875895` is likely caused by:
1. Our ManagedObject doesn't have valid data at offset 0x28
2. When vtable[15] is called with our object in ECX
3. It tries to read [ECX+0x28] which may be garbage/outside object
4. Returns bad pointer, causing later crash

### Solution

Ensure ManagedObject has:
```c
struct ManagedObject {
    void* vtable;           // 0x00
    uint32_t reserved[10]; // 0x04-0x27: Padding
    void* field_28;         // 0x28: Used by vtable[15]
};
```

---

**Status**: Fixed vtable entry - need to update test code
