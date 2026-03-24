#include "launchercommandline.h"

#include <windows.h>
#include <winver.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

#include <spdlog/spdlog.h>

namespace mxo {
namespace libltbase {

CLauncherCommandLine::CLauncherCommandLine() {
    Reset();
}

CLauncherCommandLine::~CLauncherCommandLine() {
    FreeFilteredArgvOwned();
}

// anchor: launcher.exe:0x409950
bool CLauncherCommandLine::ParseCommandLine(int argc, char** argv) {
    Reset();

    const std::uint32_t pointerCount = (argc > 0) ? static_cast<std::uint32_t>(argc) : 1u;
    filteredArgvOwned_ = static_cast<char**>(std::calloc(pointerCount, sizeof(char*)));
    if (!filteredArgvOwned_) {
        return false;
    }
    filteredArgvOwnedCapacity_ = pointerCount;

    PendingValueTarget pendingValueTarget = PendingValueTarget::kNone;
    for (int originalArgIndex = 0; originalArgIndex < argc; ++originalArgIndex) {
        const char* argument = (argv && argv[originalArgIndex]) ? argv[originalArgIndex] : "";

        if (pendingValueTarget != PendingValueTarget::kNone) {
            if (!ConsumeValueSwitch(pendingValueTarget, argument)) {
                return false;
            }
            pendingValueTarget = PendingValueTarget::kNone;
            continue;
        }

        const PendingValueTarget newPendingTarget = PendingTargetForSwitch(argument);
        if (newPendingTarget != PendingValueTarget::kNone) {
            pendingValueTarget = newPendingTarget;
            continue;
        }

        if (ConsumeBooleanSwitch(argument)) {
            continue;
        }

        if (!AppendFilteredArg(argument)) {
            return false;
        }
    }

    ProbeOptionsCfgAutodetectGate();

    // Abort early with error if -user, -pwd AND -char args are not provided
    bool hasUser = (authUsername_[0] != '\0');
    bool hasPwd = (authPassword_[0] != '\0');
    bool hasChar = (launcherCharacter_[0] != '\0');
    if (!hasUser || !hasPwd || !hasChar) {
        spdlog::error("ERROR: launcher requires -user, -pwd AND -char arguments");
        return false;
    }

    return true;
}

// UNANCHORED: replacement-side wrapper for the original CWinApp_InitInstance call to
// CConsoleVar_ParseCommandLineAndConfig(filteredArgCount, filteredArgv, 0).
bool CLauncherCommandLine::ParseRuntimeConsoleVariables() {
    runtimeConsoleErrors_.lines.clear();
    return CConsoleVar::ParseCommandLineAndConfig(filteredArgCount_, filteredArgvOwned_, &runtimeConsoleErrors_);
}

// UNANCHORED: deliberate replacement-launcher policy, not original ParseCommandLine behavior.
void CLauncherCommandLine::ForceDefaultNoPatchBranch() {
    if (switchNoPatch_) {
        return;
    }

    switchNoPatch_ = true;
    launcherGlobal4C8B1D_ = false;
    RebuildNoPatchVersionState();
}

void CLauncherCommandLine::Reset() {
    FreeFilteredArgvOwned();

    filteredArgCount_ = 0;

    std::memset(authUsername_, 0, sizeof(authUsername_));
    std::memset(authPassword_, 0, sizeof(authPassword_));
    std::memset(launcherCharacter_, 0, sizeof(launcherCharacter_));
    std::memset(launcherSession_, 0, sizeof(launcherSession_));

    switchClone_ = false;
    switchSilent_ = false;
    switchNoPatch_ = false;
    switchRecover_ = false;
    switchDeleteChar_ = false;
    switchJustPatch_ = false;
    switchNoEula_ = false;
    switchSkipLaunch_ = false;
    switchLPTest_ = false;

    launcherGlobal4C8B1C_ = true;
    launcherGlobal4C8B1D_ = true;
    launcherGlobal4D2C64_ = false;
    autodetectExitCode_ = 0;

    noPatchLauncherVersionBits_ = FloatBitsFromCString("0.1");
    noPatchClientVersionBits_ = noPatchLauncherVersionBits_;
    std::memset(noPatchLauncherVersionString_, 0, sizeof(noPatchLauncherVersionString_));
    std::memset(noPatchClientVersionString_, 0, sizeof(noPatchClientVersionString_));

    runtimeConsoleErrors_.lines.clear();
}

std::uint32_t CLauncherCommandLine::FilteredArgCount() const {
    return filteredArgCount_;
}

char** CLauncherCommandLine::FilteredArgv() const {
    return filteredArgvOwned_;
}

const char* CLauncherCommandLine::AuthUsername() const {
    return authUsername_;
}

const char* CLauncherCommandLine::AuthPassword() const {
    return authPassword_;
}

const char* CLauncherCommandLine::LauncherCharacter() const {
    return launcherCharacter_;
}

const char* CLauncherCommandLine::LauncherSession() const {
    return launcherSession_;
}

bool CLauncherCommandLine::SwitchClone() const {
    return switchClone_;
}

bool CLauncherCommandLine::SwitchSilent() const {
    return switchSilent_;
}

bool CLauncherCommandLine::SwitchNoPatch() const {
    return switchNoPatch_;
}

bool CLauncherCommandLine::SwitchRecover() const {
    return switchRecover_;
}

bool CLauncherCommandLine::SwitchDeleteChar() const {
    return switchDeleteChar_;
}

bool CLauncherCommandLine::SwitchJustPatch() const {
    return switchJustPatch_;
}

bool CLauncherCommandLine::SwitchNoEula() const {
    return switchNoEula_;
}

bool CLauncherCommandLine::SwitchSkipLaunch() const {
    return switchSkipLaunch_;
}

bool CLauncherCommandLine::SwitchLPTest() const {
    return switchLPTest_;
}

bool CLauncherCommandLine::LauncherGlobal4C8B1C() const {
    return launcherGlobal4C8B1C_;
}

bool CLauncherCommandLine::LauncherGlobal4C8B1D() const {
    return launcherGlobal4C8B1D_;
}

bool CLauncherCommandLine::LauncherGlobal4D2C64() const {
    return launcherGlobal4D2C64_;
}

std::uint32_t CLauncherCommandLine::AutodetectExitCode() const {
    return autodetectExitCode_;
}

void CLauncherCommandLine::SetAutodetectExitCode(std::uint32_t exitCode) {
    autodetectExitCode_ = exitCode;
}

std::uint32_t CLauncherCommandLine::NoPatchLauncherVersionBits() const {
    return noPatchLauncherVersionBits_;
}

std::uint32_t CLauncherCommandLine::NoPatchClientVersionBits() const {
    return noPatchClientVersionBits_;
}

const char* CLauncherCommandLine::NoPatchLauncherVersionString() const {
    return noPatchLauncherVersionString_;
}

const char* CLauncherCommandLine::NoPatchClientVersionString() const {
    return noPatchClientVersionString_;
}

const ConsoleParseErrorSink& CLauncherCommandLine::RuntimeConsoleErrors() const {
    return runtimeConsoleErrors_;
}

char* CLauncherCommandLine::DuplicateArgString(const char* value) {
    if (!value) {
        value = "";
    }

    const std::size_t valueLength = std::strlen(value);
    char* copy = static_cast<char*>(std::malloc(valueLength + 1));
    if (!copy) {
        return nullptr;
    }

    std::memcpy(copy, value, valueLength + 1);
    return copy;
}

bool CLauncherCommandLine::CopyIntoFixedBuffer(
    char* destination,
    std::uint32_t destinationSize,
    const char* value) {
    if (!destination || destinationSize == 0) {
        return false;
    }

    if (!value) {
        value = "";
    }

    std::strncpy(destination, value, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
    return true;
}

std::uint32_t CLauncherCommandLine::FloatBitsFromCString(const char* value) {
    const float parsedValue = value ? std::strtof(value, nullptr) : 0.0f;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &parsedValue, sizeof(bits));
    return bits;
}

void CLauncherCommandLine::FreeFilteredArgvOwned() {
    if (!filteredArgvOwned_) {
        filteredArgvOwnedCapacity_ = 0;
        return;
    }

    for (std::uint32_t i = 0; i < filteredArgvOwnedCapacity_; ++i) {
        if (filteredArgvOwned_[i]) {
            std::free(filteredArgvOwned_[i]);
            filteredArgvOwned_[i] = nullptr;
        }
    }

    std::free(filteredArgvOwned_);
    filteredArgvOwned_ = nullptr;
    filteredArgvOwnedCapacity_ = 0;
}

bool CLauncherCommandLine::AppendFilteredArg(const char* value) {
    if (!filteredArgvOwned_ || filteredArgCount_ >= filteredArgvOwnedCapacity_) {
        return false;
    }

    filteredArgvOwned_[filteredArgCount_] = DuplicateArgString(value);
    if (!filteredArgvOwned_[filteredArgCount_]) {
        return false;
    }

    ++filteredArgCount_;
    return true;
}

CLauncherCommandLine::PendingValueTarget CLauncherCommandLine::PendingTargetForSwitch(const char* value) const {
    if (!value || !value[0]) {
        return PendingValueTarget::kNone;
    }

    if (::lstrcmpiA(value, "-qluser") == 0 || ::lstrcmpiA(value, "-user") == 0) {
        return PendingValueTarget::kUser;
    }
    if (::lstrcmpiA(value, "-qlpwd") == 0 || ::lstrcmpiA(value, "-pwd") == 0) {
        return PendingValueTarget::kPassword;
    }
    if (::lstrcmpiA(value, "-qlchar") == 0 || ::lstrcmpiA(value, "-char") == 0) {
        return PendingValueTarget::kCharacter;
    }
    if (::lstrcmpiA(value, "-qlsession") == 0 || ::lstrcmpiA(value, "-session") == 0) {
        return PendingValueTarget::kSession;
    }
    if (::lstrcmpiA(value, "-qlver") == 0) {
        return PendingValueTarget::kQlVersion;
    }

    return PendingValueTarget::kNone;
}

bool CLauncherCommandLine::ConsumeBooleanSwitch(const char* value) {
    if (!value || !value[0]) {
        return false;
    }

    if (::lstrcmpiA(value, "-clone") == 0) {
        switchClone_ = true;
        return true;
    }
    if (::lstrcmpiA(value, "-silent") == 0) {
        switchSilent_ = true;
        return true;
    }
    if (::lstrcmpiA(value, "-nopatch") == 0) {
        switchNoPatch_ = true;
        launcherGlobal4C8B1D_ = false;
        RebuildNoPatchVersionState();
        return true;
    }
    if (::lstrcmpiA(value, "-deletechar") == 0) {
        switchDeleteChar_ = true;
        return true;
    }
    if (::lstrcmpiA(value, "-recover") == 0) {
        switchRecover_ = true;
        return true;
    }
    if (::lstrcmpiA(value, "-justpatch") == 0) {
        switchJustPatch_ = true;
        launcherGlobal4C8B1C_ = false;
        return true;
    }
    if (::lstrcmpiA(value, "-noeula") == 0) {
        switchNoEula_ = true;
        launcherGlobal4C8B1C_ = false;
        return true;
    }
    if (::lstrcmpiA(value, "-skiplaunch") == 0) {
        switchSkipLaunch_ = true;
        return true;
    }
    if (::lstrcmpiA(value, "-lptest") == 0) {
        switchLPTest_ = true;
        return true;
    }

    return false;
}

bool CLauncherCommandLine::ConsumeValueSwitch(PendingValueTarget target, const char* value) {
    switch (target) {
        case PendingValueTarget::kUser:
            return CopyIntoFixedBuffer(authUsername_, sizeof(authUsername_), value);
        case PendingValueTarget::kPassword:
            return CopyIntoFixedBuffer(authPassword_, sizeof(authPassword_), value);
        case PendingValueTarget::kCharacter:
            return CopyIntoFixedBuffer(launcherCharacter_, sizeof(launcherCharacter_), value);
        case PendingValueTarget::kSession:
            return CopyIntoFixedBuffer(launcherSession_, sizeof(launcherSession_), value);
        case PendingValueTarget::kQlVersion:
            return true;
        case PendingValueTarget::kNone:
        default:
            return false;
    }
}

void CLauncherCommandLine::ProbeOptionsCfgAutodetectGate() {
    launcherGlobal4D2C64_ = false;

    struct _stat optionsStat = {};
    if (_stat("options.cfg", &optionsStat) != 0) {
        launcherGlobal4D2C64_ = true;
        return;
    }

    std::tm* optionsUtc = std::gmtime(&optionsStat.st_mtime);
    if (!optionsUtc) {
        return;
    }
    if (!IsTmBeforeAutodetectCutoff(optionsUtc)) {
        return;
    }

    const std::time_t currentTime = std::time(nullptr);
    std::tm* currentUtc = std::gmtime(&currentTime);
    if (!currentUtc) {
        return;
    }
    if (IsTmBeforeAutodetectCutoff(currentUtc)) {
        return;
    }

    launcherGlobal4D2C64_ = true;
}

// anchor: launcher.exe:0x409a73..0x409b46
// The original -nopatch branch seeds mediator version values from launcher.exe/client.dll version info.
void CLauncherCommandLine::RebuildNoPatchVersionState() {
    noPatchLauncherVersionBits_ = FloatBitsFromCString("0.1");
    noPatchClientVersionBits_ = noPatchLauncherVersionBits_;
    std::memset(noPatchLauncherVersionString_, 0, sizeof(noPatchLauncherVersionString_));
    std::memset(noPatchClientVersionString_, 0, sizeof(noPatchClientVersionString_));

    TryBuildOriginalVersionFloatString(
        "launcher.exe",
        noPatchLauncherVersionString_,
        sizeof(noPatchLauncherVersionString_),
        &noPatchLauncherVersionBits_);
    TryBuildOriginalVersionFloatString(
        "client.dll",
        noPatchClientVersionString_,
        sizeof(noPatchClientVersionString_),
        &noPatchClientVersionBits_);
}

bool CLauncherCommandLine::TryBuildOriginalVersionFloatString(
    const char* modulePath,
    char* out,
    std::uint32_t outSize,
    std::uint32_t* outBits) const {
    if (!modulePath || !out || outSize < 8 || !outBits) {
        return false;
    }

    out[0] = '\0';

    DWORD handle = 0;
    const DWORD versionInfoSize = GetFileVersionInfoSizeA(modulePath, &handle);
    if (versionInfoSize == 0) {
        return false;
    }

    void* versionInfo = std::malloc(versionInfoSize);
    if (!versionInfo) {
        return false;
    }

    bool ok = false;
    do {
        if (!GetFileVersionInfoA(modulePath, 0, versionInfoSize, versionInfo)) {
            break;
        }

        VS_FIXEDFILEINFO* fixedInfo = nullptr;
        UINT fixedInfoSize = 0;
        if (!VerQueryValueA(versionInfo, "\\", reinterpret_cast<LPVOID*>(&fixedInfo), &fixedInfoSize) ||
            !fixedInfo || fixedInfoSize < sizeof(VS_FIXEDFILEINFO)) {
            break;
        }

        const std::uint32_t major = HIWORD(fixedInfo->dwFileVersionMS);
        const std::uint32_t minor = LOWORD(fixedInfo->dwFileVersionMS);
        const std::uint32_t build = HIWORD(fixedInfo->dwFileVersionLS);
        const std::uint32_t revision = LOWORD(fixedInfo->dwFileVersionLS);
        const std::uint32_t majorQuotient = major / 10u;
        const std::uint32_t majorRemainder = major % 10u;

        std::snprintf(
            out,
            outSize,
            "%u.%u%u%u%u",
            static_cast<unsigned>(majorQuotient),
            static_cast<unsigned>(majorRemainder),
            static_cast<unsigned>(minor),
            static_cast<unsigned>(build),
            static_cast<unsigned>(revision));
        *outBits = FloatBitsFromCString(out);
        ok = true;
    } while (false);

    std::free(versionInfo);
    return ok;
}

bool CLauncherCommandLine::IsTmBeforeAutodetectCutoff(const std::tm* value) {
    if (!value) {
        return false;
    }

    const int year = value->tm_year + 1900;
    const int month = value->tm_mon;
    const int day = value->tm_mday;

    if (year != 2005) {
        return year < 2005;
    }
    if (month != 3) {
        return month < 3;
    }
    return day < 25;
}

} // namespace libltbase
} // namespace mxo
