#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"

namespace mxo::liblttcp {
class CMarginConnection_0x4aff38;
}

namespace mxo::auth::internal {
class FeedbackSizeTransformAdapterLarge;
class FeedbackSizeTransformAdapterSmall;
}

namespace mxo::ltlogin {

struct AuthBootstrap680ReplyAuthDataValidatorACSketch;

class CLTLoginMediator;
struct AuthBootstrap680RsaPublicKeyPairOwnedState;

struct AuthBootstrap680SmallStringMirror {
    // Source-owned mirror of the three-dword small-string family populated by `0x407dd0`.
    // The `owned` backing string is replacement-only storage; the original object only exposes
    // the first three pointer fields.
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
    std::string owned;
};

class __attribute__((packed)) AuthBootstrapReplyCopyShadowF4_0x44add0 {
public:
    // Source-owned shadow of the original reply-derived `0x136` heap block copied into child
    // `+0xf4` by `0x448140`.
    //
    // Unpacked auth block layout (310 bytes / 0x136):
    // - +0x00 .. +0x7f = 128-byte auth-signature span
    // - +0x80 .. +0x135 = signed-data span (0xb6 bytes)
    // Fields of interest within signedData80:
    // - +0xa8 = signedData80[0x28] (BootstrapRaw08AuxHandle)
    // - +0xac = signedData80[0x2c] (expiry time)
    // - +0xd1 = signedData80[0x51] (low public-exponent byte)
    // - +0xd2 .. +0x131 = signedData80[0x52..0xb1] (modulus bytes)
    std::array<uint8_t, 0x80> authSignature00{};
    std::array<uint8_t, 0xb6> signedData80{};

    // anchor: launcher.exe:0x44ae40
    void BuildSignedDataMd5Digest(std::array<uint8_t, 16>* outDigest) const;

