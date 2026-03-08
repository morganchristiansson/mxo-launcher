# OnPlayerUpdate

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when a player's state is updated (e.g., position, health, status)
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnPlayerUpdate(PlayerUpdateEvent* playerEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `PlayerUpdateEvent*` | playerEvent | Player update event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## PlayerUpdateEvent Structure

```c
struct PlayerUpdateEvent {
    uint32_t playerId;         // Unique player identifier
    uint32_t sessionId;        // Session ID where player is located
    uint32_t updateType;       // Type of update (position, health, status, etc.)
    uint32_t data1;            // Update data field 1
    uint32_t data2;            // Update data field 2
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
reg.eventType = EVENT_PLAYER_UPDATE;
reg.callbackFunc = MyOnPlayerUpdate;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(playerEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for player update events
mov eax, [player_update_event]    ; Get callback function pointer
test eax, eax
je skip_callback
push playerEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: Player update handler
int MyOnPlayerUpdate(PlayerUpdateEvent* event, void* userData) {
    // Validate event
    if (!event || event->playerId == 0) return -1;

    // Log player update
    printf("Player %d updated (type: %d, data1: %d, data2: %d)\n", 
           event->playerId,
           event->updateType,
           event->data1,
           event->data2);

    // Update game state based on update type
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        switch (event->updateType) {
            case UPDATE_POSITION:
                session->UpdatePlayerPosition(event->playerId, 
                                             event->data1, 
                                             event->data2);
                break;
            case UPDATE_HEALTH:
                session->UpdatePlayerHealth(event->playerId, 
                                            event->data1);
                break;
            case UPDATE_STATUS:
                session->UpdatePlayerStatus(event->playerId, 
                                           event->data1);
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
// Example: Client-side player update notification
int ClientOnPlayerUpdate(PlayerUpdateEvent* event, void* userData) {
    // Update local player state
    Players* players = GetPlayers();
    if (players) {
        players->UpdatePlayer(event->playerId, 
                              event->updateType,
                              event->data1,
                              event->data2);
    }

    // Send update notification to launcher
    SendPlayerUpdateNotification(event->playerId, 
                                 event->sessionId,
                                 event->updateType,
                                 event->data1,
                                 event->data2);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Player %d updated (type: %d)" | Inferred | Player update logging |
| "Data1: %d, Data2: %d" | Inferred | Update data display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnWorldUpdate](callbacks/game/OnWorldUpdate.md)** - World state changes
- **[OnGameState](callbacks/game/OnGameState.md)** - Overall game state management

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with player join/leave callbacks
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnWorldUpdate