# Current crash analysis - 2026-04-06

This file replaces the older stale `0x875895` note.

Current active crash work should be anchored to the later post-login client-shell/runtime path that
now starts successfully, reaches visible game entry, and then dies in one of two late families.

## Current state

Replacement now reliably reaches:
- state8 raw `0x10`
- state9 raw `0x11`
- state12 / event `0x18`
- visible **Waiting for Regionserver**
- in-game `MATRIX_ONLINE` window
- `RunClientDLL` active frame loop

So the current crash is no longer an early auth/bootstrap or immediate event-`0x18` blocker.

## Crash family A: late render / widget recursion

Latest representative dump:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_39.dmp`

Backtrace:
- `d3d9+0x32e2c`
- `client.dll:0x6233821a`
- `client.dll:0x62452827`
- `client.dll:0x6244ef4d`
- `client.dll:0x624330b7`
- `client.dll:0x6244ad4b`
- repeated `client.dll:0x624330b7`
- `client.dll:0x62429a29`
- `client.dll:0x6217477f`
- `client.dll:0x62006c8e`
- `client.dll:0x6200118a`

Current named function map:
- `0x62006c30 = ClientShell_MainLoopPumpAndRunFrame`
- `0x621736f0 = ClientShell_RunFrame`
- `0x62159ef0 = ClientShell_RenderFrameAndPresent`
- `0x624299d0 = WidgetManager_DrawCurrentRootWidget`
- `0x62432fa0 = UIWidget_DrawChildWidgetsRecursive`

Current best read:
- the active replacement route survives the immediate late-login handoff
- then later dies during recursive widget drawing / render submission
- the deepest client frames now look like ordinary UI/render traversal rather than direct login
  mediator logic

## Crash family B: alternate null-vcall in `ClientShell_RunFrame`

Representative dump:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_38.dmp`

Backtrace:
- `EIP = 0x00000000`
- return address on stack = `client.dll:0x62173bdc`
- then back out through:
  - `client.dll:0x62006c8e`
  - `client.dll:0x6200118a`

Exact static site:
```asm
62173bcd  mov ecx,[esi+0xd0]
62173bd7  mov edx,[ecx]
62173bd9  call [edx+0x68]
```

Important dump fact from this family:
- crash-time `EDX = 0x0a2eb1c8`
- that is heap-shaped, not a static client `.rdata` vftable address

Current best read:
- client-shell field `+0xd0` sometimes holds a wrong/incomplete runtime object on the replacement
  route
- or at least one whose first dword is not the expected polymorphic vftable for this call site

## Practical consequence

The current crash investigation should stay focused on:
1. how client-shell field `+0xd0` is populated on the late runtime path
2. which widget/runtime object tree later reaches:
   - `WidgetManager_DrawCurrentRootWidget`
   - `UIWidget_DrawChildWidgetsRecursive`
   - deeper draw submission
3. which replacement-specific late state/data mismatch can produce either:
   - a bad `+0xd0` runtime object
   - or a bad widget/render tree deeper in the same frame path

## Non-root-cause notes

These are no longer the best primary crash explanations:
- early auth/bootstrap fidelity
- immediate event-`0x18` handoff failure
- `mcd.cfg` body mismatch
  - current active original/reference and replacement `mcd.cfg` are bit-identical again
- original temp-copy / alternate AppData path side effects
  - user explicitly does **not** want those reproduced for now

## Current supporting diagnostics in source

`src/resurrections.cpp` now logs extra crash-time client context for this path:
- `DAT_629e68a8` client-shell slot
- client-shell state around `+0x18 .. +0x34`
- client-shell runtime-object field `+0xd0 .. +0xec`
- `client shell +0xd0` pointed object and first vftable words when readable
- render-family globals used by the late frames:
  - `DAT_629f84e8`
  - `DAT_629f1748`
  - `DAT_62a01e5c`
  - `DAT_62a333a4`

Use those logs on the next rerun before widening scope.
