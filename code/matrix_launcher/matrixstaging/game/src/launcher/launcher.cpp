#include "launcher.h"

#include <winsock2.h>
#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "autodetectdialog.h"
#include "../../../../src/diagnostics.h"
#include "../../../../src/launcher_mediator_abi.h"
#include "../../../../src/launcher_network_object_abi.h"
#include "../../../../src/launcher_replacement_support.h"
#include "../libltclientlogin/loginmediator.h"
#include "../../../runtime/src/libltbase/launchercommandline.h"

// anchor: launcher.exe:0x40a55c..0x40a5a4 / caller-clean 8-argument export frame
using InitClientDLLFunc = int (*)(
    uint32_t filteredArgCount,
    char** filteredArgv,
    HMODULE hClientDll,
    HMODULE hCresDll,
    void* launcherNetworkObject,
    void* pILTLoginMediator_0x4af2b8Default,
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
CLauncherCommandLine g_LauncherCommandLine;
CLauncher g_Launcher;
void* g_pLauncherObject6304 = NULL;
void* g_pILTLoginMediator_0x4af2b8Default = NULL;
uint8_t g_LauncherInitClientFlagByte = 0;   // original byte: [0x4d2c69]
char g_LastWorldName[256] = {0};

extern bool DiagnosticInitializePreclientEnvironmentLike402EC0();
extern bool IsRecoveredPreclientEnvironmentActive();

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


const char* MaskedArgValue(const char* value);
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
    }
    return true;
}

