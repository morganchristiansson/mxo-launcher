# OnConnectionError

## Overview

**Category**: Network  
**Direction**: Launcher → Client  
**Purpose**: Connection error notification callback

---

## Function Signature

```c
void OnConnectionError(ConnectionEvent* connEvent, uint32_t connId, uint32_t errorCode);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ConnectionEvent*` | connEvent | Connection event details |
| `uint32_t` | connId | Unique connection identifier |
| `uint32_t` | errorCode | Connection error code |

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
| `CERT_DisconnectRequest` | Client disconnecting |
| `CERT_DisconnectReply` | Server rejecting disconnect |

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

Register connection error callback during initialization:

```c
// Register connection error callback
CallbackRegistration reg;
reg.eventType = EVENT_CONNECTION_ERROR;
reg.callbackFunc = OnConnectionError;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnConnectionError callback
mov eax, [connection_error_callback]  ; Load callback pointer
test eax, eax                         ; Check if NULL
je skip_error_callback                ; Skip if no callback

; Prepare parameters
push errorCode                        ; Connection error code
push connId                           ; Connection ID
push connEvent                        ; Connection event structure
call eax                              ; Invoke callback
add esp, 12                            ; Cleanup stack

skip_error_callback:
```

---

## Implementation

### Launcher Side

```c
// Trigger connection error event
void OnConnectionErrorTriggered(ConnectionObject* conn, uint32_t errorCode) {
    // Create connection event
    ConnectionEvent event;
    event.eventType = LTMS_NODATABASECONNECTION;
    event.connId = conn->connId;
    event.status = errorCode;
    event.timestamp = GetTickCount();
    event.remoteIP = conn->ip_address;
    event.remotePort = conn->port;
    event.sessionId = conn->sessionId;
    
    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_CONNECTION_ERROR);
    if (entry && entry->callback) {
        OnConnectionErrorCallback callback = (OnConnectionErrorCallback)entry->callback;
        callback(&event, conn->connId, errorCode);
    }
    
    // Log error
    Log("LaunchPadClient %d connection error: %d", conn->connId, errorCode);
}

// Handle connection error
int HandleConnectionError(uint32_t errorCode) {
    switch (errorCode) {
        case LTAUTH_ALREADYCONNECTED:
            return LTAUTH_SUCCESS;  // Already connected
        case LTMS_NODATABASECONNECTION:
            return LTMS_NO_DATABASE_CONNECTION;  // No database connection
        case LTMS_INCOMPATIBLECLIENTVERSION:
            return LTMS_INCOMPATIBLE_CLIENT_VERSION;  // Incompatible client version
        case LTMS_UNKNOWNSESSION:
            return LTMS_UNKNOWN_SESSION;  // Unknown session
        case LTMS_SOESESSIONVALIDATIONFAILED:
            return LTMS_SOE_SESSION_VALIDATION_FAILED;  // SOE session validation failed
        case LTMS_SOESESSIONCONSUMEFAILED:
            return LTMS_SOE_SESSION_CONSUME_FAILED;  // SOE session consume failed
        case LTMS_SOESESSIONSTARTPLAYFAILED:
            return LTMS_SOE_SESSION_START_PLAY_FAILED;  // SOE session start play failed
    }
    
    return LTAUTH_SUCCESS;  // Default success
}
```

### Client Side

```c
// Connection error callback implementation
void MyOnConnectionErrorCallback(ConnectionEvent* connEvent, uint32_t connId, uint32_t errorCode) {
    printf("Connection %d error: %d\n", connId, errorCode);
    
    switch (errorCode) {
        case LTAUTH_ALREADYCONNECTED:
            HandleAlreadyConnected(connId);
            break;
            
        case LTMS_NODATABASECONNECTION:
            HandleNoDatabaseConnection(connId);
            break;
            
        case LTMS_INCOMPATIBLECLIENTVERSION:
            HandleIncompatibleClientVersion(connId);
            break;
    }
}

// Register connection error callback
void RegisterConnectionErrorCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_CONNECTION_ERROR;
    reg.callbackFunc = MyOnConnectionErrorCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];
    
    int callbackId = regFunc(obj, &reg);
    printf("Registered connection error callback, ID=%d\n", callbackId);
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

### Typical Error Handling Sequence

```
1. Connection attempt fails
   └─> CERT_ConnectRequest sent to server

2. Server rejects connection
   └─> Returns error code

3. Launcher receives error
   └─> Updates state to LTTCP_NOTCONNECTED
   └─> Triggers OnConnectionError callback

4. Client processes OnConnectionError
   └─> Handles specific error code
   └─> Logs error message
   └─> Prepares for retry

5. Retry connection
   └─> Send CERT_ConnectRequest again
```

---

## Diagnostic Strings

| String | Context | Purpose |
|--------|---------|---------|
| "LaunchPadClient %d connection error: %d" | Connection error logging | Track error lifecycle |
| `LTTCP_SUCCESS` | State constant | Success state |
| `LTMS_NODATABASECONNECTION` | State constant | No database connection |
| `CERT_ConnectRequest` | Protocol event | Connection request |
| `CERT_DisconnectReply` | Protocol event | Disconnect reply |

---

## Notes

- **Critical network callback** for all error handling
- Called when connection fails or encounters errors
- Provides specific error code for debugging
- Includes connection ID for context
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

- [OnConnect](OnConnect.md) - Connection established callback
- [OnDisconnect](OnDisconnect.md) - Connection closed callback
- [OnPacket](OnPacket.md) - Packet received callback
- [OnConnectionError](OnConnectionError.md) - Connection error callback

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
- **State Machine**: LTTCP_AlreadyConnected → LTTCP_NotConnected

---

**Status**: ✅ Documented  
**Confidence**: High (inferred from connection state machine and protocol)  
**Last Updated**: 2025-06-17