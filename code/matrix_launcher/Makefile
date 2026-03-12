# Matrix Online launcher reimplementation Makefile

CXX = i686-w64-mingw32-g++
GAME_DIR = ~/MxO_7.6005
TARGET = $(GAME_DIR)/resurrections.exe
CRASHREPORTER_STUB = $(GAME_DIR)/crashreporter.stub.exe
SRC = src/resurrections.cpp src/diagnostics.cpp \
	src/liblttcp/lttcpconnection.cpp src/liblttcp/ltthreadperclienttcpengine.cpp \
	src/liblttcp/cmessageconnection.cpp
CRASHREPORTER_STUB_SRC = tools/crashreporter_stub.cpp

-include secrets/launcher_login.local.mk

LAUNCHER_ARGS = $(if $(MXO_USER),-user $(MXO_USER)) $(if $(MXO_PASS),-pwd $(MXO_PASS)) $(if $(MXO_SESSION),-session $(MXO_SESSION)) $(if $(MXO_CHAR),-char $(MXO_CHAR))

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) -Wall -Wextra -O2 -o $@ $^ -Wl,--export-all-symbols -lversion

$(CRASHREPORTER_STUB): $(CRASHREPORTER_STUB_SRC)
	$(CXX) -Wall -Wextra -O2 -mwindows -o $@ $^

run: $(TARGET)
	cd $(GAME_DIR) && wine ./resurrections.exe $(LAUNCHER_ARGS)

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

.PHONY: all run crashdump run_incomplete_init run_forced_runtime run_stub_mediator run_binder_mediator run_stub_both run_binder_both install_crashreporter_stub restore_crashreporter clean
