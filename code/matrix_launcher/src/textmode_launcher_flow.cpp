#include "textmode_launcher_flow.h"

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

#include <spdlog/spdlog.h>

#include "diagnostics.h"
#include "launcher_network_object_abi.h"
#include "launcher_replacement_support.h"
#include "../matrixstaging/game/src/launcher/launcher.h"
#include "../matrixstaging/game/src/libltclientlogin/loginmediator.h"
#include "../matrixstaging/runtime/src/libltbase/launchercommandline.h"

extern mxo::libltbase::CLauncherCommandLine g_LauncherCommandLine;
extern void* g_pLauncherObject6304;
extern void* g_pILTLoginMediatorDefault;
extern char g_LastWorldName[256];

namespace {

bool g_TextModePreClientFlowCompleted = false;

const char* MaskedArgValue(const char* value) {
    if (!value || !value[0]) {
        return "<empty>";
    }
    return "<provided>";
}

static mxo::ltlogin::CLTLoginMediator* InstalledLauncherMediatorModel() {
    return dynamic_cast<mxo::ltlogin::CLTLoginMediator*>(mxo::ltlogin::ILTLoginMediator::Default);
}

static mxo::ltlogin::CLTLoginMediator* ActiveLauncherMediatorModel() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
        return const_cast<mxo::ltlogin::CLTLoginMediator*>(mediator->ResolveActiveStateSourceScaffold());
    }
    return mxo::ltlogin::CLTLoginMediator::ActiveStateSourceScaffold();
}

static void ResetLauncherPostedLoginResultIfPresent() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
        mediator->ResetPostedLoginResultScaffold();
    }
}

static uint32_t LauncherLastLoginEventOrZero() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
        return mediator->LastPostedEventScaffold();
    }
    return 0u;
}

static uint32_t LauncherLastLoginErrorOrZero() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
        return mediator->LastPostedErrorScaffold();
    }
    return 0u;
}

static bool LauncherHasSuccessfulPreClientAuthState() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
        return mediator->LastPostedEventScaffold() == 5u &&
               mediator->RecoveredCharacterCountScaffold() != 0u;
    }
    return false;
}

static uint32_t LauncherRecoveredCharacterCount() {
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        return mediator->RecoveredCharacterCountScaffold();
    }
    return 0u;
}

static bool LauncherRecoveredCharacterName(uint32_t slotIndex, char* outName, size_t outNameCapacity) {
    if (!outName || outNameCapacity == 0u) {
        return false;
    }
    outName[0] = '\0';
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        if (const auto* slotRecord = mediator->RecoveredCharacterByIndexScaffold(slotIndex);
            slotRecord != nullptr && !slotRecord->heapString14.empty()) {
            std::strncpy(outName, slotRecord->heapString14.c_str(), outNameCapacity - 1u);
            outName[outNameCapacity - 1u] = '\0';
            return true;
        }
    }
    return false;
}

static void LauncherSetSelectedCharacterIndex(uint32_t slotIndex) {
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        mediator->CharacterRouteIndexCc8() = static_cast<uint8_t>(slotIndex & 0xffu);
        spdlog::info(
            "DIAGNOSTIC: launcher selected character route index cc8 set to 0x{:02x}",
            static_cast<unsigned>(mediator->CharacterRouteIndexCc8()));
    }
}

static bool LauncherFindRecoveredWorldDescriptorIndexByName(
    const char* worldName,
    uint32_t* outDescriptorIndex) {
    if (!worldName || !worldName[0] || !outDescriptorIndex) {
        return false;
    }
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        const uint32_t worldCount = mediator->GetWorldCount();
        for (uint32_t i = 0u; i < worldCount; ++i) {
            const char* candidate = mediator->GetWorldNameByIndex(i);
            if (candidate && std::strcmp(candidate, worldName) == 0) {
                *outDescriptorIndex = i;
                return true;
            }
        }
    }
    return false;
}

