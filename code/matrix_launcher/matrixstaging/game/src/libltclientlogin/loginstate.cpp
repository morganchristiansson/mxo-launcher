#include "loginstate.h"

#include "loginmediator.h"
#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {
namespace {

struct State11Packet0x4dFixedPayload {
    // anchor: launcher.exe:0x43a470 / packet payload tag written after the outer builder reserves
    // a fixed 0x4d-byte payload span through the shared envelope object.
    static constexpr uint8_t kPayloadTag0c = 0x0c;
    static constexpr size_t kRealFirstNameOffset = 0x45;
    static constexpr size_t kRealLastNameOffset = 0x47;
    static constexpr size_t kBackgroundOffset = 0x49;
    static constexpr size_t kGameSessionIdOffset = 0x4b;
    static constexpr size_t kFixedByteCount = 0x4d;
    static constexpr size_t kMaxPayloadByteCount = 0xffc;
};

class RecoveredPacketBuilderEnvelope {
public:
    // anchor: launcher.exe:0x439840
    RecoveredPacketBuilderEnvelope() {
        // Current best source-owned mirror of the local helper object initialized by `0x439840`:
        // - acquires/installs a shared payload object
        // - stores the active payload write base as `shared + 0x0c`
        payloadBytes_.reserve(State11Packet0x4dFixedPayload::kMaxPayloadByteCount);
    }

    uint8_t* PayloadBase() {
        return payloadBytes_.empty() ? nullptr : payloadBytes_.data();
    }

    const uint8_t* PayloadBase() const {
        return payloadBytes_.empty() ? nullptr : payloadBytes_.data();
    }

    uint32_t PayloadByteCount() const {
        return static_cast<uint32_t>(payloadBytes_.size());
    }

    const std::vector<uint8_t>& PayloadBytes() const {
        return payloadBytes_;
    }

protected:
    void ResetPayloadToFixedByteCount0x4d() {
        payloadBytes_.assign(State11Packet0x4dFixedPayload::kFixedByteCount, 0);
    }

    void ResizePayload(size_t fixedByteCount) {
        payloadBytes_.assign(fixedByteCount, 0);
    }

    void WritePayloadByte(size_t offset, uint8_t value) {
        if (offset < payloadBytes_.size()) {
            payloadBytes_[offset] = value;
        }
    }

    void WritePayloadU16LE(size_t offset, uint16_t value) {
        if (offset + 1 >= payloadBytes_.size()) {
            return;
        }
        payloadBytes_[offset] = static_cast<uint8_t>(value & 0xffu);
        payloadBytes_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
    }

    void WritePayloadU32LE(size_t offset, uint32_t value) {
        if (offset + 3 >= payloadBytes_.size()) {
            return;
        }
        payloadBytes_[offset] = static_cast<uint8_t>(value & 0xffu);
        payloadBytes_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
        payloadBytes_[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffu);
        payloadBytes_[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffu);
    }

    uint16_t AppendLengthPrefixedString(size_t offsetField, const char* text, size_t bound) {
        if (!text) {
            return 0;
        }

        size_t textLength = 0;
        while (textLength < bound && text[textLength] != '\0') {
            ++textLength;
        }
        if (textLength == 0) {
            return 0;
        }
        if (payloadBytes_.size() + 2 > State11Packet0x4dFixedPayload::kMaxPayloadByteCount) {
            return 0;
        }

        const size_t remainingAfterLength =
            State11Packet0x4dFixedPayload::kMaxPayloadByteCount - payloadBytes_.size() - 2;
        const uint16_t storedLength = static_cast<uint16_t>(std::min(textLength, remainingAfterLength));
        const uint16_t fieldOffset = static_cast<uint16_t>(payloadBytes_.size());
        WritePayloadU16LE(offsetField, fieldOffset);
        payloadBytes_.push_back(static_cast<uint8_t>(storedLength & 0xffu));
        payloadBytes_.push_back(static_cast<uint8_t>((storedLength >> 8) & 0xffu));
        payloadBytes_.insert(payloadBytes_.end(), text, text + storedLength);
        return storedLength;
    }

private:
    std::vector<uint8_t> payloadBytes_;
};

struct State10Packet0x0aFixedPayload {
    static constexpr uint8_t kPayloadTag0a = 0x0a;
    static constexpr size_t kCharacterNameOffset = 0x01;
    static constexpr size_t kFixedByteCount = 0x03;
};

class State10Packet0x0aBuilder final : public RecoveredPacketBuilderEnvelope {
public:
    // anchor: launcher.exe:0x43a1f0
    void ResetAndInitialize() {
        ResizePayload(State10Packet0x0aFixedPayload::kFixedByteCount);
        WritePayloadByte(0x00, State10Packet0x0aFixedPayload::kPayloadTag0a);
        WritePayloadU16LE(State10Packet0x0aFixedPayload::kCharacterNameOffset, 0);
    }

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* text) {
        AppendLengthPrefixedString(State10Packet0x0aFixedPayload::kCharacterNameOffset, text, 0xffffu);
    }
};

