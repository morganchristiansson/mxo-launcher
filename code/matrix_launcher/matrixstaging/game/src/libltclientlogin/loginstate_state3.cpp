#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b5208
const char* CLTLoginState_State3::DebugName() const {
    return "CLTLoginState_State3";
}

// anchor: launcher.exe:0x00438cf0 (vtable 0x004b5208 slot 7)
uint32_t CLTLoginState_State3::Slot7_GetStateId() const {
    return 3;
}

}  // namespace mxo::ltlogin
