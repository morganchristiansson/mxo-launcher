# Matrix Online Launcher Makefile
# Build configuration for Windows executables

# Compiler settings (MinGW-w64) - 32-bit for Wine compatibility
CXX = i686-w64-mingw32-g++

# Project name
TARGET_DIR = ~/MxO_7.6005
TARGET = $(TARGET_DIR)/resurrections.exe
SRC = src/main.cpp src/dll_deployer.c

# Include path for header files
INCLUDE = -Iinclude

# Default target
all: $(TARGET)

# Build the executable
# Note: We DON'T use -static because the original launcher uses MSVCR71.dll
# and the C runtime needs to be shared with client.dll
$(TARGET): $(SRC)
	$(CXX) -O2 $(INCLUDE) -o $(TARGET) $(SRC) -mwindows

# Build the proper launcher with master database support
launcher_proper.exe: launcher_proper.cpp master_database.h
	$(CXX) -Wall -Wextra -O2 -o $@ $< -Wl,--export-all-symbols

# Build test executables
test_mxowrap.exe: test_mxowrap.cpp
	$(CXX) -Wall -Wextra -O2 -o $@ $<

test_mxowrap_import.exe: test_mxowrap_import.cpp
	$(CXX) -Wall -Wextra -O2 -o $@ $< -L~/MxO_7.6005 -lmxowrap

# Clean build artifacts
clean:
	rm -f $(TARGET) launcher_proper.exe test_mxowrap.exe test_mxowrap_import.exe

# Debug build (with symbols)
debug: CXXFLAGS = -Wall -Wextra -g -DDEBUG
debug: LDFLAGS = -mwindows
debug: $(TARGET)

# Run with Wine (if available)
run: $(TARGET)
	@echo "Running with Wine..."
	cd $(TARGET_DIR) && xvfb-run wine $(TARGET)

.PHONY: all clean debug run launcher_proper.exe test_mxowrap.exe test_mxowrap_import.exe
