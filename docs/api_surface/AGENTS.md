# API Surface Agent

## Agent Role: API_ANALYST

**Primary Responsibility**: API Surface Extraction and Documentation

---

## Current Status

- **Priority**: 1 (CRITICAL)
- **Progress**: 80%
- **Binaries Analyzed**: launcher.exe, client.dll
- **Callbacks Documented**: 13/39+ (33%)

---

## Completed Tasks

- [x] Create API surface documentation structure
- [x] Extract ALL exported functions from launcher.exe
- [x] Document API surface (function names, signatures)
- [x] Find function pointer tables (117 tables, 5,145 pointers)
- [x] Locate client.dll integration points (SetMasterDatabase)
- [x] Analyze API function signatures (VTable-based)
- [x] Map data structures used by API
- [x] Document callback mechanisms (bidirectional)
- [x] Create callback documentation template
- [x] Validate callbacks against assembly code

---

## Remaining Tasks

- [ ] Document remaining 26 callbacks
- [ ] Create complete function signatures for all VTable functions
- [ ] Document packet protocol in detail
- [ ] Map all message types with codes

---

## Disassembly Tool Guide

### radare2 (r2) - Primary Analysis Tool

radare2 is the main tool for reverse engineering and binary analysis.

#### Basic Commands

```bash
# Start interactive analysis
r2 launcher.exe

# Analyze all (auto-analyze)
r2 -c 'aaa' launcher.exe

# Quiet mode (no colors, scriptable)
r2 -q -c 'command' launcher.exe
```

#### Common Analysis Workflows

**1. List Functions**
```bash
# List all functions
r2 -q -c 'afl' launcher.exe

# List functions matching pattern
r2 -q -c 'afl~packet' launcher.exe

# Count functions
r2 -q -c 'afl' launcher.exe | wc -l
```

**2. Disassemble Functions**
```bash
# Disassemble specific function
r2 -q -c 'aaa; s fcn.0044bca0; pdf' launcher.exe

# Disassemble at address
r2 -q -c 's 0x004143f0; pd 50' launcher.exe

# Disassemble with analysis
r2 -q -c 'aaa; pdf @ fcn.0044bca0' launcher.exe
```

**3. Search Strings**
```bash
# Search all strings
r2 -q -c 'izz' launcher.exe

# Search strings matching pattern
r2 -q -c 'izz~packet' launcher.exe
r2 -q -c 'izz~Packet' launcher.exe

# Search in specific section
r2 -q -c 'izz~0x004c' launcher.exe  # Strings in .data section
```

**4. Cross-References (XRefs)**
```bash
# Find references to address
r2 -q -c 'aaa; axt 0x004b81e8' launcher.exe

# Find references from address
r2 -q -c 'aaa; axf 0x0044bca0' launcher.exe
```

**5. Section Analysis**
```bash
# List sections
r2 -q -c 'iS' launcher.exe

# Show section info
r2 -q -c 'iS~.text' launcher.exe
```

**6. Exports/Imports**
```bash
# List exports
r2 -q -c 'iE' launcher.exe

# List imports
r2 -q -c 'ii' launcher.exe
```

**7. Hex Dumps**
```bash
# Hex dump at address
r2 -q -c 'px 512 @ 0x004c6000' launcher.exe

# Hex dump words
r2 -q -c 'pxw 512 @ 0x004c6000' launcher.exe
```

#### Analyzing Packet Callbacks

Complete workflow for analyzing packet-related code:

```bash
# 1. Find packet strings
r2 -q -c 'izz~Packet' launcher.exe | grep -E "SendPacket|OnOperationCompleted|DecryptPacket"

# 2. Find cross-references to packet strings
r2 -q -c 'aaa; axt 0x004b81e8' launcher.exe

# 3. Analyze the function using the string
r2 -q -c 'aaa; s fcn.0044bca0; pdf' launcher.exe | head -100

# 4. Find all packet-related functions
r2 -q -c 'aaa; afl~Packet' launcher.exe
```

#### Analyzing VTables

```bash
# Find vtable references
r2 -q -c 'pxw 512 @ 0x004a9988' launcher.exe

# Analyze vtable usage
r2 -q -c 'aaa; axt 0x004a9988' launcher.exe

# Find constructor that sets vtable
r2 -q -c 'aaa; s 0x00401000; pd 50' launcher.exe
```

#### Analyzing Master Database

```bash
# Analyze SetMasterDatabase export
r2 -q -c 'aaa; s 0x004143f0; pdf' launcher.exe

# Find references to master database
r2 -q -c 'aaa; axt 0x004d3d54' launcher.exe
```

### readpe - PE Analysis Tool

Quick PE header and export/import analysis.

```bash
# Show all PE information
readpe --all launcher.exe

# Show exports only
readpe --exports launcher.exe

# Show imports only
readpe --imports launcher.exe

# Show sections
readpe --sections launcher.exe
```

### objdump - Alternative Tool

GNU tool for binary analysis.

