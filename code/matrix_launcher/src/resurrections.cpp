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
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "diagnostics.h"

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
using ErrorClientDLLFunc = void (*)();

static FILE* g_LogFile = NULL;
static HMODULE g_hCres = NULL;
static HMODULE g_hClient = NULL;
static InitClientDLLFunc g_InitClientDLL = NULL;
static RunClientDLLFunc g_RunClientDLL = NULL;
static TermClientDLLFunc g_TermClientDLL = NULL;
static ErrorClientDLLFunc g_ErrorClientDLL = NULL;

// Launcher-owned runtime values mirrored from the original startup path.
// arg1/arg2 now use launcher-owned filtered storage again, but the original
// 0x409950 switch-consumption/options.cfg preprocessing path is still incomplete.
static uint32_t g_FilteredArgCount = 0;
static char** g_FilteredArgv = NULL;
static char** g_FilteredArgvOwned = NULL;
static uint32_t g_FilteredArgvOwnedCapacity = 0;

// Diagnostic state tracking for pre-crash analysis
static const char* g_LastMediatorMethod = NULL;
static uint32_t g_LastMediatorCallCount = 0;
static void* g_LastMediatorSelf = NULL;

// Buffer to store raw bytes around potential crash points for pattern matching
static uint8_t g_ArgvSnapshot[64] = {0};
static bool g_ArgvSnapshotValid = false;
static char g_AuthUsername[256] = {};
static char g_AuthPassword[256] = {};
static void* g_pLauncherObject6304 = NULL;       // original: [0x4d6304]
static void* g_pILTLoginMediatorDefault = NULL;  // original: [0x4d2c58]
static void* g_pILTLoginMediatorSelection3584 = NULL; // original sibling slot: [0x4d3584]
static uint32_t g_CLauncherFieldA8 = 0;          // original: [CLauncher+0xa8], high 8 bits used
static uint32_t g_CLauncherFieldAC = 0;          // original: [CLauncher+0xac], low 24 bits used
static uint32_t g_PackedArg7Selection = 0;       // packed from [this+0xa8]/[this+0xac]
static uint32_t g_FlagByte = 0;                  // original: [0x4d2c69]
static char g_LastWorldName[256] = {0};         // original registry value: Last_WorldName
static char g_LauncherCharacter[256] = {};
static char g_LauncherSession[256] = {};
static char g_LauncherQlVersion[256] = {};
static bool g_LauncherSwitchClone = false;
static bool g_LauncherSwitchSilent = false;
static bool g_LauncherSwitchNoPatch = false;
static bool g_LauncherSwitchRecover = false;
static bool g_LauncherSwitchDeleteChar = false;
static bool g_LauncherSwitchJustPatch = false;
static bool g_LauncherSwitchNoEula = false;
static bool g_LauncherSwitchSkipLaunch = false;
static bool g_LauncherSwitchLPTest = false;
static bool g_LauncherGlobal4C8B1C = true;      // original .data init = 1, cleared by -justpatch / -noeula
static bool g_LauncherGlobal4C8B1D = true;      // original .data init = 1, cleared by -nopatch
static bool g_LauncherGlobal4D2C64 = false;     // original options.cfg/autodetect gate
static DWORD g_AutodetectExitCode = 0;
static void* g_pClientDBFromCallback = NULL;

static const char* kLauncherRegistryKeyPath = "Software\\Monolith Productions\\The Matrix Online\\";

struct RecoveredLauncherSelectionRecord {
    const char* worldName;
    const char* routeHostPrefix;
    uint32_t worldIndexLow24;
    uint32_t variantIndexHigh8;
    uint32_t worldType;
    uint32_t variantState;
};

static const RecoveredLauncherSelectionRecord kRecoveredLauncherSelectionRecords[] = {
    // Current live bounded-evidence entry for the first in-game replacement path:
    // - selection name = Reality
    // - arg7 packed selection = 0x0500002a
    // - default route host prefix = reality
    {"Reality", "reality", 0x00002au, 0x05u, 1u, 0u},
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

struct DiagnosticInitClientFrameSnapshot {
    bool valid;
    uint32_t args[8];
};

static DiagnosticPreclientEnvironmentState g_PreclientEnvironment = {};
static DiagnosticInitClientFrameSnapshot g_InitClientFrameSnapshot = {};

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase);

void Log(const char* fmt, ...);

static void LogWordSpan(const char* label, const void* base, size_t wordCount);
static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);

template <typename T>
static T ResolveProc(HMODULE module, const char* name) {
    FARPROC proc = GetProcAddress(module, name);
    T typed = nullptr;
    static_assert(sizeof(typed) == sizeof(proc), "function pointer size mismatch");
    std::memcpy(&typed, &proc, sizeof(typed));
    return typed;
}

void Log(const char* fmt, ...) {
    if (!g_LogFile) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_LogFile, fmt, args);
    va_end(args);
    fprintf(g_LogFile, "\n");
    fflush(g_LogFile);
}

