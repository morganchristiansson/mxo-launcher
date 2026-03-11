# launcher.exe -> client.dll loading sequence

## Source of truth

- startup driver around `launcher.exe:0x40b739..0x40b7d5`
- client export resolution/calls at `launcher.exe:0x40a4d0..0x40a6fb`
- helper loaders at `0x40a380`, `0x40a420`, `0x40a780`

## Canonical sequence

### 0. Build filtered client argv state
Call site on the broader startup path:
- `0x40b595 -> 0x409950`

What `0x409950` does:
- allocates launcher-owned argv-like storage
- stores count/pointer at `0x4d2c5c` / `0x4d2c60`
- strips launcher-owned switches such as `-clone`, `-silent`, and `-nopatch`
- forwards the remaining arguments into the filtered array used later by `InitClientDLL`
- probes `options.cfg` during this preprocessing stage
- sets launcher-global state based on that `options.cfg` probe, including flag `0x4d2c64`

### New clarification: how forwarding actually happens

Static tracing of `0x409950` now supports this more precise model:

1. get CRT `argc` / `argv`
2. allocate a new pointer array sized from CRT `argc`
3. zero that array
4. initialize `0x4d2c5c = 0`
5. walk original CRT argv one entry at a time
6. for each entry:
   - compare against launcher-owned switches via `_stricmp`
   - if matched, consume it into launcher-global state instead of forwarding it
   - if not matched, duplicate the string into a fresh heap buffer and store the duplicated pointer into `0x4d2c60[0x4d2c5c]`
   - then increment `0x4d2c5c`

The strongest forwarding proof is the sequence around `0x409d8f..0x409dba`:

```asm
push eax                    ; length + 1
call 0x403260              ; allocate duplicated string
mov  ecx, [0x4d2c60]
mov  edx, [0x4d2c5c]
mov  [ecx + edx*4], eax    ; store duplicated char* into filtered argv array
...
inc dword [0x4d2c5c]       ; increment filtered count
```

So the client-facing argv passed to `InitClientDLL` is **not raw CRT argv and not just a borrowed pointer list**.
It is a launcher-built filtered argv array containing **fresh duplicated strings** for forwarded entries.

This is the strongest current evidence that `InitClientDLL` arg1/arg2 are **filtered launcher-owned storage**, not raw CRT `argc` / `argv`.

### New clarification: `options.cfg` is not just probed, it gates a pre-client launcher step

Static tracing now shows this relationship:

- `0x409950` performs `_stat("options.cfg")` and time/date checks
- if the probe path decides the temporary config is relevant, it sets launcher flag `0x4d2c64 = 1`
- later startup at `0x40b75a` reads `0x4d2c64`
- if set, startup constructs a helper object on the stack and calls `0x401520`
- after that helper returns, startup destroys the helper object and calls `0x4012e0`
- `0x4012e0` is the known `DeleteFileA("options.cfg")` cleanup path

`0x401520` itself is now much better understood:

- it constructs an MFC-style dialog/helper object
- it brings the launcher window to the top
- it launches:
  - `autodetect_settings.exe setopts hide`
- it waits up to `0xea60` milliseconds (60 seconds)
- on success, it records the child process exit code through `GetExitCodeProcess`
- it then calls `0x4013c0`, which populates UI strings like:
  - `Default Settings:`
  - `Detail:`
  - `Memory:`
  - `Continue`
  using `Low` / `Medium` / `High` text derived from the recorded result bytes

So `options.cfg` is part of a real launcher-owned preprocessing branch before the client-loading sequence continues.
The strongest current model is:

1. `launcher.exe` notices relevant temporary config state,
2. launches `autodetect_settings.exe setopts hide`,
3. consumes the resulting settings/result state through launcher-owned code,
4. cleans up `options.cfg`,
5. and only then continues toward `cres.dll` / `client.dll` loading.

This is currently better supported than any claim that the launcher literally injects `+Windowed 1` into forwarded argv.

### 1. Build launcher object at `0x4d6304`
Call site:
- `0x40b740 -> 0x40a380`

What `0x40a380` does:
- allocates `0xb4` bytes
- constructs a launcher-owned object
- stores it at `0x4d6304`
- registers / hands it through the runtime interface pointer stored at `0x4d2c58`

Canonical object docs:
- `../startup_objects/0x4d6304_network_engine.md`
- `../startup_objects/0x4d2c58_ILTLoginMediator_Default.md`

### 2. Perform additional launcher environment setup
Call site:
- `0x40b74d -> 0x402ec0`

This happens before any `client.dll` entry point is called.

Canonical note:
- `PRECLIENT_ENVIRONMENT_402EC0.md`

Current best interpretation:
- launcher thread / message / event environment setup that blocks until ready

### 3. Load `cres.dll`
Call site:
- `0x40b7b1 -> 0x40a780`

What `0x40a780` does:
- `LoadLibraryA("cres.dll")`
- stores the handle at `0x4d2c4c`

### 4. Load `client.dll`
Call site:
- `0x40b7bc -> 0x40a420`

What `0x40a420` does:
- `LoadLibraryA("client.dll")`
- stores the handle at `0x4d2c50`

### 5. Resolve client exports
Call site:
- `0x40b7c7 -> 0x40a4d0`

Resolved in order:
- `InitClientDLL`
- `RunClientDLL`
- `TermClientDLL`
- `ErrorClientDLL`

### 6. Call `InitClientDLL`
Still inside `0x40a4d0`:
- arguments prepared at `0x40a55c..0x40a5a4`
- exact mapping documented in `../../client.dll/InitClientDLL/README.md`

### 7. Call `RunClientDLL`
On the successful path inside `0x40a4d0`:
- `call [ebp-0x14]` invokes `RunClientDLL`

### 8. Cleanup after the client phase
After the main client path returns:
- `0x40b88a -> 0x40b360` tears down `0x4d6304` / `0x4d2c58` related state
- `0x40b88f -> 0x40a000` frees parsed-argument storage from `0x4d2c60`

## Hard boundary from static analysis

In this traced startup path, `launcher.exe`:
- loads modules,
- resolves exports,
- calls `InitClientDLL`,
- calls `RunClientDLL`,
- but does **not** directly call `client.dll!SetMasterDatabase()`.

## High-confidence conclusions

1. `cres.dll` is part of the original startup path and is passed to `InitClientDLL`.
2. The launcher does significant work before calling `InitClientDLL`.
3. A minimal host that only loads `client.dll` and passes mostly `NULL` arguments is not equivalent to the original launcher.
4. No client-memory injection is required by the original launcher path we are trying to reproduce.

## Current implication

The current custom launcher is still missing launcher-owned setup that happens before `RunClientDLL`. That is the primary line of investigation.

A fresh 2026-03-11 diagnostic dump from the project-local `launcher_proper.exe` confirms:

- `cres.dll` can be present on the crashing path,
- the real 8-argument `InitClientDLL` frame shape can be used,
- but if `InitClientDLL` still returns `-7` and runtime is forced anyway,
- `client.dll` later crashes at `0x623b3573`.

So the remaining mismatch versus original `launcher.exe` is not just module load order.
It is the missing launcher-owned state established before a legitimate `RunClientDLL` call.
