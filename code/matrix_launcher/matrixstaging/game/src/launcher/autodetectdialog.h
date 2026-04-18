#pragma once

#include <cstddef>
#include <cstdint>

// anchor: launcher.exe:0x004a9b40
// Recovered autodetect dialog object used from CLauncher::InitInstance when 0x4d2c64 is set.
// This source-owned model intentionally omits the actual GUI/modal loop while preserving the
// original object layout anchors and helper-process/runtime behavior as closely as practical.
class CAutodetectDialog {
public:
    // anchor: launcher.exe:0x401520
    CAutodetectDialog();

    // anchor: launcher.exe:0x4012e0
    ~CAutodetectDialog();

    // UNANCHORED: no-GUI wrapper for the original 0x40b75a -> 0x401520 -> DoModal consumption path.
    bool RunWithoutGui();

    std::uint32_t ExitCode() const;
    const char* DetailLabel() const;
    const char* MemoryLabel() const;

private:
    // anchor: launcher.exe:0x401640
    bool OnInitDialogWithoutGui();

    // anchor: launcher.exe:0x401590
    unsigned RunWorkerThreadMain();
    static unsigned __stdcall WorkerThreadStart(void* context);

    // anchor: launcher.exe:0x4013c0
    void ApplyExitCodeUiState();

    // anchor: launcher.exe:0x401300
    void EndDialogWithoutGui();

    // anchor: launcher.exe:0x401350
    static const char* LabelForQualityByte(std::uint32_t value);

    static bool LaunchHiddenAutodetectProcess(std::uint32_t* exitCode, bool* timedOut);

public:
    std::uint8_t m_RecoveredBaseTo73[0x74] = {};
    std::uint8_t m_ResultReady74 = 0;
    void* m_Icon78 = nullptr;
    void* m_WorkerThreadHandle = nullptr;
    std::uint32_t m_WorkerThreadId = 0;
    std::uint32_t m_ExitCode = 0;
    char m_DetailLabel[32] = {};
    char m_MemoryLabel[32] = {};
    bool m_LaunchedWorker = false;
};

static_assert(offsetof(CAutodetectDialog, m_ResultReady74) == 0x74, "CAutodetectDialog +0x74 drifted");
static_assert(offsetof(CAutodetectDialog, m_Icon78) == 0x78, "CAutodetectDialog +0x78 drifted");


