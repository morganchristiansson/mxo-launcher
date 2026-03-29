/**
 * Matrix Online launcher reimplementation scaffold.
 *
 * Goal:
 * - follow the original launcher.exe startup order as closely as current static
 *   knowledge allows
 * - do NOT inject into client.dll memory
 * - do NOT treat ad-hoc NULL-heavy InitClientDLL calls as the canonical path
 */

#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <sys/stat.h>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "diagnostics.h"
#include "../matrixstaging/runtime/src/libltbase/launchercommandline.h"
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

extern HMODULE g_hCres;
extern HMODULE g_hClient;
extern int g_CrtArgc;
extern char** g_CrtArgv;

// Launcher-owned command-line preprocessing now lives in a dedicated recovered model.
extern mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;

// Launcher-owned startup/auth state outside the command-line parser.
extern CLauncher g_Launcher;                    // original global object: [0x4d3368]
extern void* g_pLauncherObject6304;            // original: [0x4d6304]
extern void* g_pILTLoginMediatorDefault;       // original: [0x4d2c58]
extern void* g_pILTLoginMediatorSelection3584; // original sibling slot: [0x4d3584]
extern uint32_t g_PackedArg7Selection;         // packed from [CLauncher+0xa8]/[CLauncher+0xac]
extern uint32_t g_FlagByte;                    // original: [0x4d2c69]
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


#ifndef DBG_PRINTEXCEPTION_C
#define DBG_PRINTEXCEPTION_C ((DWORD)0x40010006)
#endif
#ifndef DBG_PRINTEXCEPTION_WIDE_C
#define DBG_PRINTEXCEPTION_WIDE_C ((DWORD)0x4001000A)
#endif

static constexpr DWORD kMsvcThreadNameException = 0x406d1388u;

static void LogWordSpan(const char* label, const void* base, size_t wordCount);
static const char* DiagnosticExceptionCodeName(DWORD exceptionCode);
static const char* DiagnosticExceptionClassification(DWORD exceptionCode);
static void LogDiagnosticExceptionSnapshot(const char* heading, EXCEPTION_POINTERS* exceptionInfo);
static LONG CALLBACK DiagnosticVectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);
static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);

static void LogWordSpan(const char* label, const void* base, size_t wordCount) {
    const uint32_t* words = static_cast<const uint32_t*>(base);
    if (!label || !base || wordCount == 0) return;
    for (size_t i = 0; i < wordCount; i += 4) {
        spdlog::info(
            "{} @ {} [+0x{:02x}]=0x{:08x} [+0x{:02x}]=0x{:08x} [+0x{:02x}]=0x{:08x} [+0x{:02x}]=0x{:08x}",
            label,
            fmt::ptr(base),
            static_cast<unsigned>(i * 4),
            words[i + 0],
            static_cast<unsigned>((i + 1) * 4),
            (i + 1 < wordCount) ? words[i + 1] : 0,
            static_cast<unsigned>((i + 2) * 4),
            (i + 2 < wordCount) ? words[i + 2] : 0,
            static_cast<unsigned>((i + 3) * 4),
            (i + 3 < wordCount) ? words[i + 3] : 0);
    }
}

static const char* DiagnosticExceptionCodeName(DWORD exceptionCode) {
    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        case DBG_PRINTEXCEPTION_C: return "DBG_PRINTEXCEPTION_C";
        case DBG_PRINTEXCEPTION_WIDE_C: return "DBG_PRINTEXCEPTION_WIDE_C";
        default: break;
    }

    if (exceptionCode == kMsvcThreadNameException) {
        return "MSVC_THREAD_NAME_EXCEPTION";
    }

    return "unknown";
}

static const char* DiagnosticExceptionClassification(DWORD exceptionCode) {
    if (exceptionCode == kMsvcThreadNameException) {
        return "debugger thread-name notification";
    }
    if (exceptionCode == DBG_PRINTEXCEPTION_C || exceptionCode == DBG_PRINTEXCEPTION_WIDE_C) {
        return "debugger print notification";
    }
    return "ordinary exception";
}

static void LogDiagnosticExceptionSnapshot(const char* heading, EXCEPTION_POINTERS* exceptionInfo) {
    EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
    CONTEXT* context = exceptionInfo->ContextRecord;

    spdlog::info("=== {} ===", heading);
    spdlog::info(
        "exception code=0x{:08x} ({}) classification={} flags=0x{:08x} address={}",
        static_cast<unsigned long>(record->ExceptionCode),
        DiagnosticExceptionCodeName(record->ExceptionCode),
        DiagnosticExceptionClassification(record->ExceptionCode),
        static_cast<unsigned long>(record->ExceptionFlags),
        fmt::ptr(record->ExceptionAddress));
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        const char* accessKind = "unknown";
        if (record->ExceptionInformation[0] == 0) accessKind = "read";
        else if (record->ExceptionInformation[0] == 1) accessKind = "write";
        else if (record->ExceptionInformation[0] == 8) accessKind = "execute";
        spdlog::info(
            "access violation kind={} target={}",
            accessKind,
            fmt::ptr(reinterpret_cast<void*>(record->ExceptionInformation[1])));
    }
    spdlog::info(
        "registers: eip=0x{:08x} esp=0x{:08x} ebp=0x{:08x} eax=0x{:08x} ebx=0x{:08x} ecx=0x{:08x} edx=0x{:08x} esi=0x{:08x} edi=0x{:08x}",
        static_cast<unsigned long>(context->Eip),
        static_cast<unsigned long>(context->Esp),
        static_cast<unsigned long>(context->Ebp),
        static_cast<unsigned long>(context->Eax),
        static_cast<unsigned long>(context->Ebx),
        static_cast<unsigned long>(context->Ecx),
        static_cast<unsigned long>(context->Edx),
        static_cast<unsigned long>(context->Esi),
        static_cast<unsigned long>(context->Edi));

    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(reinterpret_cast<const void*>(context->Eip), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        spdlog::info(
            "eip page: base={} allocBase={} regionSize=0x{:08x} protect=0x{:08x} state=0x{:08x} type=0x{:08x}",
            fmt::ptr(mbi.BaseAddress),
            fmt::ptr(mbi.AllocationBase),
            static_cast<unsigned long>(mbi.RegionSize),
            static_cast<unsigned long>(mbi.Protect),
            static_cast<unsigned long>(mbi.State),
            static_cast<unsigned long>(mbi.Type));
    }

    LogWordSpan("exception stack", reinterpret_cast<const void*>(context->Esp), 16);
    if (g_LauncherCommandLine.FilteredArgv()) {
        LogWordSpan("current arg2 filteredArgv", g_LauncherCommandLine.FilteredArgv(), 4);
    }
    if (g_pLauncherObject6304) {
        LogWordSpan("current arg5 launcherObject", g_pLauncherObject6304, 8);
    }
    if (g_pILTLoginMediatorDefault) {
        LogWordSpan("current arg6 mediator", g_pILTLoginMediatorDefault, 8);
    }
}

static LONG CALLBACK DiagnosticVectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) {
        spdlog::info("DIAGNOSTIC: vectored exception handler invoked with incomplete state");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    LogDiagnosticExceptionSnapshot("Vectored exception (continuing search)", exceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) {
        spdlog::info("DIAGNOSTIC: unhandled exception filter invoked with incomplete state");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    LogDiagnosticExceptionSnapshot("Unhandled exception", exceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

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

    spdlog::info(
        "DIAGNOSTIC: pre-client environment scaffold active threadHandle={} threadId=0x{:08x} state44={} state45={} state48={}",
        fmt::ptr(g_PreclientEnvironment.threadHandle),
        g_PreclientEnvironment.threadId,
        g_PreclientEnvironment.readyFlag44,
        g_PreclientEnvironment.readyFlag45,
        fmt::ptr(g_PreclientEnvironment.readyPointer48));
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

static void UnloadClientLibraries() {
    g_Launcher.UnloadClientDLL();
    g_Launcher.UnloadCresDLL();
}

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
    DiagnosticShutdownPreclientEnvironment();
    g_Launcher.CleanupRecoveredInitClientState();
    UnloadClientLibraries();
    g_LauncherCommandLine.Reset();
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
    AddVectoredExceptionHandler(1, DiagnosticVectoredExceptionHandler);
    SetUnhandledExceptionFilter(DiagnosticUnhandledExceptionFilter);

    g_CrtArgc = argc;
    g_CrtArgv = argv;

    spdlog::info("Mode: original startup order, no client-memory injection");
    spdlog::info("Default branch target: nopatch path");

    if (!PatchClientDllMxowrapImportToDbghelp()) {
        return FinishAndReturn(1);
    }

    return FinishAndReturn(g_Launcher.InitInstance() ? 0 : 1);
}
