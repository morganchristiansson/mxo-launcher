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

### Queued connection-context seam

Current highest-value client.dll-facing ABI seam is the queued connection-context adapter rooted in
`CBaseConnection_QueueContextScaffold`.

Current recovered queue-consumer slot contract:
- object size: `0x0c`
- `+0x00` = vfptr / synthetic vtable pointer
- `+0x04` = auto-release flag tested on type-1 close work
- `+0x08` = owning `CBaseConnection_0x4b8018*`
- `vtable[1]` / slot `+0x04` = optional auto-release entry
- `vtable[4]` / slot `+0x10` = `OnOperationCompleted(void*)`

Important modeling note:
- this adapter is still a synthetic minimal slot surface, not yet a proven drop-in native
  `CBaseConnection_0x4b8018`-family object
- replacing it with a real class requires matching the queue consumer's raw slot expectations, not
  just matching field layout


- Current binding model is one active arg5 shell attached to one `CLTThreadPerClientTCPEngine_0x4b2768` sidecar through `CLTThreadPerClientTCPEngine_0x4b2768Binding`.
- Queue-family experiment result:
  - moving live queue/event/lock ownership onto the real engine object did **not** break launcher-side auth flow
  - but post-`InitClientDLL` client.dll margin progression still directly consumes the arg5 queue runner / queue subobject family
  - simple shell queue-pair copying restored visibility but created split-brain mutable queue state and later crashed inside client.dll queue consumption
- Current best read:
  - arg5 queue bytes at `+0x0c..+0x5b` must remain the authoritative live completed-operation queue storage while a launcher shell is attached
  - helper entry surfaces at `+0x5c/+0x60` can stay as ABI wrappers that forward into engine-owned event/lock behavior
  - the engine sidecar therefore has to operate on the attached shell queue-pair storage, not on a separately-mutated copied mirror
  - current cleanup-lock pruning pass routes arg5 `+0x98` helper calls into the engine-owned cleanup lock while still leaving the shell-local embedded `CRITICAL_SECTION` initialized as inert ABI backing until we prove no raw byte/probe dependence remains
  - current tree-family pruning pass removes shell-side fake pre-allocation of `+0x80` / `+0x8c` tree heads; after attach those pointer fields are populated only from the real engine object's authoritative tree storage
  - the shell no longer mirrors `+0x84` / `+0x90` live tree counts from the engine sidecar; those dwords stay as inert ABI padding unless proven client-visible

## Small queued work items

- `ConnectionStatusWorkItem`
  - ctor with payload: `launcher.exe:0x435050`
  - deleting dtor/free-list return path: `launcher.exe:0x435c30`
  - vftable: `launcher.exe:0x4b3df8`
  - recovered 3-dword shape: `vfptr`, `workType=2`, `statusOrPayloadDword08`
  - current source now models this one as a real polymorphic class (`CLTThreadPerClientTCPEngine_ConnectionStatusWorkItem_0x4b3df8`) instead of a raw scaffold so the queued-release path also exercises compiler-emitted vfptr/vtable layout for the payload-bearing type-2 item
- `CloseWorkItem`
  - ctor: `launcher.exe:0x435070`
  - deleting dtor/free-list return path: `launcher.exe:0x435c80`
  - vftable: `launcher.exe:0x4b3e00`
  - recovered 3-dword shape: `vfptr`, `workType=1`, `statusOrPayloadDword08=0`
  - current source now models this one as a real polymorphic class (`CLTThreadPerClientTCPEngine_CloseWorkItem_0x4b3e00`) instead of a raw scaffold so the queued-release path exercises compiler-emitted vfptr/vtable layout under the MSVC-compatible toolchain

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
