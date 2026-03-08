# OnLoginEvent Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **INCORRECT** - Not a Simple C Callback

The documentation incorrectly describes `OnLoginEvent` as a C-style callback function. 

**Reality**: `OnLoginEvent` is a **C++ virtual member function** of observer classes:

```cpp
class CLTEvilBlockingLoginObserver {
    void OnLoginEvent(int eventNumber);
    void OnLoginError(int errorNumber);
};

class CLTLoginObserver_PassThrough {
    void OnLoginEvent(int eventNumber, const char* serverResult);
    void OnLoginError(int errorNumber);
};
```

### 2. **INCORRECT** - Function Signature

**Documented**:
```c
int OnLoginEvent(LoginObserverEvent* observerEvent, void* userData);
```

**Actual** (from disassembly at 0x41b520):
```cpp
// thiscall convention (this in ECX)
void CLTEvilBlockingLoginObserver::OnLoginEvent(int eventNumber);
```

**Assembly Evidence**:
```asm
  41b520:	push   %ebp
  41b521:	mov    %esp,%ebp
  41b523:	mov    0x4d3e54,%edx     ; Load logging context
  41b529:	push   %ebx
  41b52a:	mov    0x8(%ebp),%ebx    ; Get eventNumber parameter
  ...
  41b586:	push   %ebx              ; Push eventNumber
  41b586:	push   $0x4af494         ; "Event# %d" format string
  41b58b:	push   %edx
  41b58c:	call   0x414f70          ; Logging function
  ...
  41b610:	ret    $0x4              ; Return and clean 4 bytes
```

### 3. **INCORRECT** - Parameter Type

**Documented**: `LoginObserverEvent*` structure (16 bytes)

**Actual**: Simple `int eventNumber`

The function accepts an integer event number, not a structure. Evidence:
- Only one stack parameter at `ebp+8` 
- Used directly in logging: `"Event# %d"`
- Compared against stored event number at offset `0xc` in the object

### 4. **INCORRECT** - Return Value

**Documented**: Returns `int` (0, -1, or 1)

**Actual**: Returns `void`

Evidence from assembly:
- No value in `eax` before `ret`
- Function returns `ret $0x4` (just cleans stack)

---

## Identified Classes and Methods

### String References Found

| String | Address | Class/Method |
|--------|---------|--------------|
| `"CLTEvilBlockingLoginObserver::OnLoginEvent(): Got event we're waiting for!"` | 0xaf448 | Blocking observer |
| `"CLTEvilBlockingLoginObserver::OnLoginEvent(): Event# %d"` | 0xaf494 | Blocking observer |
| `"CLTEvilBlockingLoginObserver::OnLoginError(): Error# %d"` | 0xaf50c | Blocking observer |
| `"CLTLoginObserver_PassThrough::OnLoginEvent(): Event# %d  ServerResult# %s"` | 0xaf638 | Pass-through observer |
| `"CLTLoginObserver_PassThrough::OnLoginError(): Error# %d"` | 0xaf684 | Pass-through observer |
| `"CLTLoginMediator::PostEvent(): Event# %d"` | 0xaff80 | Mediator |
| `"CLTLoginMediator::PostError(): Error# %d"` | 0xaffac | Mediator |

---

## Observer Pattern Architecture

The login system uses the **Observer Pattern**:

```
ILTLoginMediator (interface)
        ↑
CLTLoginMediator (concrete mediator)
        |
        |--- PostEvent(int eventNumber)
        |--- PostError(int errorNumber)
        |
        v
[Observer Interface - likely ILTLoginObserver]
        ↑
        |-------------------------------------------|
        |                                           |
CLTEvilBlockingLoginObserver          CLTLoginObserver_PassThrough
        |                                           |
        +-- OnLoginEvent(int eventNum)             +-- OnLoginEvent(int eventNum, const char* result)
        +-- OnLoginError(int errorNum)             +-- OnLoginError(int errorNum)
```

---

## Correct Function Signatures

### CLTEvilBlockingLoginObserver::OnLoginEvent

