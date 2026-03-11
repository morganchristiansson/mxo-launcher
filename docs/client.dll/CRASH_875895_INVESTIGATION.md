# Crash at 0x875895 - Deep Investigation

## Date: 2025-03-11

## Key Findings

### 1. Memory at Crash Address

Using radare2 at `client.dll+0x875895`:
```
0x00875895  invalid
0x00875896  invalid
0x00875897  invalid
...
```

All bytes are `0xFF` - this is **NOT CODE**, it's uninitialized/padding data!

### 2. Code Section Bounds

```
.text section: 0x62001000 - 0x62877000 (size 0x867000)
```

The crash at `0x62875895` is at offset `0x875895`, which is:
- Within the .text section bounds (0x62001000 to 0x62877000)
- But that specific area contains 0xFF padding

### 3. Mystery

Why is execution jumping to an address containing 0xFF?

Theories:
1. Function pointer in a vtable is wrong (points to padding)
2. Indirect call using corrupted/bad index
3. Branch target calculation overflow
4. We're using the wrong vtable entirely

### 4. Register State

```
EAX: 620af9d0    - Our vtable address (correct)
EBX: 629ddfc8    - Unknown object
ECX: 62999968    - Our injected StateObject
EDX: 00000002    - Parameter
```

### 5. Hypothesis

The crash happens because:
1. Code loads function pointer from vtable (correct)
2. Somehow miscalculates offset/index
3. Jumps to wrong address: `vtable_base + wrong_offset` = 0x875895
4. Lands in padding -> executes 0xFF 0xFF (invalid)

**OR**

1. Code does `call [eax+offset]` with offset that's too large
2. Jumps into unused space between functions

### 6. Why Our Fix Didn't Work

We set field_28 to 0x62006c40, but either:
- This isn't the function being called
- Something else is overwriting it
- The vtable index being used is wrong

---

## Next Steps

1. Find the actual instruction that jumps to 0x875895
2. Trace backward to see how calculation went wrong
3. Check what other vtable indices are being used
4. Verify our StateObject layout matches expectations

---

## Related Files

- `GLOBAL_OBJECT_VTABLES.md` - Vtable analysis
- `CRASH_875895_ANALYSIS.md` - Initial crash analysis
- `test_fixed_state.cpp` - Test code

---

**Status**: Deep investigation required - need to trace the jump
