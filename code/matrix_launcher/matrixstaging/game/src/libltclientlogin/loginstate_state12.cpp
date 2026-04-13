#include "loginstate.h"

namespace mxo::ltlogin {

// Focused post-state9/state12 split:
// - keep the state-0x0c final-leaf identity in its own TU
// - this keeps post-state9 continuation work scoped to the late existing-character path instead of
//   rereading the broader shared login-state file
// - canonical references:
//   - `../../../../docs/launcher.exe/VTABLES/0x004b5230.md`
//   - `../../../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md`

CLTLoginState_State12::CLTLoginState_State12() {
    // anchor: launcher.exe:0x00439d80 + helper-dispatch init at `0x43b300`
    // The dispatch table creates this final leaf with byte `this+4 = 1`, which is what makes the
    // shared slot-6 handler (`0x004397e0`) delegate into `0x41c5c0` instead of immediately
    // writing `0x12000005`.
    // Current natural-original status note:
    // - `0x43c180` success now proves the launcher switches into state `0x0c`
    // - a representative live run at that boundary was visibly at "Waiting for Regionserver"
    // - newer `0x41b450` / `0x41cfb0` review now also shows why the old immediate-leaf theory is
    //   too strong: the success tail switches helper state and then posts event `0x18` through the
    //   owner listener tree before any later shared final-leaf slot-6 hit is proven
    // - follow-up late probes on `0x004397e0` / `0x0041c5c0` still did **not** hit naturally
    // - so keep this as the strongest current state-identity lead for the post-state9 continuation,
    //   but do not yet claim that the natural path immediately falls into the shared final-leaf
    //   slot-6 handler behind it
    slot6DispatchByte4_ = 1u;
}

// anchor: launcher.exe vtable 0x004b5230
const char* CLTLoginState_State12::DebugName() const {
    return "CLTLoginState_State12";
}

// anchor: launcher.exe:0x00438d00 (vtable 0x004b5230 slot 7)
uint32_t CLTLoginState_State12::GetStateId() const {
    return 12;
}

}  // namespace mxo::ltlogin
