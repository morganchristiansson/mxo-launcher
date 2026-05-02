// Force Crypto++ to expose the MSVC-compatibility macro set when building
// with Clang targeting the MSVC ABI. Upstream config_ver.h intentionally does
// not define CRYPTOPP_MSC_VERSION for Clang, but the MSVC STL allocator path
// used by this experiment expects Crypto++'s allocator compatibility members
// that are gated on that macro.

#ifndef MXO_COMPAT_CRYPTOPP_CLANG_MSVC_PREINCLUDE_CONFIG_VER_H
#define MXO_COMPAT_CRYPTOPP_CLANG_MSVC_PREINCLUDE_CONFIG_VER_H

#include "config_ver.h"

#if defined(__clang__) && defined(_MSC_VER) && !defined(CRYPTOPP_MSC_VERSION)
#define CRYPTOPP_MSC_VERSION (_MSC_VER)
#endif

#endif
