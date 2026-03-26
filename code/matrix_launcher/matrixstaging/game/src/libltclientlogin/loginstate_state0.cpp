#include "loginstate.h"
#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b51e0
// Current best init-path role:
// - `0x41b160 = CLTLoginMediator_Initialize` installs helper `0x4f7868` into owner `+0x10`
// - helper `0x4f7868` is the `0x004b51e0` state-0 object
// - slot 3 is still the shared no-op stub, so the practical first transition is owner-driven
//   (`0x41ecd0 = ProcessLoginRequest` -> helper/state `2`), not a state0-local slot-3 body
const char* CLTLoginState_State0::DebugName() const {
    return "CLTLoginState_State0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0::Slot7_GetStateId() const {
    return 0;
}

}  // namespace mxo::ltlogin