```bash
# Disassemble all
objdump -d launcher.exe > disasm.txt

# Show sections
objdump -h launcher.exe

# Show symbols
objdump -t launcher.exe
```

---

## Analysis Workflows

### Workflow 1: Documenting a Callback

1. **Find callback name** in strings:
   ```bash
   r2 -q -c 'izz~CallbackName' launcher.exe
   ```

2. **Find function using the string**:
   ```bash
   r2 -q -c 'aaa; axt <string_address>' launcher.exe
   ```

3. **Disassemble the function**:
   ```bash
   r2 -q -c 'aaa; s <func_address>; pdf' launcher.exe
   ```

4. **Analyze parameters**:
   - Look at function prologue (stack allocation)
   - Check `[ebp+8]`, `[ebp+c]`, etc. for parameters
   - Check register usage (ECX for `this` pointer)

5. **Document in callbacks/template**:
   ```bash
   cp callbacks/TEMPLATE.md callbacks/<category>/<CallbackName>.md
   ```

### Workflow 2: Validating Data Structures

1. **Find structure usage**:
   ```bash
   r2 -q -c 'aaa; s <func>; pdf' launcher.exe
   ```

2. **Check field accesses**:
   - Look for `[ecx+0xXX]` patterns
   - Note offsets used

3. **Cross-reference with other functions**:
   ```bash
   r2 -q -c 'aaa; axt <structure_address>' launcher.exe
   ```

### Workflow 3: Finding VTable Functions

1. **Locate vtable address**:
   ```bash
   r2 -q -c 'pxw 160 @ 0x004a9988' launcher.exe
   ```

2. **Analyze each function**:
   ```bash
   r2 -q -c 'aaa; s <func_addr>; pdf' launcher.exe
   ```

3. **Map vtable offsets to functions**:
   - Offset 0x00 = vtable[0]
   - Offset 0x04 = vtable[1]
   - etc.

---

## Binary Information

### launcher.exe
- **File**: `../../launcher.exe` (5,267,456 bytes)
- **Format**: PEI-i386 (32-bit executable)
- **Entry Point**: 0x0048be94
- **Export**: SetMasterDatabase (Ordinal 1, 0x004143f0)

### Sections
| Section | Start | End | Purpose |
|---------|-------|-----|---------|
| `.text` | 0x00401000 | 0x004a8000 | Code (672KB) |
| `.rdata` | 0x004a9000 | 0x004c6000 | VTables (118KB) |
| `.data` | 0x004c6000 | 0x004d3000 | Runtime data (52KB) |
| `.rsrc` | 0x00519000 | 0x00519000 | Resources (4.2MB) |

### client.dll
- **File**: `../../client.dll` (11,000,000+ bytes)
- **Exports**: 5 functions (InitClientDLL, SetMasterDatabase, etc.)
- **Master Database Address**: 0x629f14a0

---

## Key Findings

### API Architecture
- **Single Export**: SetMasterDatabase is the only exported function
- **VTable-based**: 117 vtables with 5,145 function pointers
- **Runtime Discovery**: All API functions discovered at runtime
- **Bidirectional**: Both launcher→client and client→launcher calls

### Callback System
- **Storage**: Callbacks at offsets 0x20, 0x24, 0x28 in objects
- **Registration**: Via vtable[4], vtable[23], vtable[24]
- **Categories**: Lifecycle, Network, Game, UI, Monitor
- **Total Estimated**: 50-100+ callbacks

### Documentation
- **Template**: `callbacks/TEMPLATE.md`
- **Guide**: `callbacks/TEMPLATE_GUIDE.md`
- **Index**: `callbacks/CALLBACK_INDEX.md`
- **Validated**: OnPacket callback confirmed against assembly

---

## Tips for Success

### Do's
- ✅ Always run `aaa` (analyze all) before disassembly
- ✅ Use `~` (tilde) for grep-like filtering in r2
- ✅ Check multiple cross-references
- ✅ Validate structures against multiple functions
- ✅ Document confidence level (High/Medium/Low)
- ✅ Include reproduction commands in documentation

### Don'ts
- ❌ Don't skip analysis (`aaa` is required)
- ❌ Don't trust single data point
- ❌ Don't forget to validate against binary
- ❌ Don't leave placeholder text in documentation
- ❌ Don't assume calling conventions without checking

---

## Quick Reference Commands

```bash
# Analyze everything
r2 -q -c 'aaa' launcher.exe

# Find string
r2 -q -c 'izz~pattern' launcher.exe

# Disassemble function
r2 -q -c 'aaa; s address; pdf' launcher.exe

# Find xrefs
r2 -q -c 'aaa; axt address' launcher.exe

# List functions
r2 -q -c 'afl' launcher.exe

# Hex dump
r2 -q -c 'pxw size @ address' launcher.exe

# Show sections
r2 -q -c 'iS' launcher.exe

# Show exports
r2 -q -c 'iE' launcher.exe
```
