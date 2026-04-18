#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <conio.h>
#include <direct.h>
#include <io.h>
#include <vector>

#include <spdlog/spdlog.h>

#include "launcher_network_object_abi.h"
#include "launcher_replacement_support.h"
#include "server_config.h"
#include "../matrixstaging/game/src/launcher/launcher.h"
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"
#include "../matrixstaging/runtime/src/libltbase/launchercommandline.h"

extern mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;
extern void* g_pLauncherObject6304;
extern char g_LastWorldName[256];



// Forward declarations for helper functions defined later in this file
void WriteMatrixConsoleText(const char* text, bool appendNewline);
void WriteMatrixConsoleFormattedLine(const char* format, ...);
bool ReadInteractiveLauncherIndex(const char* prompt, uint32_t upperBoundExclusive, uint32_t* outIndex);

// Load server configurations from servers.cfg
void LoadServerConfigurations() {
    std::vector<ServerConfig> configs;

    // Try to load from game directory first
    configs = LoadServerConfigs("servers.cfg");
    spdlog::info("DIAGNOSTIC: loaded {} servers from servers.cfg", configs.size());

    // If not found, try relative path from executable
    if (configs.empty()) {
        configs = LoadServerConfigs("../servers.cfg");
        spdlog::info("DIAGNOSTIC: loaded {} servers from ../servers.cfg", configs.size());
    }

    // If still empty, use default production server
    if (configs.empty()) {
        spdlog::warn("DIAGNOSTIC: servers.cfg not found, using default production server");
        ServerConfig defaultConfig;
        defaultConfig.name = "production";
        defaultConfig.authServerDnsName = "auth.lith.thematrixonline.net";
        defaultConfig.authServerPort = 11000;
        defaultConfig.marginServerSuffix = ".lith.thematrixonline.net";
        defaultConfig.marginServerPort = 10000;
        defaultConfig.kServerPublicModulusB64 =
            "qMIfEkrXWpRr44ecWMzJHV7Hjg9bnru2PZv3NydzOZ6uab52wET+RoHhIzv+zJb3"
            "zBhmETAtsrmNnBXiW7tfqPK0xf6lb9RbvupfnfYSHO5WaEcWEi0JjQRBevg9d8ql"
            "ETo9Hrfy9PEfpeK1T2WF+xxx73chvBTB12Paa7yT+Ik=";
        defaultConfig.kServerPublicExponentB64 = "EQ==";
        configs.push_back(std::move(defaultConfig));
    }

    for (const auto& cfg : configs) {
        spdlog::info("DIAGNOSTIC: server '{}' -> {}:{}", cfg.name, cfg.authServerDnsName, cfg.authServerPort);
    }

    SetServerConfigs(std::move(configs));
}

// Interactive server selection menu
bool SelectServerInteractive() {
    const auto& configs = GetAllServerConfigs();
    spdlog::info("DIAGNOSTIC: SelectServerInteractive called with {} servers", configs.size());

    if (configs.size() <= 1) {
        // Only one server available, use it automatically
        if (!configs.empty()) {
            WriteMatrixConsoleFormattedLine(
                "Using server: %s (%s)",
                configs[0].name.c_str(),
                configs[0].authServerDnsName.c_str());
            SetSelectedServerConfig(&configs[0]);
        }
        return true;
    }

    WriteMatrixConsoleText("Select server:", true);
    for (size_t i = 0; i < configs.size(); ++i) {
        const ServerConfig& config = configs[i];
        WriteMatrixConsoleFormattedLine(
            "  [%u] %s - %s:%u",
            static_cast<unsigned>(i + 1),
            config.name.c_str(),
            config.authServerDnsName.c_str(),
            config.authServerPort);
    }

    uint32_t selectedIndex = 0;
    while (!ReadInteractiveLauncherIndex(
            "Server index: ",
            static_cast<uint32_t>(configs.size()) + 1,
            &selectedIndex) ||
           selectedIndex == 0) {
        WriteMatrixConsoleText("Invalid server index.", true);
    }

    const ServerConfig* selected = &configs[selectedIndex - 1];
    SetSelectedServerConfig(selected);
    WriteMatrixConsoleFormattedLine(
        "Selected: %s (%s)",
        selected->name.c_str(),
        selected->authServerDnsName.c_str());
    return true;
}

const char* MaskedArgValue(const char* value) {
    if (!value || !value[0]) {
        return "<empty>";
    }
    return "<provided>";
}

