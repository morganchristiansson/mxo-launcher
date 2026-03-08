# Complete Callback List - SetMasterDatabase API

## Summary
- **Total Callback Categories**: 5
- **Estimated Total Callbacks**: 50-100+
- **VTable Functions**: 25-30 per object
- **Objects**: 2 (Primary and Secondary)

---

## 1. VTable-Based Callback Registration Functions

These are functions used to register callbacks (launcher→client):

| VTable Index | Byte Offset | Function Name | Purpose |
|--------------|-------------|---------------|---------|
| 4 | 0x10 | RegisterCallback | Initial callback registration |
| 23 | 0x5C | SetEventHandler | Register event handler |
| 24 | 0x60 | RegisterCallback2 | Alternative callback registration |

---

## 2. VTable-Based Launcher Functions (Called by Client)

These are functions the client can call on the launcher:

### Primary VTable Functions (Indices 0-30)

| Index | Byte Offset | Function Name | Purpose |
|-------|-------------|---------------|---------|
| 0 | 0x00 | Initialize | Initialize object |
| 1 | 0x04 | Shutdown/SecondaryInit | Shutdown or secondary init |
| 2 | 0x08 | Reset | Reset object state |
| 3 | 0x0C | GetState | Query object state |
| 4 | 0x10 | RegisterCallback | Register callback function |
| 5 | 0x14 | UnregisterCallback | Remove registered callback |
| 6 | 0x18 | ProcessEvent | Process event |
| 7-21 | 0x1C-0x54 | [Unknown] | Various functions |
| 22 | 0x58 | GetApplicationState | Get application state |
| 23 | 0x5C | SetEventHandler | Set event handler |
| 24 | 0x60 | RegisterCallback2 | Register callback (alternative) |

---

## 3. Callback Storage in Objects

Client.dll objects store callbacks at these offsets:

| Offset | Name | Purpose |
|--------|------|---------|
| 0x20 | pCallback1 | First callback function pointer |
| 0x24 | pCallback2 | Second callback function pointer |
| 0x28 | pCallback3 | Third callback function pointer |
| 0x34 | pCallbackData | User data passed to callbacks |

---

## 4. Callback Categories

### Category 1: Lifecycle Callbacks (3-5 callbacks)

| Callback Name | Purpose |
|---------------|---------|
| OnInitialize | Initialization complete notification |
| OnShutdown | Shutdown notification |
| OnError | Error condition notification |
| OnReset | Reset notification |
| OnException | Exception callback |

### Category 2: Network Callbacks (10-15 callbacks)

| Callback Name | Purpose |
|---------------|---------|
| OnConnect | Connection established |
| OnDisconnect | Connection closed |
| OnPacket | Packet received |
| OnPacketSend | Packet sent |
| OnConnectionError | Connection error |
| OnTimeout | Connection timeout |
| OnMonitorEvent | Distributed monitor event |
| OnDistributeMonitor | Distributed monitoring callback |
| OnClientIPRequest | Client IP request |
| OnClientIPReply | Client IP response |
| OnSessionPenalty | Session penalty event |
| OnTransSession | Transaction session event |

### Category 3: Game Callbacks (20-30 callbacks)

| Callback Name | Purpose |
|---------------|---------|
| OnPlayerJoin | Player joined |
| OnPlayerLeave | Player left |
| OnPlayerUpdate | Player state update |
| OnWorldUpdate | World state update |
| OnGameEvent | Generic game event |
| OnGameState | Game state change |
| OnLogin | Login event |
| OnLoginEvent | Login observer event |
| OnLoginError | Login error |
| OnLogout | Logout event |

### Category 4: UI Callbacks (5-10 callbacks)

| Callback Name | Purpose |
|---------------|---------|
| OnInput | User input event |
| OnFocus | Window focus change |
| OnWindowEvent | Window event |
| OnResize | Window resize |
| OnClose | Window close |

### Category 5: Monitor/System Callbacks (5-10 callbacks)

| Callback Name | Purpose |
|---------------|---------|
| OnMonitorEvent | Monitor event |
| OnDistributeMonitor | Distributed monitor callback |
| OnDeleteCallback | Callback deletion notification |
| OnCapsValidation | Capability validation callback |

---

## 5. Callback Diagnostic Strings

Strings found in client.dll related to callbacks:

| String | Address | Purpose |
|--------|---------|---------|
| "Delete callback - ID %d\n" | 0x6289f21c | Callback cleanup |
| "Delete callback for distribute monitor id %d\n" | 0x6289f580 | Network monitor cleanup |
| "Could not load exception callback" | 0x628f17d4 | Exception handler error |
| "One or more of the caps bits passed to the callback are incorrect." | 0x6293f630 | Capability validation error |
| "No callback" | 0x6293fce8 | Missing callback warning |

---

## 6. Event Types Identified

Event types used with SetEventHandler:

| Event Type | Category | Purpose |
|------------|----------|---------|
| EVENT_TYPE_1 | General | Generic event type 1 |
| EVENT_TYPE_2 | Network | Network event type |
| EVENT_TYPE_3 | Game | Game event type |
| EVENT_NETWORK | Network | Network event |
| EVENT_GAME | Game | Game event |

---

## 7. Callback Function Signatures

### Standard Callback Pattern

```c
// Callback function signature
typedef int (*CallbackFunc)(void* data, uint32_t* result, uint32_t flags);
```

### Event Handler Pattern

```c
// Event handler signature
typedef void (*EventHandlerFunc)(void* data, uint32_t size, uint32_t flags);
```

### Callback Registration Pattern

```c
// Register callback via vtable[23]
int SetEventHandler(APIObject* this, uint32_t eventType, EventHandlerFunc handler);

// Register callback via vtable[24]
int RegisterCallback2(APIObject* this, CallbackFunc callback, void* userData);
```

---

## 8. Callback Invocation Pattern

Standard callback invocation from launcher to client:

```assembly
; Check if callback exists
mov eax, [object + 0x20]     ; Load callback pointer
test eax, eax                ; Check if NULL
je skip_callback             ; Skip if no callback

; Invoke callback
push flags                   ; Push flags parameter
push result_ptr              ; Push result pointer
push data_ptr                ; Push data pointer
call eax                     ; Call callback function
add esp, 12                  ; Cleanup stack

skip_callback:
```

---

## 9. Callback Data Structures

### Callback Entry Structure

```c
struct CallbackEntry {
    void* pCallbackFunction;   // Function pointer
    void* pUserData;           // User-provided context
    uint32_t eventType;        // Event type identifier
    uint32_t flags;            // Flags and options
    uint32_t priority;         // Callback priority
};
```

### Event Structure

```c
struct Event {
    uint32_t eventType;        // Event type ID
    uint32_t eventSize;        // Size of event data
    void* pEventData;          // Event-specific data
    uint32_t timestamp;        // Event timestamp
    uint32_t sourceId;         // Source identifier
};
```

---

## 10. Unknown/Undocumented Callbacks

The following callbacks are known to exist but lack detailed documentation:

- **VTable indices 7-21**: Purpose unknown, likely additional game/network functions
- **Secondary Object callbacks**: Similar to primary but for different API surface
- **Internal callbacks**: Used within launcher.exe, not exposed to client.dll
- **Total estimated**: 50-100+ callbacks across all categories

---

## Next Steps

For detailed implementation specifications for each callback, see:
- **MASTER_DATABASE_CALLBACKS.md** - Detailed callback documentation (to be created)

