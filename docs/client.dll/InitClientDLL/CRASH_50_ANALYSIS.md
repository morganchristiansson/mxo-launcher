# Crash 50 Analysis - New Pattern

## Crash Location

```
Crash EIP: 0x003e3b3b
arg2:      0x003e3b38
Difference: +3 bytes (EIP = arg2 + 3)

Stack at crash:
0x0065fcc4: 00000001 62000000 10000000 003e0710
              ↑ argc     client    cres     launcher
0x0065fcd4: 0041ab40 00000000 00000000 0000006d
              ↑ mediator
```

## arg2 Memory Layout

```
arg2 (0x003e3b38):
  [0x003e3b38] = 50 3b 3e 00 (argv[0] = 0x003e3b50)
  [0x003e3b3c] = 00 00 00 00 (argv[1] = NULL)
  [0x003e3b40] = 00 00 00 00 (argv[2] = NULL)
  [0x003e3b44] = 00 00 00 00 (argv[3] = NULL)
  
Crash at: 0x003e3b3b (inside argv[0] pointer bytes!)
          0x003e3b3b = 0x003e3b38 + 3
          Points to byte 3 of argv[0] pointer
```

## Analysis

The crash is **NOT** at arg2 base (0x003e3b38).
The crash is **NOT** at arg2[0] content (0x003e3b50).

The crash is at **arg2[0] pointer itself + 3 bytes**!

Looking at memory:
- arg2[0] value = 0x003e3b50 (little-endian: 50 3b 3e 00)
- Crash at 0x003e3b3b

Wait - 0x003e3b3b is between arg2 and arg2[0]!

Actually, looking more carefully:
- arg2 array starts at 0x003e3b38
- arg2[0] pointer is at 0x003e3b38-0x003e3b3b (4 bytes: 50 3b 3e 00)
- Crash at 0x003e3b3b is **byte 3 of the pointer value itself**

## Hypothesis: Code is Executing argv Array Structure

If code somehow got the address of arg2 and started executing:
- First 4 bytes (0x003e3b38-0x003e3b3b): `50 3b 3e 00`
- Disassembly: `50` = push eax, `3b` = cmp, `3e` = ds: prefix, `00` = add [eax], al

Crash at 0x003e3b3b means it executed:
- 0x003e3b38: 50 = push eax
- 0x003e3b39: 3b = cmp (incomplete, continues)
- 0x003e3b3a: 3e = ds: prefix
- 0x003e3b3b: 00 = add [eax], al  <-- CRASH (access violation)

**EAX = 0x00000001 at crash.**

This confirms: **Code is executing the argv array as instructions!**

## Why Previous Callback Test Didn't Show This?

The callback test set arg2 to 0x00411030 (static .data section) with special values. The crash occurred because:
- Arg2[0] = 0xCCCCCCC3 (our sentinel)
- Client tried to dereference this as string pointer
- Not execute it

But in normal run:
- Arg2[0] = 0x003e3b50 (heap pointer to exe path)
- Some code path executes arg2 array

## The Key Question

**What triggers execution of arg2 array?**

Two possibilities:
1. **Direct call to arg2**: `call arg2` (but callback test showed this reads arg2[0])
2. **Corrupted return address**: Something overwrites stack return with address near arg2

## Reproducibility

This pattern is consistent across multiple runs. Latest crash_51 (2026-03-12):
```
EIP: 003e3b3b  (arg2 + 3)
arg2: 003e3b38
ESI:  003e3b44  (arg2 + 0x0c = &arg2[3])
```

## Analysis Pending

Need to determine what calls arg2. Options:
1. Direct call/jump in client code
2. Indirect call through corrupted pointer
3. Stack corruption with arg2 address

Next step: Static analysis of client.dll to find call sites near arg2-related code.

