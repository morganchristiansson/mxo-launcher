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

// Implementation of Packet_MsCreateCharacterRequest_0x4b53c8 shared parse/builder family

// anchor: launcher.exe:0x43a330
Packet_MsCreateCharacterRequest_0x4b53c8::Packet_MsCreateCharacterRequest_0x4b53c8(
    mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
    char resolveFieldsNow) {
    messageRef08 = incomingMessageRef;
    if (incomingMessageRef != nullptr) {
        incomingMessageRef->AddRef();
    }

    if (incomingMessageRef != nullptr && incomingMessageRef->headerless10 == 0) {
        payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
            incomingMessageRef->messageStorage0c
                ? incomingMessageRef->messageStorage0c->payloadBytes0c.data()
                : nullptr));
    } else if (incomingMessageRef != nullptr && incomingMessageRef->messageStorage0c != nullptr) {
        uint8_t* const payloadBase = incomingMessageRef->messageStorage0c->payloadBytes0c.data();
        const uint8_t encodedHeaderByte = payloadBase[0x01u];
        const uint32_t lookupHigh = g_MessageOffsetLookupTable[(encodedHeaderByte >> 4u) & 7u];
        const uint32_t lookupLow = g_MessageOffsetLookupTable[encodedHeaderByte & 7u];
        payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
            payloadBase + lookupHigh + lookupLow + 0x12u));
        incomingMessageRef->headerless10 = 1;
    } else {
        payloadPtr04 = 0u;
    }

    createRefParam0c = static_cast<uint8_t>(resolveFieldsNow != 0);
    ResolveFields(resolveFieldsNow);
    if (resolveFieldsNow == '\0') {
        if (payloadAlias10 == nullptr) {
            valid = false;
            status = 0u;
            characterIdLow = 0u;
            characterIdHigh = 0u;
            optionalTextOffset01 = 0u;
            debugString14 = nullptr;
            payloadSize18 = 0u;
            return;
        }
        uint8_t* const currentMessage = static_cast<uint8_t*>(payloadAlias10);
        currentMessage[0x00] = 0x0bu;
        *reinterpret_cast<uint16_t*>(currentMessage + 0x01u) = 0u;
        *reinterpret_cast<uint32_t*>(currentMessage + 0x03u) = 0u;
        *reinterpret_cast<uint32_t*>(currentMessage + 0x07u) = 0u;
        *reinterpret_cast<uint32_t*>(currentMessage + 0x0bu) = 0u;
    }

    valid = payloadAlias10 != nullptr;
    if (!valid) {
        return;
    }

    uint8_t* const currentMessage = static_cast<uint8_t*>(payloadAlias10);
    if (currentMessage[0] != 0x0bu) {
        valid = false;
        return;
    }

    optionalTextOffset01 = ReadU16LE(currentMessage + 0x01u);
    status = ReadU32LE(currentMessage + 0x03u);
    characterIdLow = ReadU32LE(currentMessage + 0x07u);
    characterIdHigh = ReadU32LE(currentMessage + 0x0bu);
    optionalText = reinterpret_cast<const char*>(debugString14);
    optionalTextLength = payloadSize18;
}

// anchor: launcher.exe:0x43a2d0
void Packet_MsCreateCharacterRequest_0x4b53c8::InitializePayloadSize() {
    ResolveFields(static_cast<char>(createRefParam0c));
}

// anchor: launcher.exe:0x43a2d0
void Packet_MsCreateCharacterRequest_0x4b53c8::ResolveFields(char resolveFieldsNow) {
    payloadAlias10 = reinterpret_cast<void*>(static_cast<uintptr_t>(payloadPtr04));
    if (resolveFieldsNow == '\0') {
        if (messageRef08 != nullptr) {
            messageRef08->GrowPayloadByteCount(0x0fu);
            if (messageRef08->messageStorage0c != nullptr) {
                payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
                    messageRef08->messageStorage0c->payloadBytes0c.data()));
                payloadAlias10 = reinterpret_cast<void*>(static_cast<uintptr_t>(payloadPtr04));
            }
        }
        return;
    }

    uint8_t* const currentMessage = static_cast<uint8_t*>(payloadAlias10);
    if (currentMessage != nullptr && ReadU16LE(currentMessage + 0x01u) != 0u) {
        const uint16_t optionalTextOffset = ReadU16LE(currentMessage + 0x01u);
        optionalTextLength = ReadU16LE(currentMessage + optionalTextOffset);
        uint8_t* const optionalTextBytes = currentMessage + optionalTextOffset + 2u;
        debugString14 = reinterpret_cast<const char*>(optionalTextBytes);
        payloadSize18 = optionalTextLength;
        if (optionalTextLength != 0u) {
            optionalTextBytes[optionalTextLength - 1u] = 0;
        }
        return;
    }

    optionalTextLength = 0u;
    payloadSize18 = 0u;
    debugString14 = nullptr;
}

// Implementation of Packet_MsLoadCharacterReply_0x4b542c

