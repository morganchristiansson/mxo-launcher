#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../runtime/src/libltmessaging/messageconnection.h"

namespace mxo::ltlogin {

struct State11Packet0x4dFixedPayload {
    // anchor: launcher.exe:0x43a470 / packet payload tag written after the outer builder reserves
    // a fixed 0x4d-byte payload span through the retained message-ref's inner storage.
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

struct State11Packet0x4dBuilderRawScaffold {
    // anchor family: launcher.exe vtable `0x004b53c8`
    // Current best local helper shape on top of the shared `0x439840` envelope front matter:
    // - `+0x10` = packet payload base used by the fixed-field writers
    // - `+0x14/+0x1c/+0x24/+0x2c` = repeated reservation triplets
    //   `(write pointer, reserved content byte count)` for the four appended strings
    mxo::liblttcp::CMessageConnectionPacketBuilderPayloadScaffold builder00{};
    mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realFirstName14{};
    mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realLastName1c{};
    mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold background24{};
    mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold gameSessionId2c{};
};

static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, realFirstName14) == 0x14, "state11 builder realFirstName reservation offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, realLastName1c) == 0x1c, "state11 builder realLastName reservation offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, background24) == 0x24, "state11 builder background reservation offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, gameSessionId2c) == 0x2c, "state11 builder gameSessionId reservation offset mismatch");

template <typename RawBuilderT>
class RecoveredPacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x439840
    RecoveredPacketBuilderEnvelopeBase() {
        // Shared local packet-builder envelope initializer:
        // - installs the base helper vtable `0x004af2a4`
        // - retains a live outer message-ref through `0x455cd0 -> 0x455c60`
        // - caches payload base at raw `+0x04`
        // - stores the retained message-ref at raw `+0x08`
        messageRef_.ResetForPacketBuilderScaffold(/*headerless=*/false);
        raw_.builder00.envelope00.vtable00 = RawVtablePointer(kBasePacketBuilderEnvelopeVtable00);
        raw_.builder00.envelope00.payloadBase04 =
            MessageStorage() ? MessageStorage()->PayloadBaseScaffold() : nullptr;
        raw_.builder00.envelope00.messageRef08 = &messageRef_;
    }

    RecoveredPacketBuilderEnvelopeBase(const RecoveredPacketBuilderEnvelopeBase&) = delete;
    RecoveredPacketBuilderEnvelopeBase& operator=(const RecoveredPacketBuilderEnvelopeBase&) = delete;
    RecoveredPacketBuilderEnvelopeBase(RecoveredPacketBuilderEnvelopeBase&&) = delete;
    RecoveredPacketBuilderEnvelopeBase& operator=(RecoveredPacketBuilderEnvelopeBase&&) = delete;

    uint8_t* PayloadBase() {
        return raw_.builder00.envelope00.payloadBase04;
    }

    const uint8_t* PayloadBase() const {
        return raw_.builder00.envelope00.payloadBase04;
    }

    uint32_t PayloadByteCount() const {
        return MessageStorage() ? MessageStorage()->PayloadByteCountScaffold() : 0u;
    }

    mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& Envelope() {
        return raw_.builder00.envelope00;
    }

    const mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& Envelope() const {
        return raw_.builder00.envelope00;
    }