    // anchor: launcher.exe:0x44add0
    bool IsFresh(int timeBias) const;
    // anchor: launcher.exe:0x44aec0
    uint32_t VerifyWithValidator(
        AuthBootstrap680ReplyAuthDataValidatorACSketch* validator,
        const AuthBootstrap680RsaPublicKeyPairOwnedState& publicKeyPair,
        int timeBias) const;
};

struct AuthBootstrap680BigIntObjects_0x4ba50c {
    // Exact `0x14`-byte big-int object family initialized by `0x45d000` and copied by `0x45de10`.
    // Current field certainty:
    // - `+0x00` = vtable `0x004ba50c`
    // - `+0x08` = allocated word capacity
    // - `+0x0c` = digits pointer
    // - `+0x10` = sign / trailing state copied verbatim by `0x45de10`
    // Keep `+0x04` reserved until a stronger semantic name is proven.
    uint32_t vtable00 = 0u;
    uint32_t reserved04 = 0u;
    uint32_t capacityWords08 = 0u;
    void* digits0c = nullptr;
    uint32_t sign10 = 0u;
};
static_assert(sizeof(AuthBootstrap680BigIntObjects_0x4ba50c) == 0x14u);

struct AuthBootstrap680RsaPublicKeyPairSubobject0cSketch {
    // Common `0x40`-byte subobject constructed at worker `+0x0c` by `0x4420f0` inside both the
    // raw-`0x08` worker and the validator families.
    //
    // Strong current field map from `0x4420f0 / 0x447120 / 0x447020`:
    // - `+0x08` = reply/public-key modulus big-int object copied from constructor arg1
    // - `+0x1c` = reply/public-key exponent big-int object copied from constructor arg2
    // - both outer worker constructors therefore store the parsed raw-`0x07` public key at outer
    //   `+0x14` and `+0x28`
    // - `0x41f090` returns outer `this+0x0c`, and
    //   `0x468ea0 = AuthBootstrap680Raw08PublicKeyWorker_QueryEncryptedOutputLength` then reads
    //   the embedded modulus object through this subobject `+0x08`
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    AuthBootstrap680BigIntObjects_0x4ba50c modulus08{};
    AuthBootstrap680BigIntObjects_0x4ba50c exponent1c{};
    uint32_t helperThunk30 = 0u;
    uint32_t helperThunk34 = 0u;
    uint32_t helperThunk38 = 0u;
    uint32_t helperVtable3c = 0u;
};
static_assert(offsetof(AuthBootstrap680RsaPublicKeyPairSubobject0cSketch, modulus08) == 0x08u);
static_assert(offsetof(AuthBootstrap680RsaPublicKeyPairSubobject0cSketch, exponent1c) == 0x1cu);
static_assert(sizeof(AuthBootstrap680RsaPublicKeyPairSubobject0cSketch) == 0x40u);

struct AuthBootstrap680Raw08PerChunkNodeBufferHelper1cSketch {
    // Constructor-built `0x1c` helper rooted at per-chunk worker `+0x14` by `0x454f10(..., 0x100)`.
    // Current bounded map from `0x454f10 / 0x454ff0 / 0x455520 / 0x455560`:
    // - final ctor-time vtable `0x004ba110`
    // - `+0x08` = ctor-requested node size (`0x100` in the raw-`0x08` path)
    // - `+0x0c/+0x10` = duplicated heap-node pointer seeded by `0x454f10`
    // - `+0x18` = pending-byte count / tail state drained by `0x454ff0`
    // - sibling virtuals query strings `"NodeSize"` and `"OutputBuffer"`, so this helper sits
    //   in a node-buffer / output-buffer adapter family rather than being a plain inline scratch
    //   byte array
    // Keep the untouched dword at `+0x14` reserved until the node object at `+0x0c/+0x10` is
    // typed more tightly.
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t nodeSize08 = 0u;
    void* heapNode0c = nullptr;
    void* heapNode10 = nullptr;
    uint32_t reserved14 = 0u;
    uint32_t pendingByteCount18 = 0u;
};
static_assert(sizeof(AuthBootstrap680Raw08PerChunkNodeBufferHelper1cSketch) == 0x1cu);

struct AuthBootstrap680Raw08PerChunkWorker48Sketch {
    // Temporary `0x48`-byte helper allocated by
    // `0x4382c0 = AuthBootstrap680Raw08PublicKeyWorker_CreatePerChunkWorker` and consumed by
    // `0x468280 = AuthBootstrap680Raw08PublicKeyWorker_EncryptChunkIntoCiphertext`.
    //
    // Stronger current bounded map from `0x438120 / 0x4382c0 / 0x468280` plus the helper vtable:
    // - final vtable `0x004b4478`
    // - `0x438120` first runs `0x453570(this, param3)`
    //   - that seeds the inherited helper front matter and stores ctor arg3 at `+0x08`
    // - `+0x14` = constructed node-buffer/output-buffer helper subobject above
    //   from `0x454f10(this+0x14, 0x100)`
    // - `+0x30` = ctor arg1 copied verbatim by `0x438120`
    // - `+0x34` = ctor arg2 / outer raw-`0x08` worker pointer copied verbatim by `0x438120`
    // - `+0x40/+0x44` = ctor-zeroed tail dwords
    // - active encrypt path `0x468280` does not read `+0x30/+0x34/+0x40/+0x44`; current evidence
    //   says those belong to sibling virtuals on the same helper family, not to the hot chunk body
    // - helper vtable `+0x1c / 0x437870` releases the constructed `+0x14` subobject through
    //   `0x455470`
    //
    // `0x468280` proves this helper is part of the exact launcher encrypt path, but its inherited
    // filter/transform family is still too loose to claim a faithful source reimplementation.
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    std::array<uint8_t, 0x0c> reserved08To13{};
    AuthBootstrap680Raw08PerChunkNodeBufferHelper1cSketch nodeBufferHelper14{};
    uint32_t ctorArg30 = 0u;
    void* ctorOwnerWorker34 = nullptr;
    uint32_t reserved38 = 0u;
    uint32_t reserved3c = 0u;
    uint32_t ctorZeroTail40 = 0u;
    uint32_t ctorZeroTail44 = 0u;
};
static_assert(offsetof(AuthBootstrap680Raw08PerChunkWorker48Sketch, nodeBufferHelper14) == 0x14u);
static_assert(offsetof(AuthBootstrap680Raw08PerChunkWorker48Sketch, ctorArg30) == 0x30u);
static_assert(offsetof(AuthBootstrap680Raw08PerChunkWorker48Sketch, ctorOwnerWorker34) == 0x34u);
static_assert(offsetof(AuthBootstrap680Raw08PerChunkWorker48Sketch, ctorZeroTail40) == 0x40u);
static_assert(sizeof(AuthBootstrap680Raw08PerChunkWorker48Sketch) == 0x48u);

struct AuthBootstrap680Raw08PublicKeyWorkerA8Sketch {
    // Concrete `+0xa8` worker family rebuilt by `0x447780` through:
    // - pool `0x466580` (`size = 0x5c`)
    // - ctor `0x447120 = AuthBootstrap680Raw08PublicKeyWorker_ConstructFromReplyPublicKey`
    // - raw send-time consumers:
    //   - `0x468ea0 = AuthBootstrap680Raw08PublicKeyWorker_QueryEncryptedOutputLength`
    //   - `0x468f00 = AuthBootstrap680Raw08PublicKeyWorker_EncryptIntoPacketBuilder`
    //
    // Current concrete layout certainty:
    // - final vtable `0x004b75e4`
    // - `+0x0c` = common RSA public-key pair subobject above
    // - vtable `+0x1c` = per-chunk encrypt step used from `0x468f00`
    //   - disassembly proves `0x468f00` pushes four stack args here and `0x468280` returns
    //     with `ret 0x10`; the decompiler undercounts this unless checked against assembly
    // - vtable `+0x20` = allocate the temporary `0x48` helper above
    // - vtable `+0x24` = ciphertext-block/modulus query consumed by `0x468ea0/0x468f00`
    //   - `0x468ea0` then runs the modulus object through `0x45a400`, so the ciphertext block
    //     byte count is derived from the RSA modulus bit length rather than from a generic
    //     encryptor-side constant
    // - vtable `+0x28` = same tiny getter reused again
    // - `0x468f00` itself loops over plaintext chunks and advances the packet-builder output by one
    //   ciphertext block per iteration
    // - `+0x4c/+0x50/+0x54/+0x58` = ctor-seeded helper/vtable family still kept raw pending deeper
    //   recovery of the exact encryption-worker inheritance stack
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t helperVtable08 = 0u;
    AuthBootstrap680RsaPublicKeyPairSubobject0cSketch publicKeyPair0c{};
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
    bool EncryptPlaintextIntoCiphertextScaffold(
        const AuthBootstrap680RsaPublicKeyPairOwnedState& ownedState,
        const uint8_t* plaintextBytes,
        size_t plaintextByteCount,
        std::vector<uint8_t>* outCiphertextBytes) const;
};
static_assert(offsetof(AuthBootstrap680Raw08PublicKeyWorkerA8Sketch, publicKeyPair0c) == 0x0cu);
static_assert(sizeof(AuthBootstrap680Raw08PublicKeyWorkerA8Sketch) == 0x5cu);

struct AuthBootstrap680ValidatorTemporaryWorker84Sketch {
    // Temporary validator worker allocated by `0x4472f0` through pool `0x4665b0` (`size = 0x84`)
    // and returned from validator vtable `0x004b7580 +0x1c`.
    //
    // Strong current inner-worker map from `0x4472f0 / 0x447390 / 0x468520 / 0x447340`:
    // - final vtable `0x004b7668`
    // - ctor-stage vtable `0x004b76b0`
    // - `+0x14/+0x18` = decoded-signature representative byte count / pointer written by
    //   `0x468520` after the outer validator applies its RSA public-key path to the signature
    // - `+0x34` and `+0x48` = adjacent `0x14`-byte big-int objects seeded by `0x45d000`
    // - `+0x5c` = one-byte ready/empty-update flag toggled by `0x447340`
    // - `+0x60` = MD5 accumulator object initialized by `0x43d410`; worker vtable `+0x44`
    //   returns `this+0x60`
    //
    // Keep the untouched front-matter dwords reserved until the remaining inherited helper
    // families on `0x004b7668 / 0x004b76b0` are named more tightly.
    uint32_t vtable00 = 0u;
    uint32_t reserved04 = 0u;
    uint32_t reserved08 = 0u;
    uint32_t reserved0c = 0u;
    uint32_t reserved10 = 0u;
    uint32_t decodedSignatureByteCount14 = 0u;
    void* decodedSignatureBytes18 = nullptr;
    uint32_t reserved1c = 0u;
    uint32_t reserved20 = 0u;
    uint32_t reserved24 = 0u;
    uint32_t reserved28 = 0u;
    uint32_t reserved2c = 0u;
    uint32_t reserved30 = 0u;
    AuthBootstrap680BigIntObjects_0x4ba50c bigInt34{};
    AuthBootstrap680BigIntObjects_0x4ba50c bigInt48{};
    uint8_t readyOrEmptyUpdateFlag5c = 0u;
    std::array<uint8_t, 3> padding5d{};
    std::array<uint8_t, 0x24> md5Accumulator60{};
};
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, decodedSignatureByteCount14) == 0x14u);
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, decodedSignatureBytes18) == 0x18u);
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, bigInt34) == 0x34u);
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, bigInt48) == 0x48u);
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, readyOrEmptyUpdateFlag5c) == 0x5cu);
static_assert(offsetof(AuthBootstrap680ValidatorTemporaryWorker84Sketch, md5Accumulator60) == 0x60u);
static_assert(sizeof(AuthBootstrap680ValidatorTemporaryWorker84Sketch) == 0x84u);

