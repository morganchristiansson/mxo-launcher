# Investigation: Why is execution jumping to filteredArgv?

## Core mystery

FilterArgv base address is being executed as code. The bytes there are pointer values, not instructions.

Actual memory at filteredArgv (0x003e2bb8):
```
Address    Content        Meaning
003e2bb8:  08 40 3e 00    pointer to argv[0] data (0x003e4008)
003e2bc4:  00 00 00 00    NULL terminator
003e2bc8:  de c0 ad de    sentinel 0xDEADC0DE
003e2bcc:  be ba fe ca    sentinel 0xCAFEBABE
```

Disassembly of pointer values:
```
003e2bb8: 08 40         OR [EAX+40], CL
003e2bba: 3e 00 00      DS: ADD [EAX], AL
003e2bbd: 00 00         ADD [EAX], AL
003e2bbf: 00            ???
003e2bc0: 00            ???
003e2bc1: 00            ???
003e2bc2: 00            ADD [EAX], AL
003e2bc4: 00            (argv[1] content - ESI points here!)
```

## Why ESI = &argv[3]?

If the client code does:
```
mov eax, [some_structure]   ; loads 0x003e2bb8 (filteredArgv)
mov esi, [eax+0xc]            ; loads 0x00DEADC0DE (our sentinel) into ESI
```

Then tries to:
```
call [eax]                    ; calls address at [0x003e2bb8] = 0x003e4008 (string)
```

OR:
```
jmp eax                       ; jumps to 0x003e2bb8 directly (CRASH)
```

## Hypothesis: Virtual call with wrong vtable pointer

If some object has:
- vtable pointer = filteredArgv (0x003e2bb8)
- field at +0xC = ESI target

Then virtual call through vtable[0] would:
1. Load vtable address = 0x003e2bb8
2. Call function at vtable[0] = jump to 0x003e4008... wait, that's the string

Actually no - vtable[0] would be CONTENT of first pointer:
- vtable[0] = 0x003e4008 (the string data address, not code)

So a virtual call would try to run the string data, not the pointer array!

Wait - the CRASH is at 0x003e2bb8, not 0x003e4008. So the jump went to the vtable address, not to vtable[0].

This suggests a DOUBLE indirect: calling the vtable pointer itself as code, not calling through it.

Hypothesis: Code does:
```cpp
void* vtable = obj->vtable;  // loads 0x003e2bb8
call vtable;                  // WRONG - should be call [vtable]
```

Instead of:
```cpp
call [obj->vtable];           // Correct indirect call
```

So the bug is somewhere that stores filteredArgv as a callback but forgets to dereference it.

## Key question: What is at Caller=0x62170f4e?

ConsumeSelectionContext was called from client.dll+0x62170f4e.

Around this address, the code might be setting up a callback or vtable pointer.

We need to see if this code writes filteredArgv (arg2) somewhere that later gets called incorrectly.

## Next action: Make argv[0] a valid code address

Created `g_ExecutableArgv0Trampoline` - a buffer with `ret` instruction. Experiment: point filteredArgv[0] to this trampoline. If client calls through slot 0, it will return safely and we can log the call.

However, the crash at filteredArgv BASE (not argv[0] contents) suggests the issue is different. The problem is likely:

```cpp
void* callback = filteredArgv;  // Wrong: storing array pointer
call callback;                   // Jumps to filteredArgv base
```

Instead of:
```cpp
void* callback = filteredArgv[0]; // Correct: storing first element
call callback;                   // Would call argv[0] contents
```

So the bug is likely a missing level of indirection somewhere in the client setup code that copies arg2.

## Alternative approach: Track callback writes

Since we know the crash happens after ConsumeSelectionContext, instrument the client to log writes to function pointer fields. This will catch the bad assignment before crash.

This requires either:
1. Debugging with memory breakpoints in client.dll
2. Static analysis of client.dll around 0x62170f4e
3. Code injection to hook writes

Most practical for now: static analysis with Ghidra/IDA of the code around 0x62170f4e to understand what should happen after ConsumeSelectionContext.