static bool LauncherGetDeleteCharacterProfileRootName(char* outName, size_t outNameCapacity) {
    if (!outName || outNameCapacity == 0u) {
        return false;
    }
    outName[0] = '\0';
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        const char* profileRootName = mediator->GetCrashReporterUsername5c(nullptr);
        if (profileRootName && profileRootName[0]) {
            std::strncpy(outName, profileRootName, outNameCapacity - 1u);
            outName[outNameCapacity - 1u] = '\0';
            return true;
        }
    }
    return false;
}

static uint32_t LauncherBeginDeleteRecoveredCharacter(uint32_t slotIndex) {
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        return mediator->BeginDeleteCharacterBySlotIndexScaffold(slotIndex);
    }
    return 1u;
}

static uint32_t LauncherFinalizeDeleteRecoveredCharacter(uint32_t slotIndex) {
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        return mediator->RemoveSlotRecordAndCompactRouteStateByIndex(slotIndex);
    }
    return 1u;
}

static bool LauncherResolveRecoveredCharacterSelectionForWriteback(
    uint32_t slotIndex,
    char* outCharacterName,
    size_t outCharacterNameCapacity,
    char* outWorldName,
    size_t outWorldNameCapacity,
    uint32_t* outDescriptorIndex) {
    if (mxo::ltlogin::CLTLoginMediator* mediator = ActiveLauncherMediatorModel()) {
        const mxo::ltlogin::SlotRecordState004b5328* slotRecord = nullptr;
        uint32_t descriptorIndex = 0u;
        if (!mediator->BuildPartialSelectionContextForRecoveredCharacterScaffold(
                slotIndex,
                nullptr,
                &descriptorIndex,
                &slotRecord) ||
            slotRecord == nullptr) {
            return false;
        }
        if (outCharacterName && outCharacterNameCapacity != 0u) {
            outCharacterName[0] = '\0';
            std::strncpy(outCharacterName, slotRecord->heapString14.c_str(), outCharacterNameCapacity - 1u);
            outCharacterName[outCharacterNameCapacity - 1u] = '\0';
        }
        if (outWorldName && outWorldNameCapacity != 0u) {
            outWorldName[0] = '\0';
            const char* worldName =
                mediator->GetDescriptorInlineNameByIndex(static_cast<uint8_t>(descriptorIndex));
            if (worldName && worldName[0]) {
                std::strncpy(outWorldName, worldName, outWorldNameCapacity - 1u);
                outWorldName[outWorldNameCapacity - 1u] = '\0';
            }
        }
        if (outDescriptorIndex) {
            *outDescriptorIndex = descriptorIndex;
        }
        return true;
    }
    return false;
}

constexpr WORD kMatrixConsoleGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY;

// Replacement-only text-mode launcher divergence:
// - original page-7 list builder `0x40e480` inserts the `"- - -"` create-character sentinel only
//   while fewer than 3 active entries match the selected world
// - current text-mode host deliberately raises that soft cap so manual create-character testing is
//   still reachable on accounts that already have more than 3 characters
static constexpr uint32_t kTextModeCreateCharacterSoftLimit = 10u;

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

