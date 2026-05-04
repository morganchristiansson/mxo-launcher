#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

static uint16_t ReadU16LEState2(const uint8_t* bytes) {
    if (!bytes) {
        return 0u;
    }
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8u);
}

static uint32_t ReadU32LEState2(const uint8_t* bytes) {
    if (!bytes) {
        return 0u;
    }
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) |
           (static_cast<uint32_t>(bytes[3]) << 24u);
}

static const uint8_t* AuthBootstrap680SelectWorldTempRecordByIndex(
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject,
    size_t index) {
    if (!parseObject || !parseObject->worldTempRecords44 ||
        index >= parseObject->worldTempRecordCount48) {
        return nullptr;
    }

    const uint8_t* const worldRecord = parseObject->worldTempRecords44 + index * 0x20u;
    parseObject->currentWorldTempRecord6c = worldRecord;
    if (worldRecord != nullptr) {
        const_cast<uint8_t*>(worldRecord)[0x16u] = 0u;
    }
    return worldRecord;
}

static Packet_WorldList_0x4b533c* AuthBootstrap680InitializeWorldDescriptorFromSelectedTempRecord(
    Packet_WorldList_0x4b533c* descriptor,
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject) {
    if (!descriptor || !parseObject || !parseObject->currentWorldTempRecord6c) {
        return nullptr;
    }

    descriptor->payloadPtr04 = reinterpret_cast<uint32_t>(parseObject->worldDescriptorAccessor5c.packetBody04);
    descriptor->messageRef08 =
        static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(
            parseObject->worldDescriptorAccessor5c.incomingMessage08);
    if (descriptor->messageRef08) {
        descriptor->messageRef08->AddRef();
    }
    descriptor->createRefParam0c = parseObject->worldDescriptorAccessor5c.resolveFields0c;
    descriptor->payloadAlias10 = const_cast<uint8_t*>(parseObject->currentWorldTempRecord6c);

    const uint8_t* const worldRecord = parseObject->currentWorldTempRecord6c;
    descriptor->worldId01 = ReadU16LEState2(worldRecord + 0x01u);
    descriptor->inlineNamePlus03 = reinterpret_cast<const char*>(worldRecord + 0x03u);
    descriptor->status17 = worldRecord[0x17u];
    descriptor->type18 = worldRecord[0x18u];
    descriptor->serverVersion19 = ReadU32LEState2(worldRecord + 0x19u);
    descriptor->serverLanguage1d = worldRecord[0x1du];
    descriptor->privateFlag1e = worldRecord[0x1eu];
    descriptor->populationLevel1f = worldRecord[0x1fu];
    return descriptor;
}

static const uint8_t* AuthBootstrap680SelectCharacterTempRecordByIndex(
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject,
    size_t index,
    const char** outHandleText,
    uint16_t* outHandleLength) {
    if (outHandleText) {
        *outHandleText = "";
    }
    if (outHandleLength) {
        *outHandleLength = 0u;
    }
    if (!parseObject || !parseObject->characterTempRecords3c ||
        index >= parseObject->characterTempRecordCount40) {
        return nullptr;
    }

    const uint8_t* const record = parseObject->characterTempRecords3c + index * 0x0eu;
    parseObject->currentCharacterTempRecord80 = record;
    const uint16_t handleOffset = ReadU16LEState2(record + 1u);
    if (handleOffset != 0u) {
        uint8_t* const mutableHandleLengthBytes = const_cast<uint8_t*>(record + handleOffset);
        const uint16_t handleLength = ReadU16LEState2(mutableHandleLengthBytes);
        char* const handleText = reinterpret_cast<char*>(mutableHandleLengthBytes + 2u);
        parseObject->currentCharacterHandle84 = reinterpret_cast<const uint8_t*>(handleText);
        parseObject->currentCharacterHandleByteLength88 = handleLength;
        if (handleLength != 0u) {
            handleText[handleLength - 1u] = '\0';
        }
        if (outHandleText) {
            *outHandleText = handleText;
        }
        if (outHandleLength) {
            *outHandleLength = handleLength;
        }
    } else {
        parseObject->currentCharacterHandle84 = nullptr;
        parseObject->currentCharacterHandleByteLength88 = 0u;
    }
    return record;
}

