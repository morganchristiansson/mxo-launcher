#include "ltipaddresslist.h"

#include "../libltnet/sys/pc/pcdns.h"
#include "../libltnet/sys/pc/pcsocket.h"

#include <winsock2.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace mxo::liblttcp {
namespace {

static std::unordered_map<const CLTIPAddressList*, std::vector<uint32_t>>
    g_CLTIPAddressListBackingStorage;

static std::vector<uint32_t>& BackingStorage(CLTIPAddressList* self) {
    return g_CLTIPAddressListBackingStorage[self];
}

static void SyncPointerFields(CLTIPAddressList* self, size_t nextIndex) {
    if (!self) {
        return;
    }

    std::vector<uint32_t>& storage = BackingStorage(self);
    if (storage.empty()) {
        self->begin_ = nullptr;
        self->end_ = nullptr;
        self->capacity_ = nullptr;
        self->next_ = nullptr;
        return;
    }

    self->begin_ = storage.data();
    self->end_ = self->begin_ + storage.size();
    self->capacity_ = self->begin_ + storage.capacity();
    if (nextIndex > storage.size()) {
        nextIndex = storage.size();
    }
    self->next_ = self->begin_ + nextIndex;
}

static bool SeedShuffleGeneratorOnce() {
    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }
    return seeded;
}

}  // namespace

void CLTIPAddressList::Reset() {
    std::vector<uint32_t>& storage = BackingStorage(this);
    storage.clear();
    SyncPointerFields(this, 0u);
}

size_t CLTIPAddressList::Count() const {
    if (!begin_ || !end_ || end_ < begin_) {
        return 0u;
    }
    return static_cast<size_t>(end_ - begin_);
}

bool CLTIPAddressList::Empty() const {
    return Count() == 0u;
}

// anchor: launcher.exe:0x00440bf0
void CLTIPAddressList::ShuffleResolvedRange() {
    std::vector<uint32_t>& storage = BackingStorage(this);
    if (storage.size() < 2u) {
        return;
    }

    SeedShuffleGeneratorOnce();

    // Static-RE note:
    // - original `0x440bf0` walks forward from element 1 to end-1
    // - on each step it computes a swap index in `[0, currentIndex]` from
    //   `rand() * (currentIndex + 1) / 32768.0`
    for (size_t currentIndex = 1; currentIndex < storage.size(); ++currentIndex) {
        const double scaled = static_cast<double>(std::rand()) * static_cast<double>(currentIndex + 1u) /
                              (static_cast<double>(RAND_MAX) + 1.0);
        const size_t swapIndex = static_cast<size_t>(scaled);
        std::swap(storage[currentIndex], storage[swapIndex]);
    }

    const size_t nextIndex = (next_ && begin_ && next_ >= begin_)
        ? static_cast<size_t>(next_ - begin_)
        : 0u;
    SyncPointerFields(this, nextIndex);
}

// anchor: launcher.exe:0x00440cd0
bool CLTIPAddressList::ResolveAndAppendToken(const char* token, uint32_t flags) {
    if (!token || !token[0]) {
        return false;
    }

    std::vector<uint32_t>& storage = BackingStorage(this);

    if ((flags & kFlagIgnoreHostsFile) != 0u) {
        const bool resolved = DNSResolveNameToIPList(
            token,
            &storage,
            kDNSResolveNameToIPListFlagNoHostsFile);
        SyncPointerFields(this, storage.size());
        return resolved;
    }

    if (!CLTSocketLayer::Init()) {
        return false;
    }

    hostent* host = gethostbyname(token);
    if (!host || host->h_addrtype != AF_INET || !host->h_addr_list) {
        return false;
    }

    bool resolvedAny = false;
    for (char** addressIt = host->h_addr_list; *addressIt; ++addressIt) {
        uint32_t ipv4NetworkOrder = 0;
        std::memcpy(&ipv4NetworkOrder, *addressIt, sizeof(ipv4NetworkOrder));
        storage.push_back(ipv4NetworkOrder);
        resolvedAny = true;
    }

    SyncPointerFields(this, storage.size());
    return resolvedAny;
}

// anchor: launcher.exe:0x00440d80
bool CLTIPAddressList::Reinit(const char* hostList, uint32_t flags) {
    std::vector<uint32_t>& storage = BackingStorage(this);
    storage.clear();
    SyncPointerFields(this, 0u);

    std::string scratch = hostList ? hostList : "";
    scratch.push_back('\0');
    const char* const delimiters = ";";
    for (char* token = std::strtok(scratch.data(), delimiters);
         token != nullptr;
         token = std::strtok(nullptr, delimiters)) {
        if (!ResolveAndAppendToken(token, flags)) {
            spdlog::warn(
                "CLTIPAddressList::Reinit scaffold failed token='{}' hostList='{}' flags=0x{:02x}",
                token,
                hostList ? hostList : "",
                flags);
            storage.clear();
            SyncPointerFields(this, 0u);
            return false;
        }
    }

    if ((flags & kFlagShuffle) != 0u) {
        ShuffleResolvedRange();
    }

    SyncPointerFields(this, 0u);
    return !storage.empty();
}

// anchor: launcher.exe:0x00440bb0
uint32_t CLTIPAddressList::GetNextAddress(bool wrap) {
    if (Empty()) {
        return 0u;
    }

    if (next_ == end_) {
        if (!wrap) {
            return 0u;
        }
        next_ = begin_;
    }

    if (!next_ || !begin_ || next_ < begin_ || next_ >= end_) {
        return 0u;
    }

    return *next_++;
}

}  // namespace mxo::liblttcp
