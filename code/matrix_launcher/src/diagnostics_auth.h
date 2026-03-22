#pragma once

#include <cstdint>

namespace mxo::liblttcp {
class CLTThreadPerClientTCPEngine;
}

namespace mxo::ltlogin {
class CLTLoginMediator;
}

// Internal auth-side diagnostics split.
// This file exists to keep launcher-owned auth diagnostics/state out of src/diagnostics.cpp.

void DiagnosticAuthResetState();
void DiagnosticAuthInitializeForEngine(void* owner, mxo::liblttcp::CLTThreadPerClientTCPEngine* engine);
void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword);
void DiagnosticAuthPollLiveConnectionTraffic(void* owner);
mxo::ltlogin::CLTLoginMediator* DiagnosticAuthGetLoginController();
const char* DiagnosticAuthCurrentCharacterName();
uint32_t DiagnosticAuthCurrentCharacterIdLow();
uint32_t DiagnosticAuthCurrentCharacterIdHigh();
const char* DiagnosticAuthCurrentRealFirstName();
const char* DiagnosticAuthCurrentRealLastName();
const char* DiagnosticAuthCurrentBackground();

// Bridge helpers implemented in src/launcher_network_object_abi.cpp so auth-side
// diagnostics can stay split without duplicating launcher-object queue/layout logic.
bool DiagnosticAuthBridgePushQueue0C(void* owner, uint32_t value0, uint32_t value1);
void DiagnosticAuthBridgeSyncOwnerState(void* owner);
