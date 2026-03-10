# TODO - Matrix Online Launcher Reverse Engineering

## Overview
Project to document and reverse engineer the Matrix Online launcher.exe.

## Critical Discovery - March 7, 2026 ⚠️

**The launcher has a stupidly large API surface it shares with client.dll!**

This changes everything:
- Launcher is NOT just a UI app
- Launcher handles ALL TCP communications
- client.dll depends on launcher API
- API surface is intentionally large
- Protocol handling is centralized in launcher

---

## Priority: CRITICAL ⚠️

### API Surface Analysis (NEW - Most Important)
**Agent**: API_ANALYST | **Folder**: `api_surface/`

- [ ] Extract ALL exported functions from launcher.exe
- [ ] Document API surface (function names, signatures)
- [ ] Find function pointer tables
- [ ] Locate client.dll integration points
- [ ] Analyze API function signatures
- [ ] Map data structures used by API
- [ ] Document callback mechanisms
- [ ] Trace client.dll usage of launcher API

**Status**: CRITICAL - Just discovered
**Priority**: 1 (CRITICAL)
**Next Task**: Extract all exports

---

### Network Communication Analysis (NEW)
**Agent**: NETWORK_ANALYST | **Folder**: `network_analysis/`

- [ ] Extract network-related functions
- [ ] Document TCP handling code
- [ ] Parse packet structures
- [ ] Map protocol dispatch
- [ ] Document communication flow
- [ ] Analyze socket management
- [ ] Reverse engineer network protocol
- [ ] Find session management code

**Status**: CRITICAL - Just discovered
**Priority**: 1 (CRITICAL)
**Next Task**: Find TCP handling code

---

### Code Disassembly Analysis
**Agent**: DISASSEMBLER | **Folder**: `code_disassembly/`

- [ ] Disassemble entry point
- [ ] Identify API-related functions
- [ ] Build call graph for API functions
- [ ] Document control flow in API code
- [ ] Identify API call sequences
- [ ] Disassemble network handling code

**Status**: Pending
**Priority**: 2 (HIGH)
**Next Task**: Entry point disassembly

---

## Priority: HIGH

### Documentation Structure
- [x] Create documentation folder structure
- [x] Write README.md for project
- [x] Write PE Headers documentation
- [x] Write Imports documentation
- [x] Write Exports documentation
- [x] Write Resources documentation
- [x] Write Sections documentation
- [x] Write String Table documentation
- [x] Write Analysis notes documentation
- [x] Create API Surface documentation
- [x] Create Network Analysis documentation
- [x] Create Code Disassembly documentation

### Initial Analysis
- [x] Extract basic file information
- [x] Parse DOS header
- [x] Parse PE signature
- [x] Parse COFF header
- [x] Parse Optional Header
- [ ] Extract and document section headers
- [ ] Build complete string table
- [ ] Parse import address table
- [ ] Parse resource directory

### String Analysis
- [x] Extract initial 100 strings
- [ ] Find all "Matrix Online" references
- [ ] Categorize UI strings
- [ ] Categorize error messages
- [ ] Categorize version info strings
- [ ] Document all URLs found
- [ ] Create string index

### Section Analysis
- [ ] Analyze `.text` section (entry point)
- [ ] Analyze `.rdata` section (constants)
- [ ] Analyze `.data` section (variables)
- [ ] Analyze `STLPORT_` section (runtime library)
- [ ] Analyze `.rsrc` section (resources)
- [ ] Analyze `.reloc` section (relocations)

### Import Analysis
- [ ] Extract all imported DLLs
- [ ] Categorize imports by function type
- [ ] Document Windows API calls used
- [ ] Check for networking functions
- [ ] Check for UI functions
- [ ] Check for memory management functions

### Resource Analysis
- [ ] Extract all icons
- [ ] Extract all dialogs
- [ ] Extract version information
- [ ] Document resource types
- [ ] Categorize by function

### Disassembly
- [ ] Disassemble entry point
- [ ] Identify all functions
- [ ] Build call graph
- [ ] Document control flow
- [ ] Identify API call sequences

### Findings
- [x] **CRITICAL: Launcher has large API surface with client.dll**
- [x] **CRITICAL: Launcher handles all TCP communications**
- [ ] Document key discoveries
- [ ] Create function reference
- [ ] Document game protocol (if found)
- [ ] Document client-server communication
- [ ] Create security observations

---

## Priority: MEDIUM

### Tool Development
- [ ] Create Python script for PE analysis
- [ ] Implement import table parser
- [ ] Implement resource extractor
- [ ] Create string categorizer
- [ ] Build function finder

### Network Analysis
- [ ] Analyze network-related code
- [ ] Document protocol headers
- [ ] Identify packet structures
- [ ] Map client-server messages

---

## Priority: LOW

### Cleanup
- [ ] Organize extracted files
- [ ] Create backup of original
- [ ] Document file locations
- [ ] Archive intermediate results

### Final Documentation
- [ ] Compile all findings
- [ ] Create final report
- [ ] Generate function reference
- [ ] Write summary document

---

## Notes
- Project started: March 7, 2026
- Initial PE structure documented
- Basic string extraction complete
- Matrix Online confirmed as Monolith Productions game
- **CRITICAL**: Launcher has large API surface with client.dll
- **CRITICAL**: Launcher handles all TCP communications
