# OnShutdown

## Overview

**Category**: Lifecycle  
**Direction**: Launcher → Client  
**Purpose**: Shutdown notification

---

## Function Signature

```c
void OnShutdown(uint32_t shutdownFlags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `uint32_t` | shutdownFlags | Shutdown type flags |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Documentation Status

**Status**: ⏳ Partially Documented  
**Confidence**: Medium (inferred from lifecycle)  
**Last Updated**: 2025-06-17

---

## Related Callbacks

- [OnInitialize](OnInitialize.md)
- [OnError](OnError.md)

---

**Needs**: More details from runtime analysis
