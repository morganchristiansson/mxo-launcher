#include "launcher.h"

#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "../../../../src/diagnostics.h"
#include "../../../runtime/src/libltbase/launchercommandline.h"

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

extern HMODULE g_hCres;
extern HMODULE g_hClient;
extern int g_CrtArgc;
extern char** g_CrtArgv;
extern mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;
extern void* g_pLauncherObject6304;
extern void* g_pILTLoginMediatorDefault;
extern void* g_pILTLoginMediatorSelection3584;
extern uint32_t g_PackedArg7Selection;
extern uint32_t g_FlagByte;
extern char g_LastWorldName[256];

extern bool PatchClientDllMxowrapImportToDbghelp();
extern bool PreloadDependencies();
extern bool DiagnosticInitializePreclientEnvironmentLike402EC0();
extern void RunOptionsCfgAutodetectStepIfNeeded();
extern bool IsRecoveredPreclientEnvironmentActive();

namespace {

const char* kLauncherRegistryKeyPath = "Software\\Monolith Productions\\The Matrix Online\\";

struct RecoveredLauncherSelectionRecord {
    const char* worldName;
    const char* routeHostPrefix;
    uint32_t worldIndexLow24;
    uint32_t variantIndexHigh8;
    uint32_t selectionGateByte100;
    uint32_t variantState;
};

const RecoveredLauncherSelectionRecord kRecoveredLauncherSelectionRecords[] = {
    {"Reality", "reality", 0x00002au, 0x05u, 1u, 0u},
};

template <typename T>
T ResolveProc(HMODULE module, const char* name) {
    FARPROC proc = GetProcAddress(module, name);
    T typed = nullptr;
    static_assert(sizeof(typed) == sizeof(proc), "function pointer size mismatch");
    std::memcpy(&typed, &proc, sizeof(typed));
    return typed;
}

const RecoveredLauncherSelectionRecord* AsRecoveredSelection(const void* opaqueSelection) {
    return static_cast<const RecoveredLauncherSelectionRecord*>(opaqueSelection);
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

void LogLauncherPreprocessingState() {
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
        spdlog::info("launcher autodetect exitCode = {}", (unsigned long)g_LauncherCommandLine.AutodetectExitCode());
    }
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

bool StoreLastWorldNameInRegistry(const char* worldName) {
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

const RecoveredLauncherSelectionRecord* FindRecoveredLauncherSelectionRecord(const char* worldName) {
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

void LogArgvContentsAsBytes(const char* label, char** argv, uint32_t count) {
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

void LogKnownStartupState(const mxo::launcher::CLauncher& launcher) {
    spdlog::info("=== Known startup frame ===");
    spdlog::info("arg1 filteredArgCount        = 0x{:08x}", g_LauncherCommandLine.FilteredArgCount());
    spdlog::info("arg2 filteredArgv            = {}", fmt::ptr(g_LauncherCommandLine.FilteredArgv()));
    if (g_LauncherCommandLine.FilteredArgv()) {
        LogArgvContentsAsBytes("arg2", g_LauncherCommandLine.FilteredArgv(), g_LauncherCommandLine.FilteredArgCount());
    }
    spdlog::info("arg3 hClientDll              = {}", fmt::ptr(g_hClient));
    spdlog::info("arg4 hCresDll                = {}", fmt::ptr(g_hCres));
    spdlog::info("arg5 launcherNetworkObject   = {}", fmt::ptr(g_pLauncherObject6304));
    spdlog::info("arg6 ILTLoginMediatorDefault = {}", fmt::ptr(g_pILTLoginMediatorDefault));
    spdlog::info("CLauncher+0xa8 placeholder   = 0x{:08x}", launcher.m_FieldA8);
    spdlog::info("CLauncher+0xac placeholder   = 0x{:08x}", launcher.m_FieldAC);
    spdlog::info("Last_WorldName               = {}", g_LastWorldName[0] ? g_LastWorldName : "<unavailable>");
    spdlog::info("arg7 packedArg7Selection     = 0x{:08x}", g_PackedArg7Selection);
    spdlog::info("arg8 flagByte                = 0x{:08x}", g_FlagByte);
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

uint32_t CLauncher::BuildPackedArg7Selection() const {
    return (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24);
}

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
    spdlog::info("DIAGNOSTIC: filtered argv final count = {}", g_LauncherCommandLine.FilteredArgCount());
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

bool CLauncher::BuildStartupContextFromRecoveredSelection(RecoveredLauncherStartupContext* startupContext) {
    if (!startupContext) {
        return false;
    }

    std::memset(startupContext, 0, sizeof(*startupContext));
    LoadLastWorldNameFromRegistry(g_LastWorldName, sizeof(g_LastWorldName));

    if (g_LastWorldName[0]) {
        lstrcpynA(startupContext->mediatorSelectionName, g_LastWorldName, sizeof(startupContext->mediatorSelectionName));
        spdlog::info("DIAGNOSTIC: using persisted Last_WorldName as launcher selection name = '{}'", startupContext->mediatorSelectionName);
    } else if (sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]) > 0) {
        std::strncpy(startupContext->mediatorSelectionName, kRecoveredLauncherSelectionRecords[0].worldName, sizeof(startupContext->mediatorSelectionName) - 1);
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

    startupContext->recoveredSelection = FindRecoveredLauncherSelectionRecord(startupContext->mediatorSelectionName);
    const RecoveredLauncherSelectionRecord* recoveredSelection = AsRecoveredSelection(startupContext->recoveredSelection);
    if (recoveredSelection) {
        std::strncpy(startupContext->mediatorSelectionName, recoveredSelection->worldName, sizeof(startupContext->mediatorSelectionName) - 1);
        startupContext->mediatorSelectionName[sizeof(startupContext->mediatorSelectionName) - 1] = '\0';
        m_FieldA8 = recoveredSelection->variantIndexHigh8;
        m_FieldAC = recoveredSelection->worldIndexLow24;
        spdlog::info(
            "DIAGNOSTIC: seeded launcher selection defaults from recovered world '{}' -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} selectionGateByte100={} variantState={} routePrefix='{}'",
            recoveredSelection->worldName,
            m_FieldA8,
            m_FieldAC,
            BuildPackedArg7Selection(),
            (unsigned)recoveredSelection->selectionGateByte100,
            (unsigned)recoveredSelection->variantState,
            recoveredSelection->routeHostPrefix ? recoveredSelection->routeHostPrefix : "");
    } else if (startupContext->mediatorSelectionName[0] && lstrcmpiA(startupContext->mediatorSelectionName, "standalone") != 0) {
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection defaults for world '{}'; keeping zeroed launcher arg7 fields until that world has a recovered launcher-owned selection record",
            startupContext->mediatorSelectionName);
    }

    g_PackedArg7Selection = BuildPackedArg7Selection();
    if ((m_FieldA8 | m_FieldAC) != 0) {
        spdlog::info("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x{:08x}", g_PackedArg7Selection);
    }

    startupContext->mediatorSelectedSelectionGateByte100 = recoveredSelection ? recoveredSelection->selectionGateByte100 : 1u;
    startupContext->mediatorSelectedVariantState = recoveredSelection ? recoveredSelection->variantState : 0u;
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

bool CLauncher::PrepareInitClientStateFromStartupContext(const RecoveredLauncherStartupContext& startupContext) {
    if (!PreloadDependencies()) {
        spdlog::info("ERROR: preload failed");
        return false;
    }

    DiagnosticInstallMediatorViaBinderScaffold(&g_pILTLoginMediatorDefault);

    spdlog::info("=== configuring arg6 / sibling mediator state for InitClientDLL ===");

    const uint32_t selectedHighByte = (g_PackedArg7Selection >> 24) & 0xffu;
    const uint32_t selectionPackedLow24 = g_PackedArg7Selection & 0x00ffffffu;
    const uint32_t worldUpperBoundExclusive = (selectionPackedLow24 < 0xffu) ? (selectionPackedLow24 + 1u) : 1u;
    const uint32_t variantUpperBoundExclusive = (selectedHighByte < 0xffu) ? (selectedHighByte + 1u) : 1u;
    DiagnosticConfigureMediatorSelection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        startupContext.mediatorSelectionName,
        startupContext.mediatorSelectionName,
        selectionPackedLow24,
        selectedHighByte,
        startupContext.mediatorSelectedSelectionGateByte100,
        startupContext.mediatorSelectedVariantState);
    DiagnosticConfigureMediatorProfileName(g_LauncherCommandLine.AuthUsername()[0] ? g_LauncherCommandLine.AuthUsername() : NULL);
    DiagnosticConfigureMediatorAuthName(g_LauncherCommandLine.AuthUsername()[0] ? g_LauncherCommandLine.AuthUsername() : NULL);
    DiagnosticConfigureMediatorAuthPassword(g_LauncherCommandLine.AuthPassword()[0] ? g_LauncherCommandLine.AuthPassword() : NULL);
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
    const RecoveredLauncherSelectionRecord* recoveredSelection = AsRecoveredSelection(startupContext.recoveredSelection);
    if (recoveredSelection && recoveredSelection->routeHostPrefix && recoveredSelection->routeHostPrefix[0]) {
        std::strncpy(marginRoutePrefix, recoveredSelection->routeHostPrefix, sizeof(marginRoutePrefix) - 1);
        marginRoutePrefix[sizeof(marginRoutePrefix) - 1] = '\0';
        spdlog::info("DIAGNOSTIC: using recovered route host prefix '{}' for world '{}'", marginRoutePrefix, recoveredSelection->worldName);
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

void CLauncher::LogInitInstanceFaithfulnessGaps() const {
    spdlog::info("=== Original-path gaps still missing ===");
    spdlog::info("arg1/arg2 status: launcher-owned filtered argv storage now follows the original 0x409950 -> 0x4173d0 two-stage parse shape, but runtime console registry/config fidelity is still scaffold-level");
    spdlog::info("arg5 status: current launcher object scaffold materialized 0x4d6304-style object (not yet faithful ctor/internal state)");
    spdlog::info("arg6 status: current binder-backed path materialized ILTLoginMediator.Default (not yet faithful launcher reconstruction)");
    if (g_pILTLoginMediatorSelection3584) {
        spdlog::info("arg7 status: sibling 0x4d3584-style ILTLoginMediator selection slot currently reuses the active mediator object and rebuilds a8/ac through +0xfc/+0x100/+0xe4");
    } else {
        spdlog::info("missing: reconstruct sibling ILTLoginMediator.Default-style slot at 0x4d3584 for launcher-owned arg7 selection resolution");
    }
    if (IsRecoveredPreclientEnvironmentActive()) {
        spdlog::info("pre-client env status: current 0x402ec0-style launcher thread/message scaffold active (not yet faithful original class/import path)");
    } else {
        spdlog::info("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
    }
    spdlog::info("");
}

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

    const bool allowInitWithCurrentStartupScaffold =
        g_pILTLoginMediatorDefault && g_pLauncherObject6304 && g_pILTLoginMediatorSelection3584;
    if (!allowInitWithCurrentStartupScaffold) {
        spdlog::error("Refusing to call InitClientDLL with incomplete launcher state.");
        return false;
    }

    LogKnownStartupState(*this);
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

    spdlog::info("=== Calling TermClientDLL on the active launcher path ===");
    const int termResult = termClientDLL();
    spdlog::info("TermClientDLL returned: {}", termResult);
    if (termResult <= 0) {
        spdlog::error("TermClientDLL failed.");
        return false;
    }

    return true;
}

bool CLauncher::InitInstance() {
    spdlog::info("NOTE: arg1/arg2 now follow the original ParseCommandLine -> CConsoleVar_ParseCommandLineAndConfig staging, but runtime console-variable registration/config-file fidelity is still scaffolded.");
    spdlog::info("NOTE: launcher-owned nopatch setup, arg5, arg6, arg7, and arg8 remain incomplete.");
    if (!ParseCommandLineStage()) {
        return false;
    }
    spdlog::info("");

    spdlog::info("DIAGNOSTIC: active launcher runtime path = binder-backed mediator + launcher object scaffold + InitClientDLL/RunClientDLL + launcher-owned auth begin");

    RecoveredLauncherStartupContext startupContext = {};
    if (!BuildStartupContextFromRecoveredSelection(&startupContext)) {
        return false;
    }
    if (!PrepareInitClientStateFromStartupContext(startupContext)) {
        return false;
    }
    LogInitInstanceFaithfulnessGaps();

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

} // namespace launcher
} // namespace mxo
