#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>
#include <string>
#include <system_error>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x4b503c
const char* CLTLoginState_State4_0x4b503c::DebugName() const {
    return "CLTLoginState_State4_0x4b503c";
}

// anchor: launcher.exe:0x4393f0 (vtable 0x4b503c slot 2)
uint32_t CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate(void* workItem) {
    if (!workItem || !g_CurrentLoginMediator) {
        return 0u;
    }

    // anchor: launcher.exe:0x4393fa-0x439401 - call workItem->GetWorkType() == 2?
    const auto* workItemHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    if (workItemHeader->workType !=
        mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem);
    }

    // anchor: launcher.exe:0x439411-0x439420 - call GetStatusOrPayloadDword() twice (faithful to original)
    // First call: store to g_CurrentLoginMediator->worldListCountOrStatus80
    const auto* workItemPayload = static_cast<const uint32_t*>(workItem);
    const uint32_t statusFirst = workItemPayload[2];  // offset 0x8
    g_CurrentLoginMediator->worldListCountOrStatus80 = statusFirst;
    // Second call: test for zero (original calls the getter again, faithful to 0x439420)
    const uint32_t status = workItemPayload[2];  // offset 0x8, same as first call

    if (status != 0u) {
        g_CurrentLoginMediator->marginConnectionFlag2d_ = 1;
        if (g_CurrentLoginMediator->marginBeginCount24_ < static_cast<uint32_t>(g_CurrentLoginMediator->marginAddressList3c_.Count())) {
            Slot3_BeginOrContinue(cachedUpstreamOrArg_0x4);
            spdlog::info(
                "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate non-zero status=0x{:08x} ({}) cachedUpstream={} attemptCount24={} candidateCount={} owner+0x2d=1 -> retry slot3",
                static_cast<unsigned>(status),
                std::system_category().message(status),
                fmt::ptr(cachedUpstreamOrArg_0x4),
                static_cast<unsigned>(g_CurrentLoginMediator->marginBeginCount24_),
                static_cast<unsigned>(g_CurrentLoginMediator->marginAddressList3c_.Count()));
            return 1u;
        }

        const CLTLoginState* const cachedUpstreamBeforeTransition = cachedUpstreamOrArg_0x4;
        const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
        if (nextHelperStateId != 13u) {
            // anchor: launcher.exe:0x4392d0 / 0x439473
            // - clear cached upstream at `this+4`
            // - write owner `+0x104 = -1`
            // - direct owner `+0x24 = 0`
            // - switch helper/state to `3`
            cachedUpstreamOrArg_0x4 = nullptr;
            g_CurrentLoginMediator->marginCurrentWorldId104_ = -1;
            g_CurrentLoginMediator->marginBeginCount24_ = 0u;
            (void)g_CurrentLoginMediator->SetCurrentState(3u);
        }
        g_CurrentLoginMediator->PostError(6u);
        spdlog::info(
            "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate non-zero status=0x{:08x} ({}) retry exhausted cachedUpstream={} upstreamPhaseCode={} currentState={} cachedUpstreamNow={} owner+0x104={} -> PostError(0x06)",
            static_cast<unsigned>(status),
            std::system_category().message(status),
            fmt::ptr(cachedUpstreamBeforeTransition),
            static_cast<unsigned>(nextHelperStateId),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
            fmt::ptr(cachedUpstreamOrArg_0x4),
            g_CurrentLoginMediator->marginCurrentWorldId104_);
        return 1u;
    }

    // Ghidra/disassembly recheck for `0x439495..0x4394c8`:
    // - read cached upstream from `this+4`
    // - call cached upstream vtable `+0x18`
    // - clear `this+4 = 0`
    // - write owner `+0x104 = -1`
    // - switch helper through `0x41b450`
    // - post event `0x0e`
    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    cachedUpstreamOrArg_0x4 = nullptr;
    g_CurrentLoginMediator->marginCurrentWorldId104_ = -1;
    const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);
    g_CurrentLoginMediator->PostEvent(0x0eu);
    spdlog::info(
        "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate status=0x{:08x} ({}) cachedUpstreamPhaseCode={} -> currentState={} switchDispatchResult=0x{:08x} owner+0x104=-1 then PostEvent(0x0e)",
        static_cast<unsigned>(status),
        std::system_category().message(status),
        static_cast<unsigned>(nextHelperStateId),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x439300 (vtable 0x4b503c slot 3)
void CLTLoginState_State4_0x4b503c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    if (!g_CurrentLoginMediator) {
        return;
    }

    // Faithfulness/ownership correction from the fresh `0x439300` disassembly review:
    // - `0x439300` belongs to `CLTLoginState_State4_0x4b503c` vtable `0x4b503c` slot 3
    // - this object caches the first incoming upstream/helper pointer at `this+4`
    // - it then calls that cached object's vtable `+0x18` and uses the returned phase/state code
    //   for the real case split
    // - only the narrow owner-side route getters and `0x41e500` transport/init stay on the
    //   mediator
    if (cachedUpstreamOrArg_0x4 == nullptr) {
        cachedUpstreamOrArg_0x4 = upstreamOrArg;
    }

    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    switch (upstreamPhaseCode) {
        case 6: {
            const std::vector<char>& routeDescriptor30 = g_CurrentLoginMediator->GetRouteDescriptor30();
            g_CurrentLoginMediator->BeginMarginConnection(
                routeDescriptor30.empty() ? nullptr : routeDescriptor30.data(),
                0u);
            return;
        }

        case 7:
        case 8:
        case 13: {
            // `0x439328..0x439345`
            // - read owner byte `+0xcc8`
            // - call owner vtable `+0xe0(slot, 0)`
            // - forward the returned route-host text into `0x41e500`
            const uint8_t routeSlot =
                g_CurrentLoginMediator->selectionRouteState684_.currentSlotOrSelectionIndex644_;
            g_CurrentLoginMediator->BeginMarginConnection(
                g_CurrentLoginMediator->GetVariantWorldName(routeSlot),
                0u);
            return;
        }

        case 10: {
            g_CurrentLoginMediator->BeginMarginConnection(
                g_CurrentLoginMediator->GetWorldNameByIndex(
                    g_CurrentLoginMediator->createCharacterData108.selectedWorldField24),
                0u);
            return;
        }

        default: {
            // `0x439398..0x4393c4`
            // - read owner dword `+0x104`
            // - if it is not `-1`, call owner vtable `+0xfc(index)`
            // - only when the returned pointer is non-null call `0x41e500`
            const int32_t field104Value = g_CurrentLoginMediator->marginCurrentWorldId104_;
            if (field104Value == -1) {
                return;
            }
            const char* const routeHostText =
                g_CurrentLoginMediator->GetWorldNameByIndex(static_cast<uint32_t>(field104Value));
            if (routeHostText == nullptr) {
                return;
            }
            g_CurrentLoginMediator->BeginMarginConnection(routeHostText, 0u);
            return;
        }
    }
}

// anchor: launcher.exe:0x439190 (vtable 0x4b503c slot 6)
uint32_t CLTLoginState_State4_0x4b503c::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return 0;
}

// anchor: launcher.exe:0x4686b0 (vtable 0x4b503c slot 7)
uint32_t CLTLoginState_State4_0x4b503c::GetStateId() const {
    return 4;
}

}  // namespace mxo::ltlogin
