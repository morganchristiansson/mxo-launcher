# Global Objects Initialization Analysis

## Date: 2025-03-10

## Status: INVESTIGATION IN PROGRESS

---

## Global Object at 0x629f1748

### Usage Pattern

This object is accessed extensively in RunClientDLL and throughout client.dll:

```assembly
; RunClientDLL (0x62006c4d)
mov ecx, [0x629f1748]    ; Load object
call 0x622a4c10          ; Call method (expects valid object)

mov eax, [0x629f1748]
mov edi, [eax+0x38]      ; Access field at offset 0x38
mov esi, [eax+0x3c]      ; Access field at offset 0x3c
```

### Object Structure (Partial)

Based on usage:

```c
struct UnknownObject629f1748 {
    void* vtable;            // 0x00
    // ... unknown fields ...
    void* field_38;          // 0x38
    void* field_3c;          // 0x3c
    // ... more fields ...
};
```

Size: At least 0x40 (64) bytes

### Access Locations

Found in many functions:
- `0x6200290d` - Timer/calculator function
- `0x6200303a` - Event processing
- `0x62006c4d` - RunClientDLL (CRASH HERE)
- `0x62008893` - Unknown
- `0x6200914e` - Unknown
- And many more...

### Function 0x622a4c10

The method called on this object:

```assembly
622a4c10: push ebp
622a4c11: mov ebp, esp
622a4c13: sub esp, 0x28
622a4c16: push ebx
622a4c17: push esi
622a4c18: push edi
622a4c19: mov edi, ecx        ; 'this' pointer
622a4c1b: mov ecx, [edi]      ; Get field at 0x00
622a4c1d: mov eax, [edi+0xc]  ; Get field at 0x0C
622a4c20: inc ecx             ; Increment counter
622a4c21: mov [edi], ecx      ; Store back
622a4c23: mov ecx, [edi+0x10] ; Get field at 0x10
...
```

This looks like a time/frame counter object!

---

## Global Object at 0x629df7d0

### Usage

Another critical object used in RunClientDLL:

```assembly
; RunClientDLL (0x62006c40)
mov ecx, [0x629df7d0]    ; Load object
mov eax, [ecx]           ; Get vtable
push 0
call [eax+0x10]          ; Call vtable[4]
```

### Object Type

This has a vtable, so it's a C++ object. Used similarly to an APIObject.

---

## Global Object at 0x629df7f0

### Usage in InitClientDLL

```assembly
; InitClientDLL (0x6200130b)
mov ecx, [0x629df7f0]    ; Load object
mov edx, [ecx]           ; Get vtable
call [edx+0x10]          ; Call vtable[4]

mov ecx, [0x629df7f0]
mov eax, [ecx]
call [eax+0x58]          ; Call vtable[22]

mov ecx, [0x629df7f0]
mov edx, [ecx]
push eax
call [edx+0x60]          ; Call vtable[24]
```

This is accessed right after the parameter parsing, suggesting it's critical for initialization.

---

## Hypothesis

These global objects appear to be:

1. **0x629f1748** - Game loop timer/frame counter
2. **0x629df7d0** - Primary interface object
3. **0x629df7f0** - Configuration/settings object

They should be initialized by:
- The real launcher.exe before loading client.dll
- OR by some initialization function we haven't called yet
- OR they should be passed as parameters to InitClientDLL

---

## Next Steps

1. Check InitClientDLL parameters more carefully
2. Look for initialization functions in client.dll that set up globals
3. Check if launcher.exe creates these objects
4. Try creating stub objects with proper vtables

---

## Related Code

Test showing the crash:
```bash
cd ~/MxO_7.6005
wine test_detailed_fixed.exe
# Crashes at RunClientDLL when accessing 0x629f1748
```

Log file: `/tmp/detailed_log_fixed.txt`

---

**Status**: Investigating
**Next Action**: Trace InitClientDLL parameter usage
