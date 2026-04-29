#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rsa.h"

#include "../../../runtime/src/libltcrypto/auth_crypto.h"

namespace mxo::ltlogin {

struct AuthBootstrap680RsaPublicKeyPairOwnedState;
struct AuthBootstrap680ReplyAuthDataValidatorACSketch;

struct AuthBootstrap680SmallStringMirror {
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
    std::string owned;
};

class __attribute__((packed)) AuthBootstrapReplyCopyShadowF4_0x44add0 {
public:
    std::array<uint8_t, 0x80> authSignature00{};
    std::array<uint8_t, 0xb6> signedData80{};

    void BuildSignedDataMd5Digest(std::array<uint8_t, 16>* outDigest) const;
    bool IsFresh(int timeBias) const;
    uint32_t VerifyWithValidator(
        AuthBootstrap680ReplyAuthDataValidatorACSketch* validator,
        const AuthBootstrap680RsaPublicKeyPairOwnedState& publicKeyPair,
        int timeBias) const;
};

struct AuthBootstrap680BigIntObjects_0x4ba50c {
    uint32_t vtable00 = 0u;
    uint32_t reserved04 = 0u;
    uint32_t capacityWords08 = 0u;
    void* digits0c = nullptr;
    uint32_t sign10 = 0u;
};
static_assert(sizeof(AuthBootstrap680BigIntObjects_0x4ba50c) == 0x14u);


// Recovered raw `0x08` helper stack note:
// - `launcher.exe:0x438120` = `CryptoPP_PK_DefaultEncryptionFilter_ctor`
// - `launcher.exe:0x438320` = `CryptoPP_PK_DefaultEncryptionFilter_Put2`
// - `launcher.exe:0x4382c0` = `CryptoPP_PK_Encryptor_CreateEncryptionFilter`
//
// Source no longer keeps a dedicated `AuthBootstrap680Raw08PerChunkWorker48Sketch` because the
// auth path only needs the inner `PK_DefaultEncryptionFilter` semantics, not the exact launcher
// helper object layout. The remaining recovered raw-0x08 orchestration stays on the outer
// `RSAES_OAEP_SHA_Encryptor` family helpers in `authbootstrap680.cpp`.

struct AuthBootstrap680Raw08PublicKeyWorkerA8Sketch {
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t helperVtable08 = 0u;
    CryptoPP::RSA::PublicKey publicKey{};
    uint32_t helperThunk4c = 0u;
    uint32_t helperThunk50 = 0u;
    uint32_t helperThunk54 = 0u;
    uint32_t helperVtable58 = 0u;

    void ResetAsRecoveredLeaf(AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState);
    bool ConstructFromReplyPublicKey(
        AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
        const uint8_t* modulusBytes,
        size_t modulusByteCount,
        uint8_t exponentByte);
    uint32_t QueryEncryptedOutputLengthScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
        size_t plaintextByteCount) const;
    uint32_t QueryCiphertextChunkByteCountScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) const;
    uint32_t QueryPlaintextChunkByteCountScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState) const;
    bool EncryptPlaintextChunkScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
        const uint8_t* plaintextBytes,
        size_t plaintextByteCount,
        uint8_t* ciphertextBytes,
        size_t ciphertextByteCapacity) const;
};

// Recovered validator accumulator note:
// - `launcher.exe:0x4472f0` = `CryptoPP_PK_MessageAccumulatorMD5_Create`
// - `launcher.exe:0x447390` = `CryptoPP_PK_MessageAccumulatorBase_Construct`
// - `launcher.exe:0x447340` = `CryptoPP_PK_MessageAccumulatorBase_Update`
// - `launcher.exe:0x447380` = `CryptoPP_PK_MessageAccumulatorMD5_AccessHash`
// - `launcher.exe:0x468520` loads the RSA-decoded signature bytes into the accumulator state
// - `launcher.exe:0x467ee0` / `0x467f70` finalize against the outer verifier object
//
// Source no longer keeps a fake `0x84` launcher-owned worker layout. The active auth path only
// needs the recovered boundary split between:
// - the outer `RSASSA_PKCS1v15_MD5_Verifier`-compatible object
// - the temporary MD5-backed PK_MessageAccumulator used during verify

struct AuthBootstrap680ReplyAuthDataValidatorACSketch {
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t helperVtable08 = 0u;
    CryptoPP::RSA::PublicKey publicKey{};
    uint32_t helperThunk4c = 0u;
    uint32_t helperThunk50 = 0u;

    void ResetAsRecoveredLeaf(AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState);
    bool ConstructFromReplyPublicKey(
        AuthBootstrap680RsaPublicKeyPairOwnedState* ownedState,
        const uint8_t* modulusBytes,
        size_t modulusByteCount,
        uint8_t exponentByte);
    bool VerifySignatureRecoveredFinalizeScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
        const uint8_t* signedBytes,
        size_t signedByteCount,
        const uint8_t* signatureBytes,
        size_t signatureByteCount) const;
};

