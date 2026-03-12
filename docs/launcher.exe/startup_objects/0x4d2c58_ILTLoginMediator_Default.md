# launcher global `0x4d2c58`

## High-confidence identity

`0x4d2c58` is **not** the storage for a wrapper object.
It is a **runtime interface pointer slot** populated through launcher-side registration.

The strongest static identifier currently attached to this slot is:

- string argument: `"ILTLoginMediator.Default"` at `0x4ab34c`

So the current canonical name is:

- **`ILTLoginMediator.Default` runtime interface pointer**

## Source of truth

### Dynamic initializer
- `launcher.exe:0x494ab0`

```asm
push 0x1
push 0x4d2c58
push 0x4ab34c      ; "ILTLoginMediator.Default"
mov  ecx, 0x4d3218
call 0x4030d0
```

### Constructor path
- `0x4030d0`
- base ctor `0x4143c0`
- registration helper `0x414030`

## What the constructor proves

Inside `0x4030d0`:

- `[this+4]  = 0x4ab34c` → interface name string
- `[this+8]  = 1`
- `[this+0xc] = 0x4d2c58` → output slot address
- `*(uint32_t*)0x4d2c58 = 0` → zero the runtime pointer slot before registration
- then `0x414030(this)` registers the wrapper with launcher-global registry state rooted at `0x4d3d54`

So:

- object at `0x4d3218` = wrapper / binder / registrar
- global at `0x4d2c58` = resolved runtime interface pointer used later by startup

## Why this matters

When startup later does:

```asm
mov ecx, [0x4d2c58]
mov edx, [ecx]
call [edx+...]
```

it is using the **resolved interface pointer value**, not a wrapper object.

That means our reimplementation should not model `0x4d2c58` as a plain homemade struct.
It needs the original launcher-side acquisition / registration behavior.

## Verified uses in the original startup path

### Nopatch branch configures it
At `0x409a73` and `0x409a98` the launcher:

- parses `"0.1"` via `0x417440`
- loads `ecx = [0x4d2c58]`
- calls vtable methods at offsets:
  - `+0x1c`
  - `+0x24`

So the nopatch path actively configures the interface before client startup.

### Startup hands `0x4d6304` into it
At `0x40a3e9..0x40a3fe`:

```asm
mov ecx, [0x4d2c58]
mov [0x4d6304], eax
mov edx, [ecx]
push eax            ; eax = object just built for 0x4d6304
call [edx+0x08]
```

So one of the interface methods consumes the newly built launcher object from `0x4d6304`.

### Client startup receives it as InitClientDLL arg6
At `0x40a587`:

```asm
mov eax, [0x4d2c58]
push eax            ; arg6 to InitClientDLL
```

### Teardown also uses it
At `0x40b360..0x40b409` the launcher uses the same interface during cleanup, including vtable offsets:

- `+0x164`
- `+0x16c`
- `+0x0c`

## High-confidence conclusions

1. `0x4d2c58` is a **runtime interface pointer slot**.
2. Its registration string is **`ILTLoginMediator.Default`**.
3. The original nopatch path configures this interface before `InitClientDLL`.
4. The launcher passes the resolved pointer value to `InitClientDLL`.
5. A replacement launcher that skips this acquisition/registration path is not equivalent to the original.

### New sibling-slot clarification

Fresh static review of `launcher.exe:0x496480..0x496491` shows that the launcher also registers **another output slot** through the same wrapper ctor `0x4030d0` with the same interface string `"ILTLoginMediator.Default"`:

- output slot: `0x4d3584`

That is important because the current arg7-selection writer around `0x40d763..0x40d810` consults `0x4d3584` through methods `+0xfc`, `+0x100`, and `+0xe4` before writing `0x4d3410 / 0x4d3414`.

So `0x4d2c58` is not the only launcher-global slot tied to this interface name.
The launcher appears to keep at least one **sibling `ILTLoginMediator.Default`-style pointer slot** involved in world/selection resolution.

## Early method surface observed so far

### Launcher-observed offsets on `0x4d2c58`

From original `launcher.exe` startup/teardown:

