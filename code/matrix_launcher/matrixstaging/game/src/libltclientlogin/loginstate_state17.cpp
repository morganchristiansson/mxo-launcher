#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

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

}  // namespace mxo::ltlogin
