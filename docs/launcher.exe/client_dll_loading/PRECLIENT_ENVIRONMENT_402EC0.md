# launcher.exe pre-client environment setup (`0x402ec0`)

## Source of truth

- call site: `launcher.exe:0x40b74d -> 0x402ec0`
- this happens **after** `0x40a380` builds `0x4d6304`
- this happens **before** `cres.dll` and `client.dll` are loaded

## What the function does at a high level

`0x402ec0` performs launcher-side environment setup and waits for that setup to become ready before the client startup path continues.

It is not part of `client.dll` and it is not optional launcher fluff.

## Static evidence

### Creates / starts a launcher-side object
The function begins with:

```asm
push 0
push 4
push 0
push 0
push 0x4aa134
call 0x48b970
mov  edi, eax
```

The nearby rdata includes the string:

- `CLauncherThread`

which strongly suggests this path constructs or starts a launcher-thread-style object.

### Waits for readiness
It repeatedly polls fields in the returned object:

- `[edi+0x48]`
- `[edi+0x44]`
- `[edi+0x45]`

with `Sleep(10)`-style waits between polls.

That means startup blocks until this launcher-side environment reaches a ready state.

### Uses Windows messaging / event style APIs
The function later loops over an API pair loaded from imports and repeatedly processes objects referenced through a local stack slot.
This is consistent with message/event pumping or queued work processing before the client phase proceeds.

### Uses a class / event identifier
The path pushes:

- `0x4aa134`
- later `0x12`
- and uses readiness flags in the created object

which further supports the conclusion that this is launcher runtime setup, not dead code.

## What it means for reimplementation

The original launcher path is not just:

1. build `0x4d6304`
2. load `cres.dll`
3. load `client.dll`
4. call `InitClientDLL`

It is:

1. build `0x4d6304`
2. perform pre-client environment setup at `0x402ec0`
3. then load `cres.dll`
4. then load `client.dll`
5. then call `InitClientDLL`

So a launcher that skips `0x402ec0` is still missing part of the original path.

## Current best interpretation

`0x402ec0` likely establishes a launcher thread / message / event environment that the rest of the launcher assumes is live before handing off to the client.

## New clarification from current scaffold work

The custom launcher now includes a **diagnostic pre-client environment scaffold** intended to be closer to `0x402ec0` than simply skipping the step entirely.

Current scaffold behavior:

- creates a launcher-owned helper thread before `cres.dll` / `client.dll` load
- forces a thread message queue into existence with `PeekMessage`
- marks diagnostic readiness state analogous to the original polling pattern:
  - `state44 = 0`
  - `state45 = 1`
  - `state48 = non-NULL`
- keeps a simple message pump alive until launcher shutdown

This is still **not** a faithful reconstruction of the original path:

- it does not recreate the real object returned by `0x48b970`
- it does not reproduce the original imported helper sequence around `0x48b88c` / message dispatch exactly
- it does not model the original `CLauncherThread` internal fields beyond a minimal readiness analogue

## Current experiment result with the scaffold enabled

With this diagnostic `0x402ec0`-style scaffold active, the launcher still reaches the same deep client path and then crashes in the same later way:

- deep mediator sequence still reaches first `+0x170`, `+0x124`, second `+0x170`
- late crash still redirects execution into the current `arg2 filteredArgv` area

So the new scaffold improves **startup-shape fidelity**, but it did **not** by itself remove the current post-startup-context crash.

That is useful evidence:

- skipping `0x402ec0` was a real faithfulness gap,
- but the present late crash is not explained solely by the absence of any launcher thread/message environment.

## Open questions

- What exact concrete class is returned by `0x48b970` here?
- Which import functions in the loops correspond to message retrieval / dispatch / wait operations?
- Is this environment required by `InitClientDLL` directly, or by later launcher/client interaction during `RunClientDLL`?

## Updated priority note

New `InitClientDLL` experiments suggest the old immediate `-7` barrier is more directly explained by arg6 (`ILTLoginMediator.Default`) than by `0x402ec0`.

So `0x402ec0` remains important for original-faithful startup, but it is no longer the leading explanation for the first observed `InitClientDLL = -7` failure.
