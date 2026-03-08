# Matrix Online Launcher

A simple Windows executable launcher for Matrix Online.

## Project Structure

```
matrix_launcher/
├── README.md
├── src/
│   └── main.cpp          # Main source code
├── assets/
│   └── logo.png          # Launcher logo (optional)
├── include/
│   └── config.h          # Configuration headers
└── build/                # Build output directory
```

## Building

This project uses MinGW to build Windows executables.

### Prerequisites
- MinGW-w64 (g++)
- Make
- CMake (optional)

### Build Commands

```bash
# Configure and build
cd matrix_launcher
make

# Or using MinGW directly
g++ -o launcher.exe src/main.cpp
```

## Features

- Simple executable wrapper
- Cross-platform source code
- Easy to extend with Matrix Online integration