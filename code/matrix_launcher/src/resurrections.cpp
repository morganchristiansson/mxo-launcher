/**
 * Matrix Online launcher reimplementation scaffold.
 *
 * Goal:
 * - follow the original launcher.exe startup order as closely as current static
 *   knowledge allows
 * - do NOT inject into client.dll memory
 * - do NOT treat ad-hoc NULL-heavy InitClientDLL calls as the canonical path
 *
 * Current state:
 * - loads cres.dll before client.dll
 * - resolves the same client exports as launcher.exe
 * - models the real 8-argument InitClientDLL frame shape
 * - still lacks faithful reconstruction of launcher-owned objects:
 *     - 0x4d6304 : launcher-owned network/engine object
 *     - 0x4d2c58 : resolved ILTLoginMediator.Default interface pointer
 *
 * By default this scaffold stops before calling InitClientDLL with knowingly
 * incomplete launcher state. To deliberately experiment anyway, set:
 *
 *   MXO_FORCE_INCOMPLETE_INIT=1
 *
 * and optionally:
 *
 *   MXO_FORCE_RUNCLIENT=1
 *
 * Diagnostic only:
 *
 *   MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE=1
 *
 * which allows RunClientDLL to be called even after InitClientDLL fails.
 *
 *   MXO_STUB_LOGIN_MEDIATOR=1
 *
 * which injects a minimal launcher-side ILTLoginMediator.Default stub as arg6
 * for differential experiments.
 */

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>

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
// Only hCres/hClient are currently reconstructed here.
static uint32_t g_FilteredArgCount = 0;
static char** g_FilteredArgv = NULL;
static void* g_pLauncherObject6304 = NULL;         // original: [0x4d6304]
static void* g_pILTLoginMediatorDefault = NULL;   // original: [0x4d2c58]
static uint32_t g_PackedArg7Selection = 0;        // original: [this+0xa8]/[this+0xac]
static uint32_t g_FlagByte = 0;                   // original: [0x4d2c69]

static void* g_pClientDBFromCallback = NULL;

struct MinimalLoginMediatorStub {
    void** vtable;
};

static MinimalLoginMediatorStub g_LoginMediatorStub = {};
static void* g_LoginMediatorVtable[96] = {0};
static const char g_MediatorName[] = "ILTLoginMediator.Default";
static const char g_MediatorStringA[] = "resurrections";
static const char g_MediatorStringB[] = "nopatch";
static const char g_MediatorStringC[] = "standalone";

extern "C" DLLEXPORT void __stdcall SetMasterDatabase(void* pMasterDatabase);
static void Log(const char* fmt, ...);

static const char* __thiscall Mediator_GetName(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorName;
}

static int __thiscall Mediator_RegisterEngine(MinimalLoginMediatorStub* self, void* object) {
    (void)self;
    Log("MediatorStub::RegisterEngine(%p)", object);
    return 1;
}

static void __thiscall Mediator_ClearEngine(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::ClearEngine()");
}

static uint32_t __thiscall Mediator_IsReady(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsReady() -> 1");
    return 1;
}

static void __thiscall Mediator_SetValue1(MinimalLoginMediatorStub* self, void* value) {
    (void)self;
    Log("MediatorStub::SetValue1(%p)", value);
}

static uint32_t __thiscall Mediator_IsConnected(MinimalLoginMediatorStub* self) {
    (void)self;
    Log("MediatorStub::IsConnected() -> 1");
    return 1;
}

static const char* __thiscall Mediator_GetDisplayName(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorStringA;
}

static const char* __thiscall Mediator_GetString0(MinimalLoginMediatorStub* self) {
    (void)self;
    return g_MediatorStringA;
}

static const char* __thiscall Mediator_GetString1(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    (void)value;
    return g_MediatorStringB;
}

static const char* __thiscall Mediator_GetString2(MinimalLoginMediatorStub* self, const char* value) {
    (void)self;
    (void)value;
    return g_MediatorStringC;
}

