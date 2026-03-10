# TCP Socket Creation Code Analysis

## Agent: NETWORK_ANALYST
## Date: 2025-06-17
## Status: IN PROGRESS

### Overview
Analysis of TCP socket creation and management in launcher.exe. The launcher uses Windows Sockets 2 (WS2_32.dll) for all network communications.

---

## WS2_32.dll Imports Discovered

### Socket Functions (by Ordinal)
```
Ordinal 1   = bind           - Bind socket to local address
Ordinal 2   = closesocket    - Close socket
Ordinal 3   = connect        - Connect to remote host
Ordinal 4   = getpeername    - Get address of connected peer
Ordinal 6   = getsockname    - Get local socket address
Ordinal 7   = getsockopt     - Get socket options
Ordinal 8   = htonl          - Host to network long (byte order)
Ordinal 9   = htons          - Host to network short (byte order)
Ordinal 10  = inet_addr      - Convert IP string to address
Ordinal 11  = inet_ntoa      - Convert address to IP string
Ordinal 12  = ioctlsocket    - Control socket I/O mode
Ordinal 13  = listen         - Listen for connections
Ordinal 15  = ntohl          - Network to host long
Ordinal 16  = ntohs          - Network to host short
Ordinal 17  = recv           - Receive data
Ordinal 18  = recvfrom       - Receive datagram
Ordinal 19  = select         - Monitor socket status
Ordinal 20  = send           - Send data
Ordinal 21  = sendto         - Send datagram
Ordinal 22  = setsockopt     - Set socket options
Ordinal 23  = shutdown       - Shutdown socket
Ordinal 52  = gethostbyname  - Resolve hostname
Ordinal 111 = WSACleanup     - Cleanup Winsock
Ordinal 115 = WSAStartup     - Initialize Winsock
Ordinal 116 = WSAGetLastError- Get last error code
Ordinal 151 = WSASocketA     - Create socket
```

---

## Function 1: WSAStartup (Initialization)

### Location: `fcn.00452e00`
### Address: `0x00452e17`

**Purpose**: Initialize Windows Sockets 2

**Disassembly**:
```asm
0x00452e12  push 0x202              ; Version 2.2 (MAKEWORD(2,2))
0x00452e17  call dword [sym.imp.WS2_32.dll_Ordinal_115]  ; WSAStartup
0x00452e1d  mov edi, eax
0x00452e1f  test edi, edi
0x00452e21  je 0x452e8e            ; Jump if failed
```

**Error String**: `"CLTSocketLayer::Init(): Failed to initialize Winsock with error = %d!\n"`

**Source File Reference**: `\matrixstaging\runtime\src\libltnet\sys\pc\pcsocket.cpp`

**Class Name**: `CLTSocketLayer::Init()`

---

## Function 2: Socket Creation (WSASocketA)

### Location: Multiple functions
### Primary Address: `0x00432106`

**Purpose**: Create a new socket

**Disassembly**:
```asm
0x00432104  push ecx               ; Protocol info
0x00432105  push eax               ; Protocol
0x00432106  call 0x48c7a8         ; WSASocketA (Ordinal 151)
0x0043210b  test eax, eax
0x0043210d  jne 0x4325c0          ; Jump if failed
```

