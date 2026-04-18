/**
 * Matrix Online launcher reimplementation scaffold.
 *
 * Goal:
 * - follow the original launcher.exe startup order as closely as current static
 *   knowledge allows
 * - do NOT inject into client.dll memory
 * - do NOT treat ad-hoc NULL-heavy InitClientDLL calls as the canonical path
 */

#include <winsock2.h>
#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <sys/stat.h>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "diagnostics.h"
#include "../matrixstaging/game/src/launcher/launcher.h"

// Include the login mediator header for world list builder access
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"

namespace mxo {
namespace ltlogin {
    using namespace std;
}
}

#define DLLEXPORT __declspec(dllexport)

using mxo::launcher::CLauncher;

extern int g_CrtArgc;
extern char** g_CrtArgv;
extern char** g_LauncherFilteredArgv;          // original: [0x4d2c60]

// Launcher-owned startup/auth state outside the command-line parser.
extern CLauncher g_Launcher;                    // original global object: [0x4d3368]
extern void* g_pLauncherObject6304;            // original: [0x4d6304]
extern void* g_pILTLoginMediatorDefault;       // original: [0x4d2c58]
extern void* g_pILTLoginMediatorSelection3584; // original sibling slot: [0x4d3584]
extern char g_LastWorldName[256];              // original registry value: Last_WorldName

struct DiagnosticPreclientEnvironmentState {
    HANDLE threadHandle;
    DWORD threadId;
    HANDLE readyEvent;
    HANDLE stopEvent;
    volatile LONG readyFlag44;
    volatile LONG readyFlag45;
    void* readyPointer48;
};

static DiagnosticPreclientEnvironmentState g_PreclientEnvironment = {};

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase);

static DWORD WINAPI DiagnosticPreclientThreadProc(LPVOID) {
    MSG msg = {};
    PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE);

    g_PreclientEnvironment.readyPointer48 = &g_PreclientEnvironment;
    InterlockedExchange(&g_PreclientEnvironment.readyFlag44, 0);
    InterlockedExchange(&g_PreclientEnvironment.readyFlag45, 1);
    SetEvent(g_PreclientEnvironment.readyEvent);
    spdlog::info(
        "DIAGNOSTIC: pre-client launcher thread ready threadId=0x{:08x} state44={} state45={} state48={}",
        GetCurrentThreadId(),
        g_PreclientEnvironment.readyFlag44,
        g_PreclientEnvironment.readyFlag45,
        fmt::ptr(g_PreclientEnvironment.readyPointer48));

    while (WaitForSingleObject(g_PreclientEnvironment.stopEvent, 10) == WAIT_TIMEOUT) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    spdlog::info("DIAGNOSTIC: pre-client launcher thread stopping");
    return 0;
}

bool DiagnosticInitializePreclientEnvironmentLike402EC0() {
    if (g_PreclientEnvironment.threadHandle) {
        return true;
    }

    std::memset(&g_PreclientEnvironment, 0, sizeof(g_PreclientEnvironment));
    g_PreclientEnvironment.readyEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_PreclientEnvironment.stopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_PreclientEnvironment.readyEvent || !g_PreclientEnvironment.stopEvent) {
        spdlog::info("DIAGNOSTIC: pre-client environment event creation failed ({})", (unsigned long)GetLastError());
        return false;
    }

    g_PreclientEnvironment.threadHandle = CreateThread(
        NULL,
        0,
        DiagnosticPreclientThreadProc,
        NULL,
        0,
        &g_PreclientEnvironment.threadId);
    if (!g_PreclientEnvironment.threadHandle) {
        spdlog::info("DIAGNOSTIC: pre-client environment thread creation failed ({})", (unsigned long)GetLastError());
        return false;
    }

    DWORD waitResult = WaitForSingleObject(g_PreclientEnvironment.readyEvent, 5000);
    if (waitResult != WAIT_OBJECT_0) {
        spdlog::info("DIAGNOSTIC: pre-client environment readiness wait failed ({})", (unsigned long)waitResult);
        return false;
    }

    return true;
}

