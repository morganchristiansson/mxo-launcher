#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <d3d9.h>

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

using DiagnosticDirect3DCreate9Func = IDirect3D9* (WINAPI*)(UINT);
using DiagnosticIDirect3D9CreateDeviceFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3D9*,
    UINT,
    D3DDEVTYPE,
    HWND,
    DWORD,
    D3DPRESENT_PARAMETERS*,
    IDirect3DDevice9**);
using DiagnosticIDirect3DDevice9PresentFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    const RECT*,
    const RECT*,
    HWND,
    const RGNDATA*);
using DiagnosticIDirect3DDevice9BeginSceneFunc = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using DiagnosticIDirect3DDevice9EndSceneFunc = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using DiagnosticIDirect3DDevice9SetVertexDeclarationFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    IDirect3DVertexDeclaration9*);
using DiagnosticIDirect3DDevice9SetFVFFunc = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD);
using DiagnosticIDirect3DDevice9SetStreamSourceFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    UINT,
    IDirect3DVertexBuffer9*,
    UINT,
    UINT);
using DiagnosticIDirect3DDevice9SetIndicesFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    IDirect3DIndexBuffer9*);
using DiagnosticIDirect3DDevice9DrawPrimitiveFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    D3DPRIMITIVETYPE,
    UINT,
    UINT);
using DiagnosticIDirect3DDevice9DrawIndexedPrimitiveFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    D3DPRIMITIVETYPE,
    INT,
    UINT,
    UINT,
    UINT,
    UINT);
using DiagnosticIDirect3DDevice9DrawPrimitiveUPFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    D3DPRIMITIVETYPE,
    UINT,
    const void*,
    UINT);
using DiagnosticIDirect3DDevice9DrawIndexedPrimitiveUPFunc = HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,
    D3DPRIMITIVETYPE,
    UINT,
    UINT,
    UINT,
    const void*,
    D3DFORMAT,
    const void*,
    UINT);
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
static HWND g_LastLoggedD3DErrorDialog = NULL;
static const void* g_LastClientShellObserved = nullptr;
static uint32_t g_LastClientShellState20 = 0xffffffffu;
static ULONGLONG g_LastClientShellState20ChangeTick = 0;
static uint32_t g_LastLoggedState20StallQuarterSeconds = 0;
static const void* g_LastClientShellRuntimeObjectD0 = nullptr;
static const void* g_LastClientShellRuntimeVftableD0 = nullptr;
static bool g_DumpedClientPiTableAtRuntime = false;
static DiagnosticDirect3DCreate9Func g_OriginalDirect3DCreate9 = nullptr;
static DiagnosticIDirect3D9CreateDeviceFunc g_OriginalIDirect3D9CreateDevice = nullptr;
static DiagnosticIDirect3DDevice9PresentFunc g_OriginalIDirect3DDevice9Present = nullptr;
static DiagnosticIDirect3DDevice9BeginSceneFunc g_OriginalIDirect3DDevice9BeginScene = nullptr;
static DiagnosticIDirect3DDevice9EndSceneFunc g_OriginalIDirect3DDevice9EndScene = nullptr;
static DiagnosticIDirect3DDevice9SetVertexDeclarationFunc g_OriginalIDirect3DDevice9SetVertexDeclaration = nullptr;
static DiagnosticIDirect3DDevice9SetFVFFunc g_OriginalIDirect3DDevice9SetFVF = nullptr;
static DiagnosticIDirect3DDevice9SetStreamSourceFunc g_OriginalIDirect3DDevice9SetStreamSource = nullptr;
static DiagnosticIDirect3DDevice9SetIndicesFunc g_OriginalIDirect3DDevice9SetIndices = nullptr;
static DiagnosticIDirect3DDevice9DrawPrimitiveFunc g_OriginalIDirect3DDevice9DrawPrimitive = nullptr;
static DiagnosticIDirect3DDevice9DrawIndexedPrimitiveFunc g_OriginalIDirect3DDevice9DrawIndexedPrimitive = nullptr;
static DiagnosticIDirect3DDevice9DrawPrimitiveUPFunc g_OriginalIDirect3DDevice9DrawPrimitiveUP = nullptr;
static DiagnosticIDirect3DDevice9DrawIndexedPrimitiveUPFunc g_OriginalIDirect3DDevice9DrawIndexedPrimitiveUP = nullptr;
static DiagnosticD3DCompileFunc g_OriginalD3DCompile = nullptr;
static std::set<std::string> g_DumpedShaderSourcePaths;

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

