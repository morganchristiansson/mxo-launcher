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

// Include the login mediator header for world list builder access
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"

namespace mxo {
namespace ltlogin {
    using namespace std;
}
}

#define DLLEXPORT __declspec(dllexport)

using InitClientDLLFunc = int (*)(
    uint32_t filteredArgCount,
    char** filteredArgv,
    HMODULE hClientDll,
    HMODULE hCresDll,
    void* launcherNetworkObject,
    void* pILTLoginMediatorDefault,
    uint32_t packedArg7Selection,
    uint32_t flagByte);

using RunClientDLLFunc = int (*)();
using TermClientDLLFunc = int (*)();
using ErrorClientDLLFunc = const char* (*)();

struct RecoveredLauncherStartupContext;

class CLauncher {
public:
    // anchor: launcher.exe:0x4097f0
    // This replacement class intentionally does not derive from MFC yet.
    // It only preserves the currently useful recovered field tail and startup helpers.
    uint8_t m_RecoveredBaseToA3[0xa4] = {};
    uint32_t m_FieldA4 = 0;          // original: [this+0xa4]
    uint32_t m_FieldA8 = 0xffffffff; // original: [this+0xa8], high 8 bits feed InitClientDLL arg7
    uint32_t m_FieldAC = 0;          // original: [this+0xac], low 24 bits feed InitClientDLL arg7
    uint8_t m_FieldB0 = 0;           // original: [this+0xb0]

    // anchor: launcher.exe:0x40b430
    bool InitInstance();

    // anchor: launcher.exe:0x409950 / 0x4173d0
    bool ParseCommandLineStage() const;

    // anchor: launcher.exe:0x40a780
    bool LoadCresDLL() const;

    // anchor: launcher.exe:0x40a420
    bool LoadClientDLL() const;

    // anchor: launcher.exe:0x40a760
    void UnloadClientDLL() const;

    // anchor: launcher.exe:0x40a7a0
    void UnloadCresDLL() const;

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    bool BuildRecoveredStartupContext(RecoveredLauncherStartupContext* startupContext);

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    bool PrepareRecoveredInitClientState(const RecoveredLauncherStartupContext& startupContext);

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    void LogOriginalPathGapSummary() const;

    uint32_t BuildPackedArg7Selection() const;
    bool RunClientDllLifecycle() const;
};

static_assert(offsetof(CLauncher, m_FieldA4) == 0xa4, "CLauncher +0xa4 drifted");
static_assert(offsetof(CLauncher, m_FieldA8) == 0xa8, "CLauncher +0xa8 drifted");
static_assert(offsetof(CLauncher, m_FieldAC) == 0xac, "CLauncher +0xac drifted");
static_assert(offsetof(CLauncher, m_FieldB0) == 0xb0, "CLauncher +0xb0 drifted");

static HMODULE g_hCres = NULL;
static HMODULE g_hClient = NULL;
static int g_CrtArgc = 0;
static char** g_CrtArgv = NULL;

// Launcher-owned command-line preprocessing now lives in a dedicated recovered model.
static mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;

// Launcher-owned startup/auth state outside the command-line parser.
static CLauncher g_Launcher;                     // original global object: [0x4d3368]
static void* g_pLauncherObject6304 = NULL;      // original: [0x4d6304]
static void* g_pILTLoginMediatorDefault = NULL; // original: [0x4d2c58]
static void* g_pILTLoginMediatorSelection3584 = NULL; // original sibling slot: [0x4d3584]
static uint32_t g_PackedArg7Selection = 0;      // packed from [CLauncher+0xa8]/[CLauncher+0xac]
static uint32_t g_FlagByte = 0;                 // original: [0x4d2c69]
static char g_LastWorldName[256] = {0};         // original registry value: Last_WorldName

static const char* kLauncherRegistryKeyPath = "Software\\Monolith Productions\\The Matrix Online\\";

struct RecoveredLauncherSelectionRecord {
    const char* worldName;
    const char* routeHostPrefix;
    uint32_t worldIndexLow24;
    uint32_t variantIndexHigh8;
    uint32_t selectionGateByte100;
    uint32_t variantState;
};

static const RecoveredLauncherSelectionRecord kRecoveredLauncherSelectionRecords[] = {
    // Current live bounded-evidence entry for the first in-game replacement path:
    // - selection name = Reality
    // - arg7 packed selection = 0x0500002a
    // - default route host prefix = reality
    {"Reality", "reality", 0x00002au, 0x05u, 1u, 0u},
};

struct RecoveredLauncherStartupContext {
    char mediatorSelectionName[64];
    const RecoveredLauncherSelectionRecord* recoveredSelection;
    uint32_t mediatorSelectedSelectionGateByte100;
    uint32_t mediatorSelectedVariantState;
    uint32_t nopatchLauncherVersionValue;
    uint32_t nopatchClientVersionValue;
};

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
static void LogKnownStartupState();

