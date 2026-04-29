# Packet_AsGetPublicKeyRequest_0x4b6c74 ctor shape

Anchors:
- `launcher.exe:0x444390` = `Packet_AsGetPublicKeyRequest_0x4b6c74::Packet_AsGetPublicKeyRequest_ctor`
- `launcher.exe:0x447eb0` = `AuthBootstrap680Child_0x441290::SendGetPublicKeyRequest`
- `launcher.exe:0x448140` = `AuthBootstrap680Child_0x441290::HandleInboundAuthMessage`

## Summary

Static RE shows that vtable `0x004b6c74` is shared by two related launcher shapes:

1. a compact `Packet_0x4af2a4`-sized builder used by `SendGetPublicKeyRequest` for raw auth opcode `0x06`
2. a larger `0x8c` parse shell used by `HandleInboundAuthMessage` when decoding raw auth opcode `0x0b`

`0x444390` is the incoming-message ctor/init path for the larger parse shell, not a child helper belonging to `AuthBootstrap680Child_0x441290`.

## Evidence from `0x444390`

The decompile shows classic ctor behavior:
- seeds the base packet vftable first
- stores/addrefs the incoming `CMessageConnectionMessageRef`
- chooses `payload+0x0c` vs headerless locator-adjusted payload start
- retables the object to `0x004b6c74`
- stores the `resolveFields` byte at `+0x0c`
- initializes the two embedded parse-accessor subobjects at `+0x5c` and `+0x70`
- branches to either:
  - `0x4436b0` for writable-body reset when `resolveFields == 0`
  - `0x443470` for resolved field-view setup when `resolveFields != 0`

## Callsite mapping

### `0x448140` / `HandleInboundAuthMessage`
For opcode `0x0b`, the child constructs a temporary local packet view with:
- `workItem` as the incoming message ref
- `resolveFields = 1`

That local view is then copied into child-owned storage (`child+0xf0`) via the `0x4449c0` copy helper.

### `0x447eb0` / `SendGetPublicKeyRequest`
The outbound path does **not** call `0x444390`.
It uses the default/base ctor shape for the compact builder, retables to `0x004b6c74`, grows a fixed 9-byte payload, and fills:
- `[payload+0x00] = 0x06`
- `[payload+0x01] = child->launcherVersion2C`
- `[payload+0x05] = child->currentPublicKeyId9C`

## Source guidance

Mirror this in source as one named `Packet_AsGetPublicKeyRequest_0x4b6c74` family:
- keep `InitializePayloadSize()` for the compact send-builder behavior
- keep `0x444390` named as the incoming-message ctor/init path for the larger parse shell
- do not attribute `0x444390` to `AuthBootstrap680Child` ownership just because `0x448140` calls it
- source may treat `Packet_AsGetPublicKeyRequest_0x4b6c74` as the canonical semantic type for the
  stored/copyable raw `0x0b` auth-reply parse object, even though launcher.exe expresses that
  larger shell through the same vtable family rather than a normal C++ subclass layout
