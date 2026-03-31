#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <spdlog/spdlog.h>

#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "loginstate_packet_builder_scaffold.h"

namespace mxo::ltlogin {
namespace {

static void CopyCStringIntoFixed(char* dest, size_t destSize, const uint8_t* src, size_t srcAvailable) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::fill(dest, dest + destSize, '\0');
    if (!src || srcAvailable == 0u) {
        return;
    }

    size_t copyCount = 0u;
    while (copyCount + 1u < destSize && copyCount < srcAvailable && src[copyCount] != '\0') {
        dest[copyCount] = static_cast<char>(src[copyCount]);
        ++copyCount;
    }
    dest[copyCount] = '\0';
}

static void AppendOwnedSectionBytesU16(void*& buffer, uint16_t& length, const uint8_t* src, uint16_t appendLen) {
    if (!src || appendLen == 0u) {
        return;
    }

    const size_t oldLength = length;
    const size_t newLength = oldLength + appendLen;
    void* newBuffer = buffer ? std::realloc(buffer, newLength) : std::malloc(newLength);
    if (!newBuffer) {
        return;
    }

    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, src, appendLen);
    buffer = newBuffer;
    length = static_cast<uint16_t>(newLength & 0xffffu);
}

static void ResetOwnedSectionBytes(void*& buffer, uint16_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void ResetOwnedSectionBytes(void*& buffer, uint32_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void ResetState11ReplyOwnerState(CLTLoginMediator::PostAuthMarginLoadingState& ownerState) {
    // anchor: launcher.exe:0x438a50
    // Shared `+0xf1c` reset helper used by both state8 (`0x43f930`) and state11 (`0x440320`).
    std::fill(std::begin(ownerState.characterNameBufferF1c), std::end(ownerState.characterNameBufferF1c), '\0');
    ownerState.characterReplyFieldF3c = 0u;
    ownerState.characterReplyFieldF40 = 0u;
    ownerState.characterReplyFieldF44 = 0x1000u;
    std::fill(ownerState.characterFlagsF48.begin(), ownerState.characterFlagsF48.end(), 0u);
    std::fill(ownerState.secondaryCharacterDataF68.begin(), ownerState.secondaryCharacterDataF68.end(), 0u);
    std::fill(ownerState.characterRecordPointersF88.begin(), ownerState.characterRecordPointersF88.end(), 0u);
    std::fill(ownerState.section0StringF8c.begin(), ownerState.section0StringF8c.end(), '\0');
    std::fill(ownerState.section0StringFac.begin(), ownerState.section0StringFac.end(), '\0');
    std::fill(ownerState.section0StringFcc.begin(), ownerState.section0StringFcc.end(), '\0');
    std::fill(ownerState.state8Section0RawF88.begin(), ownerState.state8Section0RawF88.end(), 0u);
    ownerState.replySectionData13cc = 0u;
    ownerState.replySectionData13d0 = 0u;
    ResetOwnedSectionBytes(
        ownerState.state8Section0OverflowBuffer13f0,
        ownerState.state8Section0OverflowLength13f4,
        ownerState.section0Flag13f6);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer13f8, ownerState.allocatedBufferLength13fc, ownerState.flag13fe);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1400, ownerState.allocatedBufferLength1404, ownerState.flag1406);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1408, ownerState.allocatedBufferLength140c, ownerState.allocatedBufferFlag140e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1410, ownerState.allocatedBufferLength1414, ownerState.flag1416);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1418, ownerState.allocatedBufferLength141c, ownerState.allocatedBufferFlag141e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1420, ownerState.allocatedBufferLength1424, ownerState.allocatedBufferFlag1426);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1428, ownerState.allocatedBufferLength142c, ownerState.allocatedBufferFlag142e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1430, ownerState.allocatedBufferLength1434, ownerState.flag1436);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1438, ownerState.allocatedBufferLength143c, ownerState.flag143e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1440, ownerState.allocatedBufferLength1444, ownerState.flag1448);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer144c, ownerState.allocatedBufferLength1450, ownerState.flag1452);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1454, ownerState.allocatedBufferLength1458, ownerState.flag145a);
}