// anchor: launcher.exe:0x41b520
// anchor: launcher.exe:0x41b620
// anchor: launcher.exe:0x41b6c0
// Text-mode blocking wait analogue over the proven `ILTLoginMediator` observer contract.
// Keep the method semantics close to `CLTEvilBlockingLoginObserver`, while keeping one important
// negative result explicit: the original page-6 rich-edit auth path is still driven by callback
// `0x4091d0`, not by a proven `WaitForEvent(5)` caller.
class TextModeBlockingLoginObserver {
public:
    void* launcherNetworkObject04 = nullptr; // original blocking observer also keeps arg5 at +0x04
    float timeoutSeconds08 = 0.0f;           // original stores timeout seconds at +0x08
    uint32_t expectedEvent0c = 0u;           // original stores expected event at +0x0c
    uint8_t waiting10 = 0u;                  // original clears this to unblock the wait loop
    uint8_t padding11[3] = {};
    uint32_t result14 = 0u;                  // original writes `0` on success, status80 on error
    uint32_t error18 = 0u;                   // original stores OnLoginError(errorNumber) here
    bool registered1c = false;

    virtual void OnLoginEvent(uint32_t eventId) {
        if (eventId == expectedEvent0c) {
            result14 = 0u;
            waiting10 = 0u;
        }
    }

    virtual void OnLoginError(uint32_t errorId) {
        error18 = errorId;
        result14 = mxo::ltlogin::ILTLoginMediator::Default->GetLastLoginStatus();
        waiting10 = 0u;
    }

    bool RegisterForExpectedEvent(void* launcherNetworkObject, uint32_t expectedEvent) {
        launcherNetworkObject04 = launcherNetworkObject;
        expectedEvent0c = expectedEvent;
        waiting10 = 1u;
        result14 = 0u;
        error18 = 0u;
        registered1c = true;
        (void)mxo::ltlogin::ILTLoginMediator::Default->RegisterLoginObserver(this);
        return true;
    }

    void Unregister() {
        if (!registered1c) {
            return;
        }
        registered1c = false;
        (void)mxo::ltlogin::ILTLoginMediator::Default->UnregisterLoginObserver(this);
    }

    uint32_t WaitUntilDone(DWORD timeoutMs) {
        timeoutSeconds08 = static_cast<float>(timeoutMs) / 1000.0f;
        const DWORD startTick = GetTickCount();
        while (waiting10 != 0u) {
            if ((GetTickCount() - startTick) >= timeoutMs) {
                result14 = 0x12000003u;
                waiting10 = 0u;
                break;
            }
            mxo::ltlogin::ILTLoginMediator::Default->HelperSlot13c_InvokeSessionHelperVtable4();
            LauncherPumpNetworkEngineAbiShell(launcherNetworkObject04, /*nonBlocking=*/true);
            Sleep(10u);
        }
        Unregister();
        return result14;
    }

    bool SawError() const {
        return error18 != 0u;
    }
};

struct TextModeLauncherSelectionRow {
    uint32_t descriptorIndexLowWord = 0u;   // `0x40e480` packed item-data low word
    uint32_t selectionIndexHighWord = 0xffffu; // `0x40e480` packed item-data high word
    uint32_t descriptorStatus = 0u;         // `+0x100` consumer in `0x40d530/0x40d6f0`
    uint32_t slotRecordStatus = 7u;         // `+0xe4` consumer in `0x40d530/0x40d6f0`
    char worldName[256] = {};
    char selectionName[256] = {};

    bool IsCreatePlaceholder() const {
        return (selectionIndexHighWord & 0xffffu) == 0xffffu;
    }
};

constexpr WORD kMatrixConsoleGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
static constexpr uint32_t kLauncherSelectionCreatePlaceholderThreshold = 3u;
static constexpr uint32_t kLauncherSelectionCreateHighWord = 0xffffu;

static void CopyTextModeBuffer(char* outText, size_t outCapacity, const char* sourceText) {
    if (!outText || outCapacity == 0u) {
        return;
    }
    outText[0] = '\0';
    if (!sourceText || !sourceText[0]) {
        return;
    }
    std::strncpy(outText, sourceText, outCapacity - 1u);
    outText[outCapacity - 1u] = '\0';
}

// anchor: launcher.exe:0x40d530
static bool LauncherSelectionList_RowAllowsPrimaryAction(uint32_t descriptorStatus) {
    if (descriptorStatus == 1u) {
        return true;
    }
    if (descriptorStatus == 2u || descriptorStatus == 5u) {
        return mxo::ltlogin::ILTLoginMediator::Default->HasBootstrapRaw08AuxHandle54();
    }
    return false;
}

// anchor: launcher.exe:0x40d530
static bool LauncherSelectionList_RowResolvesThroughCommand8(const TextModeLauncherSelectionRow& row) {
    return LauncherSelectionList_RowAllowsPrimaryAction(row.descriptorStatus) &&
           (row.slotRecordStatus == 0u || row.slotRecordStatus == 7u);
}

