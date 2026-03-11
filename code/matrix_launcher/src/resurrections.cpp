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
static char** g_FilteredArgvOwned = NULL;
static void* g_pLauncherObject6304 = NULL;       // original: [0x4d6304]
static void* g_pILTLoginMediatorDefault = NULL;  // original: [0x4d2c58]
static uint32_t g_PackedArg7Selection = 0;       // original: [this+0xa8]/[this+0xac]
static uint32_t g_FlagByte = 0;                  // original: [0x4d2c69]
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

static int FinishAndReturn(int code) {
    DiagnosticStopWindowTrace();
    if (g_FilteredArgvOwned) {
        std::free(g_FilteredArgvOwned);
        g_FilteredArgvOwned = NULL;
    }
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

static void LogKnownStartupState() {
    Log("=== Known startup frame ===");
    Log("arg1 filteredArgCount         = 0x%08x", g_FilteredArgCount);
    Log("arg2 filteredArgv            = %p", g_FilteredArgv);
    Log("arg3 hClientDll              = %p", g_hClient);
    Log("arg4 hCresDll                = %p", g_hCres);
    Log("arg5 launcherNetworkObject   = %p", g_pLauncherObject6304);
    Log("arg6 ILTLoginMediatorDefault = %p", g_pILTLoginMediatorDefault);
    Log("arg7 packedArg7Selection    = 0x%08x", g_PackedArg7Selection);
    Log("arg8 flagByte                = 0x%08x", g_FlagByte);
}

static bool ConfigureFilteredArgv(int argc, char* argv[]) {
    const bool hasLauncherAuthArgs = (argc >= 3 && argv[1] && argv[1][0] && argv[2] && argv[2][0]);

    Log("=== Placeholder auth argv ===");
    Log("username argv[1] = %s", MaskedArgValue((argc >= 2) ? argv[1] : NULL));
    Log("password argv[2] = %s", MaskedArgValue((argc >= 3) ? argv[2] : NULL));

    if (!hasLauncherAuthArgs) {
        g_FilteredArgCount = static_cast<uint32_t>(argc);
        g_FilteredArgv = argv;
        return true;
    }

    const int filteredCount = 1 + ((argc > 3) ? (argc - 3) : 0);
    g_FilteredArgvOwned = static_cast<char**>(std::calloc(filteredCount + 1, sizeof(char*)));
    if (!g_FilteredArgvOwned) {
        Log("ERROR: failed to allocate filtered argv storage");
        return false;
    }

    g_FilteredArgvOwned[0] = argv[0];
    for (int src = 3, dst = 1; src < argc; ++src, ++dst) {
        g_FilteredArgvOwned[dst] = argv[src];
    }

    g_FilteredArgCount = static_cast<uint32_t>(filteredCount);
    g_FilteredArgv = g_FilteredArgvOwned;

    Log("launcher-only auth args detected; stripped 2 argv entries before InitClientDLL");
    if (argc > 3) {
        Log("forwarded extra launcher argv count = %d", argc - 3);
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

    char mediatorSelectionName[64] = {0};
    if (!EnvStringValue("MXO_MEDIATOR_SELECTION_NAME", mediatorSelectionName, sizeof(mediatorSelectionName))) {
        std::strcpy(mediatorSelectionName, "standalone");
    }
    if (EnvUint32Value("MXO_ARG7_SELECTION", &g_PackedArg7Selection)) {
        Log("DIAGNOSTIC: overridden arg7 packed selection from env = 0x%08x", g_PackedArg7Selection);
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
