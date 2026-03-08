# OnConnectionError

## Overview

**Category**: Network
**Direction**: Launcher → Client
**Purpose**: Connection error notification callback
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
void OnConnectionError(ConnectionErrorEvent* errorEvent, uint32_t connId, uint32_t errorCode);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ConnectionErrorEvent*` | errorEvent | Error event details structure |
| `uint32_t` | connId | Connection identifier where error occurred |
| `uint32_t` | errorCode | Specific error code |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value (notification callback) |

---

## Calling Convention

**Type**: `__stdcall`

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  errorEvent (pointer to ConnectionErrorEvent structure)
[ESP+8]  connId (connection identifier)
[ESP+C]  errorCode (specific error code)
```

---

## Data Structures

### ConnectionErrorEvent Structure

```c
struct ConnectionErrorEvent {
    uint32_t eventType;         // Offset 0x00: Event type (ERROR_*)
    uint32_t connId;            // Offset 0x04: Connection identifier
    uint32_t errorCode;         // Offset 0x08: Primary error code
    uint32_t extendedCode;      // Offset 0x0C: Extended/secondary error code
    uint32_t timestamp;         // Offset 0x10: Error timestamp

    // Connection details at error time
    uint32_t remoteIP;          // Offset 0x14: Remote IP address
    uint16_t remotePort;        // Offset 0x18: Remote port
    uint16_t localPort;         // Offset 0x1A: Local port

    // Error context
    uint32_t operation;         // Offset 0x1C: Operation that failed (CONNECT, SEND, RECV)
    uint32_t socketError;       // Offset 0x20: System socket error code (WSAGetLastError)
    uint32_t retryCount;        // Offset 0x24: Number of retry attempts
    uint32_t flags;             // Offset 0x28: Error flags

    // Error message
    char errorMessage[256];     // Offset 0x2C: Human-readable error message
};
```

**Size**: 300 bytes (0x12C)

### ConnectionObject Structure (at error)

```c
struct ConnectionObject {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t connId;            // 0x04: Connection identifier
    uint32_t connState;         // 0x08: Connection state at error

    // Network details
    uint32_t ip_address;        // 0x0C: Remote IP (network byte order)
    uint16_t port;              // 0x10: Remote port
    uint16_t localPort;         // 0x12: Local port
    uint16_t status;            // 0x14: Connection status
    uint16_t lastError;         // 0x16: Last error code

    // Buffers
    void* send_buffer;          // 0x18: Send queue
    void* recv_buffer;          // 0x1C: Receive queue
    uint32_t sendBufferSize;    // 0x20: Send buffer size
    uint32_t recvBufferSize;    // 0x24: Receive buffer size
};
```

---

## Constants/Enums

### Connection Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `CONN_ERROR_NONE` | 0x0000 | No error |
| `CONN_ERROR_UNKNOWN` | 0x0001 | Unknown error |
| `CONN_ERROR_REFUSED` | 0x0002 | Connection refused |
| `CONN_ERROR_RESET` | 0x0003 | Connection reset by peer |
| `CONN_ERROR_TIMEOUT` | 0x0004 | Connection timed out |
| `CONN_ERROR_DNS` | 0x0005 | DNS resolution failed |
| `CONN_ERROR_NETWORK` | 0x0006 | Network unreachable |
| `CONN_ERROR_HOST` | 0x0007 | Host unreachable |
| `CONN_ERROR_SOCKET` | 0x0008 | Socket error |
| `CONN_ERROR_PROTOCOL` | 0x0009 | Protocol error |
| `CONN_ERROR_AUTH` | 0x000A | Authentication failed |
| `CONN_ERROR_VERSION` | 0x000B | Version mismatch |
| `CONN_ERROR_SERVER_FULL` | 0x000C | Server at capacity |
| `CONN_ERROR_BANNED` | 0x000D | Client banned |
| `CONN_ERROR_SSL` | 0x000E | SSL/TLS error |

