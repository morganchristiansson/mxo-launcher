# OnTimeout

## Overview

**Category**: Network
**Direction**: Launcher → Client
**Purpose**: Connection timeout notification callback
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
void OnTimeout(TimeoutEvent* timeoutEvent, uint32_t connId, uint32_t timeoutType);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `TimeoutEvent*` | timeoutEvent | Timeout event details structure |
| `uint32_t` | connId | Connection identifier where timeout occurred |
| `uint32_t` | timeoutType | Type of timeout that occurred |

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
[ESP+4]  timeoutEvent (pointer to TimeoutEvent structure)
[ESP+8]  connId (connection identifier)
[ESP+C]  timeoutType (timeout type identifier)
```

---

## Data Structures

### TimeoutEvent Structure

```c
struct TimeoutEvent {
    uint32_t eventType;         // Offset 0x00: Event type (TIMEOUT_*)
    uint32_t connId;            // Offset 0x04: Connection identifier
    uint32_t timeoutType;       // Offset 0x08: Timeout type
    uint32_t timestamp;         // Offset 0x0C: Timeout detection timestamp

    // Timeout details
    uint32_t operation;         // Offset 0x10: Operation that timed out
    uint32_t elapsedMs;         // Offset 0x14: Elapsed time in milliseconds
    uint32_t timeoutMs;         // Offset 0x18: Configured timeout value
    uint32_t retryCount;        // Offset 0x1C: Number of retry attempts

    // Connection details
    uint32_t remoteIP;          // Offset 0x20: Remote IP address
    uint16_t remotePort;        // Offset 0x24: Remote port
    uint16_t localPort;         // Offset 0x26: Local port

    // State information
    uint32_t lastActivity;      // Offset 0x28: Timestamp of last activity
    uint32_t pendingOps;        // Offset 0x2C: Number of pending operations
    uint32_t flags;             // Offset 0x30: Timeout flags
};
```

**Size**: 52 bytes (0x34)

### ConnectionTimeoutConfig Structure

```c
struct ConnectionTimeoutConfig {
    uint32_t connectTimeout;    // Offset 0x00: Connection establishment timeout (ms)
    uint32_t sendTimeout;       // Offset 0x04: Send operation timeout (ms)
    uint32_t recvTimeout;       // Offset 0x08: Receive operation timeout (ms)
    uint32_t idleTimeout;       // Offset 0x0C: Idle connection timeout (ms)
    uint32_t keepaliveInterval; // Offset 0x10: Keepalive interval (ms)
    uint32_t keepaliveTimeout;  // Offset 0x14: Keepalive response timeout (ms)
    uint32_t handshakeTimeout;  // Offset 0x18: Protocol handshake timeout (ms)
    uint32_t sessionTimeout;    // Offset 0x1C: Session establishment timeout (ms)
};
```

**Size**: 32 bytes (0x20)

---

## Constants/Enums

### Timeout Types

| Constant | Value | Description |
|----------|-------|-------------|
| `TIMEOUT_TYPE_CONNECT` | 0x0001 | Connection establishment timeout |
| `TIMEOUT_TYPE_SEND` | 0x0002 | Send operation timeout |
| `TIMEOUT_TYPE_RECV` | 0x0003 | Receive operation timeout |
| `TIMEOUT_TYPE_IDLE` | 0x0004 | Idle connection timeout |
| `TIMEOUT_TYPE_KEEPALIVE` | 0x0005 | Keepalive response timeout |
| `TIMEOUT_TYPE_HANDSHAKE` | 0x0006 | Protocol handshake timeout |
| `TIMEOUT_TYPE_SESSION` | 0x0007 | Session establishment timeout |
| `TIMEOUT_TYPE_DNS` | 0x0008 | DNS resolution timeout |

### Default Timeout Values

| Timeout Type | Default Value | Description |
|--------------|---------------|-------------|
| `DEFAULT_CONNECT_TIMEOUT` | 30000 ms | 30 seconds for connection |
| `DEFAULT_SEND_TIMEOUT` | 10000 ms | 10 seconds for send |
| `DEFAULT_RECV_TIMEOUT` | 30000 ms | 30 seconds for receive |
| `DEFAULT_IDLE_TIMEOUT` | 300000 ms | 5 minutes idle |
| `DEFAULT_KEEPALIVE_INTERVAL` | 30000 ms | 30 seconds keepalive |
| `DEFAULT_KEEPALIVE_TIMEOUT` | 10000 ms | 10 seconds for keepalive response |
| `DEFAULT_HANDSHAKE_TIMEOUT` | 60000 ms | 60 seconds for handshake |
| `DEFAULT_SESSION_TIMEOUT` | 60000 ms | 60 seconds for session |

### Timeout Flags

| Flag | Bit | Description |
|------|-----|-------------|
| `TIMEOUT_FLAG_RECOVERABLE` | 0 | Timeout can be recovered |
| `TIMEOUT_FLAG_RETRYABLE` | 1 | Operation can be retried |
| `TIMEOUT_FLAG_KEEPALIVE_FAILED` | 2 | Keepalive probe failed |
| `TIMEOUT_FLAG_NO_RESPONSE` | 3 | No response received |
| `TIMEOUT_FLAG_NETWORK_ISSUE` | 4 | Suspected network issue |

---

## Usage

### Registration

Register timeout callback during initialization:

```c
// Register timeout callback
CallbackRegistration reg;
reg.eventType = EVENT_TIMEOUT;
reg.callbackFunc = OnTimeout;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnTimeout callback
mov eax, [timeout_callback]     ; Load callback pointer
test eax, eax                   ; Check if NULL
je skip_callback                ; Skip if no callback