static void LogWordSpan(const char* label, const void* base, size_t wordCount) {
    const uint32_t* words = static_cast<const uint32_t*>(base);
    if (!label || !base || wordCount == 0) return;
    for (size_t i = 0; i < wordCount; i += 4) {
        Log(
            "%s @ %p [+0x%02x]=%08x [+0x%02x]=%08x [+0x%02x]=%08x [+0x%02x]=%08x",
            label,
            base,
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

static const char* InitClientArgName(size_t index) {
    switch (index) {
        case 0: return "arg1 filteredArgCount";
        case 1: return "arg2 filteredArgv";
        case 2: return "arg3 hClientDll";
        case 3: return "arg4 hCresDll";
        case 4: return "arg5 launcherNetworkObject";
        case 5: return "arg6 ILTLoginMediatorDefault";
        case 6: return "arg7 packedArg7Selection";
        case 7: return "arg8 flagByte";
        default: return NULL;
    }
}

static void CaptureInitClientFrameSnapshot() {
    g_InitClientFrameSnapshot.valid = true;
    g_InitClientFrameSnapshot.args[0] = g_FilteredArgCount;
    g_InitClientFrameSnapshot.args[1] = reinterpret_cast<uint32_t>(g_FilteredArgv);
    g_InitClientFrameSnapshot.args[2] = reinterpret_cast<uint32_t>(g_hClient);
    g_InitClientFrameSnapshot.args[3] = reinterpret_cast<uint32_t>(g_hCres);
    g_InitClientFrameSnapshot.args[4] = reinterpret_cast<uint32_t>(g_pLauncherObject6304);
    g_InitClientFrameSnapshot.args[5] = reinterpret_cast<uint32_t>(g_pILTLoginMediatorDefault);
    g_InitClientFrameSnapshot.args[6] = g_PackedArg7Selection;
    g_InitClientFrameSnapshot.args[7] = g_FlagByte;

    spdlog::info("DIAGNOSTIC: preserved InitClientDLL argument frame snapshot");
    for (size_t i = 0; i < 8; ++i) {
        spdlog::info(
            "  {} = {:08x}",
            InitClientArgName(i),
            (unsigned)g_InitClientFrameSnapshot.args[i]);
    }
}

static void LogCrashStackVsInitFrame(uint32_t crashEsp) {
    if (!g_InitClientFrameSnapshot.valid || crashEsp == 0) return;

    Log("DIAGNOSTIC: comparing crash stack against preserved InitClientDLL argument frame");
    for (size_t i = 0; i < 8; ++i) {
        Log(
            "  saved %s = %08x",
            InitClientArgName(i),
            (unsigned)g_InitClientFrameSnapshot.args[i]);
    }

    const uint32_t* stackWords = reinterpret_cast<const uint32_t*>(crashEsp);
    for (size_t stackIndex = 0; stackIndex < 8; ++stackIndex) {
        const uint32_t value = stackWords[stackIndex];
        for (size_t argIndex = 0; argIndex < 8; ++argIndex) {
            if (value != 0 && value == g_InitClientFrameSnapshot.args[argIndex]) {
                Log(
                    "  crash esp[%u] = %08x matches %s",
                    (unsigned)stackIndex,
                    (unsigned)value,
                    InitClientArgName(argIndex));
            }
        }
    }
}

static void DiagnosticSnapshotArgvMemory() {
  if (!g_FilteredArgv || !g_FilteredArgvOwned) return;
  // Take a snapshot of argv array memory for post-crash analysis
  std::memcpy(g_ArgvSnapshot, &g_FilteredArgv[0],
              (g_FilteredArgCount + 2) * sizeof(char*) > sizeof(g_ArgvSnapshot)
                  ? sizeof(g_ArgvSnapshot)
                  : (g_FilteredArgCount + 2) * sizeof(char*));
  g_ArgvSnapshotValid = true;
}

static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) {
        Log("DIAGNOSTIC: unhandled exception filter invoked with incomplete state");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
    CONTEXT* context = exceptionInfo->ContextRecord;

    Log("=== Unhandled exception ===");
    Log(
        "exception code=%08lx flags=%08lx address=%p",
        (unsigned long)record->ExceptionCode,
        (unsigned long)record->ExceptionFlags,
        record->ExceptionAddress);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        const char* accessKind = "unknown";
        if (record->ExceptionInformation[0] == 0) accessKind = "read";
        else if (record->ExceptionInformation[0] == 1) accessKind = "write";
        else if (record->ExceptionInformation[0] == 8) accessKind = "execute";
        Log(
            "access violation kind=%s target=%p",
            accessKind,
            reinterpret_cast<void*>(record->ExceptionInformation[1]));
    }
    Log(
        "registers: eip=%08lx esp=%08lx ebp=%08lx eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx",
        (unsigned long)context->Eip,
        (unsigned long)context->Esp,
        (unsigned long)context->Ebp,
        (unsigned long)context->Eax,
        (unsigned long)context->Ebx,
        (unsigned long)context->Ecx,
        (unsigned long)context->Edx,
        (unsigned long)context->Esi,
        (unsigned long)context->Edi);

    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(reinterpret_cast<const void*>(context->Eip), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        Log(
            "eip page: base=%p allocBase=%p regionSize=0x%08lx protect=0x%08lx state=0x%08lx type=0x%08lx",
            mbi.BaseAddress,
            mbi.AllocationBase,
            (unsigned long)mbi.RegionSize,
            (unsigned long)mbi.Protect,
            (unsigned long)mbi.State,
            (unsigned long)mbi.Type);
    }

    LogWordSpan("crash stack", reinterpret_cast<const void*>(context->Esp), 16);
    LogCrashStackVsInitFrame(context->Esp);
    if (g_FilteredArgv) {
        LogWordSpan("current arg2 filteredArgv", g_FilteredArgv, 4);

      // Analyze if crash is in argv memory (arg2+2 pattern detection)
      uintptr_t eip = static_cast<uintptr_t>(context->Eip);
      uintptr_t argvStart = reinterpret_cast<uintptr_t>(g_FilteredArgv);
      size_t argvSize = (g_FilteredArgCount + 1) * sizeof(char*);
      if (eip >= argvStart && eip < argvStart + argvSize) {
        Log("!!! CRASH IN ARGV MEMORY !!!");
        Log("    eip=%p is at argv+%zu (argv[%zu])",
            reinterpret_cast<void*>(eip),
            eip - argvStart,
            (eip - argvStart) / sizeof(char*));
      }
      if (g_ArgvSnapshotValid) {
        LogWordSpan("argv memory snapshot", g_ArgvSnapshot, 8);
      }
    }
    if (g_pLauncherObject6304) {
        LogWordSpan("current arg5 launcherObject", g_pLauncherObject6304, 8);
    }
    if (g_pILTLoginMediatorDefault) {
        LogWordSpan("current arg6 mediator", g_pILTLoginMediatorDefault, 8);
    }
    if (g_LastMediatorMethod) {
      Log("last mediator method: %s (call#%u, self=%p)",
          g_LastMediatorMethod, g_LastMediatorCallCount, g_LastMediatorSelf);
    }

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

enum class LauncherValueTarget {
    None,
    User,
    Password,
    Character,
    Session,
    QlVersionIgnored,
};

static bool CopyLauncherString(char* destination, size_t destinationSize, const char* value) {
    if (!destination || destinationSize < 2) return false;
    if (!value) value = "";
    std::strncpy(destination, value, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
    return true;
}

static uint32_t FloatBitsFromCString(const char* value) {
    float parsed = value ? std::strtof(value, NULL) : 0.0f;
    uint32_t bits = 0;
    std::memcpy(&bits, &parsed, sizeof(bits));
    return bits;
}

static bool TryBuildOriginalClientVersionFloatString(char* out, size_t outSize, uint32_t* outBits) {
    if (!out || outSize < 8 || !outBits) return false;
    out[0] = '\0';
    *outBits = 0;

    DWORD handle = 0;
    DWORD versionInfoSize = GetFileVersionInfoSizeA("client.dll", &handle);
    if (versionInfoSize == 0) {
        Log("DIAGNOSTIC: GetFileVersionInfoSizeA('client.dll') failed (%lu)", (unsigned long)GetLastError());
        return false;
    }

    void* versionInfo = std::malloc(versionInfoSize);
    if (!versionInfo) {
        Log("DIAGNOSTIC: failed to allocate client.dll version-info buffer (%lu bytes)", (unsigned long)versionInfoSize);
        return false;
    }

    bool ok = false;
    do {
        if (!GetFileVersionInfoA("client.dll", 0, versionInfoSize, versionInfo)) {
            Log("DIAGNOSTIC: GetFileVersionInfoA('client.dll') failed (%lu)", (unsigned long)GetLastError());
            break;
        }

        VS_FIXEDFILEINFO* fixedInfo = NULL;
        UINT fixedInfoSize = 0;
        if (!VerQueryValueA(versionInfo, "\\", reinterpret_cast<LPVOID*>(&fixedInfo), &fixedInfoSize) ||
            !fixedInfo || fixedInfoSize < sizeof(VS_FIXEDFILEINFO)) {
            Log("DIAGNOSTIC: VerQueryValueA('client.dll', '\\') failed");
            break;
        }

        const uint32_t major = HIWORD(fixedInfo->dwFileVersionMS);
        const uint32_t minor = LOWORD(fixedInfo->dwFileVersionMS);
        const uint32_t build = HIWORD(fixedInfo->dwFileVersionLS);
        const uint32_t revision = LOWORD(fixedInfo->dwFileVersionLS);
        const uint32_t majorQuotient = major / 10u;
        const uint32_t majorRemainder = major % 10u;

        std::snprintf(
            out,
            outSize,
            "%u.%u%u%u%u",
            (unsigned)majorQuotient,
            (unsigned)majorRemainder,
            (unsigned)minor,
            (unsigned)build,
            (unsigned)revision);
        *outBits = FloatBitsFromCString(out);
        ok = true;
    } while (false);

    std::free(versionInfo);
    return ok;
}

static void ResetLauncherPreprocessingState() {
    std::memset(g_AuthUsername, 0, sizeof(g_AuthUsername));
    std::memset(g_AuthPassword, 0, sizeof(g_AuthPassword));
    std::memset(g_LauncherCharacter, 0, sizeof(g_LauncherCharacter));
    std::memset(g_LauncherSession, 0, sizeof(g_LauncherSession));
    std::memset(g_LauncherQlVersion, 0, sizeof(g_LauncherQlVersion));
    g_LauncherSwitchClone = false;
    g_LauncherSwitchSilent = false;
    g_LauncherSwitchNoPatch = false;
    g_LauncherSwitchRecover = false;
    g_LauncherSwitchDeleteChar = false;
    g_LauncherSwitchJustPatch = false;
    g_LauncherSwitchNoEula = false;
    g_LauncherSwitchSkipLaunch = false;
    g_LauncherSwitchLPTest = false;
    g_LauncherGlobal4C8B1C = true;
    g_LauncherGlobal4C8B1D = true;
    g_LauncherGlobal4D2C64 = false;
    g_AutodetectExitCode = 0;
}

static LauncherValueTarget LauncherValueTargetForSwitch(const char* value) {
    if (!value || !value[0]) return LauncherValueTarget::None;
    if (lstrcmpiA(value, "-user") == 0 || lstrcmpiA(value, "-qluser") == 0) return LauncherValueTarget::User;
    if (lstrcmpiA(value, "-pwd") == 0 || lstrcmpiA(value, "-qlpwd") == 0) return LauncherValueTarget::Password;
    if (lstrcmpiA(value, "-char") == 0 || lstrcmpiA(value, "-qlchar") == 0) return LauncherValueTarget::Character;
    if (lstrcmpiA(value, "-session") == 0 || lstrcmpiA(value, "-qlsession") == 0) return LauncherValueTarget::Session;
    if (lstrcmpiA(value, "-qlver") == 0) return LauncherValueTarget::QlVersionIgnored;
    return LauncherValueTarget::None;
}

static bool ConsumeLauncherBooleanSwitch(const char* value) {
    if (!value || !value[0]) return false;
    if (lstrcmpiA(value, "-clone") == 0) {
        g_LauncherSwitchClone = true;
        return true;
    }
    if (lstrcmpiA(value, "-silent") == 0) {
        g_LauncherSwitchSilent = true;
        return true;
    }
    if (lstrcmpiA(value, "-nopatch") == 0) {
        g_LauncherSwitchNoPatch = true;
        g_LauncherGlobal4C8B1D = false;
        return true;
    }
    if (lstrcmpiA(value, "-recover") == 0) {
        g_LauncherSwitchRecover = true;
        return true;
    }
    if (lstrcmpiA(value, "-deletechar") == 0) {
        g_LauncherSwitchDeleteChar = true;
        return true;
    }
    if (lstrcmpiA(value, "-justpatch") == 0) {
        g_LauncherSwitchJustPatch = true;
        g_LauncherGlobal4C8B1C = false;
        return true;
    }
    if (lstrcmpiA(value, "-noeula") == 0) {
        g_LauncherSwitchNoEula = true;
        g_LauncherGlobal4C8B1C = false;
        return true;
    }
    if (lstrcmpiA(value, "-skiplaunch") == 0) {
        g_LauncherSwitchSkipLaunch = true;
        return true;
    }
    if (lstrcmpiA(value, "-lptest") == 0) {
        g_LauncherSwitchLPTest = true;
        return true;
    }
    return false;
}

static bool ConsumeLauncherValueSwitch(LauncherValueTarget target, const char* value) {
    switch (target) {
        case LauncherValueTarget::User:
            return CopyLauncherString(g_AuthUsername, sizeof(g_AuthUsername), value);
        case LauncherValueTarget::Password:
            return CopyLauncherString(g_AuthPassword, sizeof(g_AuthPassword), value);
        case LauncherValueTarget::Character:
            return CopyLauncherString(g_LauncherCharacter, sizeof(g_LauncherCharacter), value);
        case LauncherValueTarget::Session:
            return CopyLauncherString(g_LauncherSession, sizeof(g_LauncherSession), value);
        case LauncherValueTarget::QlVersionIgnored:
            return true;
        case LauncherValueTarget::None:
        default:
            return false;
    }
}

static void LogLauncherPreprocessingState() {
    Log("=== Launcher switch preprocessing ===");
    Log("auth username      = %s", MaskedArgValue(g_AuthUsername));
    Log("auth password      = %s", MaskedArgValue(g_AuthPassword));
    Log("launcher character  = %s", MaskedArgValue(g_LauncherCharacter));
    Log("launcher session    = %s", MaskedArgValue(g_LauncherSession));
    Log("launcher qlver      = %s", MaskedArgValue(g_LauncherQlVersion));
    Log(
        "launcher flags      = clone:%d silent:%d nopatch:%d recover:%d deletechar:%d justpatch:%d noeula:%d skiplaunch:%d lptest:%d",
        g_LauncherSwitchClone ? 1 : 0,
        g_LauncherSwitchSilent ? 1 : 0,
        g_LauncherSwitchNoPatch ? 1 : 0,
        g_LauncherSwitchRecover ? 1 : 0,
        g_LauncherSwitchDeleteChar ? 1 : 0,
        g_LauncherSwitchJustPatch ? 1 : 0,
        g_LauncherSwitchNoEula ? 1 : 0,
        g_LauncherSwitchSkipLaunch ? 1 : 0,
        g_LauncherSwitchLPTest ? 1 : 0);
    Log(
        "launcher globals    = 4c8b1c:%d 4c8b1d:%d 4d2c64:%d 4d2c65:%d 4d2c66:%d 4d2c6a:%d",
        g_LauncherGlobal4C8B1C ? 1 : 0,
        g_LauncherGlobal4C8B1D ? 1 : 0,
        g_LauncherGlobal4D2C64 ? 1 : 0,
        g_LauncherSwitchRecover ? 1 : 0,
        g_LauncherSwitchJustPatch ? 1 : 0,
        g_LauncherSwitchClone ? 1 : 0);
    if (g_LauncherGlobal4D2C64) {
        Log("launcher autodetect exitCode = %lu", (unsigned long)g_AutodetectExitCode);
    }
}

static bool IsTmBeforeAutodetectCutoff(const std::tm* value) {
    if (!value) return false;

    const int year = value->tm_year + 1900;
    const int month = value->tm_mon;
    const int day = value->tm_mday;

    if (year != 2005) return year < 2005;
    if (month != 3) return month < 3;
    return day < 25;
}

static bool IsTmOnOrAfterAutodetectCutoff(const std::tm* value) {
    return value && !IsTmBeforeAutodetectCutoff(value);
}

static void ProbeOptionsCfgAutodetectGate() {
    g_LauncherGlobal4D2C64 = false;

    struct _stat optionsStat = {};
    if (_stat("options.cfg", &optionsStat) != 0) {
        g_LauncherGlobal4D2C64 = true;
        Log("DIAGNOSTIC: options.cfg probe -> missing/unstatable, setting 4d2c64=1");
        return;
    }

    std::tm* optionsLocalTime = std::localtime(&optionsStat.st_mtime);
    if (!optionsLocalTime) {
        Log("DIAGNOSTIC: options.cfg probe -> localtime(mtime) failed, keeping 4d2c64=0");
        return;
    }

    Log(
        "DIAGNOSTIC: options.cfg mtime local = %04d-%02d-%02d",
        optionsLocalTime->tm_year + 1900,
        optionsLocalTime->tm_mon + 1,
        optionsLocalTime->tm_mday);

    if (!IsTmBeforeAutodetectCutoff(optionsLocalTime)) {
        Log("DIAGNOSTIC: options.cfg is not older than original 2005-04-25 cutoff, keeping 4d2c64=0");
        return;
    }

    std::time_t currentTime = std::time(NULL);
    std::tm* currentLocalTime = std::localtime(&currentTime);
    if (!currentLocalTime) {
        g_LauncherGlobal4D2C64 = true;
        Log("DIAGNOSTIC: current localtime probe failed after stale options.cfg, setting 4d2c64=1");
        return;
    }

    Log(
        "DIAGNOSTIC: current local date = %04d-%02d-%02d",
        currentLocalTime->tm_year + 1900,
        currentLocalTime->tm_mon + 1,
        currentLocalTime->tm_mday);

    if (IsTmOnOrAfterAutodetectCutoff(currentLocalTime)) {
        g_LauncherGlobal4D2C64 = true;
        Log("DIAGNOSTIC: options.cfg is stale relative to original cutoff, setting 4d2c64=1");
    } else {
        Log("DIAGNOSTIC: current date is still before original cutoff, keeping 4d2c64=0");
    }
}

static void RunOptionsCfgAutodetectStepIfNeeded() {
    if (!g_LauncherGlobal4D2C64) return;

    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    char commandLine[] = "autodetect_settings.exe setopts hide";

    Log("=== Original-style options.cfg autodetect step ===");
    Log("DIAGNOSTIC: launching '%s' with currentDir='.'", commandLine);

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
        Log(
            "WARNING: CreateProcessA for autodetect_settings.exe failed (%lu); continuing original-path scaffold without that side effect",
            (unsigned long)GetLastError());
        return;
    }

    DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 60000);
    Log("DIAGNOSTIC: autodetect wait result = %lu", (unsigned long)waitResult);

    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
            g_AutodetectExitCode = exitCode;
            Log("DIAGNOSTIC: autodetect exit code = %lu (0x%08lx)", (unsigned long)exitCode, (unsigned long)exitCode);
        } else {
            Log("WARNING: GetExitCodeProcess for autodetect_settings.exe failed (%lu)", (unsigned long)GetLastError());
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        Log("WARNING: autodetect_settings.exe did not finish within 60000 ms");
    } else {
        Log("WARNING: WaitForSingleObject for autodetect_settings.exe failed (%lu)", (unsigned long)GetLastError());
    }

    if (processInfo.hThread) CloseHandle(processInfo.hThread);
    if (processInfo.hProcess) CloseHandle(processInfo.hProcess);

    struct _stat optionsStat = {};
    if (_stat("options.cfg", &optionsStat) == 0) {
        Log("DIAGNOSTIC: autodetect produced options.cfg size=%ld bytes", (long)optionsStat.st_size);
    } else {
        Log("DIAGNOSTIC: options.cfg not present after autodetect child returned");
    }

    if (DeleteFileA("options.cfg")) {
        Log("DIAGNOSTIC: deleted temporary options.cfg after autodetect step");
    } else {
        DWORD deleteError = GetLastError();
        Log("DIAGNOSTIC: DeleteFileA('options.cfg') failed (%lu)", (unsigned long)deleteError);
    }
}

