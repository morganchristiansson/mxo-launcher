#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern HMODULE g_hClient;

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
static const void* g_LastClientShellObserved = nullptr;
static uint32_t g_LastClientShellState20 = 0xffffffffu;
static ULONGLONG g_LastClientShellState20ChangeTick = 0;
static uint32_t g_LastLoggedState20StallQuarterSeconds = 0;
static const void* g_LastClientShellRuntimeObjectD0 = nullptr;
static const void* g_LastClientShellRuntimeVftableD0 = nullptr;
static bool g_DumpedClientPiTableAtRuntime = false;

// Stub for removed D3D diagnostics
void DiagnosticLogLastD3DDeviceActivity() {
}

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

    const uint32_t* words = static_cast<const uint32_t*>(base);
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

static uintptr_t DiagnosticPointerToClientAbsolute(const void* address) {
    const uint8_t* clientBase =
        reinterpret_cast<const uint8_t*>(g_hClient ? g_hClient : GetModuleHandleA("client.dll"));
    const uint8_t* pointerBytes = static_cast<const uint8_t*>(address);
    if (!clientBase || !pointerBytes || pointerBytes < clientBase) {
        return 0u;
    }
    return 0x62000000u + static_cast<uintptr_t>(pointerBytes - clientBase);
}

static const char* DiagnosticDescribeModuleForAddress(const void* address) {
    if (!address) {
        return "<null>";
    }
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        return "<unknown>";
    }
    const HMODULE allocationBase = static_cast<HMODULE>(mbi.AllocationBase);
    if (allocationBase == g_hClient || allocationBase == GetModuleHandleA("client.dll")) {
        return "client.dll";
    }
    if (allocationBase == GetModuleHandleA("r3d9.dll")) {
        return "r3d9.dll";
    }
    if (allocationBase == GetModuleHandleA("d3d9.dll")) {
        return "d3d9.dll";
    }
    if (allocationBase == GetModuleHandleA(nullptr)) {
        return "resurrections.exe";
    }
    return "<other-module>";
}

static const void* DiagnosticClientAbsoluteToPointer(uintptr_t absoluteAddress) {
    const uint8_t* clientBase =
        reinterpret_cast<const uint8_t*>(g_hClient ? g_hClient : GetModuleHandleA("client.dll"));
    if (!clientBase || absoluteAddress < 0x62000000u) {
        return nullptr;
    }
    return clientBase + (absoluteAddress - 0x62000000u);
}

static void DiagnosticLogClientBlIlCorpusObject() {
    const void* const object = DiagnosticClientAbsoluteToPointer(0x629ee290u);
    const void* const flagBl = DiagnosticClientAbsoluteToPointer(0x629e95b0u);
    const void* const flagIl = DiagnosticClientAbsoluteToPointer(0x629e95b1u);
    if (flagBl && DiagnosticReadableMemoryRange(flagBl, 2)) {
        const uint8_t blLoaded = *static_cast<const uint8_t*>(flagBl);
        const uint8_t ilLoaded = *static_cast<const uint8_t*>(flagIl);
        spdlog::info(
            "ClientBlIlCorpus flags blLoaded=0x{:02x} ilLoaded=0x{:02x}",
            static_cast<unsigned>(blLoaded),
            static_cast<unsigned>(ilLoaded));
    }
    LogWordSpanIfReadable("ClientBlIlCorpus DAT_629ee290", object, 24);
}

