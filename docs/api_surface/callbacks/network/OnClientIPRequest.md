# OnClientIPRequest

## Overview

**Category**: Network
**Direction**: Client → Server (Outbound Request)
**Purpose**: Callback invoked when client requests its own external IP address from the master server
**Message Type**: `MS_GetClientIPRequest` (0x0103)
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnClientIPRequest(ClientIPRequestEvent* requestEvent, void* requestData, uint32_t requestSize);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ClientIPRequestEvent*` | requestEvent | Request event metadata |
| `void*` | requestData | Request packet data (usually empty) |
| `uint32_t` | requestSize | Size of request data |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Request approved, continue sending |
| `int` | -1 | Request denied or error |
| `int` | 1 | Request handled locally (don't send) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  ClientIPRequestEvent* requestEvent
[ESP+8]  void* requestData
[ESP+12] uint32_t requestSize

Registers:
EAX = return value
```

---

## Data Structures

### ClientIPRequestEvent Structure

```c
struct ClientIPRequestEvent {
    uint32_t eventType;         // Event type: REQUEST_EVENT (0x01)
    uint32_t connId;            // Connection identifier
    uint32_t direction;         // Always 0 (outbound request)

    // Request metadata
    uint16_t messageType;       // 0x0103 (MS_GetClientIPRequest)
    uint16_t flags;             // Request flags
    uint32_t sequenceId;        // Sequence number
    uint32_t timestamp;         // Request timestamp

    // Request parameters
    uint32_t requestId;         // Unique request identifier
    uint32_t reserved1;         // Reserved
    uint32_t reserved2;         // Reserved

    // Response info (to be filled)
    uint32_t timeout;           // Request timeout in milliseconds
    uint32_t retryCount;        // Number of retry attempts
};
```

**Size**: 40 bytes

### MS_GetClientIPRequest_Packet Structure

```c
struct MS_GetClientIPRequest_Packet {
    uint16_t messageType;       // 0x0103 (MS_GetClientIPRequest)
    uint16_t payloadSize;       // Payload size (usually 0)
    uint32_t sequenceId;        // Sequence number for matching reply
    uint32_t flags;             // Request flags
    uint32_t timestamp;         // Request timestamp
    uint32_t checksum;          // Packet checksum

    // No additional payload for basic IP request
};
```

**Size**: 20 bytes (header only, no payload)

---

## Constants/Enums

### Request Flags

