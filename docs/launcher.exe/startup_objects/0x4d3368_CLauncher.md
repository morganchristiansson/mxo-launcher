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
call 0x40a380  ; Launcher_InitializeThreadPerClientTCPEngine
...
mov ecx, ebx
call 0x40a780  ; CLauncher_LoadCresDLL
...
mov ecx, ebx
call 0x40a420  ; CLauncher_LoadClientDLL
...
mov ecx, ebx
call 0x40a4d0  ; Launcher_RunClientDllLifecycle
```

So the `this` used at the `InitClientDLL` call site is the global object at `0x4d3368`.

## Important field relation
Because the object base is `0x4d3368`, the arg7 source fields at the `InitClientDLL` call site are:

- `[this+0xa8]` = `0x4d3410`
- `[this+0xac]` = `0x4d3414`

These should now be treated as object fields on `CLauncher`, not as unrelated globals.

## New naming pass for `CLauncher::InitInstance`

A focused Ghidra readability pass on `launcher.exe:0x40b430` now uses these helper names for the
called subroutines on the original startup path:

- `0x40a000 = Launcher_FreeFilteredCommandLineStorage`
- `0x40a0c0 = Launcher_HasMultipleLauncherOrMatrixProcesses`
- `0x40a180 = Launcher_CheckExpectedBootstrapParentExited`
- `0x40a300 = Launcher_CopyFileIfMissingOrChanged`
- `0x40a880 = Launcher_PrepareTempMatrixCloneFiles`
- `0x40a900 = Launcher_RelaunchSelfAsTempMatrixCloneAndQuit`
- `0x40aa70 = Launcher_RelaunchLauncherWithoutCloneOrRecoverAndQuit`
- `0x40b360 = Launcher_TeardownThreadPerClientEngineAndMediator`
- `0x41ab10 = Launcher_WriteDxDiagReport`

Important scope note for the replacement launcher:
- the original patch/update corridor copies the launcher into a temp `matrix.exe` and relaunches
  itself there so patching can proceed without normal Windows file-lock conflicts
- current project direction keeps the active replacement on the effective nopatch path and does
  **not** treat that temp self-copy/relaunch behavior as a fidelity target
- so the names above stay as original-path documentation, not as a mandate to recreate that patch
  bootstrap in source

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
- `0x40b7af..0x40b7da`: `this->CLauncher_LoadCresDLL()`, `this->CLauncher_LoadClientDLL()`,
  `this->Launcher_RunClientDllLifecycle()`, then `this->CLauncher_UnloadClientDLL()` /
  `this->CLauncher_UnloadCresDLL()` cleanup

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
  - low 16 bits = total-world / descriptor index from sibling slot `+0xfc`
  - high 16 bits = matching active selection-entry index when sibling slot `+0xe0` returns the
    same world name, otherwise `0xffff`
  - tighter auth-valid read from the paired `+0xe4 = 0x41b2a0` consumer now makes that high word
    better fit the slot-record / character-entry index on page `7`
  - negative result: sibling slot `+0xe0` is a string getter used for world-name matching there,
    not a bool gate

New surrounding launcher-UI tightening now narrows the upstream caller path too:

- `0x4047d0`
  - launcher dialog/page switcher over owner byte/dword state at `CLauncher+0x74...`
  - newer review of case `7` is the important selection-page anchor:
    - shows the list control at `CLauncher+0xa0c`
    - registers observer callback `+0x170` when needed
    - immediately posts UI command `0x0f` through `0x405a20` to disable selection/continue controls
    - then calls the world-list population helper at `0x40e480`
  - practical read: this is the launcher-owned selection-page setup that precedes later
    `0x40d530/0x40d820` interaction
- `0x40f070`
  - callback registered from the same case-`7` selection-page setup
  - on callback code `0x1c` it rebuilds the list through `0x40e480 -> 0x40e1c0`
  - preserves the selected row by low-16 world index across that rebuild instead of by the full
    packed item-data dword
  - on callback code `0x21` it exits through `0x40b8f0` / quit-message path
  - practical read: selection UI is observer-refreshed while the page is live; the list is not just
    a one-shot static producer
- `0x40f180`
  - newer secondary-family tightening now closes `DAT_004d3588` materially:
    - it writes `DAT_004d3588 = columnIndex + 1`
    - rotates per-column phase bytes in `0x004d358c[mode]`
    - when that phase rolls back to `0`, it also clears `DAT_004d3588` to `0`
    - then calls `0x40e1c0`
  - together with comparator `0x40cf40`, current best read is:
    - `DAT_004d3588 = 0` -> insertion-order replay
    - `1` -> compare payload string `+0x08`
    - `2` -> compare payload string `+0x14`
    - `3` -> compare payload string `+0x20`
    - `4` -> compare numeric `atoi(payload + 0x2c)`
    - `5` -> special status-class compare with payload `+0x44` tick-count tiebreak
- `0x40e1c0`
  - repaints the visible list from the launcher-owned node list at `CListCtrl+0x68`
  - newer node-layout tightening from `0x40dbb0/0x40dc40/0x40d8d0/0x40dfd0` now makes that list
    materially more concrete:
    - the real row object is `0x48` bytes
    - layout is:
      - `+0x00/+0x04` = intrusive next/prev pointers
      - `+0x08` = total-world index dword
      - `+0x0c` = active selection-entry index dword (`0xffff` when unmatched)
      - `+0x10/+0x1c/+0x28/+0x34` = four copied display strings
      - `+0x40` = dword availability/sort-class flag
      - `+0x44` = `GetTickCount()` stamp from row creation
    - `0x40e480` builds those rows through `0x40dc40 -> 0x40dbb0`
    - `0x40e1c0` has two replay modes:
      - direct row-node iteration
      - wrapper-node iteration where node `+0x04` points at the real `0x48` row object and the
        replay path then consumes that pointed row's payload
    - current mode byte `0x4d3588` plus comparator `0x40cf40` drive the sort/replay behavior:
      - mode `1` compares row string `+0x08`
      - mode `2` compares row string `+0x14`
      - mode `3` compares row string `+0x20`
      - mode `4` compares numeric `atoi(+0x2c)`
      - mode `5` uses the `+0x14/+0x20` status class family with `+0x44` timestamp tiebreak
      - mode `0` falls back to direct unsorted row order
  - it rehydrates each row's packed item data from stored low/high 16-bit indices and restores the
    selected row by matching the remembered low-16 world index
  - negative result for the deeper mediator bridge:
    - this launcher-owned row payload is still only a `0x40` UI display/sort snapshot
    - it is **not** an in-place match for the later mediator-owned `0xb4` input consumed by
      `0x41c1f0`
- `0x405a20 = LauncherLoginDialog_DispatchUiCommand`
  - newer command split is tighter than the older single-success-path read:
    - command `11`
      - reached from the page-`7` primary button (`dialog +0x204`) and from Enter on page `7`
      - enters page `6` when `0x4c8b1d == 0`, otherwise page `3`
      - practical consequence: the primary page-`7` button is **not** the direct `0x40d6f0`
        resolve/writeback path
    - command `8`
      - reached from list double-click `0x40d820`
      - calls `0x40d6f0`
      - on success continues through patch-check / launch-side logic and eventually exits that UI path
  - case `9` calls `0x40ec70`
    - newer decompile-backed tightening from that helper family:
      - it loads resource `0x0008` (`Deleted characters cannot be recovered. Are you certain...`) and
        formats it with the selected character name, using resource `0x00aa` as the message-box
        caption
      - it reads the selected row item-data high word as a signed active selection-entry index
        (current tighter auth-valid read: slot-record / character-entry index)
      - calls sibling mediator slot `+0xf0 = 0x41c390` with that selected row high word
      - waits through `0x41b6c0` for event `8`
      - on **success** (`WaitForEvent` result `0`) it:
        - builds `Profiles\%s\%s` from sibling mediator `+0xdc` then `+0x5c`
        - deletes that profile directory with `SHFileOperationA(FO_DELETE)` when present
        - calls sibling slot `+0xe8 = 0x41ec00 = CLTLoginMediator_RemoveSlotRecordAndCompactRouteStateByIndex`
        - rebuilds the list through `0x40e480 -> 0x40e1c0`
      - practical consequence: this is now best read as the launcher **delete-character** command /
        state7-event8 removal corridor, not as the hidden success-side producer for the later
        `+0xec = 0x41c1f0` `0xb4` selection-context commit
      - negative result: because this whole corridor is removal-oriented, it still does **not**
        expose the later `+0xec = 0x41c1f0` producer
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
  - practical read: launcher double-click activation routes into the direct selection-resolve path,
    while page-`7` primary-button / Enter still route through command `11`

- newer manual credential-side tightening from the same dialog family now closes the upstream page
  corridor better too:
  - page `2` primary button command `11` enters page `6` on the nopatch branch (`0x4c8b1d == 0`)
  - page `6` key handling runs through
    `0x408ee0 -> 0x408840 -> 0x408400 -> 0x4091d0`
  - `0x408400` builds the exact `0x41ecd0`-style credential block on the stack and submits it
    through sibling resolved slot `0x4d2734` vtable `+0x30`
  - raw mediator-vtable clarification now closes that call more tightly:
    - raw mediator vtable memory stores `0x41ecd0` at `0x004b01f8`
    - so the launcher page-`6` helper's `call [eax+0x30]` reaches
      `CLTLoginMediator::ProcessLoginRequest`
  - callback success dispatches command `7`, which on the nopatch branch leads into page `11`
  - page `11` Enter / command `10` returns to page `7`
  - remaining negative result: the original rich-edit observer/prompt lifecycle
    (`+0x170` register, `0x4091d0` callback, unregister on success/error) is still not modeled
    exactly on the replacement host

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
    after successful auth mirror the launcher dialog's page `11` -> command `10` -> page `7`
    transition and page-`7` command-`8` selection writeback before client load
  - keep the later direct owner `+0xec` call on the proven client-side producer path at
    `client.dll:0x62170f48`
- current bounded source consequence:
  - the no-GUI launcher bridge now mirrors launcher-side page-`7` command-`8` writeback into
    `CLauncher+0xa8/+0xac` plus `Last_WorldName`
  - newer tightening now also mirrors the packed row-item split more closely there:
    - low word = matched world / descriptor index
    - high word = selected active entry index (current tighter auth-valid read: slot-record /
      character-entry index)
  - it seeds the later wrapper-facing `+0x120` character source block and current slot route index
  - but it no longer claims a pre-client direct `+0xf0/+0xec` owner commit
  - newer direct-producer tightening from `client.dll:0x6211d3e0 + 0x62170e2a..0x62170f48` narrows
    the `0xb4` snapshot more than before:
    - the client first zero-initializes the full handoff
    - then only proves writes at `+0x00` and `+0x24..+0xa4`
    - practical consequence: both `block04` (`+0x04..+0x13`) and `block14` (`+0x14..+0x23`) are
      currently better treated as zero on the proven direct success-side path
  - newer owner-side tightening from `0x41c1f0 + 0x43bd20` matters for how far that bridge claim
    can go:
    - state8 later serializes the persisted snapshot blocks in packet order
      `0x09/0x19/0x29`, `0x79/0x89/0x99/0xa9`, then `0x39/0x49/0x59/0x69`
    - but the packet's fixed GCID dwords still come separately from the current slot record, not
      from persisted snapshot block `+0xcd0[0..1]`
    - so recovered slot-record character/world/descriptor data should stay only as separate shadow
      evidence for now, not as proven `+0xec` input semantics
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
- New answer for the outer blocking gate before `InitInstance` falls through:
  - `0x40b74d..0x40b752` calls `0x402ec0`
  - `0x402ec0` starts `CLauncherThread` (`runtimeclass 0x4aa134`)
    - `0x407580 = CLauncherThread_InitInstance` allocates the separate `0xb30` launcher
      dialog/controller object and stores it at thread `+0x48`
    - current best provisional class identity for that object is now `LauncherLoginDialog`
      - ctor `0x403390`
      - create/init body `0x406470 = LauncherLoginDialog_CreateAndInitializeWindow`
      - neighboring methods `0x4047d0 / 0x405a20 / 0x40d530 / 0x40d820`
      - exact original C++ type spelling is still open
    - `0x406470` creates that dialog window + child controls and marks dialog `+0x68` ready
    - `0x402ec0` waits for `thread+0x48`, waits for dialog `+0x68`, then pumps messages until
      thread `+0x45` says the launcher dialog path has exited
  - the successful selection corridor on that dialog is now tighter too:
    - state-7 page setup `0x4047d0` shows the selection list, registers observer `0x40f070`, and
      posts UI command `0x0f`
    - page-`7` primary button / Enter post command `11`, which is a page-transition path
    - list double-click `0x40d820` posts command `8` into `0x405a20`
    - `0x405a20` case `8` calls `0x40d6f0`
    - case `8` falls through to `DAT_004d259c = 1` only when:
      - `0x40d6f0` succeeds
      - the optional patch gate either skips `0x40ac00` or `0x40ac00` returns `0`
      - mediator slot `+0x138` does not divert into the launchpad-gated state18 branch
    - then it calls `0x40b8f0` and posts quit
  - that is the current best read for how original `InitInstance` stays blocked until launcher UI
    login/selection is complete before the later `0x40b7af..0x40b7c7` fallthrough
  - implementation consequence for the replacement launcher:
    - launcher auth/selection belongs on the pre-client side of the later DLL-load corridor
    - it should happen before the replacement falls through to `LoadCresDLL` / `LoadClientDLL`
- New narrowed bridge read from launcher selection into mediator `+0xec`:
  - launcher-side success path now closes concretely through
    `0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl`
  - that helper writes `CLauncher+0xa8/+0xac` (`0x4d3410/0x4d3414`) and persists `Last_WorldName`
  - the direct success-side `+0xec` call is then best read later from
    `client.dll:0x62170e2a..0x62170f48 = InitClientDLL_BeginLoadingCharacterFlow`
    - zero-inits a stack-local `0xb4` object
    - stores arg7 high-8 selector into the first dword
    - fills later fields through the selection-cfg loader family
    - calls arg6 `+0xec` at `0x62170f48`
  - practical consequence: the missing bridge is no longer best modeled as a hidden direct launcher
    virtual call into `+0xec`; it is a launcher-writeback-then-client-producer corridor
- Remaining questions:
  - exact original C++ type spelling for the provisional `LauncherLoginDialog` class
  - which later owner method(s) append the `+0x1470` late-entry string-triple list later exposed
    through arg6 `+0x118`?
- New negative result from this pass:
  - direct xrefs to mediator commit writers still do not close the upstream bridge
  - `0x41c1f0` currently shows only the vtable data reference at `0x004b02b4`
  - sibling `0x41c3c0` likewise only shows its vtable data reference at `0x004b02e8`
  - a byte-pattern scan for direct launcher virtual calls also found:
    - one concrete launcher call to `+0xf0` at `0x40ed76`
    - no matching direct launcher call sites to `+0xec` or `+0x120` on the current scan
  - so the real launcher-side producer still appears to reach those later commit writers through a
    narrower indirect/object-dispatch path than a simple easy-to-xref direct virtual call