static char* DuplicateArgString(const char* value) {
    if (!value) value = "";
    const size_t len = std::strlen(value);
    char* copy = static_cast<char*>(std::malloc(len + 1));
    if (!copy) return NULL;
    std::memcpy(copy, value, len + 1);
    return copy;
}

static void FreeFilteredArgvOwned() {
    if (!g_FilteredArgvOwned) {
        g_FilteredArgvOwnedCapacity = 0;
        return;
    }

    for (uint32_t i = 0; i < g_FilteredArgvOwnedCapacity; ++i) {
        if (g_FilteredArgvOwned[i]) {
            std::free(g_FilteredArgvOwned[i]);
            g_FilteredArgvOwned[i] = NULL;
        }
    }

    std::free(g_FilteredArgvOwned);
    g_FilteredArgvOwned = NULL;
    g_FilteredArgvOwnedCapacity = 0;
}

static DWORD WINAPI DiagnosticPreclientThreadProc(LPVOID) {
    MSG msg = {};
    PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE);

    g_PreclientEnvironment.readyPointer48 = &g_PreclientEnvironment;
    InterlockedExchange(&g_PreclientEnvironment.readyFlag44, 0);
    InterlockedExchange(&g_PreclientEnvironment.readyFlag45, 1);
    SetEvent(g_PreclientEnvironment.readyEvent);
    Log(
        "DIAGNOSTIC: pre-client launcher thread ready threadId=%lu state44=%ld state45=%ld state48=%p",
        (unsigned long)GetCurrentThreadId(),
        (long)g_PreclientEnvironment.readyFlag44,
        (long)g_PreclientEnvironment.readyFlag45,
        g_PreclientEnvironment.readyPointer48);

    while (WaitForSingleObject(g_PreclientEnvironment.stopEvent, 10) == WAIT_TIMEOUT) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    Log("DIAGNOSTIC: pre-client launcher thread stopping");
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
        Log("DIAGNOSTIC: pre-client environment event creation failed (%lu)", (unsigned long)GetLastError());
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
        Log("DIAGNOSTIC: pre-client environment thread creation failed (%lu)", (unsigned long)GetLastError());
        return false;
    }

    DWORD waitResult = WaitForSingleObject(g_PreclientEnvironment.readyEvent, 5000);
    if (waitResult != WAIT_OBJECT_0) {
        Log("DIAGNOSTIC: pre-client environment readiness wait failed (%lu)", (unsigned long)waitResult);
        return false;
    }

    Log(
        "DIAGNOSTIC: pre-client environment scaffold active threadHandle=%p threadId=%lu state44=%ld state45=%ld state48=%p",
        g_PreclientEnvironment.threadHandle,
        (unsigned long)g_PreclientEnvironment.threadId,
        (long)g_PreclientEnvironment.readyFlag44,
        (long)g_PreclientEnvironment.readyFlag45,
        g_PreclientEnvironment.readyPointer48);
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

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
    DiagnosticShutdownPreclientEnvironment();
    FreeFilteredArgvOwned();
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = NULL;
    }
    return code;
}

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    g_pClientDBFromCallback = pMasterDatabase;
    Log("launcher export SetMasterDatabase called: %p", pMasterDatabase);
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
        Log("preload %-12s : %s (%p)", dlls[i], h ? "OK" : "FAIL", h);
        if (!h) return false;
    }
    return true;
}

