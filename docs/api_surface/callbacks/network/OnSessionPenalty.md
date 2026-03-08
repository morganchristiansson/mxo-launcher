# OnSessionPenalty

## Overview

**Category**: Network
**Direction**: Bidirectional (Request and Response)
**Purpose**: Callback for handling transaction session penalties - used to apply rate limiting, anti-spam, or behavioral penalties to client sessions
**Message Types**: 
- `AS_SetTransSessionPenaltyRequest` (0x0003) - Client→Server
- `AS_PSSetTransSessionPenaltyRequest` (0x0004) - Client→Server (PlayStation)
- `AS_PSSetTransSessionPenaltyReply` (0x0005) - Server→Client (PlayStation)
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnSessionPenalty(SessionPenaltyEvent* penaltyEvent, void* eventData, uint32_t eventSize);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `SessionPenaltyEvent*` | penaltyEvent | Penalty event metadata and details |
| `void*` | eventData | Penalty request/response data |
| `uint32_t` | eventSize | Size of event data |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Penalty processed successfully |
| `int` | -1 | Processing failed or penalty rejected |
| `int` | 1 | Penalty acknowledged (stop further processing) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  SessionPenaltyEvent* penaltyEvent
[ESP+8]  void* eventData
[ESP+12] uint32_t eventSize

Registers:
EAX = return value
```

---

## Data Structures

### SessionPenaltyEvent Structure

```c
struct SessionPenaltyEvent {
    uint32_t eventType;         // Event type (REQUEST or REPLY)
    uint32_t connId;            // Connection identifier
    uint32_t direction;         // 0 = request, 1 = reply

    // Event metadata
    uint16_t messageType;       // 0x0003, 0x0004, or 0x0005
    uint16_t flags;             // Penalty flags
    uint32_t sequenceId;        // Sequence number
    uint32_t timestamp;         // Event timestamp

    // Penalty details
    uint32_t sessionId;         // Target session ID
    uint32_t penaltyType;       // Type of penalty
    uint32_t penaltyDuration;   // Duration in seconds
    uint32_t penaltyReason;     // Reason code

    // Request/Response specific
    uint32_t penaltyValue;      // Penalty severity/value
    uint32_t penaltyFlags;      // Additional penalty flags
    uint32_t transactionId;     // Transaction identifier
    uint32_t resultCode;        // Result code (for replies)

    // Additional data
    uint32_t reserved1;
    uint32_t reserved2;
};
```

**Size**: 56 bytes

### AS_SetTransSessionPenaltyRequest_Packet Structure

```c
struct AS_SetTransSessionPenaltyRequest_Packet {
    uint16_t messageType;       // 0x0003 (AS_SetTransSessionPenaltyRequest)
    uint16_t payloadSize;       // Payload size
    uint32_t sequenceId;        // Sequence number
    uint32_t flags;             // Request flags
    uint32_t timestamp;         // Request timestamp
    uint32_t checksum;          // Packet checksum

    // Payload
    uint32_t sessionId;         // Session to apply penalty to
    uint32_t penaltyType;       // Type of penalty
    uint32_t penaltyDuration;   // Duration in seconds
    uint32_t penaltyReason;     // Reason code
    uint32_t penaltyValue;      // Penalty severity
    uint32_t penaltyFlags;      // Additional flags
    uint32_t transactionId;     // Transaction ID
};
```

**Size**: 44 bytes (20-byte header + 24-byte payload)

### AS_PSSetTransSessionPenaltyRequest_Packet Structure

```c
struct AS_PSSetTransSessionPenaltyRequest_Packet {
    uint16_t messageType;       // 0x0004 (AS_PSSetTransSessionPenaltyRequest)
    uint16_t payloadSize;       // Payload size
    uint32_t sequenceId;        // Sequence number
    uint32_t flags;             // PlayStation-specific flags
    uint32_t timestamp;         // Request timestamp
    uint32_t checksum;          // Packet checksum

