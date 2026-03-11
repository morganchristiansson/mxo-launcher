# Initialization Sequence Analysis

## Overview

**Status**: IN PROGRESS  
**Last Updated**: 2025-03-10  
**Related**: MASTER_DATABASE.md, client_dll_api_discovery.md

This document describes the initialization sequence between launcher.exe and client.dll based on reverse engineering efforts.

## Current Understanding

### Exported Functions (client.dll)

| Function | Address | Purpose |
|----------|---------|---------|
| `ErrorClientDLL` | 0x620011d0 | Error handling |
| `InitClientDLL` | 0x620012a0 | Primary initialization |
| `RunClientDLL` | 0x62001180 | Main execution loop |
| `SetMasterDatabase` | 0x6229d760 | API registration |
| `TermClientDLL` | 0x620011a0 | Cleanup and termination |

### Initialization Flow (Theoretical)

Based on documentation, the sequence should be:

```
1. launcher.exe starts
2. launcher.exe loads client.dll via LoadLibraryA()
3. launcher.exe creates Master Database structure (36 bytes)
4. launcher.exe calls SetMasterDatabase(masterDB_ptr) 
   ↓
   client.dll stores pointer at 0x629f14a0
   client.dll initializes internal objects
   client.dll calls launcher vtable functions
   ↓
5. launcher.exe calls InitClientDLL(params...)
6. launcher.exe calls RunClientDLL()
7. Game runs
8. launcher.exe calls TermClientDLL() on exit
```

### Alternative Flow (Needs Investigation)

**CRITICAL**: Documentation suggests InitClientDLL might need to be called BEFORE SetMasterDatabase!

```
1. launcher.exe loads client.dll
2. launcher.exe calls InitClientDLL(params...) ← FIRST?
3. launcher.exe calls SetMasterDatabase(masterDB_ptr) ← SECOND?
4. launcher.exe calls RunClientDLL()
```

**This needs to be verified by disassembling the original launcher.exe!**

## Current Implementation Status

### What Works

✅ Loading all dependency DLLs:
- MFC71.dll
- MSVCR71.dll  
- dbghelp.dll (mxowrap.dll)
- r3d9.dll
- binkw32.dll
- pythonMXO.dll
- dllMediaPlayer.dll
- dllWebBrowser.dll

✅ Loading client.dll successfully

✅ Finding all exported functions

### What Doesn't Work

❌ Calling SetMasterDatabase - crashes with:
```
page fault on read access to 0x00000005
at address 0x6229CB9E (client.dll+0x29cb9e)
instruction: mov 0x04(%edx), %edx
where EDX = 0x00000001
```

❌ Unknown InitClientDLL parameter requirements

❌ Unknown Master Database structure exact layout

## Crash Analysis

### Location

The crash occurs in `client.dll` at offset `0x29cb9e` inside the `SetMasterDatabase` call flow.

### Assembly Context

```assembly
SetMasterDatabase(0x6229d760):
    mov ebx, [ebp+8]           ; Get master database pointer (SUCCESS)
    test ebx, ebx              ; Validate not NULL (SUCCESS)
    je .exit                   
    
    call get_or_create_master_db  ; ← CRASH HAPPENS HERE
    mov edi, [0x629f14a0]      ; Load global master database
    cmp ebx, [edi]             ; Compare with identifier
    ...
```

The crash is in the `get_or_create_master_db` helper function at address `0x6229d110`.

### Stack Trace

```
0 0x6229cb9e in client (+0x29cb9e)
1 0x6229d78e in client (+0x29d78e)
2 our_launcher.exe
```

### Register State at Crash

```
EIP: 0x6229cb9e
EDX: 0x00000001  ← Problem: trying to read EDX+4 = 0x00000005
EBX: 0x00406140  ← Our MasterDatabase pointer (correct)
EAX: 0x00982d8c
```

### Root Cause

Client.dll is trying to dereference what should be a pointer to an object, but instead getting the value `1`. This suggests:

1. **Wrong structure layout** - Our struct doesn't match what client.dll expects
2. **Wrong field values** - We're setting fields incorrectly
3. **Missing initialization** - Client.dll expects some setup we haven't done

## Master Database Structure

### What We Know (From Documentation)

```c
struct MasterDatabase {
    void* pVTable;              // Offset 0x00: Virtual function table
    uint32_t refCount;          // Offset 0x04: Reference count
    uint32_t stateFlags;        // Offset 0x08: State flags
    void* pPrimaryObject;       // Offset 0x0C: Primary API object
    uint32_t primaryData1;      // Offset 0x10: Unknown
    uint32_t primaryData2;      // Offset 0x14: Unknown
    void* pSecondaryObject;     // Offset 0x18: Secondary API object
    uint32_t secondaryData1;    // Offset 0x1C: Unknown
    uint32_t secondaryData2;    // Offset 0x20: Unknown
};  // Total: 36 bytes (0x24)
```

### What We're Passing

```
[0x00] pVTable:          0x406040 (our vtable)
[0x04] refCount:         1
[0x08] stateFlags:       0x1
[0x0C] pPrimaryObject:   0x406100
[0x10] primaryData1:     0
[0x14] primaryData2:     0
[0x18] pSecondaryObject: 0x4060c0
[0x1C] secondaryData1:   0
[0x20] secondaryData2:   0
```

### What Client.dll Expects

**UNKNOWN** - This is what needs to be reverse engineered!

The crash suggests client.dll is reading from an object pointer field and getting `1` instead, which means either:
- The struct layout is wrong
- The pPrimaryObject/pSecondaryObject fields aren't valid pointers to C++ objects
- Client.dll expects a different identifier/vtable at offset 0

## Next Steps

### Immediate Tasks

1. **Disassemble SetMasterDatabase** in client.dll to understand exact expectations
2. **Disassemble original launcher.exe** to see actual initialization sequence
3. **Try calling InitClientDLL BEFORE SetMasterDatabase** (might be the correct order)

### Reverse Engineering Tasks

1. **Map client.dll!SetMasterDatabase** (0x6229d760):
   - What does `get_or_create_master_db` expect?
   - What fields does it read from the parameter?
   - What should the "identifier" field be?

2. **Map client.dll!InitClientDLL** (0x620012a0):
   - What are the 6 parameters?
   - What do they mean?
   - Are any required before SetMasterDatabase?

3. **Study original launcher.exe**:
   - How does it initialize client.dll?
   - What order does it call functions?
   - What does it pass?

### Implementation Tasks

1. Create proper C++ objects with vtables (not just structs)
2. Implement actual vtable functions that client.dll can call
3. Test different initialization sequences
4. Add proper error handling and logging

## Files to Study

### Documentation

- `../../docs/api_surface/MASTER_DATABASE.md` - Master database structure
- `../../docs/api_surface/client_dll_api_discovery.md` - API discovery mechanism
- `../../docs/api_surface/data_passing_mechanisms.md` - Parameter passing

### Code

- `../../launcher.exe` - Original launcher (needs disassembly)
- `../../client.dll` - Client library (needs disassembly of SetMasterDatabase)

## Test Programs

- `launcher_proper.cpp` - Our attempt at proper initialization
- `test_passing.cpp` - Diagnostic showing what we pass
- `test_master_db.cpp` - Structure validation

## Questions

1. **Should InitClientDLL be called before SetMasterDatabase?**
2. **What is the "identifier" field at offset 0 supposed to be?**
3. **Are pPrimaryObject/pSecondaryObject supposed to be C++ objects with vtables?**
4. **Does client.dll expect specific vtable functions to be implemented?**

## References

- Original forum post about client.dll exports
- radare2 analysis of client.dll
- Wine debugging output
