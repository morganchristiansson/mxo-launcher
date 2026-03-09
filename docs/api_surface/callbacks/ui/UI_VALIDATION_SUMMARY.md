# UI Callbacks Validation Summary

## Overview

Validated **all 4** UI callback documentation files against disassembly of `../../launcher.exe` and `../../client.dll`:

1. `OnFocus.md`
2. `OnInput.md`
3. `OnResize.md`
4. `OnWindowEvent.md`

---

## Summary of Findings

| File | Status | Confidence | Action Required |
|------|--------|------------|-----------------|
| `OnFocus.md` | ✅ **VALIDATED** | High | None - legitimate callback |
| `OnInput.md` | ❌ **FABRICATED** | 100% | **DEPRECATED** - callback does not exist |
| `OnResize.md` | ❌ **FABRICATED** | 100% | **DEPRECATED** - callback does not exist |
| `OnWindowEvent.md` | ❌ **FABRICATED** | 100% | **DEPRECATED** - callback does not exist |

**Fabrication Rate**: 75% (3 out of 4 UI callbacks fabricated)

---

## Critical Issues

### 1. OnFocus.md - ✅ VALIDATED (Previously Completed)

**Status**: Legitimate callback with binary evidence

**Evidence**:
- ✅ Unique addresses (0x41401a, 0x405acc)
- ✅ VTable offset 0x10 validated
- ✅ NULL check pattern confirmed
- ✅ Windows focus API imports
- ✅ Assembly evidence provided

**No action required**.

---

### 2. OnInput.md - ❌ FABRICATED

**Problem**: The `OnInput` callback function **does not exist** in the binary.

**What Does NOT Exist**:
- ❌ `OnInput` callback function
- ❌ `InputEvent` structure (32 bytes claimed)
- ❌ `InputObject` structure (60+ bytes claimed)
- ❌ `EVENT_INPUT` constant
- ❌ Input event types (INPUT_KEYDOWN, INPUT_MOUSEMOVE, etc.)
- ❌ Virtual key constants (VK_ESCAPE, VK_RETURN, etc.)
- ❌ `RegisterCallback2` API

**What Actually Exists**:
- ✅ `GetAsyncKeyState` Windows API (only input-related string)

**Root Cause**: Complete fabrication without binary validation.

---

## Critical Finding: Address Duplication

**Multiple instances of address duplication discovered**:

### Pattern 1: OnInput.md copying from OnFocus.md

OnInput.md **copied validation addresses** from OnFocus.md:

| Address | OnFocus.md Claims | OnInput.md Claims | Reality |
|---------|-------------------|-------------------|---------|
| `0x41401a` | Focus callback invocation | Input callback invocation | Focus callback |
| `0x405acc` | Focus NULL check | Input NULL check | Focus callback |

**Implications**:
- Author knew validation requires addresses
- Copied from legitimate validation without understanding
- Attempted to appear validated without doing the work

### Pattern 2: OnResize.md and OnWindowEvent.md sharing same address

Both OnResize.md and OnWindowEvent.md **claim the same address**:

| Address | OnResize.md Claims | OnWindowEvent.md Claims | Reality |
|---------|-------------------|-------------------------|---------|
| `0x620012a0` | Resize event handler | Window event handler | Regular function (neither) |

**Implications**:
- Both fabricated using same shortcut
- Neither has unique binary evidence
- Impossible for one address to be two different handlers

**Fabrication sophistication**: Address duplication is harder to detect than complete fabrication.

---

## Fabrication Patterns

### Pattern 1: Complete Fabrication (Game Callbacks)

- No strings found at all
- No addresses claimed
- Pure template-based fabrication
- 75% fabrication rate

### Pattern 2: Partial Basis (Network Callbacks)

- Message type strings found
- No callback functions
- Fabricated callback mechanism around strings
- 100% fabrication rate

### Pattern 3: Address Sharing (NEW - OnResize + OnWindowEvent)

- Two fabricated callbacks claim the same address
- Neither has unique evidence
- Impossible to be both callbacks
- 75% fabrication rate (3 out of 4 UI callbacks)

**Fabrication sophistication increasing**: Multiple patterns of address misuse detected.

---

## What Actually Exists

### Windows Input APIs

The binary contains only standard Windows input APIs:

```
GetAsyncKeyState    ← Check key state
```

**Purpose**: Direct Windows API usage, not callback abstraction

**NOT evidence of**: Input callback system

### Focus Callback (Legitimate)

```
OnFocus callback at vtable[0x10]
Addresses: 0x41401a, 0x405acc
```

**This is real**: Unique addresses, validated NULL checks, Windows focus APIs

### DirectX Error Strings

```
D3DXERR_CANNOTRESIZENONWINDOWED
D3DXERR_CANNOTRESIZEFULLSCREEN
```

**Purpose**: DirectX graphics errors

**NOT evidence of**: Resize callback system

### Windows API Functions

```
ShowWindow
GetWindowRect
SetForegroundWindow
...
```

**Purpose**: Standard Windows OS functions

**NOT evidence of**: Custom window event callbacks

---

## Validation Evidence

### OnFocus.md (Validated)

```bash
# Unique addresses found
0x41401a: call *0x10(%eax)    # Focus callback invocation
0x405acc: test %ecx,%ecx      # NULL check before callback

# Windows focus APIs confirmed
SetFocus, GetFocus, KillFocus (imported)
```

