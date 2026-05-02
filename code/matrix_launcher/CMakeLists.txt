cmake_minimum_required(VERSION 3.28)

project(matrix_launcher LANGUAGES CXX)

if(MSVC)
    # Native MSVC builds keep the usual DLL CRT default for now.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

if(MXO_CLANG_MSVC_ABI)
    # For the experimental Clang/MSVC-ABI path under Wine, prefer the retail static MSVC runtime
    # so the produced launcher does not depend on Wine's incomplete msvcp140/vcruntime/ucrt DLL
    # implementation surface.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(MXO_MINGW_LIBSTDCXX_ROOT "/usr/lib/gcc/i686-w64-mingw32/13-win32/include/c++" CACHE PATH
    "MinGW libstdc++ internal header root used by the recovered SGI tree helper lineage")
set(MXO_MINGW_LIBSTDCXX_TARGET_ROOT "/usr/lib/gcc/i686-w64-mingw32/13-win32/include/c++/i686-w64-mingw32" CACHE PATH
    "MinGW libstdc++ target-specific internal header root used by the recovered SGI tree helper lineage")

set(MXO_GAME_DIR "$ENV{HOME}/MxO_7.6005" CACHE PATH "Matrix Online game directory")
set(MXO_DEPLOY_TO_GAME_DIR ON CACHE BOOL "Copy built launchers into MXO_GAME_DIR after build")
set(MXO_BUILD_PROFILE "debug" CACHE STRING "Compatibility profile label used in output paths")
set(MXO_OUTPUT_BASENAME "resurrections" CACHE STRING "Launcher executable basename without .exe")
set(MXO_CRASHREPORTER_STUB_BASENAME "crashreporter.stub" CACHE STRING "Crash reporter stub basename without .exe")
set_property(CACHE MXO_BUILD_PROFILE PROPERTY STRINGS debug release)
set(MXO_DEBUG_LEVEL "slim" CACHE STRING "Debug info level: full, slim, or none")
set_property(CACHE MXO_DEBUG_LEVEL PROPERTY STRINGS full slim none)
set(MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR OFF CACHE BOOL "Retain old MinGW arg5 wrapper mode opt-out macro")

if(NOT CMAKE_BUILD_TYPE)
    if(MXO_BUILD_PROFILE STREQUAL "release")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
endif()

set(MXO_OUTPUT_ROOT "${CMAKE_SOURCE_DIR}/build/${MXO_BUILD_PROFILE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MXO_OUTPUT_ROOT}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MXO_OUTPUT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MXO_OUTPUT_ROOT}")

set(MXO_LAUNCHER_SOURCES
    src/resurrections.cpp
    src/diagnostics.cpp
    src/launcher_replacement_support.cpp
    src/textmode_launcher_flow.cpp
    src/server_config.cpp
    matrixstaging/runtime/src/libltbase/launchercommandline.cpp
    matrixstaging/runtime/src/libltbase/ltresult.cpp
    src/launcher_mediator_abi.cpp
    src/launcher_network_object_abi.cpp
    matrixstaging/runtime/src/libltbase/consolevar.cpp
    matrixstaging/runtime/src/libltbase/consoleint.cpp
    matrixstaging/runtime/src/libltbase/consolestring.cpp
    matrixstaging/runtime/src/libltbase/consolebool.cpp
    matrixstaging/runtime/src/libltbase/consolefloat.cpp
    matrixstaging/runtime/src/liblttcp/lttcpconnection.cpp
    matrixstaging/runtime/src/liblttcp/ltipaddresslist.cpp
    matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp
    matrixstaging/runtime/src/libltnet/sys/pc/pcsocket.cpp
    matrixstaging/runtime/src/libltnet/sys/pc/pcdns.cpp
    matrixstaging/runtime/src/libltmessaging/messageconnection.cpp
    matrixstaging/runtime/src/libltmessaging/crypto_init_helper.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator_active_state.cpp
    matrixstaging/game/src/libltclientlogin/authbootstrap680.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator_auth_entry.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator_events.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator_margin_route.cpp
    matrixstaging/game/src/libltclientlogin/loginmediator_state9.cpp
    matrixstaging/game/src/launcher/launcher.cpp
    matrixstaging/game/src/launcher/autodetectdialog.cpp
    matrixstaging/game/src/libltclientlogin/loginstate.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state0.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state1.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state2.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state3.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state4.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state5.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state6.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state7.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state8.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state9.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state10.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state11.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state12.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state13.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state14.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state15.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state16.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state17.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state18.cpp
    matrixstaging/game/src/libltclientlogin/loginstate_state19.cpp
    matrixstaging/game/src/libltclientlogin/client_chunk_hashes.cpp
    matrixstaging/game/src/libltclientlogin/launchpad.cpp
    matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp
    matrixstaging/runtime/src/libltcrypto/filters.cpp
    matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp
)