class State11Packet0x4dBuilder final : public RecoveredPacketBuilderEnvelope {
public:
    // anchor: launcher.exe:0x43a470
    void ResetAndInitialize() {
        ResetPayloadToFixedByteCount0x4d();
        WritePayloadByte(0x00, State11Packet0x4dFixedPayload::kPayloadTag0c);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kRealFirstNameOffset, 0);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kRealLastNameOffset, 0);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kBackgroundOffset, 0);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kGameSessionIdOffset, 0);
    }

    void SetFixedDword(size_t payloadOffset, uint32_t value) {
        WritePayloadU32LE(payloadOffset, value);
    }

    // anchor: launcher.exe:0x43a640
    void SetRealFirstName(const char* text) {
        AppendLengthPrefixedString(State11Packet0x4dFixedPayload::kRealFirstNameOffset, text, 0x20);
    }

    // anchor: launcher.exe:0x43a740
    void SetRealLastName(const char* text) {
        AppendLengthPrefixedString(State11Packet0x4dFixedPayload::kRealLastNameOffset, text, 0x20);
    }

    // anchor: launcher.exe:0x43a840
    void SetBackground(const char* text) {
        AppendLengthPrefixedString(State11Packet0x4dFixedPayload::kBackgroundOffset, text, 0x20);
    }

    // anchor: launcher.exe:0x43a940
    void SetGameSessionId(const char* text) {
        AppendLengthPrefixedString(State11Packet0x4dFixedPayload::kGameSessionIdOffset, text, 0xffffu);
    }
};

struct State8StructuredMarginPacketFixedPayload {
    static constexpr uint8_t kPayloadTag0f = 0x0f;
    static constexpr size_t kGameSessionIdOffset = 0xb9;
    static constexpr size_t kFixedByteCount = 0xbb;
};

class State8StructuredMarginPacketBuilder final : public RecoveredPacketBuilderEnvelope {
public:
    // anchor: launcher.exe:0x43ac10 = CLTLoginMediatorPacket0x0f_ResetAndInitialize
    void ResetAndInitialize() {
        ResizePayload(State8StructuredMarginPacketFixedPayload::kFixedByteCount);
        WritePayloadByte(0x00, State8StructuredMarginPacketFixedPayload::kPayloadTag0f);
        WritePayloadU32LE(0x01, 0);
        WritePayloadU32LE(0x05, 0);
        WritePayloadU16LE(State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset, 0);
    }

    void SetFixedDword(size_t payloadOffset, uint32_t value) {
        WritePayloadU32LE(payloadOffset, value);
    }

    void SetSelectionBlock(size_t payloadOffset, const std::array<uint32_t, 4>& block) {
        WritePayloadU32LE(payloadOffset + 0x0, block[0]);
        WritePayloadU32LE(payloadOffset + 0x4, block[1]);
        WritePayloadU32LE(payloadOffset + 0x8, block[2]);
        WritePayloadU32LE(payloadOffset + 0xc, block[3]);
    }

    // anchor: launcher.exe:0x43ada0 = CLTLoginMediatorPacket0x0f_SetGameSessionId
    // helper-local length reservation/writeback mirrors launcher.exe:0x43acf0 =
    // CLTLoginMediatorPacket0x0f_ReserveGameSessionId through AppendLengthPrefixedString().
    void SetGameSessionId(const char* text) {
        AppendLengthPrefixedString(State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset, text, 0xffffu);
    }

};

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

struct ParsedState11LoadCharacterReplyScaffold {
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
};

static ParsedState11LoadCharacterReplyScaffold ParseState11LoadCharacterReplyScaffold(
    const std::vector<uint8_t>& bytes) {
    ParsedState11LoadCharacterReplyScaffold out = {};
    if (bytes.size() < 0x10 || bytes[0] != 0x10) {
        return out;
    }

    out.valid = true;
    out.status = ReadU32LE(bytes.data() + 1);
    out.field05 = ReadU32LE(bytes.data() + 5);
    out.handoffWord09 = ReadU16LE(bytes.data() + 9);
    out.expectedSectionCount0b = bytes[0x0b];
    out.shouldSeedExpectedSectionCount = (bytes[0x0c] == 0x01);
    out.sectionSelectorMinus2 = static_cast<uint8_t>(bytes[0x0d] - 2u);
    out.sectionOffset0e = ReadU16LE(bytes.data() + 0x0e);

    if (out.sectionOffset0e != 0u &&
        static_cast<size_t>(out.sectionOffset0e) + 2u <= bytes.size()) {
        out.sectionByteCount = ReadU16LE(bytes.data() + out.sectionOffset0e);
        const size_t payloadOffset = static_cast<size_t>(out.sectionOffset0e) + 2u;
        if (payloadOffset <= bytes.size()) {
            out.sectionData = bytes.data() + payloadOffset;
            const size_t remaining = bytes.size() - payloadOffset;
            if (out.sectionByteCount > remaining) {
                out.sectionByteCount = static_cast<uint16_t>(remaining);
            }
        }
    }

    return out;
}

