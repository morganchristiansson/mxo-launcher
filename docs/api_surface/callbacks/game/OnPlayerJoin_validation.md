# OnPlayerJoin Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Function Does Not Exist

The documentation describes an `OnPlayerJoin` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)

$ strings launcher.exe | grep -i "PlayerJoin"
(no results)

$ strings launcher.exe | grep -i "join" | grep -i player
(no results)
```

### 2. **NO PLAYER EVENT CALLBACKS EXIST**

The binary contains **no player-related event callbacks** of any kind:
- ❌ No `OnPlayerJoin`
- ❌ No `OnPlayerLeave`
- ❌ No `OnPlayerUpdate`
- ❌ No `OnPlayer*` pattern matches

**String Search Results**:
```bash
$ strings launcher.exe | grep -E "^On[A-Z]" | sort -u
(no matches for OnPlayer*)
```

The only `On*` methods found are:
- `CLTEvilBlockingLoginObserver::OnLoginEvent()`
- `CLTEvilBlockingLoginObserver::OnLoginError()`
- `CLTLoginObserver_PassThrough::OnLoginEvent()`
- `CLTLoginObserver_PassThrough::OnLoginError()`

### 3. **PLAYER COMMANDS DO EXIST** (Not Callbacks)

The binary does contain player-related **commands**, but these are not callbacks:

| String | Address | Type | Purpose |
|--------|---------|------|---------|
| `BootPlayer` | 0x4aa9ec | Command | Kick player from game |
| `SummonPlayer` | 0x4aaa10 | Command | Teleport player |
| `FreezePlayer` | 0x4aa9bc | Command | Freeze player movement |
| `KillPlayer` | 0x4aa9e0 | Command | Kill player character |
| `PacifyPlayer` | 0x4aa9ac | Command | Disable player combat |
| `SilencePlayer` | 0x4aaa20 | Command | Mute player chat |
| `SendPlayerToLoadingArea` | 0x4aaa00 | Command | Send to loading screen |

These are **admin/GM commands**, not event callbacks.

---

## Documentation Claims vs Reality

### Claimed Function Signature

```c
int OnPlayerJoin(PlayerJoinEvent* playerEvent, void* userData);
```

**Status**: ❌ **FABRICATED** - Function does not exist

### Claimed Structure

```c
struct PlayerJoinEvent {
    uint32_t playerId;
    uint32_t sessionId;
    uint32_t joinTime;
    uint32_t gameMode;
    uint16_t flags;
};
```

**Status**: ❌ **FABRICATED** - No such structure exists

### Claimed Registration Pattern

```c
// Register via ProcessEvent vtable (index 6, offset 0x18)
CallbackRegistration reg;
reg.eventType = EVENT_PLAYER_JOIN;
```

**Status**: ❌ **FABRICATED** - No `EVENT_PLAYER_JOIN` constant exists

---

## What Actually Exists

### Player Management Commands

The binary contains **server-side player management commands** (not callbacks):

```assembly
; Found in .rdata section at 0x4aa9ec
BootPlayer       ; Kick player
SummonPlayer     ; Teleport player
FreezePlayer     ; Freeze player
KillPlayer       ; Kill player
PacifyPlayer     ; Disable combat
SilencePlayer    ; Mute chat
SendPlayerToLoadingArea  ; Send to loading
```

These are **administrative commands** issued by server operators, not event callbacks triggered by player actions.

### Game Event System

The binary does have an event system, but it's limited to:

1. **World Events**:
   - `TriggerWorldEvent`
   - `UntriggerWorldEvent`
   - `ListWorldEvents`

2. **Login Events** (Observer Pattern):
   - `CLTEvilBlockingLoginObserver::OnLoginEvent()`
   - `CLTLoginObserver_PassThrough::OnLoginEvent()`

3. **Message/Connection Events**:
   - `CMessageConnection::OnOperationCompleted()`

**No player-specific events were found.**

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnPlayerJoin` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | C callback function | N/A | ❌ **N/A** |
| Parameters | `PlayerJoinEvent*` structure | N/A | ❌ **N/A** |
| Return Value | `int` (0, -1, 1) | N/A | ❌ **N/A** |
| Direction | Client → Launcher | N/A | ❌ **N/A** |
| Registration | ProcessEvent vtable | N/A | ❌ **N/A** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Root Cause Analysis

### Why This Happened

1. **Pattern Matching Assumption**: Documentation author assumed that because login callbacks exist (`OnLoginEvent`, `OnLoginError`), player callbacks must also exist
2. **No Binary Validation**: Documentation was written without checking the actual binary
3. **Template Inflation**: Template was filled with plausible-sounding but fabricated details
4. **"Medium Confidence" Flag Ignored**: The "Medium confidence - inferred from game event patterns" warning was not heeded

