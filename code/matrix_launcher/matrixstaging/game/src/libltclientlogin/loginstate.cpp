#include "loginstate.h"

#include "loginmediator.h"
#include "../../../../src/diagnostics.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace mxo::ltlogin {
namespace {

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
    RecoveredPacketBuilderEnvelope() {
        // Current best source-owned mirror of the local helper object initialized by `0x439840`:
        // - acquires/installs a shared payload object
        // - stores the active payload write base as `shared + 0x0c`
        // - original local envelope layout is now a little tighter from disassembly:
        //   - `this+0x08` = retained shared packet/message object
        //   - `this+0x04` = base payload pointer (`shared + 0x0c`)
        //   - state-specific builders then cache string-reservation pointers/lengths in later
        //     helper-local fields before `0x41af70` forwards the whole envelope object
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

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static void AppendOwnedSectionBytesU16(void*& buffer, uint16_t& length, const uint8_t* src, uint16_t appendLen) {
    if (!src || appendLen == 0u) {
        return;
    }

    const size_t oldLength = length;
    const size_t newLength = oldLength + appendLen;
    void* newBuffer = buffer ? std::realloc(buffer, newLength) : std::malloc(newLength);
    if (!newBuffer) {
        return;
    }

    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, src, appendLen);
    buffer = newBuffer;
    length = static_cast<uint16_t>(newLength & 0xffffu);
}

static void AppendOwnedSectionBytesU32(void*& buffer, uint32_t& length, const uint8_t* src, uint16_t appendLen) {
    if (!src || appendLen == 0u) {
        return;
    }

    const size_t oldLength = length;
    const size_t newLength = oldLength + appendLen;
    void* newBuffer = buffer ? std::realloc(buffer, newLength) : std::malloc(newLength);
    if (!newBuffer) {
        return;
    }

    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, src, appendLen);
    buffer = newBuffer;
    length = static_cast<uint32_t>(newLength);
}

static void ResetOwnedSectionBytes(void*& buffer, uint16_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void ResetOwnedSectionBytes(void*& buffer, uint32_t& length, uint8_t& flag) {
    if (buffer) {
        std::free(buffer);
        buffer = nullptr;
    }
    length = 0u;
    flag = 0u;
}

static void CopyBoundedRawBytes(uint8_t* dest, size_t destSize, const uint8_t* src, size_t srcSize) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::memset(dest, 0, destSize);
    if (!src || srcSize == 0u) {
        return;
    }

    std::memcpy(dest, src, std::min(destSize, srcSize));
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

using PostAuthMarginLoadingState = CLTLoginMediator::PostAuthMarginLoadingState;
using SlotRecordState = CLTLoginMediator::SlotRecordState004b5328;

static void ResetState8ReplyOwnerState(PostAuthMarginLoadingState& ownerState) {
    std::fill(std::begin(ownerState.characterNameBufferF1c), std::end(ownerState.characterNameBufferF1c), '\0');
    ownerState.characterReplyFieldF3c = 0u;
    ownerState.characterReplyFieldF40 = 0u;
    std::fill(ownerState.characterFlagsF48.begin(), ownerState.characterFlagsF48.end(), 0u);
    std::fill(ownerState.secondaryCharacterDataF68.begin(), ownerState.secondaryCharacterDataF68.end(), 0u);
    std::fill(ownerState.characterRecordPointersF88.begin(), ownerState.characterRecordPointersF88.end(), 0u);
    std::fill(ownerState.section0StringF8c.begin(), ownerState.section0StringF8c.end(), '\0');
    std::fill(ownerState.section0StringFac.begin(), ownerState.section0StringFac.end(), '\0');
    std::fill(ownerState.section0StringFcc.begin(), ownerState.section0StringFcc.end(), '\0');
    std::fill(ownerState.state8Section0RawF88.begin(), ownerState.state8Section0RawF88.end(), 0u);
    ownerState.replySectionData13cc = 0u;
    ownerState.replySectionData13d0 = 0u;
    ownerState.section0Flag13f6 = 0u;
    ResetOwnedSectionBytes(
        ownerState.state8Section0OverflowBuffer13f0,
        ownerState.state8Section0OverflowLength13f4,
        ownerState.section0Flag13f6);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer13f8, ownerState.allocatedBufferLength13fc, ownerState.flag13fe);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1400, ownerState.allocatedBufferLength1404, ownerState.flag1406);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1408, ownerState.allocatedBufferLength140c, ownerState.allocatedBufferFlag140e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1410, ownerState.allocatedBufferLength1414, ownerState.flag1416);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1418, ownerState.allocatedBufferLength141c, ownerState.allocatedBufferFlag141e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1420, ownerState.allocatedBufferLength1424, ownerState.allocatedBufferFlag1426);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1428, ownerState.allocatedBufferLength142c, ownerState.allocatedBufferFlag142e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1430, ownerState.allocatedBufferLength1434, ownerState.flag1436);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1438, ownerState.allocatedBufferLength143c, ownerState.flag143e);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1440, ownerState.allocatedBufferLength1444, ownerState.flag1448);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer144c, ownerState.allocatedBufferLength1450, ownerState.flag1452);
    ResetOwnedSectionBytes(ownerState.allocatedBuffer1454, ownerState.allocatedBufferLength1458, ownerState.flag145a);
    ownerState.state8Section10ChunkBitmap = 0u;
    ownerState.state8Section11Dword145c = 0u;
    ownerState.state8Section11String1460.clear();
}