struct DiagnosticD3D9ActivityState {
    unsigned long long presentCount = 0;
    unsigned long long beginSceneCount = 0;
    unsigned long long endSceneCount = 0;
    unsigned long long drawPrimitiveCount = 0;
    unsigned long long drawIndexedPrimitiveCount = 0;
    unsigned long long drawPrimitiveUPCount = 0;
    unsigned long long drawIndexedPrimitiveUPCount = 0;
    unsigned long long lastPresentedDrawCallTotal = 0;
    DWORD currentFVF = 0;
    void* currentVertexDeclaration = nullptr;
    void* currentStream0Buffer = nullptr;
    UINT currentStream0Offset = 0;
    UINT currentStream0Stride = 0;
    void* currentIndices = nullptr;
    const char* lastDrawKind = "<none>";
    D3DPRIMITIVETYPE lastPrimitiveType = D3DPT_FORCE_DWORD;
    INT lastBaseVertexIndex = 0;
    UINT lastMinVertexIndex = 0;
    UINT lastNumVertices = 0;
    UINT lastStartIndex = 0;
    UINT lastPrimitiveCount = 0;
    UINT lastStartVertex = 0;
    UINT lastVertexStreamZeroStride = 0;
    D3DFORMAT lastIndexDataFormat = D3DFMT_UNKNOWN;
    bool warnedMissingVertexLayout = false;
    bool warnedMissingStream0 = false;
    bool warnedMissingIndices = false;
};

static DiagnosticD3D9ActivityState g_D3D9ActivityState = {};
static std::set<std::string> g_LoggedSuspiciousD3D9DrawSites;
static std::set<std::string> g_LoggedD3D9VertexLayoutSetSites;

static constexpr size_t kIDirect3D9VtableEntryCount = 17;
static constexpr size_t kIDirect3D9CreateDeviceVtableIndex = 16;
static constexpr size_t kIDirect3DDevice9VtableEntryCount = 119;
static constexpr size_t kIDirect3DDevice9PresentVtableIndex = 17;
static constexpr size_t kIDirect3DDevice9BeginSceneVtableIndex = 41;
static constexpr size_t kIDirect3DDevice9EndSceneVtableIndex = 42;
static constexpr size_t kIDirect3DDevice9DrawPrimitiveVtableIndex = 81;
static constexpr size_t kIDirect3DDevice9DrawIndexedPrimitiveVtableIndex = 82;
static constexpr size_t kIDirect3DDevice9DrawPrimitiveUPVtableIndex = 83;
static constexpr size_t kIDirect3DDevice9DrawIndexedPrimitiveUPVtableIndex = 84;
static constexpr size_t kIDirect3DDevice9SetVertexDeclarationVtableIndex = 87;
static constexpr size_t kIDirect3DDevice9SetFVFVtableIndex = 89;
static constexpr size_t kIDirect3DDevice9SetStreamSourceVtableIndex = 100;
static constexpr size_t kIDirect3DDevice9SetIndicesVtableIndex = 104;

static bool DiagnosticShouldLogFrameOrdinal(unsigned long long frameOrdinal) {
    return frameOrdinal <= 8ull || ((frameOrdinal & (frameOrdinal - 1ull)) == 0ull) || (frameOrdinal % 60ull) == 0ull;
}

static const char* DiagnosticDescribePrimitiveType(D3DPRIMITIVETYPE primitiveType) {
    switch (primitiveType) {
        case D3DPT_POINTLIST: return "POINTLIST";
        case D3DPT_LINELIST: return "LINELIST";
        case D3DPT_LINESTRIP: return "LINESTRIP";
        case D3DPT_TRIANGLELIST: return "TRIANGLELIST";
        case D3DPT_TRIANGLESTRIP: return "TRIANGLESTRIP";
        case D3DPT_TRIANGLEFAN: return "TRIANGLEFAN";
        default: return "<unknown>";
    }
}