; Prepare parameters
push timeoutType                ; Timeout type
push connId                     ; Connection ID
push timeoutEvent               ; Timeout event structure
call eax                        ; Invoke callback
add esp, 12                     ; Cleanup stack

skip_callback:
; Handle timeout
mov ecx, [connection_object]
; Check retry count
cmp [timeoutEvent + 0x1C], 3
jae max_retries
; Attempt retry
call RetryOperation
jmp done

max_retries:
; Close connection
call Disconnect
done:
```

### C++ Pattern

```c
// Handle timeout event
void HandleTimeout(TimeoutEvent* event, uint32_t connId, uint32_t timeoutType) {
    printf("Connection %d timeout: type=%d, elapsed=%dms, limit=%dms\n",
           connId, timeoutType, event->elapsedMs, event->timeoutMs);

    // Check if retryable
    if (event->flags & TIMEOUT_FLAG_RETRYABLE) {
        printf("Timeout is retryable\n");
        if (event->retryCount < 3) {
            RetryOperation(connId, event->operation);
            return;
        }
    }

    // Handle based on type
    switch (timeoutType) {
        case TIMEOUT_TYPE_CONNECT:
            printf("Connection establishment timed out\n");
            break;
        case TIMEOUT_TYPE_KEEPALIVE:
            printf("Keepalive response timed out\n");
            break;
        // ... other cases
    }
}
```

---

## Implementation

### Launcher Side

```c
// Trigger timeout event
void OnConnectionTimeout(ConnectionObject* conn, uint32_t timeoutType,
                          uint32_t operation, uint32_t elapsedMs) {
    // Create timeout event
    TimeoutEvent event;
    event.eventType = EVENT_TIMEOUT;
    event.connId = conn->connId;
    event.timeoutType = timeoutType;
    event.timestamp = GetTickCount();
    event.operation = operation;
    event.elapsedMs = elapsedMs;
    event.timeoutMs = GetTimeoutValue(conn, timeoutType);
    event.retryCount = conn->retryCount;
    event.remoteIP = conn->ip_address;
    event.remotePort = conn->port;
    event.localPort = conn->localPort;
    event.lastActivity = conn->lastActivity;
    event.pendingOps = conn->pendingOperations;
    event.flags = DetermineTimeoutFlags(timeoutType);

    // Update connection state
    conn->lastError = CONN_ERROR_TIMEOUT;

    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_TIMEOUT);
    if (entry && entry->callback) {
        OnTimeoutCallback callback = (OnTimeoutCallback)entry->callback;
        callback(&event, conn->connId, timeoutType);
    }

    // Log timeout
    Log("Connection timeout: conn=%d, type=%d, elapsed=%dms",
        conn->connId, timeoutType, elapsedMs);
}