protected:
    static constexpr uintptr_t kBasePacketBuilderEnvelopeVtable00 = 0x004af2a4u;

    static void** RawVtablePointer(uintptr_t address) {
        return reinterpret_cast<void**>(address);
    }

    void ResetPacketPayloadBuilderScaffold(
        uintptr_t packetBuilderVtable00,
        size_t fixedByteCount) {
        mxo::liblttcp::CMessageConnectionMessageStorage* const messageStorage = MessageStorage();
        if (!messageStorage) {
            return;
        }

        raw_.builder00.envelope00.vtable00 = RawVtablePointer(packetBuilderVtable00);
        raw_.builder00.builderFlag0c = 0u;
        raw_.builder00.padding0d_0f[0] = 0u;
        raw_.builder00.padding0d_0f[1] = 0u;
        raw_.builder00.padding0d_0f[2] = 0u;
        messageStorage->ResetPayloadByteCountScaffold(static_cast<uint16_t>(fixedByteCount));
        raw_.builder00.packetPayload10 = raw_.builder00.envelope00.payloadBase04;
    }

    void ClearReservation(
        mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold& reservation) {
        reservation.writePointer00 = nullptr;
        reservation.reservedContentByteCount04 = 0u;
        reservation.reservedPadding06 = 0u;
    }

    uint8_t* PacketPayload10() {
        return raw_.builder00.packetPayload10;
    }

    const uint8_t* PacketPayload10() const {
        return raw_.builder00.packetPayload10;
    }

    void WritePayloadByte(size_t offset, uint8_t value) {
        mxo::liblttcp::CMessageConnectionMessageStorage* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = PacketPayload10();
        if (!messageStorage || !packetPayload || offset >= messageStorage->PayloadByteCountScaffold()) {
            return;
        }
        packetPayload[offset] = value;
    }

    void WritePayloadU16LE(size_t offset, uint16_t value) {
        mxo::liblttcp::CMessageConnectionMessageStorage* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = PacketPayload10();
        const uint16_t payloadByteCount =
            messageStorage ? messageStorage->PayloadByteCountScaffold() : 0u;
        if (!packetPayload || offset + 1u >= payloadByteCount) {
            return;
        }
        packetPayload[offset] = static_cast<uint8_t>(value & 0xffu);
        packetPayload[offset + 1u] = static_cast<uint8_t>((value >> 8) & 0xffu);
    }

    void WritePayloadU32LE(size_t offset, uint32_t value) {
        mxo::liblttcp::CMessageConnectionMessageStorage* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = PacketPayload10();
        const uint16_t payloadByteCount =
            messageStorage ? messageStorage->PayloadByteCountScaffold() : 0u;
        if (!packetPayload || offset + 3u >= payloadByteCount) {
            return;
        }
        packetPayload[offset] = static_cast<uint8_t>(value & 0xffu);
        packetPayload[offset + 1u] = static_cast<uint8_t>((value >> 8) & 0xffu);
        packetPayload[offset + 2u] = static_cast<uint8_t>((value >> 16) & 0xffu);
        packetPayload[offset + 3u] = static_cast<uint8_t>((value >> 24) & 0xffu);
    }

    uint16_t ReserveLengthPrefixedString(
        size_t offsetField,
        const char* text,
        size_t bound,
        mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold* reservation) {
        if (reservation) {
            ClearReservation(*reservation);
        }

        mxo::liblttcp::CMessageConnectionMessageStorage* const messageStorage = MessageStorage();
        const uint8_t* const payloadBase = PayloadBase();
        uint8_t* const packetPayload = PacketPayload10();
        if (!messageStorage || !payloadBase || !packetPayload || !text) {
            return 0u;
        }

        size_t textLengthWithoutNul = 0u;
        while (textLengthWithoutNul < bound && text[textLengthWithoutNul] != '\0') {
            ++textLengthWithoutNul;
        }

        const uint16_t currentPayloadByteCount = messageStorage->PayloadByteCountScaffold();
        const uint16_t remainingAfterLength =
            messageStorage->RemainingAppendableByteCountScaffold();
        if (remainingAfterLength < 2u) {
            return 0u;
        }

        // Fresh original state8/state11 reservation tightening from `0x43a230 / 0x43acf0`:
        // - reserve helper takes content byte count including the terminating NUL
        // - stored offset field is relative to builder payload base `+0x10`, not to the raw
        //   envelope payload cache at `+0x04`
        const uint16_t storedLength = static_cast<uint16_t>(
            std::min<size_t>(textLengthWithoutNul + 1u, remainingAfterLength - 2u));
        const uint16_t requestedGrowth = static_cast<uint16_t>(storedLength + 2u);
        const uint16_t newPayloadByteCount =
            messageStorage->GrowPayloadByteCountScaffold(requestedGrowth);
        if (newPayloadByteCount != currentPayloadByteCount + requestedGrowth) {
            return 0u;
        }

        uint8_t* const reservationHeader =
            raw_.builder00.envelope00.payloadBase04 + currentPayloadByteCount;
        if (!reservationHeader || reservationHeader < packetPayload) {
            return 0u;
        }

        const uint16_t payloadRelativeOffset = static_cast<uint16_t>(
            reservationHeader - packetPayload);
        WritePayloadU16LE(offsetField, payloadRelativeOffset);
        reservationHeader[0] = static_cast<uint8_t>(storedLength & 0xffu);
        reservationHeader[1] = static_cast<uint8_t>((storedLength >> 8) & 0xffu);

        if (reservation) {
            reservation->writePointer00 = reservationHeader + 2u;
            reservation->reservedContentByteCount04 = storedLength;
        }
        return storedLength;
    }

    void WriteReservedCString(
        const mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold& reservation,
        const char* text) {
        if (!text || !reservation.writePointer00 || reservation.reservedContentByteCount04 == 0u) {
            return;
        }

        if (reservation.reservedContentByteCount04 > 1u) {
            std::copy_n(
                text,
                reservation.reservedContentByteCount04 - 1u,
                reinterpret_cast<char*>(reservation.writePointer00));
        }
        reservation.writePointer00[reservation.reservedContentByteCount04 - 1u] = 0u;
    }

    mxo::liblttcp::CMessageConnectionMessageStorage* MessageStorage() {
        return messageRef_.messageStorage0c;
    }

    const mxo::liblttcp::CMessageConnectionMessageStorage* MessageStorage() const {
        return messageRef_.messageStorage0c;
    }

    RawBuilderT raw_{};
    mxo::liblttcp::CMessageConnectionMessageRef messageRef_{};
};

