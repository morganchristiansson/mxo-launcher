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
- `RET_BYPASS_HACK.md` - Diagnostic-only `ret` workaround note
- `NEXT_STEPS.md` - Actionable recommendations

## Addendum: temporary `ret` bypass does not change the diagnosis

A newer practical debugging result is that the visible late crash can be stepped past temporarily by forcing the bad `arg2` landing to execute `ret`.
That is useful only for reaching later behavior during interactive debugging.
It does **not** change the core conclusion here:

- the `arg2` crash is still collateral damage
- the underlying problem is still earlier corruption / bad state
- the real fix is still reconstructing `arg5` / `arg6` / `arg7`, not teaching argv storage to execute

Newer in-launcher validation tightened this further:
- with `MXO_ARG2_RET_BYPASS=1`, the simulated `ret` currently pops into stale startup-frame `arg3 hClientDll = 0x62000000`
- execution then immediately faults at `client.dll+0x3`
- and no later arg5 traffic becomes visible before that second fault

So even the diagnostic bypass currently reinforces the same conclusion:
this is a corrupted return chain, not a hidden valid continuation we merely failed to expose.

Newer preserved-frame logging tightened that further by showing the late crash-time stack walking directly through stale saved `InitClientDLL` args (`arg3`, `arg4`, `arg5`, `arg6`, and in non-zero arg7 runs also `arg7`).
A newer non-zero arg7 probe also showed that later client `+0x40` requests arrive as `selectionIndex=0x05000005` under `arg7=0x0500002a`.
A fresh static pass now explains that value as a client-side scratch mutation of the original arg7 stack slot at `0x62170dc1..0x62170e59`, and also shows that the client stores the original masked low-24-bit selection id separately through the nearby `0x629e1c7c` / `0x620011e0` path before that mutation.
Newer follow-up static work now identifies that sink more concretely as the client-side console-int `CreateCharacterWorldIndex`.
That makes the current split cleaner:
- masked low-24-bit selection id -> persisted client-owned `CreateCharacterWorldIndex`
- later mutated arg7 scratch dword -> `arg6->+0x40` lookup key shape
The diagnostic mediator now accepts the scratch-shaped request, but the late crash still did not move (`crash_60` / `crash_61`, still `EIP=0x003e5e8a`).
And a newer runtime observation keeps the `CreateCharacterWorldIndex` side interesting rather than dismissing it: the current patched-client crash is visibly happening during the in-game `Loading Character` phase.
Important ordering correction, though: that visible `"Loading Character"` text is pushed earlier at `client.dll:0x62170f2a`, immediately before the already-observed `arg6->+0xec` call at `0x62170f48`, so it does **not** yet prove the later direct `CreateCharacterWorldIndex` consumer at `0x62054cbd`.
A follow-up rerun after adding diagnostic mediator slot `+0x120` likewise still showed no `+0x120` traffic before the same late crash (`crash_62`, still `EIP=0x003e5e8a`).
And a newer post-`+0xec` static pass narrows the immediate continuation further:
- after `+0xec`, `0x62170b00` just jumps to `0x62170f62`
- calls `0x6216a1c0`
- returns success to its caller at `0x620015fd`
- and that enclosing helper returns success directly at `0x62001634`
So the already-reached `+0xec` helper itself currently shows **no further mediator traffic after `+0xec`** before success returns to the enclosing `InitClientDLL` logic.
That in turn strengthens the current corrupted-return-chain interpretation: the loading/selection helper appears to finish successfully, so the visible late `arg2+2` failure may be occurring during or immediately after the enclosing `InitClientDLL` success return / unwind rather than inside another unseen mediator callback in that helper.
A newer debugger-assisted trace sharpens that further:
- break at launcher `call g_InitClientDLL` site `resurrections.exe:0x411181`
- step into `client.dll:0x620012a0`
- use `finish`
- the function does **not** return to launcher `0x411187`
- instead it resolves directly into current `arg2 filteredArgv` base (`0x003e5d88` on that run)
- and one more instruction step advances to `0x003e5d8a`, matching the familiar crash family
So the current best model was that `InitClientDLL` itself was eventually unwinding/returning into corrupted argv storage before control got back to the launcher call site.

A newer static + debugger pass then resolved that corruption more concretely:
- the early mediator auth-name chain at `client.dll:0x62001325..0x62001362` calls `+0x58`, then `+0x60`, then `+0x5c`, and finally does `add esp, 0x14`
- that proves the single-arg mediator slots `+0x60` and `+0x5c` are expected to be **caller-clean** on this path
- the scaffold had exposed both as normal `__thiscall` methods compiled as `ret 4`
- debugger inspection then showed the exact overwrite:
  - at `0x62001319`, saved top-level return address `[ebp+4]` was still launcher `0x411187`
  - by `0x62001365`, `esp` had drifted to `ebp+8`
  - so `push esi` at `0x62001365` overwrote the saved return address with stale `esi = arg2 filteredArgv + 0x0c`
- replacing `+0x60` / `+0x5c` with caller-clean naked wrappers (`ret`, not `ret 4`) stopped reproducing the old late `arg2+2` crash family on the current binder path
- the same path still reaches `+0xec`, and `InitClientDLL` now returns cleanly to the launcher with value `1`
- a follow-up launcher-side correction then fixed our own local interpretation of that result:
  - the replacement launcher had been treating any non-zero `InitClientDLL` return as failure
  - on the observed current client success path, positive return `1` is the relevant success result
  - after correcting that handling, the same run now logs `InitClientDLL succeeded, but RunClientDLL is gated.`

So the old late `arg2` crash family is now best treated as a **resolved scaffold calling-convention bug**.
That shifts the active next step away from tracing an unexplained corrupted return chain and toward the legitimate post-success question: how to continue faithfully from the now-clean positive-return `InitClientDLL` result into the next startup phase.

## Final Takeaway

**We've been debugging the crash, but we should be debugging the corruption.**

The arg2 execution is collateral damage from memory corruption earlier in InitClientDLL. Fix the root cause (proper arg5/arg6/arg7), and the arg2 crash will disappear naturally.
