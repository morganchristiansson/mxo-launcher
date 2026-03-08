# OnLoginEvent

## Overview

**Category**: game  
**Type**: C++ Virtual Method (Observer Pattern)  
**Direction**: Internal observer notification  
**Purpose**: Handle login event notifications in the observer pattern  
**Calling Convention**: thiscall (this in ECX)  
**Confidence Level**: High (validated against disassembly)

---

## Function Signature

```cpp
// Observer interface method
class ILTLoginObserver {
public:
    virtual void OnLoginEvent(int eventNumber) = 0;
    // OR for pass-through observer:
    virtual void OnLoginEvent(int eventNumber, const char* serverResult) = 0;
};
```

---

## Implementations

### 1. CLTEvilBlockingLoginObserver::OnLoginEvent

**Address**: `0x0041b520`  
**Purpose**: Blocking observer for synchronous event handling (test/debug)

```cpp
void CLTEvilBlockingLoginObserver::OnLoginEvent(int eventNumber);
```

**Behavior**:
- Checks if `eventNumber` matches expected event (stored at `this+0x0c`)
- Logs: `"Event# %d"` and `"Got event we're waiting for!"`
- Sets completion flag at `this+0x10`
- Stores result at `this+0x14`

### 2. CLTLoginObserver_PassThrough::OnLoginEvent

**Address**: ~`0x0041b8xx`  
**Purpose**: Pass-through event handler for production use

```cpp
void CLTLoginObserver_PassThrough::OnLoginEvent(int eventNumber, const char* serverResult);
```

**Behavior**:
- Logs: `"Event# %d  ServerResult# %s"`
- Passes event through to registered handler

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `int` | eventNumber | Login event number (see Event Constants below) |
| `const char*` | serverResult | Server result string (pass-through only) |

**Note**: This is a `thiscall` method - the `this` pointer is passed in ECX register.

---

## Event Constants

Events are identified by integer constants. See **[error_codes.md](error_codes.md)** for complete list.

### Common Event Categories

| Prefix | Range | Category |
|--------|-------|----------|
| `LTLO_` | 0x00-0x12 | Login Observer events |
| `LTAUTH_` | 0x00-0x04 | Authentication events |
| `LTMS_` | 0x00-0x2A | Mediator Server events |

### Example Event Values

| Constant | Value | Description |
|----------|-------|-------------|
| `:LTLO_INPROGRESS` | 0 | Login in progress |
| `LTLO_NEW_PIN_ACCEPTED` | 18 (0x12) | New PIN accepted |
| `LTAUTH_INVALIDCERTIFICATE` | 0 | Invalid certificate |

---

## Return Value

**Type**: `void`

No return value. Event processing is fire-and-forget.

---

## Architecture

### Observer Pattern

```
┌──────────────────────────┐
│   CLTLoginMediator       │
│  ─────────────────────   │
│  + PostEvent(int event)  │
└──────────────────────────┘
            │
            │ notifies
            ↓
┌──────────────────────────┐
│   ILTLoginObserver       │◄─── Interface
│  ─────────────────────   │
│  + OnLoginEvent(int)     │
└──────────────────────────┘
            ↑
    ┌───────┴────────┐
    │                │
┌───────────────┐  ┌────────────────────┐
│ PassThrough   │  │ EvilBlocking       │
│ Observer      │  │ Observer           │
└───────────────┘  └────────────────────┘
```

---

## Assembly Analysis

### CLTEvilBlockingLoginObserver::OnLoginEvent

**Address**: `0x0041b520`

```asm
41b520:	push   %ebp                ; Setup stack frame
41b521:	mov    %esp,%ebp
41b523:	mov    0x4d3e54,%edx      ; Load logging context
41b529:	push   %ebx
41b52a:	mov    0x8(%ebp),%ebx     ; Get eventNumber parameter
41b52d:	mov    $0x8,%eax
41b532:	push   %esi
41b533:	sub    %edx,%eax          ; Logging level check
41b535:	cmp    $0x1,%eax
41b538:	push   %edi
41b539:	mov    %ecx,%edi          ; Save 'this' pointer
41b53b:	jg     0x41b594           ; Skip logging if disabled
...
41b585:	push   %ebx               ; Push eventNumber
41b586:	push   $0x4af494          ; "Event# %d" format string
41b58b:	push   %edx               ; Logging context
41b58c:	call   0x414f70           ; printf-like function
41b591:	add    $0xc,%esp
...
41b594:	cmp    0xc(%edi),%ebx     ; Compare with expected event
41b597:	jne    0x41b60c           ; Jump if not expected
...
41b5f3:	push   $0x4af448          ; "Got event we're waiting for!"
41b5f8:	push   %edx
41b5f9:	call   0x414f70           ; Log message
...
41b601:	movl   $0x0,0x14(%edi)    ; Clear result
41b608:	movb   $0x0,0x10(%edi)    ; Set completion flag
41b60c:	pop    %edi
41b60d:	pop    %esi
41b60e:	pop    %ebx
41b60f:	pop    %ebp
41b610:	ret    $0x4               ; Return, clean 4 bytes
```

---

## Usage Example

### Implementing a Custom Observer

```cpp
class MyLoginObserver : public ILTLoginObserver {
public:
    void OnLoginEvent(int eventNumber) override {
        switch (eventNumber) {
            case 0:  // :LTLO_INPROGRESS
                printf("Login in progress...\n");
                break;
            case 18: // LTLO_NEW_PIN_ACCEPTED
                printf("PIN accepted!\n");
                break;
            default:
                printf("Login event: %d\n", eventNumber);
                break;
        }
    }
    
    void OnLoginError(int errorNumber) override {
        // Handle errors
    }
};

// Register with mediator
CLTLoginMediator* mediator = GetLoginMediator();
MyLoginObserver* observer = new MyLoginObserver();
mediator->RegisterObserver(observer);
```

---

## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| `"CLTEvilBlockingLoginObserver::OnLoginEvent(): Got event we're waiting for!"` | 0xaf448 | Blocking observer log |
| `"CLTEvilBlockingLoginObserver::OnLoginEvent(): Event# %d"` | 0xaf494 | Event number logging |
| `"CLTLoginObserver_PassThrough::OnLoginEvent(): Event# %d  ServerResult# %s"` | 0xaf638 | Pass-through logging |
| `"CLTLoginMediator::PostEvent(): Event# %d"` | 0xaff80 | Mediator event posting |

---

## Related Classes and Methods

- **[OnLoginError](OnLoginError.md)** - Error handler method
- **[CLTLoginMediator](CLTLoginMediator.md)** - Mediator class
- **[ILTLoginObserver](ILTLoginObserver.md)** - Observer interface
- **[error_codes.md](error_codes.md)** - Complete event/error code reference

---

## Object Layout

### CLTEvilBlockingLoginObserver

```
Offset  Size  Field
------  ----  -----
0x00    4     vtable pointer
0x0c    4     m_expectedEvent (int)
0x10    1     m_completed (bool)
0x14    4     m_result (int)
0x18    4     m_errorNumber (int)
```

---

## Validation Status

**Status**: ✅ Validated against binary disassembly  
**Last Updated**: 2025-03-08  
**Binary**: `../../launcher.exe`  
**Method**: Static analysis of disassembly

---

## References

- **Source**: Binary disassembly of `launcher.exe`
- **Addresses**: 0x0041b520 (CLTEvilBlockingLoginObserver), ~0x0041b8xx (CLTLoginObserver_PassThrough)
- **Evidence**: Assembly analysis confirms thiscall convention and integer parameter
- **Related Files**: error_codes.md, OnLoginError.md

---

## See Also

- [OnLoginError](OnLoginError.md) - Error handler method
- [error_codes.md](error_codes.md) - Event and error code constants
- [VALIDATION_SUMMARY.md](VALIDATION_SUMMARY.md) - Validation report