static bool LoadCresDLL() {
    Log("=== Load cres.dll ===");
    g_hCres = LoadLibraryA("cres.dll");
    Log("cres.dll handle: %p", g_hCres);
    return g_hCres != NULL;
}

static bool LoadClientDLL() {
    Log("=== Load client.dll ===");
    g_hClient = LoadLibraryA("client.dll");
    Log("client.dll handle: %p", g_hClient);
    return g_hClient != NULL;
}

static bool ResolveClientExports() {
    Log("=== Resolve client exports ===");

    g_InitClientDLL = ResolveProc<InitClientDLLFunc>(g_hClient, "InitClientDLL");
    g_RunClientDLL = ResolveProc<RunClientDLLFunc>(g_hClient, "RunClientDLL");
    g_TermClientDLL = ResolveProc<TermClientDLLFunc>(g_hClient, "TermClientDLL");
    g_ErrorClientDLL = ResolveProc<ErrorClientDLLFunc>(g_hClient, "ErrorClientDLL");

    Log("InitClientDLL : %p", g_InitClientDLL);
    Log("RunClientDLL  : %p", g_RunClientDLL);
    Log("TermClientDLL : %p", g_TermClientDLL);
    Log("ErrorClientDLL: %p", g_ErrorClientDLL);

    return g_InitClientDLL && g_RunClientDLL && g_TermClientDLL && g_ErrorClientDLL;
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
        Log("DIAGNOSTIC: HKLM Last_WorldName key open failed (%ld)", (long)openResult);
        return false;
    }

    DWORD type = 0;
    DWORD size = outSize;
    LONG queryResult = RegQueryValueExA(key, "Last_WorldName", NULL, &type, reinterpret_cast<LPBYTE>(out), &size);
    RegCloseKey(key);
    if (queryResult != ERROR_SUCCESS) {
        Log("DIAGNOSTIC: HKLM Last_WorldName query failed (%ld)", (long)queryResult);
        out[0] = '\0';
        return false;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        Log("DIAGNOSTIC: HKLM Last_WorldName unexpected registry type %lu", (unsigned long)type);
        out[0] = '\0';
        return false;
    }

    out[outSize - 1] = '\0';
    Log("DIAGNOSTIC: loaded HKLM Last_WorldName='%s'", out);
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

static uint32_t BuildPackedArg7Selection() {
    return (g_CLauncherFieldAC & 0x00ffffffu) | ((g_CLauncherFieldA8 & 0xffu) << 24);
}


static void LogArgvContentsAsBytes(const char* label, char** argv, uint32_t count) {
  if (!argv || count == 0) return;
  Log("%s pointer array @ %p:", label, argv);
  for (uint32_t i = 0; i < count + 3 && i < 8; ++i) {
    Log("  argv[%u] = %p", i, argv[i]);
  }
  if (argv[0]) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(argv[0]);
    Log("%s argv[0] data @ %p: %02x %02x %02x %02x %02x %02x %02x %02x",
        label, argv[0],
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7]);
    Log("%s argv[0] as string: \'%s\'", label, argv[0]);
  }
}
static void LogKnownStartupState() {
    Log("=== Known startup frame ===");
    Log("arg1 filteredArgCount         = 0x%08x", g_FilteredArgCount);
    Log("arg2 filteredArgv            = %p", g_FilteredArgv);
  if (g_FilteredArgv) { LogArgvContentsAsBytes("arg2", g_FilteredArgv, g_FilteredArgCount); }
    Log("arg3 hClientDll              = %p", g_hClient);
    Log("arg4 hCresDll                = %p", g_hCres);
    Log("arg5 launcherNetworkObject   = %p", g_pLauncherObject6304);
    Log("arg6 ILTLoginMediatorDefault = %p", g_pILTLoginMediatorDefault);
    Log("CLauncher+0xa8 placeholder   = 0x%08x", g_CLauncherFieldA8);
    Log("CLauncher+0xac placeholder   = 0x%08x", g_CLauncherFieldAC);
    Log("Last_WorldName               = %s", g_LastWorldName[0] ? g_LastWorldName : "<unavailable>");
    Log("arg7 packedArg7Selection    = 0x%08x", g_PackedArg7Selection);
    Log("arg8 flagByte                = 0x%08x", g_FlagByte);
}

