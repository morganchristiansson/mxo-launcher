# auth_crypto.h infidel-struct cleanup

Date: 2026-04-30

## Call-site grep summary

`matrixstaging/runtime/src/libltcrypto/auth_crypto.h` used to expose five source-owned structs:

- `FramedPacket`
  - grep callers:
    - `matrixstaging/game/src/libltclientlogin/authbootstrap680.cpp`
    - `matrixstaging/game/src/libltclientlogin/loginstate_state14.cpp`
    - `matrixstaging/runtime/src/libltmessaging/messageconnection.cpp`
    - `matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp`
- `AuthBlobLayout`
  - only referenced by dead helper `BuildAuthRequestBlobPlaintext` in `sessionkeyencryption.cpp`
- `AuthRequestLayout`
  - only referenced by dead helper `BuildDefaultAuthHeaderBytes` in `auth_internal.h`
- `AuthChallengeResponseLayout`
  - only referenced by `authbootstrap680.cpp`
- `MarginConnectReply`
  - no live callers; superseded by packet/view classes such as `Packet_MarginConnectReplyView_0x4b5378`

## Static-RE validation

### `FramedPacket`

Ghidra decompile at `launcher.exe:0x447eb0` shows the GetPublicKey sender builds a real packet object, writes the fixed 9-byte payload inline, then directly calls the connection send virtual. There is no separate launcher-side `{header,payload,bytes}` aggregate object.

So source now uses:

- `std::vector<uint8_t> packetBytes`
- `size_t packetHeaderByteCount`

instead of pretending a reusable launcher struct exists.

### `AuthChallengeResponseLayout`

Ghidra decompile at `launcher.exe:0x448140`, especially the `0x44831c..0x448467` region, shows the challenge-response path does not consult a generic layout object. It constructs `Packet_AsAuthChallengeResponse_0x4b6cf4` inline and calls specific helpers:

- `0x444040` append password string
- `0x444140` append secondary/station string
- `0x4441a0(0x20)` write fixed word `0x0020`
- `0x443660(0x20 - (payloadLen & 0x0f))` append zero padding

The previous layout booleans were source-owned speculation. The surviving source now spells out the fixed values inline with an anchor comment.

### `AuthBlobLayout` / `AuthRequestLayout`

No live call sites remained. They only fed helper code that was not part of the active launcher-owned path and had no validated one-to-one launcher object behind it.

### `MarginConnectReply`

No live call sites remained. Active margin/bootstrap receive code already uses packet/view classes and direct byte vectors rather than a synthetic umbrella reply struct.

## Follow-up: remaining margin encrypt/decrypt wrappers

Static-RE on the last two public helpers showed their only live launcher.exe call sites are not in the auth bootstrap layer at all, but in the message-connection stream-encryption module:

- write path `launcher.exe:0x44d390`
  - decompile shows the helper always constructs a sink and unconditionally calls `CPacketEncryptor_EncryptPacket(1, payload, len)`
  - there is no separate CERT bypass branch in the recovered launcher body
- read path `launcher.exe:0x44d500`
  - decompile shows the helper loops read transforms and calls `CPacketDecryptor_DecryptPacket(..., &endpointKey.familyPortDword)`
  - packet validation/CRC/timestamp semantics belong to the packet decryptor path, not a generic public auth helper
- packet crypto inner helpers:
  - `launcher.exe:0x44c750` = `CPacketEncryptor_EncryptPacket`
  - `launcher.exe:0x44bca0` = `CPacketDecryptor_DecryptPacket`

So the old public `EncryptMarginPayloadPacket(...)` / `DecryptMarginPayloadPacket(...)` wrappers were another source-owned layer. They were removed from `auth_crypto.h`, and the narrow surviving payload encrypt/decrypt scaffolds now live next to their only validated callers in `matrixstaging/runtime/src/libltmessaging/messageconnection.cpp`.

## Follow-up: read-helper family is not `cls_0x4c0540`

