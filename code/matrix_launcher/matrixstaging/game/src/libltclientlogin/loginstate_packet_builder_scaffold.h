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
    // Current best local helper shape on top of the shared `0x439840` packet front matter:
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
// Packet builder base - shared helpers (source-only convenience)
// =============================================================================
// Note: These helpers are source-only conveniences. The original launcher.exe
// uses direct payload manipulation inlined into the packet-specific builders.
// Keep these for source code clarity, but anchor packet builders to static-RE.
namespace mxo::ltlogin {

// Note: The original launcher.exe does NOT have a shared "PacketBuilderEnvelopeBase" class.
// Each packet builder class directly inherits from Packet_0x4af2a4 with its own vtable.
// Source code should use the concrete classes (Packet_MsDeleteCharacterRequest_0x4b53f0, Packet_AsAuthChallengeResponse_0x4b53b4, Packet_MsCreateCharacterRequest_0x4b53c8, Packet_MsLoadCharacterRequest_0x4b5418)
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

// Note: keep the `_14_` suffix as the source-of-truth offset cue here.
// `offsetof` on these virtual packet builders triggers noisy non-standard-layout warnings.

// anchor: launcher.exe vtable 0x004b5404 / packet 0x0e reply parser
// anchor: launcher.exe:0x43aae0 = ctor / parse wrapper
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, no additional raw fields.
// Static-RE field mapping for the decompiler view:
// - mbr_0x4  -> payloadPtr04
// - mbr_0x8  -> messageRef08
// - mbr_0xc  -> createRefParam0c
// - mbr_0x10 -> payloadAlias10
//
// Margin opcode 0x0e = MS_DeleteCharacterReply (used by state7 for delete-character completion)
class Packet_MsDeleteCharacterReply_0x4b5404 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    // anchor: launcher.exe:0x43aae0
    Packet_MsDeleteCharacterReply_0x4b5404(
        mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* param_1,
        char param_2) {
        payloadPtr04 = 0u;
        messageRef08 = param_1;
        if (param_1 != nullptr) {
            param_1->AddRef();
        }

        if (param_1 != nullptr && param_1->messageStorage0c != nullptr) {
            const uint8_t* const payloadBase = param_1->messageStorage0c->PayloadBase();
            if (payloadBase != nullptr) {
                if (param_1->headerless10 == 0u) {
                    payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(const_cast<uint8_t*>(payloadBase)));
                } else {
                    const uint8_t headerByte = payloadBase[0x0du];
                    const uint32_t payloadBaseAddress =
                        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param_1->messageStorage0c->PayloadBase()));
                    const uint32_t logicalPayloadAddress =
                        ::g_MessageOffsetLookupTable[headerByte >> 4 & 7] +
                        ::g_MessageOffsetLookupTable[headerByte & 7] + payloadBaseAddress + 0x1eu;
                    payloadPtr04 = logicalPayloadAddress;
                    param_1->headerless10 = 1u;
                }
            }
        }

        createRefParam0c = static_cast<uint8_t>(param_2);
        payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);
        if (param_2 == '\0') {
            if (messageRef08 != nullptr) {
                messageRef08->GrowPayloadByteCount(0x0du);
            }
            if (messageRef08 != nullptr && messageRef08->messageStorage0c != nullptr) {
                payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
                payloadPtr04 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payloadAlias10));
            }
            uint8_t* const payload = static_cast<uint8_t*>(payloadAlias10);
            if (payload != nullptr) {
                payload[0] = 0x0eu;
                *reinterpret_cast<uint32_t*>(payload + 0x01u) = 0u;
                *reinterpret_cast<uint32_t*>(payload + 0x05u) = 0u;
                *reinterpret_cast<uint32_t*>(payload + 0x09u) = 0u;
            }
        }
    }
};

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
class Packet_AsAuthChallengeResponse_0x4b53b4 : public mxo::liblttcp::Packet_0x4af2a4 {
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

// Note: keep the `_14_` suffix as the source-of-truth offset cue here.
// `offsetof` on these virtual packet builders triggers noisy non-standard-layout warnings.

// anchor: launcher.exe vtable 0x004b53c8 / packet 0x0c builder
// anchor: launcher.exe:0x43a470 = ResetAndInitialize
// anchor: launcher.exe:0x43a640/0x43a740/0x43a840/0x43a940 = Set* helpers (mediator methods)
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, then 4 reservation scaffolds at +0x28/+0x30/+0x38/+0x40
//
// Margin opcode 0x0c (0x4d) = MS_CreateCharacterRequest (used by state11)
class Packet_MsCreateCharacterRequest_0x4b53c8 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
    Packet_MsCreateCharacterRequest_0x4b53c8() = default;

    // anchor: launcher.exe:0x43a330
    Packet_MsCreateCharacterRequest_0x4b53c8(
        ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessageRef,
        char resolveFieldsNow);

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

    // anchor: launcher.exe:0x43a2d0 / shared vtable family parse refresh used by state10
    void InitializePayloadSize() override;

    // anchor: launcher.exe:0x43a2d0
    void ResolveFields(char resolveFieldsNow);

    bool valid = false;
    uint32_t status = 0;
    uint32_t characterIdLow = 0;
    uint32_t characterIdHigh = 0;
    uint16_t optionalTextOffset01 = 0;
    const char* optionalText = nullptr;
    uint16_t optionalTextLength = 0;

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

// Note: keep the `_14_/_1c_/_24_/_2c_` suffixes as the source-of-truth offset cues here.
// `offsetof` on these virtual packet builders triggers noisy non-standard-layout warnings.

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
            messageRef08->messageStorage0c->ResetPayloadByteCount(0x23u);
            payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
            payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
        }

        uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
        if (payload) {
            payload[0] = 0x06;
            *reinterpret_cast<uint32_t*>(payload + 0x01) = 0u;
            *reinterpret_cast<uint32_t*>(payload + 0x05) = 0u;
            payload[0x09] = 0x01;
            *reinterpret_cast<uint32_t*>(payload + 0x0a) = 0x11186887u;
            *reinterpret_cast<uint32_t*>(payload + 0x0e) = 0x7460a4b0u;
            // GobFileGuid at +0x12 (4 dwords)
            *reinterpret_cast<uint32_t*>(payload + 0x12 + 0x0) = 0u;
            *reinterpret_cast<uint32_t*>(payload + 0x12 + 0x4) = 0u;
            *reinterpret_cast<uint32_t*>(payload + 0x12 + 0x8) = 0u;
            *reinterpret_cast<uint32_t*>(payload + 0x12 + 0xc) = 0u;
            payload[0x22] = 0u;
        }
    }

    // Direct payload access for static-RE faithful manipulation.
    uint8_t* PayloadBase() { return static_cast<uint8_t*>(payloadAlias10); }
    const uint8_t* PayloadBase() const { return static_cast<const uint8_t*>(payloadAlias10); }
};

static_assert(sizeof(Packet_MsConnectRequest_0x4b5364) == sizeof(mxo::liblttcp::Packet_0x4af2a4), "Packet_MsConnectRequest_0x4b5364 size mismatch");

// =============================================================================
// Packet_AsGetPublicKeyRequest_0x4b6c74 - Shared auth packet family for raw opcodes 0x06 / 0x0b
// =============================================================================
// anchor: launcher.exe vtable 0x004b6c74
// anchor: launcher.exe:0x447eb0 = AuthBootstrap680Child_0x441290::SendGetPublicKeyRequest
// anchor: launcher.exe:0x444390 = Packet_AsGetPublicKeyRequest_0x4b6c74::Packet_AsGetPublicKeyRequest_ctor
// anchor: launcher.exe:0x4449c0 = Packet_AsGetPublicKeyRequest_0x4b6c74::AuthBootstrap680AuthReplyParseObject_Copy
//
// Source-semantic model:
// - outbound role: compact raw-auth opcode `0x06` builder using the inherited packet base
// - inbound role: larger auth-reply parse shell used by opcode `0x0b` ctor/copy helper family
//
// Fidelity note:
// - launcher.exe overlays the parse shell on the same vtable family rather than expressing it as a
//   normal C++ subclass extension. Source code does not need that ABI trick, so this class keeps
//   both semantic roles together as one larger source-owned type.
class Packet_AsGetPublicKeyRequest_0x4b6c74 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
 static constexpr uint8_t kPayloadTag06 = 0x06;
 static constexpr size_t kLauncherVersionOffset = 0x01;
 static constexpr size_t kCurrentPublicKeyIdOffset = 0x05;
 static constexpr size_t kFixedByteCount = 0x09;

 // Source-semantic overlap with inherited Packet_0x4af2a4 storage:
 // - payloadPtr04      <-> packetBody04
 // - messageRef08      <-> incomingMessage08
 // - createRefParam0c  <-> resolveFields0c
 // - payloadAlias10    <-> replyHeader10
 // - debugString14     <-> stringField05Bytes14
 // - payloadSize18     <-> stringField05Length18
 // - packetType1a/1b   <-> padding1a
 // - characterIdLow1c  <-> authDataBytes1c
 // - characterIdHigh20 <-> authDataByteLength20/padding22
 // The parse-shell-only tail begins after that overlapped prefix.
 const uint8_t* PacketBody04() const { return reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(payloadPtr04)); }
 void SetPacketBody04(const uint8_t* packetBody) { payloadPtr04 = reinterpret_cast<uint32_t>(packetBody); }

 mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* IncomingMessage08() const { return messageRef08; }
 void SetIncomingMessage08(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingMessage) { messageRef08 = incomingMessage; }

 uint8_t ResolveFields0c() const { return createRefParam0c; }
 void SetResolveFields0c(uint8_t resolveFields) { createRefParam0c = resolveFields; }

 const uint8_t* ReplyHeader10() const { return static_cast<const uint8_t*>(payloadAlias10); }
 void SetReplyHeader10(const uint8_t* replyHeader) { payloadAlias10 = const_cast<uint8_t*>(replyHeader); }

 const uint8_t* StringField05Bytes14() const { return reinterpret_cast<const uint8_t*>(debugString14); }
 void SetStringField05Bytes14(const uint8_t* stringFieldBytes) { debugString14 = reinterpret_cast<const char*>(stringFieldBytes); }

 uint16_t StringField05Length18() const { return payloadSize18; }
 void SetStringField05Length18(uint16_t stringFieldLength) { payloadSize18 = stringFieldLength; }

 const uint8_t* AuthDataBytes1c() const { return reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(characterIdLow1c)); }
 void SetAuthDataBytes1c(const uint8_t* authDataBytes) { characterIdLow1c = reinterpret_cast<uint32_t>(authDataBytes); }

 uint16_t AuthDataByteLength20() const { return static_cast<uint16_t>(characterIdHigh20 & 0xffffu); }
 void SetAuthDataByteLength20(uint16_t authDataByteLength) {
  characterIdHigh20 = (characterIdHigh20 & 0xffff0000u) | static_cast<uint32_t>(authDataByteLength);
 }

 void ClearAuthDataPadding22() { characterIdHigh20 &= 0xffffu; }
 const uint8_t* encryptedPrivateExponentBytes24 = nullptr;
 uint16_t encryptedPrivateExponentByteLength28 = 0u;
 std::array<uint8_t, 2> padding2a{};
 const uint8_t* opaqueField0fBytes2c = nullptr;
 uint16_t opaqueField0fByteLength30 = 0u;
 std::array<uint8_t, 2> padding32{};
 const uint8_t* opaqueField11Bytes34 = nullptr;
 uint16_t opaqueField11ByteLength38 = 0u;
 std::array<uint8_t, 2> padding3a{};
 const uint8_t* characterTempRecords3c = nullptr;
 uint16_t characterTempRecordCount40 = 0u;
 std::array<uint8_t, 2> padding42{};
 const uint8_t* worldTempRecords44 = nullptr;
 uint16_t worldTempRecordCount48 = 0u;
 std::array<uint8_t, 2> padding4a{};
 const uint8_t* opaqueField1bBytes4c = nullptr;
 uint16_t opaqueField1bByteLength50 = 0u;
 std::array<uint8_t, 2> padding52{};
 const uint8_t* replyString1dBytes54 = nullptr;
 uint16_t replyString1dByteLength58 = 0u;
 std::array<uint8_t, 2> padding5a{};
 AuthBootstrap680AuthReplyParseAccessor10Sketch worldDescriptorAccessor5c{};
 const uint8_t* currentWorldTempRecord6c = nullptr;
 AuthBootstrap680AuthReplyParseAccessor10Sketch slotRecordAccessor70{};
 const uint8_t* currentCharacterTempRecord80 = nullptr;
 const uint8_t* currentCharacterHandle84 = nullptr;
 uint16_t currentCharacterHandleByteLength88 = 0u;
 std::array<uint8_t, 2> padding8a{};

 // anchor: launcher.exe:0x447eb0 / local packet init after Packet_0x4af2a4 default ctor
 // The original grows a 9-byte payload, zeros both trailing dwords, then the caller patches in
 // child `+0x2c` launcherVersion and child `+0x9c` currentPublicKeyId before send.
 void InitializePayloadSize() override {
  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

  if (!messageRef08 || !messageRef08->messageStorage0c) {
   return;
  }

  messageRef08->GrowPayloadByteCount(kFixedByteCount);

  // Source-owned stability: refresh the inherited payload pointer after growth before writing.
  payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
  payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);

  uint8_t* payload = PayloadBase();
  if (payload) {
   payload[0] = kPayloadTag06;
   *reinterpret_cast<uint32_t*>(payload + kLauncherVersionOffset) = 0u;
   *reinterpret_cast<uint32_t*>(payload + kCurrentPublicKeyIdOffset) = 0u;
  }

  debugString14 = nullptr;
  payloadSize18 = 0u;
 }

 uint8_t* PayloadBase() { return static_cast<uint8_t*>(payloadAlias10); }
 const uint8_t* PayloadBase() const { return static_cast<const uint8_t*>(payloadAlias10); }
};

