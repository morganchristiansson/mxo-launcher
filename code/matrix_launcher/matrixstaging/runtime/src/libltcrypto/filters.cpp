// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltcrypto\filters.cpp
//
// Transitional reimplementation note:
// This file currently hosts low-level auth text/packet parse helpers that are below the
// launcher-owned login state machine but not yet split to exact original class boundaries.
//
// Address anchors:
// - later incoming auth-reply owner/helper handler: launcher.exe:0x4401a0
//   - current best `CLTLoginState` mapping: `CLTLoginState_State10_0x4b512c` vtable `0x004b512c` slot 6
//   - older Ghidra label: `CLTLoginMediator_Helper10_HandleAuthReply`
// - concrete auth-reply parser object helper: launcher.exe:0x43a330
// - auth opcode read helper used by that later path: launcher.exe:0x41bc20
// - exact original parser VAs for 0x07/0x09 packet bodies: [not yet isolated]

#include "auth_internal.h"

namespace mxo::auth {

const char* AuthOpcodeName(uint8_t rawCode) {
    switch (rawCode) {
        case 0x06: return "AS_GetPublicKeyRequest";
        case 0x07: return "AS_GetPublicKeyReply";
        case 0x08: return "AS_AuthRequest";
        case 0x09: return "AS_AuthChallenge";
        case 0x0a: return "AS_AuthChallengeResponse";
        case 0x0b: return "AS_AuthReply";
        case 0x35: return "AS_GetWorldListRequest";
        case 0x36: return "AS_GetWorldListReply";
        default: return "<unknown-auth-code>";
    }
}

std::string HexEncode(const uint8_t* data, size_t size) {
    static const char kHexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(size * 2u);
    for (size_t i = 0; i < size; ++i) {
        const uint8_t value = data[i];
        hex.push_back(kHexDigits[(value >> 4u) & 0x0fu]);
        hex.push_back(kHexDigits[value & 0x0fu]);
    }
    return hex;
}

std::string HexEncode(const std::vector<uint8_t>& bytes) {
    return bytes.empty() ? std::string() : HexEncode(bytes.data(), bytes.size());
}


}  // namespace mxo::auth
