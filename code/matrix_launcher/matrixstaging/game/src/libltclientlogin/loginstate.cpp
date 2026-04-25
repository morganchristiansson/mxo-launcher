#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include "../../../../matrixstaging/runtime/src/libltmessaging/messageconnection.h"
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
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    return header->workType;
}

}  // namespace

// anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(void* workItem) {
    if (!workItem || !g_CurrentLoginMediator) {
        return 0u;
    }

    const uint32_t workType = LoginStateWorkItemTypeScaffold(workItem);
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        return 0u;
    }

    if (g_CurrentLoginMediator->authConnectionFlag2c_ != 0u) {
        g_CurrentLoginMediator->PostEvent(1u);
        spdlog::info(
            "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed armed owner+0x2c -> event=0x01 currentState={}",
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (g_CurrentLoginMediator->GetLastLoginStatus() == 0u) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 1u;
    }
    const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(0u);
    g_CurrentLoginMediator->PostError(1u);
    spdlog::info(
        "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed unarmed owner+0x2c -> owner+0x80=0x{:08x} currentState={} switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(void* workItem) {
    if (!workItem || !g_CurrentLoginMediator) {
        return 0u;
    }

    const uint32_t workType = LoginStateWorkItemTypeScaffold(workItem);
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeClose) {
        return 0u;
    }

    if (g_CurrentLoginMediator->MarginConnectionCloseWaitEvent0fGateArmedScaffold()) {
        g_CurrentLoginMediator->PostEvent(0x0fu);
        spdlog::info(
            "CLTLoginState::Slot2_HandleSecondaryGate shared close-gate observed armed owner+0x2d -> event=0x0f currentState={}",
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (g_CurrentLoginMediator->GetLastLoginStatus() == 0u) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 1u;
    }
    (void)g_CurrentLoginMediator->SetCurrentState(3u);
    g_CurrentLoginMediator->PostError(7u);
    spdlog::info(
        "CLTLoginState::Slot2_HandleSecondaryGate shared close-gate observed unarmed owner+0x2d -> owner+0x80=0x{:08x} currentState={}",
        static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-3 rows)
void CLTLoginState::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    // The original body is a prototype-agnostic bare `ret`. Source keeps a truthy no-op return
    // here only as a C++ placeholder for states that still inherit that stub.
    return;
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
    if (g_CurrentLoginMediator != nullptr) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000004u;
    }
    return 0u;
}

// anchor: launcher.exe:0x004397c0 (shared slot-6 failure stub on selected vtables only)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
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
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    // Exact recovered shape from `0x004397e0`:
    // - when object byte `this+4 == 1`, delegate to owner helper `0x41c5c0`
    // - if that helper returns `< 1`, return success-ish immediately
    // - otherwise write owner `+0x80 = 0x12000005` and fail
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    // Live-path caution tightened again from the latest breakpoint-only original run:
    // - after the proven state9 success tail (`0x41b450(0x0c) -> 0x41cfb0(0x18)`), the natural run
    //   later re-hit `0x41cfb0` with event `0x0f` and entered game
    // - it still did not hit `0x004397e0` or `0x41c5c0` on that continuation
    // - so keep this as a later probe path, not as the already-proven immediate post-state9 flow
    if (slot6DispatchByte4_ == 1u && g_CurrentLoginMediator != nullptr) {
        const uint32_t dispatchResult = g_CurrentLoginMediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (dispatchResult < 1u) {
            spdlog::info(
                "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=1 delegated to owner callback84 workItem={} -> dispatchResult=0x{:08x}",
                fmt::ptr(workItem),
                static_cast<unsigned>(dispatchResult));
            return 1u;
        }
    }

    if (g_CurrentLoginMediator != nullptr) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000005u;
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

