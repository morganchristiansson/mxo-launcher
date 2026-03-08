# OnConnect

## Overview

**Category**: Network  
**Direction**: Launcher → Client  
**Purpose**: Connection established notification callback

---

## Function Signature

```c
void OnConnect(ConnectionEvent* connEvent, uint32_t connId, uint32_t flags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ConnectionEvent*` | connEvent | Connection event details |
| `uint32_t` | connId | Unique connection identifier |
| `uint32_t` | flags | Connection flags and options |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Connection State Machine

The connection system uses a state machine pattern:

```
LTTCP_NotConnected
        ↓ [Connect request]
LTTCP_Connecting
        ↓ [Connection established]
LTTCP_AlreadyConnected
        ↓ [Disconnect request]
LTTCP_NotConnected
```

### Connection States

| State | Constant | Description |
|-------|----------|-------------|
| Disconnected | `LTTCP_NOTCONNECTED` | No active connection |
| Connecting | `LTTCP_CONNECTING` | Connection in progress |
| Connected | `LTTCP_ALREADYCONNECTED` | Connection active |
| Session Progress | `LTMS_SESSIONESTABLISHINPROGRESS` | Session being established |
| Session Ready | `LTMS_ALREADYCONNECTED` | Session fully established |

---

## Connection Event Types

### CERT Protocol Events

| Event Type | Description |
|------------|-------------|
| `CERT_ConnectRequest` | Client requesting connection |
| `CERT_ConnectReply` | Server accepting connection |
| `CERT_NewSessionKey` | New session key created |

### Master Server Events

| Event Type | Description |
|------------|-------------|
| `LTMS_ALREADYCONNECTED` | Connection to master server ready |
| `LTMS_SESSIONESTABLISHINPROGRESS` | Session establishment in progress |
| `LTMS_NODATABASECONNECTION` | No database connection available |

---

## Connection Data Structures

### ConnectionEvent Structure

```c
struct ConnectionEvent {
    uint32_t eventType;         // Event type (CERT_*, LTMS_*)
    uint32_t connId;            // Connection identifier
    uint32_t status;            // Connection status code
    uint32_t timestamp;         // Event timestamp
    
    // Connection details
    uint32_t remoteIP;          // Remote IP address
    uint16_t remotePort;        // Remote port
    uint16_t localPort;         // Local port
    
    // Session info
    uint32_t sessionId;         // Session ID if established
    uint32_t flags;             // Connection flags
};
```

### ConnectionObject Structure

```c
struct ConnectionObject {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t connId;            // 0x04: Connection identifier
    uint32_t connState;         // 0x08: Connection state
    
    // Network details
    uint32_t ip_address;        // 0x0C: Remote IP (network byte order)
    uint16_t port;              // 0x10: Remote port
    uint16_t localPort;         // 0x12: Local port
    uint16_t status;            // 0x14: Connection status
    uint16_t reserved;          // 0x16: Reserved
    
    // Buffers
    void* send_buffer;          // 0x18: Send queue
    void* recv_buffer;          // 0x1C: Receive queue
    uint32_t sendBufferSize;    // 0x20: Send buffer size
    uint32_t recvBufferSize;    // 0x24: Receive buffer size
    
    // Session data
    uint32_t sessionId;         // 0x28: Session ID
    uint32_t sessionKey[4];     // 0x2C: Session key (128-bit)
};
```

---

## Usage

### Registration

Register connection callback during initialization:

```c
// Register connection callback
CallbackRegistration reg;
reg.eventType = EVENT_CONNECTION;
reg.callbackFunc = OnConnect;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnConnect callback
mov eax, [connection_callback]  ; Load callback pointer
test eax, eax                   ; Check if NULL
je skip_callback                ; Skip if no callback

; Prepare parameters
push flags                      ; Connection flags
push connId                     ; Connection ID
push connEvent                  ; Connection event structure
call eax                        ; Invoke callback
add esp, 12                     ; Cleanup stack

skip_callback:
```

---

## Implementation

### Launcher Side

```c
// Trigger connection event
void OnConnectionEstablished(ConnectionObject* conn) {
    // Create connection event
    ConnectionEvent event;
    event.eventType = CERT_CONNECTREPLY;
    event.connId = conn->connId;
    event.status = LTTCP_ALREADYCONNECTED;
    event.timestamp = GetTickCount();
    event.remoteIP = conn->ip_address;
    event.remotePort = conn->port;
    event.sessionId = conn->sessionId;
    
    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_CONNECTION);
    if (entry && entry->callback) {
        OnConnectCallback callback = (OnConnectCallback)entry->callback;
        callback(&event, conn->connId, 0);
    }
    
    // Log connection
    Log("LaunchPadClient %d connections opened/closed", conn->connId);
}

// Connect to remote host
int Connect(ConnectionObject* conn, const char* host, uint16_t port) {
    if (!conn || !host) {
        return -1;
    }
    
    // Check state
    if (conn->connState == LTTCP_ALREADYCONNECTED) {
        return LTAUTH_ALREADYCONNECTED;  // Already connected
    }
    
    // Set state
    conn->connState = LTTCP_CONNECTING;
    
    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        conn->connState = LTTCP_NOTCONNECTED;
        return -1;
    }
    
    // Connect
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(port);
    
    int result = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (result == 0) {
        conn->connState = LTTCP_ALREADYCONNECTED;
        conn->connId = GenerateConnId();
        
        // Trigger callback
        OnConnectionEstablished(conn);
        return 0;
    }
    
    // Failed
    closesocket(sock);
    conn->connState = LTTCP_NOTCONNECTED;
    return -1;
}
```

