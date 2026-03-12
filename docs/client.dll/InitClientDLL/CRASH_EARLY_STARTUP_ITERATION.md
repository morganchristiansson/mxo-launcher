# Early Startup Crash Iteration Notes

## Session Summary: 2026-03-12

### Padding experiment result: SAME CRASH LOCATION

Added padding sentinels after argv terminator:
- `filteredArgv[1] = 0xDEADC0DE`
- `filteredArgv[2] = 0xCAFEBABE`
- `filteredArgv[3] = 0xBEEFCAFE`
- `filteredArgv[4] = NULL`

Result: **Crash still at EIP = 0x003e2bb8 = filteredArgv base**
- The padding didn't change the crash location at all
- Client is not "walking off" the end of argv
- Client is jumping to the START of filteredArgv, not past it

### Refined hypothesis: Indirect jump via vtable/callback

The crash at filteredArgv base (not into padding) suggests:
1. Some code stored `filteredArgv` as a pointer (maybe void* or callback)
2. Later code dereferences that pointer expecting a function/vtable
3. Jump goes to `*filteredArgv` which would be `filteredArgv[0]` contents

But wait - the bytes at 0x003e2bb8 would be the first pointer in the array:
- `0e 40 3e 00` in little-endian = pointer to string

So executing there would start with `0e` (PUSH CS) - not obviously useful code.

Alternative: Maybe the code stores a pointer TO filteredArgv somewhere, treating it as a vtable/structure, then calls offset 0 from that structure?

### Key question: Why is ESI = &filteredArgv[3]?

ESI = filteredArgv + 0x0c = &filteredArgv[3]

This suggests code was iterating or accessing element 3. But with only 1 argv element, filteredArgv[3] would be our padding sentinel (0xBEEFCAFE).

Maybe the client thinks filteredArgv is a larger structure? Like a vtable or object with 4+ slots?

### Current State (2026-03-12 session)

### Confirmed crash family: `EIP = filteredArgv`

Crash dump analysis (crash_39.dmp) reveals:
```
EIP: 003e2bb8
ESP: 0065fcc8
arg2 filteredArgv from log: 003e2bb8  ← EXACT MATCH with EIP

Stack at crash:
0x0065fcc8: 62000000 10000000 003e0710 0041ab40
              ↓          ↓         ↓         ↓
           client.dll cres.dll  launcher  mediator
                                   object
```

The crash is executing filteredArgv memory as code. The stack shows InitClientDLL argument pattern, suggesting the crash happens during or immediately after InitClientDLL returns.

### Timeline leading to crash

Last mediator calls before log truncation:
```
MediatorStub::ConsumeSelectionContext(0065fbe0) [count=1 caller=62170f4e copied=0041a760]
ConsumeSelectionContext copied @ 0041a760 [+0x00]=00000000 ...
```

The crash happens after ConsumeSelectionContext (+0xec) but before/during the next expected mediator calls (+0xf4, +0x148, +0x174, +0x178).

### Critical observation: ESI context

From winedbg ESI = `0x003e2bc4` = filteredArgv + 0x0c = &filteredArgv[3]

This suggests code was iterating through argv or expected a structure at argv+offset.

### Hypothesis: argv structure mismatch

The filtered argv currently has:
- filteredArgv[0] = "Z:\home\morgan\MxO_7.6005\resurrections.exe"
- filteredArgv[1] = NULL (terminator)
- filteredArgCount = 1

But the client may expect:
- A vtable/callback pointer array following argv
- Specific count/argc=argc relationship
- Non-null entries at argv[2], argv[3], etc.

### New diagnostics effectiveness

✓ Argv memory snapshot taken before InitClientDLL
✓ Exception handler correctly identifies crash in argv memory
✗ Exception handler didn't fire (crash caught by game or wine first)

### Next experiments priority

1. **Sentinel injection**: Place recognizable magic values at expected crash offsets to identify which pointer is being dereferenced wrong

2. **Argv padding**: Add 4-8 NULL slots after argv as a safety buffer

3. **Arg7 variation**: Test with non-zero arg7 selection to see if crash offset changes

4. **Double-check selection context**: The +0xec handoff may need additional fields populated

### Current arg7 state

```
arg7 packed = 0x00000000
a8 = 0x00000000 (variant high byte)
ac = 0x00000000 (world low 24 bits)
```

This is the "standalone" default. Previous crashes with `MXO_MEDIATOR_SELECTION_NAME=Vector` had slightly different addresses but same pattern.

### Action items

- [ ] Implement argv memory protection pages with sentinels
- [ ] Add padding slots to filteredArgv
- [ ] Test if crash offset correlates with argv content vs pointer
- [ ] Check if ESI offset (argv+0xc) hints at specific expected structure
### Refined hypothesis: Indirect jump via vtable/callback

The crash at filteredArgv base (not into padding) suggests:
1. Some code stored `filteredArgv` as a pointer (maybe void* or callback)
2. Later code dereferences that pointer expecting a function/vtable
3. Jump goes to `*filteredArgv` which would be `filteredArgv[0]` contents

Log evidence:
```
arg2 pointer array @ 003e2bb8:
  argv[0] = 003e4008  -> "Z:\home\morgan\MxO_7.6005\resurrections.exe"
  argv[1] = 00000000  -> NULL terminator  
  argv[2] = deadc0de  -> padding sentinel
  argv[3] = cafebabe  -> padding sentinel
```

### CRITICAL: ESI = &filteredArgv[3] = 0x003e2bc4

From crash dump (crash_41.dmp):
- ESI = 0x003e2bc4 = filteredArgv + 0x0c = **&argv[3]**
- So the code **knew about argv[3]** - it was accessing or iterating to it!

### Revised theory: Client confusion about arg2 structure

Hypothesis: **The client thinks filteredArgv is not an argv array but a vtable/object structure**

The consistent ESI = &argv[3] across crashes suggests the client accesses slot 3 of what it thinks is a vtable. This means:

1. Something in the setup told client that arg2 is a pointer to a structure with 4+ slots
2. The client used slot 0 as a function pointer (crash at filteredArgv[0])
3. The client used slot 3 as data (ESI = &argv[3])

Possibility: Maybe arg2 is SUPPOSED to be a pointer to a structure where:
- slot 0 = a callback function
- slot 3 = some data/context

The original launcher might pass a different structure, not a filtered argv array?

### What arg2 is supposed to be

Looking at original launcher.exe disassembly around 0x409950:
- arg2 is built from argv after processing launcher switches
- It's passed as `char**` to InitClientDLL
- But maybe the CLIENT expects something different...

Alternative hypothesis: The client may be corrupting its own data structures, and filteredArgv happens to reside at an address that matches some expected pattern.

### Next experiment: Control filteredArgv content at slot 0

Since the crash tries to execute argv[0], let's try making argv[0] point to:
1. A ret instruction (to see if we can survive the call)
2. An int3 (to see if we can breakpoint there)
3. A recognizable pattern to confirm execution

This will tell us definitively if code is jumping to argv[0].