bool PromptForMissingLauncherCredentialsIfNeeded() {
    const bool missingUser = (g_LauncherCommandLine.AuthUsername()[0] == '\0');
    const bool missingPwd = (g_LauncherCommandLine.AuthPassword()[0] == '\0');
    if (!missingUser && !missingPwd) {
        return true;
    }

    WriteMatrixConsoleText("Wake up, Neo...", true);

    char inputBuffer[256] = {};
    if (missingUser) {
        if (!ReadInteractiveLauncherField("Username: ", inputBuffer, sizeof(inputBuffer)) ||
            inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive username prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthUsername(inputBuffer);
    }
    if (missingPwd) {
        if (!ReadInteractiveLauncherPasswordField("Password: ", inputBuffer, sizeof(inputBuffer)) ||
            inputBuffer[0] == '\0') {
            spdlog::error("ERROR: interactive password prompt failed or was left empty");
            return false;
        }
        g_LauncherCommandLine.SetAuthPassword(inputBuffer);
    }

    spdlog::info(
        "DIAGNOSTIC: replacement pre-client auth prompted for missing launcher credentials username={} password={}",
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

} // namespace

namespace mxo::launcher::replacement {

bool TextModePreClientFlowCompleted() {
    return g_TextModePreClientFlowCompleted;
}

void ResetTextModePreClientFlowCompleted() {
    g_TextModePreClientFlowCompleted = false;
}

} // namespace mxo::launcher::replacement

namespace mxo::launcher {

// UNANCHORED: replacement-owned text-mode pre-client auth/selection bridge.
// Fidelity correction from newer launcher/client recovery:
// - auth should enter through the faithful page-6 submit boundary (`0x408400 -> +0x30 -> 0x41ecd0`),
//   not the older source-owned BeginAuthConnection shortcut
// - existing-character selection should stop at launcher-style `0x40d6f0` writeback into
//   `CLauncher+0xa8/+0xac` plus `Last_WorldName`, not seed the later create-character `+0x120`
//   source block preemptively
// - create-character remains a client-owned continuation after the launcher writes the sentinel
//   row high word `0xffff`
// Replacement-only ownership note:
// - this implementation intentionally lives under src/ so launcher.cpp can stay focused on
//   recovered launcher-owned startup coordination and anchored method bodies.
bool CLauncher::RunPreClientAuthAndCharacterSelectionStage() {
    replacement::ResetTextModePreClientFlowCompleted();
    if (!DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface()) {
        spdlog::error(
            "ERROR: pre-client auth cannot begin because the faithful mediator +0x30 submit surface is unavailable");
        return false;
    }

    if (!PromptForMissingLauncherCredentialsIfNeeded()) {
        return false;
    }

    for (uint32_t attempt = 1u;; ++attempt) {
        mxo::ltlogin::ProcessLoginRequestInputSketch submitLoginRequestInput = {};
        if (!BuildNoGuiProcessLoginRequestInput(
                g_LauncherCommandLine.AuthUsername(),
                g_LauncherCommandLine.AuthPassword(),
                &submitLoginRequestInput)) {
            spdlog::error("ERROR: failed to build no-GUI ProcessLoginRequest input");
            return false;
        }

        ResetLauncherPostedLoginResultIfPresent();
        const uint32_t submitResult =
            DiagnosticSubmitLoginRequestViaResolvedMediatorSurface(submitLoginRequestInput);
        spdlog::info(
            "DIAGNOSTIC: pre-client launcher auth attempt={} submitResult=0x{:08x}",
            static_cast<unsigned>(attempt),
            static_cast<unsigned>(submitResult));

        // Fidelity correction:
        // - original launcher page-6 submit enters through `0x408400 -> +0x30 -> 0x41ecd0`
        // - `ProcessLoginRequest` itself already performs the happy-path `state0 -> state2`
        //   handoff, and state2/state1 own the later auth-connect/bootstrap continuation
        // - so the text-mode host should not issue an extra out-of-band auth begin after a
        //   successful `+0x30` submit; doing so is less faithful and perturbs retry behavior
        const DWORD startTick = GetTickCount();
        bool authSucceeded = false;
        while ((GetTickCount() - startTick) < 60000u) {
            LauncherPumpNetworkEngineAbiShell(g_pLauncherObject6304, /*nonBlocking=*/true);
            if (LauncherHasSuccessfulPreClientAuthState()) {
                authSucceeded = true;
                break;
            }
            const uint32_t loginError = LauncherLastLoginErrorOrZero();
            if (loginError != 0u) {
                spdlog::info(
                    "DIAGNOSTIC: pre-client launcher auth attempt={} terminated with loginError=0x{:02x}",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(loginError));
                break;
            }
            Sleep(10u);
        }

        if (!authSucceeded) {
            const uint32_t loginError = LauncherLastLoginErrorOrZero();
            if (loginError == 0u) {
                spdlog::warn(
                    "WARNING: pre-client launcher auth timed out before success/error resolution; re-prompting credentials instead of falling through into client load");
                WriteMatrixConsoleText(
                    "Login timed out before the launcher received a success/error result. Please re-enter your username and password.",
                    true);
            } else {
                spdlog::warn(
                    "WARNING: pre-client launcher auth ended with error=0x{:02x}; re-prompting credentials instead of falling through into client load",
                    static_cast<unsigned>(loginError));
                WriteMatrixConsoleFormattedLine(
                    "Login failed (0x%08x). Please re-enter your username and password.",
                    static_cast<unsigned>(loginError));
            }

            // anchor: launcher.exe:0x4091d0 -> sibling +0x34 -> launcher.exe:0x41c0d0
            // Rich-edit observer failure path in the original launcher closes the current auth
            // connection and restores helper state0 before the user retries credentials.
            ResetLauncherPostedLoginResultIfPresent();
            if (mxo::ltlogin::CLTLoginMediator* mediator = InstalledLauncherMediatorModel()) {
                mediator->RequestAuthCloseAndSwitchToState0();
            }
            const DWORD authRetryResetStartTick = GetTickCount();
            while ((GetTickCount() - authRetryResetStartTick) < 5000u) {
                LauncherPumpNetworkEngineAbiShell(g_pLauncherObject6304, /*nonBlocking=*/true);
                if (LauncherLastLoginEventOrZero() == 1u ||
                    (InstalledLauncherMediatorModel() != nullptr &&
                     InstalledLauncherMediatorModel()->IsAuthConnectionQuiescentForRetryScaffold())) {
                    break;
                }
                Sleep(10u);
            }

            char inputBuffer[256] = {};
            if (!ReadInteractiveLauncherField("Username: ", inputBuffer, sizeof(inputBuffer)) ||
                inputBuffer[0] == '\0') {
                spdlog::error("ERROR: interactive username re-prompt failed or was left empty");
                return false;
            }
            g_LauncherCommandLine.SetAuthUsername(inputBuffer);
            if (!ReadInteractiveLauncherPasswordField("Password: ", inputBuffer, sizeof(inputBuffer)) ||
                inputBuffer[0] == '\0') {
                spdlog::error("ERROR: interactive password re-prompt failed or was left empty");
                return false;
            }
            g_LauncherCommandLine.SetAuthPassword(inputBuffer);
            continue;
        }

character_selection_menu:
        WriteMatrixConsoleText("The Matrix has you...", true);

        const uint32_t recoveredCharacterCount = LauncherRecoveredCharacterCount();
        const bool createCharacterPlaceholderAvailable =
            (recoveredCharacterCount < kTextModeCreateCharacterSoftLimit);
        const bool deleteCharacterOptionAvailable = (recoveredCharacterCount != 0u);
        const uint32_t createCharacterMenuIndex = recoveredCharacterCount;
        const uint32_t deleteCharacterMenuIndex =
            recoveredCharacterCount + (createCharacterPlaceholderAvailable ? 1u : 0u);

        WriteMatrixConsoleText("Available characters:", true);
        for (uint32_t i = 0u; i < recoveredCharacterCount; ++i) {
            char characterName[256] = {};
            const bool haveCharacterName =
                LauncherRecoveredCharacterName(i, characterName, sizeof(characterName));
            WriteMatrixConsoleFormattedLine(
                "  [%u] %s",
                static_cast<unsigned>(i + 1u),
                haveCharacterName ? characterName : "<unresolved>");
        }
        if (createCharacterPlaceholderAvailable) {
            WriteMatrixConsoleFormattedLine(
                "  [%u] - - - (Create Character)",
                static_cast<unsigned>(createCharacterMenuIndex + 1u));
        }
        if (deleteCharacterOptionAvailable) {
            WriteMatrixConsoleFormattedLine(
                "  [%u] Delete existing character",
                static_cast<unsigned>(deleteCharacterMenuIndex + 1u));
        }

        uint32_t selectedMenuIndex = 0u;
        bool selectedFromCommandLine = false;
        bool createCharacterPlaceholderSelected = false;
        bool deleteCharacterOptionSelected = false;
        if (g_LauncherCommandLine.LauncherCharacter()[0] != '\0') {
            for (uint32_t i = 0u; i < recoveredCharacterCount; ++i) {
                char characterName[256] = {};
                if (LauncherRecoveredCharacterName(i, characterName, sizeof(characterName)) &&
                    std::strcmp(characterName, g_LauncherCommandLine.LauncherCharacter()) == 0) {
                    selectedMenuIndex = i;
                    selectedFromCommandLine = true;
                    break;
                }
            }
        }
        if (!selectedFromCommandLine) {
            const uint32_t menuCount =
                recoveredCharacterCount +
                (createCharacterPlaceholderAvailable ? 1u : 0u) +
                (deleteCharacterOptionAvailable ? 1u : 0u);
            if (menuCount > 1u) {
                while (true) {
                    uint32_t oneBasedSelection = 0u;
                    if (ReadInteractiveLauncherIndex(
                            "Character index: ",
                            menuCount + 1u,
                            &oneBasedSelection) &&
                        oneBasedSelection > 0u) {
                        selectedMenuIndex = oneBasedSelection - 1u;
                        break;
                    }
                    WriteMatrixConsoleText("Invalid character index.", true);
                }
            }
        }
        createCharacterPlaceholderSelected =
            createCharacterPlaceholderAvailable && (selectedMenuIndex == createCharacterMenuIndex);
        deleteCharacterOptionSelected =
            deleteCharacterOptionAvailable && (selectedMenuIndex == deleteCharacterMenuIndex);

        // anchor: launcher.exe:0x40ec70
        // Text-mode delete bridge over the original page-7 delete-character corridor:
        // - choose a concrete recovered slot-record index
        // - require explicit typed-name confirmation so we do not delete the wrong character
        // - then call the anchored mediator primitives `+0xf0` / wait event `8` / `+0xe8`
        if (deleteCharacterOptionSelected) {
            WriteMatrixConsoleText("Delete which character?", true);
            for (uint32_t i = 0u; i < recoveredCharacterCount; ++i) {
                char characterName[256] = {};
                const bool haveCharacterName =
                    LauncherRecoveredCharacterName(i, characterName, sizeof(characterName));
                WriteMatrixConsoleFormattedLine(
                    "  [%u] %s",
                    static_cast<unsigned>(i + 1u),
                    haveCharacterName ? characterName : "<unresolved>");
            }

            uint32_t deleteOneBasedIndex = 0u;
            while (!ReadInteractiveLauncherIndex(
                "Delete character index: ",
                recoveredCharacterCount + 1u,
                &deleteOneBasedIndex) ||
                deleteOneBasedIndex == 0u) {
                WriteMatrixConsoleText("Invalid character index.", true);
            }
            const uint32_t deleteSlotIndex = deleteOneBasedIndex - 1u;

            char deleteCharacterName[256] = {};
            if (!LauncherRecoveredCharacterName(
                    deleteSlotIndex,
                    deleteCharacterName,
                    sizeof(deleteCharacterName))) {
                WriteMatrixConsoleText("Failed to resolve character name for deletion.", true);
                goto character_selection_menu;
            }

            WriteMatrixConsoleFormattedLine(
                "Type '%s' to confirm deletion:",
                deleteCharacterName);
            char confirmBuffer[256] = {};
            if (!ReadInteractiveLauncherField("Confirm: ", confirmBuffer, sizeof(confirmBuffer)) ||
                std::strcmp(confirmBuffer, deleteCharacterName) != 0) {
                WriteMatrixConsoleText("Deletion cancelled.", true);
                goto character_selection_menu;
            }

            ResetLauncherPostedLoginResultIfPresent();
            (void)LauncherBeginDeleteRecoveredCharacter(deleteSlotIndex);

            // anchor: launcher.exe:0x40ec70
            // The original launcher calls sibling `+0xf0 = 0x41c390` and immediately enters the
            // event-8 wait; it does not gate the flow on the return value from `+0xf0`.
            const DWORD deleteStartTick = GetTickCount();
            bool deleteSucceeded = false;
            while ((GetTickCount() - deleteStartTick) < 30000u) {
                LauncherPumpNetworkEngineAbiShell(g_pLauncherObject6304, /*nonBlocking=*/true);
                if (LauncherLastLoginEventOrZero() == 8u) {
                    deleteSucceeded = true;
                    break;
                }
                if (LauncherLastLoginErrorOrZero() != 0u) {
                    break;
                }
                Sleep(10u);
            }

            if (!deleteSucceeded) {
                WriteMatrixConsoleFormattedLine(
                    "Delete failed (error=0x%08x).",
                    static_cast<unsigned>(LauncherLastLoginErrorOrZero()));
                goto character_selection_menu;
            }

            char deleteProfileRootName[256] = {};
            const char* deleteProfileRootNameToUse = g_LauncherCommandLine.AuthUsername();
            if (LauncherGetDeleteCharacterProfileRootName(
                    deleteProfileRootName,
                    sizeof(deleteProfileRootName)) &&
                deleteProfileRootName[0] != '\0') {
                deleteProfileRootNameToUse = deleteProfileRootName;
            }

            (void)DeleteLauncherProfileDirectoryForCharacter(
                deleteProfileRootNameToUse,
                deleteCharacterName);

            (void)LauncherFinalizeDeleteRecoveredCharacter(deleteSlotIndex);

            WriteMatrixConsoleFormattedLine("Deleted character '%s'.", deleteCharacterName);
            goto character_selection_menu;
        }

        char persistedSelectionName[256] = {};
        if (replacement::LoadLastWorldNameFromRegistry(
                persistedSelectionName,
                sizeof(persistedSelectionName))) {
            spdlog::info(
                "DIAGNOSTIC: loaded HKLM Last_WorldName fallback='{}' on character-selection path",
                persistedSelectionName);
        }

        char selectedCharacterName[256] = {};
        char selectedSelectionName[256] = {};
        uint32_t selectedDescriptorIndex = m_FieldAC & 0x00ffffffu;
        uint32_t selectedRowHighWordSelectionIndex = 0xffffu;

        if (!createCharacterPlaceholderSelected) {
            if (!LauncherResolveRecoveredCharacterSelectionForWriteback(
                    selectedMenuIndex,
                    selectedCharacterName,
                    sizeof(selectedCharacterName),
                    selectedSelectionName,
                    sizeof(selectedSelectionName),
                    &selectedDescriptorIndex)) {
                spdlog::error(
                    "ERROR: failed to resolve recovered character-selection metadata index={} through mediator-owned slot/world tables",
                    static_cast<unsigned>(selectedMenuIndex));
                return false;
            }
            selectedRowHighWordSelectionIndex = selectedMenuIndex & 0xffffu;
            LauncherSetSelectedCharacterIndex(selectedMenuIndex);
            g_LauncherCommandLine.SetLauncherCharacter(selectedCharacterName);
        } else {
            if (g_LastWorldName[0] != '\0') {
                std::strncpy(selectedSelectionName, g_LastWorldName, sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            } else if (persistedSelectionName[0] != '\0') {
                std::strncpy(
                    selectedSelectionName,
                    persistedSelectionName,
                    sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            } else if (
                const replacement::RecoveredLauncherSelectionRecord* defaultSelection =
                    replacement::DefaultRecoveredLauncherSelectionRecord();
                defaultSelection != nullptr && defaultSelection->selectionName != nullptr) {
                std::strncpy(
                    selectedSelectionName,
                    defaultSelection->selectionName,
                    sizeof(selectedSelectionName) - 1u);
                selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            }
            selectedCharacterName[0] = '\0';
            selectedRowHighWordSelectionIndex = 0xffffu;
            g_LauncherCommandLine.SetLauncherCharacter("");
            if (selectedSelectionName[0] != '\0') {
                uint32_t resolvedDescriptorIndex = 0u;
                if (LauncherFindRecoveredWorldDescriptorIndexByName(
                        selectedSelectionName,
                        &resolvedDescriptorIndex)) {
                    selectedDescriptorIndex = resolvedDescriptorIndex;
                }
            }
        }

        if (!selectedSelectionName[0] && persistedSelectionName[0]) {
            std::strncpy(selectedSelectionName, persistedSelectionName, sizeof(selectedSelectionName) - 1u);
            selectedSelectionName[sizeof(selectedSelectionName) - 1u] = '\0';
            spdlog::info(
                "DIAGNOSTIC: falling back to persisted Last_WorldName='{}' on character-selection path",
                selectedSelectionName);
        }

        if (!selectedSelectionName[0]) {
            spdlog::error(
                "ERROR: no launcher world-selection name was available for page-7 style writeback");
            return false;
        }

        // anchor: launcher.exe:0x4047d0 / case 7
        // anchor: launcher.exe:0x40d530
        // anchor: launcher.exe:0x40d820
        // anchor: launcher.exe:0x405a20 / command 8
        // anchor: launcher.exe:0x40d6f0
        // No-GUI faithful bridge:
        // - existing-character selection stops at launcher-style command-8 writeback
        // - create-character uses the same writeback helper with row high word `0xffff`
        // - the later create-character arg6 `+0x120` submit stays on the client-owned continuation
        uint32_t resolvedA8 = m_FieldA8;
        uint32_t resolvedAC = m_FieldAC;
        char resolvedSelectionName[sizeof(g_LastWorldName)] = {};
        const bool resolvedViaCommand8 =
            g_pILTLoginMediatorDefault != nullptr &&
            DiagnosticResolveLauncherSelectionFromMediator(
                g_pILTLoginMediatorDefault,
                selectedDescriptorIndex & 0x00ffffffu,
                selectedRowHighWordSelectionIndex,
                &resolvedA8,
                &resolvedAC,
                resolvedSelectionName,
                sizeof(resolvedSelectionName));

        if (resolvedViaCommand8) {
            m_FieldA8 = resolvedA8;
            m_FieldAC = resolvedAC;
            const char* finalWorldName =
                resolvedSelectionName[0] ? resolvedSelectionName : selectedSelectionName;
            std::strncpy(g_LastWorldName, finalWorldName, sizeof(g_LastWorldName) - 1u);
            g_LastWorldName[sizeof(g_LastWorldName) - 1u] = '\0';
            replacement::StoreLastWorldNameInRegistry(g_LastWorldName);
        } else {
            m_FieldA8 = createCharacterPlaceholderSelected ? 0xffffffffu : (selectedMenuIndex & 0xffffu);
            m_FieldAC = selectedDescriptorIndex & 0x00ffffffu;
            std::strncpy(g_LastWorldName, selectedSelectionName, sizeof(g_LastWorldName) - 1u);
            g_LastWorldName[sizeof(g_LastWorldName) - 1u] = '\0';
            replacement::StoreLastWorldNameInRegistry(g_LastWorldName);
            spdlog::warn(
                "WARNING: pre-client no-GUI selection could not mirror page7 command8 / 0x40d6f0 via descriptorIndex={} selectionHighWord=0x{:04x}; using bounded launcher writeback fallback world='{}' fallbackA8=0x{:08x} fallbackAC=0x{:08x}",
                static_cast<unsigned>(selectedDescriptorIndex),
                static_cast<unsigned>(selectedRowHighWordSelectionIndex & 0xffffu),
                g_LastWorldName,
                m_FieldA8,
                m_FieldAC);
        }

        g_TextModePreClientFlowCompleted = true;
        WriteMatrixConsoleText("Follow the white rabbit. Knock, Knock, Neo.", true);
        spdlog::info(
            "DIAGNOSTIC: pre-client launcher auth/selection complete event=0x{:02x} createPlaceholder={} recoveredCharacterCount={} selectedMenuIndex={} character='{}' world='{}' a8=0x{:08x} ac=0x{:08x}",
            static_cast<unsigned>(LauncherLastLoginEventOrZero()),
            createCharacterPlaceholderSelected ? 1u : 0u,
            static_cast<unsigned>(recoveredCharacterCount),
            static_cast<unsigned>(selectedMenuIndex),
            selectedCharacterName[0] ? selectedCharacterName : "- - -",
            selectedSelectionName[0] ? selectedSelectionName : "<unresolved>",
            m_FieldA8,
            m_FieldAC);
        return true;
    }
}

} // namespace mxo::launcher
