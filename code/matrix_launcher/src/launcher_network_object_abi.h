#pragma once

namespace mxo::liblttcp {
class CLTThreadPerClientTCPEngine;
}

// UNANCHORED: launcher-owned bridge from the arg5 ABI shell to the shared liblttcp engine sidecar.
// Keep the raw launcher-object layout in `launcher_network_object_abi.cpp`, but let other
// launcher-side surfaces resolve the real engine instead of reinterpreting the ABI shell pointer as
// a `CLTThreadPerClientTCPEngine*`.
mxo::liblttcp::CLTThreadPerClientTCPEngine* LauncherNetworkEngineFromAbiShell(void* ownerPtr);

// UNANCHORED: launcher-owned replacement for the original 0x40a380 allocation/registration path.
void LauncherInstallNetworkEngineAbiShell(void** outLauncherObjectPtr, void* mediatorPtr);

// UNANCHORED: launcher-owned replacement for the original 0x40b389..0x40b404 release/clear path.
void LauncherReleaseNetworkEngineAbiShell(void** launcherObjectPtr, void* mediatorPtr);

// UNANCHORED: launcher-owned diagnostic log for the active arg5 primary-dispatch mode / vptr.
void LauncherLogNetworkEngineAbiShellDispatchState(void* launcherObjectPtr, const char* phase);

// UNANCHORED: launcher-owned poll helper for pre-client auth/selection sequencing.
void LauncherPumpNetworkEngineAbiShell(void* launcherObjectPtr, bool nonBlocking);

