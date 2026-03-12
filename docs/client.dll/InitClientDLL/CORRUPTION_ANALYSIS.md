# Stack Corruption Analysis - Crash 51

## Critical Finding: EBP Corrupted

```
EIP:  0x003e3b3b (arg2 + 3)
EBP:  0x00000001  <- CORRUPTED! Should be ~0x0065fdxx
ESP:  0x0065fcc4
EAX:  0x00000001
```

**EBP = 0x00000001 is invalid** - normal EBP should point to previous stack frame.

## Interpretation: Return Address Overwritten

Standard x86 call sequence:
```
call InitClientDLL
  -> push return_addr
  -> push ebp        (frame setup)
  -> mov ebp, esp
```

If EBP is corrupt, likely happened:
1. Stack buffer overflow during InitClientDLL
2. Return address overwritten with arg2+3
3. Function "returned" to arg2+3 (our code)
4. New frame setup failed (EBP=1 is garbage)

## Stack Layout Evidence

```
0x0065fcc4: 00000001 62000000 10000000 003e0710
              argc    client    cres     launcher
0x0065fcd4: 0041ab40 00000000 00000000 0000006d
              mediator
```

This is InitClientDLL **argument push order**, not the actual call frame.

The real return address should be higher on stack, but we don't see it because:
- Crash happened after InitClientDLL "returned" (to wrong address)
- Stack unwound or corrupted

## Arg2 Execution Confirmed

Disassembly at arg2:
```
0x003e3b38: 50           push eax
0x003e3b39: 3b 3e        cmp edi, [esi]
0x003e3b3b: 00 ...       add [eax], al  <- CRASH (EAX=1)
```

The bytes `50 3b 3e` match the **little-endian pointer value 0x003e3b50**:
- arg2[0] = pointer to "Z:\home\...\resurrections.exe"
- Little-endian bytes: 50 3b 3e 00

## Hypothesis: Callback Test vs Normal Run

| Scenario | arg2[0] Value | Client Behavior | Result |
|----------|--------------|-----------------|--------|
| Callback test | 0xCCCCCCC3 | Tries to dereference as pointer | Crash early (data access) |
| Normal run | 0x003e3b50 | Executes as code | Crash at arg2+3 (code execution) |

The difference: **Valid pointer (0x003e3b50) allows reaching code execution path.**

## Root Cause Candidates

### Candidate 1: Stack Smashing in strcpy
If client does:
```cpp
strcpy(buffer, arg2[0]);  // "Z:\home\...\resurrections.exe"
```
And buffer is too small, overflow overwrites return address.

### Candidate 2: Format String Bug
If client trusts arg2[0] content:
```cpp
printf(arg2[0]);  // Format string vulnerability
```

### Candidate 3: Intentional Callback (but wrong value)
Some code path legitimately calls arg2, but with wrong understanding of type.

## Testing Strategy

### Test 1: Shorten arg2[0]
Replace exe path with short string to avoid overflow:
```cpp
g_FilteredArgv[0] = "X";  // Single char
```
If crash moves or changes, confirms overflow theory.

### Test 2: Guard Page
Put arg2 at page boundary with protetion to catch exact access.

### Test 3: Stack Canaries
Add canary values before InitClientDLL call.

## Next Immediate Step

Run with shortened argv[0]:
```bash
MXO_SHORT_ARGV0=1 make run_binder_both
```

If crash at different address (not arg2+3), confirms string overflow theory.
