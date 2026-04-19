#include "loginstate.h"
#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b51e0
// Current tighter init-path role:
// - `0x41b160 = CLTLoginMediator_Initialize` installs helper `0x4f7868` into owner `+0x10`
// - helper `0x4f7868` is the `0x004b51e0` state-0 object
// - fresh happy-path WineDbg now proves `0x41ecd0 = ProcessLoginRequest` still runs while the
//   current helper is state0
// - slot 3 is still the shared no-op stub, so the first submit transition stays owner-driven
//   (`ProcessLoginRequest` clears owner `+0xf4`, then switches to helper/state `2`)
// - that makes state0 the explicit initial idle/start helper, not the startup default submit
//   coordinator
const char* CLTLoginState_State0_0x4b51e0::DebugName() const {
    return "CLTLoginState_State0_0x4b51e0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0_0x4b51e0::GetStateId() const {
    return 0;
}

}  // namespace mxo::ltlogin