static uint32_t __thiscall Mediator_GetArg7HighByteFloor(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

static uint32_t __thiscall Mediator_ShouldExportA(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

static uint32_t __thiscall Mediator_ShouldExportB(MinimalLoginMediatorStub* self) {
    (void)self;
    return 0;
}

static void InitializeMediatorStub() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::memset(g_LoginMediatorVtable, 0, sizeof(g_LoginMediatorVtable));
    g_LoginMediatorVtable[0] = (void*)Mediator_GetName;          // +0x00
    g_LoginMediatorVtable[2] = (void*)Mediator_RegisterEngine;   // +0x08
    g_LoginMediatorVtable[3] = (void*)Mediator_ClearEngine;      // +0x0c
    g_LoginMediatorVtable[4] = (void*)Mediator_IsReady;          // +0x10
    g_LoginMediatorVtable[7] = (void*)Mediator_SetValue1;        // +0x1c
    g_LoginMediatorVtable[9] = (void*)Mediator_SetValue1;        // +0x24
    g_LoginMediatorVtable[11] = (void*)Mediator_IsConnected;     // +0x2c
    g_LoginMediatorVtable[14] = (void*)Mediator_GetDisplayName;  // +0x38
    g_LoginMediatorVtable[22] = (void*)Mediator_GetString0;      // +0x58
    g_LoginMediatorVtable[23] = (void*)Mediator_GetString2;      // +0x5c
    g_LoginMediatorVtable[24] = (void*)Mediator_GetString1;      // +0x60
    g_LoginMediatorVtable[54] = (void*)Mediator_GetArg7HighByteFloor; // +0xd8
    g_LoginMediatorVtable[89] = (void*)Mediator_ShouldExportA;   // +0x164
    g_LoginMediatorVtable[91] = (void*)Mediator_ShouldExportB;   // +0x16c

    g_LoginMediatorStub.vtable = g_LoginMediatorVtable;
}

template <typename T>
static T ResolveProc(HMODULE module, const char* name) {
    FARPROC proc = GetProcAddress(module, name);
    T typed = nullptr;
    static_assert(sizeof(typed) == sizeof(proc), "function pointer size mismatch");
    std::memcpy(&typed, &proc, sizeof(typed));
    return typed;
}

static void Log(const char* fmt, ...) {
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

int main(int argc, char* argv[]) {
    g_LogFile = fopen("resurrections.log", "w");

    Log("Matrix Online launcher reimplementation scaffold");
    Log("===============================================");
    Log("Mode: original startup order, no client-memory injection");
    Log("");

    g_FilteredArgCount = static_cast<uint32_t>(argc);
    g_FilteredArgv = argv;

    Log("NOTE: arg1/arg2 currently reuse process argc/argv as placeholders.");
    Log("NOTE: arg5/arg6/arg7/arg8 are still unresolved launcher-owned state.");
    Log("");

    if (!PreloadDependencies()) {
        Log("ERROR: preload failed");
        return 1;
    }

    // Original order from launcher.exe:
    //   0x40a380  -> build 0x4d6304            (not yet reconstructed here)
    //   0x402ec0  -> environment setup         (not yet reconstructed here)
    //   0x40a780  -> LoadLibraryA("cres.dll")
    //   0x40a420  -> LoadLibraryA("client.dll")
    //   0x40a4d0  -> resolve exports + Init/Run/Term/Error path

    Log("=== Original-path gaps still missing ===");
    Log("missing: build/register launcher object at 0x4d6304");
    Log("missing: resolve ILTLoginMediator.Default into 0x4d2c58");
    Log("missing: original pre-client environment setup at 0x402ec0 (launcher thread / message readiness path)");
    Log("");

    if (!LoadCresDLL()) {
        Log("ERROR: failed to load cres.dll");
        return 1;
    }

    if (!LoadClientDLL()) {
        Log("ERROR: failed to load client.dll");
        return 1;
    }

    if (!ResolveClientExports()) {
        Log("ERROR: missing required client exports");
        return 1;
    }

    const bool forceIncompleteInit = EnvFlagEnabled("MXO_FORCE_INCOMPLETE_INIT");
    const bool forceRunClient = EnvFlagEnabled("MXO_FORCE_RUNCLIENT");
    const bool forceRunAfterInitFailure = EnvFlagEnabled("MXO_FORCE_RUNCLIENT_AFTER_INIT_FAILURE");
    const bool useMediatorStub = EnvFlagEnabled("MXO_STUB_LOGIN_MEDIATOR");

    if (useMediatorStub) {
        InitializeMediatorStub();
        g_pILTLoginMediatorDefault = &g_LoginMediatorStub;
        Log("DIAGNOSTIC: using MinimalLoginMediatorStub for arg6 (%p)", g_pILTLoginMediatorDefault);
    }

    LogKnownStartupState();
    Log("");

    if (!forceIncompleteInit) {
        Log("Refusing to call InitClientDLL with knowingly incomplete launcher state.");
        Log("Set MXO_FORCE_INCOMPLETE_INIT=1 to run a deliberate original-path experiment.");
        if (g_LogFile) fclose(g_LogFile);
        return 2;
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
            if (g_LogFile) fclose(g_LogFile);
            return 1;
        }
        Log("DIAGNOSTIC OVERRIDE: continuing to RunClientDLL despite InitClientDLL failure.");
    }

    if (!forceRunClient) {
        Log("InitClientDLL succeeded, but RunClientDLL is gated.");
        Log("Set MXO_FORCE_RUNCLIENT=1 for a deliberate runtime experiment.");
        if (g_LogFile) fclose(g_LogFile);
        return 0;
    }

    Log("=== Forced experiment: calling RunClientDLL ===");
    g_RunClientDLL();
    Log("RunClientDLL returned");

    if (g_LogFile) fclose(g_LogFile);
    return 0;
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
