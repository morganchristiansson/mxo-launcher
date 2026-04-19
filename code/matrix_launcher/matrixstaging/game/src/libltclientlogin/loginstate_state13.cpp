#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b50dc
const char* CLTLoginState_State13_0x4b50dc::DebugName() const {
    return "CLTLoginState_State13_0x4b50dc";
}

// anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
uint32_t CLTLoginState_State13_0x4b50dc::Slot2_HandleSecondaryGate(void* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439680");
}

// anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
void CLTLoginState_State13_0x4b50dc::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    (void)upstreamOrArg;
    (void)PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bb90");
    return;
}

// anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
uint32_t CLTLoginState_State13_0x4b50dc::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bc60");
}

// anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
uint32_t CLTLoginState_State13_0x4b50dc::GetStateId() const {
    return 13;
}

}  // namespace mxo::ltlogin
