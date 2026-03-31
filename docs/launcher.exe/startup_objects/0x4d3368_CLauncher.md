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

## Entry-flow relation

The PE entrypoint is `0x48be94`, which resolves CRT startup and then calls:

```asm
AfxWinMain(GetModuleHandleA(NULL), NULL, commandLineTail, showWindow)
```

So the original launcher reaches `CLauncher::InitInstance` through the normal MFC GUI startup,
not through a custom top-level launcher stub. The replacement `main()` is therefore only a bounded
host-side stand-in for that MFC path.

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
  - newer decompilation now makes this tighter:
    - allocates `0xb4` bytes for the arg5 `CLTThreadPerClientTCPEngine`
    - runs `CLTThreadPerClientTCPEngine_ctor(this, 0)`
    - immediately registers the object with `ILTLoginMediator.Default` through vtable slot `+0x08`
    - practical negative result: this helper itself does **not** show username/password prompt or
      character-selection logic; it is engine construction/registration
- `0x40b74d..0x40b752`: `0x402ec0()` pre-client thread / message bringup gate
  - newer decompilation now makes this tighter too:
    - starts an MFC worker thread with `AfxBeginThread`
    - waits on worker-side readiness bytes/fields
    - pumps the launcher message queue with `PeekMessage/TranslateMessage/DispatchMessage`
    - posts `WM_QUIT` to that thread and waits for completion
    - practical negative result: this looks like launcher pre-client environment bringup, not the
      missing password-submit / character-choice API
- `0x40b75a..0x40b790`: optional autodetect dialog path when `0x4d2c64 != 0`
- `0x40b790..0x40b7af`: `_access(DAT_004d4cbc,0)` plus `0x41ab10(0)` side gate
  - newer decompilation now closes this more tightly too:
    - `_access(...)` checks the DXDiag-style output file path at `DAT_004d4cbc`
    - on missing file it calls `0x41ab10(0)`, which creates/overwrites that file through DXDiag
      COM interfaces and returns
    - practical negative result: this is a diagnostics-file side gate, not the missing auth /
      character-selection authorization step
- `0x40b7af..0x40b7da`: `this->0x40a780()`, `this->0x40a420()`, `this->0x40a4d0()`, then
  `this->0x40a760()` / `this->0x40a7a0()` cleanup

## New tightening on the real pre-corridor gate question

The current best static read is now narrower:

- the straight-line corridor at `0x40b7af..0x40b7c7` is real and unconditional once reached
- the helpers immediately before it inside `InitInstance` (`0x40a380`, `0x402ec0`, autodetect,
  `_access` / `0x41ab10`) do **not** look like the original username/password re-prompt or
  character-choice authorization boundary
- so the real launcher-owned gate that delays progress before client load is more likely in an
  **earlier launcher UI / observer-driven phase that must complete before control returns to this
  tail of `CLauncher::InitInstance`**

A concrete new anchor on that earlier phase is the launcher selection writer:

- `launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl`
- this helper operates on a launcher-owned `CListCtrl`-style object
- at `0x40d763..0x40d810` it:
  - reads the selected row's packed item data
  - consults sibling `ILTLoginMediator.Default` slot `0x4d3584` through methods `+0xfc`, `+0x100`,
    and `+0xe4`
  - persists `Last_WorldName`
  - writes the final split selection into `CLauncher+0xa8/+0xac`
    (`0x4d3410 / 0x4d3414`)
- newer upstream producer tightening from `0x40e480` now explains that packed list-row item data:
  - low 16 bits = total-world index from sibling slot `+0xfc`
  - high 16 bits = matching active-world index when sibling slot `+0xe0` returns the same world
    name, otherwise `0xffff`
  - negative result: sibling slot `+0xe0` is a string getter used for world-name matching there,
    not a bool gate

New surrounding launcher-UI tightening now narrows the upstream caller path too:

- `0x4047d0`
  - launcher dialog/page switcher over owner byte/dword state at `CLauncher+0x74...`
  - newer review of case `7` is the important selection-page anchor:
    - shows the list control at `CLauncher+0xa0c`
    - registers observer callback `+0x170` when needed
    - immediately posts UI command `0x0f` through `0x405a20` to disable selection/continue controls
    - then calls the world-list population helper at `0x40e6c0`
  - practical read: this is the launcher-owned selection-page setup that precedes later
    `0x40d530/0x40d820` interaction
