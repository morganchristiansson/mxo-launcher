#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// Phase A of the auth-reply adoption: build +0xd84 world descriptors.
// anchor: launcher.exe:0x43f300 (world-descriptor loop in the one-time gate body)
//
// The original at 0x43f300 builds world descriptors first, then writes field114/118,
// then resets selection-route state and builds character slot records. This phase A
// covers only the world-descriptor portion so the caller can interleave field114/118
// between phases A and B to match binary ordering.
void AdoptAuthReplyIntoRecoveredMediatorState_WorldDescriptors(CLTLoginMediator* mediator) {
    if (!mediator) {
        return;
    }

    // Address anchors:
    // - launcher.exe:0x43f300 broader auth-reply writer
    // - launcher.exe:0x441260 / 0x441330 narrower auth-reply adoption helpers nearby in the same
    //   broader early-auth corridor
    // Keep this helper scoped to the broader auth-reply adoption used by state2/current existing-
    // character auth bridges.
    // Important create/delete correction from the latest static pass:
    // - `0x4401a0` is not an auth-reply adopter
    // - it is the later margin-side `MS_ClaimCharacterNameReply` append helper for state10 slot 6
    // - so do not treat this broader auth-table rebuild as the owner-side body for `0x4401a0`
    mediator->worldSlots_.fill(nullptr);
    mediator->worldPayloadSlots_.fill(nullptr);
    mediator->worldDescriptorValidD84_.fill(false);
    mediator->worldDescriptorCountD80_ = 0;

    const size_t worldCount = std::min(mediator->worldSlots_.size(), mediator->lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        mediator->worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&mediator->lastAuthReply_.worlds[i]);
        mediator->SeedRecoveredWorldDescriptorFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.worlds[i]);
        ++mediator->worldDescriptorCountD80_;
    }
}

