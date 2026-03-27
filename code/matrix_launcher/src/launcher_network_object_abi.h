#pragma once

namespace mxo::liblttcp {
class CLTThreadPerClientTCPEngine;
}

// Internal bridge from the arg5 ABI shell to the shared liblttcp engine sidecar.
// Keep the raw launcher-object layout in `launcher_network_object_abi.cpp`, but let other
// launcher-side surfaces resolve the real engine instead of reinterpreting the arg5 stub pointer as
// a `CLTThreadPerClientTCPEngine*`.
mxo::liblttcp::CLTThreadPerClientTCPEngine* DiagnosticGetLauncherObjectEngine(void* ownerPtr);
