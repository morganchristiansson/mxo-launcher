#pragma once

#include <cstddef>
#include <cstdint>

// Forward declare message ref type (defined in liblttcp namespace)
namespace mxo { namespace liblttcp { class CMessageConnectionMessageRef_0x4ba23c; } }

// Minimal shared packet builder envelope base for inheritance.
// anchor: launcher.exe:0x004af2a4 / vtable
// anchor: launcher.exe:0x439840 / ctor
namespace mxo {

class PacketBuilder_0x4af2a4 {
public:
    void** vtable00 = nullptr;
    uint32_t nopatchLauncherVersionValue04 = 0;
    liblttcp::CMessageConnectionMessageRef_0x4ba23c* messageRef08 = nullptr;
    uint32_t ownerReadyFlag0c = 0;
    void* payloadBegin10 = nullptr;
    uint16_t payloadLength14 = 0;
    uint8_t statusByte16 = 0;
    uint32_t characterIdLow1c = 0;
    uint32_t characterIdHigh20 = 0;
    uint16_t worldId24 = 0;
    
    PacketBuilder_0x4af2a4() = default;
    ~PacketBuilder_0x4af2a4() = default;
};

} // namespace mxo