template <typename T>
static T ResolveProc(HMODULE module, const char* name) {
    FARPROC proc = GetProcAddress(module, name);
    T typed = nullptr;
    static_assert(sizeof(typed) == sizeof(proc), "function pointer size mismatch");
    std::memcpy(&typed, &proc, sizeof(typed));
    return typed;
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

static void LowercaseAsciiCopy(char* destination, size_t destinationSize, const char* source) {
    if (!destination || destinationSize == 0) return;
    destination[0] = '\0';
    if (!source) return;

    size_t write = 0;
    for (size_t i = 0; source[i] && write + 1 < destinationSize; ++i) {
        unsigned char c = static_cast<unsigned char>(source[i]);
        if (c >= 'A' && c <= 'Z') {
            destination[write++] = static_cast<char>(c - 'A' + 'a');
        } else {
            destination[write++] = static_cast<char>(c);
        }
    }
    destination[write] = '\0';
}

static const char* MaskedArgValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

static void LogLauncherPreprocessingState() {
    spdlog::info("=== Launcher switch preprocessing ===");
    spdlog::info("auth username      = {}", MaskedArgValue(g_LauncherCommandLine.AuthUsername()));
    spdlog::info("auth password      = {}", MaskedArgValue(g_LauncherCommandLine.AuthPassword()));
    spdlog::info("launcher character  = {}", MaskedArgValue(g_LauncherCommandLine.LauncherCharacter()));
    spdlog::info("launcher session    = {}", MaskedArgValue(g_LauncherCommandLine.LauncherSession()));
    spdlog::info(
        "launcher flags      = clone:{} silent:{} nopatch:{} recover:{} deletechar:{} justpatch:{} noeula:{} skiplaunch:{} lptest:{}",
        g_LauncherCommandLine.SwitchClone() ? 1 : 0,
        g_LauncherCommandLine.SwitchSilent() ? 1 : 0,
        g_LauncherCommandLine.SwitchNoPatch() ? 1 : 0,
        g_LauncherCommandLine.SwitchRecover() ? 1 : 0,
        g_LauncherCommandLine.SwitchDeleteChar() ? 1 : 0,
        g_LauncherCommandLine.SwitchJustPatch() ? 1 : 0,
        g_LauncherCommandLine.SwitchNoEula() ? 1 : 0,
        g_LauncherCommandLine.SwitchSkipLaunch() ? 1 : 0,
        g_LauncherCommandLine.SwitchLPTest() ? "1" : "0");
    spdlog::info(
        "launcher globals    = 4c8b1c:{} 4c8b1d:{} 4d2c64:{} 4d2c65:{} 4d2c66:{} 4d2c6a:{}",
        g_LauncherCommandLine.LauncherGlobal4C8B1C() ? 1 : 0,
        g_LauncherCommandLine.LauncherGlobal4C8B1D() ? 1 : 0,
        g_LauncherCommandLine.LauncherGlobal4D2C64() ? 1 : 0,
        g_LauncherCommandLine.SwitchRecover() ? 1 : 0,
        g_LauncherCommandLine.SwitchJustPatch() ? 1 : 0,
        g_LauncherCommandLine.SwitchClone() ? 1 : 0);
    if (g_LauncherCommandLine.LauncherGlobal4D2C64()) {
        spdlog::info(
            "launcher autodetect exitCode = {}",
            (unsigned long)g_LauncherCommandLine.AutodetectExitCode());
    }
}

static void RunOptionsCfgAutodetectStepIfNeeded() {
    if (!g_LauncherCommandLine.LauncherGlobal4D2C64()) return;

    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    char commandLine[] = "autodetect_settings.exe setopts hide";

    spdlog::info("=== Original-style options.cfg autodetect step ===");
    spdlog::info("DIAGNOSTIC: launching '{}' with currentDir='.'", commandLine);

    if (!CreateProcessA(
            NULL,
            commandLine,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            ".",
            &startupInfo,
            &processInfo)) {
        spdlog::info(
            "WARNING: CreateProcessA for autodetect_settings.exe failed ({}); continuing original-path scaffold without that side effect",
            GetLastError());
        return;
    }

    DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 60000);
    spdlog::info("DIAGNOSTIC: autodetect wait result = {}", waitResult);

    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
            g_LauncherCommandLine.SetAutodetectExitCode(exitCode);
            spdlog::info(
                "DIAGNOSTIC: autodetect exit code = {} (0x{:08x})",
                static_cast<unsigned long>(exitCode),
                static_cast<unsigned long>(exitCode));
        } else {
            spdlog::info("WARNING: GetExitCodeProcess for autodetect_settings.exe failed ({})", GetLastError());
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        spdlog::info("WARNING: autodetect_settings.exe did not finish within 60000 ms");
    } else {
        spdlog::info("WARNING: WaitForSingleObject for autodetect_settings.exe failed ({})", GetLastError());
    }

    if (processInfo.hThread) CloseHandle(processInfo.hThread);
    if (processInfo.hProcess) CloseHandle(processInfo.hProcess);

    struct _stat optionsStat = {};
    if (_stat("options.cfg", &optionsStat) == 0) {
        spdlog::info("DIAGNOSTIC: autodetect produced options.cfg size={} bytes", optionsStat.st_size);
    } else {
        spdlog::info("DIAGNOSTIC: options.cfg not present after autodetect child returned");
    }

    if (DeleteFileA("options.cfg")) {
        spdlog::info("DIAGNOSTIC: deleted temporary options.cfg after autodetect step");
    } else {
        DWORD deleteError = GetLastError();
        spdlog::info("DIAGNOSTIC: DeleteFileA('options.cfg') failed ({})", (unsigned long)deleteError);
    }
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

