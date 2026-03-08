# Launcher.exe Actual Export Analysis

**Date**: March 7, 2026  
**File**: `launcher.exe` (5,267,456 bytes)  
**Tool**: readpe --all

## Executive Summary

**CRITICAL FINDING**: launcher.exe has a **minimal export table** with only ONE named export.

This fundamentally changes our understanding of the API surface. The launcher does NOT expose a traditional large API to client.dll. Instead, the API must be discovered through:

1. Internal function pointer tables
2. Callback mechanisms
3. Runtime registration
4. Direct internal calls

## Export Table Analysis

### Named Exports (1 function)

| Ordinal | Address | Name |
|---------|---------|------|
| 1 | 0x143f0 | SetMasterDatabase |

### Key Observations

1. **Minimal Export Table**: Only one function is explicitly exported
2. **Ordinal 1**: Suggests this is the primary API entry point
3. **Function Name**: SetMasterDatabase implies database/session management

### What This Means

The launcher.exe does NOT contain a traditional large API surface. The "stupidly large API" mentioned in the critical discovery must be:

1. **Internal function pointers** - Not exported, but used internally
2. **Runtime-discovered** - Client.dll discovers functions at runtime
3. **Callback-based** - Functions registered dynamically
4. **Hidden from static analysis** - Not visible in export table

## PE Structure

### Sections

| Name | Size | VMA | Characteristics |
|------|------|-----|-----------------|
| .text | 0xa8000 (688KB) | 0x1000 | CODE, EXECUTE, READONLY |
| .rdata | 0x1d000 (117KB) | 0xa9000 | DATA, READONLY |
| .data | 0xd000 (53KB) | 0xc6000 | DATA, READ/WRITE |
| STLPORT | 0x1000 (4KB) | 0xff000 | DATA, READ/WRITE |
| .rsrc | 0x419000 (4.2MB) | 0x100000 | DATA, READONLY |
| .reloc | 0x188dc (99KB) | 0x519000 | DATA, READONLY, DISCARDABLE |

### Entry Point

- **Address**: 0x1000 (relative to .text section)
- **Purpose**: Program initialization, sets up internal function pointers

## Implications for API Analysis

### What We Need to Find

1. **Internal Function Pointer Tables**
   - Location: Likely in `.rdata` section
   - Purpose: Store callbacks and function pointers for client.dll
   - Method: Disassemble and search for pointer arrays

2. **Runtime Registration Mechanisms**
   - Pattern: Function pointer assignments in `.text`
   - Purpose: Register client callbacks with launcher
   - Method: Disassemble entry point

3. **Shared Data Structures**
   - Location: Likely in `.data` section
   - Purpose: Session, packet, connection objects
   - Method: Disassemble and identify structures

4. **Client.dll Integration Points**
   - How client.dll discovers launcher functions
   - Runtime discovery vs static linking
   - Method: Analyze client.dll code

### Next Steps

1. **Disassemble entry point** (0x1000) to find internal pointer setup
2. **Search `.rdata`** for static function pointer tables
3. **Analyze callback registration** in `.text` section
4. **Extract data structures** from `.data` section
5. **Study client.dll** to understand how it uses launcher API

## Technical Details

### SetMasterDatabase Analysis

- **Address**: 0x143f0
- **Ordinal**: 1
- **Purpose**: Likely the main API entry point for database/session management
- **Client Usage**: This is the ONLY function client.dll can call directly

### Why Only One Export?

1. **Security**: Minimize exposed API surface
2. **Flexibility**: Internal pointers allow dynamic behavior
3. **Performance**: Direct internal calls avoid export overhead
4. **Architecture**: Launcher handles all network, uses internal mechanisms

## Conclusion

The launcher.exe has a **minimal static export table** but likely contains a **large internal API** discovered at runtime. The critical discovery about the "stupidly large API surface" must be found through:

1. Internal function pointer tables
2. Runtime discovery mechanisms
3. Client.dll integration code
4. Callback registration patterns

**Priority**: Disassembly and internal API discovery is now CRITICAL.

---

**Status**: Phase 1 Complete  
**Next Phase**: Disassembly and internal API discovery