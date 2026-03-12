#pragma once

#include <windows.h>
#include <cstdint>

void Log(const char* fmt, ...);

void DiagnosticInstallMediatorStub(void** outMediatorPtr);
void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr);
void DiagnosticConfigureMediatorSelection(uint32_t selectionUpperBoundExclusive, const char* mappedSelectionName);
void DiagnosticConfigureMediatorProfileName(const char* profileName);
void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr, uint32_t parsedNoPatchValue, uint32_t clientVersionValue);
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr);
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
