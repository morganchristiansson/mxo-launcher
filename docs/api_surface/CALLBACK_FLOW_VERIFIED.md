# Callback Flow - Verified

## What Actually Happens

Based on detailed logging with test_detailed.exe:

### The Flow

```
1. We call client.dll!SetMasterDatabase(our_master_db)
   ↓
2. Client.dll calls our vtable[0] (Initialize)
   - obj = our_master_db (0x0040d100)
   - config = some_internal_pointer (0x009a5358)
   ↓
3. client.dll!SetMasterDatabase returns
   ↓
4. We call InitClientDLL(...)
   ↓
5. InitClientDLL returns 0 (success)
   ↓
6. We call RunClientDLL()
   ↓
7. Game attempts to create window
```

### What DOESN'T Happen

- ❌ Client.dll does NOT call our exported `SetMasterDatabase`
- Our exported SetMasterDatabase is there as a formality, but client.dll doesn't use it in this flow

### Callbacks Verified

| VTable Index | Function | Called? | Evidence |
|--------------|----------|---------|----------|
| 0 | Initialize | ✅ YES | "[CALLBACK] vtable[0] Initialize called!" |
| 1 | Shutdown | ❌ Not yet | Not in logs |
| 3 | GetState | ❌ Not yet | Not in logs |
| 4 | RegisterCallback | ❌ Not yet | Not in logs |
| 23 | SetEventHandler | ❌ Not yet | Not in logs |

### Parameters to vtable[0] (Initialize)

```c
int Launcher_Initialize(APIObject* obj, void* config) {
    // obj = our master database pointer
    // config = internal pointer from client.dll
}
```

## Implications

1. **We call client.dll's SetMasterDatabase** - not the other way around
2. **Client.dll uses vtable dispatch** to call our functions
3. **Our exported SetMasterDatabase** is not used in the standard flow
4. **refCount=0 is critical** - this determines whether client.dll initializes

## Working Configuration

```c
// Master Database
memset(&g_MasterDB, 0, sizeof(MasterDatabase));
g_MasterDB.pVTable = g_VTable;
g_MasterDB.pPrimaryObject = &g_PrimaryObject;
// refCount = 0 (critical!)

// Primary Object
memset(&g_PrimaryObject, 0, sizeof(APIObject));
g_PrimaryObject.pVTable = g_VTable;

// VTable
g_VTable[0] = Launcher_Initialize;  // Called during SetMasterDatabase
g_VTable[1] = Launcher_Shutdown;
g_VTable[3] = Launcher_GetState;
```

## Test Results

```
=== CALLING client.dll!SetMasterDatabase ===
Passing our master DB: 0040d100
[CALLBACK] vtable[0] Initialize called!  ← Client called us!
  obj=0040d100, config=009a5358
Returned from client.dll!SetMasterDatabase
=== CALLING InitClientDLL ===
InitClientDLL returned: 0
=== CALLING RunClientDLL ===
<game creates window>
```

## Conclusion

The callback mechanism is **vtable-based**, not export-based. Client.dll receives our master database, extracts the vtable pointer, and calls functions through it.

**Status**: ✅ Fully working
