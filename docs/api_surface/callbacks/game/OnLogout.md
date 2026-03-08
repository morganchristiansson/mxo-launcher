# OnLogout

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-06-18
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in either launcher.exe or client.dll.
>
> **Verification**:
> ```bash
> strings ../../launcher.exe | grep -i "OnLogout"     # No results
> strings ../../client.dll | grep -i "OnLogout"      # No results
> grep -i "OnLogout" /tmp/launcher_disasm.txt        # No matches
> ```
>
> See [OnLogout_validation.md](OnLogout_validation.md) for detailed validation report.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of both `launcher.exe` and `client.dll` confirms:

```bash
# String search
$ strings ../../launcher.exe | grep -i "OnLogout"
(no results)

$ strings ../../client.dll | grep -i "OnLogout"
(no results)

# Disassembly search
$ grep -i "OnLogout" /tmp/launcher_disasm.txt
(no matches)

$ grep -i "OnLogout" /tmp/client_disasm.txt
(no matches)

# Only logout-related string found
$ strings ../../client.dll | grep -i "logout"
RESULT_REQUIRES_CLIENT_LOGOUT  # This is a result code, not a callback
```

### ✅ What Actually Exists

**Login Observer Pattern** (C++ virtual methods):

The game uses a C++ observer pattern for login events, not C callbacks:

```cpp
// Actual interface (C++ virtual methods)
class ILTLoginObserver {
public:
    virtual void OnLoginEvent(int eventNumber) = 0;
    virtual void OnLoginError(int errorNumber) = 0;
};

// Implementations found in binary:
CLTEvilBlockingLoginObserver::OnLoginEvent(int eventNumber);
CLTEvilBlockingLoginObserver::OnLoginError(int errorNumber);
CLTLoginObserver_PassThrough::OnLoginEvent(int eventNumber, const char* serverResult);
CLTLoginObserver_PassThrough::OnLoginError(int errorNumber);
```

**Key Points**:
- ✅ Login events use observer pattern
- ✅ C++ virtual methods (thiscall), not C callbacks
- ❌ NO logout callbacks or observers exist
- ❌ Logout is handled internally without notification

---

## Original Documentation (FABRICATED)

<details>
<summary>Click to view original fabricated documentation</summary>

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

**Note**: This was fabricated. No such function exists.

</details>

---

## Correct Information

### Login Event System

The binary implements a **login observer pattern** with these methods:

| Method | Type | Purpose |
|--------|------|---------|
| `OnLoginEvent(int)` | C++ virtual method | Handle login events |
| `OnLoginError(int)` | C++ virtual method | Handle login errors |

**Documentation**:
- ✅ [OnLoginEvent.md](OnLoginEvent.md) - Correct, validated
- ✅ [OnLoginError.md](OnLoginError.md) - Correct, validated

### Logout Handling

**Logout is NOT exposed via callbacks or observers**:
- Handled internally by server
- No notification to client.dll
- No event system for logout
- Session cleanup is silent

### Actual Logout-Related Code

The only logout-related code found:
```
RESULT_REQUIRES_CLIENT_LOGOUT
```

This is a **result code constant** used for internal flow control, not a callback event.

---

## Fabrication Pattern

This callback follows the same fabrication pattern as the player callbacks:

| Aspect | Value | Indicator |
|--------|-------|-----------|
| Confidence | Medium | ⚠️ "Inferred from game event patterns" |
| Validation | ❌ None | Not checked against binary |
| Template | Generic callback signature | Plausible but false |

**Pattern**: "Login callbacks exist → Logout callbacks must exist" ❌ **WRONG**

---

## Related Files

**Also Fabricated**:
- ❌ [OnPlayerJoin.md](OnPlayerJoin.md) - Does not exist
- ❌ [OnPlayerLeave.md](OnPlayerLeave.md) - Does not exist
- ❌ [OnPlayerUpdate.md](OnPlayerUpdate.md) - Does not exist
- ❌ [OnLogout.md](OnLogout.md) - This file

**Valid Documentation**:
- ✅ [OnLoginEvent.md](OnLoginEvent.md) - Correct (C++ observer pattern)
- ✅ [OnLoginError.md](OnLoginError.md) - Correct (C++ observer pattern)

**Validation Reports**:
- ✅ [OnLogout_validation.md](OnLogout_validation.md) - Detailed analysis
- ✅ [PLAYER_CALLBACKS_VALIDATION.md](PLAYER_CALLBACKS_VALIDATION.md) - Related validation

---

## References

- **Validation Report**: [OnLogout_validation.md](OnLogout_validation.md)
- **Binary**: `../../launcher.exe` and `../../client.dll`
- **Analysis Method**: String search, disassembly analysis
- **Status**: Function does not exist

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Last Updated**: 2025-06-18
**Validator**: Binary Analysis
**Action**: Do not use

---

**See Also**: [VALIDATION_SUMMARY.md](VALIDATION_SUMMARY.md) for complete validation results.