// anchor: launcher.exe:0x40e480
// Text-mode page-7 list builder over the same `ILTLoginMediator.Default` slot family used by the
// original launcher selection list:
// - `+0xf8/+0xfc` enumerate total world descriptors
// - `+0xd8/+0xdc/+0xe0/+0xe4` enumerate active selection entries / slot records
// - matching rows pack low word = descriptor index, high word = active entry / slot-record index
// - when fewer than 3 active entries match a world, insert `"- - -"` with high word `0xffff`
static bool BuildTextModeSelectionRows(std::vector<TextModeLauncherSelectionRow>* outRows) {
    if (!outRows) {
        return false;
    }
    outRows->clear();

    const uint32_t totalWorldDescriptorCount = mxo::ltlogin::ILTLoginMediator::Default->GetWorldCount();
    const uint32_t activeSelectionEntryCount =
        mxo::ltlogin::ILTLoginMediator::Default->GetArg7SelectionUpperBoundExclusive();

    for (uint32_t descriptorIndex = 0u; descriptorIndex < totalWorldDescriptorCount; ++descriptorIndex) {
        const char* worldName = mxo::ltlogin::ILTLoginMediator::Default->GetWorldNameByIndex(descriptorIndex);
        if (!worldName || !worldName[0]) {
            continue;
        }

        const uint32_t descriptorStatus =
            mxo::ltlogin::ILTLoginMediator::Default->GetWorldSelectionGateByteByIndex(descriptorIndex);
        uint32_t matchingActiveEntryCount = 0u;

        for (uint32_t activeEntryIndex = 0u; activeEntryIndex < activeSelectionEntryCount; ++activeEntryIndex) {
            const char* activeWorldName =
                mxo::ltlogin::ILTLoginMediator::Default->GetVariantWorldName(activeEntryIndex);
            if (!activeWorldName || _stricmp(worldName, activeWorldName) != 0) {
                continue;
            }

            TextModeLauncherSelectionRow row = {};
            row.descriptorIndexLowWord = descriptorIndex & 0xffffu;
            row.selectionIndexHighWord = activeEntryIndex & 0xffffu;
            row.descriptorStatus = descriptorStatus;
            row.slotRecordStatus =
                mxo::ltlogin::ILTLoginMediator::Default->GetVariantState(static_cast<int32_t>(activeEntryIndex));
            CopyTextModeBuffer(row.worldName, sizeof(row.worldName), worldName);
            CopyTextModeBuffer(
                row.selectionName,
                sizeof(row.selectionName),
                mxo::ltlogin::ILTLoginMediator::Default->MapSelectionName(activeEntryIndex));
            outRows->push_back(row);
            ++matchingActiveEntryCount;
        }

        if (matchingActiveEntryCount < kLauncherSelectionCreatePlaceholderThreshold) {
            TextModeLauncherSelectionRow row = {};
            row.descriptorIndexLowWord = descriptorIndex & 0xffffu;
            row.selectionIndexHighWord = kLauncherSelectionCreateHighWord;
            row.descriptorStatus = descriptorStatus;
            row.slotRecordStatus =
                mxo::ltlogin::ILTLoginMediator::Default->GetVariantState(-1);
            CopyTextModeBuffer(row.worldName, sizeof(row.worldName), worldName);
            CopyTextModeBuffer(row.selectionName, sizeof(row.selectionName), "- - -");
            outRows->push_back(row);
        }
    }

    return true;
}

// anchor: launcher.exe:0x40d6f0
static bool LauncherSelectionList_ResolveSelectionFromRow(
    const TextModeLauncherSelectionRow& row,
    uint32_t* outFieldA8,
    uint32_t* outFieldAC,
    char* outWorldName,
    size_t outWorldNameCapacity) {
    if (!outFieldA8 || !outFieldAC) {
        return false;
    }

    const uint32_t descriptorIndex = row.descriptorIndexLowWord & 0xffffu;
    const uint32_t selectionIndexHighWord = row.selectionIndexHighWord & 0xffffu;
    const int32_t signedSelectionIndex = static_cast<int16_t>(selectionIndexHighWord);
    const uint32_t descriptorStatus =
        mxo::ltlogin::ILTLoginMediator::Default->GetWorldSelectionGateByteByIndex(descriptorIndex);
    const uint32_t slotRecordStatus =
        mxo::ltlogin::ILTLoginMediator::Default->GetVariantState(signedSelectionIndex);
    const char* worldName = mxo::ltlogin::ILTLoginMediator::Default->GetWorldNameByIndex(descriptorIndex);

    if (!worldName || !LauncherSelectionList_RowAllowsPrimaryAction(descriptorStatus) ||
        (slotRecordStatus != 0u && slotRecordStatus != 7u)) {
        return false;
    }

    *outFieldA8 = static_cast<uint32_t>(signedSelectionIndex);
    *outFieldAC = descriptorIndex & 0x00ffffffu;
    CopyTextModeBuffer(outWorldName, outWorldNameCapacity, worldName);
    return true;
}

