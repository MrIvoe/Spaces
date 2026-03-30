#include "App.h"
#include <fstream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Simple debug log
    std::wofstream log(L"C:\\Users\\MrIvo\\AppData\\Local\\SimpleFences\\debug.log", std::ios::trunc);
    if (log.is_open())
    {
        log << L"SimpleFences starting\n";
        log.close();
    }

    App app;
    if (!app.Initialize(hInstance))
        return 1;

    return app.Run();
}