struct AuthBootstrap680ReplyAuthDataValidatorACSketch {
    // Concrete validator family used at child `+0xa4` and `+0xac`:
    // - `0x447260` allocates this family lazily for the `qspubkey.dat` validator at child `+0xa4`
    // - `0x447780` allocates the same family again for child `+0xac`
    // - pool `0x4665a0` (`size = 0x54`)
    // - ctor `0x447020 = AuthBootstrap680ReplyAuthDataValidator_ConstructFromReplyPublicKey`
    // - high-value vtable chain now closed from `0x468f80 / 0x44aec0`:
    //   - `+0x1c` = allocate/return the `0x84` temporary validator worker above
    //   - `+0x20` = load signature bytes into that worker (`0x468520`)
    //   - `+0x28` = finalize verification on the temporary worker
    //   - `+0x2c` = convenience wrapper that performs allocate/load/finalize around caller bytes
    //
    // Current concrete layout certainty:
    // - final vtable `0x004b7580`
    // - `+0x0c` = same RSA public-key pair subobject used by the raw-`0x08` worker
    // - helper family now points much more specifically at RSA/EMSA-PKCS1-v1_5(MD5):
    //   - `0x446f30` builds the algorithm string `RSA/EMSA-PKCS1-v1_5(MD5)`
    //   - `0x445410` returns the 18-byte MD5 `DigestInfo` prefix used by the finalize path
    // - `+0x4c/+0x50` = ctor-seeded helper/vtable family still kept raw pending the last naming
    //   pass over the inherited validator stack
    uint32_t vtable00 = 0u;
    uint32_t helperVtable04 = 0u;
    uint32_t helperVtable08 = 0u;
    AuthBootstrap680RsaPublicKeyPairSubobject0cSketch publicKeyPair0c{};
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
static_assert(offsetof(AuthBootstrap680ReplyAuthDataValidatorACSketch, publicKeyPair0c) == 0x0cu);
static_assert(sizeof(AuthBootstrap680ReplyAuthDataValidatorACSketch) == 0x54u);

using AuthBootstrap680LazyPubkeyDatValidatorA4Sketch =
    AuthBootstrap680ReplyAuthDataValidatorACSketch;

struct AuthBootstrap680Field54HelperSketch {
    // Concrete helper subobject rooted at child `+0x54`.
    // High-value anchors:
    // - `0x445500` seeds `0x004b695c / 0x004b68a8 / 0x004b41e0`
    // - `0x4686e0(this+0x54, 0x180)` builds the buffered helper body
    // - `0x468dc0(this+0x54, 0, 0x20)` applies the initial `0x20`-byte setup
    // - vtable `+0x18 / 0x468640 = AuthBootstrap680Field54Helper_FillBytes` fills caller bytes;
    //   `0x4474f0` asks for `0x10`
    //   and writes the result into child `+0x84 .. +0x93`
    // - the same helper family is reused by `0x44d250` for transform-worker associated seeds
    // Current bounded field map from `0x4686e0 / 0x468b60 / 0x468d30`:
    // - `+0x0c` = reserved / still-untyped dword between the front-matter vtable family and the
    //             buffered-output body
    // - `+0x10/+0x14` = buffered output window byte count / pointer (`0x180` bytes)
    // - `+0x18` = reserved / still-untyped dword between the two buffer families
    // - `+0x1c/+0x20` = scratch-prefix byte count / pointer (`0x40` bytes)
    // - `+0x24/+0x28` = internal buffered-stream state used by the refill/read helpers
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
    // Common packet-bound `0x10` accessor family used inside the copied `+0xf0` auth-reply parse
    // object.
    // - `0x4399e0 = AuthBootstrap680AuthReplyWorldDescriptorAccessor_InitFromIncomingMessage`
    // - `0x444300 = AuthBootstrap680AuthReplySlotRecordAccessor_InitFromIncomingMessage`
    // Those helpers show the same front matter for the world-descriptor and slot-record accessors
    // rooted at parse-copy `+0x5c` and `+0x70`.
    uint32_t vtable00 = 0u;
    const uint8_t* packetBody04 = nullptr;
    void* incomingMessage08 = nullptr;
    uint8_t resolveFields0c = 0u;
    std::array<uint8_t, 3> padding0d{};
};
static_assert(sizeof(AuthBootstrap680AuthReplyParseAccessor10Sketch) == 0x10u);

struct AuthBootstrap680AuthReplyParseObjectF0Sketch {
    // Source-owned mirror of the copied `0x8c` auth-reply parse object stored at child `+0xf0` by
    // `0x448140`, copied through `0x4449c0`, and later released by `0x444900`.
    //
    // Strong current field map from `0x444390 / 0x443470 / 0x4449c0 / 0x43f300`, plus
    // `0x438990 = AuthBootstrap680AuthReplyParseObject_SelectCharacterTempRecordByIndex` for the
    // later character-temp-record cursor at `+0x80/+0x84/+0x88`:
    // - `+0x10` = resolved reply-header/body base; later fixed-field consumers read
    //   `replyHeader10 + 0x07` and `replyHeader10 + 0x15`
    // - variable field pairs are length-prefixed packet-body views keyed by raw reply-header
    //   offsets:
    //   - `+0x14/+0x18` <- header offset `+0x05` string-like field
    //   - `+0x1c/+0x20` <- header offset `+0x0b` auth-data copy-shadow bytes
    //   - `+0x24/+0x28` <- header offset `+0x0d` encrypted private-exponent bytes
    //   - `+0x2c/+0x30` <- header offset `+0x0f` opaque blob copied later to child `+0x108`
    //   - `+0x34/+0x38` <- header offset `+0x11` opaque blob copied later to child `+0x10c`
    //   - `+0x3c/+0x40` <- header offset `+0x13` character temp-record base/count
    //   - `+0x44/+0x48` <- header offset `+0x19` world temp-record base/count
    //   - `+0x4c/+0x50` <- header offset `+0x1b` another opaque blob family
    //   - `+0x54/+0x58` <- header offset `+0x1d` trailing reply string copied by
    //                     `0x43d480 = AuthBootstrap680_CopyReplyString54`
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

// anchor: launcher.exe:0x4b7134 / vtable 0x004b7134
// Base class containing fields +0x00 through +0xf4
// Constructor: launcher.exe:0x445500 = AuthBootstrap680ChildBase_0x4b7134::ctor
// Destructor: launcher.exe:0x445610 = AuthBootstrap680ChildBase_0x4b7134::dtor
class AuthBootstrap680ChildBase_0x4b7134 {
public:
 // VTable at 0x004b7134:
 // +0x00: destructor (0x00445610)
 // +0x04..+0x44: virtual methods
 // +0x48..+0x4b: null terminator (0x004b7194-0x004b7197)
 // +0x4c..+0x50: additional vmethods including ~dtor wrapper (0x004469a0)
 // +0x54..+0x68: more vmethods (0x00446f10, 0x00443830, 0x00443850, etc.)

