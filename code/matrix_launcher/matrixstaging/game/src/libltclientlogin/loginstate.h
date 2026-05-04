#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
    // `CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader_GetWorkType(workItem)`.
    // Uses g_CurrentLoginMediator instead of passing mediator as parameter (faithful to static-RE).
    virtual uint32_t Slot1_HandlePrimaryGate(void* workItem);

    // anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot2_HandleSecondaryGate(void* workItem);

    // anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by selected slot-3 rows)
    // Valid as a reused stub address, but not the canonical meaning of slot 3 across the family:
    // most live states override slot 3 with their real begin/continue body.
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg);

    // anchor: launcher.exe:0x00441790 (shared raw `ret` stub reused by many slot-4 rows)
    virtual uint32_t Slot4_NoOp();

    // anchor: launcher.exe:0x004397c0 (shared slot-5 failure stub on many vtables)
    // Naming correction: slot 5 should stay `AuthMessageDispatch` across the family.
    virtual uint32_t AuthMessageDispatch(void* workItem);

    // anchor: launcher.exe:0x004397c0 (shared slot-6 failure stub on selected vtables only)
    // Valid as a reused default body, but many live states override slot 6 with concrete receive /
    // completion handlers.
    // Uses g_CurrentLoginMediator (faithful to static-RE).
    virtual uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem);

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

protected:
    CLTLoginState* cachedUpstreamOrArg_0x4 = nullptr;
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
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

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
class CLTLoginState_State0_0x4b51e0 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b51e0 slot 10 shared initializer)
    CLTLoginState_State0_0x4b51e0() = default;

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
class CLTLoginState_State1_0x4b4fc4 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439060 (vtable 0x004b4fc4 slot 10 initializer)
    CLTLoginState_State1_0x4b4fc4() = default;

public:
    // anchor: launcher.exe vtable 0x004b4fc4
    const char* DebugName() const override { return "CLTLoginState_State1_0x4b4fc4"; }

    // anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
    uint32_t Slot1_HandlePrimaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5014
// docs: ../../docs/launcher.exe/VTABLES/0x004b5014.md
class CLTLoginState_AuthenticatePending_0x4b5014 : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidCharacterStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): Character %s (gcid = %I64u) has an invalid status (%d)!  Forcing it to AUTHDBCHARSTATUS_INVALID.";
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_AuthenticatePending::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    // anchor: launcher.exe:0x004391e0 (vtable 0x004b5014 slot 10 initializer)
    CLTLoginState_AuthenticatePending_0x4b5014() = default;

    // anchor: launcher.exe vtable 0x004b5014
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

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
class CLTLoginState_State3_0x4b5208 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b5208 slot 10 shared initializer)
    CLTLoginState_State3_0x4b5208() = default;

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
class CLTLoginState_State4_0x4b503c : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004392a0 (vtable 0x004b503c slot 10 initializer)
    CLTLoginState_State4_0x4b503c() = default;

    // anchor: launcher.exe vtable 0x004b503c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5064
// docs: ../../docs/launcher.exe/VTABLES/0x004b5064.md
class CLTLoginState_State5_0x4b5064 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004394f0 (vtable 0x004b5064 slot 10 initializer)
    CLTLoginState_State5_0x4b5064() = default;

    // anchor: launcher.exe vtable 0x004b5064
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b508c
// docs: ../../docs/launcher.exe/VTABLES/0x004b508c.md
class CLTLoginState_State6_0x4b508c : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004395f0 (vtable 0x004b508c slot 10 initializer)
    CLTLoginState_State6_0x4b508c() = default;

    // anchor: launcher.exe vtable 0x004b508c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b50b4
// docs: ../../docs/launcher.exe/VTABLES/0x004b50b4.md
// Provisional better-name suggestion: `CLTLoginState_MarginRouteProbePending`
class CLTLoginState_State7_0x4b50b4 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439620 (vtable 0x004b50b4 slot 10 initializer)
    CLTLoginState_State7_0x4b50b4() = default;

    // anchor: launcher.exe vtable 0x004b50b4
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5104
// docs: ../../docs/launcher.exe/VTABLES/0x004b5104.md
// Provisional better-name suggestion: `CLTLoginState_MarginLoadCharacterPending`
// Focused source home for the active state8 body:
// - `loginstate_state8.cpp`
class CLTLoginState_State8_0x4b5104 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004396c0 (vtable 0x004b5104 slot 10 initializer)
    CLTLoginState_State8_0x4b5104() = default;

private:
    // `0x43f930` uses byte-sized fields on the 8-byte state object at `this+4/+5` as
    // reply-fragment progress counters. Keep the offset suffixes explicit so source stays aligned
    // with the static-RE-backed helper-local storage.
    uint8_t replySectionsSeen04_ = 0;
    uint8_t replySectionsExpected05_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b5104
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043bd20 (vtable 0x004b5104 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438c90 (vtable 0x004b5104 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b517c
// docs: ../../docs/launcher.exe/VTABLES/0x004b517c.md
// Provisional better-name suggestion: `CLTLoginState_LoadCharacterFollowupPending`
// Focused source home for the active late-login body:
// - `loginstate_state9.cpp`
class CLTLoginState_State9_0x4b517c : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439750 (vtable 0x004b517c slot 10 initializer)
    CLTLoginState_State9_0x4b517c() = default;

private:
    // `0x00439780` consumes a local helper payload from `this+4/+6`, while the
    // state8/state11 completion tails also prove a distinct live byte write at `this+5`.
    // Newer natural-original WineDbg now confirms a representative slot-3 handoff as:
    // - `this+4 = 0`
    // - `this+6 = 0x2710`
    // And disassembly proves the additional producer writes:
    // - `0x43f984`: `MOV byte ptr [EDI + 0x5], DL`
    // - `0x440373`: `MOV byte ptr [EDI + 0x5], AL`
    uint8_t pendingByte4_ = 0;
    uint8_t pendingReplySectionCount5_ = 0;
    uint16_t pendingWord6_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b517c
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439780 (vtable 0x004b517c slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043c180 (vtable 0x004b517c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // Fidelity: original producer tails write helper-local payload bytes at `this+4/+5`
    // plus the handoff word at `this+6` before switching into helper9/state9.
    // Current source mirrors that explicit handoff shape even though `0x439780` only
    // consumes `this+4` and `this+6` directly.
    void SetPendingPayload(uint8_t byte4, uint8_t replySectionCount5, uint16_t word6) {
        pendingByte4_ = byte4;
        pendingReplySectionCount5_ = replySectionCount5;
        pendingWord6_ = word6;
    }

    // anchor: launcher.exe:0x00438cc0 (vtable 0x004b517c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b512c
// docs: ../../docs/launcher.exe/VTABLES/0x004b512c.md
// Provisional better-name suggestion: `CLTLoginState_ClaimCharacterNamePending`
class CLTLoginState_State10_0x4b512c : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004396f0 (vtable 0x004b512c slot 10 initializer)
    CLTLoginState_State10_0x4b512c() = default;

    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b5154
// docs: ../../docs/launcher.exe/VTABLES/0x004b5154.md
// Provisional better-name suggestion: `CLTLoginState_CreateCharacterLoadPending`
class CLTLoginState_State11_0x4b5154 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439720 (vtable 0x004b5154 slot 10 initializer)
    CLTLoginState_State11_0x4b5154() = default;

private:
    // `0x440320` uses two byte-sized fields on the 8-byte helper11 object at `this+4/+5` as
    // reply-fragment progress counters. Keep the offset suffixes explicit so source stays aligned
    // with the static-RE-backed helper-local storage.
    uint8_t replySectionsSeen04_ = 0;
    uint8_t replySectionsExpected05_ = 0;

public:
    // anchor: launcher.exe vtable 0x004b5154
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

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
class CLTLoginState_State12_0x4b5230 : public CLTLoginState_AbstractFinalLeafBase {
public:
    // anchor: launcher.exe:0x00439d80 (vtable 0x004b5230 slot 10 shared initializer)
    CLTLoginState_State12_0x4b5230();

    // anchor: launcher.exe vtable 0x004b5230
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00438d00 (vtable 0x004b5230 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b50dc
// docs: ../../docs/launcher.exe/VTABLES/0x004b50dc.md
// Provisional better-name suggestion: `CLTLoginState_LateMarginRouteProbePending`
class CLTLoginState_State13_0x4b50dc : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00439650 (vtable 0x004b50dc slot 10 initializer)
    CLTLoginState_State13_0x4b50dc() = default;

    // anchor: launcher.exe vtable 0x004b50dc
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
    uint32_t Slot2_HandleSecondaryGate(void* workItem) override;

    // anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b4fec
// docs: ../../docs/launcher.exe/VTABLES/0x004b4fec.md
class CLTLoginState_WorldListPending_0x4b4fec : public CLTLoginState {
public:
    static constexpr const char* kLogInvalidWorldType =
        "CLTLoginState_WorldListPending_0x4b4fec::AuthMessageDispatch(): World %s (id = %d) has an invalid type (%d)!  Forcing it to WORLDTYPE_INVALID.";
    static constexpr const char* kLogInvalidWorldStatus =
        "CLTLoginState_WorldListPending_0x4b4fec::AuthMessageDispatch(): World %s (id = %d) has an invalid status (%d)!  Forcing it to WORLDSTATUS_INVALID.";

    // anchor: launcher.exe:0x004391b0 (vtable 0x004b4fec slot 10 initializer)
    CLTLoginState_WorldListPending_0x4b4fec() = default;

    // anchor: launcher.exe vtable 0x004b4fec
    const char* DebugName() const override;

    // anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x0043d4d0 (vtable 0x004b4fec slot 5)
    uint32_t AuthMessageDispatch(void* workItem) override;

    // anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b0b88
// docs: ../../docs/launcher.exe/VTABLES/0x004b0b88.md
class CLTLoginState_State15_0x4b0b88 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420650 (vtable 0x004b0b88 slot 10 initializer)
    CLTLoginState_State15_0x4b0b88() = default;

    // anchor: launcher.exe vtable 0x004b0b88
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0bb0
// docs: ../../docs/launcher.exe/VTABLES/0x004b0bb0.md
class CLTLoginState_State16_0x4b0bb0 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004206f0 (vtable 0x004b0bb0 slot 10 initializer)
    CLTLoginState_State16_0x4b0bb0() = default;

    // anchor: launcher.exe vtable 0x004b0bb0
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0bd8
// docs: ../../docs/launcher.exe/VTABLES/0x004b0bd8.md
class CLTLoginState_State17_0x4b0bd8 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420860 (vtable 0x004b0bd8 slot 10 initializer)
    CLTLoginState_State17_0x4b0bd8() = default;

    // anchor: launcher.exe vtable 0x004b0bd8
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
    uint32_t GetStateId() const override;
};

// anchor: launcher.exe vtable 0x004b0c00
// docs: ../../docs/launcher.exe/VTABLES/0x004b0c00.md
class CLTLoginState_State18_0x4b0c00 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x00420930 (vtable 0x004b0c00 slot 10 initializer)
    CLTLoginState_State18_0x4b0c00() = default;

    // anchor: launcher.exe vtable 0x004b0c00
    const char* DebugName() const override;

    // anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

    // anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
    uint32_t GetStateId() const override;

    // anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
    uint32_t Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context) override;
};

// anchor: launcher.exe vtable 0x004b0c28
// docs: ../../docs/launcher.exe/VTABLES/0x004b0c28.md
class CLTLoginState_State19_0x4b0c28 : public CLTLoginState {
public:
    // anchor: launcher.exe:0x004209b0 (vtable 0x004b0c28 slot 10 initializer)
    CLTLoginState_State19_0x4b0c28() = default;

    // anchor: launcher.exe vtable 0x004b0c28
    const char* DebugName() const override;

    // anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
    void Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) override;

    // anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
    uint32_t Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) override;

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
class Packet_MsLoadCharacterReply_0x4b542c : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // Static-RE shape from `0x43ae50` / `0x43af20`:
    // - binary object writes stay within the shared packet prefix (`+0x04/+0x08/+0x0c/+0x10/+0x14/+0x18`)
    // - the default empty reply body is a 0x10-byte inline source-owned scratch buffer layered on top
    //   of source fields below, not proof of a larger launcher.exe child-object ABI
    // - all semantic decode fields (`valid/status/field05/...`) are source-owned cached views of the
    //   packet bytes and must not be treated as client-facing object layout proof
    // More faithful constructor using CMessageConnectionMessageRef_0x4ba23c directly.
    // Static-RE now treats this as a Packet_0x4af2a4-derived 5-slot vtable surface:
    // - slots 0/1/4 inherited from Packet_0x4af2a4
    // - slot 2 overridden by 0x43cca0
    // - slot 3 overridden by 0x43af20
    // anchor: launcher.exe:0x43ae50
    Packet_MsLoadCharacterReply_0x4b542c(
        mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
        char initializeEmptyReply);

    ~Packet_MsLoadCharacterReply_0x4b542c() override = default;

    // anchor: launcher.exe:0x43cca0 / vtable +0x08
    void DebugString(int formatType = 2) override;

    // anchor: launcher.exe:0x43af20 / vtable +0x0c
    void InitializePayloadSize() override;

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

    uint8_t* messageBase04() const {
        return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(payloadPtr04));
    }

    void setMessageBase04(uint8_t* messageBase) {
        payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(messageBase));
    }

    uint8_t* currentMessage10() const {
        return static_cast<uint8_t*>(payloadAlias10);
    }

    void setCurrentMessage10(uint8_t* currentMessage) {
        payloadAlias10 = currentMessage;
    }

    const uint8_t* dataSectionBytes14() const {
        return reinterpret_cast<const uint8_t*>(debugString14);
    }

    void setDataSectionBytes14(const uint8_t* dataSectionBytes) {
        debugString14 = reinterpret_cast<const char*>(dataSectionBytes);
    }

};

}  // namespace mxo::ltlogin
