#pragma once

#include <cstdint>

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
//   - that success handoff belongs to `CLTLoginState_State16` slot 8 on vtable `0x004b0bb0`
//     and stays documented on the login-state side
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
class LaunchPadClient {
public:
    LaunchPadClient() = default;

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
};

}  // namespace mxo::ltlogin
