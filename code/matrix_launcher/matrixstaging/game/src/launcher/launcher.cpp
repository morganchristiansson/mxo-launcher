// String anchors point at the original source location for the main launcher class.
// This file is intentionally a recovered class skeleton, not the live executable entrypoint.
// The current runnable implementation still lives in src/resurrections.cpp.

#include <cstddef>
#include <cstdint>

namespace mxo {
namespace launcher {

struct RecoveredLauncherStartupContext;

class CLauncher {
public:
    // anchor: launcher.exe:0x4097f0
    // Original binary shape:
    // - derives from MFC CWinApp
    // - installs vtable 0x004abfe0
    // This dead-code scaffold intentionally does not model the MFC base chain yet.
    CLauncher() = default;

    // anchor: launcher.exe:0x40b430
    bool InitInstance();

    // anchor: launcher.exe:0x409950 / 0x4173d0
    bool ParseCommandLineStage() const;

    // anchor: launcher.exe:0x40a780
    bool LoadCresDLL() const;

    // anchor: launcher.exe:0x40a420
    bool LoadClientDLL() const;

    // anchor: launcher.exe:0x40a4d0
    bool RunClientDllLifecycle() const;

    // anchor: launcher.exe:0x40a760
    void UnloadClientDLL() const;

    // anchor: launcher.exe:0x40a7a0
    void UnloadCresDLL() const;

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    bool BuildRecoveredStartupContext(RecoveredLauncherStartupContext* startupContext);

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    bool PrepareRecoveredInitClientState(const RecoveredLauncherStartupContext& startupContext);

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    void LogOriginalPathGapSummary() const;

    // anchor: launcher.exe:0x40a55c / 0x40b430
    uint32_t BuildPackedArg7Selection() const;

    // Recovered tail fields currently proven useful from the original object.
    // The omitted MFC base / earlier fields account for offsets 0x00..0xa3 in the original binary.
    uint8_t m_RecoveredBaseToA3[0xa4] = {};
    uint32_t m_FieldA4 = 0;          // original: [this+0xa4]
    uint32_t m_FieldA8 = 0xffffffff; // original: [this+0xa8]
    uint32_t m_FieldAC = 0;          // original: [this+0xac]
    uint8_t m_FieldB0 = 0;           // original: [this+0xb0]
};

struct RecoveredLauncherStartupContext {
    char mediatorSelectionName[64];
    const void* recoveredSelection;
    uint32_t mediatorSelectedSelectionGateByte100;
    uint32_t mediatorSelectedVariantState;
    uint32_t nopatchLauncherVersionValue;
    uint32_t nopatchClientVersionValue;
};

static_assert(offsetof(CLauncher, m_FieldA4) == 0xa4, "CLauncher +0xa4 drifted");
static_assert(offsetof(CLauncher, m_FieldA8) == 0xa8, "CLauncher +0xa8 drifted");
static_assert(offsetof(CLauncher, m_FieldAC) == 0xac, "CLauncher +0xac drifted");
static_assert(offsetof(CLauncher, m_FieldB0) == 0xb0, "CLauncher +0xb0 drifted");

uint32_t CLauncher::BuildPackedArg7Selection() const {
    return (m_FieldAC & 0x00ffffffu) | ((m_FieldA8 & 0xffu) << 24);
}

bool CLauncher::InitInstance() {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

bool CLauncher::ParseCommandLineStage() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

bool CLauncher::LoadCresDLL() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

bool CLauncher::LoadClientDLL() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

bool CLauncher::RunClientDllLifecycle() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

void CLauncher::UnloadClientDLL() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
}

void CLauncher::UnloadCresDLL() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
}

bool CLauncher::BuildRecoveredStartupContext(RecoveredLauncherStartupContext* /*startupContext*/) {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

bool CLauncher::PrepareRecoveredInitClientState(const RecoveredLauncherStartupContext& /*startupContext*/) {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
    return false;
}

void CLauncher::LogOriginalPathGapSummary() const {
    // UNANCHORED: live behavior still owned by src/resurrections.cpp.
}

} // namespace launcher
} // namespace mxo