static bool DiagnosticInitializePreclientEnvironmentLike402EC0() {
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

static void DiagnosticShutdownPreclientEnvironment() {
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

static void UnloadClientLibraries() {
    g_Launcher.UnloadClientDLL();
    g_Launcher.UnloadCresDLL();
}

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
    DiagnosticShutdownPreclientEnvironment();
    UnloadClientLibraries();
    g_LauncherCommandLine.Reset();
    return code;
}

static bool PatchClientDllMxowrapImportToDbghelp() {
    // Narrow startup-owned file patch from the current local proof:
    // `client.dll.patched` only differs here by replacing `mxowrap.dll` with same-length
    // `dbghelp.dll` at offset `0x978d76`.
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

static bool PreloadDependencies() {
    const char* dlls[] = {
        "MFC71.dll",
        "MSVCR71.dll",
        "dbghelp.dll",
        "r3d9.dll",
        "binkw32.dll",
        NULL
    };

    for (int i = 0; dlls[i]; ++i) {
        HMODULE h = LoadLibraryA(dlls[i]);
        spdlog::info("preload {:12s} : {} ({})", dlls[i], h ? "OK" : "FAIL", fmt::ptr(h));
        if (!h) return false;
    }
    return true;
}

bool CLauncher::LoadCresDLL() const {
    spdlog::info("=== Load cres.dll ===");
    g_hCres = LoadLibraryA("cres.dll");
    spdlog::info("cres.dll handle: {}", fmt::ptr(g_hCres));
    return g_hCres != NULL;
}

bool CLauncher::LoadClientDLL() const {
    if (!PatchClientDllMxowrapImportToDbghelp()) {
        return false;
    }
    spdlog::info("=== Load client.dll ===");
    g_hClient = LoadLibraryA("client.dll");
    spdlog::info("client.dll handle: {}", fmt::ptr(g_hClient));
    return g_hClient != NULL;
}

void CLauncher::UnloadClientDLL() const {
    if (g_hClient) {
        FreeLibrary(g_hClient);
        g_hClient = NULL;
    }
}

void CLauncher::UnloadCresDLL() const {
    if (g_hCres) {
        FreeLibrary(g_hCres);
        g_hCres = NULL;
    }
}

static void LogClientLifecycleFailure(const char* phase, ErrorClientDLLFunc errorClientDLL) {
    const char* clientError = nullptr;
    if (errorClientDLL) {
        clientError = errorClientDLL();
    }

    spdlog::error("{} failed{}{}",
        phase ? phase : "Client DLL lifecycle phase",
        clientError ? ": " : "",
        clientError ? clientError : "");
}

uint32_t CLauncher::BuildPackedArg7Selection() const {
    return (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24);
}

bool CLauncher::RunClientDllLifecycle() const {
    // anchor: launcher.exe:0x40a4d0
    spdlog::info("=== Launcher_RunClientDllLifecycle (0x40a4d0-shaped) ===");

    if (!g_hClient) {
        spdlog::error("client.dll is not loaded before lifecycle dispatch");
        return false;
    }

    const InitClientDLLFunc initClientDLL = ResolveProc<InitClientDLLFunc>(g_hClient, "InitClientDLL");
    const RunClientDLLFunc runClientDLL = ResolveProc<RunClientDLLFunc>(g_hClient, "RunClientDLL");
    const TermClientDLLFunc termClientDLL = ResolveProc<TermClientDLLFunc>(g_hClient, "TermClientDLL");
    const ErrorClientDLLFunc errorClientDLL = ResolveProc<ErrorClientDLLFunc>(g_hClient, "ErrorClientDLL");

    spdlog::info("InitClientDLL  : {}", fmt::ptr(initClientDLL));
    spdlog::info("RunClientDLL   : {}", fmt::ptr(runClientDLL));
    spdlog::info("TermClientDLL  : {}", fmt::ptr(termClientDLL));
    spdlog::info("ErrorClientDLL : {}", fmt::ptr(errorClientDLL));

    if (!initClientDLL || !runClientDLL || !termClientDLL || !errorClientDLL) {
        spdlog::error("launcher.exe:0x40a4d0 requires InitClientDLL/RunClientDLL/TermClientDLL/ErrorClientDLL exports");
        return false;
    }

    const bool allowInitWithCurrentStartupScaffold =
        g_pILTLoginMediatorDefault && g_pLauncherObject6304 && g_pILTLoginMediatorSelection3584;
    if (!allowInitWithCurrentStartupScaffold) {
        spdlog::error("Refusing to call InitClientDLL with incomplete launcher state.");
        return false;
    }

    LogKnownStartupState();
    spdlog::info("");

    const uint32_t packedArg7Selection = BuildPackedArg7Selection();
    g_PackedArg7Selection = packedArg7Selection;

    spdlog::info("=== Calling InitClientDLL with current launcher startup scaffold ===");
    const int initResult = initClientDLL(
        g_LauncherCommandLine.FilteredArgCount(),
        g_LauncherCommandLine.FilteredArgv(),
        g_hClient,
        g_hCres,
        g_pLauncherObject6304,
        g_pILTLoginMediatorDefault,
        packedArg7Selection,
        g_FlagByte);
    spdlog::info("InitClientDLL returned: {}", initResult);
    if (initResult <= 0) {
        LogClientLifecycleFailure("InitClientDLL", errorClientDLL);
        return false;
    }

    // The original 0x40a4d0 helper goes straight from a successful InitClientDLL into RunClientDLL.
    // The replacement launcher keeps this narrow recovered auth handoff in between because the active
    // launcher-owned runtime path still depends on it.
    if (DiagnosticCanBeginAuthConnection()) {
        const uint32_t authConnectResult = DiagnosticBeginAuthConnection();
        spdlog::info("DIAGNOSTIC: post-init auth auto-begin result = 0x{:08x}", (unsigned)authConnectResult);
    }

    spdlog::info("=== Calling RunClientDLL on the active launcher path ===");
    const int runResult = runClientDLL();
    spdlog::info("RunClientDLL returned: {}", runResult);
    if (runResult <= 0) {
        LogClientLifecycleFailure("RunClientDLL", errorClientDLL);
        return false;
    }

    // Keep the current narrow faithful sequencing: positive RunClientDLL success continues into
    // TermClientDLL so the real client owns its normal shutdown persistence path.
    spdlog::info("=== Calling TermClientDLL on the active launcher path ===");
    const int termResult = termClientDLL();
    spdlog::info("TermClientDLL returned: {}", termResult);
    if (termResult <= 0) {
        spdlog::error("TermClientDLL failed.");
        return false;
    }

    return true;
}

static bool LoadLastWorldNameFromRegistry(char* out, DWORD outSize) {
    if (!out || outSize < 2) return false;
    out[0] = '\0';

    HKEY key = NULL;
    LONG openResult = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        kLauncherRegistryKeyPath,
        0,
        KEY_QUERY_VALUE,
        &key);
    if (openResult != ERROR_SUCCESS) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName key open failed ({})", openResult);
        return false;
    }

    DWORD type = 0;
    DWORD size = outSize;
    LONG queryResult = RegQueryValueExA(key, "Last_WorldName", NULL, &type, reinterpret_cast<LPBYTE>(out), &size);
    RegCloseKey(key);
    if (queryResult != ERROR_SUCCESS) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName query failed ({})", queryResult);
        out[0] = '\0';
        return false;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName unexpected registry type {}", type);
        out[0] = '\0';
        return false;
    }

    out[outSize - 1] = '\0';
    spdlog::info("DIAGNOSTIC: loaded HKLM Last_WorldName='{}'", out);
    return out[0] != '\0';
}

