#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mxo::ltlogin {
namespace {

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

static uint32_t PlaceholderStateAction(const char* debugName, const char* anchor) {
    (void)debugName;
    (void)anchor;
    return 1;
}

static uint32_t RecoverCachedUpstreamPhaseCode(const void* cachedUpstreamOrArg) {
    // `0x439300` calls the cached upstream/helper object's vtable `+0x18`.
    // The source scaffold keeps that as the shared `DispatchPhaseCode()` wrapper over the
    // recovered login-state family.
    const auto* cachedUpstreamState = static_cast<const CLTLoginState*>(cachedUpstreamOrArg);
    return cachedUpstreamState ? cachedUpstreamState->DispatchPhaseCode() : 0u;
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

static uint32_t BeginMarginConnectionIfResolved(
    CLTLoginMediator* mediator,
    const char* routeHostText,
    uint8_t cachedRouteSelector) {
    if (!mediator || !routeHostText || routeHostText[0] == '\0') {
        return 0u;
    }
    return mediator->BeginMarginConnectionScaffold(routeHostText, cachedRouteSelector);
}

}  // namespace

// anchor: reconstructed shared login-state family surface spanning launcher.exe vtable families
const char* CLTLoginState_AbstractFinalLeafBase::DebugName() const {
    return "CLTLoginState_AbstractFinalLeafBase";
}

// anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 3 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 4 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot4_NoOp() {
    return 1;
}

// anchor: launcher.exe:0x004397c0 (shared slot 5 failure stub on multiple vtables)
uint32_t CLTLoginState::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004397c0 (default shared slot 6 failure stub on selected vtables)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00441790 (shared slot 8 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
uint32_t CLTLoginState::Slot9_IsNetworkDriven() const {
    return 1;
}

// anchor: launcher.exe:0x004439300 consults slot-7-style state/helper ids before margin-route dispatch
uint32_t CLTLoginState::DispatchPhaseCode() const {
    return Slot7_GetStateId();
}

// anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    // Exact recovered shape from `0x004397e0`:
    // - when object byte `this+4 == 1`, delegate to owner helper `0x41c5c0`
    // - if that helper returns `< 1`, return success-ish immediately
    // - otherwise write owner `+0x80 = 0x12000005` and fail
    // Live-path caution tightened again from the latest breakpoint-only original run:
    // - after the proven state9 success tail (`0x41b450(0x0c) -> 0x41cfb0(0x18)`), the natural run
    //   later re-hit `0x41cfb0` with event `0x0f` and entered game
    // - it still did not hit `0x004397e0` or `0x41c5c0` on that continuation
    // - so keep this as a later probe path, not as the already-proven immediate post-state9 flow
    if (slot6DispatchByte4_ == 1u && mediator != nullptr) {
        const uint32_t dispatchResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (dispatchResult < 1u) {
            spdlog::info(
                "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=1 delegated to owner callback84 workItem={} -> dispatchResult=0x{:08x}",
                fmt::ptr(workItem),
                static_cast<unsigned>(dispatchResult));
            return 1u;
        }
    }

    if (mediator != nullptr) {
        mediator->WorldListCountOrStatus80() = 0x12000005u;
    }
    spdlog::info(
        "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=0x{:02x} set owner+0x80=0x12000005 workItem={}",
        static_cast<unsigned>(slot6DispatchByte4_),
        fmt::ptr(workItem));
    return 0u;
}

// anchor: launcher.exe:0x00437b40 (vtable 0x004b51b8 slot 9)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot9_IsNetworkDriven() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b51e0
const char* CLTLoginState_State0::DebugName() const {
    return "CLTLoginState_State0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0::Slot7_GetStateId() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b4fc4
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004390b0");
}

// anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
uint32_t CLTLoginState_State1::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439090");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
uint32_t CLTLoginState_State1::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
uint32_t CLTLoginState_State1::Slot7_GetStateId() const {
    return 1;
}

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
uint32_t CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439210");
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best recovered role from `0x43f300`:
    // - parses an auth-side message through the mediator-owned `+0x680` helper object
    // - on the success branch (`case 2`) it performs the broader post-auth table writeback before
    //   the narrower helper10 selected-slot path becomes relevant
    // - concrete broader writeback now looks like:
    //   - build owner `+0xd84` as a world-descriptor table
    //   - validate world status/type there
    //   - build owner `+0x688` as a character-slot record table
    //   - validate character status there
    //   - seed owner `+0x818` by matching each character record's world id against the
    //     world-descriptor table and copying the descriptor name
    //   - post event `5`, then switch helper state based on the current helper object
    // Current source ownership note:
    // - the replacement launcher now mirrors the reconstructed `+0xd84/+0x688/+0x818` families
    //   inside `CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState()` so later state-8
    //   margin dispatch can consume reconstructed data instead of only fallback state
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::Slot7_GetStateId() const {
    return 2;
}

// anchor: launcher.exe vtable 0x004b5208
const char* CLTLoginState_State3::DebugName() const {
    return "CLTLoginState_State3";
}

// anchor: launcher.exe:0x00438cf0 (vtable 0x004b5208 slot 7)
uint32_t CLTLoginState_State3::Slot7_GetStateId() const {
    return 3;
}

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4::DebugName() const {
    return "CLTLoginState_State4";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004393f0");
}

// anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
uint32_t CLTLoginState_State4::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Faithfulness/ownership correction from the fresh `0x439300` disassembly review:
    // - `0x439300` belongs to `CLTLoginState_State4` vtable `0x004b503c` slot 3
    // - this object caches the first incoming upstream/helper pointer at `this+4`
    // - it then calls that cached object's vtable `+0x18` and uses the returned phase/state code
    //   for the real case split
    // - only the narrow owner-side route getters and `0x41e500` transport/init stay on the
    //   mediator
    if (cachedUpstreamOrArg_ == nullptr) {
        cachedUpstreamOrArg_ = upstreamOrArg;
    }

    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    switch (upstreamPhaseCode) {
        case 6:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteDescriptor(),
                0u);

        case 7:
        case 8:
        case 13:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromCurrentCharacterSlot(),
                mediator->CharacterRouteIndexCc8());

        case 10:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromDescriptorIndex(mediator->SourceField12c()),
                static_cast<uint8_t>(mediator->SourceField12c() & 0xffu));

        default: {
            // Current source-owned mirror for the default branch's owner `+0x104` dword remains
            // `CurrentMarginRouteState().currentWorldId`; keep the field meaning provisional and
            // only preserve the original `!= -1 -> owner vtable +0xfc -> if non-null call 0x41e500`
            // structure here.
            const int32_t field104Value = mediator->CurrentMarginRouteState().currentWorldId;
            if (field104Value == -1) {
                return 0u;
            }
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromWorldId(static_cast<uint32_t>(field104Value)),
                0u);
        }
    }
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
uint32_t CLTLoginState_State4::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
uint32_t CLTLoginState_State4::Slot7_GetStateId() const {
    return 4;
}

// anchor: launcher.exe vtable 0x004b5064
const char* CLTLoginState_State5::DebugName() const {
    return "CLTLoginState_State5";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439590");
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
uint32_t CLTLoginState_State5::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439520");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5::Slot7_GetStateId() const {
    return 5;
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
    Log(
        "DIAGNOSTIC: CLTLoginState_State6::Slot6_HandleSecondaryMessage entered this=%p mediator=%p stagedMarginBytes=%u currentState=%s",
        this,
        mediator,
        mediator ? (unsigned)mediator->StagedIncomingMarginPacketBytes().size() : 0u,
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

// anchor: launcher.exe vtable 0x004b50b4
const char* CLTLoginState_State7::DebugName() const {
    return "CLTLoginState_State7";
}

// anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
uint32_t CLTLoginState_State7::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043ba20");
}

// anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
uint32_t CLTLoginState_State7::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bae0");
}

// anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
uint32_t CLTLoginState_State7::Slot7_GetStateId() const {
    return 7;
}

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Fresh `0x43bf90` read from decompilation + disassembly:
    // - sends raw margin opcode `0x0a`
    //   - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //     `MS_ClaimCharacterNameRequest`
    // - precheck owner `+0x1c` connection state through `0x41b4b0`
    //   - on failure, original switches helper state to `4`
    // - then check owner byte `+0xf14`
    //   - on zero, original switches helper state to `6`
    // - initialize local packet-builder family `0x43a1f0`
    // - copy owner `+0x108` (`CharacterName`) through `0x43aa80`
    // - send through `0x41af70`
    // - post event `0x13`
    if (!mediator->State10HasReadyConnectionState2()) {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6");
        return 0u;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(mediator->SourceLeadString108().data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x13u);

    Log(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x%02x totalBytes=0x%02x CharacterName='%s' -> sendResult=0x%08x then posts event=0x13",
        (unsigned)State10Packet0x0aFixedPayload::kFixedByteCount,
        (unsigned)packetBuilder.PayloadByteCount(),
        mediator->SourceLeadString108().data(),
        (unsigned)sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    // Ownership correction from the vtable docs + Ghidra decompilation:
    // - `0x4401a0` belongs to `CLTLoginState_State10` slot 6, not to the mediator vtable
    // - the state entry itself handles raw auth code `0x0b`, performs the owner writeback, then
    //   switches helper state to `11`
    // - the mediator keeps only the narrower staged-packet + owner-state helpers
    const uint32_t handled = mediator->HandleStagedAuthReplyPacketScaffold();
    if (handled == 0u) {
        return 0u;
    }

    if (CLTLoginState* nextState = mediator->ScaffoldState11()) {
        mediator->SwitchHelperStateScaffold(0x0bu, nextState);
    } else {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage parsed AS_AuthReply but has no registered helper11 state");
    }
    return handled;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

// anchor: launcher.exe vtable 0x004b5154
const char* CLTLoginState_State11::DebugName() const {
    return "CLTLoginState_State11";
}

// anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
uint32_t CLTLoginState_State11::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Faithfulness correction:
    // - `0x43c020` belongs to `CLTLoginState_State11` slot 3, so the packet build/send shape
    //   should live here, not on the mediator
    // - original body:
    //   - sends raw margin opcode `0x0c`
    //     - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //       `MS_CreateCharacterRequest`
    //   - treats `ESI = owner + 0x108`
    //   - creates a packet-builder object through `0x439840`
    //   - resets/initializes the raw `0x4d` payload through `0x43a470`
    //   - writes 17 dwords from owner `+0x134..+0x174`
    //   - appends `RealFirstName`, `RealLastName`, optional `Background`, and `GameSessionID`
    //     through `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940`
    //   - newer `0x43e540` debug-printer review makes those 17 dwords concrete:
    //     SkinToneID, BodyID, HeadID, HairID, HairColorID, TattooID, FacialHairID,
    //     FacialHairColorID, StartingHat, StartingGlasses, StartingShirt, StartingGloves,
    //     StartingCoat, StartingPants, StartingTights, StartingShoes, TraitID
    //   - calls `0x41af70` to forward the completed packet-envelope object through the current
    //     margin connection send path (`0x448cf0`), not to serialize raw bytes itself
    //   - then posts event `0x15`
    // Active-path caution:
    // - this is a very real character-data sender, but the natural-original password-submit path is
    //   still not live-proven here; no natural hit yet on `0x41c3c0` or `0x43c020`
    // - current replacement-launcher runtime proof now lands here explicitly:
    //   `... -> helperState 0x0b -> event 0x15 -> State11::Slot3 send -> Loading Character`
    //   and then stalls before any incoming `MS_LoadCharacterReply`
    const auto& sourceDwords134 = mediator->SourceDwords134();
    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    State11Packet0x4dBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();

    // Keep write order aligned with the original disassembly of `0x43c020`.
    packetBuilder.SetFixedDword(0x01, sourceDwords134[0]);
    packetBuilder.SetFixedDword(0x05, sourceDwords134[1]);
    packetBuilder.SetFixedDword(0x09, sourceDwords134[2]);
    packetBuilder.SetFixedDword(0x0d, sourceDwords134[3]);
    packetBuilder.SetFixedDword(0x11, sourceDwords134[4]);
    packetBuilder.SetFixedDword(0x15, sourceDwords134[5]);
    packetBuilder.SetFixedDword(0x19, sourceDwords134[6]);
    packetBuilder.SetFixedDword(0x1d, sourceDwords134[7]);
    packetBuilder.SetFixedDword(0x35, sourceDwords134[13]);
    packetBuilder.SetFixedDword(0x25, sourceDwords134[9]);
    packetBuilder.SetFixedDword(0x3d, sourceDwords134[15]);
    packetBuilder.SetFixedDword(0x2d, sourceDwords134[11]);
    packetBuilder.SetFixedDword(0x21, sourceDwords134[8]);
    packetBuilder.SetFixedDword(0x39, sourceDwords134[14]);
    packetBuilder.SetFixedDword(0x31, sourceDwords134[12]);
    packetBuilder.SetFixedDword(0x29, sourceDwords134[10]);
    packetBuilder.SetFixedDword(0x41, sourceDwords134[16]);

    packetBuilder.SetRealFirstName(reinterpret_cast<const char*>(mediator->SourceBlock178().data()));
    packetBuilder.SetRealLastName(reinterpret_cast<const char*>(mediator->SourceBlock198().data()));
    packetBuilder.SetBackground(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()));
    packetBuilder.SetGameSessionId(mediator->GetGameSessionId664());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x15u);

    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue built fixed-0x4d margin payload payloadTag=0x{:02x} fixedBytes=0x{:02x} totalBytes=0x{:02x} SkinToneID=0x{:08x} RealFirstName='{}' RealLastName='{}' Background='{}' GameSessionID='{}' -> sendResult=0x{:08x} then posts event=0x15",
        static_cast<unsigned>(State11Packet0x4dFixedPayload::kPayloadTag0c),
        static_cast<unsigned>(State11Packet0x4dFixedPayload::kFixedByteCount),
        static_cast<unsigned>(packetBuilder.PayloadByteCount()),
        static_cast<unsigned>(sourceDwords134[0]),
        reinterpret_cast<const char*>(mediator->SourceBlock178().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock198().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()),
        mediator->GetGameSessionId664() ? mediator->GetGameSessionId664() : "<empty>",
        static_cast<unsigned>(sendResult));
    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue awaiting first helper11 reply; slot6 requires a later raw-0x10 that survives the base margin code-2/4/5 filter currentState={} marginReceiveCount={} filteredBeforeSlot6={} slot6DispatchCount={}",
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->MarginPacketReceiveCountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketFilteredBeforeSlot6CountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketSlot6DispatchCountScaffold()));
    return sendResult;
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->StagedIncomingMarginPacketBytes();
    const uint16_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(stagedBytes);
    if (!parsed.valid) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; helper11 slot6 only handles raw-0x10 after the base margin code-2/4/5 filter",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    // Ownership correction mirrors slot 3 as well:
    // - `0x440320` belongs to `CLTLoginState_State11` slot 6
    // - the state object owns reply-progress counters and the helper11 -> helper9 handoff
    // - mediator keeps the narrower owner-buffer mutation helper because those writes target the
    //   mediator-owned `0x4f78b8` state area
    const uint32_t handled = mediator->HandleStagedMarginLoadCharacterReplyPacketScaffold();
    if (handled == 0u) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage entered raw-0x10 receive path but owner-side parse/gate failed status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} currentState={}",
            static_cast<unsigned>(parsed.status),
            static_cast<unsigned>(parsed.field05),
            static_cast<unsigned>(parsed.handoffWord09),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
        return 0u;
    }

    if (parsed.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = parsed.expectedSectionCount0b;
    }
    if (replySectionsExpected_ == 0u) {
        replySectionsExpected_ = 1u;
    }
    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                // `0x440320` writes parsed word `+9` into helper9 `this+6` before switching state.
                // Current source-owned mirror keeps that on the concrete state9 object.
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x440320 completion tail posts event 0x16 after switching to helper9.
        mediator->PostEventScaffold(0x16u);

        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage completed helper11 reply progression status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u -> currentState=helper9 event=0x16",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
    } else {
        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage routed helper11 reply status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u seedCount=%u",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_,
            parsed.shouldSeedExpectedSectionCount ? 1u : 0u);
    }
    return handled;
}

// anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
uint32_t CLTLoginState_State11::Slot7_GetStateId() const {
    return 11;
}

// anchor: launcher.exe vtable 0x004b50dc
const char* CLTLoginState_State13::DebugName() const {
    return "CLTLoginState_State13";
}

// anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
uint32_t CLTLoginState_State13::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439680");
}

// anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
uint32_t CLTLoginState_State13::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bb90");
}

// anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
uint32_t CLTLoginState_State13::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bc60");
}

// anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
uint32_t CLTLoginState_State13::Slot7_GetStateId() const {
    return 13;
}

// anchor: launcher.exe vtable 0x004b4fec
const char* CLTLoginState_WorldListPending::DebugName() const {
    return "CLTLoginState_WorldListPending";
}

// anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
uint32_t CLTLoginState_WorldListPending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b830");
}

// anchor: launcher.exe:0x0043d4d0 (string/file anchors: loginstate.cpp, CLTLoginState_WorldListPending::AuthMessageDispatch())
uint32_t CLTLoginState_WorldListPending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best contextual role from the vtable and string anchors:
    // - vtable 0x004b4fec / slot 5
    // - AS_GetWorldListReply / AS_PSGetWorldListReply
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
uint32_t CLTLoginState_WorldListPending::Slot7_GetStateId() const {
    return 14;
}

