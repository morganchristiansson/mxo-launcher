# Buffer Overflow Confirmed (2026-03-12)

## Critical Experiment: MXO_SHORT_ARGV0=1

**Test:** Replace argv[0] (exe path) with single character "X"

### Control Case (Normal Path)
```
argv[0] = "Z:\home\morgan\MxO_7.6005\resurrections.exe"
arg2 = 0x003e3b38
Crash EIP = 0x003e3b3b = arg2 + 3
EBP = 0x00000001 (corrupted)
```

### Test Case (Short "X")
```
argv[0] = "X"
arg2 = 0x003e5d50 (different heap alloc)
Crash EIP = 0x003e5d52 = arg2 + 2  <- MOVED!
EBP = 0x00000001 (still corrupted)
```

## Conclusion: Buffer Overflow Confirmed

The crash location **changed** from arg2+3 to arg2+2 when argv[0] was shortened.

**Root cause:** Some code copies argv[0] into a fixed-size buffer, causing overflow that overwrites the return address with a pointer near arg2.

## Mechanism

Original hypothesis:
```cpp
// Somewhere in client or mediator:
char buffer[4];  // Too small
strcpy(buffer, arg2[0]);  // "Z:\home\..." overflows
// Overwrites: return address with &arg2
// Function returns to arg2, executes pointer bytes
```

Evidence:
1. EBP = 0x00000001 (corrupted frame pointer)
2. Crash at arg2 + offset (varies with argv[0] length)
3. Stack shows InitClientDLL args intact (overflow from different frame)

## Disassembly at New Crash Point

With short "X", arg2 bytes are:
```
arg2[0] pointer = 0x003e5d70 (little-endian: 70 5d 3e 00)

Disassembly at crash (0x003e5d52 = arg2 + 2):
0x003e5d50: 70           jo (jump if overflow)
0x003e5d51: 5d           pop ebp
0x003e5d52: 3e 00        ds: add [eax], al  <- CRASH
```

Crash at arg2+2 vs arg2+3 because:
- Normal: long path → overflow writes return addr pointing to arg2+3
- Short: "X" → shorter overflow → return addr points to arg2+2

## Source of Overflow

Need to find code that copies arg2[0] without bounds checking. Candidates:

1. **client.dll!InitClientDLL** itself - processes argv
2. **mediator callbacks** - get profile name from +0x38
3. **Options.cfg handling** - path formatting

## Next Investigation

### Immediate Priority
Find where the overflow happens:
```
1. Instrument all strcpy/memcpy in our mediators
2. Check if mediator's +0x38 (GetProfileRootName) has overflow
3. Search client.dll for strcpy with arg2-related inputs
```

### Pattern to Search
```asm
; x86 pattern for overflow via strcpy
lea dest, [ebp-0xNN]    ; Local buffer on stack
push src                ; arg2[0] or derived
push dest
call strcpy             ; No bounds check
; dest overflow overwrites return address
```

## Validation

Overflow theory explains:
- Why callback test crashed at different point (tried to deref 0xCCCCCCC3 first)
- Why EBP is always 0x00000001 (corrupted)
- Why crash offset changes with argv[0] length
- Why execution happens at arg2 (return address overwritten)

## Immediate Action

Add canary detection before InitClientDLL:
```cpp
void* canary_before = __builtin_return_address(0);
int result = InitClientDLL(...);
void* canary_after = __builtin_return_address(0);
if (canary_before != canary_after) {
    Log("STACK SMASHED: Return addr changed!");
}
```
