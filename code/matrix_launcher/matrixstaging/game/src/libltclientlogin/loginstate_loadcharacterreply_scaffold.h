#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

class LoadCharacterReplyEnvelope {
public:
    // anchor: launcher.exe:0x43ae50
    LoadCharacterReplyEnvelope(
        const std::vector<uint8_t>& incomingMarginMessageBytes,
        bool initializeEmptyReply)
        : incomingMarginMessageBytes08_(&incomingMarginMessageBytes),
          initializeEmptyReply0c_(initializeEmptyReply) {
        messageBase04_ = incomingMarginMessageBytes.empty()
            ? nullptr
            : const_cast<uint8_t*>(incomingMarginMessageBytes.data());
        RefreshDataSectionView(static_cast<char>(initializeEmptyReply ? 1 : 0));
        if (!initializeEmptyReply) {
            ResetToDefaultMessage();
        }

        valid = currentMessage10_ != nullptr &&
                incomingMarginMessageBytes.size() >= 0x10u &&
                currentMessage10_[0] == 0x10u;
        if (!valid) {
            return;
        }

        status = ReadU32LE(currentMessage10_ + 1u);
        field05 = ReadU32LE(currentMessage10_ + 5u);
        handoffWord09 = ReadU16LE(currentMessage10_ + 9u);
        expectedSectionCount0b = currentMessage10_[0x0b];
        shouldSeedExpectedSectionCount = (currentMessage10_[0x0c] == 0x01u);
        sectionSelectorMinus2 = static_cast<uint8_t>(currentMessage10_[0x0d] - 2u);
        sectionOffset0e = ReadU16LE(currentMessage10_ + 0x0eu);
        sectionByteCount = dataSectionByteCount18_;
        sectionData = dataSectionBytes14_;
    }

    // anchor: launcher.exe:0x43ae00
    void RefreshDataSectionView(char initializeEmptyReply) {
        currentMessage10_ = messageBase04_;
        if (initializeEmptyReply == '\0') {
            dataSectionBytes14_ = nullptr;
            dataSectionByteCount18_ = 0u;
            return;
        }
        if (currentMessage10_ == nullptr || incomingMarginMessageBytes08_ == nullptr ||
            incomingMarginMessageBytes08_->size() < 0x10u) {
            dataSectionBytes14_ = nullptr;
            dataSectionByteCount18_ = 0u;
            return;
        }

        const uint16_t sectionOffset0eLocal = ReadU16LE(currentMessage10_ + 0x0eu);
        if (sectionOffset0eLocal != 0u &&
            static_cast<size_t>(sectionOffset0eLocal) + 2u <= incomingMarginMessageBytes08_->size()) {
            dataSectionByteCount18_ = ReadU16LE(currentMessage10_ + sectionOffset0eLocal);
            dataSectionBytes14_ = currentMessage10_ + sectionOffset0eLocal + 2u;
            const size_t remaining = incomingMarginMessageBytes08_->size() - (sectionOffset0eLocal + 2u);
            if (dataSectionByteCount18_ > remaining) {
                dataSectionByteCount18_ = static_cast<uint16_t>(remaining);
            }
            return;
        }

        dataSectionByteCount18_ = 0u;
        dataSectionBytes14_ = nullptr;
    }

    // anchor: launcher.exe:0x43af20
    void ResetToDefaultMessage() {
        defaultMessageStorage_.fill(0u);
        messageBase04_ = defaultMessageStorage_.data();
        currentMessage10_ = messageBase04_;
        currentMessage10_[0x00] = 0x10u;
        currentMessage10_[0x0b] = 1u;
        dataSectionBytes14_ = nullptr;
        dataSectionByteCount18_ = 0u;

        valid = true;
        status = 0u;
        field05 = 0u;
        handoffWord09 = 0u;
        expectedSectionCount0b = 1u;
        shouldSeedExpectedSectionCount = false;
        sectionSelectorMinus2 = static_cast<uint8_t>(0u - 2u);
        sectionOffset0e = 0u;
        sectionByteCount = 0u;
        sectionData = nullptr;
    }

    // anchor: launcher.exe:0x43cca0
    void AppendDebugString(std::string& out, int verbosityLevel) const {
        if (verbosityLevel == 2 || verbosityLevel == 3) {
            out += "Status:" + std::to_string(status);
            out += " CharacterID:" + std::to_string(field05);
            out += " UDPPort:" + std::to_string(handoffWord09);
            out += " MessageNumber:" + std::to_string(expectedSectionCount0b);
            out += " FinalMessage:" + std::to_string(shouldSeedExpectedSectionCount ? 1 : 0);
            out += " DataType:" + std::to_string(static_cast<unsigned>(sectionSelectorMinus2 + 2u));
            if (verbosityLevel == 2) {
                out += " Data:(Array of size " + std::to_string(sectionByteCount) + ") ";
            } else {
                out += " Data:[";
                for (uint16_t i = 0; i < sectionByteCount; ++i) {
                    out += std::to_string(sectionData ? static_cast<unsigned>(sectionData[i]) : 0u);
                    out += ',';
                }
                out += "] ";
            }
        }
    }

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
    uint8_t* messageBase04_ = nullptr;
    const std::vector<uint8_t>* incomingMarginMessageBytes08_ = nullptr;
    bool initializeEmptyReply0c_ = false;
    uint8_t* currentMessage10_ = nullptr;
    const uint8_t* dataSectionBytes14_ = nullptr;
    uint16_t dataSectionByteCount18_ = 0;
    std::array<uint8_t, 0x10> defaultMessageStorage_{};
};


}  // namespace mxo::ltlogin