 // anchor: launcher.exe:0x41b160 = owner init path
 // anchor: launcher.exe:0x445500 = base ctor
 //
 // High-value child/module anchors:
 // - vtable `0x004b7134`
 // - ready-side dispatcher `0x448050`
 // - raw `0x06` builder `0x447eb0`
 // - raw `0x08` builder `0x4474f0`
 // - auth-reply copy/materializer `0x448140`

 // Direct `0x439210 -> 0x448050` staging writes now closed concretely enough to keep the
 // destination mapping inline here:
 // - child `+0x04` <- owner `+0x94 + 0x00` inline string
 // - child `+0x10` <- owner `+0x94 + 0x20` inline string
 // - child `+0x1c` <- owner `+0x94 + 0x60` small-string begin/data pointer (NULL becomes "")
 // - child `+0x28` <- ready-branch immediate `1`
 // - child `+0x2c` <- first dword from the owner-side getter reached through `0x439210`
 // - child `+0x30 .. +0x3f` <- owner `+0x94 + 0x40 .. + 0x4f`
 // - child `+0x40 .. +0x4f` <- owner `+0x94 + 0x50 .. + 0x5f`
 // - child `+0x50` <- owner-side send target result returned to `0x439210`
 AuthBootstrap680SmallStringMirror string04; // original child `+0x04`
 AuthBootstrap680SmallStringMirror string10; // original child `+0x10`
 AuthBootstrap680SmallStringMirror string1C; // original child `+0x1c`
 uint32_t loginType28 = 0; // original child `+0x28`; current ready-branch call shape pushes immediate `1`; `0x4474f0` later uses the low byte as raw `0x08` loginType
 uint32_t launcherVersion2C = 0; // original child `+0x2c`; `0x447eb0` uses it in raw `0x06`
 std::array<uint8_t, 16> block30{}; // original child `+0x30 .. +0x3f`
 std::array<uint8_t, 16> block40{}; // original child `+0x40 .. +0x4f`
 void* sendTarget50 = nullptr; // original child `+0x50`; indirect sender target consumed by `0x447eb0/0x4474f0`

 // Original child `+0x54 .. +0x7f` is the concrete helper subobject above, not an opaque gap.
 // `0x4474f0` calls helper vtable `+0x18 / 0x468640` here and copies the returned `0x10` bytes
 // into child `+0x84 .. +0x93`.
 AuthBootstrap680Field54HelperSketch feedbackSeedHelper54{}; // original child `+0x54 .. +0x7f`

 uint32_t authServerTimeBias80 = 0; // original child `+0x80`; `0x448140` stores `time(NULL) - GetPublicKeyReply.currentTime`, and later `0x4474f0` / `AuthBootstrapReplyCopyShadowF4_IsFresh` / `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` use it to reconstruct current auth-server time
 std::array<uint8_t, 16> feedbackSeed84{}; // original child `+0x84 .. +0x93`; helper-generated seed block from child `+0x54`, later reused by the `0x4474f0` transform-worker setup
 mxo::auth::internal::FeedbackSizeTransformAdapterLarge* feedbackTransformLarge94 = nullptr; // original child `+0x94`; allocated in `0x4474f0` through `FeedbackSizeTransformAdapter_ConstructLarge`
 mxo::auth::internal::FeedbackSizeTransformAdapterSmall* feedbackTransformSmall98 = nullptr; // original child `+0x98`; allocated in `0x4474f0` through `FeedbackSizeTransformAdapter_ConstructSmall`
 uint32_t currentPublicKeyId9C = 0; // original child `+0x9c`
 uint8_t authRequestReadyA0 = 0; // original child `+0xa0`; `0x447f50` sets this byte and `0x448050` branches on it before choosing raw `0x06` vs raw `0x08`
 std::array<uint8_t, 3> paddingA1ToA3{}; // original child `+0xa1 .. +0xa3`
 AuthBootstrap680LazyPubkeyDatValidatorA4Sketch* lazyPubkeyDatValidatorA4 = nullptr; // original child `+0xa4`; lazy `qspubkey.dat` validator family built by `0x447260/0x447c10` and consulted by `0x447780 -> 0x468f80`
 AuthBootstrap680Raw08PublicKeyWorkerA8Sketch* raw08PublicKeyWorkerA8 = nullptr; // original child `+0xa8`; live reply-public-key worker materialized by `0x447f50 -> 0x447780` and consumed by `0x4474f0` through `0x468ea0/0x468f00`
 AuthBootstrap680ReplyAuthDataValidatorACSketch* replyAuthDataValidatorAC = nullptr; // original child `+0xac`; sibling validator materialized by `0x447f50 -> 0x447780` and consumed by `0x44aec0 = AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`

