# Launcher-owned object ABI boundaries

Purpose:
- keep cross-object ABI-boundary notes out of per-object docs when the lesson applies to more than one startup object
- use this for launcher-owned objects that cross into `client.dll` or other original MSVC-built code
- current concrete case study: arg5 / `0x4d6304`
- likely future related surface: arg6 / `0x4d2c58`

## Core rule

Do **not** assume that a source-level C++ class in the replacement launcher can directly replace an original launcher-owned object just because:
- the method names line up,
- the virtual slot count looks similar,
- or current generated methods happen to return with matching `ret N` cleanup.

For any object crossing into original code, check separately:
- object size
- field layout / embedded subobjects
- runtime-visible helper surfaces
- actual vtable address-point format
- `this` identity expectations
- stack cleanup / calling convention details

## Current 0x4d6304 / arg5 case

### Short answer

For the current codebase, `LauncherObjectPrimaryVtable` should **not** be removed wholesale.

Best current answer is a constrained hybrid:
- keep the launcher-owned `0xb4` arg5 ABI shell as the top-level boundary
- keep the shell because original code still consumes raw embedded object addresses at:
  - `+0x0c` / `+0x34`
  - `+0x5c`
  - `+0x60`
  - `+0x98`
- but do **not** treat every one of those surfaces as permanently shell-owned implementation logic
- move recovered field/helper/container semantics into `mxo::liblttcp::CLTThreadPerClientTCPEngine`
  where source ownership is conceptually engine-owned
- let the shell become mostly a raw-address / vtable / lifetime boundary that attaches those live
  shell addresses to the fuller target-class implementation
- do **not** replace the whole shell with the native C++ `CLTThreadPerClientTCPEngine` object
- do **not** blindly assign the native GCC vtable as arg5

## Why the size mismatch is **not** explained by “missing virtual slots”

Short answer: **no**.

Missing or extra virtual slots affect the **vtable size**, not the in-memory size of the object instance, unless the class also changed data members / bases / embedded subobjects.

Current evidence:
- launcher-visible arg5 shell size: `0xb4`
  - `src/launcher_network_object_abi.cpp`
  - `LauncherObjectAbiShell`
- current native sidecar size: `0xf0`
  - `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.h`
  - `mxo::liblttcp::CLTThreadPerClientTCPEngine`
  - confirmed by the new MinGW runtime probe that constructs a real native object and reads its
    live first-word vptr / field offsets

That size difference is explained by **data/layout mismatch**, not by missing vtable entries.

The launcher-visible shell still carries client-visible state at:
- `+0x0c` / `+0x34` queue pair
- `+0x5c` helper object
- `+0x60` helper object + `CRITICAL_SECTION`
- `+0x7c` event handle
- `+0x80` / `+0x8c` sentinel-headed containers
- `+0x98` helper object + `CRITICAL_SECTION`

The native sidecar class does not have that same object layout.

So:
- adding stub virtual functions would only change the vtable surface
- it would **not** materialize the missing embedded helper objects or data fields inside the object body
- and it would **not** make `client.dll` field reads like `arg5+0x60` or `arg5+0x98` valid by itself

Could we artificially pad the native class to `0xb4`?
- mechanically, yes
- but that alone still would not make it the original object
- you would still need the **correct field meanings, offsets, helper subobjects, ownership, and locking/event behavior**

So “just stub slots to pad and match” is the wrong model here.
The important mismatch is the **instance layout**, not just the number of virtual entries.

## How the GCC / MinGW vtable preamble differs here

Current native GCC-emitted vtable for:
- `mxo::liblttcp::CLTThreadPerClientTCPEngine`

Object-file evidence from this pass:
- the raw vtable symbol starts with two metadata dwords before the first function pointer:
  1. `offset-to-top`
  2. `typeinfo` pointer
- only after those does the function pointer array begin

On 32-bit single-inheritance here, that means:
- raw symbol base = metadata header
- object's stored `vptr` = **address point** at `raw + 8`

So if the raw table looked conceptually like:

```text
raw_vtable:
  +0x00 offset_to_top
  +0x04 typeinfo
  +0x08 slot0
  +0x0c slot1
  ...
```

then the live object stores:

```text
this->vptr = &raw_vtable[+0x08]
```

That is why blindly copying the raw GNU vtable symbol into a launcher-owned flat slot table would be wrong.

### Contrast with the current launcher shell table

Current launcher arg5 shell uses a plain source-owned table of function pointers:
- slot 0 is directly the first function pointer
- there is no GCC metadata preamble in front of it

So these are **not interchangeable representations**.

### Practical contrast with MSVC-style expectations

MSVC object models typically present the object's vfptr as pointing directly at the function-slot area used for dispatch.
RTTI metadata exists, but not with the same GCC/Itanium-style “raw symbol has preamble, object points to address-point inside it” convention.

So for this project, the safe working rule is:
- do not assume GCC vtable data can be copied byte-for-byte into an MSVC-consumed launcher object
- treat the native GCC vtable as compiler-private layout unless proven otherwise

## What still matters besides the vtable preamble

Even if we used the live GCC object vptr value instead of the raw symbol:
- that still would **not** fix the arg5 problem
- because the object body is still the wrong size and wrong shape

So the two blockers are separate:
1. the GCC vtable uses a different address-point convention
2. the native sidecar object is not the original launcher-visible arg5 object layout

## Useful narrowing from this pass

Current generated cleanup shapes do match for the known native methods and current wrappers.
Examples from the built objects:
- slot `0` -> `ret 4`
- slot `1` -> `ret 0xc`
- slot `6` -> `ret 4`
- slot `7` -> `ret 8`
- slot `8` -> `ret 0x10`
- slot `12` -> `ret 4`

So the current blocker is **not** just “MinGW always emits the wrong cleanup.”

For arg5, the bigger blockers are:
- shell `this` vs sidecar `this`
- raw embedded helper-object address identity seen by original code
- launcher-owned top-level cleanup / allocator responsibility
- client-visible field layout

The latest focused pass narrows one important point though:
- the shell no longer needs to be the only place that knows how those helper/container surfaces
  behave
- `CLTThreadPerClientTCPEngine` now owns first-class source-level surrogates for:
  - fallback queue pair state
  - helper-family semantics for `+0x5c`, `+0x60`, `+0x98`
  - fallback event / lock state
  - sentinel-head / count mirror state for `+0x80` / `+0x8c`
- and the shell now attaches its live raw addresses to that class instead of open-coding as much of
  the behavior itself

That means a future **thin shell-to-sidecar thunk** may be fine for more individual slots than
before.
It still does **not** mean the whole shell can disappear.

## 2026-03-29 MinGW native-vptr experiment result

A narrow compile-time experiment now exists behind:
- `MXO_EXPERIMENT_MINGW_NATIVE_ARG5_VPTR`
- enabled from the build with:
  - `make MXO_EXPERIMENT_MINGW_NATIVE_ARG5_VPTR=1`

Important implementation constraints preserved in this pass:
- `InitClientDLL` still receives the **base object pointer**
- the old wrapper-table path remains the stable fallback and the non-MinGW path
- the build logs the requested mode, effective mode, stored shell vptr, and the live GCC native
  address-point probe

The experiment intentionally does **not** force native-vptr dispatch unless the native object layout
is close enough to the launcher shell.
The current MinGW runtime probe now constructs a real
`mxo::liblttcp::CLTThreadPerClientTCPEngine`, reads its live first-word vptr, and records the field
offsets the native methods would interpret as `this`.

Observed result from `~/MxO_7.6005/resurrections.log` on the experiment-enabled build:
- `requested=mingw-native-vptr`
- `effective=wrapper-table`
- `mingwNativeRequested=1`
- `nativeLayoutCompatible=0`
- live native vptr/address-point: `0x6ad4f4`
- raw-vtable-base guess: `0x6ad4ec`
- `offsetToTop=0`
- native object size: `0xf0`
- launcher shell size: `0xb4`
- native field offsets only match through the early queue/helper start:
  - `field04=0x4`
  - `field08=0x8`
  - `queue0C=0xc`
  - `queue34=0x34`
  - `wait5C=0x5c`
  - `lock60=0x60`
- but later shell-visible offsets diverge immediately after that:
  - native event field at `0x68` vs shell `+0x7c`
  - native endpoint-head pointer at `0x6c` vs shell `+0x80`
  - native endpoint count at `0x70` vs shell `+0x84`
  - native context-head pointer at `0x78` vs shell `+0x8c`
  - native context count at `0x7c` vs shell `+0x90`
  - native cleanup helper at `0x84` vs shell `+0x98`
  - native lock-helper size `0x8` vs shell embedded helper size `0x1c`

Conclusion from this pass:
- the MinGW build now has a **real, reversible native-vptr experiment switch**
- but the guard correctly refuses to install the native GCC address-point today because the object
  body/layout is still not close enough after `+0x60`
- so the current tested success case with the experiment enabled is:
  - **requested native-vptr path falls back to wrapper-table mode**
  - **the launcher still entered game successfully on that fallback path**

This is a useful negative result, not a dead end:
- it proves the build/run rollback path works cleanly
- it confirms the `+8` issue belongs to the GCC vptr/address-point, not the object pointer passed
  to `InitClientDLL`
- and it narrows the real blocker to remaining body/layout divergence rather than only vtable slot
  count or stack cleanup shape

## 0x4d6304 slot reduction summary

### Best early thin-thunk candidates
- slot `10` / `0x443810`
- slot `4` / `0x42f7c0`
- slot `11` / `0x431670`
- slot `9` / `0x42fd10`

### Slots that should stay wrapper-owned
- slot `0` / deleting dtor
  - shell still owns top-level `malloc(0xb4)` lifetime and mediator clear/release ordering

### Slots now reduced to thin shell -> class thunks after the arg5 ownership pass
- slot `1` / `MonitorPort`
- slot `2` / `UDPMonitorPort`
- slot `3` / `MonitorEphemeralUDPPort`
- slot `4` / `0x42f7c0`
- slot `5` / `UnmonitorPort`
- slot `6` / `Connect`
- slot `7` / `Close`
- slot `8` / `SendBuffer`
- slot `9` / `0x42fd10`
- slot `10` / `0x443810`
- slot `11` / `0x431670`
- slot `12` / `CleanupConnection`
  - important change: slot `12` lock/helper behavior now lives in the target class; the wrapper no
    longer acquires/releases arg5 `+0x98` itself

### Current conclusion

For arg5 / `0x4d6304`:
- keep the launcher-owned ABI shell
- keep the shell's layout contract and raw embedded-address identity
- but prefer target-class ownership for recovered engine semantics behind that shell
- if we reduce indirection later, reduce **individual slot bodies**, not the top-level shell object

## Related canonical docs

Per-object docs:
- `./0x4d6304_network_engine.md`
- `./0x4d2c58_LATE_LOGIN_ARG6_SURFACE.md`

Vtable doc:
- `../VTABLES/0x004b2768.md`
