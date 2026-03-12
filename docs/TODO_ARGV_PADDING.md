# TODO: Argv Padding Implementation

## Problem
The client crashes by executing code at `filteredArgv` address, suggesting it either:
1. Expects a vtable/callback array at filteredArgv
2. Iterates past the NULL terminator looking for more data
3. Treats filteredArgv as a structure with embedded function pointers

## Current Code Location
File: `src/resurrections.cpp`
Around line: `g_FilteredArgvOwned[filteredCount] = NULL;`

## Proposed Experiment

Add after line 928:
```cpp
// Add padding slots to detect if client iterates past terminator
if (g_FilteredArgvOwnedCapacity > filteredCount + 4) {
    g_FilteredArgvOwned[filteredCount + 1] = reinterpret_cast<char*>(0xDEADBEEF);
    g_FilteredArgvOwned[filteredCount + 2] = reinterpret_cast<char*>(0xCAFEBABE);
    g_FilteredArgvOwned[filteredCount + 3] = reinterpret_cast<char*>(0x11223344);
    g_FilteredArgvOwned[filteredCount + 4] = NULL;  // Double terminator
}
```

## Expected Outcomes
1. If crash EIP changes to 0xDEADBEEF or nearby, proves client reads past terminator
2. If crash pattern unchanged, suggests different cause
3. If ESI in crash dump changes, helps correlate structure expectations

## Also Consider
- Allocate argv at page boundary with guard page +1
- Fill padding with recognizable stack canary patterns
- Log the padding values at startup so we can confirm in crash dump
