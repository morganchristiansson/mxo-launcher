# CLTThreadPerClientTCPEngine_0x4b2768

Current high-confidence launcher.exe findings used by `matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.{h,cpp}`.

## Object layout

- derived ctor: `launcher.exe:0x431c30`
- base ctor: `launcher.exe:0x4366f0`
- primary vtable: `launcher.exe:0x4b2768`
- recovered base datatype name in Ghidra: `CLTBaseThreadPerClientTCPEngine_0x4b3e74`
- recovered lock helper datatype name in Ghidra: `CLTCriticalSectionHelper_0x4add70`

Recovered field roles:

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

- The launcher arg5 shell still needs queue/event/lock/tree state mirrored into the bound engine sidecar.
- Current source models that bridge as a single active attachment, not as a pretend multi-engine attachment map.
- This matches current launcher startup reality: one arg5 shell binds to one `CLTThreadPerClientTCPEngine_0x4b2768` sidecar through `CLTThreadPerClientTCPEngine_0x4b2768Binding`.

## Worker creation / queueing

- `0x431ff0` allocates a worker thread, stores it at `[connection+0x08]`, inserts it into the context tree under the `+0x98` cleanup lock, and optionally starts it.
- `0x4364d0` is a distinct dequeue helper used by the zero-thread stop/drain path.
- `0x436820` is the enqueue boundary.
- `0x436b10` is the main completed-operation queue consumer body.

## Thread base

- `0x4528d0` = `CLTThread::Start`
- `0x452800` = `_beginthreadex` start thunk / thread main sequencing
- `0x452660` = `CLTThread::Stop`

Important fidelity note:

- `0x452660` calls `ExitThread(0)` on self-stop in launcher.exe before taking the terminate path.
- Current source intentionally does not hard-exit on self-stop yet, to keep launcher behavior stable while the surrounding lifecycle remains partially scaffolded.
