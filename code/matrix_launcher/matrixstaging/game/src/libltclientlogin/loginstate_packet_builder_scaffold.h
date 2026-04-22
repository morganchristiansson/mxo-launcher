#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../runtime/src/libltmessaging/messageconnection.h"

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

} // namespace ltlogin

// =============================================================================
// Packet_MsConnectChallengeResponse_0x4b5378 - Packet 0x08 (MS_ConnectChallengeResponse) builder
// =============================================================================
// anchor: launcher.exe vtable 0x004b5378 (10 slots, 40 bytes)
// anchor: launcher.exe:0x43fd20..0x440a30 = State6 opcode-7 handler builds opcode-0x08 reply
//
// VTable layout at 0x4b5378 (10 slots observed in object scanner):
// - Slot 0 (+0x00): destructor
// - Slot 1 (+0x04): StubReturn0
// - Slot 2 (+0x08): DebugString
// - Slot 3 (+0x0c): ResetAndInitialize (packet 0x08 setup at 0x43fd20..0x43fd70)
// - Slot 4 (+0x10): GetPayloadBase
// - Remaining slots: inherited patterns
//
// Object layout: inherits Packet_0x4af2a4 base at +0x00, size = 0x28 bytes
// No reservation scaffolds needed - this is a fixed-size 17-byte payload.
//
// Usage pattern from 0x4409b0 (State6 slot6 opcode-7 handling):
// 1. Stack-allocate Packet_MsConnectChallengeResponse_0x4b5378
// 2. Call ResetAndInitialize - reserves 0x11 bytes, writes opcode 0x08, clears fields
// 3. Write goHereAddr at +0x09 (from opcode-7 reply payload +0x01)
// 4. Write sessionSecret at +0x0d (from opcode-7 reply payload +0x09)
// 5. Send through 0x41af70 = CLTLoginMediator_SendCurrentMarginPacket
// =============================================================================

namespace mxo::ltlogin {

struct State6ChallengeResponsePayload {
  // anchor: launcher.exe opcode analysis at 0x4409b0..0x4409d0
  // Packet layout: [0x08][statusCode:4][metricIdBase:4][goHereAddr:4][sessionSecret:4]
  static constexpr uint8_t kPayloadTag08 = 0x08;
  static constexpr size_t kStatusCodeOffset = 0x01;
  static constexpr size_t kMetricIdBaseOffset = 0x05;
  static constexpr size_t kGoHereAddrOffset = 0x09;
  static constexpr size_t kSessionSecretOffset = 0x0d;
  static constexpr size_t kFixedByteCount = 0x11;  // 17 bytes total
};

// anchor: launcher.exe:0x43fd20 / packet 0x08 challenge response builder for state6
// Margin opcode 0x08 = MS_ConnectChallengeResponse (reply to opcode 0x07)
class Packet_MsConnectChallengeResponse_0x4b5378 : public mxo::liblttcp::Packet_0x4af2a4 {
 public:
  // anchor: launcher.exe:0x43fd20..0x43fd70
  // Original implementation pattern:
  // 1. Creates/retains message ref CMessageConnectionMessageRef_0x4ba23c
  // 2. Reserves payload byte count 0x11 (17 bytes)
  // 3. Writes opcode byte 0x08 at payload[0]
  // 4. Clears statusCode, metricIdBase, goHereAddr, sessionSecret to 0
  void ResetAndInitialize() {
    if (!messageRef08) {
      messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
      if (messageRef08) {
        messageRef08->AddRef();
        messageRef08->ResetForPacketBuilder(false, 0);
      }
    }

    if (messageRef08 && messageRef08->messageStorage0c) {
      messageRef08->messageStorage0c->ResetPayloadByteCount(
          State6ChallengeResponsePayload::kFixedByteCount);
      payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
      payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
    }

    uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
    if (payload) {
      payload[0] = State6ChallengeResponsePayload::kPayloadTag08;
      *reinterpret_cast<uint32_t*>(payload + State6ChallengeResponsePayload::kStatusCodeOffset) = 0u;
      *reinterpret_cast<uint32_t*>(payload + State6ChallengeResponsePayload::kMetricIdBaseOffset) =
          0u;
      *reinterpret_cast<uint32_t*>(payload + State6ChallengeResponsePayload::kGoHereAddrOffset) =
          0u;
      *reinterpret_cast<uint32_t*>(payload + State6ChallengeResponsePayload::kSessionSecretOffset) =
          0u;
    }
  }

  // anchor: launcher.exe:0x4409c0..0x4409d0 - inline writes from parsed opcode-7 reply
  void SetChallengeResponseFields(uint32_t goHereAddr, uint32_t sessionSecret) {
    uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
    if (payload) {
      *reinterpret_cast<uint32_t*>(payload + State6ChallengeResponsePayload::kGoHereAddrOffset) =
          goHereAddr;
      *reinterpret_cast<uint32_t*>(payload +
                                    State6ChallengeResponsePayload::kSessionSecretOffset) =
          sessionSecret;
    }
  }
};

}  // namespace ltlogin

// =============================================================================
// Packet_MsLoadCharacterRequest_0x4b5418 - Packet 0x0f (MS_LoadCharacterRequest) builder
// =============================================================================
// anchor: launcher.exe vtable 0x004b5418 (5 slots, 20 bytes)
// anchor: launcher.exe:0x43ac10 = Packet_MsLoadCharacterRequest_0x4b5418::ResetAndInitialize
// anchor: launcher.exe:0x43acf0 = CLTLoginMediator::ReserveGameSessionId (mediator helper)
// anchor: launcher.exe:0x43ada0 = CLTLoginMediator::SetGameSessionId (mediator helper)
//
// VTable layout at 0x4b5418 (5 slots):
// - Slot 0 (+0x00): 0x443aa0 - destructor (inherited from Packet_0x4af2a4)
// - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited, returns 0)
// - Slot 2 (+0x08): 0x4425f0 - DebugString (inherited or packet-specific)
// - Slot 3 (+0x0c): 0x43ac10 - ResetAndInitialize (OVERRIDDEN - packet 0x0f setup)
// - Slot 4 (+0x10): 0x481760 - GetPayloadBase (inherited)
//
// Object layout: inherits Packet_0x4af2a4 base at +0x00, size = 0x28 bytes
// Note: The original 0x43acf0/0x43ada0 helpers operate on CLTLoginMediator fields,
// NOT on the packet builder. The mediator has its own caching fields for the
// GameSessionID reservation write pointer and length.
//
// Usage pattern from 0x43bd20 (State8 slot3):
//   1. Stack-allocate Packet_MsLoadCharacterRequest_0x4b5418
//   2. Call 0x43ac10 = ResetAndInitialize
//   3. Write fixed dwords at payload +0x01 and +0x05 (character ID pair)
//   4. Write selection blocks at payload +0x09, +0x19, +0x29, etc.
//   5. Call mediator's SetGameSessionId helper (operates on mediator fields)
//   6. Send through 0x41af70 = CLTLoginMediator_SendCurrentMarginPacket
// =============================================================================

namespace mxo::ltlogin {

// Static-RE faithful implementation of packet 0x0f builder.
// anchor: launcher.exe:0x43ac10
//
// Margin opcode 0x0f = MS_LoadCharacterRequest (used by state8)
class Packet_MsLoadCharacterRequest_0x4b5418 : public mxo::liblttcp::Packet_0x4af2a4 {
public:

    // anchor: launcher.exe:0x43ac10 = Packet_MsLoadCharacterRequest_0x4b5418::ResetAndInitialize
    // Original implementation:
    //   1. Calls Packet_0x4af2a4 default ctor (sets vtable to 0x4af2a4, creates message ref)
    //   2. Overwrites vtable to 0x4b5418
    //   3. Sets createRefParam0c = 0
    //   4. Sets payloadBegin10 = nopatchLauncherVersionValue04 (payload base)
    //   5. Calls messageStorage->GrowPayloadByteCount(0xbb)
    //   6. Writes opcode 0x0f to payload[0]
    //   7. Clears payload[1-4] and payload[5-8] (character ID dwords)
    //   8. Clears payload[0xb9-0xba] (GameSessionID offset)
    //   9. Clears heapString14 and payloadLength14
    void ResetAndInitialize() {
        // Step 1-2: Base ctor already called if constructed on stack
        // Note: In C++ the vtable pointer is implicit, set by the constructor.
        // The original at 0x43ac14 calls Packet_0x4af2a4 ctor, then overwrites vtable.
        // With C++ inheritance, the vtable is already correct for Packet_MsLoadCharacterRequest_0x4b5418.

        // Step 3: Clear flag
        createRefParam0c = 0u;

        // Step 4: Cache payload base
        payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

        // Step 5: Grow payload to 0xbb bytes
        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->GrowPayloadByteCount(0xbb);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        // Step 6-8: Initialize fixed payload structure
        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = 0x0fu;  // Opcode
            // Clear character ID dwords at +0x01 and +0x05
            *reinterpret_cast<uint32_t*>(payload + 0x01) = 0u;
            *reinterpret_cast<uint32_t*>(payload + 0x05) = 0u;
            // Clear GameSessionID offset at +0xb9
            *reinterpret_cast<uint16_t*>(payload + 0xb9) = 0u;
        }

        // Step 9: Clear string/length fields
        debugString14 = nullptr;
        payloadSize18 = 0u;

    }

    // Note: Original code accesses payloadAlias10 field directly.

    // Friend declarations for mediator helper access
    friend class CLTLoginMediator;

public:
    // Override virtual methods to match 5-slot vtable
    ~Packet_MsLoadCharacterRequest_0x4b5418() override = default;
    uint32_t StubReturn0() override { return 0u; }
    void DebugString(int /*formatType*/ = 2) override {}
    void InitializePayloadSize() override {}
    void* GetPayloadBase() override { return payloadAlias10; }
};

// Packet_MsLoadCharacterRequest_0x4b5418 layout:
//   Packet_0x4af2a4 base: 0x28 bytes (+0x00..+0x27)
// Total: 0x28 bytes
static_assert(sizeof(Packet_MsLoadCharacterRequest_0x4b5418) == 0x28, "Packet_MsLoadCharacterRequest_0x4b5418 size mismatch");

}  // namespace mxo::ltlogin

// =============================================================================
// Packet builder envelope base - shared helpers (source-only convenience)
// =============================================================================
// Note: These helpers are source-only conveniences. The original launcher.exe
// uses direct payload manipulation inlined into the packet-specific builders.
// Keep these for source code clarity, but anchor packet builders to static-RE.
namespace mxo::ltlogin {

// Note: The original launcher.exe does NOT have a shared "PacketBuilderEnvelopeBase" class.
// Each packet builder class directly inherits from Packet_0x4af2a4 with its own vtable.
// Source code should use the concrete classes (Packet_MsDeleteCharacterRequest_0x4b53f0, Packet_MsClaimCharacterNameRequest_0x4b53b4, Packet_MsCreateCharacterRequest_0x4b53c8, Packet_MsLoadCharacterRequest_0x4b5418)
// and access fields directly.

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

// anchor: launcher.exe vtable 0x004b53f0 / packet 0x0d builder
// anchor: launcher.exe:0x43a9a0 = ResetAndInitialize
// anchor: launcher.exe:0x43aa80 = SetCharacterName (mediator helper, not class method)
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, reservation14_ at +0x28
//
// Margin opcode 0x0d = MS_DeleteCharacterRequest (used by state7 for character selection probe)
class Packet_MsDeleteCharacterRequest_0x4b53f0 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x43a9a0
    void ResetAndInitialize() {
        // Initialize message ref if not already done by ctor
        if (!messageRef08) {
            messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
            if (messageRef08) {
                messageRef08->AddRef();
                messageRef08->ResetForPacketBuilder(false, 0);
            }
        }

        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->ResetPayloadByteCount(
                State7Packet0x0dFixedPayload::kFixedByteCount);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = State7Packet0x0dFixedPayload::kPayloadTag0d;
            *reinterpret_cast<uint16_t*>(payload + State7Packet0x0dFixedPayload::kCharacterNameOffset) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State7Packet0x0dFixedPayload::kCharacterIdLowOffset) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State7Packet0x0dFixedPayload::kCharacterIdHighOffset) = 0u;
        }

        reservation14_.writePointer00 = nullptr;
        reservation14_.reservedContentByteCount04 = 0u;
        reservation14_.reservedPadding06 = 0u;
    }

    // Reservation scaffold for character name (follows base class fields).
    // anchor: launcher.exe:0x43a9a0 decompilation shows this field pattern.
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold reservation14_{};
};

static_assert(offsetof(Packet_MsDeleteCharacterRequest_0x4b53f0, reservation14_) == 0x28, "Packet_MsDeleteCharacterRequest_0x4b53f0 reservation14_ offset mismatch");

struct State10Packet0x0aFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0a` = `MS_ClaimCharacterNameRequest`
    static constexpr uint8_t kPayloadTag0a = 0x0a;
    static constexpr size_t kCharacterNameOffset = 0x01;
    static constexpr size_t kFixedByteCount = 0x03;
};

// anchor: launcher.exe vtable 0x004b53b4 / packet 0x0a builder
// anchor: launcher.exe:0x43a1f0 = ResetAndInitialize
// anchor: launcher.exe:0x43aa80 = SetCharacterName (mediator helper)
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, reservation14_ at +0x28
//
// Margin opcode 0x0a = MS_ClaimCharacterNameRequest (used by state10)
class Packet_MsClaimCharacterNameRequest_0x4b53b4 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x43a1f0
    void ResetAndInitialize() {
        if (!messageRef08) {
            messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
            if (messageRef08) {
                messageRef08->AddRef();
                messageRef08->ResetForPacketBuilder(false, 0);
            }
        }

        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->ResetPayloadByteCount(
                State10Packet0x0aFixedPayload::kFixedByteCount);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = State10Packet0x0aFixedPayload::kPayloadTag0a;
            *reinterpret_cast<uint16_t*>(payload + State10Packet0x0aFixedPayload::kCharacterNameOffset) = 0u;
        }

        reservation14_.writePointer00 = nullptr;
        reservation14_.reservedContentByteCount04 = 0u;
        reservation14_.reservedPadding06 = 0u;
    }

    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold reservation14_{};
};

static_assert(offsetof(Packet_MsClaimCharacterNameRequest_0x4b53b4, reservation14_) == 0x28, "Packet_MsClaimCharacterNameRequest_0x4b53b4 reservation14_ offset mismatch");

// anchor: launcher.exe vtable 0x004b53c8 / packet 0x0c builder
// anchor: launcher.exe:0x43a470 = ResetAndInitialize
// anchor: launcher.exe:0x43a640/0x43a740/0x43a840/0x43a940 = Set* helpers (mediator methods)
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, then 4 reservation scaffolds at +0x28/+0x30/+0x38/+0x40
//
// Margin opcode 0x0c (0x4d) = MS_CreateCharacterRequest (used by state11)
class Packet_MsCreateCharacterRequest_0x4b53c8 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x43a470
    void ResetAndInitialize() {
        if (!messageRef08) {
            messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
            if (messageRef08) {
                messageRef08->AddRef();
                messageRef08->ResetForPacketBuilder(false, 0);
            }
        }

        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->ResetPayloadByteCount(
                State11Packet0x4dFixedPayload::kFixedByteCount);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = State11Packet0x4dFixedPayload::kPayloadTag0c;
            *reinterpret_cast<uint16_t*>(payload + State11Packet0x4dFixedPayload::kRealFirstNameOffset) = 0u;
            *reinterpret_cast<uint16_t*>(payload + State11Packet0x4dFixedPayload::kRealLastNameOffset) = 0u;
            *reinterpret_cast<uint16_t*>(payload + State11Packet0x4dFixedPayload::kBackgroundOffset) = 0u;
            *reinterpret_cast<uint16_t*>(payload + State11Packet0x4dFixedPayload::kGameSessionIdOffset) = 0u;
        }

        ClearReservation(realFirstName14_);
        ClearReservation(realLastName1c_);
        ClearReservation(background24_);
        ClearReservation(gameSessionId2c_);
    }

    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realFirstName14_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold realLastName1c_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold background24_{};
    ::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold gameSessionId2c_{};

private:
    void ClearReservation(::mxo::liblttcp::CMessageConnectionPacketBuilderReservationScaffold& res) {
        res.writePointer00 = nullptr;
        res.reservedContentByteCount04 = 0u;
        res.reservedPadding06 = 0u;
    }
};

static_assert(offsetof(Packet_MsCreateCharacterRequest_0x4b53c8, realFirstName14_) == 0x28, "Packet_MsCreateCharacterRequest_0x4b53c8 realFirstName14_ offset mismatch");
static_assert(offsetof(Packet_MsCreateCharacterRequest_0x4b53c8, realLastName1c_) == 0x30, "Packet_MsCreateCharacterRequest_0x4b53c8 realLastName1c_ offset mismatch");
static_assert(offsetof(Packet_MsCreateCharacterRequest_0x4b53c8, background24_) == 0x38, "Packet_MsCreateCharacterRequest_0x4b53c8 background24_ offset mismatch");
static_assert(offsetof(Packet_MsCreateCharacterRequest_0x4b53c8, gameSessionId2c_) == 0x40, "Packet_MsCreateCharacterRequest_0x4b53c8 gameSessionId2c_ offset mismatch");

struct State8MarginPacketFixedPayload {
    // anchor: launcher.exe:0x41bf70 = CLTLoginMediator_MarginOpcodeName
    // raw margin opcode `0x0f` = `MS_LoadCharacterRequest`
    static constexpr uint8_t kPayloadTag0f = 0x0f;
    static constexpr size_t kGameSessionIdOffset = 0xb9;
    static constexpr size_t kFixedByteCount = 0xbb;
};

// Note: The original launcher.exe does NOT have a separate derived class for
// packet 0x0f building. Code at 0x43bd20 uses Packet_MsLoadCharacterRequest_0x4b5418 directly on the stack.
// The SetGameSessionId logic at 0x43ada0 is a CLTLoginMediator method that operates
// on the mediator's caching fields (networkEngine14, authConnection18), not on the
// packet object itself.
//
// For source code clarity, packet 0x0f sending should:
//   1. Stack-allocate Packet_MsLoadCharacterRequest_0x4b5418
//   2. Call ResetAndInitialize()
//   3. Write payload fields directly via PayloadBase()
//   4. Call CLTLoginMediator::SetGameSessionIdOnPacket() helper (mediator method)
//   5. Send via CLTLoginMediator::SendCurrentMarginPacket()

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

// anchor: launcher.exe vtable 0x004b5364 / packet 0x06 builder
// anchor: launcher.exe:0x43b8f0 = ResetAndInitialize
//
// Margin opcode 0x06 = MS_ConnectRequest (used by state6)
class Packet_MsConnectRequest_0x4b5364 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x43b8f0
    void ResetAndInitialize() {
        if (!messageRef08) {
            messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
            if (messageRef08) {
                messageRef08->AddRef();
                messageRef08->ResetForPacketBuilder(false, 0);
            }
        }

        if (messageRef08 && messageRef08->messageStorage0c) {
            messageRef08->messageStorage0c->ResetPayloadByteCount(
                State6Packet0x06FixedPayload::kFixedByteCount);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = State6Packet0x06FixedPayload::kPayloadTag06;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kLauncherVersionOffset) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kClientVersionOffset) = 0u;
            payload[State6Packet0x06FixedPayload::kStateByteOffset] = State6Packet0x06FixedPayload::kStateByteValue;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kFixedDwordAOffset) = State6Packet0x06FixedPayload::kFixedDwordA;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kFixedDwordEOffset) = State6Packet0x06FixedPayload::kFixedDwordE;
            // GobFileGuid at +0x12 (4 dwords)
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x0) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x4) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0x8) = 0u;
            *reinterpret_cast<uint32_t*>(payload + State6Packet0x06FixedPayload::kGobFileGuidOffset + 0xc) = 0u;
            payload[State6Packet0x06FixedPayload::kCurrentHelperPhaseOffset] = 0u;
        }
    }

    // Direct payload access for static-RE faithful manipulation.
    uint8_t* PayloadBase() { return static_cast<uint8_t*>(payloadAlias10); }
    const uint8_t* PayloadBase() const { return static_cast<const uint8_t*>(payloadAlias10); }
};

static_assert(sizeof(Packet_MsConnectRequest_0x4b5364) == sizeof(mxo::liblttcp::Packet_0x4af2a4), "Packet_MsConnectRequest_0x4b5364 size mismatch");

}  // namespace mxo::ltlogin
