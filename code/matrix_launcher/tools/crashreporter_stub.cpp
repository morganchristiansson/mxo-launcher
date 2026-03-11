#include <windows.h>
#include <cstdio>

static void AppendLogLine(const char* line) {
    FILE* f = std::fopen("crashreporter_stub.log", "a");
    if (!f) return;
    std::fprintf(f, "%s\n", line);
    std::fclose(f);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char header[256] = {0};
    std::snprintf(
        header,
        sizeof(header),
        "%04u-%02u-%02u %02u:%02u:%02u crashreporter stub invoked",
        (unsigned)st.wYear,
        (unsigned)st.wMonth,
        (unsigned)st.wDay,
        (unsigned)st.wHour,
        (unsigned)st.wMinute,
        (unsigned)st.wSecond);
    AppendLogLine(header);

    const char* cmdline = GetCommandLineA();
    if (cmdline && cmdline[0]) {
        FILE* f = std::fopen("crashreporter_stub.log", "a");
        if (f) {
            std::fprintf(f, "cmdline: %s\n\n", cmdline);
            std::fclose(f);
        }
    }

    return 0;
}
