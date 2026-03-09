# OnInput

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-03-08
> **Status**: ❌ **CALLBACK FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in the launcher binary.
>
> **CRITICAL**: This documentation copied validation addresses from [OnFocus.md](OnFocus.md) without binary evidence.
>
> See [OnInput_validation.md](OnInput_validation.md) for detailed disassembly analysis.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Callback Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnInput"
(no results)
```

### ❌ Fabricated Input Event System

**None of the claimed elements exist**:

| Claimed Element | Status |
|----------------|--------|
| `OnInput` callback function | ❌ Does not exist |
| `InputEvent` structure (32 bytes) | ❌ Does not exist |
| `InputObject` structure (60+ bytes) | ❌ Does not exist |
| `EVENT_INPUT` constant | ❌ Does not exist |
| Input event types (INPUT_KEYDOWN, etc.) | ❌ Do not exist |
| Virtual key constants (VK_ESCAPE, etc.) | ❌ Not found as strings |
| `RegisterCallback2` API | ❌ Does not exist |

### 🚨 CRITICAL: Address Duplication

**This documentation copied addresses from OnFocus.md validation:**

| Address | Claimed By | Actual Owner |
|---------|------------|--------------|
| `0x41401a` | OnInput.md | OnFocus.md (focus callback) |
| `0x405acc` | OnInput.md | OnFocus.md (focus NULL check) |
| `0x4d2598` | OnInput.md | OnFocus.md (object pointer) |

**This is fabrication**: No unique addresses exist for OnInput.

---

## What Actually Exists

### GetAsyncKeyState Windows API

The only input-related string found in the binary:

```bash
$ strings launcher.exe | grep -i "GetAsyncKeyState"
GetAsyncKeyState
```

**What it is**: Standard Windows API function to check key state

**What it is NOT**:
- ❌ A callback function
- ❌ An event system
- ❌ An input abstraction layer

**Usage**: Direct Windows API call used internally by the launcher

---

## Fabrication Details

### What Was Fabricated

1. **Complete callback mechanism** - No registration/deregistration
2. **Event structures** - InputEvent, InputObject (no binary evidence)
3. **Event types** - INPUT_KEYDOWN, INPUT_MOUSEMOVE, etc. (not found)
4. **Virtual key codes** - VK_ESCAPE, VK_RETURN, etc. (not found as strings)
5. **Registration API** - RegisterCallback2 (does not exist)
6. **Validation evidence** - Copied from OnFocus.md

### Why This is Fabrication

- ⚠️ No unique assembly addresses
- ⚠️ Copied validation from another callback
- ⚠️ No string evidence for any claimed elements
- ⚠️ No EVENT_* constants
- ⚠️ No callback registration mechanism
- ⚠️ Detailed structures without binary basis

---

## Comparison with Validated OnFocus Callback

### OnFocus.md (VALIDATED)

- ✅ Unique addresses (0x41401a, 0x405acc)
- ✅ VTable offset validated
- ✅ NULL check pattern confirmed
- ✅ Windows focus API imports found
- ✅ Assembly evidence provided

### OnInput.md (FABRICATED)

- ❌ No unique addresses (copied from OnFocus)
- ❌ No VTable offset validated
- ❌ No NULL check pattern
- ❌ No input-specific Windows API
- ❌ No assembly evidence (stolen)

---

## Related UI Callbacks

**Validated**:
- ✅ [OnFocus.md](OnFocus.md) - Real callback with unique addresses

**Needs Validation**:
- ⚠️ [OnResize.md](OnResize.md) - Needs validation
- ⚠️ [OnWindowEvent.md](OnWindowEvent.md) - Needs validation

---

## References

- **Validation Report**: [OnInput_validation.md](OnInput_validation.md)
- **Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
- **Analysis Method**: String search, disassembly analysis, cross-reference check
- **Status**: Callback function does not exist, addresses duplicated from OnFocus.md

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Critical Issue**: Address duplication from validated callback
**Last Updated**: 2025-03-08
**Validator**: Binary Analysis
**Action**: Do not use