static uint32_t PlaceholderStateAction(const char* debugName, const char* anchor) {
    (void)debugName;
    (void)anchor;
    return 1;
}

static uint32_t RecoverCachedUpstreamPhaseCode(const void* cachedUpstreamOrArg) {
    // `0x439300` calls the cached upstream/helper object's vtable `+0x18`.
    // The source scaffold keeps that as the shared `DispatchPhaseCode()` wrapper over the
    // recovered login-state family.
    const auto* cachedUpstreamState = static_cast<const CLTLoginState*>(cachedUpstreamOrArg);
    return cachedUpstreamState ? cachedUpstreamState->DispatchPhaseCode() : 0u;
}

static uint32_t BeginMarginConnectionIfResolved(
    CLTLoginMediator* mediator,
    const char* routeHostText,
    uint8_t cachedRouteSelector) {
    if (!mediator || !routeHostText || routeHostText[0] == '\0') {
        return 0u;
    }
    return mediator->BeginMarginConnectionScaffold(routeHostText, cachedRouteSelector);
}

}  // namespace

// anchor: reconstructed shared login-state family surface spanning launcher.exe vtable families
const char* CLTLoginState_AbstractFinalLeafBase::DebugName() const {
    return "CLTLoginState_AbstractFinalLeafBase";
}

// anchor: launcher.exe:0x00438d80 (shared slot 1 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438df0 (shared slot 2 gate across multiple login-state vtables)
uint32_t CLTLoginState::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 3 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00441790 (shared slot 4 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot4_NoOp() {
    return 1;
}

// anchor: launcher.exe:0x004397c0 (shared slot 5 failure stub on multiple vtables)
uint32_t CLTLoginState::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004397c0 (default shared slot 6 failure stub on selected vtables)
uint32_t CLTLoginState::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00441790 (shared slot 8 no-op stub on multiple vtables)
uint32_t CLTLoginState::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00437860 (shared slot 9 getter stub returning 1 on most live states)
uint32_t CLTLoginState::Slot9_IsNetworkDriven() const {
    return 1;
}

// anchor: launcher.exe:0x004439300 consults slot-7-style state/helper ids before margin-route dispatch
uint32_t CLTLoginState::DispatchPhaseCode() const {
    return Slot7_GetStateId();
}

// anchor: launcher.exe:0x004397e0 (vtable 0x004b51b8 slot 6)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00437b40 (vtable 0x004b51b8 slot 9)
uint32_t CLTLoginState_AbstractFinalLeafBase::Slot9_IsNetworkDriven() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b51e0
const char* CLTLoginState_State0::DebugName() const {
    return "CLTLoginState_State0";
}

// anchor: launcher.exe:0x00437b50 (vtable 0x004b51e0 slot 7)
uint32_t CLTLoginState_State0::Slot7_GetStateId() const {
    return 0;
}

// anchor: launcher.exe vtable 0x004b4fc4
const char* CLTLoginState_State1::DebugName() const {
    return "CLTLoginState_State1";
}

// anchor: launcher.exe:0x004390b0 (vtable 0x004b4fc4 slot 1)
uint32_t CLTLoginState_State1::Slot1_HandlePrimaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004390b0");
}

// anchor: launcher.exe:0x00439090 (vtable 0x004b4fc4 slot 3)
uint32_t CLTLoginState_State1::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439090");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b4fc4 slot 6)
uint32_t CLTLoginState_State1::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x0044e360 (vtable 0x004b4fc4 slot 7)
uint32_t CLTLoginState_State1::Slot7_GetStateId() const {
    return 1;
}

// anchor: launcher.exe vtable 0x004b5014
const char* CLTLoginState_AuthenticatePending::DebugName() const {
    return "CLTLoginState_AuthenticatePending";
}

// anchor: launcher.exe:0x00439210 (vtable 0x004b5014 slot 3)
uint32_t CLTLoginState_AuthenticatePending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439210");
}

// anchor: launcher.exe:0x0043f300 (string/file anchors: loginstate.cpp, CLTLoginState_AuthenticatePending::AuthMessageDispatch())
uint32_t CLTLoginState_AuthenticatePending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best recovered role from `0x43f300`:
    // - parses an auth-side message through the mediator-owned `+0x680` helper object
    // - on the success branch (`case 2`) it performs the broader post-auth table writeback before
    //   the narrower helper10 selected-slot path becomes relevant
    // - concrete broader writeback now looks like:
    //   - build owner `+0xd84` as a world-descriptor table
    //   - validate world status/type there
    //   - build owner `+0x688` as a character-slot record table
    //   - validate character status there
    //   - seed owner `+0x818` by matching each character record's world id against the
    //     world-descriptor table and copying the descriptor name
    //   - post event `5`, then switch helper state based on the current helper object
    // Current source ownership note:
    // - the replacement launcher now mirrors the reconstructed `+0xd84/+0x688/+0x818` families
    //   inside `CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState()` so later state-8
    //   margin dispatch can consume reconstructed data instead of only fallback state
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00418150 (vtable 0x004b5014 slot 7)
uint32_t CLTLoginState_AuthenticatePending::Slot7_GetStateId() const {
    return 2;
}

