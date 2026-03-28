#pragma once

#include <cstddef>
#include <cstdint>

namespace mxo {
namespace launcher {

struct RecoveredLauncherStartupContext {
    char mediatorSelectionName[64];
    const void* recoveredSelection;
    uint32_t mediatorSelectedSelectionGateByte100;
    uint32_t mediatorSelectedVariantState;
    uint32_t nopatchLauncherVersionValue;
    uint32_t nopatchClientVersionValue;
};

class CLauncher {
public:
    // anchor: launcher.exe:0x4097f0
    // Original binary shape:
    // - derives from MFC CWinApp
    // - installs vtable 0x004abfe0
    // This replacement class intentionally does not model the MFC base chain yet.
    uint8_t m_RecoveredBaseToA3[0xa4] = {};
    uint32_t m_FieldA4 = 0;          // original: [this+0xa4]
    uint32_t m_FieldA8 = 0xffffffff; // original: [this+0xa8]
    uint32_t m_FieldAC = 0;          // original: [this+0xac]
    uint8_t m_FieldB0 = 0;           // original: [this+0xb0]

    // anchor: launcher.exe:0x40b430
    bool InitInstance();

    // anchor: launcher.exe:0x409950 / 0x4173d0
    bool ParseCommandLineStage() const;

    // anchor: launcher.exe:0x40a780
    bool LoadCresDLL() const;

    // anchor: launcher.exe:0x40a420
    bool LoadClientDLL() const;

    // anchor: launcher.exe:0x40a760
    void UnloadClientDLL() const;

    // anchor: launcher.exe:0x40a7a0
    void UnloadCresDLL() const;

    // Recovered grouping inside launcher.exe:0x40b430
    bool BuildStartupContextFromRecoveredSelection(RecoveredLauncherStartupContext* startupContext);

    // Recovered grouping inside launcher.exe:0x40b430
    bool PrepareInitClientStateFromStartupContext(const RecoveredLauncherStartupContext& startupContext);

    // Recovered grouping inside launcher.exe:0x40b430
    void LogInitInstanceFaithfulnessGaps() const;

    // anchor: launcher.exe:0x40a55c / 0x40b430
    uint32_t BuildPackedArg7Selection() const;

    // anchor: launcher.exe:0x40a4d0
    bool RunClientDllLifecycle() const;
};

static_assert(offsetof(CLauncher, m_FieldA4) == 0xa4, "CLauncher +0xa4 drifted");
static_assert(offsetof(CLauncher, m_FieldA8) == 0xa8, "CLauncher +0xa8 drifted");
static_assert(offsetof(CLauncher, m_FieldAC) == 0xac, "CLauncher +0xac drifted");
static_assert(offsetof(CLauncher, m_FieldB0) == 0xb0, "CLauncher +0xb0 drifted");

} // namespace launcher
} // namespace mxo
