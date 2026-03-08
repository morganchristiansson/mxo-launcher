# OnClientIPReply

## Overview

**Category**: Network
**Direction**: Server → Client (Inbound Response)
**Purpose**: Callback invoked when master server responds with client's external IP address
**Message Type**: `MS_GetClientIPReply` (0x0104)
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnClientIPReply(ClientIPReplyEvent* replyEvent, void* replyData, uint32_t replySize);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ClientIPReplyEvent*` | replyEvent | Reply event metadata and IP address |
| `void*` | replyData | Pointer to reply packet data |
| `uint32_t` | replySize | Size of reply packet data |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Reply processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Reply consumed (stop further processing) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  ClientIPReplyEvent* replyEvent
[ESP+8]  void* replyData
[ESP+12] uint32_t replySize

Registers:
EAX = return value
```

---

## Data Structures

### ClientIPReplyEvent Structure

```c
struct ClientIPReplyEvent {
    uint32_t eventType;         // Event type: REPLY_EVENT (0x02)
    uint32_t connId;            // Connection identifier
    uint32_t direction;         // Always 1 (inbound reply)

    // Reply metadata
    uint16_t messageType;       // 0x0104 (MS_GetClientIPReply)
    uint16_t flags;             // Reply flags
    uint32_t sequenceId;        // Sequence number (matches request)
    uint32_t timestamp;         // Reply timestamp

    // IP address information
    uint32_t externalIP;        // External/public IP address
    uint16_t externalPort;      // External port number
    uint16_t ipVersion;         // IP version (4 or 6)

    // Internal IP (if NAT detected)
    uint32_t internalIP;        // Internal/local IP address
    uint16_t internalPort;      // Internal port number
    uint16_t natType;           // NAT type detected

    // Additional info
    uint32_t requestId;         // Matches request ID
    uint32_t responseTime;      // Response time in milliseconds
};
```

**Size**: 52 bytes

### MS_GetClientIPReply_Packet Structure

```c
struct MS_GetClientIPReply_Packet {
    uint16_t messageType;       // 0x0104 (MS_GetClientIPReply)
    uint16_t payloadSize;       // Payload size
    uint32_t sequenceId;        // Sequence number (matches request)
    uint32_t flags;             // Reply flags
    uint32_t timestamp;         // Reply timestamp
    uint32_t checksum;          // Packet checksum

    // Payload
    uint32_t externalIP;        // External IP address (network byte order)
    uint16_t externalPort;      // External port (network byte order)
    uint16_t ipVersion;         // IP version (4 = IPv4, 6 = IPv6)

    uint32_t internalIP;        // Internal IP address
    uint16_t internalPort;      // Internal port
    uint16_t natType;           // NAT type
};
```

**Size**: 38 bytes (20-byte header + 18-byte payload)

### NAT Type Enumeration

```c
enum NATType {
    NAT_NONE = 0,               // No NAT (direct connection)
    NAT_FULL_CONE = 1,          // Full cone NAT
    NAT_RESTRICTED_CONE = 2,    // Restricted cone NAT
    NAT_PORT_RESTRICTED = 3,    // Port restricted cone NAT
    NAT_SYMMETRIC = 4,          // Symmetric NAT
    NAT_UNKNOWN = 5             // NAT type unknown
};
```

---

## Constants/Enums

### Reply Flags

| Flag | Bit Mask | Description |
|------|----------|-------------|
| `IP_REPLY_FLAG_SUCCESS` | 0x0001 | IP address retrieved successfully |
| `IP_REPLY_FLAG_ERROR` | 0x0002 | Error occurred |
| `IP_REPLY_FLAG_NAT_DETECTED` | 0x0004 | NAT detected between client and server |
| `IP_REPLY_FLAG_IPV6` | 0x0008 | IPv6 address provided |
| `IP_REPLY_FLAG_PORT_MAPPED` | 0x0010 | Port mapping detected |

### IP Version

| Constant | Value | Description |
|----------|-------|-------------|
| `IP_VERSION_4` | 4 | IPv4 address |
| `IP_VERSION_6` | 6 | IPv6 address |

### Message Types

| Constant | Value | Description |
|----------|-------|-------------|
| `MS_GetClientIPRequest` | 0x0103 | Client IP request |
| `MS_GetClientIPReply` | 0x0104 | Client IP reply |

---

## Usage

### Registration

Register the OnClientIPReply callback to handle IP address responses:

```c
// Method 1: Using RegisterCallback2
CallbackRegistration reg;
reg.eventType = EVENT_CLIENT_IP_REPLY;
reg.callbackFunc = MyOnClientIPReply;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Receive IP reply packet
mov ax, 0x0104               ; MS_GetClientIPReply message type
cmp [packet + 0], ax         ; Check message type
jne not_ip_reply

; Parse IP address from packet
mov eax, [packet + 20]       ; External IP at offset 20
mov ebx, [packet + 24]       ; External port and version
mov ecx, [packet + 28]       ; Internal IP

; Store in event structure
mov [replyEvent + 24], eax   ; externalIP
mov [replyEvent + 28], bx    ; externalPort
mov [replyEvent + 30], 4     ; ipVersion = IPv4
mov [replyEvent + 32], ecx   ; internalIP

; Check if callback registered
mov eax, [object + ip_reply_callback_offset]
test eax, eax
je skip_callback

; Invoke callback
push replySize               ; [ESP+12]
push replyData               ; [ESP+8]
push replyEvent              ; [ESP+4]
call eax                     ; Call OnClientIPReply
add esp, 12

test eax, eax
js handle_error              ; Negative = error

skip_callback:
; Store cached IP
mov eax, [replyEvent + 24]   ; externalIP
mov [cached_ip], eax
```

### C++ Pattern

```c
// Client-side IP reply handler
int MyOnClientIPReply(ClientIPReplyEvent* replyEvent, void* data, uint32_t size) {
    // Check if successful
    if (replyEvent->flags & IP_REPLY_FLAG_ERROR) {
        printf("[IP REPLY] Error retrieving IP address\n");
        return -1;
    }

    // Extract IP address
    uint32_t ip = replyEvent->externalIP;
    uint16_t port = replyEvent->externalPort;

    // Convert to string format
    printf("[IP REPLY] External IP: %d.%d.%d.%d:%d\n",
           (ip >> 0) & 0xFF,
           (ip >> 8) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 24) & 0xFF,
           port);

    // Check for NAT
    if (replyEvent->flags & IP_REPLY_FLAG_NAT_DETECTED) {
        printf("[IP REPLY] NAT detected (type %d)\n", replyEvent->natType);

        uint32_t internalIP = replyEvent->internalIP;
        printf("[IP REPLY] Internal IP: %d.%d.%d.%d:%d\n",
               (internalIP >> 0) & 0xFF,
               (internalIP >> 8) & 0xFF,
               (internalIP >> 16) & 0xFF,
               (internalIP >> 24) & 0xFF,
               replyEvent->internalPort);
    }

    // Cache the result
    g_CachedExternalIP = ip;
    g_CachedExternalPort = port;

    // Store response time for statistics
    g_LastIPRequestTime = replyEvent->responseTime;

    return 0;  // Continue processing
}

// Register callback
void RegisterIPReplyCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_CLIENT_IP_REPLY;
    reg.callbackFunc = MyOnClientIPReply;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered IP reply callback, ID=%d\n", id);
}
```

---

## Implementation

### Launcher Side

```c
// Launcher: Handle IP reply packet from master server
int HandleClientIPReply(ConnectionObject* conn, void* packet, uint32_t size) {
    if (!conn || !packet || size < sizeof(MS_GetClientIPReply_Packet)) {
        return -1;
    }

    MS_GetClientIPReply_Packet* reply = (MS_GetClientIPReply_Packet*)packet;

    // Validate checksum
    if (reply->checksum != CalculateChecksum(packet, size)) {
        Log("IP reply checksum mismatch");
        return -1;
    }

    // Find matching request by sequence ID
    IPRequest* request = FindIPRequest(conn, reply->sequenceId);
    if (!request) {
        Log("No matching IP request found for reply");
        return -1;
    }

    // Prepare event
    ClientIPReplyEvent event;
    event.eventType = EVENT_CLIENT_IP_REPLY;
    event.connId = conn->connId;
    event.messageType = MS_GetClientIPReply;
    event.sequenceId = reply->sequenceId;
    event.timestamp = GetCurrentTime();

    // Convert IP addresses from network byte order
    event.externalIP = ntohl(reply->externalIP);
    event.externalPort = ntohs(reply->externalPort);
    event.ipVersion = reply->ipVersion;
    event.internalIP = ntohl(reply->internalIP);
    event.internalPort = ntohs(reply->internalPort);
    event.natType = reply->natType;

    event.requestId = request->requestId;
    event.responseTime = GetCurrentTime() - request->sendTime;

    // Store flags
    event.flags = reply->flags;

    // Invoke callback
    if (conn->pIPReplyCallback) {
        conn->pIPReplyCallback(&event, packet, size);
    }

    // Update connection state with external IP
    conn->externalIP = event.externalIP;
    conn->externalPort = event.externalPort;

    // Mark request as complete
    CompleteIPRequest(request);

    return 0;
}
```

### Client Side

