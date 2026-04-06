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

struct DiagnosticD3DShaderMacro {
    LPCSTR Name;
    LPCSTR Definition;
};

using DiagnosticD3DCompileFunc = HRESULT(WINAPI*)(
    LPCVOID,
    SIZE_T,
    LPCSTR,
    const DiagnosticD3DShaderMacro*,
    void*,
    LPCSTR,
    LPCSTR,
    UINT,
    UINT,
    void**,
    void**);

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
static DiagnosticD3DCompileFunc g_OriginalD3DCompile = nullptr;
static std::set<std::string> g_DumpedShaderSourcePaths;

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

static bool DiagnosticEnsureParentDirectoriesForFile(const char* path) {
    if (!path || !path[0]) {
        return false;
    }

    std::string mutablePath(path);
    for (size_t i = 3; i < mutablePath.size(); ++i) {
        if (mutablePath[i] != '\\' && mutablePath[i] != '/') {
            continue;
        }
        const char saved = mutablePath[i];
        mutablePath[i] = '\0';
        if (mutablePath.size() > 3) {
            const DWORD attrs = GetFileAttributesA(mutablePath.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                (void)CreateDirectoryA(mutablePath.c_str(), NULL);
            }
        }
        mutablePath[i] = saved;
    }
    return true;
}

static void DiagnosticDumpShaderSourceFile(const char* path, const void* sourceData, size_t sourceBytes) {
    if (!path || !path[0] || !sourceData || sourceBytes == 0) {
        return;
    }
    if (!g_DumpedShaderSourcePaths.insert(path).second) {
        return;
    }

    DiagnosticEnsureParentDirectoriesForFile(path);
    FILE* file = std::fopen(path, "wb");
    if (!file) {
        spdlog::info("DiagnosticD3DCompile dump path='{}' openFailed=1", path);
        return;
    }
    const size_t written = std::fwrite(sourceData, 1, sourceBytes, file);
    std::fclose(file);
    spdlog::info(
        "DiagnosticD3DCompile dump path='{}' bytes={} written={}",
        path,
        static_cast<unsigned long long>(sourceBytes),
        static_cast<unsigned long long>(written));
}

static void DiagnosticReleaseD3DBlob(void* blob) {
    if (!blob) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(blob);
    if (!vtable || !vtable[2]) {
        return;
    }
    using ReleaseFn = ULONG(__stdcall*)(void*);
    ReleaseFn releaseFn = reinterpret_cast<ReleaseFn>(vtable[2]);
    (void)releaseFn(blob);
}

static bool DiagnosticShaderSourceLooksLikeMissingPositionInput(
    const std::string& sourceText,
    std::string* outPatchedText) {
    if (outPatchedText) {
        outPatchedText->clear();
    }
    if (sourceText.find("matrixTransformNoBones(input.pos,normal);") == std::string::npos) {
        return false;
    }
    if (sourceText.find("pos : POSITION") != std::string::npos) {
        return false;
    }

    const size_t structStart = sourceText.find("struct VS_INPUT {");
    if (structStart == std::string::npos) {
        return false;
    }
    const size_t bodyStart = sourceText.find('{', structStart);
    if (bodyStart == std::string::npos) {
        return false;
    }
    const size_t structEnd = sourceText.find("};", bodyStart);
    if (structEnd == std::string::npos) {
        return false;
    }

    const std::string body = sourceText.substr(bodyStart + 1, structEnd - (bodyStart + 1));
    for (char ch : body) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return false;
        }
    }

    if (!outPatchedText) {
        return true;
    }

    *outPatchedText = sourceText;
    outPatchedText->replace(
        structStart,
        structEnd + 2 - structStart,
        "struct VS_INPUT {\nfloat3 pos : POSITION;\n};");
    return true;
}

