# SetMasterDatabase - CORRECT Usage

## Date: 2025-03-11
## Status: VERIFIED WORKING

---

## CRITICAL: Pass NULL to SetMasterDatabase!

### The WRONG Way (Documents say this, but it crashes)
```c
MasterDatabase db;
db.pVTable = myVTable;
db.refCount = 1;  // or 0
db.pPrimaryObject = &primaryObj;
// ... initialize fields ...
SetMasterDatabase(&db);  // ❌ WRONG - This crashes!
```

### The CORRECT Way
```c
SetMasterDatabase(NULL);  // ✅ CORRECT - Let client.dll create its own!
```

---

## Why Passing a Structure Crashes

### What Actually Happens

From disassembly of `client.dll!SetMasterDatabase` at `0x6229d760`:

1. **Creates internal MasterDatabase** at `0x629f14a0`
2. **If parameter != NULL**:
   - Treats parameter as a linked-list node
   - Tries to link it into internal structures
   - **Corrupts the structure** by overwriting offset 0x0C

3. **Then calls vtable[0]** on what it thinks is the primary object, but which is now corrupted

### The Linked-List Node Structure

```c
struct ListNode {
    void* data;      // Offset 0x00
    ListNode* next;  // Offset 0x04
    ListNode* prev;  // Offset 0x08
};
```

When you pass a MasterDatabase, client.dll does linked-list operations on:
- `MasterDatabase.offset_0x00` (pVTable/identifier)
- `MasterDatabase.offset_0x04` (refCount)
- `MasterDatabase.offset_0x08` (stateFlags)

This corrupts your structure!

---

## Correct Initialization Sequence

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
setMasterDB(NULL);  // ✅ This creates internal structures correctly

// 5. Call InitClientDLL
int result = init(argc, argv, hClient, nullptr, nullptr, 0, 0, nullptr);

// 6. If successful, call RunClientDLL
if (result == 0) {
    run();
}
```

---

## Global Objects Required for RunClientDLL

After `SetMasterDatabase(NULL)` and `InitClientDLL`, you must inject these objects:

### 1. Timer Object at `0x629f1748`
```c
struct TimerObject {
    uint32_t counter;       // 0x00: Frame counter
    uint32_t field_04;      // 0x04
    uint32_t field_08;      // 0x08
    uint32_t field_0c;      // 0x0C: Time value
    uint32_t field_10;      // 0x10: Frame time
    uint32_t field_14;      // 0x14
    uint32_t field_18;      // 0x18
    uint32_t field_1c;      // 0x1C
    uint32_t field_20;      // 0x20
    uint32_t field_24;      // 0x24
    float field_28;         // 0x28: Time delta
    void* pAnimator;        // 0x2C: Animator object
    float field_30;         // 0x30
    float field_34;         // 0x34
    uint32_t field_38;      // 0x38: Frame count
    uint32_t field_3c;      // 0x3C
};
```

### 2. State Object at `0x62999968`
```c
struct StateObject {
    void* vtable;           // 0x00: Vtable pointer
    void* pManagedObject;   // 0x04: Pointer to managed object
};
```

---

## Related Files

- `TEST_RESULTS.md` - Actual test results showing crashes and fixes
- `GLOBAL_OBJECTS.md` - Details on all required global objects
- `../client.dll/SetMasterDatabase/CORRECT_USAGE.md` - This file

---

## References

- Original (WRONG) documentation: `MASTER_DATABASE.md`
- Disassembly: `client.dll+0x6229d760` (SetMasterDatabase function)
- Test code: `~/MxO_7.6005/test_fixed_state.cpp`

---

**Last Updated**: 2025-03-11
**Verified**: ✅ Working with NULL parameter
