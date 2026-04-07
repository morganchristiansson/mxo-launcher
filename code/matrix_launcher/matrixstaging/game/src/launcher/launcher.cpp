#include "launcher.h"

#include <windows.h>
#include <shellapi.h>
#include <winver.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <conio.h>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "autodetectdialog.h"
#include "../../../../src/diagnostics.h"
#include "../../../../src/launcher_network_object_abi.h"
#include "../libltclientlogin/loginmediator_base.h"
#include "../../../runtime/src/libltbase/launchercommandline.h"

// anchor: launcher.exe:0x40a55c..0x40a5a4 / caller-clean 8-argument export frame
using InitClientDLLFunc = int (*)(
    uint32_t filteredArgCount,
    char** filteredArgv,
    HMODULE hClientDll,
    HMODULE hCresDll,
    void* launcherNetworkObject,
    void* pILTLoginMediatorDefault,
    uint32_t packedArg7Selection,
    uint32_t launcherInitClientFlagByte);
using RunClientDLLFunc = int (*)();
using TermClientDLLFunc = int (*)();
using ErrorClientDLLFunc = const char* (*)();

HMODULE g_hCres = NULL;                     // original: [0x4d2c4c]
HMODULE g_hClient = NULL;                   // original: [0x4d2c50]
int g_CrtArgc = 0;
char** g_CrtArgv = NULL;
uint32_t g_LauncherFilteredArgCount = 0;    // original: [0x4d2c5c]
char** g_LauncherFilteredArgv = NULL;       // original: [0x4d2c60]
mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;
mxo::launcher::CLauncher g_Launcher;
void* g_pLauncherObject6304 = NULL;
void* g_pILTLoginMediatorDefault = NULL;
uint8_t g_LauncherInitClientFlagByte = 0;   // original byte: [0x4d2c69]
char g_LastWorldName[256] = {0};

extern bool DiagnosticInitializePreclientEnvironmentLike402EC0();
extern bool IsRecoveredPreclientEnvironmentActive();

namespace {

const char* kLauncherRegistryKeyPath = "Software\\Monolith Productions\\The Matrix Online\\";
bool g_PreClientAuthAndCharacterSelectionCompleted = false;

struct RecoveredLauncherSelectionRecord {
    const char* selectionName;
    const char* routeHostPrefix;
    uint32_t selectionGateByte100;
    uint32_t variantState;
};

constexpr uint32_t kRecoveredSelectionWorldIndexLow24 = 0x00002au;

const RecoveredLauncherSelectionRecord kRecoveredLauncherSelectionRecords[] = {
    {"Reality", "reality", 1u, 0u},
};

template <typename T>
T ResolveProc(HMODULE module, const char* name) {
    FARPROC proc = GetProcAddress(module, name);
    T typed = nullptr;
    static_assert(sizeof(typed) == sizeof(proc), "function pointer size mismatch");
    std::memcpy(&typed, &proc, sizeof(typed));
    return typed;
}

void LowercaseAsciiCopy(char* destination, size_t destinationSize, const char* source) {
    if (!destination || destinationSize == 0) return;
    destination[0] = '\0';
    if (!source) return;

    size_t write = 0;
    for (size_t i = 0; source[i] && write + 1 < destinationSize; ++i) {
        unsigned char c = static_cast<unsigned char>(source[i]);
        destination[write++] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    }
    destination[write] = '\0';
}

const char* MaskedArgValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

constexpr WORD kMatrixConsoleGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY;

// Replacement-only text-mode launcher divergence:
// - original page-7 list builder `0x40e480` inserts the `"- - -"` create-character sentinel only
//   while fewer than 3 active entries match the selected world
// - current text-mode host deliberately raises that soft cap so manual create-character testing is
//   still reachable on accounts that already have more than 3 characters
static constexpr uint32_t kTextModeCreateCharacterSoftLimit = 10u;

void WriteMatrixConsoleText(const char* text, bool appendNewline) {
    const char* safeText = text ? text : "<null>";
    HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO consoleInfo = {};
    const bool haveConsoleInfo =
        errorHandle != INVALID_HANDLE_VALUE &&
        errorHandle != nullptr &&
        GetConsoleScreenBufferInfo(errorHandle, &consoleInfo) != 0;

    if (haveConsoleInfo) {
        SetConsoleTextAttribute(errorHandle, kMatrixConsoleGreen);
        std::fputs(safeText, stderr);
        if (appendNewline) {
            std::fputc('\n', stderr);
        }
        std::fflush(stderr);
        SetConsoleTextAttribute(errorHandle, consoleInfo.wAttributes);
        return;
    }

    std::fputs("\x1b[92m", stderr);
    std::fputs(safeText, stderr);
    if (appendNewline) {
        std::fputc('\n', stderr);
    }
    std::fputs("\x1b[0m", stderr);
    std::fflush(stderr);
}

void WriteMatrixConsoleFormattedLine(const char* format, ...) {
    if (!format) {
        return;
    }

    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    WriteMatrixConsoleText(buffer, true);
}

bool ReadInteractiveLauncherField(const char* prompt, char* buffer, size_t bufferSize) {
    if (!prompt || !buffer || bufferSize < 2u) {
        return false;
    }

    WriteMatrixConsoleText(prompt, false);

    if (!std::fgets(buffer, static_cast<int>(bufferSize), stdin)) {
        buffer[0] = '\0';
        return false;
    }

    size_t length = std::strlen(buffer);
    while (length != 0u && (buffer[length - 1u] == '\n' || buffer[length - 1u] == '\r')) {
        buffer[--length] = '\0';
    }
    return true;
}

bool ReadInteractiveLauncherPasswordField(const char* prompt, char* buffer, size_t bufferSize) {
    if (!prompt || !buffer || bufferSize < 2u) {
        return false;
    }

    buffer[0] = '\0';
    WriteMatrixConsoleText(prompt, false);

    HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD consoleMode = 0;
    const bool haveConsoleInput =
        inputHandle != INVALID_HANDLE_VALUE &&
        inputHandle != nullptr &&
        GetConsoleMode(inputHandle, &consoleMode) != 0;

    if (!haveConsoleInput) {
        if (!std::fgets(buffer, static_cast<int>(bufferSize), stdin)) {
            buffer[0] = '\0';
            return false;
        }
        size_t length = std::strlen(buffer);
        while (length != 0u && (buffer[length - 1u] == '\n' || buffer[length - 1u] == '\r')) {
            buffer[--length] = '\0';
        }
        return true;
    }

    size_t write = 0;
    while (true) {
        const int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            std::fputc('\n', stderr);
            std::fflush(stderr);
            buffer[write] = '\0';
            return true;
        }
        if ((ch == '\b' || ch == 127) && write != 0u) {
            --write;
            continue;
        }
        if (ch == 0 || ch == 0xe0) {
            (void)_getch();
            continue;
        }
        if (write + 1u < bufferSize) {
            buffer[write++] = static_cast<char>(ch);
        }
    }
}

