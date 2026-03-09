# OnInput Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Callback Function Does Not Exist

The documentation describes an `OnInput` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnInput"
(no results)

$ strings launcher.exe | grep -i "input" | grep -v "^Input"
LTAS_UNKNOWNPUBLICKEYID
LTAUTH_AUTHKEYSIGINVALID
RegCloseKey
RegOpenKeyExA
CERT_NewSessionKey
...
GetAsyncKeyState    ← Only actual input-related string
```

**No evidence found for**:
- ❌ `OnInput` callback function
- ❌ `InputEvent` structure
- ❌ `InputObject` structure
- ❌ `EVENT_INPUT` constant
- ❌ Input event types (INPUT_KEYDOWN, INPUT_MOUSEMOVE, etc.)
- ❌ Virtual key code constants (VK_ESCAPE, VK_RETURN, etc.)
- ❌ `RegisterCallback2` API

### 2. **ADDRESSED DUPLICATED FROM OnFocus.md**

**Critical Issue**: The documentation references addresses that are **already claimed by OnFocus.md**:

| Address | Claimed By | Purpose |
|---------|------------|---------|
| `0x41401a` | OnFocus.md | Focus callback invocation |
| `0x405acc` | OnFocus.md | Focus callback NULL check |
| `0x4d2598` | OnFocus.md | Object pointer |

**Evidence from OnFocus.md**:
```
Callback signature confirmed at address `0x41401a`
NULL check pattern validated at `0x405acc`
```

**This means**: OnInput.md has NO unique addresses of its own. It appears to have copied addresses from OnFocus.md validation.

### 3. **NO INPUT EVENT TYPES**

The documentation claims various input event types, but none exist in binary:

**Claimed**:
```c
INPUT_KEYDOWN
INPUT_KEYUP
INPUT_MOUSEMOVE
INPUT_MOUSEBUTTONDOWN
INPUT_MOUSEBUTTONUP
INPUT_MOUSEDOWN
INPUT_MOUSEUP
```

**Search Results**:
```bash
$ strings launcher.exe | grep -i "INPUT_KEY\|INPUT_MOUSE"
(no results)
```

### 4. **NO VIRTUAL KEY CONSTANTS**

Documentation lists virtual key codes, but not found in binary:

**Claimed**:
```
VK_ESCAPE (0x1B)
VK_TAB (0x09)
VK_RETURN (0x0D)
VK_BACK (0x08)
...
```

**Search Results**:
```bash
$ strings launcher.exe | grep -E "VK_ESCAPE|VK_RETURN|VK_SPACE|VK_SHIFT"
(no results)
```

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Name | `OnInput` callback | Does not exist | ❌ **FABRICATED** |
| Function Signature | 3 parameters, void return | N/A | ❌ **FABRICATED** |
| InputEvent Structure | 32 bytes claimed | Does not exist | ❌ **FABRICATED** |
| InputObject Structure | 60+ bytes claimed | Does not exist | ❌ **FABRICATED** |
| EVENT_INPUT Constant | Event registration | Does not exist | ❌ **FABRICATED** |
| Input Event Types | 7 types claimed | Not found | ❌ **FABRICATED** |
| Virtual Key Codes | 15+ constants | Not found | ❌ **FABRICATED** |
| RegisterCallback2 | Registration API | Does not exist | ❌ **FABRICATED** |
| Binary Addresses | Copied from OnFocus | Not unique | ❌ **DUPLICATED** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## What Actually Exists

### GetAsyncKeyState Windows API

The only input-related string found:

```bash
$ strings launcher.exe | grep -i "GetAsyncKeyState"
GetAsyncKeyState
```

**Purpose**: Windows API function that retrieves the state of a virtual key

**Usage**: Direct Windows API call, not a callback mechanism

**This is NOT**:
- A callback function
- An event system
- An input abstraction layer

It's just a standard Windows API that the launcher may use internally to check key states.

### No Input Callback System

**What does NOT exist**:
- No input event callback registration
- No InputEvent structure
- No input event dispatcher
- No EVENT_INPUT constant
- No input callback mechanism

---

## Fabrication Pattern

### Evidence of Fabrication

1. **No unique addresses**: All addresses copied from OnFocus.md
2. **No string evidence**: No "OnInput" or related strings in binary
3. **No constants**: No INPUT_* or VK_* constants found
4. **No structures**: No InputEvent or InputObject structures
5. **No registration API**: No callback registration mechanism

### Fabrication Source

**Likely created by**:
1. Assuming UI callbacks exist (pattern assumption)
2. Copying structure from OnFocus.md (address duplication)
3. Fabricating input event details from general Windows programming knowledge
4. No binary validation performed

### Template-Based Fabrication

**Pattern**:
- Similar to other fabricated callbacks
- Detailed structures without binary evidence
- "Medium Confidence" flag
- No unique assembly addresses
- Copied validation evidence from another callback

---

## Comparison with OnFocus.md

### OnFocus.md (Validated)

```c
void OnFocus(FocusEvent* focusEvent, uint32_t windowId, uint32_t eventType);
```

**Evidence**:
- ✅ Unique addresses (0x41401a, 0x405acc)
- ✅ VTable offset 0x10 validated
- ✅ NULL check pattern confirmed
- ✅ Windows focus API imports

### OnInput.md (Fabricated)

```c
void OnInput(InputEvent* inputEvent, uint32_t inputId, uint32_t flags);
```

**Evidence**:
- ❌ No unique addresses
- ❌ Copied addresses from OnFocus.md
- ❌ No VTable offset validated
- ❌ No NULL check pattern
- ❌ No input-specific Windows API

**Key Difference**: OnFocus has unique addresses and validation; OnInput has none.

---

## Address Duplication Analysis

### Addresses Claimed by Both

**0x41401a - Callback Invocation**:

OnFocus.md claims:
```assembly
41401a: ff 50 10    call *0x10(%eax)    ; Call vtable[0x10] (focusCallback)
```

OnInput.md would need to claim the same code, but:
- This is already documented as focus callback
- No evidence this is an input callback
- Address is NOT unique to OnInput

**0x405acc - NULL Check Pattern**:

OnFocus.md claims:
```assembly
405acc: 8b 0d 98 25 4d 00    mov 0x4d2598,%ecx    ; Load object pointer
405ad2: 85 c9                test %ecx,%ecx        ; Check if NULL
405ad4: 74 4c                je  0x405b22          ; Skip if NULL
405ad6: 8b 01                mov (%ecx),%eax       ; Load vtable
405ad8: ff 50 10             call *0x10(%eax)      ; Invoke callback at vtable[0x10]
```

This is clearly for the focus callback, not input.

---

## Root Cause Analysis

### Why This Happened

1. **Pattern Assumption**: "UI category has callbacks, so OnInput must exist"
2. **No Binary Validation**: Never searched for OnInput in binary
3. **Address Copying**: Reused addresses from OnFocus.md validation
4. **Template Filling**: Fabricated details to fill template
5. **No Uniqueness Check**: Didn't verify addresses were unique

### Similar to Other Fabrications

**Game Callbacks** (75% fabricated):
- No strings found at all
- Complete fabrication

**Network Callbacks** (100% fabricated):
- Message type strings found
- Fabricated callback mechanism

**UI Callbacks** (50% fabricated so far):
- OnFocus: ✅ Validated (has unique addresses)
- OnInput: ❌ Fabricated (no unique addresses, copied from OnFocus)

---

## Binary Evidence

### Search Results

```bash
# Search for callback function
$ strings launcher.exe | grep -i "OnInput"
(no results)

