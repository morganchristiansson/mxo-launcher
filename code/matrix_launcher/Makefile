# Matrix Online launcher reimplementation Makefile

CXX = i686-w64-mingw32-g++
GAME_DIR = ~/MxO_7.6005
TARGET = $(GAME_DIR)/resurrections.exe
SRC = src/resurrections.cpp src/diagnostics.cpp

-include secrets/launcher_login.local.mk

LAUNCHER_ARGS = $(if $(MXO_USER),$(MXO_USER)) $(if $(MXO_PASS),$(MXO_PASS))

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) -Wall -Wextra -O2 -o $@ $^ -Wl,--export-all-symbols

run: $(TARGET)
	cd $(GAME_DIR) && DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

crashdump:
	@latest=$$(cd $(GAME_DIR) && ls -t MatrixOnline_0.0_crash_*.dmp 2>/dev/null | head -1); \
	if [ -z "$$latest" ]; then \
		echo "No crash dump found in $(GAME_DIR)"; \
		exit 1; \
	fi; \
	echo "Latest crash dump: $(GAME_DIR)/$$latest"; \
	cd $(GAME_DIR) && printf 'info reg\nbt\ninfo share\nquit\n' | DISPLAY=:1 winedbg "$$latest"

run_incomplete_init: $(TARGET)
	cd $(GAME_DIR) && MXO_FORCE_INCOMPLETE_INIT=1 DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_forced_runtime: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		MXO_FORCE_RUNCLIENT=1 \
		MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE=1 \
		DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_stub_mediator: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_STUB_LOGIN_MEDIATOR=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_binder_mediator: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_BINDER_LOGIN_MEDIATOR=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_stub_both: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_STUB_LOGIN_MEDIATOR=1 \
		MXO_STUB_LAUNCHER_OBJECT=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

run_binder_both: $(TARGET)
	cd $(GAME_DIR) && \
		MXO_BINDER_LOGIN_MEDIATOR=1 \
		MXO_STUB_LAUNCHER_OBJECT=1 \
		MXO_FORCE_INCOMPLETE_INIT=1 \
		DISPLAY=:1 wine ./resurrections.exe $(LAUNCHER_ARGS)

clean:
	rm -f $(TARGET)

.PHONY: all run crashdump run_incomplete_init run_forced_runtime run_stub_mediator run_binder_mediator run_stub_both run_binder_both clean
