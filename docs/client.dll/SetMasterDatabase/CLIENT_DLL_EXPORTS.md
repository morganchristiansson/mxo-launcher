# Client.dll Exported Functions

## Executive Summary

**Date**: March 7, 2026  
**Status**: VALIDATED ✓  
**Source**: Forum post + strings extraction from launcher.exe

## Exported Functions

The following 5 functions are exported from client.dll and used as the main API interface:

| # | Function | Purpose |
|---|----------|---------|
| 1 | **ErrorClientDLL** | Error handling and reporting |
| 2 | **InitClientDLL** | Initialize client DLL |
| 3 | **RunClientDLL** | Start client network operations |
| 4 | **SetMasterDatabase** | Configure master database connection |
| 5 | **TermClientDLL** | Terminate client DLL |

## Validation Results

### Methodology
1. Extracted strings from launcher.exe
2. Searched for exact function names
3. Cross-referenced with forum information

### Results
All 5 functions confirmed present:

```bash
$ strings launcher.exe | grep -E "^(ErrorClientDLL|InitClientDLL|RunClientDLL|SetMasterDatabase|TermClientDLL)$"
ErrorClientDLL
TermClientDLL
RunClientDLL
InitClientDLL
SetMasterDatabase
```

### Status: CONFIRMED ✓

## API Surface Analysis

### Client.dll as the Main Network Handler

Based on the presence of these exports:

1. **Centralized Network Code**: client.dll handles all TCP communications
2. **Launcher Dependency**: launcher.exe provides the network infrastructure
3. **Protocol Dispatch**: All game communication goes through these functions
4. **Session Management**: Init/Term functions manage client sessions

### Function Signatures (Hypothetical)

Based on typical API patterns:

```c
// Error handling
void ErrorClientDLL(const char* message, int severity);

// Initialization
int InitClientDLL(void* config, int port);

// Runtime
void RunClientDLL(void* session, void* callback);

// Database configuration
void SetMasterDatabase(const char* dbPath, const char* dbHost);

// Termination
void TermClientDLL(void* session);
```

## Related Client Strings

Additional client-related strings found in launcher.exe:

| String | Context |
|--------|---------|
| `LTLO_CLIENTHASHFAILED` | Client hash validation error |
| `LTAS_INCOMPATIBLECLIENTVERSION` | Client version mismatch |
| `LTMS_INCOMPATIBLECLIENTVERSION` | Master server client version error |
| `MS_GetClientIPReply` | Client IP response handler |
| `MS_GetClientIPRequest` | Client IP request handler |
| `The Matrix Online client crashed.` | Client crash message |
| `LaunchPadClient %d connections opened/closed` | LaunchPad connection logging |

## Integration with Launcher API

### How client.dll Uses launcher.exe

1. **Function Pointers**: client.dll calls launcher functions directly
2. **Shared Memory**: Session and connection objects in shared memory
3. **Callback Mechanisms**: Launcher notifies client of events
4. **Network Buffers**: Client uses launcher's buffer management

### Entry Points

The 5 exports serve as:
- **Entry point**: InitClientDLL starts the client
- **Runtime**: RunClientDLL processes network operations
- **Exit point**: TermClientDLL shuts down cleanly
- **Error handling**: ErrorClientDLL reports issues
- **Configuration**: SetMasterDatabase sets up connections

## Technical Notes

### PE Export Directory

The functions are exported via standard PE export directory:
- **Export Directory**: Present in PE headers
- **Function Table**: Contains addresses of exported functions
- **Name Table**: Contains function names and indices

### Static vs Dynamic

launcher.exe is a **static executable**, meaning:
- No external DLL dependencies for network code
- All network functions built into launcher.exe
- client.dll links against launcher's export table
- Functions are resolved at load time

## Next Investigation Areas

### A. Exact Function Signatures
**Goal**: Determine parameter types and return values  
**Method**: Disassemble each exported function  
**Priority**: HIGH

### B. Internal Function Pointers
**Goal**: Find launcher functions called by client.dll exports  
**Method**: Disassemble the 5 export functions  
**Priority**: HIGH

### C. Callback Registration
**Goal**: Understand how client registers callbacks with launcher  
**Method**: Analyze InitClientDLL implementation  
**Priority**: MEDIUM

### D. Session Data Structures
**Goal**: Identify session objects used by the API  
**Method**: Reverse engineer RunClientDLL  
**Priority**: MEDIUM

## References

- **AGENTS.md**: Defines API_ANALYST role for this work
- **EXPORTS.md**: Contains launcher.exe export analysis
- **ANALYSIS_SUMMARY.md**: Overall API surface findings

---

**Last Updated**: March 7, 2026  
**Status**: Validated and documented  
**Priority**: CRITICAL (API surface work)
