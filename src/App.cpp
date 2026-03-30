#include "App.h"
#include "FenceStorage.h"
#include "Persistence.h"
#include "FenceManager.h"
#include "TrayMenu.h"
#include "Win32Helpers.h"
#include <commctrl.h>
#include <fstream>

#pragma comment(lib, "Comctl32.lib")

static void LogDebug(const wchar_t* msg)
{
    std::wofstream log(L"C:\\Users\\MrIvo\\AppData\\Local\\SimpleFences\\debug.log", std::ios::app);
    if (log.is_open())
    {
        log << msg << L"\n";
        log.close();
    }
}

App::App() = default;

App::~App() = default;

bool App::Initialize(HINSTANCE hInstance)
{
    LogDebug(L"App::Initialize starting");
    m_hInstance = hInstance;

    // Initialize common controls (legacy but reliable)
    InitCommonControls();
    LogDebug(L"InitCommonControls done");

    LogDebug(L"Creating FenceStorage");
    // Create storage and persistence
    m_storage = std::make_unique<FenceStorage>();
    m_persistence = std::make_unique<Persistence>(m_storage->GetMetadataPath());

    LogDebug(L"Creating FenceManager");
    // Create manager
    m_manager = std::make_unique<FenceManager>(std::move(m_storage), std::move(m_persistence));

    LogDebug(L"Creating TrayMenu");
    // Create tray icon
    m_tray = std::make_unique<TrayMenu>(this);
    if (!m_tray->Create(hInstance))
    {
        LogDebug(L"TrayMenu::Create failed");
        return false;
    }

    LogDebug(L"Loading persisted fences");
    // Load persisted fences
    if (!m_manager->LoadAll())
    {
        LogDebug(L"LoadAll failed");
        return false;
    }

    LogDebug(L"Initialize completed successfully");
    return true;
}

int App::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

void App::Exit()
{
    PostQuitMessage(0);
}

void App::CreateFenceNearCursor()
{
    if (!m_manager)
        return;

    POINT pt{};
    GetCursorPos(&pt);
    m_manager->CreateFenceAt(pt.x, pt.y);
}

FenceManager* App::GetFenceManager() const
{
    return m_manager.get();
}
