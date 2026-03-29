# pcdns.cpp / DNSResolveNameToIPList

## High-confidence recovered function

- `launcher.exe:0x0046e910 = DNSResolveNameToIPList`
- source-file string anchor: `0x004bbdb8 = \matrixstaging\runtime\src\libltnet\sys\pc\pcdns.cpp`

## Current best behavior

From decompilation of `0x0046e910`:

- loads `dnsapi.dll` dynamically with `LoadLibraryA("dnsapi.dll")`
- resolves two procedures with `GetProcAddress`:
  - `DnsQuery_A`
  - `DnsRecordListFree`
- if either procedure is missing:
  - logs
    `DNSResolveNameToIPList(): Caller used FLAG_NOHOSTSFILE flag, but the API required for that feature is not available on this machine!`
  - returns failure
- otherwise:
  - if input flag bit `0x04` is set, passes query flag `0x40`
    - current best symbolic identity: `DNS_QUERY_NO_HOSTS_FILE`
  - performs `DnsQuery_A(name, DNS_TYPE_A, queryFlags, ...)`
  - iterates the returned record list
  - accepts A records whose section low bits are `0` or `1`
  - appends each IPv4 address into the caller-supplied list object
  - frees the returned DNS record list with `DnsRecordListFree(..., 1)`
- frees `dnsapi.dll` before returning

## Relationship to CLTIPAddressList

Current best call chain:

- `0x00440d80 = CLTIPAddressList_Reinit`
- `0x00440cd0 = CLTIPAddressList_ResolveAndAppendToken`
- `0x00456c20 = CLTIPAddressList_ResolveAndAppendToken_NoHostsFileDnsQuery`
- `0x0046e910 = DNSResolveNameToIPList`

That path is only reached when `CLTIPAddressList` flag bit `0x2` is set.

Current best meaning of that bit:

- `CLTIPAddressList bit 0x2` = ignore hosts file
- `DNSResolveNameToIPList bit 0x04` = no-hosts-file DNS query

That aligns with nearby config strings:

- `IgnoreHostsFileForAuth`
- `IgnoreHostsFileForMargin`

## VTable status

No `pcdns.cpp`-owned class vtable has been recovered from this focused pass.

Current evidence says this file is centered on a plain helper function, not on a vtable-backed object family:

- the source-file string xref pass only surfaced `0x0046e910 = DNSResolveNameToIPList`
- no ctor/dtor/vtable write for a `pcdns.cpp` class family has been found

## Source alignment

Current source home:

- `matrixstaging/runtime/src/libltnet/sys/pc/pcdns.h`
- `matrixstaging/runtime/src/libltnet/sys/pc/pcdns.cpp`

Current source uses the same high-value shape:

- dynamic `dnsapi.dll` load
- `DnsQuery_A` / `DnsRecordListFree` lookup
- `DNS_QUERY_NO_HOSTS_FILE` when the recovered no-hosts-file flag is set
- A-record extraction into the shared `CLTIPAddressList` scaffold