// anchor: launcher.exe vtable 0x004b5208
const char* CLTLoginState_State3::DebugName() const {
    return "CLTLoginState_State3";
}

// anchor: launcher.exe:0x00438cf0 (vtable 0x004b5208 slot 7)
uint32_t CLTLoginState_State3::Slot7_GetStateId() const {
    return 3;
}

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4::DebugName() const {
    return "CLTLoginState_State4";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004393f0");
}

// anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
uint32_t CLTLoginState_State4::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    if (!mediator) {
        return 0u;
    }

    // Faithfulness/ownership correction from the fresh `0x439300` disassembly review:
    // - `0x439300` belongs to `CLTLoginState_State4` vtable `0x004b503c` slot 3
    // - this object caches the first incoming upstream/helper pointer at `this+4`
    // - it then calls that cached object's vtable `+0x18` and uses the returned phase/state code
    //   for the real case split
    // - only the narrow owner-side route getters and `0x41e500` transport/init stay on the
    //   mediator
    if (cachedUpstreamOrArg_ == nullptr) {
        cachedUpstreamOrArg_ = upstreamOrArg;
    }

    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_);
    switch (upstreamPhaseCode) {
        case 6:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteDescriptor(),
                0u);

        case 7:
        case 8:
        case 13:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromCurrentCharacterSlot(),
                mediator->CharacterRouteIndexCc8());

        case 10:
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromDescriptorIndex(mediator->SourceField12c()),
                static_cast<uint8_t>(mediator->SourceField12c() & 0xffu));

        default: {
            // Current source-owned mirror for the default branch's owner `+0x104` dword remains
            // `CurrentMarginRouteState().currentWorldId`; keep the field meaning provisional and
            // only preserve the original `!= -1 -> owner vtable +0xfc -> if non-null call 0x41e500`
            // structure here.
            const int32_t field104Value = mediator->CurrentMarginRouteState().currentWorldId;
            if (field104Value == -1) {
                return 0u;
            }
            return BeginMarginConnectionIfResolved(
                mediator,
                mediator->ResolveMarginRouteFromWorldId(static_cast<uint32_t>(field104Value)),
                0u);
        }
    }
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
uint32_t CLTLoginState_State4::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
uint32_t CLTLoginState_State4::Slot7_GetStateId() const {
    return 4;
}

// anchor: launcher.exe vtable 0x004b5064
const char* CLTLoginState_State5::DebugName() const {
    return "CLTLoginState_State5";
}

// anchor: launcher.exe:0x00439590 (vtable 0x004b5064 slot 2)
uint32_t CLTLoginState_State5::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439590");
}

// anchor: launcher.exe:0x00439520 (vtable 0x004b5064 slot 3)
uint32_t CLTLoginState_State5::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439520");
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b5064 slot 6)
uint32_t CLTLoginState_State5::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return 0;
}

// anchor: launcher.exe:0x00438c60 (vtable 0x004b5064 slot 7)
uint32_t CLTLoginState_State5::Slot7_GetStateId() const {
    return 5;
}

// anchor: launcher.exe vtable 0x004b508c
const char* CLTLoginState_State6::DebugName() const {
    return "CLTLoginState_State6";
}

// anchor: launcher.exe:0x0043b8f0 (vtable 0x004b508c slot 3)
uint32_t CLTLoginState_State6::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b8f0");
}

// anchor: launcher.exe:0x00440780 (vtable 0x004b508c slot 6)
uint32_t CLTLoginState_State6::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00440780");
}

// anchor: launcher.exe:0x00438c70 (vtable 0x004b508c slot 7)
uint32_t CLTLoginState_State6::Slot7_GetStateId() const {
    return 6;
}

// anchor: launcher.exe vtable 0x004b50b4
const char* CLTLoginState_State7::DebugName() const {
    return "CLTLoginState_State7";
}

// anchor: launcher.exe:0x0043ba20 (vtable 0x004b50b4 slot 3)
uint32_t CLTLoginState_State7::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043ba20");
}

// anchor: launcher.exe:0x0043bae0 (vtable 0x004b50b4 slot 6)
uint32_t CLTLoginState_State7::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bae0");
}

// anchor: launcher.exe:0x00438c80 (vtable 0x004b50b4 slot 7)
uint32_t CLTLoginState_State7::Slot7_GetStateId() const {
    return 7;
}

// anchor: launcher.exe vtable 0x004b5104
const char* CLTLoginState_State8::DebugName() const {
    return "CLTLoginState_State8";
}

