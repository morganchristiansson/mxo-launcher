// Server configuration parsing using consolevar infrastructure.
// Parses servers.cfg to load multiple server definitions.

#include "server_config.h"

#include "../matrixstaging/runtime/src/libltbase/consolevar.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace mxo::launcher {

namespace {

// Global selected server config (loaded from servers.cfg)
std::vector<ServerConfig> g_ServerConfigs;
const ServerConfig* g_SelectedServerConfig = nullptr;

// Helper to trim whitespace from a string
std::string TrimString(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    if (start == str.size()) {
        return "";
    }
    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        --end;
    }
    return str.substr(start, end - start + 1);
}

// Helper to remove surrounding quotes from a value
std::string UnquoteString(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

// Parse a single server section into a ServerConfig struct
// The section name becomes the server name
bool ParseServerSection(
    mxo::libltbase::ConsoleConfigParseState& state,
    ServerConfig& outConfig,
    mxo::libltbase::ConsoleParseErrorSink& errors) {
    outConfig = ServerConfig();
    outConfig.name = state.configSectionName;
    
    bool hasAuthServer = false;
    
    while (mxo::libltbase::CConsoleVar::ParseConfigFileSectionLine(state, &errors)) {
        if (state.currentName.empty()) {
            continue;
        }
        
        const std::string value = UnquoteString(TrimString(state.currentValue));
        
        if (_stricmp(state.currentName.c_str(), "authServerDnsName") == 0) {
            outConfig.authServerDnsName = value;
            hasAuthServer = true;
        } else if (_stricmp(state.currentName.c_str(), "authServerPort") == 0) {
            outConfig.authServerPort = static_cast<uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (_stricmp(state.currentName.c_str(), "marginServerSuffix") == 0) {
            outConfig.marginServerSuffix = value;
        } else if (_stricmp(state.currentName.c_str(), "marginServerPort") == 0) {
            outConfig.marginServerPort = static_cast<uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (_stricmp(state.currentName.c_str(), "kServerPublicModulusB64") == 0) {
            outConfig.kServerPublicModulusB64 = value;
        } else if (_stricmp(state.currentName.c_str(), "kServerPublicExponentB64") == 0) {
            outConfig.kServerPublicExponentB64 = value;
        }
        // Unknown vars are silently ignored (ignoreUnknownVars is true by default)
    }
    
    // A valid server config must have at least an auth server name
    return hasAuthServer && !outConfig.authServerDnsName.empty();
}

// Find all section names in the config file
// Returns vector of (sectionName, filePosition) pairs
std::vector<std::pair<std::string, long>> FindAllSections(
    FILE* file,
    mxo::libltbase::ConsoleParseErrorSink& /*errors*/) {
    std::vector<std::pair<std::string, long>> sections;
    if (!file) {
        return sections;
    }
    
    char lineBuffer[0x5dc];
    while (std::fgets(lineBuffer, sizeof(lineBuffer), file)) {
        char* cursor = lineBuffer;
        while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        
        if (*cursor == '[') {
            const long position = std::ftell(file) - std::strlen(lineBuffer);
            ++cursor;
            char* sectionEnd = std::strchr(cursor, ']');
            if (sectionEnd) {
                *sectionEnd = '\0';
                std::string sectionName = TrimString(cursor);
                // Skip "server" prefix sections - we want the actual server names
                if (!sectionName.empty() && sectionName.find("server ") == 0) {
                    // Extract name from [server "name"] format
                    size_t nameStart = 6; // skip "server "
                    while (nameStart < sectionName.size() && sectionName[nameStart] == ' ') {
                        ++nameStart;
                    }
                    if (nameStart < sectionName.size() && sectionName[nameStart] == '"') {
                        ++nameStart;
                        size_t nameEnd = sectionName.find('"', nameStart);
                        if (nameEnd != std::string::npos) {
                            sectionName = sectionName.substr(nameStart, nameEnd - nameStart);
                        }
                    } else {
                        sectionName = sectionName.substr(nameStart);
                    }
                }
                sections.emplace_back(sectionName, position);
            }
        }
    }
    
    return sections;
}

} // namespace

std::vector<ServerConfig> LoadServerConfigs(const char* configPath) {
    std::vector<ServerConfig> configs;
    
    if (!configPath || !configPath[0]) {
        return configs;
    }
    
    FILE* file = std::fopen(configPath, "rt");
    if (!file) {
        // File doesn't exist - return empty list (caller can use defaults)
        return configs;
    }
    std::fclose(file);
    
    mxo::libltbase::ConsoleConfigParseState state;
    state.configFilePath = configPath;
    state.ignoreUnknownVars = true;
    
    mxo::libltbase::ConsoleParseErrorSink errors;
    
    // Open the file
    state.primaryFile = std::fopen(configPath, "rt");
    if (!state.primaryFile) {
        return configs;
    }
    state.activeFile = state.primaryFile;
    
    // First pass: find all section names
    std::vector<std::pair<std::string, long>> sections = FindAllSections(state.primaryFile, errors);
    
    // Reset file position
    std::fseek(state.primaryFile, 0, SEEK_SET);
    state.activeFile = state.primaryFile;
    
    // Second pass: parse each section
    char lineBuffer[0x5dc];
    
    for (const auto& [sectionName, sectionPos] : sections) {
        // Seek to section and parse it
        std::fseek(state.primaryFile, sectionPos, SEEK_SET);
        state.activeFile = state.primaryFile;
        state.configSectionName = sectionName;
        state.foundSectionHeader = false;
        state.reachedEndOfFile = false;
        
        // Read and validate section header
        if (!mxo::libltbase::CConsoleVar::GetNextConfigFileLine(state, lineBuffer, sizeof(lineBuffer), &errors)) {
            continue;
        }
        
        // Verify we're at the expected section
        char* cursor = lineBuffer;
        while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (*cursor != '[') {
            continue;
        }
        
        ServerConfig config;
        if (ParseServerSection(state, config, errors)) {
            configs.push_back(std::move(config));
        }
    }
    
    mxo::libltbase::CConsoleVar::CloseConfigState(state);
    return configs;
}

const ServerConfig* FindServerConfigByName(
    const std::vector<ServerConfig>& configs,
    const char* name) {
    if (!name || !name[0]) {
        return nullptr;
    }
    
    for (const auto& config : configs) {
        if (_stricmp(config.name.c_str(), name) == 0) {
            return &config;
        }
    }
    
    return nullptr;
}

const ServerConfig* GetSelectedServerConfig() {
    return g_SelectedServerConfig;
}

void SetSelectedServerConfig(const ServerConfig* config) {
    g_SelectedServerConfig = config;
}

void SetServerConfigs(std::vector<ServerConfig>&& configs) {
    g_ServerConfigs = std::move(configs);
    if (!g_ServerConfigs.empty() && !g_SelectedServerConfig) {
        g_SelectedServerConfig = &g_ServerConfigs[0];
    }
}

const std::vector<ServerConfig>& GetAllServerConfigs() {
    return g_ServerConfigs;
}

} // namespace mxo::launcher