static bool StoreLastWorldNameInRegistry(const char* worldName) {
    if (!worldName || !worldName[0]) return false;

    HKEY key = NULL;
    DWORD disposition = 0;
    LONG createResult = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        kLauncherRegistryKeyPath,
        0,
        NULL,
        0,
        KEY_SET_VALUE,
        NULL,
        &key,
        &disposition);
    if (createResult != ERROR_SUCCESS) {
        spdlog::warn("DIAGNOSTIC: HKLM Last_WorldName key create/open for write failed ({})", (long)createResult);
        return false;
    }

    const size_t byteCount = std::strlen(worldName) + 1;
    LONG setResult = RegSetValueExA(
        key,
        "Last_WorldName",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(worldName),
        static_cast<DWORD>(byteCount));
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        spdlog::warn("DIAGNOSTIC: HKLM Last_WorldName write failed ({})", (long)setResult);
        return false;
    }

    spdlog::info(
        "DIAGNOSTIC: persisted HKLM Last_WorldName='{}'{}",
        worldName,
        (disposition == REG_CREATED_NEW_KEY) ? " (created key)" : "");
    return true;
}

static void CanonicalizeLauncherSelectionLookupName(char* destination, size_t destinationSize, const char* source) {
    if (!destination || destinationSize == 0) return;
    destination[0] = '\0';
    if (!source) return;

    while (*source == ' ' || *source == '\t' || *source == '\r' || *source == '\n') {
        ++source;
    }

    size_t sourceLength = std::strlen(source);
    while (sourceLength > 0) {
        const char c = source[sourceLength - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        --sourceLength;
    }

    char trimmed[128] = {0};
    const size_t copyLength = (sourceLength < sizeof(trimmed) - 1) ? sourceLength : (sizeof(trimmed) - 1);
    if (copyLength != 0) {
        std::memcpy(trimmed, source, copyLength);
        trimmed[copyLength] = '\0';
    }
    LowercaseAsciiCopy(destination, destinationSize, trimmed);
}

static const RecoveredLauncherSelectionRecord* FindRecoveredLauncherSelectionRecord(const char* worldName) {
    if (!worldName || !worldName[0]) return NULL;

    char normalizedInput[128] = {0};
    CanonicalizeLauncherSelectionLookupName(normalizedInput, sizeof(normalizedInput), worldName);
    if (!normalizedInput[0]) return NULL;

    for (size_t i = 0; i < sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]); ++i) {
        const RecoveredLauncherSelectionRecord& record = kRecoveredLauncherSelectionRecords[i];

        char normalizedRecordWorld[128] = {0};
        CanonicalizeLauncherSelectionLookupName(normalizedRecordWorld, sizeof(normalizedRecordWorld), record.worldName);
        if (normalizedRecordWorld[0] && std::strcmp(normalizedRecordWorld, normalizedInput) == 0) {
            return &record;
        }

        char normalizedRoutePrefix[128] = {0};
        CanonicalizeLauncherSelectionLookupName(normalizedRoutePrefix, sizeof(normalizedRoutePrefix), record.routeHostPrefix);
        if (normalizedRoutePrefix[0] && std::strcmp(normalizedRoutePrefix, normalizedInput) == 0) {
            return &record;
        }
    }

    return NULL;
}

