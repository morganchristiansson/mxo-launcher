# Family - `CLTThreadPerClientTCPEngine` queued work items

## Current best family name

This is the shared queued work-item family consumed by connection completion handlers.
On the active `CMessageConnection::OnOperationCompleted` path, the important concrete leaf is
`CParsedPacketWorkItem`.

## Inheritance tree

```text
0x004b2134  CLTThreadPerClientTCPEngine_WorkItemHeader
├── 0x004b3df8  type-2 connection-status work item
├── 0x004b3e00  type-1 close work item
└── 0x004b3e08  CParsedPacketWorkItem
```

## Shared helper getters over the root prefix

These are not separate family members, but they are the canonical accessor helpers for the shared
root:

- `0x4816f0 = CLTThreadPerClientTCPEngine_WorkItemHeader_GetWorkType`
- `0x434d00 = CLTThreadPerClientTCPEngine_WorkItemHeader_GetStatusOrPayloadDword`

## Proven roles

### `0x004b2134 = CLTThreadPerClientTCPEngine_WorkItemHeader`

Shared `0x0c` work-item root/prefix:

- `+0x00` = vtable
- `+0x04` = work type
- `+0x08` = status/payload dword

Current active-path use inside `0x4490c0`:

- type `1` = close completion
- type `2` = connect completion
- type `3` = parsed packet

### `0x004b3df8 = type-2 connection-status work item`

Small concrete `0x0c` queued item used for worker connect-status submission.

### `0x004b3e00 = type-1 close work item`

Small concrete `0x0c` queued item used for worker close / peer-closed submission.

### `0x004b3e08 = CParsedPacketWorkItem`

Concrete type-`3` queued work item used after the parser assembles a full framed packet.

Important derived fields consumed by `0x4490c0`:

- retained fragments
- `currentCursor24`
- `assembledByteCount28`

## Receive-path role

```text
parser accumulates retained fragments
  -> emits CParsedPacketWorkItem
  -> queued callback enters CMessageConnection::OnOperationCompleted
  -> shared root prefix checked first
  -> derived parsed-packet state consumed next
```

## Scope caution

Do **not** fold the separate `CMessageConnectionCompletionHelper` event object into this family.
That helper participates in work-type `1/2` handling, but it is a different child-object family,
not a work-item-root-derived queued object.

## Related docs

- `CMessageConnection_OnOperationCompleted_families.md`
- `FAMILY_CLTTCPReadOperation.md`
- `0x004b2134.md`
- `0x004b3df8.md`
- `0x004b3e00.md`
- `0x004b3e08.md`
- `0x004b7928.md`
- `0x004baf84.md`
