# client.dll!RunClientDLL

## Current status

There are now **three distinct runtime states** worth keeping separate:

1. **Safe default scaffold**
   - uses the faithful launcher-side startup order
   - currently reaches clean `InitClientDLL = 1`
   - stops before runtime by default

2. **Deliberate binder/scaffold `RunClientDLL` experiment on the clean path**
   - same binder/scaffold setup
   - but with `MXO_FORCE_RUNCLIENT=1`
   - now reaches a stable runtime loop instead of immediately crashing

3. **Older diagnostic forced-runtime-after-failed-init path**
   - entered `RunClientDLL` after `InitClientDLL = -7`
   - crashed at `client.dll+0x3b3573`
   - still useful as historical evidence, but no longer the best description of the current binder path

## Original launcher success contract

Static review of `launcher.exe:0x40a4d0` now makes the return-value contract explicit.

After the three client exports, the original launcher does:

```asm
40a5a9: test eax,eax    ; InitClientDLL result
40a5ab: jg   0x40a61f

40a622: test eax,eax    ; RunClientDLL result
40a624: jg   0x40a698

40a6bc: test eax,eax    ; TermClientDLL result
40a6be: jg   0x40a6fb
```

And the wrapper returns overall success via:

```asm
40a6fd: mov al,0x1
```

