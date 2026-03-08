# OnDisconnect

## Overview

**Category**: Network
**Direction**: Launcher → Client
**Purpose**: Connection closed notification callback
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
void OnDisconnect(DisconnectEvent* disconnectEvent, uint32_t connId, uint32_t reason);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `DisconnectEvent*` | disconnectEvent | Disconnect event details structure |
| `uint32_t` | connId | Connection identifier being closed |
| `uint32_t` | reason | Disconnect reason code |

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
[ESP+4]  disconnectEvent (pointer to DisconnectEvent structure)
[ESP+8]  connId (connection identifier)
[ESP+C]  reason (disconnect reason code)
```

---

## Data Structures

### DisconnectEvent Structure

```c
struct DisconnectEvent {
    uint32_t eventType;         // Offset 0x00: Event type (DISCONNECT_*)
    uint32_t connId;            // Offset 0x04: Connection being closed
    uint32_t reason;            // Offset 0x08: Disconnect reason code
    uint32_t timestamp;         // Offset 0x0C: Event timestamp

    // Connection details at disconnect time
    uint32_t remoteIP;          // Offset 0x10: Remote IP address
    uint16_t remotePort;        // Offset 0x14: Remote port
    uint16_t localPort;         // Offset 0x16: Local port

    // Statistics
    uint32_t bytesSent;         // Offset 0x18: Total bytes sent
    uint32_t bytesReceived;     // Offset 0x1C: Total bytes received
    uint32_t packetsSent;       // Offset 0x20: Packets sent count
    uint32_t packetsReceived;   // Offset 0x24: Packets received count
    uint32_t sessionDuration;   // Offset 0x28: Session duration in seconds

    // Session info
    uint32_t sessionId;         // Offset 0x2C: Session ID (if established)
    uint32_t flags;             // Offset 0x30: Disconnect flags
};
```

**Size**: 52 bytes (0x34)

### ConnectionObject Structure (at disconnect)

```c
struct ConnectionObject {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t connId;            // 0x04: Connection identifier
    uint32_t connState;         // 0x08: Connection state (LTTCP_NOTCONNECTED)

    // Network details
    uint32_t ip_address;        // 0x0C: Remote IP (network byte order)
    uint16_t port;              // 0x10: Remote port
    uint16_t localPort;         // 0x12: Local port
    uint16_t status;            // 0x14: Connection status
    uint16_t disconnectReason;  // 0x16: Reason for disconnect

    // Buffers (to be freed)
    void* send_buffer;          // 0x18: Send queue (pending sends)
    void* recv_buffer;          // 0x1C: Receive queue (pending receives)
    uint32_t sendBufferSize;    // 0x20: Send buffer size
    uint32_t recvBufferSize;    // 0x24: Receive buffer size

    // Session data
    uint32_t sessionId;         // 0x28: Session ID
    uint32_t sessionKey[4];     // 0x2C: Session key (to be cleared)
};
```

---

## Constants/Enums

### Disconnect Reason Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `DISCONNECT_REASON_NORMAL` | 0x0000 | Normal disconnect (client initiated) |
| `DISCONNECT_REASON_TIMEOUT` | 0x0001 | Connection timed out |
| `DISCONNECT_REASON_REMOTE_CLOSE` | 0x0002 | Remote host closed connection |
| `DISCONNECT_REASON_ERROR` | 0x0003 | Network error |
| `DISCONNECT_REASON_SHUTDOWN` | 0x0004 | Application shutdown |
| `DISCONNECT_REASON_RESET` | 0x0005 | Connection reset by peer |
| `DISCONNECT_REASON_AUTH_FAILED` | 0x0006 | Authentication failed |
| `DISCONNECT_REASON_SESSION_EXPIRED` | 0x0007 | Session expired |
| `DISCONNECT_REASON_SERVER_FULL` | 0x0008 | Server at capacity |
| `DISCONNECT_REASON_KICKED` | 0x0009 | Kicked by server/admin |

### Disconnect Event Types

| Constant | Value | Description |
|----------|-------|-------------|
| `DISCONNECT_EVENT_CLOSE` | 0x0001 | Connection closing |
| `DISCONNECT_EVENT_CLOSED` | 0x0002 | Connection fully closed |
| `DISCONNECT_EVENT_ERROR` | 0x0003 | Disconnect due to error |

### Disconnect Flags

| Flag | Bit | Description |
|------|-----|-------------|
| `DISCONNECT_FLAG_FLUSH_SEND` | 0 | Attempt to flush send buffer |
| `DISCONNECT_FLAG_KEEP_SESSION` | 1 | Keep session data |
| `DISCONNECT_FLAG_RECONNECTABLE` | 2 | Connection can be re-established |
| `DISCONNECT_FLAG_CLEANUP_PENDING` | 3 | Pending cleanup operations |

---

## Usage

### Registration

Register disconnect callback during initialization:

```c
// Register disconnect callback
CallbackRegistration reg;
reg.eventType = EVENT_DISCONNECT;
reg.callbackFunc = OnDisconnect;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnDisconnect callback
mov eax, [disconnect_callback]  ; Load callback pointer
test eax, eax                   ; Check if NULL
je skip_callback                ; Skip if no callback

