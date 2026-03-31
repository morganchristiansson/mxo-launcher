#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"

namespace mxo::liblttcp {
class CMarginConnection;
}

namespace mxo::ltlogin {

class CLTLoginMediator;

struct AuthBootstrap680SmallStringMirror {
    // Source-owned mirror of the three-dword small-string family populated by `0x407dd0`.
    // The `owned` backing string is replacement-only storage; the original object only exposes
    // the first three pointer fields.
    const char* begin = nullptr;
    const char* current = nullptr;
    const char* capacity = nullptr;
    std::string owned;
};

struct __attribute__((packed)) AuthBootstrapReplyCopyShadowF4Sketch {
    // Source-owned shadow of the original reply-derived `0x136` heap block copied into child
    // `+0xf4` by `0x448140`.
    //
    // Static `0x448140 = AuthBootstrap680_HandleInboundAuthMessage` /
    // `0x44add0 = AuthBootstrapReplyCopyShadowF4_IsFresh` /
    // `0x44aec0 = AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` now tighten this materially:
    // - this block is not `[u16 3][u16 0x0136] + tail`
    // - the copied `0x136` bytes line up directly as:
    //   - `+0x00 .. +0x7f` = 128-byte auth-signature span
    //   - `+0x80 .. +0x135` = signed-data span (`0xb6` bytes)
    // - high-value verified suffix offsets:
    //   - `+0x85` = signed-data `+0x05` (wrapper-facing `owner+0x680->+0xf4+0x85`)
    //   - `+0xa8` = signed-data `+0x28` (wrapper-facing `owner+0x680->+0xf4+0xa8`)
    //   - `+0xac` = signed-data expiry-time dword used by
    //               `AuthBootstrapReplyCopyShadowF4_IsFresh` /
    //               `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`
    //   - `+0xd1` = low public-exponent byte used by `0x448140`
    //   - `+0xd2 .. +0x131` = modulus bytes used by `0x448140`
    std::array<uint8_t, 0x80> authSignature00{};
    std::array<uint8_t, 0xb6> signedData80{};
};
static_assert(offsetof(AuthBootstrapReplyCopyShadowF4Sketch, authSignature00) == 0x00);
static_assert(offsetof(AuthBootstrapReplyCopyShadowF4Sketch, signedData80) == 0x80);
static_assert(sizeof(AuthBootstrapReplyCopyShadowF4Sketch) == 0x136);

struct AuthBootstrap680BigIntObject20Scaffold {
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
static_assert(sizeof(AuthBootstrap680BigIntObject20Scaffold) == 0x14u);

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

struct AuthBootstrap680ChildSketch {
    // Current best source-owned mirror of the separate phase-2 auth/bootstrap child allocated by:
    // - launcher.exe:0x41b160 = owner init path
    // - launcher.exe:0x441290 = child ctor used by that path
    // - launcher.exe:0x445500 = earlier base-ctor layer used by the same child
    //
    // High-value child/module anchors:
    // - vtable `0x004b7134`
    // - ready-side dispatcher `0x448050`
    // - raw `0x06` builder `0x447eb0`
    // - raw `0x08` builder `0x4474f0`
    // - auth-reply copy/materializer `0x448140`
    //
    // Keep this source-owned mirror explicitly child-scoped:
    // - it is rooted at mediator owner `+0x680`
    // - it is not evidence that the original child type was literally named after the mediator
    // - wrapper methods on `CLTLoginMediator` stay thin so callers do not need broad churn

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
    AuthBootstrap680SmallStringMirror string04;  // original child `+0x04`
    AuthBootstrap680SmallStringMirror string10;  // original child `+0x10`
    AuthBootstrap680SmallStringMirror string1C;  // original child `+0x1c`
    uint32_t loginType28 = 0;                    // original child `+0x28`; current ready-branch call shape pushes immediate `1`; `0x4474f0` later uses the low byte as raw `0x08` loginType
    uint32_t launcherVersion2C = 0;              // original child `+0x2c`; `0x447eb0` uses it in raw `0x06`
    std::array<uint8_t, 16> block30{};          // original child `+0x30 .. +0x3f`
    std::array<uint8_t, 16> block40{};          // original child `+0x40 .. +0x4f`
    void* sendTarget50 = nullptr;                // original child `+0x50`; indirect sender target consumed by `0x447eb0/0x4474f0`

    // Original child `+0x54 .. +0x7f` is the concrete helper subobject above, not an opaque gap.
    // `0x4474f0` calls helper vtable `+0x18 / 0x468640` here and copies the returned `0x10` bytes
    // into child `+0x84 .. +0x93`.
    AuthBootstrap680Field54HelperSketch feedbackSeedHelper54{}; // original child `+0x54 .. +0x7f`

    uint32_t authServerTimeBias80 = 0;           // original child `+0x80`; `0x448140` stores `time(NULL) - GetPublicKeyReply.currentTime`, and later `0x4474f0` / `AuthBootstrapReplyCopyShadowF4_IsFresh` / `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` use it to reconstruct current auth-server time
    std::array<uint8_t, 16> feedbackSeed84{}; // original child `+0x84 .. +0x93`; helper-generated seed block from child `+0x54`, later reused by the `0x4474f0` transform-worker setup
    void* feedbackTransformLarge94 = nullptr;    // original child `+0x94`; allocated in `0x4474f0`
    void* feedbackTransformSmall98 = nullptr;    // original child `+0x98`; allocated in `0x4474f0`
    uint32_t currentPublicKeyId9C = 0;           // original child `+0x9c`
    uint8_t authRequestReadyA0 = 0;              // original child `+0xa0`; `0x447f50` sets this byte and `0x448050` branches on it before choosing raw `0x06` vs raw `0x08`
    std::array<uint8_t, 3> paddingA1ToA3{};      // original child `+0xa1 .. +0xa3`
    void* lazyPubkeyDatStateA4 = nullptr;        // original child `+0xa4`; lazy `pubkey.dat`-backed state built by `0x447260/0x447c10` and reused by `0x447eb0/0x447f50`
    void* raw08PublicKeyWorkerA8 = nullptr;      // original child `+0xa8`; live reply-public-key worker materialized by `0x447f50 -> 0x47780` and consumed by `0x4474f0`
    void* replyAuthDataValidatorAC = nullptr;    // original child `+0xac`; sibling validation worker consumed by `AuthBootstrapReplyCopyShadowF4_VerifyWithValidator` before the `+0xf4` copy is accepted

