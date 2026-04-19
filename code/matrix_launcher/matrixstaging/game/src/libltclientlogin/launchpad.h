#pragma once

#include <cstdint>
#include <string>

namespace mxo::ltlogin {

class CLTLoginMediator;

// Recovered source-file anchor:
// - `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`
//
// Current best role from launcher.exe:
// - pre-game account / subscription / play-request layer
// - launches or validates the external/launcher-side login flow before the direct MxO auth
//   helper chain becomes active
// - boundary note:
//   - do not classify the `"LaunchPad login successful."` body at `0x4207c0` as
//     `LaunchPadClient` ownership
//   - that success handoff belongs to `CLTLoginState_State16_0x4b0bb0` slot 8 on vtable `0x004b0bb0`
//     and stays documented on the login-state side
//   - likewise keep the `g_LaunchPadGateState16State18` state16/state18 family and the earlier
//     startup `+0x140` `station_login` side effect separate from this status-handler vtable
//
// Current high-confidence function anchors in this family:
// - `0x420440` = `LaunchPadClient_OnConnectionOpened`
// - `0x4204f0` = `LaunchPadClient_OnSessionClosed`
// - `0x420580` = `LaunchPadClient_OnSubscriptionValidation`
// - `0x420ef0` = `LaunchPadClient_OnPlayRequestStatus`
// - `0x421220` = `LaunchPadClient_OnLoginRequestStatus`
// - `0x488360` = `LaunchPadClient_DispatchConnectionStatus`
// Detailed ownership / writer-path notes for the vtable-backed status handlers now live in:
// - `launchpad.cpp`
// - `../../docs/launcher.exe/VTABLES/0x004b0e48.md`
//
// Important separation rule:
// - `LaunchPadClient` is its own class/vtable family (`0x004b0e48`)
// - it is **not** the `CLTLoginMediator` class and **not** part of the `CLTLoginState_*` family
// - the mediator pointer passed into the success-path mirrors below is only the downstream owner
//   writeback target recovered from the original handlers
//
// Note: CLTLoginMediator lazy-allocates a LaunchPadClient_0x4b0e48 at owner+0x65c
// and calls through its vtable[+0x04] (CheckObjectTimeout).
class LaunchPadClient_0x4b0e48 {
public:
    LaunchPadClient_0x4b0e48() = default;

    // Virtual method for vtable slot +0x04 call fidelity
    // anchor: launcher.exe:0x4203d0 / vtable[+0x04] = CheckObjectTimeout
    // This is called from CLTLoginMediator::HelperSlot13c_InvokeSessionHelperVtable4
    virtual void InvokeVtableSlot4();

    // Source-owned success-path mirrors for the concrete mediator-owner writeback already proven in
    // the original LaunchPadClient handlers. Non-success branches remain documented in the vtable
    // notes.
    uint32_t OnLoginRequestStatus(
        CLTLoginMediator* mediator,
        uint32_t resultCode,
        const char* sourceBlock94FirstString,
        uint32_t sharedMarginPacketField660,
        const char* sessionText);  // launcher.exe:0x421220
    uint32_t OnPlayRequestStatus(
        CLTLoginMediator* mediator,
        uint32_t resultCode,
        const char* gameSessionId);  // launcher.exe:0x420ef0

    uint32_t OnConnectionOpened();        // launcher.exe:0x420440
    uint32_t OnSessionClosed();           // launcher.exe:0x4204f0
    uint32_t OnSubscriptionValidation();  // launcher.exe:0x420580

    // Fields for session callback helper at CLTLoginMediator +0x65c
    // These are used when this object is allocated as the session callback helper
    void* currentState10 = nullptr;      // helper +0x10 -> owner/mediator pointer
    std::string authConnection18;        // helper +0x18
    uint32_t marginBeginCount24 = 0;     // helper +0x24
    uint32_t authConnectAttemptCount28 = 0;  // helper +0x28
    uint8_t authConnectionFlag2c = 0;    // helper +0x2c
    uint8_t marginConnectionCloseWaitEvent0fGateArmed_2d = 0;  // helper +0x2d
};

// Backward compatibility alias
using LaunchPadClient = LaunchPadClient_0x4b0e48;

}  // namespace mxo::ltlogin
