# OnLogin Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **NO EVIDENCE** - Function Does Not Exist

**Searched for**: `OnLogin` (excluding `OnLoginEvent` and `OnLoginError`)

**Result**: ❌ **NOT FOUND IN BINARY**

```bash
$ strings ../../launcher.exe | grep -i "OnLogin" | grep -v "OnLoginEvent\|OnLoginError"
(no output)
```

### 2. **FABRICATED** - Documentation Based on Incorrect Inference

The `OnLogin` callback appears to be **completely fabricated** based on pattern matching assumptions. There is no such function in the binary.

---

## What Actually Exists

The login system uses a different architecture:

### LaunchPad System

The binary contains a LaunchPad client system with these messages:

```
LaunchPad login successful.
LaunchPadClient received result %d (%s) on login request
LaunchPadClient received error %d (%s) on login request
```

**Source files** (from debug strings):
- `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`
- `\matrixstaging\game\src\libltclientlogin\loginmediator.cpp`
- `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`

### Login Observer Pattern (Actual)

The binary implements an **observer pattern** with:
- `CLTLoginObserver_PassThrough::OnLoginEvent()` - handles login events
- `CLTLoginObserver_PassThrough::OnLoginError()` - handles login errors
- `CLTLoginMediator` - coordinates login flow

---

## What the Documentation Claims vs Reality

| Documented Claim | Reality | Status |
|------------------|---------|--------|
| `OnLogin` callback function | Does not exist | ❌ **FABRICATED** |
| `LoginEvent` structure | No such structure found | ❌ **FABRICATED** |
| Event-based callback | Observer pattern used | ❌ **WRONG** |
| 16-byte structure parameter | Integer event numbers | ❌ **WRONG** |

---

## Possible Confusion

The documentation author may have confused:

1. **`OnLoginEvent`** (exists) ↔ **`OnLogin`** (does not exist)
2. **Observer pattern events** ↔ **Simple callbacks**
3. **C++ virtual methods** ↔ **C function pointers**

---

## Correct Login Flow

Based on binary analysis:

```
LaunchPadClient
    ↓
CLTLoginMediator
    ↓ (PostEvent/PostError)
ILTLoginObserver implementations
    ↓
OnLoginEvent(int eventNumber) or OnLoginError(int errorNumber)
```

---

## Validation Summary

| Aspect | Status |
|--------|--------|
| Function existence | ❌ **NOT FOUND** |
| Function signature | ❌ **N/A** |
| Parameters | ❌ **N/A** |
| Return value | ❌ **N/A** |
| Documentation accuracy | ❌ **COMPLETELY FABRICATED** |

**Overall Status**: ❌ **ENTIRE DOCUMENTATION INVALID - FUNCTION DOES NOT EXIST**

---

## Recommendations

1. **Delete this documentation** - function does not exist
2. **Rename to match reality** - if documenting login flow, use `OnLoginEvent`
3. **Document LaunchPad system** instead
4. **Document CLTLoginMediator** and observer pattern
5. **Avoid fabricating** callbacks based on assumptions

---

## Actual Login-Related Functions Found

| Function/Class | Type | Purpose |
|----------------|------|---------|
| `CLTLoginMediator` | Class | Login state machine coordinator |
| `CLTLoginObserver_PassThrough` | Class | Pass-through event handler |
| `CLTEvilBlockingLoginObserver` | Class | Blocking event handler (test/debug) |
| `OnLoginEvent` | Method | Handle login events |
| `OnLoginError` | Method | Handle login errors |
| `LaunchPadClient` | Class | SOE LaunchPad integration |

**Note**: There is NO standalone `OnLogin` callback function.
