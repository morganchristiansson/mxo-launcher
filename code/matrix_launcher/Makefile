# Matrix Online launcher reimplementation Makefile

CXX = i686-w64-mingw32-g++-posix
AR ?= i686-w64-mingw32-ar
RANLIB ?= i686-w64-mingw32-ranlib
GAME_DIR = ~/MxO_7.6005
TARGET = $(GAME_DIR)/resurrections.exe
CRASHREPORTER_STUB = $(GAME_DIR)/crashreporter.stub.exe
CRASHREPORTER_STUB_SRC = tools/crashreporter_stub.cpp

CRYPTOPP_DIR = third_party/cryptopp890
SPDLOG_DIR = third_party/spdlog-1.17.0

# Transitional note:
# - canonical public auth declarations now live under:
#   matrixstaging/runtime/src/libltcrypto/auth_crypto.h
# - a compatibility wrapper remains at src/auth/auth_crypto.h
# - low-level auth implementation is now split conservatively under recovered runtime-style paths:
#   - matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp
#   - matrixstaging/runtime/src/libltcrypto/filters.cpp
#   - matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp
# - launcher/login/runtime class scaffolds now start moving under recovered source-style paths
#   rooted at matrixstaging/
# Launcher object dependencies
LAUNCHER_OBJS = \
	$(BUILD_DIR)/src/resurrections.o \
	$(BUILD_DIR)/src/diagnostics.o \
	$(BUILD_DIR)/src/launcher_replacement_support.o \
	$(BUILD_DIR)/src/textmode_launcher_flow.o \
	$(BUILD_DIR)/src/server_config.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/launchercommandline.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/ltresult.o \
	$(BUILD_DIR)/src/launcher_mediator_abi.o \
	$(BUILD_DIR)/src/launcher_network_object_abi.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/consolevar.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/consoleint.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/consolestring.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/consolebool.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltbase/consolefloat.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/liblttcp/lttcpconnection.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/liblttcp/ltipaddresslist.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltnet/sys/pc/pcsocket.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltnet/sys/pc/pcdns.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltmessaging/messageconnection.o \
        $(BUILD_DIR)/matrixstaging/runtime/src/libltmessaging/crypto_init_helper.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator_active_state.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/authbootstrap680.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator_auth_entry.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator_events.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator_margin_route.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginmediator_state9.o \
	$(BUILD_DIR)/matrixstaging/game/src/launcher/launcher.o \
	$(BUILD_DIR)/matrixstaging/game/src/launcher/autodetectdialog.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state0.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state1.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state2.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state3.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state4.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state5.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state6.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state7.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state8.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state9.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state10.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state11.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state12.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state13.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state14.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state15.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state16.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state17.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state18.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/loginstate_state19.o \
	$(BUILD_DIR)/matrixstaging/game/src/libltclientlogin/launchpad.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltcrypto/filters.o \
	$(BUILD_DIR)/matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.o

BUILD_DIR = build
CRYPTOPP_LIB = $(BUILD_DIR)/libcryptopp.a
SPDLOG_LIB = $(BUILD_DIR)/libspdlog.a

-include secrets/launcher_login.local.mk

LAUNCHER_ARGS = -nopatch -noeula $(if $(MXO_SERVER),-server $(MXO_SERVER)) $(if $(MXO_USER),-user $(MXO_USER)) $(if $(MXO_PASS),-pwd $(MXO_PASS)) $(if $(MXO_SESSION),-session $(MXO_SESSION)) $(if $(MXO_CHAR),-char $(MXO_CHAR))

all: $(TARGET)

CPPFLAGS = -I$(CRYPTOPP_DIR) -I$(SPDLOG_DIR)/include
CXXFLAGS = -Wall -Wextra -O0 -g -fno-omit-frame-pointer
LDFLAGS = -L$(BUILD_DIR)
LDLIBS = -lversion -lws2_32 -lcryptopp

# MinGW arg5 native-vptr mode:
# - enabled by default on __MINGW32__ builds
# - opt out with: make MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR=1
ifeq ($(MXO_DISABLE_MINGW_NATIVE_ARG5_VPTR),1)
CPPFLAGS += -DMXO_DISABLE_MINGW_NATIVE_ARG5_VPTR
endif

CXXFLAGS += -MMD -MP
-include $(LAUNCHER_OBJS:.o=.d)

# Object file for each source
# Object file for each source - pattern rule handles all src/*.cpp files
$(BUILD_DIR)/src/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

# Object file for matrixstaging/ sources - pattern rule handles all matrixstaging/*.cpp files
$(BUILD_DIR)/matrixstaging/%.o: matrixstaging/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

# Target depends on launcher objects and cryptopp (spdlog is order-only prerequisite)
$(TARGET): $(LAUNCHER_OBJS) | $(SPDLOG_DIR) $(CRYPTOPP_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Download cryptopp and spdlog if they don't exist
$(CRYPTOPP_DIR):
	@echo "Downloading cryptopp..."
	@mkdir -p $(dir $@)
	@curl -k -L -o $(dir $@)/cryptopp890.zip https://www.cryptopp.com/cryptopp890.zip && \
		unzip -q $(dir $@)/cryptopp890.zip -d $@ && \
		rm $(dir $@)/cryptopp890.zip

$(SPDLOG_DIR):
	@echo "Downloading spdlog..."
	@mkdir -p $(dir $@)
	@curl -L -o $(dir $@)/spdlog-1.17.0.tar.gz https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz && \
		tar -xzf $(dir $@)/spdlog-1.17.0.tar.gz -C $(dir $@) && \
		rm $(dir $@)/spdlog-1.17.0.tar.gz

$(CRYPTOPP_LIB): $(CRYPTOPP_DIR)
	@mkdir -p $(dir $@)
	$(MAKE) -C $(CRYPTOPP_DIR) CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" static
	cp $(CRYPTOPP_DIR)/libcryptopp.a $@

# spdlog is header-only, included via CPPFLAGS. No build step needed.

$(CRASHREPORTER_STUB): $(CRASHREPORTER_STUB_SRC)
	$(CXX) -Wall -Wextra -O2 -mwindows -o $@ $^

run: $(TARGET)
	cd $(GAME_DIR) && wine ./resurrections.exe $(LAUNCHER_ARGS)

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

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)

.PHONY: all run crashdump install_crashreporter_stub restore_crashreporter clean