struct State7Packet0x0dFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0d` = smaller current-selection/current-character route probe used by
    // state7 (`0x43ba20`).
    static constexpr uint8_t kPayloadTag0d = 0x0d;
    static constexpr size_t kCharacterNameOffset = 0x01;
    static constexpr size_t kCharacterIdLowOffset = 0x03;
    static constexpr size_t kCharacterIdHighOffset = 0x07;
    static constexpr size_t kFixedByteCount = 0x0b;
};

class State7Packet0x0dBuilder final
    : public RecoveredPacketBuilderEnvelopeBase<
          mxo::liblttcp::CMessageConnectionPacketBuilderPayloadWithReservationScaffold> {
public:
    static constexpr uintptr_t kPacketBuilderVtable00 = 0x004b53f0u;

    // anchor: launcher.exe:0x43a9a0 / local packet-builder family `0x004b53f0`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            kPacketBuilderVtable00,
            State7Packet0x0dFixedPayload::kFixedByteCount);
        ClearReservation(raw_.reservation14);
        WritePayloadByte(0x00, State7Packet0x0dFixedPayload::kPayloadTag0d);
        WritePayloadU16LE(State7Packet0x0dFixedPayload::kCharacterNameOffset, 0u);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdLowOffset, 0u);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdHighOffset, 0u);
    }

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* text) {
        if (!text || raw_.reservation14.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State7Packet0x0dFixedPayload::kCharacterNameOffset,
                text,
                0xffffu,
                &raw_.reservation14) != 0u) {
            WriteReservedCString(raw_.reservation14, text);
        }
    }

    void SetCharacterIdPair(uint32_t low, uint32_t high) {
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdLowOffset, low);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdHighOffset, high);
    }
};

struct State10Packet0x0aFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0a` = `MS_ClaimCharacterNameRequest`
    static constexpr uint8_t kPayloadTag0a = 0x0a;
    static constexpr size_t kCharacterNameOffset = 0x01;
    static constexpr size_t kFixedByteCount = 0x03;
};

class State10Packet0x0aBuilder final
    : public RecoveredPacketBuilderEnvelopeBase<
          mxo::liblttcp::CMessageConnectionPacketBuilderPayloadWithReservationScaffold> {
public:
    static constexpr uintptr_t kPacketBuilderVtable00 = 0x004b53b4u;

    // anchor: launcher.exe:0x43a1f0 / local packet-builder family `0x004b53b4`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            kPacketBuilderVtable00,
            State10Packet0x0aFixedPayload::kFixedByteCount);
        ClearReservation(raw_.reservation14);
        WritePayloadByte(0x00, State10Packet0x0aFixedPayload::kPayloadTag0a);
        WritePayloadU16LE(State10Packet0x0aFixedPayload::kCharacterNameOffset, 0u);
    }

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* text) {
        if (!text || raw_.reservation14.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State10Packet0x0aFixedPayload::kCharacterNameOffset,
                text,
                0xffffu,
                &raw_.reservation14) != 0u) {
            WriteReservedCString(raw_.reservation14, text);
        }
    }
};

