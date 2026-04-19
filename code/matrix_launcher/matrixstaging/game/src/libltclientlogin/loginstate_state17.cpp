#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b0bd8
const char* CLTLoginState_State17_0x4b0bd8::DebugName() const {
    return "CLTLoginState_State17_0x4b0bd8";
}

// anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
void CLTLoginState_State17_0x4b0bd8::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    (void)PlaceholderStateAction(DebugName(), "launcher.exe:0x00420890");
    return;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
uint32_t CLTLoginState_State17_0x4b0bd8::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
uint32_t CLTLoginState_State17_0x4b0bd8::GetStateId() const {
    return 17;
}

}  // namespace mxo::ltlogin
