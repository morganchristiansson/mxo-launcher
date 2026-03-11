# SetMasterDatabase - Actual Runtime Behavior

## Executive Summary

**CRITICAL DISCOVERY**: The actual behavior of SetMasterDatabase differs significantly from the documented API surface.

## Date: 2025-03-10

## Status: BREAKTHROUGH - Major Discovery

---

## The Problem

Documentation suggested SetMasterDatabase accepts a MasterDatabase pointer that we create and populate. This is **INCORRECT**.

## Actual Behavior

### Function Signature
```c
void SetMasterDatabase(void* pMasterDatabase);
```

### Implementation Analysis

From disassembly at `0x6229d760`:

```assembly
; Step 1: Create internal MasterDatabase
call get_or_create_master_db    ; 0x6229d110
mov edi, [0x629f14a0]           ; Load internal master database

; Step 2: Check if parameter matches internal identifier
cmp ebx, [edi]                  ; Compare param with internal->identifier
je skip_init                    ; Skip if same

; Step 3: Initialize primary object
mov eax, [edi+0xc]              ; Get internal->pPrimaryObject
test eax, eax                   ; Check if exists
jne already_initialized

; Step 4: Treat parameter as linked-list node!
push ebx                        ; Push our MasterDatabase pointer
mov ecx, esi                    ; ECX = &internal->pPrimaryObject
call init_linked_list_node      ; 0x6229cb40

; Step 5: Call vtable[0] on internal structures
mov eax, [edi]                  ; Get identifier
test eax, eax
je skip_vtable

mov ecx, [esi]                  ; Get object pointer
mov edx, [ecx]                  ; Get vtable
push eax                        ; Push identifier
call [edx]                      ; Call vtable[0]
```

### Critical Finding #1: Linked-List Node Structure

The function at `0x6229cb40` treats the passed parameter as a **linked-list node**:

```c
struct ListNode {
    void* data;      // Offset 0x00
    ListNode* next;  // Offset 0x04
    ListNode* prev;  // Offset 0x08
};
```

It performs linked-list operations on fields at offsets 0x00, 0x04, and 0x08 of the passed structure.

### Critical Finding #2: Structure Corruption

When we pass a non-NULL MasterDatabase:

1. **Offset 0x00** (pIdentifier) → Treated as node data
2. **Offset 0x04** (refCount) → Treated as next pointer
3. **Offset 0x08** (stateFlags) → Treated as prev pointer
4. **Offset 0x0C** (pPrimaryObject) → Gets OVERWRITTEN by init function

Then client.dll tries to call vtable[0] on what it thinks is an object at offset 0x0C, but which now points to our corrupted MasterDatabase structure!

### Critical Finding #3: NULL Parameter Works

**Solution**: Pass NULL to SetMasterDatabase!

```c
// CORRECT way to call SetMasterDatabase
SetMasterDatabase(NULL);
```

When NULL is passed:
- client.dll creates its own internal MasterDatabase at `0x629f14a0`
- No structure corruption occurs
- InitClientDLL can proceed successfully

---

## Test Results

### Test 1: Passing MasterDatabase Structure (WRONG)
```
Result: CRASH at 0x6229cb9e
Reason: Null pointer dereference when trying to dereference corrupted structure
```

### Test 2: Passing NULL (CORRECT)
```
Result: SUCCESS
- SetMasterDatabase(NULL) returns successfully
- InitClientDLL returns 0 (success)
- RunClientDLL crashes (different issue - uninitialized globals)
```

---

## Updated Initialization Sequence

### Correct Sequence

```c
// 1. Pre-load dependencies
LoadLibraryA("MFC71.dll");
LoadLibraryA("MSVCR71.dll");
LoadLibraryA("dbghelp.dll");
LoadLibraryA("r3d9.dll");
LoadLibraryA("binkw32.dll");

// 2. Load client.dll
HMODULE hClient = LoadLibraryA("client.dll");

// 3. Get exports
auto setMasterDB = GetProcAddress(hClient, "SetMasterDatabase");
auto init = GetProcAddress(hClient, "InitClientDLL");
auto run = GetProcAddress(hClient, "RunClientDLL");

// 4. Call SetMasterDatabase with NULL
setMasterDB(NULL);  // IMPORTANT: Must be NULL!

// 5. Call InitClientDLL
int result = init(nullptr, nullptr, hClient, nullptr, nullptr, 0, 0, nullptr);

// 6. Call RunClientDLL (still crashes due to other issues)
if (result == 0) {
    run();
}
```

---

## Internal Global Addresses

Client.dll uses these global variables:

| Address | Purpose | Status |
|---------|---------|--------|
| `0x629f14a0` | Internal MasterDatabase pointer | Created by SetMasterDatabase |
| `0x629f1748` | Unknown object used in RunClientDLL | **NOT INITIALIZED** - Causes crash |
| `0x629df7f0` | Primary object pointer | Used extensively |
| `0x629df7d0` | Another primary object | Used in RunClientDLL |

---

## What Still Needs Initialization

The crash in RunClientDLL occurs because:

```assembly
; RunClientDLL at 0x62006c4d
mov ecx, [0x629f1748]    ; Load global object
call 0x622a4c10          ; Call function that expects valid object
mov eax, [0x629f1748]    ; Load again
mov edi, [eax+0x38]      ; CRASH: eax is NULL or invalid
```

**Next Step**: Find where `0x629f1748` should be initialized.

---

## Implications for MasterDatabase Structure

The actual MasterDatabase structure used by client.dll internally:

```c
struct MasterDatabaseInternal {
    ListNode* pIdentifier;     // 0x00: Linked-list node or NULL
    uint32_t refCount;         // 0x04: Reference count
    uint32_t stateFlags;       // 0x08: State flags
    APIObject* pPrimaryObject; // 0x0C: Primary API object
    uint32_t primaryData1;     // 0x10
    uint32_t primaryData2;     // 0x14
    APIObject* pSecondaryObject; // 0x18: Secondary API object
    uint32_t secondaryData1;   // 0x1C
    uint32_t secondaryData2;   // 0x20
};
```

Note: Offset 0x00 should be a ListNode pointer, not a vtable!

---

## Files Updated

- `test_detailed_fixed.cpp` - Working test with NULL SetMasterDatabase
- `/tmp/detailed_log_fixed.txt` - Log showing successful initialization

---

## References

- `MASTER_DATABASE.md` - Original (now outdated) documentation
- `client.dll` disassembly at `0x6229d760`
- Function `get_or_create_master_db` at `0x6229d110`
- Function `init_linked_list_node` at `0x6229cb40`

---

**Last Updated**: 2025-03-10 16:20
**Status**: VERIFIED - SetMasterDatabase(NULL) works correctly
**Next Task**: Initialize global objects for RunClientDLL
