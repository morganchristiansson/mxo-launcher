# pcsocket.cpp / CLTSocketLayer

## High-confidence recovered function

- `launcher.exe:0x00452e00 = CLTSocketLayer::Init`
- source-file string anchor: `0x004b9bc0 = \matrixstaging\runtime\src\libltnet\sys\pc\pcsocket.cpp`

## Current best behavior

`CLTSocketLayer::Init` is a process-wide Winsock bootstrap helper.

From decompilation/disassembly of `0x00452e00`:

- always calls `WSAStartup(MAKEWORD(2,2), &wsaData)`
- on failure:
  - logs `CLTSocketLayer::Init(): Failed to initialize Winsock with error = %d!`
  - returns failure (`AL = 0`)
- on success:
  - uses one global byte gate `0x004f830c` so the detailed environment logging only happens once
  - logs the Winsock version / highest version / description / system status
  - when the negotiated Winsock major version is at least `2`:
    - opens a datagram socket
    - queries `SO_MAX_MSG_SIZE`
    - reads `HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\MaxUserPort`
    - logs `MaxUserPort` plus max UDP packet size
  - otherwise logs legacy `iMaxSockets` plus max UDP datagram size
  - returns success (`AL = 1`)

## How it fits the rest of launcher.exe

Current direct callers recovered so far:

- `0x004366f0 = CLTBaseThreadPerClientTCPEngine_ctor`
  - base engine ctor calls `CLTSocketLayer::Init` before queue-thread allocation
- `0x00452270 = CLTSocketLayer_InitConnectedLoopbackWakeupSocketHandle`
  - helper used by both accept-thread and worker-thread wakeup socket setup

Related helper family:

- `0x00452270 = CLTSocketLayer_InitConnectedLoopbackWakeupSocketHandle`
  - creates a connected loopback UDP socket used as a wakeup handle
  - calls `CLTSocketLayer::Init` first
- `0x00452300 = CLTSocketLayer_DeleteWakeupSocketHandle`
  - closes the wakeup socket handle and then calls `WSACleanup`
- `0x00452320 = CLTSocketLayer_SignalWakeupSocketHandle`
  - `send(handle, "", 0, 0)` wakeup poke

So the recovered launcher flow is:

1. socket layer init for process-wide Winsock bootstrap
2. engine / thread helpers create raw sockets on top of that
3. wakeup sockets are plain socket-handle helpers, not separate virtual objects

## VTable status

No `CLTSocketLayer` instance vtable has been recovered.

Current evidence points to a static-utility-style class only:

- `0x00452e00` has no `this` pointer
- the file-string xref pass only surfaced this one function for `pcsocket.cpp`
- no ctor/dtor writing a `CLTSocketLayer` vtable has been found
- the surrounding wakeup-socket helpers operate on raw socket-handle storage, not on a vtable-backed subobject

So for now the correct answer is:

- `CLTSocketLayer` **does not currently appear to participate in a recovered vtable-backed object family**

## Source alignment

Current source was updated to route duplicated Winsock bootstrap paths through:

- `matrixstaging/runtime/src/libltnet/sys/pc/pcsocket.cpp`
- `matrixstaging/runtime/src/libltnet/sys/pc/pcsocket.h`

Current scaffold now mirrors the highest-value original `0x452e00` side effects more closely:

- `WSAStartup(MAKEWORD(2,2), ...)`
- one-time logging gate
- UDP `SO_MAX_MSG_SIZE` query
- original registry API shape using `RegOpenKeyA(... "Tcpip\\Parameters" ...)`
- `RegQueryValueExA(... "MaxUserPort" ...)`
- split logging between:
  - `MaxUserPort` path for Winsock `2.x+`
  - legacy `Max sockets` path otherwise

Current callers now use that central helper instead of each keeping a separate local `WSAStartup` cache:

- `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_auth_entry.cpp`