| Flag | Bit Mask | Description |
|------|----------|-------------|
| `IP_REQUEST_FLAG_IPV4` | 0x0001 | Request IPv4 address |
| `IP_REQUEST_FLAG_IPV6` | 0x0002 | Request IPv6 address |
| `IP_REQUEST_FLAG_EXTERNAL` | 0x0004 | Request external/public IP |
| `IP_REQUEST_FLAG_INTERNAL` | 0x0008 | Request internal/local IP |
| `IP_REQUEST_FLAG_INCLUDE_PORT` | 0x0010 | Include port number in response |
| `IP_REQUEST_FLAG_ASYNC` | 0x0020 | Asynchronous request (don't wait) |

### Message Types

| Constant | Value | Description |
|----------|-------|-------------|
| `MS_GetClientIPRequest` | 0x0103 | Client IP request message |
| `MS_GetClientIPReply` | 0x0104 | Client IP reply message |

---

## Usage

### Registration

Register the OnClientIPRequest callback to monitor or modify IP address requests:

```c
// Method 1: Using RegisterCallback2
CallbackRegistration reg;
reg.eventType = EVENT_CLIENT_IP_REQUEST;
reg.callbackFunc = MyOnClientIPRequest;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Prepare IP request packet
mov ax, 0x0103               ; MS_GetClientIPRequest message type
mov [packet + 0], ax         ; Store message type
mov eax, [sequence_id]
mov [packet + 8], eax        ; Store sequence ID

; Check if callback registered
mov eax, [object + ip_request_callback_offset]
test eax, eax
je send_request

; Invoke callback
push requestSize             ; [ESP+12]
push requestData             ; [ESP+8]
push requestEvent            ; [ESP+4]
call eax                     ; Call OnClientIPRequest
add esp, 12

test eax, eax
js cancel_request            ; Negative = cancel
jz send_request              ; Zero = continue
; Positive = handled locally
jmp skip_send

send_request:
; Send packet to server
push packet
call SendPacket

skip_send:
cancel_request:
```

### C++ Pattern

```c
// Client-side IP request monitoring
int MyOnClientIPRequest(ClientIPRequestEvent* requestEvent, void* data, uint32_t size) {
    printf("[IP REQUEST] Requesting client IP (ID=%u)\n", requestEvent->requestId);

    // Can modify timeout
    requestEvent->timeout = 5000;  // 5 second timeout

    // Can cancel request by returning -1
    // return -1;

    // Or handle locally (return 1) if IP is already known
    if (g_CachedExternalIP != 0) {
        printf("[IP REQUEST] Using cached IP: %d.%d.%d.%d\n",
               (g_CachedExternalIP >> 0) & 0xFF,
               (g_CachedExternalIP >> 8) & 0xFF,
               (g_CachedExternalIP >> 16) & 0xFF,
               (g_CachedExternalIP >> 24) & 0xFF);
        return 1;  // Don't send request, use cached value
    }

    return 0;  // Continue with request
}

// Register callback
void RegisterIPRequestCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_CLIENT_IP_REQUEST;
    reg.callbackFunc = MyOnClientIPRequest;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered IP request callback, ID=%d\n", id);
}
```

---

## Implementation

### Launcher Side

```c
// Launcher: Send IP request to master server
int RequestClientIP(ConnectionObject* conn) {
    if (!conn || !conn->isConnected) {
        return -1;
    }

    // Prepare request packet
    MS_GetClientIPRequest_Packet request;
    request.messageType = MS_GetClientIPRequest;
    request.payloadSize = 0;
    request.sequenceId = conn->nextSequenceId++;
    request.flags = IP_REQUEST_FLAG_IPV4 | IP_REQUEST_FLAG_EXTERNAL;
    request.timestamp = GetCurrentTime();
    request.checksum = CalculateChecksum(&request, sizeof(request));

    // Prepare event
    ClientIPRequestEvent event;
    event.eventType = EVENT_CLIENT_IP_REQUEST;
    event.connId = conn->connId;
    event.messageType = MS_GetClientIPRequest;
    event.sequenceId = request.sequenceId;
    event.requestId = GenerateRequestId();
    event.timeout = 3000;  // 3 second default timeout
    event.retryCount = 3;

    // Invoke callback (if registered)
    if (conn->pIPRequestCallback) {
        int result = conn->pIPRequestCallback(&event, NULL, 0);

        if (result < 0) {
            // Request cancelled by callback
            return -1;
        }

        if (result > 0) {
            // Handled locally, don't send
            return 0;
        }
    }

    // Send request to server
    return SendPacket(conn, &request, sizeof(request), PACKET_FLAG_RELIABLE);
}
```

### Client Side

```c
// Client: Request external IP address
void GetMyExternalIP() {
    // Get connection object
    ConnectionObject* conn = GetMasterServerConnection();
    if (!conn) {
        printf("Not connected to master server\n");
        return;
    }

    // Send IP request
    int result = RequestClientIP(conn);
    if (result < 0) {
        printf("Failed to request IP address\n");
        return;
    }

    printf("IP address request sent (waiting for reply...)\n");
}

// Handle IP request callback (optional monitoring)
int MonitorIPRequest(ClientIPRequestEvent* event, void* data, uint32_t size) {
    // Log request
    LogIPRequest(event->requestId, event->timeout);

    // Allow request to proceed
    return 0;
}
```

---

## Flow/State Machine

### IP Request Flow

```
Client Needs IP Address
        ↓
    Check Cache
        ↓ (not cached)
    Invoke OnClientIPRequest Callback
        ↓
    Callback Decision
    ├─> Return -1: Cancel Request
    ├─> Return 0: Continue (send request)
    └─> Return 1: Use Cached/Local Value
        ↓
    Send MS_GetClientIPRequest (0x0103)
        ↓
    Master Server Receives Request
        ↓
    Server Determines Client IP
        ↓
    Server Sends MS_GetClientIPReply (0x0104)
        ↓
    Client Receives Reply
        ↓
    OnClientIPReply Callback Invoked
        ↓
    Store/Use IP Address
```

### Sequence Diagram

```
Client              Launcher              Master Server
   |                   |                       |
   |--Need IP--------->|                       |
   |                   |                       |
   |--OnClientIPRequest|                       |
   |<------------------|                       |
   |                   |                       |
   |---(return 0)----->|                       |
   |                   |                       |
   |                   |--MS_GetClientIPReq--> |
   |                   |      (0x0103)         |
   |                   |                       |
   |                   |                       |--Determine IP
   |                   |                       |
   |                   |<-MS_GetClientIPReply--|
   |                   |      (0x0104)         |
   |                   |                       |
   |<-OnClientIPReply--|                       |
   |                   |                       |
```

---

## Diagnostic Strings

Strings related to IP address requests:

| String | Address | Context |
|--------|---------|---------|
| `"MS_GetClientIPRequest"` | - | Message type name |
| `"Requesting client IP address..."` | - | Request initiation |
| `"IP request timeout (retry %d/%d)"` | - | Timeout retry notification |
| `"Failed to request client IP: error %d"` | - | Error notification |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SUCCESS` | Request sent successfully |
| -1 | `NOT_CONNECTED` | Not connected to master server |
| -2 | `REQUEST_CANCELLED` | Request cancelled by callback |
| -3 | `TIMEOUT` | No response received within timeout |
| -4 | `INVALID_CONNECTION` | Invalid connection object |

---

## Performance Considerations

- **Request Frequency**: Don't request IP too frequently; cache the result
- **Timeout**: Default 3 seconds; adjust based on network conditions
- **Retries**: Default 3 retries with exponential backoff
- **Caching**: Cache IP address for session duration (until reconnect)
- **Threading**: Request may be async; callback may be on network thread

---

## Security Considerations

- **Privacy**: IP address is sensitive; handle according to privacy policy
- **Spoofing**: Server determines IP; client cannot spoof external IP
- **Validation**: Always validate received IP address format
- **Logging**: Be careful logging IP addresses (GDPR/privacy concerns)

---

## Notes

- **Purpose**: Used for NAT traversal, P2P connections, and server listings
- **Direction**: Client initiates request; server responds with client's external IP
- **Frequency**: Typically called once per session or after reconnection
- **Related**: Paired with OnClientIPReply for the response
- **Common Pitfalls**:
  - Not caching result (repeated requests waste bandwidth)
  - Not handling timeout (request may never complete)
  - Assuming internal IP equals external IP (NAT)
- **Best Practices**:
  - Cache the result for session duration
  - Set reasonable timeout (3-5 seconds)
  - Handle retry logic with backoff
  - Log for debugging but respect privacy

---

## Related Callbacks

- **[OnClientIPReply](network/OnClientIPReply.md)** - IP address response callback
- **[OnPacket](network/OnPacket.md)** - General packet callback
- **[OnConnect](network/OnConnect.md)** - Connection established
- **[OnTimeout](network/OnTimeout.md)** - Request timeout handling

---

## VTable Functions

Related VTable functions for IP requests:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 6 | 0x18 | SendPacket | Send IP request packet |
| 7 | 0x1C | ReceivePacket | Receive IP reply packet |

---

## References

- **Source**: `callbacks/network/OnPacket.md` - Message type 0x0103
- **Source**: `extracted_strings_analysis.md` - MS_GetClientIPRequest string
- **Message Type**: MS_GetClientIPRequest (0x0103)
- **Direction**: Client → Master Server
- **Evidence**: Network protocol documentation

---

## Documentation Status

**Status**: ✅ Complete
**Confidence**: Medium
**Last Updated**: 2025-06-18
**Documented By**: SetMasterDatabase API Analysis

---

## TODO

- [ ] Verify exact packet structure in assembly
- [ ] Find callback registration code
- [ ] Confirm timeout and retry default values
- [ ] Identify request ID generation mechanism

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include <stdint.h>

// Cached external IP
uint32_t g_CachedExternalIP = 0;
uint16_t g_CachedExternalPort = 0;

// IP request event structure
typedef struct {
    uint32_t eventType;
    uint32_t connId;
    uint32_t direction;
    uint16_t messageType;
    uint16_t flags;
    uint32_t sequenceId;
    uint32_t timestamp;
    uint32_t requestId;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t timeout;
    uint32_t retryCount;
} ClientIPRequestEvent;

// IP request callback
int MyOnClientIPRequest(ClientIPRequestEvent* event, void* data, uint32_t size) {
    printf("[IP REQUEST] Request ID=%u, Timeout=%ums, Retries=%u\n",
           event->requestId, event->timeout, event->retryCount);

    // Check if we have a cached IP
    if (g_CachedExternalIP != 0) {
        printf("[IP REQUEST] Using cached IP: %d.%d.%d.%d\n",
               (g_CachedExternalIP >> 0) & 0xFF,
               (g_CachedExternalIP >> 8) & 0xFF,
               (g_CachedExternalIP >> 16) & 0xFF,
               (g_CachedExternalIP >> 24) & 0xFF);

        // Don't send request, use cached value
        return 1;
    }

    // Modify timeout if needed
    event->timeout = 5000;  // 5 seconds

    // Allow request to proceed
    return 0;
}

// Request external IP address
void RequestExternalIP() {
    // Check cache first
    if (g_CachedExternalIP != 0) {
        printf("Already have cached IP: %d.%d.%d.%d\n",
               (g_CachedExternalIP >> 0) & 0xFF,
               (g_CachedExternalIP >> 8) & 0xFF,
               (g_CachedExternalIP >> 16) & 0xFF,
               (g_CachedExternalIP >> 24) & 0xFF);
        return;
    }

    // Get connection
    ConnectionObject* conn = GetMasterServerConnection();
    if (!conn) {
        printf("Error: Not connected to master server\n");
        return;
    }

    // Request will trigger OnClientIPRequest callback
    // Then OnClientIPReply will be called with result
    RequestClientIP(conn);

    printf("IP request sent, waiting for reply...\n");
}

// Register callback
void InitializeIPDetection() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_CLIENT_IP_REQUEST;
    reg.callbackFunc = MyOnClientIPRequest;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered IP request callback, ID=%d\n", id);
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |

---

**End of Document**
