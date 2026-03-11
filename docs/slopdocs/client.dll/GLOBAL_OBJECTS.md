# Global Objects Required by RunClientDLL

## Date: 2025-03-11
## Status: PARTIALLY DOCUMENTED

---

## Overview

After calling `SetMasterDatabase(NULL)` and `InitClientDLL`, client.dll expects certain global objects to be initialized before `RunClientDLL` can execute.

---

## Known Global Objects

### 1. MasterDatabase at `0x629f14a0`

**Created by**: `SetMasterDatabase(NULL)`

**Size**: 36 bytes (0x24)

**Status**: ✅ Automatically created

**Structure**:
```c
struct MasterDatabase {
    void* pIdentifier;         // 0x00: Linked-list node or NULL
    uint32_t refCount;         // 0x04: Reference count
    uint32_t stateFlags;       // 0x08: State flags
    void* pPrimaryObject;      // 0x0C: Primary API object
    uint32_t primaryData1;     // 0x10
    uint32_t primaryData2;     // 0x14
    void* pSecondaryObject;    // 0x18: Secondary API object
    uint32_t secondaryData1;   // 0x1C
    uint32_t secondaryData2;   // 0x20
};
```

---

### 2. Timer Object at `0x629f1748`

**Created by**: MUST BE INJECTED

**Size**: At least 64 bytes (0x40)

**Status**: ❌ Not created by client.dll - must be injected

**Usage**: Frame/time tracking in game loop

**Structure**:
```c
struct TimerObject {
    uint32_t counter;       // 0x00: Frame counter (incremented each call)
    uint32_t field_04;      // 0x04
    uint32_t field_08;      // 0x08
    uint32_t field_0c;      // 0x0C: Used in time calculations
    uint32_t field_10;      // 0x10: Used in time calculations  
    uint32_t field_14;      // 0x14
    uint32_t field_18;      // 0x18
    uint32_t field_1c;      // 0x1C
    uint32_t field_20;      // 0x20
    uint32_t field_24;      // 0x24
    float field_28;         // 0x28: Float (time delta?)
    void* pAnimator;        // 0x2C: Pointer to animator object (has vtable)
    float field_30;         // 0x30: Float value
    float field_34;         // 0x34: Float value
    uint32_t field_38;      // 0x38: Frame count
    uint32_t field_3c;      // 0x3C
};
```

**Inject Method**:
```c
TimerObject* pTimer = (TimerObject*)((char*)clientBase + 0x9f1748);
memset(pTimer, 0, sizeof(TimerObject));
pTimer->field_0c = 100;
pTimer->field_10 = 16;
pTimer->field_28 = 0.016f;
```

---

### 3. State Object at `0x62999968`

**Created by**: MUST BE INJECTED

**Size**: 8 bytes (structure) + referenced objects

**Status**: ❌ Not created by client.dll - must be injected

**Usage**: State machine for game logic

**Structure**:
```c
struct StateObject {
    void* vtable;           // 0x00: Vtable pointer (use 0x628af9d0 or 0x628af978 from client.dll)
    void* pManagedObject;   // 0x04: Pointer to managed object (must have vtable at offset 0x00)
};
```

**Vtables in client.dll**:
- `0x628af9d0`: State vtable #1
- `0x628af978`: State vtable #2

**Inject Method**:
```c
StateObject* pState = (StateObject*)((char*)clientBase + 0x999968);
pState->vtable = (void*)((char*)clientBase + 0xaf9d0);  // Use client.dll's vtable
pState->pManagedObject = &yourManagedObject;  // Your managed object
```

**Managed Object Requirements**:
- Offset 0x00: Vtable pointer
- Vtable at offset 0x3C: Function pointer (called during game loop)

---

### 4. Unknown Object at `0x629df7d0`

**Created by**: Unknown

**Status**: ❓ Need to investigate

**Usage**: Used in RunClientDLL, calls vtable[4]

---

### 5. Unknown Object at `0x629df7f0`

**Created by**: Unknown

**Status**: ❓ Need to investigate

**Usage**: Used in InitClientDLL and RunClientDLL

---

## Crash Locations and Missing Objects

| Crash Address | Missing Object | Fix |
|---------------|----------------|-----|
| `client+0x29cb9e` | Passing non-NULL to SetMasterDatabase | Pass NULL |
| `client+0x3b3573` | Timer object at 0x629f1748 | Inject TimerObject |
| `client+0x3b3573` | State object at 0x62999968 | Inject StateObject |
| `client+0x875895` | Unknown | Investigating |

---

## Injection Code Example

```c
HMODULE clientBase = GetModuleHandleA("client.dll");

// Inject Timer Object
TimerObject timer;
memset(&timer, 0, sizeof(timer));
timer.field_0c = 100;
timer.field_10 = 16;
timer.field_28 = 0.016f;

TimerObject* pGlobalTimer = (TimerObject*)((char*)clientBase + 0x9f1748);
memcpy(pGlobalTimer, &timer, sizeof(timer));

// Inject State Object
ManagedObject managedObj;
managedObj.vtable = myVtable;

StateObject state;
state.vtable = (void*)((char*)clientBase + 0xaf9d0);
state.pManagedObject = &managedObj;

StateObject* pGlobalState = (StateObject*)((char*)clientBase + 0x999968);
memcpy(pGlobalState, &state, sizeof(state));
```

---

## References

- Disassembly: Function `0x622a4c10` (Timer method)
- Disassembly: Function `0x623b3560` (State method)
- Test code: `~/MxO_7.6005/test_fixed_state.cpp`

---

**Last Updated**: 2025-03-11
**Status**: Incomplete - more objects need investigation
