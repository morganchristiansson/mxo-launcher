# Initialization Sequence - TODO List

## Priority: CRITICAL

This is the current blocking issue preventing game startup.

## Immediate Actions (Today)

### 1. Reverse Engineer SetMasterDatabase
**File**: `client.dll` at address `0x6229d760`

**Tasks**:
- [ ] Disassemble complete SetMasterDatabase function
- [ ] Map the `get_or_create_master_db` helper function (0x6229d110)
- [ ] Document exact structure expectations
- [ ] Find what causes EDX=1 instead of a pointer
- [ ] Identify required vtable functions

**Tools**: radare2, GDB, x64dbg

### 2. Study Original Launcher
**File**: `launcher.exe` (original)

**Tasks**:
- [ ] Find where it loads client.dll
- [ ] Find where it calls SetMasterDatabase
- [ ] Find where it calls InitClientDLL
- [ ] Document the EXACT sequence
- [ ] Extract parameter values passed
- [ ] Document Master Database structure creation

**Addresses to investigate**:
- String reference: "client.dll"
- String reference: "InitClientDLL"
- String reference: "SetMasterDatabase"

### 3. Test Alternative Sequence
**Hypothesis**: InitClientDLL might need to be called BEFORE SetMasterDatabase

**Tasks**:
- [ ] Modify launcher_proper.cpp to call InitClientDLL first
- [ ] Test with null parameters
- [ ] Test with minimal parameters
- [ ] Document results

## Short-term Tasks (This Week)

### 4. Implement Proper VTable Functions
**Goal**: Create real C++ objects with working vtables

**Tasks**:
- [ ] Implement all vtable functions from MASTER_DATABASE.md
- [ ] Test each function individually
- [ ] Add logging to track client.dll calls
- [ ] Verify calling conventions

**VTable functions to implement** (offsets from docs):
- [ ] 0x00: Initialize
- [ ] 0x04: Shutdown
- [ ] 0x08: Reset
- [ ] 0x0C: GetState
- [ ] 0x10: RegisterCallback
- [ ] 0x58: GetApplicationState
- [ ] 0x5C: SetEventHandler
- [ ] 0x60: RegisterCallback2

### 5. Map InitClientDLL Parameters
**File**: `client.dll` at address `0x620012a0`

**Tasks**:
- [ ] Disassemble InitClientDLL
- [ ] Document what each of 6 parameters does
- [ ] Find if any are required
- [ ] Determine valid values

### 6. Create Test Suite
**Goal**: Systematic testing of initialization

**Tests**:
- [ ] Test 1: Call InitClientDLL before SetMasterDatabase
- [ ] Test 2: Call SetMasterDatabase before InitClientDLL
- [ ] Test 3: Call with minimal Master Database
- [ ] Test 4: Call with full Master Database
- [ ] Test 5: Call with null Master Database
- [ ] Document all results

## Medium-term Tasks (Next Week)

### 7. Complete API Surface Documentation
**Goal**: Document all client.dll ↔ launcher.exe interaction

**Tasks**:
- [ ] Map all vtable functions (50-100 functions estimated)
- [ ] Document callback registration mechanism
- [ ] Document event handling
- [ ] Create API reference

### 8. Debugging Infrastructure
**Goal**: Make debugging easier

**Tasks**:
- [ ] Create Wine wrapper script with debug logging
- [ ] Add GDB macros for common tasks
- [ ] Create crash dump analyzer
- [ ] Document debugging procedures

### 9. Integration Testing
**Goal**: Verify complete initialization works

**Tasks**:
- [ ] Test with all dependencies loaded
- [ ] Test RunClientDLL execution
- [ ] Verify game window appears
- [ ] Test clean shutdown via TermClientDLL

## Long-term Tasks

### 10. Create Reference Implementation
**Goal**: Working launcher that properly initializes client.dll

**Tasks**:
- [ ] Implement complete Master Database
- [ ] Implement all required vtable functions
- [ ] Add error handling
- [ ] Add logging
- [ ] Document the implementation

### 11. Protocol Analysis
**Goal**: Understand game protocol

**Tasks**:
- [ ] Capture network traffic
- [ ] Analyze packet structures
- [ ] Document protocol
- [ ] Create protocol reference

## Questions to Answer

### Critical Questions

1. **What is the correct initialization sequence?**
   - InitClientDLL then SetMasterDatabase?
   - SetMasterDatabase then InitClientDLL?
   - Something else?

2. **What does SetMasterDatabase expect?**
   - Exact structure layout?
   - Required field values?
   - VTable format?

3. **What does InitClientDLL expect?**
   - Valid parameter values?
   - Required resources?
   - Window handle?

### Important Questions

4. **How are callbacks registered?**
   - Through vtable functions?
   - Direct function pointers?
   - Event system?

5. **What triggers RunClientDLL to start?**
   - What state must be initialized?
   - What resources must be ready?

## Known Issues

### Current Blockers

1. **SetMasterDatabase crash** - EDX=1 instead of pointer
2. **Unknown InitClientDLL parameters** - No documentation
3. **Unknown structure layout** - Might not match docs

### Technical Debt

1. **No error handling** - All calls assume success
2. **No logging** - Hard to debug without traces
3. **No tests** - Manual testing only

## Resources

### Documentation
- `INITIALIZATION_SEQUENCE.md` - This investigation
- `MASTER_DATABASE.md` - Structure documentation
- `client_dll_api_discovery.md` - API discovery
- `data_passing_mechanisms.md` - Parameter passing

### Code
- `launcher_proper.cpp` - Our implementation
- `test_passing.cpp` - Diagnostic tool
- `test_master_db.cpp` - Structure test

### Binaries
- `launcher.exe` - Original launcher (5.3 MB)
- `client.dll` - Game client (11 MB)

### Tools
- radare2 - Disassembly
- GDB - Debugging
- Wine - Windows emulation

## Progress Tracking

### Completed
- ✅ Dependency loading (MFC71, MSVCR71, dbghelp, r3d9, binkw32)
- ✅ client.dll loading
- ✅ Export resolution (all 5 functions found)
- ✅ Structure layout defined (36 bytes)
- ✅ Diagnostic tools created

### In Progress
- 🔄 SetMasterDatabase reverse engineering
- 🔄 Initialization sequence investigation
- 🔄 VTable implementation

### Blocked
- ⏸️ InitClientDLL parameter mapping (waiting on SetMasterDatabase)
- ⏸️ RunClientDLL execution (waiting on InitClientDLL)
- ⏸️ Game window display (waiting on RunClientDLL)

## Updates

### 2025-03-10
- Created investigation document
- Identified crash in SetMasterDatabase
- Created diagnostic tools
- Updated TODO list
