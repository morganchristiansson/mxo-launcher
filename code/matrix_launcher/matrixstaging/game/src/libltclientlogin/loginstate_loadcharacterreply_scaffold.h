#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mxo::ltlogin {

inline uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t ReadU32LE(const uint8_t* p) {
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

inline ParsedState11LoadCharacterReplyScaffold ParseState11LoadCharacterReplyScaffold(
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

}  // namespace mxo::ltlogin
