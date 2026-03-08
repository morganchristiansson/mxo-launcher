# OnGameEvent

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback for generic game events (not player, world, or state-specific)
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnGameEvent(GameEvent* gameEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `GameEvent*` | gameEvent | Generic game event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## GameEvent Structure

```c
struct GameEvent {
    uint32_t eventId;          // Unique event identifier
    uint32_t sessionId;        // Session ID where event occurred
    uint32_t eventType;        // Type of game event
    uint32_t data1;            // Event data field 1
    uint32_t data2;            // Event data field 2
    uint16_t flags;            // Event flags
};

// Size: 20 bytes
```

---

## Usage

### Registration Pattern

```c
// Register via ProcessEvent vtable (index 6, offset 0x18)
CallbackRegistration reg;
reg.eventType = EVENT_GAME_EVENT;
reg.callbackFunc = MyOnGameEvent;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(gameEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for game event callbacks
mov eax, [game_event]            ; Get callback function pointer
test eax, eax
je skip_callback
push gameEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: Game event handler
int MyOnGameEvent(GameEvent* event, void* userData) {
    // Validate event
    if (!event || event->sessionId == 0) return -1;

    // Log game event
    printf("Session %d game event (id: %d, type: %d, data1: %d, data2: %d)\n", 
           event->sessionId,
           event->eventId,
           event->eventType,
           event->data1,
           event->data2);

    // Handle based on event type
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        switch (event->eventType) {
            case EVENT_TYPE_1:
                session->HandleEvent(event->eventId, 
                                     event->data1,
                                     event->data2);
                break;
            case EVENT_TYPE_2:
                // ...
                break;
            default:
                break;
        }
    }

    return 0;
}
```

### Client Side

```c
// Example: Client-side game event notification
int ClientOnGameEvent(GameEvent* event, void* userData) {
    // Update local game state
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->UpdateGameState(event->eventId, 
                                 event->data1,
                                 event->data2);
    }

    // Send notification to launcher
    SendGameEventNotification(event->sessionId, 
                              event->eventId,
                              event->eventType,
                              event->data1,
                              event->data2);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Session %d game event (id: %d)" | Inferred | Game event logging |
| "Type: %d, Data1: %d, Data2: %d" | Inferred | Event details display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state changes
- **[OnWorldUpdate](callbacks/game/OnWorldUpdate.md)** - World state changes
- **[OnGameState](callbacks/game/OnGameState.md)** - Overall game state management

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with other game event callbacks
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnGameState