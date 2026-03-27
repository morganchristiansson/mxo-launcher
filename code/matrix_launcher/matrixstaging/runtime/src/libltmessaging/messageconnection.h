#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../liblttcp/ltthreadperclienttcpengine.h"

#ifdef DispatchMessage
#undef DispatchMessage
#endif

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
//       target by itself; it is helper `0x4f7890` (`CLTLoginState_State10`, vtable `0x4b512c`) slot `+0x14`
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
// - inherited send path now tightens better as:
//   - vtable slot 10 / `0x448cf0` = `CMessageConnection::SendPacket`
//   - consumes a message/envelope object, performs packet-agenda filtering, then reaches the
//     lower submit helper `0x448a00`
//   - that lower helper forwards final byte pointer/size together with `self` to engine `+0x20`
//   - current best engine mapping there is slot-8 / SendBuffer
// - vtable +0x1c / 0x449cd0 -> likely endpoint-update / ensure-connected wrapper
//   - updates stored endpoint at +0x24 and then calls engine +0x18 with `self`
//   - current best engine mapping there is slot-6 / Connect
// - vtable +0x0c / 0x449ca0 -> likely close/abort wrapper
//   - calls engine +0x1c with `self`
//   - current best engine mapping there is slot-7 / Close
//
// ============================================================
// VTable 0x004afef0 - CMessageConnection (Intermediate Base)
// ============================================================
// 0x004afef0 - FUN_0041cf50
// 0x004afefc - CLTTCPConnection::CLTTCPConnection at 0x00449ca0
// 0x004aff00 - FUN_00449a70
// 0x004aff04 - CLTTCPConnection::OnReceive at 0x00449d40
// 0x004aff08 - CLTTCPConnection::OnClose at 0x00449fd0
// 0x004aff0c - CLTTCPConnection::Close at 0x00449cd0
// 0x004aff10 - CLTTCPConnection::~CLTTCPConnection at 0x00449d20
// 0x004aff14 - FUN_0041cf30
// 0x004aff18 - SendPacket at 0x00448cf0
// 0x004aff1c - FUN_00449a30
// 0x004aff34 - FUN_0041ce80

// ============================================================
// VTable 0x004aff38 - CMarginConnection (Type D)
// ============================================================
// Inheritance Chain:
// CLTTCPConnection (0x004b8034) [Base]
// └── CMessageConnection (0x004afef0) [Intermediate Base]
//     └── Type A (0x004b64a8) [Base for Type D]
//         └── CMarginConnection (Type D) [0x004aff38] - Leaf
// ============================================================
// 0x004aff38 - FUN_0041cf80 (Type D initialization)
// 0x004aff48 - FUN_0044af60 (Advanced message routing)
// 0x004aff64 - FUN_0044af20 (Message dispatcher)
// Inherits: CLTTCPConnection methods + CMessageConnection::SendPacket

// Important current limitation for this starter skeleton:
// - the recovered original engine entry on this path is more connection-object-oriented
//   than the placeholder engine signatures currently model
// - keep the names stable, but treat the exact live method signatures as still provisional

struct CMessageConnectionMessageScaffold {
    // Source-owned bridge for the original shared message object consumed by
    // `0x448cf0 -> 0x448a00`.
    // Current best narrowed fields that materially affect the final byte submit:
    // - original inner object `+0x08` participates in payload-cap checks (`0xffc`)
    // - original inner object `+0x0a/+0x0b` hold the framed payload length/header bytes
    // - original payload bytes begin at `+0x0c`
    // - builder helpers such as `0x43acf0` grow the payload through shared-object vtable `+0x18`
    //   (`0x4557b0`) before later copy helpers write bytes into the newly reserved tail
    // - crucial newer live-original correction:
    //   an earlier `0x448a00` capture with submitted bytes beginning `01 03 00 36 ...` and
    //   length `0x13b` was later proven to return to `0x441f9f`, i.e. a different send family,
    //   not the state8 `0x43bf64` call path
    // - newer targeted state8 stops at `0x41af70/0x41cf30` with return `0x43bf64` instead show the
    //   natural state8 shared object still carrying raw `0x0f` payload bytes and a length of
    //   `0x0be`, so the remaining active state8 gap is much narrower than that earlier mixed-send
    //   read suggested
    static constexpr uint16_t kMaxPayloadByteCount = 0x0ffcu;
    static constexpr uint16_t kBuilderReservedBytes08 = 0x002bu;  // anchor: launcher.exe:0x455bd0

    uint16_t reservedBytes08 = kBuilderReservedBytes08;
    std::vector<uint8_t> payloadBytesFrom0c;

    void ResetForPacketBuilderScaffold();
    void ResetPayloadByteCountScaffold(uint16_t payloadByteCount);
    uint16_t GrowPayloadByteCountScaffold(uint16_t additionalByteCount);
    uint16_t PayloadByteCountScaffold() const;
    uint16_t RemainingAppendableByteCountScaffold() const;
    uint8_t* PayloadBaseScaffold();
    const uint8_t* PayloadBaseScaffold() const;
    std::vector<uint8_t> BuildFramedBytesFrom0aScaffold() const;
};

