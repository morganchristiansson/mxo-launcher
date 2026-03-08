# OnPlayerJoin

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
> 
> **Validation Date**: 2025-03-08
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
> 
> This callback was fabricated and does not exist in the launcher binary.
> 
> See [OnPlayerJoin_validation.md](OnPlayerJoin_validation.md) for detailed disassembly analysis.
> 
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)

$ strings launcher.exe | grep -i "PlayerJoin"
(no results)
```

### ❌ No Player Event Callbacks Exist

The binary contains **no player-related event callbacks**:
- No `OnPlayerJoin`
- No `OnPlayerLeave`
- No `OnPlayerUpdate`

### ✅ What Actually Exists

The binary does contain player **management commands** (not callbacks):

| Command | Purpose | Type |
|---------|---------|------|
| `BootPlayer` | Kick player from game | Admin command |
| `SummonPlayer` | Teleport player | Admin command |
| `FreezePlayer` | Freeze player movement | Admin command |
| `KillPlayer` | Kill player character | Admin command |
| `PacifyPlayer` | Disable player combat | Admin command |
| `SilencePlayer` | Mute player chat | Admin command |
| `SendPlayerToLoadingArea` | Send to loading screen | Admin command |

These are **server-side admin/GM commands**, not event callbacks.

---

## Original Documentation (FABRICATED)

<details>
<summary>Click to view original fabricated documentation</summary>

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

**Note**: This structure was fabricated and does not exist in the binary.

</details>

---

## Correct Information

### Player Management System

The launcher uses **admin commands** for player management, not event callbacks:

1. **Server-side commands**: Admins/GMs can execute player management commands
2. **No event system**: Player join/leave is not exposed via callbacks
3. **Session tracking**: Handled internally by server, not via API callbacks

### Actual Event Systems

The binary does have event systems for:

1. **Login Events** (Observer Pattern):
   - `CLTEvilBlockingLoginObserver::OnLoginEvent()`
   - `CLTLoginObserver_PassThrough::OnLoginEvent()`

2. **World Events**:
   - `TriggerWorldEvent`
   - `UntriggerWorldEvent`
   - `ListWorldEvents`

3. **Connection Events**:
   - `CMessageConnection::OnOperationCompleted()`

---

## Related Files

**Also Fabricated**:
- ❌ [OnPlayerLeave.md](OnPlayerLeave.md) - Does not exist
- ❌ [OnPlayerUpdate.md](OnPlayerUpdate.md) - Does not exist

**Needs Validation**:
- ⚠️ [OnWorldUpdate.md](OnWorldUpdate.md) - Validate existence
- ⚠️ [OnGameState.md](OnGameState.md) - Validate existence

**Valid Documentation**:
- ✅ [OnLoginEvent.md](OnLoginEvent.md) - Corrected (C++ observer pattern)
- ✅ [OnLoginError.md](OnLoginError.md) - Corrected (C++ observer pattern)

---

## References

- **Validation Report**: [OnPlayerJoin_validation.md](OnPlayerJoin_validation.md)
- **Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
- **Analysis Method**: String search, disassembly analysis
- **Status**: Function does not exist

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**  
**Last Updated**: 2025-03-08  
**Validator**: Binary Analysis  
**Action**: Do not use

---

**Next**: See [VALIDATION_SUMMARY.md](VALIDATION_SUMMARY.md) for complete validation results.