So on this startup path the original launcher treats:
- **positive** `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns as success
- `0` or negative values as failure / error-reporting paths

That means the current binder result:
- `InitClientDLL returned: 1`

is now evidence-backed **success**, not a local guess.

## Current high-value deliberate runtime experiment

Command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_TRACE_WINDOWS=1 \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

Observed result on the current binder/scaffold path:
- `InitClientDLL` still returns cleanly with `1`
- `RunClientDLL` is entered deliberately
- no fresh crash dump is produced during timed runs
- `RunClientDLL` does **not** return within the timed run window
- the process reaches a real visible runtime window instead of dying immediately

Window-trace evidence from `resurrections.log`:
- `WindowTrace hwnd=00030058 visible=1 iconic=0 class='MATRIX_ONLINE' title='The Matrix Online' style=0x96ca0000 exStyle=0x00000100 rect=(200,150)-(600,450)`
- later on the same run:
  - `WindowTrace hwnd=00030058 visible=1 iconic=0 class='MATRIX_ONLINE' title='The Matrix Online' style=0x96000000 exStyle=0x00000008 rect=(0,0)-(800,600)`

So the deliberate runtime experiment now progresses at least this far:
- creates a real `MATRIX_ONLINE` window
- transitions it into fullscreen `800x600`
- stays alive in runtime long enough to keep polling rather than immediately faulting

## What is dynamically live now

Current `resurrections.log` on that deliberate runtime path shows repeated traffic from exactly three surfaces:

- `MediatorStub::IsConnected() -> 1`
- `LauncherObjectStub::Subobject60::Slot0(...)`
- `LauncherObjectStub::Subobject60::Slot1(...)`

Representative counts from a timed run:
- `MediatorStub::IsConnected` -> `10917`
- `LauncherObjectStub::Subobject60::Slot0` -> `10917`
- `LauncherObjectStub::Subobject60::Slot1` -> `10917`

No new crash dump was produced on that run family.

## Static explanation of the current runtime loop

`RunClientDLL` export itself is tiny:

```asm
62001180: push 0x629ddfc8
62001185: call 0x62006c30
6200118d: mov  eax,0x1
62001192: ret
```

So if the internal runtime loop ever returns normally, the exported `RunClientDLL` return observed by the launcher should be **`1`**.
That matches the original launcher success contract already recovered statically.

`TermClientDLL` now shows the same top-level success shape:

```asm
620011a0: mov  ecx,0x629ddfc8
620011a5: call 0x6216a2f0
620011aa: call 0x62002f30
620011af: mov  eax,[0x629df7f0]
...
620011c7: mov  eax,0x1
620011cc: ret
```

So if `TermClientDLL` returns normally, the exported return presented back to the launcher should also be **`1`**.

The real `RunClientDLL` work is inside `0x62006c30`.
On the current path, the important runtime branch is:

```asm
62006cb1: mov ecx, [0x629df7f0]
62006cb7: call [edx+0x2c]      ; mediator runtime gate
62006cbc: test al, al
62006cbe: je   0x62006ccf
62006cc0: mov ecx, [0x62b073e4] ; stored InitClientDLL arg5
62006cc6: test ecx, ecx
62006cc8: je   0x62006ccf
62006cca: call 0x62532130
```

That means two now-live facts matter:

1. mediator `+0x2c` is a real runtime gate on the `RunClientDLL` path
2. stored `InitClientDLL` arg5 is then fed into deeper runtime work

The arg5-side helper `0x62532130` immediately checks `[ecx+0x04]` and, on the current scaffold, falls into `0x62531c10(1)`.
That helper then does:

```asm
62531c20: mov eax, [esi+0x60]
62531c26: mov ecx, edi
62531c28: call [eax]           ; arg5 +0x60 slot 0
62531c2a: mov ecx, [esi+0x1c]
62531c2d: cmp ecx, [esi+0x0c]
62531c36: mov edx, [esi+0x44]
62531c39: cmp edx, [esi+0x34]
...
62532046: mov edx, [edi]
6253204a: call [edx+0x4]       ; arg5 +0x60 slot 1
62532053: ret 4
```

With the current queue layout this is now interpreted more precisely as:
- `+0x0c` = queue0C `current0`
- `+0x1c` = queue0C `current1`
- `+0x34` = queue34 `current0`
- `+0x44` = queue34 `current1`

A newer static comparison also shows that this client path is not some unrelated ad-hoc poll.
`client.dll:0x62531c10` is structurally the same queue-consumer family as original `launcher.exe:0x436b10`:
- both acquire arg5 helper `+0x60`
- both compare queue0C `current1/current0`
- both compare queue34 `current1/current0`
- both use arg5 helper `+0x5c` as the wait/signal helper when the queues are empty
- both release arg5 helper `+0x60` before returning

That comparison also tightens one important behavioral detail:
- `RunClientDLL` reaches `0x62532130`
- `0x62532130` calls `0x62531c10(1)`
- so the current runtime path is executing the **non-blocking poll variant** of this shared arg5 queue-consumer logic, not the blocking wait variant

So the current deliberate runtime experiment proves that all of these are now dynamically live on the runtime path:
- arg5 helper subobject at `+0x60`
- the surrounding shared queue-consumer logic at `0x62531c10`
- arg5 queue cursor comparisons at:
  - queue0C `current1` vs `current0`
  - queue34 `current1` vs `current0`

New runtime logging on the same path shows those queue cursors remain unchanged through repeated polling:
- queue0C: `current0 == current1 == block0 == block1`
- queue34: `current0 == current1 == block0 == block1`
- that remained true through at least count `1024` in the sampled run

A newer static pass on the original launcher side now explains what is missing behind that idle state.
The original arg5 producer path is `launcher.exe:0x436820 -> 0x436670`:
- `0x436820` acquires arg5 helper `+0x60`
- snapshots whether both queues were empty before enqueue
- calls `0x436670(argA, argB, queueSelect)` to push an **8-byte pair** into one of the two queues
- `queueSelect = 0` uses queue0C
- `queueSelect != 0` uses queue34
- then releases arg5 helper `+0x60`
- and if both queues were previously empty, signals arg5 helper `+0x5c` slot `0`

Representative original xrefs to that producer helper now identified statically include:
- `launcher.exe:0x4302d5`
- `launcher.exe:0x4325aa`
- `launcher.exe:0x4329cc`
- `launcher.exe:0x449d8a`

A newer pass over those concrete xrefs tightens one more detail.
In the currently identified producer callsites, the third argument to `0x436820` is always `0`, so the observed startup/runtime producer traffic is currently only evidenced for **queue0C**, not queue34.
That means:
- queue34 is definitely part of the shared consumer logic
- but no equally concrete startup-era producer xref has yet been identified for queue34 specifically

Those same xrefs also show the queued 8-byte pair shape more narrowly than before:
- first dword: typically a freshly allocated small work-item-like object
  - examples built through:
    - `0x435090` (`0x2c` allocation)
    - `0x435010` (`0x20` allocation)
    - `0x435050` (`0x0c` allocation with immediate code/value payload such as `0x7000001`)
    - `0x435070` (`0x0c` allocation with `[obj+0x04]=1`, `[obj+0x08]=0`)
- second dword: typically a stable owner/context pointer or paired object
  - examples include `[esi+0x38]`, `edi`, or other caller-held context values depending on the producer path

A broader pass over the remaining producer xrefs now shows they are not all the same one-shot shape.
Current observed queue0C producer families include:
- enqueue existing object + stable context
- enqueue freshly allocated `0x0c` / `0x20` / `0x2c` work-item object + stable context
- paired back-to-back submissions in the same function
- fallback/null submissions such as `(work=0, context=0)` or `(work=0, context=stable)`
- at least one looped submit-and-wait style path at `launcher.exe:0x449d8a`

So the next missing launcher-owned state is unlikely to be a single magic boolean alone.
The original launcher appears to drive arg5 through a **family of queue0C submissions** with multiple work-item shapes and some multi-step submission patterns.

A newer import-backed pass also tightens what kind of work those submissions represent.
The currently documented queue0C producer family is now best interpreted as a **network-engine event / status queue**, not a generic launcher task list:
- `0x4302d5` sits in a function that later calls `WS2_32!recvfrom`, so that producer is best read as receive-side / packet-side work submission
- helper `0x449b40` now resolves as a socket factory around `WS2_32!socket(AF_INET, type, protocol)` plus option setup
- original arg5 slot `6` / `Connect` (`0x4328a0`) uses that helper as `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)` and later calls `WS2_32!connect`, so its queue submissions are part of a TCP connect / connect-status family
- original arg5 slot `2` / `UDPMonitorPort` (`0x4325d0`) uses the same helper as `socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)`, then `setsockopt(...SO_REUSEADDR...)`, then `bind`, which identifies that method as a UDP bind/setup path in the same engine family
- original arg5 slot `1` / `MonitorPort` (`0x431ce0`) is now also string-backed and populates the endpoint-keyed `+0x80` side with `AcceptThread`-style worker payloads
- slot `2` / `UDPMonitorPort` success now also clearly creates a `WorkerThread` payload in `+0x8c` and marks `[worker+0x34] = 2`
- slot `6` / `Connect` success clearly creates a `WorkerThread` payload in `+0x8c` and marks `[worker+0x34] = 1`
- slot `7` / `Close` and slot `8` / `SendBuffer` both explicitly gate on connection state `1` or `2`, which makes those slot-2/slot-6 success writes more structurally relevant to later runtime behavior
- coded `0x435050(payload)` work items now look even more status-like because later wait logic at `0x449d40` checks values in the same `0x700000x` family (`0x7000000`, `0x700000b`) seen near `0x4329cc`

That makes the current runtime idle loop more specific than before:
- the scaffold is not merely missing “some arbitrary producer activity”
- it is currently missing the launcher-owned state and/or earlier socket-driving steps that should populate arg5's **network-engine queue0C** path before the client-side consumer poll begins
- with the newer original names in hand, the highest-value missing launcher activity now looks like some combination of:
  - `MonitorPort`
  - `UDPMonitorPort`
  - `Connect`
  - and the resulting `AcceptThread` / `WorkerThread` population behind arg5 `+0x80` / `+0x8c`

A further client/launcher comparison now explains why current `RunClientDLL` logs still never show arg5 primary slot `12` traffic.
When the queue is **not** empty, client `0x62531e31..0x62531fe7` mirrors launcher `0x436d31..0x436ee7`:
- it dequeues one 8-byte pair
- interprets the first dword as a queued work-item object
- interprets the second dword as an associated context/owner object
- and then calls arg5 primary vtable offset `+0x30`

On the client path that call is:

```asm
62531fb8: mov edx, [esi]
62531fba: push edi
62531fbb: mov ecx, esi
62531fbd: call [edx+0x30]   ; arg5 primary slot 12
```

A newer original-launcher pass now clarifies what happens around that milestone.
Launcher consumer `0x436d31..0x436ee7` does not simply dequeue and call slot `12` in isolation.
It:
- dequeues one `(workItem, context)` pair
- uses `0x4816f0(workItem)` to read `[workItem+0x04]` for logging / dispatch context
- calls arg5 primary slot `12` with the dequeued `context`
- then calls `context->+0x10(workItem)`

A newer worker/connection pass also improves the likely type of that `context` pointer.
It is now best treated not just as an anonymous owner pointer, but likely as a **`CMessageConnection`-family object** on important paths:
- vtable `0x4b7928` is followed by `CMessageConnection::SendPacket` / `CMessageConnection::OnOperationCompleted` strings
- constructor path around `0x448b40` stores an engine pointer at object `+0x10`
- newer ctor/vtable review now also shows that `0x448b40` first builds a base **`CLTTCPConnection`-family** object and then overwrites the vtable to `0x4b7928`
  - base vtable `0x4b8018` is now string-backed by nearby `CLTTCPConnection::OnReceive()` strings
  - so the current best read is `CLTTCPConnection` base + derived `CMessageConnection`
- methods on that class then call back into the engine and also enqueue `(work, self, 0)` through `0x436820`
- current best callback mapping on that class is now:
  - vtable `+0x10` / `0x4490c0` = likely `OnOperationCompleted(workItem)`
  - vtable `+0x20` / `0x449d20` = likely `SendPacket(...)` -> engine `+0x20` (`SendBuffer`)
  - vtable `+0x1c` / `0x449cd0` = likely endpoint-update / ensure-connected wrapper -> engine `+0x18` (`Connect`)
  - vtable `+0x0c` / `0x449ca0` = likely close/abort wrapper -> engine `+0x1c` (`Close`)
- newer wrapper-level narrowing also improves the real engine-call signatures:
  - `0x449cd0` copies a requested endpoint into `self+0x24` and then calls engine `+0x18` with **`self`**
  - `0x449ca0` forwards **`self`** into engine `+0x1c`
  - `0x449d20` forwards send args together with **`self`** into engine `+0x20`
  - so on the important current path, arg5 slots `6/7/8` are best treated as **connection-object-based** entrypoints, not merely raw `(ip,port)` helpers

And static recovery of original slot `12` (`0x4316a0`, now string-backed as `CleanupConnection`) now shows that it:
- acquires arg5 helper `+0x98`
- searches arg5 `+0x8c` by the raw context pointer as key
- consumes/removes a payload object from that pointer-keyed container
- performs teardown / state-transition work on that payload object
- and then calls `0x44ab60(context)` before releasing the lock

So the current runtime loop is not merely touching arg5 in the abstract.
It is repeatedly executing the **non-blocking consumer side** of a real queue engine, while the scaffold still has no evidence-backed producer traffic feeding even the now-best-understood queue0C startup path.
That also now explains why current deliberate `RunClientDLL` runs never reach arg5 primary slot `12`: with queue0C still empty, the consumer never advances into the queued-work branch that would call it.

## Current interpretation

The current binder/scaffold path is no longer best described as:
- "forced `RunClientDLL` immediately crashes"

It is now better described as:
- `InitClientDLL` succeeds cleanly with positive return `1`
- a deliberate `RunClientDLL` experiment reaches the actual runtime loop
- that loop repeatedly polls mediator `+0x2c` and runs the **non-blocking arg5 queue-consumer path** rooted at `0x62532130 -> 0x62531c10(1)`
- runtime logging now shows both arg5 queues staying in the same empty cursor state (`queue0C current0==current1`, `queue34 current0==current1`)
- the current scaffold therefore appears to stay on an **empty-work / idle-loop** path rather than progressing into richer runtime activity because the original arg5 producer side (`0x436820 -> 0x436670`) is still not being driven faithfully

That shifts the next runtime question again.
The most likely blocker is now:
- not an immediate runtime null dereference,
- but missing launcher-owned state that should populate or advance the arg5-owned work path beyond the current empty-loop behavior.

A newer launcher-side direct-xref pass also tightens one practical expectation there.
Current direct uses of global `0x4d6304` still only show:
- creation / mediator registration
- passing as `InitClientDLL` arg5
- descriptor embedding / default-object fallback
- teardown release

So the next missing activity may not show up as one simple obvious direct `launcher-main -> g_4d6304->Connect(...)` call on the raw global alone.
Current best reading is that the engine's meaningful runtime setup is likely driven more indirectly, through the engine's own connection/worker/helper objects and the subsystems that own them after arg5 is constructed and registered.

## New negative runtime result after partial arg5 `src/liblttcp/` wiring

A fresh deliberate runtime rerun was made after incrementally wiring the diagnostics arg5 scaffold into the new original-name classes for:
- slot `1` / `MonitorPort`
- slot `2` / `UDPMonitorPort`
- slot `6` / `Connect`
- slot `7` / `Close`
- slot `8` / `SendBuffer`
- slot `12` / `CleanupConnection`

Representative command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_TRACE_WINDOWS=1 \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

Observed result on that rerun:
- `InitClientDLL` still returns cleanly with `1`
- `RunClientDLL` still reaches the same stable visible runtime path and did not return within the timed run window
- but **no new slot `6` / `7` / `8` / `12` traffic appeared in `resurrections.log`**
- the runtime log still only showed:
  - mediator `+0x2c` / `IsConnected()`
  - arg5 helper `+0x60` slot `0`
  - arg5 helper `+0x60` slot `1`
- queue state also remained unchanged on that rerun:
  - queue0C `current0 == current1 == block0 == block1`
  - queue34 `current0 == current1 == block0 == block1`

So this partial wiring changed the internal implementation structure, but did **not** yet change the currently observed runtime behavior.
Current best reading remains:
- the live deliberate `RunClientDLL` path is still stuck in the same empty-work non-blocking consumer loop
- the newly wired class-backed slot `6/7/8/12` paths are still gated behind runtime state we are not yet reaching
- this is therefore useful negative evidence that simply moving starter semantics into `src/liblttcp/` is not, by itself, enough to make the current binder/scaffold runtime path feed queue0C or reach the later slot-12 cleanup branch
- user subjectively reported that the visible `Loading Character` phase seemed to remain on screen longer/usefully after the recent refactor, but current logs still do **not** prove a new deeper transition there; for now that should be treated as suggestive, not canonical runtime advancement
- newer implementation-side auth-connect work now also confirms that a real launcher-side TCP auth connection can be made on the binder/scaffold path before the runtime loop
- newer queue/work milestone after that wiring:
  - the replacement launcher can now deliberately enqueue original-shaped queue0C `(workItem, context)` pairs after successful auth/margin connect attempts
  - on deliberate `RunClientDLL` runs, the client now visibly consumes those queued items instead of staying purely in the old empty-cursor loop from the very first sample
  - current runtime evidence on that path now includes:
    - queue0C cursor change before first consume (`sameCursor=0`)
    - raw context callback activity:
      - `DIAGNOSTIC: raw message-connection context OnOperationCompleted ...`
    - queued work-item release activity:
      - `DIAGNOSTIC: releasing queued work item ...`
  - with auth + exact-host-overridden margin enabled together, two queued connect-status items are currently consumed in order on that path
  - newer static narrowing now also explains why that is still not enough by itself to produce the first faithful outbound auth/message send:
    - original `Connect` success path `0x4329b9..0x4329cc` constructs `0x435050(0x7000001)` which is a **type-2** status work item, then enqueues `(workItem, connection, 0)`
    - auth-side startup path `0x41d170` builds a **derived** connection object with vtable `0x4afef0`
    - margin-side startup path `0x41e500` builds another derived connection object with vtable `0x4aff38`
    - those derived families use wrapper `OnOperationCompleted` entries `0x449a70` / `0x44af60` on top of base `0x4490c0`
    - newer focused review of `CMessageConnection +0x7c / +0x80` now narrows those helper objects substantially:
      - ctor helper `0x436080` builds a `0x24` event+critical-section object
      - final helper shape is:
        - main method `+0x00` = `SetEvent(...)`
        - main method `+0x04` = `WaitForSingleObject(timeout)` with lock release/reacquire
        - embedded lock helper at `+0x04`
        - event handle at `+0x20`
      - base completion dispatcher `0x4490c0` uses them as optional completion notifiers:
        - work type `2` -> signal `connection+0x7c`
        - work type `1` -> signal `connection+0x80`
      - current best subtype read is:
        - `+0x7c` = type-2 status/connect-style completion helper
        - `+0x80` = type-1 close/terminal completion helper
      - importantly, the auth/margin startup path currently builds derived connections through `0x4417e0 -> 0x448b40(flag=0)`, so both `+0x7c` and `+0x80` remain **NULL** on that startup path
      - that means the current auth/margin type-2 connect-status path does **not** get resolved by those helper objects alone; it falls through `0x449a70 / 0x44af60` into the owner callback / fallback chain instead
    - important correction from newer message-code review:
      - later auth helper body `0x4401a0` handles raw auth message code `0x0b`
      - auth message-name mapping at `0x41bd10` uses `code - 6`, so raw `0x0b` resolves to **`AS_AuthReply`**, not `AS_GetPublicKeyReply`
      - margin/loading helper body `0x440320` similarly handles raw code `0x10`, which resolves through `0x41bf70` to **`MS_LoadCharacterReply`**, not an initial connect request
      - so those callbacks are currently best treated as later **incoming packet handlers** on the type-3 receive path, not direct proof of the first outbound request after connect
    - newer auth-side fallback-chain review now gives the strongest corrected answer yet for the auth half:
      - auth wrapper `0x449a70` runs in this order:
        - base `0x4490c0`
        - owner callback `[self+0xa4]->+0x17c`
        - fallback `0x448a60` if that callback also returns `0`
      - crucial correction: owner `+0x17c` is not fixed body `0x4401a0`
        - owner `+0x17c` = thunk `0x41f260`
        - `0x41f260` forwards to the owner's current helper/state object at `+0x10`
        - then jumps to helper vtable `+0x14`
      - so the concrete auth-side body depends on the currently selected helper from the
        `0x4f7868` family managed by `0x41b450(...)`
      - later helper body `0x4401a0` remains important, but now in the corrected role:
        - it is helper `0x4f7890` (vtable `0x4b512c`) slot `+0x14`
        - it only has a real handling path for raw code `0x0b` / **`AS_AuthReply`**
        - on the successful branch (`[reply+3] < 1`), it parses via `0x43a330`, stores reply/result state back into owner `+0x80`, appends a small owner record under `+0x684`, mirrors the current record index to owner byte `+0xcc8`, then calls:
          - `0x41b450(0x0b)`
          - string-backed `CLTLoginMediator::PostEvent(0x14)` via `0x41cfb0`
        - newer helper-side follow-up on that same success branch is still negative for first-send purposes:
          - `0x41b450(0x0b)` selects helper `0x4f7894` (vtable `0x4b5154`)
          - its concrete `+0x8` method `0x43c020` prepares owner-side data and posts event `0x15`
          - it still does **not** show the missing first auth send
        - on the failure branch (`[reply+3] >= 1`), it instead calls:
          - `0x41b450(3)`
          - string-backed `CLTLoginMediator::PostError(0x0b)` via `0x41d090`
      - fallback `0x448a60` is likewise not a send path:
        - it only logs string-backed `Got unhandled op of type %d with status %s`
        - using work-item status from `0x434d00(workItem)` and work-item type from `0x4816f0(workItem)`
      - current best auth-side conclusion is therefore stricter than the earlier softer wording:
        - the first faithful outbound auth/message send is **not** initiated from later helper body `0x4401a0`
        - and it is **not** initiated from fallback `0x448a60`
        - so this owner/helper/fallback chain is later incoming/fallback handling, not the missing first-send origin
    - newer helper-family tracing now narrows the earlier launcher-owned auth/bootstrap lead more concretely than before:
      - `0x41b450(1)` selects helper `0x4f786c`
      - helper `0x4f786c` enter path `+0x08 / 0x439090` starts auth connect through `0x41d170`
      - direct code xrefs to auth wrapper `0x41af60` still only tie down the later helper `0x4f78a0 +0x08 / 0x43b830`
        - that later auth-channel path remains:
          - `0x41af60`
          - auth connection `+0x24 / 0x41cf30`
          - auth connection `+0x28 / 0x448cf0`
          - send helper `0x448a00`
          - connection `+0x20 / 0x449d20`
          - engine `+0x20` / current best `SendBuffer`
        - and the packet object there still has raw code `0x35`, which maps through the auth table to **`AS_GetWorldListRequest`**
      - so the earlier bootstrap auth send now looks less like “another direct `0x41af60` caller we simply have not found yet” and more like a different launcher-owned object path
      - current strongest earlier credential/bootstrap auth lead is now helper `0x4f7870` selected through `0x41b450(2)`:
        - `0x439210` first gates on `0x41b490()` (`auth connection state == 2`)
        - if auth is not connected yet, it falls back to `0x41b450(1)`
        - on the connected branch it gathers launcher-owned owner data through:
          - owner `+0x168`
          - owner `+0x20`
          - owner `+0x38`
        - then calls `0x448050`, which is currently only xref'd from `0x439210`
      - `0x448050` now also resolves more concretely than before:
        - it runs as a method on the extra owner child allocated by `0x41290` / base ctor `0x45500`
          and stored at owner `+0x680`
        - that phase-2 bootstrap object is size `0x11c`
        - it copies three string sources plus two 16-byte blocks into bootstrap state and stores
          an outbound send target at bootstrap `+0x50`
        - crucial correction: the branch at `0x44811e` is not just a loose byte-ish mode flag
          but the nullness of **pointer `+0xa0`** inside that bootstrap object
      - `0x448050` then branches by bootstrap helper pointer `+0xa0` into two launcher-owned outbound packet builders that both send indirectly through bootstrap `+0x50 -> +0x24`:
        - `0x447eb0`
          - taken when bootstrap helper pointer `+0xa0 == NULL`
          - builds/sends raw auth code `0x06`
          - strongest current **`AS_GetPublicKeyRequest`** candidate
        - `0x4474f0`
          - taken when bootstrap helper pointer `+0xa0 != NULL`
          - builds/sends raw auth code `0x08`
          - strongest current **`AS_AuthRequest`** candidate
          - also emits a later auxiliary raw `0x1b` packet on that same indirect path
      - newer bootstrap-object follow-up now also tightens what those fields mean later:
        - `0x429b0` uses bootstrap helper pointer `+0xa0 -> +0x1c` on a later incoming path
        - it writes 16-byte challenge-derived material to bootstrap `+0x85 .. +0x94`
        - then `0x41470` derives/caches a dword-ish token at bootstrap `+0x9c`
        - that same `+0x9c` value is then used by both raw `0x06` and raw `0x08` send builders
      - important channel-specific correction from the same pass still holds:
        - margin-side wrapper traffic must be read through the separate margin table
        - for example, raw code `0x06` on `0x41af70` maps to **`MS_GetClientIPRequest`**, not auth-side `AS_GetPublicKeyRequest`
      - current best auth-side conclusion is therefore now tighter than “completely unresolved before world-list”:
        - the missing earlier launcher-owned auth/bootstrap send still sits **before** the later `AS_GetWorldListRequest` path
        - but the highest-value concrete target is now the **phase-2 helper chain**
          - `0x4f7870 +0x08 / 0x439210`
          - `0x448050`
          - `0x447eb0` raw `0x06` / candidate `AS_GetPublicKeyRequest`
          - `0x4474f0` raw `0x08` / candidate `AS_AuthRequest`
        - the remaining unresolved detail is no longer the coarse role of `+0xa0` itself
        - it is now the more specific identity/ownership of:
          - the bootstrap helper pointer stored at `+0xa0`
          - the outbound send target stored at `+0x50`
          - and the exact selected-source object shape passed into `0x448050`
        - but the launcher-owned auth progression claim itself is now stronger, not weaker
    - new env-gated diagnostic runtime validation against the live auth socket now gives the first concrete outbound/reply pair on this launcher-owned auth path:
      - command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_BEGIN_AUTH_CONNECTION=1 \
  MXO_DIAGNOSTIC_SEND_AS_GETPUBLICKEY=1 \
  MXO_DIAGNOSTIC_AUTH_LAUNCHER_VERSION=76005 \
  MXO_DIAGNOSTIC_AUTH_CUR_PUBLIC_KEY_ID=0 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

      - important scope label:
        - this is a **diagnostic bootstrap experiment**, not a claim of faithful original-equivalent launcher progression yet
        - `76005` is a practical guessed launcher-version value from current `7.6005`, not yet a fully recovered canonical owner-field proof
      - current evidence-backed result from that run:
        - env-gated send log:
          - `auth bootstrap experiment rawCode=0x06 request='AS_GetPublicKeyRequest' launcherVersion=76005 currentPublicKeyId=0 byteCount=10 -> sendResult=0x00000001`
        - a new queued auth receive item then appears on the same runtime path:
          - `queued connection-status work item label='AuthReceivePacket' ... type=3 payload=0x00000198`
        - receive preview + framing decode now log:
          - `received-bytes label='AuthConnection' total=408 preview=81 96 07 ... 04 00 00 00 12`
          - `auth receive framing payloadLength=406 headerBytes=2 rawCode=0x07 likelyMessage='AS_GetPublicKeyReply'`
        - newer static formatter alignment with `0x44e20` now also makes the preview itself more informative than a generic blob:
          - `81 96` = 2-byte length prefix for payload `0x0196 = 406`
          - payload byte `0x07` = `AS_GetPublicKeyReply`
          - next dword `00 00 00 00` = plausible reply status `0`
          - next dword = plausible `CurrentTime`
          - next dword `04 00 00 00` = plausible `PublicKeyId = 4`
          - next byte `0x12` aligns with the later formatter's `KeySize` field
      - strongest current reading from that diagnostic pair:
        - the small-payload send framing model taken from `0x448a00` is now runtime-supported enough to matter
        - the raw `0x06` packet shape from `0x447eb0` is now runtime-supported enough to matter
        - and the server reply beginning with raw `0x07` strongly supports the current `AS_GetPublicKeyRequest -> AS_GetPublicKeyReply` read on this launcher-owned auth path
      - that still does **not** prove the full faithful original field population yet
        - but it materially upgrades the auth-bootstrap work from pure static suspicion to a live send/reply diagnostic foothold
    - current deliberate queue injection still uses a raw diagnostic context callback, so it bypasses that original post-connect auth/margin completion chain
  - current runtime log now makes that bypass/narrowing explicit with the updated static lead carried in source scaffolding as:
    - `DIAGNOSTIC: routed auth type-2 connect-status payload=0x07000001 into CLTLoginMediator scaffold -> handled=1 nextOutboundRequest='phase2-bootstrap: +0xa0 NULL => AS_GetPublicKeyRequest, non-NULL => AS_AuthRequest' laterIncomingReplyAnchor='AS_AuthReply'`
- this is still not faithful original-equivalent queue semantics yet, but it is a concrete step past the previous totally empty queue0C runtime state
- newer non-blocking live-socket receive polling is now also wired into the helper `+0x60` runtime surface, but current timed auth-connect runs have not yet produced any logged type-3 receive work items (`AuthReceivePacket` / `MarginReceivePacket`) on this path
- fresh validation reruns after the auth-side owner/fallback-chain narrowing did **not** change the live runtime picture:
  - plain `make run_binder_both` still ends at:
    - `InitClientDLL returned: 1`
    - `InitClientDLL succeeded, but RunClientDLL is gated.`
  - deliberate runtime/auth command:

```bash
cd /home/morgan/mxo/code/matrix_launcher && \
  MXO_FORCE_RUNCLIENT=1 \
  MXO_BEGIN_AUTH_CONNECTION=1 \
  MXO_ARG7_SELECTION=0x0500002a \
  MXO_MEDIATOR_SELECTION_NAME=Vector \
  make run_binder_both
