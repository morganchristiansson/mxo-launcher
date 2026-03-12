# Diagnostic-only `ret` bypass for the late `arg2` crash family

## Why this note exists

A new practical debugging result is that the current late crash family which lands in launcher-owned `arg2 filteredArgv` storage can be stepped past temporarily by forcing the bad landing site to behave like a single x86 `ret` (`0xc3`).

This should be treated only as a **debugger-assisted diagnostic bypass**.
It is **not** a faithful launcher behavior, **not** a real fix, and **not** evidence that `arg2` itself is the root problem.

## What this means

The current crash family already had a strong evidence trail pointing to bad control flow resuming inside current `arg2` storage:

- faulting `EIP` tracks the current launcher-owned `arg2` allocation
- nearby runs vary as `arg2+2`, `arg2+3`, or equivalent heap-relative landings
- the broader investigation already established that `arg2` is normally consumed as argv data, not as a legitimate callback target

The new `ret` bypass result is consistent with that model:

- control is already wrong by the time execution lands in `arg2`
- forcing that landing to execute a `ret` can pop one more return target and continue farther
- so the immediate `arg2` faulting bytes are acting as a **bad intermediate landing pad**, not as the real semantic destination the client wanted

## What this does **not** prove

This hack does **not** prove any of the following:

- that `arg2` should be executable
- that the launcher should synthesize a trampoline in argv storage
- that the old callback/vtable hypotheses were actually right after all
- that the current underlying corruption is solved

It only proves that the immediate fault can be bypassed long enough to observe whatever breaks next.

## Why this is still only a hack

Using `ret` here is effectively papering over a corrupted control-flow edge.
That means:

- the original corruption still happened earlier
- stack / ownership / state assumptions are still wrong
- any later progress reached after the bypass must be interpreted as **diagnostic-only**

This is the same class of caveat as forcing `RunClientDLL` after failed init:
useful for narrowing later expectations, but not original-equivalent behavior.

## Best current interpretation

The practical `ret` bypass strengthens the existing interpretation rather than replacing it:

1. launcher-owned startup state is still incomplete
2. some earlier client path corrupts or misdirects control flow
3. execution later lands inside current `arg2` storage as collateral damage
4. a manual `ret` can step over that one bad landing
5. the real fix is still to reconstruct the missing launcher/client state that prevents the misdirection in the first place

Current highest-value suspects remain:

- `arg5` launcher object completeness (`0x4d6304`)
- `arg6` mediator correctness/completeness (`0x4d2c58`)
- `arg7` packed selection state and its launcher-side derivation
- remaining pre-client launcher setup around `0x402ec0` / `0x409950`

## Current implementation in the diagnostic scaffold

The replacement launcher now has an opt-in diagnostic path:

- `MXO_ARG2_RET_BYPASS=1`
- optional limit override: `MXO_ARG2_RET_BYPASS_MAX=<n>`

Implementation detail:
- a vectored/unhandled exception hook watches for access violations whose faulting `EIP` lands inside current launcher-owned `arg2 filteredArgv` storage
- when enabled, it simulates a single x86 `ret` by:
  - reading `*(uint32_t*)ESP` as the next target
  - setting `EIP = popped_target`
  - advancing `ESP += 4`

This is still only a diagnostic control-flow bypass.
It is not intended startup behavior.

## Validation result so far

The new in-launcher bypass was useful, but it also showed an important limitation.

Representative run:
- `cd /home/morgan/mxo/code/matrix_launcher && MXO_ARG2_RET_BYPASS=1 MXO_ARG2_RET_BYPASS_MAX=4 make run_binder_both`

Observed sequence:
1. normal late crash still lands at current `arg2 + 2`
2. the bypass fires once
3. the popped target is **not** a meaningful later client continuation
4. it is the stale startup-frame value for `arg3 hClientDll = 0x62000000`
5. execution then immediately faults again at `client.dll+0x3` (`0x62000003`)

Representative dump/log evidence:
- `~/MxO_7.6005/MatrixOnline_0.0_crash_57.dmp`
  - `EIP=0x62000003`
  - stack top after the bypass begins with:
    - `0x003e71c8` (arg5)
    - `0x0041bb60` (arg6)
- `~/MxO_7.6005/resurrections.log`
  - `DIAGNOSTIC: ARG2 RET BYPASS #1/4 at eip=003e5e82 argv+0x2 avKind=1 -> simulating ret to 62000000`

Interpretation:
- the bypass does **not** recover a valid hidden continuation
- it just walks into stale values from the original `InitClientDLL` argument frame
- so, on the current path, it does **not** yet expose any later arg5 traffic

## Recommended use

Use the `ret` bypass only for:

- interactive differential debugging
- confirming what value sits on top of the corrupted return chain
- collecting more evidence about which launcher-owned state is still missing

Do **not**:

- bake it into the intended launcher path
- describe it as a fix for `InitClientDLL`
- use it as a success criterion

## Related docs

- `CRASH_EIP_003E2B82.md`
- `FINAL_INVESTIGATION_SUMMARY.md`
- `BUFFER_OVERFLOW_CONFIRMED.md`
- `OVERFLOW_SOURCE_ANALYSIS.md`
