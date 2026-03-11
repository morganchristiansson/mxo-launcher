# RunClientDLL Documentation

## Date: 2025-03-11

---

## Function Signature

```c
void RunClientDLL(void);
```

**Address**: `0x62001180` (export from client.dll)  
**Called by**: launcher.exe after successful InitClientDLL

---

## Expected Initialization Sequence

1. launcher.exe loads client.dll
2. launcher.exe calls InitClientDLL()
3. During InitClientDLL:
   - client.dll discovers launcher!SetMasterDatabase
   - client.dll calls SetMasterDatabase(clientDB)
   - launcher stores callback info
4. InitClientDLL returns (0 = success)
5. launcher.exe calls RunClientDLL()
6. Game loop starts

---

## Current Crashes

### Crash Location: 0x623b3573

**Registers at crash**:
```
EIP: 0x623b3573
ECX: 0x00000000  <- NULL pointer!
EBX: 0x62999968  <- StateObject global
EAX: 0x00000000
```

**Instruction**:
```asm
623b3573: mov eax, [ecx]  ; CRASH - ECX is NULL
```

**Root Cause**:
- StateObject at 0x62999968 has NULL at offset 0x04 (pManagedObject)
- Code reads [0x62999968+0x04] → NULL
- Then tries to dereference NULL → CRASH

---

## Required Globals

### StateObject at 0x62999968

**Structure**:
```c
struct StateObject {
    void* vtable;           // 0x00: Set to 0x628af9d0 by InitClientDLL
    void* pManagedObject;   // 0x04: MUST be valid, currently NULL
};
```

**Initialization**:
- vtable is set by InitClientDLL (client.dll internal)
- pManagedObject needs to be set by SetMasterDatabase callback or remain unset

### Object at 0x629df7d0

**Structure**:
```c
struct RunClientObject {
    void* vtable;           // 0x00: Called via vtable[4]
    void* pInner;          // 0x04: Inner object
};
```

---

## Call Chain to Crash

```
RunClientDLL (0x62001180)
  -> RunClientDLL function body
    -> call vtable[4] on 0x629df7d0
      -> function at 0x623b36a0
        -> call vtable[3] on inner object from 0x629df7d0+0x04
          -> eventually reaches 0x623b3570
            -> mov ecx, [ebx+0x04]  ; Get pManagedObject
            -> mov eax, [ecx]       ; CRASH - dereference NULL
```

---

## Status

- InitClientDLL returns SUCCESS (0)
- SetMasterDatabase callback not received (returns success anyway)
- Missing object initialization at 0x62999968+0x04
- Need to determine proper initialization path

---

## Files

- `CRASH_ANALYSIS.md` - Detailed crash analysis
- `REQUIRED_GLOBALS.md` - All required global objects

---

**Related**: `../InitClientDLL/`
