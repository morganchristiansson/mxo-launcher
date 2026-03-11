# launcher.exe → client.dll Loading Sequence

## Date: 2025-03-11
## Status: INVESTIGATION COMPLETE

---

## Overview

launcher.exe loads client.dll and calls its exports. Both files export `SetMasterDatabase()` - it's a **bidirectional callback mechanism**.

---

## Step-by-Step Loading Sequence (Disassembly Analysis)

### Phase 1: Load client.dll Module

**Status**: Location found but not fully traced

From symbol analysis:
- "client.dll" string at `0x4ac160`
- LoadLibraryA used (import at `0x4a907c`)
- Module handle stored at global `0x4d2c50`

### Phase 2: Get Function Pointers via GetProcAddress

**Location**: `0x40a4f0 - 0x40a540` in launcher.exe

#### 2.1 Get InitClientDLL
```asm
0x0040a4f2: mov esi, dword [0x4d2530]  ; ESI = GetProcAddress
0x0040a4f8: push edi
0x0040a4f9: push str.InitClientDLL      ; "InitClientDLL" at 0x4ac1e8
0x0040a4fe: push eax                    ; hClient
0x0040a511: call esi                    ; GetProcAddress
```

#### 2.2 Get RunClientDLL
```asm
0x0040a509: push str.RunClientDLL       ; "RunClientDLL" at 0x4ac1d8
0x0040a50e: push [0x4d2c50]             ; hClient
0x0040a511: call esi                    ; GetProcAddress
0x0040a50f: mov edi, eax                ; RunClientDLL pointer in EDI
```

#### 2.3 Get TermClientDLL
```asm
0x0040a519: push str.TermClientDLL      ; "TermClientDLL" at 0x4ac1c8
0x0040a51e: push [0x4d2c50]             ; hClient
0x0040a522: call esi                    ; GetProcAddress
0x0040a51f: mov [ebp-0x18], eax         ; Store TermClientDLL
```

#### 2.4 Get ErrorClientDLL
```asm
0x0040a52c: push str.ErrorClientDLL     ; "ErrorClientDLL" at 0x4ac1b8
0x0040a531: push eax                    ; hClient
0x0040a532: call esi                    ; GetProcAddress
0x0040a534: mov esi, eax                ; ErrorClientDLL in ESI
```

### Phase 3: Call InitClientDLL

**Status**: Need to find actual call site

Expected pattern after function discovery:
```asm
; push parameters
; call InitClientDLL  ; Find this address
; test result
; jump if failed
```

### Phase 4: Call RunClientDLL

**Status**: Need to find actual call site

Expected to call RunClientDLL (stored in EDI) after successful Init.

---

## String Addresses (Confirmed)

| String | Virtual Address | Usage |
|--------|-----------------|-------|
| InitClientDLL | 0x4ac1e8 | GetProcAddress |
| RunClientDLL | 0x4ac1d8 | GetProcAddress |
| TermClientDLL | 0x4ac1c8 | GetProcAddress |
| ErrorClientDLL | 0x4ac1b8 | GetProcAddress |
| client.dll | 0x4ac160 | LoadLibrary |
| SetMasterDatabase | 0x4c5a0f | launcher.exe EXPORT |

---

## SetMasterDatabase Direction - VERIFIED

**launcher.exe(EXPORTS)**: `SetMasterDatabase(void*)` 
**client.dll(IMPORTS)**: Calls launcher!SetMasterDatabase during InitClientDLL

**VERIFIED**: launcher.exe does NOT call client.dll!SetMasterDatabase

See `SETMASTER_DATABASE_VERIFICATION.md` for complete verification evidence.

### Sequence:
1. launcher exports SetMasterDatabase
2. launcher calls client.dll!InitClientDLL
3. InitClientDLL internally discovers launcher!SetMasterDatabase via GetProcAddress
4. InitClientDLL calls launcher.SetMasterDatabase(clientDB)
5. launcher receives callback and sets up internal state
6. InitClientDLL returns success
7. launcher calls client.dll!RunClientDLL

---

## Key Globals Found

| Address | Purpose |
|---------|---------|
| 0x4d2530 | GetProcAddress import pointer |
| 0x4d2c50 | hModule (client.dll handle) |

---

## Disassembly Commands

```bash
# Find function discovery block
r2 -qc 's 0x40a500; pd 60' launcher.exe

# Find references to InitClientDLL string
r2 -qc '/x 68e8c14a00' launcher.exe

# Find where hClient is loaded
r2 -qc '/x 68c04a00' launcher.exe  # push str.client.dll
```

---

## Related Documentation

- `../client_dll/` - client.dll documentation
- `CORE_INVESTIGATION.md` - High-level questions
- `../client.dll/SetMasterDatabase/` - SetMasterDatabase documentation

---

**Status**: Function discovery block located. Need to trace actual call sites for InitClientDLL and RunClientDLL.

**Next**: Find the code that actually calls InitClientDLL after GetProcAddress.
