# SetMasterDatabase Disassembly

## Address: 0x4143f0 (launcher.exe)

---

## Complete Disassembly

```asm
SetMasterDatabase(void* param):
4143f0: 55                    push ebp
4143f1: 8b ec                 mov ebp, esp
4143f3: 53                    push ebx
4143f4: 8b 5d 08              mov ebx, [ebp+0x8]    ; Get parameter
4143f7: 85 db                 test ebx, ebx
4143f9: 74 50                 je 0x41444b           ; If NULL, return immediately

; Non-NULL path (when param provided):
4143fb: 57                    push edi
4143fc: e8 5f fa ff ff        call 0x413e60         ; Internal function
414401: 8b 3d 54 3d 4d 00     mov edi, [0x4d3d54]   ; Load global at 0x4d3d54
414407: 3b 1f                 cmp ebx, [edi]        ; Compare with global[0]
414409: 74 3f                 je 0x41444a           ; If match, skip

41440b: 8b 47 0c              mov eax, [edi+0xc]    ; Get global[0x0c]
41440e: 85 c0                 test eax, eax
414410: 56                    push esi
414411: 8d 77 0c              lea esi, [edi+0xc]    ; ESI = &global[0x0c]
414414: 75 33                 jne 0x414449          ; If not NULL, skip

414416: 53                    push ebx
414417: 8b ce                 mov ecx, esi
414419: e8 b2 f5 ff ff        call 0x4139d0         ; Call internal (linked list insert?)
41441e: 8b 07                 mov eax, [edi]        ; Get global[0]
414420: 85 c0                 test eax, eax
414422: 74 19                 je 0x41443d           ; If NULL, skip

414424: 8b 0e                 mov ecx, [esi]        ; Get global[0x0c]
414426: 8b 11                 mov edx, [ecx]        ; Get vtable
414428: 50                    push eax
414429: ff 12                 call [edx]            ; Call vtable[0]

41442b: a1 54 3d 4d 00        mov eax, [0x4d3d54]   ; Reload global
414430: 8b 08                 mov ecx, [eax]        ; Get global[0]
414432: 8b 11                 mov edx, [ecx]        ; Get vtable
414434: ff 52 04              call [edx+0x4]        ; Call vtable[1]

414437: 8b 3d 54 3d 4d 00     mov edi, [0x4d3d54]   ; Reload global

41443d: 8d 47 0c              lea eax, [edi+0xc]
414440: 50                    push eax
414441: 8d 4f 18              lea ecx, [edi+0x18]
414444: e8 07 f5 ff ff        call 0x413950         ; Call internal

414449: 5e                    pop esi
41444a: 5f                    pop edi

; NULL path (return):
41444b: 5b                    pop ebx
41444c: 5d                    pop ebp
41444d: c3                    ret
```

---

## Analysis

### Global Structure at 0x4d3d54

This appears to be the **Master Database** structure:

```c
struct MasterDB {
    void* field_00;    // 0x00: Identifier/link
    void* field_04;    // 0x04
    void* field_08;    // 0x08
    void* pPrimaryObject;  // 0x0c: Primary object (vobj?)
    ...
};
```

### Behavior

When param is **NULL**:
- Returns immediately (no initialization)

When param is **non-NULL**:
1. Calls internal function at 0x413e60
2. Checks if param matches global[0] (avoid duplicates)
3. Handles linked-list insertion (offset 0x0c)
4. Calls callbacks via vtables
5. Performs registration

### Key Offsets

| Offset | Purpose |
|----------|---------|
| 0x00 | First field (identifier) |
| 0x0c | Primary object pointer |
| 0x18 | Secondary structure |

---

## Comparison with client.dll SetMasterDatabase

| Feature | launcher.exe (0x4143f0) | client.dll (0x6229d760) |
|---------|------------------------|-------------------------|
| Null check | ✅ Returns early | ✅ Returns early |
| Global storage | 0x4d3d54 | 0x629f14a0 |
| Linked list ops | ✅ Yes (0x0c) | ✅ Yes (0x0c) |
| Callbacks | Via vtable | Via vtable |

---

## Conclusion

The real launcher **accepts non-NULL parameters** and performs complex registration.

Our test's SetMasterDatabase export is **too simple** - it just logs and returns.

The real implementation would need to:
1. Store the client's database pointer
2. Set up global structures
3. Initialize the linked-list at offset 0x0c
4. Call any registered callbacks

---

**Status**: Disassembly complete
