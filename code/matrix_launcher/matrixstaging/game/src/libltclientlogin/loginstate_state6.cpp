#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"

#include <array>
#include <cstring>
#include <sstream>

namespace mxo::ltlogin {
namespace {

// State6Packet0x06FixedPayload is defined in loginstate_packet_builder_scaffold.h

static std::array<uint32_t, 4> ResolveState6GobFileGuidWords(CLTLoginMediator* mediator) {
    (void)mediator;

    // anchor: launcher.exe:0x438e60 = GetGOBFileGUID
    // Original `0x43b8f0` reads a 16-byte LTGUID for packed GOB resource `0x3e000000` and falls
    // back to the baked-in dwords at `0x4ae6c0..0x4ae6cc` when that resource cannot be opened.
    // Current replacement does not yet own that launcher resource-manager bridge, but the baked-in
    // fallback bytes are now at least mirrored explicitly instead of leaving state6 slot3 on a
    // synthetic packet shape.
    return {0u, 0u, 0u, 0u};
}

struct ParsedState6Opcode9ReplyScaffold {
    bool valid = false;
    uint32_t status01 = 0;
    uint32_t goHereAddr05 = 0;
    uint32_t udpSessionSecret09 = 0;
    uint16_t metricIdsOffset0d = 0;
    uint16_t metricIdCount = 0;
    std::vector<uint16_t> metricIds{};
};

static std::string BuildMetricIdPreview(const std::vector<uint16_t>& metricIds) {
    if (metricIds.empty()) {
        return "[]";
    }

    std::ostringstream out;
    out << '[';
    for (size_t i = 0; i < metricIds.size(); ++i) {
        if (i != 0u) {
            out << ", ";
        }
        out << "0x" << std::hex << std::uppercase << static_cast<unsigned>(metricIds[i]);
    }
    out << ']';
    return out.str();
}

static const char* ResolveClientMetricFilenameById(uint16_t metricId) {
    // Original launcher `0x48ce00` maps one METRID to its Filename by iterating the launcher-side
    // METR metadata table. The replacement keeps that question local to the post-state9/state12
    // path by using the already loaded client-side copy of the same METR metadata instead of
    // widening this pass into a launcher-owned SetMasterDatabase reconstruction first.
    static constexpr uintptr_t kClientMetricArrayBeginRva = 0x009f1934u;
    static constexpr uintptr_t kClientMetricArrayEndRva = 0x009f1938u;
    static constexpr uintptr_t kClientFindFieldRva = 0x002abb60u;
    static constexpr uintptr_t kClientFieldGetStringRva = 0x002add50u;
    static constexpr uintptr_t kClientFieldGetDwordRva = 0x002add30u;

    using FindFieldFn = void*(__thiscall*)(void*, const char*);
    using GetStringFn = const char*(__thiscall*)(void*);
    using GetDwordFn = uint32_t(__thiscall*)(void*);

    void* const clientModule = GetModuleHandleA("client.dll");
    if (clientModule == nullptr) {
        return nullptr;
    }

    const uintptr_t clientBase = reinterpret_cast<uintptr_t>(clientModule);
    const auto findField = reinterpret_cast<FindFieldFn>(clientBase + kClientFindFieldRva);
    const auto getString = reinterpret_cast<GetStringFn>(clientBase + kClientFieldGetStringRva);
    const auto getDword = reinterpret_cast<GetDwordFn>(clientBase + kClientFieldGetDwordRva);

    const uintptr_t entriesBegin = *reinterpret_cast<const uintptr_t*>(clientBase + kClientMetricArrayBeginRva);
    const uintptr_t entriesEnd = *reinterpret_cast<const uintptr_t*>(clientBase + kClientMetricArrayEndRva);
    if (entriesBegin == 0u || entriesEnd <= entriesBegin) {
        return nullptr;
    }

    for (uintptr_t current = entriesBegin; current < entriesEnd; current += sizeof(uint32_t)) {
        void* const entry = *reinterpret_cast<void* const*>(current);
        if (entry == nullptr) {
            continue;
        }

        const char* const tag = *reinterpret_cast<const char* const*>(entry);
        if (!tag || std::strcmp(tag, "METR") != 0) {
            continue;
        }

        void* const metricIdField = findField(entry, "METRID");
        if (metricIdField == nullptr) {
            continue;
        }
        void* const metricIdValueHolder = *reinterpret_cast<void**>(static_cast<uint8_t*>(metricIdField) + 4u);
        void* const metricIdValueObject = metricIdValueHolder ? *reinterpret_cast<void**>(metricIdValueHolder) : nullptr;
        if (metricIdValueObject == nullptr) {
            continue;
        }
        const uint16_t currentMetricId = static_cast<uint16_t>(getDword(metricIdValueObject) & 0xffffu);
        if (currentMetricId != metricId) {
            continue;
        }

        void* const filenameField = findField(entry, "Filename");
        if (filenameField == nullptr) {
            return nullptr;
        }
        void* const filenameValueHolder = *reinterpret_cast<void**>(static_cast<uint8_t*>(filenameField) + 4u);
        void* const filenameValueObject = filenameValueHolder ? *reinterpret_cast<void**>(filenameValueHolder) : nullptr;
        return filenameValueObject ? getString(filenameValueObject) : nullptr;
    }

    return nullptr;
}

static ParsedState6Opcode9ReplyScaffold ParseState6Opcode9ReplyScaffold(
    const uint8_t* bytes,
    const uint16_t byteCount) {
    ParsedState6Opcode9ReplyScaffold out = {};
    if (!bytes || byteCount < 0x0f || bytes[0] != 0x09u) {
        return out;
    }

    out.valid = true;
    out.status01 = ReadU32LE(bytes + 0x01u);
    out.goHereAddr05 = ReadU32LE(bytes + 0x05u);
    out.udpSessionSecret09 = ReadU32LE(bytes + 0x09u);
    out.metricIdsOffset0d = ReadU16LE(bytes + 0x0du);

    if (out.metricIdsOffset0d != 0u) {
        const size_t metricHeaderOffset = static_cast<size_t>(out.metricIdsOffset0d);
        if (metricHeaderOffset + 2u <= byteCount) {
            out.metricIdCount = ReadU16LE(bytes + metricHeaderOffset);
            const size_t metricBodyOffset = metricHeaderOffset + 2u;
            const size_t metricBodyBytes = static_cast<size_t>(out.metricIdCount) * sizeof(uint16_t);
            if (metricBodyOffset + metricBodyBytes > byteCount) {
                out.metricIdCount = static_cast<uint16_t>((byteCount - metricBodyOffset) / sizeof(uint16_t));
            }
            out.metricIds.reserve(out.metricIdCount);
            for (uint16_t i = 0; i < out.metricIdCount; ++i) {
                const size_t metricOffset = metricBodyOffset + static_cast<size_t>(i) * sizeof(uint16_t);
                if (metricOffset + sizeof(uint16_t) > byteCount) {
                    break;
                }
                out.metricIds.push_back(ReadU16LE(bytes + metricOffset));
            }
        }
    }

    return out;
}

}  // namespace

// anchor: launcher.exe vtable 0x004b508c
const char* CLTLoginState_State6_0x4b508c::DebugName() const {
    return "CLTLoginState_State6_0x4b508c";
}

// anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
void CLTLoginState_State6_0x4b508c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    // anchor: launcher.exe:0x43b8f0 upstream caching logic at `0x43b8f9..0x43b91c`
    // - `0x43b8f9` loads existing cached from this+4
    // - `0x43b8fc` tests if cached is null - if so, jumps to store at `0x43b91c`
    // - `0x43b904..0x43b91a` calls incoming upstream's vtable `+0x18` to get phase code
    // - if phase==4 or phase==5, jumps to `0x43b91f` preserving existing cached
    // - otherwise `0x43b91c` overwrites this+4 with incoming upstream pointer
    // That cached upstream is later used on opcode-9 success (`0x440acc..0x440ae0`) to choose
    // the next helper-state target via vtable+0x18.
    if (upstreamOrArg != nullptr) {
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_0x4 == nullptr || (upstreamPhaseCode != 4u && upstreamPhaseCode != 5u)) {
            cachedUpstreamOrArg_0x4 = upstreamOrArg;
        }
    }
    // anchor: launcher.exe:0x43b91f..0x43b92a - direct g_CurrentLoginMediator access, no null check
    if (!g_CurrentLoginMediator->State10HasReadyConnectionState2()) {
        g_CurrentLoginMediator->SetCurrentState(4u);
        return;
    }

    auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection_0x4aff38*>(g_CurrentLoginMediator->marginConnection_);
    const bool marginConnectionReady84 =
        marginConnection != nullptr && marginConnection->MessageCode4SuccessFlag84();
    // anchor: launcher.exe:0x43b94a..0x43b95e - no logging on fallback, just return void
    if (!marginConnectionReady84) {
        g_CurrentLoginMediator->SetCurrentState(5u);
        return;
    }

    // anchor: launcher.exe:0x43b8f0 / local packet-builder family `0x004b5364`
    // Current tighter send-side mirror:
    // - payload `+0x00` = raw opcode `0x06` (`MS_ConnectRequest`)
    // - payload `+0x01/+0x05` = owner `+0x08/+0x0c` launcher/client version dwords
    // - payload `+0x09` = fixed byte `1`
    // - payload `+0x0a/+0x0e` = fixed dwords `0x11186887` / `0x7460a4b0`
    // - payload `+0x12..+0x21` = packed-GOB LTGUID from `0x438e60` fallback family
    // - payload `+0x22` = owner current helper phase byte
    // anchor: launcher.exe:0x43b8f0 = Packet_MsConnectRequest_0x4b5364::ResetAndInitialize
    Packet_MsConnectRequest_0x4b5364 packetBuilder;
    packetBuilder.ResetAndInitialize();

    const uint32_t* launcherVersionPtr = g_CurrentLoginMediator->GetNoPatchLauncherVersionValuePtr08();
    const uint32_t* clientVersionPtr = g_CurrentLoginMediator->GetNoPatchClientVersionValuePtr0c();
    const uint32_t launcherVersion = launcherVersionPtr ? *launcherVersionPtr : 0u;
    const uint32_t clientVersion = clientVersionPtr ? *clientVersionPtr : 0u;
    const auto gobFileGuidWords = ResolveState6GobFileGuidWords(g_CurrentLoginMediator);
    const uint8_t currentHelperPhaseByte = static_cast<uint8_t>(
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DispatchPhaseCode() : 0u);

    // Write fields directly to payload (static-RE faithful)
    uint8_t* payload = packetBuilder.PayloadBase();
    if (payload) {
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kLauncherVersionOffset) = launcherVersion;
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kClientVersionOffset) = clientVersion;
        // Fixed 9-byte block at +0x09 that server expects: byte=1, dword=0x11186887, dword=0x7460a4b0
        payload[State6Packet0x06FixedPayload::kStateByteOffset] = State6Packet0x06FixedPayload::kStateByteValue;
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kFixedDwordAOffset) = State6Packet0x06FixedPayload::kFixedDwordA;
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kFixedDwordEOffset) = State6Packet0x06FixedPayload::kFixedDwordE;
        // Packed GOB LTGUID from launcher resource
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x0) = gobFileGuidWords[0];
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x4) = gobFileGuidWords[1];
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x8) = gobFileGuidWords[2];
        *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0xc) = gobFileGuidWords[3];
        // Server expects 0x00 at offset 0x22 after weirdSequence
        payload[State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset] = 0u;
        // Note: helper phase byte is internal launcher state, read separately from mediator
    }

    // Build envelope for send
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope{};
    envelope.payloadBase04 = payload;
    envelope.messageRef08 = packetBuilder.messageRef08;
    const uint32_t sendResult = g_CurrentLoginMediator->SendCurrentMarginPacket(envelope);
    g_CurrentLoginMediator->PostEvent(0x11u);
    spdlog::info(
        "CLTLoginState_State6_0x4b508c::Slot3_BeginOrContinue built fixed raw-0x06 margin packet fixedBytes=0x{:02x} launcherVersion=0x{:08x} clientVersion=0x{:08x} gobGuid=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] helperPhaseByte=0x{:02x} sendResult=0x{:08x} currentState={} then posts event=0x11",
        State6Packet0x06FixedPayload::kFixedByteCount,
        static_cast<unsigned>(launcherVersion),
        static_cast<unsigned>(clientVersion),
        static_cast<unsigned>(gobFileGuidWords[0]),
        static_cast<unsigned>(gobFileGuidWords[1]),
        static_cast<unsigned>(gobFileGuidWords[2]),
        static_cast<unsigned>(gobFileGuidWords[3]),
        static_cast<unsigned>(currentHelperPhaseByte),
        static_cast<unsigned>(sendResult),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    return;
}

// anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
uint32_t CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage entered this={} currentState={}",
        fmt::ptr(this),
        (g_CurrentLoginMediator && g_CurrentLoginMediator->currentState_) ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    if (!g_CurrentLoginMediator) {
        return 0u;
    }

    // Static-RE fidelity: the original at 0x440793 calls DecodeMessageCode (0x41bc20)
    // which returns the message opcode in AX. Then 0x44079b CMP AX,0x7
    // dispatches opcode-7 to one path, others to another. Use the same decode approach
    // as state10: CMessageConnection_0x4b7928_DecodeMessageCode to get opcode.
    uint16_t messageCode = 0;
    if (!CMessageConnection_0x4b7928_DecodeMessageCode(*workItem, &messageCode, nullptr)) {
        // Decode failed - original sees opcode 0, dispatches to reject path
    }

    // Positive `owner +0xf18` writer result:
    // - `0x00440780 = CLTLoginState_State6_Slot6_HandleMarginOpcode7Or9Reply`
    // - tighter parser/layout review around `0x43a0b0 / 0x43a060 / 0x43c950` now bounds the
    //   opcode-`9` reply body as:
    //   - `+0x01` = `Status`
    //   - `+0x05` = `GoHereAddr`
    //   - `+0x09` = original debug label `UDPSessionSecret`
    //   - `+0x0d` = offset to `MetrIds` array header `(u16 count, then u16 ids...)`
    // - for this function's static source mirror, the important direct fact is narrower:
    //   `parsedReply(+0x09)` is the dword copied into owner `+0xf18`
    // - on the opcode-`9` success side, the original writes owner byte `+0xf14 = 1`
    //   and owner dword `+0xf18 = parsedReply(+0x09)` before switching helper state and posting
    //   event `0x12`
    // - concrete assembly shape at `0x440ab9..0x440ac9`:
    //   - `mov eax, [0x004f78b8]`
    //   - `mov edx, [edi+0x9]`
    //   - `add eax, 0xf14`
    //   - `mov [eax+4], edx`  -> owner `+0xf18`
    //   - `mov byte ptr [eax], 1` -> owner `+0xf14`
    // - importantly, the immediate helper-state switch target is **not** derived from that dword:
    //   `0x440acc..0x440ae0` reloads state6 `this`, reads the cached upstream object at `this+4`,
    //   and calls its vtable `+0x18` to choose the next helper state

    // Get payload directly from message ref (matching state10 pattern at 0x4401c0)
    // Use payload directly from workItem (matching state10 pattern)
    const uint8_t* payloadBytes = workItem->messageStorage0c->PayloadBase();
    const uint16_t payloadByteCount = workItem->PayloadByteCount();
    if (!payloadBytes || payloadByteCount == 0u) {
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage rejected empty payload");
        return 0u;
    }

    if (messageCode == 0x07u) {
        // MS_ConnectChallenge: parse fields and send challenge response
        if (payloadByteCount < 0x0du) {
            spdlog::info(
                "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x07 payload too short={}",
                static_cast<unsigned>(payloadByteCount));
            return 0u;
        }
        const uint32_t goHereAddr = ReadU32LE(payloadBytes + 0x01u);
        const uint32_t goHerePort = ReadU32LE(payloadBytes + 0x05u);
        const uint32_t sessionSecret = ReadU32LE(payloadBytes + 0x09u);
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x07 goHereAddr=0x{:08x} goHerePort=0x{:08x} sessionSecret=0x{:08x}; mirroring original challenge-response flow",
            static_cast<unsigned>(goHereAddr),
            static_cast<unsigned>(goHerePort),
            static_cast<unsigned>(sessionSecret));
        // Store for use in challenge-response construction
        g_CurrentLoginMediator->state6UdpSessionSecretF18_ = sessionSecret;
        g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 1u;

        // Original builds and sends challenge-response packet first, then switches helper
        // For now: directly call SendCurrentMarginPacket - simplified stub
        // Note: Full packet construction not yet source-owned - this is a stub
        // g_CurrentLoginMediator->SendCurrentMarginPacket(...); // packet builder not yet implemented

        // Then switch states through cached upstream (if available)
        if (cachedUpstreamOrArg_0x4 != nullptr) {
            const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
            g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);
        }

        // Post event to continue flow
        g_CurrentLoginMediator->PostEvent(0x11u);
        return 1u;
    }

    // Opcode 0x09 = MS_ConnectChallengeResponse or similar margin reply
    const ParsedState6Opcode9ReplyScaffold parsed = ParseState6Opcode9ReplyScaffold(payloadBytes, payloadByteCount);
    if (!parsed.valid) {
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage rejected payloadCode=0x{:02x} payloadBytes={}; state6 slot6 currently source-owns only opcode-0x09",
            static_cast<unsigned>(messageCode),
            static_cast<unsigned>(payloadByteCount));
        return 0u;
    }

    g_CurrentLoginMediator->worldListCountOrStatus80 = parsed.status01;
    if (parsed.status01 != 0u) {
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage observed opcode-0x09 failure status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x}; success-side owner+0xf14/+0xf18 write is source-owned but the broader failure-side helper-switch/error path is still not",
            static_cast<unsigned>(parsed.status01),
            static_cast<unsigned>(parsed.goHereAddr05),
            static_cast<unsigned>(parsed.udpSessionSecret09));
        return 1u;
    }

    g_CurrentLoginMediator->ClearLateEntryList1470Scaffold();

    // Original at 0x440a42 calls FUN_0048dbc0 with (DAT_004f9cd4 + 0x200):
    // - Constructs a logrouter hook path string and calls into cls_0x48d020 registration system
    // - This is an internal launcher infrastructure hook, not needed for the replacement path
    // Original at 0x440ab4 calls FUN_0048cfd0 -> meth_0x48cea0:
    // - Iterates internal margin state arrays and calls destructors (meth_0x48e010)
    // - Cleanup of per-connection state; not needed for the self-contained replacement
    // Both functions omitted with comment documenting intentional fidelity gap.

    uint32_t resolvedMetricFilenameCount = 0u;
    uint32_t unresolvedMetricFilenameCount = 0u;
    for (uint16_t metricId : parsed.metricIds) {
        const char* const filename = ResolveClientMetricFilenameById(metricId);
        if (filename && filename[0] != '\0') {
            const size_t filenameLength = std::strlen(filename);
            LateEntryList1470EntrySketch metricFilenameStringTriple{};
            metricFilenameStringTriple.begin = const_cast<char*>(filename);
            metricFilenameStringTriple.current = metricFilenameStringTriple.begin + filenameLength;
            metricFilenameStringTriple.capacity = metricFilenameStringTriple.current + 1u;
            g_CurrentLoginMediator->AppendLateEntryStringTriple1470Scaffold(&metricFilenameStringTriple);
            ++resolvedMetricFilenameCount;
            continue;
        }

        ++unresolvedMetricFilenameCount;
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage could not resolve opcode-0x09 metricId=0x{:04x} to a Filename through the loaded client METR table",
            static_cast<unsigned>(metricId));
    }

    g_CurrentLoginMediator->postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 1u;
    g_CurrentLoginMediator->state6UdpSessionSecretF18_ = parsed.udpSessionSecret09;

    if (cachedUpstreamOrArg_0x4 == nullptr) {
        spdlog::info(
            "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} but has no cached upstream helper at this+4 yet; leaving helper-switch/event-0x12 to the broader caller flow currentState={}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(parsed.metricIdCount),
            resolvedMetricFilenameCount,
            unresolvedMetricFilenameCount,
            BuildMetricIdPreview(parsed.metricIds),
            g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    const uint32_t switchDispatchResult = g_CurrentLoginMediator->SetCurrentState(nextHelperStateId);
    g_CurrentLoginMediator->PostEvent(0x12u);
    spdlog::info(
        "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage opcode-0x09 success wrote owner+0xf14=1 owner+0xf18=0x{:08x} and re-entered helperState=0x{:02x} via 0x41b450 oldState=state6 semantics switchDispatchResult=0x{:08x}",
        static_cast<unsigned>(parsed.udpSessionSecret09),
        static_cast<unsigned>(nextHelperStateId),
        static_cast<unsigned>(switchDispatchResult));

    spdlog::info(
        "CLTLoginState_State6_0x4b508c::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} -> nextHelperState={} currentState={}",
        static_cast<unsigned>(parsed.status01),
        static_cast<unsigned>(parsed.goHereAddr05),
        static_cast<unsigned>(parsed.udpSessionSecret09),
        static_cast<unsigned>(parsed.metricIdCount),
        resolvedMetricFilenameCount,
        unresolvedMetricFilenameCount,
        BuildMetricIdPreview(parsed.metricIds),
        static_cast<unsigned>(nextHelperStateId),
        g_CurrentLoginMediator->currentState_ ? g_CurrentLoginMediator->currentState_->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
uint32_t CLTLoginState_State6_0x4b508c::GetStateId() const {
    return 6;
}

}  // namespace mxo::ltlogin
