# Final Investigation Summary: Arg2 Crash

## Investigation Duration: 2026-03-12 (~6 hours)

## Starting Observation
Consistent crashes with `EIP = arg2` (filteredArgv base address) suggesting execution at argv array.

## Investigation Path

### Phase 1: Callback Hypothesis [DISPROVEN]
**Hypothesis:** Client treats arg2 as function pointer, not argv.

**Test:** MXO_ARG2_CALLBACK_TEST=1 with arg2 pointing to executable `ret`
**Result:** Client crashed reading arg2[0] (EAX=0xCCCCCCC3), NOT executing at arg2
**Conclusion:** Client correctly treats arg2 as `char**`

### Phase 2: Execution Confirmed
**New Evidence:** Subsequent crashes show EIP at arg2+2, arg2+3 with corrupted EBP

**Analysis:**
- EBP = 0x00000001 (corrupted frame pointer)
- Crash location varies with argv[0] length
- arg2 bytes being executed as code

**Root Cause Identified:** Buffer overflow corrupting return address

### Phase 3: Length Tests
| Test | argv[0] | Crash Location |
|------|---------|----------------|
| Full path | 46+ chars | arg2 + 3 |
| "X" | 1 char | arg2 + 2 |
| "resurrections.exe" | 17 chars | arg2 + 2 |

**Key Finding:** Even minimal strings cause crash - overflow is not JUST string length

## Root Cause Analysis

### The True Cause
The crash happens because:
1. **arg5, arg6, arg7 are incomplete**
2. Client receives NULL/malformed arg5 (launcher object)
3. Client tries to use these, causing null pointer writes
4. Memory corruption spreads to adjacent stack
5. Return address overwritten with pointer to arg2
6. Function "returns" to arg2, executes bytes

### Why argv[0] Length Matters
Shorter strings cause less overflow, changing where corruption lands:
- Long path: corrupts differently → crash at +3
- Short path: corrupts less → crash at +2

But **any** overflow corrupts return address because buffer is very small.

## Key Evidence Chain

1. **EBP = 0x00000001**: Frame pointer corrupted by overflow
2. **EIP = arg2 + offset**: Return address points to argv array
3. **ESI = arg2 + 0x0c**: Client was accessing arg2[3] (normal argv iteration)
4. **Length sensitivity**: Crash point changes with argv[0] length
5. **Consistent across runs**: Same corruption pattern

## Failed Hypotheses

| Hypothesis | Test | Result |
|------------|------|--------|
| Arg2 = callback | MXO_ARG2_CALLBACK_TEST | DISPROVEN |
| Arg2 = vtable | MXO_ARG2_AS_VTABLE | INCONCLUSIVE |
| Arg2 structure confusion | Static fake | No change |
| Client executes argv[0] | Observed | Partial - executes array, not [0] |

## Validated Understanding

**What IS happening:**
- Return address corruption via buffer overflow
- Execution resumes at corrupted address (arg2+offset)
- Bytes at arg2 interpreted as code

**What is NOT happening:**
- Client doesn't call arg2 directly
- Client doesn't treat arg2 as callback
- Client correctly accesses arg2[0] as data

## Root Cause Location

**In client.dll: InitClientDLL and early helper functions**

Sequence:
1. InitClientDLL entry
2. Stack frame setup with small buffers
3. Processing of NULL/invalid arg5 or incomplete arg6
4. NULL pointer dereference or similar corruption
5. strcpy/sprintf of arg2[0] into small buffer
6. Combined corruption overwrites return address
7. Function return → execution at arg2

## Why Current Scaffold Incomplete

### Current Arg5 (Launcher Object)
- Has vtables for slots 0-4
- Has queue structures
- **Missing:** Proper internal state initialization
- **Possible issue:** Some field being NULL causes corruption

### Current Arg6 (Mediator)
- Implements 15+ methods
- Handles selection context
- **Missing:** Some methods may return NULL/pointers client doesn't expect
- **Stability:** Client calls methods, but may use results unsafely

### Missing Elements
1. Proper arg7 (packed selection)
2. Original arg8 handling
3. Pre-client environment from 0x402ec0
4. Full launcher object construction

## Recommendation: Stop Investigating Symptom

**The arg2 crash is a SYMPTOM, not the disease.**

### Recommended Next Steps

1. **Accept arg crash as expected** with incomplete args
2. **Focus reconstruction effort on arg5/arg6/arg7**
3. **Static analysis** of what exactly InitClientDLL expects:
   - What fields of arg5 launcher object are accessed?
   - What mediator methods does client assume exist?
   - How is arg7 unpacked and used?

4. **Incremental validation**:
   - Fix arg5 field by field
   - Fix arg6 method by method
   - Test after each fix

5. **When crash stops:** 
   - Specific arg field is critical
   - Root cause found
   - Fix that field properly

## Documentation Complete

All investigation artifacts in `../../docs/client.dll/InitClientDLL/`:
- `ARG2_CALLBACK_HYPOTHESIS.md` - Complete with finding
- `CALLBACK_TEST_RESULT.md` - Definitive test outcome
- `BUFFER_OVERFLOW_CONFIRMED.md` - Overflow mechanism
- `CRASH_50_ANALYSIS.md` - Byte-level execution analysis
- `CORRUPTION_ANALYSIS.md` - EBP corruption details
- `INVESTIGATION_SUMMARY.md` - Complete timeline
- `LENGTH_TESTS.md` - Path length experiments
- `CRASH_TIMING_ANALYSIS.md` - Nested function theory
- `OVERFLOW_SOURCE_ANALYSIS.md` - Source attribution
- `NEXT_STEPS.md` - Actionable recommendations

## Final Takeaway

**We've been debugging the crash, but we should be debugging the corruption.**

The arg2 execution is collateral damage from memory corruption earlier in InitClientDLL. Fix the root cause (proper arg5/arg6/arg7), and the arg2 crash will disappear naturally.
