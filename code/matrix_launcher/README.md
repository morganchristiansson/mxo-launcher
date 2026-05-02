# Matrix Online Launcher

This project is a **reimplementation scaffold** for the original Matrix Online `launcher.exe`.

It is not a generic sample launcher.
Its purpose is to reproduce the original startup behavior closely enough to launch `client.dll` on the same path as the original binary.

## Source of Truth

- original binary: `~/MxO_7.6005/launcher.exe`
- active implementation: `src/resurrections.cpp` + `src/diagnostics.cpp`
- active executable name: `resurrections.exe`
- project notes: `AGENTS.md`
- canonical docs: `../../docs/`

Current runtime note:
- for now, active progress runs use the hex-edited `~/MxO_7.6005/client.dll` variant that imports `dbghelp.dll`
- backup copies are kept as `client.dll.original` and `client.dll.patched`
- the original import-layout client still pulls us back into the old `mxowrap.dll` problem
- this is a pragmatic progress choice, not the final faithful endpoint

## Current State

The current launcher path already:
- preloads support DLLs
- loads `cres.dll` before `client.dll`
- resolves the client exports used by original `launcher.exe`
- uses the correct original 8-argument `InitClientDLL` frame shape
- rebuilds launcher-owned arg7 from recovered selection state
- reuses persisted `Last_WorldName`
- negotiates with auth and margin on the active path
- passes through the old loading-area boundary and enters game on the current live world path

Current bounded assumptions:
- current servers expose only one practical world, so defaulting to the first recovered/persisted world is an acceptable shortcut for the active path
- the active recovered world entry is still bounded to `Reality -> 0x0500002a`

Current blocker is no longer early startup bringup.
Current work is now about tightening faithfulness of the active launcher-owned runtime path after successful entry.

## Build

Canonical build logic now lives in CMake, but the old `make` workflow is intentionally kept as a wrapper.

### Prerequisites on Ubuntu

You already have:
- `clang++`
- `cmake`
- `ninja`
- `lld-link`

What is still missing for the requested MSVC-targeted Clang build is a **Windows SDK + MSVC CRT sysroot**.
On Ubuntu, the easiest route is usually `xwin`.

Expected environment after installing/extracting that sysroot:

```bash
export MXO_MSVC_SYSROOT=/path/to/xwin-splat
```

Expected layout under `MXO_MSVC_SYSROOT`:

```text
crt/include
crt/lib/x86
sdk/include/shared
sdk/include/ucrt
sdk/include/um
sdk/lib/ucrt/x86
sdk/lib/um/x86
```

### Configure and build with CMake directly

```bash
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/i686-windows-msvc-clang.cmake \
  -DMXO_BUILD_PROFILE=debug

cmake --build build/debug --target resurrections
```

### Compatibility wrapper

```bash
make
```

That wrapper now delegates to CMake and still deploys the built executable to:

- `~/MxO_7.6005/resurrections.exe`

## Run

Safe default run:

```bash
make run
```

Optional local credentials:

```make
# secrets/launcher_login.local.mk
MXO_USER := your-username
MXO_PASS := your-password
# optional:
# MXO_SESSION := your-session-token
# MXO_CHAR := your-character-name
```

If that file exists, the wrapper Makefile passes launcher-style switches to `resurrections.exe`, e.g.:
- `-user <name>`
- `-pwd <password>`
- optional `-session <token>`
- optional `-char <name>`

The launcher consumes those switches into launcher-owned preprocessing state and strips them back out before `InitClientDLL`.
It also now consumes known launcher-only switches like:
- `-clone`
- `-silent`
- `-nopatch`
- `-recover`
- `-deletechar`
- `-justpatch`
- `-noeula`
- `-skiplaunch`
- `-lptest`
- `-qluser`
- `-qlpwd`
- `-qlchar`
- `-qlsession`
- `-qlver`

instead of forwarding them blindly into `InitClientDLL`.

Current live existing-character runtime path is:

```bash
make run
```

`resurrections.exe` now rebuilds arg7 the same way the original launcher does:
- high 8 bits from `CLauncher+0xa8`
- low 24 bits from `CLauncher+0xac`

Current replacement status for the first live world path:
- when the selected world name resolves to `Reality`, the launcher seeds recovered launcher-side
  arg7 defaults (`0x0500002a`) from that world selection
- the old split-field arg7 env shims are no longer part of the active launcher path

By default the launcher now probes the original registry location for:
- `HKLM\\Software\\Monolith Productions\\The Matrix Online\\Last_WorldName`

and reuses that persisted world selection when rebuilding launcher-owned arg7 state.

Latest crash dump summary:

```bash
make crashdump
```

Optional iteration helper to avoid the interactive crash reporter GUI during diagnostic crash loops:

```bash
make install_crashreporter_stub
```

That target:
- builds a tiny no-op `crashreporter.exe` replacement
- preserves the original as `~/MxO_7.6005/crashreporter.exe.original`
- replaces the active runtime copy with the stub
- logs invocations to `~/MxO_7.6005/crashreporter_stub.log`

Restore the original reporter with:

```bash
make restore_crashreporter
```

This is only a workflow aid for crash iteration. It is not part of the launcher reimplementation itself.

## Documentation

Start here:
- `../../docs/launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../docs/client.dll/InitClientDLL/README.md`
- `../../docs/client.dll/RunClientDLL/README.md`
- `../../docs/launcher.exe/startup_objects/README.md`

## Next Work

- keep tightening the active launcher-owned runtime path now that it reaches and enters game
- replace remaining bounded recovered scaffolds with more faithful launcher-owned construction where evidence exists
- continue the late-runtime parity pass after the old state9/state12 bridge and later observer/event continuations
- keep source, recovered behavior, and canonical docs aligned as the active path gets less scaffolded
