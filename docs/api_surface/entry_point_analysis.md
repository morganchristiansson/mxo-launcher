# Entry Point Analysis - launcher.exe

**Task**: Phase 2.1 - Disassemble entry point with radare2  
**Status**: COMPLETED  
**Date**: 2025-06-20  
**Binary**: launcher.exe (5,267,456 bytes)  
**Architecture**: x86 (32-bit), PE32  
**Entry Point**: 0x0048be94 (virtual address)

---

## Executive Summary

Successfully disassembled and analyzed the entry point of launcher.exe. The executable uses a standard MSVC CRT (C Runtime) initialization sequence before transferring control to MFC71 (Microsoft Foundation Classes) framework. The launcher is a Windows GUI application that uses MFC for its core functionality.

---

## Key Findings

### 1. Entry Point Location

- **Virtual Address**: 0x0048be94
- **Physical Address**: 0x0008be94
- **Type**: CRT Startup Code
- **Framework**: MFC71 (Microsoft Foundation Classes)

### 2. Initialization Sequence

The entry point follows a standard Windows CRT initialization pattern:

#### Phase 1: CRT Initialization (0x0048be94 - 0x0048bf4c)
1. **PE Header Validation** (0x0048beae - 0x0048befa)
   - Validates MZ signature (0x5A4D)
   - Validates PE signature (0x4550)
   - Checks PE format version (0x10b or 0x20b)

2. **Runtime Setup**
   - Calls `__set_app_type(2)` - Sets application type
   - Initializes global variables
   - Calls `__p__fmode()` - File mode initialization
   - Calls `__p__commode()` - Commit mode initialization
   - Sets up `_adjust_fdiv` for floating-point operations

3. **Command Line Parsing** (0x0048bf9c)
   - Calls `__getmainargs()` to parse command-line arguments
   - Sets up argc, argv, and environ

4. **Windows Integration** (0x0048bff8)
   - Calls `GetStartupInfoA()` to retrieve startup information
   - Configures application show mode

#### Phase 2: Application Initialization (0x0048c014)
- **Main Function Call**: Calls function at 0x00490c69
- This function immediately jumps to `MFC71.DLL_Ordinal_1207`
- **Key Finding**: The launcher is built on MFC framework

### 3. Export Function: SetMasterDatabase

**Location**: 0x004143f0  
**Signature**: `void __stdcall SetMasterDatabase(void* database)`

#### Function Analysis:
```
SetMasterDatabase:
  - Validates database pointer
  - Calls initialization function at 0x413e60
  - Stores database pointer in global variable (0x4d3d54)
  - Performs reference counting (0x4d3d58)
  - Calls internal initialization routines
  - Returns void
```

#### Key Observations:
- Single exported function (Ordinal 1)
- Appears to set a master database pointer
- Uses global state management
- Implements reference counting mechanism

---

## Binary Structure

### Sections Identified:

| Section | Start VA | End VA | Purpose |
|---------|----------|--------|---------|
| `.text` | 0x001000 | 0x0a8000 | Code segment |
| `.rdata` | 0x0a9000 | 0x0c6000 | Read-only data |
| `.data` | 0x0c6000 | 0x0d0000 | Data segment |
| `STLPORT` | 0x0ff000 | 0x100000 | STL runtime |
| `.rsrc` | 0x100000 | 0x519000 | Resources |

### Import Dependencies:

#### Major DLLs:
1. **MFC71.DLL** - Microsoft Foundation Classes
2. **MSVCR71.dll** - Microsoft C Runtime
3. **KERNEL32.dll** - Windows API

#### Key Imports:
- `GetModuleHandleA` - Module handle retrieval
- `GetStartupInfoA` - Startup configuration
- `__getmainargs` - Command-line parsing
- `_setmbcp` - Code page setup
- MFC71 ordinal functions (578, 1207, etc.)

---

## MFC Integration

### Discovery:
The entry point reveals **heavy MFC usage**:
- Application entry transfers to MFC71.DLL
- Uses MFC runtime initialization
- Implements MFC message loop pattern

### MFC Functions Identified:
1. **Ordinal 1207**: Main initialization routine
2. **Ordinal 578**: Constructor/destructor operations

This indicates the launcher uses MFC document/view architecture for its UI and database handling.

