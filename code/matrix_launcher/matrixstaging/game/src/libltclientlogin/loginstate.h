#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../runtime/src/libltmessaging/messageconnection.h"

namespace mxo::ltlogin {

class CLTLoginMediator;

uint32_t RecoverCachedUpstreamPhaseCode(const void* cachedUpstreamOrArg);

// anchor: launcher.exe:0x004397d0 (slot 3 no-op stub on multiple vtables)
uint32_t PlaceholderStateAction(const char* debugName, const char* anchor);

// anchor: reconstructed shared login-state surface spanning launcher.exe vtable families
// - 0x004b0b88
// - 0x004b0bb0
// - 0x004b0bd8
// - 0x004b0c00
// - 0x004b0c28
// - 0x004b4fc4
// - 0x004b4fec
// - 0x004b5014
// - 0x004b503c
// - 0x004b5064
// - 0x004b508c
// - 0x004b50b4
// - 0x004b50dc
// - 0x004b5104
// - 0x004b512c
// - 0x004b5154
// - 0x004b517c
// - 0x004b51b8
// - 0x004b51e0
// - 0x004b5208
// - 0x004b5230
class CLTLoginState {
public:
    virtual ~CLTLoginState() = default;

    // anchor: reconstructed shared login-state family surface
    virtual const char* DebugName() const = 0;

    // anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
    // Exact launcher call shape keeps the queued work item on the stack and reads its type through
    // `CLTThreadPerClientTCPEngine_WorkItemHeader_GetWorkType(workItem)`.
    // Uses g_CurrentLoginMediator instead of passing mediator as parameter (faithful to static-RE).
    virtual uint32_t Slot1_HandlePrimaryGate(void* workItem);

    // anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot2_HandleSecondaryGate(void* workItem);

    // anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-3 rows)
    // Valid as a reused stub address, but not the canonical meaning of slot 3 across the family:
    // most live states override slot 3 with their real begin/continue body.
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot3_BeginOrContinue(void* upstreamOrArg);

    // anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by many slot-4 rows)
    virtual uint32_t Slot4_NoOp();

    // anchor: launcher.exe:0x004397c0 (shared slot-5 failure stub on many vtables)
    // Naming correction: slot 5 should stay `AuthMessageDispatch` across the family.
    virtual uint32_t AuthMessageDispatch(void* workItem);

    // anchor: launcher.exe:0x004397c0 (shared slot-6 failure stub on selected vtables only)
    // Valid as a reused default body, but many live states override slot 6 with concrete receive /
    // completion handlers.
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem);

    // anchor: reconstructed shared slot 7 state-id surface
    virtual uint32_t GetStateId() const = 0;

    // anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-8 rows)
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context);

    // anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
    virtual uint32_t Slot9_IsNetworkDriven() const;

    // anchor: launcher.exe:0x00439300 and launcher.exe:0x004439300 consult slot-7-style helper codes;
    // reimplementation wrapper forwards to GetStateId().
    virtual uint32_t DispatchPhaseCode() const;
};

// anchor: launcher.exe vtable 0x004b51b8
// docs: ../../docs/launcher.exe/VTABLES/0x004b51b8.md
class CLTLoginState_AbstractFinalLeafBase : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b51b8 slot 10 initializer)
    CLTLoginState_AbstractFinalLeafBase() = default;

protected:
    // anchor: launcher.exe:0x004397e0 shared final-leaf slot-6 byte gate at `this+4`
    // - state0 / state3 keep this byte `0`
    // - state12 initializes it to `1`, which enables owner-callback84 secondary dispatch
    uint8_t slot6DispatchByte4_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b51b8
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x0048bc34 (vtable 0x004b51b8 slot 7 purecall)
    uint32_t GetStateId() const override = 0;

    // anchor: launcher.exe:0x00437b40 (vtable 0x004b51b8 slot 9)
    uint32_t Slot9_IsNetworkDriven() const override;
};

// anchor: launcher.exe vtable 0x004b51e0
// docs: ../../docs/launcher.exe/VTABLES/0x004b51e0.md
// Current tighter startup role:
// - mediator init installs this as the initial idle/start current helper
// - slot 3 stays the inherited shared no-op stub
// - the first happy-path submit transition remains owner-owned
//   (`CLTLoginMediator::ProcessLoginRequest`), which switches into helper/state `2`
// So state0 is the startup helper, not the startup submit coordinator.
class CLTLoginState_State0 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b51e0 slot 10 shared initializer)
    CLTLoginState_State0() = default;

    // Intentionally no Slot3 override here:
    // original slot 3 is the shared no-op stub, so submit ownership stays on the mediator/owner
    // path rather than moving into a state0-local body.

    // anchor: launcher.exe vtable 0x004b51e0
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b4fc4
// docs: ../../docs/launcher.exe/VTABLES/0x004b4fc4.md
// Provisional better-name suggestion: `CLTLoginState_AuthConnectPending`
class CLTLoginState_State1 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439060 (vtable 0x004b4fc4 slot 10 initializer)
    CLTLoginState_State1() = default;

private:
    // anchor: launcher.exe:0x00439090 stores the upstream/helper object at `this+4` before
    // starting the auth connection.
    void* cachedUpstreamOrArg_ = nullptr;

public:
    // anchor: launcher.exe vtable 0x004b4fc4
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
    uint32_t Slot1_HandlePrimaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5014
// docs: ../../docs/launcher.exe/VTABLES/0x004b5014.md
class CLTLoginState_AuthenticatePending : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidCharacterStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): Character %s (gcid = %I64u) has an invalid status (%d)!  Forcing it to AUTHDBCHARSTATUS_INVALID.";
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    // anchor: launcher.exe:0x004391e0 (vtable 0x004b5014 slot 10 initializer)
    CLTLoginState_AuthenticatePending() = default;

private:
    // anchor: launcher.exe:0x00439210 stores the upstream/helper object at `this+4` unless the
    // incoming helper already reports phase/state code `1` through vtable `+0x18`.
    void* cachedUpstreamOrArg_ = nullptr;

public:
    // anchor: launcher.exe vtable 0x004b5014
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043f300 (vtable 0x004b5014 slot 5)
    uint32_t AuthMessageDispatch(void* workItem) override;

    // anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5208
// docs: ../../docs/launcher.exe/VTABLES/0x004b5208.md
// Provisional better-name suggestion: `CLTLoginState_SelectionContextPending`
// Current practical role on the proven happy path:
// - state2 switches into state3
// - state3 then waits as the current helper while owner-side mediator methods
//   `0x41c390/0x41c1f0` consume selection-context input and advance into states `7/8`
// - no extra early helper-switch hits were observed between that `state2 -> state3` switch and the
//   live `0x41c1f0` stop
// So keep state3 as the waiting/helper-id leaf here rather than inventing a state3-local slot-3
// body.
class CLTLoginState_State3 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b5208 slot 10 shared initializer)
    CLTLoginState_State3() = default;

    // Intentionally no Slot3 override here:
    // the current practical advance out of state3 belongs to the owner-side mediator methods
    // `0x41c390/0x41c1f0`, while vtable slot 3 still resolves to the shared tiny stub.

    // anchor: launcher.exe vtable 0x004b5208
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00438cf0 (vtable 0x004b5208 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b503c
// docs: ../../docs/launcher.exe/VTABLES/0x004b503c.md
class CLTLoginState_State4 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004392a0 (vtable 0x004b503c slot 10 initializer)
    CLTLoginState_State4() = default;

private:
    // anchor: launcher.exe:0x00439300 stores the first incoming upstream/helper pointer at `this+4`
    // and then reuses that cached object for the later vtable `+0x18` case split.
    void* cachedUpstreamOrArg_ = nullptr;

public:

    // anchor: launcher.exe vtable 0x004b503c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5064
// docs: ../../docs/launcher.exe/VTABLES/0x004b5064.md
class CLTLoginState_State5 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004394f0 (vtable 0x004b5064 slot 10 initializer)
    CLTLoginState_State5() = default;

private:
    // anchor: launcher.exe:0x00439520 stores a cached upstream/helper pointer at `this+4`.
    void* cachedUpstreamOrArg_ = nullptr;

public:
    // anchor: launcher.exe vtable 0x004b5064
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b508c
// docs: ../../docs/launcher.exe/VTABLES/0x004b508c.md
class CLTLoginState_State6 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004395f0 (vtable 0x004b508c slot 10 initializer)
    CLTLoginState_State6() = default;

private:
    // anchor: launcher.exe:0x0043b8f0 stores a cached upstream/helper pointer at `this+4`.
    // `0x00440780` then uses that cached object's vtable `+0x18` to choose the next helper-state
    // target after opcode-`9` success.
    void* cachedUpstreamOrArg_ = nullptr;

public:
    // anchor: launcher.exe vtable 0x004b508c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b50b4
// docs: ../../docs/launcher.exe/VTABLES/0x004b50b4.md
// Provisional better-name suggestion: `CLTLoginState_MarginRouteProbePending`
class CLTLoginState_State7 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439620 (vtable 0x004b50b4 slot 10 initializer)
    CLTLoginState_State7() = default;

    // anchor: launcher.exe vtable 0x004b50b4
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5104
// docs: ../../docs/launcher.exe/VTABLES/0x004b5104.md
// Provisional better-name suggestion: `CLTLoginState_MarginLoadCharacterPending`
// Focused source home for the active state8 body:
// - `loginstate_state8.cpp`
class CLTLoginState_State8 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004396c0 (vtable 0x004b5104 slot 10 initializer)
    CLTLoginState_State8() = default;

private:
    // `0x43f930` uses byte-sized fields on the 8-byte state object at `this+4/+5` as
    // reply-fragment progress counters. Keep them state-owned here instead of smearing them onto
    // the mediator.
    uint8_t replySectionsSeen_ = 0;
    uint8_t replySectionsExpected_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b5104
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043bd20 (vtable 0x004b5104 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438c90 (vtable 0x004b5104 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b517c
// docs: ../../docs/launcher.exe/VTABLES/0x004b517c.md
// Provisional better-name suggestion: `CLTLoginState_LoadCharacterFollowupPending`
// Focused source home for the active late-login body:
// - `loginstate_state9.cpp`
class CLTLoginState_State9 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439750 (vtable 0x004b517c slot 10 initializer)
    CLTLoginState_State9() = default;

private:
    // `0x00439780` consumes a local byte/word payload from `this+4/+6`.
    // Newer natural-original WineDbg now confirms a representative live handoff there as:
    // - `this+4 = 0`
    // - `this+6 = 0x2710`
    uint8_t pendingByte4_ = 0;
    uint8_t padding5_ = 0;
    uint16_t pendingWord6_ = 0;

public:
    void SetPendingPayload(uint8_t byte4, uint16_t word6) {
        pendingByte4_ = byte4;
        pendingWord6_ = word6;
    }

    bool HasPendingPayload() const {
        return pendingByte4_ != 0u || pendingWord6_ != 0u;
    }

    // anchor: launcher.exe vtable 0x004b517c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439780 (vtable 0x004b517c slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043c180 (vtable 0x004b517c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438cc0 (vtable 0x004b517c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b512c
// docs: ../../docs/launcher.exe/VTABLES/0x004b512c.md
// Provisional better-name suggestion: `CLTLoginState_ClaimCharacterNamePending`
class CLTLoginState_State10 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004396f0 (vtable 0x004b512c slot 10 initializer)
    CLTLoginState_State10() = default;

    // UNANCHORED: source-owned shared owner-state writeback helper used by the broader state2
    // slot-5 auth-reply success path (`0x43f300`) and by the current existing-character auth
    // bridge. Keep this separate from the later state10 margin-side claim-name reply append at
    // `0x4401a0`.
    static void AdoptAuthReplyIntoRecoveredMediatorStateScaffold(CLTLoginMediator* mediator);

    // anchor: launcher.exe vtable 0x004b512c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5154
// docs: ../../docs/launcher.exe/VTABLES/0x004b5154.md
// Provisional better-name suggestion: `CLTLoginState_CreateCharacterLoadPending`
class CLTLoginState_State11 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439720 (vtable 0x004b5154 slot 10 initializer)
    CLTLoginState_State11() = default;

private:
    // `0x440320` uses two byte-sized fields on the 8-byte helper11 object at `this+4/+5` as
    // reply-fragment progress counters. Keep them state-owned here rather than smearing them onto
    // the mediator.
    uint8_t replySectionsSeen_ = 0;
    uint8_t replySectionsExpected_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b5154
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5230
// docs:
// - ../../docs/launcher.exe/VTABLES/0x004b5230.md
// - ../../docs/launcher.exe/state_machine/POST_STATE9_CONTINUATION.md
// Focused source home for the current state-0x0c leaf identity:
// - `loginstate_state12.cpp`
// Provisional better-name suggestion: `CLTLoginState_FinalMarginLeaf12`
class CLTLoginState_State12 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b5230 slot 10 shared initializer)
    CLTLoginState_State12();

    // anchor: launcher.exe vtable 0x004b5230
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00438d00 (vtable 0x004b5230 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b50dc
// docs: ../../docs/launcher.exe/VTABLES/0x004b50dc.md
// Provisional better-name suggestion: `CLTLoginState_LateMarginRouteProbePending`
class CLTLoginState_State13 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439650 (vtable 0x004b50dc slot 10 initializer)
    CLTLoginState_State13() = default;

    // anchor: launcher.exe vtable 0x004b50dc
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b4fec
// docs: ../../docs/launcher.exe/VTABLES/0x004b4fec.md
class CLTLoginState_WorldListPending : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_WorldListPending::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_WorldListPending::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    // anchor: launcher.exe:0x004391b0 (vtable 0x004b4fec slot 10 initializer)
    CLTLoginState_WorldListPending() = default;

    // anchor: launcher.exe vtable 0x004b4fec
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043d4d0 (vtable 0x004b4fec slot 5)
    uint32_t AuthMessageDispatch(void* workItem) override;

    // anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b0b88
// docs: ../../docs/launcher.exe/VTABLES/0x004b0b88.md
class CLTLoginState_State15 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420650 (vtable 0x004b0b88 slot 10 initializer)
    CLTLoginState_State15() = default;

    // anchor: launcher.exe vtable 0x004b0b88
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0bb0
// docs: ../../docs/launcher.exe/VTABLES/0x004b0bb0.md
class CLTLoginState_State16 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004206f0 (vtable 0x004b0bb0 slot 10 initializer)
    CLTLoginState_State16() = default;

    // anchor: launcher.exe vtable 0x004b0bb0
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0bd8
// docs: ../../docs/launcher.exe/VTABLES/0x004b0bd8.md
class CLTLoginState_State17 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420860 (vtable 0x004b0bd8 slot 10 initializer)
    CLTLoginState_State17() = default;

    // anchor: launcher.exe vtable 0x004b0bd8
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b0c00
// docs: ../../docs/launcher.exe/VTABLES/0x004b0c00.md
class CLTLoginState_State18 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420930 (vtable 0x004b0c00 slot 10 initializer)
    CLTLoginState_State18() = default;

    // anchor: launcher.exe vtable 0x004b0c00
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0c28
// docs: ../../docs/launcher.exe/VTABLES/0x004b0c28.md
class CLTLoginState_State19 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004209b0 (vtable 0x004b0c28 slot 10 initializer)
    CLTLoginState_State19() = default;

    // anchor: launcher.exe vtable 0x004b0c28
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
    uint32_t Slot3_BeginOrContinue(void* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef* workItem) override;

    // anchor: launcher.exe:0x00420350 (vtable 0x004b0c28 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x00420a00 (vtable 0x004b0c28 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// Inline helpers for little-endian parsing
inline uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// anchor: launcher.exe:0x4b542c
class LoadCharacterReplyEnvelope_0x4b542c {
public:
    // More faithful constructor using CMessageConnectionMessageRef directly
    // anchor: launcher.exe:0x43ae50
    LoadCharacterReplyEnvelope_0x4b542c(
        mxo::liblttcp::CMessageConnectionMessageRef* incomingMessageRef,
        char initializeEmptyReply);

    ~LoadCharacterReplyEnvelope_0x4b542c() {
        // Release the message ref if we hold one
        if (incomingMessageRef08_ != nullptr) {
            incomingMessageRef08_->Release();
        }
    }

    // anchor: launcher.exe:0x43ae00
    void RefreshDataSectionView(char initializeEmptyReply);

    // anchor: launcher.exe:0x43af20
    void ResetToDefaultMessage();

    // anchor: launcher.exe:0x43cca0
    void AppendDebugString(std::string& out, int verbosityLevel) const;

    bool valid = false;
    uint32_t status = 0;
    uint32_t field05 = 0;
    uint16_t handoffWord09 = 0;
    uint8_t expectedSectionCount0b = 0;
    bool shouldSeedExpectedSectionCount = false;
    uint8_t sectionSelectorMinus2 = 0xff;
    uint16_t sectionOffset0e = 0;
    uint16_t sectionByteCount = 0;
    const uint8_t* sectionData = nullptr;

private:
    // vtable pointer - initially set to temp vtable, then swapped to real vtable
    // anchor: launcher.exe:0x4b542c
    void** vftptr_0x0 = nullptr;
    uint8_t* messageBase04_ = nullptr;  // +0x04
    mxo::liblttcp::CMessageConnectionMessageRef* incomingMessageRef08_ = nullptr;  // +0x08
    bool initializeEmptyReply0c_ = false;  // +0x0c
    uint8_t* currentMessage10_ = nullptr;  // +0x10
    const uint8_t* dataSectionBytes14_ = nullptr;  // +0x14
    uint16_t dataSectionByteCount18_ = 0;  // +0x18
    // +0x1c: defaultMessageStorage_ inline array
    std::array<uint8_t, 0x10> defaultMessageStorage_{};
};

}  // namespace mxo::ltlogin