    // Original child `+0xb0 .. +0xeb` now keeps the narrower `0x448140` success-side prep family
    // explicit as the same three adjacent `0x14`-byte big-int objects the launcher ctor seeds with
    // `0x45d000` and the success path overwrites via `0x45de10`:
    // - `+0xb0` <- modulus bytes from copied `+0xf4 + 0xd2 .. + 0x131`
    // - `+0xc4` <- low public-exponent byte from copied `+0xf4 + 0xd1`
    // - `+0xd8` <- derived 96-byte private-exponent/transform output used by the same prep path
    AuthBootstrap680BigIntObject20Scaffold modulusBigIntB0{};        // original child `+0xb0`
    AuthBootstrap680BigIntObject20Scaffold publicExponentBigIntC4{}; // original child `+0xc4`
    AuthBootstrap680BigIntObject20Scaffold privateExponentBigIntD8{}; // original child `+0xd8`

    uint32_t inboundAuthStatusEc = 1;           // original child `+0xec`; seeded by `0x445500`, then overwritten by `0x448140` with inbound auth status/error state
    AuthBootstrap680AuthReplyParseObjectF0Sketch* authReplyParseObjectF0 = nullptr; // original child `+0xf0`; `0x448140` stores a copied `0x8c` auth-reply parse object here via `0x4449c0`, and `0x444900` later releases it
    void* authReplyCopyShadowF4 = nullptr;       // original child `+0xf4`; reply-derived copied `0x136` block used later by `0x433c0 -> 0x41b500 -> 0x41ce80 -> 0x441f30`
    AuthBootstrap680SmallStringMirror stringF8;  // original child `+0xf8 .. +0x100`; `0x441330` writes the prompt-password small-string neighboring the `+0xf0` auth-reply parse/copy family, and owner vtable `+0x60 / 0x41f3c0` later surfaces its begin pointer
    uint8_t crashReporterPromptForSecurId104 = 1; // original child `+0x104`; sibling `0x441330` SecurID-tail flag surfaced by owner vtable `+0x58 / 0x41f390`
    std::array<uint8_t, 3> padding105{};         // original child `+0x105 .. +0x107`
    void* opaqueReplyBlob108 = nullptr;          // original child `+0x108`; `0x441170` copies parse-object field `+0x2c/+0x30` (raw reply-header offset `+0x0f`) from the copied `+0xf0` family
    void* opaqueReplyBlob10C = nullptr;          // original child `+0x10c`; `0x441170` copies parse-object field `+0x34/+0x38` (raw reply-header offset `+0x11`) from the copied `+0xf0` family
    uint32_t authReplySuccessHeaderDword07_110 = 0; // original child `+0x110`; `0x43f300` writes the fixed raw reply-header dword at `authReplyParseObjectF0->replyHeader10 + 0x07` before the one-time success gate
    uint32_t authReplySuccessField15_114 = 0; // original child `+0x114`; `0x441260` writes the fixed raw reply-header dword at `authReplyParseObjectF0->replyHeader10 + 0x15`
    uint32_t authReplySuccessField15Timestamp118 = 0; // original child `+0x118`; `0x441260` writes current time alongside `+0x114`
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

struct AuthBootstrap680Ops {
    static void EraseOwnedState(const CLTLoginMediator* mediator);

    static uint32_t PrepareAndDispatch(CLTLoginMediator& mediator);
    static uint32_t HandleInboundAuthMessage(CLTLoginMediator& mediator);

    static void* BootstrapRaw08AuxHandle50(const CLTLoginMediator& mediator);
    static bool HasBootstrapRaw08AuxHandle54(const CLTLoginMediator& mediator);
    static uint8_t GetCrashReporterPromptForSecurId58(const CLTLoginMediator& mediator);

    static uint32_t SendAuthGetPublicKeyRequest(CLTLoginMediator& mediator);
    static uint32_t SendAuthRequestFromReply(
        CLTLoginMediator& mediator,
        const mxo::auth::GetPublicKeyReply& reply);
    static uint32_t SendAuthChallengeResponse(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthChallenge& challenge);
    static void LogParsedAuthReply(
        const CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);

    static void SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig(CLTLoginMediator& mediator);
    static void ResetRecoveredAuthBootstrapDynamicStateScaffold(CLTLoginMediator& mediator);
    static void SyncRecoveredAuthBootstrapAfterGetPublicKeyReplyScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::GetPublicKeyReply& reply);
    static void SyncRecoveredAuthBootstrapAfterAuthChallengeResponseScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthChallengeResponseBuildResult& buildResult);
    static bool PrepareState5MarginConnectionCopySendScaffold(
        CLTLoginMediator& mediator,
        mxo::liblttcp::CMarginConnection& marginConnection);
    static void SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);
    static void SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessPregateScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);
    static bool ConsumeState2AuthReplySuccessOneTimeGateScaffold(CLTLoginMediator& mediator);
    static void SyncRecoveredAuthBootstrapAfterState2AuthReplySuccessOneTimeScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);
};

}  // namespace mxo::ltlogin
