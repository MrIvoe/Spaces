#pragma once
#include <windows.h>

extern "C" {

__declspec(dllexport) BOOL WINAPI IvoeShellIntegration_Initialize();
__declspec(dllexport) void WINAPI IvoeShellIntegration_Shutdown();

}