using AuthBootstrap680LazyPubkeyDatValidatorA4Sketch =
    AuthBootstrap680ReplyAuthDataValidatorACSketch;

// Launcher-owned wrapper around embedded Crypto++ RNG / BufferedTransformation slices.
// Final ctor state from `launcher.exe:0x4686e0` is:
// - `+0x00 = 0x004b695c` launcher wrapper vtable
// - `+0x04 = 0x004b68a8` old `CryptoPP::RandomPool` / modern `OldRandomPool`
// - `+0x08 = 0x004b41e0` `CryptoPP::BufferedTransformation`
struct AuthBootstrap680Field54HelperSketch {
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t helperVtable08 = 0u;
    uint32_t reserved0c = 0u;
    uint32_t bufferedOutputByteCount10 = 0u;
    uint8_t* bufferedOutputBytes14 = nullptr;
    uint32_t reserved18 = 0u;
    uint32_t scratchPrefixByteCount1c = 0u;
    uint8_t* scratchPrefixBytes20 = nullptr;
    uint32_t bufferedStreamState24 = 0u;
    uint32_t nextBufferedOutputByte28 = 0u;
};
static_assert(sizeof(AuthBootstrap680Field54HelperSketch) == 0x2cu);

struct AuthBootstrap680AuthReplyParseAccessor10Sketch {
    uint32_t vtable00 = 0u;
    const uint8_t* packetBody04 = nullptr;
    void* incomingMessage08 = nullptr;
    uint8_t resolveFields0c = 0u;
    std::array<uint8_t, 3> padding0d{};
};
static_assert(sizeof(AuthBootstrap680AuthReplyParseAccessor10Sketch) == 0x10u);

struct AuthBootstrap680AuthReplyParseObjectF0Sketch {
    uint32_t vtable00 = 0u;
    const uint8_t* packetBody04 = nullptr;
    void* incomingMessage08 = nullptr;
    uint8_t resolveFields0c = 0u;
    std::array<uint8_t, 3> padding0d{};
    const uint8_t* replyHeader10 = nullptr;
    const uint8_t* stringField05Bytes14 = nullptr;
    uint16_t stringField05Length18 = 0u;
    std::array<uint8_t, 2> padding1a{};
    const uint8_t* authDataBytes1c = nullptr;
    uint16_t authDataByteLength20 = 0u;
    std::array<uint8_t, 2> padding22{};
    const uint8_t* encryptedPrivateExponentBytes24 = nullptr;
    uint16_t encryptedPrivateExponentByteLength28 = 0u;
    std::array<uint8_t, 2> padding2a{};
    const uint8_t* opaqueField0fBytes2c = nullptr;
    uint16_t opaqueField0fByteLength30 = 0u;
    std::array<uint8_t, 2> padding32{};
    const uint8_t* opaqueField11Bytes34 = nullptr;
    uint16_t opaqueField11ByteLength38 = 0u;
    std::array<uint8_t, 2> padding3a{};
    const uint8_t* characterTempRecords3c = nullptr;
    uint16_t characterTempRecordCount40 = 0u;
    std::array<uint8_t, 2> padding42{};
    const uint8_t* worldTempRecords44 = nullptr;
    uint16_t worldTempRecordCount48 = 0u;
    std::array<uint8_t, 2> padding4a{};
    const uint8_t* opaqueField1bBytes4c = nullptr;
    uint16_t opaqueField1bByteLength50 = 0u;
    std::array<uint8_t, 2> padding52{};
    const uint8_t* replyString1dBytes54 = nullptr;
    uint16_t replyString1dByteLength58 = 0u;
    std::array<uint8_t, 2> padding5a{};
    AuthBootstrap680AuthReplyParseAccessor10Sketch worldDescriptorAccessor5c{};
    const uint8_t* currentWorldTempRecord6c = nullptr;
    AuthBootstrap680AuthReplyParseAccessor10Sketch slotRecordAccessor70{};
    const uint8_t* currentCharacterTempRecord80 = nullptr;
    const uint8_t* currentCharacterHandle84 = nullptr;
    uint16_t currentCharacterHandleByteLength88 = 0u;
    std::array<uint8_t, 2> padding8a{};
};
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, replyHeader10) == 0x10u);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, authDataBytes1c) == 0x1cu);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, encryptedPrivateExponentBytes24) == 0x24u);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, opaqueField0fBytes2c) == 0x2cu);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, opaqueField11Bytes34) == 0x34u);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, characterTempRecords3c) == 0x3cu);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, worldTempRecords44) == 0x44u);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, replyString1dBytes54) == 0x54u);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, worldDescriptorAccessor5c) == 0x5cu);
static_assert(offsetof(AuthBootstrap680AuthReplyParseObjectF0Sketch, slotRecordAccessor70) == 0x70u);
static_assert(sizeof(AuthBootstrap680AuthReplyParseObjectF0Sketch) == 0x8cu);

}  // namespace mxo::ltlogin
