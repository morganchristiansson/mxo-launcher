# Buffer Overflow Source Analysis

## Confirmed: Overflow in InitClientDLL

The crash happens **during** InitClientDLL execution, with return address overwritten to point at arg2.

## Timeline

```
1. InitClientDLL called
2. Mediator methods called (AttachStartupContext, ProvideStartupTriple, etc.)
3. ConsumeSelectionContext (+0xec) - last logged mediator call
4. [CRASH - execution at arg2+offset]
```

The overflow happens between step 3 and crash, within InitClientDLL.

## Suspected Locations

Based on docs, client.dll has path helpers:
- `0x62195f00` - formats `Profiles\%s\`
- `0x62195ff0` - formats `Profiles\%s\%s_%X\`

But these use profile name ("resurrections"), not full exe path.

## Likely Culprit: Direct arg2[0] Processing

The exe path: `Z:\home\morgan\MxO_7.6005\resurrections.exe`
- Length: 46 characters (46+ with null = 47 bytes)

If client code has:
```cpp
char buffer[32];  // Fixed small buffer
strcpy(buffer, arg2[0]);  // 47 bytes into 32-byte buffer
// Overflows by 15 bytes, likely hitting return address
```

## Pattern Evidence

| argv[0] | Length | Crash Offset |
|---------|--------|--------------|
| Long path | 46+ | arg2 + 3 |
| "X" | 1 | arg2 + 2 |

Shorter overflow with shorter string confirms calculation based on length.

## Hypothesis: Client Copies Exe Path

Somewhere in InitClientDLL, client code does:
```cpp
// Get executable path for logging/config
char exePath[MAX_PATH];  // But actually smaller buffer
strcpy(exePath, arg2[0]);  // arg2[0] = path to resurrections.exe
```

Overflow overwrites return address with exePath pointer = &arg2.

## Where to Look in client.dll

Search patterns:

### Pattern 1: Direct strcpy with arg2
```asm
; At InitClientDLL entry or early
mov eax, [ebp+arg2]      ; Load arg2
mov ecx, [eax]            ; Load arg2[0]
lea edx, [ebp-0x20]       ; Local buffer (32 bytes)
push ecx                  ; Source: exe path
push edx                  ; Dest: local buffer
call strcpy              ; Overflow
```

### Pattern 2: String operations on startup
```asm
; Look for string ops using arg-derived values
push offset "Profiles\\%s\\%s"
mov eax, [arg2_ptr]
push [eax]                ; exe path
push [profile_name]
lea ecx, [ebp-0x40]       ; Result buffer
call sprintf              ; May overflow
```

### Pattern 3: Window title / command line storage
```asm
; Window creation uses exe name
push [arg2_exe_name]      ; "resurrections.exe"
lea eax, [ebp-0x20]       ; Window text buffer
call strcpy
```

## Immediate Test

Add canary to detect overflow source:

```cpp
// In launcher, before InitClientDLL
char canary[8] = "OVERFLW";
memset(canary, 0xAA, sizeof(canary));  // Pattern

InitClientDLL(...);

// Check after return
for (int i=0; i<sizeof(canary); i++) {
    if (canary[i] != 0xAA) {
        Log("CORRUPTION at offset %d", i);
    }
}
```

## Static Analysis Required

Disassemble client.dll `InitClientDLL`:
1. Look for `strcpy`, `sprintf` calls
2. Check first 100 instructions for stack setup
3. Find code referencing arg2 (filteredArgv)
4. Check stack frame size vs buffer sizes

Entry point: `client.dll:620012a0`

## Resolution Priority

**High**: Find if our stub code can prevent the overflow or if we need to provide shorter path
**Medium**: Identify exact overflow function for documentation
**Low**: Patch client (not feasible) or work around in launcher

## Workaround Hypothesis

Can we prevent crash by:
1. Providing shorter argv[0]? (Yes! MXO_SHORT_ARGV0=1 delayed crash)
2. But this may break legitimate exe path usage
3. Need to find minimum safe length