; Prepare parameters
push reason                     ; Disconnect reason
push connId                     ; Connection ID
push disconnectEvent            ; Disconnect event structure
call eax                        ; Invoke callback
add esp, 12                     ; Cleanup stack

skip_callback:
; Continue with cleanup
mov ecx, [connection_object]
mov edx, [ecx]                  ; Get vtable
call dword [edx + 0x14]         ; Call vtable[5] - UnregisterCallback
```

### C++ Pattern

```c
// Handle disconnect event
void HandleDisconnect(DisconnectEvent* event, uint32_t connId, uint32_t reason) {
    // Store connection stats before cleanup
    g_LastDisconnectStats.bytesSent = event->bytesSent;
    g_LastDisconnectStats.bytesReceived = event->bytesReceived;
    g_LastDisconnectStats.duration = event->sessionDuration;

    // Log disconnect
    printf("Connection %d closed: reason=%d, duration=%ds\n",
           connId, reason, event->sessionDuration);
}
```

---

## Implementation

### Launcher Side

```c
// Trigger disconnect event
void OnConnectionClosed(ConnectionObject* conn, uint32_t reason) {
    // Create disconnect event
    DisconnectEvent event;
    event.eventType = DISCONNECT_EVENT_CLOSED;
    event.connId = conn->connId;
    event.reason = reason;
    event.timestamp = GetTickCount();
    event.remoteIP = conn->ip_address;
    event.remotePort = conn->port;
    event.localPort = conn->localPort;

    // Get statistics
    event.bytesSent = conn->bytesSent;
    event.bytesReceived = conn->bytesReceived;
    event.packetsSent = conn->packetsSent;
    event.packetsReceived = conn->packetsReceived;
    event.sessionDuration = (GetTickCount() - conn->connectTime) / 1000;

    event.sessionId = conn->sessionId;
    event.flags = 0;

    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_DISCONNECT);
    if (entry && entry->callback) {
        OnDisconnectCallback callback = (OnDisconnectCallback)entry->callback;
        callback(&event, conn->connId, reason);
    }

    // Log connection lifecycle
    Log("LaunchPadClient %d connections opened/closed", conn->connId);
}

// Close connection
int Disconnect(ConnectionObject* conn, uint32_t reason) {
    if (!conn) {
        return -1;
    }

    // Check state
    if (conn->connState == LTTCP_NOTCONNECTED) {
        return 0;  // Already disconnected
    }

    // Update state
    conn->connState = LTTCP_NOTCONNECTED;
    conn->disconnectReason = reason;

    // Close socket
    if (conn->socket != INVALID_SOCKET) {
        shutdown(conn->socket, SD_BOTH);
        closesocket(conn->socket);
        conn->socket = INVALID_SOCKET;
    }

    // Trigger callback
    OnConnectionClosed(conn, reason);

    // Clear session key (security)
    memset(conn->sessionKey, 0, sizeof(conn->sessionKey));

    // Free buffers
    if (conn->send_buffer) {
        free(conn->send_buffer);
        conn->send_buffer = NULL;
    }
    if (conn->recv_buffer) {
        free(conn->recv_buffer);
        conn->recv_buffer = NULL;
    }

    return 0;
}
```

### Client Side

```c
// Disconnect callback implementation
void MyOnDisconnectCallback(DisconnectEvent* disconnectEvent, uint32_t connId, uint32_t reason) {
    printf("Connection %d closed\n", connId);
    printf("Remote: %d.%d.%d.%d:%d\n",
        (disconnectEvent->remoteIP >> 0) & 0xFF,
        (disconnectEvent->remoteIP >> 8) & 0xFF,
        (disconnectEvent->remoteIP >> 16) & 0xFF,
        (disconnectEvent->remoteIP >> 24) & 0xFF,
        disconnectEvent->remotePort);

    printf("Reason: %d\n", disconnectEvent->reason);
    printf("Duration: %d seconds\n", disconnectEvent->sessionDuration);
    printf("Bytes: sent=%d, received=%d\n",
           disconnectEvent->bytesSent,
           disconnectEvent->bytesReceived);

    // Handle based on reason
    switch (disconnectEvent->reason) {
        case DISCONNECT_REASON_NORMAL:
            printf("Normal disconnect\n");
            break;

        case DISCONNECT_REASON_TIMEOUT:
            printf("Connection timed out - attempting reconnect\n");
            ScheduleReconnect(disconnectEvent->remoteIP, disconnectEvent->remotePort);
            break;

        case DISCONNECT_REASON_REMOTE_CLOSE:
            printf("Remote host closed connection\n");
            break;

        case DISCONNECT_REASON_ERROR:
            printf("Network error occurred\n");
            break;

        case DISCONNECT_REASON_KICKED:
            printf("Kicked from server\n");
            break;

        default:
            printf("Unknown disconnect reason: %d\n", disconnectEvent->reason);
    }

    // Update application state
    g_ConnectionState = CONN_STATE_DISCONNECTED;
}