| Offset | Current use | Confidence |
|---:|---|---|
| `+0x08` | launcher hands newly built `0x4d6304` object into mediator | high |
| `+0x0c` | teardown / cleanup path | medium |
| `+0x1c` | nopatch path passes parsed `"0.1"`-derived value | medium |
| `+0x24` | nopatch path passes client-version-derived value | medium |
| `+0x58` | launcher crashreporter/auth seeding path reads a byte-ish flag and stores it to crashreporter prompt state | medium |
| `+0x5c` | launcher crashreporter/auth seeding path reads the value later used as crashreporter **username** | high |
| `+0x60` | launcher crashreporter/auth seeding path reads the value later used as crashreporter **password** | high |
| `+0x164` | teardown conditional check | medium |
| `+0x16c` | teardown conditional check | medium |

Current scaffold note:
- the replacement launcher now still uses parsed `"0.1"` for `+0x1c`,
- and it now rebuilds the `+0x24` value from the on-disk `client.dll` version resource using the same `%d.%d%d%d%d`-style float-string shaping recovered from the original nopatch path,
- instead of using the older identical `0.1` placeholder for both slots.

### New crashreporter/auth-default seeding clarification

Fresh static comparison now shows that the original launcher and client both contain a parallel crashreporter-default string/config surface, and that the launcher seeds it from `ILTLoginMediator.Default` through a path that is **distinct** from the early `InitClientDLL` chained auth-name call site.

Original launcher path:
- `launcher.exe:0x409220..0x409254`
- calls mediator methods individually, not as the `+0x58 -> +0x60 -> +0x5c` chain used by the early client init path:
  - `+0x5c`, then `call 0x42ee50`
  - `+0x60`, then `call 0x42ee80`
  - `+0x58`, then `call 0x42ede0`

Those launcher helpers seed these launcher globals:
- `0x42ee50(value)` -> copies string into `0x4d7418`
- `0x42ee80(value)` -> copies string into `0x4d7424`
- `0x42ede0(value)` -> stores low byte to `0x4d73b8`

Original launcher crashreporter builder `0x42ef70` then uses those globals as:
- `0x4d7418` -> crashreporter `+Username`
- `0x4d7424` -> crashreporter `+Password`
- `0x4d73b8` -> crashreporter `+PromptForSecurId`

Client-side mirrored surface:
- individual setters:
  - `client.dll:0x6236fa01(value)` -> `0x62a27568`
  - `client.dll:0x6236fa10(value)` -> `0x62a27574`
  - `client.dll:0x6236f980/0x6236f9b0` fill the parallel app-name / intro globals `0x62a27550 / 0x62a2755c`
  - `client.dll:0x6236fa40(a,b,c,d,flag)` seeds the whole group at once
- client crashreporter builder `0x6236fb00` then uses:
  - `0x62a27568` -> `+Username`
  - `0x62a27574` -> `+Password`
  - `0x62a27508` -> `+PromptForSecurId`

Important implication for the replacement launcher:
- the current scaffold already propagates username strongly enough that downstream crashreporter args can show `morgan`
- newer scaffold cleanup now also wires mediator `+0x60` from explicit auth-password state while preserving the caller-clean wrapper shape and masking password values in logs
- the highest-value current reconstruction target became the mediator-backed **password** path corresponding to original launcher `+0x60 -> 0x42ee80 -> 0x4d7424`, not only the already-studied early client `+0x58/+0x60/+0x5c` chain at `0x62001325..0x62001362`

New runtime validation with a disposable test credential now confirms that this password path is materially working on the binder/scaffold init-success path.
Representative diagnostic run:
- binder mediator + stub launcher object
- disposable auth:
  - username = `pwcheck`
  - password = `PW_TEST_7Q9X2M4K`
- deliberate post-init crash via replacement-launcher env knob `MXO_DIAGNOSTIC_CRASH_AFTER_INIT_SUCCESS=1`

Observed `crashreporter_stub.log` result:
- `+Username "pwcheck"`
- `+Password "PW_TEST_7Q9X2M4K"`
- `+PromptForSecurId "1"`