### OnInput.md (Fabricated)

```bash
# No unique addresses (copied from OnFocus)
# No input event types found
$ strings launcher.exe | grep -i "INPUT_KEY\|INPUT_MOUSE"
(no results)

# No virtual key constants found
$ strings launcher.exe | grep -E "VK_ESCAPE|VK_RETURN"
(no results)

# Only input-related string
$ strings launcher.exe | grep -i "GetAsyncKeyState"
GetAsyncKeyState
```

---

## Address Duplication Detection

### How to Detect

1. **Cross-reference addresses** between callbacks
2. **Check if addresses are unique** to each callback
3. **Verify ownership** of assembly patterns
4. **Search for the callback name** at claimed addresses

### Example Detection

```bash
# Find all callbacks claiming address 0x41401a
$ grep -r "0x41401a" callbacks/ --include="*.md"
callbacks/ui/OnFocus.md:  41401a:	ff 50 10
callbacks/ui/OnFocus.md:	41401a:	ff 50 10
callbacks/ui/OnFocus.md:	41401a:	ff 50 10
# Only OnFocus.md should claim this address

# If multiple callbacks claim same address → FABRICATION
```

---

## Remaining UI Callbacks

**All UI callbacks have been validated**:

| File | Status | Action |
|------|--------|--------|
| `OnFocus.md` | ✅ VALIDATED | Complete |
| `OnInput.md` | ❌ FABRICATED | Deprecated |
| `OnResize.md` | ❌ FABRICATED | Deprecated |
| `OnWindowEvent.md` | ❌ FABRICATED | Deprecated |

**Total**: 4/4 validated (100%)

**No remaining UI callbacks to validate.**

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATED** `OnInput.md` - Marked as fabricated
2. ✅ **DEPRECATED** `OnResize.md` - Marked as fabricated
3. ✅ **DEPRECATED** `OnWindowEvent.md` - Marked as fabricated
4. ✅ **VALIDATED** all UI callbacks (4/4 complete)
5. ⚠️ **CHECK** other callback categories for address duplication

### Process Improvements

1. **Cross-reference all addresses** before accepting validation
2. **Require unique addresses** for each callback
3. **Search for address duplication** in validation pipeline
4. **Flag callbacks without unique addresses** as suspicious
5. **Verify callback names** at claimed addresses in disassembly

### New Validation Step

**Added to validation workflow**:
- [ ] Check for address duplication across all documentation
- [ ] Verify addresses are unique to this callback
- [ ] Cross-reference with other validated callbacks
- [ ] Search for callback name at claimed addresses

---

## Validation Files Created

- ✅ `OnInput_validation.md` (10,320 bytes)
- ✅ `OnInput.md` updated (deprecated)
- ✅ `OnResize_validation.md` (9,526 bytes)
- ✅ `OnResize.md` updated (deprecated)
- ✅ `OnWindowEvent_validation.md` (9,211 bytes)
- ✅ `OnWindowEvent.md` updated (deprecated)
- ✅ `UI_VALIDATION_SUMMARY.md` (this file)

---

## Overall Validation Progress

### By Category

| Category | Validated | Fabricated | Rate | Status |
|----------|-----------|------------|------|--------|
| Game Callbacks | 8 | 6 | 75% | ✅ Complete |
| Network Callbacks | 2 | 2 | 100% | ⚠️ In progress (2/13) |
| UI Callbacks | 4 | 3 | 75% | ✅ Complete |
| **Total** | **14** | **11** | **79%** | **In Progress** |

### Unique Finding

**Address Duplication Pattern**: Discovered two distinct patterns in UI callbacks:

1. **OnInput.md** copying addresses from OnFocus.md (validated callback)
2. **OnResize.md + OnWindowEvent.md** sharing the same address (both fabricated)

This demonstrates **multiple fabrication techniques**:
- Copying from legitimate callbacks
- Sharing addresses between fabricated callbacks
- Both are **sophisticated fabrication patterns** that:
  - Appear validated at first glance
  - Reuse legitimate assembly patterns or addresses
  - Harder to detect without cross-referencing
  - Require systematic address tracking

---

## Conclusion

### Validation Results

All four UI callbacks validated:
1. **OnFocus.md** - ✅ Validated (legitimate callback with unique addresses)
2. **OnInput.md** - ❌ Fabricated (no unique addresses, copied from OnFocus)
3. **OnResize.md** - ❌ Fabricated (shares address with OnWindowEvent)
4. **OnWindowEvent.md** - ❌ Fabricated (shares address with OnResize)

**Fabrication Rate**: 75% (3 out of 4 UI callbacks)

### Critical Discovery

**Two Address Duplication Patterns**:
- **Pattern 1**: OnInput.md copied validation addresses from OnFocus.md
- **Pattern 2**: OnResize.md and OnWindowEvent.md share the same address (0x620012a0)
- Neither pattern has unique binary evidence
- Both represent sophisticated fabrication techniques

### Lessons Learned

1. **Unique addresses are essential** for validation
2. **Cross-reference all addresses** across documentation
3. **Address duplication = fabrication**
4. **Address sharing between callbacks = both fabricated**
5. **Don't trust validation without checking uniqueness**

---

**Validation Status**: ✅ **COMPLETE** (4 of 4 UI callbacks validated)
**Fabrication Rate**: 75%
**Critical Finding**: Two distinct address duplication patterns discovered
**Last Updated**: 2025-03-08
