# Family - `CLTTCPReadOperation`

## Current best family name

This is the receive-fragment input family that feeds the parser and ultimately
`CMessageConnection::OnOperationCompleted`.

## Inheritance tree

```text
shared low-level refcounted base contract
0x004b211c  CRefCountedReadOperationBase
└── 0x004b2300  CLTTCPReadOperation
```

## Utility helper tied to this family

```text
0x434fa0  CLTTCPReadOperationRefHandle_AssignRetained
```

That helper is not a vtable family member itself.
It is a tiny one-pointer retained-fragment handle used by:

- parser field `CVariableLengthPrefixedTCPStreamParser.currentCursorFragmentRef04`
- `CMessageConnection::OnOperationCompleted` stack locals during retained-fragment traversal

## Proven roles

### `0x004b211c = CRefCountedReadOperationBase`

Shared low-level base contract with:

- deleting dtor
- plain/non-interlocked AddRef / Release
- ResetRefCount
- SetRefCountFromPtr

Caution:
- this base contract is reused outside the read-fragment family too
- it also reappears under the `CMessageConnectionMessage` family
- tiny helper-body reuse also leaks into the unrelated `0x004c0540` abstract helper cluster
  - `0x42f7e0` is reused there as slot `+0x00`
  - that OOAnalyzer namespace collision does **not** mean `0x004c0540` is the read-operation base

### `0x004b2300 = CLTTCPReadOperation`

Concrete live receive-fragment leaf used by the worker-thread receive path.

Recovered front matter:

- `+0x00` = vtable
- `+0x04` = interlocked refcount
- `+0x08` = byte count
- `+0x0c` = first payload byte

Key behavior:

- `0x42fd50 = CLTTCPReadOperation_dtor`
  - collapses back to `0x004b211c`
  - delete-flag free path goes through `0x452520`, not a raw `free(this)` call
- `0x42f850 = CLTTCPReadOperation_AddRef`
  - interlocked increment of `+0x04`
- `0x42f860 = CLTTCPReadOperation_Release`
  - interlocked decrement of `+0x04`
  - zero -> vtable `+0x0c`
- `0x452350 = CLTTCPReadOperation_SetByteCount`
  - clamps/writes `+0x08`

Allocator/free helpers tied to the live receive path:

- `0x452560 = CLTTCPReadOperation_AllocateStorage`
  - fixed-size wrapper that always requests `0x100c`
- `0x452400 = CLTTCPReadOperationFixedAllocator_AllocateStorage`
  - lock-protected fixed-size pool
  - backing-block list head = `0x004f817c`
  - free-list head = `0x004f8180`
- `0x452520 = CLTTCPReadOperation_FreeStorage`
  - returns one fragment allocation back to that same free-list

## Receive-path role

```text
worker thread
  -> allocates CLTTCPReadOperation
  -> recv(..., fragment+0x0c, ...)
  -> CLTTCPConnection::OnReceive(fragment)
  -> CVariableLengthPrefixedTCPStreamParser::Parse(fragment, ...)
  -> retained into CParsedPacketWorkItem
```

So this family is the **front-end input fragment family**, not the queued parsed-packet work-item
family.

## Source lockstep

Current source now models this family as an actual class hierarchy instead of a raw prefix struct
plus manual vtable record:

A newer fidelity pass also tightened the tiny retained-fragment handle seam recovered at
`0x434fa0 = CLTTCPReadOperationRefHandle_AssignRetained`:
- parser field `+0x04` is no longer treated in source as just a naked fragment pointer slot
- source now models it as a one-pointer retained-fragment handle helper that does:
  - Release old fragment
  - store new fragment
  - AddRef new fragment
- the static RE now also narrows the second arg more accurately:
  - it is any one-pointer fragment slot (parser field or stack local), not only a dedicated helper
    object instance
- this is a better match for the original parser/current-cursor lifetime model and removes another
  raw-pointer shortcut from the active receive path

- `matrixstaging/runtime/src/liblttcp/lttcpconnection.h/.cpp`
  - `CRefCountedReadOperationBase` mirrors the shared `0x004b211c` base contract
  - `CLTTCPReadOperation` mirrors the live `0x004b2300` leaf and keeps the exact 12-byte prefix
    explicit (`sizeof(...) == 0x0c`)
  - payload bytes are represented directly as the variable-length tail beginning immediately after
    that prefix, not as a fake inline field or wrapper-accessor layer
  - source no longer uses `calloc/free` on this seam
    - `operator new(std::nothrow)` now mirrors the fixed-size `0x452560 -> 0x452400` allocator path
    - deleting dtor / `operator delete` now return storage through the same `0x452520`-style
      free-list path
    - current source also mirrors the original backing-block-list vs free-list split, even though
      the tracked-allocation counters and first-block sizing heuristic from `0x452400` are still a
      narrower follow-up gap
- `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
  and `messageconnection.cpp`
  - parser / consumer code now uses the recovered class layout directly
  - source-only AddRef/Release/payload wrapper helpers over the read-operation family have been
    removed from this seam
  - parsed-packet consumer copy now also walks retained fragments through the recovered
    `0x4350c0 / 0x435510 / 0x434fa0` traversal/handle chain instead of source-owned raw first/next
    fragment shortcuts

## Family boundary note

Do not merge the unrelated abstract helper cluster rooted at `0x004c0540` into this family.

Current best split is:
- `FAMILY_CLTTCPReadOperation.md`
  - the real receive-fragment family rooted at `0x004b211c -> 0x004b2300`
- `FAMILY_0x004c0540_refcounted_buffer_cluster.md`
  - a separate abstract non-interlocked refcounted buffer/helper cluster

The confusion came from tiny helper-body reuse only, not from a proven inheritance link.

## Related docs

- `CMessageConnection_OnOperationCompleted_families.md`
- `FAMILY_CLTThreadPerClientTCPEngine_WorkItem.md`
- `FAMILY_0x004c0540_refcounted_buffer_cluster.md`
- `0x004b211c.md`
- `0x004b2300.md`
- `0x004baf84.md`
- `0x004b7928.md`
