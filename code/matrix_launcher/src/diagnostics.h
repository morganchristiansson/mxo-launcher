#pragma once

#include <windows.h>
#include <cstdint>

void Log(const char* fmt, ...);

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
void DiagnosticConfigureLoginControllerCharacterSeed(
    const char* characterName,
    const char* gameSessionId,
    uint32_t selectedWorldIndexLow24);
void DiagnosticMirrorSelectionContextIntoLoginController(const void* selectionContext, uint32_t byteCount);
void DiagnosticMirrorState9StartupTripleIntoLoginController(void* callback84, void* object88, void* object8c);
uint32_t DiagnosticFillState9CallbackBlob18c(void* outBuffer, uint32_t arg2, uint32_t arg3);
const void* DiagnosticGetState9CallbackSeedPointer85D4();
bool DiagnosticCanBeginAuthConnection();
uint32_t DiagnosticBeginAuthConnection();
uint32_t DiagnosticBeginMarginConnection();
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