```

  - still shows one queued auth type-2 connect-status item consumed:
    - `queued connection-status work item label='AuthConnectStatus' ... type=2 payload=0x07000001`
    - `raw message-connection context OnOperationCompleted ... type=2 payload=0x07000001`
    - `routed auth type-2 connect-status ... nextOutboundRequest='phase2-bootstrap: +0xa0 NULL => AS_GetPublicKeyRequest, non-NULL => AS_AuthRequest' ...`
    - `releasing queued work item ...`
  - after that first consume, queue0C returns to the same empty-cursor idle loop:
    - first sample: `sameCursor=0`
    - later samples: `sameCursor=1`
  - repeated live surfaces remained unchanged through the timed run:
    - mediator `+0x2c` / `IsConnected()`
    - arg5 helper `+0x60` slot `0`
    - arg5 helper `+0x60` slot `1`
  - no logged type-3 receive work items appeared before timeout
  - no new runtime surface yet contradicted the newer negative conclusion that `0x4401a0` / `0x448a60` are not the missing first-send origin
- current user-observed runtime impression after these queue/connect milestones is that the in-game `Loading Character` phase now appears to remain visible/useful longer than before; current logs still do **not** prove a later faithful transition there yet, but this is now consistent with the stronger runtime markers that the path is doing more than the old totally empty loop

## Relationship to the older `client.dll+0x3b3573` crash

The older forced-runtime crash remains useful historical evidence:
- it came from running `RunClientDLL` after failed `InitClientDLL = -7`
- it still documents what happened on that intentionally invalid path

But it is no longer the best canonical description of the current binder/scaffold runtime state.
The current clean-init deliberate runtime path now has a more valuable signature:
- stable fullscreen window
- repeated mediator `+0x2c`
- repeated arg5 `+0x60`
- no immediate crash during timed runs

## Related docs

- `../InitClientDLL/README.md`
- `CRASH_623B3573.md`
- `../../launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../launcher.exe/startup_objects/0x4d6304_network_engine.md`
- `../../launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`
