# Crash Chain Analysis

## Date: 2025-03-11

## Call Chain from RunClientDLL

### Step 1: RunClientDLL Entry (0x62006c40)

```asm
62006c40: mov ecx, [0x629df7d0]    ; Load object from global
62006c46: mov eax, [ecx]            ; Get vtable
62006c48: push 0x0
62006c4a: call [eax+0x10]           ; Call vtable[4]
```

**Uses global object at**: `0x629df7d0`
**Calls**: vtable slot 4 (offset 0x10)

---

### Step 2: VTable[4] = 0x623b36a0

From vtable at 0x620af9d0 + 0x10 = 0x620af9e0:
```
628af9e0: a0363b62  -> 0x623b36a0
```

```asm
623b36a0: push ebp
623b36a1: mov ebp, esp
...
623b36ab: mov eax, [edi+0x4]       ; Get field at offset 0x4
623b36ae: test eax, eax
623b36b0: je 0x623b36b9            ; Skip if NULL
623b36b2: mov ecx, eax
623b36b4: mov eax, [ecx]           ; Get vtable from inner object
623b36b6: call [eax+0x0c]          ; Call vtable[3] on inner object
```

**Dereferences**: [object + 0x4] - The inner object pointer!
**Calls**: vtable slot 3 (offset 0x0c)

---

### Step 3: VTable[3] = 0x623b2d50

From vtable at 0x620af9d0 + 0x0c = 0x620af9dc:
```
628af9dc: 502d3b62  -> 0x623b2d50
```

```asm
623b2d50: push esi
623b2d51: mov esi, ecx              ; Save this pointer
623b2d53: mov ecx, [0x62a28848]     ; Load another global
623b2d59: test ecx, ecx
623b2d5b: je 0x623b2d62
623b2d5d: mov eax, [ecx]
623b2d5f: call [eax+0x44]           ; Call vtable[17]
```

**Dereferences**: Multiple globals
**Uses**: 0x62a28848

---

## The Missing Object

The crash happens because:

1. **0x629df7d0**: Global object used by RunClientDLL
2. **[0x629df7d0 + 0x4]**: Inner object pointer (must be valid!)
3. **Inner object's vtable[3]**: Called, probably leads to crash

The object at `0x629df7d0` must have:
- Valid vtable at offset 0x00 (for vtable[4] call)
- Valid inner object pointer at offset 0x04

---

## New Required Global: 0x629df7d0

**Status**: UNKNOWN - Need to investigate

**Structure**:
```c
struct Object_at_df7d0 {
    void* vtable;           // 0x00: Vtable pointer
    void* pInnerObject;     // 0x04: Inner object (called via vtable[3])
};
```

**VTable for object at 0x629df7d0**:
- Slot 4: 0x623b36a0

---

## Fix Required

Inject object at 0x629df7d0:

```c
struct OuterObject {
    void* vtable;           // 0x00: Points to vtable with 0x623b36a0 at slot 4
    void* pInner;           // 0x04: Points to another object
};

OuterObject outer;
outer.vtable = /* vtable with correct slot 4 */;
outer.pInner = &innerObject;  // Needs its own vtable

OuterObject* pGlobal = (OuterObject*)((char*)clientBase + 0x9df7d0);
*pGlobal = outer;
```

---

## Additional Globals Found

| Address | Usage |
|---------|-------|
| 0x629df7d0 | RunClientDLL main object |
| 0x62a28848 | Used by vtable[3] function |
| 0x62a28844 | Used by vtable[3] function |

---

**Status**: Need to inject object at 0x629df7d0 with valid structure
