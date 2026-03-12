# client.dll!InitClientDLL

## Source of truth

- original call site in `launcher.exe` at `0x40a55c..0x40a5a4`
- surrounding startup path in `launcher.exe` at `0x40b740..0x40b7d5`

## What matters

The original launcher does **not** call `InitClientDLL` with mostly `NULL` placeholders.
It constructs launcher-owned state first, then passes an 8-argument frame.

## Correct argument mapping

At `launcher.exe:0x40a55c..0x40a5a4` the call sequence is:

```asm
mov eax, [ebx+0xac]
mov ecx, [ebx+0xa8]
xor edx, edx
mov dl, [0x4d2c69]
and eax, 0x00ffffff
shl ecx, 0x18
or  eax, ecx
mov ecx, [0x4d6304]
push edx        ; arg8
mov edx, [0x4d2c4c]
push eax        ; arg7
mov eax, [0x4d2c58]
push eax        ; arg6
push ecx        ; arg5
mov ecx, [0x4d2c60]
push edx        ; arg4
mov edx, [0x4d2c5c]
mov eax, [0x4d2c50]
push eax        ; arg3
push ecx        ; arg2
push edx        ; arg1
call edi        ; InitClientDLL
add esp, 0x20
```

## Reconstructed signature

```c
int InitClientDLL(
    uint32_t filteredArgCount,         // from 0x4d2c5c
    char**   filteredArgv,             // from 0x4d2c60
    HMODULE  hClientDll,               // from 0x4d2c50
    HMODULE  hCresDll,                 // from 0x4d2c4c
    void*    launcherNetworkObject,    // from 0x4d6304
    void*    pILTLoginMediatorDefault, // from 0x4d2c58
    uint32_t packedArg7Selection,      // [this+0xa8]/[this+0xac]
    uint32_t flagByte                  // from 0x4d2c69
);
```

Names after arg4 are still provisional, but the sources and order are not.

## Known launcher-side prerequisites

Before the call, the original launcher has already:

1. parsed launcher-filtered command-line state into `0x4d2c5c` / `0x4d2c60`
2. built a launcher-owned object and stored it at `0x4d6304`
3. loaded `cres.dll` into `0x4d2c4c`
4. loaded `client.dll` into `0x4d2c50`
5. resolved the runtime interface pointer at `0x4d2c58` (`ILTLoginMediator.Default`)
6. selected nopatch-related state reflected in `0x4d2c69`

### New clarification: arg1/arg2 are filtered launcher-owned argv storage

Static analysis of `launcher.exe:0x409950` now tightens the meaning of `0x4d2c5c` / `0x4d2c60`.

That function:
- allocates a pointer array sized from CRT `argc`
- stores the array pointer at `0x4d2c60`
- zeroes it
- initializes `0x4d2c5c = 0`
- walks CRT `argv`
- consumes launcher-owned switches like `-clone`, `-silent`, and `-nopatch`
- duplicates surviving / forwarded entries into fresh heap strings
- stores those duplicated pointers into the `0x4d2c60` array
- increments `0x4d2c5c` for each forwarded entry
- later frees that array through `0x40a000`

So arg1/arg2 to `InitClientDLL` are **not simply raw CRT `argc` / `argv`**.
They are a launcher-built filtered argv-like pair backed by launcher-owned duplicated strings.

Current scaffold note:
- the custom launcher now consumes a broader evidence-backed set of launcher-owned switches while building its duplicated arg1/arg2 storage, including:
  - `-clone`, `-silent`, `-nopatch`
  - `-user` / `-qluser`
  - `-pwd` / `-qlpwd`
  - `-char` / `-qlchar`
  - `-session` / `-qlsession`
  - `-qlver`
  - `-recover`, `-deletechar`, `-justpatch`, `-noeula`, `-skiplaunch`, `-lptest`
- it also now keeps launcher-owned copies of the user/password/character/session-style values as explicit launcher preprocessing state
- but the original `0x409950` preprocessing path is still broader than that, especially around launcher-global side effects, exact flag derivation, and the `options.cfg` / autodetect branch

### New clarification: `options.cfg` is probed during the arg-filtering phase

The same `0x409950` path also probes `options.cfg` via `_stat` and date checks before the later client-loading sequence proceeds.
This establishes that temporary config handling is part of launcher-side startup preparation that happens before `InitClientDLL`.

