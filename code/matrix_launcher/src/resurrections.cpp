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
#include <exception>
#include <memory>
#include <new>
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
static bool DiagnosticReadableMemoryRange(const void* base, size_t byteCount);
static void LogWordSpanIfReadable(const char* label, const void* base, size_t wordCount);
static const void* DiagnosticClientAbsoluteToPointer(uintptr_t absoluteAddress);
static void LogClientCrashContext();
static const char* DiagnosticExceptionCodeName(DWORD exceptionCode);
static const char* DiagnosticExceptionClassification(DWORD exceptionCode);
static void LogDiagnosticExceptionSnapshot(const char* heading, EXCEPTION_POINTERS* exceptionInfo);
static LONG CALLBACK DiagnosticVectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);
static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
static void DiagnosticEmergencyWrite(const char* text);
static void DiagnosticBadAllocNewHandler();
static void DiagnosticTerminateHandler();

static void DiagnosticEmergencyWrite(const char* text) {
    const char* const message = text ? text : "<null>";
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    HANDLE stderrHandle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderrHandle != INVALID_HANDLE_VALUE && stderrHandle != nullptr) {
        DWORD written = 0;
        const DWORD length = static_cast<DWORD>(std::strlen(message));
        if (length != 0u) {
            WriteFile(stderrHandle, message, length, &written, nullptr);
        }
        WriteFile(stderrHandle, "\n", 1, &written, nullptr);
    }
}

static void DiagnosticBadAllocNewHandler() {
    DiagnosticEmergencyWrite("DiagnosticBadAllocNewHandler: operator new allocation failure");
    throw std::bad_alloc();
}

static void DiagnosticTerminateHandler() {
    DiagnosticEmergencyWrite("DiagnosticTerminateHandler: std::terminate invoked");
    try {
        std::exception_ptr current = std::current_exception();
        if (current) {
            try {
                std::rethrow_exception(current);
            } catch (const std::bad_alloc&) {
                DiagnosticEmergencyWrite("DiagnosticTerminateHandler: current exception is std::bad_alloc");
            } catch (const std::exception& exception) {
                DiagnosticEmergencyWrite("DiagnosticTerminateHandler: current exception is std::exception");
                DiagnosticEmergencyWrite(exception.what());
            } catch (...) {
                DiagnosticEmergencyWrite("DiagnosticTerminateHandler: current exception is non-std");
            }
        } else {
            DiagnosticEmergencyWrite("DiagnosticTerminateHandler: no current exception");
        }
    } catch (...) {
        DiagnosticEmergencyWrite("DiagnosticTerminateHandler: failed while classifying termination exception");
    }
    std::abort();
}

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