enum class CMessageConnectionPacketNameFamilyScaffold : uint8_t {
    kUnknown = 0,
    kAuth = 1,
    kMargin = 2,
};

struct CMessageConnectionPacketAgendaScaffold {
    // Source-owned mirror of the lazy packet-processing agenda object rooted at original
    // connection `+0x74`.
    // Current best narrowed shape from `0x448980 -> 0x469b10 -> 0x469850 -> 0x469740`:
    // - object is lazy-created, not constructor-owned
    // - object has distinct read/write helper sides
    // - send path `0x448cf0` consults the write side and may replace/discard the envelope
    bool created = false;
    uint32_t configuredReadHelperCount = 0;
    uint32_t configuredWriteHelperCount = 0;
};

struct CMessageConnectionEnvelopeScaffold {
    // Source-owned bridge for the original local envelope family forwarded by `0x41af70`
    // through `0x41cf30` into `0x448cf0`.
    // Current best narrowed material fields:
    // - original envelope `+0x08` = shared message object
    // - original envelope `+0x10` = headerless/send-mode flag consumed by `0x448cf0`
    std::shared_ptr<CMessageConnectionMessageScaffold> sharedMessage;
    uint8_t headerless10 = 0;
};

class CMessageConnection : public CLTTCPConnection {
public:
    // anchor: launcher.exe:0x41cf50
    CMessageConnection();
    // UNANCHORED: source-owned convenience ctor that seeds the recovered base engine field.
    explicit CMessageConnection(CLTThreadPerClientTCPEngine* engine);
    // anchor: launcher.exe:0x449d20 deleting-wrapper family via CLTTCPConnection base
    ~CMessageConnection();

    // UNANCHORED: source-owned compatibility wrapper over the recovered base-connection `+0x10` engine field.
    void SetEngine(CLTThreadPerClientTCPEngine* engine);
    // UNANCHORED: source-owned compatibility accessor over the recovered base-connection `+0x10` engine field.
    CLTThreadPerClientTCPEngine* Engine() const;

    // UNANCHORED: source-owned wrapper over base `CLTTCPConnection::Connect` / engine slot 6.
    // This keeps the connection-object-oriented call site out of diagnostics.cpp.
    uint32_t EnsureConnected();

    // anchor: launcher.exe:0x448cf0
    // string-backed original name: CMessageConnection::SendPacket
    // current best read:
    // - original `0x448cf0` consumes a message/envelope object, not bare payload bytes
    // - it runs packet-agenda filtering first, then reaches a lower submit helper that forwards
    //   final byte pointer/size through engine slot 8 / SendBuffer using `this` as the
    //   connection object
    // - the raw-byte overload remains as the older narrow bridge used by auth-side scaffold sends
    uint32_t SendPacket(const void* packetData, uint32_t packetByteCount, void* completionContext = nullptr);

    // Source-owned launcher-only bridge for the narrowed state8/state10/state11 send-authenticity
    // gap. It preserves the now-recovered envelope/message split from
    // `0x41af70 -> 0x41cf30 -> 0x448cf0 -> 0x448a00` without pretending the full original shared
    // message-object shape is already recovered.
    // Important current narrowing from original-launcher WineDbg at the natural first state8 send:
    // - margin connection `+0x70` was live (`0x41ce40`)
    // - connection `+0x74` was still null on that send
    // - submitted payload length there was `0x13b`, not the current replacement `0x0bb`
    // So the remaining blocker is no longer best framed as agenda presence alone; the stronger
    // current gap is the richer original message-object content that our scaffold still does not
    // build.
    // UNANCHORED: source-owned launcher-only bridge for the local envelope builder seam.
    static CMessageConnectionEnvelopeScaffold BuildPacketBuilderEnvelopeScaffold(bool headerless = false);
    // UNANCHORED: source-owned launcher-only bridge that wraps raw payload bytes in the local envelope scaffold.
    static CMessageConnectionEnvelopeScaffold BuildPayloadEnvelopeScaffold(
        const void* packetData,
        uint32_t packetByteCount,
        bool headerless = false);
    // UNANCHORED: source-owned launcher-only bridge for the current envelope submit seam.
    uint32_t SendPacketEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope);

    // anchor: launcher.exe:0x448960
    // Narrow source-owned mirror of the per-connection packet-name callback configuration:
    // - original writes connection `+0x78 = enabled`
    // - when enabled, original writes connection `+0x70 = callback`
    void ConfigurePacketNameFamilyScaffold(
        CMessageConnectionPacketNameFamilyScaffold family,
        bool packetizedMessagesEnabled);
    // UNANCHORED: source-owned accessor for the packet-name family scaffold.
    CMessageConnectionPacketNameFamilyScaffold PacketNameFamilyScaffold() const;
    // UNANCHORED: source-owned accessor for the packetized-messages enable scaffold byte.
    bool PacketizedMessagesEnabledScaffold() const;

    // anchor: launcher.exe:0x448980
    // Narrow source-owned mirror of the lazy packet-agenda object at connection `+0x74`.
    // Current source model keeps only the creation/ownership fact and helper counts explicit.
    void EnsurePacketAgendaScaffold();
    // UNANCHORED: source-owned accessor for the lazy packet-agenda scaffold pointer.
    const CMessageConnectionPacketAgendaScaffold* PacketAgendaScaffold() const;

    // anchor: launcher.exe:0x449a70
    uint32_t ProcessPacketResult(const void* packetData, uint32_t byteCount);

    // anchor: launcher.exe:0x448a60
    // string-backed original name: CMessageConnection::OnOperationCompleted
    // current best read:
    // - completion/receive-side bridge back into engine/queue handling
    uint32_t OnOperationCompleted(void* workCode);

    // UNANCHORED: source-owned helper mirroring the current queue producer context-key shape.
    // Current best reading: queue0C often receives (workItem, this, 0) from this class.
    void* ContextKey() { return this; }

    // anchor: launcher.exe:0x449a30
    uint32_t ProcessDispatchResult(const void* packetData, uint32_t byteCount);

