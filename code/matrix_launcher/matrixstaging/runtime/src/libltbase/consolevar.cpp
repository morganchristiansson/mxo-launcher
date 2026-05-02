// String anchor: 0x004ade94
// Recovered scaffold for launcher.exe console variable parsing.
// Original source not available.

#include "consolevar.h"

#include "../../../../compat/sgi_tree_compat.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

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

// Console-variable registry tree wrappers above the shared SGI/libstdc++ `_Rb_tree` core.
// Current best static-RE anchors:
// - `0x4157b0` = `CConsoleVarRegistryTree_Find`
// - `0x415c40` = `CConsoleVarRegistryTree_InsertNode`
// - `0x415fc0` = `CConsoleVarRegistryTree_InsertUniqueHint`
// - `0x4161f0` = `CConsoleVarRegistryTree_ctor`
// - `0x456a90` = `CConsoleVarRegistryTree_EraseNode`
// Current source direction:
// - use direct `_Rb_tree` insert/erase mechanics
// - keep only the case-insensitive (`_stricmp`) registry-wrapper logic and source-owned node
//   ownership here
using ConsoleVarRegistryNode = mxo::sgi_tree::_Rb_tree_node<std::pair<const char*, CConsoleVar*>>;

struct ConsoleVarRegistryTreeHead18 {
    unsigned char colorOrFlag;
    unsigned char padding[3];
    void* root;
    void* first;
    void* last;
    unsigned char keyAndPayload[0x8];
};

struct ConsoleVarRegistryTreeObject0C {
    ConsoleVarRegistryTreeHead18* header = nullptr;
    std::uint32_t nodeCount = 0;
    std::uint32_t reserved08 = 0;
};

static_assert(sizeof(ConsoleVarRegistryTreeHead18) == 0x18, "console registry head size mismatch");
static_assert(sizeof(ConsoleVarRegistryTreeObject0C) == 0x0c, "console registry tree object size mismatch");
static_assert(sizeof(ConsoleVarRegistryNode) == 0x18, "console registry node size mismatch");

static ConsoleVarRegistryTreeObject0C* g_CConsoleVarRegistryTree = nullptr;

struct ConsoleRegistryFreeDeleter {
    void operator()(void* memory) const {
        std::free(memory);
    }
};

static std::unique_ptr<ConsoleVarRegistryTreeObject0C>& ConsoleVarRegistryTreeStorage() {
    static std::unique_ptr<ConsoleVarRegistryTreeObject0C> storage;
    return storage;
}

static std::unique_ptr<ConsoleVarRegistryTreeHead18, ConsoleRegistryFreeDeleter>& ConsoleVarRegistryHeadStorage() {
    static std::unique_ptr<ConsoleVarRegistryTreeHead18, ConsoleRegistryFreeDeleter> storage(nullptr);
    return storage;
}

static std::unordered_map<ConsoleVarRegistryNode*, std::unique_ptr<ConsoleVarRegistryNode>>&
ConsoleVarRegistryNodeStorage() {
    static std::unordered_map<ConsoleVarRegistryNode*, std::unique_ptr<ConsoleVarRegistryNode>> nodes;
    return nodes;
}

template <typename Head>
static mxo::sgi_tree::_Rb_tree_node_base* ConsoleRegistryHeaderBase(Head* head) {
    return reinterpret_cast<mxo::sgi_tree::_Rb_tree_node_base*>(head);
}

template <typename Head>
static const mxo::sgi_tree::_Rb_tree_node_base* ConsoleRegistryHeaderBase(const Head* head) {
    return reinterpret_cast<const mxo::sgi_tree::_Rb_tree_node_base*>(head);
}

static void InitializeConsoleVarRegistryTreeHead18(ConsoleVarRegistryTreeHead18* head) {
    mxo::sgi_tree::_Rb_tree_node_base* header = ConsoleRegistryHeaderBase(head);
    if (!header) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    header->_M_color = mxo::sgi_tree::_S_red;
    header->_M_parent = nullptr;
    header->_M_left = header;
    header->_M_right = header;
}

static int CompareConsoleVarRegistryKeys(const char* lhs, const char* rhs) {
    return _stricmp(lhs ? lhs : "", rhs ? rhs : "");
}

static ConsoleVarRegistryTreeObject0C* EnsureConsoleVarRegistryTree() {
    if (g_CConsoleVarRegistryTree) {
        return g_CConsoleVarRegistryTree;
    }

    auto tree = std::make_unique<ConsoleVarRegistryTreeObject0C>();
    if (!tree) {
        return nullptr;
    }

    std::unique_ptr<ConsoleVarRegistryTreeHead18, ConsoleRegistryFreeDeleter> header(
        static_cast<ConsoleVarRegistryTreeHead18*>(std::malloc(sizeof(ConsoleVarRegistryTreeHead18))));
    if (!header) {
        return nullptr;
    }
    InitializeConsoleVarRegistryTreeHead18(header.get());
    tree->header = header.get();

    g_CConsoleVarRegistryTree = tree.get();
    ConsoleVarRegistryHeadStorage() = std::move(header);
    ConsoleVarRegistryTreeStorage() = std::move(tree);
    return g_CConsoleVarRegistryTree;
}

