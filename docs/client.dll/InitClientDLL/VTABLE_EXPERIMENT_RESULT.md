# VTable Experiment Result - BREAKTHROUGH

## Experiment: MXO_ARG2_AS_VTABLE=1

**Status: CONFIRMED - arg2 is being treated as a vtable pointer**

## Evidence

### Setup
Environment: `MXO_ARG2_AS_VTABLE=1` enabled vtable slot experiment.

Vtable slots (8-byte buffers with trap instructions):
- g_vtableSlot0 @ 0x00411020: `{0x90,0x90,0x90,0x90,0xc3,0xcc,0xcc,0xcc}` = nops + ret
- g_vtableSlot1 @ 0x00411018: `{0xeb,0xfe,...}` = jmp to self
- g_vtableSlot2 @ 0x00411010: `{0xf4,...}` = halt
- g_vtableSlot3 @ 0x00411008: `{0xcd,0x03,0xc3,...}` = int3 + ret

### Result

Logged arg2 content:
```
arg2 pointer array @ 003e5e58:
  argv[0] = 00411020  ✓ g_vtableSlot0
  argv[1] = 00411018  ✓ g_vtableSlot1
  argv[2] = 00411010  ✓ g_vtableSlot2
  argv[3] = 00411008  ✓ g_vtableSlot3

arg2 argv[0] data @ 00411020:
  90 90 90 90 c3 cc cc cc  ✓ EXACT slot0 content
```

### CRITICAL: Crash location unchanged!

```
Crash EIP: 003e5e58
arg2 filteredArgv: 003e5e58
```

**The crash is at arg2 BASE, not at argv[0] content!**

## Conclusion

The code is NOT doing:
```cpp
void* slot0 = arg2[0];  // Load argv[0] = 0x00411020
call slot0;              // Jump to 0x00411020 (would execute nops+ret)
```

The code IS doing:
```cpp
void** vtable = arg2;     // Load 0x003e5e58 (filteredArgv itself)
call [vtable];            // Jumps to *0x003e5e58 = would try to execute 0x00411020 as code address
// OR
call vtable;              // Jumps to 0x003e5e58 directly (treating pointer array as code)
```

Actually looking at the bytes:
- At 0x003e5e58: 20 10 41 00 = 0x00411020 (the slot0 address)
- If executed: `20 10` = AND [EAX+0x10], AL - then `41 00` = INC ECX

The CRASH EIP is at filteredArgv base, meaning the code is either:
1. Calling/jumping directly to the `filteredArgv` address (not through it)
2. Dereferencing once: `call *void_ptr` where void_ptr = filteredArgv

## Next Step

The client code has:
```cpp
void* callback = filteredArgv;  // WRONG - should be filteredArgv[0]
callback();                     // Executes filteredArgv memory
```

We need to find WHERE this assignment happens. Options:
1. Static analysis of client.dll around InitClientDLL entry
2. Add memory write tracing to catch the bad assignment
3. Try different arg2 format to match client's expectation

The question is: What should arg2 ACTUALLY be?
