// Recovered launcher.exe command-line preprocessing scaffold.
// Original source not available.

#pragma once

#include <cstdint>
#include <ctime>

#include "consolevar.h"

class CLauncherCommandLine {
public:
    CLauncherCommandLine();
    ~CLauncherCommandLine();

    // anchor: launcher.exe:0x409950
    bool ParseCommandLine(int argc, char** argv);

    // anchor: launcher.exe:0x40b59a -> 0x4173d0
    // Replacement wrapper for the original InitInstance call shape:
    // CConsoleVar_ParseCommandLineAndConfig(filteredArgCount, filteredArgv, 0)
    bool ParseRuntimeConsoleVariables();

    // UNANCHORED: replacement-only runtime policy that keeps the current launcher on the
    // nopatch branch after the faithful 0x409950 parse stage. This must not mutate
    // whether the original -nopatch switch was actually present on the command line.
    void ApplyReplacementDefaultNoPatchPolicy();

    void Reset();

    std::uint32_t FilteredArgCount() const;
    char** FilteredArgv() const;

    const char* AuthUsername() const;
    const char* AuthPassword() const;
    const char* LauncherCharacter() const;
    void SetAuthUsername(const char* value);
    void SetAuthPassword(const char* value);
    void SetLauncherCharacter(const char* value);
    const char* LauncherSession() const;
    void SetLauncherSession(const char* value);
    const char* LauncherServer() const;
    void SetLauncherServer(const char* value);

    bool SwitchClone() const;
    bool SwitchSilent() const;
    bool SwitchNoPatch() const;
    bool SwitchRecover() const;
    bool SwitchDeleteChar() const;
    bool SwitchJustPatch() const;
    bool SwitchNoEula() const;
    bool SwitchSkipLaunch() const;
    bool SwitchLPTest() const;

    bool EulaFlowEnabled() const;
    bool PatchFlowEnabled() const;
    bool LauncherGlobal4D2C64() const;
    bool ReplacementDefaultNoPatchPolicyActive() const;

    std::uint32_t NoPatchLauncherVersionValue() const;
    std::uint32_t NoPatchClientVersionValue() const;
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
        kServer = 6,
    };

    static char* DuplicateArgString(const char* value);
    static bool CopyIntoFixedBuffer(char* destination, std::uint32_t destinationSize, const char* value);
    static std::uint32_t ParseOriginalVersionDword(const char* value);

    void FreeFilteredArgvOwned();
    bool AppendFilteredArg(const char* value);
    PendingValueTarget PendingTargetForSwitch(const char* value) const;
    bool ConsumeBooleanSwitch(const char* value);
    bool ConsumeValueSwitch(PendingValueTarget target, const char* value);
    void ProbeOptionsCfgAutodetectGate();
    void RebuildNoPatchVersionState();
    bool TryBuildOriginalVersionStringAndValue(
        const char* modulePath,
        char* out,
        std::uint32_t outSize,
        std::uint32_t* outValue) const;
    static bool IsTmBeforeAutodetectCutoff(const std::tm* value);

    std::uint32_t filteredArgCount_ = 0;
    char** filteredArgvOwned_ = nullptr;
    std::uint32_t filteredArgvOwnedCapacity_ = 0;

    char authUsername_[256] = {};
    char authPassword_[256] = {};
    char launcherCharacter_[256] = {};
    char launcherSession_[256] = {};
    char launcherServer_[256] = {};

    bool switchClone_ = false;
    bool switchSilent_ = false;     // observed -silent presence; 0x409950 shows no extra state write here
    bool switchNoPatch_ = false;    // observed explicit -nopatch presence from 0x409950
    bool switchRecover_ = false;
    bool switchDeleteChar_ = false; // observed -deletechar presence; 0x409950 shows no extra state write here
    bool switchJustPatch_ = false;
    bool switchNoEula_ = false;
    bool switchSkipLaunch_ = false; // observed -skiplaunch presence; 0x409950 shows no extra state write here
    bool switchLPTest_ = false;     // observed -lptest presence; 0x409950 shows no extra state write here

    bool eulaFlowEnabled_ = true;  // anchor: launcher.exe:0x4c8b1c
    bool patchFlowEnabled_ = true; // anchor: launcher.exe:0x4c8b1d
    bool launcherGlobal4D2C64_ = false;
    bool forcedDefaultNoPatchBranch_ = false;

    std::uint32_t noPatchLauncherVersionValue_ = 0;
    std::uint32_t noPatchClientVersionValue_ = 0;
    char noPatchLauncherVersionString_[32] = {};
    char noPatchClientVersionString_[32] = {};

    ConsoleParseErrorSink runtimeConsoleErrors_;
};