    // PlayStation-specific payload
    uint32_t sessionId;         // Session ID
    uint32_t penaltyType;       // Penalty type
    uint32_t penaltyDuration;   // Duration in seconds
    uint32_t penaltyReason;     // Reason code
    uint64_t psnId;             // PlayStation Network ID
    uint32_t transactionId;     // Transaction ID
};
```

**Size**: 48 bytes (20-byte header + 28-byte payload)

### AS_PSSetTransSessionPenaltyReply_Packet Structure

```c
struct AS_PSSetTransSessionPenaltyReply_Packet {
    uint16_t messageType;       // 0x0005 (AS_PSSetTransSessionPenaltyReply)
    uint16_t payloadSize;       // Payload size
    uint32_t sequenceId;        // Sequence number (matches request)
    uint32_t flags;             // Reply flags
    uint32_t timestamp;         // Reply timestamp
    uint32_t checksum;          // Packet checksum

    // Payload
    uint32_t sessionId;         // Session ID
    uint32_t resultCode;        // Result code
    uint32_t penaltyApplied;    // Whether penalty was applied
    uint32_t transactionId;     // Transaction ID (matches request)
};
```

**Size**: 36 bytes (20-byte header + 16-byte payload)

---

## Constants/Enums

### Penalty Types

| Constant | Value | Description |
|----------|-------|-------------|
| `PENALTY_TYPE_RATE_LIMIT` | 0x0001 | Rate limiting penalty |
| `PENALTY_TYPE_SPAM` | 0x0002 | Anti-spam penalty |
| `PENALTY_TYPE_ABUSE` | 0x0003 | Behavioral abuse penalty |
| `PENALTY_TYPE_EXPLOIT` | 0x0004 | Exploit/hack penalty |
| `PENALTY_TYPE_TEMP_BAN` | 0x0005 | Temporary ban |
| `PENALTY_TYPE_PERMA_BAN` | 0x0006 | Permanent ban |
| `PENALTY_TYPE_RESTRICTED` | 0x0007 | Restricted access |

### Penalty Reasons

| Constant | Value | Description |
|----------|-------|-------------|
| `PENALTY_REASON_SPAM` | 0x0001 | Spamming behavior |
| `PENALTY_REASON_HARASSMENT` | 0x0002 | Harassment |
| `PENALTY_REASON_CHEATING` | 0x0003 | Cheating/hacking |
| `PENALTY_REASON_EXPLOIT` | 0x0004 | Using exploits |
| `PENALTY_REASON_RMT` | 0x0005 | Real money trading |
| `PENALTY_REASON_TOXIC` | 0x0006 | Toxic behavior |
| `PENALTY_REASON_CUSTOM` | 0x0007 | Custom/admin reason |

### Penalty Flags

| Flag | Bit Mask | Description |
|------|----------|-------------|
| `PENALTY_FLAG_IMMEDIATE` | 0x0001 | Apply immediately |
| `PENALTY_FLAG_CUMULATIVE` | 0x0002 | Stack with existing penalties |
| `PENALTY_FLAG_WARN_ONLY` | 0x0004 | Warning only (no actual penalty) |
| `PENALTY_FLAG_APPEALABLE` | 0x0008 | Can be appealed |
| `PENALTY_FLAG_PERMANENT` | 0x0010 | Permanent penalty |
| `PENALTY_FLAG_PLATFORM` | 0x0020 | Platform-specific (PS/Xbox) |

### Result Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `PENALTY_RESULT_SUCCESS` | 0 | Penalty applied successfully |
| `PENALTY_RESULT_FAILED` | 1 | Failed to apply penalty |
| `PENALTY_RESULT_EXISTS` | 2 | Penalty already exists |
| `PENALTY_RESULT_INVALID` | 3 | Invalid penalty parameters |
| `PENALTY_RESULT_UNAUTHORIZED` | 4 | Not authorized to apply penalty |
| `PENALTY_RESULT_CANCELLED` | 5 | Penalty cancelled |

### Message Types

| Constant | Value | Description |
|----------|-------|-------------|
| `AS_SetTransSessionPenaltyRequest` | 0x0003 | Standard penalty request |
| `AS_PSSetTransSessionPenaltyRequest` | 0x0004 | PlayStation penalty request |
| `AS_PSSetTransSessionPenaltyReply` | 0x0005 | PlayStation penalty reply |

---

## Usage

### Registration

Register the OnSessionPenalty callback to monitor or handle session penalties:

```c
// Method 1: Using RegisterCallback2
CallbackRegistration reg;
reg.eventType = EVENT_SESSION_PENALTY;
reg.callbackFunc = MyOnSessionPenalty;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Apply session penalty
mov ax, 0x0003               ; AS_SetTransSessionPenaltyRequest
mov [packet + 0], ax         ; Store message type

