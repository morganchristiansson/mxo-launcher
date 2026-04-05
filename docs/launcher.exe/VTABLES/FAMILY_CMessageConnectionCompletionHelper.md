# Family - `CMessageConnectionCompletionHelper`

## Current best family name

This is the small event + embedded-lock helper family used by `CMessageConnection` for work-type
`1` and `2` completion signaling.

## Inheritance tree

```text
0x004b3e18  CMessageConnectionCompletionHelper ctor/base table
└── 0x004b3e20  CMessageConnectionCompletionHelper
```

## Proven roles

### `0x004b3e18`

Transient ctor/base table seen during `0x436080` construction.
This is not the final live table used by normal completion handling.

### `0x004b3e20 = CMessageConnectionCompletionHelper`

Live helper table with two rows:

- `0x435f90 = CMessageConnectionCompletionHelper_SignalEvent`
- `0x435fa0 = CMessageConnectionCompletionHelper_LeaveLockAndWaitForEvent`

Recovered object layout:

- `+0x00` = helper vtable
- `+0x04` = embedded lock-helper root
- `+0x08` = embedded `CRITICAL_SECTION`
- `+0x20` = event handle

## `CMessageConnection` ownership

```text
CMessageConnection
├── +0x7c  connectCompletionHelper
└── +0x80  closeCompletionHelper
```

`CMessageConnection::OnOperationCompleted` uses them like this:

- type `2` -> signal `+0x7c`
- type `1` -> signal `+0x80`

## Scope caution

This helper family is **not** part of the queued work-item inheritance chain.
It is a child-object/helper family stored on the connection object itself.

## Related docs

- `CMessageConnection_OnOperationCompleted_families.md`
- `0x004b3e20.md`
- `0x004b7928.md`
