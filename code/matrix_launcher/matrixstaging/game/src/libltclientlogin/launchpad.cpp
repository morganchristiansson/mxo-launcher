#include "launchpad.h"

#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

namespace {

static const char* NonEmptyOrFallback(const char* value, const char* fallback) {
    return (value && value[0] != '\0') ? value : fallback;
}

}  // namespace

// anchor: launcher.exe vtable 0x004b0e48
// docs: ../../docs/launcher.exe/VTABLES/0x004b0e48.md
// Current best class ownership for this file:
// - `0x004b0e48` = LaunchPadClient vtable family
// - this is a separate class from `CLTLoginMediator`
// - this is also separate from the `CLTLoginState_*` family
// - keep LaunchPad-owned status-handler behavior here instead of re-describing it from the
//   mediator implementation files
// - key vtable entries now kept here:
//   - `0x420440` = `LaunchPadClient_OnConnectionOpened`
//   - `0x4204f0` = `LaunchPadClient_OnSessionClosed`
//   - `0x420580` = `LaunchPadClient_OnSubscriptionValidation`
//   - `0x421220` = `LaunchPadClient_OnLoginRequestStatus`
//   - `0x420ef0` = `LaunchPadClient_OnPlayRequestStatus`

uint32_t LaunchPadClient::OnLoginRequestStatus(
    CLTLoginMediator* mediator,
    uint32_t resultCode,
    const char* sourceBlock94FirstString,
    uint32_t sharedMarginPacketField660,
    const char* sessionText) {
    // anchor: launcher.exe:0x421220
    // anchor: launcher.exe vtable 0x004b0e48 + 0x14
    // Current best recovered LaunchPadClient success effects only:
    // - on `resultCode == 0`, the original handler first calls mediator vtable `+0x144`
    // - then pushes stack arg `[ebp+0x14]` into owner vtable `+0x14c` / `0x41f330`, writing
    //   owner dword `+0x660`
    // - then pushes stack arg `[ebp+0x10]` into owner vtable `+0x150` / `0x41f270`, writing the
    //   first inline string of the owner `+0x94` source block
    // - it separately logs `Session %s` from stack arg `[ebp+0x20]`, but that success path does
    //   **not** directly write owner `+0x664`
    // - keep that ownership here: this is LaunchPadClient behavior writing into mediator-owned
    //   state, not evidence that LaunchPadClient and CLTLoginMediator are the same class
    if (!mediator || resultCode != 0u) {
        return 0u;
    }

    mediator->SetSharedMarginPacketField660(sharedMarginPacketField660);
    mediator->SetLaunchPadSourceBlock94FirstString(sourceBlock94FirstString);

    spdlog::info(
        "DIAGNOSTIC: mirrored launchpad login-request success owner660=0x{:08x} source94.inlineString00='{}' session='{}'",
        sharedMarginPacketField660,
        NonEmptyOrFallback(sourceBlock94FirstString, "<empty>"),
        NonEmptyOrFallback(sessionText, "<empty>"));
    return 1u;
}

uint32_t LaunchPadClient::OnPlayRequestStatus(
    CLTLoginMediator* mediator,
    uint32_t resultCode,
    const char* gameSessionId) {
    // anchor: launcher.exe:0x420ef0
    // anchor: launcher.exe vtable 0x004b0e48 + 0x18
    // Current best recovered LaunchPadClient success effects only:
    // - on `resultCode == 0`, this path copies its callback string into mediator owner `+0x664`
    // - state8/state11 packet builders later read that same string through owner vtable `+0x148`
    //   / `0x41f320` as `GameSessionID`
    // - current best concrete read is therefore that `GameSessionID` is launchpad/play-session
    //   owned, not part of the branch-specific `0x41c3c0` post-auth source writer
    // - keep that ownership split explicit: this write belongs to LaunchPadClient's vtable path,
    //   even though the destination storage lives on the mediator owner object
    if (!mediator || resultCode != 0u) {
        return 0u;
    }

    mediator->SetGameSessionId664(gameSessionId);

    spdlog::info(
        "DIAGNOSTIC: mirrored launchpad play-request success GameSessionID='%s'",
        NonEmptyOrFallback(gameSessionId, "<empty>"));
    return 1u;
}

uint32_t LaunchPadClient::OnConnectionOpened() {
    // anchor: launcher.exe:0x420440
    // anchor: launcher.exe vtable 0x004b0e48 + 0x08
    // string-backed anchor:
    // - "LaunchPadClient %d connections opened (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnSessionClosed() {
    // anchor: launcher.exe:0x4204f0
    // anchor: launcher.exe vtable 0x004b0e48 + 0x0c
    // string-backed anchor:
    // - "LaunchPadClient %d connections closed (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnSubscriptionValidation() {
    // anchor: launcher.exe:0x420580
    // anchor: launcher.exe vtable 0x004b0e48 + 0x10
    // string-backed anchor:
    // - "LaunchPadClient connection failed (now %d total) from %s"
    return 0;
}

uint32_t LaunchPadClient::OnConnectionStatusCheck() {
    // anchor: launcher.exe:0x4207c0
    // string-backed anchor:
    // - "LaunchPad login successful."
    // important handoff:
    // - success switches mediator helper state 1
    // - helper state 1 then reaches 0x439090 -> 0x41d170
    return 0;
}

}  // namespace mxo::ltlogin