; Fill penalty details
mov eax, [session_id]
mov [packet + 20], eax       ; sessionId
mov eax, [penalty_type]
mov [packet + 24], eax       ; penaltyType
mov eax, [duration]
mov [packet + 28], eax       ; penaltyDuration
mov eax, [reason]
mov [packet + 32], eax       ; penaltyReason

; Check if callback registered
mov eax, [object + penalty_callback_offset]
test eax, eax
je send_penalty

; Invoke callback
push eventSize               ; [ESP+12]
push eventData               ; [ESP+8]
push penaltyEvent            ; [ESP+4]
call eax                     ; Call OnSessionPenalty
add esp, 12

test eax, eax
js cancel_penalty            ; Negative = cancel
jz send_penalty              ; Zero = continue

send_penalty:
; Send packet to authentication server
push packet
call SendPacket

cancel_penalty:
```

### C++ Pattern

```c
// Client-side penalty monitoring
int MyOnSessionPenalty(SessionPenaltyEvent* event, void* data, uint32_t size) {
    // Check direction
    if (event->direction == 0) {
        // Outbound request
        printf("[PENALTY] Requesting penalty for session %u: type=%u, duration=%us\n",
               event->sessionId, event->penaltyType, event->penaltyDuration);

        // Can modify or cancel penalty request
        if (event->penaltyDuration > 86400) {  // More than 1 day
            printf("[PENALTY] Duration too long, rejecting\n");
            return -1;  // Cancel request
        }
    } else {
        // Inbound reply (PlayStation)
        printf("[PENALTY] Reply received: session=%u, result=%u\n",
               event->sessionId, event->resultCode);

        if (event->resultCode == PENALTY_RESULT_SUCCESS) {
            printf("[PENALTY] Penalty applied successfully\n");
        } else {
            printf("[PENALTY] Failed to apply penalty: %u\n", event->resultCode);
        }
    }

    return 0;  // Continue processing
}

