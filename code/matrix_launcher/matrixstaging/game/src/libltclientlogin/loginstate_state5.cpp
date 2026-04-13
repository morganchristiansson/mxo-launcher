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

}  // namespace

// anchor: launcher.exe vtable 0x004b5064
const char* CLTLoginState_State5::DebugName() const {
    return "CLTLoginState_State5";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5::Slot2_HandleSecondaryGate(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!workItem || !mediator) {
        return 0u;
    }

    const uint32_t workType = LoginState5WorkItemTypeScaffold(workItem);
    if (workType != 0x0bu) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem);
    }

    const uint32_t status = LoginState5WorkItemPayloadScaffold(workItem);
    if (status != 0u) {
        mediator->marginConnectionFlag2d_ = 1;
    }

    // Ghidra/disassembly recheck for `0x4395c8..0x4395d9`:
    // - local type-`0x0b` completion restores through cached upstream `this+4`
    // - calls cached upstream vtable `+0x18`
    // - passes that state id to `0x41b450`
    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    const uint32_t switchDispatchResult = mediator->SwitchHelperStateByIdScaffold(nextHelperStateId);

    spdlog::info(
        "CLTLoginState_State5::Slot2_HandleSecondaryGate handled local type0x0b status=0x{:08x} cachedUpstream={} nextHelperState=0x{:02x} owner+0x2d={} currentState={} switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(status),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(nextHelperStateId),
        mediator->MarginConnectionCloseWaitEvent0fGateArmedScaffold() ? 1u : 0u,
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
uint32_t CLTLoginState_State5::Slot3_BeginOrContinue(void* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (upstreamOrArg != nullptr) {
        // Ghidra/disassembly recheck for `0x43952a..0x439549`:
        // - if `this+4` already exists and incoming upstream phase is `2` or `4`, keep the
        //   existing cached pointer
        // - otherwise overwrite `this+4 = upstream`
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_ == nullptr || (upstreamPhaseCode != 2u && upstreamPhaseCode != 4u)) {
            cachedUpstreamOrArg_ = upstreamOrArg;
        }
    }
    if (!mediator) {
        return 0u;
    }

    const auto* authReplyCopyShadowF4 =
        static_cast<const AuthBootstrapReplyCopyShadowF4Sketch*>(
            mediator->AuthBootstrapReplyCopyShadowF4Scaffold());
    const bool replyCopyShadowStillValid = mediator->HasValidState5ReplyCopyShadowF4Scaffold();
    if (!replyCopyShadowStillValid) {
        const uint32_t switchDispatchResult = mediator->SwitchHelperStateByIdScaffold(2u);
        spdlog::info(
            "CLTLoginState_State5::Slot3_BeginOrContinue replyCopyShadowStillValid=0 cachedUpstream={} incomingUpstream={} authReplyCopyShadowF4={} currentState={} -> helper2 switchDispatchResult=0x{:08x}",
            fmt::ptr(cachedUpstreamOrArg_),
            fmt::ptr(upstreamOrArg),
            fmt::ptr(authReplyCopyShadowF4),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
            static_cast<unsigned>(switchDispatchResult));
        return switchDispatchResult;
    }

    const uint32_t sendResult = mediator->PrepareState5MarginConnectionCopySendScaffold();
    mediator->PostEvent(0x10u);
    spdlog::info(
        "CLTLoginState_State5::Slot3_BeginOrContinue replyCopyShadowStillValid=1 cachedUpstream={} incomingUpstream={} authReplyCopyShadowF4={} sendResult=0x{:08x} currentState={} then PostEvent(0x10)",
        fmt::ptr(cachedUpstreamOrArg_),
        fmt::ptr(upstreamOrArg),
        fmt::ptr(authReplyCopyShadowF4),
        static_cast<unsigned>(sendResult),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return sendResult;
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5::Slot6_HandleSecondaryMessage(void* workItem) {
    (void)workItem;
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5::GetStateId() const {
    return 5;
}

}  // namespace mxo::ltlogin
