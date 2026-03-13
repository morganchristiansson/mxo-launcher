#pragma once

#include <cstdint>

namespace mxo::ltlogin {

class CLTLoginMediator;

// Reimplementation note:
// Prefer original launcher/client names where string-backed evidence exists.
// For the login state machine, the most concrete current names come from recovered
// diagnostic/log strings rather than fully named public RTTI.
// Keep those names in source scaffolds even while behavior remains skeletal.
//
// Practical policy note:
// - when these methods later gain real behavior, prefer reusing the original log/error strings
//   where appropriate instead of inventing fresh text
// - that keeps the implementation anchored to disassembly and makes log comparison easier
class CLTLoginState {
public:
    virtual ~CLTLoginState() = default;

    virtual const char* DebugName() const = 0;

    // Current owner-state dispatcher (`launcher.exe:0x439300`) consults an object through
    // vtable `+0x18` before deciding which connection-init path to take.
    // The exact semantic name of that query is still open, so keep this generic for now.
    virtual uint32_t DispatchPhaseCode() const;
};

// String-backed original name recovered from launcher/client logging:
// - CLTLoginState_WorldListPending::AuthMessageDispatch()
class CLTLoginState_WorldListPending : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_WorldListPending::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_WorldListPending::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    const char* DebugName() const override;

    // Current best contextual role:
    // - auth-server/world-list response handling
    // - current canonical string anchors include:
    //   - "AS_GetWorldListReply"
    //   - "AS_PSGetWorldListReply"
    //   - CLTLoginState_WorldListPending::AuthMessageDispatch()...
    uint32_t AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator);
};

// String-backed original name recovered from launcher/client logging:
// - CLTLoginState_AuthenticatePending::AuthMessageDispatch()
class CLTLoginState_AuthenticatePending : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidCharacterStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): Character %s (gcid = %I64u) has an invalid status (%d)!  Forcing it to AUTHDBCHARSTATUS_INVALID.";
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    const char* DebugName() const override;

    // Current best contextual role:
    // - auth-server authentication/character-selection response handling
    // - current canonical string anchors include:
    //   - "AS_AuthReply"
    //   - "AS_AuthChallenge"
    //   - "AS_AuthChallengeResponse"
    //   - "AS_PSAuthenticateReply"
    //   - CLTLoginState_AuthenticatePending::AuthMessageDispatch()...
    uint32_t AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator);
};

}  // namespace mxo::ltlogin
