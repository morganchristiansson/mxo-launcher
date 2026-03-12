# Arg2 Investigation - COMPLETE: Hypothesis Disproven

## Status: INVESTIGATION COMPLETE

**Finding:** arg2 is correctly treated as `char**`, NOT as a callback function.

## Test That Disproved the Hypothesis

**MXO_ARG2_CALLBACK_TEST=1** (2026-03-12)

### Test Setup
- Set arg2 to point to executable `ret` instruction
- Expected: If arg2 is callback, client would execute `ret`
- Actual: Client crashed trying to **read** arg2[0] as data pointer

### Critical Evidence
```
EAX = cccccc c3  <- Value from arg2[0]
Crash: Access violation reading target=ccccccca
EIP = 0040fd5b   <- In launcher code, NOT at arg2
```

**Conclusion:** Client dereferences arg2 as `char**`, not as function pointer.

## Correct Understanding

Per original launcher.exe disassembly at 0x40a55c:
```asm
mov ecx, [0x4d2c60]    ; Load arg2 = pointer to argv array
push ecx                ; Pass as arg2
```

And client.dll correctly receives:
```c
int InitClientDLL(
    uint32_t argc,
    char** argv,        // <- arg2 is argv array
    ...
);
```

## Why Did We Think Arg2 Was The Problem?

Previous crashes showed `EIP = arg2 base`, which suggested execution at arg2.
However, the callback test proved:
1. Client reads arg2[0], doesn't execute it
2. The crash at arg2 base was likely a **side effect** of different corruption

## Current Status Of Understanding

| Aspect | Correct Understanding |
|--------|---------------------|
| arg2 Type | `char**` (argv array) ✓ |
| arg2 Usage | Data reads, not execution ✓ |
| Crash at arg2 | Side effect, not cause ✗ |
| Actual Crash Source | **UNKNOWN** - needs new investigation |

## Documentation of Experiments

1. **VTable Experiment** (MXO_ARG2_AS_VTABLE)
   - Result: Crash at arg2 base, not at slot0
   - Now understood: Client read slot0 as data, crashed on dereference

2. **Static Fake Experiment** (MXO_ARG2_STATIC_FAKE)
   - Result: Crash at old heap address
   - Now understood: Client stored arg2 internally, used later

3. **Callback Test** (MXO_ARG2_CALLBACK_TEST)
   - Result: Crash reading arg2[0] as data
   - Definitive proof: arg2 is data, not code

## Revised Interpretation of Earlier Data

All previous experiments are consistent with `char**` usage:
- ESI = arg2+0x0c: Client was iterating argv (slot 3)
- Crash at arg2 base: Coincidence or stack corruption
- Client caching arg2: Normal behavior for later dereference

## Next Investigation Priority

Since arg2 is not the issue, focus on:
1. **Stack corruption** around InitClientDLL return
2. **Other function pointers** being corrupted
3. **Mediator callback paths** in arg5/arg6
4. **Selection context** handling at +0xec

## Related Documentation

- `CALLBACK_TEST_RESULT.md` - Detailed test results
- `CRASH_EARLY_STARTUP_ITERATION.md` - Earlier crash patterns
- `README.md` - Correct InitClientDLL signature
