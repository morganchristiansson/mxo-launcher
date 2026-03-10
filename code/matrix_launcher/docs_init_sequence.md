# InitClientDLL Next Steps

## Current Status

✅ Successfully loading all dependencies (MFC71, MSVCR71, dbghelp, r3d9, binkw32)
✅ Successfully loading client.dll
✅ Found SetMasterDatabase export at 0x6229d760
❌ Crash when calling SetMasterDatabase - trying to dereference EDX=1

## Problem Analysis

The crash happens in `client.dll` at offset `0x29cb9e`:
```assembly
mov 0x04(%edx), %edx    ; EDX=1, trying to read from 0x00000005
```

This is INSIDE the SetMasterDatabase call flow, specifically in the `get_or_create_master_db` helper function.

### Root Cause

From documentation in `../../docs/api_surface/MASTER_DATABASE.md` and `client_dll_api_discovery.md`:

1. **SetMasterDatabase calls `get_or_create_master_db`** which creates client.dll's internal structure
2. Client.dll has its OWN global master database at `0x629f14a0`
3. Client.dll expects specific object layouts that we haven't reverse engineered yet

### What We Need to Reverse Engineer

The exact structure that `SetMasterDatabase` expects. According to the docs:

```assembly
SetMasterDatabase(0x6229d760):
    mov ebx, [ebp+8]           ; Get parameter (master database pointer)
    test ebx, ebx              ; Validate not NULL
    je .exit                   ; Exit if NULL
    
    call get_or_create_master_db  ; Initialize local structure (CRASH HERE)
    mov edi, [0x629f14a0]      ; Load global master database
```

The crash is in `get_or_create_master_db` (at `0x6229d110` according to docs).

## Next Steps

### Option 1: Use GDB to disassemble client.dll
We need to reverse engineer the actual SetMasterDatabase function to understand:
- What structure it expects
- What the "identifier" field should be
- What the primary/secondary objects should contain

### Option 2: Skip SetMasterDatabase and call InitClientDLL directly
According to the documentation flow:
1. launcher.exe creates master database
2. launcher.exe calls **InitClientDLL FIRST** (with parameters)
3. launcher.exe calls SetMasterDatabase

We might have the order wrong!

### Option 3: Study the original launcher.exe
Disassemble the original launcher.exe to see how IT calls these functions.

## Recommended Approach

1. **Disassemble client.dll!SetMasterDatabase** to see exact expectations
2. **Disassemble original launcher.exe** to see how it initializes client.dll
3. **Try calling InitClientDLL BEFORE SetMasterDatabase** (might be the correct order!)

## Files to Study

- `../../docs/api_surface/data_passing_mechanisms.md` - InitClientDLL parameter details
- `../../docs/api_surface/MASTER_DATABASE.md` - Complete structure layout
- Original `launcher.exe` disassembly at the initialization sequence
