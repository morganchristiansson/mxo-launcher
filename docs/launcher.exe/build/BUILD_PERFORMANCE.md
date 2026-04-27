# Build performance note

## Build profiles

- `make`
  - default **debug-slim** build
  - keeps line tables for backtraces with `-g1`
  - uses `-pipe` and `-pthread`

- `make full_debug`
  - full DWARF debug info with `-g`
  - use this only when deep debugging is needed

- `make release`
  - slim final launcher build
  - uses `-Os -DNDEBUG`
  - uses `-ffunction-sections -fdata-sections`
  - links with `--gc-sections` and strips the final executable

## Why

The old default build produced very large `.o` files and a very large final `resurrections.exe`, which made the final link step expensive.

## Measured impact

On this machine:

### Debug-slim profile (`make`)

- `build/debug/` size: about **100 MB**
- final `resurrections.exe`: about **97 MB**
- standalone relink time: about **9.9 seconds**
- full `make -j4` from clean: about **1:44** wall clock

### Release profile (`make release`)

- `build/release/` size: about **46 MB**
- final `resurrections.exe`: about **3.06 MB**
- produces a stripped, slim launcher suitable for final distribution

## Notes

The lighter debug format keeps the build useful for reverse-engineering work without carrying the full DWARF payload in every object file.
