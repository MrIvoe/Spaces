#include "ShellIntegration.h"

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI IvoeShellIntegration_Initialize() {
    // Optional module boundary. Keep behavior off by default unless host opts in.
    return TRUE;
}

extern "C" __declspec(dllexport) void WINAPI IvoeShellIntegration_Shutdown() {
}
