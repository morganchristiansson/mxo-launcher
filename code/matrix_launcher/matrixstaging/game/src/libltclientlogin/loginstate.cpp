#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
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
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(void* workItem, CLTLoginMediator* mediator) {
    if (!workItem || !mediator) {
        return 0u;
    }

    const uint32_t workType = LoginStateWorkItemTypeScaffold(workItem);
    if (workType != mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeClose) {
        return 0u;
    }

    if (mediator->AuthConnectionFlag2c() != 0u) {
        mediator->PostEvent(1u);
        spdlog::info(
            "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed armed owner+0x2c -> event=0x01 currentState={}",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    if (mediator->GetLastLoginStatus() == 0u) {
        mediator->WorldListCountOrStatus80() = 1u;
    }
    const uint32_t switchDispatchResult = mediator->SwitchHelperStateByIdScaffold(0u);
    mediator->PostError(1u);
    spdlog::info(
        "CLTLoginState::Slot1_HandlePrimaryGate shared auth close-gate observed unarmed owner+0x2c -> owner+0x80=0x{:08x} currentState={} switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(mediator->WorldListCountOrStatus80()),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(void* workItem, CLTLoginMediator* mediator) {
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
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    if (mediator->GetLastLoginStatus() == 0u) {
        mediator->WorldListCountOrStatus80() = 1u;
    }
    (void)mediator->SwitchHelperStateByIdScaffold(3u);
    mediator->PostError(7u);
    spdlog::info(
        "CLTLoginState::Slot2_HandleSecondaryGate shared close-gate observed unarmed owner+0x2d -> owner+0x80=0x{:08x} currentState={}",
        static_cast<unsigned>(mediator->WorldListCountOrStatus80()),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-3 rows)
uint32_t CLTLoginState::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
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
uint32_t CLTLoginState::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    // Exact recovered side effect from `0x004397c0`:
    // - write owner `+0x80 = 0x12000004`
    // - return false-like
    // launcher.exe reaches owner `+0x80` through the global current-mediator singleton; source
    // mirrors the same state through the already-threaded mediator pointer instead of inventing a
    // second global owner handle.
    if (mediator != nullptr) {
        mediator->WorldListCountOrStatus80() = 0x12000004u;
    }
    return 0u;
}

// anchor: launcher.exe:0x004397c0 (shared slot-6 failure stub on selected vtables only)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0u;
}

// anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-8 rows)
uint32_t CLTLoginState::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    // Same caveat as slot 3/4: the original body is only `ret`.
    return 1u;
}

// anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
uint32_t CLTLoginState::Slot9_IsNetworkDriven() const {
    return 1;
}

// anchor: launcher.exe:0x004439300 consults slot-7-style state/helper ids before margin-route dispatch
uint32_t CLTLoginState::DispatchPhaseCode() const {
    return Slot7_GetStateId();
}

// anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    // Exact recovered shape from `0x004397e0`:
    // - when object byte `this+4 == 1`, delegate to owner helper `0x41c5c0`
    // - if that helper returns `< 1`, return success-ish immediately
    // - otherwise write owner `+0x80 = 0x12000005` and fail
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
        mediator->WorldListCountOrStatus80() = 0x12000005u;
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

}  // namespace mxo::ltlogin
