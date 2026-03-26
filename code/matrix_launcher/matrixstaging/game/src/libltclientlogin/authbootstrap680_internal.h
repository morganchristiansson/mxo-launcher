#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../../../runtime/src/libltcrypto/auth_crypto.h"

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

struct AuthBootstrapReplyShadowF4Sketch {
    // Bounded source-owned mirror of the validated auth-reply heap block copied into the
    // phase-2 bootstrap child `+0xf4` by `0x448140`.
    //
    // Current strongest anchored fields inside that copied `0x136` block are exactly the two
    // later owner-vtable exposures we care about on the active runtime path:
    // - `+0x85 .. +0x94` = shared 16-byte challenge/material family
    // - `+0xa8`         = raw-`0x08` aux-handle / availability family
    std::array<uint8_t, 0x85> prefix00{};
    std::array<uint8_t, 16> material85{};
    std::array<uint8_t, 0x13> gap95ToA7{};
    void* raw08AuxHandleA8 = nullptr;
};
static_assert(offsetof(AuthBootstrapReplyShadowF4Sketch, material85) == 0x85);
static_assert(offsetof(AuthBootstrapReplyShadowF4Sketch, raw08AuxHandleA8) == 0xa8);

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

    AuthBootstrap680SmallStringMirror string04;  // original child `+0x04`
    AuthBootstrap680SmallStringMirror string10;  // original child `+0x10`
    AuthBootstrap680SmallStringMirror string1C;  // original child `+0x1c`
    uint32_t loginType28 = 0;                    // original child `+0x28`; `0x4474f0` uses the low byte as raw `0x08` loginType
    uint32_t launcherVersion2C = 0;              // original child `+0x2c`; `0x447eb0` uses it in raw `0x06`
    std::array<uint8_t, 16> block30{};          // original child `+0x30 .. +0x3f`; copied by `0x448050`
    std::array<uint8_t, 16> block40{};          // original child `+0x40 .. +0x4f`; copied by `0x448050`
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
    void* phase2HelperA0 = nullptr;              // original child `+0xa0`; `0x448050` branches on its low-byte nullness
    void* lazyRaw06StateA4 = nullptr;            // original child `+0xa4`; lazy raw-`0x06` helper owned by `0x447eb0`
    void* raw08AuxHandleA8 = nullptr;            // original child `+0xa8`; post-`0x07` availability/worker family used by `0x4474f0`
    void* fieldAC = nullptr;                     // original child `+0xac`; sibling helper/object family still open

    // Original child `+0xb0 .. +0xeb` is still unresolved in current source.
    std::array<uint8_t, 0x3c> opaqueStateB0ToEb{};

    uint32_t stateFlagEC = 1;                    // original child `+0xec`; seeded by `0x445500`
    void* fieldF0 = nullptr;                     // original child `+0xf0`
    void* fieldF4 = nullptr;                     // original child `+0xf4`; points at `AuthBootstrapReplyShadowF4Sketch` when materialized
    AuthBootstrap680SmallStringMirror stringF8;  // original child `+0xf8`; small-string family whose begin pointer is surfaced by owner vtable `+0x60 / 0x41f3c0`
    void* fieldFC = nullptr;                     // original child `+0xfc`
    void* field100 = nullptr;                    // original child `+0x100`
    uint8_t crashReporterPromptForSecurId104 = 1; // original child `+0x104`; surfaced by owner vtable `+0x58 / 0x41f390`
    std::array<uint8_t, 3> padding105{};         // original child `+0x105 .. +0x107`
    uint32_t field108 = 0;                       // original child `+0x108`
    uint32_t field10C = 0;                       // original child `+0x10c`
    uint32_t field110 = 0;                       // original child `+0x110`
    uint32_t field114 = 0;                       // original child `+0x114`
    uint32_t field118 = 0;                       // original child `+0x118`
};

struct AuthBootstrap680Ops {
    static void EraseSidecar(const CLTLoginMediator* mediator);

    static uint32_t PrepareAndDispatchPhase2(CLTLoginMediator& mediator);

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
    static void SyncRecoveredAuthBootstrapAfterAuthReplyScaffold(
        CLTLoginMediator& mediator,
        const mxo::auth::AuthReply& reply);
};

}  // namespace mxo::ltlogin