---

## Internal Function Pointers

### Key Functions Discovered:

| Address | Purpose | Notes |
|---------|---------|-------|
| 0x00490c69 | Main entry wrapper | Calls MFC initialization |
| 0x00413e60 | Initialization helper | Called by SetMasterDatabase |
| 0x004139d0 | Database operations | Part of database management |
| 0x00413950 | Cleanup routine | Resource management |
| 0x00413b30 | Validation routine | Database validation |
| 0x00409790 | Memory management | Free operation |

### Global Variables:

| Address | Type | Purpose |
|---------|------|---------|
| 0x4d3d54 | void* | Master database pointer |
| 0x4d3d58 | int | Reference counter |
| 0x4fea18 | DWORD | Runtime flag 1 |
| 0x4fea1c | DWORD | Runtime flag 2 |
| 0x4fea14 | DWORD | Floating-point adjustment |

---

## API Surface Discovery

### Finding:
The **internal API is NOT exposed through exports**. The large API surface mentioned in requirements is accessed through:

1. **Virtual Function Tables** - Common in MFC applications
2. **Function Pointer Structures** - Likely in `.rdata` section
3. **MFC Message Maps** - Windows message handling
4. **Runtime Registration** - Dynamic function discovery

### Next Steps Required:
- Analyze `.rdata` section for vtables (0xa9000 - 0xc6000)
- Identify MFC message map structures
- Map database interface functions
- Find client.dll communication points

---

## Implications for Client.dll Integration

### Communication Mechanism:
1. **SetMasterDatabase** is the entry point for client.dll
2. Client.dll likely passes a database object pointer
3. Launcher stores pointer globally for all operations
4. MFC framework handles UI and event processing

### Hypothesis:
- Client.dll calls `SetMasterDatabase` at initialization
- Launcher exposes internal functions through the database object
- Communication uses virtual function calls through database interface
- Network code is integrated into database/database controller classes

---

## Technical Details

### Calling Convention:
- **Primary**: `__stdcall` (Windows API standard)
- **MFC**: `__thiscall` (C++ member functions)
- **Exports**: `__stdcall`

### Build Information:
- **Compiler**: Microsoft Visual C++ .NET 2003 (7.1)
- **MFC Version**: 7.1 (MFC71.DLL)
- **Debug Symbols**: Available (launcher.pdb)
- **Build Date**: Sat Sep 13 05:51:14 2008

---

## Tools Used

- **radare2 (r2)**: Disassembly and binary analysis
- **objdump**: PE header verification
- **readpe**: Export table analysis (attempted, not available)

### Commands Executed:
```bash
# Entry point discovery
r2 -q -c 'ie' ../../launcher.exe

# Entry point disassembly
r2 -q -c 's entry0; pd 150' ../../launcher.exe

# Export analysis
r2 -q -c 'iE' ../../launcher.exe

# SetMasterDatabase analysis
r2 -q -c 's 0x004143f0; pd 100' ../../launcher.exe
```

---

## Recommendations for Phase 2.2

### Immediate Next Steps:

1. **Analyze .rdata Section** (0xa9000 - 0xc6000)
   - Search for virtual function tables
   - Identify MFC message maps
   - Find database interface structures

2. **Map MFC Classes**
   - Identify CWinApp derivative
   - Find CDocument/CView classes
   - Map message handlers

3. **Database Interface Discovery**
   - Analyze database object structure
   - Map virtual function table
   - Identify callback mechanisms

4. **Client.dll Integration Points**
   - Find where client.dll loads
   - Identify API discovery mechanism
   - Map function registration

### Priority: CRITICAL
The MFC architecture suggests a sophisticated API hidden behind virtual function tables and message maps. Manual vtable reconstruction will be necessary.

---

## Files Generated

- `entry_point_analysis.md` - This document
- TODO.md - Updated with completion status

---

## Conclusion

The entry point analysis reveals a well-structured MFC application with a single export point for database initialization. The large internal API surface is accessed through MFC's object-oriented architecture rather than explicit exports. Understanding the database object structure and MFC class hierarchy will be crucial for mapping the complete API surface.

**Status**: Phase 2.1 COMPLETED  
**Next Task**: Phase 2.2 - Internal Function Pointer Tables (CRITICAL)