file(GLOB MXO_CRYPTOPP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/third_party/cryptopp890/*.cpp")
list(FILTER MXO_CRYPTOPP_SOURCES EXCLUDE REGEX "/(adhoc|bench|bench[1-3]|benchmark|cryptest|datatest|dlltest|fipsalgt|fipstest|regtest[0-9]*|test|testproc|validat[0-9]*|xtrcrypt)\\.cpp$")

add_library(cryptopp STATIC ${MXO_CRYPTOPP_SOURCES})
target_include_directories(cryptopp PUBLIC third_party/cryptopp890)
target_compile_options(cryptopp PUBLIC
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MINGW}>>:-include${CMAKE_SOURCE_DIR}/compat/cryptopp_clang_gnu/preinclude_config_ver.h>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:-include${CMAKE_SOURCE_DIR}/compat/cryptopp_clang_msvc/preinclude_config_ver.h>
)
target_compile_definitions(cryptopp PUBLIC
    CRYPTOPP_DISABLE_ASM=1
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_SECURE_NO_WARNINGS>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_USE_BUILTIN_OFFSETOF>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING>
)
target_compile_features(cryptopp PUBLIC cxx_std_17)
set_target_properties(cryptopp PROPERTIES OUTPUT_NAME cryptopp)

add_executable(resurrections WIN32 ${MXO_LAUNCHER_SOURCES})
set_target_properties(resurrections PROPERTIES OUTPUT_NAME "${MXO_OUTPUT_BASENAME}")
target_compile_features(resurrections PRIVATE cxx_std_17)
target_include_directories(resurrections PRIVATE
    third_party/cryptopp890
    third_party/spdlog-1.17.0/include
)
target_link_libraries(resurrections PRIVATE cryptopp version ws2_32)
if(MXO_CLANG_MSVC_ABI)
    # Keep the experimental MSVC-ABI build on the retail static runtime/STL set so Wine does not
    # need to provide the full MSVCP140 runtime surface.
    target_link_options(resurrections PRIVATE
        LINKER:/nodefaultlib:msvcrtd
        LINKER:/nodefaultlib:msvcprtd
        LINKER:/nodefaultlib:msvcrt
        LINKER:/nodefaultlib:msvcprt
        LINKER:/nodefaultlib:vcruntime
        LINKER:/nodefaultlib:ucrt
        LINKER:/defaultlib:libcmt
        LINKER:/defaultlib:libcpmt
        LINKER:/defaultlib:libvcruntime
        LINKER:/defaultlib:libucrt
    )
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MINGW)
    # GNU-target Clang needs the MinGW posix thread runtime explicitly once we
    # force the posix libstdc++ header/lib selection in the experimental toolchain.
    target_link_libraries(resurrections PRIVATE winpthread)
endif()
target_compile_definitions(resurrections PRIVATE
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_SECURE_NO_WARNINGS>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_USE_BUILTIN_OFFSETOF>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING>
    $<$<BOOL:${MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR}>:MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR>
)

add_executable(crashreporter_stub WIN32 tools/crashreporter_stub.cpp)
set_target_properties(crashreporter_stub PROPERTIES OUTPUT_NAME "${MXO_CRASHREPORTER_STUB_BASENAME}")
target_compile_features(crashreporter_stub PRIVATE cxx_std_17)
target_compile_definitions(crashreporter_stub PRIVATE
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_SECURE_NO_WARNINGS>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_CRT_USE_BUILTIN_OFFSETOF>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:_SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING>
)
if(MXO_CLANG_MSVC_ABI)
    target_link_options(crashreporter_stub PRIVATE
        LINKER:/nodefaultlib:msvcrtd
        LINKER:/nodefaultlib:msvcprtd
        LINKER:/nodefaultlib:msvcrt
        LINKER:/nodefaultlib:msvcprt
        LINKER:/nodefaultlib:vcruntime
        LINKER:/nodefaultlib:ucrt
        LINKER:/defaultlib:libcmt
        LINKER:/defaultlib:libcpmt
        LINKER:/defaultlib:libvcruntime
        LINKER:/defaultlib:libucrt
    )
endif()

set(MXO_COMMON_WARNINGS -Wall -Wextra)
set(MXO_COMMON_COMPILE_OPTIONS ${MXO_COMMON_WARNINGS})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND MXO_COMMON_COMPILE_OPTIONS -pipe)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND MXO_COMMON_COMPILE_OPTIONS -Os -DNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(resurrections PRIVATE -Wl,--gc-sections -Wl,-s)
else()
    list(APPEND MXO_COMMON_COMPILE_OPTIONS -O0 -fno-omit-frame-pointer)
endif()

if(MXO_DEBUG_LEVEL STREQUAL "full")
    list(APPEND MXO_COMMON_COMPILE_OPTIONS -g)
elseif(MXO_DEBUG_LEVEL STREQUAL "slim")
    list(APPEND MXO_COMMON_COMPILE_OPTIONS -g1)
elseif(MXO_DEBUG_LEVEL STREQUAL "none")
else()
    message(FATAL_ERROR "Unsupported MXO_DEBUG_LEVEL='${MXO_DEBUG_LEVEL}' (expected full/slim/none)")
endif()

target_compile_options(resurrections PRIVATE
    ${MXO_COMMON_COMPILE_OPTIONS}
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:-idirafter${MXO_MINGW_LIBSTDCXX_ROOT}>
    $<$<OR:$<BOOL:${MSVC}>,$<BOOL:${MXO_CLANG_MSVC_ABI}>>:-idirafter${MXO_MINGW_LIBSTDCXX_TARGET_ROOT}>
)
target_compile_options(crashreporter_stub PRIVATE -Wall -Wextra -O2)

set(MXO_DEPLOYED_EXE "${MXO_GAME_DIR}/${MXO_OUTPUT_BASENAME}.exe")
set(MXO_DEPLOYED_CRASHREPORTER_STUB "${MXO_GAME_DIR}/${MXO_CRASHREPORTER_STUB_BASENAME}.exe")

if(MXO_DEPLOY_TO_GAME_DIR)
    add_custom_command(
        TARGET resurrections POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MXO_GAME_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:resurrections>" "${MXO_DEPLOYED_EXE}"
        BYPRODUCTS "${MXO_DEPLOYED_EXE}"
        COMMENT "Deploying resurrections.exe into ${MXO_GAME_DIR}"
    )

    add_custom_command(
        TARGET crashreporter_stub POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MXO_GAME_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:crashreporter_stub>" "${MXO_DEPLOYED_CRASHREPORTER_STUB}"
        BYPRODUCTS "${MXO_DEPLOYED_CRASHREPORTER_STUB}"
        COMMENT "Deploying crashreporter.stub.exe into ${MXO_GAME_DIR}"
    )
endif()

message(STATUS "MXO build profile: ${MXO_BUILD_PROFILE}")
message(STATUS "MXO game dir: ${MXO_GAME_DIR}")
message(STATUS "MXO deploy to game dir: ${MXO_DEPLOY_TO_GAME_DIR}")
message(STATUS "MXO debug level: ${MXO_DEBUG_LEVEL}")
message(STATUS "MXO output basename: ${MXO_OUTPUT_BASENAME}")
message(STATUS "MXO crashreporter stub basename: ${MXO_CRASHREPORTER_STUB_BASENAME}")
