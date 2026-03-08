# API Surface Analysis - launcher.exe

## Critical Discovery

**The launcher has a stupidly large API surface that shares functionality with client.dll**

This changes everything. The launcher is NOT just a UI application - it's a:
- TCP communication handler
- Client coordination hub
- Game protocol manager
- Network session controller

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    launcher.exe                         │
│  (Launcher Application / Network Hub)                   │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │          API Interface Layer                      │  │
│  │  (Large API surface shared with client.dll)      │  │
│  │  - Client initialization                          │  │
│  │  - TCP communication                              │  │
│  │  - Game protocol handling                         │  │
│  │  - Session management                             │  │
│  └──────────────────────────────────────────────────┘  │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │          Network Communication Layer              │  │
│  │  - TCP socket management                          │  │
│  │  - Packet construction                            │  │
│  │  - Packet parsing                                 │  │
│  │  - Protocol dispatch                              │  │
│  └──────────────────────────────────────────────────┘  │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │          UI Layer                                 │  │
│  │  - Window creation                                │  │
│  │  - Dialog management                              │  │
│  │  - User interaction                               │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘

                    ↓ Client.dll Communication
           ┌──────────────────────────────────┐
           │           client.dll             │
           │  (Game Client Library)           │
           │  - Uses launcher API             │
           │  - Handles actual game logic     │
           └──────────────────────────────────┘
```

## API Categories

### 1. Client Communication
- Client initialization
- Client connection/disconnection
- Client state management
- Client data transfer

### 2. Network Communication
- TCP socket creation
- Connection management
- Packet sending/receiving
- Protocol dispatch

### 3. Game Protocol
- Login protocol
- Session protocol
- Game state protocol
- Chat protocol

### 4. Session Management
- Session creation
- Session maintenance
- Session destruction
- Session recovery

## API Surface Characteristics

### Shared with client.dll
- **Large API surface**
- **Function pointers**
- **Callback mechanisms**
- **Message passing**
- **Event handling**

### Expected API Functions

```
Client Communication:
- Client_Init
- Client_Connect
- Client_Disconnect
- Client_SendData
- Client_ReceiveData
- Client_GetStatus

Network Communication:
- Network_CreateSocket
- Network_ConnectToHost
- Network_SendPacket
- Network_ReceivePacket
- Network_CloseConnection

Game Protocol:
- Protocol_Login
- Protocol_Session
- Protocol_GameState
- Protocol_Chat
- Protocol_Matchmaking

Session Management:
- Session_Create
- Session_Start
- Session_Resume
- Session_Destroy
- Session_Save
```

## Analysis Approach

### 1. API Discovery
- Parse function exports
- Find function pointer tables
- Identify callback structures
- Locate API entry points

### 2. API Interface Analysis
- Document function signatures
- Map parameter structures
- Identify return values
- Track data flow

### 3. API Usage Analysis
- Find client.dll calls to launcher API
- Document API usage patterns
- Map communication flow
- Identify protocol dispatch

## Known API Patterns

### Function Pointer Table
```c
typedef struct {
    void (*Init)(const char*);
    void (*Connect)(const char*);
    void (*Disconnect)(void);
    // ... many more
} API_Surface;
```

### Callback Mechanism
```c
typedef void (*ClientCallback)(int errorCode);
typedef void (*DataCallback)(const void* data, int length);
```

### Message Passing
```c
typedef struct {
    uint32_t command;
    void* data;
    int dataLength;
} API_Message;
```

## Impact on Reverse Engineering

### 1. Protocol Discovery
- Can reverse engineer game protocols
- Understand client-server communication
- Map network traffic

### 2. Client Integration
- Understand how client.dll works
- Document API interface
- Enable client modification

### 3. Network Analysis
- Reverse engineer TCP protocol
- Understand packet structure
- Map communication flow

## Priority Tasks

### HIGH
- [ ] Extract all exported functions
- [ ] Document API surface
- [ ] Find function pointer tables
- [ ] Locate client.dll integration points

### MEDIUM
- [ ] Analyze API function signatures
- [ ] Map data structures
- [ ] Document callback mechanisms
- [ ] Trace client.dll usage

### LOW
- [ ] Optimize API interface
- [ ] Create API documentation
- [ ] Build API reference

## Notes

- This launcher is essentially a **network hub**
- client.dll depends on it for all communication
- API surface is intentionally large
- Protocol handling is centralized here
- This is a **critical design decision**

## Status
- **Discovery Date**: March 7, 2026
- **API Functions Identified**: 0
- **API Surface Mapped**: 0%
- **Analysis Complete**: No