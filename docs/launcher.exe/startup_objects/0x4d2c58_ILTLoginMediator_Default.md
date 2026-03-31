# launcher global `0x4d2c58`

## High-confidence identity

`0x4d2c58` is **not** the storage for a wrapper object.
It is a **runtime interface pointer slot** populated through launcher-side registration.

The strongest static identifier currently attached to this slot is:

- string argument: `"ILTLoginMediator.Default"` at `0x4ab34c`

So the current canonical name is:

- **`ILTLoginMediator.Default` runtime interface pointer**

Relevant recovered source-file anchors nearby in the same launcher/login area:
- `\matrixstaging\game\src\libltclientlogin\loginmediator.cpp`
- `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`
- `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`

Current replacement-launcher source split:
- launcher-owned shared mediator/auth/margin state/model lives under:
  - `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- focused post-auth slot/route and margin-route init surface now lives under:
  - `matrixstaging/game/src/libltclientlogin/loginmediator_margin_route.cpp`
- focused arg6 / startup-selection surface now lives under:
  - `matrixstaging/game/src/libltclientlogin/loginmediator_arg6.cpp`
- focused late-login/state9 surface now lives under:
  - `matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp`
  - `matrixstaging/game/src/libltclientlogin/loginmediator_state9_submit_scaffold.h`
  - `matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp`
  - `matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp`
  - `src/launcher_mediator_state9_abi.cpp`
- the broader replacement `ILTLoginMediator.Default` ABI/vtable shell remains rooted in:
  - `src/launcher_mediator_abi.cpp`
- that broader ABI shell is now source-split into focused implementation files for lower-noise RE work:
  - `src/launcher_mediator_abi_profile_paths.cpp`
  - `src/launcher_mediator_abi_selection_cfg.cpp`
  - `src/launcher_mediator_abi_mcd.cpp`
- diagnostics-only window tracing remains in `src/diagnostics.cpp`

Focused docs:
- late-login arg6 subset (`+0xd4`, `+0x124`, `+0x18c`):
  - `0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`
- non-`mcd.cfg` selection cfg corpus subset (`+0x68 .. +0xb8`):
  - `0x4d2c58_SELECTION_CFG_CORPUS.md`
- `mcd.cfg` persistence subset:
  - `0x4d2c58_MCD_CFG_PERSISTENCE.md`
- post-state9 / state-`0x0c` continuation:
  - `../state_machine/POST_STATE9_CONTINUATION.md`

Those path strings do **not** by themselves prove exact class ownership for every auth/login packet path, but they do support treating this interface as part of the launcher/game-side login layer rather than a generic runtime-only socket helper.

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

## Current implementation milestone (2026-03-18)

Focused early-path result from current replacement-launcher work:
- the default `make run` path now auto-enables the current binder-backed `ILTLoginMediator.Default` scaffold together with the current `0x40a380`-style arg5 build/register step instead of requiring manual diagnostic env flags just to reach `InitClientDLL`
- this change is intentionally narrow: it promotes the original launcher control-flow obligation
  - build `0x4d6304`
  - hand it into `ILTLoginMediator.Default` via `+0x08`
  - pass the resolved arg6/arg5 pair into `InitClientDLL`
- it does **not** claim faithful completion of the deeper mediator/object internals yet

Representative validation run:
- command: `make run`
- current result:
  - reaches `InitClientDLL`
  - `InitClientDLL returned: 1`
  - launcher-owned auth auto-begin still follows on that path
  - `RunClientDLL` remains intentionally gated by default

That makes the first obstacle narrower than before:
- the earliest blocker is no longer simply "default run never reaches InitClientDLL because arg5/arg6 are absent"
- the next early-path work should stay focused on improving the faithfulness of the now-default startup-object path rather than keeping it behind diagnostic-only switches

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

That is important because the current arg7-selection writer is now closed to the surrounding helper:

- `launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl`
- inside that function, the final writeback at `0x40d763..0x40d810` consults `0x4d3584` through
  methods `+0xfc`, `+0x100`, and `+0xe4` before writing `0x4d3410 / 0x4d3414`
- newer `0x40cdb0` tightening now narrows that middle gate:
  - `+0x100 == 1` accepts directly
  - `+0x100 == 2/5` calls sibling slot `+0x54`
  - any other value rejects before the final writeback
- newer `0x40e480` review now also fixes one stale read of the sibling list-builder family:
  - slot `+0xe0` returns a world-name string used to match active-world entries against the
    total-world list before packed row item-data is written
  - it is **not** a bool-style availability gate
  - row item-data then packs:
    - low 16 bits = total-world index (`+0xfc`)
    - high 16 bits = matching active-world index, else `0xffff`
- replacement-side fidelity consequence from that tightening:
  - anchored startup getters `+0xf8/+0xd8/+0xe4/+0xfc/+0x100/+0xdc/+0xe0` should prefer startup
    table storage seeded from the recovered launcher selection state
  - avoid routing those anchored getters back through separate unanchored
    `Arg6MappedSelectionName` / `Arg6MappedVariantName` / selected-state helpers when the same
    values can live in the table the original launcher-side world-list code actually walks
- the same helper also persists `Last_WorldName` and clearly operates on a launcher-owned
  `CListCtrl`-style selection UI object rather than on a raw config parser path

So `0x4d2c58` is not the only launcher-global slot tied to this interface name.
The launcher appears to keep at least one **sibling `ILTLoginMediator.Default`-style pointer slot** involved in world/selection resolution.

Bounded practical consequence:
- this sibling slot is now a concrete static bridge between launcher UI selection state and the
  `CLauncher+0xa8/+0xac` values later consumed by `InitClientDLL`
- but the helper body itself still looks like selection validation/writeback, not the whole modal /
  observer wait that keeps original startup blocked until auth + character choice are complete

## Early method surface observed so far

### Launcher-observed offsets on `0x4d2c58`

From original `launcher.exe` startup/teardown:

| Offset | Current use | Confidence |
|---:|---|---|
| `+0x08` | launcher hands newly built `0x4d6304` object into mediator | high |
| `+0x0c` | teardown / cleanup path | medium |
| `+0x1c` | nopatch path passes parsed `"0.1"`-derived value | medium |
| `+0x24` | nopatch path passes client-version-derived value | medium |
| `+0x58` | launcher crashreporter/auth seeding path reads the low byte later stored as crashreporter `PromptForSecurId` | medium |
| `+0x5c` | launcher crashreporter/auth seeding path reads the value later used as crashreporter **username** | high |
| `+0x60` | launcher crashreporter/auth seeding path reads the value later used as crashreporter **password** | high |
| `+0x164` | teardown auth-close / wait-event-`1` predicate; tiny body `0x41b3f0` sets owner byte `+0x2c` and may call auth connection `+0x0c(1)` | medium |
| `+0x16c` | teardown margin-close / wait-event-`0x0f` predicate, but the strongest owner-side anchor is still state9 success helper `0x41b420` | medium |

New teardown-slot clarification:
- `+0x164` and `+0x16c` are not anonymous export-style booleans.
- direct launcher teardown `0x40b360` now reads as:
  - call `+0x164`, then wait for event `1` only when it returns non-zero
  - call `+0x16c`, then wait for event `0x0f` only when it returns non-zero
- current best slot split is therefore:
  - `+0x164` = auth-connection close / shared-slot1-event-`1` arm
  - `+0x16c` = wrapper-facing margin-connection close / shared-slot2-event-`0x0f` arm
    even though the same tiny body is also the owner-side state9 opcode-`0x11` success helper

Current implementation note:
- the replacement launcher now uses the original packed-version parser shape from `0x417440`
  rather than treating these as IEEE float bits,
- it still applies parsed `"0.1"` as the fallback seed for both slots,
- and it rebuilds the slot values from the on-disk PE version resources with the same
  `%d.%d%d%d%d`-style string shaping recovered from the original nopatch path:
  - `launcher.exe` -> `7.6004`
  - `client.dll` -> `7.6005`
- that launcher-side `7.6004` result also matches the current `vlck.ltb` corpus value.

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
- the current implementation already propagates username strongly enough that downstream crashreporter args can show `morgan`
- newer implementation cleanup now keeps the caller-clean wrapper shape but prefers bootstrap child `+0xf8` for mediator `+0x60` once the early auth-success path has populated it; current fallback remains the explicit auth-password state when that child string is still absent
- the highest-value current reconstruction target became the mediator-backed **password** path corresponding to original launcher `+0x60 -> 0x42ee80 -> 0x4d7424`, not only the already-studied early client `+0x58/+0x60/+0x5c` chain at `0x62001325..0x62001362`

Historical runtime validation with a disposable test credential confirmed that this password path was materially working on the earlier init-success path.
Representative validation run:
- active mediator/launcher path at the time
- disposable auth:
  - username = `pwcheck`
  - password = `PW_TEST_7Q9X2M4K`
- controlled post-init crash validation run

Observed `crashreporter_stub.log` result:
- `+Username "pwcheck"`
- `+Password "PW_TEST_7Q9X2M4K"`
- `+PromptForSecurId "1"`

So, on that path:
- crashreporter username propagation is now confirmed end-to-end
- crashreporter password propagation is now also confirmed end-to-end
- and the current implementation's mediator-backed auth seeding is now much closer to the original launcher's `+0x5c/+0x60/+0x58` crashreporter-default behavior than before

### Client-observed offsets on arg6-resolved `ILTLoginMediator.Default`

From `client.dll` static init and early `InitClientDLL` analysis:

| Offset | Earliest observed role | Confidence |
|---:|---|---|
| `+0x10` | readiness / availability gate; this is part of the old `-7` barrier | high |
| `+0x2c` | repeated `RunClientDLL` runtime gate before arg5-owned work at `0x62006cb9..0x62006cca` (`IsConnected()` in the current implementation) | high |
| `+0x38` | returns profile-root string used by client `Profiles\\%s\\...` formatting path | high |
| `+0x3c` | returns default selection index when the client asks for `0xff` fallback selection | medium |
| `+0x40` | returns selection-descriptor object for the arg7-derived selection index, including name + low-24-bit id data; replacement source now keeps this wrapper-facing ABI object explicit and owner-owned on `CLTLoginMediator`, not as ABI-shell globals | medium |
| `+0x44` | returns the current character-slot record object used by the later profile/save path; keep this wrapper-facing ABI object split explicit from the owner-side `+0x688[owner+0xcc8]` accessor family, and the replacement now owns the non-null scratch object on `CLTLoginMediator` instead of wrapper globals | high |
| `+0x48` | returns world/selection-style C-string in later real-user startup path | high |
| `+0x4c` | returns profile/session-style C-string immediately after `+0x48` in later real-user startup path | high |
| `+0x50` | later runtime distributed-object/RCC path calls this as a nullable pointer getter; `client.dll:0x625c86d0` converts non-null into flag `0x30`, and launcher-side static analysis now narrows it to `owner +0x680 -> +0xf4 -> +0xa8`; see `0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md` for the full child lifecycle | high |
| `+0x58` | low-byte crashreporter prompt flag in early init logging/config path; client passes it as `FUN_6236fa40(..., flag)` and launcher stores it to crashreporter `PromptForSecurId` | high |
| `+0x5c` | crashreporter username string getter on the early init chain; client path proves this single-arg wrapper-facing call shape is **caller-clean** | high |
| `+0x60` | crashreporter password string getter on the early init chain; client path proves this single-arg wrapper-facing call shape is **caller-clean** | high |
| `+0x8c` | live `mcd.cfg` mediator-backed gate; original `client.dll:0x62198fa0` calls this first and branches to on-disk fallback only when it returns `0`; corrected launcher getter proof now anchors `0x41f150` to owner byte `+0x13f6`, while `+0x1452` belongs to the neighboring live `cl.cfg` gate at arg6 `+0x88` | high |
| `+0xbc` | live `mcd.cfg` mediator-backed header getter; original `client.dll:0x62198fa0` copies `0x20` bytes from here into `DAT_629ea67c`; launcher getter is now anchored as `0x41f170` returning owner `+0xf48` | high |
| `+0xc0` | live `mcd.cfg` mediator-backed body getter; original `client.dll:0x62198fa0` copies `0x465` bytes from here into its local/global `mcd` body state; launcher getter is now anchored as `0x41f180` returning owner `+0xf88` | high |
| `+0xc4` | live `mcd.cfg` mediator-backed overflow-tail getter; original `client.dll:0x62198fa0` asks for pointer+length here; launcher getter is now anchored as `0x41aec0` returning owner `+0x13f0` and optional out-length `+0x13f4` | high |
| `+0xc8` | sibling state8 section-`0x0b` bool getter; launcher getter now anchored as `0x41f190` testing owner dword `+0x145c` | medium |
| `+0xcc` | sibling state8 section-`0x0b` dword getter; launcher getter now anchored as `0x41f1a0` returning owner dword `+0x145c` | medium |
| `+0xd0` | sibling state8 section-`0x0b` small-string-like getter; launcher getter now anchored as `0x41f1b0` returning owner `+0x1460` | medium |
| `+0xd4` | state9 follow-on client path (`0x620065e0`) fetches a 16-byte pointer here, then packages it through `0x62530630`; current best read = same launcher-owned Twofish seed/key family reused by state9 callback/blob work | medium |
| `+0xd8` | arg7 high-byte / world-selection gate in `InitClientDLL_BeginLoadingCharacterFlow` (`0x62170b00`) | high |
| `+0xdc` | maps arg7-derived selection to string/resource in deeper init | medium |
| `+0xec` | consumes assembled `0xb4` selection/config structure in deeper init | medium |
| `+0xf4` | later runtime/profile paths treat return value like a broader profile / character-info block; launcher getter is now anchored as `0x41f1c0 = return owner + 0xf1c`, while the real producer is the earlier state8/load-character reply path `0x43f930` that materializes the broader `+0xf1c/+0xf48/+0xf88/+0x13f0` family later consumed by UI + `mcd.cfg` persistence | high |
| `+0x120` | later loading-character path passes a large stack-built state object here before UI teardown / transition work | medium |
| `+0x124` | wrapper-facing `ProvideStartupTriple`: accepts `INetShell/INetMgr/ILTDistrObjExecutive` triple in deeper init | medium |
| `+0x13c` | `WaitForEvent` loop pump; calls launcher owner helper `+0x65c` vtable `+0x04` when present (`0x4202c0`); replacement wrapper minimization now forwards directly into a `CLTLoginMediator` owner method | medium |
| `+0x148` | accepts a runtime object/descriptor in later runtime setup paths | low |
| `+0x170` | registers an observer/listener object into launcher owner `+0x674` (`0x41ddb0`); current best container home is `0x4f78b8_OBSERVER_TREE_PLUS674.md` | high |
| `+0x174` | unregisters an observer/listener object from launcher owner `+0x674` (`0x41dde0`); current best container home is `0x4f78b8_OBSERVER_TREE_PLUS674.md` | high |
| `+0x178` | returns launcher owner status/result dword `+0x80` (`0x41f240`); client observer error handlers consult this after `PostError`, so state8 `MS_LoadCharacterReply` failures can carry the raw server status (for example `0x0b000025`) into the normal popup path | high |
| `+0x18c` | later callback84-side writer queried indirectly through `ClientNetShell +0x38`; fills client scratch buffer later surfaced as pair `(&0x629e0284, 0x20)`; active replacement now source-owns the state9-gated blob fill closely enough to run it live | high |

Many later runtime paths use even more offsets (`+0xf4`, `+0x10c`, `+0x118`, `+0x120`, `+0x148`, `+0x154`, `+0x158`, `+0x160`, `+0x174`, `+0x178`, etc.), which is strong evidence that the real interface is broad and not a tiny ad-hoc object.

New late-runtime crash consequence from the current in-game path:
- replacement-launcher crash dumps `MatrixOnline_0.0_crash_95.dmp` / `_96.dmp` now pin one missing arg6 ABI slot concretely
- both dumps stop at `EIP=0x00000000` with top return address `0x625c86db`
- direct disassembly of `client.dll:0x625c86d0` shows:
  - load resolved mediator global `0x629df7f0`
  - call mediator vtable `+0x50`
  - convert null/non-null into `0` or `0x30`
- so a null **function pointer** at arg6 `+0x50`, not merely a null return value, is enough to produce that crash shape
- this makes arg6 `+0x50` a proven required late-runtime surface on the active in-game path
- the full child lifecycle behind this getter now has its own canonical home:
  - `0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`
- arg6-facing consequence only:
  - raw `0x07` success eventually prepares raw `0x08`
  - auth-reply adoption later copies a `0x136` block into child `+0xf4`
  - only then do `0x41f370` / `0x41f3a0` expose the `+0xa8` worker / `+0x85` material family through arg6
- the replacement now mirrors that timing more closely by keeping `+0x50` null until successful auth-reply adoption instead of returning a permanent flat null stub

Current practical note on `+0x120`:
- newer static review places one `+0x120` use inside a broader loading-character path (`0x620547c0..0x62054eac`) that also directly reads client-side `CreateCharacterWorldIndex` current value `0x629e1cb0`
- but the currently visible `"Loading Character"` status text on patched-client runs is already explained earlier by `client.dll:0x62170f2a`, immediately before the already-observed `+0xec` handoff at `0x62170f48`
- and a follow-up diagnostic rerun with mediator slot `+0x120` instrumented still showed no `+0x120` traffic before the same late `EIP=0x003e5e8a` crash (`crash_62`)
- newer post-`+0xec` static review also shows that the already-reached `InitClientDLL_BeginLoadingCharacterFlow` (`0x62170b00`) helper performs no further mediator calls after `+0xec` before returning success through `0x620015fd` / `0x62001634`
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

## New callback84 / state9 submit consequence from client.dll

Newer client-side static tightening now gives a concrete reason why direct reuse of the captured
`+0x124(netShell, netMgr, distrObjExecutive)` triple is still insufficient for the launcher-side
state9 submit path.

### Client-side binder/global proof

`client.dll` keeps its own binder-managed resolved mediator slot:

- interface string: `"ILTLoginMediator.Default"`
- output slot: `0x629df7f0`
- static init around `0x627c3bd0` mirrors the launcher-side binder pattern and zeroes that slot
  before later registry/service resolution populates it

Representative static-init bytes there are now best read as:

```asm
627c3bd0: push 0x1
627c3bd2: push 0x628688a0      ; "ILTLoginMediator.Default"
627c3bd7: mov  ecx, 0x629df8e8 ; client-side binder wrapper object
...
627c3beb: mov  dword ptr [0x629df8f4], 0x629df7f0
627c3bf5: mov  dword ptr [0x629df7f0], 0x0
```

So the client callback side is not using a free-floating singleton chosen ad hoc at the `0x41de40`
problem site; it is reusing the same binder/resolution model already known for launcher arg6.

### Callback84 wrapper proof

The transplanted callback84 object currently resolves to the client `ClientNetShell` family:

- constructor bytes at `0x62006920` install vtable `0x6286d810`
- nearby class-name getter returns string `"ClientNetShell"` from `0x6286d870`
- vtable `+0x38` = `client.dll:0x62006580`

That `+0x38` method is now best read as:

```c
if (g_ResolvedILTLoginMediatorDefault_629df7f0 != NULL &&
    g_ResolvedILTLoginMediatorDefault_629df7f0->vtbl->IsReady10()) {
    g_ResolvedILTLoginMediatorDefault_629df7f0->vtbl->FillBuffer18c(&DAT_629e0284, 900, 0);
    *outLow  = (uint32_t)&DAT_629e0284;
    *outHigh = 0x20;
}
```

### Launcher-side implementation now identified

The original launcher-side arg6 implementation behind that client call is now tightened too:

- mediator vtable base: `0x004b01c8`
- arg6 slot `+0x18c` = vtable entry `0x004b0354`
- implementation: `0x0041e690 = CLTLoginMediator_FillState9CallbackBlob18c`

Current best read of `0x41e690`:

- first checks current helper/state id through owner `+0x10`
  - if current state is not `9`, returns `0x12000009`
- fetches the current slot record through owner vtable `+0x44`
- copies current slot payload id pair into the output buffer:
  - out `+0x00` = payload dword `+0x03` (current slot id low)
  - out `+0x04` = payload dword `+0x07` (current slot id high)
- copies caller arguments into the same buffer:
  - out `+0x08` = arg2 (`900` on the client `ClientNetShell +0x38` path)
  - out `+0x0c` = arg3 (`0` on that path)
- seeds the second half with one more owner field before the transform step:
  - out `+0x10` = owner dword `+0xf18`
  - current provenance answer for that field is now materially tighter:
    - `0x41ee60` zero-initializes owner `+0xf18`
    - `0x440780 = CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply` is the concrete
      non-init writer on opcode-`9` success
    - current best cross-checked read there is:
      owner `+0xf18` = opcode-`9` `UDPSessionSecret` / session-id dword
- then materializes the trailing 16-byte half of the blob in place from a current
  margin-connection-side source:
  - mediator vtable `+0xd4` = `0x41b4f0 = CLTLoginMediator_GetMarginConnectionField85D4`
  - direct assembly there is only:
    - `mov eax,[ecx+0x1c]`
    - `add eax,0x85`
    - `ret`
  - so original `+0xd4` is just the live pointer `owner + 0x1c + 0x85`, not a fallback chooser
  - `0x41e690` calls that slot and passes the returned pointer straight into the transform-helper
    constructor path; no alternate seed branch is visible in the bounded launcher body
  - `0x442d00` code-5 handling writes the consumed 16-byte payload tail back into connection
    `+0x85 .. +0x94`, which matches that later `+0xd4` read
  - `0x41df60 = FeedbackSizeTransformAdapter_ConstructSmall` builds a small
    `FeedbackSize` transform adapter around that source
  - `0x44b190 = FeedbackSizeTransformAdapter_InvokeConfigure40` dispatches the adapter's
    configure call
  - `0x44b570 = FeedbackSizeTransformAdapter_TransformBuffer` then transforms one 16-byte block
    in place
  - newer cross-use tightening matters here too:
    - the same helper family is reused by `AuthBootstrap680_SendAuthRequest`
    - neighboring helper vtables carry literals like `"ValueNames"` and `"EMSA-PKCS1-v1_5"`
    - current best read is therefore shared Crypto++-style parameterized transform machinery,
      not state9-only custom glue

So the callback84 pair is now tighter than just “some opaque scratch bytes”:

- pointer = `&DAT_629e0284`
- length = `0x20`
- contents = **state9 callback blob** built from:
  - current slot id pair
  - caller args
  - current margin-connection-side trailing material rooted at `owner +0x1c + 0x85`

A separate launcher-side lifecycle answer is now tighter too:

- within the bounded active mediator/state9 scope,
  - `0x41ee60` zero-initializes owner `+0x84/+0x88/+0x8c`
  - `0x41f1d0` is the concrete non-init triple store from `+0x124(netShell, netMgr, distrObjExecutive)`
  - later active-path `0x41de40` only reads owner `+0x88`
- no later launcher-side write to owner `+0x88` is isolated yet in that bounded active scope
- practical current read therefore stays:
  preserve the startup-provided netMgr wrapper provenance instead of assuming a later launcher-owned
  rewrite of object88 without new proof

Practical consequences:

1. callback84 `+0x38` is **not** self-contained on the captured `netShell` object
2. it depends on the client-side resolved mediator global at `0x629df7f0`
3. the pair returned to launcher state9 submit is concretely narrowed as:
   - first dword = pointer to client scratch buffer `0x629e0284`
   - second dword = fixed exposed length `0x20`
4. the blob content itself is materially tighter too:
   - first half = current slot id pair + caller args `(900, 0)`
   - second half starts from owner `+0xf18`
   - and its tail is materialized through shared `ValueNames` / `FeedbackSize` transform helpers
     fed from mediator `+0xd4 -> owner +0x1c + 0x85`
   - replacement `+0x18c` now mirrors that by sourcing the seed through `+0xd4` alone instead of
     keeping an extra local fallback chain in the blob builder
   - on the replacement path, that means live connection `+0x85 .. +0x94` is the real preferred
     source; keep the older launcher-owned bootstrap-sidecar path only as bounded fallback behind
     `+0xd4` until runtime is stable enough to prune it without risking game entry
5. a second launcher-side getter still corroborates that the `+0x85` family is reused outside the
   immediate state9 path:
   - `0x41f3a0` exposes `owner + 0x680 -> +0xf4 + 0x85`
   - `0x41f3c0` exposes the same child's `+0xf8` begin pointer
   - for the full child `+0xf4/+0xf8` ownership/lifecycle, see:
     - `0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`
6. newer active replacement progress now closes the go/no-go question on live `+0x18c` enough for
   the current state9 path:
   - string anchor `AssemblyTwofish`
   - parameter names `IV` + `FeedbackSize`
   - zero-IV storage at `0x4d4d50`
   - two natural-original samples matched exactly under a one-block Twofish transform of
     `[ownerF18, 0, 0, 0]`
   - the replacement now uses that live `+0x18c` path on the active runtime branch

This is the strongest current static explanation for the crashdump-backed result that raw direct reuse
of the captured `+0x124` objects regressed the deliberate launcher run: callback84 is a wrapper around
broader binder-managed mediator state, not a sealed transport collaborator.

## What the stub experiments proved

The opt-in mediator stub in the custom launcher showed this progression:

1. with arg6 = `NULL`, `InitClientDLL` hit the old `-7` path,
2. after supplying minimal arg6 methods (`+0x00`, `+0x10`, then `+0xd8`, `+0x38`),
3. startup moved past the old immediate `-7` barrier and into deeper post-network-shell / rendering startup.
4. on a real user run with correct audio/video permissions, startup progressed further again, created a real Matrix Online window, and then crashed on missing mediator slots `+0x48` and later `+0x4c` in the post-`IsReady()` path.
5. after adding diagnostic implementations for `+0x48` and `+0x4c`, startup advanced again into the post-selection path and hit `+0x170` / `+0x124` with concrete objects (`startupContext`, `INetShell`, `INetMgr`, `ILTDistrObjExecutive`) before the next crash.
6. the latest patched-client progress dump no longer shows `EIP=0`; it lands at `EIP=0x003e3b90`, which suggests later bad state / signature mismatch is now more likely than a simple missing-slot crash.

## Historical note: older `+0x170` interpretation is superseded

Later original-launcher runtime + vtable proof materially corrected the old read on this area:

- `+0x170` is now better read as **observer/listener registration** into launcher owner `+0x674`
  (`0x41ddb0`), not as a generic startup-context adoption slot
- `+0x174` is now better read as **observer/listener unregistration** (`0x41dde0`)
- `+0x178` is now better read as **return owner status/result dword `+0x80`** (`0x41f240`)
- `+0x10c` is now better read as the wrapper-facing small-string-like getter over owner `+0x30`
  (`0x41f2c0`)
- `+0x118` is now better read as the wrapper-facing vector-like getter over owner `+0x1470`
  (`0x41af50`)
- `+0x13c` is now better read as the `WaitForEvent` loop pump that invokes owner helper `+0x65c`
  vtable `+0x04` when present (`0x4202c0`)
- replacement wrapper minimization now keeps the ABI-shaped `+0x10c/+0x118` objects and the
  `+0x13c` action on `CLTLoginMediator`, while `src/launcher_mediator_abi.cpp` only forwards those
  slots through `g_LoginMediatorVtable`
- the old scaffold log names `AttachStartupContext`, `AttachRuntimeObject(+0x174)`, and
  `ConsumeRuntimeDescriptor` are historical and misleading for these late-runtime slots

Keep the older notes below only as startup-era diagnostic history.

## Historical diagnostic record: old post-`+170` / post-`+124` evidence

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

The same `InitClientDLL_BeginLoadingCharacterFlow` (`0x62170b00`) family also calls `0x62195ff0` / `0x62195f00`, which format paths like:

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
- and `+0xf4` is **not** just a plain string helper and now also appears broader than the older plain selection/config-snapshot read.
  Newer client-side UI/persistence proof later tightens that further:
  - `client.dll:0x620f1c60` (`P` / character-info dialog family) calls `arg6->+0xf4`, treats the base return value like a C-string, and also reads strings at `+0x70` and `+0x90`
  - `client.dll:0x62197830` (`mcd.cfg` persistence family) calls `arg6->+0xf4` and copies strings from `+0x70`, `+0x90`, and `+0xb0` into the saved profile file
  - newer live original WineDbg now closes the getter itself too:
    - original run reached `client.dll:0x62198fa0`
    - took the mediator-backed branch (`+0x8c != 0`), not the on-disk fallback
    - then called launcher getters in this exact order:
      - `+0xbc -> 0x41f170 -> owner +0xf48`
      - `+0xc0 -> 0x41f180 -> owner +0xf88`
      - `+0xc4 -> 0x41aec0 -> owner +0x13f0` with out-length from `+0x13f4`
      - then entered `0x62197830`
    - static review plus the same bounded vtable recovery also close `+0xf4` itself:
      - `+0xf4 -> 0x41f1c0 = return owner +0xf1c`
      - so the real missing/interesting producer is not a late ad-hoc `+0xf4` builder, but the earlier owner materialization path, currently best anchored as state8/load-character reply `0x43f930`
  - practical consequence: replacement work should now target the **owner `+0xf1c/+0xf48/+0xf88/+0x13f0` persistence family** first, not only a free-standing synthetic `+0xf4` string block
  - source ownership/logging consequence:
    - replacement source now keeps focused persistence-family logs at both ends of that bridge
    - producer-side logs live in `CLTLoginState_State8::Slot6_HandleSecondaryMessage`
    - getter-side logs live in launcher arg6 slots `+0xbc/+0xc0/+0xc4/+0xf4`
    - those are meant as future grep-able anchors if later in-game regressions disturb the same family

### What the newer reruns showed

Representative latest dumps:

- `~/MxO_7.6005/MatrixOnline_0.0_crash_33.dmp`
  - default scaffold selection name
  - `EIP=0x003e2b62`
  - current `arg2 filteredArgv = 0x003e2b60`
- `~/MxO_7.6005/MatrixOnline_0.0_crash_34.dmp`
- `~/MxO_7.6005/MatrixOnline_0.0_crash_35.dmp`
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
  - another launcher-owned client-config expectation inside the same `InitClientDLL_BeginLoadingCharacterFlow` (`0x62170b00`) / `0x622a39d0` family,
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

Representative earlier non-zero arg7 run family:
- packed selection `0x0500002a`

Current practical correction:
- the correct world name for this path is `Reality`
- newer auth-reply evidence points to `reality.lith.thematrixonline.net`
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

The replacement launcher now accepts that scratch-shaped request diagnostically and returns the configured descriptor with the expected:
- `packedSelectionId=0x00002a`
- `matchMode=arg7-scratch-shape`

Current correction:
- the descriptor name for this path should align with `Reality`, not the old incorrect override

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
- newer live original `matrix.exe` WineDbg tracing now strengthens the downstream meaning of that same `0xb4` object:
  - password confirmation hits owner `+0xec` / `0x41ecd0`
    - live stop there also tightens the input layout:
      - `+0x00` = username
      - `+0x20` = password
      - observed active branch had `g_LaunchPadGateState16State18 == 0`
  - corrected happy-path rerun from a stable EULA attach now proves the early helper chain more precisely:
    - a second fresh-process rerun with the correct password reproduced the same early chain, so the earlier wrong-password detour does not appear to have contaminated this proved happy-path sequence
    - before submit, owner `+0x10` is `0x004b51e0` = state `0`
    - `0x41b450(2)` switches `state0 -> state2`
    - `0x439210` (`state2` slot 3) is entered with upstream `state0`
    - `0x41b450(1)` switches `state2 -> state1`
    - `0x439090` (`state1` slot 3) is entered with upstream `state2`
    - `0x4390b0` (`state1` slot 1) later uses cached upstream `state2`
    - `0x41b450(2)` switches `state1 -> state2`
    - `0x439210` (`state2` slot 3) is re-entered with upstream `state1`
    - `0x41b450(3)` switches `state2 -> state3`
  - while current helper vtable is `0x004b5208` (state `3`), the launcher reaches `0x41c1f0`
  - newer live stop now confirms `0x41c1f0` itself is reached on that same branch
  - a second fresh rerun with the correct password reproduced that same state-`3` arrival at
    `0x41c1f0`, which makes the earlier wrong-password detour unlikely to have introduced extra
    backward helper transitions on the proved happy path
  - no extra early helper-switch hits were observed during that narrow state-`3` wait period
    before `0x41c1f0`
  - practical current read therefore stays narrow:
    - state `3` is the waiting helper identity on this branch
    - the owner-side methods keep selection-context ownership
    - `0x41c390` is the sibling byte setter that checks current state code `> 2`, stores owner
      `+0xcc8`, then switches helper state to `7`
    - `0x41c1f0` is the full snapshot writer that checks current state code `> 2`, copies the
      first dword into owner byte `+0xcc8`, copies the remaining `0xb0` bytes into owner
      `+0xcd0 .. +0xd7f`, and switches helper state to `8`
  - that is stronger evidence for a state3 wait phase than for any state3-local slot-3 body
- replacement-launcher follow-up now mirrors that copied arg6 `+0xec` snapshot directly into the
  source-owned `CLTLoginMediator::PersistSelectionContextForState8(...)` model path instead of
  leaving the recovered `0xb4` object only as ABI-side diagnostic storage
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

## Standalone auth-only probe docs moved

The standalone auth probe notes no longer live in `startup_objects/`.
They now have a dedicated canonical home under:
- `../auth/README.md`

That auth doc covers:
- the host-native probe build/run flow
- the working `0x06 -> 0x07 -> 0x08 -> 0x09 -> 0x0A -> 0x0B` sequence
- the reply-derived RSA key fix for `AS_AuthRequest`
- semantic `AS_AuthReply` parsing
- how to use the probe as a reference while fixing launcher-owned auth

## Launcher-owned auth note

Packet-level auth protocol behavior now belongs under:
- `../auth/README.md`

Prefer that auth folder as the canonical home for:
- `0x06 -> 0x07 -> 0x08 -> 0x09 -> 0x0A -> 0x0B` loop details
- reply-derived RSA key notes
- launcher-side auth integration milestones in `resurrections.exe`

This startup-object doc should stay focused on what that auth work implies about `0x4d2c58` itself:
- auth remains **launcher-owned**
- `ILTLoginMediator.Default` is carrying real launcher-side auth traffic on the scaffold path
- but full original automatic helper/state-machine reconstruction is still incomplete

## Current implication for reimplementation

The current custom launcher should stop treating arg6 as a vague `master database` or arbitrary placeholder.
It is much more likely a launcher-resolved `ILTLoginMediator.Default`-style interface pointer that must already be live by the time `InitClientDLL` is called.

See also:
- `0x4d2c58_RESOLUTION_MECHANISM.md`
- `0x4f78b8_AUTHBOOTSTRAP_CHILD_PLUS680.md`
