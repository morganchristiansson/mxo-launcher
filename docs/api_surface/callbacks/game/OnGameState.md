# OnGameState

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when the overall game state changes (e.g., starting, pausing, ending)
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnGameState(GameStateEvent* gameStateEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `GameStateEvent*` | gameStateEvent | Game state change event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## GameStateEvent Structure

```c
struct GameStateEvent {
    uint32_t sessionId;        // Session ID where state change occurred
    uint32_t stateType;        // Type of game state (START, PAUSE, END, etc.)
    uint32_t data1;            // State change data field 1
    uint32_t data2;            // State change data field 2
    uint16_t flags;            // Event flags
};

// Size: 16 bytes
```

---

## Usage

### Registration Pattern

```c
// Register via ProcessEvent vtable (index 6, offset 0x18)
CallbackRegistration reg;
reg.eventType = EVENT_GAME_STATE;
reg.callbackFunc = MyOnGameState;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(gameStateEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for game state event callbacks
mov eax, [game_state_event]      ; Get callback function pointer
test eax, eax
je skip_callback
push gameStateEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: Game state handler
int MyOnGameState(GameStateEvent* event, void* userData) {
    // Validate event
    if (!event || event->sessionId == 0) return -1;

    // Log game state change
    printf("Session %d state changed (type: %d, data1: %d, data2: %d)\n", 
           event->sessionId,
           event->stateType,
           event->data1,
           event->data2);

    // Handle based on state type
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        switch (event->stateType) {
            case STATE_START:
                session->StartGame(event->data1, event->data2);
                break;
            case STATE_PAUSE:
                session->PauseGame(event->data1, event->data2);
                break;
            case STATE_END:
                session->EndGame(event->data1, event->data2);
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
// Example: Client-side game state notification
int ClientOnGameState(GameStateEvent* event, void* userData) {
    // Update local game state
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->UpdateGameState(event->stateType, 
                                 event->data1,
                                 event->data2);
    }

    // Send notification to launcher
    SendGameStateNotification(event->sessionId, 
                              event->stateType,
                              event->data1,
                              event->data2);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Session %d state changed (type: %d)" | Inferred | Game state logging |
| "State: %d, Data1: %d, Data2: %d" | Inferred | State details display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state changes
- **[OnWorldUpdate](callbacks/game/OnWorldUpdate.md)** - World state changes
- **[OnGameEvent](callbacks/game/OnGameEvent.md)** - Generic game events

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with other game state-related callbacks
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnLogin