# Initial PE Header Analysis - launcher.exe

## File Information

- **File Path**: `/home/pi/mxo/launcher.exe`
- **Size**: 5,267,456 bytes (5,049,600 KB)
- **Type**: PE32 executable (GUI) Intel 80386, for MS Windows
- **Sections**: 6 sections

## DOS Header Analysis

### Magic Number
```
Offset: 0x0000
Value: 4D5A (MZ)
```

### Entry Point Information
```
Original DOS Entry Point: 0x0004
New PE Entry Point: 0xFFFF (high), 0x0000 (low)
```

### PE Header Offset
```
Offset: 0x003C
Value: 0x1801
```

## PE Signature

### Magic Number
```
Value: 454C4520 (PE)
```

### COFF Header Size
```
Value: 0x20 bytes (32 bytes)
```

### Optional Header Size
```
Value: 0xE0 bytes (224 bytes)
```

### Characteristics
```
Value: 0x0106
Flags:
- IMAGE_FILE_EXECUTABLE_IMAGE (0x0001)
- IMAGE_FILE_LARGE_ADDRESS_AWARE (0x20000)
```

## COFF Header

| Field | Value | Description |
|-------|-------|-------------|
| Machine | 0x14C | i386 (Intel 80386) |
| NumberOfSections | 0x06 | 6 sections |
| TimeStamp | 0xB238CB48 | File creation timestamp |
| PointerToSymbolTable | 0x00000000 | No symbols |
| NumberOfSymbols | 0x00000000 | No symbols |
| SizeOfOptionalHeader | 0x0E00 | 3584 bytes |

## Optional Header (PE32)

### Magic Number
```
Value: 0x10B (PE32)
```

### Characteristics
```
- IMAGE_FILE_EXECUTABLE_IMAGE
- IMAGE_FILE_LARGE_ADDRESS_AWARE
```

### Subsystem
```
Value: 0x02 (Windows GUI)
```

### Image Base
```
Value: 0x40100000
```

### Section Alignment
```
Value: 0x1000 (4096 bytes)
```

### File Alignment
```
Value: 0x200 (512 bytes)
```

## Entry Point Calculation

```
Image Base:      0x40100000
Entry Point RVA: 0x0001000
Entry Point:     0x40110000
```

## Section Summary (6 sections)

| Name | Virtual Size | File Size | Virtual Address | File Offset | Characteristics |
|------|--------------|-----------|-----------------|-------------|-----------------|
| .text | ? | ? | ? | ? | 0x60000000 |
| .data | ? | ? | ? | ? | 0xC0000040 |
| .rdata | ? | ? | ? | ? | 0x40000040 |
| .bss | ? | ? | ? | ? | 0xC0000040 |
| .reloc | ? | ? | ? | ? | 0x40000040 |
| .rsrc | ? | ? | ? | ? | 0x40000040 |

## Next Steps

1. Parse section headers to get exact sizes and offsets
2. Extract import table
3. Analyze resource directory
4. Build string table
5. Disassemble entry point

## Notes

- This is a standard Windows PE executable
- No symbols present (stripped)
- Large address aware (can use >2GB addresses)
- 32-bit executable format