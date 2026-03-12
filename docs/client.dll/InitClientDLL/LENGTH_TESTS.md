# Path Length Tests - Results

## Test Matrix

| Test | argv[0] | Length | arg2 Address | Crash EIP | Offset |
|------|---------|--------|--------------|-----------|--------|
| Normal | Full path "Z:\home\..." | 46+ | 0x003e3b38 | 0x003e3b3b | +3 |
| Short | "X" | 1 | 0x003e5d50 | 0x003e5d52 | +2 |
| Exe Only | "resurrections.exe" | 17 | 0x003e5d50 | 0x003e5d52 | +2 |

## Key Finding

Even **17 characters** ("resurrections.exe") causes overflow at **arg2 + 2**.

The client's buffer must be **very small** (< 17 bytes) or the overflow happens with any meaningful path.

## Analysis

### Hypothesis: Buffer is ~8-12 Bytes

If buffer is small (e.g., `char buf[12]`):
- Full path (46 bytes): overflow by 34 bytes → crash at +3
- Short "X" (1 byte): overflow by ... hmm, 1 byte shouldn't overflow much

Wait - even with "X" we get crash at +2. This suggests:
1. Overflow happens even with minimal strings, OR
2. Overflow is not the only/dominant issue

### Hypothesis: Multiple Factors

The crash might require:
1. Some string processing (any length)
2. Combined with missing/NULL other args

Without proper arg5/arg6/arg7, the string processing might write to wrong location.

## Implication

**Simply shortening argv[0] is NOT sufficient** - the overflow/corruption happens even with short strings.

## Root Cause Reassessment

Maybe the issue is NOT string overflow but:

1. **Missing arg5/arg6** - Client expects objects, gets NULL
2. **Client writes to arg5/arg6** - Dereferences NULL, corrupts adjacent memory
3. **arg2 happens to be adjacent** - Gets corrupted as side effect

## Stack Layout Possibility

```
 High addresses
  ...
  arg2 ptr      <- Points to argv[0]
  arg1 (argc)
  ret addr      <- InitClientDLL return
  saved EBP     <- Previous frame
  local vars    <- arg5, arg6 local copies
  ...
 Low addresses
```

If arg5/arg6 are NULL and client writes to them, it might corrupt arg2 pointer.

## Test Priority Shift

Since length doesn't prevent crash, focus on:
1. **Providing proper arg5** (launcher object)
2. **Providing proper arg6** (ILTLoginMediator)
3. **Static analysis** of what InitClientDLL expects

The crash at arg2 is likely a **side effect** of missing required objects.

## Current Best Theory

The client crashes because:
1. InitClientDLL receives incomplete/NULL arg5, arg6, arg7
2. Client tries to use these NULL pointers
3. Writes to invalid memory (NULL pointer dereference)
4. Corrupts adjacent data, potentially including arg2 area
5. Later code executing arg2 bytes

**Fixing arg2 length won't help** - need to fix the root cause (missing objects).
