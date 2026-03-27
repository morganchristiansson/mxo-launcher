#pragma once

#include <cstdint>

namespace mxo::liblttcp {

// =============================================================================
// ILTTCPEngine - VTable 0x004b2768 pure virtual interface
// =============================================================================
// Faithful source-location note:
// - recovered source-file anchor:
//   `\matrixstaging\runtime\src\liblttcp\ilttcpengine.cpp`
// - canonical RE references remain:
//   - docs/launcher.exe/startup_objects/0x4d6304_network_engine.md
//   - docs/launcher.exe/VTABLES/0x004b2768.md
//
// This mirrors the launcher arg5 / network-engine primary vtable shape so the current
// CLTThreadPerClientTCPEngine scaffolding can keep moving toward a direct ABI-compatible surface.
// Keep the slot names stable where they are string-backed or already anchored in the canonical
// vtable docs; keep low-confidence slots explicitly labeled by address.
// =============================================================================
class ILTTCPEngine {
public:
    // +0x00
    virtual int Release(uint32_t flags) = 0;
    // +0x04
    virtual uint32_t MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) = 0;
    // +0x08
    virtual uint32_t UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext) = 0;
    // +0x0c
    virtual uint32_t MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext) = 0;
    // +0x10
    virtual uint32_t Slot4_42F7C0(void* arg1) = 0;
    // +0x14
    virtual uint32_t UnmonitorPort(uint16_t portHostOrder, uint32_t* outSocketHandle, uint32_t ipv4NetworkOrder) = 0;
    // +0x18
    virtual uint32_t Connect(void* contextKey) = 0;
    // +0x1c
    virtual uint32_t Close(void* contextKey, bool graceful) = 0;
    // +0x20
    virtual uint32_t SendBuffer(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext) = 0;
    // +0x24
    virtual uint32_t Slot9_42FD10(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) = 0;
    // +0x28
    virtual uint32_t Slot10_443810(void* arg1) = 0;
    // +0x2c
    virtual uint32_t Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) = 0;
    // +0x30
    virtual uint32_t CleanupConnection(void* contextKey) = 0;
};

}  // namespace mxo::liblttcp
