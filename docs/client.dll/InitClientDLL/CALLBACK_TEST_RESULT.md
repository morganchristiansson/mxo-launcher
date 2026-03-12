# Callback Test Result - HYPOTHESIS DISPROVEN

## Test: MXO_ARG2_CALLBACK_TEST=1

**Status:** COMPLETE - arg2 is NOT called as a function

## Test Design

Set arg2 to point to executable memory containing:
```
00411030: c3 cc cc cc cc cc cc cc  (ret + int3 padding)
00411038: 00 00 00 00              (padding)
```

**Hypothesis if arg2 is callback:**
- Client would execute `ret` at 00411030
- Either return safely OR crash on int3
- **BUT EIP would be 00411030**

**Hypothesis if arg2 is argv:**
- Client reads arg2[0] = 0xCCCCCCC3 (little-endian: c3 cc cc cc)
- Tries to dereference as string pointer
- **Crashes trying to read from invalid address**

## Result: ARG2 IS DATA (char**), NOT CALLBACK

```
=== Unhandled exception ===
exception code=c0000005 flags=00000000 address=0040fd5b
access violation kind=read target=ccccccca
registers:
  eip=0040fd5b esp=0065fcc0 ebp=0065fef8
  eax=ccccccc3 ebx=00000004 ecx=003e2b50 edx=00000002
```

**Critical evidence:**
- EAX = `0xCCCCCCC3` - Exactly the value stored at arg2[0]!
- Target = `0xCCCCCCCA` - Client tried to read from address derived from our sentinel
- EIP = `0x0040FD5B` (in our code), NOT at arg2 address

**Interpretation:**
1. Client read arg2[0] into EAX
2. Client tried to dereference EAX as pointer
3. 0xCCCCCCC3 is invalid address → access violation

## Conclusion

**ARG2 IS CORRECTLY TREATED AS char** BY CLIENT**

The original launcher was right: arg2 is a pointer to argv array, not a callback function.

**The crash at arg2 base in previous runs was a RED HERRING** - likely a side effect of memory corruption elsewhere, not the direct cause.

## Implications

### What we know:
1. arg2 is correctly loaded as `char**`
2. Client expects arg2[0] to be a valid string pointer
3. The crash is NOT from calling arg2 as code

### What we don't know:
1. Why does crash EIP sometimes equal arg2 base?
2. What is the ACTUAL source of control flow corruption?
3. Is the crash in a different code path than we thought?

## Next Investigation Focus

Since arg2 usage is correct, the crash must be from:
1. **Stack corruption** - Return address overwritten
2. **Callback storage elsewhere** - Some other pointer being corrupted
3. **Mediator object issue** - Something in arg5/arg6 path
4. **Different crash family** - Multiple overlapping issues

### Immediate next steps:
1. Restore normal arg2 (heap-allocated argv)
2. Look for other callback/pointer storage
3. Check if crash is consistent with stack smashing pattern