 // Original child `+0xb0 .. +0xeb` now keeps the narrower `0x448140` success-side prep family
 // explicit as the same three adjacent `0x14`-byte big-int objects the launcher ctor seeds with
 // `0x45d000` and the success path overwrites via `0x45de10`:
 // - `+0xb0` <- modulus bytes from copied `+0xf4 + 0xd2 .. + 0x131`
 // - `+0xc4` <- low public-exponent byte from copied `+0xf4 + 0xd1`
 // - `+0xd8` <- derived 96-byte private-exponent/transform output used by the same prep path
 AuthBootstrap680BigIntObjects_0x4ba50c modulusBigIntB0{}; // original child `+0xb0`
 AuthBootstrap680BigIntObjects_0x4ba50c publicExponentBigIntC4{}; // original child `+0xc4`
 AuthBootstrap680BigIntObjects_0x4ba50c privateExponentBigIntD8{}; // original child `+0xd8`

 uint32_t inboundAuthStatusEc = 1; // original child `+0xec`; seeded by `0x445500`, then overwritten by `0x448140` with inbound auth status/error state
 AuthBootstrap680AuthReplyParseObjectF0Sketch* authReplyParseObjectF0 = nullptr; // original child `+0xf0`; `0x448140` stores a copied `0x8c` auth-reply parse object here via `0x4449c0`, and `0x444900` later releases it
 AuthBootstrapReplyCopyShadowF4_0x44add0* authReplyCopyShadowF4 = nullptr; // original child `+0xf4`

