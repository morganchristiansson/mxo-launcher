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

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

add_compile_options(
    --target=i686-w64-windows-gnu
    --gcc-toolchain=${MXO_MINGW_GCC_ROOT}
)

add_link_options(
    --target=i686-w64-windows-gnu
    --gcc-toolchain=${MXO_MINGW_GCC_ROOT}
)
