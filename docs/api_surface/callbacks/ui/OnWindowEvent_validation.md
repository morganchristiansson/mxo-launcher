# OnWindowEvent Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` and `../../client.dll` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Callback Function Does Not Exist

The documentation describes an `OnWindowEvent` callback function that **does not exist** in either binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnWindow"
(no results)

$ strings client.dll | grep -i "OnWindow"
(no results)
```

**No evidence found for**:
- ❌ `OnWindowEvent` callback function
- ❌ `WindowEvent` structure
- ❌ `EVENT_WINDOW` constant
- ❌ `WT_CLOSE`, `WT_RESIZE`, `WT_FOCUS`, `WT_DRAG` event types
- ❌ `SetEventHandler` API
- ❌ Window state constants (WS_VISIBLE, WS_HIDDEN, etc.)

### 2. **ADDRESS DUPLICATION** - Same Address as OnResize

**Critical Issue**: Both `OnWindowEvent.md` and `OnResize.md` claim the **same address**:

```bash
$ grep "0x620012a0" callbacks/ui/OnResize.md callbacks/ui/OnWindowEvent.md
callbacks/ui/OnResize.md:- **Address**: Resize event handler in InitClientDLL at 0x620012a0
callbacks/ui/OnWindowEvent.md:- **Address**: Window event handler in InitClientDLL at 0x620012a0
```

**This is impossible**: One address cannot be both a window event handler AND a resize handler.

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

**Analysis**: This is a regular function, not a callback dispatcher. No evidence it's specifically a window event handler.

### 4. **NO EVENT CONSTANTS**

Documentation claims window event types, but none exist:

**Claimed**:
```c
WT_CLOSE     0x0001
WT_RESIZE    0x0002
WT_FOCUS     0x0003
WT_DRAG      0x0004

WS_VISIBLE   0x0001
WS_HIDDEN    0x0002
WS_DISABLED  0x0004
```

**Search Results**:
```bash
$ strings launcher.exe | grep -E "WT_CLOSE|WT_RESIZE|WT_FOCUS|WT_DRAG"
(no results)

$ strings client.dll | grep -E "WT_CLOSE|WT_RESIZE|WT_FOCUS|WT_DRAG"
(no results)
```

### 5. **ONLY WINDOW-RELATED STRINGS**

The only window-related strings found:

```bash
$ strings launcher.exe | grep -i "window"
PatchCheckWindow
EnableWindow
invalid window size
GetWindowsDirectoryA
BringWindowToTop
SetForegroundWindow
GetWindowRect
IsWindowVisible
ShowWindow
IsWindow
GetForegroundWindow
GetWindowDC
GetDesktopWindow
```

**Context**: These are Windows API function names, not evidence of a custom callback system.

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Name | `OnWindowEvent` callback | Does not exist | ❌ **FABRICATED** |
| Function Signature | 2 parameters, int return | N/A | ❌ **FABRICATED** |
| WindowEvent Structure | 16 bytes claimed | Does not exist | ❌ **FABRICATED** |
| EVENT_WINDOW Constant | Event registration | Does not exist | ❌ **FABRICATED** |
| WT_* Event Types | 4 types claimed | Not found | ❌ **FABRICATED** |
| SetEventHandler API | Registration method | Does not exist | ❌ **FABRICATED** |
| Binary Address | 0x620012a0 (client.dll) | Duplicated with OnResize | ❌ **DUPLICATED** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## What Actually Exists

### Windows API Functions

Standard Windows API functions for window management:

```
ShowWindow
IsWindow
GetWindowRect
SetForegroundWindow
BringWindowToTop
IsWindowVisible
...
```

**Purpose**: Standard Windows API for window management

**NOT evidence of**: Custom window event callback system

### "invalid window size" String

```
invalid window size    at 0x4bf868 in launcher.exe
```

**Purpose**: Validation error message

**NOT evidence of**: Window event callback

---

## Fabrication Pattern

### Evidence of Fabrication

1. **No function name in binary**: "OnWindowEvent" not found
2. **No event constants**: No WT_* or WS_* constants
3. **No registration API**: No SetEventHandler found
4. **Address duplication**: Same address as OnResize.md
5. **Wrong binary**: Claims client.dll address, but no evidence there either

### Fabrication Source

**Likely created by**:
1. Assuming UI callbacks exist for all window operations
2. Copying structure from other callback documentation
3. Claiming same address as OnResize (fabrication shortcut)
4. Using Windows API names as "evidence" (misinterpretation)
5. No binary validation performed

### Similar to Other Fabrications

**Pattern matches**:
- OnInput.md - Fabricated, copied addresses from OnFocus
- OnResize.md - Fabricated, shares address with OnWindowEvent
- OnWindowEvent.md - Fabricated, shares address with OnResize

**Fabrication technique**: Address sharing between fabricated callbacks

---

## Address Duplication Analysis

### Both OnWindowEvent.md and OnResize.md Claim 0x620012a0

**OnWindowEvent.md claims**:
```
Window event handler in InitClientDLL at 0x620012a0
```

**OnResize.md claims**:
```
Resize event handler in InitClientDLL at 0x620012a0
```

**Reality**:
- Only ONE function can exist at this address
- Function exists but is not specifically a window event handler
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
$ strings launcher.exe | grep -i "OnWindow"
(no results)

$ strings client.dll | grep -i "OnWindow"
(no results)

# Search for event constants
$ strings launcher.exe | grep -E "WT_CLOSE|WT_RESIZE|EVENT_WINDOW"
(no results)

# Search for window state constants
$ strings launcher.exe | grep -E "WS_VISIBLE|WS_HIDDEN"
(no results)

# Search for registration API
$ strings launcher.exe | grep -i "SetEventHandler"
(no results)
```

