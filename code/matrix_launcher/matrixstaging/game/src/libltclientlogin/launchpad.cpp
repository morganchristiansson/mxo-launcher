#include "launchpad.h"

namespace mxo::ltlogin {

uint32_t LaunchPadClient::OnLoginRequestStatus() {
    // launcher.exe:0x421220
    // current best role: login-request result handling plus subscription/status filtering
    return 0;
}

uint32_t LaunchPadClient::OnPlayRequestStatus() {
    // launcher.exe:0x420ef0
    // current best role: play-request result handling after earlier LaunchPad-side login work
    return 0;
}

uint32_t LaunchPadClient::OnConnectionOpened() {
    // launcher.exe:0x420440
    // string-backed anchor:
    // - "LaunchPadClient %d connections opened (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnSessionClosed() {
    // launcher.exe:0x4204f0
    // string-backed anchor:
    // - "LaunchPadClient %d connections closed (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnSubscriptionValidation() {
    // launcher.exe:0x420580
    // string-backed anchor:
    // - "LaunchPadClient connection failed (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnConnectionStatusCheck() {
    // launcher.exe:0x4207c0
    // string-backed anchor:
    // - "LaunchPad login successful."
    // important handoff:
    // - success switches mediator helper state 1
    // - helper state 1 then reaches 0x439090 -> 0x41d170
    return 0;
}

}  // namespace mxo::ltlogin