// anchor: launcher.exe:0x0043bd20 (vtable 0x004b5104 slot 3)
uint32_t CLTLoginState_State8::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Ownership/fidelity correction:
    // - `0x43bd20` is `CLTLoginState_State8` slot 3
    // - it owns a structured packet-builder path, not a mediator slot body
    // - current best read from decompilation:
    //   - early prechecks can switch helper states `4` or `6`
    //   - fetch current slot record through owner vtable `+0x44`
    //   - initialize packet-builder family `0x43ac10`
    //   - write the current character id pair plus selection snapshot blocks
    //     `+0xcd0..+0xd7f` in the original write order
    //   - append `GameSessionID` through `0x43ada0`
    //   - send through `0x41af70`
    //   - post event `9`
    const CLTLoginMediator::SlotRecordState004b5328* currentSlotRecord = mediator->GetCurrentSlotRecord();
    State8StructuredMarginPacketBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();

    packetBuilder.SetFixedDword(0x01, currentSlotRecord ? currentSlotRecord->globalCharacterIdLow03 : 0u);
    packetBuilder.SetFixedDword(0x05, currentSlotRecord ? currentSlotRecord->globalCharacterIdHigh07 : 0u);

    // Keep block write order aligned with the original `0x43bd20` disassembly, not numeric order.
    packetBuilder.SetSelectionBlock(0x09, mediator->SelectionContextBlockCd0());
    packetBuilder.SetSelectionBlock(0x19, mediator->SelectionContextBlockCe0());
    packetBuilder.SetSelectionBlock(0x29, mediator->SelectionContextBlockCf0());
    packetBuilder.SetSelectionBlock(0x79, mediator->SelectionContextBlockD40());
    packetBuilder.SetSelectionBlock(0x89, mediator->SelectionContextBlockD50());
    packetBuilder.SetSelectionBlock(0x99, mediator->SelectionContextBlockD60());
    packetBuilder.SetSelectionBlock(0xa9, mediator->SelectionContextBlockD70());
    packetBuilder.SetSelectionBlock(0x39, mediator->SelectionContextBlockD00());
    packetBuilder.SetSelectionBlock(0x49, mediator->SelectionContextBlockD10());
    packetBuilder.SetSelectionBlock(0x59, mediator->SelectionContextBlockD20());
    packetBuilder.SetSelectionBlock(0x69, mediator->SelectionContextBlockD30());

    packetBuilder.SetGameSessionId(mediator->GetGameSessionId664());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(
        packetBuilder.PayloadBase(),
        packetBuilder.PayloadByteCount());

    Log(
        "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue built structured margin packet fixedBytes=0x%02x totalBytes=0x%02x gcidLow=0x%08x gcidHigh=0x%08x blockCd0_0=0x%08x blockD70_3=0x%08x GameSessionID='%s' -> sendResult=0x%08x then posts event=9",
        (unsigned)State8StructuredMarginPacketFixedPayload::kFixedByteCount,
        (unsigned)packetBuilder.PayloadByteCount(),
        currentSlotRecord ? (unsigned)currentSlotRecord->globalCharacterIdLow03 : 0u,
        currentSlotRecord ? (unsigned)currentSlotRecord->globalCharacterIdHigh07 : 0u,
        (unsigned)mediator->SelectionContextBlockCd0()[0],
        (unsigned)mediator->SelectionContextBlockD70()[3],
        mediator->GetGameSessionId664() ? mediator->GetGameSessionId664() : "<empty>",
        (unsigned)sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x0043f930 (vtable 0x004b5104 slot 6)
uint32_t CLTLoginState_State8::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043f930");
}

// anchor: launcher.exe:0x00438c90 (vtable 0x004b5104 slot 7)
uint32_t CLTLoginState_State8::Slot7_GetStateId() const {
    return 8;
}

// anchor: launcher.exe vtable 0x004b517c
const char* CLTLoginState_State9::DebugName() const {
    return "CLTLoginState_State9";
}

// anchor: launcher.exe:0x00439780 (vtable 0x004b517c slot 3)
uint32_t CLTLoginState_State9::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;

    // Narrow ownership correction from `0x004b517c` docs + Ghidra:
    // - this state consumes the helper-local byte/word payload at `this+4/+6`
    // - forwards them into owner helper `0x41de40`
    // - clears the local payload
    // - posts event `0x17` on success
    // The concrete owner helper is still unresolved in source, so keep the state-local payload
    // lifecycle here and leave the deeper owner-side effect as a logged scaffold step.
    Log(
        "DIAGNOSTIC: CLTLoginState_State9::Slot3_BeginOrContinue consuming helper-local payload byte4=0x%02x word6=0x%04x (owner helper 0x41de40 still unresolved)",
        (unsigned)pendingByte4_,
        (unsigned)pendingWord6_);
    pendingByte4_ = 0;
    pendingWord6_ = 0;
    return 1u;
}

// anchor: launcher.exe:0x0043c180 (vtable 0x004b517c slot 6)
uint32_t CLTLoginState_State9::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043c180");
}

// anchor: launcher.exe:0x00438cc0 (vtable 0x004b517c slot 7)
uint32_t CLTLoginState_State9::Slot7_GetStateId() const {
    return 9;
}

// anchor: launcher.exe vtable 0x004b512c
const char* CLTLoginState_State10::DebugName() const {
    return "CLTLoginState_State10";
}

// anchor: launcher.exe:0x0043bf90 (vtable 0x004b512c slot 3)
uint32_t CLTLoginState_State10::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Fresh `0x43bf90` read from decompilation + disassembly:
    // - precheck owner `+0x1c` connection state through `0x41b4b0`
    //   - on failure, original switches helper state to `4`
    // - then check owner byte `+0xf14`
    //   - on zero, original switches helper state to `6`
    // - initialize local packet-builder family `0x43a1f0`
    // - copy owner `+0x108` (`CharacterName`) through `0x43aa80`
    // - send through `0x41af70`
    // - post event `0x13`
    if (!mediator->State10HasReadyConnectionState2()) {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6");
        return 0u;
    }

    State10Packet0x0aBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();
    packetBuilder.SetCharacterName(mediator->SourceLeadString108().data());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(
        packetBuilder.PayloadBase(),
        packetBuilder.PayloadByteCount());

    Log(
        "DIAGNOSTIC: CLTLoginState_State10::Slot3_BeginOrContinue built raw-0x0a packet fixedBytes=0x%02x totalBytes=0x%02x CharacterName='%s' -> sendResult=0x%08x then posts event=0x13",
        (unsigned)State10Packet0x0aFixedPayload::kFixedByteCount,
        (unsigned)packetBuilder.PayloadByteCount(),
        mediator->SourceLeadString108().data(),
        (unsigned)sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x004401a0 (vtable 0x004b512c slot 6)
uint32_t CLTLoginState_State10::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    // Ownership correction from the vtable docs + Ghidra decompilation:
    // - `0x4401a0` belongs to `CLTLoginState_State10` slot 6, not to the mediator vtable
    // - the state entry itself handles raw auth code `0x0b`, performs the owner writeback, then
    //   switches helper state to `11`
    // - the mediator keeps only the narrower staged-packet + owner-state helpers
    const uint32_t handled = mediator->HandleStagedAuthReplyPacketScaffold();
    if (handled == 0u) {
        return 0u;
    }

    if (CLTLoginState* nextState = mediator->ScaffoldState11()) {
        mediator->SetCurrentState(nextState);
    } else {
        Log(
            "DIAGNOSTIC: CLTLoginState_State10::Slot6_HandleSecondaryMessage parsed AS_AuthReply but has no registered helper11 state");
    }
    return handled;
}

// anchor: launcher.exe:0x00438ca0 (vtable 0x004b512c slot 7)
uint32_t CLTLoginState_State10::Slot7_GetStateId() const {
    return 10;
}

// anchor: launcher.exe vtable 0x004b5154
const char* CLTLoginState_State11::DebugName() const {
    return "CLTLoginState_State11";
}

