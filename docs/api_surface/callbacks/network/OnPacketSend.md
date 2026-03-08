# OnPacketSend

## Overview

**Category**: Network
**Direction**: Launcher → Client
**Purpose**: Network packet sent notification callback - invoked after a packet is successfully transmitted
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnPacketSend(PacketSendEvent* sendEvent, void* packetData, uint32_t packetSize);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `PacketSendEvent*` | sendEvent | Packet send event metadata and status |
| `void*` | packetData | Pointer to sent packet data buffer |
| `uint32_t` | packetSize | Size of sent packet in bytes |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Send notification processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Packet logged/recorded (stop further processing) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  PacketSendEvent* sendEvent
[ESP+8]  void* packetData
[ESP+12] uint32_t packetSize

Registers:
EAX = return value
```

---

## Data Structures

### PacketSendEvent Structure

```c
struct PacketSendEvent {
    uint32_t eventType;         // Event type: SEND_EVENT (0x02)
    uint32_t connId;            // Connection identifier
    uint32_t direction;         // Always 1 (sent)

    // Packet metadata
    uint16_t messageType;       // Message type from packet header
    uint16_t flags;             // Packet flags (reliable, encrypted, etc.)
    uint32_t sequenceId;        // Sequence number for ordering
    uint32_t timestamp;         // Send timestamp

    // Send status
    uint32_t bytesSent;         // Number of bytes actually sent
    uint32_t sendStatus;        // Send result status code
    uint32_t socketError;       // Socket error code (if failed)

    // Destination info
    uint32_t destIP;            // Destination IP address
    uint16_t destPort;          // Destination port
    uint16_t reserved;          // Padding

    // Buffer info
    void* pOriginalBuffer;      // Original packet buffer pointer
    uint32_t originalSize;      // Original packet size before compression
    uint32_t compressedSize;    // Size after compression (if compressed)

    // Statistics
    uint32_t latency;           // Send latency in milliseconds
    uint32_t retryCount;        // Number of retries attempted
};
```

**Size**: 56 bytes

### SendStatus Enumeration

```c
enum SendStatus {
    SEND_SUCCESS = 0,           // Packet sent successfully
    SEND_PENDING = 1,           // Send in progress (async)
    SEND_FAILED = 2,            // Send failed
    SEND_TIMEOUT = 3,           // Send timed out
    SEND_DISCONNECTED = 4,      // Connection closed during send
    SEND_BUFFER_FULL = 5,       // Send buffer full
    SEND_COMPRESSED = 6,        // Packet was compressed
    SEND_ENCRYPTED = 7          // Packet was encrypted
};
```

### Packet Flags

| Flag | Bit Mask | Description |
|------|----------|-------------|
| `PACKET_FLAG_RELIABLE` | 0x0001 | Guaranteed delivery |
| `PACKET_FLAG_COMPRESSED` | 0x0002 | Packet is compressed (zlib) |
| `PACKET_FLAG_ENCRYPTED` | 0x0004 | Packet is encrypted (DES) |
| `PACKET_FLAG_PRIORITY` | 0x0008 | High priority packet |
| `PACKET_FLAG_BROADCAST` | 0x0010 | Broadcast to all clients |
| `PACKET_FLAG_ACK_REQUIRED` | 0x0020 | Requires acknowledgment |

---

## Usage

### Registration

Register the OnPacketSend callback to monitor outbound network traffic:

```c
// Method 1: Using RegisterCallback2
CallbackRegistration reg;
reg.eventType = EVENT_PACKET_SEND;
reg.callbackFunc = MyOnPacketSend;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; SendPacket function invokes callback after transmission
SendPacket:
    push ebp
    mov ebp, esp

    ; Send packet via socket
    ; ... (socket send code)

    ; Check if send callback registered
    mov eax, [object + send_callback_offset]
    test eax, eax
    je skip_callback

    ; Prepare callback parameters
    push packetSize           ; [ESP+12]
    push packetData           ; [ESP+8]
    lea ecx, [sendEvent]
    push ecx                  ; [ESP+4]
    call eax                  ; Call OnPacketSend callback
    add esp, 12

skip_callback:
    mov eax, [send_status]    ; Return send status
    pop ebp
    ret
