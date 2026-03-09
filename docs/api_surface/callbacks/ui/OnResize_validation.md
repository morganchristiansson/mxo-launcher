# OnResize Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` and `../../client.dll` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Callback Function Does Not Exist

The documentation describes an `OnResize` callback function that **does not exist** in either binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnResize"
(no results)

$ strings client.dll | grep -i "OnResize"
(no results)

$ strings launcher.exe | grep -i "resize"
(no results)

$ strings client.dll | grep -i "resize"
D3DXERR_CANNOTRESIZENONWINDOWED    ← DirectX error, not callback
D3DXERR_CANNOTRESIZEFULLSCREEN     ← DirectX error, not callback
CRYPT_E_FILERESIZED                ← Crypto error, not callback
```

**No evidence found for**:
- ❌ `OnResize` callback function
- ❌ `ResizeEvent` structure
- ❌ `EVENT_RESIZE` constant
- ❌ `WT_RESIZE` event type
- ❌ `SetEventHandler` API
- ❌ Window state constants (WS_VISIBLE, WS_HIDDEN, etc.)

### 2. **ADDRESS DUPLICATION** - Same Address as OnWindowEvent

**Critical Issue**: Both `OnResize.md` and `OnWindowEvent.md` claim the **same address**:

```bash
$ grep "0x620012a0" callbacks/ui/OnResize.md callbacks/ui/OnWindowEvent.md
callbacks/ui/OnResize.md:- **Address**: Resize event handler in InitClientDLL at 0x620012a0
callbacks/ui/OnWindowEvent.md:- **Address**: Window event handler in InitClientDLL at 0x620012a0
```

**This is impossible**: One address cannot be both a resize handler AND a window event handler.

### 3. **CLAIMED ADDRESS IN CLIENT.DLL**

The claimed address is in `client.dll`, not `launcher.exe`:

**Address**: `0x620012a0` in client.dll

**Actual code at this address**:
```assembly
620012a0: push   %ebp
620012a1: mov    %esp,%ebp
620012a3: movb   $0x0,0x629dde88
620012aa: call   0x623d09a0
...
```

**Analysis**: This is a regular function, not a callback dispatcher. No evidence it's specifically a resize event handler.

### 4. **NO EVENT CONSTANTS**

Documentation claims window event types, but none exist:

**Claimed**:
```c
WT_RESIZE    0x0002
WS_VISIBLE   0x0001
WS_HIDDEN    0x0002
WS_DISABLED  0x0004
```

**Search Results**:
```bash
$ strings launcher.exe | grep -E "WT_RESIZE|WS_VISIBLE|WS_HIDDEN"
(no results)

$ strings client.dll | grep -E "WT_RESIZE|WS_VISIBLE|WS_HIDDEN"
(no results)
```

### 5. **ONLY WINDOW-RELATED STRING**

The only window-related string found:

```bash
$ strings launcher.exe | grep -i "window size"
invalid window size    at 0x4bf868
```

**Context**: This is likely a validation error message, not evidence of a callback system.

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Name | `OnResize` callback | Does not exist | ❌ **FABRICATED** |
| Function Signature | 2 parameters, int return | N/A | ❌ **FABRICATED** |
| ResizeEvent Structure | 16 bytes claimed | Does not exist | ❌ **FABRICATED** |
| EVENT_RESIZE Constant | Event registration | Does not exist | ❌ **FABRICATED** |
| WT_RESIZE Event Type | Window event type | Not found | ❌ **FABRICATED** |
| SetEventHandler API | Registration method | Does not exist | ❌ **FABRICATED** |
| Binary Address | 0x620012a0 (client.dll) | Duplicated with OnWindowEvent | ❌ **DUPLICATED** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## What Actually Exists

### DirectX Error Strings

The only "resize" related strings are DirectX errors:

```
D3DXERR_CANNOTRESIZENONWINDOWED
D3DXERR_CANNOTRESIZEFULLSCREEN
Resize does not work for non-windowed contexts
Resize does not work for full-screen
```

**Purpose**: DirectX graphics API error messages

**NOT evidence of**: Window resize callback system

### Windows API Functions

Standard Windows API functions for windows:

```
ShowWindow
IsWindow
GetWindowRect
SetForegroundWindow
...
```

**Purpose**: Standard Windows API for window management

**NOT evidence of**: Custom callback system

### "invalid window size" String

```
invalid window size    at 0x4bf868 in launcher.exe
```

**Purpose**: Likely a validation error message

**NOT evidence of**: Resize callback

---

## Fabrication Pattern

### Evidence of Fabrication

1. **No function name in binary**: "OnResize" not found
2. **No event constants**: No WT_* or WS_* constants
3. **No registration API**: No SetEventHandler found
4. **Address duplication**: Same address as OnWindowEvent.md
5. **Wrong binary**: Claims client.dll address, but no evidence there either

### Fabrication Source

**Likely created by**:
1. Assuming UI callbacks exist for all window operations
2. Copying structure from other callback documentation
3. Claiming same address as OnWindowEvent (fabrication shortcut)
4. Using DirectX error strings as "evidence" (misinterpretation)
5. No binary validation performed

### Similar to OnInput Fabrication

**Pattern**:
- ❌ No unique addresses (duplicated from OnWindowEvent)
- ❌ No string evidence in binary
- ❌ Fabricated event types without constants
- ❌ Fabricated registration mechanism
- ❌ Detailed structures without binary basis

---

## Address Duplication Analysis

### Both OnResize.md and OnWindowEvent.md Claim 0x620012a0

**OnResize.md claims**:
```
Resize event handler in InitClientDLL at 0x620012a0
```

**OnWindowEvent.md claims**:
```
Window event handler in InitClientDLL at 0x620012a0
```

**Reality**:
- Only ONE function can exist at this address
- Function exists but is not specifically a resize handler
- No evidence it's a callback dispatcher
- Both documentations cannot be correct

**Conclusion**: At least one (likely both) is fabricated.

---

## Comparison with Other UI Callbacks

### OnFocus.md (VALIDATED)

- ✅ Unique addresses in launcher.exe
- ✅ VTable offset validated
- ✅ Windows focus API imports
- ✅ Assembly evidence provided

### OnInput.md (FABRICATED)

- ❌ No unique addresses
- ❌ Copied from OnFocus.md
- ❌ No callback mechanism

### OnResize.md (FABRICATED)

- ❌ Duplicated address with OnWindowEvent
- ❌ No callback function
- ❌ No event constants
- ❌ No registration API

### OnWindowEvent.md (FABRICATED)

- ❌ Duplicated address with OnResize
- ❌ No callback function
- ❌ Same fabrication pattern

**Fabrication Rate**: 75% (3 out of 4 UI callbacks fabricated)

---

## Binary Evidence

### Search Results

```bash
# Search for callback function
$ strings launcher.exe | grep -i "OnResize"
(no results)

