# Arg2 Direct Call Hypothesis

## CONFIRMED: Client Stores and Calls Arg2 Directly

The static fake experiment provided definitive evidence:

### Experiment Result:
- Set `g_FakeArgv` at static address `0x00411040`
- arg2 filteredArgv logged as `0x00411040` (correct)
- **BUT crash at EIP = `0x003e5e42`** (old heap address)

This proves:
1. Client received arg2 = `0x003e5e42` (heap) at InitClientDLL time
2. Client STORED this value internally
3. Later, client calls that stored value: `call 0x003e5e42`
4. Current arg2 value (`0x00411040`) was NOT used - confirms stored copy

## The Problem

The client code pattern:
```cpp
// During InitClientDLL:
void* stored_callback = arg2;  // Saves 0x003e5e42

// Later:
call stored_callback;  // Calls 0x003e5e42 (filteredArgv base)
```

The client expects arg2 to be callable, but we pass `char**` (pointer to string array).

## Possible Explanations

### Explanation 1: Wrong Interface Understanding
The InitClientDLL signature in reimplementation:
```cpp
int InitClientDLL(
    uint32_t filteredArgCount,  // arg1
    char** filteredArgv,         // arg2 - WE PASS THIS
    HMODULE hClientDll,
    HMODULE hCresDll,
    void* launcherNetworkObject,
    void* pILTLoginMediatorDefault,
    uint32_t packedArg7Selection,
    uint32_t flagByte
);
```

But maybe the CLIENT expects:
```cpp
int InitClientDLL(
    uint32_t argc,
    void* callback,              // NOT argv - a callback function!
    HMODULE hClientDll,
    ...
);
```

### Explanation 2: Context-Dependent Arg2
Maybe arg2 serves dual purpose:
- In some modes: `char** argv`
- In other modes: `void* callback`

The client behavior changes based on some condition (arg5? arg6? arg7?).

### Explanation 3: Structure Confusion
Maybe arg2 is supposed to be a pointer to a structure like:
```cpp
struct StartupConfig {
    void (*callback)();  // Slot 0: function pointer
    char** argv;         // Slot 1: argv array
    int argc;            // Slot 2
    ...
};
```

And we're passing `&argv` instead of `&config`.

## Critical Test

To determine which explanation is correct:

**Test:** Make arg2 point to executable code (a `ret` instruction)
- If client runs without crash → Explanation 1 or 2 (expects callback)
- If client crashes differently → Need different approach

Implementation:
```cpp
// Create executable buffer with 'ret'
static uint8_t ret_instruction = 0xC3;

// Set arg2 to point directly to it
char** fake_arg2 = reinterpret_cast<char**>(&ret_instruction);
InitClientDLL(..., fake_arg2, ...);
```

If this works, we know arg2 must be executable code, not data.
