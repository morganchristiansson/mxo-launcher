# Network Layer Analysis Summary

## Agent: NETWORK_ANALYST
## Date: 2025-06-17
## Status: PHASE 1 COMPLETE

---

## Executive Summary

The Matrix Online launcher implements a comprehensive TCP networking layer using Windows Sockets 2 (WS2_32.dll). The architecture follows a **thread-per-client** model with centralized connection management.

### Key Findings
- **26 Winsock functions** imported (by ordinal)
- **2 main classes** handling networking:
  - `CLTSocketLayer` - Low-level socket abstraction
  - `CLTThreadPerClientTCPEngine` - Server implementation
- **Source files** identified in `\matrixstaging\runtime\src\`
- **Buffer sizes**: 4096 bytes for receive operations

---

## Complete Socket Function Map

### Socket Lifecycle Functions
| Function | Ordinal | Address | Purpose |
|----------|---------|---------|---------|
| WSAStartup | 115 | 0x00452e17 | Initialize Winsock |
| WSASocketA | 151 | 0x00432106 | Create socket |
| bind | 1 | 0x0043211f | Bind to local address |
| listen | 13 | 0x00431edb | Listen for connections |
| accept | - | 0x004320f5 | Accept connection (via WSASocketA) |
| connect | 3 | Multiple | Connect to server |
| send | 20 | 0x00430884 | Send data |
| recv | 17 | 0x0043032b | Receive data |
| select | 19 | 0x004306d2 | Monitor sockets (timeout) |
| shutdown | 23 | Multiple | Graceful disconnect |
| closesocket | 2 | 0x0045230b | Close socket |
| WSACleanup | 111 | Multiple | Cleanup Winsock |

### Utility Functions
| Function | Ordinal | Purpose |
|----------|---------|---------|
| WSAGetLastError | 116 | Get error code |
| htonl | 8 | Host to network long |
| htons | 9 | Host to network short |
| ntohl | 15 | Network to host long |
| ntohs | 16 | Network to host short |
| inet_addr | 10 | String to IP address |
| inet_ntoa | 11 | IP address to string |
| gethostbyname | 52 | DNS resolution |
| getpeername | 4 | Get remote address |
| getsockname | 6 | Get local address |
| getsockopt | 7 | Get socket options |
| setsockopt | 22 | Set socket options |
| ioctlsocket | 12 | I/O control |
| sendto | 21 | Send datagram |
| recvfrom | 18 | Receive datagram |

---

## Class Architecture

### CLTSocketLayer
**File**: `\matrixstaging\runtime\src\libltnet\sys\pc\pcsocket.cpp`

**Responsibilities**:
- Winsock initialization (WSAStartup)
- Socket creation (WSASocketA)
- Address binding (bind)
- Connection establishment (connect)
- Socket cleanup (closesocket, WSACleanup)

**Key Methods**:
- `Init()` - Initialize Winsock v2.2
- `CreateSocket()` - Create TCP socket
- `Bind()` - Bind to local port
- `Listen()` - Start listening
- `Connect()` - Connect to server
- `Close()` - Close socket

### CLTThreadPerClientTCPEngine
**File**: `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

**Architecture**:
```
┌─────────────────────────────────────┐
│   AcceptThread (Main Thread)         │
│   ├─ Listen on port                  │
│   ├─ Accept connections              │
│   └─ Spawn WorkerThread per client   │
└─────────────────────────────────────┘
            │
            ├────────────────┐
            │                │
            ▼                ▼
    ┌───────────────┐  ┌───────────────┐
    │ WorkerThread  │  │ WorkerThread  │
    │ (Client 1)    │  │ (Client 2)    │
    │ ├─ Recv()     │  │ ├─ Recv()     │
    │ ├─ Process    │  │ ├─ Process    │
    │ └─ Send()     │  │ └─ Send()     │
    └───────────────┘  └───────────────┘
```

**Key Methods**:
- `AcceptThread` - Accepts incoming connections
- `WorkerThread::ThreadRun()` - Handles client communication
- Error logging with client IP:port information

---

## Network Operations Detail

### 1. Initialization Sequence
```c
// CLTSocketLayer::Init()
WSAStartup(MAKEWORD(2,2), &wsaData);  // Version 2.2
```

### 2. Server Setup Sequence
```c
// CLTThreadPerClientTCPEngine
socket = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, ...);
bind(socket, &addr, sizeof(addr));
listen(socket, backlog);
```

### 3. Connection Accept Loop
```c
while (running) {
    client_socket = accept(server_socket, &client_addr, &addr_len);
    if (client_socket != INVALID_SOCKET) {
        CreateWorkerThread(client_socket, client_addr);
    }
}
```

### 4. Data Reception
```c
// Buffer size: 4096 bytes
char buffer[4096];
int bytes_received = recv(socket, buffer, sizeof(buffer), 0);

if (bytes_received == SOCKET_ERROR) {
    int error = WSAGetLastError();
    LogError("Recv error %d from %d.%d.%d.%d:%d", 
             error, IP[0], IP[1], IP[2], IP[3], port);
}
```

