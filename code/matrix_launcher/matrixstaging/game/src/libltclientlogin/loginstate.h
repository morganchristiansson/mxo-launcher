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
// Recovered source-file anchor:
// - `\matrixstaging\game\src\libltclientlogin\loginstate.cpp`
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
    // - current concrete packet-side anchor now firmly includes later incoming `AS_AuthReply`
    //   handling on the auth path through helper body `0x4401a0`
    // - important correction: `0x4401a0` is not the generic owner `+0x17c` callback itself
    //   - owner `+0x17c` is thunk `0x41f260`
    //   - that thunk forwards to the owner's current helper/state object at `+0x10`, then jumps
    //     to helper vtable `+0x14`
    //   - `0x4401a0` is one later helper-state `+0x14` body (`0x4f7890` / `0x4b512c`)
    // - that path now also narrows one important negative point:
    //   - `0x4401a0` parses later `AS_AuthReply`, updates owner-side state, and then reaches
    //     `CLTLoginMediator::PostEvent(0x14)` or `PostError(0x0b)` depending on reply result
    //   - it is therefore not direct proof of the first outbound auth request after connect
    // - the earlier exact `GetPublicKey` request/reply ordering is still not settled enough to
    //   present as a fixed sequence here
    // - current canonical string anchors include:
    //   - "AS_GetPublicKeyRequest"
    //   - "AS_GetPublicKeyReply"
    //   - "AS_AuthReply"
    //   - "AS_AuthChallenge"
    //   - "AS_AuthChallengeResponse"
    //   - "AS_PSAuthenticateReply"
    //   - CLTLoginState_AuthenticatePending::AuthMessageDispatch()...
    uint32_t AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator);
};

}  // namespace mxo::ltlogin
