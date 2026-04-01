#include "App.h"
#include "FenceStorage.h"
#include "Persistence.h"
#include "FenceManager.h"
#include "TrayMenu.h"
#include "Win32Helpers.h"
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

App::App() = default;

App::~App()
{
    Shutdown();
}

bool App::Initialize(HINSTANCE hInstance)
{
    Win32Helpers::LogInfo(L"App::Initialize starting");
    m_hInstance = hInstance;

    // Initialize common controls (legacy but reliable)
    InitCommonControls();
    Win32Helpers::LogInfo(L"InitCommonControls done");

    Win32Helpers::LogInfo(L"Creating FenceStorage");
    // Create storage and persistence
    m_storage = std::make_unique<FenceStorage>();
    m_persistence = std::make_unique<Persistence>(m_storage->GetMetadataPath());

    Win32Helpers::LogInfo(L"Creating FenceManager");
    // Create manager
    m_manager = std::make_unique<FenceManager>(std::move(m_storage), std::move(m_persistence));

    Win32Helpers::LogInfo(L"Creating TrayMenu");
    // Create tray icon
    m_tray = std::make_unique<TrayMenu>(this);
    if (!m_tray->Create(hInstance))
    {
        Win32Helpers::LogError(L"TrayMenu::Create failed");
        return false;
    }

    Win32Helpers::LogInfo(L"Loading persisted fences");
    // Load persisted fences
    if (!m_manager->LoadAll())
    {
        Win32Helpers::LogError(L"LoadAll failed");
        return false;
    }

    Win32Helpers::LogInfo(L"Initialize completed successfully");
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
    Shutdown();
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

void App::Shutdown()
{
    if (m_shutdownStarted)
    {
        return;
    }

    m_shutdownStarted = true;
    Win32Helpers::LogInfo(L"App shutdown started");

    if (m_manager)
    {
        m_manager->SaveAll();
        m_manager->Shutdown();
    }

    if (m_tray)
    {
        m_tray->Destroy();
        m_tray.reset();
    }

    m_manager.reset();
    m_persistence.reset();
    m_storage.reset();
    Win32Helpers::LogInfo(L"App shutdown completed");
}
