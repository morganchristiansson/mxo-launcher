#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

struct WindowTraceEntry {
    HWND hwnd;
    LONG style;
    LONG exStyle;
    RECT rect;
    BOOL visible;
    BOOL iconic;
};

static DWORD g_MainProcessId = 0;
static HANDLE g_hWindowTraceThread = NULL;
static volatile LONG g_WindowTraceRunning = 0;
static WindowTraceEntry g_WindowTraceEntries[32] = {};
static int g_WindowTraceEntryCount = 0;
static int g_LastWindowTraceCount = -1;
static std::string g_LastClientLoadingStateText;
static std::string g_LastClientLoadingStateSource;


static bool g_ClientLoadingTextHookEnvChecked = false;
static bool g_ClientLoadingTextHookRequested = false;
static bool g_ClientLoadingTextHookInstalled = false;
static HMODULE g_ClientLoadingTextHookModule = NULL;
static uint8_t* g_ClientLoadingTextHookTarget = nullptr;
static uint8_t* g_ClientLoadingTextHookTrampoline = nullptr;
static uint8_t* g_ClientLoadingTextHookStub = nullptr;
static uint8_t g_ClientLoadingTextHookOriginalBytes[16] = {};
static constexpr uintptr_t kClientDllImageBase = 0x62000000u;
static constexpr uintptr_t kClientLoadingTextFunctionAbsolute = 0x6215b930u;
static constexpr uintptr_t kClientLoadingTextFunctionRva =
    kClientLoadingTextFunctionAbsolute - kClientDllImageBase;
static constexpr size_t kClientLoadingTextHookPatchSize = 6u;
static constexpr uint8_t kExpectedClientLoadingTextHookPrologue[kClientLoadingTextHookPatchSize] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c,
};

static_assert(sizeof(void*) == 4, "client loading-text diagnostic hook assumes x86/32-bit build");


// UNANCHORED: diagnostic-only window-state cache/update helper.
static void UpsertWindowTraceEntry(
    HWND hwnd,
    LONG style,
    LONG exStyle,
    const RECT& rect,
    BOOL visible,
    BOOL iconic,
    const char* className,
    const char* title) {

    int index = -1;
    for (int i = 0; i < g_WindowTraceEntryCount; ++i) {
        if (g_WindowTraceEntries[i].hwnd == hwnd) {
            index = i;
            break;
        }
    }

    const bool isNew = (index < 0);
    WindowTraceEntry previous = {};
    if (!isNew) previous = g_WindowTraceEntries[index];

    const bool changed = isNew ||
        previous.style != style ||
        previous.exStyle != exStyle ||
        previous.rect.left != rect.left ||
        previous.rect.top != rect.top ||
        previous.rect.right != rect.right ||
        previous.rect.bottom != rect.bottom ||
        previous.visible != visible ||
        previous.iconic != iconic;

    if (changed) {
        spdlog::info(
            "WindowTrace hwnd={} visible={} iconic={} class='{}' title='{}' style=0x{} exStyle=0x{} rect=({}:{})-({}:{})",
            fmt::ptr(hwnd),
            visible ? 1 : 0,
            iconic ? 1 : 0,
            className,
            title,
            (unsigned long)style,
            (unsigned long)exStyle,
            (long)rect.left,
            (long)rect.top,
            (long)rect.right,
            (long)rect.bottom);
    }

    if (isNew) {
        if (g_WindowTraceEntryCount >= (int)(sizeof(g_WindowTraceEntries) / sizeof(g_WindowTraceEntries[0]))) {
            return;
        }
        index = g_WindowTraceEntryCount++;
    }

    g_WindowTraceEntries[index].hwnd = hwnd;
    g_WindowTraceEntries[index].style = style;
    g_WindowTraceEntries[index].exStyle = exStyle;
    g_WindowTraceEntries[index].rect = rect;
    g_WindowTraceEntries[index].visible = visible;
    g_WindowTraceEntries[index].iconic = iconic;
}

// UNANCHORED: diagnostic-only EnumWindows callback for current-process windows.
static BOOL CALLBACK WindowTraceEnumProc(HWND hwnd, LPARAM lParam) {
    int* count = reinterpret_cast<int*>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != g_MainProcessId) return TRUE;

    ++(*count);

    char className[256] = {0};
    char title[256] = {0};
    GetClassNameA(hwnd, className, sizeof(className));
    GetWindowTextA(hwnd, title, sizeof(title));

    RECT rect = {};
    GetWindowRect(hwnd, &rect);

    UpsertWindowTraceEntry(
        hwnd,
        GetWindowLongA(hwnd, GWL_STYLE),
        GetWindowLongA(hwnd, GWL_EXSTYLE),
        rect,
        IsWindowVisible(hwnd),
        IsIconic(hwnd),
        className,
        title);

    return TRUE;
}

// UNANCHORED: diagnostic-only polling thread for launcher/client window transitions.
static DWORD WINAPI WindowTraceThreadProc(LPVOID) {
    DWORD lastWidth = 0;
    DWORD lastHeight = 0;
    DWORD lastBpp = 0;
    DWORD lastHz = 0;

    spdlog::info("WindowTrace: started for pid {}", (unsigned long)g_MainProcessId);

    while (InterlockedCompareExchange(&g_WindowTraceRunning, 0, 0) != 0) {
        DEVMODEA mode = {};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
            if (mode.dmPelsWidth != lastWidth ||
                mode.dmPelsHeight != lastHeight ||
                mode.dmBitsPerPel != lastBpp ||
                mode.dmDisplayFrequency != lastHz) {
                lastWidth = mode.dmPelsWidth;
                lastHeight = mode.dmPelsHeight;
                lastBpp = mode.dmBitsPerPel;
                lastHz = mode.dmDisplayFrequency;
            }
        }

        int count = 0;
        EnumWindows(WindowTraceEnumProc, reinterpret_cast<LPARAM>(&count));
        if (count != g_LastWindowTraceCount) {
            g_LastWindowTraceCount = count;
            spdlog::info("WindowTrace top-level window count: {}", count);
        }

        Sleep(250);
    }

    spdlog::info("WindowTrace: stopped");
    return 0;
}

void DiagnosticStartWindowTrace() {
    if (g_hWindowTraceThread) return;

    g_MainProcessId = GetCurrentProcessId();
    InterlockedExchange(&g_WindowTraceRunning, 1);
    g_hWindowTraceThread = CreateThread(NULL, 0, WindowTraceThreadProc, NULL, 0, NULL);
    if (!g_hWindowTraceThread) {
        InterlockedExchange(&g_WindowTraceRunning, 0);
        spdlog::info("WindowTrace: CreateThread failed ({})", GetLastError());
    }
}

void DiagnosticStopWindowTrace() {
    if (!g_hWindowTraceThread) return;
    InterlockedExchange(&g_WindowTraceRunning, 0);
    WaitForSingleObject(g_hWindowTraceThread, 1000);
    CloseHandle(g_hWindowTraceThread);
    g_hWindowTraceThread = NULL;
}

void DiagnosticLogClientLoadingStateText(const char* text, const char* source) {
    const std::string nextText = (text && text[0]) ? text : "<empty>";
    const std::string nextSource = (source && source[0]) ? source : "<unspecified>";
    if (g_LastClientLoadingStateText == nextText && g_LastClientLoadingStateSource == nextSource) {
        return;
    }

    g_LastClientLoadingStateText = nextText;
    g_LastClientLoadingStateSource = nextSource;
    spdlog::info(
        "ClientLoadingState text='{}' source={} (client-visible loading/status text boundary)",
        nextText,
        nextSource);
}

static bool DiagnosticEnvFlagEnabled(const char* variableName) {
    if (!variableName || !variableName[0]) {
        return false;
    }

    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(variableName, value, sizeof(value));
    if (length == 0 || length >= sizeof(value)) {
        return false;
    }

    value[sizeof(value) - 1u] = '\0';
    return
        value[0] == '1' ||
        _stricmp(value, "true") == 0 ||
        _stricmp(value, "yes") == 0 ||
        _stricmp(value, "on") == 0;
}

static bool DiagnosticClientLoadingTextHookRequestedViaEnv() {
    if (!g_ClientLoadingTextHookEnvChecked) {
        const bool hookDisabled =
            DiagnosticEnvFlagEnabled("MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK");
        g_ClientLoadingTextHookRequested = !hookDisabled;
        g_ClientLoadingTextHookEnvChecked = true;
        if (hookDisabled) {
            spdlog::info(
                "DIAGNOSTIC: MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK disabled exact client loading-text hook");
        } else {
            spdlog::info(
                "DIAGNOSTIC: exact client loading-text hook enabled by default");
        }
    }
    return g_ClientLoadingTextHookRequested;
}

