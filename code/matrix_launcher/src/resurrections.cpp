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
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdlib>

#include "diagnostics.h"

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

using RunClientDLLFunc = void (*)();
using TermClientDLLFunc = void (*)();
using ErrorClientDLLFunc = void (*)();

static FILE* g_LogFile = NULL;
static HMODULE g_hCres = NULL;
static HMODULE g_hClient = NULL;
static InitClientDLLFunc g_InitClientDLL = NULL;
static RunClientDLLFunc g_RunClientDLL = NULL;
static TermClientDLLFunc g_TermClientDLL = NULL;
static ErrorClientDLLFunc g_ErrorClientDLL = NULL;

// Launcher-owned runtime values mirrored from the original startup path.
// arg1/arg2 are still placeholder process argc/argv until the launcher-owned
// filtered storage path is reconstructed.
static uint32_t g_FilteredArgCount = 0;
static char** g_FilteredArgv = NULL;
static char* g_FilteredArgvOwned[16] = {0};
static char g_FilteredArgStorage[16][512] = {};
static uint32_t g_FilteredArgvOwnedCapacity = 0;
static const char* g_AuthUsername = NULL;
static void* g_pLauncherObject6304 = NULL;       // original: [0x4d6304]
static void* g_pILTLoginMediatorDefault = NULL;  // original: [0x4d2c58]
static uint32_t g_CLauncherFieldA8 = 0;          // original: [CLauncher+0xa8], high 8 bits used
static uint32_t g_CLauncherFieldAC = 0;          // original: [CLauncher+0xac], low 24 bits used
static uint32_t g_PackedArg7Selection = 0;       // packed from [this+0xa8]/[this+0xac]
static uint32_t g_FlagByte = 0;                  // original: [0x4d2c69]
static char g_LastWorldName[256] = {0};         // original registry value: Last_WorldName
static void* g_pClientDBFromCallback = NULL;

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase);
void Log(const char* fmt, ...);

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

static bool EnvFlagEnabled(const char* name) {
    char value[8] = {0};
    DWORD len = GetEnvironmentVariableA(name, value, sizeof(value));
    return len > 0;
}

static bool EnvStringValue(const char* name, char* out, DWORD outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    DWORD len = GetEnvironmentVariableA(name, out, outSize);
    return len > 0 && len < outSize;
}

static bool EnvUint32Value(const char* name, uint32_t* outValue) {
    char buffer[64] = {0};
    if (!outValue || !EnvStringValue(name, buffer, sizeof(buffer))) return false;

    char* end = NULL;
    unsigned long parsed = std::strtoul(buffer, &end, 0);
    if (end == buffer) return false;
    *outValue = static_cast<uint32_t>(parsed);
    return true;
}

static const char* MaskedArgValue(const char* value) {
    if (!value || !value[0]) return "<empty>";
    return "<provided>";
}

static void FreeFilteredArgvOwned() {
    for (uint32_t i = 0; i < g_FilteredArgvOwnedCapacity && i < 16; ++i) {
        g_FilteredArgvOwned[i] = NULL;
        g_FilteredArgStorage[i][0] = '\0';
    }
    g_FilteredArgvOwnedCapacity = 0;
}

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
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
        "Software\\Monolith Productions\\The Matrix Online\\",
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

static uint32_t BuildPackedArg7Selection() {
    return (g_CLauncherFieldAC & 0x00ffffffu) | ((g_CLauncherFieldA8 & 0xffu) << 24);
}