// Check for timeouts (called periodically)
void CheckConnectionTimeouts() {
    uint32_t currentTime = GetTickCount();

    for (ConnectionObject* conn = g_ConnectionList; conn; conn = conn->next) {
        if (conn->connState != LTTCP_ALREADYCONNECTED) {
            continue;
        }

        // Check various timeout conditions
        uint32_t elapsed;

        // Idle timeout
        elapsed = currentTime - conn->lastActivity;
        if (elapsed > conn->config.idleTimeout) {
            OnConnectionTimeout(conn, TIMEOUT_TYPE_IDLE, OPERATION_RECV, elapsed);
            continue;
        }

        // Keepalive timeout
        if (conn->keepalivePending) {
            elapsed = currentTime - conn->keepaliveSent;
            if (elapsed > conn->config.keepaliveTimeout) {
                OnConnectionTimeout(conn, TIMEOUT_TYPE_KEEPALIVE, OPERATION_RECV, elapsed);
                conn->keepalivePending = false;
            }
        }

        // Send timeout
        if (conn->sendPending) {
            elapsed = currentTime - conn->sendStarted;
            if (elapsed > conn->config.sendTimeout) {
                OnConnectionTimeout(conn, TIMEOUT_TYPE_SEND, OPERATION_SEND, elapsed);
            }
        }

        // Receive timeout
        if (conn->recvPending) {
            elapsed = currentTime - conn->recvStarted;
            if (elapsed > conn->config.recvTimeout) {
                OnConnectionTimeout(conn, TIMEOUT_TYPE_RECV, OPERATION_RECV, elapsed);
            }
        }
    }
}

// Get timeout value for type
uint32_t GetTimeoutValue(ConnectionObject* conn, uint32_t timeoutType) {
    switch (timeoutType) {
        case TIMEOUT_TYPE_CONNECT:
            return conn->config.connectTimeout;
        case TIMEOUT_TYPE_SEND:
            return conn->config.sendTimeout;
        case TIMEOUT_TYPE_RECV:
            return conn->config.recvTimeout;
        case TIMEOUT_TYPE_IDLE:
            return conn->config.idleTimeout;
        case TIMEOUT_TYPE_KEEPALIVE:
            return conn->config.keepaliveTimeout;
        case TIMEOUT_TYPE_HANDSHAKE:
            return conn->config.handshakeTimeout;
        case TIMEOUT_TYPE_SESSION:
            return conn->config.sessionTimeout;
        default:
            return DEFAULT_CONNECT_TIMEOUT;
    }
}

// Determine timeout flags
uint32_t DetermineTimeoutFlags(uint32_t timeoutType) {
    switch (timeoutType) {
        case TIMEOUT_TYPE_CONNECT:
        case TIMEOUT_TYPE_SEND:
            return TIMEOUT_FLAG_RETRYABLE | TIMEOUT_FLAG_RECOVERABLE;

        case TIMEOUT_TYPE_RECV:
        case TIMEOUT_TYPE_KEEPALIVE:
            return TIMEOUT_FLAG_RECOVERABLE | TIMEOUT_FLAG_NETWORK_ISSUE;

        case TIMEOUT_TYPE_IDLE:
            return TIMEOUT_FLAG_RECOVERABLE;

        default:
            return TIMEOUT_FLAG_NO_RESPONSE;
    }
}