// Register callback
void RegisterPenaltyCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_SESSION_PENALTY;
    reg.callbackFunc = MyOnSessionPenalty;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered penalty callback, ID=%d\n", id);
}
```

---

## Implementation

### Launcher Side (Request)

```c
// Launcher: Request session penalty
int RequestSessionPenalty(ConnectionObject* conn,
                         uint32_t sessionId,
                         uint32_t penaltyType,
                         uint32_t duration,
                         uint32_t reason) {
    if (!conn || !conn->isConnected) {
        return -1;
    }

    // Prepare request packet
    AS_SetTransSessionPenaltyRequest_Packet request;
    request.messageType = AS_SetTransSessionPenaltyRequest;
    request.payloadSize = 24;
    request.sequenceId = conn->nextSequenceId++;
    request.flags = PENALTY_FLAG_IMMEDIATE | PENALTY_FLAG_APPEALABLE;
    request.timestamp = GetCurrentTime();

    request.sessionId = sessionId;
    request.penaltyType = penaltyType;
    request.penaltyDuration = duration;
    request.penaltyReason = reason;
    request.penaltyValue = 1;  // Severity
    request.penaltyFlags = 0;
    request.transactionId = GenerateTransactionId();

    request.checksum = CalculateChecksum(&request, sizeof(request));

    // Prepare event
    SessionPenaltyEvent event;
    event.eventType = EVENT_SESSION_PENALTY;
    event.connId = conn->connId;
    event.direction = 0;  // Request
    event.messageType = AS_SetTransSessionPenaltyRequest;
    event.sequenceId = request.sequenceId;
    event.sessionId = sessionId;
    event.penaltyType = penaltyType;
    event.penaltyDuration = duration;
    event.penaltyReason = reason;
    event.transactionId = request.transactionId;

    // Invoke callback (if registered)
    if (conn->pPenaltyCallback) {
        int result = conn->pPenaltyCallback(&event, &request, sizeof(request));

        if (result < 0) {
            // Request cancelled by callback
            return -1;
        }
    }

    // Send request to authentication server
    return SendPacket(conn, &request, sizeof(request), PACKET_FLAG_RELIABLE);
}
```

### Launcher Side (Reply - PlayStation)

```c
// Launcher: Handle penalty reply
int HandlePenaltyReply(ConnectionObject* conn, void* packet, uint32_t size) {
    if (!conn || !packet || size < sizeof(AS_PSSetTransSessionPenaltyReply_Packet)) {
        return -1;
    }

    AS_PSSetTransSessionPenaltyReply_Packet* reply =
        (AS_PSSetTransSessionPenaltyReply_Packet*)packet;

    // Validate checksum
    if (reply->checksum != CalculateChecksum(packet, size)) {
        return -1;
    }

    // Prepare event
    SessionPenaltyEvent event;
    event.eventType = EVENT_SESSION_PENALTY;
    event.connId = conn->connId;
    event.direction = 1;  // Reply
    event.messageType = AS_PSSetTransSessionPenaltyReply;
    event.sequenceId = reply->sequenceId;
    event.sessionId = reply->sessionId;
    event.resultCode = reply->resultCode;
    event.transactionId = reply->transactionId;

    // Invoke callback
    if (conn->pPenaltyCallback) {
        conn->pPenaltyCallback(&event, packet, size);
    }

    // Log result
    if (reply->resultCode == PENALTY_RESULT_SUCCESS) {
        Log("Penalty applied successfully for session %u", reply->sessionId);
    } else {
        Log("Penalty failed for session %u: result %u",
            reply->sessionId, reply->resultCode);
    }

    return 0;
}
```

### Client Side (Administrator Tool)

```c
// Client: Apply penalty to player
void BanPlayer(uint32_t sessionId, uint32_t duration, const char* reason) {
    ConnectionObject* conn = GetAuthServerConnection();
    if (!conn) {
        printf("Not connected to authentication server\n");
        return;
    }

    // Request penalty
    int result = RequestSessionPenalty(
        conn,
        sessionId,
        PENALTY_TYPE_TEMP_BAN,
        duration,
        PENALTY_REASON_CUSTOM
    );

    if (result < 0) {
        printf("Failed to request penalty\n");
    } else {
        printf("Penalty request sent for session %u\n", sessionId);
    }
}

// Monitor penalty events
int MonitorPenaltyEvents(SessionPenaltyEvent* event, void* data, uint32_t size) {
    if (event->direction == 1) {
        // Reply received
        if (event->resultCode == PENALTY_RESULT_SUCCESS) {
            printf("Penalty successfully applied\n");
        } else {
            printf("Penalty application failed: %u\n", event->resultCode);
        }
    }

    return 0;
}
```

---

## Flow/State Machine

### Penalty Request Flow

```
Admin/GM Initiates Penalty
        ↓
    Invoke OnSessionPenalty Callback
        ↓
    Callback Validation
    ├─> Return -1: Cancel Request
    └─> Return 0: Continue
        ↓
    Send AS_SetTransSessionPenaltyRequest (0x0003)
        ↓
    Authentication Server Receives
        ↓
    Server Validates Request
        ↓
    Server Applies Penalty
        ↓
    Server Sends Reply (PlayStation: 0x0005)
        ↓
    Launcher Receives Reply
        ↓
    Invoke OnSessionPenalty Callback
        ↓
    Log/Notify Result