static void SeedState11FirstFragment(
    CLTLoginMediator::PostAuthMarginLoadingState& ownerState,
    uint32_t parsedField05) {
    // anchor: launcher.exe:0x440320 first-fragment seed
    // Tightened from direct disassembly:
    // - seed source is owner `+0x108/+0x12c/+0x134`, not the later current-slot record table
    // - `0x438a50` already reset the broader buffer/flag family and seeded `+0xf44 = 0x1000`
    ResetState11ReplyOwnerState(ownerState);
    ownerState.characterReplyFieldF3c = parsedField05;
    ownerState.characterReplyFieldF40 = ownerState.sourceField12c;
    std::copy(
        ownerState.sourceLeadString108.begin(),
        ownerState.sourceLeadString108.end(),
        ownerState.characterNameBufferF1c);
    ownerState.characterNameBufferF1c[sizeof(ownerState.characterNameBufferF1c) - 1u] = '\0';
    std::copy_n(ownerState.sourceDwords134.begin(), ownerState.characterFlagsF48.size(), ownerState.characterFlagsF48.begin());
    ownerState.flag13fe = 1u;
    ownerState.flag1406 = 1u;
    ownerState.flag1416 = 1u;
    ownerState.flag1448 = 1u;
    ownerState.flag1452 = 1u;
}

static void ApplyState11Section11SideEffect(
    CLTLoginMediator::PostAuthMarginLoadingState& ownerState,
    const ParsedState11LoadCharacterReplyScaffold& parsed) {
    // anchor: launcher.exe:0x43f8c0
    // Shared section-0x0b side effect used by both state8 and state11:
    // - if byteCount > 4, copy the leading dword into owner `+0x145c`
    // - copy the remaining bytes into the small-string-like family at owner `+0x1460`
    // - otherwise clear both fields
    ownerState.state8Section11Dword145c = 0u;
    ownerState.state8Section11String1460.clear();

    if (!parsed.sectionData || parsed.sectionByteCount <= 4u) {
        return;
    }

    ownerState.state8Section11Dword145c = ReadU32LE(parsed.sectionData);
    ownerState.state8Section11String1460.assign(
        reinterpret_cast<const char*>(parsed.sectionData + 4u),
        reinterpret_cast<const char*>(parsed.sectionData + parsed.sectionByteCount));
}

static void HandleState11ReplySection(
    CLTLoginMediator::PostAuthMarginLoadingState& ownerState,
    const ParsedState11LoadCharacterReplyScaffold& parsed) {
    switch (parsed.sectionSelectorMinus2) {
        case 0u:
            if (parsed.sectionData && parsed.sectionByteCount >= 0x44cu) {
                ownerState.characterRecordPointersF88[0] = ReadU32LE(parsed.sectionData + 0x00u);
                ownerState.replySectionData13cc = ReadU32LE(parsed.sectionData + 0x444u);
                ownerState.replySectionData13d0 = ReadU32LE(parsed.sectionData + 0x448u);
                CopyCStringIntoFixed(
                    ownerState.section0StringF8c.data(),
                    ownerState.section0StringF8c.size(),
                    parsed.sectionData + 0x04u,
                    parsed.sectionByteCount - 0x04u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFac.data(),
                    ownerState.section0StringFac.size(),
                    parsed.sectionData + 0x24u,
                    parsed.sectionByteCount > 0x24u ? parsed.sectionByteCount - 0x24u : 0u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFcc.data(),
                    ownerState.section0StringFcc.size(),
                    parsed.sectionData + 0x44u,
                    parsed.sectionByteCount > 0x44u ? parsed.sectionByteCount - 0x44u : 0u);
                ownerState.section0Flag13f6 = 1u;
            }
            break;
        case 3u:
            AppendOwnedSectionBytesU16(
                ownerState.allocatedBuffer1418,
                ownerState.allocatedBufferLength141c,
                parsed.sectionData,
                parsed.sectionByteCount);
            ownerState.allocatedBufferFlag141e = 1u;
            break;
        case 4u:
            AppendOwnedSectionBytesU16(
                ownerState.allocatedBuffer1420,
                ownerState.allocatedBufferLength1424,
                parsed.sectionData,
                parsed.sectionByteCount);
            ownerState.allocatedBufferFlag1426 = 1u;
            break;
        case 5u:
            AppendOwnedSectionBytesU16(
                ownerState.allocatedBuffer1428,
                ownerState.allocatedBufferLength142c,
                parsed.sectionData,
                parsed.sectionByteCount);
            ownerState.allocatedBufferFlag142e = 1u;
            break;
        case 6u:
            AppendOwnedSectionBytesU16(
                ownerState.allocatedBuffer1408,
                ownerState.allocatedBufferLength140c,
                parsed.sectionData,
                parsed.sectionByteCount);
            ownerState.allocatedBufferFlag140e = 1u;
            break;
        case 0x0bu:
            ApplyState11Section11SideEffect(ownerState, parsed);
            spdlog::info(
                "CLTLoginState_State11::Slot6_HandleSecondaryMessage applied shared section 0x0b side effect dword145c=0x{:08x} string1460Len={}",
                static_cast<unsigned>(ownerState.state8Section11Dword145c),
                static_cast<unsigned>(ownerState.state8Section11String1460.size()));
            break;
        default:
            break;
    }
}

}  // namespace

