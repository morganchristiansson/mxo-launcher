#include "loginstate.h"

#include "loginmediator.h"
#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// Focused late-login/state9 split:
// - keep the state9 slot3/slot6 body in its own TU so the next join-session deep dive can stay
//   scoped to `0x439780 -> 0x41de40 -> 0x43c180`
// - canonical continuation reference:
//   `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`

static uint32_t ReadU32LEState9(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// anchor: launcher.exe:0x43afd0 / OOAnalyzer::cls_0x4b5440 packet-view helper
struct Packet_MsEstablishUDPSessionReply_0x4b5440 : public mxo::liblttcp::Packet_0x4af2a4 {
    explicit Packet_MsEstablishUDPSessionReply_0x4b5440(
        mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
        bool initializeOpcodeByte) {
        if (messageRef08) {
            messageRef08->Release();
        }
        payloadPtr04 = 0;
        payloadAlias10 = nullptr;
        debugString14 = nullptr;
        payloadSize18 = 0;
        packetType1a = 0;

        messageRef08 = incomingMessageRef;
        if (!messageRef08) {
            return;
        }

        messageRef08->AddRef();
        if (!messageRef08->messageStorage0c) {
            return;
        }

        const uint8_t* payloadBase = messageRef08->messageStorage0c->PayloadBase();
        if (!payloadBase) {
            return;
        }

        if (messageRef08->headerless10 == 0) {
            payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(const_cast<uint8_t*>(payloadBase + 0x0cu)));
            payloadAlias10 = const_cast<uint8_t*>(payloadBase + 0x0cu);
        } else {
            const uint8_t descriptor = payloadBase[0x0du];
            auto* payloadView = const_cast<uint8_t*>(payloadBase +
                                                    g_MessageOffsetLookupTable[(descriptor >> 4) & 7u] +
                                                    g_MessageOffsetLookupTable[descriptor & 7u] + 0x1eu);
            payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payloadView));
            payloadAlias10 = payloadView;
            messageRef08->headerless10 = 1;
        }

        if (initializeOpcodeByte && payloadAlias10) {
            auto* mutablePayloadView = static_cast<uint8_t*>(payloadAlias10);
            mutablePayloadView[0] = 0x11u;
            *reinterpret_cast<uint32_t*>(mutablePayloadView + 1u) = 0u;
        }
    }

    uint32_t ParsedStatus() const {
        return payloadAlias10 ? ReadU32LEState9(static_cast<const uint8_t*>(payloadAlias10) + 1u) : 0u;
    }
};

}  // namespace

// anchor: launcher.exe vtable 0x4b517c
const char* CLTLoginState_State9_0x4b517c::DebugName() const {
    return "CLTLoginState_State9_0x4b517c";
}

// anchor: launcher.exe:0x439780 (vtable 0x4b517c slot 3)
void CLTLoginState_State9_0x4b517c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    if (!g_CurrentLoginMediator) {
        return;
    }

    // Current best read from `0x439780` + `0x41de40`:
    // - this state consumes helper-local byte/word payload at `this+4/+6`
    // - producer tails in state8/state11 also preserve a live helper byte at `this+5`
    //   (`pendingReplySectionCount5_`), even though `0x439780` itself does not pass it onward
    // - forwards `this+4/+6` into the owner helper
    // - clears the local payload regardless of branch
    // - posts event `0x17` when that helper returns `< 1`
    // - newer original-launcher WineDbg now proves the natural path continues not just into this
    //   slot, but immediately onward into `0x41de40`
    // - representative natural stop shape:
    //   - `EIP = 0x439780`
    //   - `ECX = this`
    //   - `EAX = helper-local side pointer / temp = 0xf9bb10`
    //   - `EDX = 0x4b517c` (this vtable)
    //   - `this+4 = 0`
    //   - `this+6 = 0x2710`
    const uint8_t consumedByte4 = pendingByte4_;
    const uint8_t preservedReplySectionCount5 = pendingReplySectionCount5_;
    const uint16_t consumedWord6 = pendingWord6_;
    const uint32_t submitResult = g_CurrentLoginMediator->State9SubmitFollowup(consumedByte4, consumedWord6);
    pendingByte4_ = 0;
    pendingReplySectionCount5_ = 0;
    pendingWord6_ = 0;

    if (submitResult < 1u) {
        // anchor: launcher.exe:0x439780 success-side event post after the `0x41de40` submit call.
        g_CurrentLoginMediator->PostEvent(0x17u);
        spdlog::info(
            "CLTLoginState_State9::Slot3_BeginOrContinue consumed helper-local payload byte4=0x{:02x} preservedByte5=0x{:02x} word6=0x{:04x} -> submitResult=0x{:08x} then posts event=0x17",
            static_cast<unsigned>(consumedByte4),
            static_cast<unsigned>(preservedReplySectionCount5),
            static_cast<unsigned>(consumedWord6),
            static_cast<unsigned>(submitResult));
    } else {
        spdlog::info(
            "CLTLoginState_State9::Slot3_BeginOrContinue consumed helper-local payload byte4=0x{:02x} preservedByte5=0x{:02x} word6=0x{:04x} -> submitResult=0x{:08x}",
            static_cast<unsigned>(consumedByte4),
            static_cast<unsigned>(preservedReplySectionCount5),
            static_cast<unsigned>(consumedWord6),
            static_cast<unsigned>(submitResult));
    }
    return;
}

