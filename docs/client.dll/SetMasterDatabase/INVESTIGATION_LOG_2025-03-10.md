# Investigation Log: 2025-03-10

## Summary

Attempted to implement proper InitClientDLL calling sequence based on existing documentation. Discovered critical issue: SetMasterDatabase crashes when called.

## Timeline

### 10:00 - Initial Implementation
- Created `launcher_proper.cpp` with Master Database structure
- Implemented vtable functions based on MASTER_DATABASE.md
- Added DLL pre-loading from src/main.cpp

### 11:00 - First Test Run
- Built and deployed launcher_proper.exe
- Tested with Wine
- **CRASH**: `STATUS_STACK_BUFFER_OVERRUN` (0xC0000409)

### 11:30 - Added DLL Pre-loading
- Identified missing dependency pre-loading
- Added MFC71.dll, MSVCR71.dll, dbghelp.dll, r3d9.dll, binkw32.dll pre-loading
- Rebuilt and retested

### 12:00 - New Crash Location
- **CRASH** now in client.dll!SetMasterDatabase
- Address: 0x6229CB9E (client.dll+0x29cb9e)
- Error: Page fault reading from 0x00000005
- Instruction: `mov 0x04(%edx), %edx` where EDX=1

### 12:30 - Diagnostic Tool Creation
- Created `test_passing.cpp` to dump exact structure
- Verified we're passing:
  - MasterDatabase at 0x406140
  - pVTable at 0x406040
  - pPrimaryObject at 0x406100
  - All fields initialized as per docs

### 13:00 - Root Cause Analysis
- Client.dll is calling `get_or_create_master_db` helper
- This function expects different structure layout
- OR we need to call InitClientDLL BEFORE SetMasterDatabase

## Findings

### What Works
✅ All dependencies load successfully
✅ client.dll loads at 0x62000000
✅ All 5 exports found
✅ Structure layout matches documentation (36 bytes)

### What Doesn't Work
❌ SetMasterDatabase crashes when called
❌ Client.dll dereferences EDX=1 instead of valid pointer
❌ Unknown exact structure expectations

### Critical Discovery

The crash location suggests client.dll has an internal global master database at `0x629f14a0` and the `get_or_create_master_db` function (at `0x6229d110`) is failing to properly initialize it with our data.

**Alternative hypothesis**: We might have the initialization sequence wrong. Maybe InitClientDLL should be called FIRST, then SetMasterDatabase?

## Documentation Created

1. **INITIALIZATION_SEQUENCE.md**
   - Full analysis of initialization sequence
   - What works, what doesn't
   - Questions to answer

2. **INITIALIZATION_TODO.md**
   - Prioritized TODO list
   - Immediate, short-term, and long-term tasks
   - Progress tracking

## Next Steps

### Immediate (Next Session)

1. **Disassemble SetMasterDatabase** in client.dll
   - Use radare2 to map the function
   - Find exact structure expectations
   - Understand the crash

2. **Disassemble original launcher.exe**
   - Find actual initialization sequence
   - Document exact parameters passed
   - Verify our approach

3. **Test alternative sequence**
   - Try calling InitClientDLL before SetMasterDatabase
   - Try with minimal/null parameters
   - Document results

## Code Artifacts

- `launcher_proper.cpp` - Main implementation attempt
- `launcher_proper.exe` - Compiled binary (116 KB)
- `test_passing.cpp` - Diagnostic tool
- `test_passing.exe` - Diagnostic binary
- `master_database.h` - Structure definitions

## Technical Details

### Crash Stack Trace
```
0x6229cb9e in client.dll (+0x29cb9e): mov 0x04(%edx), %edx
0x6229d78e in client.dll (+0x29d78e)
our_launcher.exe
```

### Register Dump at Crash
```
EIP: 0x6229cb9e
EDX: 0x00000001  ← Problem
EBX: 0x00406140  ← Our pointer (correct)
```

### Wine Output
```
wine: Unhandled page fault on read access to 00000005
```

## Resources Used

- Documentation: `../../docs/api_surface/MASTER_DATABASE.md`
- Documentation: `../../docs/api_surface/client_dll_api_discovery.md`
- Documentation: `../../docs/api_surface/data_passing_mechanisms.md`
- Tools: i686-w64-mingw32-g++ (compiler)
- Tools: Wine 9.0 (runtime)
- Tools: GDB (debugger via MCP)

## Questions Raised

1. What is the correct order: InitClientDLL or SetMasterDatabase first?
2. What does the "identifier" field in MasterDatabase need to be?
3. Are pPrimaryObject/pSecondaryObject supposed to be real C++ objects?
4. Does client.dll expect specific vtable implementations?

## Status

**Blocked on**: SetMasterDatabase crash  
**Priority**: CRITICAL  
**Next Action**: Reverse engineer client.dll!SetMasterDatabase

## Lessons Learned

1. Documentation is good but needs verification
2. Structure layouts might not match exactly
3. Initialization order matters
4. Need to study original launcher.exe behavior