---

## Root Cause Analysis

### Why This Happened

1. **Pattern Assumption**: "Window operations need callbacks"
2. **Address Shortcut**: Used same address as OnResize
3. **No Binary Validation**: Never searched for OnWindowEvent
4. **Misinterpreted Windows API**: Thought Windows API names were callback evidence
5. **Template Filling**: Fabricated details to match pattern

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnWindowEvent.md` - Mark as fabricated
2. ✅ **DEPRECATE** `OnResize.md` - Same fabrication pattern
3. ⚠️ **CHECK** all callbacks for address duplication patterns
4. ⚠️ **VALIDATE** addresses are unique to each callback

### Documentation Corrections

1. **Remove fabricated window event callback documentation**
2. **Document Windows API usage** (if relevant for window management)
3. **Stop assuming** window operations imply custom callbacks
4. **Verify uniqueness** of all claimed addresses

---

## Validation Checklist

- [x] Search for function name → **NOT FOUND**
- [x] Search for event constants → **NOT FOUND**
- [x] Search for window state constants → **NOT FOUND**
- [x] Check for registration API → **NOT FOUND**
- [x] Verify address uniqueness → **DUPLICATED (same as OnResize)**
- [x] Check claimed address → **Exists but not a window event handler**
- [x] Find actual window event mechanism → **None found (Windows API only)**

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnWindowEvent` callback function **does not exist** in either binary. The documentation:

1. **Fabricated** the entire callback mechanism
2. **Duplicated address** with OnResize.md (both claim 0x620012a0)
3. **Misinterpreted** Windows API names as callback evidence
4. **Invented** event types and constants without binary basis
5. **Created** detailed structures without validation

### Critical Finding: Address Sharing Pattern

**This is the SECOND pair of fabricated callbacks sharing an address**:

- OnResize.md and OnWindowEvent.md both claim 0x620012a0
- Neither has unique binary evidence
- Both are fabricated using the same shortcut

**Fabrication Pattern Identified**:
1. OnInput copied addresses from OnFocus (first pattern)
2. OnResize and OnWindowEvent share the same address (second pattern)

**Implication**: Systematic fabrication using address shortcuts.

### Corrective Action

This file should be **deprecated** immediately. It has no unique binary evidence and shares an address with another fabricated callback.

---

**Validation Status**: ❌ **FABRICATED - CALLBACK DOES NOT EXIST**
**Critical Issue**: Address duplication with OnResize.md
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-03-08
