#include "loginstate.h"

#include "loginmediator.h"

namespace mxo::ltlogin {
namespace {

static uint32_t PlaceholderStateAction(const char* debugName, const char* anchor) {
    (void)debugName;
    (void)anchor;
    return 1;
}

}  // namespace

// anchor: reconstructed shared login-state family surface spanning launcher.exe vtable families
const char* CLTLoginState_AbstractFinalLeafBase::DebugName() const {
    return "CLTLoginState_AbstractFinalLeafBase";
}

// anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 3 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 4 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot4_NoOp() {
    return 1;
}

// anchor: launcher.exe:0x004397c0 (shared slot 5 failure stub on multiple vtables)
uint32_t CLTLoginState::Slot5_HandlePrimaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004397c0 (default shared slot 6 failure stub on selected vtables)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00441790 (shared slot 8 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
uint32_t CLTLoginState::Slot9_IsNetworkDriven() const {
    return 1;
}

// anchor: launcher.exe:0x004439300 consults slot-7-style state/helper ids before margin-route dispatch
uint32_t CLTLoginState::DispatchPhaseCode() const {
    return Slot7_GetStateId();
}

// anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00437b40 (vtable 0x004b51b8 slot 9)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot9_IsNetworkDriven() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b51e0
const char* CLTLoginState_State0::DebugName() const {
    return "CLTLoginState_State0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0::Slot7_GetStateId() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b4fc4
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004390b0");
}

// anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
uint32_t CLTLoginState_State1::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439090");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
uint32_t CLTLoginState_State1::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
uint32_t CLTLoginState_State1::Slot7_GetStateId() const {
    return 1;
}

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
uint32_t CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439210");
}

// anchor: launcher.exe:0x0043f300 (vtable 0x004b5014 slot 5)
uint32_t CLTLoginState_AuthenticatePending::Slot5_HandlePrimaryMessage(void* workItem, CLTLoginMediator* mediator) {
    return AuthMessageDispatch(workItem, mediator);
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::Slot7_GetStateId() const {
    return 2;
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best recovered role from `0x43f300`:
    // - parses an auth-side message through the mediator-owned `+0x680` helper object
    // - on the success branch (`case 2`) it performs the broader post-auth table writeback before
    //   the narrower helper10 selected-slot path becomes relevant
    // - concrete broader writeback now looks like:
    //   - build owner `+0xd84` as a world-descriptor table
    //   - validate world status/type there
    //   - build owner `+0x688` as a character-slot record table
    //   - validate character status there
    //   - seed owner `+0x818` by matching each character record's world id against the
    //     world-descriptor table and copying the descriptor name
    //   - post event `5`, then switch helper state based on the current helper object
    // Current source ownership note:
    // - the replacement launcher now mirrors the reconstructed `+0xd84/+0x688/+0x818` families
    //   inside `CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState()` so later state-8
    //   margin dispatch can consume reconstructed data instead of only fallback state
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe vtable 0x004b5208
const char* CLTLoginState_State3::DebugName() const {
    return "CLTLoginState_State3";
}

// anchor: launcher.exe:0x00438cf0 (vtable 0x004b5208 slot 7)
uint32_t CLTLoginState_State3::Slot7_GetStateId() const {
    return 3;
}

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4::DebugName() const {
    return "CLTLoginState_State4";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004393f0");
}

// anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
uint32_t CLTLoginState_State4::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439300");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
uint32_t CLTLoginState_State4::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
uint32_t CLTLoginState_State4::Slot7_GetStateId() const {
    return 4;
}

// anchor: launcher.exe vtable 0x004b5064
const char* CLTLoginState_State5::DebugName() const {
    return "CLTLoginState_State5";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439590");
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
uint32_t CLTLoginState_State5::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439520");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5::Slot7_GetStateId() const {
    return 5;
}

// anchor: launcher.exe vtable 0x004b508c
const char* CLTLoginState_State6::DebugName() const {
    return "CLTLoginState_State6";
}

// anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
uint32_t CLTLoginState_State6::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b8f0");
}

// anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
uint32_t CLTLoginState_State6::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00440780");
}

// anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
uint32_t CLTLoginState_State6::Slot7_GetStateId() const {
    return 6;
}

// anchor: launcher.exe vtable 0x004b50b4
const char* CLTLoginState_State7::DebugName() const {
    return "CLTLoginState_State7";
}

// anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
uint32_t CLTLoginState_State7::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043ba20");
}

// anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
uint32_t CLTLoginState_State7::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bae0");
}

// anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
uint32_t CLTLoginState_State7::Slot7_GetStateId() const {
    return 7;
}

// anchor: launcher.exe vtable 0x004b5104
const char* CLTLoginState_State8::DebugName() const {
    return "CLTLoginState_State8";
}

// anchor: launcher.exe:0x0043bd20 (vtable 0x004b5104 slot 3)
uint32_t CLTLoginState_State8::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bd20");
}

// anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
uint32_t CLTLoginState_State8::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043f930");
}

// anchor: launcher.exe:0x00438c90 (vtable 0x004b5104 slot 7)
uint32_t CLTLoginState_State8::Slot7_GetStateId() const {
    return 8;
}

// anchor: launcher.exe vtable 0x004b517c
const char* CLTLoginState_State9::DebugName() const {
    return "CLTLoginState_State9";
}

// anchor: launcher.exe:0x00439780 (vtable 0x004b517c slot 3)
uint32_t CLTLoginState_State9::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439780");
}

// anchor: launcher.exe:0x0043c180 (vtable 0x004b517c slot 6)
uint32_t CLTLoginState_State9::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043c180");
}

// anchor: launcher.exe:0x00438cc0 (vtable 0x004b517c slot 7)
uint32_t CLTLoginState_State9::Slot7_GetStateId() const {
    return 9;
}

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bf90");
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004401a0");
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

// anchor: launcher.exe vtable 0x004b5154
const char* CLTLoginState_State11::DebugName() const {
    return "CLTLoginState_State11";
}

// anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
uint32_t CLTLoginState_State11::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043c020");
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00440320");
}

// anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
uint32_t CLTLoginState_State11::Slot7_GetStateId() const {
    return 11;
}

// anchor: launcher.exe vtable 0x004b5230
const char* CLTLoginState_State12::DebugName() const {
    return "CLTLoginState_State12";
}

// anchor: launcher.exe:0x00438d00 (vtable 0x004b5230 slot 7)
uint32_t CLTLoginState_State12::Slot7_GetStateId() const {
    return 12;
}

// anchor: launcher.exe vtable 0x004b50dc
const char* CLTLoginState_State13::DebugName() const {
    return "CLTLoginState_State13";
}

// anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
uint32_t CLTLoginState_State13::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439680");
}

// anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
uint32_t CLTLoginState_State13::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bb90");
}

// anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
uint32_t CLTLoginState_State13::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bc60");
}

// anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
uint32_t CLTLoginState_State13::Slot7_GetStateId() const {
    return 13;
}

// anchor: launcher.exe vtable 0x004b4fec
const char* CLTLoginState_WorldListPending::DebugName() const {
    return "CLTLoginState_WorldListPending";
}

// anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
uint32_t CLTLoginState_WorldListPending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b830");
}

// anchor: launcher.exe:0x0043d4d0 (vtable 0x004b4fec slot 5)
uint32_t CLTLoginState_WorldListPending::Slot5_HandlePrimaryMessage(void* workItem, CLTLoginMediator* mediator) {
    return AuthMessageDispatch(workItem, mediator);
}

// anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
uint32_t CLTLoginState_WorldListPending::Slot7_GetStateId() const {
    return 14;
}

// anchor: launcher.exe:0x0043d4d0 (string/file anchors: loginstate.cpp, CLTLoginState_WorldListPending::AuthMessageDispatch())
uint32_t CLTLoginState_WorldListPending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best contextual role from the vtable and string anchors:
    // - vtable 0x004b4fec / slot 5
    // - AS_GetWorldListReply / AS_PSGetWorldListReply
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe vtable 0x004b0b88
const char* CLTLoginState_State15::DebugName() const {
    return "CLTLoginState_State15";
}

// anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
uint32_t CLTLoginState_State15::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420680");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
uint32_t CLTLoginState_State15::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
uint32_t CLTLoginState_State15::Slot7_GetStateId() const {
    return 15;
}

// anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
uint32_t CLTLoginState_State15::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004206a0");
}

// anchor: launcher.exe vtable 0x004b0bb0
const char* CLTLoginState_State16::DebugName() const {
    return "CLTLoginState_State16";
}

// anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
uint32_t CLTLoginState_State16::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420720");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
uint32_t CLTLoginState_State16::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
uint32_t CLTLoginState_State16::Slot7_GetStateId() const {
    return 16;
}

// anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
uint32_t CLTLoginState_State16::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004207c0");
}

// anchor: launcher.exe vtable 0x004b0bd8
const char* CLTLoginState_State17::DebugName() const {
    return "CLTLoginState_State17";
}

// anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
uint32_t CLTLoginState_State17::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420890");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
uint32_t CLTLoginState_State17::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
uint32_t CLTLoginState_State17::Slot7_GetStateId() const {
    return 17;
}

// anchor: launcher.exe vtable 0x004b0c00
const char* CLTLoginState_State18::DebugName() const {
    return "CLTLoginState_State18";
}

// anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
uint32_t CLTLoginState_State18::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00421a50");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
uint32_t CLTLoginState_State18::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
uint32_t CLTLoginState_State18::Slot7_GetStateId() const {
    return 18;
}

// anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
uint32_t CLTLoginState_State18::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420960");
}

// anchor: launcher.exe vtable 0x004b0c28
const char* CLTLoginState_State19::DebugName() const {
    return "CLTLoginState_State19";
}

// anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
uint32_t CLTLoginState_State19::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004209e0");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
uint32_t CLTLoginState_State19::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420350 (vtable 0x004b0c28 slot 7)
uint32_t CLTLoginState_State19::Slot7_GetStateId() const {
    return 19;
}

// anchor: launcher.exe:0x00420a00 (vtable 0x004b0c28 slot 8)
uint32_t CLTLoginState_State19::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420a00");
}

}  // namespace mxo::ltlogin
