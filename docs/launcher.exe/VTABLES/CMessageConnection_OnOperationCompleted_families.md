# `CMessageConnection::OnOperationCompleted` family overview

This is now an **overview/index** for the multiple object families that meet on the receive path.
The detailed family trees live in separate docs.

## Top-level tree

```text
0x449d40  CLTTCPConnection::OnReceive
    |
    +-- read-fragment family
    |      0x004b211c  CRefCountedReadOperationBase   (shared low-level base contract)
    |      └── 0x004b2300  CLTTCPReadOperation
    |          helper: 0x434fa0  CLTTCPReadOperationRefHandle_AssignRetained
    |
    +-- parser bridge object
    |      0x004baf84  CVariableLengthPrefixedTCPStreamParser
    |
    +-- queued work-item family
    |      0x004b2134  CLTThreadPerClientTCPEngine_WorkItemHeader
    |      └── 0x004b3e08  CParsedPacketWorkItem
    |
    +-- completion-helper child-object family
    |      0x004b3e18  CMessageConnectionCompletionHelper ctor/base table
    |      └── 0x004b3e20  CMessageConnectionCompletionHelper
    |
    +-- message-ref / payload family
           0x004b211c  CRefCountedReadOperationBase   (same shared low-level base contract reused)
           ├── 0x004ba208  CMessageConnectionMessageStorage
           └── 0x004ba220  CMessageConnectionMessageRefBase
               └── 0x004ba23c  CMessageConnectionMessageRef
                   helper: 0x4489d0  CMessageConnectionMessageRefHandle_AssignRetained
```

## Separate family docs

- `FAMILY_CLTTCPReadOperation.md`
- `FAMILY_CLTThreadPerClientTCPEngine_WorkItem.md`
- `FAMILY_CMessageConnectionCompletionHelper.md`
- `FAMILY_CMessageConnectionMessage.md`

## Naming direction

Current best named families are now:

- `CLTTCPReadOperation`
- `CLTThreadPerClientTCPEngine` queued work-item family
- `CMessageConnectionCompletionHelper`
- `CMessageConnectionMessage`

with these important helper names now in use too:

- `CLTTCPReadOperationRefHandle_AssignRetained`
- `CMessageConnectionMessageRefHandle_AssignRetained`

## Important caution

`0x004b211c` should be treated as a **shared low-level refcounted base contract** reused by more
than one semantic family.

That means the most useful documentation split is:

- one overview/index doc here
- one separate doc per semantic family

rather than one giant combined inheritance note that over-implies a single monolithic tree.

## Related docs

- `0x004b7928.md`
- `0x004baf84.md`
- `0x004b2300.md`
- `0x004b3e08.md`
- `0x004b3e20.md`
- `0x004ba23c.md`
