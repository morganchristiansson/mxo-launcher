#pragma once

#include <cstdint>

namespace mxo::liblttcp {
class CLTThreadPerClientTCPEngine;
}

// Internal auth-side diagnostics split.
// This file exists to keep launcher-owned auth diagnostics/state out of src/diagnostics.cpp.

void DiagnosticAuthResetState();
void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword);
void DiagnosticAuthPollLiveConnectionTraffic(void* owner);

// Bridge helpers implemented in src/diagnostics.cpp so auth-side diagnostics can stay split
// without duplicating launcher-object queue/layout logic there.
bool DiagnosticAuthBridgePushQueue0C(void* owner, uint32_t value0, uint32_t value1);
void DiagnosticAuthBridgeSyncOwnerState(void* owner);
