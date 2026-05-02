#pragma once

#include <cstdint>

// Note: LoginObserverTreeNode674 is defined in loginmediator.h

namespace mxo {
namespace ltlogin {

// Historical note:
// - launcher.exe owner `+0x674` still fingerprints as an SGI/libstdc++-style RB-tree wrapper
// - but the callsites (`RegisterLoginObserver`, `UnregisterLoginObserver`, `PostEvent`,
//   `PostError`) are semantically a plain unique observer set keyed by the observer pointer
// - source now models that outer layer directly as a set-like abstraction instead of re-owning the
//   tree mechanics in this TU
class LoginObserverTreeHelper674 {
public:
    // anchor: launcher.exe:0x419510 / TreeKey
    static uintptr_t TreeKey(void* observer) {
        return reinterpret_cast<uintptr_t>(observer);
    }
};

} // namespace ltlogin
} // namespace mxo