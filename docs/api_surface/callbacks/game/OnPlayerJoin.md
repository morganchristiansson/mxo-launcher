# OnPlayerJoin

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when a player joins the game session
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnPlayerJoin(PlayerJoinEvent* playerEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `PlayerJoinEvent*` | playerEvent | Player join event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## PlayerJoinEvent Structure

```c
struct PlayerJoinEvent {
    uint32_t playerId;         // Unique player identifier
    uint32_t sessionId;        // Session ID where player joined
    uint32_t joinTime;         // Timestamp of join event
    uint32_t gameMode;         // Game mode (e.g., survival, creative)
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
reg.eventType = EVENT_PLAYER_JOIN;
reg.callbackFunc = MyOnPlayerJoin;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(playerEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for player join events
mov eax, [player_join_event]    ; Get callback function pointer
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
// Example: Player join handler
int MyOnPlayerJoin(PlayerJoinEvent* event, void* userData) {
    // Validate event
    if (!event || event->playerId == 0) return -1;

    // Log player join
    printf("Player %d joined session %d at %lu\n", 
           event->playerId, 
           event->sessionId, 
           event->joinTime);

    // Notify game logic
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->AddPlayer(event->playerId);
    }

    return 0;
}
```

### Client Side

```c
// Example: Client-side player join notification
int ClientOnPlayerJoin(PlayerJoinEvent* event, void* userData) {
    // Update local state
    Players* players = GetPlayers();
    if (players) {
        players->AddPlayer(event->playerId, 
                           event->gameMode,
                           event->flags);
    }

    // Send join notification to launcher
    SendJoinNotification(event->playerId, event->sessionId);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Player %d joined session %d" | Inferred | Player join logging |
| "Game mode: %d" | Inferred | Game mode identification |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Opposite event (player departure)
- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state changes
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

**Next Callback**: OnPlayerLeave