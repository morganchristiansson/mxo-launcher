#pragma once

// Transitional compatibility wrapper.
// Canonical public auth declarations now live under the recovered runtime-style path:
// - matrixstaging/runtime/src/libltcrypto/auth_crypto.h
// This file should keep shrinking as remaining callers move to the runtime-style home.

#include "../../matrixstaging/runtime/src/libltcrypto/auth_crypto.h"