 // anchor: launcher.exe:0x445500
 AuthBootstrap680ChildBase_0x4b7134();
 // anchor: launcher.exe:0x445610
 virtual ~AuthBootstrap680ChildBase_0x4b7134();

protected:
 // anchor: launcher.exe:0x444900 (called by base dtor)
 void ClearReplyParseAndCopyShadowFields();
};

// anchor: launcher.exe:0x441290 / vtable 0x004b7134 (shares base vtable)
// Derived class containing fields +0xf8 through +0x118
// Constructor: launcher.exe:0x441290 = AuthBootstrap680Child_0x441290::ctor (calls base ctor then handles +0xf8..+0x118)
// Destructor: launcher.exe:0x445a40 = AuthBootstrap680Child_0x441290::dtor (calls base dtor)
class AuthBootstrap680Child_0x441290 : public AuthBootstrap680ChildBase_0x4b7134 {
public:
 // Keep this source-owned mirror explicitly child-scoped:
 // - it is rooted at mediator owner `+0x680`
 // - it is not evidence that the original child type was literally named after the mediator
 // - state and owner code should call the child directly instead of routing through fake
 // mediator methods
    AuthBootstrap680SmallStringMirror stringF8;  // original child `+0xf8 .. +0x100`; `0x441330` writes the prompt-password small-string neighboring the `+0xf0` auth-reply parse/copy family, and owner vtable `+0x60 / 0x41f3c0` later surfaces its begin pointer
    uint8_t crashReporterPromptForSecurId104 = 1; // original child `+0x104`; sibling `0x441330` SecurID-tail flag surfaced by owner vtable `+0x58 / 0x41f390`
    std::array<uint8_t, 3> padding105{};         // original child `+0x105 .. +0x107`
    void* opaqueReplyBlob108 = nullptr;          // original child `+0x108`; `0x441170` copies parse-object field `+0x2c/+0x30` (raw reply-header offset `+0x0f`) from the copied `+0xf0` family
    void* opaqueReplyBlob10C = nullptr;          // original child `+0x10c`; `0x441170` copies parse-object field `+0x34/+0x38` (raw reply-header offset `+0x11`) from the copied `+0xf0` family
    uint32_t authReplySuccessHeaderDword07_110 = 0; // original child `+0x110`; `0x43f300` writes the fixed raw reply-header dword at `authReplyParseObjectF0->replyHeader10 + 0x07` before the one-time success gate
    uint32_t authReplySuccessField15_114 = 0; // original child `+0x114`; `0x441260` writes the fixed raw reply-header dword at `authReplyParseObjectF0->replyHeader10 + 0x15`
    uint32_t authReplySuccessField15Timestamp118 = 0; // original child `+0x118`; `0x441260` writes current time alongside `+0x114`

 // anchor: launcher.exe:0x441290 / 0x445500 (two-phase constructor)
 AuthBootstrap680Child_0x441290();
 // anchor: launcher.exe:0x445a40 (dtor in vtable), then base dtor at 0x445610
 ~AuthBootstrap680Child_0x441290() override;