static Packet_AsAuthReply_0x4b5328* AuthBootstrap680InitializeSlotRecordFromSelectedTempRecord(
    Packet_AsAuthReply_0x4b5328* slotRecord,
    Packet_AsGetPublicKeyRequest_0x4b6c74* parseObject) {
    if (!slotRecord || !parseObject || !parseObject->currentCharacterTempRecord80) {
        return nullptr;
    }

    slotRecord->payloadPtr04 = reinterpret_cast<uint32_t>(parseObject->slotRecordAccessor70.packetBody04);
    slotRecord->messageRef08 =
        static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(
            parseObject->slotRecordAccessor70.incomingMessage08);
    if (slotRecord->messageRef08) {
        slotRecord->messageRef08->AddRef();
    }
    slotRecord->createRefParam0c = parseObject->slotRecordAccessor70.resolveFields0c;
    slotRecord->payloadAlias10 = const_cast<uint8_t*>(parseObject->currentCharacterTempRecord80);
    slotRecord->debugString14 = reinterpret_cast<const char*>(parseObject->currentCharacterHandle84);
    slotRecord->payloadSize18 = parseObject->currentCharacterHandleByteLength88;

    const uint8_t* const record = parseObject->currentCharacterTempRecord80;
    if (slotRecord->payloadAlias10 != nullptr) {
        uint8_t* const slotPayload = static_cast<uint8_t*>(slotRecord->payloadAlias10);
        *reinterpret_cast<uint32_t*>(slotPayload + 0x03u) = ReadU32LEState2(record + 0x03u);
        *reinterpret_cast<uint32_t*>(slotPayload + 0x07u) = ReadU32LEState2(record + 0x07u);
        slotPayload[0x0bu] = record[0x0bu];
        *reinterpret_cast<uint16_t*>(slotPayload + 0x0cu) = ReadU16LEState2(record + 0x0cu);
    }
    return slotRecord;
}

}  // namespace


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
    // owner+0x94 + 0x60 is the old-MSVC2003 `std::string` member inside
    // `OwnerAuthBootstrapSource94_0x41eb80`; use its `c_str()` surface instead of a fake raw
    // three-pointer view here.
    const char* sessionToken =
        StringBeginOrNull(g_CurrentLoginMediator->ownerAuthBootstrapSource94_.sessionToken60);

    spdlog::info(
        "ROUTE CHECKPOINT: early-auth state2 ready-side owner+0x680 bootstrap-child dispatch currentState={} cachedUpstream={} cachedUpstreamPhaseCode={} (static 0x439210 ready branch feeds 0x448050)",
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(cachedUpstreamPhaseCode));
    child->PrepareAndDispatch(*g_CurrentLoginMediator, sendTarget, sessionToken);
    spdlog::info(
        "CLTLoginState_AuthenticatePending_0x4b5014::Slot3_BeginOrContinue incomingUpstream={} incomingUpstreamPhaseCode={} cachedUpstream={} cachedUpstreamPhaseCode={} currentState={} authReadyState2={} -> owner+0x680::PrepareAndDispatch()",
        fmt::ptr(upstreamOrArg),
        static_cast<unsigned>(incomingUpstreamPhaseCode),
        fmt::ptr(cachedUpstreamOrArg_0x4),
        static_cast<unsigned>(cachedUpstreamPhaseCode),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>",
        g_CurrentLoginMediator->HasReadyAuthConnectionState2() ? 1u : 0u);
    // Ghidra shows original returns void - return value is effectively ignored by callers
    return;
}