static void DiagnosticLogClientPiTable() {
    // client.dll:0x621c9d70 (`AdoptLiveSelectionPiCfgCompactRecords`) only materializes compact live
    // `pi.cfg` tuples into the first two dwords of each 12-byte slot. Track the third dword too,
    // but count active entries from `value0/value1` only so the log distinguishes payload-bearing
    // slots from slots that merely retain a common default tail value.
    const uint8_t* piTableBase = static_cast<const uint8_t*>(DiagnosticClientAbsoluteToPointer(0x629ea4e8u));
    if (!piTableBase) {
        return;
    }
    std::vector<unsigned> activeIds;
    unsigned nonZeroValue2Count = 0;
    bool haveCommonValue2 = false;
    bool allValue2Same = true;
    uint32_t commonValue2 = 0u;
    for (unsigned index = 0; index < 0x6bu; ++index) {
        const uint8_t* entry = piTableBase + 0x1d8u + index * 0x0cu;
        if (!DiagnosticReadableMemoryRange(entry, 12)) {
            return;
        }
        const uint32_t value0 = *reinterpret_cast<const uint32_t*>(entry + 0x0);
        const uint32_t value1 = *reinterpret_cast<const uint32_t*>(entry + 0x4);
        const uint32_t value2 = *reinterpret_cast<const uint32_t*>(entry + 0x8);
        if (value2 != 0u) {
            ++nonZeroValue2Count;
        }
        if (!haveCommonValue2) {
            commonValue2 = value2;
            haveCommonValue2 = true;
        } else if (value2 != commonValue2) {
            allValue2Same = false;
        }
        if (value0 == 0u && value1 == 0u) {
            continue;
        }
        activeIds.push_back(index);
        spdlog::info(
            "ClientPiTable active entry id={} value0=0x{:08x} value1=0x{:08x} value2=0x{:08x}",
            index,
            value0,
            value1,
            value2);
    }
    std::string idsText;
    for (size_t i = 0; i < activeIds.size(); ++i) {
        if (i != 0) {
            idsText += ",";
        }
        idsText += std::to_string(activeIds[i]);
    }
    spdlog::info(
        "ClientPiTable activeEntryCount={} nonZeroValue2Count={} allValue2Same={} commonValue2=0x{:08x} activeIds={}",
        activeIds.size(),
        nonZeroValue2Count,
        allValue2Same ? 1 : 0,
        commonValue2,
        idsText);
}

static const char* DiagnosticDescribeClientRuntimeVftable(const void* vftable) {
    if (!vftable) {
        return "<null>";
    }
    if (vftable == DiagnosticClientAbsoluteToPointer(0x628b1638u)) {
        return "CLTRemoteCommCtx-like";
    }
    return "<unclassified>";
}

