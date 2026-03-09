# OnWindowEvent

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-03-08
> **Status**: ❌ **CALLBACK FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in the launcher or client binary.
>
> **CRITICAL**: This documentation shares address 0x620012a0 with [OnResize.md](OnResize.md) - both cannot be correct.
>
> See [OnWindowEvent_validation.md](OnWindowEvent_validation.md) for detailed disassembly analysis.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Callback Function Does Not Exist

Binary analysis of `../../launcher.exe` and `../../client.dll` confirms:

```bash
$ strings launcher.exe | grep -i "OnWindow"
(no results)

$ strings client.dll | grep -i "OnWindow"
(no results)
```

### 🚨 CRITICAL: Address Duplication

**This documentation shares address with OnResize.md:**

| Address | Claimed By | Reality |
|---------|------------|---------|
| `0x620012a0` | OnWindowEvent.md | Function exists but not a window event callback |
| `0x620012a0` | OnResize.md | Same function claimed as resize callback |

**Both cannot be correct**. One address = one function.

### ❌ Fabricated Event System

**None of the claimed elements exist**:

| Claimed Element | Status |
|----------------|--------|
| `OnWindowEvent` callback function | ❌ Does not exist |
| `WindowEvent` structure (16 bytes) | ❌ Does not exist |
| `EVENT_WINDOW` constant | ❌ Does not exist |
| `WT_CLOSE`, `WT_RESIZE`, etc. event types | ❌ Do not exist |
| `SetEventHandler` API | ❌ Does not exist |
| Window state constants (WS_*) | ❌ Do not exist |

---

## What Actually Exists

### Windows API Functions

Standard Windows API for window management:

```
ShowWindow
IsWindow
GetWindowRect
SetForegroundWindow
BringWindowToTop
...
```

**What they are**: Standard Windows operating system functions

**NOT evidence of**: Custom window event callback system

---

## Related UI Callbacks

**Validated**:
- ✅ [OnFocus.md](OnFocus.md) - Real callback with unique addresses

**Also Fabricated**:
- ❌ [OnInput.md](OnInput.md) - Copied addresses from OnFocus
- ❌ [OnResize.md](OnResize.md) - Shares address with OnWindowEvent

---

## References

- **Validation Report**: [OnWindowEvent_validation.md](OnWindowEvent_validation.md)
- **Binary**: `../../launcher.exe` and `../../client.dll`
- **Analysis Method**: String search, disassembly analysis, address cross-reference
- **Status**: Callback function does not exist, address duplicated

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Critical Issue**: Address duplication with OnResize.md
**Last Updated**: 2025-03-08
**Validator**: Binary Analysis
**Action**: Do not use