// More faithful constructor using CMessageConnectionMessageRef_0x4ba23c directly
// anchor: launcher.exe:0x43ae50
Packet_MsLoadCharacterReply_0x4b542c::Packet_MsLoadCharacterReply_0x4b542c(
    mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
    char initializeEmptyReply) {
    createRefParam0c = static_cast<uint8_t>(initializeEmptyReply != 0);
    messageRef08 = incomingMessageRef;
    if (incomingMessageRef != nullptr) {
        incomingMessageRef->AddRef();
    }

    if (incomingMessageRef != nullptr && incomingMessageRef->headerless10 == 0) {
        setMessageBase04(incomingMessageRef->messageStorage0c
            ? incomingMessageRef->messageStorage0c->payloadBytes0c.data()
            : nullptr);
    } else if (incomingMessageRef != nullptr && incomingMessageRef->messageStorage0c != nullptr) {
        uint8_t* const payloadBase = incomingMessageRef->messageStorage0c->payloadBytes0c.data();
        const uint8_t encodedHeaderByte = payloadBase[0x01u];
        const uint32_t lookupHigh = g_MessageOffsetLookupTable[(encodedHeaderByte >> 4u) & 7u];
        const uint32_t lookupLow = g_MessageOffsetLookupTable[encodedHeaderByte & 7u];
        setMessageBase04(payloadBase + lookupHigh + lookupLow + 0x12u);
        incomingMessageRef->headerless10 = 1;
    } else {
        setMessageBase04(nullptr);
    }

    RefreshDataSectionView(initializeEmptyReply);
    if (initializeEmptyReply == '\0') {
        if (currentMessage10() == nullptr) {
            std::memset(defaultMessageStorage1c(), 0, 0x10u);
            setMessageBase04(defaultMessageStorage1c());
            setCurrentMessage10(messageBase04());
        }
        currentMessage10()[0x00] = 0x10u;
        *reinterpret_cast<uint32_t*>(currentMessage10() + 0x01u) = 0u;
        *reinterpret_cast<uint32_t*>(currentMessage10() + 0x05u) = 0u;
        *reinterpret_cast<uint16_t*>(currentMessage10() + 0x09u) = 0u;
        currentMessage10()[0x0b] = 1u;
        currentMessage10()[0x0c] = 0u;
        currentMessage10()[0x0d] = 0u;
        *reinterpret_cast<uint16_t*>(currentMessage10() + 0x0eu) = 0u;
        setDataSectionBytes14(nullptr);
        payloadSize18 = 0u;
    }

    valid = currentMessage10() != nullptr;
    if (!valid) {
        return;
    }
    if (currentMessage10()[0] != 0x10u) {
        valid = false;
        spdlog::debug("Packet_MsLoadCharacterReply: expected msgType=0x10 but got 0x{:02x}, will fallback to callback84", currentMessage10()[0]);
        return;
    }

    status = ReadU32LE(currentMessage10() + 1u);
    field05 = ReadU32LE(currentMessage10() + 5u);
    handoffWord09 = ReadU16LE(currentMessage10() + 9u);
    spdlog::debug("Packet_MsLoadCharacterReply parsed: msgType=0x{:02x} handoffWord=0x{:04x} bytes[8..15]=[0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}]",
        currentMessage10()[0],
        handoffWord09,
        currentMessage10()[8], currentMessage10()[9], currentMessage10()[10], currentMessage10()[11],
        currentMessage10()[12], currentMessage10()[13], currentMessage10()[14], currentMessage10()[15]);
    expectedSectionCount0b = currentMessage10()[0x0b];
    shouldSeedExpectedSectionCount = (currentMessage10()[0x0c] == 0x01u);
    sectionSelectorMinus2 = static_cast<uint8_t>(currentMessage10()[0x0d] - 2u);
    sectionOffset0e = ReadU16LE(currentMessage10() + 0x0eu);
    sectionByteCount = payloadSize18;
    sectionData = dataSectionBytes14();
}

// anchor: launcher.exe:0x43cca0 / vtable +0x08
void Packet_MsLoadCharacterReply_0x4b542c::DebugString(int formatType) {
    std::string out;
    AppendDebugString(out, formatType);
    if (!out.empty()) {
        spdlog::debug("Packet_MsLoadCharacterReply: {}", out);
    }
}

// anchor: launcher.exe:0x43af20 / vtable +0x0c
void Packet_MsLoadCharacterReply_0x4b542c::InitializePayloadSize() {
    ResetToDefaultMessage();
}

// anchor: launcher.exe:0x43ae00
void Packet_MsLoadCharacterReply_0x4b542c::RefreshDataSectionView(char initializeEmptyReply) {
    setCurrentMessage10(messageBase04());
    if (initializeEmptyReply == '\0') {
        if (messageRef08 != nullptr) {
            messageRef08->GrowPayloadByteCount(0x10u);
            if (messageRef08->messageStorage0c != nullptr) {
                setMessageBase04(messageRef08->messageStorage0c->payloadBytes0c.data());
                setCurrentMessage10(messageBase04());
            }
        }
        return;
    }

    if (currentMessage10() != nullptr) {
        const uint16_t sectionOffset0eLocal = ReadU16LE(currentMessage10() + 0x0eu);
        if (sectionOffset0eLocal != 0u) {
            payloadSize18 = ReadU16LE(currentMessage10() + sectionOffset0eLocal);
            setDataSectionBytes14(currentMessage10() + sectionOffset0eLocal + 2u);
            return;
        }
    }

    payloadSize18 = 0u;
    setDataSectionBytes14(nullptr);
}

// anchor: launcher.exe:0x43af20
void Packet_MsLoadCharacterReply_0x4b542c::ResetToDefaultMessage() {
    std::memset(defaultMessageStorage1c(), 0, 0x10u);
    setMessageBase04(defaultMessageStorage1c());
    setCurrentMessage10(messageBase04());
    currentMessage10()[0x00] = 0x10u;
    currentMessage10()[0x0b] = 1u;
    setDataSectionBytes14(nullptr);
    payloadSize18 = 0u;

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
void Packet_MsLoadCharacterReply_0x4b542c::AppendDebugString(std::string& out, int verbosityLevel) const {
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
