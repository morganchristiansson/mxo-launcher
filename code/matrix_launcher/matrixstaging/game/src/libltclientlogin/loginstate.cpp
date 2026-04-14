#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe:0x004397d0 (slot 3 no-op stub on multiple vtables)
uint32_t PlaceholderStateAction(const char* debugName, const char* anchor) {
    (void)debugName;
    (void)anchor;
    return 1;
}

uint32_t RecoverCachedUpstreamPhaseCode(const void* cachedUpstreamOrArg) {
    // `0x439300` calls the cached upstream/helper object's vtable `+0x18`.
    // The source scaffold keeps that as the shared `DispatchPhaseCode()` wrapper over the
    // recovered login-state family.
    const auto* cachedUpstreamState = static_cast<const CLTLoginState*>(cachedUpstreamOrArg);
    return cachedUpstreamState ? cachedUpstreamState->DispatchPhaseCode() : 0u;
}

// anchor: reconstructed shared login-state family surface spanning launcher.exe vtable families
const char* CLTLoginState_AbstractFinalLeafBase::DebugName() const {
    return "CLTLoginState_AbstractFinalLeafBase";
}

namespace {

static uint32_t LoginStateWorkItemTypeScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* header =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

}  // namespace

// anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!workItem || !mediator) {
        return 0u;
    }

    const uint32_t workType = LoginStateWorkItemTypeScaffold(workItem);
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        return 0u;
    }

    if (mediator->authConnectionFlag2c_ != 0u) {
        mediator->PostEvent(1u);
        spdlog::info(
            "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed armed owner+0x2c -> event=0x01 currentState={}",
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (mediator->GetLastLoginStatus() == 0u) {
        mediator->worldListCountOrStatus80 = 1u;
    }
    const uint32_t switchDispatchResult = mediator->SetCurrentState(0u);
    mediator->PostError(1u);
    spdlog::info(
        "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed unarmed owner+0x2c -> owner+0x80=0x{:08x} currentState={} switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(mediator->worldListCountOrStatus80),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!workItem || !mediator) {
        return 0u;
    }

    const uint32_t workType = LoginStateWorkItemTypeScaffold(workItem);
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        return 0u;
    }

    if (mediator->MarginConnectionCloseWaitEvent0fGateArmedScaffold()) {
        mediator->PostEvent(0x0fu);
        spdlog::info(
            "CLTLoginState::Slot2_HandleSecondaryGate shared close-gate observed armed owner+0x2d -> event=0x0f currentState={}",
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (mediator->GetLastLoginStatus() == 0u) {
        mediator->worldListCountOrStatus80 = 1u;
    }
    (void)mediator->SetCurrentState(3u);
    mediator->PostError(7u);
    spdlog::info(
        "CLTLoginState::Slot2_HandleSecondaryGate shared close-gate observed unarmed owner+0x2d -> owner+0x80=0x{:08x} currentState={}",
        static_cast<unsigned>(mediator->worldListCountOrStatus80),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-3 rows)
uint32_t CLTLoginState::Slot3_BeginOrContinue(void* upstreamOrArg) {
    (void)upstreamOrArg;
    // The original body is a prototype-agnostic bare `ret`. Source keeps a truthy no-op return
    // here only as a C++ placeholder for states that still inherit that stub.
    return 1u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by many slot-4 rows)
uint32_t CLTLoginState::Slot4_NoOp() {
    // Same caveat as slot 3: the original body is only `ret`.
    return 1u;
}

// anchor: launcher.exe:0x004397c0 (shared slot-5 failure stub on many vtables)
uint32_t CLTLoginState::AuthMessageDispatch(void* workItem) {
    (void)workItem;
    // Exact recovered side effect from `0x004397c0`:
    // - write owner `+0x80 = 0x12000004`
    // - return false-like
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (mediator != nullptr) {
        mediator->worldListCountOrStatus80 = 0x12000004u;
    }
    return 0u;
}

// anchor: launcher.exe:0x004397c0 (shared slot-6 failure stub on selected vtables only)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(void* workItem) {
    (void)workItem;
    return 0u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-8 rows)
uint32_t CLTLoginState::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) {
    (void)param1;
    (void)context;
    // Same caveat as slot 3/4: the original body is only `ret`.
    return 1u;
}

// anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
uint32_t CLTLoginState::Slot9_IsNetworkDriven() const {
    return 1;
}

// anchor: launcher.exe:0x004439300 consults slot-7-style state/helper ids before margin-route dispatch
uint32_t CLTLoginState::DispatchPhaseCode() const {
    return GetStateId();
}

// anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(void* workItem) {
    // Exact recovered shape from `0x004397e0`:
    // - when object byte `this+4 == 1`, delegate to owner helper `0x41c5c0`
    // - if that helper returns `< 1`, return success-ish immediately
    // - otherwise write owner `+0x80 = 0x12000005` and fail
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    // Live-path caution tightened again from the latest breakpoint-only original run:
    // - after the proven state9 success tail (`0x41b450(0x0c) -> 0x41cfb0(0x18)`), the natural run
    //   later re-hit `0x41cfb0` with event `0x0f` and entered game
    // - it still did not hit `0x004397e0` or `0x41c5c0` on that continuation
    // - so keep this as a later probe path, not as the already-proven immediate post-state9 flow
    if (slot6DispatchByte4_ == 1u && mediator != nullptr) {
        const uint32_t dispatchResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (dispatchResult < 1u) {
            spdlog::info(
                "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=1 delegated to owner callback84 workItem={} -> dispatchResult=0x{:08x}",
                fmt::ptr(workItem),
                static_cast<unsigned>(dispatchResult));
            return 1u;
        }
    }

    if (mediator != nullptr) {
        mediator->worldListCountOrStatus80 = 0x12000005u;
    }
    spdlog::info(
        "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=0x{:02x} set owner+0x80=0x12000005 workItem={}",
        static_cast<unsigned>(slot6DispatchByte4_),
        fmt::ptr(workItem));
    return 0u;
}

