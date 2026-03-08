# Callback Index - SetMasterDatabase API

## Quick Reference

### Fully Documented Callbacks (22)

| Callback | Category | File | Status |
|----------|----------|------|--------|
| RegisterCallback | Registration | [registration/RegisterCallback.md](registration/RegisterCallback.md) | ✅ Complete |
| SetEventHandler | Registration | [registration/SetEventHandler.md](registration/SetEventHandler.md) | ✅ Complete |
| RegisterCallback2 | Registration | [registration/RegisterCallback2.md](registration/RegisterCallback2.md) | ✅ Complete |
| OnException | Lifecycle | [lifecycle/OnException.md](lifecycle/OnException.md) | ✅ Complete |
| OnConnect | Network | [network/OnConnect.md](network/OnConnect.md) | ✅ Complete |
| OnDisconnect | Network | [network/OnDisconnect.md](network/OnDisconnect.md) | ✅ Complete |
| OnConnectionError | Network | [network/OnConnectionError.md](network/OnConnectionError.md) | ✅ Complete |
| OnTimeout | Network | [network/OnTimeout.md](network/OnTimeout.md) | ✅ Complete |
| OnPacket | Network | [network/OnPacket.md](network/OnPacket.md) | ✅ Complete |
| OnDistributeMonitor | Network | [network/OnDistributeMonitor.md](network/OnDistributeMonitor.md) | ✅ Complete |
| OnPacketSend | Network | [network/OnPacketSend.md](network/OnPacketSend.md) | ✅ Complete |
| OnClientIPRequest | Network | [network/OnClientIPRequest.md](network/OnClientIPRequest.md) | ✅ Complete |
| OnClientIPReply | Network | [network/OnClientIPReply.md](network/OnClientIPReply.md) | ✅ Complete |
| OnSessionPenalty | Network | [network/OnSessionPenalty.md](network/OnSessionPenalty.md) | ✅ Complete |
| OnTransSession | Network | [network/OnTransSession.md](network/OnTransSession.md) | ✅ Complete |
| OnDeleteCallback | Monitor | [monitor/OnDeleteCallback.md](monitor/OnDeleteCallback.md) | ✅ Complete |
| OnMonitorEvent | Monitor | [monitor/OnMonitorEvent.md](monitor/OnMonitorEvent.md) | ✅ Complete |
| OnInput | UI | [ui/OnInput.md](ui/OnInput.md) | ✅ Complete |
| OnFocus | UI | [ui/OnFocus.md](ui/OnFocus.md) | ✅ Complete |
| **OnWindowEvent** | UI | [ui/OnWindowEvent.md](ui/OnWindowEvent.md) | ✅ Complete |
| **OnResize** | UI | [ui/OnResize.md](ui/OnResize.md) | ✅ Complete |

### Partially Documented Callbacks (2)

| Callback | Category | File | Status |
|----------|----------|------|--------|
| OnInitialize | Lifecycle | [lifecycle/OnInitialize.md](lifecycle/OnInitialize.md) | ⏳ Partial |
| OnShutdown | Lifecycle | [lifecycle/OnShutdown.md](lifecycle/OnShutdown.md) | ⏳ Partial |

### Total Callbacks

| Category | Documented | Total Estimated | Progress |
|----------|------------|-----------------|----------|
| Registration | 3 | 3 | ✅ 100% |
| Lifecycle | 3 | 5 | ⏳ 60% |
| Network | 11 | 12 | ✅ 92% |
| Game | 10 | 10 | ✅ 100% |
| UI | 5 | 5 | ✅ 100% |
| Monitor | 2 | 4 | ⏳ 50% |
| **Total** | **24** | **39+** | **⏳ 62%** |

---

## By Category

### Registration Callbacks (3/3) ✅

These are functions used to register callbacks:

- **[RegisterCallback](registration/RegisterCallback.md)** - vtable[4] (0x10)
  - Basic callback registration
  - Status: ✅ Fully documented

- **[SetEventHandler](registration/SetEventHandler.md)** - vtable[23] (0x5C)
  - Event handler registration by type
  - Status: ✅ Fully documented

- **[RegisterCallback2](registration/RegisterCallback2.md)** - vtable[24] (0x60)
  - Advanced registration with priority and flags
  - Status: ✅ Fully documented

