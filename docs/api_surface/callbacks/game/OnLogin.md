# OnLogin

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when a player logs in to the game session
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnLogin(LoginEvent* loginEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `LoginEvent*` | loginEvent | Login event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## LoginEvent Structure

```c
struct LoginEvent {
    uint32_t playerId;         // Unique player identifier
    uint32_t sessionId;        // Session ID where login occurred
    uint32_t loginTime;        // Timestamp of login event
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
reg.eventType = EVENT_LOGIN;
reg.callbackFunc = MyOnLogin;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(loginEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for login event callbacks
mov eax, [login_event]           ; Get callback function pointer
test eax, eax
je skip_callback
push loginEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: Login handler
int MyOnLogin(LoginEvent* event, void* userData) {
    // Validate event
    if (!event || event->playerId == 0) return -1;

    // Log login event
    printf("Player %d logged in to session %d at %lu (mode: %d)\n", 
           event->playerId, 
           event->sessionId, 
           event->loginTime,
           event->gameMode);

    // Initialize game session
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->Initialize(event->gameMode, event->flags);
    }

    return 0;
}
```

### Client Side

```c
// Example: Client-side login notification
int ClientOnLogin(LoginEvent* event, void* userData) {
    // Update local player state
    Players* players = GetPlayers();
    if (players) {
        players->Login(event->playerId, 
                       event->sessionId,
                       event->gameMode,
                       event->flags);
    }

    // Send login notification to launcher
    SendLoginNotification(event->playerId, 
                          event->sessionId,
                          event->gameMode,
                          event->flags);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Player %d logged in to session %d" | Inferred | Login logging |
| "Game mode: %d" | Inferred | Game mode display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnLoginEvent](callbacks/game/OnLoginEvent.md)** - Login observer event
- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnGameState](callbacks/game/OnGameState.md)** - Overall game state management

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with login-related callbacks
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnLoginEvent