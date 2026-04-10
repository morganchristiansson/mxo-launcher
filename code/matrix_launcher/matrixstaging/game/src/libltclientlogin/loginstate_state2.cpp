#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// UNANCHORED: source-owned registered-state lookup helper used by the current state2 auth-result
// switch mirror. Original `0x43f300` resolves helper transitions by calling back into the owner
// instead of using a local phase-code table like this.
CLTLoginState* LookupRegisteredScaffoldStateByPhaseCode(CLTLoginMediator* mediator, uint32_t phaseCode) {
    if (!mediator) {
        return nullptr;
    }

    switch (phaseCode) {
        case 0u: return mediator->ScaffoldState0();
        case 1u: return mediator->ScaffoldState1();
        case 2u: return mediator->ScaffoldState2();
        case 3u: return mediator->ScaffoldState3();
        case 4u: return mediator->ScaffoldState4();
        case 5u: return mediator->ScaffoldState5();
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
            break;
    }

    return nullptr;
}

}  // namespace

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
uint32_t CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Current tighter source-owned mirror of `0x439210`:
    // - this is the post-submit helper entered from owner-owned `ProcessLoginRequest`, not the
    //   startup default helper
    // - cache the incoming upstream/helper unless that object's phase/state code is already `1`
    // - gate on `0x41b490` / auth connection state `+0x34 == 2`
    // - if not connected yet, switch to helper/state 1 so its slot-3 body starts the auth
    //   transport connection
    // - if already connected, this state2 body owns the ready-side handoff into the owner+0x680
    //   bootstrap child: static `0x439210` gathers owner-side inputs from `+0x94`, the
    //   owner-side getter result, and the send target, then forwards that concrete call shape
    //   into `0x448050`
    // - current source now mirrors that split explicitly by calling the separate owner+0x680
    //   child directly from state2 instead of routing through a fake mediator auth-bootstrap
    //   method
    const uint32_t incomingUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
    if (incomingUpstreamPhaseCode != 1u) {
        cachedUpstreamOrArg_ = upstreamOrArg;
    }

    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    if (!mediator->HasReadyAuthConnectionState2()) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state2 -> state1 auth-connect incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        const uint32_t connectResult = mediator->BeginAuthConnectionViaState1Scaffold();
        spdlog::info(
            "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue auth transport not ready incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} -> BeginAuthConnectionViaState1Scaffold=0x{:08x}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
            static_cast<unsigned>(connectResult));
        return connectResult;
    }

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth state2 ready-side owner+0x680 bootstrap-child dispatch currentState={} cachedUpstream={} cachedUpstreamPhaseCode={} (static 0x439210 ready branch feeds 0x448050)",
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode));
    const uint32_t sendResult = mediator->AuthBootstrapChild680().PrepareAndDispatch(*mediator);
    spdlog::info(
        "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch=0x{:08x}",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        mediator->HasReadyAuthConnectionState2() ? 1u : 0u,
        static_cast<unsigned>(sendResult));
    return sendResult;
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Current best recovered role from `0x43f300`:
    // - it does not parse raw auth bytes directly
    // - instead it forwards the incoming auth-message object into
    //   `0x448140 = AuthBootstrap680_HandleInboundAuthMessage`
    // - then it interprets that child helper's return code to decide owner `+0x80`,
    //   helper-switch, and event/error flow
    // Current source-owned boundary now keeps the original object split closer:
    // - this state2 body forwards the local receive/message-ref object straight into the
    //   owner+0x680 child
    // - any replacement-only raw-payload sidecar copy now happens inside that child after it
    //   resolves the `0x41bc20`-style logical payload span from the incoming message object
    // - this state2 body still owns the early inbound auth return-code switch
    const uint32_t childResult =
        mediator->AuthBootstrapChild680().HandleInboundAuthMessage(workItem, *mediator);
    const std::vector<uint8_t>& stagedBytes = mediator->stagedIncomingAuthPacketBytes_;
    const uint8_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    if (childResult == kAuthBootstrap680InboundUnhandled) {
        mediator->WorldListCountOrStatus80() = 0x12000004u;
        spdlog::info(
            "CLTLoginState_AuthenticatePending::AuthMessageDispatch rejected staged auth bytes={} rawCode=0x{:02x}; mirrored original owner+0x80=0x12000004 and returned false-like",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    if (childResult == kAuthBootstrap680InboundHandledContinueWaiting) {
        spdlog::info(
            "CLTLoginState_AuthenticatePending::AuthMessageDispatch routed staged auth rawCode=0x{:02x} through owner+0x680 child and remained in the early wait path currentState={}",
            static_cast<unsigned>(rawCode),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    mediator->WorldListCountOrStatus80() = mediator->AuthBootstrapChild680().inboundAuthStatusEc;

    switch (childResult) {
        case kAuthBootstrap680InboundAuthReplySuccess: {
            AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
                mediator->AuthBootstrapChild680(),
                *mediator,
                mediator->lastAuthReply_);
            mediator->ResetMarginBootstrapState();
            mediator->RecoverAuthReplyPrivateExponentIntoMarginBootstrapState(mediator->lastAuthReply_);
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            if (AuthBootstrap680ConsumeState2AuthReplySuccessOneTimeGateScaffold()) {
                CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorStateScaffold(mediator);
                AuthBootstrap680SyncState2AuthReplySuccessOneTimeScaffold(
                    mediator->AuthBootstrapChild680(),
                    *mediator,
                    mediator->lastAuthReply_);
                mediator->PersistCharactersIniFromRecoveredAuthStateScaffold();
                mediator->PostEventScaffold(6u);
            }
            mediator->expectedMarginRequestName_ = "CERT_ConnectRequest";

            // Current source now mirrors the pre-gate `0x441330` subset plus the gated one-time
            // `0x441260 / 0x41e760 / PostEvent(6) / owner+0x150 / 0x441170` subset.
            // Remaining uncertainty here is mostly on exact field/blob semantics, not on the
            // existence of those side effects.
            uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
            if (nextHelperStateId == 0u || nextHelperStateId == 0x10u) {
                nextHelperStateId = 3u;
            }

            CLTLoginState* nextState = LookupRegisteredScaffoldStateByPhaseCode(mediator, nextHelperStateId);
            uint32_t switchDispatchResult = 0u;
            if (nextState != nullptr) {
                if (nextHelperStateId == 8u) {
                    // Current existing-character bridge refinement:
                    // once early auth reply success has rebuilt the owner auth/bootstrap state,
                    // re-enter state8 slot 3 immediately so the now-proven margin-connect
                    // continuation can begin from the restored helper instead of stalling at the
                    // bare helper install.
                    switchDispatchResult = mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                        nextHelperStateId,
                        nextState,
                        this,
                        "State2 raw-0x0b success -> existing-character state8 margin continuation");
                } else {
                    mediator->SwitchHelperStateScaffold(nextHelperStateId, nextState);
                }
            } else {
                spdlog::warn(
                    "CLTLoginState_AuthenticatePending::AuthMessageDispatch could not resolve registered helper state 0x{:02x} from cachedUpstream={} currentState={} after raw-0x0b success",
                    static_cast<unsigned>(nextHelperStateId),
                    fmt::ptr(cachedUpstreamOrArg_),
                    mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
            }
            mediator->PostEventScaffold(5u);

            // Replacement-only post-AS_AuthReply margin auto-begin now stays on the concrete
            // auth-reply handler path instead of broadening the thinner `0x449a30` bridge.
            uint32_t marginAutoBeginResult = 0u;
            bool triggeredMarginAutoBegin = false;
            bool deferredMarginAutoBeginToState8 = false;
            if (!mediator->postAuthMarginAutoBeginAttemptedScaffold_) {
                const uint32_t currentHelperPhaseCode =
                    mediator->CurrentState() ? mediator->CurrentState()->DispatchPhaseCode() : 0u;

                // Existing-character continuation correction:
                // - starting the margin connect while we are still on state3 leaves helper4's
                //   cached upstream aligned to the old state3 wait leaf
                // - on the natural existing-character path the first meaningful state4
                //   margin-connect entry for this continuation is the later
                //   `+0xec -> state8 slot3 -> helper4` handoff during "Loading Character"
                if (currentHelperPhaseCode == 3u) {
                    deferredMarginAutoBeginToState8 = true;
                } else {
                    mediator->postAuthMarginAutoBeginAttemptedScaffold_ = true;
                    triggeredMarginAutoBegin = true;
                    marginAutoBeginResult = mediator->BeginLauncherMarginConnectionScaffold();
                }
            }

            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch adopted early auth-reply success rawCode=0x{:02x} owner+0x80=0x{:08x} cachedUpstream={} -> nextHelperState=0x{:02x} currentState={} switchDispatchResult=0x{:08x} event=0x05 triggeredMarginAutoBegin={} deferredMarginAutoBeginToState8={} marginAutoBeginResult=0x{:08x}",
                static_cast<unsigned>(rawCode),
                static_cast<unsigned>(mediator->WorldListCountOrStatus80()),
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(nextHelperStateId),
                mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
                static_cast<unsigned>(switchDispatchResult),
                triggeredMarginAutoBegin ? 1u : 0u,
                deferredMarginAutoBeginToState8 ? 1u : 0u,
                static_cast<unsigned>(marginAutoBeginResult));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyError: {
            mediator->expectedMarginRequestName_ = nullptr;
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            if (CLTLoginState* state0 = mediator->ScaffoldState0()) {
                mediator->SwitchHelperStateScaffold(0u, state0);
            }
            mediator->PostErrorScaffold(4u);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch observed early raw-0x0b error reply owner+0x80=0x{:08x}; mirrored original state0 switch and error=4",
                static_cast<unsigned>(mediator->WorldListCountOrStatus80()));
            return 1u;
        }

        case kAuthBootstrap680InboundGetPublicKeyReplyError:
        case kAuthBootstrap680InboundGetPublicKeyWorkerError: {
            if (CLTLoginState* state0 = mediator->ScaffoldState0()) {
                mediator->SwitchHelperStateScaffold(0u, state0);
            }
            mediator->PostErrorScaffold(2u);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch observed early raw-0x07 failure childResult={} owner+0x80=0x{:08x}; mirrored original state0 switch and error=2",
                static_cast<unsigned>(childResult),
                static_cast<unsigned>(mediator->WorldListCountOrStatus80()));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyValidationError: {
            mediator->expectedMarginRequestName_ = nullptr;
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            mediator->WorldListCountOrStatus80() = 0x1200000bu;
            if (CLTLoginState* state0 = mediator->ScaffoldState0()) {
                mediator->SwitchHelperStateScaffold(0u, state0);
            }
            mediator->PostErrorScaffold(0x0fu);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch rejected early raw-0x0b success-side adoption owner+0x80=0x{:08x}; mirrored original state0 switch and error=0x0f",
                static_cast<unsigned>(mediator->WorldListCountOrStatus80()));
            return 1u;
        }

        default:
            break;
    }

    if (CLTLoginState* state0 = mediator->ScaffoldState0()) {
        mediator->SwitchHelperStateScaffold(0u, state0);
    }
    mediator->PostErrorScaffold(4u);
    spdlog::warn(
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch reached unexpected childResult={} rawCode=0x{:02x}; mirrored fallback state0 switch and error=4",
        static_cast<unsigned>(childResult),
        static_cast<unsigned>(rawCode));
    return 1u;
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::Slot7_GetStateId() const {
    return 2;
}

}  // namespace mxo::ltlogin