// anchor: launcher.exe:0x43c180 (vtable 0x4b517c slot 6)
uint32_t CLTLoginState_State9_0x4b517c::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    // Current live-status note:
    // - newer natural-original WineDbg now proves this slot-6 body is reached on the natural path
    // - representative natural stop hit the success-side branch at `0x43c1c2`
    // - observed state there matched the raw-`0x11` success interpretation:
    //   - owner `+0x80 = 0`
    //   - parsed status dword = 0
    // - representative natural run at this same boundary was visibly in the
    //   "Waiting for Regionserver" phase
    // - so the old "does natural original ever reach `0x43c180`?" question is now closed
    // - newer Ghidra-first tightening also matters for what comes next:
    //   success here is not just "set current state to 12 and immediately fall into slot 6"
    //   it first runs `0x41b420`, then `0x41b450(0x0c)`, then `0x41cfb0(0x18)`
    // - newer breakpoint-only live proof now tightens that one step further too:
    //   the same natural run later re-hit `0x41cfb0` with event `0x0f` before entering game
    // - `0x41cfb0` walks the owner `+0x674` listener tree, so observer/UI consumers are now part
    //   of the next faithful-original question too
    // - next natural-original question therefore moves later again, into the post-state9 /
    //   state-`0x0c` continuation after this slot posts event `0x18`
    if (!g_CurrentLoginMediator || workItem == nullptr) {
        return 0u;
    }

    const uint16_t messageCode = mxo::liblttcp::CMessageConnectionMessageRef_DecodeMessageCode(workItem);
    uint32_t returnValue = 1u;

    if (messageCode == 0x11u) {
        Packet_MsEstablishUDPSessionReply_0x4b5440 replyView(workItem, /*initializeOpcodeByte=*/false);
        const uint32_t parsedStatus = replyView.ParsedStatus();
        g_CurrentLoginMediator->worldListCountOrStatus80 = parsedStatus;
        if (parsedStatus < 1u) {
            g_CurrentLoginMediator->HandleState9Opcode11SuccessSideEffect();
            (void)g_CurrentLoginMediator->SetCurrentState(0x0cu);
            spdlog::info(
                "ROUTE CHECKPOINT: late-login state9 success -> state12 event=0x18 currentState={}",
                g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
            // anchor: launcher.exe:0x43c180 success tail posts event 0x18 after switching to state 0x0c.
            g_CurrentLoginMediator->PostEvent(0x18u);
            spdlog::info(
                "CLTLoginState_State9::Slot6_HandleSecondaryMessage decoded opcode-0x11 success status=0x{:08x}; original calls owner vtable +0x16c, switches helper state to 0x0c, then posts event=0x18 currentState={}",
                static_cast<unsigned>(parsedStatus),
                g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
            returnValue = 1u;
        } else {
            (void)g_CurrentLoginMediator->SetCurrentState(3u);
            // anchor: launcher.exe:0x43c180 failure tail posts error 0x0d after switching back to state 3.
            g_CurrentLoginMediator->PostError(0x0du);
            spdlog::info(
                "CLTLoginState_State9::Slot6_HandleSecondaryMessage decoded opcode-0x11 failure status=0x{:08x}; original switches helper state to 3 and posts error=0x0d currentState={}",
                static_cast<unsigned>(parsedStatus),
                g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<unchanged>");
            returnValue = 1u;
        }

        if (replyView.messageRef08 != nullptr) {
            replyView.messageRef08->Release();
            replyView.messageRef08 = nullptr;
        }
    } else {
        const uint32_t fallbackResult = g_CurrentLoginMediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (fallbackResult < 1u) {
            spdlog::info(
                "CLTLoginState_State9::Slot6_HandleSecondaryMessage delegated non-0x11 fallback through owner callback84 -> dispatchResult=0x{:08x}",
                static_cast<unsigned>(fallbackResult));
            return 1u;
        }
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State9::Slot6_HandleSecondaryMessage non-0x11 fallback through owner callback84 returned 0x{:08x}; mirrored owner+0x80=0x12000005",
            static_cast<unsigned>(fallbackResult));
        return reinterpret_cast<uintptr_t>(g_CurrentLoginMediator) & 0xffffff00u;
    }

    return returnValue;
}

// anchor: launcher.exe:0x438cc0 (vtable 0x4b517c slot 7)
uint32_t CLTLoginState_State9_0x4b517c::GetStateId() const {
    return 9;
}

}  // namespace mxo::ltlogin
