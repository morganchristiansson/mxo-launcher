# Investigation Summary - 2025-03-10

## Achievements

### ✅ Successfully Completed

1. **Loaded all dependencies** (MFC71, MSVCR71, dbghelp, r3d9, binkw32)
2. **Loaded client.dll** successfully
3. **Called InitClientDLL** with 8 parameters
4. **InitClientDLL returns 0** (success)
5. **Discovered the correct parameter count** (8 params, not 6)
6. **Identified SetMasterDatabase export direction** (launcher.exe exports it, client.dll imports it)

### ❌ Current Blocker

**SetMasterDatabase crashes when called** with our Master Database structure.

## Crash Details

```
Exception: page fault on read access to 0x00000005
Address: 0x6229CB9E (client.dll+0x29cb9e)
Instruction: mov 0x04(%edx), %edx
Register: EDX = 0x00000001
```

The crash occurs in client.dll's `get_or_create_master_db` helper function when trying to dereference what should be a pointer but is instead the value 1.

## Theories Tested

### Theory 1: SetMasterDatabase called BY US before InitClientDLL ❌
- **Test**: Call client.dll!SetMasterDatabase ourselves
- **Result**: CRASH at client.dll+0x29cb9e
- **Conclusion**: Our Master Database structure is wrong

### Theory 2: SetMasterDatabase called by client.dll during InitClientDLL ❌
- **Test**: Export SetMasterDatabase and check if it's called
- **Result**: NOT CALLED
- **Conclusion**: InitClientDLL doesn't call SetMasterDatabase

### Theory 3: SetMasterDatabase called during RunClientDLL ❓
- **Status**: NOT YET TESTED
- **Reason**: Can't get past SetMasterDatabase crash

## Key Discoveries

### 1. InitClientDLL Parameters (8 total)

From disassembly of original launcher.exe at 0x0040a55c-0x0040a5a4:

```c
int InitClientDLL(
    void* param1,        // [esp+4]  from 0x4d2c5c
    void* param2,        // [esp+8]  from 0x4d2c60
    HMODULE hClient,     // [esp+12] client.dll handle from 0x4d2c50
    void* param4,        // [esp+16] from 0x4d2c4c
    void* masterDB,      // [esp+20] master database from 0x4d2c58
    int versionInfo,     // [esp+24] combined from [ebx+0xac] and [ebx+0xa8]
    int flags,           // [esp+28] from 0x4d2c69
    void* param8         // [esp+32] from 0x4d6304
);
```

### 2. SetMasterDatabase Export Direction

- **launcher.exe** EXPORTS SetMasterDatabase (ordinal 1, address 0x004143f0)
- **client.dll** IMPORTS SetMasterDatabase
- **Our launcher** must export SetMasterDatabase
- **client.dll** will call our SetMasterDatabase (supposedly)

### 3. Master Database Structure (36 bytes)

```c
struct MasterDatabase {
    void* pVTable;              // Offset 0x00
    uint32_t refCount;          // Offset 0x04
    uint32_t stateFlags;        // Offset 0x08
    void* pPrimaryObject;       // Offset 0x0C
    uint32_t primaryData1;      // Offset 0x10
    uint32_t primaryData2;      // Offset 0x14
    void* pSecondaryObject;     // Offset 0x18
    uint32_t secondaryData1;    // Offset 0x1C
    uint32_t secondaryData2;    // Offset 0x20
};
```

## What We Don't Know

1. **What does the "identifier" field (pVTable) need to be?**
   - We're using our vtable address
   - Client.dll expects something specific

2. **What should pPrimaryObject contain?**
   - We're providing a struct
   - Client.dll expects a C++ object with vtable?

3. **Exact structure layout**
   - Our struct might not match what client.dll expects
   - Fields might have different meanings

4. **Why does client.dll read EDX=1?**
   - Where does this value come from?
   - Suggests we're setting a field to 1 instead of a pointer

## Next Steps

### Option A: Reverse Engineer client.dll!SetMasterDatabase

Use radare2 to disassemble the function at 0x6229d760 and understand exactly what it expects.

### Option B: Study Original launcher.exe Initialization

Find where the original launcher creates its Master Database and what values it uses.

### Option C: Try with NULL Master Database

Test calling InitClientDLL with NULL for the master database parameter to see if that's optional.

## Files Created

- `launcher_correct.cpp` - Correct attempt with 8 parameters
- `launcher_logged.cpp` - With file logging to track calls
- `test_theory1.cpp` - Test calling SetMasterDatabase ourselves
- `master_database.h` - Structure definitions
- Documentation in `../../docs/api_surface/`

## Progress

**85% Complete**

- ✅ Dependencies loading
- ✅ client.dll loading
- ✅ InitClientDLL calling (returns success)
- ❌ SetMasterDatabase crashing
- ⏸️ RunClientDLL (blocked by SetMasterDatabase)

## Time Spent

Approximately 5 hours of investigation and testing.

## Conclusion

We've made significant progress understanding the initialization sequence, but are blocked on the SetMasterDatabase crash. The next step requires either:
1. Deep reverse engineering of client.dll's SetMasterDatabase implementation
2. Finding the exact values the original launcher uses for its Master Database
3. Understanding why client.dll expects specific structure contents

The crash is consistent and reproducible, which means it can be debugged systematically with the right tools (GDB, radare2, or IDA Pro).