// anchor: launcher.exe vtable 0x004b0b88
const char* CLTLoginState_State15::DebugName() const {
    return "CLTLoginState_State15";
}

// anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
uint32_t CLTLoginState_State15::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420680");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
uint32_t CLTLoginState_State15::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
uint32_t CLTLoginState_State15::Slot7_GetStateId() const {
    return 15;
}

// anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
uint32_t CLTLoginState_State15::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004206a0");
}

// anchor: launcher.exe vtable 0x004b0bb0
const char* CLTLoginState_State16::DebugName() const {
    return "CLTLoginState_State16";
}

// anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
uint32_t CLTLoginState_State16::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420720");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
uint32_t CLTLoginState_State16::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
uint32_t CLTLoginState_State16::Slot7_GetStateId() const {
    return 16;
}

// anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
uint32_t CLTLoginState_State16::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004207c0");
}

// anchor: launcher.exe vtable 0x004b0bd8
const char* CLTLoginState_State17::DebugName() const {
    return "CLTLoginState_State17";
}

// anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
uint32_t CLTLoginState_State17::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420890");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
uint32_t CLTLoginState_State17::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
uint32_t CLTLoginState_State17::Slot7_GetStateId() const {
    return 17;
}

// anchor: launcher.exe vtable 0x004b0c00
const char* CLTLoginState_State18::DebugName() const {
    return "CLTLoginState_State18";
}

// anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
uint32_t CLTLoginState_State18::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    // Stronger current read from disassembly review:
    // - this is a later launchpad/session helper path, not a direct helper11 writer
    // - it fetches owner vtable `+0x130` helper `+0x65c`
    // - when conditions permit, it refreshes helper string `+0x18` from owner `+0x94 + 0x60`
    //   (the embedded small-string in the recovered auth/bootstrap source block)
    // - it then reaches `0x420e70`, which copies helper `+0x18` into owner `+0x664`
    //   (`GameSessionID`) when helper flag `+0x2d` is clear
    (void)upstreamOrArg;
    return mediator ? mediator->RefreshSessionHelperGameSessionId664FromSourceBlock94() : 0u;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
uint32_t CLTLoginState_State18::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
uint32_t CLTLoginState_State18::Slot7_GetStateId() const {
    return 18;
}

// anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
uint32_t CLTLoginState_State18::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420960");
}

// anchor: launcher.exe vtable 0x004b0c28
const char* CLTLoginState_State19::DebugName() const {
    return "CLTLoginState_State19";
}

// anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
uint32_t CLTLoginState_State19::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004209e0");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
uint32_t CLTLoginState_State19::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420350 (vtable 0x004b0c28 slot 7)
uint32_t CLTLoginState_State19::Slot7_GetStateId() const {
    return 19;
}

// anchor: launcher.exe:0x00420a00 (vtable 0x004b0c28 slot 8)
uint32_t CLTLoginState_State19::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420a00");
}

}  // namespace mxo::ltlogin
