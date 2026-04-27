#include "diagnostics.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <string>

static std::string g_LastClientLoadingStateText;
static std::string g_LastClientLoadingStateSource;

static bool g_ClientLoadingTextHookEnvChecked = false;
static bool g_ClientLoadingTextHookRequested = false;
static constexpr uintptr_t kClientDllImageBase = 0x62000000u;
static constexpr uintptr_t kClientLoadingTextFunctionAbsolute = 0x6215b930u;
static constexpr uintptr_t kClientLoadingTextFunctionRva =
    kClientLoadingTextFunctionAbsolute - kClientDllImageBase;
static constexpr size_t kClientLoadingTextHookPatchSize = 6u;
static constexpr uint8_t kExpectedClientLoadingTextHookPrologue[kClientLoadingTextHookPatchSize] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c,
};

static_assert(sizeof(void*) == 4, "client loading-text diagnostic hook assumes x86/32-bit build");

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

static const char* DiagnosticClientLoadingTextHookDispatch(const char* text, void* statusSink) {
    if (!statusSink) {
        return text;
    }

    const char* visibleText = (text && text[0]) ? text : "<empty>";
    static const struct {
        const char* original;
        const char* replacement;
    } kReplacements[] = {
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

    for (const auto& entry : kReplacements) {
        if (std::strcmp(visibleText, entry.original) == 0) {
            spdlog::info(
                "DIAGNOSTIC: replacing client loading text original='{}' replacement='{}'",
                visibleText,
                entry.replacement);
            DiagnosticLogClientLoadingStateText(
                entry.replacement,
                "client.dll:FUN_6215b930 replacement");
            return entry.replacement;
        }
    }

    DiagnosticLogClientLoadingStateText(
        visibleText,
        "client.dll:FUN_6215b930 exact");
    return visibleText;
}

bool DiagnosticMaybeInstallClientLoadingTextHook(HMODULE clientModule) {
    if (!g_ClientLoadingTextHookEnvChecked) {
        g_ClientLoadingTextHookRequested =
            !DiagnosticEnvFlagEnabled("MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK");
        g_ClientLoadingTextHookEnvChecked = true;
        spdlog::info(
            g_ClientLoadingTextHookRequested
                ? "DIAGNOSTIC: exact client loading-text hook enabled by default"
                : "DIAGNOSTIC: MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK disabled exact client loading-text hook");
    }
    if (!g_ClientLoadingTextHookRequested) {
        return false;
    }
    if (!clientModule) {
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook requested but client.dll is null");
        return false;
    }
    uint8_t* const target = reinterpret_cast<uint8_t*>(clientModule) + kClientLoadingTextFunctionRva;
    if (std::memcmp(target, kExpectedClientLoadingTextHookPrologue, kClientLoadingTextHookPatchSize) != 0) {
        spdlog::warn(
            "DIAGNOSTIC: exact client loading-text hook aborted because client.dll:{} prologue no longer matches expected bytes",
            fmt::ptr(target));
        return false;
    }

    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);
    const uintptr_t allocationGranularity =
        systemInfo.dwAllocationGranularity != 0u
            ? static_cast<uintptr_t>(systemInfo.dwAllocationGranularity)
            : static_cast<uintptr_t>(0x10000u);
    const uintptr_t alignedBase =
        reinterpret_cast<uintptr_t>(target) & ~(allocationGranularity - 1u);

    auto allocateNear = [&](size_t byteCount) -> uint8_t* {
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
    };

    auto writeRelativeJump = [](uint8_t* patchLocation, const void* destination) {
        const intptr_t displacement =
            reinterpret_cast<const uint8_t*>(destination) - (patchLocation + 5u);
        const int32_t relativeDisplacement = static_cast<int32_t>(displacement);
        patchLocation[0] = 0xe9;
        std::memcpy(patchLocation + 1u, &relativeDisplacement, sizeof(relativeDisplacement));
    };

    uint8_t* trampoline = allocateNear(kClientLoadingTextHookPatchSize + 5u);
    if (!trampoline) {
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook failed to allocate trampoline");
        return false;
    }
    std::memcpy(trampoline, target, kClientLoadingTextHookPatchSize);
    writeRelativeJump(trampoline + kClientLoadingTextHookPatchSize, target + kClientLoadingTextHookPatchSize);

    uint8_t* stub = allocateNear(64u);
    if (!stub) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        spdlog::warn("DIAGNOSTIC: exact client loading-text hook failed to allocate detour stub");
        return false;
    }

    uint8_t* cursor = stub;
    *cursor++ = 0x9c; // pushfd
    *cursor++ = 0x60; // pushad

    const uint8_t captureTextAndSink[] = {
        0x8b, 0x44, 0x24, 0x28,
        0x8b, 0x54, 0x24, 0x04,
        0x85, 0xd2,
        0x74, 0x10,
        0x52,
        0x50,
        0xb8,
    };
    std::memcpy(cursor, captureTextAndSink, sizeof(captureTextAndSink));
    cursor += sizeof(captureTextAndSink);

    const uintptr_t dispatchAddress =
        reinterpret_cast<uintptr_t>(&DiagnosticClientLoadingTextHookDispatch);
    std::memcpy(cursor, &dispatchAddress, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    const uint8_t callDispatchAndRestore[] = {
        0xff, 0xd0,
        0x83, 0xc4, 0x08,
        0x89, 0x44, 0x24, 0x28,
        0x61,
        0x9d,
        0xb8,
    };
    std::memcpy(cursor, callDispatchAndRestore, sizeof(callDispatchAndRestore));
    cursor += sizeof(callDispatchAndRestore);

    const uintptr_t trampolineAddress = reinterpret_cast<uintptr_t>(trampoline);
    std::memcpy(cursor, &trampolineAddress, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    const uint8_t jumpToTrampoline[] = {0xff, 0xe0};
    std::memcpy(cursor, jumpToTrampoline, sizeof(jumpToTrampoline));
    cursor += sizeof(jumpToTrampoline);

    FlushInstructionCache(GetCurrentProcess(), stub, static_cast<SIZE_T>(cursor - stub));

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, kClientLoadingTextHookPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        spdlog::warn(
            "DIAGNOSTIC: exact client loading-text hook failed to change page protection ({})",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }

    writeRelativeJump(target, stub);
    if (kClientLoadingTextHookPatchSize > 5u) {
        std::memset(target + 5u, 0x90, kClientLoadingTextHookPatchSize - 5u);
    }
    FlushInstructionCache(GetCurrentProcess(), target, kClientLoadingTextHookPatchSize);

    DWORD restoreProtect = 0;
    (void)VirtualProtect(target, kClientLoadingTextHookPatchSize, oldProtect, &restoreProtect);

    spdlog::info(
        "DIAGNOSTIC: installed exact client loading-text hook target={} rva=0x{:06x} stub={} trampoline={}",
        fmt::ptr(target),
        static_cast<unsigned>(kClientLoadingTextFunctionRva),
        fmt::ptr(stub),
        fmt::ptr(trampoline));
    return true;
}