class State11Packet0x4dBuilder final
    : public RecoveredPacketBuilderEnvelopeBase<State11Packet0x4dBuilderRawScaffold> {
public:
    static constexpr uintptr_t kPacketBuilderVtable00 = 0x004b53c8u;

    // anchor: launcher.exe:0x43a470 / local packet-builder family `0x004b53c8`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            kPacketBuilderVtable00,
            State11Packet0x4dFixedPayload::kFixedByteCount);
        ClearReservation(raw_.realFirstName14);
        ClearReservation(raw_.realLastName1c);
        ClearReservation(raw_.background24);
        ClearReservation(raw_.gameSessionId2c);
        WritePayloadByte(0x00, State11Packet0x4dFixedPayload::kPayloadTag0c);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kRealFirstNameOffset, 0u);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kRealLastNameOffset, 0u);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kBackgroundOffset, 0u);
        WritePayloadU16LE(State11Packet0x4dFixedPayload::kGameSessionIdOffset, 0u);
    }

    void SetFixedDword(size_t payloadOffset, uint32_t value) {
        WritePayloadU32LE(payloadOffset, value);
    }

    // anchor: launcher.exe:0x43a640
    void SetRealFirstName(const char* text) {
        if (!text || raw_.realFirstName14.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kRealFirstNameOffset,
                text,
                0x20u,
                &raw_.realFirstName14) != 0u) {
            WriteReservedCString(raw_.realFirstName14, text);
        }
    }

    // anchor: launcher.exe:0x43a740
    void SetRealLastName(const char* text) {
        if (!text || raw_.realLastName1c.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kRealLastNameOffset,
                text,
                0x20u,
                &raw_.realLastName1c) != 0u) {
            WriteReservedCString(raw_.realLastName1c, text);
        }
    }

    // anchor: launcher.exe:0x43a840
    void SetBackground(const char* text) {
        if (!text || raw_.background24.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kBackgroundOffset,
                text,
                0x20u,
                &raw_.background24) != 0u) {
            WriteReservedCString(raw_.background24, text);
        }
    }

    // anchor: launcher.exe:0x43a940
    void SetGameSessionId(const char* text) {
        if (!text || raw_.gameSessionId2c.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kGameSessionIdOffset,
                text,
                0xffffu,
                &raw_.gameSessionId2c) != 0u) {
            WriteReservedCString(raw_.gameSessionId2c, text);
        }
    }
};

struct State8StructuredMarginPacketFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0f` = `MS_LoadCharacterRequest`
    static constexpr uint8_t kPayloadTag0f = 0x0f;
    static constexpr size_t kGameSessionIdOffset = 0xb9;
    static constexpr size_t kFixedByteCount = 0xbb;
};

class State8StructuredMarginPacketBuilder final
    : public RecoveredPacketBuilderEnvelopeBase<
          mxo::liblttcp::CMessageConnectionPacketBuilderPayloadWithReservationScaffold> {
public:
    static constexpr uintptr_t kPacketBuilderVtable00 = 0x004b5418u;

    // anchor: launcher.exe:0x43ac10 = CLTLoginMediatorPacket0x0f_ResetAndInitialize
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            kPacketBuilderVtable00,
            State8StructuredMarginPacketFixedPayload::kFixedByteCount);
        ClearReservation(raw_.reservation14);
        WritePayloadByte(0x00, State8StructuredMarginPacketFixedPayload::kPayloadTag0f);
        WritePayloadU32LE(0x01, 0u);
        WritePayloadU32LE(0x05, 0u);
        WritePayloadU16LE(State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset, 0u);
    }

    void SetFixedDword(size_t payloadOffset, uint32_t value) {
        WritePayloadU32LE(payloadOffset, value);
    }

    void SetSelectionBlock(size_t payloadOffset, const std::array<uint32_t, 4>& block) {
        WritePayloadU32LE(payloadOffset + 0x0u, block[0]);
        WritePayloadU32LE(payloadOffset + 0x4u, block[1]);
        WritePayloadU32LE(payloadOffset + 0x8u, block[2]);
        WritePayloadU32LE(payloadOffset + 0xcu, block[3]);
    }

    // anchor: launcher.exe:0x43ada0 = CLTLoginMediatorPacket0x0f_SetGameSessionId
    // helper-local length reservation/writeback mirrors launcher.exe:0x43acf0 =
    // CLTLoginMediatorPacket0x0f_ReserveGameSessionId.
    void SetGameSessionId(const char* text) {
        if (!text || raw_.reservation14.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset,
                text,
                0xffffu,
                &raw_.reservation14) != 0u) {
            WriteReservedCString(raw_.reservation14, text);
        }
    }
};

}  // namespace mxo::ltlogin
