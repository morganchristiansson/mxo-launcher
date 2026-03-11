# API Surface Agent Tasks - CRITICAL PRIORITY ⚠️

## Agent: API_ANALYST

### Overview
Document the **stupidly large API surface** that launcher.exe shares with client.dll. This API handles all TCP communications and client coordination.

### NEW TOOLS AVAILABLE
- **radare2** (`r2`): Disassembly and binary analysis
- **readpe**: PE header and export analysis

### Usage
```bash
# Disassemble and analyze
r2 -c 'a* launcher.exe'

# Show exports
readpe --exports launcher.exe

# Show all headers
readpe --all launcher.exe
```

---

## Phase 1: Export Discovery (COMPLETED)
- [x] Extract PE headers using readpe
- [x] Analyze export table structure
- [x] Document minimal exports (SetMasterDatabase only)
- [x] Identify need for disassembly

### Findings
- **Only ONE named export**: SetMasterDatabase (Ordinal 1, Address 0x143f0)
- **263 ordinal entries** (mostly imported DLL functions)
- **Minimal export table** suggests internal API discovery
- **Large internal API** must be found through disassembly

---

## Phase 2: Disassembly - CRITICAL ⚠️

### 2.1 Entry Point Analysis
- [x] Disassemble entry point (0x1000) with radare2
- [x] Identify initialization sequence
- [x] Find internal function pointer setup
- [x] Document entry point behavior

**Command**: `r2 -q -c 's entry0; pd 150' ../../launcher.exe`

**Findings**: See `entry_point_analysis.md`
- Entry point at 0x0048be94 (not 0x1000 as documented)
- Uses MSVC CRT initialization sequence
- Transfers to MFC71.DLL framework
- SetMasterDatabase export at 0x004143f0 analyzed
- MFC architecture discovered (critical for API discovery)

### 2.2 Internal Function Pointer Tables
- [x] Search `.rdata` section (0xa9000 - 0xc6000) for pointer arrays
- [x] Identify static function pointer tables
- [x] Document pointer structures
- [x] Map callback function pointers

**Location**: `.rdata` section (0x4a9000 - 0x4c6000)

**MAJOR DISCOVERY** ✅
- **117 function pointer tables** found in .rdata section
- **5,145 internal function pointers** discovered
- **Largest table**: 250 entries at 0x4b62b8
- **Second largest**: 165 entries at 0x4b4fc4
- All pointers reference internal code in .text section (0x401000 - 0x4a8000)
- This confirms the "stupidly large API surface" hypothesis
- Tables represent virtual function tables (vtables) and callback tables
- See `function_pointer_tables.md` for complete analysis

### 2.3 Callback Registration
- [x] Find callback registration patterns in `.text`
- [x] Identify registration function signatures
- [x] Document callback mechanism
- [x] Map notification functions

**Pattern**: Function pointer assignments in `.text` section

**MAJOR DISCOVERY** ✅
- Found **15+ distinct vtable registration locations**
- Registration uses constructor pattern: `mov dword [object], vtable_addr`
- Identified **4 primary registration functions** (0x401000, 0x4012d0, 0x4017b0, 0x401d10)
- Mapped **10+ object types** with different vtables
- Documented **callback flow** from user action to handler
- See `callback_registration_analysis.md` for complete analysis

### 2.4 Data Structures
- [x] Extract structures from `.data` section (0xc6000 - 0xd000)
- [x] Identify session objects
- [x] Document packet structures
- [x] Map connection objects

**Location**: `.data` section

**MAJOR DISCOVERY** ✅
- **Function pointer tables**: 3,000+ runtime-modifiable function pointers
- **ILTLDLL structures**: Plugin architecture identification system
- **Numeric lookup tables**: Prime-based hash/message routing tables
- **Network infrastructure**: zlib compression + DES encryption support
- **Session/Connection objects**: Inferred structures from vtable patterns
- See `data_structures_analysis.md` for complete analysis

---

## Phase 3: Client.dll Integration - HIGH ⚠️

### 3.1 Client.dll API Discovery
- [x] Analyze how client.dll discovers launcher functions
- [x] Find runtime discovery mechanisms
- [x] Document API lookup patterns
- [x] Map client.dll function calls

**MAJOR DISCOVERY** ✅
- **Mechanism**: Runtime registration via SetMasterDatabase export
- **No static imports**: All launcher functions discovered at runtime
- **VTable-based dispatch**: Client.dll calls launcher through vtables
- **Master Database**: 36-byte structure passed from launcher to client
- **Global storage**: 0x629f14a0 in client.dll
- **API Surface**: 50-100+ functions estimated across multiple vtables
- **Bidirectional**: Both launcher→client and client→launcher communication
- See `client_dll_api_discovery.md` for complete analysis

### 3.2 Runtime Callbacks
- [x] Find callback registration in client.dll
- [x] Document callback flow
- [x] Map event notification system
- [x] Identify client event handlers