static ConsoleVarRegistryNode* ConsoleVarRegistryFindNode(const char* name) {
    if (!name || !name[0] || !g_CConsoleVarRegistryTree || !g_CConsoleVarRegistryTree->header) {
        return nullptr;
    }

    const ConsoleVarRegistryNode* candidate = nullptr;
    const mxo::sgi_tree::_Rb_tree_node_base* header = ConsoleRegistryHeaderBase(g_CConsoleVarRegistryTree->header);
    const ConsoleVarRegistryNode* node =
        (header && header->_M_parent) ? static_cast<const ConsoleVarRegistryNode*>(header->_M_parent) : nullptr;
    while (node) {
        if (CompareConsoleVarRegistryKeys(node->_M_valptr()->first, name) >= 0) {
            candidate = node;
            node = static_cast<const ConsoleVarRegistryNode*>(node->_M_left);
        } else {
            node = static_cast<const ConsoleVarRegistryNode*>(node->_M_right);
        }
    }

    return (candidate && CompareConsoleVarRegistryKeys(candidate->_M_valptr()->first, name) == 0)
        ? const_cast<ConsoleVarRegistryNode*>(candidate)
        : nullptr;
}

static CConsoleVar* FindRegisteredConsoleVar(const char* name) {
    ConsoleVarRegistryNode* node = ConsoleVarRegistryFindNode(name);
    return node ? node->_M_valptr()->second : nullptr;
}

static bool ConsoleVarRegistryInsert(CConsoleVar* var) {
    if (!var || !var->Name()[0]) {
        return false;
    }

    ConsoleVarRegistryTreeObject0C* tree = EnsureConsoleVarRegistryTree();
    if (!tree || !tree->header) {
        return false;
    }

    auto node = std::make_unique<ConsoleVarRegistryNode>();
    if (!node) {
        return false;
    }
    node->_M_valptr()->first = var->Name();
    node->_M_valptr()->second = var;

    mxo::sgi_tree::_Rb_tree_node_base* header = ConsoleRegistryHeaderBase(tree->header);
    mxo::sgi_tree::_Rb_tree_node_base* parent = header;
    ConsoleVarRegistryNode* current =
        (header && header->_M_parent) ? static_cast<ConsoleVarRegistryNode*>(header->_M_parent) : nullptr;
    bool insertLeft = true;
    while (current) {
        parent = current;
        const int cmp = CompareConsoleVarRegistryKeys(node->_M_valptr()->first, current->_M_valptr()->first);
        if (cmp == 0) {
            return false;
        }
        insertLeft = (cmp < 0);
        current = insertLeft ? static_cast<ConsoleVarRegistryNode*>(current->_M_left)
                             : static_cast<ConsoleVarRegistryNode*>(current->_M_right);
    }

    node->_M_parent = nullptr;
    node->_M_left = nullptr;
    node->_M_right = nullptr;
    node->_M_color = mxo::sgi_tree::_S_red;

    ConsoleVarRegistryNode* insertedNode = node.get();
    mxo::sgi_tree::_Rb_tree_insert_and_rebalance(insertLeft, insertedNode, parent, *header);
    ++tree->nodeCount;
    ConsoleVarRegistryNodeStorage().emplace(insertedNode, std::move(node));
    return true;
}

static bool ConsoleVarRegistryEraseByName(const char* name) {
    ConsoleVarRegistryNode* node = ConsoleVarRegistryFindNode(name);
    if (!node || !g_CConsoleVarRegistryTree || !g_CConsoleVarRegistryTree->header) {
        return false;
    }

    (void)mxo::sgi_tree::_Rb_tree_rebalance_for_erase(node, *ConsoleRegistryHeaderBase(g_CConsoleVarRegistryTree->header));
    ConsoleVarRegistryNodeStorage().erase(node);
    if (g_CConsoleVarRegistryTree->nodeCount > 0) {
        --g_CConsoleVarRegistryTree->nodeCount;
    }
    return true;
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

// Config state management helpers (for custom parsers)
void CConsoleVar::RewindConfigState(ConsoleConfigParseState& state) {
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

void CConsoleVar::CloseConfigState(ConsoleConfigParseState& state) {
    CloseIncludeFiles(state);
    if (state.primaryFile) {
        std::fclose(state.primaryFile);
    }
    state.primaryFile = nullptr;
    state.activeFile = nullptr;
}

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

    if (CConsoleVar* existing = FindRegisteredConsoleVar(name_.c_str())) {
        // Current best read from `0x4162c0`:
        // - registry insertion is unique-by-name
        // - duplicate registration attempts do not replace the existing entry
        // - the duplicate path constructs the message
        //     `A console variable named "..." already exists!`
        //   but current static RE does not yet show a surviving user-visible sink before the
        //   wrapper still reaches `CConsoleVarRegistryTree_InsertUniqueHint`
        // - keep current source behavior as a silent no-op on duplicates until stronger evidence
        //   appears for an assert/log side effect that must be mirrored
        if (existing == this) {
            return;
        }
        return;
    }

    (void)ConsoleVarRegistryInsert(this);
}

void CConsoleVar::UnregisterSelf() {
    if (name_.empty()) {
        return;
    }

    ConsoleVarRegistryNode* node = ConsoleVarRegistryFindNode(name_.c_str());
    if (!node || node->_M_valptr()->second != this) {
        return;
    }

    (void)ConsoleVarRegistryEraseByName(name_.c_str());
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


