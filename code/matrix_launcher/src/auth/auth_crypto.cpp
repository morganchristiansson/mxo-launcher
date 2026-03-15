// Deprecated transitional placeholder.
//
// This file intentionally no longer carries the active auth implementation.
// The canonical runtime-style implementation has moved to:
// - matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp
// - matrixstaging/runtime/src/libltcrypto/filters.cpp
// - matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp
//
// Canonical public declarations now live at:
// - matrixstaging/runtime/src/libltcrypto/auth_crypto.h
//
// Compatibility wrapper retained at:
// - src/auth/auth_crypto.h
//
// Address anchors for the active low-level auth path:
// - launcher.exe:0x448050 = phase-2 auth/bootstrap dispatcher
// - launcher.exe:0x447eb0 = strongest raw 0x06 / AS_GetPublicKeyRequest send anchor
// - launcher.exe:0x4474f0 = strongest raw 0x08 / AS_AuthRequest send anchor
// - launcher.exe:0x4401a0 = later owner-side AS_AuthReply handler
//
// Keep this file empty so src/auth/ is no longer an active implementation home.
