#pragma once

#include <cstdint>

#include "../liblttcp/ltthreadperclienttcpengine.h"

namespace mxo::liblttcp {

// Reimplementation note:
// This file mirrors the best current read of the queue0C context object family.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md

// Starter skeleton for the original CMessageConnection-family object now visible in
// launcher.exe strings / vtable neighborhoods.
//
// Recovered source-file anchor:
// - `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`
//
// Current evidence-backed role:
// - `0x448b40` first constructs a base CLTTCPConnection-family object, then overwrites
//   its vtable to the CMessageConnection-family vtable at `0x4b7928`
// - captures a CLTThreadPerClientTCPEngine pointer at +0x10
// - carries an endpoint copy at +0x24
// - participates in queue0C producer traffic by enqueueing (workItem, this, 0)
// - likely appears as the dequeued queue0C `context` on important consumer paths
// - string-backed methods include:
//   - CMessageConnection::SendPacket
//   - CMessageConnection::OnOperationCompleted
//
// Current best virtual mapping from launcher.exe:
// - base vtable +0x10 / 0x4490c0 -> likely OnOperationCompleted(workItem)
//   - processes work item types via [workItem+0x04]
//   - string-backed receive/completion/error handling lives on this path
// - newer startup-side narrowing now also shows important **derived** auth/margin families
//   on top of this base object:
//   - auth-side derived connection vtable `0x4afef0`
//   - margin-side derived connection vtable `0x4aff38`
//   - those families wrap base completion through `0x449a70` / `0x44af60`
//   - auth-side `0x449a70` is now narrowed one step further:
//     - after base `0x4490c0` returns 0, it calls owner `[self+0xa4]->+0x17c`
//     - that owner surface is now resolved as thunk `0x41f260`, which forwards to the
//       owner's current helper/state object at `owner+0x10`, then jumps to helper vtable `+0x14`
//     - so the concrete handling target depends on the current helper selected through the
//       `0x4f7868` family and `0x41b450(...)`, not on one fixed owner-body function alone
//     - important correction: later body `0x4401a0` is **not** the generic owner `+0x17c`
//       target by itself; it is helper `0x4f7890` (vtable `0x4b512c`) slot `+0x14`
//     - that helper body only meaningfully handles later raw auth code `0x0b`
//       (`AS_AuthReply`), then updates owner state and reaches
//       `0x41b450` + `CLTLoginMediator::PostEvent()` / `PostError()`
//     - if the current helper `+0x14` target returns 0, `0x449a70` falls through to
//       `0x448a60`, which is string-backed only as a generic
//       `Got unhandled op of type %d with status %s` logger
//   - important nuance: that auth-side owner/helper/fallback chain is therefore a later
//     incoming packet/owner-handling anchor, not direct proof of the first outbound request
//     after connect
// - connection fields `+0x7c` / `+0x80` are no longer just anonymous mystery pointers:
//   - ctor helper `0x436080` builds a `0x24` event+critical-section helper object there
//   - helper shape now reads as:
//     - main helper methods: `SetEvent()` and `WaitForSingleObject(timeout)`
//     - embedded lock helper rooted at `+0x04` (shared `0x4add70` family)
//     - event handle at `+0x20`
//   - `0x4490c0` uses them as completion notifiers:
//     - work type `2` -> signal `self+0x7c`
//     - work type `1` -> signal `self+0x80`
//   - current best subtype read:
//     - `+0x7c` = type-2 status/connect-style completion helper
//     - `+0x80` = type-1 close/terminal completion helper, supported by `0x448af0`
//       waiting on `+0x80` until connection state `+0x34` returns to `8`
//   - crucial current narrowing: startup auth/margin derived objects built through
//     `0x41d170 / 0x41e500 -> 0x4417e0 -> 0x448b40(flag=0)` leave both `+0x7c` and `+0x80`
//     as null on that path, so current auth/margin type-2 connect-status handling falls
//     through to owner callbacks instead of using these helper objects
// - vtable +0x20 / 0x449d20 -> likely SendPacket(...)
//   - forwards send args together with `self` to engine +0x20
//   - current best engine mapping there is slot-8 / SendBuffer
// - vtable +0x1c / 0x449cd0 -> likely endpoint-update / ensure-connected wrapper
//   - updates stored endpoint at +0x24 and then calls engine +0x18 with `self`
//   - current best engine mapping there is slot-6 / Connect
// - vtable +0x0c / 0x449ca0 -> likely close/abort wrapper
//   - calls engine +0x1c with `self`
//   - current best engine mapping there is slot-7 / Close
//
// Important current limitation for this starter skeleton:
// - the recovered original engine entry on this path is more connection-object-oriented
//   than the placeholder engine signatures currently model
// - keep the names stable, but treat the exact live method signatures as still provisional
class CMessageConnection : public CLTTCPConnection {
public:
    CMessageConnection();
    explicit CMessageConnection(CLTThreadPerClientTCPEngine* engine);
    ~CMessageConnection();

    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    CLTThreadPerClientTCPEngine* Engine() const;

    // Current recovered wrapper shape around engine slot 6 / Connect.
    // This keeps the connection-object-oriented call site out of diagnostics.cpp.
    uint32_t EnsureConnected();

    // Current recovered wrapper shape around engine slot 7 / Close.
    uint32_t CloseConnection(bool graceful);

    // String-backed original names kept as placeholders.
    // These are intentionally thin until the surrounding worker/completion model is recovered.
    // string-backed original name: CMessageConnection::SendPacket
    // current best read:
    // - producer-side bridge into engine/queue work
    // - currently forwards through engine slot 8 / SendBuffer using `this` as the
    //   connection object on the starter path
    uint32_t SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext = nullptr);

    // string-backed original name: CMessageConnection::OnOperationCompleted
    // current best read:
    // - completion/receive-side bridge back into engine/queue handling
    uint32_t OnOperationCompleted(uint32_t workCode);

    // Placeholder helper to mirror the currently recovered queue producer shape.
    // Current best reading: queue0C often receives (workItem, this, 0) from this class.
    void* ContextKey() { return this; }

private:
    CLTThreadPerClientTCPEngine* engine_;
};

}  // namespace mxo::liblttcp
