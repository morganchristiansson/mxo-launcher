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

## Result

All source-owned structs were deleted from `matrixstaging/runtime/src/libltcrypto/auth_crypto.h`, and the remaining packet encrypt/decrypt wrappers were also removed from the public auth header.

The public API now keeps only the pieces that still correspond to active validated behavior:

- framing enum
- `BuildVariableLengthPacket(...)` returning framed bytes + header length
- `AuthOpcodeName(...)`
