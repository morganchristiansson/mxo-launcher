#pragma once

#include <cstddef>
#include <cstdint>

namespace mxo::liblttcp {

// Recovered non-virtual helper from launcher.exe `ltipaddresslist.cpp`.
// Current best original in-object layout is exactly four pointers / 0x10 bytes:
// - `+0x00` = begin
// - `+0x04` = end
// - `+0x08` = capacity
// - `+0x0c` = current iterator
class CLTIPAddressList {
public:
    static constexpr uint32_t kFlagShuffle = 0x01;
    static constexpr uint32_t kFlagIgnoreHostsFile = 0x02;

    // anchor: launcher.exe:0x00440d80
    bool Reinit(const char* hostList, uint32_t flags);

    // anchor: launcher.exe:0x00440bb0
    uint32_t GetNextAddress(bool wrap);

    void Reset();
    size_t Count() const;
    bool Empty() const;

private:
    // anchor: launcher.exe:0x00440cd0
    bool ResolveAndAppendToken(const char* token, uint32_t flags);

    // anchor: launcher.exe:0x00440bf0
    void ShuffleResolvedRange();

public:
    uint32_t* begin_ = nullptr;
    uint32_t* end_ = nullptr;
    uint32_t* capacity_ = nullptr;
    uint32_t* next_ = nullptr;
};

static_assert(sizeof(CLTIPAddressList) == 0x10, "CLTIPAddressList size mismatch");

}  // namespace mxo::liblttcp