static void LogArgvContentsAsBytes(const char* label, char** argv, uint32_t count) {
    if (!argv || count == 0) return;
    spdlog::info("{} pointer array @ {}:", label, fmt::ptr(argv));
    for (uint32_t i = 0; i < count && i < 8; ++i) {
        spdlog::info("  argv[{}] = {}", i, fmt::ptr(argv[i]));
    }
    if (argv[0]) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(argv[0]);
        spdlog::info(
            "{} argv[0] data @ {}: {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
            label,
            fmt::ptr(argv[0]),
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7]);
        spdlog::info("{} argv[0] as string: '{}'", label, argv[0]);
    }
}

static void LogKnownStartupState() {
    spdlog::info("=== Known startup frame ===");
    spdlog::info("arg1 filteredArgCount        = 0x{:08x}", g_LauncherCommandLine.FilteredArgCount());
    spdlog::info("arg2 filteredArgv            = {}", fmt::ptr(g_LauncherCommandLine.FilteredArgv()));
    if (g_LauncherCommandLine.FilteredArgv()) {
        LogArgvContentsAsBytes(
            "arg2",
            g_LauncherCommandLine.FilteredArgv(),
            g_LauncherCommandLine.FilteredArgCount());
    }
    spdlog::info("arg3 hClientDll              = {}", fmt::ptr(g_hClient));
    spdlog::info("arg4 hCresDll                = {}", fmt::ptr(g_hCres));
    spdlog::info("arg5 launcherNetworkObject   = {}", fmt::ptr(g_pLauncherObject6304));
    spdlog::info("arg6 ILTLoginMediatorDefault = {}", fmt::ptr(g_pILTLoginMediatorDefault));
    spdlog::info("CLauncher+0xa8 placeholder   = 0x{:08x}", g_Launcher.m_FieldA8);
    spdlog::info("CLauncher+0xac placeholder   = 0x{:08x}", g_Launcher.m_FieldAC);
    spdlog::info("Last_WorldName               = {}", g_LastWorldName[0] ? g_LastWorldName : "<unavailable>");
    spdlog::info("arg7 packedArg7Selection     = 0x{:08x}", g_PackedArg7Selection);
    spdlog::info("arg8 flagByte                = 0x{:08x}", g_FlagByte);
}