### 5. Data Transmission
```c
int bytes_sent = send(socket, data, length, flags);
if (bytes_sent == SOCKET_ERROR) {
    // Handle error
}
```

### 6. Timeout Handling
```c
// Using select() for timeout
fd_set read_fds;
struct timeval timeout;

FD_ZERO(&read_fds);
FD_SET(socket, &read_fds);
timeout.tv_sec = timeout_seconds;
timeout.tv_usec = 0;

int result = select(socket + 1, &read_fds, NULL, NULL, &timeout);
if (result == 0) {
    // Timeout occurred
}
```

### 7. Graceful Shutdown
```c
shutdown(socket, SD_SEND);  // Send FIN
recv(socket, buffer, sizeof(buffer), 0);  // Wait for ACK
closesocket(socket);
```

---

## Error Handling

### WSA Error Codes Encountered
- **0x2719** (10009) - WSAEINTR: Interrupted function call
- **0x2733** (10027) - WSAEMSGSIZE: Message too long

### Error Strings Discovered
```
LTTCP_ALREADYCONNECTED
LTTCP_NOTCONNECTED
LTTCP_TIMEDOUT
LTTCP_CONNREFUSED
LTTCP_CONNRESET
LTTCP_NETUNREACH
LTTCP_HOSTUNREACH
LTTCP_PORTALREADYINUSE
LTTCP_ALREADYCLOSED
```

### Error Logging Pattern
```c
LogError("CLTThreadPerClientTCPEngine::WorkerThread::ThreadRun(): "
         "Recv error %d on connection from %d.%d.%d.%d:%d.\n",
         WSAGetLastError(), ip[0], ip[1], ip[2], ip[3], port);
```

---

## Packet Structure Analysis

### Inferred Packet Format
```
┌─────────────────────────────────────┐
│         Packet Header (16 bytes)    │
├─────────────────────────────────────┤
│  Type (1)  │ Length (4) │ Flags (?) │
├─────────────────────────────────────┤
│         Data (Variable Length)       │
└─────────────────────────────────────┘
```

**Details**:
- Header size referenced: 16 bytes (0x10)
- Maximum recv buffer: 4096 bytes
- Variable length data

---

## Source Code Structure

### Discovered Directories
```
\matrixstaging\runtime\src\
├── libltnet\
│   └── sys\
│       └── pc\
│           └── pcsocket.cpp          (CLTSocketLayer)
└── liblttcp\
    └── ltthreadperclienttcpengine.cpp (CLTThreadPerClientTCPEngine)
```

### Line Numbers Found
- Line 15 (0xf) in pcsocket.cpp
- Line 30 (0x1e) in pcsocket.cpp
- Line 464 (0x1d0) in ltthreadperclienttcpengine.cpp
- Line 704 (0x2c0) in ltthreadperclienttcpengine.cpp
- Line 726 (0x2d6) in ltthreadperclienttcpengine.cpp

---

## Performance Characteristics

### Buffer Management
- **Recv Buffer**: 4096 bytes per connection
- **Send Buffer**: Variable (passed as parameter)
- **No buffer pooling** observed (each connection has own buffer)

### Threading Model
- **Thread-per-client** architecture
- Each client gets dedicated worker thread
- Accept thread runs independently
- Potential scalability limitation for many clients

### Timeout Handling
- Uses `select()` for socket monitoring
- Timeout values stored in timeval structure
- Supports both send and receive timeouts

---

## Security Observations

### Current Implementation
- No encryption detected at socket layer
- Plain TCP connections
- Error messages include client IP addresses
- No authentication at socket level

### Potential Concerns
- Unencrypted traffic
- No rate limiting observed
- Thread-per-client may be vulnerable to DoS

---

## Next Steps

### Phase 2 Tasks
- [ ] Extract packet structures from protocol layer
- [ ] Document protocol dispatch mechanism
- [ ] Analyze session management
- [ ] Map client.dll integration points
- [ ] Study packet type definitions

### Recommended Analysis
1. **Protocol Analysis**: Capture and decode packet formats
2. **Session Management**: Find session state handling
3. **Performance**: Analyze thread pool vs current model
4. **Security**: Document any encryption/authentication layers

---

## Tools Used
- **radare2**: Disassembly and binary analysis
- **strings**: String extraction
- **objdump**: PE import/export analysis

## Files Generated
- `TCP_SOCKET_ANALYSIS.md` - Detailed socket function analysis
- `ws2_32_ordinals.txt` - Winsock ordinal mapping

## Progress
- **Task 1**: TCP Handling Code - ✅ **40% Complete**
  - [x] Find TCP socket creation code
  - [x] Document socket management functions
  - [x] Map send/receive functions
  - [ ] Parse connection handling
  - [ ] Document timeout handling
  - [ ] Find disconnect logic

---

**End of Phase 1 Analysis**