private:
    // UNANCHORED: source-owned packet-family name helper for current diagnostics.
    static const char* PacketNameFamilyToString(CMessageConnectionPacketNameFamilyScaffold family);
    // UNANCHORED: source-owned packet-agenda pass/filter scaffold helper.
    bool PacketAgendaAllowsEnvelopeScaffold(const CMessageConnectionEnvelopeScaffold& envelope) const;
    // UNANCHORED: source-owned lower submit helper beneath SendPacketEnvelopeScaffold.
    uint32_t SubmitEnvelopeBytesScaffold(const CMessageConnectionEnvelopeScaffold& envelope);

    CMessageConnectionPacketNameFamilyScaffold packetNameFamilyScaffold_ =
        CMessageConnectionPacketNameFamilyScaffold::kUnknown;
    bool packetizedMessagesEnabledScaffold_ = false;
    std::unique_ptr<CMessageConnectionPacketAgendaScaffold> packetAgendaScaffold_;
};

// ============================================================
// CMarginConnection class declaration
// ============================================================
class CMarginConnection : public CMessageConnection {
public:
    // ============================================================
    // FAITHFUL: VTable 0x004aff38 - Constructor at 0x0041cf80
    // Type D initialization - sets vtable pointer, calls FUN_00441820 cleanup
    // ============================================================
    CMarginConnection();

    // ============================================================
    // UNANCHORED: Not based on vtable analysis
    // Constructor with margin engine parameter
    // ============================================================
    explicit CMarginConnection(CLTThreadPerClientTCPEngine* marginEngine);

    // ============================================================
    // FAITHFUL: VTable 0x004aff38 - Destructor at 0x00449d20
    // Inherits from CLTTCPConnection (via CMessageConnection)
    // Note: Cleanup calls FUN_00441820 which sets vtable to Type A's vtable
    // ============================================================
    ~CMarginConnection();

    // ============================================================
    // UNANCHORED: Not based on vtable analysis
    // Placeholder entry point for margin engine management
    // ============================================================
    void SetMarginEngine(CLTThreadPerClientTCPEngine* marginEngine);

    // ============================================================
    // UNANCHORED: Not based on vtable analysis
    // Getter for margin engine pointer
    // ============================================================
    CLTThreadPerClientTCPEngine* MarginEngine() const;

    // ============================================================
    // FAITHFUL: VTable 0x004aff48 - FUN_0044af60 at 0x0044af60
    // 42 instructions, 7 complexity, 5 calls
    // Advanced message routing with fallback handlers for robustness
    // ============================================================
    uint32_t RouteMessage(const void* packetData, uint32_t byteCount);

    // ============================================================
    // FAITHFUL: VTable 0x004aff64 - FUN_0044af20 at 0x0044af20
    // 23 instructions, 2 complexity, 2 calls
    // Message dispatcher - calls dispatch router FUN_00442d00
    // ============================================================
    uint32_t DispatchMessage(const void* packetData, uint32_t byteCount);

    // ============================================================
    // UNANCHORED: Not based on vtable analysis
    // CERT protocol handler placeholder
    // ============================================================
    uint32_t HandleCERTMessage(const void* packetData, uint32_t byteCount);

    // ============================================================
    // UNANCHORED: Not based on vtable analysis
    // MS protocol handler placeholder
    // ============================================================
    uint32_t HandleMSMessage(const void* packetData, uint32_t byteCount);

private:
    CLTThreadPerClientTCPEngine* marginEngine_;
};

}  // namespace mxo::liblttcp
