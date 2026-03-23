#include "loginstate.h"

#include "loginmediator.h"
#include "loginstate_loadcharacterreply_scaffold.h"
#include "loginstate_packet_builder_scaffold.h"
#include "../../../../src/diagnostics.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mxo::ltlogin {
namespace {

using PostAuthMarginLoadingState = CLTLoginMediator::PostAuthMarginLoadingState;
using SlotRecordState = SlotRecordState004b5328;

static std::string FormatU32x4Block(const std::array<uint32_t, 4>& block) {
    return fmt::format(
        "[{:#010x} {:#010x} {:#010x} {:#010x}]",
        block[0],
        block[1],
        block[2],
        block[3]);
}

static bool U32x4BlockHasAnyNonZero(const std::array<uint32_t, 4>& block) {
    return block[0] != 0u || block[1] != 0u || block[2] != 0u || block[3] != 0u;
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

static void CopyCStringIntoFixed(char* dest, size_t destSize, const uint8_t* src, size_t srcAvailable) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::fill(dest, dest + destSize, '\0');
    if (!src || srcAvailable == 0u) {
        return;
    }

    size_t copyLen = 0u;
    while (copyLen + 1u < destSize && copyLen < srcAvailable && src[copyLen] != '\0') {
        dest[copyLen] = static_cast<char>(src[copyLen]);
        ++copyLen;
    }
    dest[copyLen] = '\0';
}

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

static void LogState8PersistenceFamilySnapshot(
    const PostAuthMarginLoadingState& ownerState,
    const char* reason,
    uint32_t sectionSelectorMinus2,
    uint16_t sectionByteCount,
    bool completed);

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
                if (parsed.sectionByteCount >= 4u) {
                    ownerState.characterRecordPointersF88[0] = ReadU32LE(parsed.sectionData + 0x00u);
                }
                if (parsed.sectionByteCount > 0x444u) {
                    ownerState.replySectionData13cc = ReadU32LE(parsed.sectionData + 0x444u);
                }
                if (parsed.sectionByteCount > 0x448u) {
                    ownerState.replySectionData13d0 = ReadU32LE(parsed.sectionData + 0x448u);
                }
                CopyCStringIntoFixed(
                    ownerState.section0StringF8c.data(),
                    ownerState.section0StringF8c.size(),
                    parsed.sectionByteCount > 0x04u ? (parsed.sectionData + 0x04u) : nullptr,
                    parsed.sectionByteCount > 0x04u ? parsed.sectionByteCount - 0x04u : 0u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFac.data(),
                    ownerState.section0StringFac.size(),
                    parsed.sectionByteCount > 0x24u ? (parsed.sectionData + 0x24u) : nullptr,
                    parsed.sectionByteCount > 0x24u ? parsed.sectionByteCount - 0x24u : 0u);
                CopyCStringIntoFixed(
                    ownerState.section0StringFcc.data(),
                    ownerState.section0StringFcc.size(),
                    parsed.sectionByteCount > 0x44u ? (parsed.sectionData + 0x44u) : nullptr,
                    parsed.sectionByteCount > 0x44u ? parsed.sectionByteCount - 0x44u : 0u);
                if (parsed.sectionByteCount > 0x485u && ownerState.state8Section0OverflowBuffer13f0 == nullptr) {
                    AppendOwnedSectionBytesU16(
                        ownerState.state8Section0OverflowBuffer13f0,
                        ownerState.state8Section0OverflowLength13f4,
                        parsed.sectionData + 0x485u,
                        static_cast<uint16_t>(parsed.sectionByteCount - 0x485u));
                }
                ownerState.section0Flag13f6 = 1u;
                spdlog::info(
                    "CLTLoginState_State8 section0 parsed name='{}' first='{}' last='{}' background='{}' ptr0=0x{:08x} extra13cc=0x{:08x} extra13d0=0x{:08x}",
                    ownerState.characterNameBufferF1c[0] ? std::string(ownerState.characterNameBufferF1c) : std::string("<empty>"),
                    ownerState.section0StringF8c[0] ? std::string(ownerState.section0StringF8c.data()) : std::string("<empty>"),
                    ownerState.section0StringFac[0] ? std::string(ownerState.section0StringFac.data()) : std::string("<empty>"),
                    ownerState.section0StringFcc[0] ? std::string(ownerState.section0StringFcc.data()) : std::string("<empty>"),
                    static_cast<unsigned>(ownerState.characterRecordPointersF88[0]),
                    static_cast<unsigned>(ownerState.replySectionData13cc),
                    static_cast<unsigned>(ownerState.replySectionData13d0));
                LogState8PersistenceFamilySnapshot(ownerState, "section0", parsed.sectionSelectorMinus2, parsed.sectionByteCount, false);
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
            LogState8PersistenceFamilySnapshot(ownerState, "section9_clcfg1452", parsed.sectionSelectorMinus2, parsed.sectionByteCount, false);
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

static void LogState8PersistenceFamilySnapshot(
    const PostAuthMarginLoadingState& ownerState,
    const char* reason,
    uint32_t sectionSelectorMinus2,
    uint16_t sectionByteCount,
    bool completed) {
    const uint32_t bodyWord00 = ReadU32LE(ownerState.state8Section0RawF88.data());
    const uint32_t headerWord00 = ownerState.characterFlagsF48[0];
    const uint32_t headerWord04 = ownerState.characterFlagsF48[1];
    const uint32_t headerWord08 = ownerState.characterFlagsF48[2];
    const uint32_t headerWord0c = ownerState.characterFlagsF48[3];
    const uint32_t secondaryWord00 = ownerState.secondaryCharacterDataF68[0];
    const uint32_t secondaryWord04 = ownerState.secondaryCharacterDataF68[1];
    const uint32_t section11Length = static_cast<uint32_t>(ownerState.state8Section11String1460.size());

    spdlog::info(
        "CLTLoginState_State8 persistence family [{}] completed={} section={} bytes={} f1c='{}' f3c=0x{:08x} f40=0x{:08x} f48[0..3]=[0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x}] f68[0..1]=[0x{:08x} 0x{:08x}] f88_00=0x{:08x} f88_444=0x{:08x} f88_448=0x{:08x} overflow13f4=0x{:04x} gate1452={} sec11_145c=0x{:08x} sec11_len={}",
        reason ? reason : "<unknown>",
        completed ? 1u : 0u,
        static_cast<unsigned>(sectionSelectorMinus2),
        static_cast<unsigned>(sectionByteCount),
        ownerState.characterNameBufferF1c[0] ? ownerState.characterNameBufferF1c : "<empty>",
        static_cast<unsigned>(ownerState.characterReplyFieldF3c),
        static_cast<unsigned>(ownerState.characterReplyFieldF40),
        static_cast<unsigned>(headerWord00),
        static_cast<unsigned>(headerWord04),
        static_cast<unsigned>(headerWord08),
        static_cast<unsigned>(headerWord0c),
        static_cast<unsigned>(secondaryWord00),
        static_cast<unsigned>(secondaryWord04),
        static_cast<unsigned>(bodyWord00),
        static_cast<unsigned>(ownerState.replySectionData13cc),
        static_cast<unsigned>(ownerState.replySectionData13d0),
        static_cast<unsigned>(ownerState.state8Section0OverflowLength13f4),
        static_cast<unsigned>(ownerState.flag1452),
        static_cast<unsigned>(ownerState.state8Section11Dword145c),
        static_cast<unsigned>(section11Length));
}

}  // namespace

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
    //     - newer `0x43acf0 + 0x4557b0` tightening now makes the growth rule explicit:
    //       reserve `(GameSessionID byte count including NUL) + 2` bytes at the tail of the
    //       shared message object, write the resulting payload-relative offset back to fixed field
    //       `+0xb9`, then copy the text into that reservation
    //     - but fresh original-launcher WineDbg validation on the natural first state8 send now
    //       shows owner `+0x664` / `GetGameSessionId664()` returning `""` there
    //     - practical consequence: the natural `0x0bb -> 0x13b` growth is **not** explained by a
    //       non-empty `GameSessionID` append on that first existing-character send
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
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue blocked on owner+0x1c state!=2; original would switch helper state to 4 currentState={}",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 0u;
    }
    if (mediator->State10SendGateFlagF14() == 0) {
        if (CLTLoginState* fallbackState = mediator->ScaffoldState6()) {
            mediator->SwitchHelperStateScaffold(6u, fallbackState);
        }
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue blocked on owner+0xf14==0; original would switch helper state to 6 currentState={}",
            mediator->CurrentState() ? mediator->CurrentState()->DebugName() : "<unchanged>");
        return 0u;
    }

    replySectionsSeen_ = 0;
    replySectionsExpected_ = 0;
    const SlotRecordState004b5328* currentSlotRecord = mediator->GetCurrentSlotRecord();
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

    const uint32_t sendResult = mediator->SendCurrentMarginPacketScaffold(packetBuilder.EnvelopeScaffold());
    mediator->PostEventScaffold(0x09u);

    spdlog::debug(
        "CLTLoginState_State8::Slot3_BeginOrContinue state8 snapshot blocks cd0={} ce0={} cf0={} d00={} d10={} d20={} d30={} d40={} d50={} d60={} d70={}",
        FormatU32x4Block(mediator->SelectionContextBlockCd0()),
        FormatU32x4Block(mediator->SelectionContextBlockCe0()),
        FormatU32x4Block(mediator->SelectionContextBlockCf0()),
        FormatU32x4Block(mediator->SelectionContextBlockD00()),
        FormatU32x4Block(mediator->SelectionContextBlockD10()),
        FormatU32x4Block(mediator->SelectionContextBlockD20()),
        FormatU32x4Block(mediator->SelectionContextBlockD30()),
        FormatU32x4Block(mediator->SelectionContextBlockD40()),
        FormatU32x4Block(mediator->SelectionContextBlockD50()),
        FormatU32x4Block(mediator->SelectionContextBlockD60()),
        FormatU32x4Block(mediator->SelectionContextBlockD70()));

    const unsigned nonZeroSnapshotBlockCount =
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCd0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCe0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockCf0())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD00())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD10())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD20())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD30())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD40())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD50())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD60())) +
        static_cast<unsigned>(U32x4BlockHasAnyNonZero(mediator->SelectionContextBlockD70()));
    const char* gameSessionId = mediator->GetGameSessionId664();

    spdlog::info(
        "DIAGNOSTIC: CLTLoginState_State8::Slot3_BeginOrContinue built structured margin packet fixedBytes=0x{:02x} totalBytes=0x{:02x} gcidLow=0x{:08x} gcidHigh=0x{:08x} nonZeroSnapshotBlocks={}/11 blockCd0_0=0x{:08x} blockD70_3=0x{:08x} GameSessionID='{}' -> sendResult=0x{:08x} then posts event=9",
        State8StructuredMarginPacketFixedPayload::kFixedByteCount,
        packetBuilder.PayloadByteCount(),
        currentSlotRecord ? currentSlotRecord->globalCharacterIdLow03 : 0u,
        currentSlotRecord ? currentSlotRecord->globalCharacterIdHigh07 : 0u,
        nonZeroSnapshotBlockCount,
        mediator->SelectionContextBlockCd0()[0],
        mediator->SelectionContextBlockD70()[3],
        gameSessionId ? gameSessionId : "<empty>",
        sendResult);
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
            spdlog::info(
                "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage delegated non-0x10 fallback through owner callback84 -> dispatchResult=0x{:08x}",
                fallbackResult);
            return 1u;
        }
        mediator->WorldListCountOrStatus80() = 0x12000005u;
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage non-0x10 fallback through owner callback84 returned 0x{:08x}; mirrored owner+0x80=0x12000005",
            fallbackResult);
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
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage observed failure status=0x{:08x}; original would switch helper state to 3 and post error=10 currentState={}",
            parsed.status,
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
        LogState8PersistenceFamilySnapshot(ownerState, "completed", parsed.sectionSelectorMinus2, parsed.sectionByteCount, true);

        if (CLTLoginState* nextBase = mediator->ScaffoldState9()) {
            if (auto* nextState = dynamic_cast<CLTLoginState_State9*>(nextBase)) {
                nextState->SetPendingPayload(/*byte4=*/0, parsed.handoffWord09);
            }
            mediator->SwitchHelperStateScaffold(9u, nextBase);
        }
        // anchor: launcher.exe:0x43f930 completion tail posts event 0x0b after switching to helper9.
        mediator->PostEventScaffold(0x0bu);

        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage completed state8 reply progression status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} firstFragment={} usedCurrentSlotRecord={} -> currentState=helper9 event=0x0b",
            parsed.status,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            parsed.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_,
            firstFragment ? 1u : 0u,
            usedCurrentSlotRecord ? 1u : 0u);
        replySectionsSeen_ = 0;
        replySectionsExpected_ = 0;
        ownerState.state8Section10ChunkBitmap = 0u;
    } else {
        spdlog::info(
            "DIAGNOSTIC: CLTLoginState_State8::Slot6_HandleSecondaryMessage routed state8 reply status=0x{:08x} section={} bytes={} handoffWord=0x{:04x} seen={} expected={} seedCount={} firstFragment={} usedCurrentSlotRecord={}",
            parsed.status,
            parsed.sectionSelectorMinus2,
            parsed.sectionByteCount,
            parsed.handoffWord09,
            replySectionsSeen_,
            replySectionsExpected_,
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

}  // namespace mxo::ltlogin
