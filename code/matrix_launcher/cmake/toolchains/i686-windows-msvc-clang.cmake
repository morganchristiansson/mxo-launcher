set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_RC_COMPILER llvm-rc)

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MXO_MSVC_SYSROOT)
set(MXO_MSVC_SYSROOT "$ENV{MXO_MSVC_SYSROOT}" CACHE PATH "Path to xwin-extracted Windows SDK + MSVC CRT sysroot")
if(NOT MXO_MSVC_SYSROOT)
    message(FATAL_ERROR
        "MXO_MSVC_SYSROOT is not set.\n"
        "Install/provide a Windows SDK + MSVC CRT sysroot first (xwin is the easiest route on Ubuntu), then export:\n"
        "  export MXO_MSVC_SYSROOT=/path/to/xwin-root\n"
        "Supported layouts under that directory:\n"
        "  xwin splat style:\n"
        "    crt/include\n"
        "    crt/lib/x86\n"
        "    sdk/include/shared\n"
        "    sdk/include/ucrt\n"
        "    sdk/include/um\n"
        "    sdk/lib/ucrt/x86\n"
        "    sdk/lib/um/x86\n"
        "  xwin --use-winsysroot-style:\n"
        "    VC/Tools/MSVC/<ver>/include\n"
        "    VC/Tools/MSVC/<ver>/lib/x86\n"
        "    Windows Kits/10/Include/<ver>/{shared,ucrt,um}\n"
        "    Windows Kits/10/Lib/<ver>/{ucrt,um}/x86")
endif()

if(EXISTS "${MXO_MSVC_SYSROOT}/crt/include")
    set(MXO_CRT_INCLUDE "${MXO_MSVC_SYSROOT}/crt/include")
    set(MXO_CRT_LIB "${MXO_MSVC_SYSROOT}/crt/lib/x86")
    set(MXO_SDK_INCLUDE_SHARED "${MXO_MSVC_SYSROOT}/sdk/include/shared")
    set(MXO_SDK_INCLUDE_UCRT "${MXO_MSVC_SYSROOT}/sdk/include/ucrt")
    set(MXO_SDK_INCLUDE_UM "${MXO_MSVC_SYSROOT}/sdk/include/um")
    set(MXO_SDK_LIB_UCRT "${MXO_MSVC_SYSROOT}/sdk/lib/ucrt/x86")
    set(MXO_SDK_LIB_UM "${MXO_MSVC_SYSROOT}/sdk/lib/um/x86")
elseif(EXISTS "${MXO_MSVC_SYSROOT}/VC/Tools/MSVC")
    file(GLOB MXO_MSVC_TOOLSET_VERSIONS LIST_DIRECTORIES true "${MXO_MSVC_SYSROOT}/VC/Tools/MSVC/*")
    list(SORT MXO_MSVC_TOOLSET_VERSIONS COMPARE NATURAL ORDER DESCENDING)
    list(GET MXO_MSVC_TOOLSET_VERSIONS 0 MXO_MSVC_TOOLSET_ROOT)

    file(GLOB MXO_WINSDK_VERSIONS LIST_DIRECTORIES true "${MXO_MSVC_SYSROOT}/Windows Kits/10/Include/*")
    list(SORT MXO_WINSDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
    list(GET MXO_WINSDK_VERSIONS 0 MXO_WINSDK_INCLUDE_ROOT)
    get_filename_component(MXO_WINSDK_VERSION_NAME "${MXO_WINSDK_INCLUDE_ROOT}" NAME)
    set(MXO_WINSDK_LIB_ROOT "${MXO_MSVC_SYSROOT}/Windows Kits/10/Lib/${MXO_WINSDK_VERSION_NAME}")

    set(MXO_CRT_INCLUDE "${MXO_MSVC_TOOLSET_ROOT}/include")
    set(MXO_CRT_LIB "${MXO_MSVC_TOOLSET_ROOT}/lib/x86")
    set(MXO_SDK_INCLUDE_SHARED "${MXO_WINSDK_INCLUDE_ROOT}/shared")
    set(MXO_SDK_INCLUDE_UCRT "${MXO_WINSDK_INCLUDE_ROOT}/ucrt")
    set(MXO_SDK_INCLUDE_UM "${MXO_WINSDK_INCLUDE_ROOT}/um")
    set(MXO_SDK_LIB_UCRT "${MXO_WINSDK_LIB_ROOT}/ucrt/x86")
    set(MXO_SDK_LIB_UM "${MXO_WINSDK_LIB_ROOT}/um/x86")
else()
    message(FATAL_ERROR "Unrecognized MXO_MSVC_SYSROOT layout at '${MXO_MSVC_SYSROOT}'")
endif()

foreach(path
    MXO_CRT_INCLUDE
    MXO_CRT_LIB
    MXO_SDK_INCLUDE_SHARED
    MXO_SDK_INCLUDE_UCRT
    MXO_SDK_INCLUDE_UM
    MXO_SDK_LIB_UCRT
    MXO_SDK_LIB_UM)
    if(NOT EXISTS "${${path}}")
        message(FATAL_ERROR "Required sysroot path missing: ${path}='${${path}}'")
    endif()
endforeach()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
    "${MXO_CRT_INCLUDE}"
    "${MXO_SDK_INCLUDE_SHARED}"
    "${MXO_SDK_INCLUDE_UCRT}"
    "${MXO_SDK_INCLUDE_UM}"
)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES})

add_compile_options(
    --target=i686-pc-windows-msvc
    -fms-compatibility
    -fms-extensions
)

add_link_options(
    --target=i686-pc-windows-msvc
    -fuse-ld=lld-link
    LINKER:/libpath:${MXO_CRT_LIB}
    LINKER:/libpath:${MXO_SDK_LIB_UCRT}
    LINKER:/libpath:${MXO_SDK_LIB_UM}
)

set(CMAKE_C_STANDARD_LIBRARIES_INIT
    "kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib ws2_32.lib version.lib"
)
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "${CMAKE_C_STANDARD_LIBRARIES_INIT}")