// anchor: launcher.exe:0x00437b40 (vtable 0x004b51b8 slot 9)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot9_IsNetworkDriven() const {
    return 0;
}

// Implementation of LoadCharacterReplyEnvelope_0x4b542c

// More faithful constructor using CMessageConnectionMessageRef directly
// anchor: launcher.exe:0x43ae50
LoadCharacterReplyEnvelope_0x4b542c::LoadCharacterReplyEnvelope_0x4b542c(
    mxo::liblttcp::CMessageConnectionMessageRef* incomingMessageRef,
    char initializeEmptyReply)
    : initializeEmptyReply0c_(!!initializeEmptyReply) {
    // Extract raw bytes from the message ref (more faithful to static-RE)
    if (incomingMessageRef && incomingMessageRef->messageStorage0c) {
        const auto* storage = incomingMessageRef->messageStorage0c;
        const uint8_t* payloadBase = storage->PayloadBaseScaffold();
        uint16_t payloadByteCount = storage->PayloadByteCountScaffold();
        messageBase04_ = payloadByteCount > 0 ? const_cast<uint8_t*>(payloadBase) : nullptr;
        // Also track via the vector pointer for RefreshDataSectionView compatibility
        incomingMarginMessageBytes08_ = nullptr;  // N/A for message ref approach
    } else {
        messageBase04_ = nullptr;
        incomingMarginMessageBytes08_ = nullptr;
    }

    RefreshDataSectionView(initializeEmptyReply);
    if (!initializeEmptyReply) {
        ResetToDefaultMessage();
    }

    // For headerless messages, we can't validate the same way - use payload presence
    valid = currentMessage10_ != nullptr;
    if (!valid) {
        return;
    }

    // Parse standard message envelope fields
    status = ReadU32LE(currentMessage10_ + 1u);
    field05 = ReadU32LE(currentMessage10_ + 5u);
    handoffWord09 = ReadU16LE(currentMessage10_ + 9u);
    expectedSectionCount0b = currentMessage10_[0x0b];
    shouldSeedExpectedSectionCount = (currentMessage10_[0x0c] == 0x01u);
    sectionSelectorMinus2 = static_cast<uint8_t>(currentMessage10_[0x0d] - 2u);
    sectionOffset0e = ReadU16LE(currentMessage10_ + 0x0eu);
    sectionByteCount = dataSectionByteCount18_;
    sectionData = dataSectionBytes14_;
}