// anchor: launcher.exe:0x0043c020 (vtable 0x004b5154 slot 3)
uint32_t CLTLoginState_State11::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    if (!mediator) {
        return 0u;
    }

    // Faithfulness correction:
    // - `0x43c020` belongs to `CLTLoginState_State11` slot 3, so the packet build/send shape
    //   should live here, not on the mediator
    // - original body:
    //   - treats `ESI = owner + 0x108`
    //   - creates a packet-builder object through `0x439840`
    //   - resets/initializes the raw `0x4d` payload through `0x43a470`
    //   - writes 17 dwords from owner `+0x134..+0x174`
    //   - appends `RealFirstName`, `RealLastName`, optional `Background`, and `GameSessionID`
    //     through `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940`
    //   - calls `0x41af70` to send through the current margin connection
    //   - then posts event `0x15`
    const auto& sourceDwords134 = mediator->SourceDwords134();
    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    State11Packet0x4dBuilder packetBuilder;
    packetBuilder.ResetAndInitialize();

    // Keep write order aligned with the original disassembly of `0x43c020`.
    packetBuilder.SetFixedDword(0x01, sourceDwords134[0]);
    packetBuilder.SetFixedDword(0x05, sourceDwords134[1]);
    packetBuilder.SetFixedDword(0x09, sourceDwords134[2]);
    packetBuilder.SetFixedDword(0x0d, sourceDwords134[3]);
    packetBuilder.SetFixedDword(0x11, sourceDwords134[4]);
    packetBuilder.SetFixedDword(0x15, sourceDwords134[5]);
    packetBuilder.SetFixedDword(0x19, sourceDwords134[6]);
    packetBuilder.SetFixedDword(0x1d, sourceDwords134[7]);
    packetBuilder.SetFixedDword(0x35, sourceDwords134[13]);
    packetBuilder.SetFixedDword(0x25, sourceDwords134[9]);
    packetBuilder.SetFixedDword(0x3d, sourceDwords134[15]);
    packetBuilder.SetFixedDword(0x2d, sourceDwords134[11]);
    packetBuilder.SetFixedDword(0x21, sourceDwords134[8]);
    packetBuilder.SetFixedDword(0x39, sourceDwords134[14]);
    packetBuilder.SetFixedDword(0x31, sourceDwords134[12]);
    packetBuilder.SetFixedDword(0x29, sourceDwords134[10]);
    packetBuilder.SetFixedDword(0x41, sourceDwords134[16]);

    packetBuilder.SetRealFirstName(reinterpret_cast<const char*>(mediator->SourceBlock178().data()));
    packetBuilder.SetRealLastName(reinterpret_cast<const char*>(mediator->SourceBlock198().data()));
    packetBuilder.SetBackground(reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()));
    packetBuilder.SetGameSessionId(mediator->GetGameSessionId664());

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(
        packetBuilder.PayloadBase(),
        packetBuilder.PayloadByteCount());

    Log(
        "DIAGNOSTIC: CLTLoginState_State11::Slot3_BeginOrContinue built fixed-0x4d margin payload payloadTag=0x%02x fixedBytes=0x%02x totalBytes=0x%02x SkinToneID=0x%08x RealFirstName='%s' RealLastName='%s' Background='%s' GameSessionID='%s' -> sendResult=0x%08x then posts event=0x15",
        (unsigned)State11Packet0x4dFixedPayload::kPayloadTag0c,
        (unsigned)State11Packet0x4dFixedPayload::kFixedByteCount,
        (unsigned)packetBuilder.PayloadByteCount(),
        (unsigned)sourceDwords134[0],
        reinterpret_cast<const char*>(mediator->SourceBlock178().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock198().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()),
        mediator->GetGameSessionId664() ? mediator->GetGameSessionId664() : "<empty>",
        (unsigned)sendResult);
    return sendResult;
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(mediator->StagedIncomingMarginPacketBytes());
    if (!parsed.valid) {
        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage could not parse staged margin bytes as raw-0x10 helper11 reply");
        return 0u;
    }

    // Ownership correction mirrors slot 3 as well:
    // - `0x440320` belongs to `CLTLoginState_State11` slot 6
    // - the state object owns reply-progress counters and the helper11 -> helper9 handoff
    // - mediator keeps the narrower owner-buffer mutation helper because those writes target the
    //   mediator-owned `0x4f78b8` state area
    const uint32_t handled = mediator->HandleStagedMarginLoadCharacterReplyPacketScaffold();
    if (handled == 0u) {
        return 0u;
    }

    if (parsed.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = parsed.expectedSectionCount0b;
    }
    if (replySectionsExpected_ == 0u) {
        replySectionsExpected_ = 1u;
    }
    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                // `0x440320` writes parsed word `+9` into helper9 `this+6` before switching state.
                // Current source-owned mirror keeps that on the concrete state9 object.
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SetCurrentState(nextBase);
        }

        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage completed helper11 reply progression status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u -> currentState=helper9",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
    } else {
        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage routed helper11 reply status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u seedCount=%u",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_,
            parsed.shouldSeedExpectedSectionCount ? 1u : 0u);
    }
    return handled;
}

// anchor: launcher.exe:0x00438cb0 (vtable 0x004b5154 slot 7)
uint32_t CLTLoginState_State11::Slot7_GetStateId() const {
    return 11;
}

// anchor: launcher.exe vtable 0x004b5230
const char* CLTLoginState_State12::DebugName() const {
    return "CLTLoginState_State12";
}

// anchor: launcher.exe:0x00438d00 (vtable 0x004b5230 slot 7)
uint32_t CLTLoginState_State12::Slot7_GetStateId() const {
    return 12;
}

// anchor: launcher.exe vtable 0x004b50dc
const char* CLTLoginState_State13::DebugName() const {
    return "CLTLoginState_State13";
}

// anchor: launcher.exe:0x00439680 (vtable 0x004b50dc slot 2)
uint32_t CLTLoginState_State13::Slot2_HandleSecondaryGate(CLTLoginMediator* mediator) {
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00439680");
}

// anchor: launcher.exe:0x0043bb90 (vtable 0x004b50dc slot 3)
uint32_t CLTLoginState_State13::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bb90");
}

// anchor: launcher.exe:0x0043bc60 (vtable 0x004b50dc slot 6)
uint32_t CLTLoginState_State13::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043bc60");
}

// anchor: launcher.exe:0x00438cd0 (vtable 0x004b50dc slot 7)
uint32_t CLTLoginState_State13::Slot7_GetStateId() const {
    return 13;
}

// anchor: launcher.exe vtable 0x004b4fec
const char* CLTLoginState_WorldListPending::DebugName() const {
    return "CLTLoginState_WorldListPending";
}

// anchor: launcher.exe:0x0043b830 (vtable 0x004b4fec slot 3)
uint32_t CLTLoginState_WorldListPending::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x0043b830");
}

