# CLTIPAddressList

## High-confidence recovered methods

- `launcher.exe:0x00440d80 = CLTIPAddressList_Reinit`
- `launcher.exe:0x00440bb0 = CLTIPAddressList_GetNextAddress`
- `launcher.exe:0x00440cd0 = CLTIPAddressList_ResolveAndAppendToken`
- `launcher.exe:0x00440bf0 = CLTIPAddressList_ShuffleResolvedRange`
- source-file string anchor: `0x004b5f5c = \matrixstaging\runtime\src\liblttcp\ltipaddresslist.cpp`

## Object shape

Current best recovered layout is a non-virtual 0x10-byte helper:

- `+0x00` = begin pointer
- `+0x04` = end pointer
- `+0x08` = capacity pointer
- `+0x0c` = current iterator pointer

This is a small vector/iterator wrapper over resolved IPv4 dwords.

## VTable status

No `CLTIPAddressList` vtable has been recovered.

Evidence:

- no ctor/dtor writes a vtable pointer
- all recovered methods are plain `thiscall` helpers over the 4-pointer layout above
- `GetNextAddress` and `Reinit` only manipulate pointer ranges / container storage

So this class currently reads as **non-virtual**.

## `0x00440d80 = CLTIPAddressList_Reinit`

High-confidence behavior from decompilation:

- clears the active range by resetting `end = begin`
- copies the incoming host-list string to a stack scratch buffer
- tokenizes it with `strtok(..., ";")`
  - important correction from direct memory read of `_Delim_004ae628`: the delimiter is a lone semicolon, not a whitespace-trimming token set
- for each token calls `CLTIPAddressList_ResolveAndAppendToken`
- if any token fails to resolve:
  - logs `CLTIPAddressList::Reinit(): Failed to resolve addr (%s) to an IP.`
  - resets `end = begin`
  - returns early
- if flag bit `0` is set:
  - seeds `rand()` once from `time(NULL)` through global gate `0x004f79f0`
  - shuffles the resolved dword range through `CLTIPAddressList_ShuffleResolvedRange`
- logs final resolution summary and each resolved IPv4 entry
- resets current iterator pointer `+0x0c = begin`

## Flag meaning

Current best read of `flags`:

- bit `0x1` = shuffle resolved addresses
- bit `0x2` = resolve through `DNSResolveNameToIPList(..., flag=4)` path first
  - current best external meaning: **ignore hosts file** / use the no-hosts-file DNS path when available

That second bit aligns with nearby launcher config strings:

- `IgnoreHostsFileForAuth`
- `IgnoreHostsFileForMargin`
- `DNSResolveNameToIPList(): Caller used FLAG_NOHOSTSFILE flag, but the API required for that feature is not available on this machine!`

## `0x00440cd0 = CLTIPAddressList_ResolveAndAppendToken`

Current best behavior:

- if flag bit `0x2` is set, first tries `DNSResolveNameToIPList(token, this, 4)` through
  `0x00456c20 = CLTIPAddressList_ResolveAndAppendToken_NoHostsFileDnsQuery`
- otherwise falls back to `gethostbyname(token)` and copies each returned IPv4 dword into the list
  - this is now tightened beyond the earlier call-shape guess: the corresponding ws2_32 import/thunk has been renamed in Ghidra and decompilation now resolves directly as `gethostbyname`
- grows the backing range through the vector-growth helper at `0x00440c70`

## `0x00440bb0 = CLTIPAddressList_GetNextAddress`

Current best behavior:

- returns `0` when the list is empty
- if `current == end` and `wrap == false`, returns `0`
- if `current == end` and `wrap == true`, resets `current = begin`
- returns `*current` and post-increments the iterator

## Where launcher.exe uses it

Recovered launcher-side callsites:

- `0x0041b160 = CLTLoginMediator_Initialize`
  - `CLTIPAddressList_Reinit(&this+0x4c, DAT_004f7b14, authFlags)`
- `0x0041d170 = CLTLoginMediator_BeginAuthConnection`
  - `CLTIPAddressList_GetNextAddress(&this+0x4c, true)`
- `0x0041e500 = CLTLoginMediator_BeginMarginConnection`
  - `CLTIPAddressList_Reinit(&this+0x3c, marginHostList, marginFlags)`
  - `CLTIPAddressList_GetNextAddress(&this+0x3c, true)`

So `CLTLoginMediator` owns two of these helpers:

- owner `+0x3c` = margin address list
- owner `+0x4c` = auth address list

## Source alignment

Current source now models the helper more faithfully:

- `CLTLoginMediator` directly embeds `CLTIPAddressList` objects for:
  - owner `+0x3c` = margin address list
  - owner `+0x4c` = auth address list
- those embedded objects keep the recovered 0x10-byte / four-pointer layout
- source-owned backing storage for the resolved IPv4 dword arrays currently lives in a sidecar keyed by
  the helper object address so the in-object shape can stay faithful to static RE
- nearby owner state that is **outside** the helper remains separate, for example:
  - margin resolved-host cache used by source-side route-change checks
  - auth attempt counter at owner `+0x28`

Active source homes:

- `matrixstaging/runtime/src/liblttcp/ltipaddresslist.h`
- `matrixstaging/runtime/src/liblttcp/ltipaddresslist.cpp`
- `matrixstaging/runtime/src/libltnet/sys/pc/pcdns.h`
- `matrixstaging/runtime/src/libltnet/sys/pc/pcdns.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator.h`
- `matrixstaging/game/src/libltclientlogin/loginmediator.cpp`
- `matrixstaging/game/src/libltclientlogin/loginmediator_auth_entry.cpp`
