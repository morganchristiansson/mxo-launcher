# Early Startup Crash Iteration Notes

## Session Summary: 2026-03-12

### MAJOR FINDING: Arg2 is NOT the Cause

**Callback Test (MXO_ARG2_CALLBACK_TEST=1) disproved the hypothesis:**

```
Test Result:
- Set arg2 to executable 'ret' instruction
- Expected: Crash at arg2 if callback, OR execute 'ret'
- Actual:    Crash at launcher code (0x0040FD5B) with EAX=0xCCCCCCC3
- 
EAX = 0xCCCCCCC3  <- Client READ arg2[0], didn't execute it
```

**Interpretation:**
- Client correctly treats arg2 as `char**` (argv array)
- Client reads arg2[0] expecting a string pointer
- Previous `EIP = arg2_base` was a **side effect**, not direct cause

**The crash is from CONTROL FLOW CORRUPTION elsewhere, not arg2 usage.**

---

## Historical Notes (For Reference)

### Original Observation: EIP at arg2 base
Earlier crashes showed:
```
EIP: 003e2bb8
arg2 filteredArgv: 003e2bb8  ← EXACT MATCH
```

**Now understood:** This was coincidental or side effect of stack corruption causing return to wrong address.

### ESI Pattern
ESI consistently pointed to `arg2 + 0x0c` (slot 3).

**Now understood:** Client was iterating argv normally, not indicating vtable access.

---

## Current Understanding

### What We Know (Evidence-Based)
1. **arg2 is correctly `char**`** - Client dereferences, doesn't execute
2. **arg2[0] is read as data** - EAX loaded with pointer value
3. **Client crashes later** - Not during InitClientDLL argv access
4. **Stack shows InitClientDLL args** - Normal at crash point

### What We Don't Know
1. **Where does control flow go wrong?**
2. **Is it stack corruption or callback corruption?**
3. **What code executes at crash EIP?**

### New Focus
Since arg2 is ruled out, investigate:
1. **Stack around InitClientDLL return**
2. **Other function pointers** client might store
3. **Mediator callbacks** in arg5/arg6
4. **Selection context** at +0xec

---

## Revised Investigation Priority

The crash likely happens AFTER InitClientDLL, during:
1. Client-side initialization using mediator
2. Callback from mediator to client
3. Return from InitClientDLL to launcher

Next experiments:
1. Trace InitClientDLL return more carefully
2. Check if crash is in client or launcher code
3. Look for stack smashing patterns
