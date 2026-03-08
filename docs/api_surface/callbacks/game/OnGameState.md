# OnGameState

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
> 
> **Validation Date**: 2025-03-08
> **Status**: ❌ **FUNCTION DOES NOT EXIST IN BINARY**
> 
> This callback was fabricated and does not exist in the launcher binary.
> 
> See [OnGameState_validation.md](OnGameState_validation.md) for detailed disassembly analysis.
> 
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnGameState"
(no results)

$ strings launcher.exe | grep -i "GameState"
(no results)
```

### ❌ No Game State Callbacks Exist

The binary contains **no game state event callbacks**:
- No `OnGameState`
- No `GameStateEvent`
- No game state change notifications

### ✅ What Actually Exists

The binary has:
- **Session types** (configuration constants, not callbacks)
- **World events** (commands, not callbacks)
- **Login events** (C++ observer pattern, not C callbacks)

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**  
**Last Updated**: 2025-03-08  
**Validator**: Binary Analysis  
**Action**: Do not use
