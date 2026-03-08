# OnPlayerUpdate

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-06-18 (re-verified)
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in either launcher.exe or client.dll.
>
> **Verification**:
> ```bash
> strings ../../launcher.exe | grep -i "OnPlayer"     # No results
> strings ../../client.dll | grep -i "OnPlayer"      # No results
> grep -i "OnPlayer" /tmp/launcher_disasm.txt        # No matches
> ```
>
> See [PLAYER_CALLBACKS_VALIDATION.md](PLAYER_CALLBACKS_VALIDATION.md) for comprehensive validation report.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of both `launcher.exe` and `client.dll` confirms:

```bash
# String search
$ strings ../../launcher.exe | grep -i "OnPlayerUpdate"
(no results)

$ strings ../../client.dll | grep -i "OnPlayerUpdate"
(no results)

# No OnPlayer* callbacks of any kind exist
$ strings ../../launcher.exe | grep -i "OnPlayer"
(no results)
```

### Fabrication Pattern

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