void WriteMatrixConsoleText(const char* text, bool appendNewline) {
    const char* safeText = text ? text : "<null>";
    HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO consoleInfo = {};
    const bool haveConsoleInfo =
        errorHandle != INVALID_HANDLE_VALUE &&
        errorHandle != nullptr &&
        GetConsoleScreenBufferInfo(errorHandle, &consoleInfo) != 0;

    if (haveConsoleInfo) {
        SetConsoleTextAttribute(errorHandle, kMatrixConsoleGreen);
        std::fputs(safeText, stderr);
        if (appendNewline) {
            std::fputc('\n', stderr);
        }
        std::fflush(stderr);
        SetConsoleTextAttribute(errorHandle, consoleInfo.wAttributes);
        return;
    }

    std::fputs("\x1b[92m", stderr);
    std::fputs(safeText, stderr);
    if (appendNewline) {
        std::fputc('\n', stderr);
    }
    std::fputs("\x1b[0m", stderr);
    std::fflush(stderr);
}

void WriteMatrixConsoleFormattedLine(const char* format, ...) {
    if (!format) {
        return;
    }

    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    WriteMatrixConsoleText(buffer, true);
}

bool ReadInteractiveLauncherField(const char* prompt, char* buffer, size_t bufferSize) {
    if (!prompt || !buffer || bufferSize < 2u) {
        return false;
    }

    WriteMatrixConsoleText(prompt, false);

    if (!std::fgets(buffer, static_cast<int>(bufferSize), stdin)) {
        buffer[0] = '\0';
        return false;
    }

    size_t length = std::strlen(buffer);
    while (length != 0u && (buffer[length - 1u] == '\n' || buffer[length - 1u] == '\r')) {
        buffer[--length] = '\0';
    }
    return true;
}

bool ReadInteractiveLauncherPasswordField(const char* prompt, char* buffer, size_t bufferSize) {
    if (!prompt || !buffer || bufferSize < 2u) {
        return false;
    }

    buffer[0] = '\0';
    WriteMatrixConsoleText(prompt, false);

    HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD consoleMode = 0u;
    const bool haveConsoleInput =
        inputHandle != INVALID_HANDLE_VALUE &&
        inputHandle != nullptr &&
        GetConsoleMode(inputHandle, &consoleMode) != 0;

    if (!haveConsoleInput) {
        if (!std::fgets(buffer, static_cast<int>(bufferSize), stdin)) {
            buffer[0] = '\0';
            return false;
        }
        size_t length = std::strlen(buffer);
        while (length != 0u && (buffer[length - 1u] == '\n' || buffer[length - 1u] == '\r')) {
            buffer[--length] = '\0';
        }
        return true;
    }

    size_t write = 0u;
    while (true) {
        const int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            std::fputc('\n', stderr);
            std::fflush(stderr);
            buffer[write] = '\0';
            return true;
        }
        if ((ch == '\b' || ch == 127) && write != 0u) {
            --write;
            continue;
        }
        if (ch == 0 || ch == 0xe0) {
            (void)_getch();
            continue;
        }
        if (write + 1u < bufferSize) {
            buffer[write++] = static_cast<char>(ch);
        }
    }
}