// Configure connection timeouts
void SetConnectionTimeouts(ConnectionObject* conn, ConnectionTimeoutConfig* config) {
    if (config->connectTimeout > 0) {
        conn->config.connectTimeout = config->connectTimeout;
    }
    if (config->sendTimeout > 0) {
        conn->config.sendTimeout = config->sendTimeout;
    }
    if (config->recvTimeout > 0) {
        conn->config.recvTimeout = config->recvTimeout;
    }
    if (config->idleTimeout > 0) {
        conn->config.idleTimeout = config->idleTimeout;
    }
    if (config->keepaliveInterval > 0) {
        conn->config.keepaliveInterval = config->keepaliveInterval;
    }
    if (config->keepaliveTimeout > 0) {
        conn->config.keepaliveTimeout = config->keepaliveTimeout;
    }
}
```

### Client Side

```c
// Timeout callback implementation
void MyOnTimeoutCallback(TimeoutEvent* timeoutEvent, uint32_t connId, uint32_t timeoutType) {
    printf("\n=== Connection Timeout ===\n");
    printf("Connection ID: %d\n", connId);
    printf("Timeout Type: %s\n",
           timeoutType == TIMEOUT_TYPE_CONNECT ? "Connect" :
           timeoutType == TIMEOUT_TYPE_SEND ? "Send" :
           timeoutType == TIMEOUT_TYPE_RECV ? "Receive" :
           timeoutType == TIMEOUT_TYPE_IDLE ? "Idle" :
           timeoutType == TIMEOUT_TYPE_KEEPALIVE ? "Keepalive" :
           timeoutType == TIMEOUT_TYPE_HANDSHAKE ? "Handshake" : "Unknown");
    printf("Elapsed: %d ms (limit: %d ms)\n", timeoutEvent->elapsedMs, timeoutEvent->timeoutMs);
    printf("Retry Count: %d\n", timeoutEvent->retryCount);
    printf("Last Activity: %d ms ago\n", GetTickCount() - timeoutEvent->lastActivity);
    printf("Pending Operations: %d\n", timeoutEvent->pendingOps);

    // Handle based on timeout type
    switch (timeoutType) {
        case TIMEOUT_TYPE_CONNECT:
            printf("Connection establishment timed out\n");
            if (timeoutEvent->flags & TIMEOUT_FLAG_RETRYABLE) {
                printf("Attempting to reconnect...\n");
                AttemptReconnect(connId);
            }
            break;

        case TIMEOUT_TYPE_SEND:
            printf("Send operation timed out\n");
            if (timeoutEvent->retryCount < 3) {
                printf("Retrying send...\n");
                RetrySend(connId);
            } else {
                printf("Max retries exceeded\n");
            }
            break;

        case TIMEOUT_TYPE_RECV:
            printf("Receive operation timed out\n");
            // May indicate network issue
            CheckNetworkConnectivity();
            break;

        case TIMEOUT_TYPE_IDLE:
            printf("Connection idle for too long\n");
            // May need to send keepalive or reconnect
            SendKeepalive(connId);
            break;

        case TIMEOUT_TYPE_KEEPALIVE:
            printf("Keepalive response timed out\n");
            if (timeoutEvent->flags & TIMEOUT_FLAG_NETWORK_ISSUE) {
                printf("Suspected network issue - reconnecting\n");
                Reconnect(connId);
            }
            break;

        case TIMEOUT_TYPE_HANDSHAKE:
            printf("Protocol handshake timed out\n");
            // May indicate incompatible version or server issue
            break;

        default:
            printf("Unknown timeout type: %d\n", timeoutType);
    }
}

// Register timeout callback
void RegisterTimeoutCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_TIMEOUT;
    reg.callbackFunc = MyOnTimeoutCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;

    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];

    int callbackId = regFunc(obj, &reg);
    printf("Registered timeout callback, ID=%d\n", callbackId);
}

// Configure timeout values
void ConfigureTimeouts() {
    ConnectionTimeoutConfig config;
    config.connectTimeout = 15000;      // 15 seconds
    config.sendTimeout = 5000;          // 5 seconds
    config.recvTimeout = 30000;         // 30 seconds
    config.idleTimeout = 120000;        // 2 minutes
    config.keepaliveInterval = 15000;   // 15 seconds
    config.keepaliveTimeout = 5000;     // 5 seconds
    config.handshakeTimeout = 30000;    // 30 seconds
    config.sessionTimeout = 45000;      // 45 seconds

    // Apply configuration
    // (would use a VTable function to set these)
}
```

---

## Flow/State Machine

### Timeout Detection Flow

```
1. Connection active
   └─ LTTCP_ALREADYCONNECTED state

2. Monitor elapsed time
   ├─ Track last activity timestamp
   ├─ Track pending operations
   └─ Compare against timeout values