The suspicious OOAnalyzer bucket `cls_0x4c0540` does **not** match the stream-packet read helper family.

Static-RE evidence:

- the actual read-helper vtable is `0x004b86f0`
  - `+0x00 = 0x44d980` deleting destructor wrapper
  - `+0x04 = 0x44bb60`
  - `+0x08 = 0x44bb80`
  - `+0x0c = 0x44d500` read transform-and-dispatch
  - `+0x10 = 0x44c670` chained-helper delegate
- `0x44d500` iterates a helper-owned worker collection and wraps each candidate with transient Crypto++ plumbing plus a pooled read-prefix sink before calling `CPacketDecryptor_DecryptPacket`
- vtable `0x004c0540` instead contains unrelated work-item/header methods such as:
  - `0x4816f0` = get work type from `[this+4]`
  - `0x481750` = return `[this+0xc]`

So the best current model is:

- `CStreamPacketEncryptionModuleReadHelper_0x4b86f0` = real launcher helper family
- `CStreamPacketEncryptionModuleReadTransformWorker_0x4b86f0` = source-owned seed-holder mirror for the helper's worker collection
- transient Crypto++ classes participate inside `0x44d500`, but the helper itself is not a Crypto++ class renamed badly by OOAnalyzer

## Follow-up: `AS_GetPublicKeyReply` signature length in pubkey validator path

Date: 2026-05-01

Static-RE of `launcher.exe:0x468f80` closes two important details in the embedded auth-public-key verification path.

Decompile + disassembly show `0x468f80` is a plain helper, not a child-instance method. The only code xref is the auth-bootstrap child rebuild helper at `0x4477c9`, which pushes four stack args then does caller cleanup (`add esp, 0x10`).

So the better recovered shape is:

- plain `__cdecl` helper
- args = `(modulusInteger, publicExponentInteger, signatureBytes, lazyPubkeyDatValidatorA4)`
- no auth-bootstrap-child `this` pointer in the anchored helper itself
- return value consumed from `AL` only; the decompiler's `uint in_EAX` artifact is just a symptom of the bool-sized return

In source we still need a way to reach the recovered validator's backing public-key material, because our replacement verifier is implemented against source-owned Crypto++ state rather than by calling the original validator-family virtual directly. To keep the anchored helper at four arguments, the fourth source parameter is now a small view object that carries:

- the lazy validator object pointer corresponding to original `param_4`
- the associated source-owned public-key pair state used by the recovered verifier implementation

Within that helper, the launcher calls the validator-family virtual `+0x2c` with:

- signed bytes buffer length = `0x81`
  - 128-byte modulus exported into a stack buffer
  - final byte filled from the public exponent accessor at `0x45a3b0`
- signature byte length = `0x100`

So the launcher verifies the `AS_GetPublicKeyReply` embedded public key against a **256-byte signature**, not `0x80` bytes.

Our recovered source had drifted here in two ways:

- it had smuggled the auth-bootstrap child through the anchored helper signature even though the original helper is stack-arg based
- it was passing `0x80u` into `AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator(...)`

That truncation causes otherwise-valid replies to fail with `0x19000004` when `skipPublicKeyValidation = false`.

Anchors:

- `launcher.exe:0x447780` / `AuthBootstrap680Child_0x441290::RebuildReplyPublicKeyWorkers`
- `launcher.exe:0x468f80` / `AuthBootstrap680_VerifyReplyPublicKeyAgainstLazyPubkeyDatValidator`
- `launcher.exe:0x45a3b0` / byte accessor used to fetch the one-byte public exponent

## Result

All source-owned structs were deleted from `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`, and the remaining packet encrypt/decrypt wrappers were also removed from the public auth header.

The public API now keeps only the pieces that still correspond to active validated behavior:

- framing enum
- `BuildVariableLengthPacket(...)` returning framed bytes + header length
- `AuthOpcodeName(...)`
