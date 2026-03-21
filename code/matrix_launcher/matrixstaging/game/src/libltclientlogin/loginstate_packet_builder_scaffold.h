#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../../runtime/src/libltmessaging/messageconnection.h"

namespace mxo::ltlogin {

struct State11Packet0x4dFixedPayload {
    // anchor: launcher.exe:0x43a470 / packet payload tag written after the outer builder reserves
    // a fixed 0x4d-byte payload span through the shared envelope object.
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0c` = `MS_CreateCharacterRequest`
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
    RecoveredPacketBuilderEnvelope()
        : envelope_(mxo::liblttcp::CMessageConnection::BuildPacketBuilderEnvelopeScaffold(/*headerless=*/false)) {
        // Current best source-owned mirror of the local helper object initialized by `0x439840`:
        // - acquires/installs a shared payload object through the `0x455cd0 -> 0x455c60 -> 0x455bd0`
        //   family
        // - stores the active payload write base as `shared + 0x0c`
        // - original local envelope layout is now a little tighter from disassembly:
        //   - `this+0x08` = retained shared packet/message object
        //   - `this+0x04` = base payload pointer (`shared + 0x0c`)
        //   - state-specific builders then cache string-reservation pointers/lengths in later
        //     helper-local fields before `0x41af70` forwards the whole envelope object
    }

    uint8_t* PayloadBase() {
        return envelope_.sharedMessage ? envelope_.sharedMessage->PayloadBaseScaffold() : nullptr;
    }

    const uint8_t* PayloadBase() const {
        return envelope_.sharedMessage ? envelope_.sharedMessage->PayloadBaseScaffold() : nullptr;
    }

    uint32_t PayloadByteCount() const {
        return envelope_.sharedMessage ? envelope_.sharedMessage->PayloadByteCountScaffold() : 0u;
    }

    const mxo::liblttcp::CMessageConnectionEnvelopeScaffold& EnvelopeScaffold() const {
        return envelope_;
    }

protected:
    void ResetPayloadToFixedByteCount0x4d() {
        ResizePayload(State11Packet0x4dFixedPayload::kFixedByteCount);
    }

    void ResizePayload(size_t fixedByteCount) {
        if (!envelope_.sharedMessage) {
            return;
        }
        envelope_.sharedMessage->ResetPayloadByteCountScaffold(static_cast<uint16_t>(fixedByteCount));
    }

    void WritePayloadByte(size_t offset, uint8_t value) {
        if (!envelope_.sharedMessage) {
            return;
        }
        uint8_t* payloadBytes = envelope_.sharedMessage->PayloadBaseScaffold();
        if (payloadBytes && offset < envelope_.sharedMessage->PayloadByteCountScaffold()) {
            payloadBytes[offset] = value;
        }
    }

    void WritePayloadU16LE(size_t offset, uint16_t value) {
        if (!envelope_.sharedMessage) {
            return;
        }
        uint8_t* payloadBytes = envelope_.sharedMessage->PayloadBaseScaffold();
        const uint16_t payloadByteCount = envelope_.sharedMessage->PayloadByteCountScaffold();
        if (!payloadBytes || offset + 1 >= payloadByteCount) {
            return;
        }
        payloadBytes[offset] = static_cast<uint8_t>(value & 0xffu);
        payloadBytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
    }

    void WritePayloadU32LE(size_t offset, uint32_t value) {
        if (!envelope_.sharedMessage) {
            return;
        }
        uint8_t* payloadBytes = envelope_.sharedMessage->PayloadBaseScaffold();
        const uint16_t payloadByteCount = envelope_.sharedMessage->PayloadByteCountScaffold();
        if (!payloadBytes || offset + 3 >= payloadByteCount) {
            return;
        }
        payloadBytes[offset] = static_cast<uint8_t>(value & 0xffu);
        payloadBytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
        payloadBytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffu);
        payloadBytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffu);
    }

    uint16_t AppendLengthPrefixedString(size_t offsetField, const char* text, size_t bound) {
        if (!envelope_.sharedMessage || !text) {
            return 0;
        }

        size_t textLengthWithoutNul = 0;
        while (textLengthWithoutNul < bound && text[textLengthWithoutNul] != '\0') {
            ++textLengthWithoutNul;
        }

        const uint16_t currentPayloadByteCount = envelope_.sharedMessage->PayloadByteCountScaffold();
        const uint16_t remainingAfterLength = envelope_.sharedMessage->RemainingAppendableByteCountScaffold();
        if (remainingAfterLength < 2u) {
            return 0;
        }

        // Fresh original state8 send tightening from `0x43ada0`:
        // - reserve helper is passed the source string length **including** the terminating NUL
        // - even an empty-but-non-null string therefore still reserves one content byte plus the
        //   two-byte length field
        const uint16_t storedLength = static_cast<uint16_t>(std::min<size_t>(textLengthWithoutNul + 1u, remainingAfterLength - 2u));
        const uint16_t requestedGrowth = static_cast<uint16_t>(storedLength + 2u);
        const uint16_t newPayloadByteCount = envelope_.sharedMessage->GrowPayloadByteCountScaffold(requestedGrowth);
        if (newPayloadByteCount != currentPayloadByteCount + requestedGrowth) {
            return 0;
        }

        uint8_t* payloadBytes = envelope_.sharedMessage->PayloadBaseScaffold();
        if (!payloadBytes) {
            return 0;
        }

        WritePayloadU16LE(offsetField, currentPayloadByteCount);
        payloadBytes[currentPayloadByteCount + 0u] = static_cast<uint8_t>(storedLength & 0xffu);
        payloadBytes[currentPayloadByteCount + 1u] = static_cast<uint8_t>((storedLength >> 8) & 0xffu);
        if (storedLength > 1u) {
            std::copy_n(text, storedLength - 1u, reinterpret_cast<char*>(payloadBytes + currentPayloadByteCount + 2u));
        }
        payloadBytes[currentPayloadByteCount + 2u + storedLength - 1u] = 0u;
        return storedLength;
    }

private:
    mxo::liblttcp::CMessageConnectionEnvelopeScaffold envelope_;
};

struct State10Packet0x0aFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0a` = `MS_ClaimCharacterNameRequest`
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
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0f` = `MS_LoadCharacterRequest`
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
    // Newer `0x43acf0` tightening worth preserving:
    // - reserve helper writes the payload-relative offset back to fixed field `+0xb9`
    // - then caches the concrete write pointer and reserved length in helper-local fields before
    //   the caller copies the session text there
    void SetGameSessionId(const char* text) {
        AppendLengthPrefixedString(State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset, text, 0xffffu);
    }
};

}  // namespace mxo::ltlogin
