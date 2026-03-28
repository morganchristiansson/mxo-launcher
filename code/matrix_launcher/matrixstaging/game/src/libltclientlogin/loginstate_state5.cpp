#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

static uint32_t LoginState5WorkItemTypeScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* header =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

static uint32_t LoginState5WorkItemPayloadScaffold(const void* workItem) {
    if (!workItem) {
        return 0u;
    }

    const auto* payload = static_cast<const uint32_t*>(workItem);
    return payload[2];
}

static CLTLoginState* LookupRegisteredScaffoldStateById(CLTLoginMediator* mediator, uint32_t stateId) {
    if (!mediator) {
        return nullptr;
    }

    switch (stateId) {
        case 0u: return mediator->ScaffoldState0();
        case 1u: return mediator->ScaffoldState1();
        case 2u: return mediator->ScaffoldState2();
        case 3u: return mediator->ScaffoldState3();
        case 4u: return mediator->ScaffoldState4();
        case 6u: return mediator->ScaffoldState6();
        case 8u: return mediator->ScaffoldState8();
        case 9u: return mediator->ScaffoldState9();
        case 10u: return mediator->ScaffoldState10();
        case 11u: return mediator->ScaffoldState11();
        case 12u: return mediator->ScaffoldState12();
        case 13u: return mediator->ScaffoldState13();
        case 14u: return mediator->ScaffoldState14();
        case 15u: return mediator->ScaffoldState15();
        case 16u: return mediator->ScaffoldState16();
        case 17u: return mediator->ScaffoldState17();
        case 18u: return mediator->ScaffoldState18();
        case 19u: return mediator->ScaffoldState19();
        default:
            return nullptr;
    }
}

}  // namespace

// anchor: launcher.exe vtable 0x004b5064
const char* CLTLoginState_State5::DebugName() const {
    return "CLTLoginState_State5";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5::Slot2_HandleSecondaryGate(void* workItem, CLTLoginMediator* mediator) {
    if (!workItem || !mediator) {
        return 0u;
    }

    const uint32_t workType = LoginState5WorkItemTypeScaffold(workItem);
    if (workType != 0x0bu) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem, mediator);
    }

    const uint32_t status = LoginState5WorkItemPayloadScaffold(workItem);
    if (status != 0u) {
        mediator->SetMarginConnectionCloseWaitEvent0fGateArmedScaffold(true);
    }

    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    CLTLoginState* nextState = LookupRegisteredScaffoldStateById(mediator, nextHelperStateId);
    if (nextState != nullptr) {
        mediator->SwitchHelperStateScaffold(nextHelperStateId, nextState);
    }

    spdlog::info(
        "CLTLoginState_State5::Slot2_HandleSecondaryGate handled local type0x0b status=0x{:08x} cachedUpstream={} nextHelperState=0x{:02x} resolvedNextState={} owner+0x2d={} currentState={}",
        static_cast<unsigned>(status),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(nextHelperStateId),
        nextState ? nextState->DebugName() : "<null>",
        mediator->MarginConnectionCloseWaitEvent0fGateArmedScaffold() ? 1u : 0u,
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
uint32_t CLTLoginState_State5::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (upstreamOrArg != nullptr) {
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_ == nullptr || (upstreamPhaseCode != 2u && upstreamPhaseCode != 4u)) {
            cachedUpstreamOrArg_ = upstreamOrArg;
        }
    }

    const auto* authReplyCopyShadowF4 =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(
            mediator ? mediator->AuthBootstrapReplyCopyShadowF4Scaffold() : nullptr);
    spdlog::info(
        "CLTLoginState_State5::Slot3_BeginOrContinue cachedUpstream={} incomingUpstream={} incomingUpstreamPhaseCode={} mediator={} currentState={} authReplyCopyShadowF4={} (kept non-sending: 0x41b500 -> 0x41ce80/+0x98 -> 0x41f30 helper chain remains source-owned but dormant until the full 0x136 block is validated)",
        fmt::ptr(cachedUpstreamOrArg_),
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(RecoverCachedUpstreamPhaseCode(upstreamOrArg)),
        fmt::ptr(mediator),
        mediator && mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        fmt::ptr(authReplyCopyShadowF4));
    return 1u;
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5::Slot7_GetStateId() const {
    return 5;
}

}  // namespace mxo::ltlogin