3. Timeout detected
   ├─ elapsedMs > timeoutMs
   └─ Trigger OnTimeout callback

4. Invoke callback
   └─ Client processes timeout

5. Check retry/recovery
   ├─ Retryable? → Retry operation
   ├─ Recoverable? → Attempt recovery
   └─ Max retries? → Close connection

6. Update state
   └─ May transition to LTTCP_NOTCONNECTED
```

### Timeout State Machine

```
Normal Operation
        ↓ [Timeout detected]
Timeout Detected
        ↓
OnTimeout callback
        ↓
    ┌───────────────────────────────────┐
    │ Check TIMEOUT_FLAG_RETRYABLE      │
    └───────────────────────────────────┘
        ↓ Yes                    ↓ No
Retry Operation           Check Recoverable
        ↓                        ↓
    ┌─────────────┐          ┌─────────────┐
    │ retryCount  │          │ Check flags │
    │ < max?      │          └─────────────┘
    └─────────────┘                ↓
        ↓ Yes            ↓ Yes          ↓ No
Normal Operation    Recovery      OnDisconnect
(resumed)           Attempt       → Closed
        ↓
        ↓ No
Max Retries
        ↓
OnDisconnect
→ Connection Closed
```

### Sequence Diagram

```
[Client App]      [Launcher]        [Timer]         [Connection]
     |                 |                |                |
     |--- Operation -->|                |                |
     |                 |-- start timer->|                |
     |                 |                |                |
     |                 |                |-- tick --------|
     |                 |                |                |
     |                 |                |-- tick --------|
     |                 |                |   (check elapsed)
     |                 |                |                |
     |                 |                |-- timeout! --->|
     |                 |<--- timeout ---|                |
     |                 |                |                |
     |                 |-- Create TimeoutEvent            |
     |<--OnTimeout-----|                |                |
     |                 |                |                |
     |--- Retry? ----->|                |                |
     |                 |-- restart timer|                |
     |                 |                |                |
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| `"Connection timed out"` | - | General timeout message |
| `"Operation timed out"` | - | Generic operation timeout |
| `"Keepalive timed out"` | - | Keepalive failure |
| `"Handshake timed out"` | - | Protocol negotiation timeout |

---

## Error Codes

Related error codes (from OnConnectionError):

| Code | Constant | Description |
|------|----------|-------------|
| 0x0004 | `CONN_ERROR_TIMEOUT` | Connection timed out |
| `WSAETIMEDOUT` (10060) | Socket Error | Operation timed out |

---

## Performance Considerations

- **Timer Resolution**: Use high-resolution timers for accurate timeout detection
- **Check Frequency**: Balance between responsiveness and CPU usage
- **Keepalive Overhead**: Keepalive packets add network traffic
- **Timeout Values**: Tune based on network conditions and use case
- **Retry Limits**: Implement exponential backoff for retries

---

## Security Considerations

- **Keepalive Messages**: Don't include sensitive data in keepalive packets
- **Timeout Attack**: Malicious server could delay responses to cause timeouts
- **Resource Exhaustion**: Limit retry attempts to prevent resource exhaustion
- **Connection State**: Validate state before operations to prevent TOCTOU issues

---

## Notes

- **Important network callback** for connection health monitoring
- Works in conjunction with keepalive mechanism
- Provides detailed context for debugging timeout issues
- Supports retry/recovery mechanisms
- Should be registered early in initialization
- May trigger OnDisconnect after max retries

---

## Related Callbacks

- **[OnConnect](OnConnect.md)** - Connection established callback
- **[OnDisconnect](OnDisconnect.md)** - Connection closed callback
- **[OnConnectionError](OnConnectionError.md)** - Connection error callback
- **[OnPacket](OnPacket.md)** - Packet received/sent callback

---

## VTable Functions

Related VTable functions for timeout handling:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 1 | 0x04 | Connect | Establish connection |
| 5 | 0x14 | Disconnect | Close connection |
| 6 | 0x18 | SendPacket | Send data |
| 7 | 0x1C | ReceivePacket | Receive data |

---

## References

