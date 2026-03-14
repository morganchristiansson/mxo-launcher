#pragma once

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

struct GetPublicKeyReply {
    bool valid;
    bool hasEmbeddedPublicKey;
    uint32_t payloadLength;
    uint32_t status;
    uint32_t currentTime;
    uint32_t publicKeyId;
    uint8_t keySize;
    uint8_t reservedByte;
    uint8_t publicExponentByte;
    uint16_t unknownWord;
    uint16_t modulusLength;
    uint16_t signatureLength;
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> payloadBytes;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> tailBytes;
    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> signatureBytes;

    GetPublicKeyReply()
        : valid(false),
          hasEmbeddedPublicKey(false),
          payloadLength(0),
          status(0),
          currentTime(0),
          publicKeyId(0),
          keySize(0),
          reservedByte(0),
          publicExponentByte(0),
          unknownWord(0),
          modulusLength(0),
          signatureLength(0) {}
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

struct AuthRequestBuildResult {
    uint16_t usernameLengthField;
    bool includedUsernameNullTerminator;
    bool usedFixedHeaderOverride;
    bool usedProvidedPublicKey;
    FramedPacket packet;
    std::vector<uint8_t> authHeaderBytes;
    std::vector<uint8_t> keyConfigMd5Bytes;
    std::vector<uint8_t> uiConfigMd5Bytes;
    std::vector<uint8_t> blobPlaintextBytes;
    std::vector<uint8_t> blobCiphertextBytes;
    std::vector<uint8_t> twofishKeyBytes;

    AuthRequestBuildResult()
        : usernameLengthField(0),
          includedUsernameNullTerminator(true),
          usedFixedHeaderOverride(false),
          usedProvidedPublicKey(false) {}
};

struct AuthChallenge {
    bool valid;
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> payloadBytes;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> encryptedChallengeBytes;

    AuthChallenge() : valid(false) {}
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

struct AuthChallengeResponseBuildResult {
    uint16_t passwordLengthField;
    uint16_t soePasswordLengthField;
    uint16_t paddingLengthField;
    FramedPacket packet;
    std::vector<uint8_t> encryptedChallengeBytes;
    std::vector<uint8_t> decryptedChallengeBytes;
    std::vector<uint8_t> processedChallengeMd5Bytes;
    std::vector<uint8_t> plaintextBytes;
    std::vector<uint8_t> ciphertextBytes;

    AuthChallengeResponseBuildResult()
        : passwordLengthField(0),
          soePasswordLengthField(0),
          paddingLengthField(0) {}
};

struct MxoString {
    uint16_t length;
    std::string text;
    std::vector<uint8_t> rawBytes;

    MxoString() : length(0) {}
};

struct AuthCharacterEntry {
    uint8_t unknownByte;
    uint16_t handleStringOffset;
    uint64_t characterId;
    uint8_t status;
    uint16_t worldId;
    MxoString handle;

    AuthCharacterEntry()
        : unknownByte(0),
          handleStringOffset(0),
          characterId(0),
          status(0),
          worldId(0) {}
};

struct AuthWorldEntry {
    uint8_t unknownByte;
    uint16_t worldId;
    std::string worldName;
    uint8_t status;
    uint8_t type;
    uint32_t clientVersion;
    uint16_t unknown4;
    uint8_t load;

    AuthWorldEntry()
        : unknownByte(0),
          worldId(0),
          status(0),
          type(0),
          clientVersion(0),
          unknown4(0),
          load(0) {}
};

struct AuthSignedData {
    bool valid;
    uint8_t unknownByte;
    uint32_t userId1;
    std::string userName;
    uint16_t unknownShort;
    uint32_t padding1;
    uint32_t expiryTime;
    std::vector<uint8_t> padding2;
    uint16_t publicExponent;
    std::vector<uint8_t> modulusBytes;
    uint32_t timeCreated;
    std::vector<uint8_t> rawBytes;

    AuthSignedData()
        : valid(false),
          unknownByte(0),
          userId1(0),
          unknownShort(0),
          padding1(0),
          expiryTime(0),
          publicExponent(0),
          timeCreated(0) {}
};

struct AuthReply {
    bool valid;
    bool isErrorReply;
    bool hasAuthDataMarker;
    uint32_t errorCode;
    uint32_t zeroDword;
    uint16_t trailingWord;
    uint16_t offsetAuthData;
    uint16_t offsetEncryptedData;
    uint32_t unknown2;
    uint16_t offsetCharData;
    uint32_t unknown3;
    uint32_t offsetServerData;
    uint32_t offsetUsername;
    uint16_t characterCount;
    uint16_t worldCount;
    uint16_t authDataMarker;
    uint16_t encryptedPrivateExponentLength;
    MxoString username;
    std::vector<AuthCharacterEntry> characters;
    std::vector<AuthWorldEntry> worlds;
    std::vector<uint8_t> authSignatureBytes;
    AuthSignedData signedData;
    std::vector<uint8_t> encryptedPrivateExponentBytes;
    std::vector<uint8_t> headerBytes;
    std::vector<uint8_t> payloadBytes;
    std::vector<uint8_t> bytes;

