# `CMessageConnection::OnOperationCompleted` object / inheritance families

This note ties together the vtable families and tiny utility-handle helpers that meet on the
receive path centered on:

- `0x449d40 = CLTTCPConnection::OnReceive`
- `0x469bf0 = CVariableLengthPrefixedTCPStreamParser::Parse`
- `0x4490c0 = CMessageConnection::OnOperationCompleted`

It exists to keep the individual vtable docs small while still recording the higher-level family
relationships in one canonical place.

## 1. Read-fragment input family

### Proven chain

- `0x004b211c`
  - common small refcounted base contract
  - plain/non-interlocked refcount ops at `+0x04`
- `0x004b2300 = CLTTCPReadOperation`
  - concrete interlocked receive-fragment leaf used by the worker-thread receive path
  - same `+0x04` refcount field, but uses `Interlocked*`

### Tiny helper on top of that family

- `0x434fa0 = CLTTCPReadOperationFragmentRef_AssignRetained`
  - one-pointer retained-fragment handle helper
  - used by:
    - parser field `+0x04`
    - `CMessageConnection::OnOperationCompleted` stack locals while walking fragments

### Practical role on the receive path

1. worker thread allocates a `0x004b2300` fragment
2. `CLTTCPConnection::OnReceive` hands it to the parser
3. parser retains fragments into the parsed-packet work item
4. `0x4490c0` later walks those retained fragments to copy packet-body bytes

## 2. Queued work-item family

### Proven chain

- `0x004b2134`
  - shared `0x0c` work-item root / prefix
  - `+0x04 = workType`
  - `+0x08 = statusOrPayloadDword`
- `0x004b3e08 = CParsedPacketWorkItem`
  - concrete type-`3` parsed-packet work item
  - extends the shared prefix with retained-fragment traversal/cursor/body-size state

### Practical role on the receive path

1. parser accumulates fragments into the current `CParsedPacketWorkItem`
2. once a full frame is available, parser returns that object as the completed work item
3. `0x4490c0` first checks the shared root field `+0x08`
4. if zero, `0x4490c0` consumes the parsed-packet-specific fields:
   - `currentCursor24`
   - `assembledByteCount28`
   - retained-fragment traversal helpers

## 3. Completion-helper family

### Proven chain

- `0x004b3e18`
  - transient ctor/base table used during helper construction
- `0x004b3e20 = CMessageConnectionCompletionHelper`
  - live helper table after ctor finishes

### Practical role on the receive/completion path

These are the small helper children stored on `CMessageConnection`:

- connection `+0x7c` = type-`2` connect-completion helper
- connection `+0x80` = type-`1` close-completion helper

`0x4490c0` enters their embedded lock, signals their event, then leaves the lock.

## 4. Message-ref / payload family

### Proven chain

- `0x004b211c`
  - same common low-level refcounted base contract appears again here during teardown/final-release
- `0x004ba208`
  - inner payload-storage leaf
- `0x004ba220`
  - outer reset-time/base message-ref table
- `0x004ba23c`
  - live outer derived message-ref table

### Tiny helper on top of that family

- `0x4489d0 = CMessageConnectionMessageRefHandle_AssignRetained`
  - one-pointer retained outer-message-ref handle helper
  - used by read-agenda handoff / local handle seams around `0x4490c0`

### Practical role on the receive path

1. `0x4490c0` creates a fresh outer message-ref object via `0x455cd0`
2. packet-body bytes are copied into the inner payload-storage object
3. optional packet-agenda read handoff may replace that outer message-ref pointer
4. later virtual dispatch tail receives only the outer message-ref object

## End-to-end object flow

Current best object-flow picture:

```text
CLTTCPReadOperation fragment
  (0x004b2300)
        |
        v
CVariableLengthPrefixedTCPStreamParser
        |
        v
CParsedPacketWorkItem
  (0x004b3e08 on top of 0x004b2134)
        |
        v
CMessageConnection::OnOperationCompleted
  (0x4490c0)
        |
        v
CMessageConnectionMessageRef / MessageStorage
  (0x004ba23c / 0x004ba208, with 0x004ba220 as reset-time base)
        |
        v
packet-agenda read seam / local virtual dispatch tail
```

## Important cautions

### `0x004b211c` is a shared low-level contract, not one semantic class family

`0x004b211c` clearly participates in more than one higher-level object family:

- read fragments (`0x004b2300`)
- message payload/message-ref objects (`0x004ba208`, `0x004ba220`, `0x004ba23c`)

So treat it as a **shared low-level refcounted base contract**.
Do **not** assume that every child of `0x004b211c` belongs to one single concrete semantic class
hierarchy.

### `0x004ba948` is still separate

The unresolved `0x004ba948` wrapper family can expose helper/root-like behavior, but there is still
no current constructor/call-shape proof that it is the direct base of `CParsedPacketWorkItem`.

For the parsed-packet receive path, the proven work-item chain remains:

- `0x004b2134 -> 0x004b3e08`

## Related docs

- `0x004b211c.md`
- `0x004b2134.md`
- `0x004b2300.md`
- `0x004b3e08.md`
- `0x004b3e20.md`
- `0x004b7928.md`
- `0x004ba208.md`
- `0x004ba220.md`
- `0x004ba23c.md`
- `0x004baf84.md`