// More faithful constructor using CMessageConnectionMessageRef_0x4ba23c directly
// anchor: launcher.exe:0x43ae50
LoadCharacterReplyEnvelope_0x4b542c::LoadCharacterReplyEnvelope_0x4b542c(
    mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
    char initializeEmptyReply)
    : initializeEmptyReply0c_(!!initializeEmptyReply) {
    // Store the incoming message ref and call AddRef
    incomingMessageRef08_ = incomingMessageRef;
    if (incomingMessageRef != nullptr) {
        incomingMessageRef->AddRef();
    }

    // Compute messageBase04_ based on headerless flag
    if (incomingMessageRef == nullptr || incomingMessageRef->headerless10 == 0) {
        // Non-headerless path: messageBase points past the header
        if (incomingMessageRef != nullptr && incomingMessageRef->messageStorage0c) {
            messageBase04_ = reinterpret_cast<uint8_t*>(incomingMessageRef->messageStorage0c) + 0xc;
        } else {
            messageBase04_ = nullptr;
        }
    } else {
        // Headerless path: more complex offset calculation using lookup table
        if (incomingMessageRef != nullptr && incomingMessageRef->messageStorage0c) {
            uint8_t* storageBase = reinterpret_cast<uint8_t*>(incomingMessageRef->messageStorage0c);
            uint32_t iVar2 = reinterpret_cast<uint32_t>(storageBase + 0xc);
            uint8_t bVar1 = *(storageBase + 0xc + 0xd);
            uint32_t lookup1 = g_MessageOffsetLookupTable[(bVar1 >> 4) & 7];
            uint32_t lookup2 = g_MessageOffsetLookupTable[bVar1 & 7];
            messageBase04_ = reinterpret_cast<uint8_t*>(lookup1 + lookup2 + iVar2 + 0x1e);
            // Mark as headerless
            incomingMessageRef->headerless10 = 1;
        } else {
            messageBase04_ = nullptr;
        }
    }

    RefreshDataSectionView(initializeEmptyReply);
    if (!initializeEmptyReply) {
        ResetToDefaultMessage();
    }

    valid = currentMessage10_ != nullptr;
    // Verify this is actually a LoadCharacterReply (0x10) before parsing
    if (valid && currentMessage10_[0] != 0x10u) {
        spdlog::debug("LoadCharacterReplyEnvelope: expected msgType=0x10 but got 0x{:02x}, marking invalid", currentMessage10_[0]);
        return;
    }

    // Parse standard message envelope fields
    status = ReadU32LE(currentMessage10_ + 1u);
    field05 = ReadU32LE(currentMessage10_ + 5u);
    handoffWord09 = ReadU16LE(currentMessage10_ + 9u);
    spdlog::debug("LoadCharacterReplyEnvelope parsed: msgType=0x{:02x} handoffWord=0x{:04x} bytes[8..15]=[0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}]",
        currentMessage10_[0],
        handoffWord09,
        currentMessage10_[8], currentMessage10_[9], currentMessage10_[10], currentMessage10_[11],
        currentMessage10_[12], currentMessage10_[13], currentMessage10_[14], currentMessage10_[15]);
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
    if (currentMessage10_ == nullptr || incomingMessageRef08_ == nullptr) {
        dataSectionBytes14_ = nullptr;
        dataSectionByteCount18_ = 0u;
        return;
    }

    // Get payload info from the message ref's storage
    const auto* storage = incomingMessageRef08_->messageStorage0c;
    if (storage == nullptr) {
        dataSectionBytes14_ = nullptr;
        dataSectionByteCount18_ = 0u;
        return;
    }

    const uint16_t sectionOffset0eLocal = ReadU16LE(currentMessage10_ + 0x0eu);
    const uint16_t payloadByteCount = storage->PayloadByteCount();
    if (sectionOffset0eLocal != 0u &&
        static_cast<size_t>(sectionOffset0eLocal) + 2u <= payloadByteCount) {
        dataSectionByteCount18_ = ReadU16LE(currentMessage10_ + sectionOffset0eLocal);
        dataSectionBytes14_ = currentMessage10_ + sectionOffset0eLocal + 2u;
        const size_t remaining = payloadByteCount - (sectionOffset0eLocal + 2u);
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