static bool ConfigureFilteredArgv(int argc, char* argv[]) {
    ResetLauncherPreprocessingState();
    g_ArgvSnapshotValid = false;

    spdlog::info("=== Launcher argv preprocessing ===");
    spdlog::info("DIAGNOSTIC: launcher auth/state expected through launcher-style switches (e.g. -user / -pwd)");

    FreeFilteredArgvOwned();
    g_FilteredArgvOwned = static_cast<char**>(std::calloc(argc + 1, sizeof(char*)));
    if (!g_FilteredArgvOwned) {
        spdlog::error("ERROR: failed to allocate launcher-owned filtered argv pointer array");
        return false;
    }
    g_FilteredArgvOwnedCapacity = static_cast<uint32_t>(argc + 1);

    g_FilteredArgvOwned[0] = DuplicateArgString((argc > 0 && argv[0]) ? argv[0] : "");
    if (!g_FilteredArgvOwned[0]) {
        spdlog::error("ERROR: failed to duplicate filtered argv[0]");
        FreeFilteredArgvOwned();
        return false;
    }
    spdlog::info("DIAGNOSTIC: filtered argv[0] duplicated at {} -> '{}'", static_cast<void*>(g_FilteredArgvOwned[0]), g_FilteredArgvOwned[0]);

    LauncherValueTarget pendingValueTarget = LauncherValueTarget::None;
    int filteredCount = 1;

    for (int src = 1; src < argc; ++src) {
        const char* value = argv[src] ? argv[src] : "";

        if (pendingValueTarget != LauncherValueTarget::None) {
            LauncherValueTarget consumedTarget = pendingValueTarget;
            if (!ConsumeLauncherValueSwitch(consumedTarget, value)) {
                spdlog::error("ERROR: failed to consume launcher switch value from argv[{}]", src);
                FreeFilteredArgvOwned();
                return false;
            }
            if (consumedTarget == LauncherValueTarget::QlVersionIgnored) {
                spdlog::info(
                    "DIAGNOSTIC: consumed launcher switch value argv[{}] = {} (original 0x409950 appears to ignore retained qlver storage)",
                    src,
                    MaskedArgValue(value));
            } else {
                spdlog::info("DIAGNOSTIC: consumed launcher switch value argv[{}] = {}", src, MaskedArgValue(value));
            }
            pendingValueTarget = LauncherValueTarget::None;
            continue;
        }

        LauncherValueTarget newTarget = LauncherValueTargetForSwitch(value);
        if (newTarget != LauncherValueTarget::None) {
            pendingValueTarget = newTarget;
            spdlog::info("DIAGNOSTIC: consumed launcher switch '{}' during filtered argv build", value);
            continue;
        }

        if (ConsumeLauncherBooleanSwitch(value)) {
            spdlog::info("DIAGNOSTIC: consumed launcher switch '{}' during filtered argv build", value);
            continue;
        }

        g_FilteredArgvOwned[filteredCount] = DuplicateArgString(value);
        if (!g_FilteredArgvOwned[filteredCount]) {
            spdlog::error("ERROR: failed to duplicate filtered argv[{}] from src argv[{}]", filteredCount, src);
            FreeFilteredArgvOwned();
            return false;
        }
        spdlog::info(
            "DIAGNOSTIC: filtered argv[{}] duplicated at {} from src argv[{}] -> '{}'",
            filteredCount,
            static_cast<void*>(g_FilteredArgvOwned[filteredCount]),
            src,
            g_FilteredArgvOwned[filteredCount]);
        ++filteredCount;
    }

    if (pendingValueTarget != LauncherValueTarget::None) {
        spdlog::warn("WARNING: final launcher switch expected a value but argv ended early");
    }

    g_FilteredArgvOwned[filteredCount] = NULL;
    g_FilteredArgCount = static_cast<uint32_t>(filteredCount);
    g_FilteredArgv = g_FilteredArgvOwned;

    if (!g_LauncherSwitchNoPatch) {
        g_LauncherSwitchNoPatch = true;
        g_LauncherGlobal4C8B1D = false;
        spdlog::info("DIAGNOSTIC: forcing default nopatch branch semantics in replacement launcher");
    }

    ProbeOptionsCfgAutodetectGate();
    LogLauncherPreprocessingState();
    spdlog::info("DIAGNOSTIC: filtered argv final count = {}", filteredCount);
    return true;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("resurrections.log", "w");
    try {
        auto logger = spdlog::basic_logger_mt("clientstate", "resurrections_spdlog.log", true);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex&) {
    }
    AddVectoredExceptionHandler(1, reinterpret_cast<PVECTORED_EXCEPTION_HANDLER>(DiagnosticUnhandledExceptionFilter));
    SetUnhandledExceptionFilter(DiagnosticUnhandledExceptionFilter);

    Log("Matrix Online launcher reimplementation scaffold");
    Log("===============================================");
    Log("Mode: original startup order, no client-memory injection");
    Log("Default branch target: nopatch path");
    Log("DIAGNOSTIC: spdlog debug file = resurrections_spdlog.log");
    Log("");

    Log("NOTE: arg1/arg2 now use launcher-owned filtered argv storage, but launcher switch parsing/options.cfg preprocessing are still incomplete.");
    Log("NOTE: launcher-owned nopatch setup, arg5, arg6, arg7, and arg8 remain incomplete.");
    if (!ConfigureFilteredArgv(argc, argv)) {
        return FinishAndReturn(1);
    }
    Log("");

    spdlog::info(
        "DIAGNOSTIC: active launcher runtime path = binder-backed mediator + launcher object scaffold + InitClientDLL/RunClientDLL + launcher-owned auth begin");

    LoadLastWorldNameFromRegistry(g_LastWorldName, sizeof(g_LastWorldName));

    char mediatorSelectionName[64] = {0};
    if (g_LastWorldName[0]) {
        lstrcpynA(mediatorSelectionName, g_LastWorldName, sizeof(mediatorSelectionName));
        spdlog::info("DIAGNOSTIC: using persisted Last_WorldName as launcher selection name = '{}'", mediatorSelectionName);
    } else if (sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]) > 0) {
        std::strncpy(
            mediatorSelectionName,
            kRecoveredLauncherSelectionRecords[0].worldName,
            sizeof(mediatorSelectionName) - 1);
        mediatorSelectionName[sizeof(mediatorSelectionName) - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: no persisted Last_WorldName; defaulting launcher selection name to first recovered world '{}'",
            mediatorSelectionName);
    } else {
        std::strcpy(mediatorSelectionName, "standalone");
        spdlog::warn(
            "DIAGNOSTIC: no persisted Last_WorldName and no recovered launcher selection records are available; falling back to '{}'",
            mediatorSelectionName);
    }

    const RecoveredLauncherSelectionRecord* recoveredSelection =
        FindRecoveredLauncherSelectionRecord(mediatorSelectionName);
    if (recoveredSelection) {
        std::strncpy(mediatorSelectionName, recoveredSelection->worldName, sizeof(mediatorSelectionName) - 1);
        mediatorSelectionName[sizeof(mediatorSelectionName) - 1] = '\0';
        g_CLauncherFieldA8 = recoveredSelection->variantIndexHigh8;
        g_CLauncherFieldAC = recoveredSelection->worldIndexLow24;
        spdlog::info(
            "DIAGNOSTIC: seeded launcher selection defaults from recovered world '{}' -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} worldType={} variantState={} routePrefix='{}'",
            recoveredSelection->worldName,
            g_CLauncherFieldA8,
            g_CLauncherFieldAC,
            BuildPackedArg7Selection(),
            (unsigned)recoveredSelection->worldType,
            (unsigned)recoveredSelection->variantState,
            recoveredSelection->routeHostPrefix ? recoveredSelection->routeHostPrefix : "");
    } else if (mediatorSelectionName[0] && lstrcmpiA(mediatorSelectionName, "standalone") != 0) {
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection defaults for world '{}'; keeping zeroed launcher arg7 fields until that world has a recovered launcher-owned selection record",
            mediatorSelectionName);
    }

    g_PackedArg7Selection = BuildPackedArg7Selection();
    if ((g_CLauncherFieldA8 | g_CLauncherFieldAC) != 0) {
        spdlog::info("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x{:08x}", g_PackedArg7Selection);
    }

    const uint32_t mediatorSelectedWorldType = recoveredSelection ? recoveredSelection->worldType : 1u;
    const uint32_t mediatorSelectedVariantState = recoveredSelection ? recoveredSelection->variantState : 0u;

    const uint32_t nopatchParsedValue = FloatBitsFromCString("0.1");
    uint32_t nopatchClientVersionValue = nopatchParsedValue;
    char nopatchClientVersionString[32] = {0};
    if (TryBuildOriginalClientVersionFloatString(
            nopatchClientVersionString,
            sizeof(nopatchClientVersionString),
            &nopatchClientVersionValue)) {
        Log(
            "DIAGNOSTIC: rebuilt nopatch client-version float from client.dll version info = '%s' (0x%08x)",
            nopatchClientVersionString,
            nopatchClientVersionValue);
    } else {
        Log(
            "DIAGNOSTIC: failed to rebuild nopatch client-version float from client.dll version info; falling back to 0.1 (0x%08x)",
            nopatchClientVersionValue);
    }

    if (!PreloadDependencies()) {
        Log("ERROR: preload failed");
        return FinishAndReturn(1);
    }

    DiagnosticInstallMediatorViaBinderScaffold(&g_pILTLoginMediatorDefault);

    // =============================================================================
    // arg6 / sibling mediator configuration for InitClientDLL
    // Address anchor: launcher.exe:0x4d3584 = ILTLoginMediator sibling slot used by launcher-side selection resolution
    // =============================================================================
    Log("=== configuring arg6 / sibling mediator state for InitClientDLL ===");
    
    const uint32_t selectedHighByte = (g_PackedArg7Selection >> 24) & 0xffu;
    const uint32_t selectionPackedLow24 = g_PackedArg7Selection & 0x00ffffffu;
    const uint32_t worldUpperBoundExclusive =
        (selectionPackedLow24 < 0xffu) ? (selectionPackedLow24 + 1u) : 1u;
    const uint32_t variantUpperBoundExclusive =
        (selectedHighByte < 0xffu) ? (selectedHighByte + 1u) : 1u;
    DiagnosticConfigureMediatorSelection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        mediatorSelectionName,
        mediatorSelectionName,
        selectionPackedLow24,
        selectedHighByte,
        mediatorSelectedWorldType,
        mediatorSelectedVariantState);
    DiagnosticConfigureMediatorProfileName(g_AuthUsername[0] ? g_AuthUsername : NULL);
    DiagnosticConfigureMediatorAuthName(g_AuthUsername[0] ? g_AuthUsername : NULL);
    DiagnosticConfigureMediatorAuthPassword(g_AuthPassword[0] ? g_AuthPassword : NULL);

    DiagnosticConfigureLoginControllerSelectedWorldIndex(selectionPackedLow24);
    DiagnosticConfigureLoginControllerCharacterSeed(
        g_LauncherCharacter[0] ? g_LauncherCharacter : NULL,
        g_LauncherSession[0] ? g_LauncherSession : NULL);

    DiagnosticApplyDefaultNopatchMediatorConfig(
        g_pILTLoginMediatorDefault,
        nopatchParsedValue,
        nopatchClientVersionValue);

    if (g_pILTLoginMediatorDefault) {
        g_pILTLoginMediatorSelection3584 = g_pILTLoginMediatorDefault;
        Log(
            "DIAGNOSTIC: reusing current ILTLoginMediator.Default object as sibling 0x4d3584 selection slot (%p)",
            g_pILTLoginMediatorSelection3584);

        uint32_t resolvedA8 = g_CLauncherFieldA8;
        uint32_t resolvedAC = g_CLauncherFieldAC;
        char resolvedWorldName[sizeof(g_LastWorldName)] = {0};
        if (DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediatorSelection3584,
                g_CLauncherFieldAC,
                g_CLauncherFieldA8,
                &resolvedA8,
                &resolvedAC,
                resolvedWorldName,
                sizeof(resolvedWorldName))) {
            g_CLauncherFieldA8 = resolvedA8;
            g_CLauncherFieldAC = resolvedAC;
            g_PackedArg7Selection = BuildPackedArg7Selection();
            if (resolvedWorldName[0]) {
                std::strncpy(g_LastWorldName, resolvedWorldName, sizeof(g_LastWorldName) - 1);
                g_LastWorldName[sizeof(g_LastWorldName) - 1] = '\0';
                StoreLastWorldNameInRegistry(g_LastWorldName);
            }
            Log(
                "DIAGNOSTIC: arg7 rebuilt through sibling 0x4d3584-style mediator selection slot -> a8=0x%08x ac=0x%08x packed=0x%08x world='%s'",
                g_CLauncherFieldA8,
                g_CLauncherFieldAC,
                g_PackedArg7Selection,
                g_LastWorldName[0] ? g_LastWorldName : mediatorSelectionName);
        }
    }

    DiagnosticInstallLauncherObjectStub(&g_pLauncherObject6304, g_pILTLoginMediatorDefault);

    const char authServerDnsName[] = "auth.lith.thematrixonline.net";
    const uint16_t authServerPort = 11000;
    const char marginServerSuffix[] = ".lith.thematrixonline.net";
    const uint16_t marginServerPort = 10000;

    char marginRoutePrefix[256] = {0};
    if (recoveredSelection && recoveredSelection->routeHostPrefix && recoveredSelection->routeHostPrefix[0]) {
        std::strncpy(marginRoutePrefix, recoveredSelection->routeHostPrefix, sizeof(marginRoutePrefix) - 1);
        marginRoutePrefix[sizeof(marginRoutePrefix) - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: using recovered route host prefix '{}' for world '{}'",
            marginRoutePrefix,
            recoveredSelection->worldName);
    } else {
        LowercaseAsciiCopy(marginRoutePrefix, sizeof(marginRoutePrefix), mediatorSelectionName);
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
        Log("WARNING: pre-client environment scaffold failed to initialize");
    }

    RunOptionsCfgAutodetectStepIfNeeded();
    DiagnosticStartWindowTrace();

    // Original order from launcher.exe:
    //   0x40a380  -> build 0x4d6304
    //   0x402ec0  -> environment setup
    //   0x40a780  -> LoadLibraryA("cres.dll")
    //   0x40a420  -> LoadLibraryA("client.dll")
    //   0x40a4d0  -> resolve exports + Init/Run/Term/Error path

    Log("=== Original-path gaps still missing ===");
    Log("arg1/arg2 status: launcher-owned filtered argv storage present, but original 0x409950 switch handling / options.cfg preprocessing are still incomplete");
    spdlog::info("arg5 status: current launcher object scaffold materialized 0x4d6304-style object (not yet faithful ctor/internal state)");
    spdlog::info("arg6 status: current binder-backed path materialized ILTLoginMediator.Default (not yet faithful launcher reconstruction)");
    if (g_pILTLoginMediatorSelection3584) {
        spdlog::info("arg7 status: sibling 0x4d3584-style ILTLoginMediator selection slot currently reuses the active mediator object and rebuilds a8/ac through +0xfc/+0x100/+0xe4");
    } else {
        Log("missing: reconstruct sibling ILTLoginMediator.Default-style slot at 0x4d3584 for launcher-owned arg7 selection resolution");
    }
    if (g_PreclientEnvironment.threadHandle) {
        spdlog::info("pre-client env status: current 0x402ec0-style launcher thread/message scaffold active (not yet faithful original class/import path)");
    } else {
        Log("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
    }
    Log("");

    if (!LoadCresDLL()) {
        Log("ERROR: failed to load cres.dll");
        return FinishAndReturn(1);
    }

    if (!LoadClientDLL()) {
        Log("ERROR: failed to load client.dll");
        return FinishAndReturn(1);
    }

    if (!ResolveClientExports()) {
        Log("ERROR: missing required client exports");
        return FinishAndReturn(1);
    }

    LogKnownStartupState();
    Log("");

    const bool allowInitWithCurrentStartupScaffold =
        g_pILTLoginMediatorDefault && g_pLauncherObject6304 && g_pILTLoginMediatorSelection3584;

    if (!allowInitWithCurrentStartupScaffold) {
        spdlog::error("Refusing to call InitClientDLL with incomplete launcher state.");
        return FinishAndReturn(2);
    }

    spdlog::info("=== Calling InitClientDLL with current active startup scaffold ===");
    spdlog::info("DIAGNOSTIC: proceeding because the current launcher path now provides arg5 build/register, binder-backed arg6, and sibling-slot arg7 rebuild");
    CaptureInitClientFrameSnapshot();
    DiagnosticSnapshotArgvMemory();
    Log("DIAGNOSTIC: argv memory snapshotted for crash analysis");
    
    if (g_pILTLoginMediatorDefault) {
        Log("arg6 mediator object prepared for InitClientDLL: %p", g_pILTLoginMediatorDefault);
    } else {
        Log("WARNING: arg6 mediator object is NULL");
    }
    
    int initResult = g_InitClientDLL(
        g_FilteredArgCount,
        g_FilteredArgv,
        g_hClient,
        g_hCres,
        g_pLauncherObject6304,
        g_pILTLoginMediatorDefault,
        g_PackedArg7Selection,
        g_FlagByte);

    Log("InitClientDLL returned: %d", initResult);
    Log("launcher export SetMasterDatabase observed: %p", g_pClientDBFromCallback);

    // Original client code returns 1 on the observed success path and 0 / negative values on failure paths.
    // Do not treat non-zero generically as failure here.
    const bool initSucceeded = (initResult > 0);
    if (!initSucceeded) {
        Log("InitClientDLL failed.");
        return FinishAndReturn(1);
    }

    // Address anchors for the original launcher-owned auth start handoff:
    // - launcher.exe:0x4207c0 = LaunchPadClient_OnConnectionStatusCheck
    // - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection
    // - launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection
    if (DiagnosticCanBeginAuthConnection()) {
        const uint32_t authConnectResult = DiagnosticBeginAuthConnection();
        Log("DIAGNOSTIC: post-init auth auto-begin result = 0x%08x", (unsigned)authConnectResult);
    }

    spdlog::info("=== Calling RunClientDLL on the active launcher path ===");
    const int runResult = g_RunClientDLL();
    Log("RunClientDLL returned: %d", runResult);

    return FinishAndReturn(0);
}