$ strings client.dll | grep -i "OnResize"
(no results)

# Search for event constants
$ strings launcher.exe | grep -E "WT_RESIZE|EVENT_RESIZE"
(no results)

# Search for window state constants
$ strings launcher.exe | grep -E "WS_VISIBLE|WS_HIDDEN"
(no results)

# Search for registration API
$ strings launcher.exe | grep -i "SetEventHandler"
(no results)

# Only resize-related strings (DirectX errors)
$ strings client.dll | grep -i resize
D3DXERR_CANNOTRESIZENONWINDOWED
D3DXERR_CANNOTRESIZEFULLSCREEN
CRYPT_E_FILERESIZED
```

---

## Root Cause Analysis

### Why This Happened

1. **Pattern Assumption**: "Window operations need callbacks"
2. **Address Shortcut**: Used same address as OnWindowEvent
3. **No Binary Validation**: Never searched for OnResize
4. **Misinterpreted DirectX Errors**: Thought DirectX errors were callback evidence
5. **Template Filling**: Fabricated details to match pattern

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnResize.md` - Mark as fabricated
2. ✅ **DEPRECATE** `OnWindowEvent.md` - Same fabrication pattern
3. ⚠️ **CHECK** all callbacks for address duplication patterns
4. ⚠️ **VALIDATE** addresses are unique to each callback

### Documentation Corrections

1. **Remove fabricated resize callback documentation**
2. **Document DirectX errors** (if relevant for graphics debugging)
3. **Stop assuming** window operations imply callbacks
4. **Verify uniqueness** of all claimed addresses

---

## Validation Checklist

- [x] Search for function name → **NOT FOUND**
- [x] Search for event constants → **NOT FOUND**
- [x] Search for window state constants → **NOT FOUND**
- [x] Check for registration API → **NOT FOUND**
- [x] Verify address uniqueness → **DUPLICATED (same as OnWindowEvent)**
- [x] Check claimed address → **Exists but not a resize handler**
- [x] Find actual resize mechanism → **None found (DirectX errors only)**

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnResize` callback function **does not exist** in either binary. The documentation:

1. **Fabricated** the entire callback mechanism
2. **Duplicated address** with OnWindowEvent.md (both claim 0x620012a0)
3. **Misinterpreted** DirectX error strings as callback evidence
4. **Invented** event types and constants without binary basis
5. **Created** detailed structures without validation

### Critical Finding: Address Duplication Pattern

**Second instance of address duplication** (first was OnInput copying from OnFocus)

This time: **Two fabricated callbacks share the same address**

- OnResize.md claims 0x620012a0
- OnWindowEvent.md claims 0x620012a0
- Neither has unique binary evidence

**Implication**: Both are likely fabricated using the same shortcut.

### Corrective Action

This file should be **deprecated** immediately. It has no unique binary evidence and shares an address with another fabricated callback.

---

**Validation Status**: ❌ **FABRICATED - CALLBACK DOES NOT EXIST**
**Critical Issue**: Address duplication with OnWindowEvent.md
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-03-08
