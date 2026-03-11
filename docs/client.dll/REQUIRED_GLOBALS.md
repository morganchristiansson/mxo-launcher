# Required Global Objects for RunClientDLL

## Updated: 2025-03-11

---

## Summary of Objects

| Address | Object | Status | Purpose |
|---------|--------|--------|---------|
| 0x629f14a0 | MasterDatabase | Auto-created | Main database |
| 0x629f1748 | TimerObject | ✅ Inject | Frame/timer tracking |
| 0x62999968 | StateObject | ✅ Inject | State management |
| **0x629df7d0** | **Unknown** | **❌ MANDATORY** | RunClientDLL main object |
| 0x629ddfc8 | Near crash site | ? | Related to 0x629df7d0? |
| 0x62a28848 | Unknown | ? | Used by inner call |
| 0x62a28844 | Unknown | ? | Used by inner call |

---

## Critical: Object at 0x629df7d0

**Purpose**: Main object used by RunClientDLL entry point

**Structure**:
```c
struct RunClientDLL_Object {
    void* vtable;           // 0x00: Must have vtable[4] = 0x623b36a0
    void* pInnerObject;     // 0x04: Inner object with its own vtable
};
```

**Usage**:
```asm
RunClientDLL:
  mov ecx, [0x629df7d0]    ; Load this object
  mov eax, [ecx]            ; Get vtable
  call [eax+0x10]           ; Call vtable[4]
```

**VTable[4] behavior**:
- Gets [object+0x4] (inner object pointer)
- Calls vtable[3] on inner object
- Which uses more globals...

---

## Recommended Fix

Add to test code:

```c
// Object for RunClientDLL global at 0x629df7d0
struct RunClientDLL_Object {
    void* vtable;           // 0x00
    void* pInner;           // 0x04
};

static RunClientDLL_Object g_RunClientObj;
static SomeObject g_InnerObj;  // Pointed to by pInner

// In init:
HMODULE clientBase = GetModuleHandleA("client.dll");

// Inject at 0x629df7d0
RunClientDLL_Object* pRunObj = (RunClientDLL_Object*)((char*)clientBase + 0x9df7d0);
g_RunClientObj.vtable = /* vtable with 0x623b36a0 at slot 4 */;
g_RunClientObj.pInner = &g_InnerObj;
*pRunObj = g_RunClientObj;
```

---

## VTable for 0x629df7d0 object

Need vtable where slot 4 (offset 0x10) = 0x623b36a0:
```
vtable[0] = ?
vtable[1] = ?
vtable[2] = ?
vtable[3] = ?
vtable[4] = 0x623b36a0  <-- REQUIRED
```

Whether this needs to be client.dll's vtable or can be stub depends on behavior.

---

## Status

**Current**: Identified missing object at 0x629df7d0
**Next**: Create proper object with valid vtable and inner object

---

**Related**: CRASH_CHAIN_ANALYSIS.md
