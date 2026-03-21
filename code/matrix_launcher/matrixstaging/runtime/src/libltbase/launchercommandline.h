// Recovered launcher.exe command-line preprocessing scaffold.
// Original source not available.

#pragma once

#include <cstdint>
#include <ctime>

#include "consolevar.h"

namespace mxo {
namespace libltbase {

class CLauncherCommandLine {
public:
    CLauncherCommandLine();
    ~CLauncherCommandLine();

    // anchor: launcher.exe:0x409950
    bool ParseCommandLine(int argc, char** argv);

    // UNANCHORED: faithful CWinApp_InitInstance second-stage wrapper around
    // CConsoleVar_ParseCommandLineAndConfig(filteredArgCount, filteredArgv, 0)
    bool ParseRuntimeConsoleVariables();

    // UNANCHORED: diagnostic policy to preserve the current replacement-launcher
    // nopatch path without pretending that the original parser forced it.
    void ForceDefaultNoPatchBranch();

    void Reset();

    std::uint32_t FilteredArgCount() const;
    char** FilteredArgv() const;

    const char* AuthUsername() const;
    const char* AuthPassword() const;
    const char* LauncherCharacter() const;
    const char* LauncherSession() const;

    bool SwitchClone() const;
    bool SwitchSilent() const;
    bool SwitchNoPatch() const;
    bool SwitchRecover() const;
    bool SwitchDeleteChar() const;
    bool SwitchJustPatch() const;
    bool SwitchNoEula() const;
    bool SwitchSkipLaunch() const;
    bool SwitchLPTest() const;

    bool LauncherGlobal4C8B1C() const;
    bool LauncherGlobal4C8B1D() const;
    bool LauncherGlobal4D2C64() const;

    std::uint32_t AutodetectExitCode() const;
    void SetAutodetectExitCode(std::uint32_t exitCode);

    std::uint32_t NoPatchLauncherVersionBits() const;
    std::uint32_t NoPatchClientVersionBits() const;
    const char* NoPatchLauncherVersionString() const;
    const char* NoPatchClientVersionString() const;

    const ConsoleParseErrorSink& RuntimeConsoleErrors() const;

private:
    enum class PendingValueTarget : std::uint32_t {
        kNone = 0,
        kUser = 1,
        kPassword = 2,
        kCharacter = 3,
        kQlVersion = 4,
        kSession = 5,
    };

    static char* DuplicateArgString(const char* value);
    static bool CopyIntoFixedBuffer(char* destination, std::uint32_t destinationSize, const char* value);
    static std::uint32_t FloatBitsFromCString(const char* value);

    void FreeFilteredArgvOwned();
    bool AppendFilteredArg(const char* value);
    PendingValueTarget PendingTargetForSwitch(const char* value) const;
    bool ConsumeBooleanSwitch(const char* value);
    bool ConsumeValueSwitch(PendingValueTarget target, const char* value);
    void ProbeOptionsCfgAutodetectGate();
    void RebuildNoPatchVersionState();
    bool TryBuildOriginalVersionFloatString(
        const char* modulePath,
        char* out,
        std::uint32_t outSize,
        std::uint32_t* outBits) const;
    static bool IsTmBeforeAutodetectCutoff(const std::tm* value);

    std::uint32_t filteredArgCount_ = 0;
    char** filteredArgvOwned_ = nullptr;
    std::uint32_t filteredArgvOwnedCapacity_ = 0;

    char authUsername_[256] = {};
    char authPassword_[256] = {};
    char launcherCharacter_[256] = {};
    char launcherSession_[256] = {};

    bool switchClone_ = false;
    bool switchSilent_ = false;
    bool switchNoPatch_ = false;
    bool switchRecover_ = false;
    bool switchDeleteChar_ = false;
    bool switchJustPatch_ = false;
    bool switchNoEula_ = false;
    bool switchSkipLaunch_ = false;
    bool switchLPTest_ = false;

    bool launcherGlobal4C8B1C_ = true;
    bool launcherGlobal4C8B1D_ = true;
    bool launcherGlobal4D2C64_ = false;
    std::uint32_t autodetectExitCode_ = 0;

    std::uint32_t noPatchLauncherVersionBits_ = 0;
    std::uint32_t noPatchClientVersionBits_ = 0;
    char noPatchLauncherVersionString_[32] = {};
    char noPatchClientVersionString_[32] = {};

    ConsoleParseErrorSink runtimeConsoleErrors_;
};

} // namespace libltbase
} // namespace mxo
