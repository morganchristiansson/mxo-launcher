#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

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

}  // namespace mxo::ltlogin
