#pragma once

#include <windows.h>
#include <cstdint>

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
    uint32_t requestedVariantIndexHigh8,
    uint32_t* outFieldA8,
    uint32_t* outFieldAC,
    char* outWorldName,
    uint32_t outWorldNameCapacity);
void DiagnosticConfigureMediatorProfileName(const char* profileName);
void DiagnosticConfigureMediatorAuthName(const char* authName);
void DiagnosticConfigureMediatorAuthPassword(const char* authPassword);
void DiagnosticAuthSetMediatorCredentials(const char* authName, const char* authPassword);
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
void DiagnosticConfigureLoginControllerCharacterSeed(
    const char* characterName,
    const char* gameSessionId,
    uint32_t selectedWorldIndexLow24);
bool DiagnosticCanBeginAuthConnection();
uint32_t DiagnosticBeginAuthConnection();
uint32_t DiagnosticBeginMarginConnection();
void DiagnosticPumpLauncherNetwork(bool nonBlocking);
void DiagnosticResetPostedLoginResult();
bool DiagnosticHasSuccessfulPreClientAuthState();
uint32_t DiagnosticLastLoginEvent();
uint32_t DiagnosticLastLoginError();
uint32_t DiagnosticRecoveredCharacterCount();
bool DiagnosticRecoveredCharacterName(uint32_t slotIndex, char* outName, uint32_t outNameCapacity);
bool DiagnosticSelectRecoveredCharacter(uint32_t slotIndex);
bool DiagnosticAdoptRecoveredCharacterSelectionForLauncher(
    uint32_t slotIndex,
    char* outCharacterName,
    uint32_t outCharacterNameCapacity,
    char* outWorldName,
    uint32_t outWorldNameCapacity,
    uint32_t* outDescriptorIndex);
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
void DiagnosticLogClientLoadingStateText(const char* text, const char* source);
void DiagnosticLogKnownClientEngineInitStatusTextsOnce(const char* source);