// Register disconnect callback
void RegisterDisconnectCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_DISCONNECT;
    reg.callbackFunc = MyOnDisconnectCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;

    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];

    int callbackId = regFunc(obj, &reg);
    printf("Registered disconnect callback, ID=%d\n", callbackId);
}
```

---

## Flow/State Machine

### Connection Lifecycle State Machine

```
LTTCP_NotConnected
        ↓ [Connect request - OnConnect]
LTTCP_Connecting
        ↓ [Connection established]
LTTCP_AlreadyConnected
        ↓ [Disconnect request - OnDisconnect]
LTTCP_NotConnected
```

### Disconnect Sequence

```
1. Disconnect triggered
   ├─ Client calls Disconnect()
   ├─ Remote closes connection
   ├─ Timeout detected
   └─ Error occurs

2. Update connection state
   └─ Set connState = LTTCP_NOTCONNECTED

3. Close socket
   ├─ shutdown(sock, SD_BOTH)
   └─ closesocket(sock)

4. Create DisconnectEvent
   ├─ Fill event details
   ├─ Calculate statistics
   └─ Set reason code

5. Invoke OnDisconnect callback
   └─ Client processes disconnect

6. Log connection lifecycle
   └─ "LaunchPadClient %d connections opened/closed"

7. Cleanup resources
   ├─ Clear session key (security)
   ├─ Free send buffer
   └─ Free receive buffer
```

### Sequence Diagram

```
[Client App]      [Launcher]        [Network]        [Remote Host]
     |                 |                |                  |
     |---Disconnect--->|                |                  |
     |                 |-- shutdown() ->|                  |
     |                 |                |---- FIN -------->|
     |                 |                |<--- ACK ---------|
     |                 |                |<--- FIN ---------|
     |                 |                |---- ACK -------->|
     |                 |                |                  |
     |                 |-- Create DisconnectEvent          |
     |<--OnDisconnect--|                |                  |
     |                 |                |                  |
     |                 |-- Cleanup ---->|                  |
     |                 |   (free buffers, clear keys)      |
