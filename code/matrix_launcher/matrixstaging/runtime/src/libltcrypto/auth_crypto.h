#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

struct FramedPacket {
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> payloadBytes;
    std::vector<uint8_t> bytes;
};

struct AuthBlobLayout {
    uint8_t leadingByte;
    uint32_t rsaMethod;
    uint16_t someShort;
    uint32_t embeddedTime;
    std::vector<uint8_t> twofishKey;
    bool includeUsernameNullTerminator;
    int usernameLengthAdjust;

    AuthBlobLayout()
        : leadingByte(0x00),
          rsaMethod(4),
          someShort(0x001b),
          embeddedTime(0),
          includeUsernameNullTerminator(true),
          usernameLengthAdjust(0) {}
};

struct AuthRequestLayout {
    uint32_t publicKeyId;
    uint8_t loginType;
    uint16_t reservedWord;
    std::vector<uint8_t> keyConfigMd5;
    std::vector<uint8_t> uiConfigMd5;
    std::vector<uint8_t> fixedHeaderBytes;
    std::vector<uint8_t> rsaModulusBytes;
    std::vector<uint8_t> rsaExponentBytes;

    AuthRequestLayout()
        : publicKeyId(0),
          loginType(1),
          reservedWord(0) {}
};

struct AuthChallengeResponseLayout {
    uint16_t packetSomeShort;
    uint8_t plaintextLeadingByte;
    uint16_t unknown1;
    uint16_t unknown2;
    uint16_t unknown3;
    bool usePasswordLengthForUnknown2;
    bool useSoePasswordLengthForUnknown3;
    bool includePasswordNullTerminator;
    bool includeSoePasswordNullTerminator;
    uint8_t paddingByte;

    AuthChallengeResponseLayout()
        : packetSomeShort(0x001b),
          plaintextLeadingByte(0x00),
          unknown1(23),
          unknown2(0),
          unknown3(0),
          usePasswordLengthForUnknown2(true),
          useSoePasswordLengthForUnknown3(true),
          includePasswordNullTerminator(true),
          includeSoePasswordNullTerminator(true),
          paddingByte(0x00) {}
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
    FramedPacket* outPacket);

// Transitional low-level margin CERT/MS bootstrap helpers.
// These stay in the shared crypto layer because they model reusable wire/crypto behavior backed by
// open server/proxy evidence:
// - `../../../work/mxoemu/Reality/Source/MarginSocket.cpp`
// - `../../../work/mxoemu/Proxy/Logging.cpp`
// They are not a claim that final launcher-owned state progression/source ownership should live
// here permanently.

struct MarginConnectReply {
    bool valid;
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> payloadBytes;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> encryptedPayloadBytes;
    uint32_t status0;
    uint32_t status1;
    uint32_t sessionId;
    uint16_t field0d;
    uint16_t field0f;
    uint16_t field11;
    uint16_t field13;
    uint16_t field15;

    MarginConnectReply()
        : valid(false),
          status0(0),
          status1(0),
          sessionId(0),
          field0d(0),
          field0f(0),
          field11(0),
          field13(0),
          field15(0) {}
};

// Generic encrypted margin payload wrapper used by MS bootstrap packets and later margin traffic.
// Wire shape follows the open-server `EncryptedPacket` helper: random IV + Twofish-CBC ciphertext
// over `[crc32][u16 length][u32 timestamp][payload]`.
bool EncryptMarginPayloadPacket(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    const std::vector<uint8_t>& twofishKeyBytes,
    FrameMode frameMode,
    FramedPacket* outPacket);

bool DecryptMarginPayloadPacket(
    const uint8_t* encryptedPayloadBytes,
    size_t encryptedPayloadSize,
    const std::vector<uint8_t>& twofishKeyBytes,
    std::vector<uint8_t>* outPayloadBytes);

}  // namespace auth
}  // namespace mxo
