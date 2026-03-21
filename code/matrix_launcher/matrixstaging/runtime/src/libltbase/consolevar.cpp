// String anchor: 0x004ade94
// Recovered scaffold for launcher.exe console variable parsing.
// Original source not available.

#include "consolevar.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mxo {
namespace libltbase {

namespace {

struct ConsoleRuntimeGlobals {
    std::uint32_t filteredArgCount = 0;
    char** filteredArgv = nullptr;
    ConsoleConfigParseState configState;
};

ConsoleRuntimeGlobals& RuntimeGlobals() {
    static ConsoleRuntimeGlobals globals;
    return globals;
}

std::vector<CConsoleVar*>& RegisteredConsoleVars() {
    static std::vector<CConsoleVar*> vars;
    return vars;
}

CConsoleVar* FindRegisteredConsoleVar(const char* name) {
    if (!name || !name[0]) {
        return nullptr;
    }

    for (CConsoleVar* var : RegisteredConsoleVars()) {
        if (var && std::strcmp(var->Name(), name) == 0) {
            return var;
        }
    }

    return nullptr;
}

void CloseIncludeFiles(ConsoleConfigParseState& state) {
    while (!state.includeStack.empty()) {
        FILE* includedFile = state.includeStack.back();
        state.includeStack.pop_back();
        if (includedFile) {
            std::fclose(includedFile);
        }
    }
}

void RewindConfigState(ConsoleConfigParseState& state) {
    CloseIncludeFiles(state);
    state.activeFile = state.primaryFile;
    state.foundSectionHeader = false;
    state.reachedEndOfFile = false;
    state.currentName.clear();
    state.currentValue.clear();
    state.workingLine.clear();
    if (state.primaryFile) {
        std::fseek(state.primaryFile, 0, SEEK_SET);
    }
}

void CloseConfigState(ConsoleConfigParseState& state) {
    CloseIncludeFiles(state);
    if (state.primaryFile) {
        std::fclose(state.primaryFile);
    }
    state.primaryFile = nullptr;
    state.activeFile = nullptr;
}

bool EqualsIgnoreCase(const char* left, const char* right) {
    if (!left || !right) {
        return false;
    }

    while (*left && *right) {
        const unsigned char leftByte = static_cast<unsigned char>(*left);
        const unsigned char rightByte = static_cast<unsigned char>(*right);
        if (std::tolower(leftByte) != std::tolower(rightByte)) {
            return false;
        }
        ++left;
        ++right;
    }

    return (*left == '\0') && (*right == '\0');
}

std::vector<std::string> SplitExtrasCsv(const std::string& extrasCsv) {
    std::vector<std::string> sections;
    std::size_t start = 0;
    while (start <= extrasCsv.size()) {
        const std::size_t comma = extrasCsv.find(',', start);
        if (comma == std::string::npos) {
            sections.emplace_back(extrasCsv.substr(start));
            break;
        }
        sections.emplace_back(extrasCsv.substr(start, comma - start));
        start = comma + 1;
    }

    if (sections.size() == 1 && sections[0].empty()) {
        sections.clear();
    }
    return sections;
}

} // namespace

CConsoleVar::~CConsoleVar() {
    UnregisterSelf();
}

const char* CConsoleVar::Name() const {
    return name_.c_str();
}

void CConsoleVar::SetRecoveredName(const char* name) {
    name_ = name ? name : "";
}

bool CConsoleVar::InitializedFromExternalSource() const {
    return initializedFromExternalSource_;
}

void CConsoleVar::SetInitializedFromExternalSource(bool initialized) {
    initializedFromExternalSource_ = initialized;
}

void CConsoleVar::RegisterSelf() {
    if (name_.empty()) {
        return;
    }

    for (CConsoleVar* existing : RegisteredConsoleVars()) {
        if (existing == this) {
            return;
        }
    }

    RegisteredConsoleVars().push_back(this);
}

void CConsoleVar::UnregisterSelf() {
    std::vector<CConsoleVar*>& vars = RegisteredConsoleVars();
    for (std::size_t i = 0; i < vars.size(); ++i) {
        if (vars[i] == this) {
            vars.erase(vars.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

// anchor: launcher.exe:0x4164b0
void CConsoleVar::ReportParseError(ConsoleParseErrorSink* errors, const char* format, ...) {
    if (!errors || !format) {
        return;
    }

    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    errors->lines.emplace_back(buffer);
}

// anchor: launcher.exe:0x4165b0
bool CConsoleVar::ParseCommandLine(std::uint32_t argc, char** argv, ConsoleParseErrorSink* errors) {
    ConsoleRuntimeGlobals& runtime = RuntimeGlobals();
    runtime.filteredArgCount = argc;
    runtime.filteredArgv = argv;
    runtime.configState = ConsoleConfigParseState();

    // Recovered switch map from the original parser:
    //   -config <file>
    //   -configsection <section>
    //   -extras <comma-delimited-sections>
    //   -ignoreunknownvars
    //   -failonunknownvars
    // plus typed console assignments in +VarName <value> pairs.
    for (std::uint32_t argIndex = 1; argIndex < argc; ++argIndex) {
        const char* argument = (argv && argv[argIndex]) ? argv[argIndex] : "";

        if (argument[0] == '-') {
            if (std::strcmp(argument, "-config") == 0) {
                ++argIndex;
                if (argIndex >= argc) {
                    ReportParseError(
                        errors,
                        "CConsoleVar::ParseCommandLine(): No value provided for %s switch on command line!\n",
                        "-config");
                    return false;
                }
                runtime.configState.configFilePath = argv[argIndex] ? argv[argIndex] : "";
                continue;
            }

            if (std::strcmp(argument, "-configsection") == 0) {
                ++argIndex;
                if (argIndex >= argc) {
                    ReportParseError(
                        errors,
                        "CConsoleVar::ParseCommandLine(): No value provided for %s switch on command line!\n",
                        "-configsection");
                    return false;
                }
                runtime.configState.configSectionName = argv[argIndex] ? argv[argIndex] : "";
                continue;
            }

            if (std::strcmp(argument, "-extras") == 0) {
                ++argIndex;
                if (argIndex >= argc) {
                    ReportParseError(
                        errors,
                        "CConsoleVar::ParseCommandLine(): No value provided for %s switch on command line!\n",
                        "-extras");
                    return false;
                }
                runtime.configState.extrasSectionsCsv = argv[argIndex] ? argv[argIndex] : "";
                continue;
            }

            if (std::strcmp(argument, "-ignoreunknownvars") == 0) {
                runtime.configState.ignoreUnknownVars = true;
                continue;
            }

            if (std::strcmp(argument, "-failonunknownvars") == 0) {
                runtime.configState.ignoreUnknownVars = false;
                continue;
            }

            // The original parser silently ignores unknown '-' switches at this stage.
            continue;
        }

        if (argument[0] != '+' || argument[1] == '\0') {
            ReportParseError(
                errors,
                "CConsoleVar::ParseCommandLine(): Found argument (%s) that doesn't begin with a '+'!\n",
                argument);
            return false;
        }

        const char* variableName = argument + 1;
        CConsoleVar* variable = FindRegisteredConsoleVar(variableName);
        if (!variable) {
            if (!runtime.configState.ignoreUnknownVars) {
                ReportParseError(
                    errors,
                    "CConsoleVar::ParseCommandLine(): Found unrecognized variable name (%s) on command line!\n",
                    variableName);
                return false;
            }

            if (argIndex + 1 < argc) {
                ++argIndex;
            }
            continue;
        }

        ++argIndex;
        if (argIndex >= argc) {
            ReportParseError(
                errors,
                "CConsoleVar::ParseCommandLine(): No value provided for variable (%s) on command line!\n",
                variableName);
            return false;
        }

        const char* valueText = argv[argIndex] ? argv[argIndex] : "";
        if (!variable->InitializedFromExternalSource()) {
            if (!variable->ParseValue(valueText)) {
                ReportParseError(
                    errors,
                    "CConsoleVar::ParseCommandLine(): Couldn't parse value (%s) for variable (%s) on command line!\n",
                    valueText,
                    variableName);
                return false;
            }
            variable->SetInitializedFromExternalSource(true);
        }
    }

    return true;
}

// anchor: launcher.exe:0x416a30
char* CConsoleVar::GetNextConfigFileLine(
    ConsoleConfigParseState& state,
    char* lineBuffer,
    int lineBufferSize,
    ConsoleParseErrorSink* errors) {
    if (!lineBuffer || lineBufferSize <= 1) {
        return nullptr;
    }

    constexpr const char* kIncludeDirective = "include";
    constexpr const int kIncludeDirectiveLength = 7;

    while (state.activeFile) {
        char* readResult = std::fgets(lineBuffer, lineBufferSize, state.activeFile);
        if (!readResult) {
            if (!state.includeStack.empty()) {
                std::fclose(state.activeFile);
                state.activeFile = state.includeStack.back();
                state.includeStack.pop_back();
                continue;
            }
            return nullptr;
        }

        if (std::strncmp(lineBuffer, kIncludeDirective, kIncludeDirectiveLength) != 0) {
            return lineBuffer;
        }

        char* cursor = lineBuffer + kIncludeDirectiveLength;
        while (*cursor && *cursor != '#' && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }

        if (*cursor != '"') {
            ReportParseError(
                errors,
                "CConsoleVar::GetNextConfigFileLine(): Couldn't find quote after include directive!\n");
            continue;
        }

        ++cursor;
        char* includePath = cursor;
        while (*cursor && *cursor != '#' && *cursor != '"') {
            ++cursor;
        }
        if (*cursor != '"') {
            ReportParseError(
                errors,
                "CConsoleVar::GetNextConfigFileLine(): Couldn't find quote after include directive!\n");
            continue;
        }

        *cursor = '\0';
        FILE* includeFile = std::fopen(includePath, "rt");
        if (!includeFile) {
            ReportParseError(
                errors,
                "CConsoleVar::GetNextConfigFileLine(): Couldn't find inline include file (%s)!\n",
                includePath);
            continue;
        }

        state.includeStack.push_back(state.activeFile);
        state.activeFile = includeFile;
    }

    return nullptr;
}

// anchor: launcher.exe:0x416c30
bool CConsoleVar::ParseConfigFileSectionLine(ConsoleConfigParseState& state, ConsoleParseErrorSink* errors) {
    state.foundSectionHeader = false;
    state.reachedEndOfFile = false;
    state.currentName.clear();
    state.currentValue.clear();

    char lineBuffer[0x5dc] = {};
    if (!GetNextConfigFileLine(state, lineBuffer, sizeof(lineBuffer), errors)) {
        state.reachedEndOfFile = true;
        return false;
    }

    state.workingLine = lineBuffer;
    const char* cursor = state.workingLine.c_str();
    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }

    if (*cursor == '\0' || *cursor == '#') {
        return true;
    }

    if (*cursor == '[') {
        state.foundSectionHeader = true;
        return false;
    }

    if (*cursor == '=') {
        ReportParseError(
            errors,
            "CConsoleVar::ParseConfigFileSection(): Found a variable name that starts with an '=' (%s)!\n",
            state.workingLine.c_str());
        return false;
    }

    const char* variableNameStart = cursor;
    while (*cursor && *cursor != '#' && *cursor != '=' && !std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    const char* variableNameEnd = cursor;

    if (*cursor == '\0' || *cursor == '#') {
        ReportParseError(
            errors,
            "CConsoleVar::ParseConfigFileSection(): No value provided for variable (%s)! (1)\n",
            state.workingLine.c_str());
        return false;
    }

    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor != '=') {
        ReportParseError(
            errors,
            "CConsoleVar::ParseConfigFileSection(): No equal sign after variable name (%s)!\n",
            state.workingLine.c_str());
        return false;
    }

    state.currentName.assign(variableNameStart, variableNameEnd - variableNameStart);

    ++cursor;
    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor == '\0' || *cursor == '#') {
        ReportParseError(
            errors,
            "CConsoleVar::ParseConfigFileSection(): No value provided for variable (%s)! (2)\n",
            state.currentName.c_str());
        return false;
    }

    if (*cursor == '"') {
        ++cursor;
        const char* valueStart = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r' && *cursor != '"') {
            ++cursor;
        }
        if (*cursor != '"') {
            ReportParseError(
                errors,
                "CConsoleVar::ParseConfigFileSection(): Quoted value for variable \"%s\" was not terminated with an end-quote!\n",
                state.currentName.c_str());
            return false;
        }
        state.currentValue.assign(valueStart, cursor - valueStart);
        ++cursor;
    } else {
        const char* valueStart = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r' && *cursor != '#' &&
               !std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        state.currentValue.assign(valueStart, cursor - valueStart);
    }

    while (*cursor && *cursor != '\n' && *cursor != '\r' && *cursor != '#') {
        if (!std::isspace(static_cast<unsigned char>(*cursor))) {
            ReportParseError(
                errors,
                "CConsoleVar::ParseConfigFileSection(): Found uncommented text after end of value for variable name (%s)!\n",
                state.currentName.c_str());
            return false;
        }
        ++cursor;
    }

    return true;
}

// anchor: launcher.exe:0x416ee0
bool CConsoleVar::FindConfigFileSection(
    ConsoleConfigParseState& state,
    const char* sectionName,
    ConsoleParseErrorSink* errors) {
    if (!sectionName || !sectionName[0]) {
        return false;
    }

    state.foundSectionHeader = false;
    state.reachedEndOfFile = false;

    char lineBuffer[0x5dc] = {};
    char* readLine = GetNextConfigFileLine(state, lineBuffer, sizeof(lineBuffer), errors);
    while (readLine) {
        if (lineBuffer[0] == '[') {
            char* sectionEnd = std::strchr(lineBuffer + 1, ']');
            if (!sectionEnd) {
                ReportParseError(
                    errors,
                    "CConsoleVar::FindConfigFileSection(): No terminating ] on section header (%s)!\n",
                    lineBuffer);
                return false;
            }

            *sectionEnd = '\0';
            if (EqualsIgnoreCase(sectionName, lineBuffer + 1)) {
                state.foundSectionHeader = true;
                return true;
            }
        }

        readLine = GetNextConfigFileLine(state, lineBuffer, sizeof(lineBuffer), errors);
    }

    state.reachedEndOfFile = true;
    return false;
}

// anchor: launcher.exe:0x416f90
bool CConsoleVar::ParseConfigFileSection(
    ConsoleConfigParseState& state,
    void* pendingNotifications,
    ConsoleParseErrorSink* errors) {
    (void)pendingNotifications;

    while (ParseConfigFileSectionLine(state, errors)) {
        if (state.currentName.empty()) {
            continue;
        }

        CConsoleVar* variable = FindRegisteredConsoleVar(state.currentName.c_str());
        if (!variable) {
            if (!state.ignoreUnknownVars) {
                ReportParseError(
                    errors,
                    "CConsoleVar::ParseConfigFileSection(): Found unrecognized variable name (%s)!\n",
                    state.currentName.c_str());
                return false;
            }
            continue;
        }

        if (!variable->InitializedFromExternalSource()) {
            if (!variable->ParseValue(state.currentValue.c_str())) {
                ReportParseError(
                    errors,
                    "CConsoleVar::ParseConfigFileSection(): Couldn't parse value (%s) for variable (%s)!\n",
                    state.currentValue.c_str(),
                    state.currentName.c_str());
                return false;
            }
            variable->SetInitializedFromExternalSource(true);
        }
    }

    return state.foundSectionHeader || state.reachedEndOfFile;
}

// anchor: launcher.exe:0x417130
bool CConsoleVar::ParseConfigFile(ConsoleConfigParseState& state, ConsoleParseErrorSink* errors) {
    if (!state.parseConfigFileEnabled) {
        return true;
    }

    state.primaryFile = std::fopen(state.configFilePath.c_str(), "rt");
    if (!state.primaryFile) {
        if (state.configFilePath != "autoexec.cfg") {
            ReportParseError(
                errors,
                "CConsoleVar::ParseConfigFile(): Couldn't find non-standard config file (%s)!\n",
                state.configFilePath.c_str());
            return false;
        }
        return true;
    }

    state.activeFile = state.primaryFile;

    const auto parseNamedSection = [&](const std::string& sectionName) -> bool {
        RewindConfigState(state);
        if (!FindConfigFileSection(state, sectionName.c_str(), errors)) {
            ReportParseError(
                errors,
                "CConsoleVar::ParseConfigFile(): Failed to parse config section \"%s\"!\n",
                sectionName.c_str());
            return false;
        }
        if (!ParseConfigFileSection(state, nullptr, errors)) {
            ReportParseError(
                errors,
                "CConsoleVar::FindAndParseConfigFileSection(): Failed to parse requested config section (%s)!\n",
                sectionName.c_str());
            return false;
        }
        return true;
    };

    const std::vector<std::string> extraSections = SplitExtrasCsv(state.extrasSectionsCsv);
    for (const std::string& extraSection : extraSections) {
        if (!extraSection.empty() && !parseNamedSection(extraSection)) {
            CloseConfigState(state);
            return false;
        }
    }

    if (!state.configSectionName.empty() && !parseNamedSection(state.configSectionName)) {
        CloseConfigState(state);
        return false;
    }

    RewindConfigState(state);
    if (!ParseConfigFileSection(state, nullptr, errors)) {
        ReportParseError(
            errors,
            "CConsoleVar::ParseConfigFile(): Failed to parse common/global config section!\n");
        CloseConfigState(state);
        return false;
    }

    CloseConfigState(state);
    return true;
}

// anchor: launcher.exe:0x4173d0
bool CConsoleVar::ParseCommandLineAndConfig(std::uint32_t argc, char** argv, ConsoleParseErrorSink* errors) {
    ConsoleRuntimeGlobals& runtime = RuntimeGlobals();
    runtime.filteredArgCount = argc;
    runtime.filteredArgv = argv;
    runtime.configState = ConsoleConfigParseState();

    if (!ParseCommandLine(argc, argv, errors)) {
        return false;
    }
    return ParseConfigFile(runtime.configState, errors);
}

} // namespace libltbase
} // namespace mxo
