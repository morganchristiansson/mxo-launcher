# OnLoginError Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **INCORRECT** - Not a Simple C Callback

The documentation incorrectly describes `OnLoginError` as a C-style callback function.

**Reality**: `OnLoginError` is a **C++ virtual member function** of observer classes:

```cpp
class CLTEvilBlockingLoginObserver {
    void OnLoginError(int errorNumber);
};

class CLTLoginObserver_PassThrough {
    void OnLoginError(int errorNumber);
};
```

### 2. **INCORRECT** - Function Signature

**Documented**:
```c
int OnLoginError(LoginErrorEvent* errorEvent, void* userData);
```

**Actual** (from disassembly at 0x41b620):
```cpp
// thiscall convention (this in ECX)
void CLTEvilBlockingLoginObserver::OnLoginError(int errorNumber);
```

**Assembly Evidence**:
```asm
  41b620:	push   %ebp
  41b621:	mov    %esp,%ebp
  41b623:	mov    0x4d3e54,%edx     ; Load logging context
  41b629:	mov    $0x8,%eax
  41b62e:	push   %ebx
  41b62f:	mov    0x8(%ebp),%ebx    ; Get errorNumber parameter
  ...
  41b687:	push   %ebx              ; Push errorNumber
  41b687:	push   $0x4af50c         ; "Error# %d" format string
  41b68c:	push   %edx
  41b68d:	call   0x414f70          ; Logging function
  ...
  41b6b0:	ret    $0x4              ; Return and clean 4 bytes
```

### 3. **INCORRECT** - Parameter Type

**Documented**: `LoginErrorEvent*` structure (16 bytes)

**Actual**: Simple `int errorNumber`

The function accepts an integer error number, not a structure. Evidence:
- Only one stack parameter at `ebp+8`
- Used directly in logging: `"Error# %d"`
- Stored at offset `0x18` in the object

### 4. **INCORRECT** - Return Value

**Documented**: Returns `int` (0, -1, or 1)

**Actual**: Returns `void`

Evidence from assembly:
- No value in `eax` before `ret`
- Function returns `ret $0x4` (just cleans stack)

---

## Identified Methods

### CLTEvilBlockingLoginObserver::OnLoginError

**Address**: `0x0041b620`  
**Convention**: thiscall (this in ECX)  
**Signature**: `void OnLoginError(int errorNumber)`

**Assembly**:
```asm
  41b620:	push   %ebp
  41b621:	mov    %esp,%ebp
  ...
  41b62f:	mov    0x8(%ebp),%ebx    ; errorNumber parameter
  ...
  41b695:	mov    0x4d4d60,%ecx     ; Get object pointer
  41b69b:	mov    (%ecx),%eax       ; Get vtable
  41b69d:	call   *0x178(%eax)      ; Virtual method call
  41b6a3:	mov    %ebx,0x18(%esi)   ; Store errorNumber at this+0x18
  41b6a6:	mov    %eax,0x14(%esi)   ; Store result at this+0x14
  41b6a9:	movb   $0x0,0x10(%esi)   ; Clear flag at this+0x10
  41b6ad:	pop    %esi
  41b6ae:	pop    %ebx
  41b6af:	pop    %ebp
  41b6b0:	ret    $0x4              ; Clean 4 bytes
```

### CLTLoginObserver_PassThrough::OnLoginError

**Address**: ~`0x0041b9xx`  
**Convention**: thiscall (this in ECX)  
**Signature**: `void OnLoginError(int errorNumber)`

**String**: `"CLTLoginObserver_PassThrough::OnLoginError(): Error# %d"` at `0xaf684`

---

## Error Constants Identified

Error numbers correspond to these constants:

| Constant | Source |
|----------|--------|
| `LTLO_NOAUTHSERVERADDR` | Login Observer errors |
| `LTLO_UNEXPECTEDAUTHMSG` | Login Observer errors |
| `LTAUTH_INVALIDCERTIFICATE` | Authentication errors |
| `LTAUTH_EXPIREDCERTIFICATE` | Authentication errors |
| `LTAUTH_LOGINTYPENOTACCEPTED` | Authentication errors |
| `LTAUTH_ALREADYCONNECTED` | Authentication errors |
| `LTAUTH_AUTHKEYSIGINVALID` | Authentication errors |
| `LTMS_AUTHSERVERCHARACTERDELETEFAILED` | Mediator errors |
| `RESULT_USAGE_LIMIT_DENIED_LOGIN` | Result errors |

---

## Observer Pattern Architecture

```
CLTLoginMediator
    ↓ PostError(int errorNumber)
    ↓
ILTLoginObserver interface
    ↓
[Implementation classes]
    ├─ CLTEvilBlockingLoginObserver::OnLoginError()
    └─ CLTLoginObserver_PassThrough::OnLoginError()
```

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Callback Type | C callback function | C++ virtual method | ❌ **WRONG** |
| Parameters | `LoginErrorEvent*` structure | `int errorNumber` | ❌ **WRONG** |
| Return Value | `int` (0, -1, 1) | `void` | ❌ **WRONG** |
| Direction | Client → Launcher | Internal observer pattern | ❌ **WRONG** |
| Registration | ProcessEvent vtable | Observer pattern registration | ❌ **WRONG** |
| Structure Size | 16 bytes | N/A (no structure) | ❌ **WRONG** |

**Overall Status**: ❌ **DOCUMENTATION FUNDAMENTALLY INCORRECT**

---

## Recommendations

1. **Rewrite documentation** to reflect C++ observer pattern
2. **Document error constants** with their numeric values
3. **Document the observer interface** (likely `ILTLoginObserver`)
4. **Document the mediator class** (`CLTLoginMediator`)
5. **Remove references** to `LoginErrorEvent` structure
6. **Update related callbacks** (OnLogin, OnLoginEvent) similarly

---

## Binary Evidence

| Function | Address | Signature |
|----------|---------|-----------|
| `CLTEvilBlockingLoginObserver::OnLoginError` | 0x0041b620 | `void (int errorNum)` |
| `CLTLoginObserver_PassThrough::OnLoginError` | ~0x0041b9xx | `void (int errorNum)` |
| `CLTLoginMediator::PostError` | ~0x0041d0xx | `void (int errorNum)` |

---

## Comparison: Documented vs Actual

### Documented (Incorrect):
```c
// C callback
int OnLoginError(LoginErrorEvent* errorEvent, void* userData);

struct LoginErrorEvent {
    uint32_t playerId;
    uint32_t sessionId;
    uint32_t errorCode;
    uint32_t errorMessage;
    uint16_t flags;
};
```

### Actual (Correct):
```cpp
// C++ observer pattern
class ILTLoginObserver {
public:
    virtual void OnLoginError(int errorNumber) = 0;
};

class CLTEvilBlockingLoginObserver : public ILTLoginObserver {
public:
    void OnLoginError(int errorNumber) override;
    
private:
    int m_expectedEvent;      // at offset 0x0c
    bool m_completed;         // at offset 0x10
    int m_result;             // at offset 0x14
    int m_errorNumber;        // at offset 0x18
};
```
