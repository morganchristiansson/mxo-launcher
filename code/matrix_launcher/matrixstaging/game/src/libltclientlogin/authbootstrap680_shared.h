#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <integer.h>
#ifndef CRYPTOPP_ENABLE_NAMESPACE_WEAK
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#endif
#include <md5.h>
#include "rsa.h"

#include "../../../runtime/src/libltcrypto/auth_crypto.h"

namespace mxo::ltlogin {

class __attribute__((packed)) AuthBootstrapReplyCopyShadowF4_0x44add0 {
public:
    std::array<uint8_t, 0x80> authSignature00{};
    std::array<uint8_t, 0xb6> signedData80{};

    void BuildSignedDataMd5Digest(std::array<uint8_t, 16>* outDigest) const;
    bool IsFresh(int timeBias) const;
    uint32_t VerifyWithValidator(
        const CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier* validator,
        int timeBias) const;
};

// Static RE now proves launcher data type `0x4ba50c` is old Crypto++ `Integer`.
// Source therefore uses `CryptoPP::Integer` directly.


// Recovered raw `0x08` helper stack note:
// - `launcher.exe:0x438120` = `CryptoPP_PK_DefaultEncryptionFilter_ctor`
// - `launcher.exe:0x438320` = `CryptoPP_PK_DefaultEncryptionFilter_Put2`
// - `launcher.exe:0x4382c0` = `CryptoPP_PK_Encryptor_CreateEncryptionFilter`
//
// Static RE now identifies child `+0xa8` as an old Crypto++ encryptor family, so source uses the
// direct `CryptoPP::RSAES_OAEP_SHA_Encryptor` class.

// Recovered validator accumulator note:
// - `launcher.exe:0x4472f0` = `CryptoPP_PK_MessageAccumulatorMD5_Create`
// - `launcher.exe:0x447390` = `CryptoPP_PK_MessageAccumulatorBase_Construct`
// - `launcher.exe:0x447340` = `CryptoPP_PK_MessageAccumulatorBase_Update`
// - `launcher.exe:0x447380` = `CryptoPP_PK_MessageAccumulatorMD5_AccessHash`
// - `launcher.exe:0x468520` loads the RSA-decoded signature bytes into the accumulator state
// - `launcher.exe:0x467ee0` / `0x467f70` finalize against the outer verifier object
//
// Static RE now identifies child `+0xa4/+0xac` as old Crypto++ verifier-family objects, so source
// uses direct `CryptoPP::Weak::RSASSA_PKCS1v15_MD5_Verifier` leaves.
//
// Important RE clarification:
// - the raw `0x0b` auth-reply path at `0x44aec0` does *not* bypass the temporary worker family
// - it calls verifier vtable slot `+0x2c`, and the `0x4b7580` leaf maps that slot to the older
//   Crypto++ `VerifyMessage(...)` convenience (`0x437ba0`)
// - that convenience internally drives the same worker family:
//   `0x4472f0` create -> `0x468520` input signature -> accumulator `Update()` -> `0x467ee0`
//   verify/finalize
// - sibling inherited slots also line up cleanly:
//   `+0x28 = 0x437b70` ->
//     `CryptoPP::TF_VerifierBase_0x4b6e40::VerifyAccumulatorAndDestroyOwnedWorker`
//     (`Verify(PK_MessageAccumulator*)` convenience with ownership transfer)
//   `+0x34 = 0x437c20` ->
//     `CryptoPP::TF_VerifierBase_0x4b6e40::RecoverMessageWithTemporaryWorker`
//     driving `0x467f70 = AuthBootstrap680ReplyAuthDataValidator_RecoverTemporaryWorkerResultPair`
// The auth-bootstrap validator path currently only needs the `VerifyMessage(...)` route above.
// So source should prefer the direct verifier-leaf `VerifyMessage(...)` call here unless/until a
// lower-level replacement is proven to be both more faithful *and* runtime-safe.

// Launcher-owned wrapper around embedded Crypto++ RNG / BufferedTransformation slices.
// This is not a standalone Crypto++ leaf type by itself; static RE of `0x4686e0` and `0x468640`
// shows a launcher wrapper object whose active subobjects are:
// - `+0x00 = 0x004b695c` launcher wrapper vtable
// - `+0x04 = 0x004b68a8` old `CryptoPP::RandomPool` / modern `CryptoPP::OldRandomPool`
// - `+0x08 = 0x004b41e0` `CryptoPP::BufferedTransformation`
// The recovered `0x468640` fill helper dispatches through the `+0x04` RandomPool family rather
// than exposing an additional distinct Crypto++ concrete class.
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

// The old `AuthBootstrap680AuthReplyParseObjectF0Sketch` documentation-only shell has been
// retired. Source now treats `Packet_AsGetPublicKeyRequest_0x4b6c74` as the canonical semantic
// type for both the outbound raw0x06 builder role and the inbound raw0x0b auth-reply parse role.

}  // namespace mxo::ltlogin
