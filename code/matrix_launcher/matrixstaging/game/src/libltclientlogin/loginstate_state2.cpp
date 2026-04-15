#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

}  // namespace

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 / vtable 0x4b5014 slot 3
// Ghidra signature:
//   void __thiscall CLTLoginState_AuthenticatePending_Slot3_BeginOrContinue(void *this, int *pUpstreamState)
//   - Original returns void (return value in EAX is not set)
//   - Source keeps uint32_t for practical reasons but result is typically ignored
//
// Static RE now recovers the exact assembly call to PrepareAndDispatch at 0x448050:
//   call dword ptr [EDX + 0x168]  -> vtable+0x168: GetAuthConnection - sends send target result to stack
//   call dword ptr [EAX + 0x20]   -> vtable+0x20: GetNoPatchLauncherVersionValuePtr
//   call dword ptr [EDX + 0x38]  -> vtable+0x38: GetUsername returns owner+0x94 char*
//   mov ECX, dword ptr [EAX + 0x60] -> session token pointer from owner+0x94
//   push session, push sendtarget, push uiMD5, push keyMD5, push loginType=1,
//        push password, push username
//   call 0x448050
//
// Source mirrors the exact call shape via PrepareAndDispatch on owner+0x680 child.
void CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
        return;
    }

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
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        const uint32_t connectResult = mediator->BeginAuthConnectionViaState1Scaffold();
        spdlog::info(
            "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue auth transport not ready incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} -> BeginAuthConnectionViaState1Scaffold=0x{:08x}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
            static_cast<unsigned>(connectResult));
        return;
    }

    // Ready-side handoff: direct field access mirrors assembly:
    //   call dword ptr [EDX + 0x168] -> GetAuthConnection returns authConnection_ at owner+0x18
    //   call dword ptr [EAX + 0x20] -> GetNoPatchLauncherVersionValuePtr
    //   call dword ptr [EDX + 0x38] -> GetUsername returns char* to owner+0x94 (username00)
    //   mov ECX, dword ptr [EAX + 0x60] -> session token at owner+0xf4
    auto* child = mediator->authBootstrapChild680_.get();
    // Direct field access: owner+0x18 (authConnection_) instead of accessor
    void* sendTarget = mediator->authConnection_;
    // Direct offset access: owner+0x94 + 0x60 = ownerAuthBootstrapSource94_.sessionToken60.begin
    const char* sessionToken = *reinterpret_cast<const char**>(
        reinterpret_cast<uint8_t*>(&mediator->ownerAuthBootstrapSource94_) + 0x60);

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth state2 ready-side owner+0x680 bootstrap-child dispatch currentState={} cachedUpstream={} cachedUpstreamPhaseCode={} (static 0x439210 ready branch feeds 0x448050)",
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode));
    const uint32_t sendResult = child->PrepareAndDispatch(*mediator, sendTarget, sessionToken);
    spdlog::info(
        "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch=0x{:08x}",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
        mediator->HasReadyAuthConnectionState2() ? 1u : 0u,
        static_cast<unsigned>(sendResult));
    // Ghidra shows original returns void - return value is effectively ignored by callers
    return;
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
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
        mediator->authBootstrapChild680_->HandleInboundAuthMessage(workItem, *mediator);
    const std::vector<uint8_t>& stagedBytes = mediator->stagedIncomingAuthPacketBytes_;
    const uint8_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    if (childResult == kAuthBootstrap680InboundUnhandled) {
        mediator->worldListCountOrStatus80 = 0x12000004u;
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
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    mediator->worldListCountOrStatus80 = mediator->authBootstrapChild680_->inboundAuthStatusEc;
    // anchor: launcher.exe:0x43f300 / sets authConnectionFlag2c_ = 1 when childResult != 0 && childResult != 1
    mediator->authConnectionFlag2c_ = 1u;

    switch (childResult) {
        case kAuthBootstrap680InboundAuthReplySuccess: {
            AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
                *mediator->authBootstrapChild680_,
                *mediator,
                mediator->lastAuthReply_);
            // REMOVED for fidelity: original at 0x43f300 does NOT call ResetMarginBootstrapState or
            // RecoverAuthReplyPrivateExponentIntoMarginBootstrapState in auth-reply success path.
            // Original does inline recovery during margin CERT_Challenge (via margin +0xa0 bootstrap object).
            // See ContinueMarginBootstrapHandshake for lazy inline recovery implementation.
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            if (AuthBootstrap680ConsumeState2AuthReplySuccessOneTimeGateScaffold()) {
                CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorStateScaffold(mediator);
                AuthBootstrap680SyncState2AuthReplySuccessOneTimeScaffold(
                    *mediator->authBootstrapChild680_,
                    *mediator,
                    mediator->lastAuthReply_);
                mediator->PersistCharactersIniFromRecoveredAuthStateScaffold();
                mediator->PostEvent(6u);
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

            // Ghidra recheck of `0x43f300` now matters here directly:
            // - after the cached-upstream phase normalization (`0/0x10 -> 3`), the original does
            //   not resolve a helper object locally
            // - it passes only the resulting helper/state id to `0x41b450`
            // - `0x41b450` then loads `[id*4 + 0x4f7868]`, installs owner `+0x10`, and
            //   immediately re-enters the new helper slot 3 with old-state `this`
            const uint32_t switchDispatchResult =
                mediator->SetCurrentState(nextHelperStateId);
            mediator->PostEvent(5u);

            // Replacement-only post-AS_AuthReply margin auto-begin now stays on the concrete
            // auth-reply handler path instead of broadening the thinner `0x449a30` bridge.
            uint32_t marginAutoBeginResult = 0u;
            bool triggeredMarginAutoBegin = false;
            bool deferredMarginAutoBeginToState8 = false;
            if (!mediator->postAuthMarginAutoBeginAttemptedScaffold_) {
                const uint32_t currentHelperPhaseCode =
                    mediator->currentState_ ? mediator->currentState_->DispatchPhaseCode() : 0u;

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
                static_cast<unsigned>(mediator->worldListCountOrStatus80),
                fmt::ptr(cachedUpstreamOrArg_),
                static_cast<unsigned>(nextHelperStateId),
                mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
                static_cast<unsigned>(switchDispatchResult),
                triggeredMarginAutoBegin ? 1u : 0u,
                deferredMarginAutoBeginToState8 ? 1u : 0u,
                static_cast<unsigned>(marginAutoBeginResult));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyError: {
            mediator->expectedMarginRequestName_ = nullptr;
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            (void)mediator->SetCurrentState(0u);
            mediator->PostError(4u);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch observed early raw-0x0b error reply owner+0x80=0x{:08x}; mirrored original state0 switch and error=4",
                static_cast<unsigned>(mediator->worldListCountOrStatus80));
            return 1u;
        }

        case kAuthBootstrap680InboundGetPublicKeyReplyError:
        case kAuthBootstrap680InboundGetPublicKeyWorkerError: {
            (void)mediator->SetCurrentState(0u);
            mediator->PostError(2u);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch observed early raw-0x07 failure childResult={} owner+0x80=0x{:08x}; mirrored original state0 switch and error=2",
                static_cast<unsigned>(childResult),
                static_cast<unsigned>(mediator->worldListCountOrStatus80));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyValidationError: {
            mediator->expectedMarginRequestName_ = nullptr;
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);
            mediator->worldListCountOrStatus80 = 0x1200000bu;
            (void)mediator->SetCurrentState(0u);
            mediator->PostError(0x0fu);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch rejected early raw-0x0b success-side adoption owner+0x80=0x{:08x}; mirrored original state0 switch and error=0x0f",
                static_cast<unsigned>(mediator->worldListCountOrStatus80));
            return 1u;
        }

        default:
            break;
    }

    (void)mediator->SetCurrentState(0u);
    mediator->PostError(4u);
    spdlog::warn(
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch reached unexpected childResult={} rawCode=0x{:02x}; mirrored fallback state0 switch and error=4",
        static_cast<unsigned>(childResult),
        static_cast<unsigned>(rawCode));
    return 1u;
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::GetStateId() const {
    return 2;
}

}  // namespace mxo::ltlogin
