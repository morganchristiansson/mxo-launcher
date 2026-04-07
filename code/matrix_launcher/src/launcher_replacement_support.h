#pragma once

#include <cstddef>
#include <cstdint>

namespace mxo::launcher::replacement {

// Replacement-only shared launcher selection metadata.
// This does not claim an original launcher.exe helper/class boundary; it only centralizes the
// current recovered world-selection defaults used by both launcher-owned startup scaffolding and
// the replacement text-mode pre-client flow.
struct RecoveredLauncherSelectionRecord {
    const char* selectionName;
    const char* routeHostPrefix;
    uint32_t selectionGateByte100;
    uint32_t variantState;
};

const RecoveredLauncherSelectionRecord* DefaultRecoveredLauncherSelectionRecord();
const RecoveredLauncherSelectionRecord* FindRecoveredLauncherSelectionRecord(const char* selectionName);
uint32_t RecoveredSelectionWorldIndexLow24();

bool LoadLastWorldNameFromRegistry(char* out, size_t outSize);
bool StoreLastWorldNameInRegistry(const char* selectionName);

} // namespace mxo::launcher::replacement
