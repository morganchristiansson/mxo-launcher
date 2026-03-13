#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mxo {
namespace auth {

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

const char* AuthOpcodeName(uint8_t rawCode);

std::string HexEncode(const uint8_t* data, size_t size);
std::string HexEncode(const std::vector<uint8_t>& bytes);
bool HexDecode(const std::string& text, std::vector<uint8_t>* outBytes);

bool BuildVariableLengthPacket(
    const uint8_t* payload,
    size_t payloadSize,
    FrameMode mode,
    FramedPacket* outPacket);

bool ParseVariableLengthPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    FramedPacket* outPacket);

bool BuildGetPublicKeyRequestPacket(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    FrameMode frameMode,
    FramedPacket* outPacket);

bool ParseGetPublicKeyReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    GetPublicKeyReply* outReply);

bool ParseGetPublicKeyReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    GetPublicKeyReply* outReply);

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

bool ParseAuthChallengePayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthChallenge* outChallenge);

bool ParseAuthChallengePacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthChallenge* outChallenge);

bool BuildAuthChallengeResponsePacket(
    const std::vector<uint8_t>& encryptedChallengeBytes,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::string& password,
    const std::string& soePassword,
    const AuthChallengeResponseLayout& layout,
    FrameMode frameMode,
    AuthChallengeResponseBuildResult* outResult);

bool ParseAuthReplyPayload(
    const uint8_t* payloadBytes,
    size_t payloadSize,
    AuthReply* outReply);

bool ParseAuthReplyPacket(
    const uint8_t* packetBytes,
    size_t packetSize,
    AuthReply* outReply);

bool DecryptAuthReplyPrivateExponent(
    const AuthReply& reply,
    const std::vector<uint8_t>& twofishKeyBytes,
    const std::vector<uint8_t>& challengeIvBytes,
    std::vector<uint8_t>* outPrivateExponentBytes);

// Compatibility wrappers used by earlier diagnostics / tooling.
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
