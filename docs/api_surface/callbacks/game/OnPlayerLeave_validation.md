# OnPlayerLeave Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Function Does Not Exist

The documentation describes an `OnPlayerLeave` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnPlayerLeave"
(no results)

$ strings launcher.exe | grep -i "PlayerLeave"
(no results)
```

### 2. **Same Fabrication Pattern as OnPlayerJoin**

This callback follows the **exact same fabricated pattern** as `OnPlayerJoin`:
- Same C callback signature pattern
- Similar structure definitions
- Same "ProcessEvent vtable index 6, offset 0x18" reference
- "Medium confidence - inferred from game callback structure"
- No binary validation performed

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Existence | `OnPlayerLeave` callback | Does not exist | ❌ **FABRICATED** |
| Function Type | C callback function | N/A | ❌ **N/A** |
| Parameters | `PlayerLeaveEvent*` structure | N/A | ❌ **N/A** |
| Return Value | `int` (0, -1, 1) | N/A | ❌ **N/A** |
| Confidence Level | Medium | **Should be NONE** | ❌ **FALSE** |

**Overall Status**: ❌ **COMPLETE FABRICATION**

---

## Correct Information

See [OnPlayerJoin_validation.md](OnPlayerJoin_validation.md) for details on actual player management commands that exist in the binary.

---

**Validation Status**: ❌ **FABRICATED - DOES NOT EXIST**  
**Action Required**: **DEPRECATE IMMEDIATELY**
