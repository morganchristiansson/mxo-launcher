#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <string>

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
static bool g_KnownClientEngineInitStatusTextsLogged = false;
static HWND g_LastLoggedD3DErrorDialog = NULL;
static const void* g_LastClientShellObserved = nullptr;
static uint32_t g_LastClientShellState20 = 0xffffffffu;
static const void* g_LastClientShellRuntimeObjectD0 = nullptr;
static const void* g_LastClientShellRuntimeVftableD0 = nullptr;

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

static const void* DiagnosticClientAbsoluteToPointer(uintptr_t absoluteAddress) {
    const uint8_t* clientBase =
        reinterpret_cast<const uint8_t*>(g_hClient ? g_hClient : GetModuleHandleA("client.dll"));
    if (!clientBase || absoluteAddress < 0x62000000u) {
        return nullptr;
    }
    return clientBase + (absoluteAddress - 0x62000000u);
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

    if (clientShell == g_LastClientShellObserved &&
        state20 == g_LastClientShellState20 &&
        runtimeObjectD0 == g_LastClientShellRuntimeObjectD0 &&
        runtimeVftableD0 == g_LastClientShellRuntimeVftableD0) {
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

    g_LastClientShellObserved = clientShell;
    g_LastClientShellState20 = state20;
    g_LastClientShellRuntimeObjectD0 = runtimeObjectD0;
    g_LastClientShellRuntimeVftableD0 = runtimeVftableD0;
}

static BOOL CALLBACK D3DErrorChildEnumProc(HWND hwnd, LPARAM) {
    char className[256] = {0};
    char title[1024] = {0};
    GetClassNameA(hwnd, className, sizeof(className));
    GetWindowTextA(hwnd, title, sizeof(title));
    spdlog::info(
        "WindowTrace D3D Error child hwnd={} class='{}' text='{}'",
        fmt::ptr(hwnd),
        className,
        title);
    return TRUE;
}

static void DiagnosticLogD3DErrorContext(HWND hwnd) {
    if (!hwnd || hwnd == g_LastLoggedD3DErrorDialog) {
        return;
    }
    g_LastLoggedD3DErrorDialog = hwnd;

    spdlog::info("WindowTrace D3D Error dialog snapshot begin hwnd={}", fmt::ptr(hwnd));
    EnumChildWindows(hwnd, D3DErrorChildEnumProc, 0);

    // anchor: client.dll:0x62001180 = RunClientDLL drives the global client-shell object rooted at
    // DAT_629ddfc8; late replacement-specific D3D/shader failures appear after event 0x18, so log
    // the same client-shell runtime object field used by the later null-vcall family.
    const void* const clientShellSlot = DiagnosticClientAbsoluteToPointer(0x629e68a8u);
    LogWordSpanIfReadable("WindowTrace D3D Error DAT_629e68a8 slot", clientShellSlot, 4);

    const void* clientShell = nullptr;
    if (clientShellSlot && DiagnosticReadableMemoryRange(clientShellSlot, sizeof(void*))) {
        clientShell = *static_cast<const void* const*>(clientShellSlot);
    }
    if (clientShell != nullptr) {
        LogWordSpanIfReadable(
            "WindowTrace D3D Error client shell d0..ec",
            static_cast<const uint8_t*>(clientShell) + 0xd0,
            8);
        const void* const d0Field = static_cast<const uint8_t*>(clientShell) + 0xd0;
        const void* currentRuntimeObject = nullptr;
        if (DiagnosticReadableMemoryRange(d0Field, sizeof(void*))) {
            currentRuntimeObject = *static_cast<const void* const*>(d0Field);
        }
        if (currentRuntimeObject != nullptr) {
            LogWordSpanIfReadable(
                "WindowTrace D3D Error client shell +0xd0 object",
                currentRuntimeObject,
                8);
        }
    }
}

// UNANCHORED: diagnostic-only display-mode snapshot helper.
static void LogCurrentDisplayMode(const char* prefix) {
    DEVMODEA mode = {};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
        spdlog::info("{}: EnumDisplaySettingsA failed", prefix);
        return;
    }

    spdlog::info(
        "{}: {}x{} {}-bpp @{}Hz",
        prefix,
        (unsigned long)mode.dmPelsWidth,
        (unsigned long)mode.dmPelsHeight,
        (unsigned long)mode.dmBitsPerPel,
        (unsigned long)mode.dmDisplayFrequency);
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
            DiagnosticLogD3DErrorContext(hwnd);
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

void DiagnosticLogKnownClientEngineInitStatusTextsOnce(const char* source) {
    if (g_KnownClientEngineInitStatusTextsLogged) {
        return;
    }
    g_KnownClientEngineInitStatusTextsLogged = true;

    static const char* kKnownTexts[] = {
        "Initializing Client Data Cache",
        "Initializing Inventory Manager",
        "Initializing Shortcut Manager",
        "Initializing Game Object Manager",
        "Initializing Character Animations",
        "Initializing Rules Subsystem",
        "Initializing Animation Tables",
        "Initializing Chat Manager",
        "Initializing Abilities",
        "Initializing FX",
        "Initializing Metro World",
    };

    for (const char* text : kKnownTexts) {
        DiagnosticLogClientLoadingStateText(text, source);
    }
}
