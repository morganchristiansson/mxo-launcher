// Force Crypto++ GNU-style compiler feature detection for the experimental
// GNU-target MinGW Clang build before any vendor header includes config_ver.h.

#ifndef MXO_COMPAT_CRYPTOPP_PREINCLUDE_CONFIG_VER_H
#define MXO_COMPAT_CRYPTOPP_PREINCLUDE_CONFIG_VER_H

#include "config_ver.h"

#if defined(__clang__) && defined(__MINGW32__) && defined(CRYPTOPP_LLVM_CLANG_VERSION) && !defined(_MSC_VER)
#undef CRYPTOPP_LLVM_CLANG_VERSION
#undef CRYPTOPP_GCC_VERSION
#define CRYPTOPP_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#endif