```

### C++ Pattern

```c
// Client-side send packet monitoring
int MyOnPacketSend(PacketSendEvent* sendEvent, void* packetData, uint32_t packetSize) {
    // Log packet send
    printf("Packet sent: %u bytes to %d.%d.%d.%d:%d\n",
           sendEvent->bytesSent,
           (sendEvent->destIP >> 0) & 0xFF,
           (sendEvent->destIP >> 8) & 0xFF,
           (sendEvent->destIP >> 16) & 0xFF,
           (sendEvent->destIP >> 24) & 0xFF,
           sendEvent->destPort);

    // Track statistics
    g_TotalBytesSent += sendEvent->bytesSent;
    g_PacketsSent++;

    // Check for errors
    if (sendEvent->sendStatus != SEND_SUCCESS) {
        printf("Send failed: status=%d, error=%d\n",
               sendEvent->sendStatus, sendEvent->socketError);
    }

    return 0;  // Continue processing
}
```

---

## Implementation

### Launcher Side

```c
// Launcher: SendPacket implementation
int SendPacket(ConnectionObject* conn, void* data, uint32_t size, uint32_t flags) {
    // Validate parameters
    if (!conn || !data || size == 0) {
        return -1;
    }

    // Prepare packet header
    PacketHeader header;
    header.messageType = ExtractMessageType(data);
    header.payloadSize = size;
    header.sequenceId = conn->nextSequenceId++;
    header.flags = flags;
    header.timestamp = GetCurrentTime();

    // Compress if needed
    void* sendBuffer = data;
    uint32_t sendSize = size;
    if (flags & PACKET_FLAG_COMPRESSED) {
        sendBuffer = CompressPacket(data, size, &sendSize);
    }

    // Encrypt if needed
    if (flags & PACKET_FLAG_ENCRYPTED) {
        EncryptPacket(sendBuffer, sendSize, conn->sessionKey);
    }

    // Send via socket
    int bytesSent = send(conn->socket, sendBuffer, sendSize, 0);

    // Prepare send event
    PacketSendEvent sendEvent;
    sendEvent.eventType = EVENT_PACKET_SEND;
    sendEvent.connId = conn->connId;
    sendEvent.messageType = header.messageType;
    sendEvent.sequenceId = header.sequenceId;
    sendEvent.bytesSent = bytesSent;
    sendEvent.sendStatus = (bytesSent > 0) ? SEND_SUCCESS : SEND_FAILED;
    sendEvent.destIP = conn->remoteIP;
    sendEvent.destPort = conn->remotePort;

    // Invoke callback
    if (conn->pSendCallback) {
        conn->pSendCallback(&sendEvent, data, size);
    }

    // Cleanup
    if (flags & PACKET_FLAG_COMPRESSED) {
        free(sendBuffer);
    }

    return bytesSent;
}
```

### Client Side

```c
// Client: Register packet send monitor
void RegisterPacketSendMonitor() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_PACKET_SEND;
    reg.callbackFunc = MonitorOutboundTraffic;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered send monitor, ID=%d\n", id);
}

// Monitor outbound traffic for logging
int MonitorOutboundTraffic(PacketSendEvent* sendEvent, void* data, uint32_t size) {
    // Log all outbound packets
    LogPacketSend(sendEvent, data, size);

    // Track bandwidth usage
    g_BandwidthMonitor.RecordSend(sendEvent->bytesSent);

    return 0;  // Continue processing
}
```

---

## Flow/State Machine

### Packet Send Flow

```
Client Request to Send
        ↓
    Validate Data
        ↓
    Prepare Header
        ↓
    Compress? ─Yes─> Compress Packet
        ↓                    ↓
    Encrypt? ─Yes─> Encrypt Packet
        ↓                    ↓
    Send via Socket
        ↓
    Update Statistics
        ↓
    Invoke OnPacketSend Callback
        ↓
    Return Result
```

### Sequence Diagram

```
Client              Launcher              Network
   |                   |                    |
   |--SendPacket()---->|                    |
   |                   |--Compress/Encrypt->|
   |                   |                    |
   |                   |--send()----------->|
   |                   |                    |
   |                   |<--bytes sent-------|
   |                   |                    |
   |                   |--OnPacketSend()--->| (callback)
   |                   |                    |
   |<--return status---|                    |
   |                   |                    |
```

---

## Diagnostic Strings

Strings found in binaries related to packet sending:

| String | Address | Context |
|--------|---------|---------|
| `"CMessageConnection::SendPacket(): Packet being sent to %d.%d.%d.%d:%d discarded..."` | 0x004b7970 | Send logging when packet discarded |
| `"SendPacket failed: error %d"` | - | Error logging |
| `"Packet send timeout after %d retries"` | - | Timeout notification |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SEND_SUCCESS` | Packet sent successfully |
| 1 | `SEND_PENDING` | Async send in progress |
| 2 | `SEND_FAILED` | General send failure |
| 3 | `SEND_TIMEOUT` | Send operation timed out |
| 4 | `SEND_DISCONNECTED` | Connection closed |
| 5 | `SEND_BUFFER_FULL` | Send buffer full |
| -1 | `INVALID_PARAM` | Invalid parameters |
| -2 | `NOT_CONNECTED` | No active connection |

