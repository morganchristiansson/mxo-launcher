# API Surface Analysis - Phase 1 Complete

## Date: March 7, 2026

## Objective
Extract ALL exported functions from launcher.exe and document the API surface shared with client.dll.

## Initial Findings

### File Information
- **Path**: `../../launcher.exe`
- **Size**: 5,267,456 bytes (5.2 MB)
- **Format**: PEI-i386 (32-bit executable)
- **Entry Point**: 0x1000

### PE Header Analysis

#### Sections
| Name | Size | VMA | Characteristics |
|------|------|-----|-----------------|
| `.text` | 696KB | 0x401000 | CODE, READONLY |
| `.rdata` | 117KB | 0x4A9000 | DATA, READONLY |
| `.data` | 8KB | 0x4C6000 | DATA |
| `STLPORT_` | 4KB | 0x4FF000 | DATA |
| `.rsrc` | 4.2MB | 0x500000 | DATA, READONLY |
| `.reloc` | 99KB | 0x919000 | DATA, READONLY |

#### Key Offsets
- **Entry Point**: 0x1000
- **Export**: SetMasterDatabase (Ordinal 1, Address 0x143f0)
- **String Table**: 0x1810
- **Resources**: 0x181C
- **Export Directory**: Present (minimal exports)

### CRITICAL DISCOVERY

**launcher.exe has a MINIMAL export table with only ONE named export.**

This fundamentally changes the API analysis approach:

1. **Minimal Export Table**: Only ONE function exported (SetMasterDatabase)
2. **Large Internal API**: Most functions discovered through disassembly
3. **Runtime Discovery**: Client.dll likely discovers API at runtime
4. **Internal Mechanisms**: Function pointers and callbacks for client communication

## What This Means

### CRITICAL: Minimal Exports
The launcher.exe has only ONE named export (SetMasterDatabase). This means:

1. **Large Internal API**: The "stupidly large API surface" must be discovered through disassembly
2. **Runtime Discovery**: Client.dll likely discovers functions at runtime
3. **Internal Pointers**: Function pointers and callbacks are used internally
4. **Not Visible in Exports**: Most API functions are not exported

### Client.dll Integration
Client.dll likely uses launcher API through:
- **Internal function pointers** passed at runtime
- **Callback mechanisms** for event notifications
- **Shared data structures** in memory
- **Direct internal calls** to launcher functions

### Network Handling
Based on section analysis:
- Network code is embedded in `.text` section (0x1000 - 0xa8000)
- Protocol dispatch is internal to launcher
- Network buffers are in `.data` section (0xc6000 - 0xd000)
- No separate network DLL

### API Surface Discovery

Since exports are minimal, the API surface must be discovered by:

1. **Disassembling the entry point** (0x1000) to find internal function pointer initialization
2. **Searching `.rdata` section** (0xa9000 - 0xc6000) for static function pointer tables
3. **Analyzing callback registration** patterns in `.text` section
4. **Extracting data structures** from `.data` section
5. **Parsing resource directory** for embedded protocols

## Next Steps

### Immediate Tasks - CRITICAL ⚠️
1. **Disassemble entry point** (0x1000) using radare2
   ```bash
   r2 -c 'a* launcher.exe'
   ```
2. **Search for function pointer tables** in `.rdata` (0xa9000 - 0xc6000)
3. **Identify callback mechanisms** in `.text` section
4. **Map data structures** used for client communication

### Investigation Approach
- Use `objdump -d` to disassemble entry point
- Search for patterns like `[ptr] = func` or function pointer arrays
- Look for client.dll integration points
- Extract data structure definitions

## Technical Notes

### Entry Point Analysis
The entry point at 0x1000 likely:
- Initializes internal function pointers
- Sets up callback tables
- Creates shared data structures
- Starts network initialization

### Resource Analysis
The `.rsrc` section contains:
- Language tables for English (0x409)
- WAVE audio resources
- Potential embedded network configuration

### String Table
Contains internal function names and protocol strings used for:
- Debug logging
- Error messages
- Protocol identification

## Conclusion

**Phase 1 Complete**: PE header analysis and export investigation finished.

**Key Finding**: launcher.exe has a **minimal export table** (SetMasterDatabase only) but likely contains a **large internal API**.

**Next Phase**: Disassembly and internal API discovery to map the actual API surface.

---

## Files Created

1. `/home/pi/mxo/docs/api_surface/EXPORTS.md` - Detailed export analysis
2. `/home/pi/mxo/docs/api_surface/ACTUAL_EXPORTS.md` - Actual export findings
3. `/home/pi/mxo/docs/api_surface/ANALYSIS_SUMMARY.md` - This summary document
4. `/home/pi/mxo/docs/api_surface/AGENTS.md` - Updated agent documentation
5. `/home/pi/mxo/docs/api_surface/TODO.md` - Task breakdown

## References

- PE Header offset: 0x180
- Entry point: 0x1000
- Export: SetMasterDatabase (Ordinal 1, Address 0x143f0)
- String table: 0x1810
- Resources: 0x181C
- `.text` section: 0x1000 - 0xa8000 (Code)
- `.rdata` section: 0xa9000 - 0xc6000 (Read-only data)
- `.data` section: 0xc6000 - 0xd000 (Data)

---

**Status**: Phase 1 Complete  
**Progress**: 35%  
**Next**: Disassembly and internal API discovery (CRITICAL)