// =============================================================================
// Packet_AsGetPublicKeyReply_0x4b6ca4 - Parse/builder accessor for MS_ConnectChallenge (opcode 0x07)
// =============================================================================
// anchor: launcher.exe vtable 0x004b6ca4 (5 slots, 20 bytes)
// anchor: launcher.exe:0x443910 = Packet_AsGetPublicKeyReply_0x4b6ca4::InitFromIncomingMessage
// anchor: launcher.exe:0x4439f0 = Packet_AsGetPublicKeyReply_0x4b6ca4::ResetAndInitialize
//
// Recovered role:
// - the class directly subclasses Packet_0x4af2a4 and reuses the inherited fields at
//   +0x10/+0x14/+0x18/+0x1c/+0x20/+0x24 rather than adding new raw members
// - `0x443910` can either attach to an existing message-ref and decode the payload view, or grow
//   a builder payload and stamp opcode 0x07 with zeroed trailing fields
// - state6 later consumes the first three dwords as goHereAddr/goHerePort/sessionSecret
//
// Fixed payload bytes initialized by `0x443910` when the object is used as a local packet view:
//   [0x00] opcode             = 0x07
//   [0x01] goHereAddr         = dword
//   [0x05] goHerePort         = dword
//   [0x09] sessionSecret      = dword
//   [0x0d] trailingWord0d     = word
//   [0x0f] trailingByte0f     = byte
//   [0x10] trailingWord10     = word
// =============================================================================

struct State6Packet0x07FixedPayload {
 static constexpr uint8_t kPayloadTag07 = 0x07;
 static constexpr size_t kGoHereAddrOffset = 0x01;
 static constexpr size_t kGoHerePortOffset = 0x05;
 static constexpr size_t kSessionSecretOffset = 0x09;
 static constexpr size_t kTrailingWord0dOffset = 0x0d;
 static constexpr size_t kTrailingByte0fOffset = 0x0f;
 static constexpr size_t kTrailingWord10Offset = 0x10;
 static constexpr size_t kFixedByteCount = 0x12;
};

// anchor: launcher.exe vtable 0x004b6ca4 / opcode 0x07 parse-or-build accessor
// VTable layout matches the shared Packet_0x4af2a4 surface except for the slot-3 init helper:
// - slot 0 (+0x00): inherited destructor
// - slot 1 (+0x04): inherited StubReturn0
// - slot 4 (+0x10): inherited GetPayloadBase
class Packet_AsGetPublicKeyReply_0x4b6ca4 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
 // anchor: launcher.exe:0x443910
 // `resolveVariableTailViews=false` mirrors the local build path that grows the retained
 // message-ref by 0x12 bytes and stamps opcode 0x07 plus zeroed trailing fields.
 // `resolveVariableTailViews=true` mirrors the incoming-view path that decodes the effective
 // payload start from either direct or headerless storage layout and then resolves the
 // two inherited length-prefixed tail views rooted at payload+0x0d and payload+0x10.
 void InitFromIncomingMessage(
     ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* sourceMessageRef,
     bool resolveVariableTailViews) {
  if (messageRef08 && messageRef08 != sourceMessageRef) {
    messageRef08->Release();
  }

  messageRef08 = sourceMessageRef;
  if (messageRef08) {
    messageRef08->AddRef();
  }

  if (!messageRef08 || !messageRef08->messageStorage0c) {
    payloadPtr04 = 0u;
    payloadAlias10 = nullptr;
    createRefParam0c = resolveVariableTailViews ? 1u : 0u;
    return;
  }

  uint8_t* payloadBase = messageRef08->messageStorage0c->PayloadBase();
  if (!messageRef08->headerless10) {
    payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase);
  } else {
    const uint8_t descriptor = *(payloadBase + 1);
    payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase) +
        g_MessageOffsetLookupTable[(descriptor >> 4) & 7] +
        g_MessageOffsetLookupTable[descriptor & 7] +
        0x12u;
    messageRef08->headerless10 = 1u;
  }

  createRefParam0c = resolveVariableTailViews ? 1u : 0u;
  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

  if (!resolveVariableTailViews) {
    messageRef08->GrowPayloadByteCount(State6Packet0x07FixedPayload::kFixedByteCount);
    uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
    if (payload) {
      payload[0] = State6Packet0x07FixedPayload::kPayloadTag07;
      *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kGoHereAddrOffset) = 0u;
      *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kGoHerePortOffset) = 0u;
      *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kSessionSecretOffset) = 0u;
      *reinterpret_cast<uint16_t*>(payload + State6Packet0x07FixedPayload::kTrailingWord0dOffset) = 0u;
      *(payload + State6Packet0x07FixedPayload::kTrailingByte0fOffset) = 0u;
      *reinterpret_cast<uint16_t*>(payload + State6Packet0x07FixedPayload::kTrailingWord10Offset) = 0u;
    }
    debugString14 = nullptr;
    payloadSize18 = 0u;
    characterIdLow1c = 0u;
    characterIdHigh20 = 0u;
    return;
  }

  ResolveVariableTailViews();
 }

 // anchor: launcher.exe:0x443410
 // Reads the two optional length-prefixed tail fields rooted at payload+0x0d and payload+0x10
 // and stores their content pointers/lengths into the inherited +0x14/+0x18 and +0x1c/+0x20
 // slots respectively.
 void ResolveVariableTailViews() {
  const uint8_t* payload = static_cast<const uint8_t*>(payloadAlias10);
  if (!payload) {
    debugString14 = nullptr;
    payloadSize18 = 0u;
    characterIdLow1c = 0u;
    characterIdHigh20 = 0u;
    return;
  }

  const uint16_t firstFieldOffset = *reinterpret_cast<const uint16_t*>(payload + 0x0du);
  if (firstFieldOffset != 0u) {
    payloadSize18 = *reinterpret_cast<const uint16_t*>(payload + firstFieldOffset);
    debugString14 = reinterpret_cast<const char*>(payload + firstFieldOffset + 2u);
  } else {
    payloadSize18 = 0u;
    debugString14 = nullptr;
  }

  const uint16_t secondFieldOffset = *reinterpret_cast<const uint16_t*>(payload + 0x10u);
  if (secondFieldOffset != 0u) {
    characterIdHigh20 = *reinterpret_cast<const uint16_t*>(payload + secondFieldOffset);
    characterIdLow1c = reinterpret_cast<uint32_t>(payload + secondFieldOffset + 2u);
  } else {
    characterIdHigh20 = 0u;
    characterIdLow1c = 0u;
  }
 }

 // anchor: launcher.exe:0x4439f0 / vtable slot 3
 // Recomputes the effective headerless builder payload start from payloadBytes0c[1], resets the
 // retained message-ref position via the 0x41bb60 helper, grows by that offset and then stamps a
 // fresh 0x12-byte opcode 0x07 body with zeroed fixed fields. This is the builder reset path,
 // not the incoming parse-view path.
 void InitializePayloadSize() override {
  if (!messageRef08 || !messageRef08->messageStorage0c) {
    payloadPtr04 = 0u;
    payloadAlias10 = nullptr;
    return;
  }

  uint8_t* payloadBase = messageRef08->messageStorage0c->PayloadBase();
  const uint8_t descriptor = *(payloadBase + 1);
  const uint32_t payloadOffset =
      g_MessageOffsetLookupTable[(descriptor >> 4) & 7] +
      g_MessageOffsetLookupTable[descriptor & 7] +
      State6Packet0x07FixedPayload::kFixedByteCount;
  payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase) + payloadOffset;

  uint32_t localConsumedByteCount = 0u;
  (void)localConsumedByteCount;
  messageRef08->GrowPayloadByteCount(static_cast<uint16_t>(payloadOffset));

  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);
  messageRef08->GrowPayloadByteCount(State6Packet0x07FixedPayload::kFixedByteCount);

  uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
  if (payload) {
    payload[0] = State6Packet0x07FixedPayload::kPayloadTag07;
    *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kGoHereAddrOffset) = 0u;
    *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kGoHerePortOffset) = 0u;
    *reinterpret_cast<uint32_t*>(payload + State6Packet0x07FixedPayload::kSessionSecretOffset) = 0u;
    *reinterpret_cast<uint16_t*>(payload + State6Packet0x07FixedPayload::kTrailingWord0dOffset) = 0u;
    *(payload + State6Packet0x07FixedPayload::kTrailingByte0fOffset) = 0u;
    *reinterpret_cast<uint16_t*>(payload + State6Packet0x07FixedPayload::kTrailingWord10Offset) = 0u;
  }

  debugString14 = nullptr;
  payloadSize18 = 0u;
  characterIdLow1c = 0u;
  characterIdHigh20 = 0u;
 }
};

static_assert(sizeof(Packet_AsGetPublicKeyReply_0x4b6ca4) == sizeof(mxo::liblttcp::Packet_0x4af2a4), "Packet_MsConnectChallenge_0x4b6ca4 size mismatch");