// Current replacement note:
// - original CWinApp_InitInstance does a two-step parse sequence, not one raw pass:
//   - launcher.exe:0x409950 ParseCommandLine
//   - launcher.exe:0x4173d0 CConsoleVar_ParseCommandLineAndConfig(filteredArgc, filteredArgv, 0)
// - 0x409950 consumes launcher-owned switches such as -user / -pwd / -char / -session / -nopatch
//   / -clone / -recover / -justpatch / -noeula / -skiplaunch / -lptest and builds the filtered
//   argv that the runtime console parser sees afterward
// - so the faithful target is not "bypass launcher preprocessing and feed raw argv straight into
//   CConsoleVar_ParseCommandLineAndConfig"; it is "reimplement 0x409950 faithfully, then call the
//   runtime console parser on its filtered argv output"
bool CLauncher::ParseCommandLineStage() const {
    spdlog::info("=== Launcher argv preprocessing ===");
    spdlog::info(
        "DIAGNOSTIC: launcher.exe uses ParseCommandLine(0x409950) followed by "
        "CConsoleVar_ParseCommandLineAndConfig(0x4173d0)");

    if (!g_LauncherCommandLine.ParseCommandLine(g_CrtArgc, g_CrtArgv)) {
        spdlog::error("ERROR: launcher ParseCommandLine scaffold failed");
        return false;
    }

    if (!g_LauncherCommandLine.ParseRuntimeConsoleVariables()) {
        for (const std::string& errorLine : g_LauncherCommandLine.RuntimeConsoleErrors().lines) {
            spdlog::error("{}", errorLine);
        }
        spdlog::error("ERROR: CConsoleVar_ParseCommandLineAndConfig rejected the filtered argv");
        return false;
    }

    if (!g_LauncherCommandLine.SwitchNoPatch()) {
        g_LauncherCommandLine.ForceDefaultNoPatchBranch();
        spdlog::info("DIAGNOSTIC: forcing default nopatch branch semantics in replacement launcher");
    }

    LogLauncherPreprocessingState();
    spdlog::info(
        "DIAGNOSTIC: filtered argv final count = {}",
        g_LauncherCommandLine.FilteredArgCount());
    return true;
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

bool CLauncher::BuildRecoveredStartupContext(RecoveredLauncherStartupContext* startupContext) {
    if (!startupContext) {
        return false;
    }

    std::memset(startupContext, 0, sizeof(*startupContext));

    LoadLastWorldNameFromRegistry(g_LastWorldName, sizeof(g_LastWorldName));

    if (g_LastWorldName[0]) {
        lstrcpynA(startupContext->mediatorSelectionName, g_LastWorldName, sizeof(startupContext->mediatorSelectionName));
        spdlog::info(
            "DIAGNOSTIC: using persisted Last_WorldName as launcher selection name = '{}'",
            startupContext->mediatorSelectionName);
    } else if (sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]) > 0) {
        std::strncpy(
            startupContext->mediatorSelectionName,
            kRecoveredLauncherSelectionRecords[0].worldName,
            sizeof(startupContext->mediatorSelectionName) - 1);
        startupContext->mediatorSelectionName[sizeof(startupContext->mediatorSelectionName) - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: no persisted Last_WorldName; defaulting launcher selection name to first recovered world '{}'",
            startupContext->mediatorSelectionName);
    } else {
        std::strcpy(startupContext->mediatorSelectionName, "standalone");
        spdlog::warn(
            "DIAGNOSTIC: no persisted Last_WorldName and no recovered launcher selection records are available; falling back to '{}'",
            startupContext->mediatorSelectionName);
    }

    startupContext->recoveredSelection =
        FindRecoveredLauncherSelectionRecord(startupContext->mediatorSelectionName);
    if (startupContext->recoveredSelection) {
        std::strncpy(
            startupContext->mediatorSelectionName,
            startupContext->recoveredSelection->worldName,
            sizeof(startupContext->mediatorSelectionName) - 1);
        startupContext->mediatorSelectionName[sizeof(startupContext->mediatorSelectionName) - 1] = '\0';
        m_FieldA8 = startupContext->recoveredSelection->variantIndexHigh8;
        m_FieldAC = startupContext->recoveredSelection->worldIndexLow24;
        spdlog::info(
            "DIAGNOSTIC: seeded launcher selection defaults from recovered world '{}' -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} selectionGateByte100={} variantState={} routePrefix='{}'",
            startupContext->recoveredSelection->worldName,
            m_FieldA8,
            m_FieldAC,
            BuildPackedArg7Selection(),
            (unsigned)startupContext->recoveredSelection->selectionGateByte100,
            (unsigned)startupContext->recoveredSelection->variantState,
            startupContext->recoveredSelection->routeHostPrefix ? startupContext->recoveredSelection->routeHostPrefix : "");
    } else if (startupContext->mediatorSelectionName[0] &&
               lstrcmpiA(startupContext->mediatorSelectionName, "standalone") != 0) {
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection defaults for world '{}'; keeping zeroed launcher arg7 fields until that world has a recovered launcher-owned selection record",
            startupContext->mediatorSelectionName);
    }

    g_PackedArg7Selection = BuildPackedArg7Selection();
    if ((m_FieldA8 | m_FieldAC) != 0) {
        spdlog::info("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x{:08x}", g_PackedArg7Selection);
    }

    startupContext->mediatorSelectedSelectionGateByte100 =
        startupContext->recoveredSelection ? startupContext->recoveredSelection->selectionGateByte100 : 1u;
    startupContext->mediatorSelectedVariantState =
        startupContext->recoveredSelection ? startupContext->recoveredSelection->variantState : 0u;
    startupContext->nopatchLauncherVersionValue = g_LauncherCommandLine.NoPatchLauncherVersionBits();
    startupContext->nopatchClientVersionValue = g_LauncherCommandLine.NoPatchClientVersionBits();

    if (g_LauncherCommandLine.NoPatchLauncherVersionString()[0]) {
        spdlog::info(
            "DIAGNOSTIC: rebuilt nopatch launcher-version float from launcher.exe version info = '{}' (0x{:08x})",
            g_LauncherCommandLine.NoPatchLauncherVersionString(),
            startupContext->nopatchLauncherVersionValue);
    } else {
        spdlog::info(
            "DIAGNOSTIC: nopatch launcher-version float is using fallback 0.1 (0x{:08x})",
            startupContext->nopatchLauncherVersionValue);
    }
    if (g_LauncherCommandLine.NoPatchClientVersionString()[0]) {
        spdlog::info(
            "DIAGNOSTIC: rebuilt nopatch client-version float from client.dll version info = '{}' (0x{:08x})",
            g_LauncherCommandLine.NoPatchClientVersionString(),
            startupContext->nopatchClientVersionValue);
    } else {
        spdlog::info(
            "DIAGNOSTIC: nopatch client-version float is using fallback 0.1 (0x{:08x})",
            startupContext->nopatchClientVersionValue);
    }

    return true;
}

