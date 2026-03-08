# OnWorldUpdate

## Overview

**Category**: game
**Direction**: Client → Launcher (event callback)
**Purpose**: Notification callback when the game world state is updated (e.g., terrain, objects, environment changes)
**VTable Index**: N/A (event callback - registered via ProcessEvent)
**Byte Offset**: N/A
**Confidence Level**: Medium (inferred from game event patterns)

---

## Function Signature

```c
int OnWorldUpdate(WorldUpdateEvent* worldEvent, void* userData);
```

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `WorldUpdateEvent*` | worldEvent | World update event metadata |
| `void*` | userData | Additional user data/context |

---

## Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Event consumed (don't process further) |

---

## WorldUpdateEvent Structure

```c
struct WorldUpdateEvent {
    uint32_t sessionId;        // Session ID where world update occurred
    uint32_t updateType;       // Type of world update (terrain, objects, etc.)
    uint32_t data1;            // Update data field 1
    uint32_t data2;            // Update data field 2
    uint32_t data3;            // Update data field 3
    uint16_t flags;            // Event flags
};

// Size: 24 bytes
```

---

## Usage

### Registration Pattern

```c
// Register via ProcessEvent vtable (index 6, offset 0x18)
CallbackRegistration reg;
reg.eventType = EVENT_WORLD_UPDATE;
reg.callbackFunc = MyOnWorldUpdate;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->ProcessEvent(worldEvent, &reg);
```

### Assembly Pattern

```assembly
; ProcessEvent vtable call for world update events
mov eax, [world_update_event]    ; Get callback function pointer
test eax, eax
je skip_callback
push worldEvent
push userData
call eax
add esp, 8
```

---

## Implementation

### Launcher Side

```c
// Example: World update handler
int MyOnWorldUpdate(WorldUpdateEvent* event, void* userData) {
    // Validate event
    if (!event || event->sessionId == 0) return -1;

    // Log world update
    printf("Session %d world updated (type: %d, data1: %d, data2: %d, data3: %d)\n", 
           event->sessionId,
           event->updateType,
           event->data1,
           event->data2,
           event->data3);

    // Update game state based on update type
    GameSession* session = GetSession(event->sessionId);
    if (session) {
        switch (event->updateType) {
            case UPDATE_TERRAIN:
                session->UpdateTerrain(event->data1, event->data2, event->data3);
                break;
            case UPDATE_OBJECTS:
                session->UpdateObjects(event->data1, event->data2, event->data3);
                break;
            case UPDATE_ENVIRONMENT:
                session->UpdateEnvironment(event->data1, event->data2, event->data3);
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
// Example: Client-side world update notification
int ClientOnWorldUpdate(WorldUpdateEvent* event, void* userData) {
    // Update local world state
    World* world = GetWorld();
    if (world) {
        world->Update(event->updateType, 
                      event->data1,
                      event->data2,
                      event->data3);
    }

    // Send update notification to launcher
    SendWorldUpdateNotification(event->sessionId, 
                                event->updateType,
                                event->data1,
                                event->data2,
                                event->data3);

    return 0;
}
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Session %d world updated (type: %d)" | Inferred | World update logging |
| "Data1: %d, Data2: %d, Data3: %d" | Inferred | Update data display |

*Note: Exact addresses to be confirmed through string search in binary.*

---

## Related Callbacks

- **[OnPlayerJoin](callbacks/game/OnPlayerJoin.md)** - Player arrival
- **[OnPlayerLeave](callbacks/game/OnPlayerLeave.md)** - Player departure
- **[OnPlayerUpdate](callbacks/game/OnPlayerUpdate.md)** - Player state changes
- **[OnGameEvent](callbacks/game/OnGameEvent.md)** - Generic game events
- **[OnGameState](callbacks/game/OnGameState.md)** - Overall game state management

---

## References

- **Source**: Game event patterns in client_dll_callback_analysis.md Section 3.1
- **Address**: ProcessEvent vtable index 6, byte offset 0x18
- **Evidence**: Pattern matching with player-related callbacks
- **Confidence**: Medium - Inferred from game callback structure

---

## Documentation Status

**Status**: ✅ Complete (template filled)  
**Last Updated**: 2025-03-08  
**Author**: API Analyst

---

**Next Callback**: OnGameEvent