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
0x434fa0  CLTTCPReadOperationFragmentRef_AssignRetained
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
- `0x42f850 = CLTTCPReadOperation_AddRef`
  - interlocked increment of `+0x04`
- `0x42f860 = CLTTCPReadOperation_Release`
  - interlocked decrement of `+0x04`
  - zero -> vtable `+0x0c`
- `0x452350 = CLTTCPReadOperation_SetByteCount`
  - clamps/writes `+0x08`

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

- `matrixstaging/runtime/src/liblttcp/lttcpconnection.h/.cpp`
  - `CRefCountedReadOperationBaseScaffold` mirrors the shared `0x004b211c` base contract
  - `CLTTCPReadOperationFragmentScaffold` mirrors the live `0x004b2300` leaf and keeps the exact
    12-byte prefix explicit (`sizeof(...) == 0x0c`)
  - payload bytes are now represented as the variable-length tail beginning immediately after that
    prefix rather than as a fake inline `bytes0C[1]` field with a hand-built vtable object
- parser / connection call sites still use the same recovered helper seam, so this fidelity change
  tightened the class modeling without widening the active receive-path behavior

## Related docs

- `CMessageConnection_OnOperationCompleted_families.md`
- `FAMILY_CLTThreadPerClientTCPEngine_WorkItem.md`
- `0x004b211c.md`
- `0x004b2300.md`
- `0x004baf84.md`
- `0x004b7928.md`
