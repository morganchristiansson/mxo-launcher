#pragma once

#include <cstdint>

namespace mxo::liblttcp {
class CLTThreadPerClientTCPEngine_0x4b2768;
}

// UNANCHORED: launcher-owned startup bind helper from the arg5 ABI shell to the shared liblttcp
// engine sidecar. This belongs to the real `0x40a380` create/store/register handoff path, not to
// arbitrary later arg5 dispatch.
mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768* LauncherNetworkEngineFromAbiShell(void* ownerPtr);

// UNANCHORED: launcher-owned replacement for the original 0x40a380 allocation + ctor path.
// This returns the raw arg5 launcher object so `CLauncher::InitializeThreadPerClientTCPEngine()`
// can preserve the original store-to-`0x4d6304` then call-arg6-`+0x08` ordering itself.
void* LauncherCreateNetworkEngineAbiShell();

// UNANCHORED: launcher-owned replacement for the original 0x40b389..0x40b404 release/clear path.
void LauncherReleaseNetworkEngineAbiShell(void** launcherObjectPtr, void* mediatorPtr);

// UNANCHORED: launcher-owned diagnostic log for the active arg5 primary-dispatch mode / vptr.
void LauncherLogNetworkEngineAbiShellDispatchState(void* launcherObjectPtr, const char* phase);

// UNANCHORED: launcher-owned poll helper for pre-client auth/selection sequencing.
void LauncherPumpNetworkEngineAbiShell(void* launcherObjectPtr, bool nonBlocking);

// UNANCHORED: reports whether the current build exposes arg5 as the live native engine object
// rather than a detached wrapper shell.
bool LauncherNetworkEngineUsesNativeObjectStorage();


