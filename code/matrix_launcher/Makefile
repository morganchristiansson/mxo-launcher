# Matrix Online Launcher Makefile
# Build configuration for Windows executables

# Compiler settings (MinGW-w64) - 32-bit for Wine compatibility
CXX = i686-w64-mingw32-g++

# Project name
TARGET_DIR = ~/MxO_7.6005
TARGET = $(TARGET_DIR)/resurrections.exe
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
run: $(TARGET)
	@echo "Running with Wine..."
	cd $(TARGET_DIR) && wine $(TARGET)

.PHONY: all clean debug run-wine
