#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../runtime/src/libltmessaging/messageconnection.h"
#include "loginmediator_base.h"  // Packet_0x4af2a4

namespace mxo::ltlogin {

// Types from liblttcp - use explicit namespace prefix in this file
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
    ::mxo::liblttcp::CMessageConnectionPacketBuilderPayloadScaffold builder00{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realFirstName14{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realLastName1c{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold background24{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold gameSessionId2c{};
};

static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, realFirstName14) == 0x14, "state11 builder realFirstName reservation offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, realLastName1c) == 0x1c, "state11 builder realLastName reservation offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, background24) == 0x24, "state11 builder background offset mismatch");
static_assert(offsetof(State11Packet0x4dBuilderRawScaffold, gameSessionId2c) == 0x2c, "state11 builder gameSessionId offset mismatch");

}  // namespace ltlogin

// Packet builder envelope base - static-RE faithful implementation.
// Inherits from Packet_0x4af2a4 and adds send-payload helpers.
// anchor: launcher.exe:0x439840
namespace mxo::ltlogin {

class PacketBuilderEnvelopeBase : public Packet_0x4af2a4 {
public:
    PacketBuilderEnvelopeBase() {
        // Initialize message ref for packet building
        messageRef08 = &messageRef_;
        messageRef_.ResetForPacketBuilder(/*headerless=*/false);
        payloadBegin10 = MessageStorage() ? MessageStorage()->PayloadBase() : nullptr;
    }

    ~PacketBuilderEnvelopeBase() override = default;

    PacketBuilderEnvelopeBase(const PacketBuilderEnvelopeBase&) = delete;
    PacketBuilderEnvelopeBase& operator=(const PacketBuilderEnvelopeBase&) = delete;

    uint8_t* PayloadBase() { return static_cast<uint8_t*>(payloadBegin10); }
    const uint8_t* PayloadBase() const { return static_cast<const uint8_t*>(payloadBegin10); }

    uint32_t PayloadByteCount() const {
        return MessageStorage() ? MessageStorage()->PayloadByteCount() : 0u;
    }

    ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c& MessageRef() { return messageRef_; }
    const ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c& MessageRef() const { return messageRef_; }

    // Returns envelope struct for send path compatibility.
    // anchor: launcher.exe:0x439840 / envelope fields
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& Envelope() {
        envelope_.payloadBase04 = static_cast<uint8_t*>(payloadBegin10);
        envelope_.messageRef08 = messageRef08;
        return envelope_;
    }

    const ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope& Envelope() const {
        return envelope_;
    }

protected:
    // Implement pure virtual methods from Packet_0x4af2a4
    uint32_t StubReturn0() override { return 0; }
    void DebugString(int /*formatType*/ = 2) override {}
    void InitializePayloadSize() override {}
    void* GetPayloadBase() override { return payloadBegin10; }

    ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* MessageStorage() {
        return messageRef_.messageStorage0c;
    }

    const ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* MessageStorage() const {
        return messageRef_.messageStorage0c;
    }

    void ResetPacketPayloadBuilderScaffold(size_t fixedByteCount) {
        ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* const messageStorage = MessageStorage();
        if (!messageStorage) {
            return;
        }
        statusByte1a = 0u;
        messageStorage->ResetPayloadByteCount(static_cast<uint16_t>(fixedByteCount));
        payloadBegin10 = MessageStorage() ? MessageStorage()->PayloadBase() : nullptr;
    }

    void ClearReservation(
        ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold& reservation) {
        reservation.writePointer00 = nullptr;
        reservation.reservedContentByteCount04 = 0u;
        reservation.reservedPadding06 = 0u;
    }

    void WritePayloadByte(size_t offset, uint8_t value) {
        ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = static_cast<uint8_t*>(payloadBegin10);
        if (!messageStorage || !packetPayload || offset >= messageStorage->PayloadByteCount()) {
            return;
        }
        packetPayload[offset] = value;
    }

    void WritePayloadU16LE(size_t offset, uint16_t value) {
        ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = static_cast<uint8_t*>(payloadBegin10);
        const uint16_t payloadByteCount =
            messageStorage ? messageStorage->PayloadByteCount() : 0u;
        if (!packetPayload || offset + 1u >= payloadByteCount) {
            return;
        }
        packetPayload[offset] = static_cast<uint8_t>(value & 0xffu);
        packetPayload[offset + 1u] = static_cast<uint8_t>((value >> 8) & 0xffu);
    }

    void WritePayloadU32LE(size_t offset, uint32_t value) {
        ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* const messageStorage = MessageStorage();
        uint8_t* const packetPayload = static_cast<uint8_t*>(payloadBegin10);
        const uint16_t payloadByteCount =
            messageStorage ? messageStorage->PayloadByteCount() : 0u;
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
        ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold* reservation) {
        if (reservation) {
            ClearReservation(*reservation);
        }

        ::mxo::liblttcp::CMessageConnectionMessageStorage_0x4ba208* const messageStorage = MessageStorage();
        const uint8_t* const payloadBase = PayloadBase();
        uint8_t* const packetPayload = static_cast<uint8_t*>(payloadBegin10);
        if (!messageStorage || !payloadBase || !packetPayload || !text) {
            return 0u;
        }

        size_t textLengthWithoutNul = 0u;
        while (textLengthWithoutNul < bound && text[textLengthWithoutNul] != '\0') {
            ++textLengthWithoutNul;
        }

        const uint16_t currentPayloadByteCount = messageStorage->PayloadByteCount();
        const uint16_t remainingAfterLength =
            messageStorage->RemainingAppendableByteCount();
        if (remainingAfterLength < 2u) {
            return 0u;
        }

        const uint16_t storedLength = static_cast<uint16_t>(
            std::min<size_t>(textLengthWithoutNul + 1u, remainingAfterLength - 2u));
        const uint16_t requestedGrowth = static_cast<uint16_t>(storedLength + 2u);
        const uint16_t newPayloadByteCount =
            messageStorage->GrowPayloadByteCount(requestedGrowth);
        if (newPayloadByteCount != currentPayloadByteCount + requestedGrowth) {
            return 0u;
        }

        uint8_t* const reservationHeader = static_cast<uint8_t*>(payloadBegin10) + currentPayloadByteCount;
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
        const ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold& reservation,
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

    ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c messageRef_;
    ::mxo::liblttcp::CMessageConnectionPacketBuilderEnvelope envelope_{};
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

class State7Packet0x0dBuilder final : public PacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x43a9a0 / local packet-builder family `0x004b53f0`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            State7Packet0x0dFixedPayload::kFixedByteCount);
        ClearReservation(reservation14_);
        WritePayloadByte(0x00, State7Packet0x0dFixedPayload::kPayloadTag0d);
        WritePayloadU16LE(State7Packet0x0dFixedPayload::kCharacterNameOffset, 0u);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdLowOffset, 0u);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdHighOffset, 0u);
    }

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* text) {
        if (!text || reservation14_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State7Packet0x0dFixedPayload::kCharacterNameOffset,
                text,
                0xffffu,
                &reservation14_) != 0u) {
            WriteReservedCString(reservation14_, text);
        }
    }

    void SetCharacterIdPair(uint32_t low, uint32_t high) {
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdLowOffset, low);
        WritePayloadU32LE(State7Packet0x0dFixedPayload::kCharacterIdHighOffset, high);
    }

private:
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold reservation14_{};
};

struct State10Packet0x0aFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0a` = `MS_ClaimCharacterNameRequest`
    static constexpr uint8_t kPayloadTag0a = 0x0a;
    static constexpr size_t kCharacterNameOffset = 0x01;
    static constexpr size_t kFixedByteCount = 0x03;
};

class State10Packet0x0aBuilder_0x4b53b4 final : public PacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x43a1f0 / local packet-builder family `0x004b53b4`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            State10Packet0x0aFixedPayload::kFixedByteCount);
        ClearReservation(reservation14_);
        WritePayloadByte(0x00, State10Packet0x0aFixedPayload::kPayloadTag0a);
        WritePayloadU16LE(State10Packet0x0aFixedPayload::kCharacterNameOffset, 0u);
    }

    // anchor: launcher.exe:0x43aa80
    void SetCharacterName(const char* text) {
        if (!text || reservation14_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State10Packet0x0aFixedPayload::kCharacterNameOffset,
                text,
                0xffffu,
                &reservation14_) != 0u) {
            WriteReservedCString(reservation14_, text);
        }
    }

private:
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold reservation14_{};
};

class State11Packet0x4dBuilder final : public PacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x43a470 / local packet-builder family `0x004b53c8`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            State11Packet0x4dFixedPayload::kFixedByteCount);
        ClearReservation(realFirstName14_);
        ClearReservation(realLastName1c_);
        ClearReservation(background24_);
        ClearReservation(gameSessionId2c_);
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
        if (!text || realFirstName14_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kRealFirstNameOffset,
                text,
                0x20u,
                &realFirstName14_) != 0u) {
            WriteReservedCString(realFirstName14_, text);
        }
    }

    // anchor: launcher.exe:0x43a740
    void SetRealLastName(const char* text) {
        if (!text || realLastName1c_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kRealLastNameOffset,
                text,
                0x20u,
                &realLastName1c_) != 0u) {
            WriteReservedCString(realLastName1c_, text);
        }
    }

    // anchor: launcher.exe:0x43a840
    void SetBackground(const char* text) {
        if (!text || background24_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kBackgroundOffset,
                text,
                0x20u,
                &background24_) != 0u) {
            WriteReservedCString(background24_, text);
        }
    }

    // anchor: launcher.exe:0x43a940
    void SetGameSessionId(const char* text) {
        if (!text || gameSessionId2c_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State11Packet0x4dFixedPayload::kGameSessionIdOffset,
                text,
                0xffffu,
                &gameSessionId2c_) != 0u) {
            WriteReservedCString(gameSessionId2c_, text);
        }
    }

private:
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realFirstName14_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realLastName1c_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold background24_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold gameSessionId2c_{};
};

struct State8StructuredMarginPacketFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0f` = `MS_LoadCharacterRequest`
    static constexpr uint8_t kPayloadTag0f = 0x0f;
    static constexpr size_t kGameSessionIdOffset = 0xb9;
    static constexpr size_t kFixedByteCount = 0xbb;
};

class State8StructuredMarginPacket_0x4af2a4 final : public PacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x43ac10 = CLTLoginMediatorPacket0x0f_ResetAndInitialize
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            State8StructuredMarginPacketFixedPayload::kFixedByteCount);
        ClearReservation(reservation14_);
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
        if (!text || reservation14_.reservedContentByteCount04 != 0u) {
            return;
        }
        if (ReserveLengthPrefixedString(
                State8StructuredMarginPacketFixedPayload::kGameSessionIdOffset,
                text,
                0xffffu,
                &reservation14_) != 0u) {
            WriteReservedCString(reservation14_, text);
        }
    }

private:
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold reservation14_{};
};

struct State6Packet0x06FixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x06` = `MS_ConnectRequest`
    static constexpr uint8_t kPayloadTag06 = 0x06;
    static constexpr size_t kLauncherVersionOffset = 0x01;
    static constexpr size_t kClientVersionOffset = 0x05;
    static constexpr size_t kStateByteOffset = 0x09;
    static constexpr uint8_t kStateByteValue = 0x01;
    static constexpr size_t kFixedDwordAOffset = 0x0a;
    static constexpr uint32_t kFixedDwordA = 0x11186887u;
    static constexpr size_t kFixedDwordEOffset = 0x0e;
    static constexpr uint32_t kFixedDwordE = 0x7460a4b0u;
    static constexpr size_t kGobFileGuidOffset = 0x12;
    static constexpr size_t kCurrentHelperPhaseOffset = 0x22;
    static constexpr size_t kFixedByteCount = 0x23;
};

class State6Packet0x06Builder_0x4b5364 final : public PacketBuilderEnvelopeBase {
public:
    // anchor: launcher.exe:0x43b8f0 / local packet-builder family `0x004b5364`
    void ResetAndInitialize() {
        ResetPacketPayloadBuilderScaffold(
            State6Packet0x06FixedPayload::kFixedByteCount);
        WritePayloadByte(0x00, State6Packet0x06FixedPayload::kPayloadTag06);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kLauncherVersionOffset, 0u);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kClientVersionOffset, 0u);
        WritePayloadByte(
            State6Packet0x06FixedPayload::kStateByteOffset,
            State6Packet0x06FixedPayload::kStateByteValue);
        WritePayloadU32LE(
            State6Packet0x06FixedPayload::kFixedDwordAOffset,
            State6Packet0x06FixedPayload::kFixedDwordA);
        WritePayloadU32LE(
            State6Packet0x06FixedPayload::kFixedDwordEOffset,
            State6Packet0x06FixedPayload::kFixedDwordE);
        SetGobFileGuid({0u, 0u, 0u, 0u});
        WritePayloadByte(State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset, 0u);
    }

    void SetLauncherVersion(uint32_t value) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kLauncherVersionOffset, value);
    }

    void SetClientVersion(uint32_t value) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kClientVersionOffset, value);
    }

    void SetGobFileGuid(const std::array<uint32_t, 4>& guidWords) {
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x0, guidWords[0]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x4, guidWords[1]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x8, guidWords[2]);
        WritePayloadU32LE(State6Packet0x06FixedPayload::kGobFileGuidOffset + 0xc, guidWords[3]);
    }

    void SetCurrentHelperPhaseByte(uint8_t value) {
        WritePayloadByte(State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset, value);
    }
};

}  // namespace mxo::ltlogin
