#pragma once

#include <cstdint>

#include "ltthreadperclienttcpengine.h"

namespace mxo::liblttcp {

// Reimplementation note:
// This file mirrors the best current read of the queue0C context object family.
// Canonical RE references remain:
// - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
// - docs/client.dll/RunClientDLL/README.md

// Starter skeleton for the original CMessageConnection-family object now visible in
// launcher.exe strings / vtable neighborhoods.
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
// - vtable +0x10 / 0x4490c0 -> likely OnOperationCompleted(workItem)
//   - processes work item types via [workItem+0x04]
//   - string-backed receive/completion/error handling lives on this path
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