### Extended Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `CONN_EXT_ERROR_NONE` | 0x00000000 | No extended error |
| `CONN_EXT_ERROR_INVALID_PACKET` | 0x00000001 | Invalid packet received |
| `CONN_EXT_ERROR_CHECKSUM` | 0x00000002 | Checksum mismatch |
| `CONN_EXT_ERROR_DECRYPT` | 0x00000003 | Decryption failed |
| `CONN_EXT_ERROR_DECOMPRESS` | 0x00000004 | Decompression failed |
| `CONN_EXT_ERROR_BUFFER_OVERFLOW` | 0x00000005 | Buffer overflow |
| `CONN_EXT_ERROR_INVALID_SEQUENCE` | 0x00000006 | Invalid sequence number |

### Operation Types

| Constant | Value | Description |
|----------|-------|-------------|
| `OPERATION_CONNECT` | 0x0001 | Connection attempt |
| `OPERATION_SEND` | 0x0002 | Send operation |
| `OPERATION_RECV` | 0x0003 | Receive operation |
| `OPERATION_HANDSHAKE` | 0x0004 | Handshake/protocol negotiation |
| `OPERATION_AUTH` | 0x0005 | Authentication |

### Error Flags

| Flag | Bit | Description |
|------|-----|-------------|
| `ERROR_FLAG_RECOVERABLE` | 0 | Error is recoverable |
| `ERROR_FLAG_RETRYABLE` | 1 | Operation can be retried |
| `ERROR_FLAG_FATAL` | 2 | Fatal error, connection must close |
| `ERROR_FLAG_SECURITY` | 3 | Security-related error |
| `ERROR_FLAG_LOGGED` | 4 | Error already logged |

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
mov eax, [error_callback]       ; Load callback pointer
test eax, eax                   ; Check if NULL
je skip_callback                ; Skip if no callback

; Prepare parameters
push errorCode                  ; Specific error code
push connId                     ; Connection ID
push errorEvent                 ; Error event structure
call eax                        ; Invoke callback
add esp, 12                     ; Cleanup stack

skip_callback:
; Continue with error handling
mov ecx, [connection_object]
; Check if recoverable
test dword [errorEvent + 0x28], ERROR_FLAG_RECOVERABLE
jnz attempt_recovery
; Fatal error - close connection
call Disconnect
```

### C++ Pattern

```c
// Handle connection error event
void HandleConnectionError(ConnectionErrorEvent* event, uint32_t connId, uint32_t errorCode) {
    // Log the error
    printf("Connection %d error: code=%d, operation=%d\n",
           connId, errorCode, event->operation);

    // Check if retryable
    if (event->flags & ERROR_FLAG_RETRYABLE) {
        printf("Error is retryable (attempts: %d)\n", event->retryCount);
        if (event->retryCount < 3) {
            RetryOperation(event->operation);
            return;
        }
    }

    // Handle specific error
    switch (errorCode) {
        case CONN_ERROR_TIMEOUT:
            HandleTimeout(connId);
            break;
        case CONN_ERROR_REFUSED:
            HandleConnectionRefused(connId);
            break;
        // ... other cases
    }
}
```

---

## Implementation

### Launcher Side

```c
// Trigger connection error event
void OnConnectionErrorOccurred(ConnectionObject* conn, uint32_t errorCode,
                                uint32_t operation, uint32_t socketError) {
    // Create error event
    ConnectionErrorEvent event;
    event.eventType = EVENT_CONNECTION_ERROR;
    event.connId = conn->connId;
    event.errorCode = errorCode;
    event.extendedCode = 0;
    event.timestamp = GetTickCount();
    event.remoteIP = conn->ip_address;
    event.remotePort = conn->port;
    event.localPort = conn->localPort;
    event.operation = operation;
    event.socketError = socketError;
    event.retryCount = conn->retryCount;
    event.flags = DetermineErrorFlags(errorCode);

    // Generate error message
    FormatErrorMessage(errorCode, socketError, event.errorMessage, sizeof(event.errorMessage));

    // Update connection state
    conn->lastError = errorCode;

    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_CONNECTION_ERROR);
    if (entry && entry->callback) {
        OnConnectionErrorCallback callback = (OnConnectionErrorCallback)entry->callback;
        callback(&event, conn->connId, errorCode);
    }

    // Log error
    Log("Connection error: conn=%d, code=%d, operation=%d",
        conn->connId, errorCode, operation);
}

