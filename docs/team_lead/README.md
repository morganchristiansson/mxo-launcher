# Matrix Online Launcher Reverse Engineering Documentation

## Project Overview
Documenting the launcher.exe from the discontinued Matrix Online game (2001-2004).

## File Information
- **File**: launcher.exe
- **Size**: 5,267,456 bytes (~5 MB)
- **Type**: PE32 executable (GUI) Intel 80386, for MS Windows
- **Sections**: 6 sections

## Documentation Structure

### 1. [PE Headers](./pe_headers/README.md)
- DOS Header
- PE Signature
- COFF Header
- Optional Header (PE32)
- Data Directories

### 2. [Imports](./imports/README.md)
- Imported DLLs and functions
- API calls used by the launcher

### 3. [Exports](./exports/README.md)
- Exported functions
- Custom DLL exports

### 4. [Resources](./resources/README.md)
- Icon resources
- Dialog boxes
- Menu resources
- String tables
- Bitmaps

### 5. [Sections](./sections/README.md)
- `.text` - Code
- `.data` - Initialized data
- `.rdata` - Read-only data
- `.bss` - Uninitialized data
- `.reloc` - Relocation information
- `.rsrc` - Resources

### 6. [String Table](./string_table/README.md)
- All embedded strings
- UI text
- Error messages
- Game messages

### 7. [Analysis](./analysis/README.md)
- Disassembly notes
- Flow analysis
- Heuristics
- Findings

## Quick Commands

```bash
# Analyze PE structure
pefile launcher.exe

# Extract strings
strings launcher.exe > string_table.txt

# Extract resources
resdump launcher.exe

# View sections
objdump -h launcher.exe
```

## Tools Used
- `pefile` - Python PE parser
- `strings` - String extraction
- `objdump` - Object file analysis
- `resdump` - Resource extraction
- `windbg` - Debugging (if available)
- `IDA Pro` - Disassembly (if available)

## Status
- **Initial Analysis**: In Progress
- **Documentation Coverage**: 0%
- **Functions Identified**: 0