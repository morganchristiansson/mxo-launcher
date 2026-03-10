# AGENTS - Matrix Online Launcher Reverse Engineering

## Project Team Structure

This document defines the roles and responsibilities for agents working on the Matrix Online launcher reverse engineering project.

## Agent Roles and Folders

### 1. API_ANALYST ⚠️ CRITICAL
**Responsible Folder**: `api_surface/`
**Primary Responsibility**: API Surface Extraction and Documentation

**Tasks**:
- Extract ALL exported functions from launcher.exe
- Document API surface (function names, signatures)
- Find function pointer tables
- Locate client.dll integration points
- Analyze API function signatures
- Map data structures used by API
- Document callback mechanisms
- Trace client.dll usage of launcher API

**Status**: CRITICAL - Just discovered
**Priority**: 1 (CRITICAL)

---

### 2. NETWORK_ANALYST ⚠️ CRITICAL
**Responsible Folder**: `network_analysis/`
**Primary Responsibility**: Network Communication Analysis

**Tasks**:
- Extract network-related functions
- Document TCP handling code
- Parse packet structures
- Map protocol dispatch
- Document communication flow
- Analyze socket management
- Reverse engineer network protocol
- Find session management code

**Status**: CRITICAL - Just discovered
**Priority**: 1 (CRITICAL)

---

### 3. DISASSEMBLER
**Responsible Folder**: `code_disassembly/`
**Primary Responsibility**: Code Disassembly and Analysis

**Tasks**:
- Disassemble entry point
- Identify API-related functions
- Build call graph for API functions
- Document control flow in API code
- Identify API call sequences
- Disassemble network handling code

**Status**: Pending
**Priority**: 2 (HIGH)

---

## CRITICAL UPDATE - March 7, 2026

**The launcher has a stupidly large API surface it shares with client.dll!**

This changes everything:
- Launcher is NOT just a UI app
- Launcher handles ALL TCP communications
- client.dll depends on launcher API
- API surface is intentionally large
- Protocol handling is centralized in launcher

**New Priority**: API analysis is now CRITICAL

## Current Agent Status

| Agent | Folder | Status | Progress | Next Task |
|-------|--------|--------|----------|-----------|
| API_ANALYST ⚠️ | api_surface/ | CRITICAL | 0% | Extract exports |
| NETWORK_ANALYST ⚠️ | network_analysis/ | CRITICAL | 0% | Find TCP code |
| DISASSEMBLER | code_disassembly/ | Pending | 0% | Entry point |
| PE_ANALYST | pe_headers/ | Active | 100% | Section headers |
| STRING_EXTRACTOR | string_table/ | Active | 20% | Complete extraction |

## Notes
- Project started: March 7, 2026
- Initial PE structure documented
- Basic string extraction complete
- Matrix Online confirmed as Monolith Productions game
- **CRITICAL**: Launcher has large API surface with client.dll
- **CRITICAL**: Launcher handles all TCP communications