bool ReadInteractiveLauncherIndex(const char* prompt, uint32_t upperBoundExclusive, uint32_t* outIndex) {
    if (!prompt || !outIndex || upperBoundExclusive == 0u) {
        return false;
    }

    char inputBuffer[64] = {};
    if (!ReadInteractiveLauncherField(prompt, inputBuffer, sizeof(inputBuffer)) || inputBuffer[0] == '\0') {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(inputBuffer, &end, 10);
    if (end == inputBuffer || (end && *end != '\0') || parsed >= upperBoundExclusive) {
        return false;
    }

    *outIndex = static_cast<uint32_t>(parsed);
    return true;
}

bool DeleteLauncherProfileDirectoryForCharacter(const char* profileRootName, const char* characterName) {
    if (!profileRootName || !profileRootName[0] || !characterName || !characterName[0]) {
        return false;
    }

    char cwdBuffer[MAX_PATH] = {};
    if (!_getcwd(cwdBuffer, sizeof(cwdBuffer))) {
        return false;
    }

    const size_t cwdLength = std::strlen(cwdBuffer);
    std::snprintf(
        cwdBuffer + cwdLength,
        sizeof(cwdBuffer) - cwdLength,
        "\\Profiles\\%s\\%s",
        profileRootName,
        characterName);

    if (_access(cwdBuffer, 0) != 0) {
        return true;
    }

    SHFILEOPSTRUCTA deleteProfileDirectoryFileOp = {};
    deleteProfileDirectoryFileOp.wFunc = FO_DELETE;
    deleteProfileDirectoryFileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    deleteProfileDirectoryFileOp.pFrom = cwdBuffer;

    const size_t pathLength = std::strlen(cwdBuffer);
    cwdBuffer[pathLength + 1u] = '\0';
    return SHFileOperationA(&deleteProfileDirectoryFileOp) == 0;
}

bool PromptForLauncherCredentialsForSubmitAttempt(bool forceRepromptAllFields) {
    const bool missingUser = (g_LauncherCommandLine.AuthUsername()[0] == '\0');
    const bool missingPwd = (g_LauncherCommandLine.AuthPassword()[0] == '\0');
    const bool promptUser = forceRepromptAllFields || missingUser;
    const bool promptPwd = forceRepromptAllFields || missingPwd;
    if (!promptUser && !promptPwd) {
        return true;
    }

    WriteMatrixConsoleText("Wake up, Neo...", true);

    char inputBuffer[256] = {};
    if (promptUser) {
        if (!ReadInteractiveLauncherField("Username: ", inputBuffer, sizeof(inputBuffer)) ||
            inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive username prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthUsername(inputBuffer);
    }
    if (promptPwd) {
        if (!ReadInteractiveLauncherPasswordField("Password: ", inputBuffer, sizeof(inputBuffer)) ||
            inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive password prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthPassword(inputBuffer);
    }

    spdlog::info(
        "DIAGNOSTIC: text-mode launcher credential acquisition forceRepromptAllFields={} username={} password={}",
        forceRepromptAllFields ? 1u : 0u,
        MaskedArgValue(g_LauncherCommandLine.AuthUsername()),
        MaskedArgValue(g_LauncherCommandLine.AuthPassword()));
    return true;
}

void TrimAsciiWhitespaceInPlace(char* text) {
    if (!text) {
        return;
    }

    char* begin = text;
    while (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n') {
        ++begin;
    }
    if (begin != text) {
        std::memmove(text, begin, std::strlen(begin) + 1u);
    }

    size_t length = std::strlen(text);
    while (length != 0u) {
        const char trailing = text[length - 1u];
        if (trailing != ' ' && trailing != '\t' && trailing != '\r' && trailing != '\n') {
            break;
        }
        text[--length] = '\0';
    }
}

// anchor: launcher.exe:0x408400
// anchor: launcher.exe:0x41ecd0
// No-GUI launcher bridge over the original page-6 submit helper contract:
// - trim username
// - copy password
// - zero block40/block50
// - leave the small-string session-token field empty on the interactive username/password path
bool BuildNoGuiProcessLoginRequestInput(
    const char* username,
    const char* password,
    mxo::ltlogin::ProcessLoginRequestInputSketch* outInput) {
    if (!username || !password || !outInput) {
        return false;
    }

    *outInput = {};

    char trimmedUsername[sizeof(outInput->inlineString00)] = {};
    std::strncpy(trimmedUsername, username, sizeof(trimmedUsername) - 1u);
    trimmedUsername[sizeof(trimmedUsername) - 1u] = '\0';
    TrimAsciiWhitespaceInPlace(trimmedUsername);

    std::strncpy(outInput->inlineString00.data(), trimmedUsername, outInput->inlineString00.size() - 1u);
    outInput->inlineString00[outInput->inlineString00.size() - 1u] = '\0';
    std::strncpy(outInput->inlineString20.data(), password, outInput->inlineString20.size() - 1u);
    outInput->inlineString20[outInput->inlineString20.size() - 1u] = '\0';
    outInput->string60.begin = nullptr;
    outInput->string60.current = nullptr;
    outInput->string60.capacity = nullptr;
    outInput->flag6C = 0u;
    return true;
}


// UNANCHORED: text-mode analogue of the launcher page-6 rich-edit prompt/submit corridor.
// anchor: launcher.exe:0x408ee0
// anchor: launcher.exe:0x408840
// anchor: launcher.exe:0x408400
// anchor: launcher.exe:0x4091d0
// anchor: launcher.exe:0x41ecd0
bool CLauncher::RunTextModeLoginRichEditSubmitCredentialsStage() {
    if (!mxo::ltlogin::ILTLoginMediator::Default) {
        spdlog::error("ERROR: ILTLoginMediator::Default is unavailable before text-mode auth submit");
        return false;
    }
    if (!g_pLauncherObject6304) {
        spdlog::error("ERROR: arg5 launcher network object is unavailable before text-mode auth submit");
        return false;
    }
    bool forceRepromptAllFields = false;
    for (uint32_t attempt = 1u;; ++attempt) {
        if (!PromptForLauncherCredentialsForSubmitAttempt(forceRepromptAllFields)) {
            return false;
        }
        mxo::ltlogin::ProcessLoginRequestInputSketch submitLoginRequestInput = {};
        if (!BuildNoGuiProcessLoginRequestInput(
                g_LauncherCommandLine.AuthUsername(),
                g_LauncherCommandLine.AuthPassword(),
                &submitLoginRequestInput)) {
            spdlog::error("ERROR: failed to build no-GUI ProcessLoginRequest input");
            return false;
        }

        TextModeBlockingLoginObserver authObserver = {};
        authObserver.RegisterForExpectedEvent(g_pLauncherObject6304, 5u);
        const uint32_t submitResult =
            mxo::ltlogin::ILTLoginMediator::Default->ProcessLoginRequest(submitLoginRequestInput);
        const uint32_t waitResult = authObserver.WaitUntilDone(60000u);
        spdlog::info(
            "DIAGNOSTIC: pre-client launcher page6 auth attempt={} submitResult=0x{:08x} waitResult=0x{:08x} errorEvent=0x{:02x}",
            static_cast<unsigned>(attempt),
            static_cast<unsigned>(submitResult),
            static_cast<unsigned>(waitResult),
            static_cast<unsigned>(authObserver.error18 & 0xffu));

        if (!authObserver.SawError() && waitResult == 0u) {
            return true;
        }

        if (!authObserver.SawError() && waitResult == 0x12000003u) {
            spdlog::warn(
                "WARNING: pre-client launcher auth timed out before success/error resolution; re-prompting credentials instead of falling through into client load");
            WriteMatrixConsoleText(
                "Login timed out before the launcher received a success/error result. Please re-enter your username and password.",
                true);
        } else {
            spdlog::warn(
                "WARNING: pre-client launcher auth ended with errorEvent=0x{:02x} status80=0x{:08x}; re-prompting credentials instead of falling through into client load",
                static_cast<unsigned>(authObserver.error18 & 0xffu),
                static_cast<unsigned>(waitResult));
            WriteMatrixConsoleFormattedLine(
                "Login failed (0x%08x). Please re-enter your username and password.",
                static_cast<unsigned>(waitResult));
        }

        // anchor: launcher.exe:0x4091d0 -> sibling +0x34 -> launcher.exe:0x41c0d0
        // The original rich-edit failure path resets auth through mediator `+0x34` before retry.
        TextModeBlockingLoginObserver closeObserver = {};
        closeObserver.RegisterForExpectedEvent(g_pLauncherObject6304, 1u);
        mxo::ltlogin::ILTLoginMediator::Default->RequestAuthCloseAndSwitchToState0();
        const uint32_t closeWaitResult = closeObserver.WaitUntilDone(5000u);
        spdlog::info(
            "DIAGNOSTIC: auth retry reset wait result=0x{:08x} errorEvent=0x{:02x}",
            static_cast<unsigned>(closeWaitResult),
            static_cast<unsigned>(closeObserver.error18 & 0xffu));

        forceRepromptAllFields = true;
    }
}

// UNANCHORED: text-mode analogue of the launcher page-7 selection-list corridor.
// anchor: launcher.exe:0x4047d0 / case 7
// anchor: launcher.exe:0x40e480
// anchor: launcher.exe:0x40d530
// anchor: launcher.exe:0x40d820
// anchor: launcher.exe:0x405a20 / command 8
// anchor: launcher.exe:0x40d6f0
bool CLauncher::RunTextModeSelectionListStage() {
character_selection_menu:
    if (!mxo::ltlogin::ILTLoginMediator::Default) {
        spdlog::error("ERROR: ILTLoginMediator::Default is unavailable before text-mode selection");
        return false;
    }
    if (!g_pLauncherObject6304) {
        spdlog::error("ERROR: arg5 launcher network object is unavailable before text-mode selection");
        return false;
    }

    std::vector<TextModeLauncherSelectionRow> selectionRows;
    if (!BuildTextModeSelectionRows(&selectionRows) || selectionRows.empty()) {
        spdlog::error(
            "ERROR: launcher page-7 selection rows could not be rebuilt from ILTLoginMediator.Default");
        return false;
    }

    std::vector<size_t> existingRowIndices;
    existingRowIndices.reserve(selectionRows.size());
    for (size_t i = 0; i < selectionRows.size(); ++i) {
        if (!selectionRows[i].IsCreatePlaceholder() && selectionRows[i].selectionName[0] != '\0') {
            existingRowIndices.push_back(i);
        }
    }

    WriteMatrixConsoleText("The Matrix has you...", true);
    WriteMatrixConsoleText("Available world / character rows:", true);
    for (size_t i = 0; i < selectionRows.size(); ++i) {
        const TextModeLauncherSelectionRow& row = selectionRows[i];
        WriteMatrixConsoleFormattedLine(
            "  [%u] %s / %s%s",
            static_cast<unsigned>(i + 1u),
            row.worldName[0] ? row.worldName : "<unresolved world>",
            row.selectionName[0] ? row.selectionName : "<unresolved selection>",
            LauncherSelectionList_RowResolvesThroughCommand8(row) ? "" : " [unavailable]");
    }

    const bool deleteCharacterOptionAvailable = !existingRowIndices.empty();
    const uint32_t deleteCharacterMenuIndex = static_cast<uint32_t>(selectionRows.size());
    if (deleteCharacterOptionAvailable) {
        WriteMatrixConsoleFormattedLine(
            "  [%u] Delete existing character",
            static_cast<unsigned>(deleteCharacterMenuIndex + 1u));
    }

    uint32_t selectedMenuIndex = 0u;
    bool selectedFromCommandLine = false;
    if (g_LauncherCommandLine.LauncherCharacter()[0] != '\0') {
        for (size_t i = 0; i < selectionRows.size(); ++i) {
            const TextModeLauncherSelectionRow& row = selectionRows[i];
            if (!row.IsCreatePlaceholder() &&
                std::strcmp(row.selectionName, g_LauncherCommandLine.LauncherCharacter()) == 0) {
                selectedMenuIndex = static_cast<uint32_t>(i);
                selectedFromCommandLine = true;
                break;
            }
        }
    }
    if (!selectedFromCommandLine) {
        const uint32_t menuCount =
            static_cast<uint32_t>(selectionRows.size()) + (deleteCharacterOptionAvailable ? 1u : 0u);
        if (menuCount > 1u) {
            while (true) {
                uint32_t oneBasedSelection = 0u;
                if (ReadInteractiveLauncherIndex(
                        "Selection index: ",
                        menuCount + 1u,
                        &oneBasedSelection) &&
                    oneBasedSelection > 0u) {
                    selectedMenuIndex = oneBasedSelection - 1u;
                    break;
                }
                WriteMatrixConsoleText("Invalid selection index.", true);
            }
        }
    }

    if (deleteCharacterOptionAvailable && selectedMenuIndex == deleteCharacterMenuIndex) {
        WriteMatrixConsoleText("Delete which character?", true);
        for (size_t i = 0; i < existingRowIndices.size(); ++i) {
            const TextModeLauncherSelectionRow& row = selectionRows[existingRowIndices[i]];
            WriteMatrixConsoleFormattedLine(
                "  [%u] %s / %s",
                static_cast<unsigned>(i + 1u),
                row.worldName[0] ? row.worldName : "<unresolved world>",
                row.selectionName[0] ? row.selectionName : "<unresolved selection>");
        }

        uint32_t deleteOneBasedIndex = 0u;
        while (!ReadInteractiveLauncherIndex(
            "Delete character index: ",
            static_cast<uint32_t>(existingRowIndices.size()) + 1u,
            &deleteOneBasedIndex) ||
            deleteOneBasedIndex == 0u) {
            WriteMatrixConsoleText("Invalid character index.", true);
        }

        const TextModeLauncherSelectionRow& deleteRow =
            selectionRows[existingRowIndices[deleteOneBasedIndex - 1u]];
        WriteMatrixConsoleFormattedLine(
            "Type '%s' to confirm deletion:",
            deleteRow.selectionName);
        char confirmBuffer[256] = {};
        if (!ReadInteractiveLauncherField("Confirm: ", confirmBuffer, sizeof(confirmBuffer)) ||
            std::strcmp(confirmBuffer, deleteRow.selectionName) != 0) {
            WriteMatrixConsoleText("Deletion cancelled.", true);
            goto character_selection_menu;
        }

        // anchor: launcher.exe:0x40ec70
        // Text-mode delete bridge over the original page-7 delete-character corridor:
        // - call sibling `+0xf0 = 0x41c390` with the selected row high word
        // - block on event `8`
        // - on success, delete `Profiles\%s\%s` then call sibling `+0xe8`
        TextModeBlockingLoginObserver deleteObserver = {};
        deleteObserver.RegisterForExpectedEvent(g_pLauncherObject6304, 8u);
        (void)mxo::ltlogin::ILTLoginMediator::Default->SetSelectionIndexAndSwitchToState7(
            deleteRow.selectionIndexHighWord & 0xffffu);
        const uint32_t deleteWaitResult = deleteObserver.WaitUntilDone(30000u);
        if (deleteObserver.SawError() || deleteWaitResult != 0u) {
            WriteMatrixConsoleFormattedLine(
                "Delete failed (error=0x%08x).",
                static_cast<unsigned>(deleteWaitResult));
            goto character_selection_menu;
        }

        const char* deleteProfileRootNameToUse =
            mxo::ltlogin::ILTLoginMediator::Default->GetUsername();
        if (!deleteProfileRootNameToUse || !deleteProfileRootNameToUse[0]) {
            deleteProfileRootNameToUse = g_LauncherCommandLine.AuthUsername();
        }
        (void)DeleteLauncherProfileDirectoryForCharacter(
            deleteProfileRootNameToUse,
            deleteRow.selectionName);
        (void)mxo::ltlogin::ILTLoginMediator::Default->RemoveSlotRecordAndCompactRouteStateByIndex(
            deleteRow.selectionIndexHighWord & 0xffffu);

        WriteMatrixConsoleFormattedLine("Deleted character '%s'.", deleteRow.selectionName);
        goto character_selection_menu;
    }

    if (selectedMenuIndex >= selectionRows.size()) {
        spdlog::error(
            "ERROR: text-mode selection index {} was out of range for {} rows",
            static_cast<unsigned>(selectedMenuIndex),
            static_cast<unsigned>(selectionRows.size()));
        return false;
    }

    const TextModeLauncherSelectionRow& selectedRow = selectionRows[selectedMenuIndex];
    if (!LauncherSelectionList_RowResolvesThroughCommand8(selectedRow)) {
        WriteMatrixConsoleText("That row is not currently selectable from the launcher list.", true);
        goto character_selection_menu;
    }

    uint32_t resolvedA8 = 0u;
    uint32_t resolvedAC = 0u;
    char resolvedWorldName[sizeof(g_LastWorldName)] = {};
    if (!LauncherSelectionList_ResolveSelectionFromRow(
            selectedRow,
            &resolvedA8,
            &resolvedAC,
            resolvedWorldName,
            sizeof(resolvedWorldName))) {
        spdlog::error(
            "ERROR: launcher page-7 command-8 style writeback failed descriptorIndex=0x{:04x} selectionHighWord=0x{:04x}",
            static_cast<unsigned>(selectedRow.descriptorIndexLowWord & 0xffffu),
            static_cast<unsigned>(selectedRow.selectionIndexHighWord & 0xffffu));
        return false;
    }

    m_FieldA8 = resolvedA8;
    m_FieldAC = resolvedAC;
    CopyTextModeBuffer(g_LastWorldName, sizeof(g_LastWorldName), resolvedWorldName);
    StoreLastWorldNameInRegistry(g_LastWorldName);
    g_LauncherCommandLine.SetLauncherCharacter(
        selectedRow.IsCreatePlaceholder() ? "" : selectedRow.selectionName);

    WriteMatrixConsoleText("Follow the white rabbit. Knock, Knock, Neo.", true);
    spdlog::info(
        "DIAGNOSTIC: pre-client launcher auth/selection complete rowCount={} createPlaceholder={} selectedMenuIndex={} character='{}' world='{}' a8=0x{:08x} ac=0x{:08x}",
        static_cast<unsigned>(selectionRows.size()),
        selectedRow.IsCreatePlaceholder() ? 1u : 0u,
        static_cast<unsigned>(selectedMenuIndex),
        selectedRow.IsCreatePlaceholder() ? "- - -" : selectedRow.selectionName,
        g_LastWorldName,
        m_FieldA8,
        m_FieldAC);
    return true;
}

// UNANCHORED: replacement-owned pre-client auth/character-selection bridge.
// Fidelity correction from newer launcher/client recovery:
// - auth enters through the faithful page-6 submit boundary rooted at `0x408400 / 0x41ecd0`
// - blocking waits now use the proven `ILTLoginMediator` observer contract instead of synthetic
//   posted-event / posted-error latches
// - selection rows now come from the launcher page-7 `0x40e480` slot family instead of from a
//   flat recovered-character-count helper
// - existing-character selection stops at launcher-style `0x40d6f0` writeback into
//   `CLauncher+0xa8/+0xac` plus `Last_WorldName`
// - create-character remains a client-owned continuation after the launcher writes the sentinel
//   row high word `0xffff`
// Replacement-only ownership note:
// - this implementation intentionally lives under src/ so launcher.cpp can stay focused on
//   recovered launcher-owned startup coordination and anchored method bodies.

bool CLauncher::RunPreClientAuthAndCharacterSelectionStage() {
    // Load and select server first (before credentials)
    LoadServerConfigurations();
    if (!SelectServerInteractive()) {
        spdlog::error("ERROR: server selection failed");
        return false;
    }

    // Apply selected server config to mediator globals
    ApplySelectedServerConfigToMediator();

    if (!RunTextModeLoginRichEditSubmitCredentialsStage()) {
        return false;
    }
    return RunTextModeSelectionListStage();
}