void DiagnosticLogLastD3DDeviceActivity() {
    spdlog::info(
        "D3D9 activity presentCount={} beginSceneCount={} endSceneCount={} drawPrimitiveCount={} drawIndexedPrimitiveCount={} drawPrimitiveUPCount={} drawIndexedPrimitiveUPCount={} lastPresentedDrawCallTotal={}",
        g_D3D9ActivityState.presentCount,
        g_D3D9ActivityState.beginSceneCount,
        g_D3D9ActivityState.endSceneCount,
        g_D3D9ActivityState.drawPrimitiveCount,
        g_D3D9ActivityState.drawIndexedPrimitiveCount,
        g_D3D9ActivityState.drawPrimitiveUPCount,
        g_D3D9ActivityState.drawIndexedPrimitiveUPCount,
        g_D3D9ActivityState.lastPresentedDrawCallTotal);
    spdlog::info(
        "D3D9 activity layout currentFVF=0x{:08x} currentVertexDeclaration={} stream0Buffer={} stream0Offset={} stream0Stride={} indices={} lastDrawKind={} lastPrimitiveType={} lastPrimitiveCount={} lastStartVertex={} lastBaseVertexIndex={} lastMinVertexIndex={} lastNumVertices={} lastStartIndex={} lastIndexFormat={}",
        static_cast<unsigned>(g_D3D9ActivityState.currentFVF),
        fmt::ptr(g_D3D9ActivityState.currentVertexDeclaration),
        fmt::ptr(g_D3D9ActivityState.currentStream0Buffer),
        g_D3D9ActivityState.currentStream0Offset,
        g_D3D9ActivityState.currentStream0Stride,
        fmt::ptr(g_D3D9ActivityState.currentIndices),
        g_D3D9ActivityState.lastDrawKind,
        DiagnosticDescribePrimitiveType(g_D3D9ActivityState.lastPrimitiveType),
        g_D3D9ActivityState.lastPrimitiveCount,
        g_D3D9ActivityState.lastStartVertex,
        g_D3D9ActivityState.lastBaseVertexIndex,
        g_D3D9ActivityState.lastMinVertexIndex,
        g_D3D9ActivityState.lastNumVertices,
        g_D3D9ActivityState.lastStartIndex,
        static_cast<unsigned>(g_D3D9ActivityState.lastIndexDataFormat));
}

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
    if (body.find(": POSITION") != std::string::npos || body.find(":POSITION") != std::string::npos) {
        return false;
    }
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

