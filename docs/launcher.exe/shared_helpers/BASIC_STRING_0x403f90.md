# `0x403f90` basic-string helper family

## Conclusion

The recovered `StringTriple_0x403f90` / `cls_0x403f90` family is best treated as an old
**MSVC2003 `std::basic_string<char>` / `std::string` helper family**.

Current concrete recovered object view is a 12-byte pointer triple:

```cpp
struct BasicStringState {
    char* begin;
    char* current;
    char* capacity;
};
```

Semantically this behaves like a mutable null-terminated character string:
- default construction allocates a small empty buffer
- range construction copies `[begin,end)` and appends `\0`
- append/assign helpers preserve null termination
- multiple failure paths call `print_and_abort("basic_string")`

## Why source now uses direct `std::string`

Because launcher.exe is an **MSVC2003** build and the recovered 12-byte pointer-triple layout plus
`"basic_string"` abort strings line up with that older STL lineage, fidelity now favors using
**direct `std::string`** in source rather than a source-owned wrapper type.

Launcher.exe still exposes a **distinct helper family with recoverable method boundaries**, so we
keep those boundaries alive through comments and small helper shims.

Important helper anchors:
- `0x403c90` = reallocate/copy range helper
- `0x403dc0` = append-range helper
- `0x403f90` = range-copy constructor
- `0x403ff0` = sized constructor
- `0x4043b0` = concat-into helper
- `0x407a10` = default constructor
- `0x407dd0` = assign-from-range helper
- `0x41d750/0x41d7a0/0x41e410/0x41eb20/0x41f3e0/0x41f640` = array helpers over this same 12-byte entry type

Per project fidelity rules, source still preserves these anchors instead of replacing them with
unexplained ad-hoc string manipulation.

## Source mapping

Source now uses:

- direct `std::string` storage for recovered `0x403f90` fields
- route-host arrays under `CLTLoginMediatorSelectionRouteState` use
  `std::array<std::string, ...>` via `routeHostStrings194_`
- the owner `+0x30` route descriptor is stored as `std::string routeDescriptor30_`
- tiny helpers in `loginmediator_base.h` preserve the recovered
  `begin/current/capacity` sketch for ABI-facing wrapper slots

This gives us both:
- type fidelity to the recovered old-MSVC STL string identity
- explicit anchor coverage for the launcher helper family

## Naming note

Ghidra-side class naming can reasonably use a `basic_string`-flavored label with the `_0x403f90`
suffix retained for discoverability. Source now uses direct `std::string`, with `_0x403f90`
retained in helper/docs names where address-based discoverability still helps.
