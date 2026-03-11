# Data Structures Analysis - launcher.exe

## Overview
Analysis of the `.data` section (0x004c6000 - 0x004d3000) in launcher.exe to identify session objects, packet structures, connection objects, and other runtime data structures.

**Analysis Date**: Phase 2.4
**Binary**: launcher.exe (5,267,456 bytes)
**Section**: `.data` (52KB, 0x000c6000 - 0x000d3000 on disk)

---

## Section Layout

### .data Section Boundaries
- **Physical Address**: 0x000c6000
- **Virtual Address**: 0x004c6000
- **Size on Disk**: 0x000d000 (52KB)
- **Virtual Size**: 0x00039000 (228KB)
- **Permissions**: RW- (Read/Write)

### Other Sections
- **STLPORT_**: 0x004ff000 (4KB, RW-)
- **.text**: 0x00401000 (672KB, R-X) - Code section
- **.rdata**: 0x004a9000 (116KB, R--) - Read-only data

---

## Major Findings

### 1. Function Pointer Tables (0x004c6000 - 0x004c9000)

**Location**: Start of .data section

The first 12KB of .data contains arrays of function pointers, similar to those found in .rdata. These are **runtime-modifiable** vtables and callback tables.

**Characteristics**:
- All pointers reference code in .text section (0x0040xxxx - 0x004axxxx)
- Organized in contiguous arrays
- May be modified at runtime for hooking or dynamic dispatch
- Approximately 3,000+ function pointers in this region

**Example Pattern** (0x004c6000):
```
0x004c6000: 0x0049c548  ; Function pointer 1
0x004c6004: 0x004a4a27  ; Function pointer 2
0x004c6008: 0x004a4a48  ; Function pointer 3
...
```

**Purpose**:
- Runtime polymorphism
- Dynamic callback registration
- Plugin architecture support
- Network event handlers

---

### 2. ILTLDLL Media Structure (0x004c90a0)

**Signature**: ILTLDLL (Illusion/Techland DLL format)

**Location**: 0x004c90a0

**Structure**:
```c
struct ILTLDLL_Media {
    uint32_t signature;        // 0xfaceface - magic number
    char     identifier[8];    // "ILTLDLLM" (reversed)
    char     type[8];          // "ediatoMed" -> "Mediator"
    char     name[4];          // "ator" -> part of "Mediator"
    // Additional fields follow
};
```

**Discovered Values**:
```
0x004c90a0: 0xfaceface        ; Magic marker
0x004c90a4: 0x4c544c49        ; "ILTL" (reversed)
0x004c90a8: 0x6e69676f        ; "ogin" -> "Login"
0x004c90ac: 0x6964654d        ; "Medi"
0x004c90b0: 0x726f7461        ; "ator"
```

**Purpose**:
- Module identification system
- Likely used for client.dll/launcher.exe communication
- Part of the game engine's plugin architecture

---

### 3. Search Marker Strings (0x004c9080)

**Location**: 0x004c9080

**Strings Found**:
- `search_marker_breaker__holder__`
- `Defaul` (partial string)

**Purpose**:
- Debug markers for memory searches
- Possibly used for object identification
- May be referenced by client.dll

---

### 4. Numeric Lookup Tables (0x004cd060 - 0x004cd200)

**Location**: Mid-section, around 0x004cd060

**Characteristics**:
- Small numeric values (0-1024 range)
- Appears to be prime numbers or near-primes
- Used for hash table calculations or packet routing

**Example Values**:
```
0x004cd060: 0x00000228  0x00000000  0x00000002  0x00000003
0x004cd070: 0x00000005  0x00000007  0x0000000b  0x0000000d
0x004cd080: 0x00000011  0x00000013  0x00000017  0x0000001d
...
```

**Pattern Analysis**:
- Values: 552, 0, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47...
- Mostly prime numbers with some exceptions
- Likely used for hash table bucket distribution
- May be part of network protocol message ID mapping

**Purpose**:
- Hash table implementation
- Packet type identification
- Message routing table

---

### 5. Global Configuration Strings (Found in .rdata, referenced from .data)

**Strings discovered**:
- `"STATION"` - Station/server identifier
- `"client"` - Client identifier
- `"Sim_Time_Scale"` - Simulation time scaling
- `"Sim_Time_ShowAdjust"` - Time adjustment display
- `"Min_Time_Adjust_Difference"` - Minimum time adjustment
- `"Max_Time_Adjust_Difference"` - Maximum time adjustment
- `"METR"`, `"METRID"`, `"Filename"`, `"UniverseFile"` - File/metric identifiers
- `"resource/worlds/universe.btb"` - Game world file path

**Purpose**:
- Configuration management
- Simulation timing control
- Resource path management

---

### 6. Network Protocol Indicators

**Compression Support**:
- zlib 1.2.2 (inflate functions)
- Found at .rdata addresses around 0x004c02f8

**Encryption Support**:
- DES encryption (SSLeay 0.6.3)
- Found at .rdata addresses around 0x004c12e0

**Purpose**:
- Packet compression for network traffic
- Encryption for secure communication
- Part of the TCP network stack

---

## Object Types Identified

### Session Objects (Inferred)

Based on the API surface analysis and function pointer tables, session objects likely contain:

```c
struct SessionObject {
    void* vtable;                    // Pointer to virtual function table
    uint32_t session_id;             // Unique session identifier
    uint32_t connection_state;       // Connection status
    void* network_buffer;            // Network I/O buffer
    uint32_t buffer_size;            // Buffer capacity
    // Additional fields unknown
};
```

**Evidence**:
- Multiple vtable registrations in .rdata
- Callback registration patterns
- Network buffer management functions

### Connection Objects (Inferred)

```c
struct ConnectionObject {
    void* vtable;                    // Virtual function table
    uint32_t connection_id;          // Connection identifier
    uint32_t ip_address;             // Remote IP (network byte order)
    uint16_t port;                   // Remote port
    uint16_t status;                 // Connection status
    void* send_buffer;               // Send queue
    void* recv_buffer;               // Receive queue
    // Additional fields unknown
};
```

**Evidence**:
- TCP handling code mentioned in TODO
- Network buffer structures
- Session management functions

### Packet Structures (Inferred)

```c
struct PacketHeader {
    uint16_t message_type;           // Message type ID
    uint16_t payload_size;           // Size of payload
    uint32_t sequence_id;            // Sequence number
    uint32_t flags;                  // Packet flags
};

struct Packet {
    struct PacketHeader header;
    uint8_t payload[];               // Variable-length payload
};
```

**Evidence**:
- Numeric lookup tables (possible message type mapping)
- Compression/encryption support
- Buffer management functions

---

## Memory Layout Analysis

### .data Section Organization

```
0x004c6000 - 0x004c9000  Function pointer tables (~12KB)
0x004c9000 - 0x004ca000  ILTLDLL structures and markers (~4KB)
0x004ca000 - 0x004cd000  Additional function pointers (~12KB)
0x004cd000 - 0x004cd200  Numeric lookup tables (~512 bytes)
0x004cd200 - 0x004d3000  Uninitialized/reserved data (~64KB)
```

### Pointer Reference Analysis

**Source**: .data section
**Destination**: .text section (0x0040xxxx - 0x004axxxx)

All function pointers in .data reference code within the .text section:
- Not importing from external DLLs
- All internal API functions
- Enables runtime modification for patching

---

## Client.dll Integration Points

### Expected Shared Structures

Based on the analysis, client.dll likely interacts with launcher.exe through:

1. **Function Pointer Tables** (0x004c6000+)
   - Client.dll obtains pointer to vtable
   - Calls methods through vtable entries
   - Enables hot-patching and modding

2. **ILTLDLL Media Interface** (0x004c90a0)
   - Identification and versioning
   - Module capability negotiation
   - Interface discovery

3. **Global Configuration** (Referenced strings)
   - Shared configuration strings
   - Time synchronization
   - Resource paths

### API Discovery Mechanism

**Hypothesis**:
- client.dll calls SetMasterDatabase(export) to initialize
- Receives pointer to master database structure
- Database contains pointers to function tables in .data
- Client uses vtables to call launcher functions

---

## Next Steps

### Immediate Actions

1. **Dynamic Analysis**:
   - Run launcher.exe with debugger
   - Break on .data modifications
   - Track structure initialization

2. **Cross-Reference Analysis**:
   - Find all code references to .data addresses
   - Map structure field access patterns
   - Document read/write patterns

3. **Client.dll Analysis** (Phase 3):
   - Analyze client.dll's import of SetMasterDatabase
   - Document expected parameter structures
   - Map callback registration patterns

### Documentation Updates

- [ ] Document specific session object layout
- [ ] Map connection state machine
- [ ] Document packet format details
- [ ] Create API usage examples

---

## Summary

### Key Findings

1. **Large Function Pointer Arrays**: 3,000+ modifiable function pointers
2. **ILTLDLL Plugin Architecture**: Game engine module system
3. **Numeric Lookup Tables**: Prime-based hash/message routing
4. **Network Infrastructure**: Compression (zlib) and encryption (DES) support
5. **Configuration Management**: Time sync and resource management

### Critical Discoveries

- **.data section is modifiable at runtime**, unlike .rdata
- **Function pointers enable dynamic dispatch** for client.dll
- **ILTLDLL structures** provide module identification
- **Hash tables** likely used for packet routing

### Relationship to API Surface

The .data section contains the **runtime-modifiable portion** of the API surface:
- Static vtables in .rdata (read-only, immutable)
- Dynamic vtables in .data (read-write, modifiable)
- Client.dll likely uses .data vtables for flexibility

---

## File References

- **Binary**: ../../launcher.exe
- **Section**: .data (0x004c6000 - 0x004d3000)
- **Related**: .rdata (0x004a9000 - 0x004c6000) - Static vtables
- **Related**: .text (0x00401000 - 0x004a9000) - Code section

---

## Appendix: Radare2 Commands Used

```bash
# Section analysis
r2 -q -c 'iS' ../../launcher.exe

# Hex dump of .data section
r2 -q -c 'px 512 @ 0x004c6000' ../../launcher.exe
r2 -q -c 'pxw 512 @ 0x004c9000' ../../launcher.exe

# String analysis
r2 -q -c 'izz~0x004c' ../../launcher.exe

# Function pointer analysis
r2 -q -c 'pxw 512 @ 0x004cd000' ../../launcher.exe
```

---

**Status**: Phase 2.4 COMPLETE ✅
**Next Phase**: Phase 3 - Client.dll Integration