void LogKnownStartupState(const CLauncher& launcher) {
    spdlog::info("=== Known startup frame ===");
    spdlog::info("arg1 filteredArgCount        = 0x{:08x}", g_LauncherFilteredArgCount);
    spdlog::info("arg2 filteredArgv            = {}", fmt::ptr(g_LauncherFilteredArgv));
    spdlog::info("arg3 hClientDll              = {}", fmt::ptr(g_hClient));
    spdlog::info("arg4 hCresDll                = {}", fmt::ptr(g_hCres));
    spdlog::info("arg5 launcherNetworkObject   = {}", fmt::ptr(g_pLauncherObject6304));
    LauncherLogNetworkEngineAbiShellDispatchState(g_pLauncherObject6304, "InitClientDLL-args");
    spdlog::info("arg6 ILTLoginMediator_0x4af2b8Default = {}", fmt::ptr(g_pILTLoginMediator_0x4af2b8Default));
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
    // - Ghidra currently flattens the body to a free function, but the original launcher method
    //   boundary still matters for the recovered startup sequence
    // - faithful order from the raw listing is:
    //   1) allocate / construct the thread-per-client TCP engine shell (`malloc(0xb4)` + ctor)
    //   2) store the result to `0x4d6304`
    //   3) invoke the mediator's resolved vtable slot `+0x08` with that object
    //   4) return `result < 1`
    if (!g_pILTLoginMediator_0x4af2b8Default) {
        spdlog::error(
            "launcher.exe:0x40a380 requires ILTLoginMediator_0x4af2b8.Default before slot +0x08 handoff");
        return false;
    }

    // anchor: launcher.exe:0x40a3c0..0x40a3e9
    // - allocate/construct the thread-per-client TCP engine shell
    // - store it to global `0x4d6304`
    // anchor: launcher.exe:0x40a3e9..0x40a406
    // - call resolved mediator slot `+0x08` with that freshly built object
    // - preserve the original `result < 1` success test
    g_pLauncherObject6304 = LauncherCreateNetworkEngineAbiShell();

    void** mediatorVtable = *reinterpret_cast<void***>(g_pILTLoginMediator_0x4af2b8Default);
    typedef int (__thiscall *RegisterEngineFn)(void*, void*);
    RegisterEngineFn registerEngine = (mediatorVtable && mediatorVtable[2])
        ? reinterpret_cast<RegisterEngineFn>(mediatorVtable[2])
        : nullptr;
    const int registerResult = registerEngine
        ? registerEngine(g_pILTLoginMediator_0x4af2b8Default, g_pLauncherObject6304)
        : 1;

    const bool operationSucceeded = (registerResult < 1);
    spdlog::info(
        "DIAGNOSTIC: launcher.exe:0x40a380 thread-per-client TCP engine handoff mediatorResult={} object={} success={}",
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
    if (g_hClient) {
        // Diagnostic-only, disabled-by-default client-memory detour for the exact
        // `client.dll:0x6215b930` loading/status-text boundary.
        (void)DiagnosticMaybeInstallClientLoadingTextHook(g_hClient);
    }
    return g_hClient != NULL;
}

// anchor: launcher.exe:0x40a760
void CLauncher::UnloadClientDLL() const {
    DiagnosticRemoveClientLoadingTextHook();
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

// UNANCHORED: replacement-only synthesis that feeds the later 0x40a380 / 0x40b74d..0x40b7af
// corridor by materializing current arg6/arg7 startup state. This is more honest than treating the
// entire pre-client stretch as one faux method, but it still does not claim an exact original
// boundary.
bool CLauncher::MaterializeRecoveredInitClientStateFromSelectionName(const char* startupSelectionName) {
    if (!startupSelectionName) {
        return false;
    }

    char bootSelectionName[64] = {};
    std::strncpy(bootSelectionName, startupSelectionName, sizeof(bootSelectionName) - 1);
    bootSelectionName[sizeof(bootSelectionName) - 1] = '\0';

    // Replacement-only stand-in for launcher-owned selection fallback state that must already exist
    // by the time 0x40a4d0 consumes [this+0xa8]/[this+0xac]. Final world writeback is deferred to
    // the later pre-client character-selection stage after the character index is read.
    if (const RecoveredLauncherSelectionRecord* defaultSelection =
            DefaultRecoveredLauncherSelectionRecord();
        defaultSelection != nullptr && defaultSelection->selectionName != nullptr) {
        std::strncpy(bootSelectionName, defaultSelection->selectionName, sizeof(bootSelectionName) - 1);
        bootSelectionName[sizeof(bootSelectionName) - 1] = '\0';
        spdlog::info(
            "DIAGNOSTIC: defaulting launcher selection name to first recovered world '{}'",
            bootSelectionName);
    } else {
        std::strcpy(bootSelectionName, "standalone");
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection records are available; falling back to '{}'",
            bootSelectionName);
    }

    const RecoveredLauncherSelectionRecord* bootSelectionRecord =
        FindRecoveredLauncherSelectionRecord(bootSelectionName);
    if (bootSelectionRecord) {
        std::strncpy(bootSelectionName, bootSelectionRecord->selectionName, sizeof(bootSelectionName) - 1);
        bootSelectionName[sizeof(bootSelectionName) - 1] = '\0';
        m_FieldA8 = 0u;
        m_FieldAC = RecoveredSelectionWorldIndexLow24();
        const uint32_t packedArg7Selection = BuildPackedArg7Selection();
        spdlog::info(
            "DIAGNOSTIC: seeded launcher selection defaults from recovered world '{}' -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} selectionGateByte100={} variantState={} routePrefix='{}'",
            bootSelectionRecord->selectionName,
            m_FieldA8,
            m_FieldAC,
            packedArg7Selection,
            (unsigned)bootSelectionRecord->selectionGateByte100,
            (unsigned)bootSelectionRecord->variantState,
            bootSelectionRecord->routeHostPrefix ? bootSelectionRecord->routeHostPrefix : "");
    } else if (bootSelectionName[0] && lstrcmpiA(bootSelectionName, "standalone") != 0) {
        spdlog::warn(
            "DIAGNOSTIC: no recovered launcher selection defaults for world '{}'; keeping zeroed launcher arg7 fields until that world has a recovered launcher-owned selection record",
            bootSelectionName);
    }

    const uint32_t packedArg7Selection = BuildPackedArg7Selection();
    if ((m_FieldA8 | m_FieldAC) != 0) {
        spdlog::info("DIAGNOSTIC: packed arg7 rebuilt from launcher fields = 0x{:08x}", packedArg7Selection);
    }

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

    if (!PreloadDependencies()) {
        spdlog::info("ERROR: preload failed");
        return false;
    }

    DiagnosticInitializeMediatorStub();
    g_pILTLoginMediator_0x4af2b8Default = &g_LoginMediatorStub;

    const uint32_t selectedVariantState = bootSelectionRecord ? bootSelectionRecord->variantState : 0u;
    const uint32_t selectedHighByte = (packedArg7Selection >> 24) & 0xffu;
    const uint32_t selectionPackedLow24 = packedArg7Selection & 0x00ffffffu;
    const uint32_t worldUpperBoundExclusive = (selectionPackedLow24 < 0xffu) ? (selectionPackedLow24 + 1u) : 1u;
    const uint32_t variantUpperBoundExclusive = (selectedHighByte < 0xffu) ? (selectedHighByte + 1u) : 1u;
    DiagnosticConfigureMediatorSelection(
        worldUpperBoundExclusive,
        variantUpperBoundExclusive,
        bootSelectionName,
        bootSelectionName,
        selectionPackedLow24,
        selectedHighByte,
        selectedVariantState);
    DiagnosticApplyDefaultNopatchMediatorConfig(
        g_pILTLoginMediator_0x4af2b8Default,
        nopatchLauncherVersionValue,
        nopatchClientVersionValue);

    if (g_pILTLoginMediator_0x4af2b8Default) {
        uint32_t resolvedA8 = m_FieldA8;
        uint32_t resolvedAC = m_FieldAC;
        char resolvedSelectionName[sizeof(g_LastWorldName)] = {0};
        if (DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediator_0x4af2b8Default,
                m_FieldAC,
                m_FieldA8,
                &resolvedA8,
                &resolvedAC,
                resolvedSelectionName,
                sizeof(resolvedSelectionName))) {
            m_FieldA8 = resolvedA8;
            m_FieldAC = resolvedAC;
            const uint32_t packedArg7Selection2 = BuildPackedArg7Selection();
            if (resolvedSelectionName[0]) {
                std::strncpy(g_LastWorldName, resolvedSelectionName, sizeof(g_LastWorldName) - 1);
                g_LastWorldName[sizeof(g_LastWorldName) - 1] = '\0';
                StoreLastWorldNameInRegistry(g_LastWorldName);
            }
            spdlog::info(
                "DIAGNOSTIC: arg7 rebuilt through ILTLoginMediator_0x4af2b8.Default selection path -> a8=0x{:08x} ac=0x{:08x} packed=0x{:08x} world='{}'",
                m_FieldA8,
                m_FieldAC,
                packedArg7Selection2,
                g_LastWorldName[0] ? g_LastWorldName : bootSelectionName);
        }
    }

    // Read auth server config from mediator globals (set by ApplySelectedServerConfigToMediator)
    // anchor: launcher.exe:0x4f7b14 / 0x4f7a50
    const char* authServerDnsName = mxo::ltlogin::g_qsAuthServerDNSName;
    const uint16_t authServerPort = mxo::ltlogin::g_AuthServerPort;
    const bool ignoreHostsFileForAuth = (mxo::ltlogin::g_IgnoreHostsFileForAuth != 0);

    // Margin config - for now use empty suffix to connect to same host as auth
    // TODO: make this configurable per-server like auth
    const uint16_t marginServerPort = 10000;
    const bool ignoreHostsFileForMargin = false;
    char marginRoutePrefix[256] = {0};
    if (bootSelectionRecord && bootSelectionRecord->routeHostPrefix && bootSelectionRecord->routeHostPrefix[0]) {
        std::strncpy(marginRoutePrefix, bootSelectionRecord->routeHostPrefix, sizeof(marginRoutePrefix) - 1);
        marginRoutePrefix[sizeof(marginRoutePrefix) - 1] = '\0';
        spdlog::info("DIAGNOSTIC: using recovered route host prefix '{}' for world '{}'", marginRoutePrefix, bootSelectionRecord->selectionName);
    } else {
        LowercaseAsciiCopy(marginRoutePrefix, sizeof(marginRoutePrefix), bootSelectionName);
    }

    mxo::ltlogin::g_qsAuthServerDNSName = authServerDnsName ? authServerDnsName : "";
    mxo::ltlogin::g_IgnoreHostsFileForAuth = ignoreHostsFileForAuth ? 1u : 0u;
    mxo::ltlogin::g_AuthServerPort = authServerPort;
    mxo::ltlogin::g_marginServerDNSName = "";
    mxo::ltlogin::g_marginServerPort = marginServerPort;
    spdlog::info(
        "DIAGNOSTIC: launcher startup config auth='{}' authPort={} marginRoutePrefix='{}' marginPort={} ignoreAuthHosts={} ignoreMarginHosts={}",
        authServerDnsName ? authServerDnsName : "<empty>",
        authServerPort,
        marginRoutePrefix,
        marginServerPort,
        ignoreHostsFileForAuth ? 1 : 0,
        ignoreHostsFileForMargin ? 1 : 0);

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

// UNANCHORED: no-GUI wrapper for launcher.exe:0x40b75a -> 0x401520 -> DoModal -> 0x401640 -> 0x401590.
bool CLauncher::RunAutodetectDialogWithoutGui() const {
    spdlog::info("=== Running autodetect dialog path without GUI ===");
    CAutodetectDialog autodetectDialog;
    return autodetectDialog.RunWithoutGui();
}

// anchor: launcher.exe:0x40a4d0
void CLauncher::CleanupRecoveredInitClientState() const {
    if (g_pLauncherObject6304) {
        LauncherReleaseNetworkEngineAbiShell(&g_pLauncherObject6304, g_pILTLoginMediator_0x4af2b8Default);
    }
    g_pILTLoginMediator_0x4af2b8Default = NULL;
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
        g_pILTLoginMediator_0x4af2b8Default && g_pLauncherObject6304;
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
        g_pILTLoginMediator_0x4af2b8Default,
        (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24),
        g_LauncherInitClientFlagByte);
    spdlog::info("InitClientDLL returned: {}", initResult);
    if (initResult <= 0) {
        LogClientLifecycleFailure("InitClientDLL", errorClientDLL);
        return false;
    }

    // Original startup shape after `0x402ec0` returns:
    // - launcher-owned login / selection have already completed on the pre-client gate path
    // - `InitClientDLL` should therefore inherit that state directly instead of restarting auth
    //   through a synthetic post-init helper
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
    char startupSelectionName[64] = {};

    if (!ParseCommandLineStage()) {
        goto cleanup;
    }

    // UNANCHORED within 0x40b430: replacement-only arg6/arg7 startup-state materialization that
    // feeds the later anchored 0x40a380 / 0x40a4d0 call shape.
    if (!MaterializeRecoveredInitClientStateFromSelectionName(startupSelectionName)) {
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

    // Replacement-only console/UI orchestration lives in `src/textmode_launcher_flow.cpp` so this
    // recovered launcher-owned coordinator stays narrow around the 0x40b430 startup corridor.
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