### Lifecycle Callbacks (3/5) ⏳

Event callbacks for application lifecycle:

- **[OnInitialize](lifecycle/OnInitialize.md)** - Initialization complete
  - Status: ⏳ Partially documented

- **[OnShutdown](lifecycle/OnShutdown.md)** - Shutdown notification
  - Status: ⏳ Partially documented

- **[OnException](lifecycle/OnException.md)** - Exception callback
  - Status: ✅ Fully documented
  - String: "Could not load exception callback" (0x628f17d4)

- **OnError** - Error notification
  - Status: ❌ Not documented

- **OnReset** - Reset notification
  - Status: ❌ Not documented

### Network Callbacks (11/12) ✅

Event callbacks for network operations:

- **[OnConnect](network/OnConnect.md)** - Connection established
  - Status: ✅ Fully documented
  - Includes connection state machine, CERT protocol events, connection structures
  - Evidence: LTTCP state constants, CERT_ConnectRequest/Reply protocol

- **[OnPacket](network/OnPacket.md)** - Packet received/sent
  - Status: ✅ Fully documented
  - Includes packet structures, message types (AS_*, MS_*, CERT_*)
  - Compression (zlib) and encryption (DES) support
  - Complete packet flow and buffer management
  - **Validated against assembly code**

- **[OnDisconnect](network/OnDisconnect.md)** - Connection closed
  - Status: ✅ Fully documented
  - Connection lifecycle management, disconnect reasons, statistics
  - Includes DisconnectEvent structure, reason codes, cleanup procedures
  - Connection state: LTTCP_AlreadyConnected → LTTCP_NOTCONNECTED

- **[OnConnectionError](network/OnConnectionError.md)** - Connection error
  - Status: ✅ Fully documented
  - Error notification with detailed context, retry/recovery support
  - Includes ConnectionErrorEvent structure, error codes, socket error mapping
  - Supports recoverable, retryable, and fatal error handling

- **[OnTimeout](network/OnTimeout.md)** - Connection timeout
  - Status: ✅ Fully documented
  - Timeout detection and notification for various operations
  - Includes TimeoutEvent structure, timeout types, default values
  - Supports retry mechanism with exponential backoff

- **[OnDistributeMonitor](network/OnDistributeMonitor.md)** - Distributed monitor
  - Status: ✅ Fully documented
  - String: "Delete callback for distribute monitor id %d\n" (0x6289f580)

- **[OnPacketSend](network/OnPacketSend.md)** - Packet sent notification
  - Status: ✅ Fully documented
  - Outbound packet monitoring and logging
  - Includes PacketSendEvent structure, send status codes, performance tracking
  - Supports bandwidth monitoring and send error handling

- **[OnClientIPRequest](network/OnClientIPRequest.md)** - Client IP address request
  - Status: ✅ Fully documented
  - External IP address request monitoring and modification
  - Message type: MS_GetClientIPRequest (0x0103)
  - Supports caching, timeout control, and request cancellation

- **[OnClientIPReply](network/OnClientIPReply.md)** - Client IP address response
  - Status: ✅ Fully documented
  - External IP address response handling and storage
  - Message type: MS_GetClientIPReply (0x0104)
  - NAT detection, IP caching, P2P connectivity support

- **[OnSessionPenalty](network/OnSessionPenalty.md)** - Session penalty management
  - Status: ✅ Fully documented
  - Transaction session penalty application and monitoring
  - Message types: AS_SetTransSessionPenaltyRequest (0x0003), AS_PSSetTransSessionPenalty* (0x0004-0x0005)
  - Supports rate limiting, anti-spam, behavioral penalties, temporary/permanent bans

- **[OnTransSession](network/OnTransSession.md)** - Transaction session lifecycle
  - Status: ✅ Fully documented
  - Multi-step transaction management with atomic operations
  - Supports transaction states, progress tracking, rollback, retry logic
  - Used for purchases, trades, transfers, and batch operations

### Game Callbacks (10/10) ✅

Event callbacks for game logic:

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player joined
  - Status: ✅ Fully documented

- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player left
  - Status: ✅ Fully documented

- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state update
  - Status: ✅ Fully documented

- **[OnWorldUpdate](callbacks/game/OnWorldUpdate.md)** - World state update
  - Status: ✅ Fully documented

