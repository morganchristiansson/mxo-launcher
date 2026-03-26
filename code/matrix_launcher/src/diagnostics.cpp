#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <string>

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
