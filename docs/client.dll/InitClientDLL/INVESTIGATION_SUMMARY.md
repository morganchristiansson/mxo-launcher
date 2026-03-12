# Arg2 Crash Investigation - Complete Summary

## Dates Active: 2026-03-12

## Initial Observation
Consistent crashes with EIP landing at or near arg2 (filteredArgv) base address.

## Investigation Phases

### Phase 1: VTable Hypothesis (DISPROVEN)
**Hypothesis:** Client treats arg2 as vtable/callback, not argv.

**Tests:**
1. MXO_ARG2_AS_VTABLE=1 - Executable slots in argv[0-3]
2. MXO_ARG2_STATIC_FAKE=1 - Static address for arg2
3. MXO_ARG2_CALLBACK_TEST=1 - Direct callback test

**Result:** **HYPOTHESIS DISPROVED**
- Callback test showed client READS arg2[0] (0xCCCCCCC3), doesn't execute it
- Client correctly treats arg2 as `char**`
- EAX = 0xCCCCCCC3 proves data access, not code execution

### Phase 2: Execution Confirmed (REOPENED)
**New Evidence (Crash 50/51):**
```
EIP: 0x003e3b3b = arg2 + 3
ESI: 0x003e3b44 = arg2 + 0x0c (&arg2[3])

arg2 memory (little-endian pointer to argv[0]):
0x003e3b38-0x003e3b3b: 0x003e3b50 (bytes: 50 3b 3e 00)

Disassembly at crash:
0x003e3b38: 50           push eax
0x003e3b39: 3b 3e        cmp edi, [esi]
0x003e3b3b: 00 ...       add [eax], al  <- CRASH (EAX=1)
```

**Finding:** Something IS executing arg2 array as code!

**Resolution Reconciliation:**
- Normal run: Code executes argv array (50 3b 3e 00)
- Callback test: Client tries to dereference 0xCCCCCCC3 first, crashes earlier
- Same root cause, different manifestation

### Phase 3: Current Understanding

**CONFIRMED:** Something calls/jumps to arg2, treating pointer array as code.

**Evidence Chain:**
1. Callback test: EAX=0xCCCCCCC3 shows client reads arg2[0] via data path
2. Normal crashes: EIP at arg2 base shows execution path
3. Conclusion: Multiple code paths, some execute arg2

## Current Status: OPEN

**What is calling arg2?**
- Stack corruption overwriting return?
- Indirect call through corrupted pointer?
- Direct call bug in specific code path?

**Next Priority:**
1. Static analysis of client.dll for call sites
2. Add return address tracking
3. Detect if crash occurs before/after InitClientDLL return

## Key Files

### Completed Investigations
- `ARG2_CALLBACK_HYPOTHESIS.md` - Complete with finding
- `CALLBACK_TEST_RESULT.md` - Definitive test result
- `VTABLE_EXPERIMENT_RESULT.md` - Prior vtable test
- `CRASH_EARLY_STARTUP_ITERATION.md` - Historical notes

### Active Investigation
- `CRASH_50_ANALYSIS.md` - Latest crash pattern (EIP=arg2+3)
- `NEXT_STEPS.md` - Actionable next steps
- `CRASH_EIP_003E2B82.md` - Crash family documentation

## Open Questions

1. Why does callback test crash at different point than normal run?
2. Is the execution path through client.dll or mediator?
3. What code stores/calls arg2?

## Recommendation

Focus on static analysis of:
1. client.dll around InitClientDLL (0x620012a0)
2. Areas touching arg2 (0x4d2c60 equivalent in client)
3. Selection context handling (0x62170f4e)