bool CLauncher::PrepareRecoveredInitClientState(const RecoveredLauncherStartupContext& startupContext) {
    if (!PreloadDependencies()) {
        spdlog::info("ERROR: preload failed");
        return false;
    }

    DiagnosticInstallMediatorViaBinderScaffold(&g_pILTLoginMediatorDefault);

    // =============================================================================
    // arg6 / sibling mediator configuration for InitClientDLL
    // Address anchor: launcher.exe:0x4d3584 = ILTLoginMediator sibling slot used by launcher-side selection resolution
    // =============================================================================
    spdlog::info("=== configuring arg6 / sibling mediator state for InitClientDLL ===");

    const uint32_t selectedHighByte = (g_PackedArg7Selection >> 24) & 0xffu;
    const uint32_t selectionPackedLow24 = g_PackedArg7Selection & 0x00ffffffu;
    const uint32_t worldUpperBoundExclusive =
        (selectionPackedLow24 < 0xffu) ? (selectionPackedLow24 + 1u) : 1u;
    const uint32_t variantUpperBoundExclusive =
        (selectedHighByte < 0xffu) ? (selectedHighByte + 1u) : 1u;
    DiagnosticConfigureMediatorSelection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        startupContext.mediatorSelectionName,
        startupContext.mediatorSelectionName,
        selectionPackedLow24,
        selectedHighByte,
        startupContext.mediatorSelectedSelectionGateByte100,
        startupContext.mediatorSelectedVariantState);
    DiagnosticConfigureMediatorProfileName(
        g_LauncherCommandLine.AuthUsername()[0] ? g_LauncherCommandLine.AuthUsername() : NULL);
    DiagnosticConfigureMediatorAuthName(
        g_LauncherCommandLine.AuthUsername()[0] ? g_LauncherCommandLine.AuthUsername() : NULL);
    DiagnosticConfigureMediatorAuthPassword(
        g_LauncherCommandLine.AuthPassword()[0] ? g_LauncherCommandLine.AuthPassword() : NULL);

    DiagnosticConfigureLoginControllerCharacterSeed(
        g_LauncherCommandLine.LauncherCharacter()[0] ? g_LauncherCommandLine.LauncherCharacter() : NULL,
        g_LauncherCommandLine.LauncherSession()[0] ? g_LauncherCommandLine.LauncherSession() : NULL,
        selectionPackedLow24);

    DiagnosticApplyDefaultNopatchMediatorConfig(
        g_pILTLoginMediatorDefault,
        startupContext.nopatchLauncherVersionValue,
        startupContext.nopatchClientVersionValue);

    spdlog::info("=== configuring arg6 / sibling mediator state for InitClientDLL ===");

    if (g_pILTLoginMediatorDefault) {
        g_pILTLoginMediatorSelection3584 = g_pILTLoginMediatorDefault;
        spdlog::info(
            "DIAGNOSTIC: reusing current ILTLoginMediator.Default object as sibling 0x4d3584 selection slot ({})",
            fmt::ptr(g_pILTLoginMediatorSelection3584));

        uint32_t resolvedA8 = m_FieldA8;
        uint32_t resolvedAC = m_FieldAC;
        char resolvedWorldName[sizeof(g_LastWorldName)] = {0};
        if (DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediatorSelection3584,
                m_FieldAC,
                m_FieldA8,
                &resolvedA8,
                &resolvedAC,
                resolvedWorldName,
                sizeof(resolvedWorldName))) {
            m_FieldA8 = resolvedA8;
            m_FieldAC = resolvedAC;
            g_PackedArg7Selection = BuildPackedArg7Selection();
            if (resolvedWorldName[0]) {
                std::strncpy(g_LastWorldName, resolvedWorldName, sizeof(g_LastWorldName) - 1);
                g_LastWorldName[sizeof(g_LastWorldName) - 1] = '\0';
                StoreLastWorldNameInRegistry(g_LastWorldName);
            }
            spdlog::info(
                "DIAGNOSTIC: arg7 rebuilt through sibling 0x4d3584-style mediator selection slot -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} world='{}'",
                m_FieldA8,
                m_FieldAC,
                g_PackedArg7Selection,
                g_LastWorldName[0] ? g_LastWorldName : startupContext.mediatorSelectionName);
        }
    }

    DiagnosticInstallLauncherObjectStub(&g_pLauncherObject6304, g_pILTLoginMediatorDefault);

    const char authServerDnsName[] = "auth.lith.thematrixonline.net";
    const uint16_t authServerPort = 11000;
    const char marginServerSuffix[] = ".lith.thematrixonline.net";
    const uint16_t marginServerPort = 10000;

    char marginRoutePrefix[256] = {0};
    if (startupContext.recoveredSelection &&
        startupContext.recoveredSelection->routeHostPrefix &&
        startupContext.recoveredSelection->routeHostPrefix[0]) {
        std::strncpy(marginRoutePrefix, startupContext.recoveredSelection->routeHostPrefix, sizeof(marginRoutePrefix) - 1);
        marginRoutePrefix[sizeof(marginRoutePrefix) - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: using recovered route host prefix '{}' for world '{}'",
            marginRoutePrefix,
            startupContext.recoveredSelection->worldName);
    } else {
        LowercaseAsciiCopy(marginRoutePrefix, sizeof(marginRoutePrefix), startupContext.mediatorSelectionName);
    }

    const char exactMarginHostName[] = "";
    const bool ignoreHostsFileForAuth = false;
    const bool ignoreHostsFileForMargin = false;

    DiagnosticConfigureLoginControllerNetwork(
        authServerDnsName,
        authServerPort,
        ignoreHostsFileForAuth,
        marginServerSuffix,
        marginServerPort,
        ignoreHostsFileForMargin,
        marginRoutePrefix,
        exactMarginHostName);

    if (!DiagnosticInitializePreclientEnvironmentLike402EC0()) {
        spdlog::info("WARNING: pre-client environment scaffold failed to initialize");
    }

    RunOptionsCfgAutodetectStepIfNeeded();
    DiagnosticStartWindowTrace();
    return true;
}

