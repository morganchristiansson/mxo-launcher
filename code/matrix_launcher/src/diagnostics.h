#pragma once

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
bool DiagnosticCanBeginAuthConnection();
bool DiagnosticCanSubmitLoginRequestViaResolvedMediatorSurface();
uint32_t DiagnosticSubmitLoginRequestViaResolvedMediatorSurface(
    const mxo::ltlogin::ProcessLoginRequestInputSketch& input);
uint32_t DiagnosticBeginAuthConnection();
uint32_t DiagnosticBeginMarginConnection();
void DiagnosticPumpLauncherNetwork(bool nonBlocking);
void DiagnosticResetPostedLoginResult();
bool DiagnosticHasSuccessfulPreClientAuthState();
uint32_t DiagnosticLastLoginEvent();
uint32_t DiagnosticLastLoginError();
uint32_t DiagnosticRecoveredCharacterCount();
bool DiagnosticRecoveredCharacterName(uint32_t slotIndex, char* outName, uint32_t outNameCapacity);
void DiagnosticSetLauncherSelectedCharacterIndex(uint32_t slotIndex);
bool DiagnosticResolveRecoveredCharacterSelectionForLauncher(
    uint32_t slotIndex,
    char* outCharacterName,
    uint32_t outCharacterNameCapacity,
    char* outWorldName,
    uint32_t outWorldNameCapacity,
    uint32_t* outDescriptorIndex);
bool DiagnosticInstallR3d9D3DCompileHook(HMODULE r3d9Module);
bool DiagnosticInstallR3d9Direct3DCreate9Hook(HMODULE r3d9Module);
void DiagnosticLogLastD3DDeviceActivity();
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
void DiagnosticLogClientLoadingStateText(const char* text, const char* source);
void DiagnosticLogKnownClientEngineInitStatusTextsOnce(const char* source);