static void DiagnosticLogClientShellRuntimeTransitionState() {
    // anchor: client.dll:0x62172552 writes client-shell `+0xd0`; client.dll:0x62173bcd later reads
    // that same field before the alternate null-vcall family at `0x62173bd9`.
    const void* const clientShellSlot = DiagnosticClientAbsoluteToPointer(0x629e68a8u);
    if (!clientShellSlot || !DiagnosticReadableMemoryRange(clientShellSlot, sizeof(void*))) {
        return;
    }

    const void* const clientShell = *static_cast<const void* const*>(clientShellSlot);
    uint32_t state20 = 0xffffffffu;
    const void* runtimeObjectD0 = nullptr;
    const void* runtimeVftableD0 = nullptr;
    if (clientShell && DiagnosticReadableMemoryRange(static_cast<const uint8_t*>(clientShell) + 0x20, sizeof(uint32_t))) {
        state20 = *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(clientShell) + 0x20);
    }
    if (clientShell && DiagnosticReadableMemoryRange(static_cast<const uint8_t*>(clientShell) + 0xd0, sizeof(void*))) {
        runtimeObjectD0 = *reinterpret_cast<const void* const*>(static_cast<const uint8_t*>(clientShell) + 0xd0);
    }
    if (runtimeObjectD0 && DiagnosticReadableMemoryRange(runtimeObjectD0, sizeof(void*))) {
        runtimeVftableD0 = *static_cast<const void* const*>(runtimeObjectD0);
    }

    const ULONGLONG nowTick = GetTickCount64();
    if (g_LastClientShellState20 == 0xffffffffu || state20 != g_LastClientShellState20) {
        g_LastClientShellState20ChangeTick = nowTick;
        g_LastLoggedState20StallQuarterSeconds = 0;
    }

    if (clientShell == g_LastClientShellObserved &&
        state20 == g_LastClientShellState20 &&
        runtimeObjectD0 == g_LastClientShellRuntimeObjectD0 &&
        runtimeVftableD0 == g_LastClientShellRuntimeVftableD0) {
        if (state20 == 2u && g_LastClientShellState20ChangeTick != 0) {
            const uint32_t stalledQuarterSeconds =
                static_cast<uint32_t>((nowTick - g_LastClientShellState20ChangeTick) / 250u);
            if (stalledQuarterSeconds >= 2u && stalledQuarterSeconds > g_LastLoggedState20StallQuarterSeconds) {
                g_LastLoggedState20StallQuarterSeconds = stalledQuarterSeconds;
                spdlog::info(
                    "ClientShell runtime state20=0x00000002 unchanged for {} ms shell={} d0Object={} d0Vftable={}",
                    static_cast<unsigned long long>(nowTick - g_LastClientShellState20ChangeTick),
                    fmt::ptr(clientShell),
                    fmt::ptr(runtimeObjectD0),
                    fmt::ptr(runtimeVftableD0));
            }
        }
        return;
    }

    spdlog::info(
        "ClientShell runtime transition shell={} state20=0x{:08x} d0Object={} d0Vftable={} [{}]",
        fmt::ptr(clientShell),
        state20,
        fmt::ptr(runtimeObjectD0),
        fmt::ptr(runtimeVftableD0),
        DiagnosticDescribeClientRuntimeVftable(runtimeVftableD0));

    if (runtimeObjectD0 != nullptr) {
        LogWordSpanIfReadable("ClientShell runtime d0 object", runtimeObjectD0, 8);
        if (runtimeVftableD0 == DiagnosticClientAbsoluteToPointer(0x628b1638u)) {
            // Current tightened static read on this vftable family:
            // - `0x623baf60 = CLTRemoteCommCtx_ResetIO`
            // - `0x623bc640 = CLTRemoteCommCtx_IsIdle`
            // - `0x623bdd60` (`vftable +0x68`) checks object `+0x0c` and low bits of `+0x154`
            // - `0x623bdd40` reads bit 2 from byte `+0x2da`
            // - `0x623bdd10` returns dword `+0x2e4`
            const uint8_t* objectBytes = static_cast<const uint8_t*>(runtimeObjectD0);
            if (DiagnosticReadableMemoryRange(objectBytes + 0x2e4, sizeof(uint32_t))) {
                const uint32_t field0c = *reinterpret_cast<const uint32_t*>(objectBytes + 0x0c);
                const uint8_t flags154 = *(objectBytes + 0x154);
                const uint8_t flag2d8 = *(objectBytes + 0x2d8);
                const uint8_t flag2d9 = *(objectBytes + 0x2d9);
                const uint8_t flag2da = *(objectBytes + 0x2da);
                const uint32_t field2e4 = *reinterpret_cast<const uint32_t*>(objectBytes + 0x2e4);
                spdlog::info(
                    "ClientShell runtime d0 CLTRemoteCommCtx fields field0c=0x{:08x} flags154=0x{:02x} flag2d8={} flag2d9={} flag2da=0x{:02x} field2e4=0x{:08x}",
                    field0c,
                    static_cast<unsigned>(flags154),
                    static_cast<unsigned>(flag2d8),
                    static_cast<unsigned>(flag2d9),
                    static_cast<unsigned>(flag2da),
                    field2e4);
            }
        }
    }

    if (!g_DumpedClientPiTableAtRuntime && (state20 == 2u || state20 == 3u)) {
        DiagnosticLogClientBlIlCorpusObject();
        DiagnosticLogClientPiTable();
        g_DumpedClientPiTableAtRuntime = true;
    }

    g_LastClientShellObserved = clientShell;
    g_LastClientShellState20 = state20;
    g_LastClientShellRuntimeObjectD0 = runtimeObjectD0;
    g_LastClientShellRuntimeVftableD0 = runtimeVftableD0;
}


// Stub for removed D3D diagnostics
static void LogCurrentDisplayMode(const char* prefix) {
    (void)prefix;
}

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
        if (std::strcmp(title, "D3D Error") == 0) {
            // Removed D3D error logging
        }
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
    LogCurrentDisplayMode("WindowTrace display mode");

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
                LogCurrentDisplayMode("WindowTrace display mode");
            }
        }

        int count = 0;
        EnumWindows(WindowTraceEnumProc, reinterpret_cast<LPARAM>(&count));
        if (count != g_LastWindowTraceCount) {
            g_LastWindowTraceCount = count;
            spdlog::info("WindowTrace top-level window count: {}", count);
        }
        DiagnosticLogClientShellRuntimeTransitionState();

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
