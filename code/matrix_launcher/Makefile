# Matrix Online launcher compatibility Makefile
#
# Canonical build logic now lives in CMake.
# This wrapper intentionally preserves the old `make` workflow and convenience targets.

CMAKE ?= cmake
GENERATOR ?= Ninja
TOOLCHAIN_FILE ?= cmake/toolchains/i686-windows-gnu-gcc.cmake
TOOLCHAIN_TAG ?= $(notdir $(basename $(TOOLCHAIN_FILE)))
MXO_MSVC_SYSROOT ?= $(if $(wildcard third_party/xwin),$(CURDIR)/third_party/xwin,$(HOME)/.xwin)
GAME_DIR = ~/MxO_7.6005
BUILD_ROOT = build
BUILD_PROFILE ?= debug
DEBUG_LEVEL ?= slim
BUILD_DIR = $(BUILD_ROOT)/$(BUILD_PROFILE)
OUTPUT_BASENAME = resurrections-$(TOOLCHAIN_TAG)-$(BUILD_PROFILE)
CRASHREPORTER_BASENAME = crashreporter.stub-$(TOOLCHAIN_TAG)-$(BUILD_PROFILE)
TARGET = $(GAME_DIR)/$(OUTPUT_BASENAME).exe
CRASHREPORTER_STUB = $(GAME_DIR)/$(CRASHREPORTER_BASENAME).exe

-include secrets/launcher_login.local.mk

LAUNCHER_ARGS = -nopatch -noeula $(if $(MXO_SERVER),-server $(MXO_SERVER)) $(if $(MXO_USER),-user $(MXO_USER)) $(if $(MXO_PASS),-pwd $(MXO_PASS)) $(if $(MXO_SESSION),-session $(MXO_SESSION)) $(if $(MXO_CHAR),-char $(MXO_CHAR))

CMAKE_CONFIGURE = $(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
	-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
	-DMXO_MSVC_SYSROOT=$(MXO_MSVC_SYSROOT) \
	-DMXO_BUILD_PROFILE=$(BUILD_PROFILE) \
	-DMXO_GAME_DIR=$(GAME_DIR) \
	-DMXO_DEPLOY_TO_GAME_DIR=$(if $(filter 0,$(MXO_DEPLOY_TO_GAME_DIR)),OFF,ON) \
	-DMXO_DEBUG_LEVEL=$(DEBUG_LEVEL) \
	-DMXO_OUTPUT_BASENAME=$(OUTPUT_BASENAME) \
	-DMXO_CRASHREPORTER_STUB_BASENAME=$(CRASHREPORTER_BASENAME) \
	-DMXO_DISABLE_MINGW_NATIVE_ARG5_VPTR=$(if $(filter 1,$(MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR)),ON,OFF)

all: $(TARGET)

$(BUILD_DIR)/build.ninja:
	$(CMAKE_CONFIGURE)

$(TARGET): $(BUILD_DIR)/build.ninja
	$(CMAKE) --build $(BUILD_DIR) --target resurrections

$(CRASHREPORTER_STUB): $(BUILD_DIR)/build.ninja
	$(CMAKE) --build $(BUILD_DIR) --target crashreporter_stub

run: $(TARGET)
	cd $(GAME_DIR) && wine ./$(OUTPUT_BASENAME).exe $(LAUNCHER_ARGS)

run_original:
	cd $(GAME_DIR) && wine ./launcher.exe $(LAUNCHER_ARGS)

crashdump:
	@latest=$$(cd $(GAME_DIR) && ls -t MatrixOnline_0.0_crash_*.dmp 2>/dev/null | head -1); \
	if [ -z "$$latest" ]; then \
		echo "No crash dump found in $(GAME_DIR)"; \
		exit 1; \
	fi; \
	echo "Latest crash dump: $(GAME_DIR)/$$latest"; \
	cd $(GAME_DIR) && printf 'info reg\nbt\ninfo share\nquit\n' | winedbg "$$latest"

install_crashreporter_stub: $(CRASHREPORTER_STUB)
	cd $(GAME_DIR) && \
		if [ ! -f crashreporter.exe.original ]; then mv crashreporter.exe crashreporter.exe.original; fi && \
		cp crashreporter.stub.exe crashreporter.exe

restore_crashreporter:
	cd $(GAME_DIR) && \
		if [ -f crashreporter.exe.original ]; then mv -f crashreporter.exe.original crashreporter.exe; fi && \
		rm -f crashreporter.stub.exe

release:
	@$(MAKE) BUILD_PROFILE=release DEBUG_LEVEL=none all

full_debug:
	@$(MAKE) BUILD_PROFILE=debug DEBUG_LEVEL=full all

configure: $(BUILD_DIR)/build.ninja

clean:
	rm -f $(TARGET) $(CRASHREPORTER_STUB)
	rm -f $(BUILD_DIR)/$(OUTPUT_BASENAME).exe $(BUILD_DIR)/$(CRASHREPORTER_BASENAME).exe

# Full tree wipe when we really want to discard CMake state and third-party build products.
distclean: clean
	rm -rf $(BUILD_ROOT)

.PHONY: all release full_debug configure run run_original crashdump install_crashreporter_stub restore_crashreporter clean distclean
