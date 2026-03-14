#pragma once

#include <cstdint>

namespace mxo::ltlogin {

// Recovered source-file anchor:
// - `\matrixstaging\game\src\libltclientlogin\launchpad.cpp`
//
// Current best role from launcher.exe:
// - pre-game account / subscription / play-request layer
// - launches or validates the external/launcher-side login flow before the direct MxO auth
//   helper chain becomes active
// - important handoff anchor:
//   - launcher.exe:0x4207c0 = LaunchPadClient_OnConnectionStatusCheck
//   - on success it logs `LaunchPad login successful.` and switches mediator helper state `1`
//   - helper state `1` then reaches launcher.exe:0x439090 -> 0x41d170
//     (`CLTLoginMediator_Helper1_StartAuthConnection` -> `CLTLoginMediator_BeginAuthConnection`)
//
// Current high-confidence function anchors in this family:
// - `0x420440` = `LaunchPadClient_OnConnectionOpened`
// - `0x4204f0` = `LaunchPadClient_OnSessionClosed`
// - `0x420580` = `LaunchPadClient_OnSubscriptionValidation`
// - `0x4207c0` = `LaunchPadClient_OnConnectionStatusCheck`
// - `0x420ef0` = `LaunchPadClient_OnPlayRequestStatus`
// - `0x421220` = `LaunchPadClient_OnLoginRequestStatus`
// - `0x488360` = `LaunchPadClient_DispatchConnectionStatus`
class LaunchPadClient {
public:
    LaunchPadClient() = default;

    // Address anchors only for now; these are still placeholders, not faithful implementations.
    uint32_t OnLoginRequestStatus();      // launcher.exe:0x421220
    uint32_t OnPlayRequestStatus();       // launcher.exe:0x420ef0
    uint32_t OnConnectionOpened();        // launcher.exe:0x420440
    uint32_t OnSessionClosed();           // launcher.exe:0x4204f0
    uint32_t OnSubscriptionValidation();  // launcher.exe:0x420580
    uint32_t OnConnectionStatusCheck();   // launcher.exe:0x4207c0
};

}  // namespace mxo::ltlogin