static HRESULT WINAPI DiagnosticD3DCompile(
    LPCVOID sourceData,
    SIZE_T sourceBytes,
    LPCSTR sourceName,
    const DiagnosticD3DShaderMacro* defines,
    void* includeHandler,
    LPCSTR entryPoint,
    LPCSTR target,
    UINT flags1,
    UINT flags2,
    void** codeBlob,
    void** errorBlob) {
    if (!g_OriginalD3DCompile) {
        return E_FAIL;
    }

    if (sourceName && std::strstr(sourceName, "\\Shaders\\") != nullptr &&
        std::strstr(sourceName, ".fx") != nullptr) {
        DiagnosticDumpShaderSourceFile(sourceName, sourceData, static_cast<size_t>(sourceBytes));
    }

    const HRESULT result = g_OriginalD3DCompile(
        sourceData,
        sourceBytes,
        sourceName,
        defines,
        includeHandler,
        entryPoint,
        target,
        flags1,
        flags2,
        codeBlob,
        errorBlob);
    if (FAILED(result)) {
        spdlog::info(
            "DiagnosticD3DCompile source='{}' entry='{}' target='{}' bytes={} hr=0x{:08x}",
            sourceName ? sourceName : "<null>",
            entryPoint ? entryPoint : "<null>",
            target ? target : "<null>",
            static_cast<unsigned long long>(sourceBytes),
            static_cast<unsigned>(result));

        std::string patchedSource;
        const std::string sourceText(
            static_cast<const char*>(sourceData),
            static_cast<size_t>(sourceBytes));
        if (target && std::strncmp(target, "vs_", 3) == 0 &&
            DiagnosticShaderSourceLooksLikeMissingPositionInput(sourceText, &patchedSource)) {
            std::string patchedPath = sourceName ? sourceName : "shader_patched.fx";
            const size_t extPos = patchedPath.rfind(".fx");
            if (extPos != std::string::npos) {
                patchedPath.insert(extPos, ".patched");
            } else {
                patchedPath += ".patched.fx";
            }
            DiagnosticDumpShaderSourceFile(
                patchedPath.c_str(),
                patchedSource.data(),
                patchedSource.size());
            spdlog::info(
                "DiagnosticD3DCompile retrying with synthesized POSITION input source='{}' patched='{}'",
                sourceName ? sourceName : "<null>",
                patchedPath);

            if (errorBlob && *errorBlob) {
                DiagnosticReleaseD3DBlob(*errorBlob);
                *errorBlob = nullptr;
            }
            if (codeBlob && *codeBlob) {
                DiagnosticReleaseD3DBlob(*codeBlob);
                *codeBlob = nullptr;
            }

            const HRESULT retryResult = g_OriginalD3DCompile(
                patchedSource.data(),
                patchedSource.size(),
                sourceName,
                defines,
                includeHandler,
                entryPoint,
                target,
                flags1,
                flags2,
                codeBlob,
                errorBlob);
            spdlog::info(
                "DiagnosticD3DCompile retry result source='{}' entry='{}' target='{}' hr=0x{:08x}",
                sourceName ? sourceName : "<null>",
                entryPoint ? entryPoint : "<null>",
                target ? target : "<null>",
                static_cast<unsigned>(retryResult));
            return retryResult;
        }
    }
    return result;
}

bool DiagnosticInstallR3d9D3DCompileHook(HMODULE r3d9Module) {
    if (!r3d9Module) {
        return false;
    }

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(r3d9Module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(r3d9Module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress == 0 || importDir.Size == 0) {
        return false;
    }

    auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDir.VirtualAddress);
    for (; importDesc->Name != 0; ++importDesc) {
        const char* dllName = reinterpret_cast<const char*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDesc->Name);
        if (_stricmp(dllName, "D3DCOMPILER_43.dll") != 0) {
            continue;
        }

        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDesc->FirstThunk);
        auto* origThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDesc->OriginalFirstThunk);
        for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                continue;
            }
            auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(reinterpret_cast<uint8_t*>(r3d9Module) + origThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(importByName->Name), "D3DCompile") != 0) {
                continue;
            }

            DWORD oldProtect = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &oldProtect)) {
                return false;
            }
            g_OriginalD3DCompile = reinterpret_cast<DiagnosticD3DCompileFunc>(static_cast<uintptr_t>(thunk->u1.Function));
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(&DiagnosticD3DCompile);
            DWORD restoreProtect = 0;
            (void)VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &restoreProtect);
            spdlog::info(
                "DiagnosticInstallR3d9D3DCompileHook installed module={} original={} replacement={}",
                fmt::ptr(r3d9Module),
                fmt::ptr(reinterpret_cast<void*>(g_OriginalD3DCompile)),
                fmt::ptr(reinterpret_cast<void*>(&DiagnosticD3DCompile)));
            return true;
        }
    }

    return false;
}

