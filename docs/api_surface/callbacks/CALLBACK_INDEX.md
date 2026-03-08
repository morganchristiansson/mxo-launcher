# Callback Index - SetMasterDatabase API

## Quick Reference

### Fully Documented Callbacks (15)

| Callback | Category | File | Status |
|----------|----------|------|--------|
| RegisterCallback | Registration | [registration/RegisterCallback.md](registration/RegisterCallback.md) | ✅ Complete |
| SetEventHandler | Registration | [registration/SetEventHandler.md](registration/SetEventHandler.md) | ✅ Complete |
| RegisterCallback2 | Registration | [registration/RegisterCallback2.md](registration/RegisterCallback2.md) | ✅ Complete |
| OnException | Lifecycle | [lifecycle/OnException.md](lifecycle/OnException.md) | ✅ Complete |
| OnConnect | Network | [network/OnConnect.md](network/OnConnect.md) | ✅ Complete |
| OnPacket | Network | [network/OnPacket.md](network/OnPacket.md) | ✅ Complete |
| OnDistributeMonitor | Network | [network/OnDistributeMonitor.md](network/OnDistributeMonitor.md) | ✅ Complete |
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
| Network | 3 | 12 | ⏳ 25% |
| Game | 0 | 10 | ❌ 0% |
| UI | 5 | 5 | ✅ 100% |
| Monitor | 2 | 4 | ⏳ 50% |
| **Total** | **15** | **39+** | **⏳ 38%** |

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

### Network Callbacks (3/12) ⏳

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

- **[OnDistributeMonitor](network/OnDistributeMonitor.md)** - Distributed monitor
  - Status: ✅ Fully documented
  - String: "Delete callback for distribute monitor id %d\n" (0x6289f580)

- **OnDisconnect** - Connection closed
  - Status: ❌ Not documented

- **OnPacketSend** - Packet sent
  - Status: ❌ Not documented

- **OnConnectionError** - Connection error
  - Status: ❌ Not documented

- **OnTimeout** - Connection timeout
  - Status: ❌ Not documented

- **OnMonitorEvent** - Monitor event
  - Status: ❌ Not documented

- **OnClientIPRequest** - Client IP request
  - Status: ❌ Not documented

- **OnClientIPReply** - Client IP response
  - Status: ❌ Not documented

- **OnSessionPenalty** - Session penalty
  - Status: ❌ Not documented

- **OnTransSession** - Transaction session
  - Status: ❌ Not documented

### Game Callbacks (0/10) ❌

Event callbacks for game logic:

- **OnPlayerJoin** - Player joined
  - Status: ❌ Not documented

- **OnPlayerLeave** - Player left
  - Status: ❌ Not documented

- **OnPlayerUpdate** - Player state update
  - Status: ❌ Not documented

- **OnWorldUpdate** - World state update
  - Status: ❌ Not documented

- **OnGameEvent** - Generic game event
  - Status: ❌ Not documented

- **OnGameState** - Game state change
  - Status: ❌ Not documented

- **OnLogin** - Login event
  - Status: ❌ Not documented

- **OnLoginEvent** - Login observer event
  - Status: ❌ Not documented

- **OnLoginError** - Login error
  - Status: ❌ Not documented

- **OnLogout** - Logout event
  - Status: ❌ Not documented

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

- **[OnDeleteCallback](monitor/OnDeleteCallback.md)** - Callback deletion
  - Status: ✅ Fully documented
  - String: "Delete callback - ID %d\n" (0x6289f21c)

- **[OnMonitorEvent](monitor/OnMonitorEvent.md)** - Monitor event
  - Status: ✅ Fully documented
  - Related to distributed monitor system
  - Complete structures, usage examples, and references

- **OnCapsValidation** - Capability validation
  - Status: ❌ Not documented
  - String: "One or more of the caps bits passed to the callback are incorrect." (0x6293f630)

---

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

**Last Updated**: 2025-03-08  
**Progress**: 15/39+ callbacks documented (38%)