// Convenience constructor using pre-extracted bytes
// anchor: launcher.exe:0x43ae50
LoadCharacterReplyEnvelope_0x4b542c::LoadCharacterReplyEnvelope_0x4b542c(
    const std::vector<uint8_t>& incomingMarginMessageBytes,
    bool initializeEmptyReply)
    : incomingMarginMessageBytes08_(&incomingMarginMessageBytes),
      initializeEmptyReply0c_(initializeEmptyReply) {
    messageBase04_ = incomingMarginMessageBytes.empty()
        ? nullptr
        : const_cast<uint8_t*>(incomingMarginMessageBytes.data());
    RefreshDataSectionView(static_cast<char>(initializeEmptyReply ? 1 : 0));
    if (!initializeEmptyReply) {
        ResetToDefaultMessage();
    }

    valid = currentMessage10_ != nullptr &&
            incomingMarginMessageBytes.size() >= 0x10u &&
            currentMessage10_[0] == 0x10u;
    if (!valid) {
        return;
    }

    status = ReadU32LE(currentMessage10_ + 1u);
    field05 = ReadU32LE(currentMessage10_ + 5u);
    handoffWord09 = ReadU16LE(currentMessage10_ + 9u);
    expectedSectionCount0b = currentMessage10_[0x0b];
    shouldSeedExpectedSectionCount = (currentMessage10_[0x0c] == 0x01u);
    sectionSelectorMinus2 = static_cast<uint8_t>(currentMessage10_[0x0d] - 2u);
    sectionOffset0e = ReadU16LE(currentMessage10_ + 0x0eu);
    sectionByteCount = dataSectionByteCount18_;
    sectionData = dataSectionBytes14_;
}

// anchor: launcher.exe:0x43ae00
void LoadCharacterReplyEnvelope_0x4b542c::RefreshDataSectionView(char initializeEmptyReply) {
    currentMessage10_ = messageBase04_;
    if (initializeEmptyReply == '\0') {
        dataSectionBytes14_ = nullptr;
        dataSectionByteCount18_ = 0u;
        return;
    }
    if (currentMessage10_ == nullptr || incomingMarginMessageBytes08_ == nullptr ||
        incomingMarginMessageBytes08_->size() < 0x10u) {
        dataSectionBytes14_ = nullptr;
        dataSectionByteCount18_ = 0u;
        return;
    }

    const uint16_t sectionOffset0eLocal = ReadU16LE(currentMessage10_ + 0x0eu);
    if (sectionOffset0eLocal != 0u &&
        static_cast<size_t>(sectionOffset0eLocal) + 2u <= incomingMarginMessageBytes08_->size()) {
        dataSectionByteCount18_ = ReadU16LE(currentMessage10_ + sectionOffset0eLocal);
        dataSectionBytes14_ = currentMessage10_ + sectionOffset0eLocal + 2u;
        const size_t remaining = incomingMarginMessageBytes08_->size() - (sectionOffset0eLocal + 2u);
        if (dataSectionByteCount18_ > remaining) {
            dataSectionByteCount18_ = static_cast<uint16_t>(remaining);
        }
        return;
    }

    dataSectionByteCount18_ = 0u;
    dataSectionBytes14_ = nullptr;
}

// anchor: launcher.exe:0x43af20
void LoadCharacterReplyEnvelope_0x4b542c::ResetToDefaultMessage() {
    defaultMessageStorage_.fill(0u);
    messageBase04_ = defaultMessageStorage_.data();
    currentMessage10_ = messageBase04_;
    currentMessage10_[0x00] = 0x10u;
    currentMessage10_[0x0b] = 1u;
    dataSectionBytes14_ = nullptr;
    dataSectionByteCount18_ = 0u;

    valid = true;
    status = 0u;
    field05 = 0u;
    handoffWord09 = 0u;
    expectedSectionCount0b = 1u;
    shouldSeedExpectedSectionCount = false;
    sectionSelectorMinus2 = static_cast<uint8_t>(0u - 2u);
    sectionOffset0e = 0u;
    sectionByteCount = 0u;
    sectionData = nullptr;
}

// anchor: launcher.exe:0x43cca0
void LoadCharacterReplyEnvelope_0x4b542c::AppendDebugString(std::string& out, int verbosityLevel) const {
    if (verbosityLevel == 2 || verbosityLevel == 3) {
        out += "Status:" + std::to_string(status);
        out += " CharacterID:" + std::to_string(field05);
        out += " UDPPort:" + std::to_string(handoffWord09);
        out += " MessageNumber:" + std::to_string(expectedSectionCount0b);
        out += " FinalMessage:" + std::to_string(shouldSeedExpectedSectionCount ? 1 : 0);
        out += " DataType:" + std::to_string(static_cast<unsigned>(sectionSelectorMinus2 + 2u));
        if (verbosityLevel == 2) {
            out += " Data:(Array of size " + std::to_string(sectionByteCount) + ") ";
        } else {
            out += " Data:[";
            for (uint16_t i = 0; i < sectionByteCount; ++i) {
                out += std::to_string(sectionData ? static_cast<unsigned>(sectionData[i]) : 0u);
                out += ',';
            }
            out += "] ";
        }
    }
}

}  // namespace mxo::ltlogin
