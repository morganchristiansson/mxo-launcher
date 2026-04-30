// Recovered source-file anchor:
// - \matrixstaging\runtime\src\libltcrypto\sessionkeyencryption.cpp
//
// Static-RE tightening note:
// - the old source-owned `EncryptMarginPayloadPacket` / `DecryptMarginPayloadPacket` wrappers were
//   removed from the shared public auth API after validating their only live launcher.exe call
//   sites in the message-connection stream-encryption module:
//   - write path `launcher.exe:0x44d390` -> `CPacketEncryptor_EncryptPacket` (`0x44c750`)
//   - read path  `launcher.exe:0x44d500` -> `CPacketDecryptor_DecryptPacket` (`0x44bca0`)
// - those semantics belong with `CStreamPacketEncryptionModule*` in
//   `\matrixstaging\runtime\src\libltmessaging\messageconnection.cpp`, not as a generic public
//   `mxo::auth` helper surface.
//
// This file intentionally remains as a source-file anchor for future auth/session-key recovery.
