#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

struct ParsedState6Opcode9ReplyScaffold {
    bool valid = false;
    uint32_t status01 = 0;
    uint32_t goHereAddr05 = 0;
    uint32_t udpSessionSecret09 = 0;
    uint16_t metricIdsOffset0d = 0;
    uint16_t metricIdCount = 0;
};

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

// anchor: launcher.exe vtable 0x004b508c
const char* CLTLoginState_State6::DebugName() const {
    return "CLTLoginState_State6";
}

// anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
uint32_t CLTLoginState_State6::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    // Current source-owned tightening from the anchored `0x43b8f0` prologue:
    // - this object caches an upstream/helper pointer at `this+4`
    // - when an incoming upstream object reports phase/state `4` or `5`, the original keeps the
    //   existing cached pointer instead of replacing it
    // - that cached upstream later matters on opcode-`9` success because `0x440acc..0x440ae0`
    //   reads it back and calls its vtable `+0x18` to choose the next helper-state target
    if (upstreamOrArg != nullptr) {
        const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(upstreamOrArg);
        if (cachedUpstreamOrArg_ == nullptr || (upstreamPhaseCode != 4u && upstreamPhaseCode != 5u)) {
            cachedUpstreamOrArg_ = upstreamOrArg;
        }
    }

    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b8f0");
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
    // - open server/proxy mirrors place the changing `MS_ConnectReply` session id in that same
    //   wire slot, so the current best cross-checked read is:
    //   `parsedReply(+0x09) = opcode-9 UDPSessionSecret / session-id dword`
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

    mediator->State10SendGateFlagF14() = 1u;
    mediator->SetState6UdpSessionSecretF18(parsed.udpSessionSecret09);

    if (cachedUpstreamOrArg_ == nullptr) {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} but has no cached upstream helper at this+4 yet; leaving helper-switch/event-0x12 to the broader caller flow currentState={}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(parsed.metricIdCount),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    if (CLTLoginState* nextState = LookupRegisteredScaffoldStateById(mediator, nextHelperStateId)) {
        mediator->SwitchHelperStateScaffold(nextHelperStateId, nextState);
        mediator->PostEventScaffold(0x12u);
    } else {
        spdlog::info(
            "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success owner+0xf14=1 owner+0xf18=0x{:08x} metricIdCount={} but could not resolve nextHelperState={} from cachedUpstream={}; preserving currentState={}",
            static_cast<unsigned>(parsed.udpSessionSecret09),
            static_cast<unsigned>(parsed.metricIdCount),
            static_cast<unsigned>(nextHelperStateId),
            fmt::ptr(cachedUpstreamOrArg_),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 1u;
    }

    spdlog::info(
        "CLTLoginState_State6::Slot6_HandleSecondaryMessage mirrored opcode-0x09 success status=0x{:08x} goHereAddr=0x{:08x} udpSessionSecret=0x{:08x} metricIdCount={} -> nextHelperState={} currentState={}",
        static_cast<unsigned>(parsed.status01),
        static_cast<unsigned>(parsed.goHereAddr05),
        static_cast<unsigned>(parsed.udpSessionSecret09),
        static_cast<unsigned>(parsed.metricIdCount),
        static_cast<unsigned>(nextHelperStateId),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
    return 1u;
}

// anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
uint32_t CLTLoginState_State6::Slot7_GetStateId() const {
    return 6;
}

}  // namespace mxo::ltlogin
