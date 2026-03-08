# OnLogout

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when a player logs out of the game session
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnLogout(LoginErrorEvent* errorEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `LoginErrorEvent*` | errorEvent | Login error event metadata (represents logout as an error event) |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## LoginErrorEvent Structure

```c
struct LoginErrorEvent {
    uint32_t playerId;         // Unique player identifier
    uint32_t sessionId;        // Session ID where error occurred
    uint32_t errorCode;        // Error code from login system (includes logout codes)
    uint32_t errorMessage;     // Error message or error data
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
reg.eventType = EVENT_LOGIN_ERROR;
reg.callbackFunc = MyOnLogout;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(errorEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for login error event callbacks
mov eax, [login_error_event]     ; Get callback function pointer
test eax, eax
je skip_callback
push errorEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: Logout handler
int MyOnLogout(LoginErrorEvent* event, void* userData) {
    // Validate event
    if (!event || event->sessionId == 0) return -1;

    // Log logout event
    printf("Player %d logged out of session %d (code: %d, message: %d)\n", 
           event->playerId,
           event->sessionId,
           event->errorCode,
           event->errorMessage);

    // Notify game session to cleanup
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->HandleLogout(event->errorCode, 
                               event->errorMessage);
    }

    return 0;
}
```

### Client Side

```c
// Example: Client-side logout notification
int ClientOnLogout(LoginErrorEvent* event, void* userData) {
    // Update local error state
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        session->SetLoginError(event->errorCode, 
                               event->errorMessage);
    }

    // Send notification to launcher
    SendLogoutNotification(event->sessionId, 
                           event->errorCode,
                           event->errorMessage);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Player %d logged out of session %d (code: %d)" | Inferred | Error logging |
| "Error: %d, Message: %d" | Inferred | Error details display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnLogin](callbacks/game/OnLogin.md)** - Login event
- **[OnLoginEvent](callbacks/game/OnLoginEvent.md)** - Login observer event
- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnGameState](callbacks/game/OnGameState.md)** - Overall game state management

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with login-related callbacks; logout represented as error event
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: None - Game callbacks complete (10/10)