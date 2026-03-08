# OnException

## Overview

**Category**: Lifecycle  
**Direction**: Launcher → Client  
**Purpose**: Exception/error callback notification

---

## Function Signature

```c
void OnException(void* exceptionData, uint32_t exceptionCode, uint32_t flags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `void*` | exceptionData | Exception information structure |
| `uint32_t` | exceptionCode | Exception code (ERROR_*, EXCEPTION_*) |
| `uint32_t` | flags | Exception flags and severity |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Diagnostic Strings

Found in client.dll:

| String | Address | Context |
|--------|---------|---------|
| "Could not load exception callback" | 0x628f17d4 | Registration failure |

---

## Usage

### Registration

Exception callback registered during initialization:

```c
// Register exception callback
SetEventHandler(obj, EVENT_EXCEPTION, ExceptionCallback);
```

### Invocation Pattern

```c
// Launcher invokes exception callback
void InvokeExceptionCallback(APIObject* obj, void* data, uint32_t code) {
    if (obj->pCallback1) {
        ((ExceptionCallback)obj->pCallback1)(data, code, 0);
    }
}
```

---

## Exception Codes

Common exception codes:

| Code | Name | Severity |
|------|------|----------|
| 0x00000001 | EXCEPTION_ACCESS_VIOLATION | Critical |
| 0x00000002 | EXCEPTION_ARRAY_BOUNDS | Error |
| 0x00000003 | EXCEPTION_STACK_OVERFLOW | Critical |
| 0x80000001 | EXCEPTION_NETWORK_ERROR | Warning |
| 0x80000002 | EXCEPTION_TIMEOUT | Warning |

---

## Implementation

### Launcher Side

```c
void TriggerException(APIObject* obj, uint32_t code, void* data) {
    // Check if callback registered
    if (!obj->pCallback1) {
        LogError("Could not load exception callback");
        return;
    }
    
    // Invoke callback
    ExceptionCallback callback = (ExceptionCallback)obj->pCallback1;
    callback(data, code, 0);
}
```

### Client Side

```c
// Exception handler implementation
void MyExceptionHandler(void* data, uint32_t code, uint32_t flags) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            HandleAccessViolation(data);
            break;
        case EXCEPTION_NETWORK_ERROR:
            HandleNetworkError(data);
            break;
        default:
            Log("Unknown exception: 0x%08X", code);
    }
}

// Register during initialization
void InitExceptionCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    obj->SetEventHandler(EVENT_EXCEPTION, MyExceptionHandler);
}
```

---

## Exception Data Structure

```c
struct ExceptionData {
    uint32_t code;              // Exception code
    uint32_t address;           // Fault address (if applicable)
    char message[256];          // Exception message
    uint32_t severity;          // Severity level
    void* context;              // Additional context
};
```

---

## Notes

- **Critical for error handling**
- Must be registered early in initialization
- Launcher logs error if callback not available
- Should handle all exception types gracefully
- May be called during other operations (reentrant)

---

## Related Callbacks

- [OnError](OnError.md) - General error callback
- [OnInitialize](OnInitialize.md) - Initialization callback
- [OnShutdown](OnShutdown.md) - Shutdown callback

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 4.1
- **String Address**: 0x628f17d4
- **String**: "Could not load exception callback"

---

**Status**: ✅ Documented  
**Confidence**: Medium (inferred from strings)  
**Last Updated**: 2025-06-17
