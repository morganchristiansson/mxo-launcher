#include "launchercommandline.h"

#include <windows.h>
#include <winver.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

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
    return true;
}

// anchor: launcher.exe:0x40b59a -> 0x4173d0
// Replacement wrapper for the original InitInstance call to
// CConsoleVar_ParseCommandLineAndConfig(filteredArgCount, filteredArgv, 0).
bool CLauncherCommandLine::ParseRuntimeConsoleVariables() {
    runtimeConsoleErrors_.lines.clear();
    return CConsoleVar::ParseCommandLineAndConfig(filteredArgCount_, filteredArgvOwned_, &runtimeConsoleErrors_);
}

// UNANCHORED: deliberate replacement-launcher policy, not original ParseCommandLine behavior.
void CLauncherCommandLine::ApplyReplacementDefaultNoPatchPolicy() {
    if (switchNoPatch_ || forcedDefaultNoPatchBranch_) {
        return;
    }

    forcedDefaultNoPatchBranch_ = true;
    patchFlowEnabled_ = false;
}

void CLauncherCommandLine::Reset() {
    FreeFilteredArgvOwned();

    filteredArgCount_ = 0;

    std::memset(authUsername_, 0, sizeof(authUsername_));
    std::memset(authPassword_, 0, sizeof(authPassword_));
    std::memset(launcherCharacter_, 0, sizeof(launcherCharacter_));
    std::memset(launcherSession_, 0, sizeof(launcherSession_));
    std::memset(launcherServer_, 0, sizeof(launcherServer_));

    switchClone_ = false;
    switchSilent_ = false;
    switchNoPatch_ = false;
    switchRecover_ = false;
    switchDeleteChar_ = false;
    switchJustPatch_ = false;
    switchNoEula_ = false;
    switchSkipLaunch_ = false;
    switchLPTest_ = false;

    eulaFlowEnabled_ = true;
    patchFlowEnabled_ = true;
    launcherGlobal4D2C64_ = false;
    forcedDefaultNoPatchBranch_ = false;

    noPatchLauncherVersionValue_ = ParseOriginalVersionDword("0.1");
    noPatchClientVersionValue_ = noPatchLauncherVersionValue_;
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

// UNANCHORED: replacement-only interactive startup helper; original launcher used a UI prompt.
void CLauncherCommandLine::SetAuthUsername(const char* value) {
    CopyIntoFixedBuffer(authUsername_, sizeof(authUsername_), value ? value : "");
}

// UNANCHORED: replacement-only interactive startup helper; original launcher used a UI prompt.
void CLauncherCommandLine::SetAuthPassword(const char* value) {
    CopyIntoFixedBuffer(authPassword_, sizeof(authPassword_), value ? value : "");
}

// UNANCHORED: replacement-only CLI mirror of launcher UI character prefill.
void CLauncherCommandLine::SetLauncherCharacter(const char* value) {
    CopyIntoFixedBuffer(launcherCharacter_, sizeof(launcherCharacter_), value ? value : "");
}

const char* CLauncherCommandLine::LauncherSession() const {
    return launcherSession_;
}

const char* CLauncherCommandLine::LauncherServer() const {
    return launcherServer_;
}

// UNANCHORED: replacement-only CLI mirror of launcher UI session prefill.
void CLauncherCommandLine::SetLauncherSession(const char* value) {
    CopyIntoFixedBuffer(launcherSession_, sizeof(launcherSession_), value ? value : "");
}

// UNANCHORED: replacement-only CLI mirror of launcher UI server prefill.
void CLauncherCommandLine::SetLauncherServer(const char* value) {
    CopyIntoFixedBuffer(launcherServer_, sizeof(launcherServer_), value ? value : "");
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

bool CLauncherCommandLine::EulaFlowEnabled() const {
    return eulaFlowEnabled_;
}

bool CLauncherCommandLine::PatchFlowEnabled() const {
    return patchFlowEnabled_;
}

bool CLauncherCommandLine::LauncherGlobal4D2C64() const {
    return launcherGlobal4D2C64_;
}

bool CLauncherCommandLine::ReplacementDefaultNoPatchPolicyActive() const {
    return forcedDefaultNoPatchBranch_;
}

std::uint32_t CLauncherCommandLine::NoPatchLauncherVersionValue() const {
    return noPatchLauncherVersionValue_;
}

std::uint32_t CLauncherCommandLine::NoPatchClientVersionValue() const {
    return noPatchClientVersionValue_;
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

// anchor: launcher.exe:0x417440
std::uint32_t CLauncherCommandLine::ParseOriginalVersionDword(const char* value) {
    if (!value) {
        return 0u;
    }

    std::uint32_t major = 0u;
    std::size_t index = 0u;
    while (value[index] != '\0' && value[index] != '.') {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (ch < static_cast<unsigned char>('0') || ch > static_cast<unsigned char>('9')) {
            return 0u;
        }
        major = major * 10u + static_cast<std::uint32_t>(ch - static_cast<unsigned char>('0'));
        ++index;
    }

    if (major > 0xffu) {
        return 0u;
    }
    if (value[index] == '\0') {
        return major << 16u;
    }
    if (value[index] != '.') {
        return 0u;
    }

    ++index;
    std::uint32_t packedLowWord = 0u;
    std::uint32_t multiplier = 1u;
    while (value[index] != '\0') {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (ch < static_cast<unsigned char>('0') || ch > static_cast<unsigned char>('9')) {
            return 0u;
        }
        packedLowWord += static_cast<std::uint32_t>(ch - static_cast<unsigned char>('0')) * multiplier;
        multiplier *= 10u;
        ++index;
    }

    if (packedLowWord > 9999u) {
        return 0u;
    }
    return (major << 16u) + packedLowWord;
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
    if (::lstrcmpiA(value, "-server") == 0) {
        return PendingValueTarget::kServer;
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
        patchFlowEnabled_ = false;
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
        eulaFlowEnabled_ = false;
        return true;
    }
    if (::lstrcmpiA(value, "-noeula") == 0) {
        switchNoEula_ = true;
        eulaFlowEnabled_ = false;
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
        case PendingValueTarget::kServer:
            return CopyIntoFixedBuffer(launcherServer_, sizeof(launcherServer_), value);
        case PendingValueTarget::kQlVersion:
            return true;
        case PendingValueTarget::kNone:
        default:
            return false;
    }
}

// anchor: launcher.exe:0x409f34..0x409fc4
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
        launcherGlobal4D2C64_ = true;
        return;
    }
    if (IsTmBeforeAutodetectCutoff(currentUtc)) {
        return;
    }

    launcherGlobal4D2C64_ = true;
}

// anchor: launcher.exe:0x409a73..0x409c42
// The original explicit -nopatch branch seeds mediator version values from launcher.exe/client.dll
// version info after first applying the fallback string "0.1" to both mediator slots. The
// parser at `0x417440` does **not** produce IEEE float bits; it packs `major` into the high word
// and the post-dot decimal digits into the low word in reverse-order decimal form.
void CLauncherCommandLine::RebuildNoPatchVersionState() {
    noPatchLauncherVersionValue_ = ParseOriginalVersionDword("0.1");
    noPatchClientVersionValue_ = noPatchLauncherVersionValue_;
    std::memset(noPatchLauncherVersionString_, 0, sizeof(noPatchLauncherVersionString_));
    std::memset(noPatchClientVersionString_, 0, sizeof(noPatchClientVersionString_));

    TryBuildOriginalVersionStringAndValue(
        "launcher.exe",
        noPatchLauncherVersionString_,
        sizeof(noPatchLauncherVersionString_),
        &noPatchLauncherVersionValue_);
    TryBuildOriginalVersionStringAndValue(
        "client.dll",
        noPatchClientVersionString_,
        sizeof(noPatchClientVersionString_),
        &noPatchClientVersionValue_);
}

bool CLauncherCommandLine::TryBuildOriginalVersionStringAndValue(
    const char* modulePath,
    char* out,
    std::uint32_t outSize,
    std::uint32_t* outValue) const {
    if (!modulePath || !out || outSize < 8 || !outValue) {
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
        *outValue = ParseOriginalVersionDword(out);
        ok = (*outValue != 0u);
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