```cpp
// Address: 0x0041b520
// Convention: thiscall (this in ECX)
void CLTEvilBlockingLoginObserver::OnLoginEvent(int eventNumber);

// Behavior:
// - Checks if eventNumber matches expected event (stored at this+0xc)
// - Logs: "Event# %d" and "Got event we're waiting for!"
// - Sets completion flag at this+0x10
// - Stores result at this+0x14
```

### CLTLoginObserver_PassThrough::OnLoginEvent

```cpp
// Address: ~0x0041b8xx (around 0x41b95b)
// Convention: thiscall (this in ECX)
void CLTLoginObserver_PassThrough::OnLoginEvent(int eventNumber, const char* serverResult);

// Behavior:
// - Logs: "Event# %d  ServerResult# %s"
// - Passes event through to registered handler
```

### CLTEvilBlockingLoginObserver::OnLoginError

```cpp
// Address: 0x0041b620
// Convention: thiscall (this in ECX)
void CLTEvilBlockingLoginObserver::OnLoginError(int errorNumber);

// Behavior:
// - Logs: "Error# %d"
// - Stores error number at this+0x18
// - Sets completion flag at this+0x10
```

---

## Event/Error Constants

Events and errors are identified by integer constants. Some identified from strings:

- `LTLO_NOAUTHSERVERADDR`
- `LTLO_UNEXPECTEDAUTHMSG`
- `LTAUTH_INVALIDCERTIFICATE`
- `LTAUTH_EXPIREDCERTIFICATE`
- `LTAUTH_LOGINTYPENOTACCEPTED`
- `LTAUTH_ALREADYCONNECTED`
- `LTAUTH_AUTHKEYSIGINVALID`
- `LTMS_AUTHSERVERCHARACTERDELETEFAILED`
- `RESULT_USAGE_LIMIT_DENIED_LOGIN`

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Callback Type | C callback function | C++ virtual method | ❌ **WRONG** |
| Parameters | `LoginObserverEvent*` structure | `int eventNumber` | ❌ **WRONG** |
| Return Value | `int` (0, -1, 1) | `void` | ❌ **WRONG** |
| Direction | Client → Launcher | Internal observer pattern | ❌ **WRONG** |
| Registration | ProcessEvent vtable | Observer pattern registration | ❌ **WRONG** |
| Structure Size | 16 bytes | N/A (no structure) | ❌ **WRONG** |

**Overall Status**: ❌ **DOCUMENTATION FUNDAMENTALLY INCORRECT**

---

## Recommendations

1. **Rewrite documentation** to reflect C++ observer pattern
2. **Document the observer interface** (likely `ILTLoginObserver`)
3. **Document event/error constants** with their numeric values
4. **Document the mediator class** (`CLTLoginMediator`)
5. **Remove references** to `LoginObserverEvent` structure
6. **Update related callbacks** (OnLogin, OnLoginError) similarly

---

## Next Steps

1. Identify the `ILTLoginObserver` interface definition
2. Find the vtable for `CLTLoginObserver` classes
3. Map event/error constants to their numeric values
4. Document the `CLTLoginMediator` class methods
5. Understand how observers register with the mediator

---

## Binary Evidence Locations

| Function | Address | Signature |
|----------|---------|-----------|
| `CLTEvilBlockingLoginObserver::OnLoginEvent` | 0x0041b520 | `void (int eventNum)` |
| `CLTEvilBlockingLoginObserver::OnLoginError` | 0x0041b620 | `void (int errorNum)` |
| `CLTLoginObserver_PassThrough::OnLoginEvent` | ~0x0041b8xx | `void (int eventNum, const char* result)` |
| `CLTLoginObserver_PassThrough::OnLoginError` | ~0x0041b9xx | `void (int errorNum)` |
| `CLTLoginMediator::PostEvent` | ~0x0041cfxx | `void (int eventNum)` |
| `CLTLoginMediator::PostError` | ~0x0041d0xx | `void (int errorNum)` |