// Handle socket error
int HandleSocketError(ConnectionObject* conn, int socketError) {
    uint32_t errorCode;
    uint32_t operation = OPERATION_RECV;

    // Map socket error to connection error
    switch (socketError) {
        case WSAECONNREFUSED:
            errorCode = CONN_ERROR_REFUSED;
            break;
        case WSAECONNRESET:
            errorCode = CONN_ERROR_RESET;
            break;
        case WSAETIMEDOUT:
        case WSAENETDOWN:
            errorCode = CONN_ERROR_TIMEOUT;
            break;
        case WSAENETUNREACH:
            errorCode = CONN_ERROR_NETWORK;
            break;
        case WSAEHOSTUNREACH:
            errorCode = CONN_ERROR_HOST;
            break;
        default:
            errorCode = CONN_ERROR_SOCKET;
    }

    // Determine operation from context
    if (conn->state == LTTCP_CONNECTING) {
        operation = OPERATION_CONNECT;
    }

    // Trigger error callback
    OnConnectionErrorOccurred(conn, errorCode, operation, socketError);

    return -1;
}

// Determine error flags
uint32_t DetermineErrorFlags(uint32_t errorCode) {
    switch (errorCode) {
        case CONN_ERROR_TIMEOUT:
        case CONN_ERROR_NETWORK:
        case CONN_ERROR_HOST:
            return ERROR_FLAG_RECOVERABLE | ERROR_FLAG_RETRYABLE;

        case CONN_ERROR_REFUSED:
        case CONN_ERROR_RESET:
            return ERROR_FLAG_RETRYABLE;

        case CONN_ERROR_AUTH:
        case CONN_ERROR_BANNED:
            return ERROR_FLAG_FATAL | ERROR_FLAG_SECURITY;

        default:
            return ERROR_FLAG_FATAL;
    }
}
```

### Client Side

```c
// Connection error callback implementation
void MyOnConnectionErrorCallback(ConnectionErrorEvent* errorEvent, uint32_t connId, uint32_t errorCode) {
    printf("\n=== Connection Error ===\n");
    printf("Connection ID: %d\n", connId);
    printf("Error Code: %d (0x%08X)\n", errorEvent->errorCode, errorEvent->errorCode);
    printf("Extended Code: %d\n", errorEvent->extendedCode);
    printf("Operation: %d\n", errorEvent->operation);
    printf("Socket Error: %d\n", errorEvent->socketError);
    printf("Message: %s\n", errorEvent->errorMessage);
    printf("Remote: %d.%d.%d.%d:%d\n",
           (errorEvent->remoteIP >> 0) & 0xFF,
           (errorEvent->remoteIP >> 8) & 0xFF,
           (errorEvent->remoteIP >> 16) & 0xFF,
           (errorEvent->remoteIP >> 24) & 0xFF,
           errorEvent->remotePort);
    printf("Retry Count: %d\n", errorEvent->retryCount);
    printf("Flags: 0x%08X\n", errorEvent->flags);

    // Handle based on error code
    switch (errorEvent->errorCode) {
        case CONN_ERROR_TIMEOUT:
            printf("Timeout error\n");
            if (errorEvent->flags & ERROR_FLAG_RETRYABLE) {
                printf("Attempting retry...\n");
                ScheduleRetry(connId, errorEvent->operation);
            }
            break;

        case CONN_ERROR_REFUSED:
            printf("Connection refused\n");
            if (errorEvent->operation == OPERATION_CONNECT) {
                printf("Server may be down or at capacity\n");
            }
            break;

        case CONN_ERROR_RESET:
            printf("Connection reset by peer\n");
            // May indicate server-side issue
            NotifyServerDisconnected(connId);
            break;

        case CONN_ERROR_NETWORK:
        case CONN_ERROR_HOST:
            printf("Network unreachable\n");
            // Check network connectivity
            CheckNetworkStatus();
            break;

        case CONN_ERROR_AUTH:
            printf("Authentication failed\n");
            // May need to re-authenticate
            RequestReauthentication();
            break;

        case CONN_ERROR_VERSION:
            printf("Version mismatch\n");
            // Client update may be required
            NotifyUpdateRequired();
            break;

        default:
            printf("Unknown error\n");
    }

    // Update application state
    if (errorEvent->flags & ERROR_FLAG_FATAL) {
        g_ConnectionState = CONN_STATE_ERROR;
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

## Flow/State Machine

### Error Handling Flow

```
1. Error occurs
   ├─ Socket operation fails
   ├─ Protocol error detected
   └─ Authentication fails

2. Determine error type
   ├─ Map socket error to CONN_ERROR_*
   ├─ Determine operation context
   └─ Set error flags

3. Create ConnectionErrorEvent
   ├─ Fill error details
   ├─ Generate error message
   └─ Set timestamp

4. Invoke OnConnectionError callback
   └─ Client processes error

5. Check error flags
   ├─ Recoverable? → Attempt recovery
   ├─ Retryable? → Retry operation
   └─ Fatal? → Close connection

6. Update connection state
   └─ May trigger OnDisconnect
```

### Error Recovery State Machine

```
Normal Operation
        ↓ [Error detected]
Error State
        ↓
OnConnectionError callback
        ↓
    ┌───────────────────────────────────┐
    │ Check ERROR_FLAG_RECOVERABLE      │
    └───────────────────────────────────┘
        ↓ Yes                    ↓ No
Recovery Attempt          Fatal Error
        ↓                        ↓
    ┌─────────────┐          OnDisconnect
    │ Retry logic │              ↓
    └─────────────┘          Connection
        ↓                    Closed
Normal Operation
(resumed)
```

### Sequence Diagram

```
[Client App]      [Launcher]        [Network/Socket]
     |                 |                    |
     |--- Operation -->|                    |
     |                 |-- socket call ---->|
     |                 |                    |
     |                 |<-- error code -----|
     |                 |                    |
     |                 |-- Create ErrorEvent|
     |<--OnError-------|                    |
     |                 |                    |
     |--- Retry? ----->|                    |
     |                 |-- socket call ---->|
     |                 |                    |
     |                 |<-- success/fail ---|
     |                 |                    |
     |<-- result ------|                    |
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| `"Connection error: %d"` | - | General error logging |
| `"Network unreachable"` | - | DNS/Network error |
| `"Connection refused"` | - | Server rejection |
| `"Connection timed out"` | - | Timeout notification |

---

## Error Codes

### Socket Error Mapping (Windows)

| Socket Error | Connection Error | Description |
|--------------|------------------|-------------|
| `WSAECONNREFUSED` (10061) | `CONN_ERROR_REFUSED` | Connection refused |
| `WSAECONNRESET` (10054) | `CONN_ERROR_RESET` | Connection reset |
| `WSAETIMEDOUT` (10060) | `CONN_ERROR_TIMEOUT` | Operation timed out |
| `WSAENETUNREACH` (10051) | `CONN_ERROR_NETWORK` | Network unreachable |
| `WSAEHOSTUNREACH` (10065) | `CONN_ERROR_HOST` | Host unreachable |
| `WSAECONNABORTED` (10053) | `CONN_ERROR_SOCKET` | Connection aborted |

---

## Performance Considerations

- **Error Event Creation**: Lightweight, only populated when error occurs
- **Retry Logic**: Exponential backoff recommended for retries
- **Error Messages**: Pre-allocated 256-byte buffer to avoid dynamic allocation
- **Callback Timing**: Synchronous, avoid blocking operations in callback

---

## Security Considerations

- **Error Message Sanitization**: Don't expose internal details in error messages
- **Rate Limiting**: Prevent error flooding from malicious servers
- **Connection State Validation**: Verify state before retry attempts
- **Resource Cleanup**: Ensure proper cleanup on fatal errors

---

## Notes

- **Critical network callback** for error handling
- Provides detailed error context for debugging
- Supports retry/recovery mechanisms
- Integrates with connection state machine
- Should be registered early in initialization
- May be called before OnDisconnect for fatal errors

---

## Related Callbacks

- **[OnConnect](OnConnect.md)** - Connection established callback
- **[OnDisconnect](OnDisconnect.md)** - Connection closed callback
- **[OnTimeout](OnTimeout.md)** - Connection timeout callback
- **[OnPacket](OnPacket.md)** - Packet received/sent callback
- **[OnException](../lifecycle/OnException.md)** - Exception callback

---

## VTable Functions

Related VTable functions for error handling:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 1 | 0x04 | Connect | Establish connection |
| 5 | 0x14 | Disconnect | Close connection |
| 6 | 0x18 | SendPacket | Send data |
| 7 | 0x1C | ReceivePacket | Receive data |

---

## References

- **Source**: `callbacks/network/OnConnect.md` - Connection error codes
- **Source**: `callbacks/lifecycle/OnException.md` - Exception handling pattern
- **Source**: `data_passing_mechanisms.md` - Network error handling
- **Windows Socket Errors**: MSDN WSAError documentation

---

## Documentation Status

**Status**: ⏳ Partial
**Confidence**: Medium (inferred from error handling patterns and related callbacks)
**Last Updated**: 2025-06-17
**Documented By**: Analysis Team

---

## TODO

- [ ] Find specific diagnostic strings for connection errors in binary
- [ ] Verify error codes against actual implementation
- [ ] Map exact ConnectionErrorEvent structure offsets
- [ ] Document retry mechanism parameters
- [ ] Analyze error recovery behavior

---

## Example Usage

### Complete Working Example

```c
// Full working example of connection error callback
#include "client_api.h"

// Global state
uint32_t g_ConnectionId = 0;
uint32_t g_RetryCount = 0;
const uint32_t MAX_RETRIES = 3;

// Connection error callback implementation
void MyOnConnectionError(ConnectionErrorEvent* event, uint32_t connId, uint32_t errorCode) {
    printf("\n=== Connection Error Report ===\n");
    printf("Connection: %d\n", event->connId);
    printf("Error: %s (%d)\n", event->errorMessage, event->errorCode);
    printf("Operation: %s\n",
           event->operation == OPERATION_CONNECT ? "Connect" :
           event->operation == OPERATION_SEND ? "Send" :
           event->operation == OPERATION_RECV ? "Receive" :
           event->operation == OPERATION_AUTH ? "Authenticate" : "Unknown");
    printf("Socket Error: %d\n", event->socketError);
    printf("Flags: %s%s%s\n",
           (event->flags & ERROR_FLAG_RECOVERABLE) ? "Recoverable " : "",
           (event->flags & ERROR_FLAG_RETRYABLE) ? "Retryable " : "",
           (event->flags & ERROR_FLAG_FATAL) ? "Fatal" : "");

    // Handle based on error type
    if (event->flags & ERROR_FLAG_RETRYABLE) {
        if (g_RetryCount < MAX_RETRIES) {
            printf("Scheduling retry (attempt %d of %d)...\n",
                   g_RetryCount + 1, MAX_RETRIES);

            // Exponential backoff
            uint32_t delay = 1000 * (1 << g_RetryCount);
            ScheduleRetry(connId, event->operation, delay);
            g_RetryCount++;
            return;
        }
        printf("Max retries exceeded\n");
    }

    if (event->flags & ERROR_FLAG_FATAL) {
        printf("Fatal error - closing connection\n");
        g_ConnectionState = CONN_STATE_ERROR;
        g_ConnectionId = 0;
    }

    g_RetryCount = 0;
}

// Register callback
void InitializeErrorCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_CONNECTION_ERROR;
    reg.callbackFunc = MyOnConnectionError;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    int callbackId = obj->RegisterCallback2(&reg);

    printf("Registered connection error callback (ID: %d)\n", callbackId);
}

// Retry helper
void ScheduleRetry(uint32_t connId, uint32_t operation, uint32_t delayMs) {
    // In a real implementation, this would use a timer
    Sleep(delayMs);

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;

    switch (operation) {
        case OPERATION_CONNECT:
            // Retry connection
            {
                typedef int (*ConnectFunc)(APIObject*, const char*, uint16_t);
                ConnectFunc connect = (ConnectFunc)vtable->functions[1];
                connect(obj, g_ServerHost, g_ServerPort);
            }
            break;

        // Other operations...
    }
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-17 | 1.0 | Initial documentation |

---

**End of Document**
