# Appendix: Recent Investigation Findings

## Arg2 Usage Divergence Investigation

**Status:** In progress - evidence suggests misalignment in arg2 consumption

### Summary of Finding

Recent diagnostic crashes with late-startup `EIP = arg2 base address` suggest the client may treat arg2 differently than expected.

### Evidence from Diagnostic Crashes

Across multiple crash dumps, a consistent pattern emerges:

| Dump | arg2 Address | Crash EIP | Notes |
|------|-------------|-----------|-------|
| crash_39 | 0x003e2bb8 | 0x003e2bb8 | Exact match (arg2 base) |
| crash_40 | 0x003e2bb8 | 0x003e2bb8 | Exact match (arg2 base) |
| crash_41 | 0x003e5e58 | 0x003e5e58 | Exact match (arg2 base) |
| crash_43 | 0x003e5e58 | 0x003e5e58 | Exact match (arg2 base) |

**Key observation:** Crash EIP equals arg2 base address, NOT arg2[0] content.

### Register State Pattern

From crash_39.dmp:
```
EIP: 003e2bb8 (arg2 base)
ESI: 003e2bc4 (arg2 base + 0x0c = &arg2[3])
```

This suggests code pattern:
```asm
mov esi, [arg2-like-structure + 0x0c]  ; Load slot 3
mov eax, [arg2-like-structure + 0x00] ; Load slot 0
call eax                              ; Call slot 0
```

### Experiments Performed

1. **VTable Slot Experiment** (`MXO_ARG2_AS_VTABLE=1`)
   - Replaced argv[0-3] with executable code addresses
   - Result: Crash still at arg2 base, NOT at slot0 address
   - Interpretation: Client stores arg2 itself, not arg2[0]

2. **Static Fake Experiment** (`MXO_ARG2_STATIC_FAKE=1`)
   - Moved arg2 to known static address
   - Result: Crash at OLD heap address, not new static
   - Interpretation: Client caches arg2 value at InitClientDLL time

3. **Callback Test** (`MXO_ARG2_CALLBACK_TEST=1`) - IN PROGRESS
   - Set arg2 to point to executable 'ret' instruction
   - Expected: If client calls arg2 as function, will execute ret

### Hypotheses

**H1: Structure Confusion**
Client expects arg2 to point to a structure:
```cpp
struct Arg2Container {
    void* callback;      // slot 0 - function or pointer
    char** argv;         // slot 1 - actual argv
    int argc;            // slot 2
    void* context;       // slot 3 - ESI target
};
```
But launcher passes `char**` directly.

**H2: Reversed Indirection**
Client expects single indirection (call arg2[0]), but we're passing direct argv.
Or client has bug calling arg2 directly instead of arg2[0].

**H3: Cached Pointer Corruption**
Client legitimately stores arg2 for later, but something overwrites the stored copy.

### Static Analysis Required

To resolve, need disassembly of:
- client.dll InitClientDLL: how arg2 is stored
- client.dll 0x62170f4e area: caller of ConsumeSelectionContext
- client.dll: where stored arg2 value is later called

### Related Documentation

- `ARG2_CALLBACK_HYPOTHESIS.md` - **Complete investigation with finding**
- `CALLBACK_TEST_RESULT.md` - Definitive test result
- `VTABLE_EXPERIMENT_RESULT.md` - Vtable slot experiment outcome
- `CRASH_EARLY_STARTUP_ITERATION.md` - Crash pattern analysis
- `FIND_CALLBACK_ASSIGNMENT.md` - Locating callback storage

### Investigation Result: COMPLETE

**Callback Test (2026-03-12) disproved the hypothesis:**

```
Test: MXO_ARG2_CALLBACK_TEST=1 with arg2 pointing to 'ret' instruction
Result: Crash reading arg2[0] (0xCCCCCCC3), NOT executing at arg2
EAX = 0xCCCCCCC3 <- Value from arg2[0] loaded as data
Target = 0xCCCCCCCA <- Dereference attempted, caused access violation
```

**Conclusion:** arg2 is correctly treated as `char**` by client, NOT as callback.

The consistent `EIP = arg2 base` in earlier crashes was a **side effect** of memory corruption elsewhere, not the direct cause of execution at arg2.