static void DiagnosticLogDirectorySnapshot(const char* label, const char* pattern) {
    if (!label || !pattern || !pattern[0]) {
        return;
    }

    WIN32_FIND_DATAA findData = {};
    HANDLE findHandle = FindFirstFileA(pattern, &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        spdlog::info("{} pattern='{}' no entries", label, pattern);
        return;
    }

    do {
        const char* type = ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? "dir" : "file";
        const unsigned long long size =
            (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) |
            static_cast<unsigned long long>(findData.nFileSizeLow);
        spdlog::info(
            "{} pattern='{}' {}='{}' size={}",
            label,
            pattern,
            type,
            findData.cFileName,
            size);
    } while (FindNextFileA(findHandle, &findData));

    FindClose(findHandle);
}

static void DiagnosticLogShaderPathIfPresent(const char* path) {
    if (!path || !path[0]) {
        return;
    }
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        spdlog::info("WindowTrace D3D Error shader aux path='{}' exists=0", path);
        return;
    }
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::fclose(file);
    spdlog::info("WindowTrace D3D Error shader aux path='{}' exists=1 size={}", path, static_cast<long long>(fileSize));
}

static void DiagnosticLogShaderFileContextFromDialogText(const char* text) {
    if (!text || !text[0]) {
        return;
    }

    std::set<std::string> seenPaths;
    const size_t textLength = std::strlen(text);
    const char* shaderHashStart = std::strstr(text, "shader '");
    if (shaderHashStart) {
        shaderHashStart += std::strlen("shader '");
        char shaderHash[16] = {0};
        size_t hashLen = 0;
        while (hashLen < 8 && std::isxdigit(static_cast<unsigned char>(shaderHashStart[hashLen]))) {
            shaderHash[hashLen] = shaderHashStart[hashLen];
            ++hashLen;
        }
        if (hashLen == 8) {
            char auxPath[512] = {0};
            std::snprintf(
                auxPath,
                sizeof(auxPath),
                "C:\\users\\morgan\\AppData\\Local\\The Matrix Online\\Shaders\\Requests\\%s.bin",
                shaderHash);
            DiagnosticLogShaderPathIfPresent(auxPath);
            std::snprintf(
                auxPath,
                sizeof(auxPath),
                "C:\\users\\morgan\\AppData\\Local\\The Matrix Online\\Shaders\\%s.vsh",
                shaderHash);
            DiagnosticLogShaderPathIfPresent(auxPath);
            std::snprintf(
                auxPath,
                sizeof(auxPath),
                "C:\\users\\morgan\\AppData\\Local\\The Matrix Online\\Shaders\\%s.psh",
                shaderHash);
            DiagnosticLogShaderPathIfPresent(auxPath);
        }
    }

    DiagnosticLogDirectorySnapshot(
        "WindowTrace D3D Error shader dir snapshot",
        "C:\\users\\morgan\\AppData\\Local\\The Matrix Online\\Shaders\\*");
    DiagnosticLogDirectorySnapshot(
        "WindowTrace D3D Error shader requests dir snapshot",
        "C:\\users\\morgan\\AppData\\Local\\The Matrix Online\\Shaders\\Requests\\*");

    for (size_t i = 0; i + 4 < textLength; ++i) {
        const char drive = text[i];
        if (!((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z')) ||
            text[i + 1] != ':' || text[i + 2] != '\\') {
            continue;
        }

        const char* pathStart = text + i;
        const char* fxSuffix = std::strstr(pathStart, ".fx");
        if (!fxSuffix) {
            continue;
        }
        const char* afterFx = fxSuffix + 3;
        std::string path(pathStart, afterFx);
        if (!seenPaths.insert(path).second) {
            continue;
        }

        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) {
            spdlog::info("WindowTrace D3D Error shader file path='{}' exists=0", path);
            continue;
        }

        std::fseek(file, 0, SEEK_END);
        const long fileSize = std::ftell(file);
        std::rewind(file);
        std::string content;
        if (fileSize > 0) {
            content.resize(static_cast<size_t>(fileSize));
            const size_t bytesRead = std::fread(content.data(), 1, content.size(), file);
            content.resize(bytesRead);
        }
        std::fclose(file);

        spdlog::info(
            "WindowTrace D3D Error shader file path='{}' exists=1 size={} bytes",
            path,
            static_cast<long long>(fileSize));

        std::vector<unsigned> interestingLines;
        const char* search = pathStart;
        while ((search = std::strstr(search, path.c_str())) != nullptr) {
            const char* afterPath = search + path.size();
            if (*afterPath == '(') {
                char* endPtr = nullptr;
                const unsigned long lineNumber = std::strtoul(afterPath + 1, &endPtr, 10);
                if (endPtr && *endPtr == ',') {
                    interestingLines.push_back(static_cast<unsigned>(lineNumber));
                }
            }
            search = afterPath;
        }

        if (interestingLines.empty()) {
            interestingLines.push_back(1u);
        }

        std::vector<std::string> lines;
        size_t cursor = 0;
        while (cursor <= content.size()) {
            size_t end = content.find('\n', cursor);
            if (end == std::string::npos) {
                end = content.size();
            }
            std::string line = content.substr(cursor, end - cursor);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
            if (end == content.size()) {
                break;
            }
            cursor = end + 1;
        }

        std::set<unsigned> emitted;
        for (unsigned lineNumber : interestingLines) {
            const unsigned begin = (lineNumber > 3u) ? (lineNumber - 3u) : 1u;
            const unsigned end = std::min<unsigned>(lineNumber + 3u, static_cast<unsigned>(lines.size()));
            for (unsigned lineIndex = begin; lineIndex <= end; ++lineIndex) {
                if (!emitted.insert(lineIndex).second) {
                    continue;
                }
                spdlog::info(
                    "WindowTrace D3D Error shader {}:{} {}",
                    path,
                    lineIndex,
                    (lineIndex >= 1u && lineIndex <= lines.size()) ? lines[lineIndex - 1u] : "<out-of-range>");
            }
        }
    }
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
    DiagnosticLogShaderFileContextFromDialogText(title);
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
            "WindowTrace D3D Error client shell state18..34",
            static_cast<const uint8_t*>(clientShell) + 0x18,
            8);
        LogWordSpanIfReadable(
            "WindowTrace D3D Error client shell d0..ec",
            static_cast<const uint8_t*>(clientShell) + 0xd0,
            8);
        const void* const d0Field = static_cast<const uint8_t*>(clientShell) + 0xd0;
        const void* currentRuntimeObject = nullptr;
        const void* currentRuntimeVftable = nullptr;
        if (DiagnosticReadableMemoryRange(d0Field, sizeof(void*))) {
            currentRuntimeObject = *static_cast<const void* const*>(d0Field);
        }
        if (currentRuntimeObject != nullptr) {
            LogWordSpanIfReadable(
                "WindowTrace D3D Error client shell +0xd0 object",
                currentRuntimeObject,
                8);
            if (DiagnosticReadableMemoryRange(currentRuntimeObject, sizeof(void*))) {
                currentRuntimeVftable = *static_cast<const void* const*>(currentRuntimeObject);
            }
            LogWordSpanIfReadable(
                "WindowTrace D3D Error client shell +0xd0 vftable",
                currentRuntimeVftable,
                8);
            if (currentRuntimeVftable == DiagnosticClientAbsoluteToPointer(0x628b1638u)) {
                const uint8_t* objectBytes = static_cast<const uint8_t*>(currentRuntimeObject);
                if (DiagnosticReadableMemoryRange(objectBytes + 0x2e4, sizeof(uint32_t))) {
                    const uint32_t field0c = *reinterpret_cast<const uint32_t*>(objectBytes + 0x0c);
                    const uint8_t flags154 = *(objectBytes + 0x154);
                    const uint8_t flag2d8 = *(objectBytes + 0x2d8);
                    const uint8_t flag2d9 = *(objectBytes + 0x2d9);
                    const uint8_t flag2da = *(objectBytes + 0x2da);
                    const uint32_t field2e4 = *reinterpret_cast<const uint32_t*>(objectBytes + 0x2e4);
                    spdlog::info(
                        "WindowTrace D3D Error CLTRemoteCommCtx fields field0c=0x{:08x} flags154=0x{:02x} flag2d8={} flag2d9={} flag2da=0x{:02x} field2e4=0x{:08x}",
                        field0c,
                        static_cast<unsigned>(flags154),
                        static_cast<unsigned>(flag2d8),
                        static_cast<unsigned>(flag2d9),
                        static_cast<unsigned>(flag2da),
                        field2e4);
                }
            }
        }
    }

    const void* const state9CallbackBlob = DiagnosticClientAbsoluteToPointer(0x629e0284u);
    LogWordSpanIfReadable("WindowTrace D3D Error state9 callback blob", state9CallbackBlob, 8);
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
