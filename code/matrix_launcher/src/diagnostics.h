#pragma once

#include <windows.h>

void Log(const char* fmt, ...);

void DiagnosticInstallMediatorStub(void** outMediatorPtr);
void DiagnosticApplyDefaultNopatchMediatorConfig(void* mediatorPtr);
void DiagnosticInstallLauncherObjectStub(void** outLauncherObjectPtr, void* mediatorPtr);
void DiagnosticStartWindowTrace();
void DiagnosticStopWindowTrace();