// anchor: launcher.exe:0x0043d4d0 (string/file anchors: loginstate.cpp, CLTLoginState_WorldListPending::AuthMessageDispatch())
uint32_t CLTLoginState_WorldListPending::AuthMessageDispatch(void* workItem, CLTLoginMediator* mediator) {
    // Current best contextual role from the vtable and string anchors:
    // - vtable 0x004b4fec / slot 5
    // - AS_GetWorldListReply / AS_PSGetWorldListReply
    (void)workItem;
    (void)mediator;
    return 1;
}

// anchor: launcher.exe:0x00438ce0 (vtable 0x004b4fec slot 7)
uint32_t CLTLoginState_WorldListPending::Slot7_GetStateId() const {
    return 14;
}

// anchor: launcher.exe vtable 0x004b0b88
const char* CLTLoginState_State15::DebugName() const {
    return "CLTLoginState_State15";
}

// anchor: launcher.exe:0x00420680 (vtable 0x004b0b88 slot 3)
uint32_t CLTLoginState_State15::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420680");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0b88 slot 6)
uint32_t CLTLoginState_State15::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420310 (vtable 0x004b0b88 slot 7)
uint32_t CLTLoginState_State15::Slot7_GetStateId() const {
    return 15;
}

// anchor: launcher.exe:0x004206a0 (vtable 0x004b0b88 slot 8)
uint32_t CLTLoginState_State15::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004206a0");
}

// anchor: launcher.exe vtable 0x004b0bb0
const char* CLTLoginState_State16::DebugName() const {
    return "CLTLoginState_State16";
}

// anchor: launcher.exe:0x00420720 (vtable 0x004b0bb0 slot 3)
uint32_t CLTLoginState_State16::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420720");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bb0 slot 6)
uint32_t CLTLoginState_State16::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420320 (vtable 0x004b0bb0 slot 7)
uint32_t CLTLoginState_State16::Slot7_GetStateId() const {
    return 16;
}

// anchor: launcher.exe:0x004207c0 (vtable 0x004b0bb0 slot 8)
uint32_t CLTLoginState_State16::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004207c0");
}

// anchor: launcher.exe vtable 0x004b0bd8
const char* CLTLoginState_State17::DebugName() const {
    return "CLTLoginState_State17";
}

// anchor: launcher.exe:0x00420890 (vtable 0x004b0bd8 slot 3)
uint32_t CLTLoginState_State17::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420890");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0bd8 slot 6)
uint32_t CLTLoginState_State17::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420330 (vtable 0x004b0bd8 slot 7)
uint32_t CLTLoginState_State17::Slot7_GetStateId() const {
    return 17;
}

// anchor: launcher.exe vtable 0x004b0c00
const char* CLTLoginState_State18::DebugName() const {
    return "CLTLoginState_State18";
}

// anchor: launcher.exe:0x00421a50 (vtable 0x004b0c00 slot 3)
uint32_t CLTLoginState_State18::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    // Stronger current read from disassembly review:
    // - this is a later launchpad/session helper path, not a direct helper11 writer
    // - it fetches owner vtable `+0x130` helper `+0x65c`
    // - when conditions permit, it refreshes helper string `+0x18` from owner `+0x94 + 0x60`
    //   (the embedded small-string in the recovered auth/bootstrap source block)
    // - it then reaches `0x420e70`, which copies helper `+0x18` into owner `+0x664`
    //   (`GameSessionID`) when helper flag `+0x2d` is clear
    (void)upstreamOrArg;
    return mediator ? mediator->RefreshSessionHelperGameSessionId664FromSourceBlock94() : 0u;
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c00 slot 6)
uint32_t CLTLoginState_State18::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420340 (vtable 0x004b0c00 slot 7)
uint32_t CLTLoginState_State18::Slot7_GetStateId() const {
    return 18;
}

// anchor: launcher.exe:0x00420960 (vtable 0x004b0c00 slot 8)
uint32_t CLTLoginState_State18::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420960");
}

// anchor: launcher.exe vtable 0x004b0c28
const char* CLTLoginState_State19::DebugName() const {
    return "CLTLoginState_State19";
}

// anchor: launcher.exe:0x004209e0 (vtable 0x004b0c28 slot 3)
uint32_t CLTLoginState_State19::Slot3_BeginOrContinue(void* upstreamOrArg, CLTLoginMediator* mediator) {
    (void)upstreamOrArg;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004209e0");
}

// anchor: launcher.exe:0x004208e0 (vtable 0x004b0c28 slot 6)
uint32_t CLTLoginState_State19::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x004208e0");
}

// anchor: launcher.exe:0x00420350 (vtable 0x004b0c28 slot 7)
uint32_t CLTLoginState_State19::Slot7_GetStateId() const {
    return 19;
}

// anchor: launcher.exe:0x00420a00 (vtable 0x004b0c28 slot 8)
uint32_t CLTLoginState_State19::Slot8_HandleAuxiliaryEvent(uint32_t param1, void* context, CLTLoginMediator* mediator) {
    (void)param1;
    (void)context;
    (void)mediator;
    return PlaceholderStateAction(DebugName(), "launcher.exe:0x00420a00");
}

}  // namespace mxo::ltlogin
