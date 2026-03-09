# OnResize

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-03-08
> **Status**: ❌ **CALLBACK FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in the launcher or client binary.
>
> **CRITICAL**: This documentation shares address 0x620012a0 with [OnWindowEvent.md](OnWindowEvent.md) - both cannot be correct.
>
> See [OnResize_validation.md](OnResize_validation.md) for detailed disassembly analysis.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Callback Function Does Not Exist

Binary analysis of `../../launcher.exe` and `../../client.dll` confirms:

```bash
$ strings launcher.exe | grep -i "OnResize"
(no results)

$ strings client.dll | grep -i "OnResize"
(no results)
```

### 🚨 CRITICAL: Address Duplication

**This documentation shares address with OnWindowEvent.md:**

| Address | Claimed By | Reality |
|---------|------------|---------|
| `0x620012a0` | OnResize.md | Function exists but not a resize callback |
| `0x620012a0` | OnWindowEvent.md | Same function claimed as window event callback |

**Both cannot be correct**. One address = one function.

### ❌ Fabricated Event System

**None of the claimed elements exist**:

| Claimed Element | Status |
|----------------|--------|
| `OnResize` callback function | ❌ Does not exist |
| `ResizeEvent` structure (16 bytes) | ❌ Does not exist |
| `EVENT_RESIZE` constant | ❌ Does not exist |
| `WT_RESIZE` event type | ❌ Does not exist |
| `SetEventHandler` API | ❌ Does not exist |
| Window state constants (WS_*) | ❌ Do not exist |

---

## What Actually Exists

### DirectX Error Strings

The only "resize" related strings found:

```
D3DXERR_CANNOTRESIZENONWINDOWED
D3DXERR_CANNOTRESIZEFULLSCREEN
Resize does not work for non-windowed contexts
```

**What they are**: DirectX graphics error messages

**NOT evidence of**: Window resize callback system

### Windows API Functions

Standard Windows API for window management:

```
ShowWindow
GetWindowRect
SetForegroundWindow
...
```

**NOT evidence of**: Custom callback system

---

## Related UI Callbacks

**Validated**:
- ✅ [OnFocus.md](OnFocus.md) - Real callback with unique addresses

**Also Fabricated**:
- ❌ [OnInput.md](OnInput.md) - Copied addresses from OnFocus
- ❌ [OnWindowEvent.md](OnWindowEvent.md) - Shares address with OnResize

---

## References

- **Validation Report**: [OnResize_validation.md](OnResize_validation.md)
- **Binary**: `../../launcher.exe` and `../../client.dll`
- **Analysis Method**: String search, disassembly analysis, address cross-reference
- **Status**: Callback function does not exist, address duplicated

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Critical Issue**: Address duplication with OnWindowEvent.md
**Last Updated**: 2025-03-08
**Validator**: Binary Analysis
**Action**: Do not use
