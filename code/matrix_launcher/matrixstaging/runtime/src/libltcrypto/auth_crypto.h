#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mxo {
namespace auth {

// Reimplementation note:
// This header is now the **transitional public auth API** shared by the launcher path and the
// host-native auth probe.
//
// Current implementation split:
// - declarations remain here for compatibility
// - implementation now lives conservatively under recovered runtime-style paths:
//   - `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
//   - `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
//   - `\matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp`
//
// Keep launcher-owned auth progression/state writeback under the `ltlogin` layer even when
// the low-level packet builders/parsers here grow more complete.

// File anchors:
// - canonical public auth header home:
//   `\matrixstaging\runtime\src\libltcrypto\auth_crypto.h`
// - compatibility wrapper retained at:
//   `src/auth/auth_crypto.h`
//
// Address-anchor policy for this header:
// - use concrete launcher.exe addresses when the surrounding caller/consumer is known
// - use explicit `[not yet isolated]` markers where the exact original helper VA is still open

enum FrameMode {
    kFrameModeAuto = 0,
    kFrameModeForceOneByte = 1,
    kFrameModeForceTwoByte = 2,
};

// String/diagnostic helper anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
// - exact original helper VAs for these tiny helpers: [not yet isolated]
const char* AuthOpcodeName(uint8_t rawCode);

// Variable-length framing anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp`
// - exact original helper VA: [not yet isolated]
// - important currently recovered callers/builders:
//   - launcher.exe:0x447eb0
//   - launcher.exe:0x4474f0
//   - launcher.exe:0x43b830
bool BuildVariableLengthPacket(
    const uint8_t* payload,
    size_t payloadSize,
    FrameMode mode,
    std::vector<uint8_t>* outPacketBytes,
    size_t* outHeaderByteCount = nullptr);

// Transitional low-level margin CERT/MS bootstrap helpers.
// These stay in the shared crypto layer because they model reusable wire/crypto behavior backed by
// open server/proxy evidence:
// - `../../../work/mxoemu/Reality/Source/MarginSocket.cpp`
// - `../../../work/mxoemu/Proxy/Logging.cpp`
// They are not a claim that final launcher-owned state progression/source ownership should live
// here permanently.


}  // namespace auth
}  // namespace mxo
