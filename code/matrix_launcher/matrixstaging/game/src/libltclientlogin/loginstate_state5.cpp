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
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    return header->workType04;
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
const char* CLTLoginState_State5_0x4b5064::DebugName() const {
    return "CLTLoginState_State5_0x4b5064";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5_0x4b5064::Slot2_HandleSecondaryGate(void* workItem) {
    if (!workItem || !g_CurrentLoginMediator) {
        return 0u;
    }

    const uint32_t workType = LoginState5WorkItemTypeScaffold(workItem);
    if (workType != 0x0bu) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem);
    }

    const uint32_t status = LoginState5WorkItemPayloadScaffold(workItem);
    if (status != 0u) {
        g_CurrentLoginMediator->marginConnectionFlag2d_ = 1;
    }

    // Ghidra/disassembly recheck for `0x4395c8..0x4395d9`:
    // - local type-`0x0b` completion restores through cached upstream `this+4`
    // - calls cached upstream vtable `+0x18`
    // - passes that state id to `0x41b450`
    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);

    spdlog::info(
        "CLTLoginState_State5_0x4b5064::Slot2_HandleSecondaryGate handled local type0x0b status=0x{:08x} cachedUpstream={} nextHelperState=0x{:02x} owner+0x2d={} currentState={} switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(status),
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(nextHelperStateId),
        g_CurrentLoginMediator->marginConnectionFlag2d_ != 0u ? 1u : 0u,
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
void CLTLoginState_State5_0x4b5064::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    if (upstreamOrArg != nullptr) {
        // Ghidra/disassembly recheck for `0x43952a..0x439549`:
        // - if `this+4` already exists and incoming upstream phase is `2` or `4`, keep the
        //   existing cached pointer
        // - otherwise overwrite `this+4 = upstream`
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_0x4 == nullptr || (upstreamPhaseCode != 2u && upstreamPhaseCode != 4u)) {
            cachedUpstreamOrArg_0x4 = upstreamOrArg;
        }
    }
    if (!g_CurrentLoginMediator) {
        return;
    }

    const auto* authBootstrapChild = g_CurrentLoginMediator->authBootstrapChild680_.get();
    const auto* authReplyCopyShadowF4 = authBootstrapChild
                                            ? static_cast<const AuthBootstrapReplyCopyShadowF4_0x44add0*>(
                                                  authBootstrapChild->authReplyCopyShadowF4)
                                            : nullptr;
    const bool replyCopyShadowMissingOrStale =
        authBootstrapChild ? authBootstrapChild->State5ReplyCopyShadowMissingOrStale() : true;
    if (replyCopyShadowMissingOrStale) {
        const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(2u);
        spdlog::info(
            "CLTLoginState_State5_0x4b5064::Slot3_BeginOrContinue replyCopyShadowMissingOrStale=1 cachedUpstream={} incomingUpstream={} authBootstrapChild680={} authReplyCopyShadowF4={} currentState={} -> helper2 switchDispatchResult=0x{:08x}",
            fmt::ptr(cachedUpstreamOrArg_0x4),
            fmt::ptr(upstreamOrArg),
            fmt::ptr(authBootstrapChild),
            fmt::ptr(authReplyCopyShadowF4),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
            static_cast<unsigned>(switchDispatchResult));
        return;
    }

    g_CurrentLoginMediator->PrepareState5MarginConnectionCopySend();
    g_CurrentLoginMediator->PostEvent(0x10u);
    spdlog::info(
        "CLTLoginState_State5_0x4b5064::Slot3_BeginOrContinue replyCopyShadowMissingOrStale=0 cachedUpstream={} incomingUpstream={} authBootstrapChild680={} authReplyCopyShadowF4={} currentState={} then PostEvent(0x10)",
        fmt::ptr(cachedUpstreamOrArg_0x4),
        fmt::ptr(upstreamOrArg),
        fmt::ptr(authBootstrapChild),
        fmt::ptr(authReplyCopyShadowF4),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    return;
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5_0x4b5064::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5_0x4b5064::GetStateId() const {
    return 5;
}

}  // namespace mxo::ltlogin
