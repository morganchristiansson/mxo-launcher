#pragma once

#include <cstddef>
#include <cstdint>

namespace mxo {
namespace launcher {

// UNANCHORED: replacement-only handoff object for the current launcher.exe:0x40b430 recovery.
// It deliberately groups state synthesized by the replacement rather than claiming an original
// standalone struct or helper boundary in the binary.
struct RecoveredLauncherStartupContext {
    char mediatorSelectionName[64];
    const void* recoveredSelection;
    uint32_t nopatchLauncherVersionValue;
    uint32_t nopatchClientVersionValue;
};

// launcher.exe:0x40dc40 / 0x40d8d0 / 0x40e1c0
// Launcher-owned selection-page row payload kept in the `CListCtrl+0x68` node list.
// Current static-RE tightening from the live selection-page family:
// - `0x40e480` builds one 0x48-byte row node per visible world entry
// - the payload stores two indices, four strings, one dword flag, and one dword timestamp
// - `0x40e1c0` later repaints from that stored payload instead of re-querying every column cell
// - negative result for the deeper mediator bridge: this row payload is only a 0x40-byte display /
//   sort / packed-item snapshot and is not yet evidence for the later mediator-owned `0xb4`
//   `0x41c1f0` selection-context input block
struct LauncherSelectionRowPayload40Sketch {
    uint32_t totalWorldIndex00 = 0;        // low 16 bits later reused as packed item-data low word
    uint32_t activeWorldIndex04 = 0;       // low 16 bits later reused as packed item-data high word
    char column0WorldName08[0x0c] = {};   // copied by `0x40dc40`
    char column1Display14[0x0c] = {};     // copied by `0x40dc40`
    char column2Status20[0x0c] = {};      // copied by `0x40dc40`
    char column3Population2c[0x0c] = {};  // copied by `0x40dc40`
    uint32_t availabilityOrSortClass38 = 0; // source dword from `0x40e480` local `+0xfdd8`
    uint32_t tickCount3c = 0;              // `GetTickCount()` at row creation time
};

struct LauncherSelectionRowNode48Sketch {
    LauncherSelectionRowNode48Sketch* next = nullptr;
    LauncherSelectionRowNode48Sketch* prev = nullptr;
    LauncherSelectionRowPayload40Sketch payload08 = {};
};

enum class LauncherSelectionListSortMode004d3588 : uint32_t {
    kInsertionOrder = 0,
    kWorldName = 1,          // `0x40cf40` compares payload `+0x08`
    kDisplayName = 2,        // `0x40cf40` compares payload `+0x14`
    kStatusText = 3,         // `0x40cf40` compares payload `+0x20`
    kPopulationNumeric = 4,  // `0x40cf40` compares `atoi(payload + 0x2c)`
    kStatusClassThenAge = 5, // special status-class compare with `+0x44` timestamp tiebreak
};

static_assert(sizeof(LauncherSelectionRowPayload40Sketch) == 0x40, "row payload size drifted");
static_assert(sizeof(LauncherSelectionRowNode48Sketch) == 0x48, "row node size drifted");

// anchor: launcher.exe:0x004abfe0
// anchor: launcher.exe:0x4097f0
class CLauncher {
public:
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

    // anchor: launcher.exe:0x40a380
    bool InitializeThreadPerClientTCPEngine() const;

    // anchor: launcher.exe:0x40a780
    bool LoadCresDLL() const;

    // anchor: launcher.exe:0x40a420
    bool LoadClientDLL() const;

    // anchor: launcher.exe:0x40a760
    void UnloadClientDLL() const;

    // anchor: launcher.exe:0x40a7a0
    void UnloadCresDLL() const;

    // UNANCHORED: replacement-only synthesis inside launcher.exe:0x40b430 that seeds the
    // launcher-owned selection / nopatch inputs consumed later by the active nopatch path.
    bool BuildStartupContextFromRecoveredSelection(RecoveredLauncherStartupContext* startupContext);

    // UNANCHORED: replacement-only synthesis that materializes arg6/arg7-owned InitClientDLL
    // state before the later 0x40a380 / 0x40b74d..0x40b7af pre-client continuation corridor.
    bool MaterializeRecoveredInitClientStateFromStartupContext(
        const RecoveredLauncherStartupContext& startupContext);

    // UNANCHORED: recovered continuation for the 0x40b74d..0x40b790 pre-client corridor
    // (0x402ec0 gate + optional 0x40b75a autodetect path). This is not claimed as a separate
    // original method boundary.
    bool RunRecoveredPreClientBringupStage() const;

    // UNANCHORED: replacement-owned pre-client auth/character-selection bridge.
    bool RunPreClientAuthAndCharacterSelectionStage();

    // UNANCHORED: recovered grouping inside launcher.exe:0x40b430
    void LogInitInstanceFaithfulnessGaps() const;

    // UNANCHORED: no-GUI wrapper for the original 0x40b75a autodetect dialog consumption path.
    bool RunAutodetectDialogWithoutGui() const;

    // anchor: launcher.exe:0x40a55c / 0x40b430
    uint32_t BuildPackedArg7Selection() const;

    // anchor: launcher.exe:0x40a4d0
    bool RunClientDllLifecycle() const;

    // UNANCHORED: replacement cleanup wrapper for the current launcher-owned arg5/arg6 state.
    void CleanupRecoveredInitClientState() const;
};

static_assert(offsetof(CLauncher, m_FieldA4) == 0xa4, "CLauncher +0xa4 drifted");
static_assert(offsetof(CLauncher, m_FieldA8) == 0xa8, "CLauncher +0xa8 drifted");
static_assert(offsetof(CLauncher, m_FieldAC) == 0xac, "CLauncher +0xac drifted");
static_assert(offsetof(CLauncher, m_FieldB0) == 0xb0, "CLauncher +0xb0 drifted");

} // namespace launcher
} // namespace mxo
