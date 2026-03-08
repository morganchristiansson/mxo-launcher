# OnInitialize

## Overview

**Category**: Lifecycle  
**Direction**: Launcher → Client  
**Purpose**: Initialization complete notification

---

## Function Signature

```c
int OnInitialize(void* initData, uint32_t* result);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `void*` | initData | Initialization data structure |
| `uint32_t*` | result | Result code pointer |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Success |
| `int` | -1 | Initialization failed |

---

## Usage

Called after `SetMasterDatabase` to notify client that initialization is complete.

---

## Documentation Status

**Status**: ⏳ Partially Documented  
**Confidence**: Medium (inferred from lifecycle)  
**Last Updated**: 2025-06-17

---

## Related Callbacks

- [OnShutdown](OnShutdown.md)
- [OnError](OnError.md)
- [OnException](OnException.md)

---

**Needs**: More details from runtime analysis
