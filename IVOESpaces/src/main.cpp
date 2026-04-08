#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    App app(hInstance);
    if (!app.Initialize()) {
        return 1;
    }

    return app.Run();
}
