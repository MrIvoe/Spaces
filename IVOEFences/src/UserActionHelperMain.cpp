#include <windows.h>
#include <shellapi.h>
#include <string>
#include "PathMove.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }

    int exitCode = 1;
    if (argc == 4 && lstrcmpiW(argv[1], L"--move") == 0) {
        const std::wstring source = argv[2];
        const std::wstring destinationDir = argv[3];
        exitCode = PathMove::MovePathToDirectory(source, destinationDir) ? 0 : 2;
    }

    LocalFree(argv);
    return exitCode;
}
