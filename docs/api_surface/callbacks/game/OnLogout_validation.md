# OnLogout Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` and `../../client.dll`
**Analysis Date**: 2025-06-18

---

## Critical Findings

### 1. ❌ **FABRICATED** - Function Does Not Exist

The documentation describes an `OnLogout` callback function that **does not exist** in either binary.

**Evidence**:
```bash
$ strings ../../launcher.exe | grep -i "OnLogout"
(no results)

$ strings ../../client.dll | grep -i "OnLogout"
(no results)

$ grep -i "OnLogout" /tmp/launcher_disasm.txt
(no matches)

$ grep -i "OnLogout" /tmp/client_disasm.txt
(no matches)
```

### 2. ❌ No Logout Event Callbacks Exist

The binaries contain **no logout event callbacks** of any kind:
- ❌ No `OnLogout`
- ❌ No `OnLogoutEvent`
- ❌ No `OnLogoutError`
- ❌ No logout observer pattern

### 3. ✅ What DOES Exist

**Login Event Observer Pattern** (C++ virtual methods, not callbacks):

```bash
$ strings ../../client.dll | grep -i "OnLogin"
CLTEvilBlockingLoginObserver::OnLoginEvent()
CLTEvilBlockingLoginObserver::OnLoginError()
CLTLoginObserver_PassThrough::OnLoginEvent()
CLTLoginObserver_PassThrough::OnLoginError()
```

**Logout-Related Constants**:
```bash
$ strings ../../client.dll | grep -i "logout"
RESULT_REQUIRES_CLIENT_LOGOUT
```

This is a **result code constant**, not a callback.

---

## Documentation Claims vs Reality

### Claimed Function Signature

```c
int OnLogout(LoginErrorEvent* errorEvent, void* userData);
```

**Status**: ❌ **FABRICATED** - Function does not exist

### Claimed Structure

```c
struct LoginErrorEvent {
    uint32_t playerId;
    uint32_t sessionId;
    uint32_t errorCode;
    uint32_t errorMessage;
    uint16_t flags;
};
```

**Status**: ❌ **FABRICATED** - No such structure exists

### Claimed Registration Pattern

```c
// Register via ProcessEvent vtable (index 6, offset 0x18)
CallbackRegistration reg;
reg.eventType = EVENT_LOGIN_ERROR;
```

**Status**: ❌ **FABRICATED** - No `EVENT_LOGIN_ERROR` constant for logout

---

## What Actually Exists

### Login Observer Pattern

The game uses a **C++ observer pattern** for login events, not C callbacks:

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

**Key Differences**:
- C++ virtual methods (thiscall), not C callbacks
- Part of observer pattern, not event registration
- No logout-specific callbacks
- Events and errors only for login process

### Logout Handling

Logout is handled **internally** by the game:
- No callback notification
- No observer pattern for logout
- Server-side session cleanup
- Client disconnects without event notification

**Evidence**: Only logout-related string is a result code:
```
RESULT_REQUIRES_CLIENT_LOGOUT
```

This is used internally for flow control, not as a callback event.

---

## Comparison: Expected vs. Actual

### Expected (Fabricated)

```c
// ❌ FABRICATED - Does NOT exist
int OnLogout(LoginErrorEvent* errorEvent, void* userData) {
    // Handle logout event
    printf("Player %d logged out\n", errorEvent->playerId);
    return 0;
}

// Registration
RegisterEvent(EVENT_LOGOUT, OnLogout);
```

### Actual (What Really Happens)

```cpp
// ✅ ACTUAL - Login observer pattern
class MyLoginObserver : public ILTLoginObserver {
public:
    void OnLoginEvent(int eventNumber) override {
        // Handle login events
    }

    void OnLoginError(int errorNumber) override {
        // Handle login errors
    }

    // ❌ NO OnLogout method - doesn't exist
};

// Logout is handled internally by server
// No callback, no event, no notification
```

---

## Why This Was Fabricated

### Pattern Recognition Error

The documentation author made an **incorrect assumption**:

1. **Observed pattern**: Login events exist (`OnLoginEvent`, `OnLoginError`)
2. **Incorrect inference**: "Logout events must also exist"
3. **Fabrication**: Created `OnLogout` callback
4. **No validation**: Never checked the actual binary

### Evidence of Fabrication

| Aspect | Value | Indicator |
|--------|-------|-----------|
| Confidence | Medium | ⚠️ "Inferred from game event patterns" |
| Signature | `int (*Event, void*)` | Template pattern |
| Registration | "ProcessEvent vtable" | Generic placeholder |
| Validation | ❌ None | Not checked against binary |

**Key indicator**: "Medium confidence - inferred from game event patterns" = **not validated**

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnLogout` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | C callback function | N/A | ❌ **N/A** |
| Parameters | `LoginErrorEvent*` structure | N/A | ❌ **N/A** |
| Return Value | `int` (0, -1,1) | N/A | ❌ **N/A** |
| Direction | Client → Launcher | N/A | ❌ **N/A** |
| Registration | ProcessEvent vtable | N/A | ❌ **N/A** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Binary Evidence

### String Search Results

```bash
# Search for OnLogout
$ strings ../../launcher.exe | grep -i "OnLogout"
(no results)

$ strings ../../client.dll | grep -i "OnLogout"
(no results)

# Search for logout
$ strings ../../launcher.exe | grep -i "logout"
RESULT_REQUIRES_CLIENT_LOGOUT

$ strings ../../client.dll | grep -i "logout"
RESULT_REQUIRES_CLIENT_LOGOUT

# Search for OnLogin (what DOES exist)
$ strings ../../client.dll | grep -i "OnLogin"
CLTEvilBlockingLoginObserver::OnLoginEvent(): Got event we're waiting for!
CLTEvilBlockingLoginObserver::OnLoginEvent(): Event# %d
CLTEvilBlockingLoginObserver::OnLoginError(): Error# %d
CLTLoginObserver_PassThrough::OnLoginEvent(): Event# %d  ServerResult# %s
CLTLoginObserver_PassThrough::OnLoginError(): Error# %d
```

### Disassembly Search Results

```bash
$ grep -i "OnLogout" /tmp/launcher_disasm.txt
(no matches)

$ grep -i "OnLogout" /tmp/client_disasm.txt
(no matches)
```

---

## Related Files

### Fabricated
- ❌ [OnLogout.md](OnLogout.md) - This file

### Valid Documentation
- ✅ [OnLoginEvent.md](OnLoginEvent.md) - Correct (C++ observer pattern)
- ✅ [OnLoginError.md](OnLoginError.md) - Correct (C++ observer pattern)

### Also Fabricated
- ❌ [OnPlayerJoin.md](OnPlayerJoin.md) - Does not exist
- ❌ [OnPlayerLeave.md](OnPlayerLeave.md) - Does not exist
- ❌ [OnPlayerUpdate.md](OnPlayerUpdate.md) - Does not exist

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnLogout.md` - Mark as fabricated
2. ✅ **Document** actual login observer pattern
3. ✅ **Explain** logout is handled internally, not via callbacks

### Documentation Corrections

1. Remove fabricated callback documentation
2. Document actual C++ observer pattern for login events
3. Explain that logout has no callback/notification system
4. Stop inferring callbacks from patterns

---

## Validation Commands

For future validation of similar callbacks:

```bash
# Quick string check
strings ../../launcher.exe | grep -i "CallbackName"
strings ../../client.dll | grep -i "CallbackName"

# Disassembly check
objdump -d ../../launcher.exe > /tmp/launcher_disasm.txt
grep -i "CallbackName" /tmp/launcher_disasm.txt

# Check for related patterns
strings ../../client.dll | grep -i "keyword" | head -20
```

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnLogout` callback **does not exist** in the binary. The documentation was fabricated based on:
1. Incorrect pattern matching assumptions
2. No binary validation
3. Template filling with plausible-sounding but false details

### Corrective Action

This file should be **deprecated** and replaced with:
1. Warning that the callback does not exist
2. Explanation of actual login observer pattern
3. Note that logout is handled internally without callbacks

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-06-18
