# launcher.exe nopatch path

## Source of truth

- branch at `launcher.exe:0x409a52`
- helper parsing call at `0x417440`

## Key finding

`-nopatch` does **not** mean "skip launcher setup and call client.dll directly".
It means "skip the patch pipeline, but still initialize launcher-side runtime state for the nopatch path".

## Relevant branch

```asm
0x409a52: push 0x4ac190      ; "-nopatch"
0x409a57: call edi           ; string compare helper
0x409a5c: test eax, eax
0x409a5e: jne  0x409c47      ; not nopatch -> other branch

0x409a64: push 0x4ac18c      ; "0.1"
0x409a69: mov  [0x4c8b1d], al
0x409a6e: call 0x417440      ; parse "0.1"
0x409a73: mov  ecx, [0x4d2c58]
...
0x409a8b: call [edx+0x1c]

0x409a8e: push 0x4ac18c      ; "0.1"
0x409a93: call 0x417440
0x409a98: mov  ecx, [0x4d2c58]
...
0x409ab0: call [edx+0x24]
```

## What this means

In the original launcher, the nopatch branch:

1. detects the `-nopatch` argument,
2. updates launcher-global state,
3. parses the string `"0.1"`,
4. invokes methods on the launcher-owned object at `0x4d2c58`.

So even in nopatch mode, launcher object `0x4d2c58` is actively configured before the client startup path.

## Reimplementation guidance

Our replacement launcher should default to nopatch behavior, but it must still reproduce the launcher-side state transitions that the original nopatch branch performs.

Skipping the patch DLLs is correct.
Skipping launcher-side nopatch initialization is not.
