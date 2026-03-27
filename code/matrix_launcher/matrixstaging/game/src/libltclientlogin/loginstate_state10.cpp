#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// UNANCHORED: source-owned shared raw-0x0b parse/adopt helper used by state10 slot 6 and the
// current existing-character state8 auth bridge.
uint32_t CLTLoginState_State10::HandleStagedAuthReplyScaffold(CLTLoginMediator* mediator) {
    if (!mediator || mediator->stagedIncomingAuthPacketBytes_.empty()) {
        return 0u;
    }

    mxo::auth::AuthReply reply;
    if (!mxo::auth::ParseAuthReplyPayload(
            mediator->stagedIncomingAuthPacketBytes_.data(),
            mediator->stagedIncomingAuthPacketBytes_.size(),
            &reply)) {
        spdlog::warn("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthReply");
        return 0u;
    }

    mediator->lastAuthReply_ = reply;
    mediator->expectedAuthRequestName_ = nullptr;

    if (reply.isErrorReply) {
        mediator->postAuthMarginLoadingState_.worldListCountOrStatus80 = reply.errorCode;
        mediator->expectedMarginRequestName_ = nullptr;
        mediator->LogParsedAuthReply(reply);
        return 1u;
    }

    mediator->SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(reply);
    mediator->ResetMarginBootstrapState();
    mediator->RecoverAuthReplyPrivateExponentIntoMarginBootstrapState(reply);
    mediator->AdoptAuthReplyIntoRecoveredMediatorState();
    mediator->LogParsedAuthReply(reply);
    mediator->expectedMarginRequestName_ = "CERT_ConnectRequest";
    return 1u;
}

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(
    void* upstreamOrArg,
    CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Fresh `0x43bf90` read from decompilation + disassembly:
    // - sends raw margin opcode `0x0a`
    //   - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //     `MS_ClaimCharacterNameRequest`
    // - precheck owner `+0x1c` connection state through `0x41b4b0`
    //   - on failure, original switches helper state to `4`
    // - then check owner byte `+0xf14`
    //   - on zero, original switches helper state to `6`
    // - initialize local packet-builder family `0x43a1f0`
    // - copy owner `+0x108` (`CharacterName`) through `0x43aa80`
    // - send through `0x41af70`
    // - post event `0x13`
    if (!mediator->State10HasReadyConnectionState2()) {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6");
        return 0u;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(mediator->SourceLeadString108().data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x13u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x{:02x} totalBytes=0x{:02x} CharacterName='{}' -> sendResult=0x{:08x} then posts event=0x13",
        State10Packet0x0aFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        std::string(reinterpret_cast<const char*>(mediator->SourceLeadString108().data())),
        sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(
    void* workItem,
    CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->stagedIncomingAuthPacketBytes_;
    const uint8_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];

    // Ownership correction from the vtable docs + direct `0x4401a0` review:
    // - `0x4401a0` belongs to `CLTLoginState_State10` slot 6, not to the mediator vtable
    // - non-`0x0b` packets are rejected here with owner `+0x80 = 0x12000005`
    // - parsed error replies switch helper state to `3` and post error `0x0b`
    // - parsed success replies switch helper state to `11` and post event `0x14`
    // - mediator now keeps only the staged auth bytes; the shared parse/adopt helper lives here
    if (rawCode != 0x0bu) {
        mediator->WorldListCountOrStatus80() = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State10::Slot6_HandleSecondaryMessage rejected staged auth bytes={} rawCode=0x{:02x}; mirrored original owner+0x80=0x12000005 and returned false-like",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    const uint32_t handled = HandleStagedAuthReplyScaffold(mediator);
    if (handled == 0u) {
        return 0u;
    }

    if (mediator->lastAuthReply_.valid && mediator->lastAuthReply_.isErrorReply) {
        if (CLTLoginState* failureState = mediator->ScaffoldState3()) {
            mediator->SwitchHelperStateScaffold(3u, failureState);
        }
        mediator->PostErrorScaffold(0x0bu);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage observed error AS_AuthReply; mirrored original state3 switch and error=0x0b owner+0x80=0x{:08x}",
            static_cast<unsigned>(mediator->WorldListCountOrStatus80()));
        return 1u;
    }

    if (CLTLoginState* nextState = mediator->ScaffoldState11()) {
        mediator->SwitchHelperStateScaffold(0x0bu, nextState);
    } else {
        spdlog::warn(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage parsed successful AS_AuthReply but has no registered helper11 state");
    }
    mediator->PostEventScaffold(0x14u);
    return 1u;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

}  // namespace mxo::ltlogin
