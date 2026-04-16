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
void CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
        return;
    }

    const uint32_t incomingUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
    if (incomingUpstreamPhaseCode != 1u) {
        cachedUpstreamOrArg_0x4 = upstreamOrArg;
    }

    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    if (!mediator->HasReadyAuthConnectionState2()) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state2 -> state1 auth-connect incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_0x4),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        const uint32_t connectResult = mediator->BeginAuthConnectionViaState1Scaffold();
        spdlog::info(
            "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue auth transport not ready incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} -> BeginAuthConnectionViaState1Scaffold=0x{:08x}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_0x4),
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
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(cachedUpstreamPhaseCode));
    const uint32_t sendResult = child->PrepareAndDispatch(*mediator, sendTarget, sessionToken);
    spdlog::info(
        "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch=0x{:08x}",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_0x4),
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
            // anchor: launcher.exe:0x43f300 case 2 — pre-gate setup
            // Binary order: PregateScaffold (field110+stringF8), then one-time gate body,
            // then phase-code dispatch + PostEvent(5).
            AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
                *mediator->authBootstrapChild680_,
                *mediator,
                mediator->lastAuthReply_);
            // REMOVED for fidelity: original at 0x43f300 does NOT call ResetMarginBootstrapState or
            // RecoverAuthReplyPrivateExponentIntoMarginBootstrapState in auth-reply success path.
            // Original does inline recovery during margin CERT_Challenge (via margin +0xa0 bootstrap object).
            // See ContinueMarginBootstrapHandshake for lazy inline recovery implementation.

            // SOURCE-ONLY: diagnostic logging; no binary counterpart.
            AuthBootstrap680LogParsedAuthReply(*mediator, mediator->lastAuthReply_);

            if (AuthBootstrap680ConsumeState2AuthReplySuccessOneTimeGateScaffold()) {
                // anchor: launcher.exe:0x43f300 one-time gate body
                //
                // Binary ordering (verified against Ghidra decompilation of 0x43f300):
                // 1. World descriptor loop (AdoptAuthReply_WorldDescriptors)
                // 2. AuthBootstrap680_StoreField114AndTimestamp118 (field114/118)
                // 3. ResetSelectionRouteState + character count cap
                // 4. Character slot loop + route-host seeding (AdoptAuthReply_CharacterSlotRecords)
                // 5. PersistCharactersIni
                // 6. PostEvent(6)
                // 7. AuthBootstrap680_CopyReplyString54 + SetLaunchPadSourceBlock94FirstString
                // 8. AuthBootstrap680_CopyOpaqueReplyBlobs108_10c
                CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorState_WorldDescriptors(mediator);
                AuthBootstrap680SyncState2AuthReplySuccessOneTime_Field114AndTimestamp(
                    *mediator->authBootstrapChild680_,
                    mediator->lastAuthReply_);
                CLTLoginState_State10::AdoptAuthReplyIntoRecoveredMediatorState_CharacterSlotRecords(mediator);
                mediator->PersistCharactersIniFromRecoveredAuthStateScaffold();
                mediator->PostEvent(6u);
                AuthBootstrap680SyncState2AuthReplySuccessOneTime_ReplyStringAndOpaqueBlobs(
                    *mediator->authBootstrapChild680_,
                    *mediator,
                    mediator->lastAuthReply_);
            }

            // SOURCE-ONLY: expectedMarginRequestName_ has no binary counterpart in 0x43f300.
            mediator->expectedMarginRequestName_ = "CERT_ConnectRequest";

            // The binary dispatches this->field_0x4->GetStateId() up to 3 times here
            // (confirmed by improved Ghidra types: all three calls are
            // `local_1c->field4_0x4->vftptr_0x0->GetStateId()`).
            // This is a compiler code-gen artifact (register pressure causes redundant
            // re-evaluation), not a decompiler misinterpretation. Since all three calls
            // target the same virtual on the same object with no mutation between them,
            // the else branch (call 3) returns the same value already held, and the
            // source's single-call simplification is semantically equivalent:
            //   binary: if (X==0 || X==0x10) iVar6=3 else iVar6=X (via call 3)
            //   source: if (X==0 || X==0x10) nextHelper=3 else nextHelper=X (already held)
            // field_0x4 = cachedUpstreamOrArg_0x4 = the upstream state pointer set in
            // Slot3_BeginOrContinue when pUpstreamState->GetStateId() != 1.
            uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
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

            // SOURCE-ONLY: post-AS_AuthReply margin auto-begin. No binary counterpart in
            // 0x43f300 — the original returns immediately after SetCurrentState+PostEvent(5).
            // Added for source-side convenience to automatically kick off the margin connection
            // after a successful auth reply.
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
                fmt::ptr(cachedUpstreamOrArg_0x4),
                static_cast<unsigned>(nextHelperStateId),
                mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
                static_cast<unsigned>(switchDispatchResult),
                triggeredMarginAutoBegin ? 1u : 0u,
                deferredMarginAutoBeginToState8 ? 1u : 0u,
                static_cast<unsigned>(marginAutoBeginResult));
            return 1u;
        }

        // NOTE: kAuthBootstrap680InboundAuthReplyError (3) has no explicit case in the binary.
        // The binary's switch at 0x43f300 only has cases 2, 4, 5, 6, and default. Child result
        // value 3 falls into default, which does SetCurrentState(0) + PostError(4) — the same
        // behavior as the former explicit case below. Removed the explicit case to match the
        // binary; source-only additions (expectedMarginRequestName_=nullptr, LogParsedAuthReply)
        // that were on this case are dropped since the binary doesn't distinguish it.

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