 // anchor: launcher.exe:0x448050
    // Static RE mirrors original signature:
    //   void __thiscall AuthBootstrap680Child_0x441290::AuthBootstrap680_PrepareAndDispatch(
    //       AuthBootstrap680Child_0x441290 *this, char *pszUsername, char *pszPassword,
    //       undefined4 loginType, undefined4 launcherVersionOrDispatchValue,
    //       undefined4 *pKeyConfigMd5, undefined4 *pUiConfigMd5,
    //       undefined4 pSendTarget, char *pszStationOrFallback)
    // But source consolidates args to mirror caller-gathered call shape:
    //   PrepareAndDispatch(CLTLoginMediator& owner, void* sendTarget, const char* sessionTokenBegin)
    uint32_t PrepareAndDispatch(CLTLoginMediator& owner, void* sendTarget, const char* sessionTokenBegin);
    // anchor: launcher.exe:0x448140
    // Original call shape consumes the incoming auth-message object directly.
    // Ghidra currently recovers this method under namespace
    // `CStreamPacketEncryptionModuleWriteHelper_0x4b8690`; source still keeps the
    // concrete owner+0x680 implementation grouped under this child mirror.
    uint32_t HandleInboundAuthMessage(void* incomingAuthMessage, CLTLoginMediator& owner);

    void* BootstrapRaw08AuxHandle50() const;
    bool HasBootstrapRaw08AuxHandle54() const;
    uint8_t GetCrashReporterPromptForSecurId58() const;

    // anchor: launcher.exe:0x447eb0
    uint32_t SendGetPublicKeyRequest(CLTLoginMediator& owner);
    // anchor: launcher.exe:0x4474f0
    uint32_t SendAuthRequest(CLTLoginMediator& owner, const mxo::auth::GetPublicKeyReply& reply);
    // anchor: launcher.exe:0x447f50 / 0x447780 / 0x447260 / 0x447c10
    uint32_t HandleGetPublicKeyReply(CLTLoginMediator& owner, const mxo::auth::GetPublicKeyReply& reply);
    // Source-owned (NOT static-RE): inlined into HandleInboundAuthMessage case 0x09u at 0x44831c..0x448467
 uint32_t SendAuthChallengeResponse_SOURCEOWNED_NO_RE(CLTLoginMediator& owner, const mxo::auth::AuthChallenge& challenge);
};

class AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0 {
public:
    explicit AuthBootstrap680State5MarginConnectionPrepBridge_0x4435f0(AuthBootstrap680Child_0x441290& child)
        : child_(child) {}

    // anchor: launcher.exe:0x4435f0
    // Direct-call bridge over the owner `+0x680` child that first forwards child `+0xf4` into
    // connection vtable `+0x44 / 0x41ce80`, then materializes the separate connection `+0xa0`
    // prep object through standalone helper `0x443340`. The immediate state5 path stops there;
    // the first later original consumer of that stored `+0xa0` object is `0x4429b0`.
    void PrepareState5MarginConnectionCopySend(mxo::liblttcp::CMarginConnection_0x4aff38& marginConnection);

private:
    AuthBootstrap680Child_0x441290& child_;
};

enum AuthBootstrap680InboundAuthResult : uint32_t {
    kAuthBootstrap680InboundUnhandled = 0u,
    kAuthBootstrap680InboundHandledContinueWaiting = 1u,
    kAuthBootstrap680InboundAuthReplySuccess = 2u,
    kAuthBootstrap680InboundAuthReplyError = 3u,
    kAuthBootstrap680InboundGetPublicKeyReplyError = 4u,
    kAuthBootstrap680InboundGetPublicKeyWorkerError = 5u,
    kAuthBootstrap680InboundAuthReplyValidationError = 6u,
};

void AuthBootstrap680LogParsedAuthReply(
    const CLTLoginMediator& owner,
    const mxo::auth::AuthReply& reply);
void AuthBootstrap680MaterializeReplyCopyShadowScaffold(
    AuthBootstrap680Child_0x441290& child,
    CLTLoginMediator& owner,
    const mxo::auth::AuthReply& reply);
void AuthBootstrap680SyncState2AuthReplySuccessPregateScaffold(
    AuthBootstrap680Child_0x441290& child,
    CLTLoginMediator& owner,
    const mxo::auth::AuthReply& reply);
bool AuthBootstrap680ConsumeState2AuthReplySuccessOneTimeGateScaffold();
void AuthBootstrap680SyncState2AuthReplySuccessOneTime_Field114AndTimestamp(
    AuthBootstrap680Child_0x441290& child,
    const mxo::auth::AuthReply& reply);
void AuthBootstrap680SyncState2AuthReplySuccessOneTime_ReplyStringAndOpaqueBlobs(
    AuthBootstrap680Child_0x441290& child,
    CLTLoginMediator& owner,
    const mxo::auth::AuthReply& reply);

}  // namespace mxo::ltlogin
