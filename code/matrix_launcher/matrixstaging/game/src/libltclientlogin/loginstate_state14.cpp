#include <spdlog/spdlog.h>
#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

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

}  // namespace mxo::ltlogin