// anchor: launcher.exe vtable 0x004b5154
const char* CLTLoginState_State11::DebugName() const {
    return "CLTLoginState_State11";
}

// anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
uint32_t CLTLoginState_State11::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Faithfulness correction:
    // - `0x43c020` belongs to `CLTLoginState_State11` slot 3, so the packet build/send shape
    //   should live here, not on the mediator
    // - original body:
    //   - sends raw margin opcode `0x0c`
    //     - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //       `MS_CreateCharacterRequest`
    //   - treats `ESI = owner + 0x108`
    //   - creates a packet-builder object through `0x439840`
    //   - resets/initializes the raw `0x4d` payload through `0x43a470`
    //   - writes 17 dwords from owner `+0x134..+0x174`
    //   - appends `RealFirstName`, `RealLastName`, optional `Background`, and `GameSessionID`
    //     through `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940`
    //   - newer `0x43e540` debug-printer review makes those 17 dwords concrete:
    //     SkinToneID, BodyID, HeadID, HairID, HairColorID, TattooID, FacialHairID,
    //     FacialHairColorID, StartingHat, StartingGlasses, StartingShirt, StartingGloves,
    //     StartingCoat, StartingPants, StartingTights, StartingShoes, TraitID
    //   - calls `0x41af70` to forward the completed packet-envelope object through the current
    //     margin connection send path (`0x448cf0`), not to serialize raw bytes itself
    //   - then posts event `0x15`
    // Active-path caution:
    // - this is a very real character-data sender, but the natural-original password-submit path is
    //   still not live-proven here; no natural hit yet on `0x41c3c0` or `0x43c020`
    // - current replacement-launcher runtime proof now lands here explicitly:
    //   `... -> helperState 0x0b -> event 0x15 -> State11::Slot3 send -> Loading Character`
    //   and then stalls before any incoming `MS_LoadCharacterReply`
    const auto& sourceDwords134 = mediator->SourceDwords134();
    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    State11Packet0x4dBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();

    // Keep write order aligned with the original disassembly of `0x43c020`.
    packetBuilder.SetFixedDword(0x01, sourceDwords134[0]);
    packetBuilder.SetFixedDword(0x05, sourceDwords134[1]);
    packetBuilder.SetFixedDword(0x09, sourceDwords134[2]);
    packetBuilder.SetFixedDword(0x0d, sourceDwords134[3]);
    packetBuilder.SetFixedDword(0x11, sourceDwords134[4]);
    packetBuilder.SetFixedDword(0x15, sourceDwords134[5]);
    packetBuilder.SetFixedDword(0x19, sourceDwords134[6]);
    packetBuilder.SetFixedDword(0x1d, sourceDwords134[7]);
    packetBuilder.SetFixedDword(0x35, sourceDwords134[13]);
    packetBuilder.SetFixedDword(0x25, sourceDwords134[9]);
    packetBuilder.SetFixedDword(0x3d, sourceDwords134[15]);
    packetBuilder.SetFixedDword(0x2d, sourceDwords134[11]);
    packetBuilder.SetFixedDword(0x21, sourceDwords134[8]);
    packetBuilder.SetFixedDword(0x39, sourceDwords134[14]);
    packetBuilder.SetFixedDword(0x31, sourceDwords134[12]);
    packetBuilder.SetFixedDword(0x29, sourceDwords134[10]);
    packetBuilder.SetFixedDword(0x41, sourceDwords134[16]);

    packetBuilder.SetRealFirstName(reinterpret_cast<const char*>(mediator->SourceBlock178().data()));
    packetBuilder.SetRealLastName(reinterpret_cast<const char*>(mediator->SourceBlock198().data()));
    packetBuilder.SetBackground(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()));
    packetBuilder.SetGameSessionId(mediator->GetGameSessionId());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.Envelope());
    mediator->PostEventScaffold(0x15u);

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State11::Slot3_BeginOrContinue built fixed-0x4d margin payload payloadTag=0x{:02x} fixedBytes=0x{:02x} totalBytes=0x{:02x} SkinToneID=0x{:08x} BodyID=0x{:08x} HeadID=0x{:08x} HairID=0x{:08x} HairColorID=0x{:08x} TraitID=0x{:08x} RealFirstName='{}' RealLastName='{}' Background='{}' GameSessionID='{}' -> sendResult=0x{:08x} then posts event=0x15",
        State11Packet0x4dFixedPayload::kPayloadTag0c,
        State11Packet0x4dFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        sourceDwords134[0],
        sourceDwords134[1],
        sourceDwords134[2],
        sourceDwords134[3],
        sourceDwords134[4],
        sourceDwords134[16],
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock178().data())),
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock198().data())),
        std::string(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data())),
        mediator->GetGameSessionId() ? mediator->GetGameSessionId() : "<empty>",
        sendResult);
    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue awaiting first helper11 reply; slot6 requires a later raw-0x10 that survives the base margin code-2/4/5 filter currentState={} marginReceiveCount={} filteredBeforeSlot6={} slot6DispatchCount={}",
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->MarginPacketReceiveCountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketFilteredBeforeSlot6CountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketSlot6DispatchCountScaffold()));
    return sendResult;
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->StagedIncomingMarginPacketBytes();
    const uint16_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(stagedBytes);
    if (!parsed.valid) {
        mediator->WorldListCountOrStatus80() = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; mirrored original owner+0x80=0x12000005 and returned false-like",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    auto& ownerState = mediator->MutablePostAuthMarginLoadingState();
    ownerState.worldListCountOrStatus80 = parsed.status;
    if (parsed.status >= 1u) {
        ownerState.sourceLeadString108[0] = '\0';
        ownerState.characterRouteIndexCc8 = 0xffu;
        if (CLTLoginState* failureState = mediator->ScaffoldState3()) {
            mediator->SwitchHelperStateScaffold(3u, failureState);
        }
        mediator->PostErrorScaffold(12u);
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage observed failure status=0x{:08x} handoffWord=0x{:04x}; mirrored original owner+0x108 clear, owner+0xcc8=0xff, state3 switch, and error=12",
            static_cast<unsigned>(parsed.status),
            static_cast<unsigned>(parsed.handoffWord09));
        return 1u;
    }

    if (parsed.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = parsed.expectedSectionCount0b;
    }

    const bool firstFragment = (replySectionsSeen_ == 0u);
    if (firstFragment) {
        SeedState11FirstFragment(ownerState, parsed.field05);
    }

    HandleState11ReplySection(ownerState, parsed);

    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                // `0x440320` writes parsed word `+9` into helper9 `this+6` before switching state.
                // Current source-owned mirror keeps that on the concrete state9 object.
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x440320 completion tail posts event 0x16 after switching to helper9.
        // Next owner-controlled continuation is now tighter too:
        // - helper9 slot3 (`0x439780`) immediately consumes that handoff word
        // - it then calls owner `0x41de40`
        // - later state9 slot6 raw `0x11` success switches to helper12 and posts event `0x18`
        mediator->PostEventScaffold(0x16u);

        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage completed helper11 reply progression status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} firstFragment={} name='{}' -> currentState=helper9 event=0x16",
            parsed.status,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            parsed.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_,
            firstFragment ? 1u : 0u,
            ownerState.characterNameBufferF1c[0] ? ownerState.characterNameBufferF1c : "<empty>");
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
    } else {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage routed helper11 reply status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} section={} bytes={} seen={} expected={} seedCount={} firstFragment={} name='{}'",
            parsed.status,
            parsed.field05,
            parsed.handoffWord09,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            replySectionsSeen_,
            replySectionsExpected_,
            parsed.shouldSeedExpectedSectionCount ? 1u : 0u,
            firstFragment ? 1u : 0u,
            ownerState.characterNameBufferF1c[0] ? ownerState.characterNameBufferF1c : "<empty>");
    }
    return 1u;
}

// anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
uint32_t CLTLoginState_State11::Slot7_GetStateId() const {
    return 11;
}

}  // namespace mxo::ltlogin
