#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b0b88
const char* CLTLoginState_State15_0x4b0b88::DebugName() const {
    return "CLTLoginState_State15_0x4b0b88";
}

// anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
void CLTLoginState_State15_0x4b0b88::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    (void)PlaceholderStateAction(DebugName(), "launcher.exe:0x00420680");
    return;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
uint32_t CLTLoginState_State15_0x4b0b88::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
uint32_t CLTLoginState_State15_0x4b0b88::GetStateId() const {
    return 15;
}

// anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
uint32_t CLTLoginState_State15_0x4b0b88::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) {
    (void)param1;
    (void)context;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004206a0");
}

}  // namespace mxo::ltlogin