void DiagnosticShutdownPreclientEnvironment() {
    if (g_PreclientEnvironment.stopEvent) {
        SetEvent(g_PreclientEnvironment.stopEvent);
    }
    if (g_PreclientEnvironment.threadHandle) {
        WaitForSingleObject(g_PreclientEnvironment.threadHandle, 1000);
        CloseHandle(g_PreclientEnvironment.threadHandle);
        g_PreclientEnvironment.threadHandle = NULL;
    }
    if (g_PreclientEnvironment.readyEvent) {
        CloseHandle(g_PreclientEnvironment.readyEvent);
        g_PreclientEnvironment.readyEvent = NULL;
    }
    if (g_PreclientEnvironment.stopEvent) {
        CloseHandle(g_PreclientEnvironment.stopEvent);
        g_PreclientEnvironment.stopEvent = NULL;
    }
    g_PreclientEnvironment.threadId = 0;
    g_PreclientEnvironment.readyFlag44 = 0;
    g_PreclientEnvironment.readyFlag45 = 0;
    g_PreclientEnvironment.readyPointer48 = NULL;
}

bool IsRecoveredPreclientEnvironmentActive() {
    return g_PreclientEnvironment.threadHandle != NULL;
}

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
    DiagnosticShutdownPreclientEnvironment();
    return code;
}

bool PatchClientDllMxowrapImportToDbghelp() {
    // UNANCHORED: replacement-only startup patch, not part of faithful launcher.exe behavior.
    static constexpr long kClientDllImportNameOffset = 0x978d76L;
    static constexpr char kImportName[] = "dbghelp.dll";

    FILE* file = std::fopen("client.dll", "r+b");
    if (!file) {
        spdlog::info("ERROR: unable to open client.dll for startup import patch");
        return false;
    }
    if (std::fseek(file, kClientDllImportNameOffset, SEEK_SET) != 0) {
        std::fclose(file);
        spdlog::info(
            "ERROR: failed to seek client.dll to startup import patch offset 0x{:x}",
            static_cast<unsigned>(kClientDllImportNameOffset));
        return false;
    }
    if (std::fwrite(kImportName, 1, sizeof(kImportName) - 1, file) != sizeof(kImportName) - 1) {
        std::fclose(file);
        spdlog::info(
            "ERROR: failed to write client.dll startup import patch at 0x{:x}",
            static_cast<unsigned>(kClientDllImportNameOffset));
        return false;
    }
    std::fclose(file);
    spdlog::info(
        "DIAGNOSTIC: wrote client.dll startup import patch at 0x{:x} -> '{}'",
        static_cast<unsigned>(kClientDllImportNameOffset),
        kImportName);
    return true;
}

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    spdlog::info("launcher export SetMasterDatabase called: {}", fmt::ptr(pMasterDatabase));
}

static std::shared_ptr<spdlog::logger> RegisterSharedSinkLogger(
    const std::shared_ptr<spdlog::logger>& baseLogger,
    const char* loggerName) {
    if (!baseLogger || !loggerName || !loggerName[0]) {
        return nullptr;
    }

    std::shared_ptr<spdlog::logger> existingLogger = spdlog::get(loggerName);
    if (existingLogger) {
        return existingLogger;
    }

    std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(
        loggerName,
        baseLogger->sinks().begin(),
        baseLogger->sinks().end());
    spdlog::register_logger(logger);
    return logger;
}

static void InitializeLogging() {
    try {
        std::shared_ptr<spdlog::logger> logger =
            spdlog::basic_logger_mt("resurrections", "resurrections.log", true);
        spdlog::set_default_logger(logger);

        // Per-logger SPDLOG_LEVEL overrides only affect call sites that explicitly log through
        // these named loggers instead of bare spdlog::info/debug/... helpers.
        std::shared_ptr<spdlog::logger> authReceiveLogger =
            RegisterSharedSinkLogger(logger, "AuthReceivePacket");
        std::shared_ptr<spdlog::logger> marginReceiveLogger =
            RegisterSharedSinkLogger(logger, "MarginReceivePacket");

        // Default fallback levels when SPDLOG_LEVEL does not override them:
        // - resurrections stays at debug
        // - the hot receive-path loggers stay quieter at warn unless explicitly raised
        logger->set_level(spdlog::level::debug);
        if (authReceiveLogger) {
            authReceiveLogger->set_level(spdlog::level::warn);
        }
        if (marginReceiveLogger) {
            marginReceiveLogger->set_level(spdlog::level::warn);
        }

        spdlog::cfg::load_env_levels();
        spdlog::flush_on(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex&) {
    }
}

int main(int argc, char* argv[]) {
    InitializeLogging();

    g_CrtArgc = argc;
    g_CrtArgv = argv;



    if (!PatchClientDllMxowrapImportToDbghelp()) {
        return FinishAndReturn(1);
    }

    return FinishAndReturn(g_Launcher.InitInstance() ? 0 : 1);
}