// anchor: launcher.exe:0x43f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending_0x4b5014::AuthMessageDispatch(void* workItem) {
    // Static RE note: `0x43f300` immediately dereferences the global mediator and does not
    // guard `g_CurrentLoginMediator` for null before entering the child dispatch.
    // Keep the source shaped the same instead of adding a protective early return here.

    // Current best recovered role from `0x43f300`:
    // - it does not inspect staged raw auth bytes locally
    // - instead it forwards the incoming auth-message object straight into
    //   `0x448140 = AuthBootstrap680Child_0x441290::HandleInboundAuthMessage`
    // - the direct callsite loads ECX from owner `+0x680`, so this belongs to the concrete auth
    //   bootstrap child rather than the inherited stream-packet write-helper base
    // - this state2 body only switches on that child method's return code to drive owner `+0x80`,
    //   helper-switch, and event/error flow
    const uint32_t childResult =
        g_CurrentLoginMediator->authBootstrapChild680_->HandleInboundAuthMessage(
            static_cast<const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(workItem));
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

    g_CurrentLoginMediator->worldListCountOrStatus80 =
        AuthBootstrapChildFromWriteHelper(*g_CurrentLoginMediator->authBootstrapChild680_)
            .inboundAuthStatusEc;
    // anchor: launcher.exe:0x43f300 / sets authConnectionFlag2c_ = 1 when childResult != 0 && childResult != 1
    g_CurrentLoginMediator->authConnectionFlag2c_ = 1u;

    switch (childResult) {
        case kAuthBootstrap680InboundAuthReplySuccess: {
            // anchor: launcher.exe:0x43f300 case 2 — pre-gate setup
            // Binary order at the top of case 2:
            // 1. child `+0x110` = parsed success-header dword at header offset `0x07`
            // 2. inline prompt-password / SecurID mirror update at `0x441330`
            // 3. test global `DAT_004f79e0`
            // 4. if clear, set `DAT_004f79e0 = 1` and run the once-only writeback body
            auto& authBootstrapChild =
                AuthBootstrapChildFromWriteHelper(*g_CurrentLoginMediator->authBootstrapChild680_);
            const Packet_AsGetPublicKeyRequest_0x4b6c74* const parseObject =
                authBootstrapChild.authReplyParseObjectF0;
            authBootstrapChild.authReplySuccessHeaderDword07_110 =
                parseObject != nullptr && parseObject->ReplyHeader10() != nullptr
                    ? ReadU32LEState2(parseObject->ReplyHeader10() + 0x07u)
                    : 0u;

            const char* const passwordText =
                g_CurrentLoginMediator->ownerAuthBootstrapSource94_.password20.data();
            authBootstrapChild.SetPromptPasswordF8AndSecurIdFlag(passwordText);
            spdlog::info(
                "AuthBootstrap680SetPromptPasswordF8AndSecurIdFlag childStringF8Len={} promptForSecurId={}",
                static_cast<unsigned>(authBootstrapChild.stringF8.size()),
                static_cast<unsigned>(authBootstrapChild.crashReporterPromptForSecurId104));

            if (!g_authBootstrap680State2AuthReplySuccessOneTimeGate) {
                g_authBootstrap680State2AuthReplySuccessOneTimeGate = true;
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
                // 8. inline opaque-blob pointer adoption
                // anchor: launcher.exe:0x43f300 world-descriptor loop (inline, no function call)
                // Binary shape:
                // - iterates the parsed success-object world table (`this_01->mbr_0x44/0x48`)
                // - materializes owner `+0xd84`
                // - validates status/type inline with `AuthReplyWorldStatus_IsValid` /
                //   `AuthReplyWorldType_IsValid`
                //
                // Source now follows the original helper split more closely:
                // - select `currentWorldTempRecord6c` from the shared raw0x0b parse packet
                // - materialize each owner `+0xd84` entry as a `Packet_WorldList_0x4b533c`
                //   initialized from the parse object's world-descriptor accessor family
                for (Packet_WorldList_0x4b533c*& packet : g_CurrentLoginMediator->worldListPacketsD84_) {
                    delete packet;
                    packet = nullptr;
                }
                g_CurrentLoginMediator->worldListPacketCountD80_ = 0;
                {
                    const size_t worldCount = std::min(
                        g_CurrentLoginMediator->worldListPacketsD84_.size(),
                        parseObject != nullptr
                            ? static_cast<size_t>(parseObject->worldTempRecordCount48)
                            : 0u);
                    for (size_t i = 0; i < worldCount; ++i) {
                        const uint8_t* const worldRecord =
                            AuthBootstrap680SelectWorldTempRecordByIndex(
                                const_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(parseObject),
                                i);
                        if (!worldRecord) {
                            break;
                        }
                        const uint16_t worldId = ReadU16LEState2(worldRecord + 0x01u);
                        const char* const worldName = reinterpret_cast<const char*>(worldRecord + 0x03u);
                        const uint8_t rawStatus = worldRecord[0x17u];
                        const uint8_t rawType = worldRecord[0x18u];
                        const uint8_t normalizedStatus = (rawStatus != 0u && rawStatus < 6u)
                                                            ? rawStatus
                                                            : 0u;
                        const uint8_t normalizedType = (rawType != 0u && rawType < 4u)
                                                           ? rawType
                                                           : 0u;

                        if (normalizedStatus != rawStatus) {
                            spdlog::info(
                                CLTLoginState_AuthenticatePending_0x4b5014::kLogInvalidWorldStatus,
                                worldName,
                                static_cast<unsigned>(worldId),
                                static_cast<unsigned>(rawStatus));
                        }
                        if (normalizedType != rawType) {
                            spdlog::info(
                                CLTLoginState_AuthenticatePending_0x4b5014::kLogInvalidWorldType,
                                worldName,
                                static_cast<unsigned>(worldId),
                                static_cast<unsigned>(rawType));
                        }

                        Packet_WorldList_0x4b533c* descriptor = new Packet_WorldList_0x4b533c();
                        if (!descriptor) {
                            break;
                        }
                        if (!AuthBootstrap680InitializeWorldDescriptorFromSelectedTempRecord(
                                descriptor,
                                const_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(parseObject))) {
                            delete descriptor;
                            break;
                        }
                        g_CurrentLoginMediator->worldListPacketsD84_[i] = descriptor;
                        descriptor->status17 = normalizedStatus;
                        descriptor->type18 = normalizedType;
                        ++g_CurrentLoginMediator->worldListPacketCountD80_;
                    }
                }
                {
                    const Packet_AsGetPublicKeyRequest_0x4b6c74* const parseObject2 =
                        authBootstrapChild.authReplyParseObjectF0;
                    const uint32_t field114Value =
                        parseObject2 != nullptr && parseObject2->ReplyHeader10() != nullptr
                            ? ReadU32LEState2(parseObject2->ReplyHeader10() + 0x15u)
                            : 0u;
                    authBootstrapChild.StoreField114AndTimestamp118(field114Value);
                    spdlog::info(
                        "AuthBootstrap680SyncState2AuthReplySuccessOneTime_Field114AndTimestamp childField114=0x{:08x} childField118=0x{:08x}",
                        static_cast<unsigned>(authBootstrapChild.authReplySuccessField15_114),
                        static_cast<unsigned>(authBootstrapChild.authReplySuccessField15Timestamp118));
                }
                // anchor: launcher.exe:0x43f300 character-slot loop + route-host string copy (inline, no function call)
                // Binary shape:
                // - ResetSelectionRouteState
                // - clamp character count to <= 100
                // - materialize owner `+0x688`
                // - validate slot status inline and force invalid values to `7`
                // - walk owner `+0xd84` and seed `+0x818` when worldId matches
                //
                // Source again keeps the same resulting owner state while now reusing the more
                // faithful packet family split from `0x43f300`:
                // - select `currentCharacterTempRecord80/currentCharacterHandle84`
                // - initialize each owner slot entry as a `Packet_AsAuthReply_0x4b5328`
                // - then normalize status / seed route-host strings by matching world ids
                {
                    g_CurrentLoginMediator->selectionRouteState684_.ResetSelectionRouteState();
                    const size_t characterCount = std::min(
                        g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_.size(),
                        parseObject != nullptr
                            ? static_cast<size_t>(parseObject->characterTempRecordCount40)
                            : 0u);
                    g_CurrentLoginMediator->selectionRouteState684_.slotRecordCount00_ =
                        static_cast<uint8_t>(characterCount);
                    for (size_t i = 0; i < characterCount; ++i) {
                        const char* handleText = "";
                        uint16_t handleLength = 0u;
                        const uint8_t* const characterRecord =
                            AuthBootstrap680SelectCharacterTempRecordByIndex(
                                const_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(parseObject),
                                i,
                                &handleText,
                                &handleLength);
                        if (!characterRecord) {
                            break;
                        }
                        const uint8_t rawStatus = characterRecord[0x0bu];
                        const uint8_t normalizedStatus = (rawStatus <= 6u) ? rawStatus : 7u;
                        const uint32_t characterIdLow = ReadU32LEState2(characterRecord + 0x03u);
                        const uint32_t characterIdHigh = ReadU32LEState2(characterRecord + 0x07u);
                        if (normalizedStatus != rawStatus) {
                            const unsigned long long characterId =
                                static_cast<unsigned long long>(characterIdLow) |
                                (static_cast<unsigned long long>(characterIdHigh) << 32u);
                            spdlog::info(
                                CLTLoginState_AuthenticatePending_0x4b5014::kLogInvalidCharacterStatus,
                                handleText,
                                characterId,
                                static_cast<unsigned>(rawStatus));
                        }

                        auto& slotRecord = g_CurrentLoginMediator->selectionRouteState684_.slotRecordTable04_[i];
                        slotRecord = {};
                        (void)handleLength;
                        if (!AuthBootstrap680InitializeSlotRecordFromSelectedTempRecord(
                                &slotRecord,
                                const_cast<Packet_AsGetPublicKeyRequest_0x4b6c74*>(parseObject))) {
                            break;
                        }
                        if (slotRecord.payloadAlias10 != nullptr) {
                            static_cast<uint8_t*>(slotRecord.payloadAlias10)[0x0bu] = normalizedStatus;
                        }
                        g_CurrentLoginMediator->selectionRouteState684_.slotRecordValid04_[i] = true;

                        int matchedWorldIndex = -1;
                        for (size_t worldIndex = 0;
                             worldIndex < g_CurrentLoginMediator->worldListPacketCountD80_ &&
                             worldIndex < g_CurrentLoginMediator->worldListPacketsD84_.size();
                             ++worldIndex) {
                            Packet_WorldList_0x4b533c* const worldPacket =
                                g_CurrentLoginMediator->worldListPacketsD84_[worldIndex];
                            if (worldPacket != nullptr &&
                                worldPacket->worldId01 ==
                                    (slotRecord.payloadAlias10 != nullptr
                                        ? *reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(slotRecord.payloadAlias10) + 0x0cu)
                                        : 0u)) {
                                matchedWorldIndex = static_cast<int>(worldIndex);
                                break;
                            }
                        }
                        if (matchedWorldIndex >= 0) {
                            // anchor: launcher.exe:0x43f74a
                            Packet_WorldList_0x4b533c* const matchedWorldPacket =
                                g_CurrentLoginMediator->worldListPacketsD84_[static_cast<size_t>(
                                    matchedWorldIndex)];
                            if (matchedWorldPacket != nullptr) {
                                g_CurrentLoginMediator->selectionRouteState684_.routeHostStrings194_[i] =
                                    matchedWorldPacket->inlineNamePlus03;
                            }
                        }
                    }
                }
                g_CurrentLoginMediator->PersistCharactersIniFromRecoveredAuthStateScaffold();
                g_CurrentLoginMediator->PostEvent(6u);

                const Packet_AsGetPublicKeyRequest_0x4b6c74* const parseObject3 =
                    authBootstrapChild.authReplyParseObjectF0;
                const std::string replyString1d =
                    authBootstrapChild.CopyReplyString54();
                g_CurrentLoginMediator->SetLaunchPadSourceBlock94FirstString(
                    replyString1d.c_str());

                authBootstrapChild.CopyOpaqueReplyBlobs108_10c();

                spdlog::info(
                    "AuthBootstrap680SyncState2AuthReplySuccessOneTime_ReplyStringAndOpaqueBlobs ownerSource94FirstString='{}' opaqueBlob108Len={} opaqueBlob10CLen={} opaqueBlob108={} opaqueBlob10C={} parseObjectF0={}",
                    replyString1d.empty() ? "<empty>" : replyString1d.c_str(),
                    static_cast<unsigned>(parseObject3 != nullptr &&
                                          parseObject3->copiedOpaqueReplyBlob108Bytes2c != nullptr &&
                                          parseObject3->copiedOpaqueReplyBlob108ByteLength30 != 0u
                                              ? parseObject3->copiedOpaqueReplyBlob108ByteLength30
                                              : 0u),
                    static_cast<unsigned>(parseObject3 != nullptr &&
                                          parseObject3->copiedOpaqueReplyBlob10CBytes34 != nullptr &&
                                          parseObject3->copiedOpaqueReplyBlob10CByteLength38 != 0u
                                              ? parseObject3->copiedOpaqueReplyBlob10CByteLength38
                                              : 0u),
                    fmt::ptr(authBootstrapChild.opaqueReplyBlob108),
                    fmt::ptr(authBootstrapChild.opaqueReplyBlob10C),
                    fmt::ptr(parseObject3));
            }

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
        // binary; source-only logging/additional scaffolding removed here for fidelity
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
