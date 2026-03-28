#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

static uint32_t BeginMarginConnectionIfResolved(
    CLTLoginMediator* mediator,
    const char* routeHostText,
    uint8_t cachedRouteSelector) {
    if (!mediator || !routeHostText || routeHostText[0] == '\0') {
        return 0u;
    }
    return mediator->BeginMarginConnectionScaffold(routeHostText, cachedRouteSelector);
}

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4::DebugName() const {
    return "CLTLoginState_State4";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4::Slot2_HandleSecondaryGate(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
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

}  // namespace mxo::ltlogin