# Search for input event types
$ strings launcher.exe | grep -i "INPUT_KEY\|INPUT_MOUSE"
(no results)

# Search for virtual key constants
$ strings launcher.exe | grep -E "VK_ESCAPE|VK_RETURN|VK_SPACE"
(no results)

# Search for event constant
$ strings launcher.exe | grep "EVENT_INPUT"
(no results)

# Search for callback registration
$ strings launcher.exe | grep "RegisterCallback"
(no results)

# Only input-related string found
$ strings launcher.exe | grep -i "GetAsyncKeyState"
GetAsyncKeyState
```

### GetAsyncKeyState Analysis

**What it is**: Windows API function that checks if a key is up or down

**Prototype**:
```c
SHORT GetAsyncKeyState(int vKey);
```

**Usage**: Direct Windows API call, not a callback

**Not evidence of**: Input callback system

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnInput.md` - Mark as fabricated
2. ⚠️ **VALIDATE** remaining UI callbacks:
   - `OnResize.md`
   - `OnWindowEvent.md`
3. ⚠️ **CHECK** for address duplication in other callbacks
4. ⚠️ **REMOVE** all references to copied addresses

### Documentation Corrections

1. **Remove fabricated input callback documentation**
2. **Document GetAsyncKeyState usage** (if relevant for internal implementation)
3. **Stop assuming** UI category implies all callbacks exist
4. **Verify uniqueness** of all addresses before claiming validation

### Process Improvements

1. **Check for address duplication** before claiming validation
2. **Search for ALL claimed strings** in binary
3. **Verify constants exist** before documenting them
4. **Demand unique assembly evidence** for each callback
5. **Cross-reference** with other documentation to prevent duplication

---

## Validation Checklist

- [x] Search for function name → **NOT FOUND**
- [x] Search for event types → **NOT FOUND**
- [x] Search for constants → **NOT FOUND**
- [x] Search for structures → **NOT FOUND**
- [x] Check for unique addresses → **NONE (copied from OnFocus.md)**
- [x] Verify address uniqueness → **DUPLICATED**
- [x] Find actual input mechanism → **Only GetAsyncKeyState API**

---

## Conclusion

### Validation Result

❌ **COMPLETE FABRICATION**

The `OnInput` callback function **does not exist** in the binary. The documentation:

1. **Fabricated** the entire callback mechanism
2. **Copied addresses** from OnFocus.md validation
3. **Invented** input event types without evidence
4. **Claimed** virtual key constants that don't exist as strings
5. **Created** detailed structures without binary basis

### Critical Finding: Address Duplication

**This is the first validated callback that COPIED ADDRESSES from another callback's validation.**

This suggests:
- Author knew addresses were needed for validation
- Copied from OnFocus.md without understanding
- No actual binary analysis performed
- Attempted to appear validated without doing the work

### Corrective Action

This file should be **deprecated** immediately. It has NO unique binary evidence and copies validation from OnFocus.md.

---

**Validation Status**: ❌ **FABRICATED - CALLBACK DOES NOT EXIST**
**Critical Issue**: Address duplication from OnFocus.md
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-03-08