### Evidence of Fabrication

All three player callbacks follow the **exact same fabricated pattern**:

| File | Confidence | Source |
|------|------------|--------|
| `OnPlayerJoin.md` | Medium | "inferred from game event patterns" |
| `OnPlayerLeave.md` | Medium | "inferred from game callback structure" |
| `OnPlayerUpdate.md` | Medium | "inferred from game callback structure" |

All use identical:
- C callback signature pattern
- Similar structure definitions
- Same "ProcessEvent vtable index 6, offset 0x18" reference
- "Inferred" confidence levels

---

## Related Files

The following files have the same fabrication issue:

- ❌ **OnPlayerLeave.md** - Also fabricated
- ❌ **OnPlayerUpdate.md** - Also fabricated
- ❌ **OnWorldUpdate.md** - Needs validation
- ❌ **OnGameState.md** - Needs validation

---

## Actual Player-Related Functionality

### What Does Exist

1. **Player Management Commands** (admin tools):
   - `BootPlayer` - Kick player
   - `SummonPlayer` - Teleport
   - `FreezePlayer` - Immobilize
   - `KillPlayer` - Kill character
   - `PacifyPlayer` - Disable combat
   - `SilencePlayer` - Mute chat
   - `SendPlayerToLoadingArea` - Loading screen

2. **Session Management**:
   - Various SESSION_TYPE constants
   - Session establishment protocols
   - No player join/leave events

3. **World Events**:
   - `TriggerWorldEvent`
   - `UntriggerWorldEvent`
   - `ListWorldEvents`

### What Does NOT Exist

- ❌ Player join callbacks
- ❌ Player leave callbacks
- ❌ Player update callbacks
- ❌ Player event structures
- ❌ Player event registration
- ❌ `EVENT_PLAYER_*` constants

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnPlayerJoin.md` - Mark as fabricated
2. ✅ **DEPRECATE** `OnPlayerLeave.md` - Mark as fabricated
3. ✅ **DEPRECATE** `OnPlayerUpdate.md` - Mark as fabricated
4. ✅ **VALIDATE** `OnWorldUpdate.md` - Check if it exists
5. ✅ **VALIDATE** `OnGameState.md` - Check if it exists

### Documentation Corrections

1. **Remove all fabricated player callbacks**
2. **Document actual player commands** (admin tools)
3. **Document world event system** (actual events that exist)
4. **Stop inferring** callbacks from patterns

### Process Improvements

1. **Always validate against binary** before writing documentation
2. **Use disassembly tools** to confirm function existence
3. **Never assume** patterns imply existence
4. **Treat "inferred" confidence** as requiring validation
5. **Document what exists**, not what "should" exist

---

## Binary Evidence Summary

### Search Results

```bash
# Search for OnPlayerJoin
$ strings launcher.exe | grep -i "OnPlayerJoin"
(no results)

# Search for any OnPlayer* callbacks
$ strings launcher.exe | grep -E "^On[A-Z]" | grep -i player
(no results)

# Search for PlayerJoin
$ strings launcher.exe | grep -i "PlayerJoin"
(no results)

# Search for join + player
$ strings launcher.exe | grep -i "join" | grep -i player
(no results)

# Actual player-related strings found
$ strings launcher.exe | grep -i "player" | grep -E "(Boot|Summon|Freeze|Join|Leave|Update)"
BootPlayer
SummonPlayer
FreezePlayer
KillPlayer
PacifyPlayer
SilencePlayer
SendPlayerToLoadingArea
```

### String Addresses

| String | Address | Section | Type |
|--------|---------|---------|------|
| `BootPlayer` | 0x4aa9ec | .rdata | Command string |
| `SummonPlayer` | 0x4aaa10 | .rdata | Command string |
| `FreezePlayer` | 0x4aa9bc | .rdata | Command string |
| `KillPlayer` | 0x4aa9e0 | .rdata | Command string |
| `PacifyPlayer` | 0x4aa9ac | .rdata | Command string |
| `SilencePlayer` | 0x4aaa20 | .rdata | Command string |

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnPlayerJoin` callback **does not exist** in the binary. The documentation was fabricated based on:
1. Incorrect pattern matching assumptions
2. No binary validation
3. Template filling with plausible-sounding but false details

### Corrective Action

This file should be **deprecated** and replaced with a warning document explaining that:
1. The callback does not exist
2. Player management is handled via admin commands, not event callbacks
3. The actual game event system is limited to world events and login events

---

## Related Validation

See also:
- `OnPlayerLeave_validation.md` (same fabrication issue)
- `OnPlayerUpdate_validation.md` (same fabrication issue)
- `VALIDATION_SUMMARY.md` (overall validation summary)

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**  
**Action Required**: **DEPRECATE IMMEDIATELY**  
**Last Updated**: 2025-03-08
