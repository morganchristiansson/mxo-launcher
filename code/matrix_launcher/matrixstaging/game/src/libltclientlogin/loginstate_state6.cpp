#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"

#include <array>
#include <sstream>

namespace mxo::ltlogin {
namespace {

struct State6Packet0x06FixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x06` = `MS_ConnectRequest`
    static constexpr uint8_t kPayloadTag06 = 0x06;
    static constexpr size_t kLauncherVersionOffset = 0x01;
    static constexpr size_t kClientVersionOffset = 0x05;
    static constexpr size_t kStateByteOffset = 0x09;
    static constexpr uint8_t kStateByteValue = 0x01;
    static constexpr size_t kFixedDwordAOffset = 0x0a;
    static constexpr uint32_t kFixedDwordA = 0x11186887u;
    static constexpr size_t kFixedDwordEOffset = 0x0e;
    static constexpr uint32_t kFixedDwordE = 0x7460a4b0u;
    static constexpr size_t kGobFileGuidOffset = 0x12;
    static constexpr size_t kCurrentHelperPhaseOffset = 0x22;
    static constexpr size_t kFixedByteCount = 0x23;
};

struct State6Packet0x06BuilderRawScaffold {
    mxo::liblttcp::CMessageConnectionPacketBuilderPayloadScaffold builder00{};
};

class State6Packet0x06Builder final
    : public RecoveredPacketBuilderEnvelopeBase<State6Packet0x06BuilderRawScaffold> {
public:
    static constexpr uintptr_t kPacketBuilderVtable00 = 0x004b5364u;

    // anchor: launcher.exe:0x43b8f0 / local packet-builder family `0x004b5364`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            kPacketBuilderVtable00,
            State6Packet0x06FixedPayload::kFixedByteCount);
        WritePayloadByte(0x00, State6Packet0x06FixedPayload::kPayloadTag06);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kLauncherVersionOffset, 0u);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kClientVersionOffset, 0u);
        WritePayloadByte(
            State6Packet0x06FixedPayload::kStateByteOffset,
            State6Packet0x06FixedPayload::kStateByteValue);
        WritePayloadU32LE(
            State6Packet0x06FixedPayload::kFixedDwordAOffset,
            State6Packet0x06FixedPayload::kFixedDwordA);
        WritePayloadU32LE(
            State6Packet0x06FixedPayload::kFixedDwordEOffset,
            State6Packet0x06FixedPayload::kFixedDwordE);
        SetGobFileGuid({0u, 0u, 0u, 0u});
        WritePayloadByte(State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset, 0u);
    }

    void SetLauncherVersion(uint32_t value) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kLauncherVersionOffset, value);
    }

    void SetClientVersion(uint32_t value) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kClientVersionOffset, value);
    }

    void SetGobFileGuid(const std::array<uint32_t, 4>& guidWords) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x0, guidWords[0]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x4, guidWords[1]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x8, guidWords[2]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0xc, guidWords[3]);
    }

    void SetCurrentHelperPhaseByte(uint8_t value) {
        WritePayloadByte(State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset, value);
    }
};

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
    const std::vector<uint8_t>& bytes) {
    ParsedState6Opcode9ReplyScaffold out = {};
    if (bytes.size() < 0x0f || bytes[0] != 0x09u) {
        return out;
    }

    out.valid = true;
    out.status01 = ReadU32LE(bytes.data() + 0x01u);
    out.goHereAddr05 = ReadU32LE(bytes.data() + 0x05u);
    out.udpSessionSecret09 = ReadU32LE(bytes.data() + 0x09u);
    out.metricIdsOffset0d = ReadU16LE(bytes.data() + 0x0du);

    if (out.metricIdsOffset0d != 0u) {
        const size_t metricHeaderOffset = static_cast<size_t>(out.metricIdsOffset0d);
        if (metricHeaderOffset + 2u <= bytes.size()) {
            out.metricIdCount = ReadU16LE(bytes.data() + metricHeaderOffset);
            const size_t metricBodyOffset = metricHeaderOffset + 2u;
            const size_t metricBodyBytes = static_cast<size_t>(out.metricIdCount) * sizeof(uint16_t);
            if (metricBodyOffset + metricBodyBytes > bytes.size()) {
                out.metricIdCount = static_cast<uint16_t>((bytes.size() - metricBodyOffset) / sizeof(uint16_t));
            }
            out.metricIds.reserve(out.metricIdCount);
            for (uint16_t i = 0; i < out.metricIdCount; ++i) {
                const size_t metricOffset = metricBodyOffset + static_cast<size_t>(i) * sizeof(uint16_t);
                if (metricOffset + sizeof(uint16_t) > bytes.size()) {
                    break;
                }
                out.metricIds.push_back(ReadU16LE(bytes.data() + metricOffset));
            }
        }
    }

    return out;
}

static CLTLoginState* LookupRegisteredScaffoldStateById(CLTLoginMediator* mediator, uint32_t stateId) {
    if (!mediator) {
        return nullptr;
    }

    switch (stateId) {
        case 3u:
            return mediator->ScaffoldState3();
        case 4u:
            return mediator->ScaffoldState4();
        case 5u:
            return mediator->ScaffoldState5();
        case 6u:
            return mediator->ScaffoldState6();
        case 8u:
            return mediator->ScaffoldState8();
        case 9u:
            return mediator->ScaffoldState9();
        case 10u:
            return mediator->ScaffoldState10();
        case 11u:
            return mediator->ScaffoldState11();
        case 12u:
            return mediator->ScaffoldState12();
        case 13u:
            return mediator->ScaffoldState13();
        default:
            return nullptr;
    }
}

}  // namespace

// anchor: launcher.exe vtable 0x004b508c
const char* CLTLoginState_State6::DebugName() const {
    return "CLTLoginState_State6";
}

// anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
uint32_t CLTLoginState_State6::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    // Static recheck in Ghidra/disassembly for `0x43b8f0` now backs the cache rule directly:
    // - `0x43b8fc` tests state6 `this+4`
    // - `0x43b904..0x43b91a` calls incoming upstream vtable `+0x18`
    // - if that reported phase is `4` or `5`, execution jumps to `0x43b91f` and preserves the
    //   existing cached pointer
    // - otherwise `0x43b91c` overwrites `this+4 = upstream`
    // - that cached upstream later matters on opcode-`9` success because `0x440acc..0x440ae0`
    //   reads it back and calls its vtable `+0x18` to choose the next helper-state target
    if (upstreamOrArg != nullptr) {
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_ == nullptr || (upstreamPhaseCode != 4u && upstreamPhaseCode != 5u)) {
            cachedUpstreamOrArg_ = upstreamOrArg;
        }
    }
    if (!mediator) {
        return 0u;
    }

    if (!mediator->State10HasReadyConnectionState2()) {
        CLTLoginState* fallbackState = mediator->ScaffoldState4();
        const uint32_t fallbackResult = fallbackState
            ? mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                  4u,
                  fallbackState,
                  this,
                  "State6 slot3 owner+0x1c state!=2 -> helper4 margin-connect continuation")
            : 0u;
        spdlog::info(
            "CLTLoginState_State6::Slot3_BeginOrContinue blocked on owner+0x1c state!=2 -> helper4 result=0x{:08x} currentState={}",
            static_cast<unsigned>(fallbackResult),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return fallbackResult;
    }

    auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(mediator->MarginConnection());
    const bool marginConnectionReady84 =
        marginConnection != nullptr && marginConnection->MessageCode4SuccessFlag84();
    if (!marginConnectionReady84) {
        CLTLoginState* fallbackState = mediator->ScaffoldState5();
        const uint32_t fallbackResult = fallbackState
            ? mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
                  5u,
                  fallbackState,
                  this,
                  "State6 slot3 owner+0x1c+0x84==0 -> helper5 continuation")
            : 0u;
        spdlog::info(
            "CLTLoginState_State6::Slot3_BeginOrContinue blocked on margin connection +0x84==0 -> helper5 result=0x{:08x} currentState={}",
            static_cast<unsigned>(fallbackResult),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return fallbackResult;
    }

    // anchor: launcher.exe:0x43b8f0 / local packet-builder family `0x004b5364`
    // Current tighter send-side mirror:
    // - payload `+0x00` = raw opcode `0x06` (`MS_ConnectRequest`)
    // - payload `+0x01/+0x05` = owner `+0x08/+0x0c` launcher/client version dwords
    // - payload `+0x09` = fixed byte `1`
    // - payload `+0x0a/+0x0e` = fixed dwords `0x11186887` / `0x7460a4b0`
    // - payload `+0x12..+0x21` = packed-GOB LTGUID from `0x438e60` fallback family
    // - payload `+0x22` = owner current helper phase byte
    State6Packet0x06Builder packetBuilder;
    packetBuilder.ResetAndInitialize();

    const uint32_t* launcherVersionPtr = mediator->GetNoPatchLauncherVersionValuePtr08();
    const uint32_t* clientVersionPtr = mediator->GetNoPatchClientVersionValuePtr0c();
    const uint32_t launcherVersion = launcherVersionPtr ? *launcherVersionPtr : 0u;
    const uint32_t clientVersion = clientVersionPtr ? *clientVersionPtr : 0u;
    const auto gobFileGuidWords = ResolveState6GobFileGuidWords(mediator);
    const uint8_t currentHelperPhaseByte = static_cast<uint8_t>(
        mediator->CurrentState() ? mediator->CurrentState()->DispatchPhaseCode() : 0u);

    packetBuilder.SetLauncherVersion(launcherVersion);
    packetBuilder.SetClientVersion(clientVersion);
    packetBuilder.SetGobFileGuid(gobFileGuidWords);
    packetBuilder.SetCurrentHelperPhaseByte(currentHelperPhaseByte);

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.Envelope());
    mediator->PostEventScaffold(0x11u);
    spdlog::info(
        "CLTLoginState_State6::Slot3_BeginOrContinue built fixed raw-0x06 margin packet fixedBytes=0x{:02x} launcherVersion=0x{:08x} clientVersion=0x{:08x} gobGuid=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] helperPhaseByte=0x{:02x} sendResult=0x{:08x} currentState={} then posts event=0x11",
        State6Packet0x06FixedPayload::kFixedByteCount,
        static_cast<unsigned>(launcherVersion),
        static_cast<unsigned>(clientVersion),
        static_cast<unsigned>(gobFileGuidWords[0]),
        static_cast<unsigned>(gobFileGuidWords[1]),
        static_cast<unsigned>(gobFileGuidWords[2]),
        static_cast<unsigned>(gobFileGuidWords[3]),
        static_cast<unsigned>(currentHelperPhaseByte),
        static_cast<unsigned>(sendResult),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return sendResult;
}

// anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
uint32_t CLTLoginState_State6::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State6::Slot6_HandleSecondaryMessage entered this={} mediator={} stagedMarginBytes={} currentState={}",
        fmt::ptr(this),
        fmt::ptr(mediator),
        mediator ? mediator->StagedIncomingMarginPacketBytes().size() : 0u,
        (mediator && mediator->CurrentState()) ? mediator->CurrentState()->DebugName() : "<null>");
    if (!mediator) {
        return 0u;
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
    const std::vector<uint8_t>& stagedMarginBytes = mediator->StagedIncomingMarginPacketBytes();
    if (stagedMarginBytes.empty()) {
        return 0u;
    }

    if (stagedMarginBytes[0] == 0x07u) {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage observed opcode-0x07; client.dll/module-path-derived branch is still not source-owned");
        return 0u;
    }

    const ParsedState6Opcode9ReplyScaffold parsed = ParseState6Opcode9ReplyScaffold(stagedMarginBytes);
    if (!parsed.valid) {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; state6 slot6 currently source-owns only opcode-0x09",
            static_cast<unsigned>(stagedMarginBytes.size()),
            stagedMarginBytes.empty() ? 0u : static_cast<unsigned>(stagedMarginBytes[0]));
        return 0u;
    }

    mediator->WorldListCountOrStatus80() = parsed.status01;
    if (parsed.status01 != 0u) {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage observed opcode-0x09 failure status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x}; success-side owner+0xf14/+0xf18 write is source-owned but the broader failure-side helper-switch/error path is still not",
            static_cast<unsigned>(parsed.status01),
            static_cast<unsigned>(parsed.goHereAddr05),
            static_cast<unsigned>(parsed.udpSessionSecret09));
        return 1u;
    }

    mediator->ClearLateEntryList1470Scaffold();
    uint32_t resolvedMetricFilenameCount = 0u;
    uint32_t unresolvedMetricFilenameCount = 0u;
    for (uint16_t metricId : parsed.metricIds) {
        const char* const filename = ResolveClientMetricFilenameById(metricId);
        if (filename && filename[0] != '\0' && mediator->AppendLateEntryFilename1470Scaffold(filename)) {
            ++resolvedMetricFilenameCount;
        } else {
            ++unresolvedMetricFilenameCount;
            spdlog::info(
                "CLTLoginState_State6::Slot6_HandleSecondaryMessage could not resolve opcode-0x09 metricId=0x{:04x} to a Filename through the loaded client METR table",
                static_cast<unsigned>(metricId));
        }
    }

    mediator->State10SendGateFlagF14() = 1u;
    mediator->SetState6UdpSessionSecretF18(parsed.udpSessionSecret09);

    if (cachedUpstreamOrArg_ == nullptr) {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} but has no cached upstream helper at this+4 yet; leaving helper-switch/event-0x12 to the broader caller flow currentState={}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(parsed.metricIdCount),
            resolvedMetricFilenameCount,
            unresolvedMetricFilenameCount,
            BuildMetricIdPreview(parsed.metricIds),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    if (CLTLoginState* nextState = LookupRegisteredScaffoldStateById(mediator, nextHelperStateId)) {
        const uint32_t switchDispatchResult = mediator->SwitchHelperStateAndDispatchSlot3Scaffold(
            nextHelperStateId,
            nextState,
            this,
            "State6 slot6 opcode-0x09 success -> choose restored helper from cached upstream phase and re-enter its slot3");
        mediator->PostEventScaffold(0x12u);
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage opcode-0x09 success wrote owner+0xf14=1 owner+0xf18=0x{:08x} and re-entered helperState=0x{:02x} via 0x41b450 oldState=state6 semantics switchDispatchResult=0x{:08x}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(nextHelperStateId),
            static_cast<unsigned>(switchDispatchResult));
    } else {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} but could not resolve nextHelperState={} from cachedUpstream={}; preserving currentState={}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(parsed.metricIdCount),
            resolvedMetricFilenameCount,
            unresolvedMetricFilenameCount,
            BuildMetricIdPreview(parsed.metricIds),
            static_cast<unsigned>(nextHelperStateId),
            fmt::ptr(cachedUpstreamOrArg_),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    spdlog::info(
        "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x} metricIdCount={} resolvedMetricFilenameCount={} unresolvedMetricFilenameCount={} metricIds={} -> nextHelperState={} currentState={}",
        static_cast<unsigned>(parsed.status01),
        static_cast<unsigned>(parsed.goHereAddr05),
        static_cast<unsigned>(parsed.udpSessionSecret09),
        static_cast<unsigned>(parsed.metricIdCount),
        resolvedMetricFilenameCount,
        unresolvedMetricFilenameCount,
        BuildMetricIdPreview(parsed.metricIds),
        static_cast<unsigned>(nextHelperStateId),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
uint32_t CLTLoginState_State6::Slot7_GetStateId() const {
    return 6;
}

}  // namespace mxo::ltlogin