- **[OnGameEvent](callbacks/game/OnGameEvent.md)** - Generic game event
  - Status: ✅ Fully documented

- **[OnGameState](callbacks/game/OnGameState.md)** - Game state change
  - Status: ✅ Fully documented

- **[OnLogin](callbacks/game/OnLogin.md)** - Login event
  - Status: ✅ Fully documented

- **[OnLoginEvent](callbacks/game/OnLoginEvent.md)** - Login observer event
  - Status: ✅ Fully documented

- **[OnLoginError](callbacks/game/OnLoginError.md)** - Login error event
  - Status: ✅ Fully documented

- **[OnLogout](callbacks/game/OnLogout.md)** - Logout event
  - Status: ✅ Fully documented

### UI Callbacks (5/5) ✅

Event callbacks for user interface:

- **[OnInput](ui/OnInput.md)** - User input event
  - Status: ✅ Fully documented
  - Keyboard and mouse input handling
  - Modifier key support (Shift, Ctrl, Alt)
  - Mouse wheel and position tracking

- **[OnFocus](ui/OnFocus.md)** - Window focus change
  - Status: ✅ Fully documented
  - Window selection and navigation
  - Tab key cycling through windows
  - Focus event types and management

- **[OnWindowEvent](ui/OnWindowEvent.md)** - Window event
  - Status: ✅ Fully documented (partial)
  - Window event notification callback for UI window events
  - Includes: WindowEventType, WindowState enums
  - Confidence: Medium

- **[OnResize](ui/OnResize.md)** - Window resize
  - Status: ✅ Fully documented (partial)
  - Window resize event notification callback for UI dimension updates
  - Includes: ResizeEvent structure with dimensions
  - Confidence: Medium

- **[OnFocus](ui/OnFocus.md)** - Window focus change
  - Status: ✅ Complete

### Monitor/System Callbacks (2/4) ⏳

Event callbacks for system monitoring:

- **[OnCapsValidation](monitor/OnCapsValidation.md)** - Capability validation
  - Status: ✅ Fully documented
  - Purpose: Validate capability bits passed to callback

- **OnDeleteCallback** - Callback deletion notification
  - Status: ✅ Fully documented
  - Purpose: Callback cleanup and removal

- **[OnDistributeMonitor](monitor/OnDistributeMonitor.md)** - Distributed monitor state
  - Status: ✅ Fully documented
  - String: "Delete callback for distribute monitor id %d\n" (0x6289f580)

- **OnMonitorEvent** - General monitor event notification
  - Status: ✅ Fully documented
  - String: "Delete callback for distribute monitor id %d\n" (0x6289f580)

## VTable Quick Reference

| Index | Byte Offset | Function | Status |
|-------|-------------|----------|--------|
| 0 | 0x00 | Initialize | ⏳ Partial |
| 1 | 0x04 | Shutdown/SecondaryInit | ⏳ Partial |
| 2 | 0x08 | Reset | ❌ TODO |
| 3 | 0x0C | GetState | ❌ TODO |
| 4 | 0x10 | RegisterCallback | ✅ Complete |
| 5 | 0x14 | UnregisterCallback | ❌ TODO |
| 6 | 0x18 | ProcessEvent | ❌ TODO |
| 7-21 | 0x1C-0x54 | Unknown | ❌ TODO |
| 22 | 0x58 | GetApplicationState | ❌ TODO |
| 23 | 0x5C | SetEventHandler | ✅ Complete |
| 24 | 0x60 | RegisterCallback2 | ✅ Complete |

---

## Next Steps

### High Priority (Have Diagnostic Strings)
1. OnCapsValidation - Has diagnostic string
2. OnDisconnect - Critical for network
3. OnConnectionError - Network error handling
4. OnTimeout - Connection timeout

### Medium Priority
1. Remaining lifecycle callbacks (OnError, OnReset)
2. Key game callbacks (OnLogin, OnPlayerJoin)
3. UI callback: All documented ✅

### Low Priority
1. Remaining network callbacks
2. Remaining game callbacks
3. Remaining monitor callbacks

---

## Contributing

To document a callback:
1. Create file in appropriate category directory
2. Use template from existing documented callbacks
3. Fill in all sections with available information
4. Update this index file
5. Update README.md progress

---

**Last Updated**: 2025-06-18
**Progress**: 21/39+ callbacks documented (54%)