static void DiagnosticWriteRelativeJump(uint8_t* patchLocation, const void* destination) {
    const intptr_t displacement =
        reinterpret_cast<const uint8_t*>(destination) - (patchLocation + 5u);
    const int32_t relativeDisplacement = static_cast<int32_t>(displacement);
    patchLocation[0] = 0xe9;
    std::memcpy(patchLocation + 1u, &relativeDisplacement, sizeof(relativeDisplacement));
}

struct DiagnosticClientLoadingTextReplacement {
    const char* original;
    const char* replacement;
};

static const char* DiagnosticFindClientLoadingTextReplacement(const char* text) {
    if (!text || !text[0]) {
        return text;
    }

    static const DiagnosticClientLoadingTextReplacement kReplacements[] = {
        {"Initializing Client Data Cache", "Recovering Lost Fragments"},
        {"Initializing Inventory Manager", "Reclaiming Possessions"},
        {"Initializing Shortcut Manager", "Restoring Muscle Memory"},
        {"Initializing Game Object Manager", "Reforming Physical Constructs"},
        {"Initializing Character Animations", "Teaching the Body to Move Again"},
        {"Initializing Rules Subsystem", "Rewriting the Rules of This World"},
        {"Initializing Animation Tables", "Reconnecting Motion Memory"},
        {"Initializing Chat Manager", "Opening Lines of Communication"},
        {"Initializing Abilities", "Remembering What You Can Do"},
        {"Initializing FX", "Reigniting Sensory Echoes"},
        {"Initializing Metro World", "Rebuilding the City Around You"},
        {"Initializing World Render Data", "Pulling Reality Into Shape"},
        {"Initializing Interlock Database", "Accessing Locked Memories"},
        {"Initializing RichWorld", "Recreating the World Layer"},
        {"Initializing Water", "Letting Water Flow Again"},
        {"Initializing Projected Textures", "Painting the Surface of Things"},
        {"Loading Character", "Resurrecting You"},
        {"Waiting for Regionserver", "Waiting for the World to Notice"},
    };

    for (const DiagnosticClientLoadingTextReplacement& entry : kReplacements) {
        if (std::strcmp(text, entry.original) == 0) {
            return entry.replacement;
        }
    }
    return text;
}

static const char* DiagnosticClientLoadingTextHookOnText(const char* text, void* statusSink) {
    if (!statusSink) {
        return text;
    }

    const char* visibleText = text ? text : "<empty>";
    const char* replacement = DiagnosticFindClientLoadingTextReplacement(visibleText);
    if (replacement && std::strcmp(replacement, visibleText) != 0) {
        spdlog::info(
            "DIAGNOSTIC: replacing client loading text original='{}' replacement='{}'",
            visibleText,
            replacement);
        DiagnosticLogClientLoadingStateText(
            replacement,
            "client.dll:FUN_6215b930 visible replacement");
        return replacement;
    }

    DiagnosticLogClientLoadingStateText(
        visibleText,
        "client.dll:FUN_6215b930 exact hook");
    return visibleText;
}