### Client Side

```c
// Connection callback implementation
void MyOnConnectCallback(ConnectionEvent* connEvent, uint32_t connId, uint32_t flags) {
    printf("Connection %d established\n", connId);
    printf("Remote: %d.%d.%d.%d:%d\n",
        (connEvent->remoteIP >> 0) & 0xFF,
        (connEvent->remoteIP >> 8) & 0xFF,
        (connEvent->remoteIP >> 16) & 0xFF,
        (connEvent->remoteIP >> 24) & 0xFF,
        connEvent->remotePort);
    
    switch (connEvent->eventType) {
        case CERT_CONNECTREPLY:
            HandleConnectionReply(connEvent);
            break;
            
        case LTMS_ALREADYCONNECTED:
            HandleMasterServerConnection(connEvent);
            break;
            
        case CERT_NEWSESSIONKEY:
            HandleNewSessionKey(connEvent);
            break;
    }
}

// Register connection callback
void RegisterConnectionCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_CONNECTION;
    reg.callbackFunc = MyOnConnectCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];
    
    int callbackId = regFunc(obj, &reg);
    printf("Registered connection callback, ID=%d\n", callbackId);
}
```

---

## Connection Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `LTTCP_SUCCESS` | Success |
| -1 | `LTTCP_ERROR` | General error |
| 0x1001 | `LTAUTH_ALREADYCONNECTED` | Already connected |
| 0x1002 | `LTMS_NODATABASECONNECTION` | No database connection |
| 0x1003 | `LTMS_INCOMPATIBLECLIENTVERSION` | Incompatible client version |
| 0x1004 | `LTMS_UNKNOWNSESSION` | Unknown session |
| 0x1005 | `LTMS_SESSIONESTABLISHINPROGRESS` | Session establishment in progress |
| 0x1006 | `LTMS_SOESESSIONVALIDATIONFAILED` | SOE session validation failed |
| 0x1007 | `LTMS_SOESESSIONCONSUMEFAILED` | SOE session consume failed |
| 0x1008 | `LTMS_SOESESSIONSTARTPLAYFAILED` | SOE session start play failed |

---

## Connection Flow

### Typical Connection Sequence

```
1. Client initiates connection
   └─> CERT_ConnectRequest sent to server

2. Server processes request
   └─> Validates client
   └─> Creates session

3. Server sends reply
   └─> CERT_ConnectReply sent to client
   └─> Includes session ID

4. Launcher receives reply
   └─> Updates state to LTTCP_ALREADYCONNECTED
   └─> Triggers OnConnect callback

5. Client processes OnConnect
   └─> Stores connection details
   └─> Prepares for data transfer

6. Session establishment
   └─> LTMS_SessionEstablishInProgress
   └─> CERT_NewSessionKey
   └─> LTMS_AlreadyConnected
```

---

## Diagnostic Strings

| String | Context | Purpose |
|--------|---------|---------|
| "LaunchPadClient %d connections opened/closed" | Connection logging | Track connection lifecycle |
| `LTTCP_ALREADYCONNECTED` | State constant | Already connected state |
| `LTTCP_NOTCONNECTED` | State constant | Not connected state |
| `CERT_ConnectRequest` | Protocol event | Connection request |
| `CERT_ConnectReply` | Protocol event | Connection reply |

---

## Notes

- **Critical network callback** for all connection handling
- Called when connection state changes to `LTTCP_ALREADYCONNECTED`
- Provides connection ID for future operations
- Includes remote IP and port information
- May be called multiple times for session establishment phases
- Should be registered early in initialization

---

## Security Considerations

- Validate connection source before accepting
- Use session keys for encryption
- Implement connection timeouts
- Handle connection errors gracefully
- Clear sensitive data on disconnect

---

## Related Callbacks

- [OnDisconnect](OnDisconnect.md) - Connection closed callback
- [OnPacket](OnPacket.md) - Packet received callback
- [OnConnectionError](OnConnectionError.md) - Connection error callback
- [OnDistributeMonitor](OnDistributeMonitor.md) - Distributed monitoring

---

## VTable Functions

Related VTable functions for connections:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 1 | 0x04 | Connect | Establish connection |
| 5 | 0x14 | Disconnect | Close connection |
| 6 | 0x18 | SendPacket | Send data |
| 7 | 0x1C | ReceivePacket | Receive data |

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 4.2
- **Source**: `data_structures_analysis.md` Section on ConnectionObject
- **Source**: `extracted_strings_analysis.md` TCP Connection State
- **Protocol**: CERT_ConnectRequest/Reply
- **State Machine**: LTTCP_NotConnected → LTTCP_AlreadyConnected

---

**Status**: ✅ Documented  
**Confidence**: High (inferred from connection state machine and protocol)  
**Last Updated**: 2025-06-17