```

### Sequence Diagram

```
Admin          Client              Launcher          Auth Server
  |               |                    |                  |
  |--Ban Player-->|                    |                  |
  |               |                    |                  |
  |               |--RequestPenalty--->|                  |
  |               |                    |                  |
  |               |                    |--OnSessionPenalty|
  |               |<-------------------|                  |
  |               |                    |                  |
  |               |                    |--AS_SetTrans---->|
  |               |                    |    (0x0003)      |
  |               |                    |                  |
  |               |                    |                  |--Apply Penalty
  |               |                    |                  |
  |               |                    |<-AS_PSReply------|
  |               |                    |    (0x0005)      |
  |               |                    |                  |
  |               |                    |--OnSessionPenalty|
  |               |                    |----------------->|
  |               |                    |                  |
  |               |<---Result----------|                  |
  |               |                    |                  |
```

---

## Diagnostic Strings

Strings related to session penalties:

| String | Address | Context |
|--------|---------|---------|
| `"AS_SetTransSessionPenaltyRequest"` | - | Message type name |
| `"AS_PSSetTransSessionPenaltyRequest"` | - | PlayStation request |
| `"AS_PSSetTransSessionPenaltyReply"` | - | PlayStation reply |
| `"Penalty applied to session %u: type=%u, duration=%u"` | - | Success logging |
| `"Penalty failed for session %u: result=%u"` | - | Error logging |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SUCCESS` | Penalty applied successfully |
| -1 | `NOT_CONNECTED` | Not connected to auth server |
| -2 | `REQUEST_CANCELLED` | Request cancelled by callback |
| -3 | `UNAUTHORIZED` | Not authorized to apply penalties |
| -4 | `INVALID_SESSION` | Invalid session ID |
| -5 | `INVALID_PENALTY` | Invalid penalty parameters |

---

## Performance Considerations

- **Request Frequency**: Penalties are relatively rare; no performance concern
- **Timeout**: Default 5 seconds for reply
- **Packet Size**: Small packets (36-48 bytes)
- **Threading**: Callback may be on network thread
- **Logging**: Always log penalty events for audit trail

---

## Security Considerations

- **Authorization**: Only authorized users should apply penalties
- **Validation**: Validate all penalty parameters
- **Audit Trail**: Log all penalty requests and results
- **Appeals**: Support penalty appeals system
- **Rate Limiting**: Prevent abuse of penalty system
- **Privacy**: Handle user data according to privacy policy

---

## Notes

- **Purpose**: Used by administrators/GMs to penalize players for violations
- **Direction**: Bidirectional (request and reply)
- **PlayStation**: Has special message types for PSN integration
- **Persistence**: Penalties are stored in database
- **Appeals**: Can be flagged as appealable
- **Common Pitfalls**:
  - Not validating authorization
  - Not logging penalty events
  - Applying penalties without evidence
  - Not handling reply timeouts
- **Best Practices**:
  - Always validate authorization
  - Log all penalty events with reason
  - Support appeals process
  - Use appropriate penalty severity
  - Provide clear reason codes

---

## Related Callbacks

- **[OnTransSession](network/OnTransSession.md)** - Transaction session callback
- **[OnPacket](network/OnPacket.md)** - General packet callback
- **[OnConnect](network/OnConnect.md)** - Authentication server connection
- **[OnDisconnect](network/OnDisconnect.md)** - Connection closed

---

## VTable Functions

Related VTable functions for penalty requests:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 6 | 0x18 | SendPacket | Send penalty request |
| 7 | 0x1C | ReceivePacket | Receive penalty reply |

---

## References

- **Source**: `callbacks/network/OnPacket.md` - Message types 0x0003-0x0005
- **Source**: `extracted_strings_analysis.md` - AS_*TransSessionPenalty strings
- **Message Types**: AS_SetTransSessionPenaltyRequest, AS_PSSetTransSessionPenalty*
- **Direction**: Client ↔ Authentication Server
- **Evidence**: Network protocol documentation

---

## Documentation Status

**Status**: ✅ Complete
**Confidence**: Medium
**Last Updated**: 2025-06-18
**Documented By**: SetMasterDatabase API Analysis