// =============================================================================
// Packet_AsAuthChallenge_0x4b6ce0 - Parse accessor for AS_AuthChallenge (opcode 0x09)
// =============================================================================
// anchor: launcher.exe vtable 0x004b6ce0 (5 slots, 20 bytes)
// anchor: launcher.exe:0x443e30 = ResetAndInitialize (writes opcode 0x09)
// anchor: launcher.exe:0x443d90 = AuthBootstrap680AuthChallengeParseObject_InitFromIncomingMessage
//
// VTable layout at 0x4b6ce0 (5 slots):
// - Slot 0 (+0x00): 0x443aa0 - destructor (inherited from Packet_0x4af2a4)
// - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited, returns 0)
// - Slot 2 (+0x08): 0x4452a0 - DebugString
// - Slot 3 (+0x0c): 0x443e30 - ResetAndInitialize
// - Slot 4 (+0x10): 0x481760 - GetPayloadBase (inherited)
//
// Object layout: compact parse object, size = 0x14 bytes (20 bytes)
// This is a PARSE accessor class for reading incoming AS_AuthChallenge (opcode 0x09)
// Used in auth bootstrap flow at 0x448140 (HandleInboundAuthMessage)
//
// Usage pattern from 0x443d90:
// 1. Stack-allocate Packet_AsAuthChallenge_0x4b6ce0
// 2. Call InitFromIncomingMessage with source message ref
// 3. Read payload fields at fixed offsets
// =============================================================================

// anchor: launcher.exe vtable 0x004b6ce0 / parse accessor for opcode 0x09
// This is a minimal parse object, NOT a full packet builder
class Packet_AsAuthChallenge_0x4b6ce0 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
 // anchor: launcher.exe:0x443d90
 // Incoming parse-view path: retain the source message-ref, decode the effective payload start
 // from direct or headerless storage, and expose the body at +0x10.
 void InitFromIncomingMessage(
     ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* sourceMessageRef) {
  if (messageRef08 && messageRef08 != sourceMessageRef) {
   messageRef08->Release();
  }

  messageRef08 = sourceMessageRef;
  if (messageRef08) {
   messageRef08->AddRef();
  }

  if (!messageRef08 || !messageRef08->messageStorage0c) {
   payloadPtr04 = 0u;
   payloadAlias10 = nullptr;
   return;
  }

  uint8_t* payloadBase = messageRef08->messageStorage0c->PayloadBase();
  if (!messageRef08->headerless10) {
   payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase);
  } else {
   const uint8_t descriptor = *(payloadBase + 1);
   payloadPtr04 = reinterpret_cast<uint32_t>(payloadBase) +
       g_MessageOffsetLookupTable[(descriptor >> 4) & 7] +
       g_MessageOffsetLookupTable[descriptor & 7] +
       0x12u;
   messageRef08->headerless10 = 1u;
  }

  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);
  debugString14 = nullptr;
  payloadSize18 = 0u;
 }

 // anchor: launcher.exe:0x443e30 = Packet_AsAuthChallenge_0x4b6ce0::ResetAndInitialize
 // Builder-style reset path stamps a fresh 0x11-byte opcode 0x09 payload body.
 void ResetAndInitialize() {
  createRefParam0c = 0u;
  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);
  if (!messageRef08) {
   return;
  }
  messageRef08->GrowPayloadByteCount(0x11u);
  auto* payload = static_cast<uint8_t*>(payloadAlias10);
  if (payload) {
   payload[0] = 0x09u;
  }
  debugString14 = nullptr;
  payloadSize18 = 0u;
 }

 // Override virtual methods to match 5-slot vtable
 ~Packet_AsAuthChallenge_0x4b6ce0() override = default;
 uint32_t StubReturn0() override { return 0u; }
 void DebugString(int /*formatType*/ = 2) override {}
 void InitializePayloadSize() override {}
 void* GetPayloadBase() override { return payloadAlias10; }
};

// Packet_AsAuthChallenge_0x4b6ce0 layout:
// Minimal parse object inherits from Packet_0x4af2a4
// Total: same as base class (0x28 bytes) - no additional fields
static_assert(sizeof(Packet_AsAuthChallenge_0x4b6ce0) == sizeof(mxo::liblttcp::Packet_0x4af2a4), "Packet_AsAuthChallenge_0x4b6ce0 size mismatch");

// =============================================================================
// Packet_AsAuthChallengeResponse_0x4b6d08 - Minimal packet builder (opcode 0x0a)
// =============================================================================
// anchor: launcher.exe vtable 0x004b6d08 (5 slots, 20 bytes)
// anchor: launcher.exe:0x444240 = ctor/init helper
//
// This is a compact `Packet_0x4af2a4` subclass that reserves only 3 payload bytes and stamps:
// - byte  [payload+0x00] = 0x0a
// - word  [payload+0x01] = 0x0000
//
// Unlike the larger `0x4b6cf4` sibling, this class does not append extra reservation scaffolds.
class Packet_AsAuthChallengeResponse_0x4b6d08 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
 // anchor: launcher.exe:0x444240 / ctor body after Packet_0x4af2a4 default ctor
 // VTable layout matches the shared Packet_0x4af2a4 surface except for the slot-3 init helper:
 // - slot 0 (+0x00): inherited destructor
 // - slot 1 (+0x04): inherited StubReturn0
 // - slot 4 (+0x10): inherited GetPayloadBase
 void InitializePayloadSize() override {
  createRefParam0c = 0u;
  payloadAlias10 = reinterpret_cast<void*>(payloadPtr04);

  if (!messageRef08) {
    return;
  }

  messageRef08->GrowPayloadByteCount(3u);
  uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
  if (payload) {
    payload[0] = 0x0au;
    *reinterpret_cast<uint16_t*>(payload + 1) = 0u;
  }

  debugString14 = nullptr;
  payloadSize18 = 0u;
 }
};

static_assert(sizeof(Packet_AsAuthChallengeResponse_0x4b6d08) == sizeof(mxo::liblttcp::Packet_0x4af2a4), "Packet_MsClaimCharacterNameRequest_0x4b6d08 size mismatch");

// =============================================================================
// Packet_AsAuthChallengeResponse_0x4b6cf4 - Multi-field packet builder (opcode 0x0a)
// =============================================================================
// anchor: launcher.exe vtable 0x004b6cf4 (5 slots, 50 bytes)
// anchor: launcher.exe:0x443ea0 = ctor (clustered with 0x443f00 virt_meth)
// anchor: launcher.exe:0x443f00 = ResetAndInitialize (writes opcode 0x00 initially)
// anchor: launcher.exe:0x448389 = Used in AuthChallengeResponse building
//
// VTable layout at 0x4b6cf4 (5 slots):
// - Slot 0 (+0x00): 0x443aa0 - destructor (inherited)
// - Slot 1 (+0x04): 0x437b50 - StubReturn0 (inherited)
// - Slot 2 (+0x08): 0x4425f0 - DebugString
// - Slot 3 (+0x0c): 0x443f00 - ResetAndInitialize
// - Slot 4 (+0x10): 0x481760 - GetPayloadBase (inherited)
//
// Object layout: inherits Packet_0x4af2a4 at +0x00, additional fields at +0x28
// Full builder size: 0x32 bytes (50 bytes)
//
// This is a BUILDER class for sending packets with multiple length-prefixed fields
// Used in 0x448140..0x448467 for building AS_AuthChallengeResponse with:
// - Encrypted challenge (length-prefixed)
// - Password (length-prefixed)
// - SOE password (optional, length-prefixed)
// =============================================================================

// anchor: launcher.exe vtable 0x004b6cf4 / multi-field builder
// Margin opcode 0x0a = MS_ClaimCharacterNameRequest (but used generically for auth responses)
class Packet_AsAuthChallengeResponse_0x4b6cf4 : public mxo::liblttcp::Packet_0x4af2a4 {
public:
 // anchor: launcher.exe:0x443ea0 = Packet_AsAuthChallengeResponse_0x4b6cf4::ctor
 // anchor: launcher.exe:0x443f00 = Packet_AsAuthChallengeResponse_0x4b6cf4::ResetAndInitialize
 // Original implementation pattern:
 // 1. Calls Packet_0x4af2a4 default ctor
 // 2. Sets vtable to 0x4b6cf4
 // 3. Reserves 0x17 bytes for header
 // 4. Initializes payload with opcode 0x00 (opcode set later)
 // 5. Clears all length-prefixed field offsets
 void ResetAndInitialize() {
 // Base initialization
 if (!messageRef08) {
   messageRef08 = new ::mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c();
   if (messageRef08) {
     messageRef08->AddRef();
     messageRef08->ResetForPacketBuilder(false, 0);
   }
 }

 if (messageRef08 && messageRef08->messageStorage0c) {
   messageRef08->messageStorage0c->ResetPayloadByteCount(0x17);
   payloadAlias10 = messageRef08->messageStorage0c->PayloadBase();
   payloadPtr04 = reinterpret_cast<uint32_t>(payloadAlias10);
 }

 // Initialize header with opcode 0x00 (actual opcode set later)
 uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
 if (payload) {
   payload[0] = 0x00;  // Placeholder, actual opcode written later
   *reinterpret_cast<uint16_t*>(payload + 0x11) = 0u;  // First length-prefixed field pos
 }

 // Clear recovered field mirrors
 debugString14 = nullptr;
 payloadSize18 = 0u;
 characterIdLow1c = 0u;
 characterIdHigh20 = 0u;
 worldId24 = 0u;
 mbr_0x28 = 0u;
 }

 // anchor: launcher.exe:0x444040 = meth_0x444040 - AppendEncryptedChallenge
 // Appends length-prefixed string to +0x14 field (encryptedChallenge)
 // Logic: if (param_1 && payloadLength14 == 0) { reserve len+1; strncpy(field+0x14, param_1, len); }
 void AppendEncryptedChallenge(const char* str) {
   if (!str) return;
   // Already has content
   if (payloadSize18 != 0) return;
   size_t len = strlen(str);
   // Reserve space for string + null terminator
   uint16_t reserved = ReserveLengthPrefixedTail(static_cast<uint16_t>(len + 1));
   if (reserved != 0 && debugString14) {
     char* dest = const_cast<char*>(debugString14);
     strncpy(dest, str, reserved - 1);
     dest[reserved - 1] = '\0';
   }
 }

 // anchor: launcher.exe:0x444140 = meth_0x444140 - AppendPassword
 // Appends length-prefixed string to +0x1c/+0x20 field (password)
 // Logic: if (param_1 && characterIdHigh20 == 0) { reserve len+1; strncpy(characterIdLow1c, param_1, len); }
 void AppendPassword(const char* str) {
   if (!str) return;
   // Already has content
   if (characterIdHigh20 != 0) return;
   size_t len = strlen(str);
   // Reserve space for string + null terminator
   uint16_t reserved = ReserveLengthPrefixedTailForPassword(static_cast<uint16_t>(len + 1));
   if (reserved != 0 && characterIdLow1c != 0u) {
     char* dest = reinterpret_cast<char*>(static_cast<uintptr_t>(characterIdLow1c));
     strncpy(dest, str, reserved - 1);
     dest[reserved - 1] = '\0';
   }
 }

 // anchor: launcher.exe:0x4441a0 = meth_0x4441a0 - ReserveFieldLength
 // Reserves length-prefixed field of specified size
 // Returns actual bytes reserved (may be clamped to available space)
 uint16_t ReserveFieldLength(uint16_t byteCount) {
   return ReserveLengthPrefixedTail(byteCount);
 }

 // anchor: launcher.exe:0x443660 = meth_0x443660 - SetPadding
 // Sets the padding value at offset +0x28
 // Logic: mbr_0x28 = param_1; update word at (payloadBegin10 + 0x15)
 void SetPadding(uint16_t paddingBytes) {
   mbr_0x28 = paddingBytes;
   uint8_t* payload = static_cast<uint8_t*>(payloadAlias10);
   uint8_t* payloadBase = static_cast<uint8_t*>(GetPayloadBase());
   if (!payload || !payloadBase || !messageRef08 || !messageRef08->messageStorage0c) {
     return;
   }

   const uint16_t fieldOffset = *reinterpret_cast<uint16_t*>(payload + 0x15);
   uint16_t* fieldLength = reinterpret_cast<uint16_t*>(payloadBase + fieldOffset);
   const uint16_t previousPaddingBytes = *fieldLength;
   *fieldLength = paddingBytes;
   if (paddingBytes > previousPaddingBytes) {
     messageRef08->messageStorage0c->GrowPayloadByteCount(paddingBytes - previousPaddingBytes);
   }
 }

private:
 uint16_t ReserveLengthPrefixedTailForPassword(uint16_t contentByteCount) {
   const uint16_t actualCount = ReserveLengthPrefixedTail(contentByteCount);
   characterIdLow1c = reinterpret_cast<uint32_t>(const_cast<char*>(debugString14));
   characterIdHigh20 = actualCount;
   return actualCount;
 }

public:
 uint16_t mbr_0x28 = 0u;

 // Override virtual methods to match 5-slot vtable
 ~Packet_AsAuthChallengeResponse_0x4b6cf4() override = default;
 uint32_t StubReturn0() override { return 0u; }
 void DebugString(int /*formatType*/ = 2) override {}
 void InitializePayloadSize() override {}
 void* GetPayloadBase() override { return payloadAlias10; }
};

// Note: keep the `_14_/_1c_/_24_` suffixes as the source-of-truth offset cues here.
// `offsetof` on this virtual packet builder triggers noisy non-standard-layout warnings.

} // namespace mxo::ltlogin
