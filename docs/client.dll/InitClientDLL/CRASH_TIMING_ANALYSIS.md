# Crash Timing Analysis

## Observation: Different Crash Points

### Normal Path (Long Exe Path)
```
InitClientDLL called
...mediator activity...
[Crash at arg2+3 during InitClientDLL]
[No "InitClientDLL returned" logged]
```

### Short "X" Path
```
InitClientDLL called
[Crash early at arg2+2]
[Even less mediator activity]
```

## Key Finding: Both Crash DURING InitClientDLL

Neither case reaches "InitClientDLL returned" log message, meaning crashes happen inside InitClientDLL, not on return.

## Refinement: Nested Function Overflow

Stack model:
```
main()
  calls InitClientDLL()
    calls helper1()
      calls helper2() <- OVERFLOW HERE
        [Corrupts helper2's return address]
        [Returns to arg2+offset]
        [Executes arg2 bytes]
        [CRASH]
```

The corrupted return address is **helper2's return to helper1**, not InitClientDLL's return.

## EBP = 0x00000001 Explained

```
helper2 frame:
  push ebp      <- EBP saved (legitimate value)
  mov ebp, esp
  ...
  [Overflow writes over saved EBP on stack]
  ...
  pop ebp       <- Restores 0x00000001 (garbage from arg2)
  ret           <- Returns to arg2+offset
```

## Implication: Multiple Potential Overflow Sites

Different crash offsets suggest different overflow amounts:
- Longer string → overwrites more stack → different return address value
- Shorter string → less overwrite → different crash point

## Hypothesis: sprintf/format Functions

Most likely overflow in:
- `sprintf(dest, "format", exePath, ...)` - Exe path embedded in format string
- `strcpy` of exe path into struct/log entry
- Command line building with format specifier

## Client.dll Search Priority

Search `client.dll!InitClientDLL` (0x620012a0) for:
1. **Immediate string operations** (first 50 instructions)
2. **sprintf family calls**
3. **Path construction functions**
4. **Callbacks using arg2[0] content**

```asm
; Pattern: Load arg2 early
mov ecx, [ebp+0x0c]    ; arg2
mov edx, [ecx]           ; arg2[0]

; Then format/copy
push edx
push offset "Profiles\\%s\\"
lea eax, [ebp-0x20]     ; Small buffer
call sprintf            ; Overflow
```

## Critical Code Pattern

If client does:
```cpp
void InitClientDLL(int argc, char** argv, ...) {
    char configPath[32];  // Too small
    sprintf(configPath, "Config\\%s", argv[0]);  // Overflow
    // ...later code never reached
}
```

The overflow happens **immediately** on entry, before mediator calls.

## Test: Add Guard Page

Place arg2 in memory with guard page after it:
```cpp
// Allocate arg2 at page boundary
// Set next page to no-access
// If overflow writes past arg2, triggers SIGSEGV immediately
```

This would catch the exact overflow point with stack trace.

## Immediate Workaround

Provide exe path as just filename ("resurrections.exe"):
```cpp
// Instead of full path:
// "Z:\home\morgan\MxO_7.6005\resurrections.exe"
// Use:
// "resurrections.exe"
g_FilteredArgvOwned[0] = strdup("resurrections.exe");
```

If crash moves to later in execution or succeeds, confirms path-length dependency.

## Next Test

Replace argv[0] with "resurrections.exe" (17 chars) instead of "X" (1 char):
- Should be short enough to avoid overflow
- But meaningful enough for client to use
- Test with: `MXO_EXE_NAME=resurrections.exe make run_binder_both`