void CLauncher::LogOriginalPathGapSummary() const {
    // Original order from launcher.exe:
    //   0x40a380  -> build 0x4d6304
    //   0x402ec0  -> environment setup
    //   0x40a780  -> LoadLibraryA("cres.dll")
    //   0x40a420  -> LoadLibraryA("client.dll")
    //   0x40a4d0  -> resolve exports + Init/Run/Term/Error path

    spdlog::info("=== Original-path gaps still missing ===");
    spdlog::info("arg1/arg2 status: launcher-owned filtered argv storage now follows the original 0x409950 -> 0x4173d0 two-stage parse shape, but runtime console registry/config fidelity is still scaffold-level");
    spdlog::info("arg5 status: current launcher object scaffold materialized 0x4d6304-style object (not yet faithful ctor/internal state)");
    spdlog::info("arg6 status: current binder-backed path materialized ILTLoginMediator.Default (not yet faithful launcher reconstruction)");
    if (g_pILTLoginMediatorSelection3584) {
        spdlog::info("arg7 status: sibling 0x4d3584-style ILTLoginMediator selection slot currently reuses the active mediator object and rebuilds a8/ac through +0xfc/+0x100/+0xe4");
    } else {
        spdlog::info("missing: reconstruct sibling ILTLoginMediator.Default-style slot at 0x4d3584 for launcher-owned arg7 selection resolution");
    }
    if (g_PreclientEnvironment.threadHandle) {
        spdlog::info("pre-client env status: current 0x402ec0-style launcher thread/message scaffold active (not yet faithful original class/import path)");
    } else {
        spdlog::info("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
    }
    spdlog::info("");
}

bool CLauncher::InitInstance() {
    spdlog::info("NOTE: arg1/arg2 now follow the original ParseCommandLine -> CConsoleVar_ParseCommandLineAndConfig staging, but runtime console-variable registration/config-file fidelity is still scaffolded.");
    spdlog::info("NOTE: launcher-owned nopatch setup, arg5, arg6, arg7, and arg8 remain incomplete.");
    if (!ParseCommandLineStage()) {
        return false;
    }
    spdlog::info("");

    spdlog::info(
        "DIAGNOSTIC: active launcher runtime path = binder-backed mediator + launcher object scaffold + InitClientDLL/RunClientDLL + launcher-owned auth begin");

    RecoveredLauncherStartupContext startupContext = {};
    if (!BuildRecoveredStartupContext(&startupContext)) {
        return false;
    }
    if (!PrepareRecoveredInitClientState(startupContext)) {
        return false;
    }
    LogOriginalPathGapSummary();

    if (!LoadCresDLL()) {
        spdlog::info("ERROR: failed to load cres.dll");
        return false;
    }

    if (!LoadClientDLL()) {
        spdlog::info("ERROR: failed to load client.dll");
        return false;
    }

    return RunClientDllLifecycle();
}

int main(int argc, char* argv[]) {
    InitializeLogging();
    AddVectoredExceptionHandler(1, DiagnosticVectoredExceptionHandler);
    SetUnhandledExceptionFilter(DiagnosticUnhandledExceptionFilter);

    g_CrtArgc = argc;
    g_CrtArgv = argv;

    spdlog::info("Matrix Online launcher reimplementation scaffold");
    spdlog::info("===============================================");
    spdlog::info("Mode: original startup order, no client-memory injection");
    spdlog::info("Default branch target: nopatch path");
    spdlog::info("DIAGNOSTIC: spdlog debug file = resurrections_spdlog.log");
    spdlog::info("");

    return FinishAndReturn(g_Launcher.InitInstance() ? 0 : 1);
}
