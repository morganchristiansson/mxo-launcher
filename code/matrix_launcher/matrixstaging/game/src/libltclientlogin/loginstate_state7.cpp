#include "loginstate.h"
#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

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

}  // namespace mxo::ltlogin