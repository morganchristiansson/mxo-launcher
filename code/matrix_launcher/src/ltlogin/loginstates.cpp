#include "loginstates.h"

#include "loginmediator.h"

namespace mxo::ltlogin {

uint32_t CLTLoginState::DispatchPhaseCode() const {
    // Placeholder only.
    // Current launcher owner-state dispatch uses a separate state/helper object through
    // vtable `+0x18`; this scaffold only preserves the login-state naming structure.
    return 3;
}

const char* CLTLoginState_WorldListPending::DebugName() const {
    return "CLTLoginState_WorldListPending";
}

uint32_t CLTLoginState_WorldListPending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Placeholder only.
    // Keep this source anchor close to the string-backed original method name and reply family:
    // - CLTLoginState_WorldListPending::AuthMessageDispatch()
    // - AS_GetWorldListReply / AS_PSGetWorldListReply
    // - original diagnostic strings worth preserving later:
    //   - CLTLoginState_WorldListPending::kLogInvalidWorldType
    //   - CLTLoginState_WorldListPending::kLogInvalidWorldStatus
    (void)workItem;
    (void)mediator;
    return 1;
}

const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Placeholder only.
    // Keep this source anchor close to the string-backed original method name and auth reply family:
    // - CLTLoginState_AuthenticatePending::AuthMessageDispatch()
    // - AS_AuthReply / AS_AuthChallenge / AS_AuthChallengeResponse
    // - AS_PSAuthenticateReply
    // - original diagnostic strings worth preserving later:
    //   - CLTLoginState_AuthenticatePending::kLogInvalidCharacterStatus
    //   - CLTLoginState_AuthenticatePending::kLogInvalidWorldType
    //   - CLTLoginState_AuthenticatePending::kLogInvalidWorldStatus
    (void)workItem;
    (void)mediator;
    return 1;
}

}  // namespace mxo::ltlogin
