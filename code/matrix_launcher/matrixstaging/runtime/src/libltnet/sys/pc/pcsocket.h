#pragma once

namespace mxo::libltnet {

// Current best static read from launcher.exe `pcsocket.cpp`:
// - `CLTSocketLayer` is used as a process-wide socket bootstrap utility
// - recovered method `0x00452e00 = CLTSocketLayer::Init()` is a static-style entry
// - no concrete instance field accesses or CLTSocketLayer-owned vtable have been recovered yet
class CLTSocketLayer {
public:
    // anchor: launcher.exe:0x00452e00
    static bool Init();
};

}  // namespace mxo::libltnet
