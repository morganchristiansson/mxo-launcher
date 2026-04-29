# `0x403f90` basic-string helper family

## Conclusion

The recovered `StringTriple_0x403f90` / `cls_0x403f90` family is best treated as a
**`std::basic_string<char>`-like helper**.

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

## Why we are **not** flattening it to raw `std::string` in source

Even though the semantics are `std::basic_string<char>`-like, launcher.exe still exposes a
**distinct helper family with recoverable method boundaries**.

Important helper anchors:
- `0x403c90` = reallocate/copy range helper
- `0x403dc0` = append-range helper
- `0x403f90` = range-copy constructor
- `0x403ff0` = sized constructor
- `0x4043b0` = concat-into helper
- `0x407a10` = default constructor
- `0x407dd0` = assign-from-range helper
- `0x41d750/0x41d7a0/0x41e410/0x41eb20/0x41f3e0/0x41f640` = array helpers over this same 12-byte entry type

Per project fidelity rules, source keeps a dedicated wrapper so these helpers still map cleanly back
onto launcher.exe instead of disappearing into ad-hoc `std::string` calls.

## Source mapping

Source now uses:

- `matrixstaging/game/src/libltclientlogin/loginmediator_base.h`
  - `BasicString_0x403f90`
  - backed internally by `std::string owned_`
  - publicly re-exposes the recovered `begin/current/capacity` view
- route-host arrays under `CLTLoginMediatorSelectionRouteState` now use
  `routeHostStrings194_`

This gives us the readability win of standard-string semantics while still preserving the recovered
launcher helper boundaries and anchors.

## Naming note

Ghidra-side class naming can reasonably use a `basic_string`-flavored label with the `_0x403f90`
suffix retained for discoverability. Source keeps the clearer local wrapper name
`BasicString_0x403f90`.