    AuthReply()
        : valid(false),
          isErrorReply(false),
          hasAuthDataMarker(false),
          errorCode(0),
          zeroDword(0),
          trailingWord(0),
          offsetAuthData(0),
          offsetEncryptedData(0),
          unknown2(0),
          offsetCharData(0),
          unknown3(0),
          offsetServerData(0),
          offsetUsername(0),
          characterCount(0),
          worldCount(0),
          authDataMarker(0),
          encryptedPrivateExponentLength(0) {}
};

// String/diagnostic helper anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
// - exact original helper VAs for these tiny helpers: [not yet isolated]
const char* AuthOpcodeName(uint8_t rawCode);

std::string HexEncode(const uint8_t* data, size_t size);
std::string HexEncode(const std::vector<uint8_t>& bytes);
bool HexDecode(const std::string& text, std::vector<uint8_t>* outBytes);

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

bool ParseVariableLengthPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    FramedPacket* outPacket);

// Raw 0x06 / AS_GetPublicKeyRequest anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
// - strongest current original send-builder anchor:
//   - launcher.exe:0x447eb0
// - upstream branch/dispatcher anchor:
//   - launcher.exe:0x448050
bool BuildGetPublicKeyRequestPacket(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    FrameMode frameMode,
    FramedPacket* outPacket);

// Raw 0x07 / AS_GetPublicKeyReply parse anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
// - later consumer/owner-handling anchors:
//   - launcher.exe:0x41bc20
//   - launcher.exe:0x4401a0
// - exact original standalone parse helper VA for the 0x07 body: [not yet isolated]
bool ParseGetPublicKeyReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    GetPublicKeyReply* outReply);

bool ParseGetPublicKeyReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    GetPublicKeyReply* outReply);

// Raw 0x08 / AS_AuthRequest build anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
// - strongest current original send-builder anchor:
//   - launcher.exe:0x4474f0
// - branch/dispatch anchor selecting raw 0x06 vs 0x08 path:
//   - launcher.exe:0x448050
// - later material/current-key continuation anchor:
//   - launcher.exe:0x429b0
bool BuildAuthRequestBlobPlaintext(
    const std::string& username,
    const AuthBlobLayout& layout,
    std::vector<uint8_t>* outPlaintext,
    std::vector<uint8_t>* outTwofishKey,
    uint16_t* outUsernameLengthField);

bool EncryptAuthRequestBlob(
    const std::vector<uint8_t>& plaintext,
    std::vector<uint8_t>* outCiphertext);

bool EncryptAuthRequestBlobWithKeyMaterial(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& modulusBytes,
    const std::vector<uint8_t>& exponentBytes,
    std::vector<uint8_t>* outCiphertext);

bool BuildAuthRequestPacket(
    const std::string& username,
    const AuthBlobLayout& blobLayout,
    const AuthRequestLayout& requestLayout,
    FrameMode frameMode,
    AuthRequestBuildResult* outResult);

// Raw 0x09 / AS_AuthChallenge parse anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
// - exact original standalone 0x09 parser VA: [not yet isolated]
bool ParseAuthChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthChallenge* outChallenge);

bool ParseAuthChallengePacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthChallenge* outChallenge);

// Raw 0x0a / AS_AuthChallengeResponse build anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
// - later challenge/material continuation anchor:
//   - launcher.exe:0x429b0
// - exact original raw 0x0a builder/send VA: [not yet isolated]
bool BuildAuthChallengeResponsePacket(
    const std::vector<uint8_t>& encryptedChallengeBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::string& password,
    const std::string& soePassword,
    const AuthChallengeResponseLayout& layout,
    FrameMode frameMode,
    AuthChallengeResponseBuildResult* outResult);

// Raw 0x0b / AS_AuthReply parse anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\filters.cpp`
// - later owner-side handler:
//   - launcher.exe:0x4401a0
// - current concrete auth-reply parse/helper object builder:
//   - launcher.exe:0x43a330
bool ParseAuthReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthReply* outReply);

bool ParseAuthReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthReply* outReply);

// Auth-reply private-exponent decrypt anchors:
// - source file anchor:
//   `\matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp`
// - exact original decrypt/helper VA: [not yet isolated]
bool DecryptAuthReplyPrivateExponent(
    const AuthReply& reply,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::vector<uint8_t>& challengeIvBytes,
    std::vector<uint8_t>* outPrivateExponentBytes);

// Compatibility wrappers used by earlier diagnostics / tooling.
// These are explicitly transitional convenience helpers, not a claim of original source-level
// API fidelity.
std::vector<uint8_t> BuildAuthRequestBlob(
    const char* username,
    uint32_t embeddedTime,
    uint32_t rsaMethod = 4,
    uint16_t someShort = 0x1b);

std::vector<uint8_t> BuildAuthRequestBlobEx(
    const char* username,
    uint32_t rsaMethod = 4,
    uint16_t someShort = 0x1b,
    const uint8_t* twofishKey = nullptr);

std::string BuildAuthRequestBlobHex(
    const char* username,
    uint32_t rsaMethod = 4,
    uint16_t someShort = 0x1b);

}  // namespace auth
}  // namespace mxo
