#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b0c28
const char* CLTLoginState_State19::DebugName() const {
    return "CLTLoginState_State19";
}

// anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
uint32_t CLTLoginState_State19::Slot3_BeginOrContinue(void* upstreamOrArg) {
    (void)upstreamOrArg;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004209e0");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
uint32_t CLTLoginState_State19::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420350 (vtable 0x004b0c28 slot 7)
uint32_t CLTLoginState_State19::GetStateId() const {
    return 19;
}

// anchor: launcher.exe:0x00420a00 (vtable 0x004b0c28 slot 8)
uint32_t CLTLoginState_State19::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) {
    (void)param1;
    (void)context;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420a00");
}

}  // namespace mxo::ltlogin