// Phase B of the auth-reply adoption: reset selection-route state, cap +0x684 slot count to <=100,
// populate +0x688 slot records, seed +0x818 route-host strings by matching slot worldId
// against descriptor worldId.
// anchor: launcher.exe:0x43f300 (character-slot loop + route-host seeding in the one-time gate body)
//
// NOTE: the original at 0x43f300 does NOT write back to owner +0x80 inside the one-time gate body;
// +0x80 was already set from authBootstrapChild680.inboundAuthStatusEc before the switch.
// The former monolithic AdoptAuthReplyIntoRecoveredMediatorStateScaffold incorrectly wrote
// worldListCountOrStatus80 = worlds.size() here; that write has been removed.
void AdoptAuthReplyIntoRecoveredMediatorState_CharacterSlotRecords(CLTLoginMediator* mediator) {
    if (!mediator) {
        return;
    }

    const size_t worldCount = std::min(mediator->worldSlots_.size(), mediator->lastAuthReply_.worlds.size());

    mediator->selectionRouteState684_.ResetSelectionRouteState();

    const size_t characterCount = std::min(
        mediator->selectionRouteState684_.slotRecordTable04_.size(),
        mediator->lastAuthReply_.characters.size());
    mediator->selectionRouteState684_.slotRecordCount00_ = static_cast<uint8_t>(characterCount);
    for (size_t i = 0; i < characterCount; ++i) {
        mediator->SeedRecoveredCharacterSlotRecordFromAuthReply(static_cast<uint8_t>(i), mediator->lastAuthReply_.characters[i]);
        const SlotRecordState_0x4b5328& slotRecord = mediator->selectionRouteState684_.slotRecordTable04_[i];
        const int matchedWorldIndex = mediator->FindRecoveredWorldDescriptorIndexByWorldId(slotRecord.worldId0c);
        if (matchedWorldIndex >= 0) {
            // anchor: launcher.exe:0x43f74a
            // Original joins the just-built slot-record world id against the earlier +0xd84 table,
            // then copies the descriptor inline name into +0x818[currentCharacterIndex].
            mediator->selectionRouteState684_.routeHostStringTriples194_[i].Assign(
                mediator->worldDescriptorsD84_[static_cast<size_t>(matchedWorldIndex)].inlineNamePlus03);
        }
    }

    // NOTE: the binary does NOT write owner +0x80 inside the one-time gate body. It was already
    // set from inboundAuthStatusEc before the switch. Do not add a worldListCountOrStatus80
    // writeback here.

    if (characterCount != 0) {
        // Replacement-side mirror only:
        // - broader `0x43f300` resets `+0xcc8` through `0x41d270` and does not show a direct
        //   rewrite of the current-slot byte during the one-time auth adoption body
        // - current source still seeds slot 0 here so the non-GUI launcher path keeps a concrete
        //   current-slot mirror after the original table rebuild
        mediator->SetCurrentCharacterRouteIndexCc8Scaffold(0u);
    }

    if (characterCount != 0) {
        const SlotRecordState_0x4b5328& currentSlotRecord =
            mediator->selectionRouteState684_.slotRecordTable04_[0];
        mediator->marginRouteState_.pendingWorldId = currentSlotRecord.worldId0c;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(currentSlotRecord.worldId0c);
    } else if (worldCount != 0) {
        const mxo::auth::AuthWorldEntry& firstWorld = mediator->lastAuthReply_.worlds[0];
        mediator->marginRouteState_.pendingWorldId = firstWorld.worldId;
        mediator->marginRouteState_.currentWorldId = static_cast<int32_t>(firstWorld.worldId);
    }

    if (const char* routeHostPrefix =
            mediator->LookupRouteHostPrefixBySlot(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)) {
        mediator->marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        mediator->marginRouteState_.routeHostPrefix.clear();
    }

    mediator->SeedPostAuthSourceBlockFromRecoveredAuthStateIfUnset();

    const char* currentDescriptorName = "<empty>";
    if (characterCount != 0) {
        const int matchedWorldIndex =
            mediator->FindRecoveredWorldDescriptorIndexByWorldId(
                mediator->selectionRouteState684_.slotRecordTable04_[0].worldId0c);
        if (matchedWorldIndex >= 0) {
            if (const char* name = mediator->GetDescriptorInlineNameByIndex(static_cast<uint8_t>(matchedWorldIndex))) {
                currentDescriptorName = name;
            }
        }
    } else if (worldCount != 0) {
        if (const char* name = mediator->GetDescriptorInlineNameByIndex(0)) {
            currentDescriptorName = name;
        }
    }

    spdlog::info(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount={} characterCount={} currentCharacterOrRouteIndex={} currentSlotWorldId={} routeHostPrefix='{}' slotRecordHeapString='{}' currentWorldDescriptorName='{}'",
        static_cast<unsigned>(worldCount),
        static_cast<unsigned>(characterCount),
        static_cast<unsigned>(mediator->marginRouteState_.currentCharacterOrRouteIndex),
        characterCount == 0
            ? 0u
            : static_cast<unsigned>(mediator->selectionRouteState684_.slotRecordTable04_[0].worldId0c),
        mediator->marginRouteState_.routeHostPrefix.empty() ? "<empty>" : mediator->marginRouteState_.routeHostPrefix.c_str(),
        mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)
            ? mediator->LookupSlotRecordHeapStringByIndex(mediator->postAuthMarginLoadingState_0xf14.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}

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
    if (!g_CurrentLoginMediator) {
        return;
    }

    const uint32_t incomingUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
    if (incomingUpstreamPhaseCode != 1u) {
        cachedUpstreamOrArg_0x4 = upstreamOrArg;
    }

    const uint32_t cachedUpstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    if (!g_CurrentLoginMediator->HasReadyAuthConnectionState2()) {
        spdlog::info(
            "ROUTE CHECKPOINT: early-auth state2 -> state1 auth-connect incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_0x4),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        const uint32_t connectResult = g_CurrentLoginMediator->BeginAuthConnectionViaState1Scaffold();
        spdlog::info(
            "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue auth transport not ready incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} -> BeginAuthConnectionViaState1Scaffold=0x{:08x}",
            fmt::ptr(upstreamOrArg),
            static_cast<unsigned>(incomingUpstreamPhaseCode),
            fmt::ptr(cachedUpstreamOrArg_0x4),
            static_cast<unsigned>(cachedUpstreamPhaseCode),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
            static_cast<unsigned>(connectResult));
        return;
    }

    // Ready-side handoff: direct field access mirrors assembly:
    //   call dword ptr [EDX + 0x168] -> GetAuthConnection returns authConnection_ at owner+0x18
    //   call dword ptr [EAX + 0x20] -> GetNoPatchLauncherVersionValuePtr
    //   call dword ptr [EDX + 0x38] -> GetUsername returns char* to owner+0x94 (username00)
    //   mov ECX, dword ptr [EAX + 0x60] -> session token at owner+0xf4
    auto* child = g_CurrentLoginMediator->authBootstrapChild680_.get();
    // Direct field access: owner+0x18 (authConnection_) instead of accessor
    void* sendTarget = g_CurrentLoginMediator->authConnection_;
    // Direct offset access: owner+0x94 + 0x60 = ownerAuthBootstrapSource94_.sessionToken60.begin
    const char* sessionToken = *reinterpret_cast<const char**>(
        reinterpret_cast<uint8_t*>(&g_CurrentLoginMediator->ownerAuthBootstrapSource94_) + 0x60);

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth state2 ready-side owner+0x680 bootstrap-child dispatch currentState={} cachedUpstream={} cachedUpstreamPhaseCode={} (static 0x439210 ready branch feeds 0x448050)",
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(cachedUpstreamPhaseCode));
    const uint32_t sendResult = child->PrepareAndDispatch(*g_CurrentLoginMediator, sendTarget, sessionToken);
    spdlog::info(
        "CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch=0x{:08x}",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        g_CurrentLoginMediator->HasReadyAuthConnectionState2() ? 1u : 0u,
        static_cast<unsigned>(sendResult));
    // Ghidra shows original returns void - return value is effectively ignored by callers
    return;
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem) {
    if (!g_CurrentLoginMediator) {
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
        g_CurrentLoginMediator->authBootstrapChild680_->HandleInboundAuthMessage(workItem, *g_CurrentLoginMediator);
    const std::vector<uint8_t>& stagedBytes = g_CurrentLoginMediator->stagedIncomingAuthPacketBytes_;
    const uint8_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    if (childResult == kAuthBootstrap680InboundUnhandled) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000004u;
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
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    g_CurrentLoginMediator->worldListCountOrStatus80 = g_CurrentLoginMediator->authBootstrapChild680_->inboundAuthStatusEc;
    // anchor: launcher.exe:0x43f300 / sets authConnectionFlag2c_ = 1 when childResult != 0 && childResult != 1
    g_CurrentLoginMediator->authConnectionFlag2c_ = 1u;

    switch (childResult) {
        case kAuthBootstrap680InboundAuthReplySuccess: {
            // anchor: launcher.exe:0x43f300 case 2 — pre-gate setup
            // Binary order: PregateScaffold (field110+stringF8), then one-time gate body,
            // then phase-code dispatch + PostEvent(5).
            AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
                *g_CurrentLoginMediator->authBootstrapChild680_,
                *g_CurrentLoginMediator,
                g_CurrentLoginMediator->lastAuthReply_);
            // REMOVED for fidelity: original at 0x43f300 does NOT call ResetMarginBootstrapState or
            // RecoverAuthReplyPrivateExponentIntoMarginBootstrapState in auth-reply success path.
            // Original does inline recovery during margin CERT_Challenge (via margin +0xa0 bootstrap object).
            // See ContinueMarginBootstrapHandshake for lazy inline recovery implementation.

            // SOURCE-ONLY: diagnostic logging; no binary counterpart.
            AuthBootstrap680LogParsedAuthReply(*g_CurrentLoginMediator, g_CurrentLoginMediator->lastAuthReply_);

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
                AdoptAuthReplyIntoRecoveredMediatorState_WorldDescriptors(g_CurrentLoginMediator);
                AuthBootstrap680SyncState2AuthReplySuccessOneTime_Field114AndTimestamp(
                    *g_CurrentLoginMediator->authBootstrapChild680_,
                    g_CurrentLoginMediator->lastAuthReply_);
                AdoptAuthReplyIntoRecoveredMediatorState_CharacterSlotRecords(g_CurrentLoginMediator);
                g_CurrentLoginMediator->PersistCharactersIniFromRecoveredAuthStateScaffold();
                g_CurrentLoginMediator->PostEvent(6u);
                AuthBootstrap680SyncState2AuthReplySuccessOneTime_ReplyStringAndOpaqueBlobs(
                    *g_CurrentLoginMediator->authBootstrapChild680_,
                    *g_CurrentLoginMediator,
                    g_CurrentLoginMediator->lastAuthReply_);
            }

            // SOURCE-ONLY: expectedMarginRequestName_ has no binary counterpart in 0x43f300.
            g_CurrentLoginMediator->expectedMarginRequestName_ = "CERT_ConnectRequest";

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
                g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);
            g_CurrentLoginMediator->PostEvent(5u);

            // SOURCE-ONLY: post-AS_AuthReply margin auto-begin. No binary counterpart in
            // 0x43f300 — the original returns immediately after SetCurrentState+PostEvent(5).
            // Added for source-side convenience to automatically kick off the margin connection
            // after a successful auth reply.
            uint32_t marginAutoBeginResult = 0u;
            bool triggeredMarginAutoBegin = false;
            bool deferredMarginAutoBeginToState8 = false;
            if (!g_CurrentLoginMediator->postAuthMarginAutoBeginAttemptedScaffold_) {
                const uint32_t currentHelperPhaseCode =
                    g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DispatchPhaseCode() : 0u;

                // Existing-character continuation correction:
                // - starting the margin connect while we are still on state3 leaves helper4's
                //   cached upstream aligned to the old state3 wait leaf
                // - on the natural existing-character path the first meaningful state4
                //   margin-connect entry for this continuation is the later
                //   `+0xec -> state8 slot3 -> helper4` handoff during "Loading Character"
                if (currentHelperPhaseCode == 3u) {
                    deferredMarginAutoBeginToState8 = true;
                } else {
                    g_CurrentLoginMediator->postAuthMarginAutoBeginAttemptedScaffold_ = true;
                    triggeredMarginAutoBegin = true;
                    marginAutoBeginResult = g_CurrentLoginMediator->BeginLauncherMarginConnectionScaffold();
                }
            }

            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch adopted early auth-reply success rawCode=0x{:02x} owner+0x80=0x{:08x} cachedUpstream={} -> nextHelperState=0x{:02x} currentState={} switchDispatchResult=0x{:08x} event=0x05 triggeredMarginAutoBegin={} deferredMarginAutoBeginToState8={} marginAutoBeginResult=0x{:08x}",
                static_cast<unsigned>(rawCode),
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80),
                fmt::ptr(cachedUpstreamOrArg_0x4),
                static_cast<unsigned>(nextHelperStateId),
                g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
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
            (void)g_CurrentLoginMediator->SetCurrentState(0u);
            g_CurrentLoginMediator->PostError(2u);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch observed early raw-0x07 failure childResult={} owner+0x80=0x{:08x}; mirrored original state0 switch and error=2",
                static_cast<unsigned>(childResult),
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyValidationError: {
            g_CurrentLoginMediator->worldListCountOrStatus80 = 0x1200000bu;
            (void)g_CurrentLoginMediator->SetCurrentState(0u);
            g_CurrentLoginMediator->PostError(0x0fu);
            spdlog::info(
                "CLTLoginState_AuthenticatePending::AuthMessageDispatch rejected early raw-0x0b success-side adoption owner+0x80=0x{:08x}; mirrored original state0 switch and error=0x0f",
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80));
            return 1u;
        }

        default:
            break;
    }

    (void)g_CurrentLoginMediator->SetCurrentState(0u);
    g_CurrentLoginMediator->PostError(4u);
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
