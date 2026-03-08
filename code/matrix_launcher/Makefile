# Matrix Online Launcher Makefile
# Build configuration for Windows executables

# Compiler settings (MinGW-w64)
CXX = i686-w64-mingw32-g++

# Project name
TARGET = launcher.exe
SRC = src/main.cpp

# Default target
all: $(TARGET)

# Build the executable
# -static: Static linking (includes all dependencies)
# -Wl,--subsystem,console: Pure console application (no GUI)
# Produces a proper win32 console executable for Wine compatibility
$(TARGET): $(SRC)
	$(CXX) -O2 -static -o $(TARGET) $(SRC) \
		-Wl,--subsystem,console
# Clean build artifacts
clean:
	rm -f $(TARGET)

# Debug build (with symbols)
debug: CXXFLAGS = -Wall -Wextra -g -DDEBUG
debug: LDFLAGS = -mwindows
debug: $(TARGET)

# Run with Wine (if available)
run-wine: $(TARGET)
	@echo "Running with Wine..."
	@wine $(TARGET)

.PHONY: all clean debug run-wine