So, on that path:
- crashreporter username propagation is now confirmed end-to-end
- crashreporter password propagation is now also confirmed end-to-end
- and the current scaffold's mediator-backed auth seeding is now much closer to the original launcher's `+0x5c/+0x60/+0x58` crashreporter-default behavior than before

### Client-observed offsets on arg6-resolved `ILTLoginMediator.Default`

From `client.dll` static init and early `InitClientDLL` analysis:

| Offset | Earliest observed role | Confidence |
|---:|---|---|
| `+0x10` | readiness / availability gate; this is part of the old `-7` barrier | high |
| `+0x2c` | repeated `RunClientDLL` runtime gate before arg5-owned work at `0x62006cb9..0x62006cca` (`IsConnected()` in the current scaffold) | high |
| `+0x38` | returns profile-root string used by client `Profiles\\%s\\...` formatting path | high |
| `+0x3c` | returns default selection index when the client asks for `0xff` fallback selection | medium |
| `+0x40` | returns selection-descriptor object for the arg7-derived selection index, including name + low-24-bit id data | medium |
| `+0x48` | returns world/selection-style C-string in later real-user startup path | high |
| `+0x4c` | returns profile/session-style C-string immediately after `+0x48` in later real-user startup path | high |
| `+0x58` | string-producing helper in early init logging/config path | medium |
| `+0x5c` | chained string-producing helper; early auth-name path shows this single-arg slot is **caller-clean** on the client side | high |
| `+0x60` | chained string-producing helper; early auth-name path shows this single-arg slot is **caller-clean** on the client side | high |
| `+0xd8` | arg7 high-byte / world-selection gate in `0x62170b00` | high |
| `+0xdc` | maps arg7-derived selection to string/resource in deeper init | medium |
| `+0xec` | consumes assembled `0xb4` selection/config structure in deeper init | medium |
| `+0xf4` | later runtime/config paths treat return value like a persisted selection/config snapshot, not a plain C-string | medium |
| `+0x120` | later loading-character path passes a large stack-built state object here before UI teardown / transition work | medium |
| `+0x124` | accepts `INetShell/INetMgr/ILTDistrObjExecutive` triple in deeper init | medium |
| `+0x148` | accepts a runtime object/descriptor in later runtime setup paths | low |
| `+0x170` | consumes client startup context object in deeper init | medium |
| `+0x174` | accepts runtime object handles in later setup paths | medium |
| `+0x178` | consumes runtime descriptor object in later setup paths | medium |

Many later runtime paths use even more offsets (`+0xf4`, `+0x10c`, `+0x118`, `+0x120`, `+0x148`, `+0x154`, `+0x158`, `+0x160`, `+0x174`, `+0x178`, etc.), which is strong evidence that the real interface is broad and not a tiny ad-hoc object.

Current practical note on `+0x120`:
- newer static review places one `+0x120` use inside a broader loading-character path (`0x620547c0..0x62054eac`) that also directly reads client-side `CreateCharacterWorldIndex` current value `0x629e1cb0`
- but the currently visible `"Loading Character"` status text on patched-client runs is already explained earlier by `client.dll:0x62170f2a`, immediately before the already-observed `+0xec` handoff at `0x62170f48`
- and a follow-up diagnostic rerun with mediator slot `+0x120` instrumented still showed no `+0x120` traffic before the same late `EIP=0x003e5e8a` crash (`crash_62`)
- newer post-`+0xec` static review also shows that the already-reached `0x62170b00` helper performs no further mediator calls after `+0xec` before returning success through `0x620015fd` / `0x62001634`
- newer debugger + static review then resolved the old late `arg2` crash family to an early arg6 contract bug in the replacement launcher:
  - client auth-name chain `0x62001325..0x62001362` calls `+0x58`, `+0x60`, `+0x5c`, then does `add esp, 0x14`
  - that proves `+0x60` / `+0x5c` are caller-clean on this path
  - the scaffold had exposed them as callee-clean `ret 4` methods
  - fixing those two offsets to caller-clean wrappers stopped reproducing the old late `EIP=arg2+2` crash family on the current binder path
  - the deeper path now returns `InitClientDLL = 1` instead of crashing
  - and after correcting launcher-side interpretation of positive return values, the current run now cleanly logs `InitClientDLL succeeded, but RunClientDLL is gated.`
