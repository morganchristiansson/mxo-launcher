#pragma once

#include <winsock2.h>
#include <windows.h>
#include <cstdint>

namespace mxo::ltlogin {
class SubmitLoginRequestInput_0x407d50;
}

void DiagnosticLogClientLoadingStateText(const char* text, const char* source);

// Diagnostic-only runtime detour for client.dll loading/status text updates.
// Default: enabled, with the recovered replacement text mapping applied at the real client text
// boundary.
// Opt out with:
// - MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK=1
bool DiagnosticMaybeInstallClientLoadingTextHook(HMODULE clientModule);