- **Source**: `callbacks/network/OnConnectionError.md` - Timeout error handling
- **Source**: `callbacks/network/OnDisconnect.md` - Disconnect reason codes
- **Source**: `callbacks/lifecycle/OnException.md` - Timeout exception type
- **Windows Socket**: WSAETIMEDOUT documentation

---

## Documentation Status

**Status**: ⏳ Partial
**Confidence**: Medium (inferred from timeout patterns and related callbacks)
**Last Updated**: 2025-06-17
**Documented By**: Analysis Team

---

## TODO

- [ ] Find specific diagnostic strings for timeouts in binary
- [ ] Verify timeout values against actual implementation
- [ ] Map exact TimeoutEvent structure offsets
- [ ] Document keepalive packet format
- [ ] Analyze timeout/retry interaction

---

## Example Usage

### Complete Working Example

```c
// Full working example of timeout callback
#include "client_api.h"

// Global state
uint32_t g_ConnectionId = 0;
uint32_t g_TimeoutCount = 0;

// Timeout callback implementation
void MyOnTimeout(TimeoutEvent* event, uint32_t connId, uint32_t timeoutType) {
    g_TimeoutCount++;

    printf("\n=== Timeout Event #%d ===\n", g_TimeoutCount);
    printf("Connection: %d\n", event->connId);
    printf("Type: %s\n",
           event->timeoutType == TIMEOUT_TYPE_CONNECT ? "Connect" :
           event->timeoutType == TIMEOUT_TYPE_SEND ? "Send" :
           event->timeoutType == TIMEOUT_TYPE_RECV ? "Receive" :
           event->timeoutType == TIMEOUT_TYPE_IDLE ? "Idle" :
           event->timeoutType == TIMEOUT_TYPE_KEEPALIVE ? "Keepalive" : "Other");
    printf("Elapsed: %d ms / %d ms limit\n", event->elapsedMs, event->timeoutMs);
    printf("Retries: %d\n", event->retryCount);
    printf("Flags: 0x%08X (%s%s)\n", event->flags,
           (event->flags & TIMEOUT_FLAG_RETRYABLE) ? "Retryable " : "",
           (event->flags & TIMEOUT_FLAG_RECOVERABLE) ? "Recoverable" : "");

    // Handle based on flags and retry count
    if (event->flags & TIMEOUT_FLAG_RETRYABLE) {
        if (event->retryCount < 3) {
            printf("Retrying operation...\n");
            // Exponential backoff: 1s, 2s, 4s
            uint32_t delay = 1000 << event->retryCount;
            Sleep(delay);

            switch (event->timeoutType) {
                case TIMEOUT_TYPE_CONNECT:
                    AttemptReconnect(connId);
                    break;
                case TIMEOUT_TYPE_SEND:
                    RetryLastSend(connId);
                    break;
            }
            return;
        }
    }

    if (event->flags & TIMEOUT_FLAG_RECOVERABLE) {
        printf("Attempting recovery...\n");
        SendKeepalive(connId);
        return;
    }

    printf("Timeout not recoverable - closing connection\n");
    g_ConnectionId = 0;
}

// Register callback
void InitializeTimeoutCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_TIMEOUT;
    reg.callbackFunc = MyOnTimeout;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    int callbackId = obj->RegisterCallback2(&reg);

    printf("Registered timeout callback (ID: %d)\n", callbackId);
}

// Configure aggressive timeouts for debugging
void ConfigureDebugTimeouts() {
    ConnectionTimeoutConfig config;
    config.connectTimeout = 5000;       // 5 seconds (short for testing)
    config.sendTimeout = 2000;          // 2 seconds
    config.recvTimeout = 10000;         // 10 seconds
    config.idleTimeout = 30000;         // 30 seconds
    config.keepaliveInterval = 10000;   // 10 seconds
    config.keepaliveTimeout = 3000;     // 3 seconds
    config.handshakeTimeout = 15000;    // 15 seconds
    config.sessionTimeout = 20000;      // 20 seconds

    // Apply configuration via API
    // SetTimeoutConfig(&config);
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-17 | 1.0 | Initial documentation |

---

**End of Document**