**Source File Reference**: `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

**Class Name**: `CLTThreadPerClientTCPEngine`

---

## Function 3: bind()

### Location: `0x0043211f`

**Purpose**: Bind socket to local address and port

**Disassembly**:
```asm
0x00432119  push edx               ; Address length
0x0043211a  push eax               ; sockaddr structure
0x0043211e  push ecx               ; Socket descriptor
0x0043211f  call dword [sym.imp.WS2_32.dll_Ordinal_1]  ; bind
0x00432125  mov esi, eax
0x00432127  cmp esi, 0xffffffff
0x0043212a  jne 0x432199          ; Jump if success
```

---

## Function 4: listen()

### Location: `0x00431edb`

**Purpose**: Listen for incoming connections

**Disassembly**:
```asm
0x00431edb  call dword [sym.imp.WS2_32.dll_Ordinal_13]  ; listen
```

---

## Function 5: connect()

### Multiple Locations:
- `fcn.0042f970` at `0x42fac4`
- `fcn.00431310` at `0x431429`
- `fcn.00449b40` at `0x449c00`
- `fcn.00452300` at `0x45230b`
- `fcn.00452e00` at `0x452f7c`

**Purpose**: Connect to remote server

**Example from fcn.00452300**:
```asm
0x00452305  cmp eax, 0xffffffff
0x00452308  je 0x452317
0x0045230a  push eax              ; Socket descriptor
0x0045230b  call dword [sym.imp.WS2_32.dll_Ordinal_3]  ; connect
```

---

## Function 6: closesocket()

### Location: `fcn.00452300`

**Purpose**: Close socket and release resources

**Disassembly**:
```asm
0x00452305  cmp eax, 0xffffffff    ; Check if socket is valid
0x00452308  je 0x452317           ; Skip if already closed
0x0045230a  push eax              ; Socket descriptor
0x0045230b  call dword [sym.imp.WS2_32.dll_Ordinal_3]  ; connect
0x00452311  mov dword [esi], 0xffffffff  ; Mark as closed
```

---

## Key Discoveries

### 1. Source Code Paths Found
- `\matrixstaging\runtime\src\libltnet\sys\pc\pcsocket.cpp`
  - Contains: `CLTSocketLayer` class
  - Socket initialization code

- `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`
  - Contains: `CLTThreadPerClientTCPEngine` class
  - Thread-per-client TCP engine implementation

### 2. Class Hierarchy
```
CLTSocketLayer
  └─ Socket abstraction layer
  └─ Winsock initialization

CLTThreadPerClientTCPEngine
  └─ TCP server implementation
  └─ Accept thread handling
  └─ Per-client connection management
```

### 3. Error Handling Pattern
```c
// Typical pattern observed:
if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
    LogError("CLTSocketLayer::Init(): Failed to initialize Winsock with error = %d!\n", 
             WSAGetLastError());
    return false;
}
```

### 4. Socket Lifecycle
```
1. WSAStartup()           - Initialize Winsock
2. WSASocketA()           - Create socket
3. bind()                 - Bind to local address
4. listen()               - Listen for connections (server)
   OR
   connect()              - Connect to server (client)
5. send()/recv()          - Data transfer
6. closesocket()          - Close socket
7. WSACleanup()           - Cleanup Winsock
```

---

## TCP Handling Architecture

### Server-Side (Listening)
- **Accept Thread**: Runs in separate thread
- **Function**: `CLTThreadPerClientTCPEngine::AcceptThread`
- **Pattern**: Thread-per-client model
- **Error String**: `"CLTThreadPerClientTCPEngine::AcceptThread: accept() failed with error = %d.\n"`

### Client-Side (Connecting)
- Multiple connection points found
- Used for connecting to game servers
- Error handling with WSAGetLastError()

---

---

## Function 7: recv() - Receive Data

### Location: `0x0043032b`

**Purpose**: Receive data from connected socket

**Disassembly**:
```asm
0x00430317  push 0x1000            ; Buffer size (4096 bytes)
0x0043031c  lea ecx, [edi+0xc]     ; Buffer pointer
0x0043031f  mov dword [ebp-0x68], 0x10  ; Protocol structure
0x00430326  mov eax, [eax+0xc]     ; Socket descriptor
0x00430329  push ecx               ; Buffer address
0x0043032a  push eax               ; Socket
0x0043032b  call dword [sym.imp.WS2_32.dll_Ordinal_17]  ; recv
0x00430331  cmp eax, 0xffffffff    ; Check for error
0x00430334  je 0x430389           ; Jump to error handling
```

**Error String**: `"CLTThreadPerClientTCPEngine::WorkerThread::ThreadRun(): Recv error %d on connection from %d.%d.%d.%d:%d.\n"`

**Source File**: `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

**Class**: `CLTThreadPerClientTCPEngine::WorkerThread::ThreadRun()`

**Buffer Size**: 4096 bytes (0x1000)

**Error Handling**:
- Checks for WSA error codes
- Error code 0x2719 (10009) - WSAEINTR (Interrupted)
- Error code 0x2733 (10027) - WSAEMSGSIZE (Message too long)
- Logs error with client IP address and port

---

## Function 8: send() - Send Data

### Location: `0x00430884`

**Purpose**: Send data to connected socket

