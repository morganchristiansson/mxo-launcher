#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

static uint32_t LoginState4WorkItemTypeScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* header =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

static uint32_t LoginState4WorkItemPayloadScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* payload = static_cast<const uint32_t*>(workItem);
    return payload[2];
}

static uint32_t BeginMarginConnectionForState4Case(
    CLTLoginMediator* mediator,
    const char* routeHostText,
    uint8_t cachedRouteSelector) {
    if (!mediator) {
        return 0u;
    }
    return mediator->BeginMarginConnectionScaffold(routeHostText, cachedRouteSelector);
}

}  // namespace

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4::DebugName() const {
    return "CLTLoginState_State4";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4::Slot2_HandleSecondaryGate(void* workItem, CLTLoginMediator* mediator) {
    if (!workItem || !mediator) {
        return 0u;
    }

    if (LoginState4WorkItemTypeScaffold(workItem) !=
        mxo::liblttcp::CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem, mediator);
    }

    const uint32_t status = LoginState4WorkItemPayloadScaffold(workItem);
    mediator->WorldListCountOrStatus80() = status;

    if (status != 0u) {
        mediator->SetMarginConnectionCloseWaitEvent0fGateArmedScaffold(true);
        if (mediator->MarginConnectAttemptCountScaffold() < mediator->MarginConnectCandidateCountScaffold()) {
            const uint32_t retryResult = Slot3_BeginOrContinue(cachedUpstreamOrArg_, mediator);
            spdlog::info(
                "CLTLoginState_State4::Slot2_HandleSecondaryGate non-zero status=0x{:08x} cachedUpstream={} attemptCount24={} candidateCount={} owner+0x2d=1 -> retry slot3 result=0x{:08x}",
                static_cast<unsigned>(status),
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(mediator->MarginConnectAttemptCountScaffold()),
                static_cast<unsigned>(mediator->MarginConnectCandidateCountScaffold()),
                static_cast<unsigned>(retryResult));
            return 1u;
        }

        const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
        mediator->ResetMarginConnectAttemptCountScaffold();
        if (nextHelperStateId != 13u) {
            if (CLTLoginState* failureState = mediator->ScaffoldState3()) {
                mediator->SwitchHelperStateScaffold(3u, failureState);
            }
        }
        mediator->PostErrorScaffold(6u);
        spdlog::info(
            "CLTLoginState_State4::Slot2_HandleSecondaryGate non-zero status=0x{:08x} retry exhausted cachedUpstream={} upstreamPhaseCode={} -> currentState={} then PostError(0x06)",
            static_cast<unsigned>(status),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(nextHelperStateId),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    // Ghidra/disassembly recheck for `0x439495..0x4394c8`:
    // - read cached upstream from `this+4`
    // - call cached upstream vtable `+0x18`
    // - clear `this+4 = 0`
    // - write owner `+0x104 = -1`
    // - switch helper through `0x41b450`
    // - post event `0x0e`
    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    cachedUpstreamOrArg_ = nullptr;
    mediator->MutableMarginRouteState().currentWorldId = -1;
    const uint32_t switchDispatchResult = mediator->SwitchHelperStateByIdScaffold(nextHelperStateId);
    mediator->PostEventScaffold(0x0eu);
    spdlog::info(
        "CLTLoginState_State4::Slot2_HandleSecondaryGate status=0x{:08x} cachedUpstreamPhaseCode={} -> currentState={} switchDispatchResult=0x{:08x} owner+0x104=-1 then PostEvent(0x0e)",
        static_cast<unsigned>(status),
        static_cast<unsigned>(nextHelperStateId),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
uint32_t CLTLoginState_State4::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Faithfulness/ownership correction from the fresh `0x439300` disassembly review:
    // - `0x439300` belongs to `CLTLoginState_State4` vtable `0x004b503c` slot 3
    // - this object caches the first incoming upstream/helper pointer at `this+4`
    // - it then calls that cached object's vtable `+0x18` and uses the returned phase/state code
    //   for the real case split
    // - only the narrow owner-side route getters and `0x41e500` transport/init stay on the
    //   mediator
    if (cachedUpstreamOrArg_ == nullptr) {
        cachedUpstreamOrArg_ = upstreamOrArg;
    }

    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    switch (upstreamPhaseCode) {
        case 6:
            return BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteDescriptor(),
                0u);

        case 7:
        case 8:
        case 13:
            // Exact `0x439300 -> 0x41e500` consequence to preserve:
            // - this branch forwards owner byte `+0xcc8` as arg2
            // - `0x41e500` only refreshes route/address state on `arg2 == 0`
            // - so on the live state8/state13 continuation path the returned route-text pointer is
            //   forwarded even when current source still has no populated route-string table entry
            return BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteFromCurrentCharacterSlot(),
                mediator->CurrentCharacterRouteIndexCc8Scaffold());

        case 10:
            return BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteFromDescriptorIndex(
                    mediator->postAuthMarginLoadingState_.createCharacterData108.selectedWorldField24),
                static_cast<uint8_t>(
                    mediator->postAuthMarginLoadingState_.createCharacterData108.selectedWorldField24 & 0xffu));

        default: {
            // Current source-owned mirror for the default branch's owner `+0x104` dword remains
            // `CurrentMarginRouteState().currentWorldId`; keep the field meaning provisional and
            // only preserve the original `!= -1 -> owner vtable +0xfc -> if non-null call 0x41e500`
            // structure here.
            const int32_t field104Value = mediator->CurrentMarginRouteState().currentWorldId;
            if (field104Value == -1) {
                return 0u;
            }
            const char* const routeHostText =
                mediator->ResolveMarginRouteFromWorldId(static_cast<uint32_t>(field104Value));
            if (routeHostText == nullptr) {
                return 0u;
            }
            return BeginMarginConnectionForState4Case(
                mediator,
                routeHostText,
                0u);
        }
    }
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
uint32_t CLTLoginState_State4::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
uint32_t CLTLoginState_State4::Slot7_GetStateId() const {
    return 4;
}

}  // namespace mxo::ltlogin
