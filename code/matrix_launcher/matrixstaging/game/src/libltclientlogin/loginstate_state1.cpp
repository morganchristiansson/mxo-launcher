#include "loginstate.h"
#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

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

}  // namespace mxo::ltlogin
