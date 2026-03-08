# OnLoginError

## Overview

**Category**: game  
**Type**: C++ Virtual Method (Observer Pattern)  
**Direction**: Internal observer notification  
**Purpose**: Handle login error notifications in the observer pattern  
**Calling Convention**: thiscall (this in ECX)  
**Confidence Level**: High (validated against disassembly)

---

## Function Signature

```cpp
// Observer interface method
class ILTLoginObserver {
public:
    virtual void OnLoginError(int errorNumber) = 0;
};
```

---

## Implementations

### 1. CLTEvilBlockingLoginObserver::OnLoginError

**Address**: `0x0041b620`  
**Purpose**: Blocking observer for synchronous error handling (test/debug)

```cpp
void CLTEvilBlockingLoginObserver::OnLoginError(int errorNumber);
```

**Behavior**:
- Logs: `"Error# %d"`
- Stores error number at `this+0x18`
- Sets result from virtual method call at `this+0x14`
- Clears completion flag at `this+0x10`

### 2. CLTLoginObserver_PassThrough::OnLoginError

**Address**: ~`0x0041b9xx`  
**Purpose**: Pass-through error handler for production use

```cpp
void CLTLoginObserver_PassThrough::OnLoginError(int errorNumber);
```

**Behavior**:
- Logs: `"Error# %d"`
- Passes error through to registered handler

---

## Parameters

| Type | Name | Purpose |
|------|------|---------|
| `int` | errorNumber | Login error number (see Error Constants below) |

**Note**: This is a `thiscall` method - the `this` pointer is passed in ECX register.

---

## Error Constants

Errors are identified by integer constants. See **[error_codes.md](error_codes.md)** for complete list.

### Error Categories

| Prefix | Range | Category | Base |
|--------|-------|----------|------|
| `LTLO_` | 0x00-0x12 | Login Observer errors | 0x00 |
| `LTAUTH_` | 0x00-0x04 | Authentication errors | 0x00 |
| `LTMS_` | 0x00-0x2A | Mediator Server errors | 0x00 |

### Common Login Observer Errors (LTLO)

| Constant | Value | Description |
|----------|-------|-------------|
| `LTLO_NOAUTHSERVERADDR` | 1 (0x1) | No authentication server address |
| `LTLO_COULDNTRESOLVEADDR` | 2 (0x2) | Could not resolve address |
| `LTLO_SERVERREQUESTTIMEDOUT` | 3 (0x3) | Server request timed out |
| `LTLO_UNEXPECTEDAUTHMSG` | 4 (0x4) | Unexpected auth message |
| `LTLO_NOTLOGGEDIN` | 6 (0x6) | Not logged in |
| `LTLO_ALREADYLOGGEDIN` | 7 (0x7) | Already logged in |
| `LTLO_DECRYPTIONFAILURE` | 8 (0x8) | Decryption failed |
| `LTLO_CHARACTERNOTREADY` | 9 (0x9) | Character not ready |
| `LTLO_CERTVERIFYFAILED` | 11 (0xb) | Certificate verify failed |
| `LTLO_CLIENTHASHFAILED` | 12 (0xc) | Client hash failed |
| `LTLO_VERSIONREADFAILED` | 13 (0xd) | Version read failed |
| `LTLO_NOMXOSUB` | 14 (0xe) | No MxO subscription |

### Authentication Errors (LTAUTH)

| Constant | Value | Description |
|----------|-------|-------------|
| `LTAUTH_INVALIDCERTIFICATE` | 0 (0x0) | Invalid certificate |
| `LTAUTH_EXPIREDCERTIFICATE` | 1 (0x1) | Expired certificate |
| `LTAUTH_LOGINTYPENOTACCEPTED` | 2 (0x2) | Login type not accepted |
| `LTAUTH_ALREADYCONNECTED` | 3 (0x3) | Already connected |
| `LTAUTH_AUTHKEYSIGINVALID` | 4 (0x4) | Auth key signature invalid |

### Mediator Server Errors (LTMS)

| Constant | Value | Description |
|----------|-------|-------------|
| `LTMS_ALREADYCONNECTED` | 0 (0x0) | Already connected |
| `LTMS_NONAMECLAIMED` | 1 (0x1) | No name claimed |
| `LTMS_NOCHARACTERLOADED` | 2 (0x2) | No character loaded |
| `LTMS_INVALIDSECRET` | 3 (0x3) | Invalid secret |
| `LTMS_NAMEALREADYINUSE` | 4 (0x4) | Name already in use |
| `LTMS_INCOMPATIBLECLIENTVERSION` | 7 (0x7) | Incompatible version |
| `LTMS_INVALIDCHARACTERNAME` | 9 (0x9) | Invalid character name |
| `LTMS_CHARACTERNOTFOUND` | 10 (0xa) | Character not found |
| `LTMS_CLUSTERFULL` | 16 (0x10) | Cluster full |
| `LTMS_ACCOUNTINUSE` | 28 (0x1c) | Account in use |
| `LTMS_SOESESSIONVALIDATIONFAILED` | 40 (0x28) | SOE session validation failed |

**Note**: See [error_codes.md](error_codes.md) for complete list of 42+ LTMS errors.

---

## Return Value

**Type**: `void`

No return value. Error processing is fire-and-forget.

---

## Architecture

### Observer Pattern

```
┌──────────────────────────┐
│   CLTLoginMediator       │
│  ─────────────────────   │
│  + PostError(int error)  │
└──────────────────────────┘
            │
            │ notifies
            ↓
┌──────────────────────────┐
│   ILTLoginObserver       │◄─── Interface
│  ─────────────────────   │
│  + OnLoginError(int)     │
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

### CLTEvilBlockingLoginObserver::OnLoginError

**Address**: `0x0041b620`

```asm
41b620:	push   %ebp                ; Setup stack frame
41b621:	mov    %esp,%ebp
41b623:	mov    0x4d3e54,%edx      ; Load logging context
41b629:	mov    $0x8,%eax
41b62e:	push   %ebx
41b62f:	mov    0x8(%ebp),%ebx     ; Get errorNumber parameter
41b632:	sub    %edx,%eax          ; Logging level check
41b634:	cmp    $0x1,%eax
41b637:	push   %esi
41b638:	mov    %ecx,%esi          ; Save 'this' pointer
41b63a:	jg     0x41b695           ; Skip logging if disabled
...
41b680:	mov    0x4d3d60,%edx      ; Load log handle
41b686:	push   %ebx               ; Push errorNumber
41b687:	push   $0x4af50c          ; "Error# %d" format string
41b68c:	push   %edx               ; Logging context
41b68d:	call   0x414f70           ; printf-like function
41b692:	add    $0xc,%esp
...
41b695:	mov    0x4d4d60,%ecx      ; Get object pointer
41b69b:	mov    (%ecx),%eax        ; Get vtable
41b69d:	call   *0x178(%eax)       ; Virtual method call
41b6a3:	mov    %ebx,0x18(%esi)    ; Store errorNumber at this+0x18
41b6a6:	mov    %eax,0x14(%esi)    ; Store result at this+0x14
41b6a9:	movb   $0x0,0x10(%esi)    ; Clear flag at this+0x10
41b6ad:	pop    %esi
41b6ae:	pop    %ebx
41b6af:	pop    %ebp
41b6b0:	ret    $0x4               ; Return, clean 4 bytes
```

**Key Points**:
1. Single integer parameter (`errorNumber`) at `ebp+8`
2. No return value (void function)
3. Calls virtual method at vtable offset `0x178`
4. Stores error number at `this+0x18`
5. Cleans up 4 bytes on return (`ret $0x4`)

---

## Usage Example

### Implementing a Custom Error Handler

```cpp
class MyLoginObserver : public ILTLoginObserver {
public:
    void OnLoginError(int errorNumber) override {
        const char* errorName = GetErrorName(errorNumber);
        
        switch (errorNumber) {
            case 0x1:  // LTLO_NOAUTHSERVERADDR
                ShowMessageBox("Cannot connect to login server");
                break;
                
            case 0x8:  // LTLO_DECRYPTIONFAILURE
                ShowMessageBox("Security error - please reinstall");
                break;
                
            case 0x0:  // LTAUTH_INVALIDCERTIFICATE
                ShowMessageBox("Invalid security certificate");
                break;
                
            case 0x1c: // LTMS_ACCOUNTINUSE
                ShowMessageBox("Account already logged in elsewhere");
                break;
                
            default:
                printf("Login error: %s (%d)\n", errorName, errorNumber);
                break;
        }
    }
    
    void OnLoginEvent(int eventNumber) override {
        // Handle events
    }
    
private:
    const char* GetErrorName(int error) {
        // Map error numbers to names
        static const char* ltloNames[] = {
            "INPROGRESS", "NOAUTHSERVERADDR", "COULDNTRESOLVEADDR",
            "SERVERREQUESTTIMEDOUT", "UNEXPECTEDAUTHMSG", // ...
        };
        // ... lookup logic
        return "UNKNOWN";
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
| `"CLTEvilBlockingLoginObserver::OnLoginError(): Error# %d"` | 0xaf50c | Blocking observer log |
| `"CLTLoginObserver_PassThrough::OnLoginError(): Error# %d"` | 0xaf684 | Pass-through logging |
| `"CLTLoginMediator::PostError(): Error# %d"` | 0xaffac | Mediator error posting |

---

## Related Classes and Methods

- **[OnLoginEvent](OnLoginEvent.md)** - Event handler method
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

### Object Usage in OnLoginError

```cpp
// Assembly: mov %ebx,0x18(%esi)
this->m_errorNumber = errorNumber;  // at offset 0x18

// Assembly: call *0x178(%eax)
this->m_result = SomeVirtualMethod();  // at offset 0x14

// Assembly: movb $0x0,0x10(%esi)
this->m_completed = false;  // at offset 0x10
```

---

## Error Handling Flow

```
Error Occurs
     ↓
CLTLoginMediator::PostError(errorNumber)
     ↓
Notify all registered ILTLoginObserver instances
     ↓
Observer::OnLoginError(errorNumber) called
     ↓
Observer handles error (log, display, retry, etc.)
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
- **Addresses**: 0x0041b620 (CLTEvilBlockingLoginObserver), ~0x0041b9xx (CLTLoginObserver_PassThrough)
- **Evidence**: Assembly analysis confirms thiscall convention and integer parameter
- **Error Codes**: [error_codes.md](error_codes.md) for complete listing

---

## See Also

- [OnLoginEvent](OnLoginEvent.md) - Event handler method
- [error_codes.md](error_codes.md) - Event and error code constants
- [VALIDATION_SUMMARY.md](VALIDATION_SUMMARY.md) - Validation report
