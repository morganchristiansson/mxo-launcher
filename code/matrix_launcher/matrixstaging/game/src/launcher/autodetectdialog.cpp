#include "autodetectdialog.h"

#include <windows.h>
#include <process.h>

#include <cstdio>
#include <cstring>

#include <spdlog/spdlog.h>

namespace {

CAutodetectDialog* g_CurrentAutodetectDialog2570 = nullptr; // anchor: launcher.exe:0x4d2570
HANDLE g_AutodetectWorkerThread256c = nullptr;              // anchor: launcher.exe:0x4d256c
std::uint32_t g_AutodetectExitCode2568 = 0;                 // anchor: launcher.exe:0x4d2568

HANDLE OpenNulHandle(DWORD desiredAccess) {
    return CreateFileA(
        "NUL",
        desiredAccess,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
}

} // namespace

// anchor: launcher.exe:0x401520
CAutodetectDialog::CAutodetectDialog() {
    std::memset(m_RecoveredBaseTo73, 0, sizeof(m_RecoveredBaseTo73));
    m_ResultReady74 = 0;
    m_Icon78 = nullptr;
}

// anchor: launcher.exe:0x4012e0
CAutodetectDialog::~CAutodetectDialog() {
    HANDLE workerThreadHandle = reinterpret_cast<HANDLE>(m_WorkerThreadHandle);
    if (workerThreadHandle) {
        CloseHandle(workerThreadHandle);
        m_WorkerThreadHandle = nullptr;
    }
    if (g_CurrentAutodetectDialog2570 == this) {
        g_CurrentAutodetectDialog2570 = nullptr;
    }
    if (g_AutodetectWorkerThread256c == workerThreadHandle) {
        g_AutodetectWorkerThread256c = nullptr;
    }
}

// UNANCHORED: no-GUI wrapper for the original modal autodetect path.
bool CAutodetectDialog::RunWithoutGui() {
    if (!OnInitDialogWithoutGui()) {
        return false;
    }

    HANDLE workerThreadHandle = reinterpret_cast<HANDLE>(m_WorkerThreadHandle);
    if (!workerThreadHandle) {
        return (m_ResultReady74 != 0);
    }

    const DWORD waitResult = WaitForSingleObject(workerThreadHandle, 65000);
    if (waitResult != WAIT_OBJECT_0) {
        spdlog::warn(
            "UNANCHORED: no-GUI autodetect wrapper timed out waiting for worker thread (waitResult=0x{:08x})",
            static_cast<unsigned>(waitResult));
        EndDialogWithoutGui();
        return false;
    }

    CloseHandle(workerThreadHandle);
    m_WorkerThreadHandle = nullptr;
    if (g_AutodetectWorkerThread256c == workerThreadHandle) {
        g_AutodetectWorkerThread256c = nullptr;
    }
    if (g_CurrentAutodetectDialog2570 == this) {
        g_CurrentAutodetectDialog2570 = nullptr;
    }

    return (m_ResultReady74 != 0);
}

std::uint32_t CAutodetectDialog::ExitCode() const {
    return m_ExitCode;
}

const char* CAutodetectDialog::DetailLabel() const {
    return m_DetailLabel;
}

const char* CAutodetectDialog::MemoryLabel() const {
    return m_MemoryLabel;
}

// anchor: launcher.exe:0x401640
bool CAutodetectDialog::OnInitDialogWithoutGui() {
    m_ResultReady74 = 0;
    m_LaunchedWorker = false;
    g_CurrentAutodetectDialog2570 = this;

    unsigned workerThreadId = 0;
    const uintptr_t threadHandle = _beginthreadex(
        nullptr,
        0,
        &CAutodetectDialog::WorkerThreadStart,
        this,
        0,
        &workerThreadId);
    m_WorkerThreadHandle = reinterpret_cast<void*>(threadHandle);
    m_WorkerThreadId = workerThreadId;
    g_AutodetectWorkerThread256c = reinterpret_cast<HANDLE>(threadHandle);

    if (threadHandle == 0) {
        spdlog::warn(
            "UNANCHORED: _beginthreadex failed for no-GUI autodetect wrapper; applying default autodetect UI state");
        ApplyExitCodeUiState();
        return true;
    }

    m_LaunchedWorker = true;
    return true;
}

// anchor: launcher.exe:0x401590
unsigned __stdcall CAutodetectDialog::WorkerThreadStart(void* context) {
    CAutodetectDialog* self = static_cast<CAutodetectDialog*>(context);
    return self ? self->RunWorkerThreadMain() : 0u;
}

// anchor: launcher.exe:0x401590
unsigned CAutodetectDialog::RunWorkerThreadMain() {
    bool timedOut = false;
    std::uint32_t exitCode = 0;
    const bool launched = LaunchHiddenAutodetectProcess(&exitCode, &timedOut);

    m_ExitCode = exitCode;
    g_AutodetectExitCode2568 = exitCode;

    if (!launched) {
        spdlog::warn(
            "UNANCHORED: no-GUI autodetect wrapper failed to launch helper; preserving default zero exit-code state");
    } else if (timedOut) {
        spdlog::warn(
            "UNANCHORED: autodetect_settings.exe timed out after the original 60-second wait window");
    }

    if (g_CurrentAutodetectDialog2570 == this) {
        ApplyExitCodeUiState();
    }
    return 0u;
}

// anchor: launcher.exe:0x4013c0
void CAutodetectDialog::ApplyExitCodeUiState() {
    std::snprintf(
        m_DetailLabel,
        sizeof(m_DetailLabel),
        "  Detail: %s",
        LabelForQualityByte(m_ExitCode & 0xffu));
    std::snprintf(
        m_MemoryLabel,
        sizeof(m_MemoryLabel),
        "  Memory: %s",
        LabelForQualityByte((m_ExitCode >> 8) & 0xffu));
    m_ResultReady74 = 1;

    spdlog::info("autodetect title      = Default Settings:");
    spdlog::info("autodetect detail     = {}", m_DetailLabel);
    spdlog::info("autodetect memory     = {}", m_MemoryLabel);
    spdlog::info("autodetect button     = Continue");
    spdlog::info("autodetect exit code  = 0x{:08x}", m_ExitCode);
}

// anchor: launcher.exe:0x401300
void CAutodetectDialog::EndDialogWithoutGui() {
    HANDLE workerThreadHandle = reinterpret_cast<HANDLE>(m_WorkerThreadHandle);
    if (m_ResultReady74 == 0 && workerThreadHandle != nullptr) {
        TerminateThread(workerThreadHandle, 0);
        DeleteFileA("options.cfg");
    }
    if (workerThreadHandle != nullptr) {
        CloseHandle(workerThreadHandle);
        m_WorkerThreadHandle = nullptr;
    }
    g_AutodetectWorkerThread256c = nullptr;
    if (g_CurrentAutodetectDialog2570 == this) {
        g_CurrentAutodetectDialog2570 = nullptr;
    }
}

// anchor: launcher.exe:0x401350
const char* CAutodetectDialog::LabelForQualityByte(std::uint32_t value) {
    if (value == 3u) {
        return "High";
    }
    if (value == 2u) {
        return "Medium";
    }
    return "Low";
}

// anchor: launcher.exe:0x401590
bool CAutodetectDialog::LaunchHiddenAutodetectProcess(std::uint32_t* exitCode, bool* timedOut) {
    if (exitCode) {
        *exitCode = 0;
    }
    if (timedOut) {
        *timedOut = false;
    }

    STARTUPINFOA startupInfo = {};
    PROCESS_INFORMATION processInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    HANDLE nulInput = OpenNulHandle(GENERIC_READ);
    HANDLE nulOutput = OpenNulHandle(GENERIC_WRITE);
    HANDLE nulError = OpenNulHandle(GENERIC_WRITE);
    bool inheritHandles = false;

    // UNANCHORED: the original GUI launcher had no console, so the helper would not spam a
    // parent terminal. Our replacement runs under a console/Wine harness, so redirect child stdio
    // to NUL and request CREATE_NO_WINDOW to preserve the original hidden user-visible behavior.
    if (nulInput != INVALID_HANDLE_VALUE && nulOutput != INVALID_HANDLE_VALUE && nulError != INVALID_HANDLE_VALUE) {
        SetHandleInformation(nulInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(nulOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(nulError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdInput = nulInput;
        startupInfo.hStdOutput = nulOutput;
        startupInfo.hStdError = nulError;
        inheritHandles = true;
    }

    char commandLine[] = "autodetect_settings.exe setopts hide";
    const BOOL created = CreateProcessA(
        nullptr,
        commandLine,
        nullptr,
        nullptr,
        inheritHandles ? TRUE : FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        ".",
        &startupInfo,
        &processInfo);

    if (nulInput != INVALID_HANDLE_VALUE) {
        CloseHandle(nulInput);
    }
    if (nulOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(nulOutput);
    }
    if (nulError != INVALID_HANDLE_VALUE) {
        CloseHandle(nulError);
    }

    if (!created) {
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 60000);
    if (waitResult == WAIT_OBJECT_0) {
        DWORD childExitCode = 0;
        GetExitCodeProcess(processInfo.hProcess, &childExitCode);
        if (exitCode) {
            *exitCode = static_cast<std::uint32_t>(childExitCode);
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        if (timedOut) {
            *timedOut = true;
        }
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