- newer deliberate `RunClientDLL` runs on that same clean binder path now also prove that arg6 `+0x2c` is not merely an init-side readiness guess:
  - static runtime loop at `0x62006cb1..0x62006cca` calls `arg6->+0x2c`, tests `al`, and only then feeds stored arg5 into `0x62532130`
  - current runtime logs show repeated `MediatorStub::IsConnected() -> 1` traffic on that loop
  - so `+0x2c` is now high-confidence live on the `RunClientDLL` path as a repeated runtime gate before arg5-owned work

## What the stub experiments proved

The opt-in mediator stub in the custom launcher showed this progression:

1. with arg6 = `NULL`, `InitClientDLL` hit the old `-7` path,
2. after supplying minimal arg6 methods (`+0x00`, `+0x10`, then `+0xd8`, `+0x38`),
3. startup moved past the old immediate `-7` barrier and into deeper post-network-shell / rendering startup.
4. on a real user run with correct audio/video permissions, startup progressed further again, created a real Matrix Online window, and then crashed on missing mediator slots `+0x48` and later `+0x4c` in the post-`IsReady()` path.
5. after adding diagnostic implementations for `+0x48` and `+0x4c`, startup advanced again into the post-selection path and hit `+0x170` / `+0x124` with concrete objects (`startupContext`, `INetShell`, `INetMgr`, `ILTDistrObjExecutive`) before the next crash.
6. the latest patched-client progress dump no longer shows `EIP=0`; it lands at `EIP=0x003e3b90`, which suggests later bad state / signature mismatch is now more likely than a simple missing-slot crash.

## New clarification: current post-`+170` / post-`+124` evidence

Static analysis of `client.dll` around `0x62170d6a..0x62170f48` now gives a tighter interpretation of the observed deep path.

Observed order:

1. `arg6->+0x10` readiness gate must succeed,
2. `arg6->+0x170(startupContext)` is called first,
3. `arg6->+0x124(netShell, netMgr, distrObjExecutive)` is called second,
4. arg7 high-byte handling continues (`+0xd8`, `+0xdc`),
5. and only later does the client hand a stack-built selection/config object into `arg6->+0xec`.

What this proves:

- `+0x170` is reached **before** the deeper arg7-selection object assembly is finished,
- `+0x124` is not the last mediator handoff in this phase,
- so the current crash after our logged `+0x170` / `+0x124` sequence does **not** by itself prove those two slots are missing.

Current best interpretation:

- `+0x170` likely records or adopts a startup context pointer for later use,
- `+0x124` likely records the startup network triple into mediator-owned state,
- and the next meaningful state transfer in this same path is `+0xec`, which consumes a locally assembled selection/config structure.

A fresh rerun with an instrumented mediator probe still crashed before any observed `+0xec` log, but it tightened several important details:

- latest dumps: `~/MxO_7.6005/MatrixOnline_0.0_crash_3.dmp`, `..._4.dmp`, `..._5.dmp`
- logged sequence still reached:
  - `AttachStartupContext(01f7dfc8)` / `AttachStartupContext(629ddfc8)`
  - `ProvideStartupTriple(netShell=01f2a288 netMgr=01f39968 distrObjExecutive=01f89dbc)` / same addresses on later run family
  - `AttachStartupContext(01f2a760)` / `AttachStartupContext(6298a760)`
- dump `EIP` landed at `0x003e3bb0`
- that value matched the launcher's current `arg2 filteredArgv` pointer for the same run
- additional probe logging captured the actual client return sites for the mediator calls we do see:
  - first `+0x170` returned to `client.dll:0x62170da1`
  - `+0x124` returned to `client.dll:0x62170dc1`
  - later `+0x170` returned to `client.dll:0x62056590`

This materially weakens the narrow theory that our current `+0x170` / `+0x124` probes are simply using the wrong stack cleanup:

- the custom launcher's compiled mediator methods currently emit callee-cleanup returns consistent with the observed client call shapes (`ret 4` for `+0x170`, `ret 0xc` for `+0x124`, `ret 4` for `+0xec`),
- and the logged caller addresses show that both observed `+0x170` calls and the observed `+0x124` call returned to valid client code before the later crash.

So while the crash still smells like corrupted or misinterpreted state, the evidence now points more toward a **state/ownership/signature expectation beyond simple stack-pop mismatch** than toward those two slots immediately smashing the return address on exit.

Later differential runs strengthened one specific part of that conclusion.

Two contrasting launcher-side arg2 experiments now exist:

1. a temporary diagnostic run where filtered argv storage was moved into more static/global launcher-owned memory in `resurrections.exe`, and
2. a later faithfulness pass that restored arg1/arg2 to heap-backed duplicated storage closer to original `0x409950` behavior.

Results across those runs:

- earlier heap-backed crash family: `EIP` landed at the heap-backed `arg2 filteredArgv` area (`0x003e3bb0` / nearby)
- after moving `arg2` into static launcher-owned storage, a later crash landed at `EIP=0x00413183` while current `arg2 filteredArgv = 0x00413180`
- after restoring heap-backed launcher-owned argv again for faithfulness, latest crash `~/MxO_7.6005/MatrixOnline_0.0_crash_17.dmp` landed at `EIP=0x003e3bb2` while current `arg2 filteredArgv = 0x003e3bb0`

That means the bad control transfer still **tracks arg2 itself**, not merely one specific storage strategy.
So the current best interpretation remains narrow:

- some later path is still treating `arg2 filteredArgv` like a code pointer / callback / return target,
- or a later stack/call-convention corruption is overwriting control flow with the current arg2 value,
- and this behavior survives across both static-buffer and heap-backed launcher-owned arg2 experiments.

Practical implication for the diagnostic scaffold:

- keep `+0x170` / `+0x124` as **state-capturing probes**,
- log ordering and repeated calls,
- and prioritize verifying whether the client later expects the mediator to retain and expose that captured state rather than immediately inventing more unrelated vtable slots.

This is the strongest current evidence that:

- arg6 is the highest-priority missing launcher-owned state,
- and the old `-7` barrier is more directly about `ILTLoginMediator.Default` than about arg5 or `0x402ec0`.

## New clarification: `+0xec` now lands, but the late `arg2+2` crash family survives it

Newer patched-client reruns now push the mediator probe one step further than the earlier `crash_3..5` family.

### Static anchor

`client.dll:0x62170e2a..0x62170f48` builds a stack object at `[ebp-0xbc]` and passes it to `arg6->+0xec`.
The constructor at `client.dll:0x6211d3e0` zero-initializes that object through offset `+0xb0`, which fixes the current handoff size at **`0xb4` bytes**.

The same `0x62170b00` family also calls `0x62195ff0` / `0x62195f00`, which format paths like:

- `Profiles\%s\`
- `Profiles\%s\%s_%X\`
- `hl.cfg`
- `an.cfg`
- `pi.cfg`
- `ai.cfg`
- `cs.cfg`
- `bl.cfg`
- `il.cfg`
- `rl.cfg`
- `cl.cfg`
- `mcd.cfg`
- `cui.cfg`
- plus profile-root files such as `keymap.cfg` and `aui.cfg`

That materially strengthens the interpretation that:

- `+0x38` is a **profile-root string input** to the client's config-path builder,
- `+0x40` maps an arg7-derived selection request into a descriptor payload whose fields at `+3` and `+7` feed the `%s_%X` path suffix formatting,
- `+0xec` is a **selection/config state handoff**,
- and `+0xf4` is more likely a later accessor for persisted selection/config state than a simple string-returning helper.

### What the newer reruns showed

Representative latest dumps:

- `~/MxO_7.6005/MatrixOnline_0.0_crash_33.dmp`
  - default scaffold selection name
  - `EIP=0x003e2b62`
  - current `arg2 filteredArgv = 0x003e2b60`
- `~/MxO_7.6005/MatrixOnline_0.0_crash_34.dmp`
- `~/MxO_7.6005/MatrixOnline_0.0_crash_35.dmp`
  - with `MXO_MEDIATOR_SELECTION_NAME=Vector`
  - `EIP=0x003e2b82`
  - current `arg2 filteredArgv = 0x003e2b80`

In both branches the same higher-level signature survives:

- the client now definitely reaches `MediatorStub::ConsumeSelectionContext(...)` at `+0xec`,
- the diagnostic scaffold now keeps a stable copied `0xb4` snapshot of that object instead of only retaining the raw stack pointer,
- but control still later redirects into **current `arg2 filteredArgv + 2`**.

### Negative results that still narrowed the search

Two evidence-backed mediator corrections were tried in this newer family:

1. **persist the `+0xec` handoff more faithfully**
   - the scaffold now copies the full `0xb4` selection/config object to stable mediator-owned storage
   - practical result: crash signature did **not** move
2. **treat `+0x38` as profile-root text instead of arbitrary launcher name**
   - the scaffold now returns the profile/session-style name (`morgan`) from `+0x38`
   - this materially changed on-disk side effects by creating `~/MxO_7.6005/Profiles/morgan/aui.cfg`
   - but the late `arg2+2` crash still remained (`crash_35`)

This is useful narrowing, because it shows:

- the client is not blocked solely on the old `+0xec` raw-pointer lifetime issue,
- and correcting the obvious `Profiles\%s\...` root string input at `+0x38` is still **not** enough by itself.

### Updated narrow interpretation

At this point the best current read is:

- the crash is still happening **after** the deeper `+0xec` selection/config handoff,
- but **before** later observed `+0xf4` probe traffic from this scaffold family,
- and newer winedbg tracing tightens the return-chain failure point further:
  - breaking at launcher `call g_InitClientDLL` site `resurrections.exe:0x411181`
  - stepping into `client.dll:0x620012a0`
  - and using `finish`
  - does **not** return to launcher `0x411187`
  - it returns directly into current `arg2 filteredArgv` base instead
- so the remaining mismatch is more likely tied to:
  - still-incomplete arg7 low-24-bit / selection-id state,
  - another launcher-owned client-config expectation inside the same `0x62170b00` / `0x622a39d0` family,
  - or an internal `InitClientDLL` return/unwind corruption that still poisons control flow with the current `arg2` pointer before the launcher regains control.

## New diagnostic tightening after this pass

### 1. Preserved `InitClientDLL` frame now confirms the stale return chain more directly

The launcher now preserves the intended 8-argument `InitClientDLL` frame before the call and compares it against crash-time stack words in the exception logger.

Current late-crash result:
- on the `arg2+2` crash family, the crash-time stack top now directly matches the preserved startup-frame values in order:
  - `esp[0] = arg3 hClientDll`
  - `esp[1] = arg4 hCresDll`
  - `esp[2] = arg5 launcherNetworkObject`
  - `esp[3] = arg6 ILTLoginMediatorDefault`
- in a non-zero arg7 run, `esp[4]` also matched preserved `arg7 packedArg7Selection`

That materially strengthens the current interpretation that the later failure is collapsing into stale `InitClientDLL` startup-frame data rather than reaching a hidden valid continuation.

### 2. Arg7-related mediator probes are now stricter and exposed a more specific `+0x40` scratch shape

The diagnostic mediator no longer generically accepts every in-range world/variant value for the arg7-related surface.
It now:
- returns the configured selected world from `+0x3c`
- exact-matches the configured selected world / variant pair for:
  - `+0xfc`
  - `+0x100`
  - `+0xe4`
- and now also accepts one specific client-side `+0x40` scratch-shaped request derived from the current configured arg7 state
- accepts optional diagnostic overrides:
  - `MXO_MEDIATOR_WORLD_TYPE`
  - `MXO_MEDIATOR_VARIANT_STATE`

Representative non-zero arg7 run:
- `MXO_ARG7_SELECTION=0x0500002a`
- `MXO_MEDIATOR_SELECTION_NAME=Vector`
- `make run_binder_both`
- representative dumps from that run family:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_59.dmp`
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_60.dmp`
  - both still land at `EIP=0x003e5e8a`

Observed launcher-side sibling-slot phase:
- `+0xfc(worldIndex=0x2a)` -> `"Vector"`
- `+0x100(worldIndex=0x2a)` -> `1`
- `+0xe4(variantIndex=0x05)` -> `0`
- launcher-side arg7 rebuild still succeeded as:
  - `a8=0x00000005`
  - `ac=0x0000002a`
  - `packed=0x0500002a`

A fresh static pass explains why later client-side `+0x40` calls use:
- `selectionIndex=0x05000005`

Inside `client.dll:0x62170dc1..0x62170e59`, the client reuses the original arg7 stack slot as scratch storage and:
- masks low 24 bits into one register,
- shifts the high 8 bits into another,
- writes `bl` back into the low byte of `[ebp+0x14]`,
- then later reloads the full mutated dword from `[ebp+0x14]`.

So `0x0500002a` becomes `0x05000005` before later path-building helpers call `arg6->+0x40`.

Important neighboring detail from the same block:
- before mutating the arg7 stack slot, the client also stores the original masked low-24-bit selection id separately via `push esi ; mov ecx, 0x629e1c7c ; call 0x620011e0`
- newer static follow-up now identifies that sink as the client-side console-int `CreateCharacterWorldIndex`
- so the current best interpretation is that the client expects both:
  - a stable persisted low-24-bit selection id somewhere else
  - specifically through that client-owned `0x629e1c7c` state path
  - and the later scratch-shaped `+0x40` request key

The replacement launcher now accepts that scratch-shaped request diagnostically and returns the configured descriptor with:
- `mappedName='Vector'`
- `packedSelectionId=0x00002a`
- `matchMode=arg7-scratch-shape`

Practical result:
- this better matches the observed client request shape,
- but it still did **not** move the late `arg2+2` crash family.
- a follow-up rerun after adding explicit `selectionContext[0]` logging still remained in the same family:
  - `~/MxO_7.6005/MatrixOnline_0.0_crash_61.dmp`
  - `EIP=0x003e5e8a`

### 3. `+0xec` logging is now richer

`MediatorStub::ConsumeSelectionContext(+0xec)` now logs:
- the full copied `0xb4` word buffer
- the configured world / variant / profile inputs active for that handoff
- printable ASCII candidate scans inside the copied object

Representative non-zero arg7 result from that same run:
- first dword in the copied `0xb4` object changed to `0x00000005`
- runtime log now confirms it directly:
  - `DIAGNOSTIC: selectionContext[0]=0x00000005 (configuredVariant=0x05 configuredWorld=0x00002a)`
- a newer static pass now explains that field:
  - at `client.dll:0x62170de2..0x62170e3b`, the client zero-extends the arg7 high-8-bit variant value into `esi`
  - and stores that value into the first dword of the `+0xec` handoff object before the path-building helper sequence
- no printable ASCII strings were found in the copied object itself

So the current `+0xec` object is not just "some paths blob":
- its first dword now looks like the current variant/high-8 selector,
- and later fields are built from selection-specific config helpers rooted under `Profiles\%s\%s_%X\`.

## Decision for reimplementation direction

The stub should be kept only as a **differential probe**.
It is useful because it reveals which vtable slots are touched and how far startup advances.

However, the growing method surface strongly suggests that the real fix should prioritize reconstructing the original launcher-side acquisition path for `0x4d2c58`, namely:

- wrapper/binder object,
- registry object,
- resolver/service node path,
- and materialization of the real interface pointer,

rather than continuing to grow a fake mediator into a large hand-emulated interface.

## Current implication for reimplementation

The current custom launcher should stop treating arg6 as a vague `master database` or arbitrary placeholder.
It is much more likely a launcher-resolved `ILTLoginMediator.Default`-style interface pointer that must already be live by the time `InitClientDLL` is called.

See also:
- `0x4d2c58_RESOLUTION_MECHANISM.md`