**MAJOR DISCOVERY** ✅
- **Callback Registration**: VTable-based bidirectional callback system
- **Master Database**: Object pointers at 0x629f14a0 with vtables
- **Callback Objects**: Function pointers at offsets 0x20, 0x24, 0x28
- **Event Categories**: Lifecycle, Network, Game, UI, Monitor (50-100 total callbacks)
- **VTable Dispatch**: Primary invocation mechanism (no static imports)
- See `client_dll_callback_analysis.md` for complete analysis

### 3.3 Data Passing Mechanisms
- [x] Map how client.dll passes data to launcher
- [x] Document parameter structures
- [x] Identify shared memory regions
- [x] Map data transfer flows

**MAJOR DISCOVERY** ✅
- **Multi-layered architecture**: Parameters, master database, vtables, callbacks
- **Master database**: 36-byte central structure for API discovery
- **VTable dispatch**: Primary mechanism for 50-100+ function calls
- **Callback system**: Bidirectional event notification (10-20 types)
- **No shared memory**: All sharing via pointer passing
- **Runtime initialization**: All structures created dynamically
- **InitClientDLL parameters**: 6 parameters including window/context data
- **Data structures**: Command line, window creation, callback registration, network packets
- See `data_passing_mechanisms.md` for complete analysis

---

## Phase 4: API Surface Documentation - CRITICAL ⚠️

### 4.1 Function Signatures
- [ ] Reverse engineer internal function signatures
- [ ] Document parameter types and sizes
- [ ] Map return value handling
- [ ] Document error codes

### 4.2 Complete API Reference
- [ ] Create comprehensive API documentation
- [ ] Document all discovered functions
- [ ] Map function relationships
- [ ] Create usage examples

### 4.3 Callback Documentation
- [ ] Document all callback mechanisms
- [ ] Map notification functions
- [ ] Document event handlers
- [ ] Create callback reference

---

## Phase 5: Protocol Analysis - HIGH ⚠️

### 5.1 Network Code Location
- [ ] Find TCP handling code in launcher
- [ ] Identify protocol dispatch logic
- [ ] Map network buffer management
- [ ] Document session management

### 5.2 Protocol Structures
- [ ] Extract packet structures
- [ ] Document message formats
- [ ] Map protocol headers
- [ ] Identify message types

---

## Immediate Actions (TODAY)

### Priority 1 - Disassembly
1. **✅ COMPLETED: Disassemble entry point** using radare2
   - Found entry point at 0x0048be94
   - Documented CRT initialization sequence
   - Discovered MFC framework usage
   - Analyzed SetMasterDatabase export
2. **✅ COMPLETED: Find internal function pointers** in `.rdata`
   - Discovered 117 function pointer tables
   - 5,145 internal function pointers mapped
   - Complete analysis in `function_pointer_tables.md`
   - This is the "stupidly large API surface" we were looking for
3. **✅ COMPLETED: Identify callback registration** patterns
   - Found 15+ vtable registration locations
   - Documented constructor pattern usage
   - Mapped 4 primary registration functions
   - See `callback_registration_analysis.md`
4. **✅ COMPLETED: Document data structures** in `.data`
   - Mapped 3,000+ function pointers in .data section
   - Identified ILTLDLL plugin architecture
   - Documented numeric lookup tables
   - Inferred session/connection object layouts
   - See `data_structures_analysis.md`

### Priority 2 - Client.dll
1. **✅ COMPLETED: Analyze client.dll** for API discovery
   - Discovered SetMasterDatabase registration mechanism
   - Found runtime vtable-based dispatch
   - No static imports from launcher.exe
   - See `client_dll_api_discovery.md`
2. **✅ COMPLETED: Map runtime callbacks** (Phase 3.2)
   - Found callback registration in InitClientDLL
   - Documented bidirectional callback flow
   - Mapped event notification system
   - Identified 5 callback categories
   - See `client_dll_callback_analysis.md`
3. **✅ COMPLETED: Document data passing mechanisms** (Phase 3.3)
   - Mapped multi-layered data architecture
   - Documented master database structure (36 bytes)
   - Identified vtable dispatch as primary mechanism
   - No shared memory - all via pointer passing
   - See `data_passing_mechanisms.md`

---

## Status
- **Priority**: CRITICAL (1)
- **Progress**: Phase 1 Complete, Phase 2 Complete, Phase 3 Complete (100%)
- **Next Task**: Phase 4 - API Surface Documentation (CRITICAL)
- **Deadline**: Immediate
- **Tool**: radare2 (`r2`)

---

## Notes
- **CRITICAL**: Minimal exports - API must be discovered through disassembly
- **Large internal API** exists but is not visible in export table
- **Client.dll** likely discovers API at runtime
- **All network code** handled internally by launcher
- **SetMasterDatabase** is the only static export - rest is internal

---

## File References
- **Main executable**: `../../launcher.exe` (5,267,456 bytes)
- **Entry point**: 0x1000
- **Export**: SetMasterDatabase (Ordinal 1, Address 0x143f0)
- **Sections**:
  - `.text`: 0x1000 - 0xa8000 (Code)
  - `.rdata`: 0xa9000 - 0xc6000 (Read-only data)
  - `.data`: 0xc6000 - 0xd000 (Data)
  - `STLPORT`: 0xff000 (Runtime)
  - `.rsrc`: 0x100000 - 0x519000 (Resources)