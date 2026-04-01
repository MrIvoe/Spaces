#include "App.h"
#include "Win32Helpers.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    Win32Helpers::LogInfo(L"SimpleFences starting");

    App app;
    if (!app.Initialize(hInstance))
        return 1;

    return app.Run();
}
