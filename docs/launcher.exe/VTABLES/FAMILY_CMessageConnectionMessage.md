# Family - `CMessageConnectionMessage`

## Current best family name

This is the outer-message-ref / inner-payload-storage family materialized on the later receive and
send paths.

## Inheritance tree

```text
shared low-level refcounted base contract
0x004b211c  CRefCountedReadOperationBase
├── 0x004ba208  CMessageConnectionMessageStorage
└── 0x004ba220  CMessageConnectionMessageRefBase
    └── 0x004ba23c  CMessageConnectionMessageRef
```

## Utility helper tied to this family

```text
0x4489d0  CMessageConnectionMessageRefHandle_AssignRetained
```

That helper is not a vtable family member itself.
It is a tiny one-pointer retained outer-message-ref handle used by local agenda/dispatch seams.

## Proven roles

### `0x004ba208 = CMessageConnectionMessageStorage`

Inner payload-storage leaf:

- payload length header at `+0x0a/+0x0b`
- payload bytes begin at `+0x0c`

### `0x004ba220 = CMessageConnectionMessageRefBase`

Base/reset-time outer table:

- installed first by `0x455bd0`
- owns a pointer to the inner payload-storage object at `+0x0c`
- not the final live public table

### `0x004ba23c = CMessageConnectionMessageRef`

Live outer receive/send message-ref table:

- produced by `0x455c60`
- returned by `0x455cd0 = CMessageConnectionMessage_CreateRef`
- this is the object later passed down the `0x4490c0` post-copy virtual dispatch tail

## Receive-path role

```text
CMessageConnection::OnOperationCompleted
  -> CreateRef()
  -> copy parsed-packet payload into inner storage
  -> optional packet-agenda read handoff may replace outer ref
  -> later virtual tail receives only the outer message-ref object
```

## Important caution about `0x004b211c`

Here `0x004b211c` is best read as a **shared low-level refcounted base contract**, not proof that
message objects and TCP read fragments are one single semantic class family.

It is reused by both:

- `CLTTCPReadOperation`
- `CMessageConnectionMessage*`

So the most helpful tree is the semantic one above, with the explicit note that the low-level base
contract is shared.

## Current source-fidelity note

A fresh pass on the shared refcount helpers tightened one concrete source mismatch:
- the shared base contract `0x004b211c` is indeed **non-interlocked** and source was already
  matching that correctly
- but the derived message-storage / outer-message-ref families anchored to
  `0x004ba208/0x004ba220/0x004ba23c` reuse the interlocked `0x42f850/0x42f860` pair
- source had added extra safety guards before `InterlockedDecrement` on those message-object
  `Release()` methods (`if current<=0 return 0`), which is lower fidelity than the original helper
  body
- source now mirrors the original more closely there by always doing the interlocked decrement and
  only branching on the resulting zero-count final-release case

## Related docs

- `CMessageConnection_OnOperationCompleted_families.md`
- `FAMILY_CLTTCPReadOperation.md`
- `0x004b211c.md`
- `0x004ba208.md`
- `0x004ba220.md`
- `0x004ba23c.md`
- `0x004b7928.md`