static void LogKnownStartupState() {
    Log("=== Known startup frame ===");
    Log("arg1 filteredArgCount         = 0x%08x", g_FilteredArgCount);
    Log("arg2 filteredArgv            = %p", g_FilteredArgv);
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
    const bool hasLauncherAuthArgs = (argc >= 3 && argv[1] && argv[1][0] && argv[2] && argv[2][0]);

    g_AuthUsername = (argc >= 2 && argv[1] && argv[1][0]) ? argv[1] : NULL;

    Log("=== Placeholder auth argv ===");
    Log("username argv[1] = %s", MaskedArgValue((argc >= 2) ? argv[1] : NULL));
    Log("password argv[2] = %s", MaskedArgValue((argc >= 3) ? argv[2] : NULL));

    const int filteredCount = hasLauncherAuthArgs ? (1 + ((argc > 3) ? (argc - 3) : 0)) : argc;
    if (filteredCount < 0 || filteredCount > 15) {
        Log("ERROR: filtered argv count %d exceeds static launcher storage", filteredCount);
        return false;
    }

    FreeFilteredArgvOwned();
    g_FilteredArgvOwnedCapacity = 16;

    for (int dst = 0; dst < filteredCount; ++dst) {
        const int src = hasLauncherAuthArgs ? ((dst == 0) ? 0 : (dst + 2)) : dst;
        const char* value = (src < argc && argv[src]) ? argv[src] : "";
        std::strncpy(g_FilteredArgStorage[dst], value, sizeof(g_FilteredArgStorage[dst]) - 1);
        g_FilteredArgStorage[dst][sizeof(g_FilteredArgStorage[dst]) - 1] = '\0';
        g_FilteredArgvOwned[dst] = g_FilteredArgStorage[dst];
        Log("DIAGNOSTIC: filtered argv[%d] stored at %p -> '%s'", dst, g_FilteredArgvOwned[dst], g_FilteredArgvOwned[dst]);
    }

    g_FilteredArgvOwned[filteredCount] = NULL;
    g_FilteredArgCount = static_cast<uint32_t>(filteredCount);
    g_FilteredArgv = g_FilteredArgvOwned;

    if (hasLauncherAuthArgs) {
        Log("launcher-only auth args detected; stripped 2 argv entries before InitClientDLL");
        if (argc > 3) {
            Log("forwarded extra launcher argv count = %d", argc - 3);
        }
    } else {
        Log("DIAGNOSTIC: duplicated raw argv into launcher-owned filtered storage");
    }
    return true;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("resurrections.log", "w");

    Log("Matrix Online launcher reimplementation scaffold");
    Log("===============================================");
    Log("Mode: original startup order, no client-memory injection");
    Log("Default branch target: nopatch path");
    Log("");

    Log("NOTE: arg1/arg2 still reuse process argc/argv as placeholders.");
    Log("NOTE: launcher-owned nopatch setup, filtered argv storage, arg5, arg6, arg7, and arg8 remain incomplete.");
    if (!ConfigureFilteredArgv(argc, argv)) {
        return FinishAndReturn(1);
    }
    Log("");

    const bool forceIncompleteInit = EnvFlagEnabled("MXO_FORCE_INCOMPLETE_INIT");
    const bool forceRunClient = EnvFlagEnabled("MXO_FORCE_RUNCLIENT");
    const bool forceRunAfterInitFailure = EnvFlagEnabled("MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE");
    const bool useMediatorStub = EnvFlagEnabled("MXO_STUB_LOGIN_MEDIATOR");
    const bool useMediatorBinderScaffold = EnvFlagEnabled("MXO_BINDER_LOGIN_MEDIATOR");
    const bool useLauncherObjectStub = EnvFlagEnabled("MXO_STUB_LAUNCHER_OBJECT");
    const bool traceWindows = EnvFlagEnabled("MXO_TRACE_WINDOWS");

    LoadLastWorldNameFromRegistry(g_LastWorldName, sizeof(g_LastWorldName));

    char mediatorSelectionName[64] = {0};
    if (EnvStringValue("MXO_MEDIATOR_SELECTION_NAME", mediatorSelectionName, sizeof(mediatorSelectionName))) {
        Log("DIAGNOSTIC: mediator selection name overridden from env = '%s'", mediatorSelectionName);
    } else if (g_LastWorldName[0]) {
        std::strncpy(mediatorSelectionName, g_LastWorldName, sizeof(mediatorSelectionName) - 1);
        mediatorSelectionName[sizeof(mediatorSelectionName) - 1] = '\0';
        Log("DIAGNOSTIC: using Last_WorldName as mediator selection name = '%s'", mediatorSelectionName);
    } else {
        std::strcpy(mediatorSelectionName, "standalone");
    }

    uint32_t packedArg7Override = 0;
    if (EnvUint32Value("MXO_ARG7_SELECTION", &packedArg7Override)) {
        g_CLauncherFieldA8 = (packedArg7Override >> 24) & 0xffu;
        g_CLauncherFieldAC = packedArg7Override & 0x00ffffffu;
        Log(
            "DIAGNOSTIC: overridden arg7 packed selection from env = 0x%08x -> a8=0x%08x ac=0x%08x",
            packedArg7Override,
            g_CLauncherFieldA8,
            g_CLauncherFieldAC);
    }
    if (EnvUint32Value("MXO_CLAUNCHER_A8", &g_CLauncherFieldA8)) {
        Log("DIAGNOSTIC: overridden CLauncher+0xa8 from env = 0x%08x", g_CLauncherFieldA8);
    }
    if (EnvUint32Value("MXO_CLAUNCHER_AC", &g_CLauncherFieldAC)) {
        Log("DIAGNOSTIC: overridden CLauncher+0xac from env = 0x%08x", g_CLauncherFieldAC);
    }
    g_PackedArg7Selection = BuildPackedArg7Selection();
    if ((g_CLauncherFieldA8 | g_CLauncherFieldAC) != 0) {
        Log("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x%08x", g_PackedArg7Selection);
    }
    if (EnvUint32Value("MXO_ARG8_FLAG", &g_FlagByte)) {
        Log("DIAGNOSTIC: overridden arg8 flag from env = 0x%08x", g_FlagByte);
    }

    if (!PreloadDependencies()) {
        Log("ERROR: preload failed");
        return FinishAndReturn(1);
    }

    if (useMediatorBinderScaffold && useMediatorStub) {
        Log("WARNING: both MXO_BINDER_LOGIN_MEDIATOR and MXO_STUB_LOGIN_MEDIATOR set; binder scaffold wins.");
    }

    if (useMediatorBinderScaffold || useMediatorStub) {
        DiagnosticConfigureMediatorSelection(g_PackedArg7Selection >> 24, mediatorSelectionName);
        DiagnosticConfigureMediatorProfileName(g_AuthUsername);
    }

    if (useMediatorBinderScaffold) {
        DiagnosticInstallMediatorViaBinderScaffold(&g_pILTLoginMediatorDefault);
        DiagnosticApplyDefaultNopatchMediatorConfig(g_pILTLoginMediatorDefault);
    } else if (useMediatorStub) {
        DiagnosticInstallMediatorStub(&g_pILTLoginMediatorDefault);
        DiagnosticApplyDefaultNopatchMediatorConfig(g_pILTLoginMediatorDefault);
    }

    if (useLauncherObjectStub) {
        DiagnosticInstallLauncherObjectStub(&g_pLauncherObject6304, g_pILTLoginMediatorDefault);
    }

    if (traceWindows) {
        DiagnosticStartWindowTrace();
    }

    // Original order from launcher.exe:
    //   0x40a380  -> build 0x4d6304
    //   0x402ec0  -> environment setup
    //   0x40a780  -> LoadLibraryA("cres.dll")
    //   0x40a420  -> LoadLibraryA("client.dll")
    //   0x40a4d0  -> resolve exports + Init/Run/Term/Error path

    Log("=== Original-path gaps still missing ===");
    Log("missing: launcher-owned filtered argv storage / nopatch state setup");
    if (useLauncherObjectStub) {
        Log("arg5 status: diagnostic launcher object scaffold materialized 0x4d6304-style object (not yet faithful ctor/internal state)");
    } else {
        Log("missing: build/register launcher object at 0x4d6304");
    }
    if (useMediatorBinderScaffold) {
        Log("arg6 status: diagnostic binder scaffold materialized ILTLoginMediator.Default (not yet faithful launcher reconstruction)");
    } else if (useMediatorStub) {
        Log("arg6 status: direct diagnostic stub materialized ILTLoginMediator.Default (bypasses binder path)");
    } else {
        Log("missing: resolve ILTLoginMediator.Default into 0x4d2c58");
    }
    Log("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
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

    if (!forceIncompleteInit) {
        Log("Refusing to call InitClientDLL with knowingly incomplete launcher state.");
        Log("Set MXO_FORCE_INCOMPLETE_INIT=1 to run a deliberate original-path experiment.");
        return FinishAndReturn(2);
    }

    Log("=== Forced experiment: calling InitClientDLL with incomplete original-path state ===");
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

    if (initResult != 0) {
        Log("InitClientDLL failed.");
        if (!forceRunAfterInitFailure) {
            Log("Stopping. Set MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE=1 for a diagnostic-only crash experiment.");
            return FinishAndReturn(1);
        }
        Log("DIAGNOSTIC OVERRIDE: continuing to RunClientDLL despite InitClientDLL failure.");
    }

    if (!forceRunClient) {
        Log("InitClientDLL succeeded, but RunClientDLL is gated.");
        Log("Set MXO_FORCE_RUNCLIENT=1 for a deliberate runtime experiment.");
        return FinishAndReturn(0);
    }

    Log("=== Forced experiment: calling RunClientDLL ===");
    g_RunClientDLL();
    Log("RunClientDLL returned");

    return FinishAndReturn(0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        default:
            break;
    }
    return TRUE;
}