What is currently supported with high confidence:
- `options.cfg` is consulted during the launcher-side preprocessing path
- that probe can set launcher-global flag `0x4d2c64`
- startup later checks `0x4d2c64` at `0x40b75a` and runs an extra launcher-side branch before client loading continues
- that branch constructs a helper object and calls `0x401520`
- `0x401520` launches `autodetect_settings.exe setopts hide`, waits up to 60 seconds, records result bytes, and calls `0x4013c0` to populate launcher UI text such as `Default Settings:`, `Detail:`, `Memory:`, and `Continue`
- `options.cfg` is deleted later by `0x4012e0`
- this preprocessing occurs before the `client.dll` startup handoff

What is **not** yet proven:
- that `+Windowed 1` is literally injected by this path into `0x4d2c60`
- the exact writer/consumer step that materializes windowed/fullscreen behavior from launcher state into client-visible config
- whether windowed behavior is conveyed through forwarded argv, `options.cfg` contents, child-process result bytes, or some combination of those

Relevant launcher-side object docs:
- `../../launcher.exe/startup_objects/0x4d6304_network_engine.md`
- `../../launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
- `../../launcher.exe/startup_objects/0x4d2c58_RESOLUTION_MECHANISM.md`
- `../../launcher.exe/startup_objects/0x4d3368_CLauncher.md`
- `ARG7_0x4D3410_0x4D3414.md`

For the growing mediator method surface itself, see:
- `../../launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`

### New clarification: the client builds a `0xb4` selection/config object before `arg6->+0xec`

Static analysis now tightens the deepest currently reached mediator handoff.

Inside `client.dll:0x62170e2a..0x62170f48` the client:
- constructs a stack object at `[ebp-0xbc]`,
- stores the current arg7-derived **variant/high-8 value** into the first dword of that object,
- fills the rest with profile/config path fragments via helper calls such as `0x62195ff0` / `0x62195f00`,
- and then passes that object to `arg6->+0xec`.

The zero-init helper at `client.dll:0x6211d3e0` clears this object through offset `+0xb0`, which fixes its size at **`0xb4` bytes**.

This matters because:
- `+0xec` is now better understood as a **selection/config-state handoff**, not just an opaque pointer call,
- the client uses `+0x38` and `+0x40` while building path strings like `Profiles\%s\` and `Profiles\%s\%s_%X\`,
- the `+0x40` descriptor builder maps a scratch-shaped arg7-derived request back into descriptor fields at payload `+3` (name pointer) and `+7` (selection id) for that `%s_%X` formatting path,
- after `+0xec`, the current confirmed path inside `0x62170b00` just jumps to `0x62170f62`, calls `0x6216a1c0`, and then returns success to its caller at `0x620015fd`
- and later crashes that still land at current `arg2 filteredArgv + 2` survive even after the replacement launcher copies that full `0xb4` object into stable mediator-owned storage.

So the current late crash is no longer well-explained by a trivial `+0xec` lifetime bug alone, and the next likely gap is now the enclosing `InitClientDLL` logic **after** this already-confirmed-success helper returns.
That also fits the current corrupted-return-chain model better than before: the already-reached loading/selection helper appears to complete successfully, so the visible late `arg2+2` failure may be happening during or immediately after the enclosing `InitClientDLL` success return / unwind rather than inside another unseen mediator callback in that helper.

A newer debugger-assisted trace tightens that claim further.
In winedbg on the current binder/scaffold run:
- break at launcher call site `resurrections.exe:0x411181`
- step into `client.dll:0x620012a0` (`InitClientDLL`)
- then use `finish`

Result:
- execution does **not** return to launcher `0x411187`
- instead, the function-level return target lands directly in current `arg2 filteredArgv` base (`0x003e5d88` on that run)
- and a single instruction step advances to `0x003e5d8a`, matching the familiar late crash family

So the current best reading is no longer merely that some later post-return launcher logic goes wrong.
It is now stronger evidence that `InitClientDLL` itself is unwinding/returning to corrupted launcher-owned argv storage before control ever gets back to the launcher call site.

## New concrete root cause for the old late `arg2` crash family: `+0x5c` / `+0x60` cleanup mismatch

A newer static + debugger pass finally narrowed the old corrupted-return-chain bug to a specific early mediator contract mismatch.

Inside `client.dll:0x62001325..0x62001362`, the client does this early auth-name chain:

```asm
call [eax+0x58]   ; returns first string
push eax
call [edx+0x60]   ; consumes previous string, returns next string
push eax
call [eax+0x5c]   ; consumes previous string, returns next string
push eax
push 0x628689dc
push 0x628689d4
call 0x6236fa40
add  esp, 0x14
```

That final `add esp, 0x14` is the key clue.
It means the single-stack-argument mediator slots `+0x60` and `+0x5c` are expected to be **caller-clean** on this path.

The scaffold had been exposing both as ordinary `__thiscall` methods, and the generated code in `resurrections.exe` used `ret 4` for both.
That over-cleaned the stack by 8 bytes before `client.dll:0x62001365`.

Debugger narrowing then showed the exact overwrite mechanism:
- at `client.dll:0x62001319`, the top-level `InitClientDLL` saved return address at `[ebp+4]` is still launcher `0x411187`
- by `client.dll:0x62001365`, `esp` has drifted to `ebp+8`
- so the next `push esi` writes stale `esi = arg2 filteredArgv + 0x0c` directly over the saved return address slot
- later returns therefore land in current `arg2` storage and produce the old `EIP=arg2+2` crash family

The replacement launcher now exposes `+0x60` and `+0x5c` through naked caller-clean wrappers (`ret`, not `ret 4`).

Current result after that fix on the same binder path:
- the client still reaches `MediatorStub::ConsumeSelectionContext(...)` at `+0xec`
- the old late `EIP=arg2+2` crash family no longer reproduces on that run
- and `InitClientDLL` now returns cleanly to the launcher with value **`1`** instead of crashing

A follow-up launcher-side correction also matters here:
- the replacement launcher had still been treating any non-zero `InitClientDLL` return as failure
- on the observed original client path, positive return `1` is the current success result
- after correcting that local launcher interpretation, the same run now logs:
  - `InitClientDLL returned: 1`
  - `InitClientDLL succeeded, but RunClientDLL is gated.`

So the old late `arg2` family is now best understood as a **resolved scaffold calling-convention bug** in the early mediator string chain, not an irreducible mystery in the later `+0xec` path itself.
The active blocker has therefore shifted again:
- not "why does `InitClientDLL` crash into argv?"
- but rather how to proceed faithfully from the now-clean positive-return `InitClientDLL` result into the legitimate next startup phase.

### Diagnostic-only note: the current `arg2` landing can be stepped past with `ret`

A newer interactive debugging result is that the immediate late `arg2` crash can be bypassed temporarily by forcing the bad landing site to execute a single x86 `ret` (`0xc3`).

That is useful for differential debugging because it lets execution step past the immediate corrupted landing and reveal what fails next.
But it is **not** a real fix and **not** faithful startup behavior.
It should be interpreted only as further evidence that the visible `arg2` fault is collateral damage from earlier state/call-flow corruption.

Current stronger validation result:
- the replacement launcher now has an opt-in diagnostic implementation of this bypass (`MXO_ARG2_RET_BYPASS=1`)
- on the current path it does **not** reveal a meaningful later continuation
- the popped target is just stale startup-frame `arg3 hClientDll = 0x62000000`
- execution then immediately faults again at `client.dll+0x3`
- and this still does **not** surface any later arg5 method traffic
- newer preserved-frame logging now shows that the late crash-time stack directly lines up with stale saved `InitClientDLL` startup args (`arg3`, `arg4`, `arg5`, `arg6`, and in non-zero arg7 runs also `arg7`)
- a newer non-zero arg7 probe (`MXO_ARG7_SELECTION=0x0500002a`) showed that later client `+0x40` requests arrive as `selectionIndex=0x05000005`
- a fresh static pass now explains that as a client-side scratch mutation of the original arg7 stack slot at `0x62170dc1..0x62170e59`, not as a random unrelated value
- that same block also stores the original masked low-24-bit selection id separately through the nearby `0x629e1c7c` / `0x620011e0` path before the scratch rewrite, which suggests the client expects both a persisted low-24-bit selection id and the later scratch-shaped `+0x40` lookup key
- newer static work now identifies `0x629e1c7c` as a client-side console-int named `CreateCharacterWorldIndex`, not an anonymous scratch/global slot
- important ordering correction: the user-visible `"Loading Character"` status text is pushed earlier at `client.dll:0x62170f2a`, immediately before the already-observed `arg6->+0xec` call at `0x62170f48`
- so the visible loading-bar/status phase is consistent with the current `+0xec` evidence, but does **not** yet prove that the later direct `CreateCharacterWorldIndex` consumer at `0x62054cbd` has been reached
- to avoid missing the next loading-path transition, the diagnostic mediator now also exposes/logs slot `+0x120`
- a follow-up rerun after adding that slot still did **not** show any `+0x120` traffic before the same late crash (`crash_62`, still `EIP=0x003e5e8a`)
- that keeps `CreateCharacterWorldIndex` interesting, but narrows the currently confirmed visual `Loading Character` evidence back to the already-known pre-`+0xec` phase

Canonical note:
- `RET_BYPASS_HACK.md`
- `CRASH_EIP_003E2B82.md`

## Validation result from a closer original-path experiment

A 2026-03-11 experiment that:
- loaded `cres.dll` before `client.dll`,
- used the real 8-argument frame shape,
- avoided any `client.dll` memory injection,
- but still left launcher-owned objects unresolved,

produced:

- `InitClientDLL` return value: **`-7`**
- no observed launcher-side `SetMasterDatabase` callback

A follow-up diagnostic run then forced `RunClientDLL` anyway and produced fresh dump:

- `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`

This strengthens the conclusion that the remaining blocker is missing launcher-owned setup, not just export resolution or DLL order.

See:
- `INCOMPLETE_ORIGINAL_PATH_EXPERIMENT.md`
- `../RunClientDLL/README.md`

## New high-value finding: arg6 gates the old `-7` failure path

Static and diagnostic evidence now strongly indicate that **arg6 (`ILTLoginMediator.Default`) is the key blocker behind the earlier `InitClientDLL = -7` result**.

### Static proof inside `client.dll!InitClientDLL`
At function entry:

```asm
620012af: mov ecx, [ebp+0x1c]   ; arg6
620012b2: mov eax, [ebp+0x18]   ; arg5
...
620012bd: mov [0x62b073e4], eax ; store arg5 only
620012c2: call 0x6229d2a0       ; register arg6 under "ILTLoginMediator.Default"
```

This shows:

- arg5 is merely stored at first,
- arg6 is consumed immediately by the client-side binder/registry path.

### Client-side binder proof
`client.dll` has its own slot binder for:

- `ILTLoginMediator.Default` -> `0x629df7f0`

from static init around `0x627c3bd0`.

Later, the old `-7` path checks that resolved object:

```asm
620033f3: mov ecx, [0x629df7f0]
620033f9: mov eax, [ecx]
620033ff: call [eax+0x10]
```

### What `-7` actually is
The old `-7` result comes from `0x62003d47`, reached after `0x62003380` fails.
That stage corresponds to:

- **"Failed to properly set up the network shell."**

So the previous `-7` was not a generic mystery code.
It was a concrete network-shell setup failure reached before arg7/arg8 matter.

### What arg7/arg8 do *not* explain
The arg7/arg8-dependent path begins later at `0x620015e2 -> 0x62170b00`.
That path is not reached until after the earlier network-shell setup succeeds.

So:

- arg7/arg8 are important,
- but they are **not** the reason for the original `-7` barrier.

## Diagnostic stub experiment result

A new opt-in experiment using:

- `MXO_STUB_LOGIN_MEDIATOR=1`

supplied a minimal arg6 stub and changed behavior substantially:

1. the old immediate `InitClientDLL = -7` path was bypassed,
2. the client reached deeper mediator-dependent methods (`+0x10`, `+0xd8`, `+0x38`),
3. and after extending the stub far enough, the process progressed into much deeper startup / rendering activity instead of dying at the old `-7` point.

This is strong evidence that:

- reconstructing `0x4d2c58` is the highest-priority path to move the launcher forward,
- while arg5 (`0x4d6304`) appears to become important later than the original `-7` barrier.

## Implication for the reimplementation

The following ad-hoc call is not launcher-equivalent:

```c
InitClientDLL(argc, argv, hClient, NULL, NULL, 0, 0, NULL);
```

The most important missing state to fix first is now:

- a valid `ILTLoginMediator.Default` object for arg6,

followed by:

- enough of arg7/arg8,
- and then fuller reconstruction of arg5 / pre-client environment state.

## Open questions

- Which exact concrete interface methods of `ILTLoginMediator.Default` are required by `client.dll` during early runtime?
- What exact concrete launcher state is sufficient for the `ILTLoginMediator.Default` slot to resolve non-NULL in our reimplementation?
- Which fields/methods of those objects are actually consumed by `client.dll` during early runtime?
