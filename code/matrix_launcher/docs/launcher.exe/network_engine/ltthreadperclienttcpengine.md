# CLTThreadPerClientTCPEngine_0x4b2768

Current high-confidence launcher.exe findings used by `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.{h,cpp}`.

## Object layout

- derived ctor: `launcher.exe:0x431c30`
- base ctor: `launcher.exe:0x4366f0`
- primary vtable: `launcher.exe:0x4b2768`
- recovered base datatype name in Ghidra: `CLTBaseThreadPerClientTCPEngine_0x4b3e74`
- recovered lock helper datatype name in Ghidra: `CLTCriticalSectionHelper_0x4add70`

Recovered field roles:

- `+0x0c..+0x5b` = one inline completed-operation queue-pair storage object
  - Ghidra datatype: `CLTBaseThreadPerClientTCPEngine_QueuePair_0x436610`
  - first repeated queue record: pair `+0x00` / engine `+0x0c`
  - second repeated queue record: pair `+0x28` / engine `+0x34`
  - important modeling note: the inner `0x28` queue shape is still treated as a repeated recovered
    record layout, not a proven original OO class boundary
- `+0x60` = `queueLockHelper60` (`CLTCriticalSectionHelper_0x4add70`)
- `+0x98` = `cleanupLockHelper98` (`CLTCriticalSectionHelper_0x4add70`)
- `+0x80` = allocated `0x24` endpoint-tree head
- `+0x8c` = allocated `0x18` context-tree head

## Tree helpers

- `0x42fdb0` searches the endpoint tree and returns either the matching node or the tree-head sentinel.
- `0x42fe10` searches the context tree and returns either the matching node or the tree-head sentinel.
- Tree payloads are direct object pointers:
  - endpoint node payload `[node+0x20]` = `AcceptThread*`
  - context node payload `[node+0x14]` = `WorkerThread*`

## Wrapper boundary

- Current binding model is one active arg5 shell attached to one `CLTThreadPerClientTCPEngine_0x4b2768` sidecar through `CLTThreadPerClientTCPEngine_0x4b2768Binding`.
- Important queue-family experiment result:
  - moving live queue/event/lock ownership onto the real engine object did **not** break launcher-side auth flow
  - but removing shell-visible queue-pair coherence stalled post-`InitClientDLL` margin progression before `CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate(status=0)`
  - restoring shell-visible `+0x0c..+0x5b` queue-pair mirroring was enough to recover the `MarginConnectStatus` handoff
- Current best read: post-client-start margin polling still consults arg5-visible queue-pair bytes, even though helper entry surfaces at `+0x5c/+0x60` can remain thin wrappers over engine-owned event/lock behavior.

## Worker creation / queueing

- `0x431ff0` allocates a worker thread, stores it at `[connection+0x08]`, inserts it into the context tree under the `+0x98` cleanup lock, and optionally starts it.
- `0x436340` initializes one repeated `0x28` queue-shaped record.
- `0x436450` grows/appends block storage for one repeated queue-shaped record.
- `0x436610` initializes the inline queue-pair storage at base `+0x0c` by zeroing two adjacent
  repeated records and calling `0x436340` for each.
- `0x436670` pushes `(workItem, context)` into either the first record or the second record inside
  that queue-pair storage.
- `0x4364d0` is still an engine/base dequeue helper; it checks queue-thread count, takes the
  queue lock, waits through the `+0x5c` helper, and prefers the second queue record before the
  first.
- `0x436820` is the enqueue boundary.
- `0x436b10` is the main completed-operation queue consumer body.

## Thread base

- `0x4528d0` = `CLTThread::Start`
- `0x452800` = `_beginthreadex` start thunk / thread main sequencing
- `0x452660` = `CLTThread::Stop`

Important fidelity note:

- `0x452660` calls `ExitThread(0)` on self-stop in launcher.exe before taking the terminate path.
- Current source intentionally does not hard-exit on self-stop yet, to keep launcher behavior stable while the surrounding lifecycle remains partially scaffolded.
