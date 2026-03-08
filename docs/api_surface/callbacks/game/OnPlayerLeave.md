# OnPlayerLeave

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when a player leaves the game session
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnPlayerLeave(PlayerLeaveEvent* playerEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `PlayerLeaveEvent*` | playerEvent | Player leave event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## PlayerLeaveEvent Structure

```c
struct PlayerLeaveEvent {
    uint32_t playerId;         // Unique player identifier
    uint32_t sessionId;        // Session ID where player left
    uint32_t leaveTime;        // Timestamp of leave event
    uint32_t reason;           // Reason for leaving (e.g., quit, kicked)
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
reg.eventType = EVENT_PLAYER_LEAVE;
reg.callbackFunc = MyOnPlayerLeave;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(playerEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for player leave events
mov eax, [player_leave_event]    ; Get callback function pointer
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
// Example: Player leave handler
int MyOnPlayerLeave(PlayerLeaveEvent* event, void* userData) {
    // Validate event
    if (!event || event->playerId == 0) return -1;

    // Log player leave with reason
    printf("Player %d left session %d at %lu (reason: %d)\n", 
           event->playerId, 
           event->sessionId, 
           event->leaveTime,
           event->reason);

    // Notify game logic
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->RemovePlayer(event->playerId, event->reason);
    }

    return 0;
}
```

### Client Side

```c
// Example: Client-side player leave notification
int ClientOnPlayerLeave(PlayerLeaveEvent* event, void* userData) {
    // Update local state
    Players* players = GetPlayers();
    if (players) {
        players->RemovePlayer(event->playerId, 
                              event->reason,
                              event->flags);
    }

    // Send leave notification to launcher
    SendLeaveNotification(event->playerId, event->sessionId, event->reason);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Player %d left session %d" | Inferred | Player leave logging |
| "Reason: %d" | Inferred | Leave reason display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Opposite event (player arrival)
- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state changes
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Current callback

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with player join callback
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnPlayerUpdate