// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltmessaging\variablelengthprefixedtcpstreamparser.cpp
//
// Transitional reimplementation note:
// Keep the variable-length packet framing split here while the public declarations now live at:
// - `\matrixstaging\runtime\src\libltcrypto\auth_crypto.h`
// Compatibility wrapper retained at:
// - `src/auth/auth_crypto.h`
//
// Address anchors:
// - exact original helper function VA for this framing code: [not yet isolated]
// - current runtime-supported wire shape:
//   - 1-byte prefix for short payloads
//   - 2-byte prefix with high bit set for larger payloads
// - important current callers in the recovered launcher-owned auth path:
//   - launcher.exe:0x447eb0 builds/sends raw 0x06 payload
//   - launcher.exe:0x4474f0 builds/sends raw 0x08 payload
//   - launcher.exe:0x43b830 builds/sends raw 0x35 payload

#include "../libltcrypto/auth_crypto.h"

namespace mxo::auth {

bool BuildVariableLengthPacket(
    const uint8_t* payload,
    size_t payloadSize,
    FrameMode mode,
    FramedPacket* outPacket) {
    if (!payload || !outPacket) {
        return false;
    }

    outPacket->headerBytes.clear();
    outPacket->payloadBytes.assign(payload, payload + payloadSize);
    outPacket->bytes.clear();

    if (mode == kFrameModeForceOneByte) {
        if (payloadSize > 0x7fu) {
            return false;
        }
        outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
    } else {
        if (payloadSize > 0x7fffu) {
            return false;
        }
        if (mode == kFrameModeForceTwoByte || payloadSize >= 0x80u) {
            outPacket->headerBytes.push_back(
                static_cast<uint8_t>(0x80u | ((payloadSize >> 8u) & 0x7fu)));
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize & 0xffu));
        } else {
            outPacket->headerBytes.push_back(static_cast<uint8_t>(payloadSize));
        }
    }

    outPacket->bytes.reserve(outPacket->headerBytes.size() + outPacket->payloadBytes.size());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->headerBytes.begin(),
        outPacket->headerBytes.end());
    outPacket->bytes.insert(
        outPacket->bytes.end(),
        outPacket->payloadBytes.begin(),
        outPacket->payloadBytes.end());
    return true;
}

bool ParseVariableLengthPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    FramedPacket* outPacket) {
    if (!packetBytes || !outPacket || packetSize < 2u) {
        return false;
    }

    size_t headerSize = 1u;
    size_t payloadSize = 0u;
    if (packetBytes[0] & 0x80u) {
        if (packetSize < 3u) {
            return false;
        }
        headerSize = 2u;
        payloadSize =
            (static_cast<size_t>(packetBytes[0] & 0x7fu) << 8u) |
            static_cast<size_t>(packetBytes[1]);
    } else {
        payloadSize = static_cast<size_t>(packetBytes[0]);
    }

    if (headerSize + payloadSize > packetSize) {
        return false;
    }

    outPacket->headerBytes.assign(packetBytes, packetBytes + headerSize);
    outPacket->payloadBytes.assign(
        packetBytes + headerSize,
        packetBytes + headerSize + payloadSize);
    outPacket->bytes.assign(
        packetBytes,
        packetBytes + headerSize + payloadSize);
    return true;
}

}  // namespace mxo::auth
