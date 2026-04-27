#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x4b5014
const char* CLTLoginState_AuthenticatePending_0x4b5014::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x439210 / vtable 0x4b5014 slot 3
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
void CLTLoginState_AuthenticatePending_0x4b5014::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
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
        // Inline state switch
        g_CurrentLoginMediator->authAddressList4c_.Reset();
        g_CurrentLoginMediator->authConnectAttemptCount28_ = 0u;
        CLTLoginState* const upstreamState = g_CurrentLoginMediator->currentState_;
        const uint32_t result = g_CurrentLoginMediator->SetCurrentState(1u);
        (void)upstreamState; // suppress unused
        (void)result;
        spdlog::info(
            "CLTLoginState_AuthenticatePending_0x4b5014::Slot3_BeginOrContinue auth transport not ready -> SetCurrentState(1) result=0x{:08x}",
            static_cast<unsigned>(result));
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
        "CLTLoginState_AuthenticatePending_0x4b5014::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch=0x{:08x}",
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

// anchor: launcher.exe:0x43f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch(void* workItem) {
    if (!g_CurrentLoginMediator) {
        return 0u;
    }

    // Current best recovered role from `0x43f300`:
    // - it does not inspect staged raw auth bytes locally
    // - instead it forwards the incoming auth-message object straight into
    //   `0x448140 = AuthBootstrap680_HandleInboundAuthMessage`
    // - Ghidra currently places `0x448140` under namespace
    //   `CStreamPacketEncryptionModuleWriteHelper_0x4b8690`
    // - this state2 body only switches on that helper's return code to drive owner `+0x80`,
    //   helper-switch, and event/error flow
    const uint32_t childResult =
        g_CurrentLoginMediator->authBootstrapChild680_->HandleInboundAuthMessage(workItem, *g_CurrentLoginMediator);
    if (childResult == kAuthBootstrap680InboundUnhandled) {
        g_CurrentLoginMediator->worldListCountOrStatus80 = 0x12000004u;
        spdlog::info(
            "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch helper returned unhandled; mirrored original owner+0x80=0x12000004 and returned false-like");
        return 0u;
    }

    if (childResult == kAuthBootstrap680InboundHandledContinueWaiting) {
        spdlog::info(
            "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch helper consumed inbound auth message and remained in the early wait path currentState={}",
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

            // SOURCE-ONLY: diagnostic logging; no binary counterpart.
            AuthBootstrap680LogParsedAuthReply(*g_CurrentLoginMediator, g_CurrentLoginMediator->lastAuthReply_);

            if (AuthBootstrap680ConsumeState2AuthReplySuccessOneTimeGateScaffold()) {
                // anchor: launcher.exe:0x43f300 one-time gate body
                //
                // Binary ordering (verified against Ghidra decompilation of 0x43f300):
                // 1. World descriptor loop (inline at 0x43f300, fidelity: no function call)
                // 2. AuthBootstrap680_StoreField114AndTimestamp118 (field114/118)
                // 3. ResetSelectionRouteState + character count cap
                // 4. Character slot loop + route-host seeding (inline at 0x43f300, fidelity: no function call)
                // 5. PersistCharactersIni
                // 6. PostEvent(6)
                // 7. AuthBootstrap680_CopyReplyString54 + SetLaunchPadSourceBlock94FirstString
                // 8. AuthBootstrap680_CopyOpaqueReplyBlobs108_10c
                // anchor: launcher.exe:0x43f300 world-descriptor loop (inline, no function call)
                // FIDELITY: original binary has inline loops here, not function calls
                g_CurrentLoginMediator->worldSlots_.fill(nullptr);
                g_CurrentLoginMediator->worldPayloadSlots_.fill(nullptr);
                g_CurrentLoginMediator->worldDescriptorValidD84_.fill(false);
                g_CurrentLoginMediator->worldDescriptorCountD80_ = 0;
                {
                    const size_t worldCount = std::min(
                        g_CurrentLoginMediator->worldSlots_.size(),
                        g_CurrentLoginMediator->lastAuthReply_.worlds.size());
                    for (size_t i = 0; i < worldCount; ++i) {
                        g_CurrentLoginMediator->worldSlots_[i] =
                            const_cast<mxo::auth::AuthWorldEntry*>(&g_CurrentLoginMediator->lastAuthReply_.worlds[i]);
                        g_CurrentLoginMediator->worldPayloadSlots_[i] =
                            const_cast<mxo::auth::AuthWorldEntry*>(&g_CurrentLoginMediator->lastAuthReply_.worlds[i]);
                        g_CurrentLoginMediator->SeedRecoveredWorldDescriptorFromAuthReply(
                            static_cast<uint8_t>(i), g_CurrentLoginMediator->lastAuthReply_.worlds[i]);
                        ++g_CurrentLoginMediator->worldDescriptorCountD80_;
                    }
                }
                // anchor: launcher.exe:0x43f386-0x43f3a2 — world descriptor validation logging
                // Binary validates each world's status (offset 0x17) and type (offset 0x18)
                // For invalid values, it logs using LogRouter_FprintfCompatUsingTlsSourceLoc
                // and forces the field to 0 (invalid). This is inline, no function calls.
                for (size_t i = 0; i < g_CurrentLoginMediator->worldDescriptorCountD80_; ++i) {
                    mxo::auth::AuthWorldEntry& entry =
                        g_CurrentLoginMediator->lastAuthReply_.worlds[i];
                    // FIDELITY: binary checks status byte at offset 0x17, type byte at offset 0x18
                    // If invalid (==0), it logs "World %s (id = %d) has an invalid status/type!"
                    // and forces the field to 0. This is inline processing.
                    if (entry.status == 0u) {
                        std::string worldName = entry.worldName;
                        uint16_t worldId = entry.worldId;
                        spdlog::warn(
                            "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid status!  Forcing it to WORLDSTATUS_INVALID.",
                            worldName.c_str(), worldId);
                        entry.status = 0u;  // FIDELITY: binary forces field to 0
                    }
                    if (entry.type == 0u) {
                        std::string worldName = entry.worldName;
                        uint16_t worldId = entry.worldId;
                        spdlog::warn(
                            "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid type!  Forcing it to WORLDTYPE_INVALID.",
                            worldName.c_str(), worldId);
                        entry.type = 0u;  // FIDELITY: binary forces field to 0
                    }
                }
                AuthBootstrap680SyncState2AuthReplySuccessOneTime_Field114AndTimestamp(
                    *g_CurrentLoginMediator->authBootstrapChild680_,
                    g_CurrentLoginMediator->lastAuthReply_);
                // anchor: launcher.exe:0x43f300 character-slot loop + route-host string copy (inline, no function call)
                // FIDELITY: original binary has inline loops here, not function calls.
                // Also note what it does *not* do here: no post-auth source-block seeding,
                // no margin-route state backfill, and no local staged-packet/raw-code reads.
                {
                    g_CurrentLoginMediator->selectionRouteState684_.ResetSelectionRouteState();
                    const size_t characterCount = std::min(
                        g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_.size(),
                        g_CurrentLoginMediator->lastAuthReply_.characters.size());
                    g_CurrentLoginMediator->selectionRouteState684_.slotRecordCount00_ =
                        static_cast<uint8_t>(characterCount);
                    for (size_t i = 0; i < characterCount; ++i) {
                        g_CurrentLoginMediator->SeedRecoveredCharacterSlotRecordFromAuthReply(
                            static_cast<uint8_t>(i), g_CurrentLoginMediator->lastAuthReply_.characters[i]);
                        const SlotRecordState_0x4b5328& slotRecord =
                            g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_[i];
                        const int matchedWorldIndex =
                            g_CurrentLoginMediator->FindRecoveredWorldDescriptorIndexByWorldId(
                                slotRecord.worldId3c);
                        if (matchedWorldIndex >= 0) {
                            // anchor: launcher.exe:0x43f74a
                            g_CurrentLoginMediator->selectionRouteState684_.routeHostStringTriples194_[i]
                                .Assign(g_CurrentLoginMediator->worldDescriptorsD84_[static_cast<size_t>(
                                                                                      matchedWorldIndex)]
                                            .inlineNamePlus03);
                        }
                    }
                }
                // anchor: launcher.exe:0x43f3f4-0x43f410 — character slot validation logging
                // Binary validates each character's status (offset 0xb)
                // If invalid (==0), it logs "Character %s (gcid = ...) has an invalid status!"
                // and forces the field to 7 (AUTHDBCHARSTATUS_INVALID). This is inline.
                for (size_t i = 0; i < g_CurrentLoginMediator->selectionRouteState684_.slotRecordCount00_; ++i) {
                    SlotRecordState_0x4b5328& slotRecord =
                        g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_[i];
                    // FIDELITY: binary checks status byte at offset 0xb (status3a field)
                    // If invalid (==0), it logs "Character %s (gcid = ...) has an invalid status!"
                    // and forces the field to 7. This is inline processing.
                    if (slotRecord.debugString14 != nullptr && slotRecord.status3a == 0u) {
                        uint64_t gcid =
                            (static_cast<uint64_t>(slotRecord.characterIdHigh36) << 32) | slotRecord.characterIdLow32;
                        spdlog::warn(
                            "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): Character %s (gcid = %I64u) has an invalid status!  Forcing it to AUTHDBCHARSTATUS_INVALID.",
                            slotRecord.debugString14,
                            gcid);
                        slotRecord.status3a = 7u;  // FIDELITY: binary forces field to 7
                    }
                }
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
            // FIDELITY NOTE: The original binary at 0x43f300 ends here with SetCurrentState+PostEvent(5).
            // Margin connection initiation happens later in State4::Slot3_BeginOrContinue (0x439300),
            // not in the auth-reply success path.
            const uint32_t switchDispatchResult =
                g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);
            g_CurrentLoginMediator->PostEvent(5u);

            spdlog::info(
                "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch adopted early auth-reply success owner+0x80=0x{:08x} cachedUpstream={} -> nextHelperState=0x{:02x} currentState={} switchDispatchResult=0x{:08x} event=0x05",
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80),
                fmt::ptr(cachedUpstreamOrArg_0x4),
                static_cast<unsigned>(nextHelperStateId),
                g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
                static_cast<unsigned>(switchDispatchResult));
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
                "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch observed early raw-0x07 failure childResult={} owner+0x80=0x{:08x}; mirrored original state0 switch and error=2",
                static_cast<unsigned>(childResult),
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80));
            return 1u;
        }

        case kAuthBootstrap680InboundAuthReplyValidationError: {
            g_CurrentLoginMediator->worldListCountOrStatus80 = 0x1200000bu;
            (void)g_CurrentLoginMediator->SetCurrentState(0u);
            g_CurrentLoginMediator->PostError(0x0fu);
            spdlog::info(
                "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch rejected early raw-0x0b success-side adoption owner+0x80=0x{:08x}; mirrored original state0 switch and error=0x0f",
                static_cast<unsigned>(g_CurrentLoginMediator->worldListCountOrStatus80));
            return 1u;
        }

        default:
            break;
    }

    (void)g_CurrentLoginMediator->SetCurrentState(0u);
    g_CurrentLoginMediator->PostError(4u);
    spdlog::warn(
        "CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch reached unexpected childResult={}; mirrored fallback state0 switch and error=4",
        static_cast<unsigned>(childResult));
    return 1u;
}

// anchor: launcher.exe:0x418150 (vtable 0x4b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending_0x4b5014::GetStateId() const {
    return 2;
}

}  // namespace mxo::ltlogin
