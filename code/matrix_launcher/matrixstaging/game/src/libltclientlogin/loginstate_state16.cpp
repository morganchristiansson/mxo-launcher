#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b0bb0
const char* CLTLoginState_State16::DebugName() const {
    return "CLTLoginState_State16";
}

// anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
void CLTLoginState_State16::Slot3_BeginOrContinue(void* upstreamOrArg) {
    // Current ownership boundary:
    // - `0x41ecd0` can target state16 only on the alternate
    //   `g_LaunchPadGateState16State18 != 0` family
    // - that state16/state18 family is now kept explicit but default-off so the proven
    //   `state0 -> state2 -> state1 -> state2 -> state3` happy path remains favored
    // - keep this placeholder narrow until the non-happy/session branch itself is recovered as a
    //   faithful state-owned body rather than grown ad hoc inside the mediator
    (void)upstreamOrArg;
    (void)PlaceholderStateAction(DebugName(), "launcher.exe:0x00420720");
    return;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
uint32_t CLTLoginState_State16::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
uint32_t CLTLoginState_State16::GetStateId() const {
    return 16;
}

// anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
uint32_t CLTLoginState_State16::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) {
    // Ownership note from the current vtable/xref pass:
    // - `0x4207c0` is slot 8 of `CLTLoginState_State16` vtable `0x004b0bb0`
    // - the `"LaunchPad login successful."` string is a log anchor, not a `LaunchPadClient`
    //   class marker
    // - on `param1 == 1` the original logs success and switches helper state `1`, which then
    //   enters `0x439090 -> 0x41d170` on the auth-connect path
    (void)param1;
    (void)context;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004207c0");
}

}  // namespace mxo::ltlogin