- `0x405a20 = LauncherLoginDialog_DispatchUiCommand`
  - case `8` calls `0x40d6f0`
  - on success it continues through patch-check / launch-side logic and eventually exits that UI path
  - newer negative result from `0x405a20 + 0x40ac00`:
    - this success path immediately enters patch/launch-side work
    - it is **not** itself the mediator-owned selection commit boundary `0x41c390/0x41c1f0`
- `0x40d530 = LauncherSelectionList_OnSelectionChanged`
  - samples the current list selection through the same sibling mediator slot `0x4d3584`
  - validates it through the same `+0x100 -> 0x40cdb0 -> (+0x54 when gate byte is 2/5) -> +0xe4`
    family later reused by `0x40d6f0`
  - posts UI command ids `0x0f`, `0x10`, `0x11`, or `0x12` back into `0x405a20`
  - newer `0x405a20` read keeps the practical split narrower than before:
    - `0x0f` disables the selection/continue controls
    - `0x10/0x11` enable the same path with different button visuals/state
    - `0x12` is a separate launcher-side status/action branch, not the final mediator commit
  - practical read: this is launcher-side enable/disable/status control for the selection action,
    not the final arg7 writeback itself
- `0x40d820 = LauncherSelectionList_OnDoubleClickActivate`
  - posts UI command id `8` back into `0x405a20`
  - practical read: launcher double-click activation routes into the same selection-resolve action

Practical consequence:

- `CLauncher+0xa8/+0xac` are not merely late synthetic launch parameters
- they are written by a launcher-owned selection UI path before the client-load corridor
- but `0x40d6f0` still looks like **selection resolution/writeback**, not the whole blocking gate by
  itself
- current best mediator-side **selection commit** anchors are now best treated as a pair:
  - `0x41c390 = CLTLoginMediator_SetSelectionIndexAndSwitchToState7`
  - `0x41c1f0 = CLTLoginMediator_PersistSelectionContextAndSwitchToState8`
- launcher CLI-faithfulness consequence from the newer pass:
  - original `-user` / `-pwd` should be treated as launcher-login **prefill + auto-submit** inputs,
    not as permission to bypass the launcher-owned login flow
  - a no-GUI replacement therefore should not require `-char` up front
  - instead it should submit credentials through the same launcher-owned auth path first, then only
    after successful auth adopt a recovered character selection and mirror the selection commit into
    the mediator-side `0x41c390 -> 0x41c1f0` boundary before client load
- current bounded source consequence:
  - the no-GUI launcher bridge now does exactly that paired owner-side commit
  - but the surrounding `0xb4` snapshot for `0x41c1f0` is still only partially recovered: current
    source fills the highest-confidence fields (slot/index, character id pair, world id,
    descriptor index, current arg7-selection summary) while the original launcher UI producer for
    the rest of the block remains unresolved
  - newer owner-side tightening from `0x41c1f0 + 0x43bd20` matters for how far that bridge claim
    can go:
    - state8 later serializes the persisted snapshot blocks in packet order
      `0x09/0x19/0x29`, `0x79/0x89/0x99/0xa9`, then `0x39/0x49/0x59/0x69`
    - but the packet's fixed GCID dwords still come separately from the current slot record, not
      from persisted snapshot block `+0xcd0[0..1]`
    - so the bridge's seeded character-id pair should still be treated as a bounded persisted-input
      guess, not as proof that state8 slot3 reads those exact dwords back as the outbound GCID
- so the remaining high-value question is the caller/message path that bridges launcher UI selection
  into those mediator-side writers and only then lets `InitInstance` resume into `0x40b7af`

Implication for the replacement source:

- keep exact claims attached only to the anchored helpers above
- keep any extra source helpers that synthesize mediator / arg7 / nopatch state labeled as
  replacement-only groupings inside the broader `0x40b430` path
- do **not** present those source helpers as proven original subroutine boundaries unless new static
  evidence appears

## Open questions

- Which inherited base class does `CLauncher` derive from through the imported vtable thunks in `0x4abfe0`?
- Which launcher dialog / command handler / observer wait path calls `0x40d6f0` and blocks until the
  user has completed login + character selection before `InitInstance` falls through to
  `0x40b7af..0x40b7c7`?
- How does that earlier UI path synchronize with the mediator-side state3 selection-context writers
  `0x41c390 / 0x41c1f0` on the successful-auth branch?
