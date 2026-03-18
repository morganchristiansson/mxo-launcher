# Matrix Online launcher reimplementation Makefile

CXX = i686-w64-mingw32-g++-posix
AR ?= i686-w64-mingw32-ar
RANLIB ?= i686-w64-mingw32-ranlib
GAME_DIR = ~/MxO_7.6005
TARGET = $(GAME_DIR)/resurrections.exe
CRASHREPORTER_STUB = $(GAME_DIR)/crashreporter.stub.exe

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
COMMON_AUTH_SRC = \
	matrixstaging/runtime/src/libltmessaging/variablelengthprefixedtcpstreamparser.cpp \
	matrixstaging/runtime/src/libltcrypto/filters.cpp \
	matrixstaging/runtime/src/libltcrypto/sessionkeyencryption.cpp

LAUNCHER_SRC = src/resurrections.cpp src/diagnostics.cpp src/diagnostics_auth.cpp src/launcher_mediator_abi.cpp src/launcher_network_object_abi.cpp \
	matrixstaging/runtime/src/liblttcp/lttcpconnection.cpp \
	matrixstaging/runtime/src/liblttcp/ltthreadperclienttcpengine.cpp \
	matrixstaging/runtime/src/libltmessaging/messageconnection.cpp \
	matrixstaging/game/src/libltclientlogin/loginmediator.cpp \
	matrixstaging/game/src/libltclientlogin/loginstate.cpp $(COMMON_AUTH_SRC)
CRASHREPORTER_STUB_SRC = tools/crashreporter_stub.cpp
AUTH_PROBE_SRC = tools/auth_probe.cpp $(COMMON_AUTH_SRC)

BUILD_DIR = build
AUTH_PROBE = $(GAME_DIR)/auth_probe.exe
CRYPTOPP_LIB = $(BUILD_DIR)/libcryptopp.a
SPDLOG_LIB = $(BUILD_DIR)/libspdlog.a

-include secrets/launcher_login.local.mk

LAUNCHER_ARGS = $(if $(MXO_USER),-user $(MXO_USER)) $(if $(MXO_PASS),-pwd $(MXO_PASS)) $(if $(MXO_SESSION),-session $(MXO_SESSION)) $(if $(MXO_CHAR),-char $(MXO_CHAR))

all: $(TARGET)

auth_probe: $(AUTH_PROBE)

CXXFLAGS = -Wall -Wextra -O2 -I$(CRYPTOPP_DIR) -I$(SPDLOG_DIR)/include -I$(CURDIR)/matrixstaging/game/src/libltclientlogin -I$(CURDIR)/matrixstaging/runtime/src/liblttcp -I$(CURDIR)/src/liblttcp

$(TARGET): $(LAUNCHER_SRC) $(CRYPTOPP_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(LAUNCHER_SRC) $(CRYPTOPP_LIB) -Wl,--export-all-symbols -lversion -lws2_32

$(CRASHREPORTER_STUB): $(CRASHREPORTER_STUB_SRC)
	$(CXX) -Wall -Wextra -O2 -mwindows -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(CRYPTOPP_LIB): Makefile
	@mkdir -p $(dir $@)
	$(MAKE) -C $(CRYPTOPP_DIR) CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" static
	cp $(CRYPTOPP_DIR)/libcryptopp.a $@

# spdlog is header-only, included via CXXFLAGS. No build step needed.

$(AUTH_PROBE): $(AUTH_PROBE_SRC) $(CRYPTOPP_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(AUTH_PROBE_SRC) $(CRYPTOPP_LIB) -Wl,--export-all-symbols -lversion -lws2_32

run: $(TARGET)
	cd $(GAME_DIR) && wine ./resurrections.exe $(LAUNCHER_ARGS)

run_auth_probe: $(AUTH_PROBE)
	@cd $(GAME_DIR) && MXO_AUTH_PROBE_USERNAME="$(or $(MXO_AUTH_PROBE_USERNAME),$(MXO_USER))" \
	MXO_AUTH_PROBE_PASSWORD="$(or $(MXO_AUTH_PROBE_PASSWORD),$(MXO_PASS))" \
	wine ./auth_probe.exe

crashdump:
	@latest=$$(cd $(GAME_DIR) && ls -t MatrixOnline_0.0_crash_*.dmp 2>/dev/null | head -1); \
	if [ -z "$$latest" ]; then \
		echo "No crash dump found in $(GAME_DIR)"; \
		exit 1; \
	fi; \
	echo "Latest crash dump: $(GAME_DIR)/$$latest"; \
	cd $(GAME_DIR) && printf 'info reg\nbt\ninfo share\nquit\n' | winedbg "$$latest"

run_incomplete_init: $(TARGET)
	cd $(GAME_DIR) && MXO_FORCE_INCOMPLETE_INIT=1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_forced_runtime: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		MXO_FORCE_RUNCLIENT=1 \
		MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE=1 \
		wine ./resurrections.exe $(LAUNCHER_ARGS)

run_stub_mediator: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_STUB_LOGIN_MEDIATOR=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		wine ./resurrections.exe $(LAUNCHER_ARGS)

run_binder_mediator: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_BINDER_LOGIN_MEDIATOR=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		wine ./resurrections.exe $(LAUNCHER_ARGS)

run_stub_both: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_STUB_LOGIN_MEDIATOR=1 \
		MXO_STUB_LAUNCHER_OBJECT=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		wine ./resurrections.exe $(LAUNCHER_ARGS)

run_binder_both: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_BINDER_LOGIN_MEDIATOR=1 \
		MXO_STUB_LAUNCHER_OBJECT=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		wine ./resurrections.exe $(LAUNCHER_ARGS)

install_crashreporter_stub: $(CRASHREPORTER_STUB)
	cd $(GAME_DIR) && \
		if [ ! -f crashreporter.exe.original ]; then mv crashreporter.exe crashreporter.exe.original; fi && \
		cp crashreporter.stub.exe crashreporter.exe

restore_crashreporter:
	cd $(GAME_DIR) && \
		if [ -f crashreporter.exe.original ]; then mv -f crashreporter.exe.original crashreporter.exe; fi && \
		rm -f crashreporter.stub.exe

clean:
	rm -f $(TARGET) $(CRASHREPORTER_STUB)
	rm -rf $(BUILD_DIR)

.PHONY: all auth_probe run run_auth_probe crashdump run_incomplete_init run_forced_runtime run_stub_mediator run_binder_mediator run_stub_both run_binder_both install_crashreporter_stub restore_crashreporter clean
