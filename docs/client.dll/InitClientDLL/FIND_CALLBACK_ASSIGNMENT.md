# Find Where Arg2 Gets Stored as Callback

## Confirmed: Arg2 Base Address is the Callback Target

The `MXO_ARG2_AS_VTABLE=1` experiment definitively showed:
- argv[0-3] contain valid code addresses (nops+ret, etc.)
- **BUT crash EIP = 0x003e5e58 = filteredArgv base (arg2)**
- NOT at argv[0] (`0x00411020`)

This means somewhere in client.dll:
```cpp
void* obj->callback = arg2;  // WRONG - storing array pointer
// Later:
call [obj->callback]  // or call obj->callback? Either way, ends up at arg2
```

## Where to Look

### Location 1: After ConsumeSelectionContext (+0xec)

At `client.dll:0x62170f4e` (caller per crash log):
- Client builds stack object at `[ebp-0xbc]` (0xb4 bytes)
- Calls `mediator->+0xec(original_stack_object_copy)`
- What happens to arg2 in that area?

### Location 2: During AttachStartupContext (+0x170)

Previous mediator callbacks show client is accessing context at offset +0x170.
Maybe arg2 gets stored there?

### Location 3: Global/static callback slot

Some global variable in client.dll stores the callback.

## Investigation Strategy

Since we can't easily breakpoint in client.dll, let's try:

1. **Static analysis**: Disassemble client.dll around 0x62170f4e, look for writes involving arg2

2. **Heuristic experiment**: Move arg2 to a different memory region with different content
   - If crash follows arg2 new location → confirms arg2 pointer is stored
   - If crash stays at old location → confirms we found the stored pointer

3. **Parameter smuggling**: Put recognizable pattern IN arg2 address itself
   - Right now arg2 = some heap address
   - Make arg2 = address of global array with recognizable content

## Heuristic Experiment: Move filteredArgv

Current: filteredArgv = heap allocated pointer array  
Try: filteredArgv = static global array with recognizable pattern

If crash EIP = &g_StaticFakeArgv → confirms callback stores arg2 value

## Static Analysis Target

Look at client.dll around 0x62170f4e for patterns like:
```asm
mov eax, [ebp+arg2]    ; Load arg2 (the filteredArgv pointer)
mov [ecx+offset], eax  ; Store into object field
```

Then later:
```asm
mov eax, [ecx+offset]  ; Load callback
call eax               ; Call through it → crashes at arg2
```

The offset +4 (ESI) suggests slot 3 access (0xc = +12 = slot 3).
So the callback might be slot 0 of a 4-slot structure.