---

## TODO

- [ ] Verify exact packet structures in assembly
- [ ] Find authorization validation code
- [ ] Confirm PlayStation-specific fields
- [ ] Identify transaction ID generation

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include <stdint.h>

// Penalty types
typedef enum {
    PENALTY_TYPE_RATE_LIMIT = 0x0001,
    PENALTY_TYPE_SPAM = 0x0002,
    PENALTY_TYPE_ABUSE = 0x0003,
    PENALTY_TYPE_EXPLOIT = 0x0004,
    PENALTY_TYPE_TEMP_BAN = 0x0005,
    PENALTY_TYPE_PERMA_BAN = 0x0006
} PenaltyType;

// Penalty reasons
typedef enum {
    PENALTY_REASON_SPAM = 0x0001,
    PENALTY_REASON_HARASSMENT = 0x0002,
    PENALTY_REASON_CHEATING = 0x0003,
    PENALTY_REASON_EXPLOIT = 0x0004,
    PENALTY_REASON_CUSTOM = 0x0007
} PenaltyReason;

// Event structure
typedef struct {
    uint32_t eventType;
    uint32_t connId;
    uint32_t direction;
    uint16_t messageType;
    uint16_t flags;
    uint32_t sequenceId;
    uint32_t timestamp;
    uint32_t sessionId;
    uint32_t penaltyType;
    uint32_t penaltyDuration;
    uint32_t penaltyReason;
    uint32_t penaltyValue;
    uint32_t penaltyFlags;
    uint32_t transactionId;
    uint32_t resultCode;
    uint32_t reserved1;
    uint32_t reserved2;
} SessionPenaltyEvent;

// Penalty callback
int MyOnSessionPenalty(SessionPenaltyEvent* event, void* data, uint32_t size) {
    if (event->direction == 0) {
        // Outbound request
        printf("[PENALTY REQUEST] Session=%u Type=%u Duration=%us Reason=%u\n",
               event->sessionId, event->penaltyType,
               event->penaltyDuration, event->penaltyReason);

        // Validate penalty parameters
        if (event->penaltyDuration > 86400 * 7) {  // More than 7 days
            printf("[PENALTY] Duration too long, rejecting\n");
            return -1;
        }

        if (event->penaltyType == PENALTY_TYPE_PERMA_BAN) {
            printf("[PENALTY] Permanent ban requested, requiring approval\n");
            // Could implement additional approval logic here
        }
    } else {
        // Inbound reply
        printf("[PENALTY REPLY] Session=%u Result=%u Transaction=%u\n",
               event->sessionId, event->resultCode, event->transactionId);

        switch (event->resultCode) {
            case 0:  // SUCCESS
                printf("[PENALTY] ✓ Successfully applied\n");
                break;
            case 1:  // FAILED
                printf("[PENALTY] ✗ Failed to apply\n");
                break;
            case 4:  // UNAUTHORIZED
                printf("[PENALTY] ✗ Not authorized\n");
                break;
            default:
                printf("[PENALTY] ✗ Unknown result: %u\n", event->resultCode);
        }
    }

    return 0;  // Continue processing
}

// Apply temporary ban
void ApplyTemporaryBan(uint32_t sessionId, uint32_t durationSeconds) {
    ConnectionObject* conn = GetAuthServerConnection();
    if (!conn) {
        printf("Error: Not connected to authentication server\n");
        return;
    }

    printf("Applying %u second ban to session %u\n", durationSeconds, sessionId);

    // Request penalty
    int result = RequestSessionPenalty(
        conn,
        sessionId,
        PENALTY_TYPE_TEMP_BAN,
        durationSeconds,
        PENALTY_REASON_CUSTOM
    );

    if (result < 0) {
        printf("Failed to request penalty\n");
    }
}

// Register callback
void InitializePenaltySystem() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_SESSION_PENALTY;
    reg.callbackFunc = MyOnSessionPenalty;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered penalty callback, ID=%d\n", id);
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |

---

**End of Document**