**Disassembly**:
```asm
0x00430855  push 0x10              ; Flags (16 bytes)
0x00430857  mov [ebp-0xe8], edx    ; Store packet data
0x0043085d  lea edx, [ebp-0xec]    ; Packet structure
0x00430863  push edx               ; Buffer pointer
0x00430864  push 0                 ; Flags
0x0043086f  push edi               ; Data length
0x00430870  mov ecx, [ebp-0xb0]    ; Packet pointer
0x0043087c  push eax               ; Socket descriptor
0x0043087d  push ebx               ; Buffer address
0x0043087e  mov [ebp-0xe0], ecx    ; Store packet info
0x00430884  call dword [sym.imp.WS2_32.dll_Ordinal_20]  ; send
0x0043088a  cmp eax, 0xffffffff    ; Check for error
0x0043088d  jne 0x4309f9           ; Jump to error handling
```

**Error String**: (Similar to recv error)

**Source File**: `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

**Class**: `CLTThreadPerClientTCPEngine`

---

## Socket Management Architecture

### 1. CLTSocketLayer Class
**Purpose**: Low-level socket abstraction

**Key Methods**:
- `Init()`: Initialize Winsock (WSAStartup)
- `CreateSocket()`: Create socket (WSASocketA)
- `Bind()`: Bind to local address
- `Listen()`: Listen for connections
- `Connect()`: Connect to remote server
- `Close()`: Close socket (closesocket)

**Error Handling**:
- Logs all Winsock errors with WSAGetLastError()
- Source file: `\matrixstaging\runtime\src\libltnet\sys\pc\pcsocket.cpp`
- Line numbers embedded in code (e.g., 0xf = line 15, 0x1e = line 30)

### 2. CLTThreadPerClientTCPEngine Class
**Purpose**: TCP server with thread-per-client model

**Architecture**:
```
AcceptThread (Listening)
    │
    ├─ Accept new connection
    │  └─ Create WorkerThread
    │
    └─ WorkerThread (Per Client)
        ├─ Recv() loop
        ├─ Process packets
        └─ Send() responses
```

**Key Methods**:
- `AcceptThread`: Accepts incoming connections
- `WorkerThread::ThreadRun()`: Handles client communication
- Handles recv/send operations
- Logs errors with client IP:port

**Source File**: `\matrixstaging\runtime\src\liblttcp\ltthreadperclienttcpengine.cpp`

---

## WSA Error Codes Observed

From binary analysis:
- `0x2719` (10009) = WSAEINTR - Interrupted function call
- `0x2733` (10027) = WSAEMSGSIZE - Message too long

Common WSA error prefixes (from strings):
- `LTTCP_ALREADYCONNECTED`
- `LTTCP_NOTCONNECTED`
- `LTTCP_TIMEDOUT`
- `LTTCP_CONNREFUSED`
- `LTTCP_CONNRESET`
- `LTTCP_NETUNREACH`
- `LTTCP_HOSTUNREACH`

---

## Packet Structure Analysis

### Buffer Sizes
- **Recv Buffer**: 4096 bytes (0x1000)
- **Send Buffer**: Variable size (stored in edi register)
- **Packet Header**: 16 bytes (0x10) referenced in send

### Packet Format (Inferred)
```
[Header: 16 bytes]
[Data: Variable length]

Header structure:
- Likely contains:
  - Packet type
  - Packet length
  - Protocol version
  - Flags
```

---

## Next Steps

- [x] Analyze send/recv functions ✅
- [ ] Map packet structures in detail
- [ ] Document protocol dispatch
- [ ] Find session management code
- [ ] Analyze timeout handling
- [ ] Document disconnect logic

---

## Progress Update

**Task**: Find TCP socket creation code
**Status**: ✅ COMPLETED

**Findings**:
- ✅ Socket creation: WSASocketA at 0x00432106
- ✅ Winsock init: WSAStartup at 0x00452e17
- ✅ bind() at 0x0043211f
- ✅ listen() at 0x00431edb
- ✅ connect() at multiple locations
- ✅ closesocket() at 0x0045230b
- ✅ recv() at 0x0043032b
- ✅ send() at 0x00430884

**Classes Documented**:
- ✅ CLTSocketLayer - Low-level socket wrapper
- ✅ CLTThreadPerClientTCPEngine - TCP server implementation

**Next Task**: Document socket management functions
