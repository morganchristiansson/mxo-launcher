# launcher global `0x4d3368`

## High-confidence identity

`0x4d3368` is the global main launcher object used as `this` in the original startup path that eventually calls:

- `0x40a380`
- `0x40a780`
- `0x40a420`
- `0x40a4d0`

The current best canonical name is:

- **`CLauncher` global object**

## Evidence

### Global constructor
Static initializer at `0x494bd0`:

```asm
mov ecx, 0x4d3368
call 0x4097f0
```

So the object is globally constructed in `.data` at `0x4d3368`.

### Constructor
Ctor `0x4097f0`:

- calls base ctor via imported thunk `0x48ba5a`
- sets vtable to `0x4abfe0`
- initializes fields including:
  - `+0xa4 = 0`
  - `+0xa8 = 0xffffffff`
  - `+0xb0 = 0`

### String evidence
The binary contains class-name strings:

- `CLauncher`
- `CLauncherThread`

and the binary/source path strings point directly at the launcher codebase:

- `\matrixstaging\game\src\launcher\launcher.cpp`
- `c:\matrixstaging\game\matrix\launcher.pdb`

## Startup-method usage
Within startup driver `0x40b430`, the object is passed as `ecx`/`this` into the original path methods:

```asm
mov ecx, ebx
call 0x40a380
...
mov ecx, ebx
call 0x40a780
...
mov ecx, ebx
call 0x40a420
...
mov ecx, ebx
call 0x40a4d0
```

So the `this` used at the `InitClientDLL` call site is the global object at `0x4d3368`.

## Important field relation
Because the object base is `0x4d3368`, the arg7 source fields at the `InitClientDLL` call site are:

- `[this+0xa8]` = `0x4d3410`
- `[this+0xac]` = `0x4d3414`

These should now be treated as object fields on `CLauncher`, not as unrelated globals.

## Relevant internal segments inside `0x40b430`

For the current no-patch runtime path, the post-parse portion of `CLauncher::InitInstance`
contains a few static segments that matter more than any invented helper boundaries:

- `0x40b739`: `AfxInitRichEdit()`
- `0x40b740`: `this->0x40a380()` (`Launcher_InitializeThreadPerClientTCPEngine`)
- `0x40b74d..0x40b752`: `0x402ec0()` pre-client thread / message bringup gate
- `0x40b75a..0x40b790`: optional autodetect dialog path when `0x4d2c64 != 0`
- `0x40b790..0x40b7af`: `_access(DAT_004d4cbc,0)` plus `0x41ab10(0)` side gate
- `0x40b7af..0x40b7da`: `this->0x40a780()`, `this->0x40a420()`, `this->0x40a4d0()`, then
  `this->0x40a760()` / `this->0x40a7a0()` cleanup

Implication for the replacement source:

- keep exact claims attached only to the anchored helpers above
- keep any extra source helpers that synthesize mediator / arg7 / nopatch state labeled as
  replacement-only groupings inside the broader `0x40b430` path
- do **not** present those source helpers as proven original subroutine boundaries unless new static
  evidence appears

## Open questions

- Which inherited base class does `CLauncher` derive from through the imported vtable thunks in `0x4abfe0`?
- Which startup methods on `CLauncher` are responsible for populating `+0xa8/+0xac` before `InitClientDLL`?
