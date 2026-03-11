# client.dll!RunClientDLL

## Current status

`RunClientDLL` still crashes in the custom reimplementation after `InitClientDLL` returns success.

## High-confidence interpretation

The crash is currently treated as evidence that the custom launcher still does **not** reproduce the original launcher-owned environment that `client.dll` expects.

It is **not** currently treated as evidence that the final solution requires direct writes into `client.dll` globals.

## Current crash reference

### Crash site
- `client.dll+0x3b3573`
- `EIP = 0x623b3573`

### Observed state
- `ECX = 0`
- `EBX = 0x62999968`
- faulting instruction dereferences `ECX`

## What the crash tells us

The client reaches runtime that expects additional state to have been established earlier in startup.
Because the original launcher does more work before `RunClientDLL` than our current host does, the safest interpretation is:

- missing launcher-side prerequisites,
- not missing ad-hoc injected client objects.

## What not to conclude

Do **not** conclude from the crash that the proper launcher must:
- patch `client.dll` memory,
- inject timer/state objects,
- seed internal globals directly.

Those experiments only showed that the crash site can be moved.
They did not demonstrate original behavior.

## Current dependency chain

The canonical prerequisites now point back to:

- `../InitClientDLL/README.md`
- `../../launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../launcher.exe/nopatch/README.md`

## Next task

Identify which launcher-owned objects and transitions, missing before `RunClientDLL`, are normally provided by:

1. the object at `0x4d2c58`,
2. the object at `0x4d6304`,
3. the original nopatch setup path,
4. the `cres.dll` + `client.dll` startup order.
