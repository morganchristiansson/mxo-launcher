# Next Steps - Finding What Calls Arg2

## Confirmed: Something Executes Arg2 Array

From Crash 50:
- EIP = 0x003e3b3b = arg2 + 3
- Code executes bytes: `50 3b 3e 00` (argv[0] pointer in little-endian)
- Disassembly: `push eax; cmp ...; add [eax], al`

**This is NOT a coincidence. Something deliberately calls arg2.**

## Three Possibilities

### 1. Direct Call Bug
```cpp
void** callback_array = arg2;  // Wrong: got argv instead of function
callback_array[0]();            // Tries to call argv[0] content
```

But callback test showed client READS arg2[0], so this might not be it.

### 2. Stack Corruption
Some code overwrites return address with arg2:
```
Normal:    call InitClientDLL
           ... (mediator setup)
           return to launcher

Corrupted: call InitClientDLL
           [stack overflow writes arg2 over return address]
           return to arg2 (treating argv bytes as code)
```

### 3. Callback Registration Gone Wrong
Mediator or client stores wrong pointer:
```cpp
// Client registers callback for later
RegisterCallback(arg2);  // Wrong! Should be &arg2[0] or function

// Later:
InvokeCallback();  // Calls arg2
```

## Next Investigation

### Immediate Step: Add Return Address Guard Pages

Protect the stack return address region to detect overflow:

```cpp
// Before InitClientDLL
void* return_addr = __builtin_return_address(0);
Log("DIAGNOSTIC: InitClientDLL will return to %p", return_addr);

// After InitClientDLL return
void* new_return = __builtin_return_address(0);
if (new_return != return_addr) {
    Log("CORRUPTION: Return address changed from %p to %p!",
        return_addr, new_return);
}
```

### Step 2: Instrument All Callbacks

Add logging to all function pointer stores in mediator:
- `+0x08 RegisterEngine` - logs what pointer is stored
- `+0x170 AttachStartupContext` - capture context pointer
- Check if any client-side callback equals arg2

### Step 3: Validate Heap Consistency

Add heap integrity checks:
```cpp
// Before InitClientDLL
void* arg2_before = g_FilteredArgv;

// After InitClientDLL
void* arg2_after = g_FilteredArgv;
if (arg2_before != arg2_after) {
    Log("HEAP CORRUPTION: arg2 changed from %p to %p!",
        arg2_before, arg2_after);
}
```

### Step 4: Static Analysis of Client

Disassemble client.dll around:
1. `InitClientDLL` entry to find what stores arg2
2. `0x62170f4e` caller of ConsumeSelectionContext
3. Any indirect call sites that might use arg2

## Critical Trace Points

Add logging at:
1. **Before/after InitClientDLL** - capture return address
2. **Every mediator method** - check if callee address matches arg2
3. **Exception handler** - log full context when crash happens
4. **Thread creation** - if client spawns thread with bad start address

## Expected Outcomes

If stack corruption:
- Return address will change between entry and crash
- Stack will show overflow pattern

If callback stored wrong:
- Some callback registration will show arg2 being saved
- Invocation will be indirect through that callback

If direct call bug:
- Find code that does `call arg2` directly
- This would be in client.dll disassembly
