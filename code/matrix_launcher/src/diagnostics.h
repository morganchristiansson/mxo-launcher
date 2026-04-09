#pragma once

#include <winsock2.h>
#include <windows.h>
#include <cstdint>

namespace mxo::ltlogin {
struct ProcessLoginRequestInputSketch;
}

void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr);
void DiagnosticConfigureMediatorSelection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedSelectionGateByte100,
    uint32_t selectedVariantState);
bool DiagnosticResolveLauncherSelectionFromMediator(
    void* mediatorPtr,
    uint32_t requestedWorldIndexLow24,
    uint32_t requestedSelectionIndexHighWord,
    uint32_t* outFieldA8,
    uint32_t* outFieldAC,
    char* outWorldName,
    uint32_t outWorldNameCapacity);
void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr, uint32_t parsedNoPatchValue, uint32_t clientVersionValue);
void DiagnosticConfigureLoginControllerNetwork(
    const char* authDnsName,
    uint16_t authPortHostOrder,
    bool ignoreHostsFileForAuth,
    const char* marginDnsSuffix,
    uint16_t marginPortHostOrder,
    bool ignoreHostsFileForMargin,
    const char* marginRouteHostPrefix,
    const char* exactMarginHostName);
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
void DiagnosticLogClientLoadingStateText(const char* text, const char* source);

// Diagnostic-only runtime detour for client.dll loading/status text updates.
// Default: enabled, with the recovered replacement text mapping applied at the real client text
// boundary.
// Opt out with:
// - MXO_DISABLE_DIAGNOSTIC_CLIENT_LOADING_TEXT_HOOK=1
bool DiagnosticMaybeInstallClientLoadingTextHook(HMODULE clientModule);
void DiagnosticRemoveClientLoadingTextHook();