```c
// Client: Store and use external IP
uint32_t g_ExternalIP = 0;
uint16_t g_ExternalPort = 0;
uint32_t g_InternalIP = 0;
uint16_t g_InternalPort = 0;
NATType g_NATType = NAT_UNKNOWN;

int OnClientIPReply(ClientIPReplyEvent* reply, void* data, uint32_t size) {
    // Check for errors
    if (reply->flags & IP_REPLY_FLAG_ERROR) {
        printf("Error: Failed to get external IP address\n");
        return -1;
    }

    // Store external IP
    g_ExternalIP = reply->externalIP;
    g_ExternalPort = reply->externalPort;

    printf("External IP: %d.%d.%d.%d:%d\n",
           (g_ExternalIP >> 0) & 0xFF,
           (g_ExternalIP >> 8) & 0xFF,
           (g_ExternalIP >> 16) & 0xFF,
           (g_ExternalIP >> 24) & 0xFF,
           g_ExternalPort);

    // Check for NAT
    if (reply->flags & IP_REPLY_FLAG_NAT_DETECTED) {
        g_InternalIP = reply->internalIP;
        g_InternalPort = reply->internalPort;
        g_NATType = (NATType)reply->natType;

        printf("NAT detected: Type %d\n", g_NATType);
        printf("Internal IP: %d.%d.%d.%d:%d\n",
               (g_InternalIP >> 0) & 0xFF,
               (g_InternalIP >> 8) & 0xFF,
               (g_InternalIP >> 16) & 0xFF,
               (g_InternalIP >> 24) & 0xFF,
               g_InternalPort);
    } else {
        g_NATType = NAT_NONE;
        printf("No NAT detected (direct connection)\n");
    }

    // Use IP for P2P connections, server listings, etc.
    OnExternalIPReceived(g_ExternalIP, g_ExternalPort);

    return 0;
}

// Initialize IP detection system
void InitializeIPDetection() {
    // Register reply callback
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_CLIENT_IP_REPLY;
    reg.callbackFunc = OnClientIPReply;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    obj->RegisterCallback2(&reg);

    // Request IP address
    RequestExternalIP();
}
```

---

## Flow/State Machine

### IP Reply Processing Flow

```
Receive MS_GetClientIPReply (0x0104)
        ↓
    Validate Packet
        ↓
    Parse IP Address
        ↓
    Check Flags
    ├─> Error? ────> Handle Error
    └─> Success? ───> Continue
        ↓
    Invoke OnClientIPReply Callback
        ↓
    Client Processes IP
    ├─> Store External IP
    ├─> Detect NAT Type
    └─> Update Connection State
        ↓
    Cache Result
        ↓
    Use IP for P2P/Server List
```

### Sequence Diagram

```
Master Server       Launcher              Client
     |                 |                    |
     |--Determine IP-->|                    |
     |                 |                    |
     |--Send Reply---->|                    |
     |  (0x0104)       |                    |
     |                 |                    |
     |                 |--Parse Packet----->|
     |                 |                    |
     |                 |--OnClientIPReply-->|
     |                 |                    |
     |                 |                    |--Store IP
     |                 |                    |--Cache Result
     |                 |                    |--Use IP
```

---

## Diagnostic Strings

Strings related to IP address replies:

| String | Address | Context |
|--------|---------|---------|
| `"MS_GetClientIPReply"` | - | Message type name |
| `"External IP: %d.%d.%d.%d:%d"` | - | IP address logging |
| `"NAT detected: type %d"` | - | NAT type notification |
| `"Failed to get external IP: error %d"` | - | Error notification |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SUCCESS` | IP retrieved successfully |
| -1 | `INVALID_PACKET` | Malformed reply packet |
| -2 | `CHECKSUM_MISMATCH` | Packet checksum invalid |
| -3 | `NO_MATCHING_REQUEST` | No matching request found |
| -4 | `SERVER_ERROR` | Server reported error |

---

## Performance Considerations

- **Caching**: Always cache the IP address to avoid repeated requests
- **Response Time**: Typically 50-500ms depending on network
- **Packet Size**: Small packet (38 bytes), minimal bandwidth impact
- **Threading**: Callback may be on network thread; avoid blocking
- **Memory**: IP address is 4 bytes (IPv4); trivial memory footprint

---

## Security Considerations

- **Privacy**: IP address is sensitive personal data; handle per privacy policy
- **Logging**: Be cautious logging IP addresses (GDPR compliance)
- **Validation**: Validate IP address format (not 0.0.0.0 or invalid ranges)
- **Spoofing**: Server determines external IP; harder to spoof than client-reported
- **NAT Detection**: Internal IP should not be shared with other clients

---

## Notes

- **Purpose**: Essential for P2P connections, NAT traversal, and server listings
- **Direction**: Server → Client (response to OnClientIPRequest)
- **Frequency**: Called once per session or after reconnection
- **NAT Detection**: Helps determine connectivity method for P2P
- **Common Pitfalls**:
  - Not caching result (wastes bandwidth)
  - Not handling NAT (P2P may fail)
  - Not checking error flag (invalid IP)
  - Assuming IP won't change (can change during session)
- **Best Practices**:
  - Cache the result for session duration
  - Handle NAT type for P2P connections
  - Request new IP after reconnect
  - Validate IP address format
  - Respect privacy when logging

---

## Related Callbacks

- **[OnClientIPRequest](network/OnClientIPRequest.md)** - IP address request callback
- **[OnPacket](network/OnPacket.md)** - General packet callback
- **[OnConnect](network/OnConnect.md)** - Connection established
- **[OnDisconnect](network/OnDisconnect.md)** - Connection closed

---

## VTable Functions

Related VTable functions for IP replies:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 6 | 0x18 | SendPacket | Send IP request |
| 7 | 0x1C | ReceivePacket | Receive IP reply |

---

## References

- **Source**: `callbacks/network/OnPacket.md` - Message type 0x0104
- **Source**: `extracted_strings_analysis.md` - MS_GetClientIPReply string
- **Message Type**: MS_GetClientIPReply (0x0104)
- **Direction**: Master Server → Client
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
- [ ] Confirm NAT type detection mechanism
- [ ] Test IPv6 support

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include <stdint.h>

// Global IP storage
uint32_t g_ExternalIP = 0;
uint16_t g_ExternalPort = 0;
uint32_t g_InternalIP = 0;
uint16_t g_InternalPort = 0;
int g_NATType = 5;  // NAT_UNKNOWN

// Reply event structure
typedef struct {
    uint32_t eventType;
    uint32_t connId;
    uint32_t direction;
    uint16_t messageType;
    uint16_t flags;
    uint32_t sequenceId;
    uint32_t timestamp;
    uint32_t externalIP;
    uint16_t externalPort;
    uint16_t ipVersion;
    uint32_t internalIP;
    uint16_t internalPort;
    uint16_t natType;
    uint32_t requestId;
    uint32_t responseTime;
} ClientIPReplyEvent;

// NAT type names
const char* NATTypeNames[] = {
    "None (Direct)",
    "Full Cone",
    "Restricted Cone",
    "Port Restricted",
    "Symmetric",
    "Unknown"
};

// Convert IP to string
void IPToString(uint32_t ip, char* buffer) {
    sprintf(buffer, "%d.%d.%d.%d",
            (ip >> 0) & 0xFF,
            (ip >> 8) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 24) & 0xFF);
}

// IP reply callback
int MyOnClientIPReply(ClientIPReplyEvent* reply, void* data, uint32_t size) {
    // Check for errors
    if (reply->flags & 0x0002) {  // IP_REPLY_FLAG_ERROR
        printf("[IP REPLY ERROR] Failed to retrieve external IP\n");
        return -1;
    }

    // Store external IP
    g_ExternalIP = reply->externalIP;
    g_ExternalPort = reply->externalPort;

    // Log external IP
    char ipStr[16];
    IPToString(g_ExternalIP, ipStr);
    printf("[IP REPLY] External IP: %s:%d\n", ipStr, g_ExternalPort);
    printf("[IP REPLY] Response time: %ums\n", reply->responseTime);

    // Check for NAT
    if (reply->flags & 0x0004) {  // IP_REPLY_FLAG_NAT_DETECTED
        g_InternalIP = reply->internalIP;
        g_InternalPort = reply->internalPort;
        g_NATType = reply->natType;

        IPToString(g_InternalIP, ipStr);
        printf("[IP REPLY] NAT Detected: %s\n", NATTypeNames[g_NATType]);
        printf("[IP REPLY] Internal IP: %s:%d\n", ipStr, g_InternalPort);
    } else {
        g_NATType = 0;  // NAT_NONE
        printf("[IP REPLY] No NAT (Direct Connection)\n");
    }

    // IP can now be used for:
    // - P2P connections
    // - Server listings
    // - NAT traversal
    // - Connection diagnostics

    return 0;  // Success
}

// Register callback and request IP
void InitializeNetworking() {
    // Register reply callback
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_CLIENT_IP_REPLY;
    reg.callbackFunc = MyOnClientIPReply;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered IP reply callback, ID=%d\n", id);

    // Also register request callback if needed
    // ...

    // Request external IP
    printf("Requesting external IP address...\n");
    RequestExternalIP();
}

// Check if external IP is available
bool HasExternalIP() {
    return g_ExternalIP != 0;
}

// Get external IP for P2P
uint32_t GetExternalIP() {
    return g_ExternalIP;
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |

---

**End of Document**