bool ReadInteractiveLauncherIndex(const char* prompt, uint32_t upperBoundExclusive, uint32_t* outIndex) {
    if (!prompt || !outIndex || upperBoundExclusive == 0u) {
        return false;
    }

    char inputBuffer[64] = {};
    if (!ReadInteractiveLauncherField(prompt, inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(inputBuffer, &end, 10);
    if (end == inputBuffer || (end && *end != '\0') || parsed >= upperBoundExclusive) {
        return false;
    }

    *outIndex = static_cast<uint32_t>(parsed);
    return true;
}

bool DeleteLauncherProfileDirectoryForCharacter(const char* profileRootName, const char* characterName) {
    if (!profileRootName || !profileRootName[0] || !characterName || !characterName[0]) {
        return false;
    }

    char cwdBuffer[MAX_PATH] = {};
    if (!_getcwd(cwdBuffer, sizeof(cwdBuffer))) {
        return false;
    }

    const size_t cwdLength = std::strlen(cwdBuffer);
    std::snprintf(
        cwdBuffer + cwdLength,
        sizeof(cwdBuffer) - cwdLength,
        "\\Profiles\\%s\\%s",
        profileRootName,
        characterName);

    if (_access(cwdBuffer, 0) != 0) {
        return true;
    }

    SHFILEOPSTRUCTA deleteProfileDirectoryFileOp = {};
    deleteProfileDirectoryFileOp.wFunc = FO_DELETE;
    deleteProfileDirectoryFileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    deleteProfileDirectoryFileOp.pFrom = cwdBuffer;

    const size_t pathLength = std::strlen(cwdBuffer);
    cwdBuffer[pathLength + 1u] = '\0';
    return SHFileOperationA(&deleteProfileDirectoryFileOp) == 0;
}

bool PromptForMissingLauncherCredentialsIfNeeded() {
    const bool missingUser = (g_LauncherCommandLine.AuthUsername()[0] == '\0');
    const bool missingPwd = (g_LauncherCommandLine.AuthPassword()[0] == '\0');
    if (!missingUser && !missingPwd) {
        return true;
    }

    WriteMatrixConsoleText("Wake up, Neo...", true);

    char inputBuffer[256] = {};
    if (missingUser) {
        if (!ReadInteractiveLauncherField("Username: ", inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive username prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthUsername(inputBuffer);
    }
    if (missingPwd) {
        if (!ReadInteractiveLauncherPasswordField("Password: ", inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive password prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthPassword(inputBuffer);
    }

    spdlog::info(
        "DIAGNOSTIC: replacement pre-client auth prompted for missing launcher credentials username={} password={}",
        MaskedArgValue(g_LauncherCommandLine.AuthUsername()),
        MaskedArgValue(g_LauncherCommandLine.AuthPassword()));
    return true;
}

void TrimAsciiWhitespaceInPlace(char* text) {
    if (!text) {
        return;
    }

    char* begin = text;
    while (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n') {
        ++begin;
    }
    if (begin != text) {
        std::memmove(text, begin, std::strlen(begin) + 1u);
    }

    size_t length = std::strlen(text);
    while (length != 0u) {
        const char trailing = text[length - 1u];
        if (trailing != ' ' && trailing != '\t' && trailing != '\r' && trailing != '\n') {
            break;
        }
        text[--length] = '\0';
    }
}

// anchor: launcher.exe:0x408400
// anchor: launcher.exe:0x41ecd0
// No-GUI launcher bridge over the original page-6 submit helper contract:
// - trim username
// - copy password
// - zero block40/block50
// - leave the small-string session-token field empty on the interactive username/password path
bool BuildNoGuiProcessLoginRequestInput(
    const char* username,
    const char* password,
    mxo::ltlogin::ProcessLoginRequestInputSketch* outInput) {
    if (!username || !password || !outInput) {
        return false;
    }

    *outInput = {};

    char trimmedUsername[sizeof(outInput->inlineString00)] = {};
    std::strncpy(trimmedUsername, username, sizeof(trimmedUsername) - 1u);
    trimmedUsername[sizeof(trimmedUsername) - 1u] = '\0';
    TrimAsciiWhitespaceInPlace(trimmedUsername);

    std::strncpy(outInput->inlineString00.data(), trimmedUsername, outInput->inlineString00.size() - 1u);
    outInput->inlineString00[outInput->inlineString00.size() - 1u] = '\0';
    std::strncpy(outInput->inlineString20.data(), password, outInput->inlineString20.size() - 1u);
    outInput->inlineString20[outInput->inlineString20.size() - 1u] = '\0';
    outInput->string60.begin = nullptr;
    outInput->string60.current = nullptr;
    outInput->string60.capacity = nullptr;
    outInput->flag6C = 0u;
    return true;
}

void LogLauncherPreprocessingState() {
    spdlog::info("=== Launcher switch preprocessing ===");
    spdlog::info("auth username      = {}", MaskedArgValue(g_LauncherCommandLine.AuthUsername()));
    spdlog::info("auth password      = {}", MaskedArgValue(g_LauncherCommandLine.AuthPassword()));
    spdlog::info("launcher character  = {}", MaskedArgValue(g_LauncherCommandLine.LauncherCharacter()));
    spdlog::info("launcher session    = {}", MaskedArgValue(g_LauncherCommandLine.LauncherSession()));
    spdlog::info(
        "launcher flags      = clone:{} silent:{} explicit-nopatch:{} recover:{} deletechar:{} justpatch:{} noeula:{} skiplaunch:{} lptest:{}",
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
        "launcher globals    = eulaFlowEnabled:{} patchFlowEnabled:{} optionsCfgAutodetectGate:{} recoverRequested:{} justPatchRequested:{} runningAsTempMatrixClone:{} replacement-default-nopatch:{}",
        g_LauncherCommandLine.EulaFlowEnabled() ? 1 : 0,
        g_LauncherCommandLine.PatchFlowEnabled() ? 1 : 0,
        g_LauncherCommandLine.LauncherGlobal4D2C64() ? 1 : 0,
        g_LauncherCommandLine.SwitchRecover() ? 1 : 0,
        g_LauncherCommandLine.SwitchJustPatch() ? 1 : 0,
        g_LauncherCommandLine.SwitchClone() ? 1 : 0,
        g_LauncherCommandLine.ReplacementDefaultNoPatchPolicyActive() ? 1 : 0);
}

bool PreloadDependencies() {
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
        if (std::strcmp(dlls[i], "r3d9.dll") == 0) {
            const bool installedD3DCompileHook = DiagnosticInstallR3d9D3DCompileHook(h);
            spdlog::info(
                "preload {:12s} : DiagnosticInstallR3d9D3DCompileHook={} ({})",
                dlls[i],
                installedD3DCompileHook ? 1 : 0,
                fmt::ptr(h));
            const bool installedDirect3DCreate9Hook = DiagnosticInstallR3d9Direct3DCreate9Hook(h);
            spdlog::info(
                "preload {:12s} : DiagnosticInstallR3d9Direct3DCreate9Hook={} ({})",
                dlls[i],
                installedDirect3DCreate9Hook ? 1 : 0,
                fmt::ptr(h));
        }
    }
    return true;
}

bool LoadLastWorldNameFromRegistry(char* out, DWORD outSize) {
    if (!out || outSize < 2) return false;
    out[0] = '\0';

    HKEY key = NULL;
    LONG openResult = RegOpenKeyExA(HKEY_LOCAL_MACHINE, kLauncherRegistryKeyPath, 0, KEY_QUERY_VALUE, &key);
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

bool StoreLastWorldNameInRegistry(const char* selectionName) {
    if (!selectionName || !selectionName[0]) return false;

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

    const size_t byteCount = std::strlen(selectionName) + 1;
    LONG setResult = RegSetValueExA(
        key,
        "Last_WorldName",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(selectionName),
        static_cast<DWORD>(byteCount));
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        spdlog::warn("DIAGNOSTIC: HKLM Last_WorldName write failed ({})", (long)setResult);
        return false;
    }

    spdlog::info(
        "DIAGNOSTIC: persisted HKLM Last_WorldName='{}'{}",
        selectionName,
        (disposition == REG_CREATED_NEW_KEY) ? " (created key)" : "");
    return true;
}

void CanonicalizeLauncherSelectionLookupName(char* destination, size_t destinationSize, const char* source) {
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

const RecoveredLauncherSelectionRecord* FindRecoveredLauncherSelectionRecord(const char* selectionName) {
    if (!selectionName || !selectionName[0]) return NULL;

    char normalizedInput[128] = {0};
    CanonicalizeLauncherSelectionLookupName(normalizedInput, sizeof(normalizedInput), selectionName);
    if (!normalizedInput[0]) return NULL;

    for (size_t i = 0; i < sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]); ++i) {
        const RecoveredLauncherSelectionRecord& record = kRecoveredLauncherSelectionRecords[i];

        char normalizedRecordWorld[128] = {0};
        CanonicalizeLauncherSelectionLookupName(normalizedRecordWorld, sizeof(normalizedRecordWorld), record.selectionName);
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

void LogKnownStartupState(const mxo::launcher::CLauncher& launcher) {
    spdlog::info("=== Known startup frame ===");
    spdlog::info("arg1 filteredArgCount        = 0x{:08x}", g_LauncherFilteredArgCount);
    spdlog::info("arg2 filteredArgv            = {}", fmt::ptr(g_LauncherFilteredArgv));
    spdlog::info("arg3 hClientDll              = {}", fmt::ptr(g_hClient));
    spdlog::info("arg4 hCresDll                = {}", fmt::ptr(g_hCres));
    spdlog::info("arg5 launcherNetworkObject   = {}", fmt::ptr(g_pLauncherObject6304));
    LauncherLogNetworkEngineAbiShellDispatchState(g_pLauncherObject6304, "InitClientDLL-args");
    spdlog::info("arg6 ILTLoginMediatorDefault = {}", fmt::ptr(g_pILTLoginMediatorDefault));
    spdlog::info("CLauncher+0xa8 placeholder   = 0x{:08x}", launcher.m_FieldA8);
    spdlog::info("CLauncher+0xac placeholder   = 0x{:08x}", launcher.m_FieldAC);
    spdlog::info("Last_WorldName               = {}", g_LastWorldName[0] ? g_LastWorldName : "<unavailable>");
    const uint32_t packedArg7Selection = launcher.BuildPackedArg7Selection();
    spdlog::info("arg7 packedArg7Selection     = 0x{:08x}", packedArg7Selection);
    spdlog::info(
        "arg8 launcherInitClientFlagByte = 0x{:02x}",
        static_cast<unsigned>(g_LauncherInitClientFlagByte));
}

void LogClientLifecycleFailure(const char* phase, ErrorClientDLLFunc errorClientDLL) {
    const char* clientError = errorClientDLL ? errorClientDLL() : nullptr;
    spdlog::error("{} failed{}{}",
        phase ? phase : "Client DLL lifecycle phase",
        clientError ? ": " : "",
        clientError ? clientError : "");
}

} // namespace

namespace mxo {
namespace launcher {

// anchor: launcher.exe:0x40a55c / 0x40b430
uint32_t CLauncher::BuildPackedArg7Selection() const {
    return (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24);
}

// anchor: launcher.exe:0x409950 / 0x4173d0
bool CLauncher::ParseCommandLineStage() const {
    spdlog::info("=== Launcher argv preprocessing ===");
    spdlog::info(
        "DIAGNOSTIC: launcher.exe uses ParseCommandLine(0x409950) followed by "
        "CConsoleVar_ParseCommandLineAndConfig(0x4173d0)");

    g_LauncherFilteredArgCount = 0;
    g_LauncherFilteredArgv = NULL;

    if (!g_LauncherCommandLine.ParseCommandLine(g_CrtArgc, g_CrtArgv)) {
        spdlog::error("ERROR: launcher ParseCommandLine stage failed");
        return false;
    }

    g_LauncherFilteredArgCount = g_LauncherCommandLine.FilteredArgCount();
    g_LauncherFilteredArgv = g_LauncherCommandLine.FilteredArgv();

    if (!g_LauncherCommandLine.ParseRuntimeConsoleVariables()) {
        for (const std::string& errorLine : g_LauncherCommandLine.RuntimeConsoleErrors().lines) {
            spdlog::error("{}", errorLine);
        }
        spdlog::error("ERROR: CConsoleVar_ParseCommandLineAndConfig rejected the filtered argv");
        return false;
    }

    // UNANCHORED replacement bridge toward the original launcher-owned login prompt:
    // - newer nopatch/manual-login tightening now gives the closest launcher dialog corridor:
    //   - page `2` primary button (`dialog +0x204`, command `11`) enters page `6` when
    //     `g_LauncherPatchFlowEnabled == 0`
    //   - page `6` rich-edit host (`dialog +0x95c`) uses
    //     `0x408ee0/0x408840/0x408400/0x4091d0` to gather username, gather password, submit the
    //     exact `0x41ecd0`-style stack block, and handle success/error callbacks
    //   - raw-vtable clarification now closes the submit bridge directly:
    //     `0x408400` calls resolved mediator slot `+0x30`, and raw mediator vtable memory stores
    //     `0x41ecd0 = ProcessLoginRequest` at that same displacement
    // - when `-user` / `-pwd` are supplied, the original launcher still behaves like prefill +
    //   auto-submit for the same launcher flow rather than as a bypass
    // - replacement parse handling therefore keeps those switches only as optional prefill; any
    //   console-host prompt is deferred until the later pre-client auth stage so this parse/config
    //   stage stays closer to the original ownership split
    // - replacement startup still keeps the same owner boundary on `0x41ecd0`, but now prefers
    //   mirroring the launcher submit through the binder-backed raw `+0x30` surface before falling
    //   back to the direct owner call
    // - character choice is no longer required up front on the CLI path; after successful auth the
    //   replacement now chooses from the recovered launcher-owned character list before client load

    if (!g_LauncherCommandLine.SwitchNoPatch()) {
        g_LauncherCommandLine.ApplyReplacementDefaultNoPatchPolicy();
        spdlog::info(
            "UNANCHORED: replacement launcher forces the effective nopatch branch after the faithful "
            "0x409950 -> 0x4173d0 parse/config stages");
    }

    if (g_LauncherCommandLine.LauncherGlobal4D2C64()) {
        spdlog::info(
            "UNANCHORED: parser preserved launcher.exe:0x409f34 autodetect gate and the replacement "
            "now executes a no-GUI wrapper for the later 0x40b75a dialog/helper path");
    }

    LogLauncherPreprocessingState();
    spdlog::info("DIAGNOSTIC: filtered argv final count = {}", g_LauncherFilteredArgCount);
    return true;
}

// anchor: launcher.exe:0x40a380
bool CLauncher::InitializeThreadPerClientTCPEngine() const {
    // Call-shape note:
    // - `CLauncher::InitInstance` reaches this through `mov ecx, ebx; call 0x40a380`
    // - current Ghidra still decompiles the body as if `this` were unused, but keeping the source
    //   as a `CLauncher` method preserves the original `InitInstance` ownership/call boundary
    if (!g_pILTLoginMediatorDefault) {
        spdlog::error("launcher.exe:0x40a380 requires ILTLoginMediator.Default before arg5 build/register");
        return false;
    }

    const int registerResult =
        LauncherInstallNetworkEngineAbiShell(&g_pLauncherObject6304, g_pILTLoginMediatorDefault);
    const bool operationSucceeded = (registerResult < 1);
    spdlog::info(
        "DIAGNOSTIC: 0x40a380-style arg5 build/register mediatorResult={} object={} success={}",
        registerResult,
        fmt::ptr(g_pLauncherObject6304),
        operationSucceeded ? 1 : 0);
    return operationSucceeded;
}

// anchor: launcher.exe:0x40a780
bool CLauncher::LoadCresDLL() const {
    spdlog::info("=== Load cres.dll ===");
    g_hCres = LoadLibraryA("cres.dll");
    spdlog::info("cres.dll handle: {}", fmt::ptr(g_hCres));
    return g_hCres != NULL;
}

// anchor: launcher.exe:0x40a420
bool CLauncher::LoadClientDLL() const {
    spdlog::info("=== Load client.dll ===");
    g_hClient = LoadLibraryA("client.dll");
    spdlog::info("client.dll handle: {}", fmt::ptr(g_hClient));
    return g_hClient != NULL;
}

// anchor: launcher.exe:0x40a760
void CLauncher::UnloadClientDLL() const {
    if (g_hClient) {
        FreeLibrary(g_hClient);
        g_hClient = NULL;
    }
}

// anchor: launcher.exe:0x40a7a0
void CLauncher::UnloadCresDLL() const {
    if (g_hCres) {
        FreeLibrary(g_hCres);
        g_hCres = NULL;
    }
}

// UNANCHORED: replacement-only synthesis inside launcher.exe:0x40b430 before the later
// 0x40b739..0x40b7af continuation corridor. This groups state that the replacement must recover
// in order to feed arg5/arg6/arg7 on the active nopatch path, without claiming a real original
// helper boundary.
bool CLauncher::BuildRecoveredSelectionNameAndState(char* selectionName, size_t selectionNameCapacity) {
    if (!selectionName || selectionNameCapacity == 0u) {
        return false;
    }

    std::memset(selectionName, 0, selectionNameCapacity);

    // Replacement-only stand-in for launcher-owned selection fallback state that must already exist
    // by the time 0x40a4d0 consumes [this+0xa8]/[this+0xac]. Final world writeback is deferred to
    // the later pre-client character-selection stage after the character index is read.
    if (sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]) > 0) {
        std::strncpy(
            selectionName,
            kRecoveredLauncherSelectionRecords[0].selectionName,
            selectionNameCapacity - 1);
        selectionName[selectionNameCapacity - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: defaulting launcher selection name to first recovered world '{}'",
            selectionName);
    } else {
        std::strcpy(selectionName, "standalone");
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection records are available; falling back to '{}'",
            selectionName);
    }

    const RecoveredLauncherSelectionRecord* recoveredSelection =
        FindRecoveredLauncherSelectionRecord(selectionName);
    if (recoveredSelection) {
        std::strncpy(selectionName, recoveredSelection->selectionName, selectionNameCapacity - 1);
        selectionName[selectionNameCapacity - 1] = '\0';
        m_FieldA8 = 0u;
        m_FieldAC = kRecoveredSelectionWorldIndexLow24;
        const uint32_t packedArg7Selection = BuildPackedArg7Selection();
        spdlog::info(
            "DIAGNOSTIC: seeded launcher selection defaults from recovered world '{}' -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} selectionGateByte100={} variantState={} routePrefix='{}'",
            recoveredSelection->selectionName,
            m_FieldA8,
            m_FieldAC,
            packedArg7Selection,
            (unsigned)recoveredSelection->selectionGateByte100,
            (unsigned)recoveredSelection->variantState,
            recoveredSelection->routeHostPrefix ? recoveredSelection->routeHostPrefix : "");
    } else if (selectionName[0] && lstrcmpiA(selectionName, "standalone") != 0) {
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection defaults for world '{}'; keeping zeroed launcher arg7 fields until that world has a recovered launcher-owned selection record",
            selectionName);
    }

    const uint32_t packedArg7Selection = BuildPackedArg7Selection();
    if ((m_FieldA8 | m_FieldAC) != 0) {
        spdlog::info("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x{:08x}", packedArg7Selection);
    }

    // Replacement-only nopatch bookkeeping now stays local to this stage instead of crossing the
    // selection-name handoff.
    const uint32_t nopatchLauncherVersionValue = g_LauncherCommandLine.NoPatchLauncherVersionValue();
    const uint32_t nopatchClientVersionValue = g_LauncherCommandLine.NoPatchClientVersionValue();

    if (g_LauncherCommandLine.NoPatchLauncherVersionString()[0]) {
        spdlog::info(
            "DIAGNOSTIC: explicit -nopatch rebuilt packed launcher-version dword from launcher.exe version info = '{}' (0x{:08x})",
            g_LauncherCommandLine.NoPatchLauncherVersionString(),
            nopatchLauncherVersionValue);
    } else if (g_LauncherCommandLine.ReplacementDefaultNoPatchPolicyActive()) {
        spdlog::info(
            "UNANCHORED: replacement default nopatch policy is using fallback packed launcher-version dword 0.1 (0x{:08x})",
            nopatchLauncherVersionValue);
    } else {
        spdlog::info(
            "DIAGNOSTIC: nopatch launcher-version dword is using fallback 0.1 (0x{:08x})",
            nopatchLauncherVersionValue);
    }
    if (g_LauncherCommandLine.NoPatchClientVersionString()[0]) {
        spdlog::info(
            "DIAGNOSTIC: explicit -nopatch rebuilt packed client-version dword from client.dll version info = '{}' (0x{:08x})",
            g_LauncherCommandLine.NoPatchClientVersionString(),
            nopatchClientVersionValue);
    } else if (g_LauncherCommandLine.ReplacementDefaultNoPatchPolicyActive()) {
        spdlog::info(
            "UNANCHORED: replacement default nopatch policy is using fallback packed client-version dword 0.1 (0x{:08x})",
            nopatchClientVersionValue);
    } else {
        spdlog::info(
            "DIAGNOSTIC: nopatch client-version dword is using fallback 0.1 (0x{:08x})",
            nopatchClientVersionValue);
    }

    return true;
}

// UNANCHORED: replacement-only synthesis that feeds the later 0x40a380 / 0x40b74d..0x40b7af
// corridor by materializing current arg6/arg7 startup state. This is more honest than treating the
// entire pre-client stretch as one faux method, but it still does not claim an exact original
// boundary.
bool CLauncher::MaterializeRecoveredInitClientStateFromSelectionName(const char* selectionName) {
    if (!selectionName) {
        return false;
    }
    if (!PreloadDependencies()) {
        spdlog::info("ERROR: preload failed");
        return false;
    }

    DiagnosticInstallMediatorViaBinderScaffold(&g_pILTLoginMediatorDefault);

    spdlog::info("=== configuring arg6 / mediator state for InitClientDLL ===");

    const RecoveredLauncherSelectionRecord* recoveredSelection =
        FindRecoveredLauncherSelectionRecord(selectionName);
    const uint32_t selectedSelectionGateByte100 = recoveredSelection ? recoveredSelection->selectionGateByte100 : 1u;
    const uint32_t selectedVariantState = recoveredSelection ? recoveredSelection->variantState : 0u;
    const uint32_t nopatchLauncherVersionValue = g_LauncherCommandLine.NoPatchLauncherVersionValue();
    const uint32_t nopatchClientVersionValue = g_LauncherCommandLine.NoPatchClientVersionValue();

    const uint32_t packedArg7Selection = BuildPackedArg7Selection();
    const uint32_t selectedHighByte = (packedArg7Selection >> 24) & 0xffu;
    const uint32_t selectionPackedLow24 = packedArg7Selection & 0x00ffffffu;
    const uint32_t worldUpperBoundExclusive = (selectionPackedLow24 < 0xffu) ? (selectionPackedLow24 + 1u) : 1u;
    const uint32_t variantUpperBoundExclusive = (selectedHighByte < 0xffu) ? (selectedHighByte + 1u) : 1u;
    DiagnosticConfigureMediatorSelection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        selectionName,
        selectionName,
        selectionPackedLow24,
        selectedHighByte,
        selectedSelectionGateByte100,
        selectedVariantState);
    DiagnosticApplyDefaultNopatchMediatorConfig(
        g_pILTLoginMediatorDefault,
        nopatchLauncherVersionValue,
        nopatchClientVersionValue);

    if (g_pILTLoginMediatorDefault) {
        uint32_t resolvedA8 = m_FieldA8;
        uint32_t resolvedAC = m_FieldAC;
        char resolvedSelectionName[sizeof(g_LastWorldName)] = {0};
        if (DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediatorDefault,
                m_FieldAC,
                m_FieldA8,
                &resolvedA8,
                &resolvedAC,
                resolvedSelectionName,
                sizeof(resolvedSelectionName))) {
            m_FieldA8 = resolvedA8;
            m_FieldAC = resolvedAC;
            const uint32_t packedArg7Selection = BuildPackedArg7Selection();
            if (resolvedSelectionName[0]) {
                std::strncpy(g_LastWorldName, resolvedSelectionName, sizeof(g_LastWorldName) - 1);
                g_LastWorldName[sizeof(g_LastWorldName) - 1] = '\0';
                StoreLastWorldNameInRegistry(g_LastWorldName);
            }
            spdlog::info(
                "DIAGNOSTIC: arg7 rebuilt through ILTLoginMediator.Default selection path -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} world='{}'",
                m_FieldA8,
                m_FieldAC,
                packedArg7Selection,
                g_LastWorldName[0] ? g_LastWorldName : selectionName);
        }
    }

    const char authServerDnsName[] = "auth.lith.thematrixonline.net";
    const uint16_t authServerPort = 11000;
    const char marginServerSuffix[] = ".lith.thematrixonline.net";
    const uint16_t marginServerPort = 10000;

    char marginRoutePrefix[256] = {0};
    if (recoveredSelection && recoveredSelection->routeHostPrefix && recoveredSelection->routeHostPrefix[0]) {
        std::strncpy(marginRoutePrefix, recoveredSelection->routeHostPrefix, sizeof(marginRoutePrefix) - 1);
        marginRoutePrefix[sizeof(marginRoutePrefix) - 1] = '\0';
        spdlog::info("DIAGNOSTIC: using recovered route host prefix '{}' for world '{}'", marginRoutePrefix, recoveredSelection->selectionName);
    } else {
        LowercaseAsciiCopy(marginRoutePrefix, sizeof(marginRoutePrefix), selectionName);
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

    return true;
}

// UNANCHORED: recovered continuation for the original 0x40b74d..0x40b790 corridor after the
// 0x40a380 success gate.
//
// Newer static RE tightens the original 0x402ec0 gate enough to keep one important ownership fact
// explicit in source:
// - `0x402ec0` is not a tiny setup helper; it starts `CLauncherThread`
//   (`runtimeclass 0x4aa134`, create object `0x403750`, InitInstance `0x407580`), then blocks
// - that worker thread allocates the separate `0xb30` launcher dialog/controller object at
//   `thread+0x48`; current best provisional class identity for it is `LauncherLoginDialog`
// - it calls `0x406470 = LauncherLoginDialog_CreateAndInitializeWindow` and marks dialog `+0x68`
//   when the UI is ready
// - `0x402ec0` waits for that dialog pointer + ready flag, then pumps messages until thread
//   `+0x45` shows the launcher dialog path has exited
// - on the successful selection path, the dialog's state-7 page setup (`0x4047d0`) registers the
//   selection observer `0x40f070`, list interaction reaches `0x40d820 -> 0x405a20` case `8`, and
//   case `8` first calls `0x40d6f0` to resolve the current list selection into
//   `CLauncher+0xa8/+0xac` and `Last_WorldName`
// - `0x405a20` case `8` only falls through to `DAT_004d259c = 1` when:
//   - `0x40d6f0` succeeds
//   - the optional patch gate either skips `0x40ac00` or `0x40ac00` returns `0`
//   - mediator slot `+0x138` does not divert into the launchpad-gated state18 branch
// - after that, case `8` calls `0x40b8f0`, posts quit, and thereby lets `0x402ec0` return
//   non-zero so `InitInstance` can fall through to the later client-load corridor
// - negative result kept explicit: this still does not make `0x40d6f0` or `0x405a20` the owner of
//   mediator `+0xec`; the direct `+0xec` call is still later on the client side
// The current source maps the 0x402ec0-style pre-client bringup and the optional 0x40b75a
// autodetect dialog path here, while leaving the later 0x40b790 _access / 0x41ab10 file gate
// explicitly unmodeled.
bool CLauncher::RunRecoveredPreClientBringupStage() const {
    if (!DiagnosticInitializePreclientEnvironmentLike402EC0()) {
        spdlog::info("WARNING: pre-client environment bringup failed to initialize");
    }

    DiagnosticStartWindowTrace();
    if (g_LauncherCommandLine.LauncherGlobal4D2C64()) {
        if (!RunAutodetectDialogWithoutGui()) {
            spdlog::error("ERROR: autodetect dialog/helper path failed");
            return false;
        }
    }
    return true;
}

// UNANCHORED: replacement-owned text-mode pre-client auth/selection bridge.
// Fidelity correction from newer launcher/client recovery:
// - auth should enter through the faithful page-6 submit boundary (`0x408400 -> +0x30 -> 0x41ecd0`),
//   not the older source-owned BeginAuthConnection shortcut
// - existing-character selection should stop at launcher-style `0x40d6f0` writeback into
//   `CLauncher+0xa8/+0xac` plus `Last_WorldName`, not seed the later create-character `+0x120`
//   source block preemptively
// - create-character remains a client-owned continuation after the launcher writes the sentinel
//   row high word `0xffff`
bool CLauncher::RunPreClientAuthAndCharacterSelectionStage() {
    g_PreClientAuthAndCharacterSelectionCompleted = false;
    if (!DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface()) {
        spdlog::error("ERROR: pre-client auth cannot begin because the faithful mediator +0x30 submit surface is unavailable");
        return false;
    }

    if (!PromptForMissingLauncherCredentialsIfNeeded()) {
        return false;
    }

    for (uint32_t attempt = 1u;; ++attempt) {
        mxo::ltlogin::ProcessLoginRequestInputSketch submitLoginRequestInput = {};
        if (!BuildNoGuiProcessLoginRequestInput(
                g_LauncherCommandLine.AuthUsername(),
                g_LauncherCommandLine.AuthPassword(),
                &submitLoginRequestInput)) {
            spdlog::error("ERROR: failed to build no-GUI ProcessLoginRequest input");
            return false;
        }

        DiagnosticResetPostedLoginResult();
        const uint32_t submitResult =
            DiagnosticSubmitLoginRequestViaResolvedMediatorSurface(submitLoginRequestInput);
        spdlog::info(
            "DIAGNOSTIC: pre-client launcher auth attempt={} submitResult=0x{:08x}",
            static_cast<unsigned>(attempt),
            static_cast<unsigned>(submitResult));

        // Fidelity correction:
        // - original launcher page-6 submit enters through `0x408400 -> +0x30 -> 0x41ecd0`
        // - `ProcessLoginRequest` itself already performs the happy-path `state0 -> state2`
        //   handoff, and state2/state1 own the later auth-connect/bootstrap continuation
        // - so the text-mode host should not issue an extra out-of-band auth begin after a
        //   successful `+0x30` submit; doing so is less faithful and perturbs retry behavior
        const DWORD startTick = GetTickCount();
        bool authSucceeded = false;
        // Replacement text-mode pre-client gate:
        // - faithful submit now enters through `0x408400 -> +0x30 -> 0x41ecd0`
        // - current auth reply adoption on the active path can land slightly later than the older
        //   source-owned BeginAuthConnection shortcut did, so keep the wait budget comfortably above
        //   the earlier 30s replacement timeout while the launcher still blocks pre-client.
        while ((GetTickCount() - startTick) < 60000u) {
            DiagnosticPumpLauncherNetwork(/*nonBlocking=*/true);
            if (DiagnosticHasSuccessfulPreClientAuthState()) {
                authSucceeded = true;
                break;
            }
            const uint32_t loginError = DiagnosticLastLoginError();
            if (loginError != 0u) {
                spdlog::info(
                    "DIAGNOSTIC: pre-client launcher auth attempt={} terminated with loginError=0x{:02x}",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(loginError));
                break;
            }
            Sleep(10u);
        }

        if (!authSucceeded) {
            const uint32_t loginError = DiagnosticLastLoginError();
            if (loginError == 0u) {
                spdlog::warn(
                    "WARNING: pre-client launcher auth timed out before success/error resolution; re-prompting credentials instead of falling through into client load");
                WriteMatrixConsoleText(
                    "Login timed out before the launcher received a success/error result. Please re-enter your username and password.",
                    true);
            } else {
                spdlog::warn(
                    "WARNING: pre-client launcher auth ended with error=0x{:02x}; re-prompting credentials instead of falling through into client load",
                    static_cast<unsigned>(loginError));
                WriteMatrixConsoleFormattedLine(
                    "Login failed (0x%08x). Please re-enter your username and password.",
                    static_cast<unsigned>(loginError));
            }

            // anchor: launcher.exe:0x4091d0 -> sibling +0x34 -> launcher.exe:0x41c0d0
            // Rich-edit observer failure path in the original launcher closes the current auth
            // connection and restores helper state0 before the user retries credentials.
            DiagnosticResetPostedLoginResult();
            DiagnosticRequestAuthCloseAndSwitchToState0();
            const DWORD authRetryResetStartTick = GetTickCount();
            while ((GetTickCount() - authRetryResetStartTick) < 5000u) {
                DiagnosticPumpLauncherNetwork(/*nonBlocking=*/true);
                if (DiagnosticLastLoginEvent() == 1u || DiagnosticIsAuthConnectionQuiescentForRetry()) {
                    break;
                }
                Sleep(10u);
            }

            char inputBuffer[256] = {};
            if (!ReadInteractiveLauncherField("Username: ", inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
                spdlog::error("ERROR: interactive username re-prompt failed or was left empty");
                return false;
            }
            g_LauncherCommandLine.SetAuthUsername(inputBuffer);
            if (!ReadInteractiveLauncherPasswordField("Password: ", inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
                spdlog::error("ERROR: interactive password re-prompt failed or was left empty");
                return false;
            }
            g_LauncherCommandLine.SetAuthPassword(inputBuffer);
            continue;
        }

character_selection_menu:
        WriteMatrixConsoleText("The Matrix has you...", true);

        const uint32_t recoveredCharacterCount = DiagnosticRecoveredCharacterCount();
        const bool createCharacterPlaceholderAvailable =
            (recoveredCharacterCount < kTextModeCreateCharacterSoftLimit);
        const bool deleteCharacterOptionAvailable = (recoveredCharacterCount != 0u);
        const uint32_t createCharacterMenuIndex = recoveredCharacterCount;
        const uint32_t deleteCharacterMenuIndex =
            recoveredCharacterCount + (createCharacterPlaceholderAvailable ? 1u : 0u);

        WriteMatrixConsoleText("Available characters:", true);
        for (uint32_t i = 0; i < recoveredCharacterCount; ++i) {
            char characterName[256] = {};
            const bool haveCharacterName =
                DiagnosticRecoveredCharacterName(i, characterName, sizeof(characterName));
            WriteMatrixConsoleFormattedLine(
                "  [%u] %s",
                static_cast<unsigned>(i + 1u),
                haveCharacterName ? characterName : "<unresolved>");
        }
        if (createCharacterPlaceholderAvailable) {
            WriteMatrixConsoleFormattedLine(
                "  [%u] - - - (Create Character)",
                static_cast<unsigned>(createCharacterMenuIndex + 1u));
        }
        if (deleteCharacterOptionAvailable) {
            WriteMatrixConsoleFormattedLine(
                "  [%u] Delete existing character",
                static_cast<unsigned>(deleteCharacterMenuIndex + 1u));
        }

        uint32_t selectedMenuIndex = 0u;
        bool selectedFromCommandLine = false;
        bool createCharacterPlaceholderSelected = false;
        bool deleteCharacterOptionSelected = false;
        if (g_LauncherCommandLine.LauncherCharacter()[0] != '\0') {
            for (uint32_t i = 0; i < recoveredCharacterCount; ++i) {
                char characterName[256] = {};
                if (DiagnosticRecoveredCharacterName(i, characterName, sizeof(characterName)) &&
                    std::strcmp(characterName, g_LauncherCommandLine.LauncherCharacter()) == 0) {
                    selectedMenuIndex = i;
                    selectedFromCommandLine = true;
                    break;
                }
            }
        }
        if (!selectedFromCommandLine) {
            const uint32_t menuCount =
                recoveredCharacterCount +
                (createCharacterPlaceholderAvailable ? 1u : 0u) +
                (deleteCharacterOptionAvailable ? 1u : 0u);
            if (menuCount > 1u) {
                while (true) {
                    uint32_t oneBasedSelection = 0u;
                    if (ReadInteractiveLauncherIndex("Character index: ", menuCount + 1u, &oneBasedSelection) &&
                        oneBasedSelection > 0u) {
                        selectedMenuIndex = oneBasedSelection - 1u;
                        break;
                    }
                    WriteMatrixConsoleText("Invalid character index.", true);
                }
            }
        }
        createCharacterPlaceholderSelected =
            createCharacterPlaceholderAvailable && (selectedMenuIndex == createCharacterMenuIndex);
        deleteCharacterOptionSelected =
            deleteCharacterOptionAvailable && (selectedMenuIndex == deleteCharacterMenuIndex);

        // anchor: launcher.exe:0x40ec70
        // Text-mode delete bridge over the original page-7 delete-character corridor:
        // - choose a concrete recovered slot-record index
        // - require explicit typed-name confirmation so we do not delete the wrong character
        // - then call the anchored mediator primitives `+0xf0` / wait event `8` / `+0xe8`
        if (deleteCharacterOptionSelected) {
            WriteMatrixConsoleText("Delete which character?", true);
            for (uint32_t i = 0; i < recoveredCharacterCount; ++i) {
                char characterName[256] = {};
                const bool haveCharacterName =
                    DiagnosticRecoveredCharacterName(i, characterName, sizeof(characterName));
                WriteMatrixConsoleFormattedLine(
                    "  [%u] %s",
                    static_cast<unsigned>(i + 1u),
                    haveCharacterName ? characterName : "<unresolved>");
            }

            uint32_t deleteOneBasedIndex = 0u;
            while (!ReadInteractiveLauncherIndex(
                "Delete character index: ",
                recoveredCharacterCount + 1u,
                &deleteOneBasedIndex) ||
                deleteOneBasedIndex == 0u) {
                WriteMatrixConsoleText("Invalid character index.", true);
            }
            const uint32_t deleteSlotIndex = deleteOneBasedIndex - 1u;

            char deleteCharacterName[256] = {};
            if (!DiagnosticRecoveredCharacterName(deleteSlotIndex, deleteCharacterName, sizeof(deleteCharacterName))) {
                WriteMatrixConsoleText("Failed to resolve character name for deletion.", true);
                goto character_selection_menu;
            }

            WriteMatrixConsoleFormattedLine(
                "Type '%s' to confirm deletion:",
                deleteCharacterName);
            char confirmBuffer[256] = {};
            if (!ReadInteractiveLauncherField("Confirm: ", confirmBuffer, sizeof(confirmBuffer)) ||
                std::strcmp(confirmBuffer, deleteCharacterName) != 0) {
                WriteMatrixConsoleText("Deletion cancelled.", true);
                goto character_selection_menu;
            }

            DiagnosticResetPostedLoginResult();
            (void)DiagnosticBeginDeleteRecoveredCharacter(deleteSlotIndex);

            // anchor: launcher.exe:0x40ec70
            // The original launcher calls sibling `+0xf0 = 0x41c390` and immediately enters the
            // event-8 wait; it does not gate the flow on the return value from `+0xf0`.
            const DWORD deleteStartTick = GetTickCount();
            bool deleteSucceeded = false;
            while ((GetTickCount() - deleteStartTick) < 30000u) {
                DiagnosticPumpLauncherNetwork(/*nonBlocking=*/true);
                if (DiagnosticLastLoginEvent() == 8u) {
                    deleteSucceeded = true;
                    break;
                }
                if (DiagnosticLastLoginError() != 0u) {
                    break;
                }
                Sleep(10u);
            }

            if (!deleteSucceeded) {
                WriteMatrixConsoleFormattedLine(
                    "Delete failed (error=0x%08x).",
                    static_cast<unsigned>(DiagnosticLastLoginError()));
                goto character_selection_menu;
            }

            char deleteProfileRootName[256] = {};
            const char* deleteProfileRootNameToUse = g_LauncherCommandLine.AuthUsername();
            if (DiagnosticGetDeleteCharacterProfileRootName(
                    deleteProfileRootName,
                    sizeof(deleteProfileRootName)) &&
                deleteProfileRootName[0] != '\0') {
                deleteProfileRootNameToUse = deleteProfileRootName;
            }

            (void)DeleteLauncherProfileDirectoryForCharacter(
                deleteProfileRootNameToUse,
                deleteCharacterName);

            (void)DiagnosticFinalizeDeleteRecoveredCharacter(deleteSlotIndex);

            WriteMatrixConsoleFormattedLine("Deleted character '%s'.", deleteCharacterName);
            goto character_selection_menu;
        }

        char persistedSelectionName[256] = {};
        if (LoadLastWorldNameFromRegistry(persistedSelectionName, sizeof(persistedSelectionName))) {
            spdlog::info(
                "DIAGNOSTIC: loaded HKLM Last_WorldName fallback='{}' on character-selection path",
                persistedSelectionName);
        }

        char selectedCharacterName[256] = {};
        char selectedSelectionName[256] = {};
        uint32_t selectedDescriptorIndex = m_FieldAC & 0x00ffffffu;
        uint32_t selectedRowHighWordSelectionIndex = 0xffffu;

        if (!createCharacterPlaceholderSelected) {
            if (!DiagnosticResolveRecoveredCharacterSelectionForLauncher(
                    selectedMenuIndex,
                    selectedCharacterName,
                    sizeof(selectedCharacterName),
                    selectedSelectionName,
                    sizeof(selectedSelectionName),
                    &selectedDescriptorIndex)) {
                spdlog::error(
                    "ERROR: failed to resolve recovered character-selection metadata index={} through mediator-owned slot/world tables",
                    static_cast<unsigned>(selectedMenuIndex));
                return false;
            }
            selectedRowHighWordSelectionIndex = selectedMenuIndex & 0xffffu;
            DiagnosticSetLauncherSelectedCharacterIndex(selectedMenuIndex);
            g_LauncherCommandLine.SetLauncherCharacter(selectedCharacterName);
        } else {
            if (g_LastWorldName[0] != '\0') {
                std::strncpy(selectedSelectionName, g_LastWorldName, sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            } else if (persistedSelectionName[0] != '\0') {
                std::strncpy(selectedSelectionName, persistedSelectionName, sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            } else if (sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]) > 0u) {
                std::strncpy(selectedSelectionName, kRecoveredLauncherSelectionRecords[0].selectionName, sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            }
            selectedCharacterName[0] = '\0';
            selectedRowHighWordSelectionIndex = 0xffffu;
            g_LauncherCommandLine.SetLauncherCharacter("");
            if (selectedSelectionName[0] != '\0') {
                uint32_t resolvedDescriptorIndex = 0u;
                if (DiagnosticFindRecoveredWorldDescriptorIndexByName(selectedSelectionName, &resolvedDescriptorIndex)) {
                    selectedDescriptorIndex = resolvedDescriptorIndex;
                }
            }
        }

        if (!selectedSelectionName[0] && persistedSelectionName[0]) {
            std::strncpy(selectedSelectionName, persistedSelectionName, sizeof(selectedSelectionName) - 1u);
            selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            spdlog::info(
                "DIAGNOSTIC: falling back to persisted Last_WorldName='{}' on character-selection path",
                selectedSelectionName);
        }

        if (!selectedSelectionName[0]) {
            spdlog::error("ERROR: no launcher world-selection name was available for page-7 style writeback");
            return false;
        }

        // anchor: launcher.exe:0x4047d0 / case 7
        // anchor: launcher.exe:0x40d530
        // anchor: launcher.exe:0x40d820
        // anchor: launcher.exe:0x405a20 / command 8
        // anchor: launcher.exe:0x40d6f0
        // No-GUI faithful bridge:
        // - existing-character selection stops at launcher-style command-8 writeback
        // - create-character uses the same writeback helper with row high word `0xffff`
        // - the later create-character arg6 `+0x120` submit stays on the client-owned continuation
        uint32_t resolvedA8 = m_FieldA8;
        uint32_t resolvedAC = m_FieldAC;
        char resolvedSelectionName[sizeof(g_LastWorldName)] = {};
        const bool resolvedViaCommand8 =
            g_pILTLoginMediatorDefault != nullptr &&
            DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediatorDefault,
                selectedDescriptorIndex & 0x00ffffffu,
                selectedRowHighWordSelectionIndex,
                &resolvedA8,
                &resolvedAC,
                resolvedSelectionName,
                sizeof(resolvedSelectionName));

        if (resolvedViaCommand8) {
            m_FieldA8 = resolvedA8;
            m_FieldAC = resolvedAC;
            const char* finalWorldName = resolvedSelectionName[0] ? resolvedSelectionName : selectedSelectionName;
            std::strncpy(g_LastWorldName, finalWorldName, sizeof(g_LastWorldName) - 1u);
            g_LastWorldName[sizeof(g_LastWorldName) - 1u] = '\0';
            StoreLastWorldNameInRegistry(g_LastWorldName);
        } else {
            m_FieldA8 = createCharacterPlaceholderSelected
                ? 0xffffffffu
                : (selectedMenuIndex & 0xffffu);
            m_FieldAC = selectedDescriptorIndex & 0x00ffffffu;
            std::strncpy(g_LastWorldName, selectedSelectionName, sizeof(g_LastWorldName) - 1u);
            g_LastWorldName[sizeof(g_LastWorldName) - 1u] = '\0';
            StoreLastWorldNameInRegistry(g_LastWorldName);
            spdlog::warn(
                "WARNING: pre-client no-GUI selection could not mirror page7 command8 / 0x40d6f0 via descriptorIndex={} selectionHighWord=0x{:04x}; using bounded launcher writeback fallback world='{}' fallbackA8=0x{:08x} fallbackAC=0x{:08x}",
                static_cast<unsigned>(selectedDescriptorIndex),
                static_cast<unsigned>(selectedRowHighWordSelectionIndex & 0xffffu),
                g_LastWorldName,
                m_FieldA8,
                m_FieldAC);
        }

        g_PreClientAuthAndCharacterSelectionCompleted = true;
        WriteMatrixConsoleText("Follow the white rabbit. Knock, Knock, Neo.", true);
        spdlog::info(
            "DIAGNOSTIC: pre-client launcher auth/selection complete event=0x{:02x} createPlaceholder={} recoveredCharacterCount={} selectedMenuIndex={} character='{}' world='{}' a8=0x{:08x} ac=0x{:08x}",
            static_cast<unsigned>(DiagnosticLastLoginEvent()),
            createCharacterPlaceholderSelected ? 1u : 0u,
            static_cast<unsigned>(recoveredCharacterCount),
            static_cast<unsigned>(selectedMenuIndex),
            selectedCharacterName[0] ? selectedCharacterName : "- - -",
            selectedSelectionName[0] ? selectedSelectionName : "<unresolved>",
            m_FieldA8,
            m_FieldAC);
        return true;
    }
}

// UNANCHORED: no-GUI wrapper for launcher.exe:0x40b75a -> 0x401520 -> DoModal -> 0x401640 -> 0x401590.
bool CLauncher::RunAutodetectDialogWithoutGui() const {
    spdlog::info("=== Running autodetect dialog path without GUI ===");
    CAutodetectDialog autodetectDialog;
    return autodetectDialog.RunWithoutGui();
}

// UNANCHORED: replacement-only summary logger for current gaps within launcher.exe:0x40b430.
void CLauncher::LogInitInstanceFaithfulnessGaps() const {
    spdlog::info("=== Original-path gaps still missing ===");
    spdlog::info("arg1/arg2 status: launcher-owned filtered argv storage now follows the original 0x409950 -> 0x4173d0 two-stage parse shape, but runtime console registry/config fidelity is still scaffold-level");
    spdlog::info("arg5 status: current launcher-owned 0x4d6304 ABI shell now mirrors the original create/register/pass and release/clear shape, but internal ctor state is still incomplete");
    spdlog::info("arg6 status: current binder-backed path materialized ILTLoginMediator.Default (not yet faithful launcher reconstruction)");
    spdlog::info("arg7 status: current selection writeback reuses ILTLoginMediator.Default and rebuilds a8/ac through +0xfc/+0x100/+0xe4");
    if (IsRecoveredPreclientEnvironmentActive()) {
        spdlog::info("pre-client env status: current 0x402ec0-style launcher thread/message scaffold active (not yet faithful original class/import path)");
    } else {
        spdlog::info("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
    }
    spdlog::info("login dialog status: page2->command11->page6 rich-edit credential corridor (0x408ee0/0x408840/0x408400/0x4091d0) is now documented and the no-GUI host now mirrors the recovered raw +0x30 submit surface into 0x41ecd0, but it still does not recreate the original rich-edit observer/prompt lifecycle");
    spdlog::info("autodetect status: 0x409f34 gate + 0x40b75a placement now modeled, but the current implementation intentionally skips real MFC dialog creation/controls and uses a no-GUI worker wrapper instead");
    spdlog::info("file/access gate status: original 0x40b790..0x40b7af _access(DAT_004d4cbc,0) / 0x41ab10(0) side path is still not modeled on the replacement path");
    spdlog::info("");
}

// anchor: launcher.exe:0x40a4d0
void CLauncher::CleanupRecoveredInitClientState() const {
    if (g_pLauncherObject6304) {
        LauncherReleaseNetworkEngineAbiShell(&g_pLauncherObject6304, g_pILTLoginMediatorDefault);
    }
    g_pILTLoginMediatorDefault = NULL;
    g_PreClientAuthAndCharacterSelectionCompleted = false;
}

// anchor: launcher.exe:0x40a4d0
bool CLauncher::RunClientDllLifecycle() const {
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

    const bool allowInitWithCurrentStartupState =
        g_pILTLoginMediatorDefault && g_pLauncherObject6304;
    if (!allowInitWithCurrentStartupState) {
        spdlog::error("Refusing to call InitClientDLL with incomplete launcher state.");
        return false;
    }

    LogKnownStartupState(*this);
    spdlog::info("");

    spdlog::info("=== Calling InitClientDLL with current launcher startup state ===");
    // anchor: launcher.exe:0x40a55c..0x40a5a4
    // - arg1/arg2 come from launcher globals `0x4d2c5c/0x4d2c60`, not from a helper object call
    // - arg8 is a byte global at `0x4d2c69` zero-extended through `EDX` before the push
    const int initResult = initClientDLL(
        g_LauncherFilteredArgCount,
        g_LauncherFilteredArgv,
        g_hClient,
        g_hCres,
        g_pLauncherObject6304,
        g_pILTLoginMediatorDefault,
        (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24),
        g_LauncherInitClientFlagByte);
    spdlog::info("InitClientDLL returned: {}", initResult);
    if (initResult <= 0) {
        LogClientLifecycleFailure("InitClientDLL", errorClientDLL);
        return false;
    }

    // Current bounded guard after the tighter `0x4490c0` auth tail work:
    // - when the no-GUI launcher path already completed pre-client auth + character selection, the
    //   client should enter with that recovered state intact instead of restarting auth from
    //   helper2 immediately after `InitClientDLL`
    // - keep the old post-init auth auto-begin only for the paths that still arrive without that
    //   earlier pre-client completion
    if (!g_PreClientAuthAndCharacterSelectionCompleted && DiagnosticCanBeginAuthConnection()) {
        const uint32_t authConnectResult = DiagnosticBeginAuthConnection();
        spdlog::info(
            "DIAGNOSTIC: post-init auth auto-begin result = 0x{:08x} preClientAuthComplete={}",
            static_cast<unsigned>(authConnectResult),
            g_PreClientAuthAndCharacterSelectionCompleted ? 1u : 0u);
    }

    spdlog::info("=== Calling RunClientDLL on the active launcher path ===");
    const int runResult = runClientDLL();
    spdlog::info("RunClientDLL returned: {}", runResult);
    if (runResult <= 0) {
        LogClientLifecycleFailure("RunClientDLL", errorClientDLL);
        return false;
    }

    spdlog::info("=== Calling TermClientDLL on the active launcher path ===");
    const int termResult = termClientDLL();
    spdlog::info("TermClientDLL returned: {}", termResult);
    if (termResult <= 0) {
        spdlog::error("TermClientDLL failed.");
        return false;
    }

    return true;
}

// anchor: launcher.exe:0x40b430
bool CLauncher::InitInstance() {
    bool operationSucceeded = false;
    char selectionName[64] = {};

    spdlog::info("NOTE: arg1/arg2 now follow the original ParseCommandLine -> CConsoleVar_ParseCommandLineAndConfig staging, but runtime console-variable registration/config-file fidelity is still scaffolded.");
    spdlog::info("NOTE: this replacement intentionally supports only the effective nopatch branch; patch/update support remains out of scope even while startup behavior is kept close to launcher.exe.");
    spdlog::info("NOTE: launcher-owned arg5 now enters through a dedicated 0x4d6304 ABI shell, but arg5/arg6/arg7/arg8 fidelity is still incomplete.");
    if (!ParseCommandLineStage()) {
        goto cleanup;
    }
    spdlog::info("");

    spdlog::info("DIAGNOSTIC: active launcher runtime path = binder-backed mediator + launcher-owned arg5 ABI shell + InitClientDLL/RunClientDLL + faithful launcher-style auth submit (+0x30)");

    // UNANCHORED within 0x40b430: replacement-only synthesis that seeds launcher-owned selection
    // and nopatch state before the later pre-client continuation corridor.
    if (!BuildRecoveredSelectionNameAndState(selectionName, sizeof(selectionName))) {
        goto cleanup;
    }

    // UNANCHORED within 0x40b430: replacement-only arg6/arg7 startup-state materialization that
    // feeds the later anchored 0x40a380 / 0x40a4d0 call shape.
    if (!MaterializeRecoveredInitClientStateFromSelectionName(selectionName)) {
        goto cleanup;
    }

    // anchor: launcher.exe:0x40b740 -> 0x40a380
    if (!InitializeThreadPerClientTCPEngine()) {
        goto cleanup;
    }

    // Original corridor in 0x40b430:
    // - 0x40b739: `AfxInitRichEdit()`
    // - 0x40b740: `Launcher_InitializeThreadPerClientTCPEngine()`
    // - 0x40b74d..0x40b752: `0x402ec0` pre-client UI-thread/message gate
    //   - starts `CLauncherThread`, waits for `thread+0x48` provisional `LauncherLoginDialog`
    //     object + dialog `+0x68` ready, then
    //     pumps messages until thread `+0x45` shows the launcher dialog path has exited
    //   - state-7 page setup (`0x4047d0`) on that dialog registers observer `0x40f070`, while list
    //     interaction reaches `0x40d820 -> 0x405a20` case `8`
    //   - case `8` success is now tighter:
    //     `0x40d6f0` resolves selection -> optional patch gate / `0x40ac00` -> mediator `+0x138`
    //     must not divert -> `DAT_004d259c = 1` -> `0x40b8f0` -> quit
    //   - replacement consequence: launcher auth/selection should complete before the later
    //     `LoadCresDLL` / `LoadClientDLL` fallthrough too
    // - 0x40b75a..0x40b790: optional autodetect dialog path when 0x4d2c64 is set
    // - 0x40b790..0x40b7af: file/access gate remains explicitly unmodeled here
    if (!RunRecoveredPreClientBringupStage()) {
        goto cleanup;
    }

    LogInitInstanceFaithfulnessGaps();

    if (!RunPreClientAuthAndCharacterSelectionStage()) {
        goto cleanup;
    }
    if (!LoadCresDLL()) {
        spdlog::info("ERROR: failed to load cres.dll");
        goto cleanup;
    }
    if (!LoadClientDLL()) {
        spdlog::info("ERROR: failed to load client.dll");
        goto cleanup;
    }

    operationSucceeded = RunClientDllLifecycle();

cleanup:
    // Original tail ownership inside `launcher.exe:0x40b430` stays explicit here even though the
    // current replacement routes all exits through one cleanup label so replacement-owned startup
    // scaffolds do not leak across earlier failures:
    // - 0x40a760: `CLauncher_UnloadClientDLL`
    // - 0x40a7a0: `CLauncher_UnloadCresDLL`
    // - 0x40b360: `Launcher_TeardownThreadPerClientEngineAndMediator`
    // - 0x40a000: `Launcher_FreeFilteredCommandLineStorage`
    UnloadClientDLL();
    UnloadCresDLL();
    CleanupRecoveredInitClientState();
    g_LauncherCommandLine.Reset();
    g_LauncherFilteredArgCount = 0;
    g_LauncherFilteredArgv = NULL;
    g_LauncherInitClientFlagByte = 0;
    return operationSucceeded;
}

} // namespace launcher
} // namespace mxo
