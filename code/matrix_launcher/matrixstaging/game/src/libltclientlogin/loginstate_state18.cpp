#include "loginstate.h"
#include "loginmediator.h"

namespace mxo::ltlogin {

// anchor: launcher.exe vtable 0x004b0c00
const char* CLTLoginState_State18::DebugName() const {
    return "CLTLoginState_State18";
}

// anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
void CLTLoginState_State18::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    // Stronger current read from disassembly review:
    // - this is the later state18 session-helper path in the alternate
    //   `g_LaunchPadGateState16State18` family, not the active state2 -> owner+0x680
    //   bootstrap-child handoff and not a direct helper11 writer
    // - it fetches owner vtable `+0x130` helper `+0x65c`
    // - when conditions permit, it refreshes helper string `+0x18` from owner `+0x94 + 0x60`
    //   (the embedded small-string in the recovered auth/bootstrap source block)
    // - it then reaches `0x420e70`, which copies helper `+0x18` into owner `+0x664`
    //   (`GameSessionID`) when helper flag `+0x2d` is clear
    (void)upstreamOrArg;
    if (mediator) {
        mediator->RefreshSessionHelperGameSessionId664FromSourceBlock94();
    }
    return;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
uint32_t CLTLoginState_State18::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
uint32_t CLTLoginState_State18::GetStateId() const {
    return 18;
}

// anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
uint32_t CLTLoginState_State18::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) {
    (void)param1;
    (void)context;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420960");
}

}  // namespace mxo::ltlogin