---

## Performance Considerations

- **Buffer Sizes**: Packets can be 0-64KB; typical sizes 100-1500 bytes
- **Optimization Tips**:
  - Batch small packets to reduce overhead
  - Use compression for large payloads (>500 bytes)
  - Enable reliable flag only when necessary
- **Threading**: Send operations may be async; callback may be on different thread
- **Memory**: Packet data buffer is valid only during callback; copy if needed

---

## Security Considerations

- **Validation**: Verify packet size limits to prevent buffer overflows
- **Encryption**: Sensitive data should use PACKET_FLAG_ENCRYPTED
- **Rate Limiting**: Monitor send rate to prevent DoS detection
- **Data Sensitivity**: Packet data may contain sensitive information; handle carefully in logs

---

## Notes

- **Timing**: Called after packet is sent (not before)
- **Reliability**: May not be called for all packets if send fails early
- **Statistics**: Useful for bandwidth monitoring and debugging
- **Common Pitfalls**:
  - Don't modify packet data in callback (already sent)
  - Don't block in callback (may cause network stalls)
  - Don't assume packet reached destination (network unreliability)
- **Best Practices**:
  - Use for logging and statistics only
  - Keep callback fast (minimal processing)
  - Handle all error status codes

---

## Related Callbacks

- **[OnPacket](network/OnPacket.md)** - Packet received callback
- **[OnConnect](network/OnConnect.md)** - Connection established
- **[OnDisconnect](network/OnDisconnect.md)** - Connection closed
- **[OnConnectionError](network/OnConnectionError.md)** - Connection error
- **[OnTimeout](network/OnTimeout.md)** - Send timeout notification

---

## VTable Functions

Related VTable functions for packet sending:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 6 | 0x18 | SendPacket | Send network packet |
| 7 | 0x1C | SendReliable | Send with guaranteed delivery |
| 8 | 0x20 | BroadcastPacket | Send to all connections |

---

## References

- **Source**: `callbacks/network/OnPacket.md` - Related packet receive callback
- **Source**: `MASTER_DATABASE.md` - SendPacket function documentation
- **Source**: `data_passing_mechanisms.md` - Packet data structures
- **Evidence**: Connection VTable offset 0x18 (SendPacket function)
- **Evidence**: Diagnostic string at 0x004b7970 in launcher.exe

---

## Documentation Status

**Status**: ✅ Complete
**Confidence**: Medium
**Last Updated**: 2025-06-18
**Documented By**: SetMasterDatabase API Analysis

---

## TODO

- [ ] Verify exact callback registration mechanism
- [ ] Find assembly code that invokes OnPacketSend
- [ ] Confirm PacketSendEvent structure layout
- [ ] Identify specific event type constant for registration

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include <stdint.h>

// Packet send event structure
typedef struct {
    uint32_t eventType;
    uint32_t connId;
    uint16_t messageType;
    uint16_t flags;
    uint32_t sequenceId;
    uint32_t timestamp;
    uint32_t bytesSent;
    uint32_t sendStatus;
    uint32_t socketError;
    uint32_t destIP;
    uint16_t destPort;
    uint16_t reserved;
    void* pOriginalBuffer;
    uint32_t originalSize;
    uint32_t compressedSize;
    uint32_t latency;
    uint32_t retryCount;
} PacketSendEvent;

// Statistics tracking
uint64_t g_TotalBytesSent = 0;
uint32_t g_PacketsSent = 0;

// Packet send callback
int MyOnPacketSend(PacketSendEvent* sendEvent, void* packetData, uint32_t packetSize) {
    // Log packet send
    printf("[SEND] Conn=%u Type=0x%04X Seq=%u Bytes=%u Status=%d\n",
           sendEvent->connId,
           sendEvent->messageType,
           sendEvent->sequenceId,
           sendEvent->bytesSent,
           sendEvent->sendStatus);

    // Update statistics
    g_TotalBytesSent += sendEvent->bytesSent;
    g_PacketsSent++;

    // Check for errors
    if (sendEvent->sendStatus != 0) {
        printf("[SEND ERROR] Status=%d Error=%d\n",
               sendEvent->sendStatus, sendEvent->socketError);
    }

    return 0;  // Continue processing
}

// Registration function
void RegisterPacketSendCallback() {
    // Get API object from master database
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    // Register callback
    CallbackRegistration reg;
    reg.eventType = EVENT_PACKET_SEND;
    reg.callbackFunc = MyOnPacketSend;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int callbackId = obj->RegisterCallback2(&reg);
    printf("Registered send callback, ID=%d\n", callbackId);
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |

---

**End of Document**
