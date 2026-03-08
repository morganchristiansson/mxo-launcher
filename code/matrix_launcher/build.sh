#!/bin/bash
# Matrix Online Launcher - Build Script for Linux
# Cross-compiles Windows executable using MinGW-w64

set -e

echo "=========================================="
echo "Matrix Online Launcher"
echo "Build script for Linux (cross-compile)"
echo "=========================================="

# Check if Wine is installed
if ! command -v wine &> /dev/null; then
    echo "Warning: Wine is not installed or not in PATH"
    echo "To build and test with Wine, install it:"
    echo "  sudo apt install wine64"
    echo ""
    read -p "Continue anyway? [y/N] " -n 1 -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Detect MinGW-w64
echo ""
echo "Detecting compiler..."
if command -v g++-mingw64 &> /dev/null; then
    CXX="g++-mingw64"
    echo "Found: g++-mingw64"
elif command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    CXX="x86_64-w64-mingw32-g++"
    echo "Found: x86_64-w64-mingw32-g++"
elif command -v mingw-w64 &> /dev/null; then
    CXX="mingw-w64"
    echo "Found: mingw-w64"
else
    echo "Error: MinGW-w64 compiler not found!"
    echo "Install it with:"
    echo "  sudo apt install mingw-w64"
    exit 1
fi

# Create build directory
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"

echo ""
echo "Building Windows executable..."
echo "Compiler: $CXX"

# Build the launcher
"$CXX" -mwindows -O2 -o "$BUILD_DIR/launcher.exe" src/main.cpp

if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "Executable: $BUILD_DIR/launcher.exe"
    echo "=========================================="
    
    # Ask if user wants to run with Wine
    if command -v wine &> /dev/null; then
        echo ""
        read -p "Run with Wine? [y/N] " -n 1 -r
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo ""
            echo "Running: wine $BUILD_DIR/launcher.exe"
            wine "$BUILD_DIR/launcher.exe"
        fi
    fi
else
    echo ""
    echo "Build failed!"
    exit 1
fi