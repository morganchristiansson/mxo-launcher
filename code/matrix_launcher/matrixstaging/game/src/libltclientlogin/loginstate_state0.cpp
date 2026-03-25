#include "loginstate.h"
#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b51e0
const char* CLTLoginState_State0::DebugName() const {
    return "CLTLoginState_State0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0::Slot7_GetStateId() const {
    return 0;
}

}  // namespace mxo::ltlogin
