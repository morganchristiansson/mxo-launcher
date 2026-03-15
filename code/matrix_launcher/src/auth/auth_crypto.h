#pragma once

// Transitional compatibility wrapper only.
//
// Canonical public auth declarations now live under the recovered runtime-style path:
// - matrixstaging/runtime/src/libltcrypto/auth_crypto.h
//
// This wrapper remains only to avoid breaking stale include paths while the rest of the tree
// finishes moving away from src/auth/.
//
// Address anchors for the active low-level auth path:
// - launcher.exe:0x448050 = phase-2 auth/bootstrap dispatcher
// - launcher.exe:0x447eb0 = strongest raw 0x06 / AS_GetPublicKeyRequest send anchor
// - launcher.exe:0x4474f0 = strongest raw 0x08 / AS_AuthRequest send anchor
// - launcher.exe:0x4401a0 = later owner-side AS_AuthReply handler

#include "../../matrixstaging/runtime/src/libltcrypto/auth_crypto.h"
