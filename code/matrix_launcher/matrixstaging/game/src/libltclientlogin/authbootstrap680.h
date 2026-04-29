#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <integer.h>
#include <modes.h>
#include <twofish.h>

#include "authbootstrap680_shared.h"
#include "../../../runtime/src/libltmessaging/messageconnection.h"

namespace mxo::liblttcp {
class CMarginConnection_0x4aff38;
}

namespace mxo::ltlogin {

class CLTLoginMediator;
class Packet_AsGetPublicKeyReply_0x4b6ca4;

// Direct Crypto++ public-key material owned by the launcher-side wrappers.
// The raw leaf sketch fields remain only as recovered layout mirrors.
struct AuthBootstrap680RsaPublicKeyPairOwnedState {
    CryptoPP::RSA::PublicKey publicKey;
    std::vector<uint8_t> modulusBytes;
    std::vector<uint8_t> exponentBytes;
};

struct AuthBootstrap680Field54HelperOwnedState {
    std::vector<uint8_t> bufferedOutput14;
    std::vector<uint8_t> scratchPrefix20;
};


// anchor: launcher.exe:0x4b7134 / vtable 0x004b7134
// Base class containing fields +0x00 through +0xf4
// Constructor: launcher.exe:0x445500 = AuthBootstrap680ChildBase_0x4b7134::ctor
// Destructor: launcher.exe:0x445610 = AuthBootstrap680ChildBase_0x4b7134::dtor
class AuthBootstrap680ChildBase_0x4b7134
    : public mxo::liblttcp::CStreamPacketEncryptionModuleWriteHelper_0x4b8690 {
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
 CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption* feedbackTransformLarge94 = nullptr; // original child `+0x94`; points at the recovered large/decrypting CBC Twofish object built along `0x4474f0 -> 0x446d90`
 CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption* feedbackTransformSmall98 = nullptr; // original child `+0x98`; points at the recovered small/encrypting CBC Twofish object built along `0x4474f0 -> 0x41df60`
 uint32_t currentPublicKeyId9C = 0; // original child `+0x9c`
 uint8_t authRequestReadyA0 = 0; // original child `+0xa0`; `0x447f50` sets this byte and `0x448050` branches on it before choosing raw `0x06` vs raw `0x08`
 std::array<uint8_t, 3> paddingA1ToA3{}; // original child `+0xa1 .. +0xa3`
 CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier* lazyPubkeyDatValidatorA4 = nullptr; // original child `+0xa4`; lazy `qspubkey.dat` verifier family built by `0x447260/0x447c10` and consulted by `0x447780 -> 0x468f80`
 CryptoPP::RSAES_OAEP_SHA_Encryptor* raw08PublicKeyWorkerA8 = nullptr; // original child `+0xa8`; live reply-public-key encryptor materialized by `0x447f50 -> 0x447780`, consumed by `0x4474f0` through `0x468ea0/0x468f00`
 CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier* replyAuthDataValidatorAC = nullptr; // original child `+0xac`; sibling verifier materialized by `0x447f50 -> 0x447780`, consumed by `0x44aec0 = AuthBootstrapReplyCopyShadowF4_VerifyWithValidator`

 // Original child `+0xb0 .. +0xeb` stored three adjacent old-Crypto++ `Integer` objects.
 // Static RE now proves data type `0x4ba50c` is `CryptoPP::Integer`, so source keeps those
 // fields directly as real Crypto++ integers while preserving the launcher child offsets in the
 // field names/comments:
 // - `+0xb0` <- modulus bytes from copied `+0xf4 + 0xd2 .. + 0x131`
 // - `+0xc4` <- low public-exponent byte from copied `+0xf4 + 0xd1`
 // - `+0xd8` <- derived 96-byte private-exponent/transform output used by the same prep path
 CryptoPP::Integer modulusBigIntB0{}; // semantic mirror of original child `+0xb0`
 CryptoPP::Integer publicExponentBigIntC4{}; // semantic mirror of original child `+0xc4`
 CryptoPP::Integer privateExponentBigIntD8{}; // semantic mirror of original child `+0xd8`

 uint32_t inboundAuthStatusEc = 1; // original child `+0xec`; seeded by `0x445500`, then overwritten by `0x448140` with inbound auth status/error state
 AuthBootstrap680AuthReplyParseObjectF0Sketch* authReplyParseObjectF0 = nullptr; // original child `+0xf0`; `0x448140` stores a copied `0x8c` auth-reply parse object here via `0x4449c0`, and `0x444900` later releases it
 AuthBootstrapReplyCopyShadowF4_0x44add0* authReplyCopyShadowF4 = nullptr; // original child `+0xf4`

 // Source-owned trailing storage used to back the recovered child pointers/byte spans above.
 // Keep this tail explicit so we do not need a separate per-child side map.
 AuthBootstrap680Field54HelperOwnedState field54HelperOwnedState_{};
 std::unique_ptr<CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier> lazyPubkeyDatValidatorA4Owned_{};
 AuthBootstrap680RsaPublicKeyPairOwnedState lazyPubkeyDatValidatorA4PublicKeyPair0c_{};
 std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Encryptor> raw08PublicKeyWorkerA8Owned_{};
 AuthBootstrap680RsaPublicKeyPairOwnedState raw08PublicKeyWorkerA8PublicKeyPair0c_{};
 std::unique_ptr<CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier> replyAuthDataValidatorACOwned_{};
 AuthBootstrap680RsaPublicKeyPairOwnedState replyAuthDataValidatorACPublicKeyPair0c_{};
 std::unique_ptr<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Decryption> feedbackTransformLarge94Owned_{};
 std::unique_ptr<CryptoPP::CBC_Mode<CryptoPP::Twofish>::Encryption> feedbackTransformSmall98Owned_{};
 std::vector<uint8_t> authReplyParsePacketBodyBytesOwned_{};
 std::vector<uint8_t> cachedGetPublicKeyReplyPayloadBytesOwned_{};
 std::vector<uint8_t> cachedAuthChallengeCiphertextBytesOwned_{};
 std::vector<uint8_t> cachedAuthRequestTwofishKeyBytesOwned_{}; // source-owned mirror of the live 16-byte child `+0x84..+0x93` seed reused by raw `0x09/0x0a/0x0b` follow-on crypto

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

 // anchor: launcher.exe:0x441330
    void SetPromptPasswordF8AndSecurIdFlag(const char* promptPasswordWithOptionalSecurId);
    // anchor: launcher.exe:0x441260
    void StoreField114AndTimestamp118(uint32_t field114Value);
    // anchor: launcher.exe:0x441170
    void CopyOpaqueReplyBlobs108_10c();
    // anchor: launcher.exe:0x43d480
    std::string CopyReplyString54_SOURCEOWNED() const;

    // anchor: launcher.exe:0x448050
    // Only known direct caller is state2 ready-side handoff at launcher.exe:0x43928b, where
    // assembly loads ECX from CLTLoginMediator +0x680 before CALL 0x448050.
    void PrepareAndDispatch(
        CLTLoginMediator& owner,
        void* sendTarget,
        const char* sessionTokenBegin);

    // anchor: launcher.exe:0x448140
    // Sole direct caller is `0x43f321`, which loads ECX from owner `+0x680` before the CALL.
    // Keep this on the concrete auth bootstrap child even though the field layout is inherited
    // through the stream-packet write-helper base.
    uint32_t HandleInboundAuthMessage(
        const mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* incomingAuthMessage);

    // anchor: launcher.exe:0x447eb0
    // The state2 ready-side dispatcher calls this directly on the owner `+0x680` auth child when
    // `authRequestReadyA0 == 0`; it is not a standalone write-helper receiver method.
    void SendGetPublicKeyRequest();

    // anchor: launcher.exe:0x4474f0 / 0x447780 / 0x447f50
    // These helpers stay child-owned alongside `0x448140`; Ghidra currently keeps the old
    // write-helper prefix in the symbol names, but the callsites and ECX setup point at the
    // owner `+0x680` auth child.
    void SendAuthRequest();
    uint32_t RebuildReplyPublicKeyWorkers(
        uint32_t replyPublicKeyId09,
        const CryptoPP::Integer& modulusInteger,
        const CryptoPP::Integer& publicExponentInteger,
        const uint8_t* signatureBytes);
    uint32_t HandleGetPublicKeyReply(
        const Packet_AsGetPublicKeyReply_0x4b6ca4& replyPacket);

    void* BootstrapRaw08AuxHandle50() const;
    bool HasBootstrapRaw08AuxHandle54() const;
    uint8_t GetCrashReporterPromptForSecurId58() const;
};

inline AuthBootstrap680Child_0x441290& AuthBootstrapChildFromWriteHelper(
    mxo::liblttcp::CStreamPacketEncryptionModuleWriteHelper_0x4b8690& helper) {
    return static_cast<AuthBootstrap680Child_0x441290&>(helper);
}

inline const AuthBootstrap680Child_0x441290& AuthBootstrapChildFromWriteHelper(
    const mxo::liblttcp::CStreamPacketEncryptionModuleWriteHelper_0x4b8690& helper) {
    return static_cast<const AuthBootstrap680Child_0x441290&>(helper);
}

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

extern bool g_authBootstrap680State2AuthReplySuccessOneTimeGate;

}  // namespace mxo::ltlogin
