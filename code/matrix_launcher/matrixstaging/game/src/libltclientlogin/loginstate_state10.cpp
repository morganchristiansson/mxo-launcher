#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>

namespace mxo::ltlogin {

static std::string LowercaseAsciiString(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

void CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorStateScaffold(CLTLoginMediator* mediator) {
    if (!mediator) {
        return;
    }

    // Address anchors:
    // - launcher.exe:0x4401a0 slot-6 success branch
    // - launcher.exe:0x43f300 slot-record initializer
    // Direct `0x4401a0` tightening now makes the original success-side writeback more concrete:
    // - allocate a new slot record into owner `+0x688[currentCount]`
    // - copy the selected world-descriptor inline name into owner `+0x818[currentCount]`
    // - write owner `+0xcc8 = currentCount`, then increment owner `+0x684`
    // - copy current `CharacterName` (`+0x108`) plus parsed reply ids into that new slot record
    // - then switch helper state to `11` and post event `0x14`
    // Current source still rebuilds the broader recovered auth-reply tables rather than mirroring
    // that one slot-record append byte-for-byte, but keep the success-side ownership on state10
    // slot6 instead of inventing a mediator shortcut.
    mediator->worldSlots_.fill(nullptr);
    mediator->worldPayloadSlots_.fill(nullptr);
    mediator->slotRecordValid688_.fill(false);
    mediator->worldDescriptorValidD84_.fill(false);
    mediator->slotRecordCount684_ = 0;
    mediator->worldDescriptorCountD80_ = 0;
    for (CLTLoginMediator::RouteHostStringTripleState& routeString : mediator->routeHostStrings818_) {
        routeString.text.clear();
    }

    const size_t worldCount = std::min(mediator->worldSlots_.size(), mediator->lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        mediator->worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->SeedRecoveredWorldDescriptorFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.worlds[i]);
    }
    mediator->worldDescriptorCountD80_ = static_cast<uint8_t>(worldCount);

    const size_t characterCount = std::min(mediator->slotRecords688_.size(), mediator->lastAuthReply_.characters.size());
    for (size_t i = 0; i < characterCount; ++i) {
        mediator->SeedRecoveredCharacterSlotRecordFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.characters[i]);
        const SlotRecordState004b5328& slotRecord = mediator->slotRecords688_[i];
        const int matchedWorldIndex = mediator->FindRecoveredWorldDescriptorIndexByWorldId(slotRecord.worldId0c);
        if (matchedWorldIndex >= 0) {
            // Current source-owned tightening for the active state-8 margin path:
            // preserve the original descriptor-name join, but lowercase the copied text so the
            // reconstructed `+0x818` family can feed DNS host-prefix use directly (`Reality`
            // -> `reality`).
            mediator->routeHostStrings818_[i].text = LowercaseAsciiString(
                mediator->worldDescriptorsD84_[static_cast<size_t>(matchedWorldIndex)].inlineNamePlus03);
        }
    }
    mediator->slotRecordCount684_ = static_cast<uint8_t>(characterCount);

    // Writeback to owner +0x80 (world list count/status family)
    mediator->postAuthMarginLoadingState_.worldListCountOrStatus80 =
        static_cast<uint32_t>(mediator->lastAuthReply_.worlds.size());

    // Writeback to owner +0xcc8 (current character/route index byte)
    mediator->postAuthMarginLoadingState_.characterRouteIndexCc8 = 0;
    mediator->marginRouteState_.currentCharacterOrRouteIndex = 0;

    if (characterCount != 0) {
        const SlotRecordState004b5328& currentSlotRecord = mediator->slotRecords688_[0];
        mediator->marginRouteState_.pendingWorldId = currentSlotRecord.worldId0c;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(currentSlotRecord.worldId0c);
    } else if (worldCount != 0) {
        const mxo::auth::AuthWorldEntry& firstWorld = mediator->lastAuthReply_.worlds[0];
        mediator->marginRouteState_.pendingWorldId = firstWorld.worldId;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(firstWorld.worldId);
    }

    if (const char* routeHostPrefix =
            mediator->LookupRouteHostPrefixBySlot(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)) {
        mediator->marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        mediator->marginRouteState_.routeHostPrefix.clear();
    }

    mediator->SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();

    const char* currentDescriptorName = "<empty>";
    if (characterCount != 0) {
        const int matchedWorldIndex =
            mediator->FindRecoveredWorldDescriptorIndexByWorldId(mediator->slotRecords688_[0].worldId0c);
        if (matchedWorldIndex >= 0) {
            if (const char* name = mediator->GetDescriptorInlineNameByIndex(static_cast<uint8_t>(matchedWorldIndex))) {
                currentDescriptorName = name;
            }
        }
    } else if (worldCount != 0) {
        if (const char* name = mediator->GetDescriptorInlineNameByIndex(0)) {
            currentDescriptorName = name;
        }
    }

    spdlog::info(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount={} characterCount={} currentCharacterOrRouteIndex={} currentSlotWorldId={} routeHostPrefix='{}' slotRecordHeapString='{}' currentWorldDescriptorName='{}'",
        static_cast<unsigned>(worldCount),
        static_cast<unsigned>(characterCount),
        static_cast<unsigned>(mediator->marginRouteState_.currentCharacterOrRouteIndex),
        characterCount == 0 ? 0u : static_cast<unsigned>(mediator->slotRecords688_[0].worldId0c),
        mediator->marginRouteState_.routeHostPrefix.empty() ? "<empty>" : mediator->marginRouteState_.routeHostPrefix.c_str(),
        mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)
            ? mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}

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
        AuthBootstrap680Ops::LogParsedAuthReply(*mediator, reply);
        return 1u;
    }

    AuthBootstrap680Ops::SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(*mediator, reply);
    mediator->ResetMarginBootstrapState();
    mediator->RecoverAuthReplyPrivateExponentIntoMarginBootstrapState(reply);
    AdoptAuthReplyIntoRecoveredMediatorStateScaffold(mediator);
    AuthBootstrap680Ops::LogParsedAuthReply(*mediator, reply);
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
        CLTLoginState* fallbackState = mediator->ScaffoldState4();
        const uint32_t fallbackResult = fallbackState
            ? mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                  4u,
                  fallbackState,
                  this,
                  "State10 slot3 owner+0x1c state!=2 -> helper4 margin-connect continuation")
            : 0u;
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; switched/dispatched helper4 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        CLTLoginState* fallbackState = mediator->ScaffoldState6();
        const uint32_t fallbackResult = fallbackState
            ? mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                  6u,
                  fallbackState,
                  this,
                  "State10 slot3 owner+0xf14==0 -> helper6 margin-bootstrap continuation")
            : 0u;
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; switched/dispatched helper6 result=0x{:08x}",
            static_cast<unsigned>(fallbackResult));
        return fallbackResult;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(mediator->SourceLeadString108().data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.Envelope());
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
    // - parsed success replies perform the owner-side slot-record/route-name writeback, switch
    //   helper state to `11`, and post event `0x14`
    // - immediate continuation after that success is now tighter too:
    //   `state11 slot3 / 0x43c020 -> state11 slot6 / 0x440320 -> helper9 slot3 / 0x439780
    //    -> owner 0x41de40`
    // - mediator now keeps only the staged auth bytes; the shared parse/adopt plus owner-state
    //   writeback helpers live here
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
