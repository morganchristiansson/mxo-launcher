set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET i686-w64-windows-gnu)
set(CMAKE_CXX_COMPILER_TARGET i686-w64-windows-gnu)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

set(MXO_MINGW_TOOLCHAIN_PREFIX "i686-w64-mingw32" CACHE STRING
    "MinGW toolchain prefix used for Clang GNU-target builds")
set(MXO_MINGW_GCC_ROOT "/usr" CACHE PATH
    "Root containing the MinGW GCC toolchain and sysroot")
set(MXO_MINGW_GCC_INSTALL_DIR "/usr/lib/gcc/i686-w64-mingw32/13-posix" CACHE PATH
    "Exact MinGW GCC installation Clang should mirror so it picks the posix libstdc++/winpthread runtime instead of the win32 variant")
set(MXO_MINGW_LIBSTDCXX_ROOT "${MXO_MINGW_GCC_INSTALL_DIR}/include/c++" CACHE PATH
    "Posix-flavored MinGW libstdc++ header root for GNU-target Clang builds")
set(MXO_MINGW_LIBSTDCXX_TARGET_ROOT "${MXO_MINGW_GCC_INSTALL_DIR}/include/c++/i686-w64-mingw32" CACHE PATH
    "Posix-flavored MinGW target-specific libstdc++ header root for GNU-target Clang builds")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

add_compile_options(
    --target=i686-w64-windows-gnu
    -femulated-tls
    -nostdinc++
    -isystem${MXO_MINGW_LIBSTDCXX_ROOT}
    -isystem${MXO_MINGW_LIBSTDCXX_TARGET_ROOT}
    -isystem${MXO_MINGW_LIBSTDCXX_ROOT}/backward
)

add_link_options(
    --target=i686-w64-windows-gnu
    -B${MXO_MINGW_GCC_INSTALL_DIR}
    -L${MXO_MINGW_GCC_INSTALL_DIR}
    -Wl,--enable-auto-import
)