```

---

## Diagnostic Strings

Strings found in binaries related to this callback:

| String | Address | Context |
|--------|---------|---------|
| `"LaunchPadClient %d connections opened/closed"` | `0x004b...` | Connection lifecycle logging |
| `"LTTCP_NotConnected"` | - | State constant for disconnected state |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `DISCONNECT_SUCCESS` | Disconnect completed successfully |
| -1 | `DISCONNECT_ERROR_INVALID_CONN` | Invalid connection object |
| -2 | `DISCONNECT_ERROR_ALREADY_CLOSED` | Connection already closed |
| -3 | `DISCONNECT_ERROR_SOCKET` | Socket operation failed |

---

## Performance Considerations

- **Buffer Cleanup**: Send and receive buffers are freed during disconnect
- **Socket Closure**: Proper shutdown sequence ensures data is flushed
- **Callback Timing**: Callback invoked before cleanup to allow client access to stats
- **Statistics Collection**: Gathered before cleanup for accurate reporting

---

## Security Considerations

- **Session Key Clearing**: Session keys are zeroed out before memory is freed
- **Buffer Zeroing**: Consider zeroing sensitive data in buffers before freeing
- **State Validation**: Always verify connection state before operations
- **Resource Cleanup**: Ensure all resources are properly freed to prevent leaks

---

## Notes

- **Critical network callback** for connection lifecycle management
- Called when connection state changes to `LTTCP_NOTCONNECTED`
- Provides disconnect reason for proper handling
- Includes connection statistics for logging/debugging
- Invoked before resource cleanup to allow client access to final state
- Should be registered early in initialization

---

## Related Callbacks

- **[OnConnect](OnConnect.md)** - Connection established callback
- **[OnPacket](OnPacket.md)** - Packet received/sent callback
- **[OnConnectionError](OnConnectionError.md)** - Connection error callback
- **[OnTimeout](OnTimeout.md)** - Connection timeout callback
- **[OnDistributeMonitor](OnDistributeMonitor.md)** - Distributed monitoring

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

- **Source**: `callbacks/network/OnConnect.md` - Connection state machine
- **Source**: `MASTER_DATABASE.md` - Disconnect VTable function
- **Source**: `data_passing_mechanisms.md` - Network disconnection handling
- **State Machine**: LTTCP_AlreadyConnected → LTTCP_NotConnected
- **Logging**: "LaunchPadClient %d connections opened/closed"

---

## Documentation Status

**Status**: ⏳ Partial
**Confidence**: Medium (inferred from connection state machine and related callbacks)
**Last Updated**: 2025-06-17
**Documented By**: Analysis Team

---

## TODO

- [ ] Find specific diagnostic strings for disconnect in binary
- [ ] Verify disconnect reason codes against actual implementation
- [ ] Map exact DisconnectEvent structure offsets from binary
- [ ] Document timeout values for connection close
- [ ] Analyze reconnect behavior after disconnect

---

## Example Usage

### Complete Working Example

```c
// Full working example of disconnect callback
#include "client_api.h"

// Global state
uint32_t g_ConnectionId = 0;
uint32_t g_ReconnectAttempts = 0;
const uint32_t MAX_RECONNECT_ATTEMPTS = 3;

// Disconnect callback implementation
void MyOnDisconnect(DisconnectEvent* event, uint32_t connId, uint32_t reason) {
    printf("\n=== Connection %d Disconnected ===\n", connId);
    printf("Reason: %d\n", event->reason);
    printf("Remote: %d.%d.%d.%d:%d\n",
           (event->remoteIP >> 0) & 0xFF,
           (event->remoteIP >> 8) & 0xFF,
           (event->remoteIP >> 16) & 0xFF,
           (event->remoteIP >> 24) & 0xFF,
           event->remotePort);
    printf("Duration: %d seconds\n", event->sessionDuration);
    printf("Statistics:\n");
    printf("  Bytes sent: %d\n", event->bytesSent);
    printf("  Bytes received: %d\n", event->bytesReceived);
    printf("  Packets sent: %d\n", event->packetsSent);
    printf("  Packets received: %d\n", event->packetsReceived);

    // Handle reconnection for certain reasons
    if (reason == DISCONNECT_REASON_TIMEOUT ||
        reason == DISCONNECT_REASON_ERROR) {

        if (g_ReconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            printf("Scheduling reconnect attempt %d...\n", g_ReconnectAttempts + 1);
            ScheduleReconnect(event->remoteIP, event->remotePort, 5000); // 5 second delay
            g_ReconnectAttempts++;
        } else {
            printf("Max reconnect attempts reached\n");
            g_ReconnectAttempts = 0;
        }
    } else {
        // Reset reconnect counter on normal disconnect
        g_ReconnectAttempts = 0;
    }

    g_ConnectionId = 0;
}

// Register callback
void InitializeDisconnectHandler() {
    CallbackRegistration reg;
    reg.eventType = EVENT_DISCONNECT;
    reg.callbackFunc = MyOnDisconnect;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    int callbackId = obj->RegisterCallback2(&reg);

    printf("Registered disconnect callback (ID: %d)\n", callbackId);
}

// Cleanup on application exit
void Cleanup() {
    if (g_ConnectionId != 0) {
        APIObject* obj = g_MasterDatabase->pPrimaryObject;
        VTable* vtable = obj->pVTable;

        // Call Disconnect via vtable[5]
        typedef void (*DisconnectFunc)(APIObject*);
        DisconnectFunc disconnect = (DisconnectFunc)vtable->functions[5];
        disconnect(obj);
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
