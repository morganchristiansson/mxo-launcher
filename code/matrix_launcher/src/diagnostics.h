#pragma once

#include <windows.h>
#include <cstdint>

void Log(const char* fmt, ...);

void DiagnosticInstallMediatorStub(void** outMediatorPtr);
void DiagnosticInstallMediatorViaBinderScaffold(void** outMediatorPtr);
void DiagnosticConfigureMediatorSelection(uint32_t highByteFloor, const char* mappedSelectionName);
void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr);
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr);
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
