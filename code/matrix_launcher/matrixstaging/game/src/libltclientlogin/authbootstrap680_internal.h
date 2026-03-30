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
    // Static `0x448140` / `0x44add0` / `0x44aec0` now tightens this materially:
    // - this block is not `[u16 3][u16 0x0136] + tail`
    // - the copied `0x136` bytes line up directly as:
    //   - `+0x00 .. +0x7f` = 128-byte auth-signature span
    //   - `+0x80 .. +0x135` = signed-data span (`0xb6` bytes)
    // - high-value verified suffix offsets:
    //   - `+0x85` = signed-data `+0x05` (wrapper-facing `owner+0x680->+0xf4+0x85`)
    //   - `+0xa8` = signed-data `+0x28` (wrapper-facing `owner+0x680->+0xf4+0xa8`)
    //   - `+0xac` = signed-data expiry-time dword used by `0x44add0 / 0x44aec0`
    //   - `+0xd1` = low public-exponent byte used by `0x448140`
    //   - `+0xd2 .. +0x131` = modulus bytes used by `0x448140`
    std::array<uint8_t, 0x80> authSignature00{};
    std::array<uint8_t, 0xb6> signedData80{};
};
static_assert(offsetof(AuthBootstrapReplyCopyShadowF4Sketch, authSignature00) == 0x00);
static_assert(offsetof(AuthBootstrapReplyCopyShadowF4Sketch, signedData80) == 0x80);
static_assert(sizeof(AuthBootstrapReplyCopyShadowF4Sketch) == 0x136);

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

    // Original child `+0x54 .. +0x7f` is a concrete helper subobject seeded from ctor `0x445500`
    // with vtable `0x004b695c`. Current source keeps only the occupied span explicit until that
    // helper is typed tightly enough to deserve its own source-owned model.
    std::array<uint8_t, 0x2c> opaqueState54To7f{};

    uint32_t timestamp80 = 0;                    // original child `+0x80`
    std::array<uint8_t, 16> challengeMaterial85{}; // original child `+0x85 .. +0x94`
    void* feedbackTransformLarge94 = nullptr;    // original child `+0x94`; allocated in `0x4474f0`
    void* feedbackTransformSmall98 = nullptr;    // original child `+0x98`; allocated in `0x4474f0`
    uint32_t currentPublicKeyId9C = 0;           // original child `+0x9c`
    uint8_t authRequestReadyA0 = 0;              // original child `+0xa0`; `0x447f50` sets this byte and `0x448050` branches on it before choosing raw `0x06` vs raw `0x08`
    std::array<uint8_t, 3> paddingA1ToA3{};      // original child `+0xa1 .. +0xa3`
    void* lazyPubkeyDatStateA4 = nullptr;        // original child `+0xa4`; lazy `pubkey.dat`-backed state built by `0x447260/0x447c10` and reused by `0x447eb0/0x447f50`
    void* raw08PublicKeyWorkerA8 = nullptr;      // original child `+0xa8`; live reply-public-key worker materialized by `0x447f50 -> 0x47780` and consumed by `0x4474f0`
    void* fieldAC = nullptr;                     // original child `+0xac`; sibling reply-validation/transform family still open

    // Original child `+0xb0 .. +0xeb` now keeps the narrower `0x448140` success-side prep family
    // explicit as three adjacent `0x14`-byte big-int wrapper objects:
    // - `+0xb0` <- modulus bytes from copied `+0xf4 + 0xd2 .. + 0x131`
    // - `+0xc4` <- low public-exponent byte from copied `+0xf4 + 0xd1`
    // - `+0xd8` <- derived 96-byte private-exponent/transform output used by the same prep path
    // Current source still stores the raw `0x3c` object span here and keeps any owned digit buffers
    // in sidecar storage so the launcher child layout does not grow.
    std::array<uint8_t, 0x3c> opaqueStateB0ToEb{};

    uint32_t inboundAuthStatusEc = 1;           // original child `+0xec`; seeded by `0x445500`, then overwritten by `0x448140` with inbound auth status/error state
    void* fieldF0 = nullptr;                     // original child `+0xf0`; broader raw-`0x0b` parse object family still not source-owned tightly enough
    void* authReplyCopyShadowF4 = nullptr;       // original child `+0xf4`; reply-derived copied `0x136` block used later by `0x433c0 -> 0x41b500 -> 0x41ce80 -> 0x441f30`
    AuthBootstrap680SmallStringMirror stringF8;  // original child `+0xf8`; small-string family whose begin pointer is surfaced by owner vtable `+0x60 / 0x41f3c0`
    void* fieldFC = nullptr;                     // original child `+0xfc`
    void* field100 = nullptr;                    // original child `+0x100`
    uint8_t crashReporterPromptForSecurId104 = 1; // original child `+0x104`; surfaced by owner vtable `+0x58 / 0x41f390`
    std::array<uint8_t, 3> padding105{};         // original child `+0x105 .. +0x107`
    void* opaqueReplyBlob108 = nullptr;          // original child `+0x108`; copied by `0x441170` from one opaque raw-`0x0b` parse-object blob family
    void* opaqueReplyBlob10C = nullptr;          // original child `+0x10c`; copied by `0x441170` from a second opaque raw-`0x0b` parse-object blob family
    uint32_t field110 = 0;                       // original child `+0x110`; `0x43f300` writes it from raw-`0x0b` parse inner +0x07 before the one-time success gate
    uint32_t field114 = 0;                       // original child `+0x114`; `0x441260` writes it from raw-`0x0b` parse inner +0x15 inside the one-time success gate
    uint32_t field118 = 0;                       // original child `+0x118`; `0x441260` writes current time alongside `+0x114`
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
    static void EraseSidecar(const CLTLoginMediator* mediator);

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
