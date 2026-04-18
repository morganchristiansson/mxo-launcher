#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mxo::launcher {

struct ServerConfig {
    std::string name;
    std::string authServerDnsName;
    uint16_t authServerPort = 11000;
    std::string marginServerSuffix;
    uint16_t marginServerPort = 10000;
    std::string kServerPublicModulusB64;
    std::string kServerPublicExponentB64;
};

// Parse servers.cfg and return list of server configurations
// Returns empty vector if file doesn't exist or parsing fails
std::vector<ServerConfig> LoadServerConfigs(const char* configPath);

// Find a server config by name (case-insensitive)
// Returns nullptr if not found
const ServerConfig* FindServerConfigByName(
    const std::vector<ServerConfig>& configs,
    const char* name);

// Get the currently selected server config (from textmode_launcher_flow)
// Returns nullptr if no server has been selected yet
const ServerConfig* GetSelectedServerConfig();

// Set the selected server config
void SetSelectedServerConfig(const ServerConfig* config);

// Set all server configs (for testing)
void SetServerConfigs(std::vector<ServerConfig>&& configs);

// Get all server configs (for testing)
const std::vector<ServerConfig>& GetAllServerConfigs();

} // namespace mxo::launcher
