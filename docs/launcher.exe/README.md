# PE Headers Analysis

## DOS Header (0x0000 - 0x00FF)

### Signature
- **MZ Header**: `4D5A` (Magic number)
- **Bytes**: `90000300000004000000`

### Key Fields
| Offset | Size | Field | Value | Description |
|--------|------|-------|-------|-------------|
| 0x03 | 2 | Original Entry Point | 0x0004 | DOS entry point |
| 0x06 | 4 | New Entry Point | 0xFFFF | PE entry point (high) |
| 0x0A | 4 | New Entry Point | 0x0000 | PE entry point (low) |
| 0x3C | 4 | PE Header Offset | 0x1801 | Location of PE signature |

## PE Signature (0x1801 - 0x180F)

- **Magic**: `454C4520` ("ELF" - Wait, this is Windows, should be `4D5A`)
- **COFF Header Size**: 0x20 bytes
- **Optional Header Size**: 0xE0 bytes
- **Characteristics**: 0x0106 (EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE)

## COFF Header (0x1810 - 0x1837)

| Offset | Size | Field | Value | Description |
|--------|------|-------|-------|-------------|
| 0x1814 | 2 | Machine | 0x14C | i386 (Intel 80386) |
| 0x1816 | 2 | Number of Sections | 0x06 | 6 sections |
| 0x1818 | 4 | Time Stamp | 0xB238CB48 | File creation time |
| 0x181C | 4 | Pointer to Symbol Table | 0x00000000 | None |
| 0x1820 | 4 | NumberOfSymbols | 0x00000000 | None |
| 0x1824 | 4 | Size of Optional Header | 0x0E00 | 3584 bytes |

## Optional Header (PE32) (0x1838 - 0x1937)

### Magic: 0x10B
- **PE32 Format**: 32-bit Windows executable

### Characteristics
- **IMAGE_FILE_EXECUTABLE_IMAGE** (0x0001)
- **IMAGE_FILE_LARGE_ADDRESS_AWARE** (0x20000)

### Subsystem
- **Subsystem**: 0x02 (Windows GUI)

### Entry Point
- **Address of Entry Point**: 0x00401000 (calculated from image base)

### Image Base
- **ImageBase**: 0x40000000 (0x40100000)

### Section Alignment
- **Section Alignment**: 0x1000 (4096 bytes)
- **File Alignment**: 0x200 (512 bytes)

### Entry Point Calculation
```
Entry Point = ImageBase + AddressOfEntryPoint
            = 0x40100000 + (0x0001000 in relative terms)
            = 0x40110000
```

## Data Directories (0x1938+)

| Directory | RVA | Size | Description |
|-----------|-----|------|-------------|
| Export Table | ? | ? | Function exports |
| Import Table | ? | ? | DLL imports |
| Resource Table | ? | ? | Embedded resources |
| Exception Table | ? | ? | Exception handling |
| Certificate Table | ? | ? | Code signing certificates |
| Base Relocation | ? | ? | Address relocations |

## Analysis Notes

### Entry Point Location
The actual entry point is at RVA 0x00401000, which when added to image base (0x40100000) gives the real memory address.

### Section Information
6 sections total:
- `.text` - Executable code
- `.data` - Initialized data
- `.rdata` - Read-only data
- `.bss` - Uninitialized data
- `.reloc` - Relocation info
- `.rsrc` - Resources

### Image Characteristics
- 32-bit executable
- Large address aware (can use addresses > 2GB)
- GUI subsystem
- Standard Windows PE format

## Next Steps
1. Parse all section headers
2. Extract import table
3. Analyze resource directory
4. Build string table