static uint8_t* DiagnosticAllocateExecutableBlockNearAddress(
    const void* nearAddress,
    size_t byteCount) {
    if (!nearAddress || byteCount == 0u) {
        return nullptr;
    }

    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);
    const uintptr_t allocationGranularity =
        systemInfo.dwAllocationGranularity != 0u
            ? static_cast<uintptr_t>(systemInfo.dwAllocationGranularity)
            : static_cast<uintptr_t>(0x10000u);
    const uintptr_t alignedBase =
        reinterpret_cast<uintptr_t>(nearAddress) & ~(allocationGranularity - 1u);

    for (uintptr_t step = 0u; step != 0x400u; ++step) {
        const uintptr_t positiveCandidate = alignedBase + (step * allocationGranularity);
        if (void* block = VirtualAlloc(
                reinterpret_cast<void*>(positiveCandidate),
                byteCount,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE)) {
            return reinterpret_cast<uint8_t*>(block);
        }

        if (step != 0u) {
            const uintptr_t negativeCandidate = alignedBase - (step * allocationGranularity);
            if (void* block = VirtualAlloc(
                    reinterpret_cast<void*>(negativeCandidate),
                    byteCount,
                    MEM_COMMIT | MEM_RESERVE,
                    PAGE_EXECUTE_READWRITE)) {
                return reinterpret_cast<uint8_t*>(block);
            }
        }
    }

    return reinterpret_cast<uint8_t*>(VirtualAlloc(
        nullptr,
        byteCount,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
}

static bool DiagnosticBuildClientLoadingTextHookTrampoline(
    uint8_t* target,
    uint8_t** outTrampoline) {
    if (!target || !outTrampoline) {
        return false;
    }

    uint8_t* trampoline = DiagnosticAllocateExecutableBlockNearAddress(
        target,
        kClientLoadingTextHookPatchSize + 5u);
    if (!trampoline) {
        return false;
    }

    std::memcpy(trampoline, target, kClientLoadingTextHookPatchSize);
    DiagnosticWriteRelativeJump(
        trampoline + kClientLoadingTextHookPatchSize,
        target + kClientLoadingTextHookPatchSize);
    *outTrampoline = trampoline;
    return true;
}

// Diagnostic-only x86 entry stub for `client.dll:0x6215b930`.
// Disassembly-proved call shape:
// - stack `param_1` = text pointer
// - latent `ESI` = status-sink object consumed by the original body
// So the stub saves/restores all registers/flags, logs only when saved `ESI != 0`, then jumps to
// a trampoline that replays the original prologue bytes unchanged.
static bool DiagnosticBuildClientLoadingTextHookStub(
    void* target,
    void* trampoline,
    uint8_t** outStub) {
    if (!target || !trampoline || !outStub) {
        return false;
    }

    uint8_t* stub = DiagnosticAllocateExecutableBlockNearAddress(target, 64u);
    if (!stub) {
        return false;
    }

    uint8_t* cursor = stub;
    *cursor++ = 0x9c; // pushfd
    *cursor++ = 0x60; // pushad

    const uint8_t captureTextAndSink[] = {
        0x8b, 0x44, 0x24, 0x28, // mov eax, [esp+0x28] ; original param_1 text
        0x8b, 0x54, 0x24, 0x04, // mov edx, [esp+0x04] ; saved ESI from pushad
        0x85, 0xd2,             // test edx, edx
        0x74, 0x10,             // jz skip_helper_call_and_keep_original_text
        0x52,                   // push edx
        0x50,                   // push eax
        0xb8,                   // mov eax, imm32(helper)
    };
    std::memcpy(cursor, captureTextAndSink, sizeof(captureTextAndSink));
    cursor += sizeof(captureTextAndSink);

    const uintptr_t helperAddress =
        reinterpret_cast<uintptr_t>(&DiagnosticClientLoadingTextHookOnText);
    std::memcpy(cursor, &helperAddress, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    const uint8_t callHelperAndRestore[] = {
        0xff, 0xd0,             // call eax
        0x83, 0xc4, 0x08,       // add esp, 8
        0x89, 0x44, 0x24, 0x28, // mov [esp+0x28], eax ; replace original text pointer when needed
        0x61,                   // popad
        0x9d,                   // popfd
        0xb8,                   // mov eax, imm32(trampoline)
    };
    std::memcpy(cursor, callHelperAndRestore, sizeof(callHelperAndRestore));
    cursor += sizeof(callHelperAndRestore);

    const uintptr_t trampolineAddress = reinterpret_cast<uintptr_t>(trampoline);
    std::memcpy(cursor, &trampolineAddress, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    const uint8_t jumpToTrampoline[] = {
        0xff, 0xe0, // jmp eax
    };
    std::memcpy(cursor, jumpToTrampoline, sizeof(jumpToTrampoline));
    cursor += sizeof(jumpToTrampoline);

    FlushInstructionCache(GetCurrentProcess(), stub, static_cast<SIZE_T>(cursor - stub));
    *outStub = stub;
    return true;
}

// Diagnostic-only, disabled-by-default runtime detour for exact client-visible loading/status
// text capture. This is intentionally opt-in so the normal path still avoids client-memory patching.
bool DiagnosticMaybeInstallClientLoadingTextHook(HMODULE clientModule) {
    if (!DiagnosticClientLoadingTextHookRequestedViaEnv()) {
        return false;
    }
    if (!clientModule) {
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook requested but client.dll is null");
        return false;
    }
    if (g_ClientLoadingTextHookInstalled && g_ClientLoadingTextHookModule == clientModule) {
        return true;
    }

    DiagnosticRemoveClientLoadingTextHook();

    uint8_t* const target =
        reinterpret_cast<uint8_t*>(clientModule) + kClientLoadingTextFunctionRva;
    if (std::memcmp(
            target,
            kExpectedClientLoadingTextHookPrologue,
            kClientLoadingTextHookPatchSize) != 0) {
        spdlog::warn(
            "DIAGNOSTIC: exact client loading-text hook aborted because client.dll:{} prologue no longer matches expected bytes",
            fmt::ptr(target));
        return false;
    }

    uint8_t* trampoline = nullptr;
    if (!DiagnosticBuildClientLoadingTextHookTrampoline(target, &trampoline)) {
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook failed to allocate trampoline");
        return false;
    }

    uint8_t* stub = nullptr;
    if (!DiagnosticBuildClientLoadingTextHookStub(target, trampoline, &stub)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook failed to allocate detour stub");
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, kClientLoadingTextHookPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        spdlog::warn(
            "DIAGNOSTIC: exact client loading-text hook failed to change page protection ({})",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }

    std::memcpy(
        g_ClientLoadingTextHookOriginalBytes,
        target,
        kClientLoadingTextHookPatchSize);
    DiagnosticWriteRelativeJump(target, stub);
    if (kClientLoadingTextHookPatchSize > 5u) {
        std::memset(target + 5u, 0x90, kClientLoadingTextHookPatchSize - 5u);
    }
    FlushInstructionCache(GetCurrentProcess(), target, kClientLoadingTextHookPatchSize);

    DWORD restoreProtect = 0;
    (void)VirtualProtect(target, kClientLoadingTextHookPatchSize, oldProtect, &restoreProtect);

    g_ClientLoadingTextHookInstalled = true;
    g_ClientLoadingTextHookModule = clientModule;
    g_ClientLoadingTextHookTarget = target;
    g_ClientLoadingTextHookTrampoline = trampoline;
    g_ClientLoadingTextHookStub = stub;
    spdlog::info(
        "DIAGNOSTIC: installed exact client loading-text hook target={} rva=0x{:06x} stub={} trampoline={}",
        fmt::ptr(target),
        static_cast<unsigned>(kClientLoadingTextFunctionRva),
        fmt::ptr(stub),
        fmt::ptr(trampoline));
    return true;
}

void DiagnosticRemoveClientLoadingTextHook() {
    if (!g_ClientLoadingTextHookInstalled) {
        return;
    }

    if (g_ClientLoadingTextHookTarget) {
        DWORD oldProtect = 0;
        if (VirtualProtect(
                g_ClientLoadingTextHookTarget,
                kClientLoadingTextHookPatchSize,
                PAGE_EXECUTE_READWRITE,
                &oldProtect)) {
            std::memcpy(
                g_ClientLoadingTextHookTarget,
                g_ClientLoadingTextHookOriginalBytes,
                kClientLoadingTextHookPatchSize);
            FlushInstructionCache(
                GetCurrentProcess(),
                g_ClientLoadingTextHookTarget,
                kClientLoadingTextHookPatchSize);
            DWORD restoreProtect = 0;
            (void)VirtualProtect(
                g_ClientLoadingTextHookTarget,
                kClientLoadingTextHookPatchSize,
                oldProtect,
                &restoreProtect);
        } else {
            spdlog::warn(
                "DIAGNOSTIC: failed to restore exact client loading-text hook bytes ({})",
                static_cast<unsigned long>(GetLastError()));
        }
    }

    if (g_ClientLoadingTextHookStub) {
        VirtualFree(g_ClientLoadingTextHookStub, 0, MEM_RELEASE);
    }
    if (g_ClientLoadingTextHookTrampoline) {
        VirtualFree(g_ClientLoadingTextHookTrampoline, 0, MEM_RELEASE);
    }

    spdlog::info(
        "DIAGNOSTIC: removed exact client loading-text hook target={} rva=0x{:06x}",
        fmt::ptr(g_ClientLoadingTextHookTarget),
        static_cast<unsigned>(kClientLoadingTextFunctionRva));
    g_ClientLoadingTextHookInstalled = false;
    g_ClientLoadingTextHookModule = NULL;
    g_ClientLoadingTextHookTarget = nullptr;
    g_ClientLoadingTextHookTrampoline = nullptr;
    g_ClientLoadingTextHookStub = nullptr;
    std::memset(g_ClientLoadingTextHookOriginalBytes, 0, sizeof(g_ClientLoadingTextHookOriginalBytes));
}