static bool DiagnosticReadableMemoryRange(const void* base, size_t byteCount) {
    if (!base || byteCount == 0) {
        return false;
    }

    const uint8_t* cursor = static_cast<const uint8_t*>(base);
    size_t remaining = byteCount;
    while (remaining != 0) {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(cursor, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        if ((mbi.Protect & PAGE_GUARD) != 0 || (mbi.Protect & PAGE_NOACCESS) != 0) {
            return false;
        }

        const bool readable =
            (mbi.Protect & PAGE_READONLY) != 0 ||
            (mbi.Protect & PAGE_READWRITE) != 0 ||
            (mbi.Protect & PAGE_WRITECOPY) != 0 ||
            (mbi.Protect & PAGE_EXECUTE_READ) != 0 ||
            (mbi.Protect & PAGE_EXECUTE_READWRITE) != 0 ||
            (mbi.Protect & PAGE_EXECUTE_WRITECOPY) != 0;
        if (!readable) {
            return false;
        }

        const uintptr_t regionEnd =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + static_cast<uintptr_t>(mbi.RegionSize);
        const uintptr_t current = reinterpret_cast<uintptr_t>(cursor);
        if (regionEnd <= current) {
            return false;
        }
        const size_t covered = static_cast<size_t>(regionEnd - current);
        if (covered >= remaining) {
            return true;
        }
        cursor += covered;
        remaining -= covered;
    }
    return true;
}

static void LogWordSpanIfReadable(const char* label, const void* base, size_t wordCount) {
    if (!label || !base || wordCount == 0) {
        return;
    }
    if (!DiagnosticReadableMemoryRange(base, wordCount * sizeof(uint32_t))) {
        spdlog::info("{} @ {} <unreadable>", label, fmt::ptr(base));
        return;
    }
    LogWordSpan(label, base, wordCount);
}

static const void* DiagnosticClientAbsoluteToPointer(uintptr_t absoluteAddress) {
    // anchor: client.dll is loaded as the real module; crash-time helper converts fixed static-RE
    // addresses back into the live mapped image base instead of assuming 0x62000000 is still the
    // active load address.
    const uint8_t* clientBase =
        reinterpret_cast<const uint8_t*>(g_hClient ? g_hClient : GetModuleHandleA("client.dll"));
    if (!clientBase || absoluteAddress < 0x62000000u) {
        return nullptr;
    }
    return clientBase + (absoluteAddress - 0x62000000u);
}

static void LogClientCrashContext() {
    // anchor: client.dll:0x62001180 = RunClientDLL passes the global ClientShell object rooted at
    // DAT_629ddfc8 into 0x62006c30; client.dll:0x6217f370 also stores `this` into DAT_629e68a8.
    const void* const clientShellSlot = DiagnosticClientAbsoluteToPointer(0x629e68a8u);
    LogWordSpanIfReadable("client DAT_629e68a8 slot", clientShellSlot, 4);

    const void* clientShell = nullptr;
    if (clientShellSlot && DiagnosticReadableMemoryRange(clientShellSlot, sizeof(void*))) {
        clientShell = *static_cast<const void* const*>(clientShellSlot);
    }
    if (clientShell != nullptr) {
        LogWordSpanIfReadable(
            "client shell state18..34",
            static_cast<const uint8_t*>(clientShell) + 0x18,
            8);
        // anchor: client.dll:0x62173bcd reads `this + 0xd0` before the alternate null-vcall family
        // at `0x62173bd9`, so keep that current transition/runtime object visible in crash logs.
        LogWordSpanIfReadable(
            "client shell d0..ec",
            static_cast<const uint8_t*>(clientShell) + 0xd0,
            8);

        const void* currentRuntimeObject = nullptr;
        const void* currentRuntimeVftable = nullptr;
        const void* const d0Field = static_cast<const uint8_t*>(clientShell) + 0xd0;
        if (DiagnosticReadableMemoryRange(d0Field, sizeof(void*))) {
            currentRuntimeObject = *static_cast<const void* const*>(d0Field);
        }
        if (currentRuntimeObject != nullptr) {
            LogWordSpanIfReadable("client shell +0xd0 object", currentRuntimeObject, 8);
            if (DiagnosticReadableMemoryRange(currentRuntimeObject, sizeof(void*))) {
                currentRuntimeVftable = *static_cast<const void* const*>(currentRuntimeObject);
            }
            LogWordSpanIfReadable("client shell +0xd0 vftable", currentRuntimeVftable, 8);
        }
    }

    // Late render-family globals seen in the active d3d9 crash chain:
    // - client.dll:0x62337d70 uses DAT_62a01e5c
    // - client.dll:0x62452780 uses DAT_62a333a4
    // - client.dll:0x62159ef0 / broader frame loop repeatedly use DAT_629f84e8 and DAT_629f1748
    for (const auto& globalInfo : {
             std::pair<const char*, uintptr_t>{"client DAT_629f84e8 slot", 0x629f84e8u},
             std::pair<const char*, uintptr_t>{"client DAT_629f1748 slot", 0x629f1748u},
             std::pair<const char*, uintptr_t>{"client DAT_62a01e5c slot", 0x62a01e5cu},
             std::pair<const char*, uintptr_t>{"client DAT_62a333a4 slot", 0x62a333a4u},
         }) {
        const void* const slot = DiagnosticClientAbsoluteToPointer(globalInfo.second);
        LogWordSpanIfReadable(globalInfo.first, slot, 4);
        const void* pointedObject = nullptr;
        if (slot && DiagnosticReadableMemoryRange(slot, sizeof(void*))) {
            pointedObject = *static_cast<const void* const*>(slot);
        }
        if (pointedObject != nullptr) {
            std::string label = std::string(globalInfo.first) + " -> object";
            LogWordSpanIfReadable(label.c_str(), pointedObject, 8);
        }
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
    LogClientCrashContext();
    DiagnosticLogLastD3DDeviceActivity();
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
    std::set_new_handler(DiagnosticBadAllocNewHandler);
    std::set_terminate(DiagnosticTerminateHandler);
    AddVectoredExceptionHandler(1, DiagnosticVectoredExceptionHandler);
    SetUnhandledExceptionFilter(DiagnosticUnhandledExceptionFilter);

    g_CrtArgc = argc;
    g_CrtArgv = argv;

    spdlog::info("Mode: original startup order, no client-memory injection");
    spdlog::info("Default branch target: nopatch path");
    spdlog::info("Installed diagnostic std::new_handler/std::terminate hooks");

    if (!PatchClientDllMxowrapImportToDbghelp()) {
        return FinishAndReturn(1);
    }

    return FinishAndReturn(g_Launcher.InitInstance() ? 0 : 1);
}