static bool SeedState8FirstFragment(
    PostAuthMarginLoadingState& ownerState,
    CLTLoginMediator* mediator,
    uint32_t parsedField05) {
    ResetState8ReplyOwnerState(ownerState);
    ownerState.characterReplyFieldF3c = parsedField05;

    const SlotRecordState* currentSlotRecord = mediator ? mediator->GetCurrentSlotRecord() : nullptr;
    if (currentSlotRecord != nullptr) {
        if (!currentSlotRecord->heapString14.empty()) {
            const size_t copyCount = std::min(
                currentSlotRecord->heapString14.size(),
                sizeof(ownerState.characterNameBufferF1c) - 1u);
            std::copy_n(
                currentSlotRecord->heapString14.data(),
                copyCount,
                ownerState.characterNameBufferF1c);
            ownerState.characterNameBufferF1c[copyCount] = '\0';
        }
        ownerState.characterReplyFieldF40 = currentSlotRecord->worldId0c;
        ownerState.secondaryCharacterDataF68[0] = currentSlotRecord->worldId0c;
        ownerState.secondaryCharacterDataF68[1] = currentSlotRecord->status0b;
        return true;
    }

    std::copy(
        ownerState.sourceLeadString108.begin(),
        ownerState.sourceLeadString108.end(),
        ownerState.characterNameBufferF1c);
    ownerState.characterNameBufferF1c[sizeof(ownerState.characterNameBufferF1c) - 1u] = '\0';
    ownerState.characterReplyFieldF40 = ownerState.sourceField12c;
    return false;
}

static void ApplyState8Section11SideEffect(
    PostAuthMarginLoadingState& ownerState,
    const ParsedState11LoadCharacterReplyScaffold& parsed) {
    // anchor: launcher.exe:0x43f8c0
    // Current best read from disassembly:
    // - if section byteCount > 4, copy the leading dword into owner `+0x145c`
    // - then copy the remaining bytes into the small-string-like family at owner `+0x1460`
    // - otherwise clear both fields
    ownerState.state8Section11Dword145c = 0u;
    ownerState.state8Section11String1460.clear();

    if (!parsed.sectionData || parsed.sectionByteCount <= 4u) {
        return;
    }

    ownerState.state8Section11Dword145c = ReadU32LE(parsed.sectionData);
    ownerState.state8Section11String1460.assign(
        reinterpret_cast<const char*>(parsed.sectionData + 4u),
        reinterpret_cast<const char*>(parsed.sectionData + parsed.sectionByteCount));
}

static void HandleState8ReplySection(
    PostAuthMarginLoadingState& ownerState,
    const ParsedState11LoadCharacterReplyScaffold& parsed) {
    switch (parsed.sectionSelectorMinus2) {
        case 0u:
            if (!parsed.sectionData) {
                break;
            }
            {
                const size_t fixedPrefixBytes = std::min<size_t>(
                    parsed.sectionByteCount,
                    ownerState.characterFlagsF48.size() * sizeof(uint32_t));
                if (fixedPrefixBytes != 0u) {
                    std::memcpy(ownerState.characterFlagsF48.data(), parsed.sectionData, fixedPrefixBytes);
                }
                if (parsed.sectionByteCount > 0x20u) {
                    CopyBoundedRawBytes(
                        ownerState.state8Section0RawF88.data(),
                        ownerState.state8Section0RawF88.size(),
                        parsed.sectionData + 0x20u,
                        parsed.sectionByteCount - 0x20u);
                }
                if (parsed.sectionByteCount > 0x485u && ownerState.state8Section0OverflowBuffer13f0 == nullptr) {
                    AppendOwnedSectionBytesU16(
                        ownerState.state8Section0OverflowBuffer13f0,
                        ownerState.state8Section0OverflowLength13f4,
                        parsed.sectionData + 0x485u,
                        static_cast<uint16_t>(parsed.sectionByteCount - 0x485u));
                }
                ownerState.section0Flag13f6 = 1u;
            }
            break;
        case 1u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer13f8, ownerState.allocatedBufferLength13fc, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag13fe = 1u;
            break;
        case 2u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1400, ownerState.allocatedBufferLength1404, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag1406 = 1u;
            break;
        case 3u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1418, ownerState.allocatedBufferLength141c, parsed.sectionData, parsed.sectionByteCount);
            ownerState.allocatedBufferFlag141e = 1u;
            break;
        case 4u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1420, ownerState.allocatedBufferLength1424, parsed.sectionData, parsed.sectionByteCount);
            ownerState.allocatedBufferFlag1426 = 1u;
            break;
        case 5u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1428, ownerState.allocatedBufferLength142c, parsed.sectionData, parsed.sectionByteCount);
            ownerState.allocatedBufferFlag142e = 1u;
            break;
        case 6u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1408, ownerState.allocatedBufferLength140c, parsed.sectionData, parsed.sectionByteCount);
            ownerState.allocatedBufferFlag140e = 1u;
            break;
        case 7u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1410, ownerState.allocatedBufferLength1414, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag1416 = 1u;
            break;
        case 8u:
            AppendOwnedSectionBytesU32(ownerState.allocatedBuffer1440, ownerState.allocatedBufferLength1444, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag1448 = 1u;
            break;
        case 9u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer144c, ownerState.allocatedBufferLength1450, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag1452 = 1u;
            break;
        case 10u:
            if (ownerState.allocatedBuffer1454 == nullptr) {
                ownerState.allocatedBuffer1454 = std::malloc(0x7d00u);
                ownerState.allocatedBufferLength1458 = 0u;
                ownerState.state8Section10ChunkBitmap = 0u;
            }
            if (ownerState.allocatedBuffer1454 != nullptr && parsed.sectionData && parsed.expectedSectionCount0b != 0u) {
                const size_t chunkIndex = static_cast<size_t>(parsed.expectedSectionCount0b - 1u);
                const size_t chunkOffset = chunkIndex * 1000u;
                if (chunkOffset + parsed.sectionByteCount <= 0x7d00u) {
                    std::memcpy(
                        static_cast<uint8_t*>(ownerState.allocatedBuffer1454) + chunkOffset,
                        parsed.sectionData,
                        parsed.sectionByteCount);
                    if (chunkIndex < 32u) {
                        ownerState.state8Section10ChunkBitmap |= (1u << chunkIndex);
                    }
                    ownerState.allocatedBufferLength1458 = static_cast<uint16_t>(
                        ownerState.allocatedBufferLength1458 + parsed.sectionByteCount);
                    ownerState.flag145a = 1u;
                }
            }
            break;
        case 11u:
            ApplyState8Section11SideEffect(ownerState, parsed);
            spdlog::info(
                "CLTLoginState_State8::Slot6_HandleSecondaryMessage applied section 0x0b side effect dword145c=0x{:08x} string1460Len={}",
                static_cast<unsigned>(ownerState.state8Section11Dword145c),
                static_cast<unsigned>(ownerState.state8Section11String1460.size()));
            break;
        case 12u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1430, ownerState.allocatedBufferLength1434, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag1436 = 1u;
            break;
        case 13u:
            AppendOwnedSectionBytesU16(ownerState.allocatedBuffer1438, ownerState.allocatedBufferLength143c, parsed.sectionData, parsed.sectionByteCount);
            ownerState.flag143e = 1u;
            break;
        default:
            break;
    }
}

static void FinalizeState8ChunkedSection10Buffer(PostAuthMarginLoadingState& ownerState) {
    if (ownerState.allocatedBuffer1454 == nullptr) {
        return;
    }

    size_t firstChunkIndex = 0u;
    while (firstChunkIndex < 32u && ((ownerState.state8Section10ChunkBitmap >> firstChunkIndex) & 1u) == 0u) {
        ++firstChunkIndex;
    }
    if (firstChunkIndex < 32u) {
        std::memmove(
            ownerState.allocatedBuffer1454,
            static_cast<uint8_t*>(ownerState.allocatedBuffer1454) + (firstChunkIndex * 1000u),
            ownerState.allocatedBufferLength1458);
    }
    if (void* compacted = std::realloc(ownerState.allocatedBuffer1454, ownerState.allocatedBufferLength1458)) {
        ownerState.allocatedBuffer1454 = compacted;
    }
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
    // Exact recovered shape from `0x004397e0`:
    // - when object byte `this+4 == 1`, delegate to owner helper `0x41c5c0`
    // - if that helper returns `< 1`, return success-ish immediately
    // - otherwise write owner `+0x80 = 0x12000005` and fail
    // Live-path caution tightened again from the latest breakpoint-only original run:
    // - after the proven state9 success tail (`0x41b450(0x0c) -> 0x41cfb0(0x18)`), the natural run
    //   later re-hit `0x41cfb0` with event `0x0f` and entered game
    // - it still did not hit `0x004397e0` or `0x41c5c0` on that continuation
    // - so keep this as a later probe path, not as the already-proven immediate post-state9 flow
    if (slot6DispatchByte4_ == 1u && mediator != nullptr) {
        const uint32_t dispatchResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (dispatchResult < 1u) {
            spdlog::info(
                "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=1 delegated to owner callback84 workItem={} -> dispatchResult=0x{:08x}",
                fmt::ptr(workItem),
                static_cast<unsigned>(dispatchResult));
            return 1u;
        }
    }

    if (mediator != nullptr) {
        mediator->WorldListCountOrStatus80() = 0x12000005u;
    }
    spdlog::info(
        "CLTLoginState_AbstractFinalLeafBase::Slot6_HandleSecondaryMessage byte4=0x{:02x} set owner+0x80=0x12000005 workItem={}",
        static_cast<unsigned>(slot6DispatchByte4_),
        fmt::ptr(workItem));
    return 0u;
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
    // - current best read from decompilation + disassembly:
    //   - precheck owner `+0x1c` through `0x41b4b0`
    //     - on failure, original switches helper state to `4`
    //   - then gate on owner byte `+0xf14`
    //     - on zero, original switches helper state to `6`
    //   - fetch current slot record through owner vtable `+0x44`
    //   - initialize packet-builder family `0x43ac10`
    //   - write the current character id pair plus selection snapshot blocks
    //     `+0xcd0..+0xd7f` in the original write order
    //   - append `GameSessionID` through `0x43ada0`
    //   - send through `0x41af70`
    //     - newer `0x41af70` tightening matters for the current blocker:
    //       it does not serialize raw bytes itself
    //       it forwards the stack-local packet-envelope object into current margin connection
    //       vtable `+0x24` / `0x41cf30 = CMessageConnection_ForwardEnvelopeToSendPacket`
    //       and that wrapper then forwards the envelope's shared packet/message object into
    //       vtable `+0x28` / inherited `CMessageConnection::SendPacket` (`0x448cf0`)
    //   - post event `9`
    // Practical current boundary from the newest original-launcher runs:
    // - natural original reaches this sender, crosses the `0x41af70/0x41cf30` send bridge, and
    //   later does reach state8 slot 6 / `0x43f930`
    // - so the old pre-`0x43f930` survivability question is no longer the first missing natural
    //   boundary; the next target is deeper reply-side behavior inside slot 6 and the continuation
    //   after it
    // - the receive-side route for that live slot-6 hit is now tighter too:
    //   `CMarginConnection::OnOperationCompleted` (`0x44af60`) ->
    //   `CMessageConnection::OnOperationCompleted` (`0x4490c0`) ->
    //   `CBaseMarginConnection::DispatchMessage` (`0x442d00`) ->
    //   mediator re-entry `0x41f260` into the active helper/state slot-6 body
    // - newer `0x442d00/0x441bc0/0x441850` review now also rules out one tempting shortcut:
    //   the base type-4/MS wrapper path can synthesize a local type-`0x0b` completion object and
    //   fall into mediator fallback `0x41afc0`, which re-enters helper slot 2 instead of slot 6
    if (!mediator->State10HasReadyConnectionState2()) {
        if (CLTLoginState* fallbackState = mediator->ScaffoldState4()) {
            mediator->SwitchHelperStateScaffold(4u, fallbackState);
        }
        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4 currentState=%s",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        if (CLTLoginState* fallbackState = mediator->ScaffoldState6()) {
            mediator->SwitchHelperStateScaffold(6u, fallbackState);
        }
        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6 currentState=%s",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 0u;
    }

    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
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
    mediator->PostEventScaffold(0x09u);

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
    if (!mediator) {
        return 0u;
    }

    // Newer receive-side boundary tightening from `0x44af20/0x442d00` + live original WineDbg:
    // - the active state slot-6 body is now proven live on the natural password-submit path
    // - it is not the first recipient for every incoming margin message
    // - base margin dispatch fully consumes decoded message codes `2`, `4`, and `5`
    // - only other decoded message codes fall through owner `+0x184 -> 0x41f260` and land here
    // - practical consequence: the raw state8 reply opcode `0x10` belongs on that fallback path,
    //   not on the base code-4 wrapper branch
    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(mediator->StagedIncomingMarginPacketBytes());
    if (!parsed.valid) {
        const uint32_t fallbackResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (fallbackResult < 1u) {
            Log(
                "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage delegated non-0x10 fallback through owner callback84 -> dispatchResult=0x%08x",
                (unsigned)fallbackResult);
            return 1u;
        }
        mediator->WorldListCountOrStatus80() = 0x12000005u;
        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage non-0x10 fallback through owner callback84 returned 0x%08x; mirrored owner+0x80=0x12000005",
            (unsigned)fallbackResult);
        return 0u;
    }

    auto& ownerState = mediator->MutablePostAuthMarginLoadingState();
    ownerState.worldListCountOrStatus80 = parsed.status;
    if (parsed.status >= 1u) {
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
        ownerState.state8Section10ChunkBitmap = 0u;
        if (CLTLoginState* failureState = mediator->ScaffoldState3()) {
            mediator->SwitchHelperStateScaffold(3u, failureState);
        }
        mediator->PostErrorScaffold(10u);
        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage observed failure status=0x%08x; original would switch helper state to 3 and post error=10 currentState=%s",
            (unsigned)parsed.status,
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 1u;
    }

    const bool firstFragment = (replySectionsSeen_ == 0u);
    const bool usedCurrentSlotRecord = firstFragment
        ? SeedState8FirstFragment(ownerState, mediator, parsed.field05)
        : false;

    if (parsed.shouldSeedExpectedSectionCount) {
        replySectionsExpected_ = parsed.expectedSectionCount0b;
    }

    HandleState8ReplySection(ownerState, parsed);

    if (replySectionsSeen_ < 0xffu) {
        ++replySectionsSeen_;
    }

    const bool completed = (replySectionsExpected_ != 0u) && (replySectionsSeen_ >= replySectionsExpected_);
    if (completed) {
        FinalizeState8ChunkedSection10Buffer(ownerState);

        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x43f930 completion tail posts event 0x0b after switching to helper9.
        mediator->PostEventScaffold(0x0bu);

        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage completed state8 reply progression status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u firstFragment=%u usedCurrentSlotRecord=%u -> currentState=helper9 event=0x0b",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_,
            firstFragment ? 1u : 0u,
            usedCurrentSlotRecord ? 1u : 0u);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
        ownerState.state8Section10ChunkBitmap = 0u;
    } else {
        Log(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage routed state8 reply status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u seedCount=%u firstFragment=%u usedCurrentSlotRecord=%u",
            (unsigned)parsed.status,
            (unsigned)parsed.sectionSelectorMinus2,
            (unsigned)parsed.sectionByteCount,
            (unsigned)parsed.handoffWord09,
            (unsigned)replySectionsSeen_,
            (unsigned)replySectionsExpected_,
            parsed.shouldSeedExpectedSectionCount ? 1u : 0u,
            firstFragment ? 1u : 0u,
            usedCurrentSlotRecord ? 1u : 0u);
    }
    return 1u;
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
    if (!mediator) {
        return 0u;
    }

    // Current best read from `0x00439780` + `0x41de40`:
    // - this state consumes helper-local byte/word payload at `this+4/+6`
    // - forwards them into the owner helper
    // - clears the local payload regardless of branch
    // - posts event `0x17` when that helper returns `< 1`
    // - newer original-launcher WineDbg now proves the natural path continues not just into this
    //   slot, but immediately onward into `0x41de40`
    // - representative natural stop shape:
    //   - `EIP = 0x439780`
    //   - `ECX = this`
    //   - `EAX = helper-local side pointer / temp = 0x00f9bb10`
    //   - `EDX = 0x004b517c` (this vtable)
    //   - `this+4 = 0`
    //   - `this+6 = 0x2710`
    const uint8_t consumedByte4 = pendingByte4_;
    const uint16_t consumedWord6 = pendingWord6_;
    const uint32_t submitResult = mediator->State9SubmitFollowupScaffold(consumedByte4, consumedWord6);
    pendingByte4_ = 0;
    pendingWord6_ = 0;

    if (submitResult < 1u) {
        // anchor: launcher.exe:0x00439780 success-side event post after the `0x41de40` submit call.
        mediator->PostEventScaffold(0x17u);
        spdlog::info(
            "CLTLoginState_State9::Slot3_BeginOrContinue consumed helper-local payload byte4=0x{:02x} word6=0x{:04x} -> submitResult=0x{:08x} then posts event=0x17",
            static_cast<unsigned>(consumedByte4),
            static_cast<unsigned>(consumedWord6),
            static_cast<unsigned>(submitResult));
    } else {
        spdlog::info(
            "CLTLoginState_State9::Slot3_BeginOrContinue consumed helper-local payload byte4=0x{:02x} word6=0x{:04x} -> submitResult=0x{:08x}",
            static_cast<unsigned>(consumedByte4),
            static_cast<unsigned>(consumedWord6),
            static_cast<unsigned>(submitResult));
    }
    return 1u;
}

// anchor: launcher.exe:0x0043c180 (vtable 0x004b517c slot 6)
uint32_t CLTLoginState_State9::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    // Current live-status note:
    // - newer natural-original WineDbg now proves this slot-6 body is reached on the natural path
    // - representative natural stop hit the success-side branch at `0x43c1c2`
    // - observed state there matched the raw-`0x11` success interpretation:
    //   - owner `+0x80 = 0`
    //   - parsed status dword = 0
    // - representative natural run at this same boundary was visibly in the
    //   "Waiting for Regionserver" phase
    // - so the old “does natural original ever reach `0x43c180`?” question is now closed
    // - newer Ghidra-first tightening also matters for what comes next:
    //   success here is not just "set current state to 12 and immediately fall into slot 6"
    //   it first runs `0x41b420`, then `0x41b450(0x0c)`, then `0x41cfb0(0x18)`
    // - newer breakpoint-only live proof now tightens that one step further too:
    //   the same natural run later re-hit `0x41cfb0` with event `0x0f` before entering game
    // - `0x41cfb0` walks the owner `+0x674` listener tree, so observer/UI consumers are now part
    //   of the next faithful-original question too
    // - next natural-original question therefore moves later again, into the post-state9 /
    //   state-`0x0c` continuation after this slot posts event `0x18`
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& bytes = mediator->StagedIncomingMarginPacketBytes();
    if (bytes.size() < 5u || bytes[0] != 0x11u) {
        const uint32_t fallbackResult = mediator->DispatchSecondaryMessageToOwnerCallback84(workItem);
        if (fallbackResult < 1u) {
            spdlog::info(
                "CLTLoginState_State9::Slot6_HandleSecondaryMessage delegated non-0x11 fallback through owner callback84 -> dispatchResult=0x{:08x}",
                static_cast<unsigned>(fallbackResult));
            return 1u;
        }
        mediator->WorldListCountOrStatus80() = 0x12000005u;
        spdlog::info(
            "CLTLoginState_State9::Slot6_HandleSecondaryMessage non-0x11 fallback through owner callback84 returned 0x{:08x}; mirrored owner+0x80=0x12000005",
            static_cast<unsigned>(fallbackResult));
        return 0u;
    }

    const uint32_t parsedStatus = ReadU32LE(bytes.data() + 1u);
    mediator->WorldListCountOrStatus80() = parsedStatus;
    if (parsedStatus < 1u) {
        mediator->HandleState9Opcode11SuccessSideEffect();
        if (CLTLoginState* nextState = mediator->ScaffoldState12()) {
            mediator->SwitchHelperStateScaffold(0x0cu, nextState);
        }
        // anchor: launcher.exe:0x43c180 success tail posts event 0x18 after switching to state 0x0c.
        mediator->PostEventScaffold(0x18u);
        spdlog::info(
            "CLTLoginState_State9::Slot6_HandleSecondaryMessage observed raw-0x11 success status=0x{:08x}; original calls owner vtable +0x16c, switches helper state to 0x0c, then posts event=0x18 currentState={}",
            static_cast<unsigned>(parsedStatus),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 1u;
    }

    if (CLTLoginState* failureState = mediator->ScaffoldState3()) {
        mediator->SwitchHelperStateScaffold(3u, failureState);
    }
    // anchor: launcher.exe:0x43c180 failure tail posts error 0x0d after switching back to state 3.
    mediator->PostErrorScaffold(0x0du);
    spdlog::info(
        "CLTLoginState_State9::Slot6_HandleSecondaryMessage observed raw-0x11 failure status=0x{:08x}; original switches helper state to 3 and posts error=0x0d currentState={}",
        static_cast<unsigned>(parsedStatus),
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
    return 1u;
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
    // - sends raw margin opcode `0x0a`
    //   - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //     `MS_ClaimCharacterNameRequest`
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
    mediator->PostEventScaffold(0x13u);

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
        mediator->SwitchHelperStateScaffold(0x0bu, nextState);
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
    //   - sends raw margin opcode `0x0c`
    //     - `0x41bf70 = CLTLoginMediator_MarginOpcodeName` names that opcode
    //       `MS_CreateCharacterRequest`
    //   - treats `ESI = owner + 0x108`
    //   - creates a packet-builder object through `0x439840`
    //   - resets/initializes the raw `0x4d` payload through `0x43a470`
    //   - writes 17 dwords from owner `+0x134..+0x174`
    //   - appends `RealFirstName`, `RealLastName`, optional `Background`, and `GameSessionID`
    //     through `0x43a640 / 0x43a740 / 0x43a840 / 0x43a940`
    //   - newer `0x43e540` debug-printer review makes those 17 dwords concrete:
    //     SkinToneID, BodyID, HeadID, HairID, HairColorID, TattooID, FacialHairID,
    //     FacialHairColorID, StartingHat, StartingGlasses, StartingShirt, StartingGloves,
    //     StartingCoat, StartingPants, StartingTights, StartingShoes, TraitID
    //   - calls `0x41af70` to forward the completed packet-envelope object through the current
    //     margin connection send path (`0x448cf0`), not to serialize raw bytes itself
    //   - then posts event `0x15`
    // Active-path caution:
    // - this is a very real character-data sender, but the natural-original password-submit path is
    //   still not live-proven here; no natural hit yet on `0x41c3c0` or `0x43c020`
    // - current replacement-launcher runtime proof now lands here explicitly:
    //   `... -> helperState 0x0b -> event 0x15 -> State11::Slot3 send -> Loading Character`
    //   and then stalls before any incoming `MS_LoadCharacterReply`
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
    mediator->PostEventScaffold(0x15u);

    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue built fixed-0x4d margin payload payloadTag=0x{:02x} fixedBytes=0x{:02x} totalBytes=0x{:02x} SkinToneID=0x{:08x} RealFirstName='{}' RealLastName='{}' Background='{}' GameSessionID='{}' -> sendResult=0x{:08x} then posts event=0x15",
        static_cast<unsigned>(State11Packet0x4dFixedPayload::kPayloadTag0c),
        static_cast<unsigned>(State11Packet0x4dFixedPayload::kFixedByteCount),
        static_cast<unsigned>(packetBuilder.PayloadByteCount()),
        static_cast<unsigned>(sourceDwords134[0]),
        reinterpret_cast<const char*>(mediator->SourceBlock178().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock198().data()),
        reinterpret_cast<const char*>(mediator->SourceBlock1b8().data()),
        mediator->GetGameSessionId664() ? mediator->GetGameSessionId664() : "<empty>",
        static_cast<unsigned>(sendResult));
    spdlog::info(
        "CLTLoginState_State11::Slot3_BeginOrContinue awaiting first helper11 reply; slot6 requires a later raw-0x10 that survives the base margin code-2/4/5 filter currentState={} marginReceiveCount={} filteredBeforeSlot6={} slot6DispatchCount={}",
        mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>",
        static_cast<unsigned>(mediator->MarginPacketReceiveCountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketFilteredBeforeSlot6CountScaffold()),
        static_cast<unsigned>(mediator->MarginPacketSlot6DispatchCountScaffold()));
    return sendResult;
}

// anchor: launcher.exe:0x00440320 (vtable 0x004b5154 slot 6)
uint32_t CLTLoginState_State11::Slot6_HandleSecondaryMessage(void* workItem, CLTLoginMediator* mediator) {
    (void)workItem;
    if (!mediator) {
        return 0u;
    }

    const std::vector<uint8_t>& stagedBytes = mediator->StagedIncomingMarginPacketBytes();
    const uint16_t rawCode = stagedBytes.empty() ? 0u : stagedBytes[0];
    const ParsedState11LoadCharacterReplyScaffold parsed =
        ParseState11LoadCharacterReplyScaffold(stagedBytes);
    if (!parsed.valid) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage rejected staged margin bytes={} rawCode=0x{:02x}; helper11 slot6 only handles raw-0x10 after the base margin code-2/4/5 filter",
            static_cast<unsigned>(stagedBytes.size()),
            static_cast<unsigned>(rawCode));
        return 0u;
    }

    // Ownership correction mirrors slot 3 as well:
    // - `0x440320` belongs to `CLTLoginState_State11` slot 6
    // - the state object owns reply-progress counters and the helper11 -> helper9 handoff
    // - mediator keeps the narrower owner-buffer mutation helper because those writes target the
    //   mediator-owned `0x4f78b8` state area
    const uint32_t handled = mediator->HandleStagedMarginLoadCharacterReplyPacketScaffold();
    if (handled == 0u) {
        spdlog::info(
            "CLTLoginState_State11::Slot6_HandleSecondaryMessage entered raw-0x10 receive path but owner-side parse/gate failed status=0x{:08x} field05=0x{:08x} handoffWord=0x{:04x} currentState={}",
            static_cast<unsigned>(parsed.status),
            static_cast<unsigned>(parsed.field05),
            static_cast<unsigned>(parsed.handoffWord09),
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<null>");
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
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x440320 completion tail posts event 0x16 after switching to helper9.
        mediator->PostEventScaffold(0x16u);

        Log(
            "DIAGNOSTIC: CLTLoginState_State11::Slot6_HandleSecondaryMessage completed helper11 reply progression status=0x%08x section=%u bytes=%u handoffWord=0x%04x seen=%u expected=%u -> currentState=helper9 event=0x16",
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

CLTLoginState_State12::CLTLoginState_State12() {
    // anchor: launcher.exe:0x00439d80 + helper-dispatch init at `0x43b300`
    // The dispatch table creates this final leaf with byte `this+4 = 1`, which is what makes the
    // shared slot-6 handler (`0x004397e0`) delegate into `0x41c5c0` instead of immediately
    // writing `0x12000005`.
    // Current natural-original status note:
    // - `0x43c180` success now proves the launcher switches into state `0x0c`
    // - a representative live run at that boundary was visibly at "Waiting for Regionserver"
    // - newer `0x41b450` / `0x41cfb0` review now also shows why the old immediate-leaf theory is
    //   too strong: the success tail switches helper state and then posts event `0x18` through the
    //   owner listener tree before any later shared final-leaf slot-6 hit is proven
    // - follow-up late probes on `0x004397e0` / `0x0041c5c0` still did **not** hit naturally
    // - so keep this as the strongest current state-identity lead for the post-state9 continuation,
    //   but do not yet claim that the natural path immediately falls into the shared final-leaf
    //   slot-6 handler behind it
    slot6DispatchByte4_ = 1u;
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
