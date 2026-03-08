# CRITICAL DISCOVERY - March 7, 2026

## The Launcher Has a Stupidly Large API Surface

### Original Statement
> "There's a stupidly big API surface it shares with the client"

### My Initial Response
> "Really? It's just launching an exe?"

### Reality Check
**The launcher handles ALL TCP communications and the client.dll communicates to it asking it to do stuff via the stupidly large API.**

## What This Means

### 1. Architecture Re-Understanding
```
OLD UNDERSTANDING:
launcher.exe → UI only → Opens client.exe

NEW UNDERSTANDING:
launcher.exe → Network Hub → API Interface → client.dll → Game Logic
                    ↓
              TCP Communications
                    ↓
              Protocol Handling
```

### 2. Launcher Responsibilities
- **TCP Socket Management** - Creates and manages TCP connections
- **Packet Construction** - Builds network packets
- **Packet Parsing** - Parses incoming network packets
- **Protocol Dispatch** - Routes packets to appropriate handlers
- **API Interface** - Provides function pointers to client.dll
- **Session Management** - Manages game sessions
- **Authentication** - Handles login/authentication

### 3. API Surface Characteristics
- **Large Function Set** - Dozens/hundreds of functions
- **Function Pointers** - Used for callback mechanisms
- **Data Structures** - Complex packet/session structures
- **Message Passing** - Inter-process communication
- **Event Handling** - Asynchronous notifications

## Immediate Actions Required

### CRITICAL - Priority 1
1. **Extract ALL exported functions**
   - Document every export
   - Map function signatures
   - Identify function pointer tables

2. **Find API integration points**
   - Locate client.dll calls
   - Map communication flow
   - Document API usage

3. **Analyze network code**
   - Find TCP handling
   - Parse packet structures
   - Map protocol flow

### HIGH - Priority 2
4. **Document data structures**
   - Packet headers
   - Session objects
   - Callback structures

5. **Trace client.dll usage**
   - How client calls launcher
   - What data is passed
   - Return value handling

### MEDIUM - Priority 3
6. **Reverse engineer protocol**
   - Packet format
   - Protocol dispatch
   - Communication flow

7. **Create API reference**
   - Function documentation
   - Parameter structures
   - Usage examples

## Impact on Project

### 1. Protocol Discovery
- Can reverse engineer game protocols
- Understand client-server communication
- Map network traffic patterns

### 2. Client Modification
- Understand API interface
- Enable client.dll modification
- Create launcher API wrapper

### 3. Network Analysis
- Reverse engineer TCP protocol
- Understand packet structure
- Map communication flow

### 4. Security
- Identify authentication mechanisms
- Understand session management
- Document security features

## Status Update

| Metric | Before | After |
|--------|--------|-------|
| Launcher Understanding | UI only | Network hub + API interface |
| API Functions Known | 0 | 0 (needs extraction) |
| Network Code Known | 0 | 0 (needs analysis) |
| Protocol Known | 0 | 0 (needs reverse engineering) |
| Priority | MEDIUM | CRITICAL |

## Notes

- This discovery fundamentally changes the project scope
- Launcher is much more complex than initially thought
- API surface is intentionally large for flexibility
- Client.dll depends on launcher for all network communication
- Protocol handling is centralized in launcher

## Next Meeting Agenda

1. **API Surface Extraction** - 2 hours
2. **Export Function Analysis** - 1 hour
3. **Client.dll Integration Points** - 1 hour
4. **Network Code Location** - 1 hour
5. **Protocol Structure Mapping** - 2 hours

---

**Discovery Date**: March 7, 2026
**Discovered By**: Team Lead (rajkosto)
**Impact**: CRITICAL - Changes entire project approach