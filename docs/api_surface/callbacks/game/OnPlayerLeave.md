# OnPlayerLeave

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
> 
> **Validation Date**: 2025-03-08
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
> 
> This callback was fabricated and does not exist in the launcher binary.
> 
> See [OnPlayerLeave_validation.md](OnPlayerLeave_validation.md) for detailed disassembly analysis.
> 
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnPlayerLeave"
(no results)

$ strings launcher.exe | grep -i "PlayerLeave"
(no results)
```

### Same Fabrication Pattern

This callback follows the **same fabricated pattern** as `OnPlayerJoin`:
- No binary validation performed
- Template-based fabrication
- "Medium confidence - inferred from game callback structure" warning ignored

---

## Correct Information

See [OnPlayerJoin.md](OnPlayerJoin.md) for details on actual player management commands that exist in the binary.

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**  
**Last Updated**: 2025-03-08  
**Validator**: Binary Analysis  
**Action**: Do not use