static void DiagnosticLogSuspiciousD3D9DrawState(
    const char* drawKind,
    bool indexedDraw,
    void* returnAddress) {
    const uintptr_t clientAbsolute = DiagnosticPointerToClientAbsolute(returnAddress);
    const std::string siteKey = std::string(drawKind ? drawKind : "<null>") + "|" +
        std::to_string(reinterpret_cast<uintptr_t>(returnAddress));
    if (g_LoggedSuspiciousD3D9DrawSites.insert(siteKey).second) {
        spdlog::warn(
            "D3D9 suspicious draw caller={} module={} clientAbsolute={} kind={} primitiveType={} primitiveCount={} stream0={} stride={} indices={} fvf=0x{:08x} vertexDecl={}",
            fmt::ptr(returnAddress),
            DiagnosticDescribeModuleForAddress(returnAddress),
            clientAbsolute ? fmt::format("0x{:08x}", static_cast<unsigned>(clientAbsolute)) : std::string("<non-client>"),
            drawKind ? drawKind : "<null>",
            DiagnosticDescribePrimitiveType(g_D3D9ActivityState.lastPrimitiveType),
            g_D3D9ActivityState.lastPrimitiveCount,
            fmt::ptr(g_D3D9ActivityState.currentStream0Buffer),
            g_D3D9ActivityState.currentStream0Stride,
            fmt::ptr(g_D3D9ActivityState.currentIndices),
            static_cast<unsigned>(g_D3D9ActivityState.currentFVF),
            fmt::ptr(g_D3D9ActivityState.currentVertexDeclaration));
    }
    if (!g_D3D9ActivityState.warnedMissingVertexLayout &&
        g_D3D9ActivityState.currentVertexDeclaration == nullptr &&
        g_D3D9ActivityState.currentFVF == 0u) {
        g_D3D9ActivityState.warnedMissingVertexLayout = true;
        spdlog::warn(
            "D3D9 suspicious draw state kind={} vertexDecl=<null> fvf=0x00000000",
            drawKind ? drawKind : "<null>");
    }
    if (!g_D3D9ActivityState.warnedMissingStream0 &&
        g_D3D9ActivityState.currentStream0Buffer == nullptr) {
        g_D3D9ActivityState.warnedMissingStream0 = true;
        spdlog::warn(
            "D3D9 suspicious draw state kind={} stream0Buffer=<null> stride={} offset={}",
            drawKind ? drawKind : "<null>",
            g_D3D9ActivityState.currentStream0Stride,
            g_D3D9ActivityState.currentStream0Offset);
    }
    if (indexedDraw && !g_D3D9ActivityState.warnedMissingIndices &&
        g_D3D9ActivityState.currentIndices == nullptr) {
        g_D3D9ActivityState.warnedMissingIndices = true;
        spdlog::warn(
            "D3D9 suspicious draw state kind={} indices=<null>",
            drawKind ? drawKind : "<null>");
    }
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9Present(
    IDirect3DDevice9* device,
    const RECT* sourceRect,
    const RECT* destRect,
    HWND destWindowOverride,
    const RGNDATA* dirtyRegion) {
    const HRESULT hr = g_OriginalIDirect3DDevice9Present
        ? g_OriginalIDirect3DDevice9Present(device, sourceRect, destRect, destWindowOverride, dirtyRegion)
        : E_FAIL;
    ++g_D3D9ActivityState.presentCount;
    g_D3D9ActivityState.lastPresentedDrawCallTotal =
        g_D3D9ActivityState.drawPrimitiveCount +
        g_D3D9ActivityState.drawIndexedPrimitiveCount +
        g_D3D9ActivityState.drawPrimitiveUPCount +
        g_D3D9ActivityState.drawIndexedPrimitiveUPCount;
    if (DiagnosticShouldLogFrameOrdinal(g_D3D9ActivityState.presentCount)) {
        spdlog::info(
            "D3D9 Present frame={} hr=0x{:08x} totalDraws={} beginSceneCount={} endSceneCount={} lastDrawKind={} lastPrimitiveType={} lastPrimitiveCount={}",
            g_D3D9ActivityState.presentCount,
            static_cast<unsigned>(hr),
            g_D3D9ActivityState.lastPresentedDrawCallTotal,
            g_D3D9ActivityState.beginSceneCount,
            g_D3D9ActivityState.endSceneCount,
            g_D3D9ActivityState.lastDrawKind,
            DiagnosticDescribePrimitiveType(g_D3D9ActivityState.lastPrimitiveType),
            g_D3D9ActivityState.lastPrimitiveCount);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9BeginScene(IDirect3DDevice9* device) {
    ++g_D3D9ActivityState.beginSceneCount;
    return g_OriginalIDirect3DDevice9BeginScene
        ? g_OriginalIDirect3DDevice9BeginScene(device)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9EndScene(IDirect3DDevice9* device) {
    ++g_D3D9ActivityState.endSceneCount;
    return g_OriginalIDirect3DDevice9EndScene
        ? g_OriginalIDirect3DDevice9EndScene(device)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9SetVertexDeclaration(
    IDirect3DDevice9* device,
    IDirect3DVertexDeclaration9* declaration) {
    g_D3D9ActivityState.currentVertexDeclaration = declaration;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    const std::string siteKey = std::string("SetVertexDeclaration|") +
        std::to_string(reinterpret_cast<uintptr_t>(returnAddress)) + "|" +
        std::to_string(reinterpret_cast<uintptr_t>(declaration));
    if (g_LoggedD3D9VertexLayoutSetSites.insert(siteKey).second) {
        const uintptr_t clientAbsolute = DiagnosticPointerToClientAbsolute(returnAddress);
        spdlog::info(
            "D3D9 SetVertexDeclaration caller={} module={} clientAbsolute={} declaration={}",
            fmt::ptr(returnAddress),
            DiagnosticDescribeModuleForAddress(returnAddress),
            clientAbsolute ? fmt::format("0x{:08x}", static_cast<unsigned>(clientAbsolute)) : std::string("<non-client>"),
            fmt::ptr(declaration));
    }
    return g_OriginalIDirect3DDevice9SetVertexDeclaration
        ? g_OriginalIDirect3DDevice9SetVertexDeclaration(device, declaration)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9SetFVF(IDirect3DDevice9* device, DWORD fvf) {
    g_D3D9ActivityState.currentFVF = fvf;
    void* const returnAddress = __builtin_extract_return_addr(__builtin_return_address(0));
    const std::string siteKey = std::string("SetFVF|") +
        std::to_string(reinterpret_cast<uintptr_t>(returnAddress)) + "|" +
        std::to_string(static_cast<unsigned long long>(fvf));
    if (g_LoggedD3D9VertexLayoutSetSites.insert(siteKey).second) {
        const uintptr_t clientAbsolute = DiagnosticPointerToClientAbsolute(returnAddress);
        spdlog::info(
            "D3D9 SetFVF caller={} module={} clientAbsolute={} fvf=0x{:08x}",
            fmt::ptr(returnAddress),
            DiagnosticDescribeModuleForAddress(returnAddress),
            clientAbsolute ? fmt::format("0x{:08x}", static_cast<unsigned>(clientAbsolute)) : std::string("<non-client>"),
            static_cast<unsigned>(fvf));
    }
    return g_OriginalIDirect3DDevice9SetFVF
        ? g_OriginalIDirect3DDevice9SetFVF(device, fvf)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9SetStreamSource(
    IDirect3DDevice9* device,
    UINT streamNumber,
    IDirect3DVertexBuffer9* streamData,
    UINT offsetInBytes,
    UINT stride) {
    if (streamNumber == 0u) {
        g_D3D9ActivityState.currentStream0Buffer = streamData;
        g_D3D9ActivityState.currentStream0Offset = offsetInBytes;
        g_D3D9ActivityState.currentStream0Stride = stride;
    }
    return g_OriginalIDirect3DDevice9SetStreamSource
        ? g_OriginalIDirect3DDevice9SetStreamSource(device, streamNumber, streamData, offsetInBytes, stride)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9SetIndices(
    IDirect3DDevice9* device,
    IDirect3DIndexBuffer9* indexData) {
    g_D3D9ActivityState.currentIndices = indexData;
    return g_OriginalIDirect3DDevice9SetIndices
        ? g_OriginalIDirect3DDevice9SetIndices(device, indexData)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9DrawPrimitive(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT startVertex,
    UINT primitiveCount) {
    ++g_D3D9ActivityState.drawPrimitiveCount;
    g_D3D9ActivityState.lastDrawKind = "DrawPrimitive";
    g_D3D9ActivityState.lastPrimitiveType = primitiveType;
    g_D3D9ActivityState.lastStartVertex = startVertex;
    g_D3D9ActivityState.lastPrimitiveCount = primitiveCount;
    g_D3D9ActivityState.lastBaseVertexIndex = 0;
    g_D3D9ActivityState.lastMinVertexIndex = 0;
    g_D3D9ActivityState.lastNumVertices = 0;
    g_D3D9ActivityState.lastStartIndex = 0;
    g_D3D9ActivityState.lastIndexDataFormat = D3DFMT_UNKNOWN;
    DiagnosticLogSuspiciousD3D9DrawState(
        "DrawPrimitive",
        false,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    return g_OriginalIDirect3DDevice9DrawPrimitive
        ? g_OriginalIDirect3DDevice9DrawPrimitive(device, primitiveType, startVertex, primitiveCount)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9DrawIndexedPrimitive(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    INT baseVertexIndex,
    UINT minVertexIndex,
    UINT numVertices,
    UINT startIndex,
    UINT primitiveCount) {
    ++g_D3D9ActivityState.drawIndexedPrimitiveCount;
    g_D3D9ActivityState.lastDrawKind = "DrawIndexedPrimitive";
    g_D3D9ActivityState.lastPrimitiveType = primitiveType;
    g_D3D9ActivityState.lastBaseVertexIndex = baseVertexIndex;
    g_D3D9ActivityState.lastMinVertexIndex = minVertexIndex;
    g_D3D9ActivityState.lastNumVertices = numVertices;
    g_D3D9ActivityState.lastStartIndex = startIndex;
    g_D3D9ActivityState.lastPrimitiveCount = primitiveCount;
    g_D3D9ActivityState.lastStartVertex = 0;
    g_D3D9ActivityState.lastIndexDataFormat = D3DFMT_UNKNOWN;
    DiagnosticLogSuspiciousD3D9DrawState(
        "DrawIndexedPrimitive",
        true,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    return g_OriginalIDirect3DDevice9DrawIndexedPrimitive
        ? g_OriginalIDirect3DDevice9DrawIndexedPrimitive(
            device,
            primitiveType,
            baseVertexIndex,
            minVertexIndex,
            numVertices,
            startIndex,
            primitiveCount)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9DrawPrimitiveUP(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT primitiveCount,
    const void* vertexStreamZeroData,
    UINT vertexStreamZeroStride) {
    ++g_D3D9ActivityState.drawPrimitiveUPCount;
    g_D3D9ActivityState.lastDrawKind = "DrawPrimitiveUP";
    g_D3D9ActivityState.lastPrimitiveType = primitiveType;
    g_D3D9ActivityState.lastPrimitiveCount = primitiveCount;
    g_D3D9ActivityState.lastStartVertex = 0;
    g_D3D9ActivityState.lastBaseVertexIndex = 0;
    g_D3D9ActivityState.lastMinVertexIndex = 0;
    g_D3D9ActivityState.lastNumVertices = 0;
    g_D3D9ActivityState.lastStartIndex = 0;
    g_D3D9ActivityState.lastVertexStreamZeroStride = vertexStreamZeroStride;
    g_D3D9ActivityState.currentStream0Buffer = const_cast<void*>(vertexStreamZeroData);
    g_D3D9ActivityState.currentStream0Offset = 0u;
    g_D3D9ActivityState.currentStream0Stride = vertexStreamZeroStride;
    g_D3D9ActivityState.lastIndexDataFormat = D3DFMT_UNKNOWN;
    DiagnosticLogSuspiciousD3D9DrawState(
        "DrawPrimitiveUP",
        false,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    return g_OriginalIDirect3DDevice9DrawPrimitiveUP
        ? g_OriginalIDirect3DDevice9DrawPrimitiveUP(device, primitiveType, primitiveCount, vertexStreamZeroData, vertexStreamZeroStride)
        : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3DDevice9DrawIndexedPrimitiveUP(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT minVertexIndex,
    UINT numVertices,
    UINT primitiveCount,
    const void* indexData,
    D3DFORMAT indexDataFormat,
    const void* vertexStreamZeroData,
    UINT vertexStreamZeroStride) {
    ++g_D3D9ActivityState.drawIndexedPrimitiveUPCount;
    g_D3D9ActivityState.lastDrawKind = "DrawIndexedPrimitiveUP";
    g_D3D9ActivityState.lastPrimitiveType = primitiveType;
    g_D3D9ActivityState.lastMinVertexIndex = minVertexIndex;
    g_D3D9ActivityState.lastNumVertices = numVertices;
    g_D3D9ActivityState.lastPrimitiveCount = primitiveCount;
    g_D3D9ActivityState.lastStartIndex = 0;
    g_D3D9ActivityState.lastStartVertex = 0;
    g_D3D9ActivityState.lastBaseVertexIndex = 0;
    g_D3D9ActivityState.lastIndexDataFormat = indexDataFormat;
    g_D3D9ActivityState.currentIndices = const_cast<void*>(indexData);
    g_D3D9ActivityState.currentStream0Buffer = const_cast<void*>(vertexStreamZeroData);
    g_D3D9ActivityState.currentStream0Offset = 0u;
    g_D3D9ActivityState.currentStream0Stride = vertexStreamZeroStride;
    DiagnosticLogSuspiciousD3D9DrawState(
        "DrawIndexedPrimitiveUP",
        true,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    return g_OriginalIDirect3DDevice9DrawIndexedPrimitiveUP
        ? g_OriginalIDirect3DDevice9DrawIndexedPrimitiveUP(
            device,
            primitiveType,
            minVertexIndex,
            numVertices,
            primitiveCount,
            indexData,
            indexDataFormat,
            vertexStreamZeroData,
            vertexStreamZeroStride)
        : E_FAIL;
}

static bool DiagnosticInstallIDirect3DDevice9Hooks(IDirect3DDevice9* device) {
    if (!device) {
        return false;
    }
    void*** const vtableSlot = reinterpret_cast<void***>(device);
    if (!vtableSlot || !*vtableSlot) {
        return false;
    }
    void** const originalVtable = *vtableSlot;
    if (originalVtable[kIDirect3DDevice9PresentVtableIndex] == reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9Present)) {
        return true;
    }

    void** shadowVtable = static_cast<void**>(std::calloc(kIDirect3DDevice9VtableEntryCount, sizeof(void*)));
    if (!shadowVtable) {
        return false;
    }
    std::memcpy(shadowVtable, originalVtable, kIDirect3DDevice9VtableEntryCount * sizeof(void*));

    g_OriginalIDirect3DDevice9Present = reinterpret_cast<DiagnosticIDirect3DDevice9PresentFunc>(originalVtable[kIDirect3DDevice9PresentVtableIndex]);
    g_OriginalIDirect3DDevice9BeginScene = reinterpret_cast<DiagnosticIDirect3DDevice9BeginSceneFunc>(originalVtable[kIDirect3DDevice9BeginSceneVtableIndex]);
    g_OriginalIDirect3DDevice9EndScene = reinterpret_cast<DiagnosticIDirect3DDevice9EndSceneFunc>(originalVtable[kIDirect3DDevice9EndSceneVtableIndex]);
    g_OriginalIDirect3DDevice9SetVertexDeclaration = reinterpret_cast<DiagnosticIDirect3DDevice9SetVertexDeclarationFunc>(originalVtable[kIDirect3DDevice9SetVertexDeclarationVtableIndex]);
    g_OriginalIDirect3DDevice9SetFVF = reinterpret_cast<DiagnosticIDirect3DDevice9SetFVFFunc>(originalVtable[kIDirect3DDevice9SetFVFVtableIndex]);
    g_OriginalIDirect3DDevice9SetStreamSource = reinterpret_cast<DiagnosticIDirect3DDevice9SetStreamSourceFunc>(originalVtable[kIDirect3DDevice9SetStreamSourceVtableIndex]);
    g_OriginalIDirect3DDevice9SetIndices = reinterpret_cast<DiagnosticIDirect3DDevice9SetIndicesFunc>(originalVtable[kIDirect3DDevice9SetIndicesVtableIndex]);
    g_OriginalIDirect3DDevice9DrawPrimitive = reinterpret_cast<DiagnosticIDirect3DDevice9DrawPrimitiveFunc>(originalVtable[kIDirect3DDevice9DrawPrimitiveVtableIndex]);
    g_OriginalIDirect3DDevice9DrawIndexedPrimitive = reinterpret_cast<DiagnosticIDirect3DDevice9DrawIndexedPrimitiveFunc>(originalVtable[kIDirect3DDevice9DrawIndexedPrimitiveVtableIndex]);
    g_OriginalIDirect3DDevice9DrawPrimitiveUP = reinterpret_cast<DiagnosticIDirect3DDevice9DrawPrimitiveUPFunc>(originalVtable[kIDirect3DDevice9DrawPrimitiveUPVtableIndex]);
    g_OriginalIDirect3DDevice9DrawIndexedPrimitiveUP = reinterpret_cast<DiagnosticIDirect3DDevice9DrawIndexedPrimitiveUPFunc>(originalVtable[kIDirect3DDevice9DrawIndexedPrimitiveUPVtableIndex]);

    shadowVtable[kIDirect3DDevice9PresentVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9Present);
    shadowVtable[kIDirect3DDevice9BeginSceneVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9BeginScene);
    shadowVtable[kIDirect3DDevice9EndSceneVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9EndScene);
    shadowVtable[kIDirect3DDevice9SetVertexDeclarationVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9SetVertexDeclaration);
    shadowVtable[kIDirect3DDevice9SetFVFVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9SetFVF);
    shadowVtable[kIDirect3DDevice9SetStreamSourceVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9SetStreamSource);
    shadowVtable[kIDirect3DDevice9SetIndicesVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9SetIndices);
    shadowVtable[kIDirect3DDevice9DrawPrimitiveVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9DrawPrimitive);
    shadowVtable[kIDirect3DDevice9DrawIndexedPrimitiveVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9DrawIndexedPrimitive);
    shadowVtable[kIDirect3DDevice9DrawPrimitiveUPVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9DrawPrimitiveUP);
    shadowVtable[kIDirect3DDevice9DrawIndexedPrimitiveUPVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3DDevice9DrawIndexedPrimitiveUP);
    *vtableSlot = shadowVtable;

    spdlog::info(
        "DiagnosticInstallIDirect3DDevice9Hooks device={} originalVtable={} shadowVtable={}",
        fmt::ptr(device),
        fmt::ptr(originalVtable),
        fmt::ptr(shadowVtable));
    return true;
}

static HRESULT STDMETHODCALLTYPE DiagnosticIDirect3D9CreateDevice(
    IDirect3D9* direct3D,
    UINT adapter,
    D3DDEVTYPE deviceType,
    HWND focusWindow,
    DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* presentationParameters,
    IDirect3DDevice9** returnedDeviceInterface) {
    const HRESULT hr = g_OriginalIDirect3D9CreateDevice
        ? g_OriginalIDirect3D9CreateDevice(
            direct3D,
            adapter,
            deviceType,
            focusWindow,
            behaviorFlags,
            presentationParameters,
            returnedDeviceInterface)
        : E_FAIL;
    spdlog::info(
        "DiagnosticIDirect3D9CreateDevice hr=0x{:08x} adapter={} deviceType={} hwnd={} behaviorFlags=0x{:08x} backBuffer={}x{} format={} windowed={} returnedDevice={}",
        static_cast<unsigned>(hr),
        adapter,
        static_cast<unsigned>(deviceType),
        fmt::ptr(focusWindow),
        static_cast<unsigned>(behaviorFlags),
        presentationParameters ? presentationParameters->BackBufferWidth : 0u,
        presentationParameters ? presentationParameters->BackBufferHeight : 0u,
        presentationParameters ? static_cast<unsigned>(presentationParameters->BackBufferFormat) : 0u,
        presentationParameters ? static_cast<unsigned>(presentationParameters->Windowed) : 0u,
        (returnedDeviceInterface && *returnedDeviceInterface)
            ? fmt::ptr(*returnedDeviceInterface)
            : fmt::ptr(static_cast<void*>(nullptr)));
    if (SUCCEEDED(hr) && returnedDeviceInterface && *returnedDeviceInterface) {
        (void)DiagnosticInstallIDirect3DDevice9Hooks(*returnedDeviceInterface);
    }
    return hr;
}

static bool DiagnosticInstallIDirect3D9Hooks(IDirect3D9* direct3D) {
    if (!direct3D) {
        return false;
    }
    void*** const vtableSlot = reinterpret_cast<void***>(direct3D);
    if (!vtableSlot || !*vtableSlot) {
        return false;
    }
    void** const originalVtable = *vtableSlot;
    if (originalVtable[kIDirect3D9CreateDeviceVtableIndex] == reinterpret_cast<void*>(&DiagnosticIDirect3D9CreateDevice)) {
        return true;
    }
    void** shadowVtable = static_cast<void**>(std::calloc(kIDirect3D9VtableEntryCount, sizeof(void*)));
    if (!shadowVtable) {
        return false;
    }
    std::memcpy(shadowVtable, originalVtable, kIDirect3D9VtableEntryCount * sizeof(void*));
    g_OriginalIDirect3D9CreateDevice = reinterpret_cast<DiagnosticIDirect3D9CreateDeviceFunc>(originalVtable[kIDirect3D9CreateDeviceVtableIndex]);
    shadowVtable[kIDirect3D9CreateDeviceVtableIndex] = reinterpret_cast<void*>(&DiagnosticIDirect3D9CreateDevice);
    *vtableSlot = shadowVtable;
    spdlog::info(
        "DiagnosticInstallIDirect3D9Hooks direct3D={} originalVtable={} shadowVtable={}",
        fmt::ptr(direct3D),
        fmt::ptr(originalVtable),
        fmt::ptr(shadowVtable));
    return true;
}

static IDirect3D9* WINAPI DiagnosticDirect3DCreate9(UINT sdkVersion) {
    IDirect3D9* const direct3D = g_OriginalDirect3DCreate9
        ? g_OriginalDirect3DCreate9(sdkVersion)
        : nullptr;
    spdlog::info(
        "DiagnosticDirect3DCreate9 sdkVersion={} result={}",
        sdkVersion,
        fmt::ptr(direct3D));
    if (direct3D) {
        (void)DiagnosticInstallIDirect3D9Hooks(direct3D);
    }
    return direct3D;
}

bool DiagnosticInstallR3d9Direct3DCreate9Hook(HMODULE r3d9Module) {
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
        if (_stricmp(dllName, "d3d9.dll") != 0) {
            continue;
        }

        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDesc->FirstThunk);
        auto* origThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<uint8_t*>(r3d9Module) + importDesc->OriginalFirstThunk);
        for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                continue;
            }
            auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(reinterpret_cast<uint8_t*>(r3d9Module) + origThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(importByName->Name), "Direct3DCreate9") != 0) {
                continue;
            }

            DWORD oldProtect = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &oldProtect)) {
                return false;
            }
            g_OriginalDirect3DCreate9 = reinterpret_cast<DiagnosticDirect3DCreate9Func>(static_cast<uintptr_t>(thunk->u1.Function));
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(&DiagnosticDirect3DCreate9);
            DWORD restoreProtect = 0;
            (void)VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &restoreProtect);
            spdlog::info(
                "DiagnosticInstallR3d9Direct3DCreate9Hook installed module={} original={} replacement={}",
                fmt::ptr(r3d9Module),
                fmt::ptr(reinterpret_cast<void*>(g_OriginalDirect3DCreate9)),
                fmt::ptr(reinterpret_cast<void*>(&DiagnosticDirect3DCreate9)));
            return true;
        }
    }

    return false;
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
