# Matrix Online Launcher CMake Configuration
# Supports both native Windows and cross-compilation from Linux

cmake_minimum_required(VERSION 3.10)
project(MatrixLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect build platform
if(WIN32)
    message(STATUS "Building native Windows executable")
    set(BUILD_PLATFORM "windows-native")
else()
    message(STATUS "Cross-compiling for Windows")
    set(BUILD_PLATFORM "cross-compile")
endif()

# Add executable
add_executable(launcher.exe src/main.cpp)

# Link Windows libraries (only needed for native builds)
if(WIN32)
    target_link_libraries(launcher.exe ws2_32)
endif()

# Compiler flags
if(MINGW)
    message(STATUS "Using MinGW compiler")
    add_compile_options(-mwindows)
else()
    # For cross-compilation on Linux with MinGW-w64
    if(CMAKE_BUILD_TYPE STREQUAL "MinGW")
        add_compile_options(-mwindows)
        target_link_options(launcher.exe PRIVATE -mwindows)
    endif()
endif()

# Install rules
install(TARGETS launcher.exe DESTINATION bin)

message(STATUS "Platform: ${BUILD_PLATFORM}")