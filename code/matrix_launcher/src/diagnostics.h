#pragma once

#include <windows.h>
#include <cstdint>

void Log(const char* fmt, ...);

void DiagnosticInstallMediatorStub(void** outMediatorPtr);
void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr);
void DiagnosticConfigureMediatorSelection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedWorldType,
    uint32_t selectedVariantState);
bool DiagnosticResolveLauncherSelectionFromMediator(
    void* mediatorPtr,
    uint32_t requestedWorldIndexLow24,
    uint32_t requestedVariantIndexHigh8,
    uint32_t* outFieldA8,
    uint32_t* outFieldAC,
    char* outWorldName,
    uint32_t outWorldNameCapacity);
void DiagnosticConfigureMediatorProfileName(const char* profileName);
void DiagnosticConfigureMediatorAuthName(const char* authName);
void DiagnosticConfigureMediatorAuthPassword(const char* authPassword);
void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr, uint32_t parsedNoPatchValue, uint32_t clientVersionValue);
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr);
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
uint32_t DiagnosticBeginAuthConnection();
uint32_t DiagnosticBeginMarginConnection();
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
