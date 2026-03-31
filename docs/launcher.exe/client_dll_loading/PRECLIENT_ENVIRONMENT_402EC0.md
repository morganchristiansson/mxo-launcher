# launcher.exe pre-client environment setup (`0x402ec0`)

## Source of truth

- call site: `launcher.exe:0x40b74d -> 0x402ec0`
- this happens **after** `0x40a380` builds `0x4d6304`
- this happens **before** `cres.dll` and `client.dll` are loaded

## What the function does at a high level

`0x402ec0` performs launcher-side environment setup and waits for that setup to become ready before the client startup path continues.

It is not part of `client.dll` and it is not optional launcher fluff.

## Static evidence

### Starts `CLauncherThread`
The function begins with `AfxBeginThread((CRuntimeClass *)0x4aa134, ...)`.
The runtime-class record at `0x4aa134` contains the class-name string:

- `CLauncherThread`

and its create-object hook is now tightened as:

- `0x403750 = CLauncherThread_CreateObject`

### `CLauncherThread` init allocates the real launcher dialog/controller object
The thread's init path is now tightened too:

- `0x407580 = CLauncherThread_InitInstance`
  - allocates a separate `0xb30` CWnd-derived launcher dialog/controller object
  - stores it at thread `+0x48`
  - calls `0x406470` to create the window and child controls
  - shows the window on success
  - sets thread byte `+0x44` on init failure

So the object `0x402ec0` waits on is not just an abstract thread shell.
It is waiting for a worker thread whose `InitInstance` creates the real launcher dialog object.

### Waits for dialog creation and readiness
`0x402ec0` repeatedly polls the returned thread object:

- thread `+0x48` = dialog/controller pointer
- thread `+0x44` = init-failure flag
- thread `+0x45` = thread/dialog exit-complete flag

When thread `+0x44 == 0`, it also waits for:

- dialog `+0x68 != 0`

The dialog initializer `0x406470` sets that `+0x68` byte after the window and controls are created.
So `0x402ec0` blocks until the launcher dialog path is genuinely live.

### Pumps the process message queue while blocked
After the dialog is ready, `0x402ec0` repeatedly runs:

- `PeekMessage`
- `TranslateMessage`
- `DispatchMessage`

until thread `+0x45` becomes non-zero.
Then it posts `WM_QUIT` (`0x12`) to the worker thread and waits again.

This is a real pre-client launcher UI gate, not optional fluff.

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

`0x402ec0` is the outer pre-client launcher UI gate used by `CLauncher::InitInstance`.
Current best concrete corridor:

- start `CLauncherThread`
- thread `InitInstance` allocates the real launcher dialog/controller object
- wait for dialog `+0x68` ready
- pump process messages while that launcher dialog runs
- successful selection later exits through launcher dialog code such as:
  - state-7 page setup `0x4047d0`
  - selection observer `0x40f070`
  - command dispatcher `0x405a20`
- that dialog path eventually sets `DAT_004d259c` and posts quit
- only then does `0x402ec0` return so startup can continue into the DLL-load corridor

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

- What is the concrete class name of the separate `0xb30` launcher dialog/controller object allocated by `CLauncherThread_InitInstance`?
- Which exact dialog method corresponds to the `0x406470` create-and-initialize body?
- Is thread byte `+0x45` set by dialog destruction, by `ExitInstance`, or by a narrower thread helper on the quit path?

## Updated priority note

New `InitClientDLL` experiments suggest the old immediate `-7` barrier is more directly explained by arg6 (`ILTLoginMediator.Default`) than by `0x402ec0`.

So `0x402ec0` remains important for original-faithful startup, but it is no longer the leading explanation for the first observed `InitClientDLL = -7` failure.
